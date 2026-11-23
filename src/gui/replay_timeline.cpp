#include "replay_timeline.h"

#include "claude_files.h"

#include <algorithm>
#include <limits>

namespace cx::gui {
namespace {

// A session the registry no longer knows, and which has said nothing for this
// long before the playhead, is drawn as gone. The log cannot tell a session
// that was killed from one left idle, and an idle one costs a node while a
// dead one drawn for an hour is a lie about the machine.
constexpr std::int64_t kIdlePresenceMs = 30 * 60 * 1000;

// A shell whose output file has since been deleted left no record of its end.
// Drawn this long after it started, which is about what a background build
// takes, rather than for ever or not at all.
constexpr std::int64_t kShellFallbackMs = 2 * 60 * 1000;

bool turnStart(const AgentEvent& ev)
{
    // A subagent's call is not its parent's turn, but a parent with a subagent
    // running is busy, and that is already covered by its own Agent call.
    return ev.agentId.empty() && (ev.event == "UserPromptSubmit" || ev.event == "PreToolUse");
}

} // namespace

void ReplayTimeline::load(std::int64_t nowMs)
{
    events_.clear();
    liveNow_.clear();
    identity_.clear();
    learned_.clear();
    shellEnd_.clear();

    for (RegistryEntry& e : readRegistry(sessionRegistryDir())) {
        if (!e.sessionId.empty())
            liveNow_.emplace(e.sessionId, std::move(e));
    }

    absorb(readEvents(eventLogPath()));
    endMs_ = nowMs;
}

void ReplayTimeline::refresh(std::int64_t nowMs)
{
    const std::int64_t mark = events_.empty() ? 0 : events_.back().tsMs;
    std::vector<AgentEvent> fresh = readEvents(eventLogPath(), mark);

    // The lines at exactly the mark are re-read alongside anything new. They
    // come back in the order they were first read, so the count already held
    // is the count to skip.
    std::size_t held = 0;
    for (auto it = events_.rbegin(); it != events_.rend() && it->tsMs == mark; ++it)
        ++held;
    std::size_t skip = 0;
    while (skip < fresh.size() && skip < held && fresh[skip].tsMs == mark)
        ++skip;
    fresh.erase(fresh.begin(), fresh.begin() + static_cast<std::ptrdiff_t>(skip));

    absorb(std::move(fresh));
    endMs_ = std::max(endMs_, nowMs);
}

void ReplayTimeline::absorb(std::vector<AgentEvent> fresh)
{
    // Two sessions append independently and their clocks are read a moment
    // apart, so the log is nearly in order rather than exactly. Stable, so
    // lines sharing a millisecond keep the order they were written in.
    std::stable_sort(fresh.begin(), fresh.end(),
                     [](const AgentEvent& a, const AgentEvent& b) { return a.tsMs < b.tsMs; });

    for (const AgentEvent& ev : fresh) {
        if (!ev.agentId.empty() && !ev.sessionId.empty())
            learnSession(ev.sessionId, ev.cwd);
        if (ev.event == "PostToolUse" && !ev.task.empty())
            learnTask(ev);
    }

    const bool ordered = events_.empty() || fresh.empty() || fresh.front().tsMs >= events_.back().tsMs;
    events_.insert(events_.end(), std::make_move_iterator(fresh.begin()),
                   std::make_move_iterator(fresh.end()));
    if (!ordered) {
        std::stable_sort(events_.begin(), events_.end(),
                         [](const AgentEvent& a, const AgentEvent& b) { return a.tsMs < b.tsMs; });
    }

    if (!events_.empty()) {
        startMs_ = std::max(events_.front().tsMs, events_.back().tsMs - kReplayWindowMs);
        endMs_   = std::max(endMs_, events_.back().tsMs);
    }
}

void ReplayTimeline::learnSession(const std::string& sessionId, const fs::path& cwd)
{
    if (!learned_.insert(sessionId).second)
        return;

    SessionFiles f = reader_.read(sessionId, cwd);
    for (SubAgent& a : f.agents) {
        // What the files know of state is what is true now, which in replay is
        // the future. Only the hooks speak for the past.
        a.state          = AgentState::Spawned;
        a.failed         = false;
        a.lastActivityMs = 0;
        a.recent.clear();
    }
    identity_[sessionId] = std::move(f.agents);
}

void ReplayTimeline::learnTask(const AgentEvent& ev)
{
    const std::string key = ev.sessionId + "/" + ev.task;
    if (shellEnd_.contains(key))
        return;

    std::int64_t   end  = ev.tsMs + kShellFallbackMs;
    const fs::path root = claudeTempRoot();
    if (!root.empty() && !ev.cwd.empty()) {
        const fs::path file =
            root / cf::projectSlug(ev.cwd) / ev.sessionId / "tasks" / (ev.task + ".output");
        if (cf::writerHoldsOpen(file))
            end = std::numeric_limits<std::int64_t>::max();
        else if (const std::int64_t written = cf::fileWriteTimeMs(file); written > 0)
            end = std::max(written, ev.tsMs);
    }
    shellEnd_.emplace(key, end);
}

AgentList ReplayTimeline::boardAt(std::int64_t t) const
{
    const auto cut = std::upper_bound(events_.begin(), events_.end(), t,
                                      [](std::int64_t v, const AgentEvent& e) { return v < e.tsMs; });
    const std::vector<AgentEvent> upto(events_.begin(), cut);

    struct Seen {
        fs::path     cwd;
        std::int64_t first     = 0;
        std::int64_t last      = 0;   // any line but SessionEnd
        std::int64_t ended     = 0;
        std::int64_t turn      = 0;
        std::int64_t stopped   = 0;
        std::unordered_set<std::string> agents;
        std::vector<const AgentEvent*>  tasks;
    };
    std::unordered_map<std::string, Seen> seen;

    for (const AgentEvent& ev : upto) {
        if (ev.sessionId.empty())
            continue;
        Seen& s = seen[ev.sessionId];
        if (s.first == 0)
            s.first = ev.tsMs;
        if (!ev.cwd.empty())
            s.cwd = ev.cwd;

        if (ev.event == "SessionEnd") {
            s.ended = ev.tsMs;
            continue;
        }
        s.last = ev.tsMs;
        if (ev.event == "Stop")
            s.stopped = ev.tsMs;
        else if (turnStart(ev))
            s.turn = ev.tsMs;
        if (!ev.agentId.empty())
            s.agents.insert(ev.agentId);
        if (ev.event == "PostToolUse" && !ev.task.empty())
            s.tasks.push_back(&ev);
    }

    std::vector<RegistryEntry> registry;
    std::vector<SessionFiles>  files;

    for (const auto& [id, s] : seen) {
        // A SessionStart after an end is a resume under the same id.
        if (s.ended >= s.last)
            continue;

        const auto live = liveNow_.find(id);
        if (live == liveNow_.end() && t - s.last > kIdlePresenceMs)
            continue;

        RegistryEntry e;
        e.sessionId   = id;
        e.cwd         = s.cwd;
        e.status      = s.turn > s.stopped ? "busy" : "idle";
        e.startedAtMs = s.first;
        e.updatedAtMs = s.last;
        if (live != liveNow_.end()) {
            e.pid  = live->second.pid;
            e.name = live->second.name;
            if (e.cwd.empty())
                e.cwd = live->second.cwd;
        }
        registry.push_back(std::move(e));

        SessionFiles f;
        f.sessionId = id;
        if (const auto known = identity_.find(id); known != identity_.end()) {
            for (const SubAgent& a : known->second) {
                if (s.agents.contains(a.id))
                    f.agents.push_back(a);
            }
        }
        for (const AgentEvent* ev : s.tasks) {
            BackgroundTask task;
            task.id           = ev->task;
            task.lastOutputMs = ev->tsMs;
            const auto end    = shellEnd_.find(id + "/" + ev->task);
            task.writing      = end != shellEnd_.end() && t < end->second;
            f.tasks.push_back(std::move(task));
        }
        files.push_back(std::move(f));
    }

    return buildBoard(registry, upto, files, {}, {});
}

std::vector<LoggedCall> ReplayTimeline::callsBetween(std::int64_t from, std::int64_t to) const
{
    std::vector<LoggedCall> out;
    auto it = std::upper_bound(events_.begin(), events_.end(), from,
                               [](std::int64_t v, const AgentEvent& e) { return v < e.tsMs; });
    for (; it != events_.end() && it->tsMs <= to; ++it) {
        if (it->event == "PreToolUse")
            out.push_back({it->tsMs, it->tool, it->text.empty() ? it->detail : it->text});
    }
    return out;
}

std::vector<int> ReplayTimeline::density(std::int64_t from, std::int64_t to, int bins) const
{
    std::vector<int> out(static_cast<std::size_t>(std::max(bins, 0)), 0);
    if (bins <= 0 || to <= from)
        return out;

    auto it = std::lower_bound(events_.begin(), events_.end(), from,
                               [](const AgentEvent& e, std::int64_t v) { return e.tsMs < v; });
    const double span = static_cast<double>(to - from);
    for (; it != events_.end() && it->tsMs < to; ++it) {
        const auto bin = static_cast<std::size_t>(static_cast<double>(it->tsMs - from) / span * bins);
        out[std::min(bin, out.size() - 1)]++;
    }
    return out;
}

} // namespace cx::gui

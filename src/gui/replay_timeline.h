#pragma once

#include "agent_board.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// The hook log read as a timeline, and the board as it stood at any point on it.
//
// events.jsonl is append-only and every line carries a timestamp, so the board
// at time T is the live fold run over the lines written up to T. That is what
// this does: it synthesizes the registry Claude Code would have shown at T from
// the log itself, and hands the result to the same buildBoard the live watcher
// uses, so a replayed board draws with exactly the same rules.
//
// What the log cannot say is simply absent. Task lists, plans, workflow
// progress and terminal output live in files that only hold their current
// state, and a replayed board drawn with today's task list would be a board
// that never existed.
namespace cx::gui {

// How far back the playhead can go.
inline constexpr std::int64_t kReplayWindowMs = 60 * 60 * 1000;

class ReplayTimeline {
public:
    // Reads the whole log once and the registry and session files once.
    // Everything after that is in memory.
    void load(std::int64_t nowMs);

    // Picks up lines appended since the last read. Cheap enough to call while
    // playing: it reads the file again but parses only what is newer.
    void refresh(std::int64_t nowMs);

    bool         empty() const { return events_.empty(); }
    std::int64_t startMs() const { return startMs_; }
    std::int64_t endMs() const { return endMs_; }

    AgentList boardAt(std::int64_t t) const;

    // The PreToolUse lines in (from, to], oldest first, for the tool log while
    // playing forward.
    std::vector<LoggedCall> callsBetween(std::int64_t from, std::int64_t to) const;

    // Event counts across [from, to) in `bins` equal slices.
    std::vector<int> density(std::int64_t from, std::int64_t to, int bins) const;

private:
    void absorb(std::vector<AgentEvent> fresh);
    void learnSession(const std::string& sessionId, const fs::path& cwd);
    void learnTask(const AgentEvent& ev);

    std::vector<AgentEvent> events_;   // ordered by tsMs
    std::int64_t            startMs_ = 0;
    std::int64_t            endMs_   = 0;

    // Sessions with a registry entry when replay began. Proof they were alive
    // for every instant since their first line, which the log alone cannot give
    // for a session that sat idle.
    std::unordered_map<std::string, RegistryEntry> liveNow_;

    // A subagent's type, description and parent, from its meta file. Identity
    // never changes, so today's copy is true of the past; state is dropped.
    std::unordered_map<std::string, std::vector<SubAgent>> identity_;
    std::unordered_set<std::string>                        learned_;
    SessionFileReader                                      reader_;

    // When each background shell stopped writing, keyed session/task. The
    // output file's last write is the only trace a shell leaves of its end.
    std::unordered_map<std::string, std::int64_t> shellEnd_;
};

} // namespace cx::gui

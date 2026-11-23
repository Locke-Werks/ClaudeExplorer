#include "conflict_edges.h"

#include "strutil.h"

#include <algorithm>
#include <system_error>
#include <unordered_map>
#include <utility>

namespace cx::gui {
namespace {

// The same spelling agent_board gives FileWrite::key, so a cwd and a written
// path compare as strings.
std::string normalize(const fs::path& p)
{
    std::string s = narrow(p.lexically_normal().wstring());
    std::replace(s.begin(), s.end(), '\\', '/');
    for (char& ch : s) {
        if (ch >= 'A' && ch <= 'Z')
            ch = static_cast<char>(ch - 'A' + 'a');
    }
    while (s.size() > 1 && s.back() == '/')
        s.pop_back();
    return s;
}

// The repository a written file belongs to: the nearest directory above it
// holding a .git, file or folder. A worktree has a .git file of its own, so
// two worktrees of one repository are two trees here, which is the point of
// having made them.
//
// Cached by directory, hits and misses alike. A refresh asks about the same
// few dozen directories every time, and a repository does not appear or
// vanish under a running session often enough to be worth a stat per refresh.
std::string repoRootOf(const std::string& fileKey)
{
    static std::unordered_map<std::string, std::string> cache;

    const std::size_t last = fileKey.find_last_of('/');
    std::string       dir  = last == std::string::npos ? std::string() : fileKey.substr(0, last);
    std::vector<std::string> walked;
    std::string              root;
    while (!dir.empty()) {
        if (const auto it = cache.find(dir); it != cache.end()) {
            root = it->second;
            break;
        }
        walked.push_back(dir);

        std::error_code ec;
        if (fs::exists(fs::path(widen(dir + "/.git")), ec)) {
            root = dir;
            break;
        }
        const std::size_t slash = dir.find_last_of('/');
        if (slash == std::string::npos || slash == 0)
            break;
        dir.resize(slash);
    }
    for (const std::string& d : walked)
        cache[d] = root;
    return root;
}

struct Recent {
    std::string sessionId;
    std::string cwd;
    // Only writes inside the window; everything here is about now.
    std::vector<const FileWrite*> writes;
    std::vector<std::string>      roots;
};

} // namespace

std::vector<ConflictEdge> findConflicts(const AgentList& board, std::int64_t nowMs,
                                        std::int64_t windowMs)
{
    std::vector<Recent> live;
    for (const AgentCard& card : board) {
        // The same rule the graph uses for drawing a session at all. An edge
        // to a disc that is not on the canvas goes nowhere.
        if (card.column == AgentColumn::Done || card.sessionId.empty())
            continue;

        Recent r;
        r.sessionId = card.sessionId;
        if (!card.cwd.empty())
            r.cwd = normalize(card.cwd);
        for (const FileWrite& w : card.writes) {
            if (nowMs - w.tsMs > windowMs)
                continue;
            r.writes.push_back(&w);

            // A tree with no git in it still has a shape: the session's own
            // working directory, when the file sits under it.
            std::string root = repoRootOf(w.key);
            if (root.empty() && !r.cwd.empty() && w.key.starts_with(r.cwd + '/'))
                root = r.cwd;
            if (!root.empty() && std::find(r.roots.begin(), r.roots.end(), root) == r.roots.end())
                r.roots.push_back(std::move(root));
        }
        if (!r.writes.empty())
            live.push_back(std::move(r));
    }

    std::vector<ConflictEdge> out;
    for (std::size_t i = 0; i < live.size(); ++i) {
        for (std::size_t j = i + 1; j < live.size(); ++j) {
            const Recent& x = live[i];
            const Recent& y = live[j];

            ConflictEdge e;
            e.a = std::min(x.sessionId, y.sessionId);
            e.b = std::max(x.sessionId, y.sessionId);

            // Shared files, newest first by whichever side wrote it last, so
            // the label names the file the collision is happening on now.
            std::vector<std::pair<std::int64_t, const FileWrite*>> shared;
            for (const FileWrite* wx : x.writes) {
                for (const FileWrite* wy : y.writes) {
                    if (wx->key == wy->key)
                        shared.emplace_back(std::max(wx->tsMs, wy->tsMs), wx);
                }
            }
            if (!shared.empty()) {
                std::sort(shared.begin(), shared.end(),
                          [](const auto& l, const auto& r) { return l.first > r.first; });
                e.kind = ConflictKind::Collision;
                e.name = shared.front().second->name;
                e.more = static_cast<int>(shared.size()) - 1;
                out.push_back(std::move(e));
                continue;
            }

            const bool sameTree = std::any_of(x.roots.begin(), x.roots.end(), [&](const std::string& r) {
                return std::find(y.roots.begin(), y.roots.end(), r) != y.roots.end();
            });
            if (sameTree) {
                e.kind = ConflictKind::SharedTree;
                out.push_back(std::move(e));
            }
        }
    }
    return out;
}

} // namespace cx::gui

#pragma once

#include "fs.h"

#include <cstdint>
#include <string>
#include <unordered_map>

// How full a session's context window is, read off the tail of its transcript.
//
// The transcript is the only place the number exists: every assistant line
// carries the usage block the API returned for that turn, and the most recent
// one is what the next request will be charged against. A long session's file
// runs to tens of megabytes, so only the end of it is ever read, and only when
// it has changed since the last look.
namespace cx::gui {

struct ContextUsage {
    std::int64_t tokens  = 0;   // input + cache read + cache creation, last turn
    std::int64_t limit   = 0;   // zero when no usage line was found
    std::int64_t writeMs = 0;   // the transcript's last write, Unix ms
};

class TranscriptUsageReader {
public:
    // Not thread-safe; the board watcher's worker is the only caller.
    ContextUsage read(const std::string& sessionId, const fs::path& cwd);

private:
    struct Entry {
        std::uintmax_t bytes   = 0;
        std::int64_t   writeMs = 0;
        ContextUsage   usage;
    };
    std::unordered_map<std::string, Entry> cache_;
};

} // namespace cx::gui

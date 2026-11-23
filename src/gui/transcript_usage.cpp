#include "transcript_usage.h"

#include "claude_files.h"
#include "json.h"
#include "strutil.h"

#include <string_view>
#include <system_error>

namespace cx::gui {
namespace {

// One assistant line is usually a few KB, but a turn that pasted a large tool
// result can be far longer, so a miss in the short window gets one wider look
// before giving up. Neither reads the whole file.
constexpr std::int64_t kTailBytes = 256 * 1024;
constexpr std::int64_t kWideTailBytes = 4 * 1024 * 1024;

constexpr std::int64_t kDefaultLimit = 200'000;
constexpr std::int64_t kLongLimit    = 1'000'000;

std::int64_t number(const json::Value* v)
{
    if (!v || v->type != json::Value::Type::Number)
        return 0;
    return v->isInteger ? v->integer : static_cast<std::int64_t>(v->number);
}

// The newest assistant line in `text` that carries usage, walking back from the
// end. Substring checks reject almost every line before the parser sees it.
bool lastUsage(std::string_view text, ContextUsage* out)
{
    std::size_t end = text.size();
    while (end > 0) {
        std::size_t start = text.rfind('\n', end - 1);
        start             = (start == std::string_view::npos) ? 0 : start + 1;
        const std::string_view line = text.substr(start, end - start);
        end = start > 0 ? start - 1 : 0;

        if (line.find("\"usage\"") == std::string_view::npos
            || line.find("\"assistant\"") == std::string_view::npos)
            continue;

        json::Value root;
        if (!json::parse(line, &root, nullptr))
            continue;
        const json::Value* type = root.find("type");
        if (!type || type->string != "assistant")
            continue;
        const json::Value* message = root.find("message");
        const json::Value* usage   = message ? message->find("usage") : nullptr;
        if (!usage || usage->type != json::Value::Type::Object)
            continue;

        const std::int64_t tokens = number(usage->find("input_tokens"))
                                  + number(usage->find("cache_read_input_tokens"))
                                  + number(usage->find("cache_creation_input_tokens"));
        // A synthetic line with an empty usage block says nothing about the
        // window; keep walking back to a real one.
        if (tokens <= 0)
            continue;

        const json::Value* model = message->find("model");
        const bool         wide  = model && model->string.find("[1m]") != std::string::npos;

        out->tokens = tokens;
        out->limit  = (wide || tokens > kDefaultLimit) ? kLongLimit : kDefaultLimit;
        return true;
    }
    return false;
}

} // namespace

ContextUsage TranscriptUsageReader::read(const std::string& sessionId, const fs::path& cwd)
{
    if (sessionId.empty() || cwd.empty())
        return {};

    const fs::path home = cf::claudeHome();
    if (home.empty())
        return {};

    const fs::path file = home / "projects" / widen(cf::projectSlug(cwd))
                        / widen(sessionId + ".jsonl");

    std::error_code      ec;
    const std::uintmax_t bytes = fs::file_size(file, ec);
    if (ec) {
        cache_.erase(sessionId);
        return {};
    }
    const std::int64_t writeMs = cf::fileWriteTimeMs(file);

    Entry& e = cache_[sessionId];
    if (e.bytes == bytes && e.writeMs == writeMs && bytes > 0)
        return e.usage;

    ContextUsage usage;
    usage.writeMs = writeMs;
    if (!lastUsage(cf::readShared(file, kTailBytes), &usage)
        && static_cast<std::int64_t>(bytes) > kTailBytes)
        lastUsage(cf::readShared(file, kWideTailBytes), &usage);

    e.bytes   = bytes;
    e.writeMs = writeMs;
    e.usage   = usage;

    // Bounded by the live sessions that have ever been read; a stale entry is
    // a few dozen bytes and is dropped the moment its file goes.
    return usage;
}

} // namespace cx::gui

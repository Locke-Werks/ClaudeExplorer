#include "board_watcher.h"

#include <QDateTime>

#include <windows.h>

#include <system_error>

namespace cx::gui {
namespace {

// Rebuild this often whatever the directories say. ReadDirectoryChangesW drops
// changes when its buffer overflows, and there is no way of knowing that
// happened: an empty canvas and a wedged watcher look exactly the same.
constexpr DWORD kPollMs = 5000;

// One action arrives as a burst. The registry file is rewritten, the hook
// appends, and a turn writes several events in a row, so the wait is given a
// moment to go quiet and the whole burst costs one rebuild.
constexpr DWORD kSettleMs = 150;

constexpr DWORD kWatchFlags =
    FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE;

// The kernel writes FILE_NOTIFY_INFORMATION records here and nothing ever reads
// them, but the buffer still has to exist and still has to be DWORD aligned,
// which is why it is declared as an array of DWORD rather than of bytes.
constexpr DWORD kNotifyBytes = 8192;

// Cancel is first because WaitForMultipleObjects reports the lowest signalled
// handle: behind two churning directories, anything further along would be
// reached late or not at all.
constexpr DWORD kCancelSlot = 0;
constexpr DWORD kWakeSlot   = 1;

// One watched directory: the handle, the event the wait sleeps on, and the
// buffer the kernel fills. Non-copyable, because the OVERLAPPED holds a pointer
// into this object for as long as a read is in flight.
class DirWatch {
public:
    DirWatch() = default;
    ~DirWatch() { close(); }

    DirWatch(const DirWatch&)            = delete;
    DirWatch& operator=(const DirWatch&) = delete;

    bool open(const fs::path& dir)
    {
        dir_ = CreateFileW(dir.wstring().c_str(), FILE_LIST_DIRECTORY,
                           FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                           OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
                           nullptr);
        if (dir_ == INVALID_HANDLE_VALUE) {
            dir_ = nullptr;
            return false;
        }

        // Manual reset, so a completion that lands while the wait is off
        // rebuilding is still signalled when the wait comes back.
        ov_.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!ov_.hEvent)
            return false;

        arm();
        return true;
    }

    bool   valid() const { return dir_ != nullptr && ov_.hEvent != nullptr; }
    HANDLE event() const { return ov_.hEvent; }

    // Completes the read that just fired and starts the next one.
    void rearm()
    {
        DWORD bytes = 0;
        GetOverlappedResult(dir_, &ov_, &bytes, FALSE);
        arm();
    }

    void close()
    {
        if (dir_) {
            // Drain the read in flight before the buffer goes. It is a member of
            // this object and the kernel still holds a pointer to it.
            CancelIo(dir_);
            DWORD bytes = 0;
            GetOverlappedResult(dir_, &ov_, &bytes, TRUE);
            CloseHandle(dir_);
            dir_ = nullptr;
        }
        if (ov_.hEvent) {
            CloseHandle(ov_.hEvent);
            ov_.hEvent = nullptr;
        }
    }

private:
    // A failed arm leaves the event reset rather than signalled, so the wait
    // falls back to its timeout instead of spinning on a handle nothing will
    // ever complete.
    void arm()
    {
        ResetEvent(ov_.hEvent);
        ReadDirectoryChangesW(dir_, buffer_, kNotifyBytes, FALSE, kWatchFlags, nullptr, &ov_,
                              nullptr);
    }

    HANDLE     dir_ = nullptr;
    OVERLAPPED ov_{};
    DWORD      buffer_[kNotifyBytes / sizeof(DWORD)]{};
};

} // namespace

BoardWatcher::BoardWatcher(QObject* parent) : QObject(parent)
{
    // Manual reset on both: a signal raised while the worker is between waits
    // has to still be there when it comes back.
    cancel_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    wake_   = CreateEventW(nullptr, TRUE, FALSE, nullptr);
}

BoardWatcher::~BoardWatcher()
{
    shutdown();
    if (cancel_)
        CloseHandle(static_cast<HANDLE>(cancel_));
    if (wake_)
        CloseHandle(static_cast<HANDLE>(wake_));
}

void BoardWatcher::start()
{
    shutdown();
    if (cancel_)
        ResetEvent(static_cast<HANDLE>(cancel_));
    worker_ = std::thread(&BoardWatcher::run, this);
}

void BoardWatcher::shutdown()
{
    if (cancel_)
        SetEvent(static_cast<HANDLE>(cancel_));
    if (worker_.joinable())
        worker_.join();
}

void BoardWatcher::run()
{
    const fs::path sessionsDir = sessionRegistryDir();
    const fs::path log         = eventLogPath();
    const fs::path agentsDir   = log.empty() ? fs::path() : log.parent_path();

    // cxhook creates this on its first write, which on a machine where the
    // hooks were declined is never. CreateFileW cannot open a directory that is
    // not there, and a watch that failed to open is a canvas that only ever
    // updates on the poll.
    if (!agentsDir.empty()) {
        std::error_code ec;
        fs::create_directories(agentsDir, ec);
    }

    DirWatch watch[2];
    if (!sessionsDir.empty())
        watch[0].open(sessionsDir);
    if (!agentsDir.empty())
        watch[1].open(agentsDir);

    HANDLE    handles[4] = {};
    DirWatch* source[4]  = {};
    DWORD     count      = 0;
    handles[count++]     = static_cast<HANDLE>(cancel_);
    handles[count++]     = static_cast<HANDLE>(wake_);
    for (DirWatch& w : watch) {
        if (!w.valid())
            continue;
        source[count]  = &w;
        handles[count] = w.event();
        ++count;
    }

    const auto consume = [&](DWORD slot) {
        if (slot == kWakeSlot) {
            ResetEvent(static_cast<HANDLE>(wake_));
            return;
        }
        if (DirWatch* w = source[slot])
            w->rearm();
    };

    buildAndPost();

    bool stop = false;
    while (!stop) {
        const DWORD fired = WaitForMultipleObjects(count, handles, FALSE, kPollMs);
        if (fired == WAIT_TIMEOUT) {
            buildAndPost();
            continue;
        }
        // WAIT_OBJECT_0 is zero, so the return value is the index of the handle
        // that fired. Anything at or past `count` is WAIT_FAILED, which there
        // is no recovering from here.
        if (fired == kCancelSlot || fired >= count)
            break;

        consume(fired);

        for (;;) {
            const DWORD again = WaitForMultipleObjects(count, handles, FALSE, kSettleMs);
            if (again == WAIT_TIMEOUT)
                break;
            if (again == kCancelSlot || again >= count) {
                stop = true;
                break;
            }
            consume(again);
        }

        if (!stop)
            buildAndPost();
    }
}

void BoardWatcher::buildAndPost()
{
    // Every read and the fold run here rather than on the GUI thread. They open
    // and parse files, which is the whole reason this thread exists.
    const std::vector<RegistryEntry> registry = readRegistry(sessionRegistryDir());
    const std::vector<AgentEvent>    events   = readEvents(eventLogPath());

    // Live sessions only. A session directory outlives its session by design,
    // so reading every one of them would be reading the whole history of the
    // machine to describe work that finished weeks ago.
    // Background jobs are read first because they name sessions the registry
    // does not, and those sessions have subagents, shells and task lists of
    // their own that would otherwise go unread.
    const std::vector<BackgroundJob> jobs = readJobs();

    std::vector<SessionFiles> files;
    files.reserve(registry.size() + jobs.size());

    // One clock for the whole rebuild: a monitor is judged against its own
    // deadline, and two sessions read a moment apart would be a view that
    // disagrees with itself about what time it is.
    const std::int64_t now = QDateTime::currentMSecsSinceEpoch();

    const auto readSession = [&](const std::string& sessionId, const fs::path& cwd) {
        SessionFiles f = sessions_.read(sessionId, cwd);
        f.tasks        = tasks_.read(sessionId, cwd, now);
        f.todos        = readTodos(sessionId);
        files.push_back(std::move(f));
    };

    for (const RegistryEntry& e : registry) {
        if (!e.sessionId.empty())
            readSession(e.sessionId, e.cwd);
    }
    for (const BackgroundJob& job : jobs) {
        if (job.live && !job.sessionId.empty())
            readSession(job.sessionId, job.cwd);
    }

    AgentList board = buildBoard(registry, events, files, jobs, readPlans(kPlanWindowMs));

    // Emitted from inside the posted lambda, so the signal runs on the GUI
    // thread as a direct call and the list never crosses as a queued argument.
    QMetaObject::invokeMethod(
        this, [this, board = std::move(board)] { emit boardReady(board); },
        Qt::QueuedConnection);
}

} // namespace cx::gui

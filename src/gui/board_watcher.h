#pragma once

#include "agent_board.h"

#include <QObject>

#include <thread>

// Reads what is running, off the GUI thread, and hands the result over.
//
// Not a poll loop with a timer on it. Two ReadDirectoryChangesW watches do the
// waking, one on Claude Code's session registry and one on the directory the
// hook appends to, with a 5s timeout behind them as a ceiling rather than as
// the driver. That is what makes a session appearing on screen feel immediate
// while a machine with nothing happening costs nothing at all.
//
// One rebuild reads a dozen directories and parses everything in them, which is
// the whole reason this is not on the GUI thread: a canvas that stuttered every
// five seconds while it stat'd a temp tree would be worse than no canvas.
namespace cx::gui {

class BoardWatcher : public QObject {
    Q_OBJECT

public:
    explicit BoardWatcher(QObject* parent = nullptr);
    ~BoardWatcher() override;

    // Starts the worker, or restarts it. Safe to call on a running watcher.
    void start();

    // Signals the worker and joins it. Called by the destructor, so nothing can
    // post after teardown has begun.
    void shutdown();

signals:
    // Emitted on the GUI thread. By reference on purpose: the list is moved
    // into the posted lambda and emitted from inside it, so it never crosses
    // the thread boundary as a copied queued argument.
    void boardReady(const cx::gui::AgentList& board);

private:
    void run();
    void buildAndPost();

    std::thread worker_;

    // Win32 HANDLEs, held as void* so windows.h stays out of a header the rest
    // of the GUI includes.
    void* cancel_ = nullptr;
    void* wake_   = nullptr;

    // Neither is thread-safe, and neither needs to be: only the worker touches
    // them, and there is exactly one worker.
    SessionFileReader sessions_;
    TaskReader        tasks_;
};

} // namespace cx::gui

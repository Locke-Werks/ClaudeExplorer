#pragma once

#include "agent_board.h"
#include "replay_timeline.h"

#include <QElapsedTimer>
#include <QObject>

#include <cstdint>
#include <vector>

class QKeyEvent;
class QTimer;
class QWidget;

// The keys that move the playhead, and the board they produce.
//
// Keyboard only, like the rest of the window:
//
//   [  ]        step back / forward 30s, faster the longer the key is held
//   P or \      play / pause at 20x
//   L or Bksp   back to live
//
// While replaying, ExplorerWindow keeps the live watcher running and keeps the
// latest board it produced, but hands the canvas this object's boards instead.
namespace cx::gui {

class ReplayHud;

class ReplayController : public QObject {
    Q_OBJECT

public:
    // `overlay` is the widget the timeline bar is laid over the foot of.
    explicit ReplayController(QWidget* overlay);

    bool active() const { return active_; }

    // True when the key was one of replay's.
    bool handleKey(QKeyEvent* e);

signals:
    void boardReady(const cx::gui::AgentList& board);
    void toolCallsLogged(const std::vector<cx::gui::LoggedCall>& calls);
    void returnedLive();

private:
    void enter(std::int64_t t);
    void seek(std::int64_t t);
    void step(int direction, bool held);
    void togglePlay();
    void goLive();
    void onPlayTick();
    void publishRange();

    ReplayTimeline timeline_;
    ReplayHud*     hud_  = nullptr;
    QTimer*        play_ = nullptr;
    QElapsedTimer  playClock_;
    QElapsedTimer  sinceRefresh_;

    bool         active_  = false;
    bool         playing_ = false;
    std::int64_t t_       = 0;
    int          held_    = 0;   // consecutive auto-repeats of a step key
};

} // namespace cx::gui

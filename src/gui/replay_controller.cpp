#include "replay_controller.h"

#include "replay_clock.h"
#include "replay_hud.h"

#include <QDateTime>
#include <QKeyEvent>
#include <QTimer>

#include <algorithm>

namespace cx::gui {
namespace {

constexpr std::int64_t kStepMs   = 30 * 1000;
constexpr int          kMaxStepX = 20;   // ten minutes a press at full repeat
constexpr int          kRepeatsPerX = 3;
constexpr int          kSpeed    = 20;

// Ten boards a second while playing. Each one is a full fold of the log up to
// the playhead, and the graph animates between them, so more would cost the
// fold without moving anything faster.
constexpr int kPlayTickMs = 100;

// While playing, the log is re-read this often for lines written since replay
// began, so playing up to the present arrives at the present.
constexpr qint64 kRefreshMs = 2000;

constexpr int kDensityBins = 160;

std::int64_t wallMs()
{
    return QDateTime::currentMSecsSinceEpoch();
}

} // namespace

ReplayController::ReplayController(QWidget* overlay) : QObject(overlay)
{
    hud_ = new ReplayHud(overlay);

    play_ = new QTimer(this);
    play_->setInterval(kPlayTickMs);
    play_->setTimerType(Qt::CoarseTimer);
    connect(play_, &QTimer::timeout, this, &ReplayController::onPlayTick);
}

bool ReplayController::handleKey(QKeyEvent* e)
{
    switch (e->key()) {
    case Qt::Key_BracketLeft:
        step(-1, e->isAutoRepeat());
        return true;
    case Qt::Key_BracketRight:
        step(+1, e->isAutoRepeat());
        return true;
    case Qt::Key_P:
    case Qt::Key_Backslash:
        if (!e->isAutoRepeat())
            togglePlay();
        return true;
    case Qt::Key_L:
    case Qt::Key_Backspace:
        if (active_)
            goLive();
        return true;
    default:
        return false;
    }
}

void ReplayController::publishRange()
{
    hud_->setRange(timeline_.startMs(), timeline_.endMs(),
                   timeline_.density(timeline_.startMs(), timeline_.endMs(), kDensityBins));
}

void ReplayController::enter(std::int64_t t)
{
    const std::int64_t now = wallMs();
    timeline_.load(now);
    sinceRefresh_.start();
    active_ = true;
    publishRange();
    hud_->setActive(true);
    seek(t);
}

void ReplayController::seek(std::int64_t t)
{
    t_ = std::clamp(t, timeline_.startMs(), timeline_.endMs());

    // Before the board goes out: the graph reads the clock while it folds.
    setBoardClockOverride(t_);
    emit boardReady(timeline_.boardAt(t_));
    hud_->setPlayhead(t_, playing_, kSpeed);
}

void ReplayController::step(int direction, bool held)
{
    held_ = held ? held_ + 1 : 0;
    const std::int64_t delta = kStepMs * std::min(1 + held_ / kRepeatsPerX, kMaxStepX);

    if (!active_) {
        // Forward from live is already the present.
        if (direction < 0)
            enter(wallMs() - delta);
        return;
    }

    const std::int64_t to = t_ + direction * delta;
    if (direction > 0 && to >= timeline_.endMs()) {
        goLive();
        return;
    }
    seek(to);
}

void ReplayController::togglePlay()
{
    if (!active_) {
        // From live there is nothing ahead to play, so play the last hour.
        playing_ = true;
        enter(0);
    } else {
        playing_ = !playing_;
        // Play pressed at the end of the track starts it over rather than
        // stepping straight back to live.
        if (playing_ && t_ >= timeline_.endMs() - kStepMs)
            seek(timeline_.startMs());
    }

    if (playing_) {
        playClock_.start();
        play_->start();
    } else {
        play_->stop();
    }
    hud_->setPlayhead(t_, playing_, kSpeed);
}

void ReplayController::onPlayTick()
{
    const std::int64_t dt = playClock_.restart() * kSpeed;

    if (sinceRefresh_.elapsed() >= kRefreshMs) {
        timeline_.refresh(wallMs());
        sinceRefresh_.restart();
        publishRange();
    }

    const std::int64_t from = t_;
    const std::int64_t to   = t_ + dt;
    if (to >= timeline_.endMs()) {
        goLive();
        return;
    }
    seek(to);

    if (std::vector<LoggedCall> calls = timeline_.callsBetween(from, t_); !calls.empty())
        emit toolCallsLogged(calls);
}

void ReplayController::goLive()
{
    play_->stop();
    playing_ = false;
    active_  = false;
    held_    = 0;
    setBoardClockOverride(0);
    hud_->setActive(false);
    emit returnedLive();
}

} // namespace cx::gui

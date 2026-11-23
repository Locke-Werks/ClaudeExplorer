#pragma once

#include <QElapsedTimer>
#include <QWidget>

#include <cstdint>
#include <vector>

class QTimer;

// The timeline bar along the bottom edge while replaying, and nothing when live.
//
// Its own widget laid over the canvas rather than a branch of the canvas's
// paint, so the canvas knows nothing about replay and a still bar costs the
// same as a still canvas: nothing. It repaints when the playhead moves and
// while it fades, and at no other time.
namespace cx::gui {

class ReplayHud : public QWidget {
    Q_OBJECT

public:
    explicit ReplayHud(QWidget* parent);

    // Fades in or out. Hidden outright once faded out, so a live canvas has no
    // overlay at all.
    void setActive(bool on);

    void setRange(std::int64_t startMs, std::int64_t endMs, std::vector<int> density);
    void setPlayhead(std::int64_t ms, bool playing, int speed);

protected:
    void paintEvent(QPaintEvent*) override;
    bool eventFilter(QObject* watched, QEvent* e) override;

private:
    void onFade();
    void fit();

    std::int64_t     startMs_ = 0;
    std::int64_t     endMs_   = 0;
    std::int64_t     t_       = 0;
    bool             playing_ = false;
    int              speed_   = 1;
    std::vector<int> density_;
    int              densityMax_ = 0;

    bool          target_   = false;
    qreal         progress_ = 0;   // 0 hidden, 1 shown, before easing
    QTimer*       fade_     = nullptr;
    QElapsedTimer fadeClock_;
};

} // namespace cx::gui

// The canvas's transient effects: the ripple off a session that just started
// waiting, and the brief notice a key toggle leaves on screen. Kept out of
// node_explorer_panel.cpp because neither is part of drawing the graph, and
// both share one short-lived timer that must not exist while nothing plays.

#include "node_explorer_panel.h"

#include "theme_qt.h"

#include <QPainter>
#include <QTimer>

#include <algorithm>
#include <cmath>

namespace cx::gui {
namespace {

using namespace cx::theme;

// Long enough to be caught in peripheral vision, short enough to be over
// before it reads as an animation someone has to wait for.
constexpr qint64 kRippleMs = 900;

// How far past the disc the ring travels, in screen pixels. Fixed rather than
// scaled with the camera so a zoomed-out canvas still gets a visible pulse.
constexpr qreal kRippleReach = 44.0;

constexpr qint64 kFlashHoldMs = 1200;
constexpr qint64 kFlashFadeMs = 500;

// Ticks only while something plays, and sixty a second is what a ring growing
// across forty pixels needs to look continuous.
constexpr int kFxTimerMs = 16;

qreal easeOutCubic(qreal t)
{
    const qreal u = 1.0 - t;
    return 1.0 - u * u * u;
}

// A session's node is "S" + separator + id. Matched on the tail so this file
// does not need to know the separator node_graph.cpp keeps private.
bool isSessionNode(const Node& n, const std::string& sessionId)
{
    return n.kind == NodeKind::Session && n.key.size() > sessionId.size()
        && n.key.compare(n.key.size() - sessionId.size(), sessionId.size(), sessionId) == 0;
}

} // namespace

void NodeExplorerPanel::startFx()
{
    if (!fxTick_) {
        fxTick_ = new QTimer(this);
        fxTick_->setInterval(kFxTimerMs);
        fxTick_->setTimerType(Qt::PreciseTimer);
        connect(fxTick_, &QTimer::timeout, this, &NodeExplorerPanel::onFxTick);
        fxClock_.start();
    }
    if (!fxTick_->isActive())
        fxTick_->start();
    update();
}

void NodeExplorerPanel::ripple(const std::string& sessionId)
{
    // Nobody is looking at a hidden canvas, and a ripple queued now would play
    // stale on the next show.
    if (!isVisible())
        return;
    startFx();
    ripples_.push_back({sessionId, fxClock_.elapsed()});
}

void NodeExplorerPanel::flash(const QString& text)
{
    startFx();
    flashText_   = text;
    flashBornMs_ = fxClock_.elapsed();
}

void NodeExplorerPanel::onFxTick()
{
    const qint64 now = fxClock_.elapsed();
    std::erase_if(ripples_, [now](const Ripple& r) { return now - r.bornMs >= kRippleMs; });
    if (!flashText_.isEmpty() && now - flashBornMs_ >= kFlashHoldMs + kFlashFadeMs)
        flashText_.clear();

    // One last repaint to wipe the final frame, then nothing: a still canvas
    // must cost nothing.
    update();
    if (ripples_.empty() && flashText_.isEmpty())
        fxTick_->stop();
}

void NodeExplorerPanel::drawEffects(QPainter& p) const
{
    if (ripples_.empty() && flashText_.isEmpty())
        return;

    const qint64 now  = fxClock_.elapsed();
    const QSizeF view = QSizeF(size());

    p.save();
    p.setBrush(Qt::NoBrush);
    for (const Ripple& r : ripples_) {
        const qreal t = std::clamp(static_cast<qreal>(now - r.bornMs) / kRippleMs, 0.0, 1.0);
        for (const Node& n : graph_.nodes()) {
            if (!isSessionNode(n, r.sessionId))
                continue;
            const QPointF c    = camera_.toScreen(n.pos, view);
            const qreal   base = n.radius * n.popScale * camera_.scale();
            // Starts just outside the outer waiting ring, so it reads as the
            // rings throwing off a wave rather than a second disc swelling.
            const qreal radius = base + 5.0 + kRippleReach * easeOutCubic(t);
            const qreal fade   = (1.0 - t) * (1.0 - t);
            QColor      ink    = theme::c(kRed);
            ink.setAlphaF(static_cast<float>(0.85 * fade));
            p.setPen(QPen(ink, 1.0 + 0.6 * (1.0 - t)));
            p.drawEllipse(c, radius, radius);
            break;
        }
    }
    p.restore();

    if (!flashText_.isEmpty()) {
        const qint64 age = now - flashBornMs_;
        qreal        a   = 1.0;
        if (age > kFlashHoldMs)
            a = 1.0 - std::min(1.0, static_cast<qreal>(age - kFlashHoldMs) / kFlashFadeMs);
        p.save();
        p.setOpacity(a);
        // Below centre, so it does not land on "Nothing running." and is clear
        // of whatever the camera has framed in the middle.
        p.translate(0, static_cast<qreal>(height()) * 0.32);
        drawNotice(p, flashText_);
        p.restore();
    }
}

} // namespace cx::gui

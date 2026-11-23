#include "replay_hud.h"

#include "theme_qt.h"

#include <QDateTime>
#include <QEvent>
#include <QFontMetricsF>
#include <QPainter>
#include <QTimer>

#include <algorithm>
#include <cmath>

namespace cx::gui {
namespace {

using namespace cx::theme;

constexpr int    kStripPx   = 64;     // the widget's height; the bar sits in its foot
constexpr qreal  kFootPx    = 22.0;   // track above the bottom edge
constexpr qreal  kSpanFrac  = 0.40;   // of the width, centred, clear of counts and key
constexpr qreal  kMinSpanPx = 320.0;
constexpr qreal  kDensityPx = 10.0;
constexpr qreal  kFadeMs    = 220.0;
constexpr int    kFadeTickMs = 16;
constexpr std::int64_t kTickEveryMs = 5 * 60 * 1000;

QColor alpha(QColor c, qreal a)
{
    c.setAlphaF(std::clamp(a, 0.0, 1.0));
    return c;
}

// Smoothstep: a fade that starts and lands softly, so the bar arriving reads as
// a mode changing rather than as a widget popping.
qreal ease(qreal x)
{
    x = std::clamp(x, 0.0, 1.0);
    return x * x * (3.0 - 2.0 * x);
}

QString offsetText(std::int64_t behindMs)
{
    const std::int64_t s = std::max<std::int64_t>(0, behindMs / 1000);
    if (s < 60)
        return QStringLiteral("-%1s").arg(s);
    return QStringLiteral("-%1m").arg(s / 60);
}

} // namespace

ReplayHud::ReplayHud(QWidget* parent) : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    setFocusPolicy(Qt::NoFocus);
    hide();

    fade_ = new QTimer(this);
    fade_->setInterval(kFadeTickMs);
    fade_->setTimerType(Qt::CoarseTimer);
    connect(fade_, &QTimer::timeout, this, &ReplayHud::onFade);

    parent->installEventFilter(this);
    fit();
}

void ReplayHud::fit()
{
    const QWidget* p = parentWidget();
    setGeometry(0, p->height() - kStripPx, p->width(), kStripPx);
}

bool ReplayHud::eventFilter(QObject* watched, QEvent* e)
{
    if (watched == parentWidget() && e->type() == QEvent::Resize)
        fit();
    return QWidget::eventFilter(watched, e);
}

void ReplayHud::setActive(bool on)
{
    if (on == target_)
        return;
    target_ = on;
    if (on) {
        fit();
        show();
        raise();
    }
    fadeClock_.restart();
    if (!fade_->isActive())
        fade_->start();
}

void ReplayHud::onFade()
{
    const qreal step = static_cast<qreal>(fadeClock_.restart()) / kFadeMs;
    progress_        = std::clamp(progress_ + (target_ ? step : -step), 0.0, 1.0);

    if (progress_ <= 0.0 || progress_ >= 1.0)
        fade_->stop();
    if (progress_ <= 0.0)
        hide();
    else
        update();
}

void ReplayHud::setRange(std::int64_t startMs, std::int64_t endMs, std::vector<int> density)
{
    startMs_    = startMs;
    endMs_      = endMs;
    density_    = std::move(density);
    densityMax_ = density_.empty() ? 0 : *std::max_element(density_.begin(), density_.end());
    update();
}

void ReplayHud::setPlayhead(std::int64_t ms, bool playing, int speed)
{
    if (ms == t_ && playing == playing_ && speed == speed_)
        return;
    t_       = ms;
    playing_ = playing;
    speed_   = speed;
    update();
}

void ReplayHud::paintEvent(QPaintEvent*)
{
    const qreal a = ease(progress_);
    if (a <= 0.0 || endMs_ <= startMs_)
        return;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const qreal w    = static_cast<qreal>(width());
    const qreal span = std::min(w - 2.0 * kFootPx, std::max(kMinSpanPx, w * kSpanFrac));
    const qreal x0   = (w - span) * 0.5;
    const qreal x1   = x0 + span;
    const qreal y    = static_cast<qreal>(height()) - kFootPx;
    const qreal len  = static_cast<qreal>(endMs_ - startMs_);

    const auto xAt = [&](std::int64_t ms) {
        return x0 + span * std::clamp(static_cast<qreal>(ms - startMs_) / len, 0.0, 1.0);
    };

    // Density first, faint, so the track and the playhead read over it.
    if (densityMax_ > 0) {
        const qreal barW = span / static_cast<qreal>(density_.size());
        p.setPen(Qt::NoPen);
        p.setBrush(alpha(theme::c(kFg4), 0.9 * a));
        for (std::size_t i = 0; i < density_.size(); ++i) {
            if (density_[i] == 0)
                continue;
            // Square root, so one burst of a hundred tool calls does not flatten
            // every quieter minute into nothing.
            const qreal h = std::max(1.0, kDensityPx * std::sqrt(static_cast<qreal>(density_[i])
                                                                 / densityMax_));
            p.drawRect(QRectF(x0 + barW * static_cast<qreal>(i), y - 2.0 - h,
                              std::max(1.0, barW - 1.0), h));
        }
    }

    // Track: dim ahead of the playhead, brighter behind it.
    const qreal xp = xAt(t_);
    p.setPen(QPen(alpha(theme::c(kBorderHi), a), 1.0));
    p.drawLine(QPointF(xp, y), QPointF(x1, y));
    p.setPen(QPen(alpha(theme::c(kFg3), a), 1.0));
    p.drawLine(QPointF(x0, y), QPointF(xp, y));

    // Ticks on the wall-clock five minutes, taller on the hour.
    p.setPen(QPen(alpha(theme::c(kFg4), a), 1.0));
    for (std::int64_t tick = (startMs_ / kTickEveryMs + 1) * kTickEveryMs; tick < endMs_;
         tick += kTickEveryMs) {
        const bool  hour = QDateTime::fromMSecsSinceEpoch(tick).time().minute() == 0;
        const qreal x    = xAt(tick);
        p.drawLine(QPointF(x, y + 2.0), QPointF(x, y + (hour ? 7.0 : 4.0)));
    }

    // Playhead.
    p.setPen(QPen(alpha(theme::c(kFg1), a), 1.0));
    p.drawLine(QPointF(xp, y - 7.0), QPointF(xp, y + 7.0));
    p.setPen(Qt::NoPen);
    p.setBrush(alpha(theme::c(kFg1), a));
    p.drawEllipse(QPointF(xp, y), 2.5, 2.5);

    // Tag and timestamp above the track's left end.
    const QFont tagFont = theme::tracked(9, QFont::Bold, 0.2);
    const QFont txtFont = theme::tracked(10, QFont::DemiBold, 0.14);
    const qreal base    = y - kDensityPx - 9.0;

    const QString tag = QStringLiteral("REPLAY");
    p.setFont(tagFont);
    p.setPen(alpha(theme::c(kRed), a));
    p.drawText(QPointF(x0, base), tag);

    const QString text = QStringLiteral("%1   %2   %3")
                             .arg(QDateTime::fromMSecsSinceEpoch(t_).toString(QStringLiteral("HH:mm:ss")),
                                  offsetText(endMs_ - t_),
                                  playing_ ? QStringLiteral("%1x").arg(speed_)
                                           : QStringLiteral("paused"))
                             .toUpper();
    p.setFont(txtFont);
    p.setPen(alpha(theme::c(kFg2), a));
    p.drawText(QPointF(x0 + QFontMetricsF(tagFont).horizontalAdvance(tag) + 12.0, base), text);
}

} // namespace cx::gui

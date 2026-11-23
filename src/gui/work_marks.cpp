#include "work_marks.h"

#include "theme_qt.h"

#include <QPainter>
#include <QPainterPath>
#include <QtMath>

#include <algorithm>
#include <cmath>

namespace cx::gui {
namespace {

using namespace cx::theme;

struct ToolFamily {
    std::string_view name;
    ToolGlyph        glyph;
};

// Exact names. MCP tools are matched by prefix below, since their names are
// whatever the server chose.
constexpr ToolFamily kFamilies[] = {
    { "Bash",         ToolGlyph::Shell  },
    { "BashOutput",   ToolGlyph::Shell  },
    { "KillShell",    ToolGlyph::Shell  },
    { "PowerShell",   ToolGlyph::Shell  },
    { "Edit",         ToolGlyph::Edit   },
    { "MultiEdit",    ToolGlyph::Edit   },
    { "Write",        ToolGlyph::Edit   },
    { "NotebookEdit", ToolGlyph::Edit   },
    { "Read",         ToolGlyph::Read   },
    { "Grep",         ToolGlyph::Search },
    { "Glob",         ToolGlyph::Search },
    { "WebFetch",     ToolGlyph::Web    },
    { "WebSearch",    ToolGlyph::Web    },
    { "Agent",        ToolGlyph::Agent  },
    { "Task",         ToolGlyph::Agent  },
};

QColor faded(QColor c, qreal a)
{
    c.setAlpha(std::clamp(qRound(static_cast<qreal>(c.alpha()) * a), 0, 255));
    return c;
}

// Qt angles are sixteenths of a degree, counter-clockwise from three o'clock.
// The ring runs clockwise from twelve, so every angle here is degrees clockwise
// from twelve and converted at the last moment.
void arcCw(QPainter& p, const QRectF& rect, qreal fromDeg, qreal spanDeg)
{
    if (spanDeg <= 0.01)
        return;
    p.drawArc(rect, qRound((90.0 - fromDeg) * 16.0), -qRound(spanDeg * 16.0));
}

} // namespace

ToolGlyph toolGlyphFor(std::string_view tool)
{
    if (tool.substr(0, 5) == "mcp__")
        return ToolGlyph::Mcp;
    for (const ToolFamily& f : kFamilies) {
        if (f.name == tool)
            return f.glyph;
    }
    return ToolGlyph::Other;
}

void drawToolGlyph(QPainter& p, ToolGlyph g, const QRectF& box, const QColor& colour)
{
    // Drawn in a unit square from -1 to 1 and mapped onto the box. The stroke
    // is set in device pixels rather than units, so it stays a hairline however
    // far the camera zooms in instead of thickening into a cartoon.
    const qreal half = box.width() * 0.5;
    const qreal pen  = std::clamp(half * 0.16, 1.0, 1.6);

    p.save();
    p.translate(box.center());
    p.scale(half, half);

    QPen stroke(colour, pen / half);
    stroke.setCapStyle(Qt::RoundCap);
    stroke.setJoinStyle(Qt::RoundJoin);
    p.setPen(stroke);
    p.setBrush(Qt::NoBrush);

    QPainterPath path;
    switch (g) {
    case ToolGlyph::Shell:
        // A prompt: chevron and cursor.
        path.moveTo(-0.70, -0.50);
        path.lineTo(-0.15, 0.00);
        path.lineTo(-0.70, 0.50);
        path.moveTo(0.05, 0.55);
        path.lineTo(0.70, 0.55);
        p.drawPath(path);
        break;
    case ToolGlyph::Edit:
        // A pencil laid on the line it is writing.
        path.moveTo(-0.62, 0.42);
        path.lineTo(0.30, -0.50);
        path.lineTo(0.55, -0.25);
        path.lineTo(-0.37, 0.67);
        path.lineTo(-0.68, 0.73);
        path.closeSubpath();
        p.drawPath(path);
        break;
    case ToolGlyph::Read: {
        // An eye: a lens and a pupil.
        path.moveTo(-0.85, 0.0);
        path.quadTo(0.0, -0.80, 0.85, 0.0);
        path.quadTo(0.0, 0.80, -0.85, 0.0);
        p.drawPath(path);
        p.setPen(Qt::NoPen);
        p.setBrush(colour);
        p.drawEllipse(QPointF(0, 0), 0.24, 0.24);
        break;
    }
    case ToolGlyph::Search:
        p.drawEllipse(QPointF(-0.15, -0.15), 0.48, 0.48);
        p.drawLine(QPointF(0.20, 0.20), QPointF(0.72, 0.72));
        break;
    case ToolGlyph::Web:
        // A globe: rim, equator, one meridian.
        p.drawEllipse(QPointF(0, 0), 0.78, 0.78);
        p.drawEllipse(QPointF(0, 0), 0.32, 0.78);
        p.drawLine(QPointF(-0.78, 0), QPointF(0.78, 0));
        break;
    case ToolGlyph::Agent:
        // A disc throwing off a smaller one, which is the canvas's own picture
        // of a subagent being born.
        p.drawEllipse(QPointF(-0.28, 0.28), 0.42, 0.42);
        p.drawLine(QPointF(0.04, -0.04), QPointF(0.28, -0.28));
        p.setPen(Qt::NoPen);
        p.setBrush(colour);
        p.drawEllipse(QPointF(0.48, -0.48), 0.22, 0.22);
        break;
    case ToolGlyph::Mcp:
        // A plug: two prongs, the body, the cord.
        path.moveTo(-0.25, -0.82);
        path.lineTo(-0.25, -0.45);
        path.moveTo(0.25, -0.82);
        path.lineTo(0.25, -0.45);
        path.moveTo(0.0, 0.18);
        path.lineTo(0.0, 0.82);
        p.drawPath(path);
        p.drawRoundedRect(QRectF(-0.52, -0.45, 1.04, 0.63), 0.18, 0.18);
        break;
    case ToolGlyph::Other:
        p.setPen(Qt::NoPen);
        p.setBrush(colour);
        p.drawEllipse(QPointF(0, 0), 0.20, 0.20);
        break;
    }

    p.restore();
}

void drawSegmentedRing(QPainter& p, const QPointF& centre, qreal radius, qreal width,
                       const RingState& s, qreal sweep, qreal alpha)
{
    if (s.lanes <= 0.01 || radius <= 1.0)
        return;

    const QRectF rect(centre.x() - radius, centre.y() - radius, 2.0 * radius, 2.0 * radius);

    // The gap is a fixed few pixels of arc rather than a fixed angle, so a ring
    // of three and a ring of thirty both read as segments; it shrinks only when
    // the segments themselves would vanish behind it.
    const qreal slotDeg = 360.0 / s.lanes;
    const qreal gapDeg  = std::min(qRadiansToDegrees(2.6 / radius), slotDeg * 0.35);
    const int   count   = static_cast<int>(std::ceil(s.lanes - 1e-3));

    const QColor pending = faded(theme::c(kFg4), alpha);
    const QColor running = faded(theme::c(kFg3), alpha);
    const QColor done    = faded(theme::c(kFg2), alpha);
    const QColor failed  = faded(theme::c(kRedDark), alpha);

    p.setBrush(Qt::NoBrush);

    for (int i = 0; i < count; ++i) {
        const qreal from = static_cast<qreal>(i) * slotDeg + gapDeg * 0.5;
        // The last slot is partial while a new agent's segment is opening.
        const qreal room = std::min(slotDeg, 360.0 - static_cast<qreal>(i) * slotDeg);
        const qreal span = room - gapDeg;
        if (span <= 0.0)
            continue;

        const bool live  = i < s.total && i >= s.doneNow;
        const qreal fill = std::clamp(s.done - static_cast<qreal>(i), 0.0, 1.0);

        // The hollow track under every segment. Hairline, so a run with nothing
        // finished still shows how many agents it has out.
        QPen track(live ? running : pending, 1.0);
        track.setCapStyle(Qt::FlatCap);
        p.setPen(track);
        arcCw(p, rect, from, span);

        if (live && fill < 1.0) {
            // A soft highlight travelling the length of the segment, offset per
            // segment so a run of eight does not pulse in lockstep.
            const qreal phase = std::fmod(sweep * 0.55 + static_cast<qreal>(i) * 0.137, 1.0);
            const qreal len   = span * 0.38;
            const qreal head  = -len + phase * (span + len);
            const qreal a0    = std::max(0.0, head);
            const qreal a1    = std::min(span, head + len);
            const qreal glow  = std::sin(phase * M_PI);
            if (a1 > a0) {
                QPen hi(faded(theme::c(kFg2), 0.25 + 0.55 * glow), width * 0.7);
                hi.setColor(faded(hi.color(), alpha));
                hi.setCapStyle(Qt::RoundCap);
                p.setPen(hi);
                arcCw(p, rect, from + a0, a1 - a0);
            }
        }

        if (fill > 0.0) {
            QPen solid(i < s.failed ? failed : done, width);
            solid.setCapStyle(Qt::FlatCap);
            p.setPen(solid);
            arcCw(p, rect, from, span * fill);
        }
    }
}

} // namespace cx::gui

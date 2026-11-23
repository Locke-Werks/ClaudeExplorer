#pragma once

#include <QColor>
#include <QPointF>
#include <QRectF>

#include <string_view>

class QPainter;

// Two small marks drawn inside and around nodes: the glyph in a tool call's box
// and the segmented ring on a workflow run. Kept apart from the panel so the
// canvas and the key draw them with exactly the same code.
namespace cx::gui {

// Families rather than tools, because a glyph per tool would be a legend
// nobody could learn. The label beside the box still names the exact tool.
enum class ToolGlyph : unsigned char {
    Other,    // the plain dot
    Shell,    // Bash, PowerShell
    Edit,     // Edit, Write, MultiEdit, NotebookEdit
    Read,     // Read
    Search,   // Grep, Glob
    Web,      // WebFetch, WebSearch
    Agent,    // Agent, Task
    Mcp,      // anything under mcp__
};

ToolGlyph toolGlyphFor(std::string_view tool);

// Draws into `box`, a square, as hairline vector strokes so it stays crisp at
// any zoom.
void drawToolGlyph(QPainter& p, ToolGlyph g, const QRectF& box, const QColor& colour);

// One segment per subagent the run has started.
//
// `done` is eased, so a segment fills along its own length when its agent
// finishes rather than flicking from hollow to solid. `lanes` is eased too, so
// a newly spawned agent's segment opens up instead of the ring jumping to new
// divisions. `sweep` is seconds on any clock; running segments carry a soft
// highlight that travels along them, and passing the same value freezes it.
struct RingState {
    qreal lanes   = 0;   // segments to lay out, eased toward `total`
    int   total   = 0;   // agents started
    qreal done    = 0;   // eased count finished
    int   doneNow = 0;   // finished, as the data says this instant
    int   failed  = 0;   // of those, failed
};

void drawSegmentedRing(QPainter& p, const QPointF& centre, qreal radius, qreal width,
                       const RingState& s, qreal sweep, qreal alpha);

} // namespace cx::gui

#pragma once

#include "theme.h"

#include <QColor>
#include <QFont>
#include <QString>

namespace cx::gui {

// The ARCHON / Specter Point design language, translated into Qt.
//
// The token values live in core/theme.h, which has no Qt in it, so the palette
// is shared with anything that cannot link a widget toolkit. Only the
// translation is here.
namespace theme {

inline QColor c(cx::theme::Rgb v) { return QColor(v.r, v.g, v.b); }

// Loads the bundled faces. Must run before any widget exists.
void loadFonts();

// Tracked all-caps, the design language's headline treatment. A real font
// rather than a stylesheet rule, because QSS has neither letter-spacing nor
// text-transform.
QFont tracked(int px, int weight, qreal emSpacing);
QFont body(int px, int weight = QFont::Normal);
QFont mono(int px);

} // namespace theme
} // namespace cx::gui

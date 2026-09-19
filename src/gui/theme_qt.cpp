#include "theme_qt.h"

#include <QApplication>
#include <QFontDatabase>

namespace cx::gui::theme {
namespace {

using cx::theme::kBlack;
using cx::theme::kBorder;
using cx::theme::kBorderHi;
using cx::theme::kElevated;
using cx::theme::kFg1;
using cx::theme::kFg2;
using cx::theme::kFg3;
using cx::theme::kFg4;
using cx::theme::kRed;
using cx::theme::kSurface;

QString hex(cx::theme::Rgb v)
{
    return QString::asprintf("#%02X%02X%02X", v.r, v.g, v.b);
}

QString g_headingFamily = QStringLiteral("Chakra Petch");
QString g_bodyFamily    = QStringLiteral("Outfit");

} // namespace

void loadFonts()
{
    const char* faces[] = {
        ":/fonts/ChakraPetch-Regular.ttf",
        ":/fonts/ChakraPetch-SemiBold.ttf",
        ":/fonts/ChakraPetch-Bold.ttf",
        ":/fonts/Outfit-Variable.ttf",
    };

    QStringList heading, body;
    for (const char* f : faces) {
        const int id = QFontDatabase::addApplicationFont(QString::fromLatin1(f));
        if (id < 0)
            continue;
        for (const QString& fam : QFontDatabase::applicationFontFamilies(id)) {
            if (fam.startsWith(QStringLiteral("Chakra")))
                heading << fam;
            else
                body << fam;
        }
    }

    // Fall back rather than fail. A missing face is a build packaging problem,
    // not a reason to refuse to start.
    if (!heading.isEmpty())
        g_headingFamily = heading.front();
    else
        g_headingFamily = QStringLiteral("Segoe UI Semibold");

    if (!body.isEmpty())
        g_bodyFamily = body.front();
    else
        g_bodyFamily = QStringLiteral("Segoe UI");

    QFont appFont(g_bodyFamily);
    appFont.setPixelSize(13);
    QApplication::setFont(appFont);
}

QFont tracked(int px, int weight, qreal emSpacing)
{
    QFont f(g_headingFamily);
    f.setPixelSize(px);
    f.setWeight(static_cast<QFont::Weight>(weight));
    // AbsoluteSpacing takes pixels, so an em figure from the design spec has to
    // be multiplied by the size it applies at.
    f.setLetterSpacing(QFont::AbsoluteSpacing, px * emSpacing);
    return f;
}

QFont body(int px, int weight)
{
    QFont f(g_bodyFamily);
    f.setPixelSize(px);
    f.setWeight(static_cast<QFont::Weight>(weight));
    return f;
}

QFont mono(int px)
{
    QFont f(QStringLiteral("Consolas"));
    f.setStyleHint(QFont::Monospace);
    f.setPixelSize(px);
    return f;
}

} // namespace cx::gui::theme

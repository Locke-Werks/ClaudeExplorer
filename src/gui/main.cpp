#include "explorer_window.h"
#include "theme_qt.h"

#include <QApplication>
#include <QIcon>

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Claude Explorer"));
    QApplication::setOrganizationName(QStringLiteral("Locke Werks"));
    QApplication::setApplicationVersion(QStringLiteral(CX_VERSION_STRING));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/claudeexplorer.ico")));

    // Before any widget exists: the application font is set here.
    cx::gui::theme::loadFonts();

    // Nothing else happens on the way in. There is no config to read, no git to
    // find and no tree to scan, because there is nothing to configure: the
    // sources are all in fixed places under the user's profile, and a viewer
    // that refused to start over a missing setting would be a viewer that
    // cannot do the one thing it is for.
    cx::gui::ExplorerWindow w;

    // Maximized, not merely large. The camera fits its graph to whatever it is
    // given, so a bigger window is a bigger graph rather than more black around
    // the same one: room is the one thing this view can always use. The
    // constructor's resize() is still what an unmaximized window restores to.
    w.showMaximized();
    return app.exec();
}

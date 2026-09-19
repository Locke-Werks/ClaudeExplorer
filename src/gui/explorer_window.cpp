#include "explorer_window.h"

#include "board_watcher.h"
#include "node_explorer_panel.h"

#include <QEvent>
#include <QKeyEvent>
#include <QSettings>
#include <QVBoxLayout>

namespace cx::gui {
namespace {

// Big enough that the graph has somewhere to settle into. Smaller than this and
// the camera spends its whole time fitting a cluster into a letterbox.
constexpr int kDefaultWidth  = 1280;
constexpr int kDefaultHeight = 820;

// Whether the key was showing last time. Kept in the registry under HKCU rather
// than in a config file, because one boolean is not worth a file format, a
// parser, a path and a documented location.
constexpr const char* kLegendSetting = "legend";

} // namespace

ExplorerWindow::ExplorerWindow()
{
    setWindowTitle(QStringLiteral("Claude Explorer"));
    resize(kDefaultWidth, kDefaultHeight);

    canvas_ = new NodeExplorerPanel(this);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(canvas_);

    QSettings settings;
    canvas_->setLegendVisible(settings.value(QLatin1String(kLegendSetting), true).toBool());

    connect(canvas_, &NodeExplorerPanel::countsChanged, this, &ExplorerWindow::setCounts);

    // One emit, one canvas. The watcher is constructed before it is started so
    // the connection is in place when the first board lands, which on a machine
    // with sessions already running is within a few milliseconds.
    watch_ = new BoardWatcher(this);
    connect(watch_, &BoardWatcher::boardReady, canvas_, &NodeExplorerPanel::setBoard);
    watch_->start();
}

ExplorerWindow::~ExplorerWindow()
{
    // Before the canvas goes. The worker posts to this object's thread, and a
    // rebuild in flight must not land on a half-destroyed window.
    if (watch_)
        watch_->shutdown();
}

void ExplorerWindow::setCounts(const QString& summary)
{
    // The canvas paints this too. It is repeated in the title because that is
    // what a taskbar hover shows, and the point of the window is to answer the
    // question without being looked at.
    setWindowTitle(summary.isEmpty() ? QStringLiteral("Claude Explorer")
                                     : QStringLiteral("Claude Explorer  -  %1").arg(summary));
}

void ExplorerWindow::toggleFullScreen()
{
    if (isFullScreen())
        showNormal();
    else
        showFullScreen();
}

void ExplorerWindow::keyPressEvent(QKeyEvent* e)
{
    switch (e->key()) {
    case Qt::Key_F11:
        toggleFullScreen();
        return;

    // Leaves fullscreen and does nothing else. Esc closing the window is the
    // convention for a dialog, and this is not one: it is a thing left open on
    // a second monitor, where one stray keystroke should not shut it.
    case Qt::Key_Escape:
        if (isFullScreen())
            showNormal();
        return;

    case Qt::Key_K: {
        const bool on = !canvas_->legendVisible();
        canvas_->setLegendVisible(on);
        QSettings().setValue(QLatin1String(kLegendSetting), on);
        return;
    }

    default:
        break;
    }

    QWidget::keyPressEvent(e);
}

void ExplorerWindow::changeEvent(QEvent* e)
{
    QWidget::changeEvent(e);

    // Minimizing does not send the canvas a hide event, so without this the
    // frame loop goes on stepping a simulation nobody can see. QWidget::hide
    // is what the canvas already listens for; calling it here would fight the
    // window manager, so the visibility is what moves instead.
    if (e->type() == QEvent::WindowStateChange)
        canvas_->setVisible(!isMinimized());
}

} // namespace cx::gui

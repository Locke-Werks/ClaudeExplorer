#include "explorer_window.h"

#include "board_watcher.h"
#include "node_explorer_panel.h"
#include "replay_controller.h"

#include <QEvent>
#include <QIcon>
#include <QKeyEvent>
#include <QSettings>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QVBoxLayout>

#include <utility>

#include <windows.h>

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
constexpr const char* kTerminalSetting = "terminals";
constexpr const char* kNotifySetting = "notifications";

// Sessions often stop together: a workflow's agents finish in a batch and every
// parent asks at once. One toast naming the count beats a stack of three.
constexpr int kDebounceMs = 2000;

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
    canvas_->setTerminalsVisible(settings.value(QLatin1String(kTerminalSetting), true).toBool());

    connect(canvas_, &NodeExplorerPanel::countsChanged, this, &ExplorerWindow::setCounts);

    debounce_ = new QTimer(this);
    debounce_->setSingleShot(true);
    debounce_->setInterval(kDebounceMs);
    connect(debounce_, &QTimer::timeout, this, &ExplorerWindow::sendAlert);

    attention_ = new AttentionWatch(this);
    connect(attention_, &AttentionWatch::startedWaiting, this, &ExplorerWindow::onStartedWaiting);
    setNotifications(settings.value(QLatin1String(kNotifySetting), true).toBool());

    // One emit, one canvas. The watcher is constructed before it is started so
    // the connection is in place when the first board lands, which on a machine
    // with sessions already running is within a few milliseconds.
    //
    // Replay sits between the two. The watcher never stops, so live is current
    // the moment replay ends; its boards are only held back from the canvas.
    replay_ = new ReplayController(this);
    connect(replay_, &ReplayController::boardReady, canvas_, &NodeExplorerPanel::setBoard);
    connect(replay_, &ReplayController::toolCallsLogged, canvas_,
            &NodeExplorerPanel::addToolCalls);
    connect(replay_, &ReplayController::returnedLive, this, [this] { canvas_->setBoard(live_); });

    watch_ = new BoardWatcher(this);
    connect(watch_, &BoardWatcher::boardReady, this, [this](const AgentList& board) {
        live_ = board;
        if (!replay_->active())
            canvas_->setBoard(board);
    });
    connect(watch_, &BoardWatcher::toolCallsLogged, this,
            [this](const std::vector<LoggedCall>& calls) {
                if (!replay_->active())
                    canvas_->addToolCalls(calls);
            });
    // Straight from the watcher, not through replay: someone scrubbing the
    // last hour still wants to hear about a session stopping now.
    connect(watch_, &BoardWatcher::boardReady, attention_, &AttentionWatch::setBoard);
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

void ExplorerWindow::onStartedWaiting(const std::vector<AttentionWatch::Waiting>& sessions)
{
    // The ripple plays whether or not notifications are on: it is part of the
    // picture, and someone watching the canvas is exactly who it is for.
    for (const AttentionWatch::Waiting& w : sessions)
        canvas_->ripple(w.sessionId);

    if (!notify_)
        return;
    for (const AttentionWatch::Waiting& w : sessions) {
        const QString name = w.name.isEmpty() ? QStringLiteral("A session") : w.name;
        if (!pending_.contains(name))
            pending_.append(name);
    }
    // Not restarted by later arrivals: a steady trickle of waits must still
    // produce an alert two seconds after the first, not never.
    if (!debounce_->isActive())
        debounce_->start();
}

void ExplorerWindow::sendAlert()
{
    const QStringList names = std::exchange(pending_, {});

    // Checked when the alert fires rather than when the wait began, so someone
    // who switched to the window inside the debounce is not told what they are
    // already looking at.
    if (!notify_ || names.isEmpty() || isActiveWindow())
        return;

    FLASHWINFO flash{};
    flash.cbSize  = sizeof(flash);
    flash.hwnd    = reinterpret_cast<HWND>(winId());
    flash.dwFlags = FLASHW_TRAY | FLASHW_TIMERNOFG;
    FlashWindowEx(&flash);

    if (tray_) {
        const QString body = names.size() == 1
                                 ? names.front()
                                 : QStringLiteral("%1 sessions waiting").arg(names.size());
        tray_->showMessage(QStringLiteral("Claude is waiting"), body,
                           QIcon(QStringLiteral(":/claudeexplorer.ico")));
    }
}

void ExplorerWindow::setNotifications(bool on)
{
    notify_ = on;
    if (!on) {
        pending_.clear();
        debounce_->stop();
        delete tray_;
        tray_ = nullptr;
        return;
    }
    if (tray_ || !QSystemTrayIcon::isSystemTrayAvailable())
        return;

    tray_ = new QSystemTrayIcon(QIcon(QStringLiteral(":/claudeexplorer.ico")), this);
    tray_->setToolTip(QStringLiteral("Claude Explorer"));
    // No menu and no actions: the icon is there to carry toasts, and clicking
    // either it or a toast does the one thing that makes sense, which is bring
    // the canvas forward.
    connect(tray_, &QSystemTrayIcon::activated, this, &ExplorerWindow::raiseWindow);
    connect(tray_, &QSystemTrayIcon::messageClicked, this, &ExplorerWindow::raiseWindow);
    tray_->show();
}

void ExplorerWindow::raiseWindow()
{
    if (isMinimized())
        showNormal();
    raise();
    activateWindow();
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
    if (replay_->handleKey(e))
        return;

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

    case Qt::Key_T: {
        const bool on = !canvas_->terminalsVisible();
        canvas_->setTerminalsVisible(on);
        QSettings().setValue(QLatin1String(kTerminalSetting), on);
        return;
    }

    case Qt::Key_N:
        setNotifications(!notify_);
        QSettings().setValue(QLatin1String(kNotifySetting), notify_);
        canvas_->flash(notify_ ? QStringLiteral("Notifications on")
                               : QStringLiteral("Notifications off"));
        return;

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

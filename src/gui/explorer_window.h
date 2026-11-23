#pragma once

#include "attention_watch.h"

#include <QStringList>
#include <QWidget>

#include <vector>

class QKeyEvent;
class QSystemTrayIcon;
class QTimer;

namespace cx::gui {

class BoardWatcher;
class NodeExplorerPanel;

// The whole product: one canvas, and the thread that feeds it.
//
// There is no menu bar, no toolbar, no status bar and no central-widget
// hierarchy worth the name, which is why this is a QWidget and not a
// QMainWindow. The canvas fills it edge to edge and paints its own overlay, so
// windowed and fullscreen are the same picture rather than one of them being
// the same picture with furniture around it.
//
// A handful of keys and no mouse. Everything that could be clicked on this canvas is
// something the viewer deliberately cannot act on: it reports, and the session
// itself is where work gets done.
class ExplorerWindow : public QWidget {
    Q_OBJECT

public:
    ExplorerWindow();
    ~ExplorerWindow() override;

protected:
    void keyPressEvent(QKeyEvent* e) override;
    void changeEvent(QEvent* e) override;

private:
    void toggleFullScreen();

    // A session just started waiting: ripple it, and if the window is not in
    // front, queue it for the next alert.
    void onStartedWaiting(const std::vector<AttentionWatch::Waiting>& sessions);
    void sendAlert();
    void setNotifications(bool on);
    void raiseWindow();
    void setCounts(const QString& summary);

    BoardWatcher*      watch_ = nullptr;
    NodeExplorerPanel* canvas_ = nullptr;
    AttentionWatch*    attention_ = nullptr;

    // Created only while notifications are on, since a tray icon that does
    // nothing is clutter in someone else's notification area.
    QSystemTrayIcon* tray_ = nullptr;
    QTimer*          debounce_ = nullptr;
    QStringList            pending_;
    bool                   notify_ = true;
};

} // namespace cx::gui

#pragma once

#include "agent_board.h"

#include <QWidget>

class QKeyEvent;

namespace cx::gui {

class BoardWatcher;
class NodeExplorerPanel;
class ReplayController;

// The whole product: one canvas, and the thread that feeds it.
//
// There is no menu bar, no toolbar, no status bar and no central-widget
// hierarchy worth the name, which is why this is a QWidget and not a
// QMainWindow. The canvas fills it edge to edge and paints its own overlay, so
// windowed and fullscreen are the same picture rather than one of them being
// the same picture with furniture around it.
//
// Three keys and no mouse. Everything that could be clicked on this canvas is
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
    void setCounts(const QString& summary);

    BoardWatcher*      watch_  = nullptr;
    NodeExplorerPanel* canvas_ = nullptr;
    ReplayController*  replay_ = nullptr;

    // The watcher's latest board, kept while replay has the canvas so that
    // returning to live shows the present at once rather than after the next
    // rebuild.
    AgentList live_;
};

} // namespace cx::gui

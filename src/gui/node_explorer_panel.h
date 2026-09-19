#pragma once

#include "agent_board.h"
#include "node_graph.h"

#include <QElapsedTimer>
#include <QString>
#include <QWidget>

class QHideEvent;
class QPaintEvent;
class QResizeEvent;
class QShowEvent;
class QTimer;

// Every live Claude Code session on this machine, as a graph that settles.
//
// A view and nothing else: no panning, no zoom, no selection, no menu. The
// camera fits whatever is on the canvas, work pops in when it starts and pops
// out when it ends, and everything else slides out of the way.
//
// It owns no source. ExplorerWindow's BoardWatcher feeds it, and it keeps no
// copy of the list it is given.
//
// The frame loop runs only while there is something to animate and stops again
// as soon as the graph has settled, which is what the timer rules are about. A
// still picture must cost nothing: this is a window people leave open on a
// second monitor for days.
namespace cx::gui {

class NodeExplorerPanel : public QWidget {
    Q_OBJECT

public:
    explicit NodeExplorerPanel(QWidget* parent = nullptr);

    // The latest board, from ExplorerWindow's BoardWatcher. Taken by reference
    // and folded straight into nodes: nothing here keeps a copy of the list.
    void setBoard(const AgentList& board);

    // Whether the key is drawn. On by default, because nobody is born knowing
    // that a broken square with a ~ in it is an armed monitor, and there is no
    // menu to go looking in.
    bool legendVisible() const { return legend_; }
    void setLegendVisible(bool on);

signals:
    // "3 sessions  2 runs  5 agents", for the window title.
    void countsChanged(QString summary);

protected:
    void paintEvent(QPaintEvent*) override;
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;

private:
    void onTick();
    void wake();
    void warm();
    void publishCounts();

    void drawLinks(QPainter& p) const;
    void drawNode(QPainter& p, const Node& n) const;
    void drawLabel(QPainter& p, const Node& n) const;

    // The session's task list, as a column of rows beside its node. Its own
    // function rather than a branch of drawLabel because it lays out rows, a
    // mark column and an indent, none of which a label has.
    void drawTaskBlock(QPainter& p, const Node& n) const;
    void drawNotice(QPainter& p, const QString& text) const;

    // The overlay. Painted onto the canvas rather than put in a status bar, so
    // windowed and fullscreen are the same picture.
    void drawCounts(QPainter& p) const;
    void drawLegend(QPainter& p) const;

    NodeGraph graph_;
    Camera    camera_;

    QTimer*       tick_ = nullptr;
    QElapsedTimer clock_;
    qreal         accumulator_ = 0;
    int           quiet_       = 0;   // consecutive settled frames

    bool seeded_     = false;   // a board has landed, however empty
    bool firstShow_  = true;
    bool warmNeeded_ = false;   // structure moved while nobody was looking
    bool legend_     = true;

    QString counts_;
};

} // namespace cx::gui

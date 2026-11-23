#pragma once

#include "agent_board.h"

#include <QObject>
#include <QString>

#include <string>
#include <unordered_set>
#include <vector>

namespace cx::gui {

// Turns successive boards into edges: the moment a session enters NEEDS YOU.
//
// The board says which sessions are waiting now. What the window wants to act
// on is which ones started waiting since the last board, once, so a session
// that sits on a prompt for an hour raises one alert and not one per rebuild.
// A session leaving the column and coming back is a new wait and counts again.
//
// The first board is taken as a baseline and raises nothing. Opening the
// viewer onto three sessions that have been waiting since lunch is not news,
// and a burst of alerts at launch would teach the user to ignore them.
class AttentionWatch : public QObject {
    Q_OBJECT

public:
    struct Waiting {
        std::string sessionId;
        QString     name;
    };

    explicit AttentionWatch(QObject* parent = nullptr);

    void setBoard(const AgentList& board);

signals:
    void startedWaiting(const std::vector<cx::gui::AttentionWatch::Waiting>& sessions);

private:
    std::unordered_set<std::string> waiting_;
    bool                            seeded_ = false;
};

} // namespace cx::gui

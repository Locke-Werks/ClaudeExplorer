#include "attention_watch.h"

namespace cx::gui {

AttentionWatch::AttentionWatch(QObject* parent)
    : QObject(parent)
{
}

void AttentionWatch::setBoard(const AgentList& board)
{
    std::unordered_set<std::string> now;
    std::vector<Waiting>            fresh;

    for (const AgentCard& c : board) {
        if (c.column != AgentColumn::NeedsYou)
            continue;
        now.insert(c.sessionId);
        if (seeded_ && !waiting_.contains(c.sessionId))
            fresh.push_back({c.sessionId, QString::fromStdString(c.name)});
    }

    // Replaced wholesale rather than merged, so a session that stopped waiting
    // drops out and its next wait is an edge again.
    waiting_ = std::move(now);
    seeded_  = true;

    if (!fresh.empty())
        emit startedWaiting(fresh);
}

} // namespace cx::gui

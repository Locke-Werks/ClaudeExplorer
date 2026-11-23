#pragma once

#include <QDateTime>

#include <atomic>
#include <cstdint>

// The instant a board is judged against.
//
// The graph decides whether a tool call still lingers and whether a monitor is
// still armed by comparing timestamps on the board with "now". In replay, now is
// the playhead, and a reconstructed board judged against the wall clock would
// show every monitor expired and every tool call long gone. One override,
// read in the one place the graph asks the time, keeps the replay path the same
// code as the live one.
namespace cx::gui {

inline std::atomic<std::int64_t> gBoardClockOverrideMs{0};

// Zero returns to the wall clock.
inline void setBoardClockOverride(std::int64_t ms)
{
    gBoardClockOverrideMs.store(ms, std::memory_order_relaxed);
}

inline std::int64_t boardClockMs()
{
    const std::int64_t t = gBoardClockOverrideMs.load(std::memory_order_relaxed);
    return t != 0 ? t : QDateTime::currentMSecsSinceEpoch();
}

} // namespace cx::gui

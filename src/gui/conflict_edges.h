#pragma once

#include "agent_board.h"

#include <cstdint>
#include <string>
#include <vector>

// Two live sessions working in the same place at the same time.
//
// Nothing else on the canvas relates one session to another: each is its own
// cluster, laid out apart. That is right until two of them are editing the
// same file, which neither can see and which ends in one silently overwriting
// the other. This is the one cross-session fact worth an edge.
namespace cx::gui {

// How far back a write still counts. Long enough to span an edit, a build and
// a second edit; short enough that a file one session finished with an hour
// ago does not keep two sessions chained together.
inline constexpr std::int64_t kConflictWindowMs = 15 * 60 * 1000;

enum class ConflictKind {
    SharedTree,   // same repository, different files
    Collision,    // the same file, written by both
};

struct ConflictEdge {
    std::string  a, b;   // session ids, a < b, so a pair has one spelling
    ConflictKind kind = ConflictKind::SharedTree;

    // Collision only: the most recently written shared file's basename, and
    // how many more files the two share beyond it.
    std::string name;
    int         more = 0;

    std::string key() const { return a + '\x1f' + b; }
};

// Every pair of live sessions that wrote inside the same tree within the
// window ending at `nowMs`. A pair is reported once, at its worse severity.
//
// Touches the disk to find repository roots, cached per directory for the life
// of the process, so call it on a board refresh rather than per frame.
std::vector<ConflictEdge> findConflicts(const AgentList& board, std::int64_t nowMs,
                                        std::int64_t windowMs = kConflictWindowMs);

} // namespace cx::gui

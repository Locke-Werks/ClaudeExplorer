Every Claude Code session on this machine, drawn as the shape of the work.

A live session is a node, every workflow run and subagent under it is a smaller
node on a link, and the boxes hanging off a session are what it is doing right
now: the shell still writing, the monitor still armed, the tool call in flight,
the task list, the plan. The graph settles under repulsion and link tension, the
camera fits itself to whatever is on the canvas, and work pops in and out as it
starts and ends.

New in 1.2.0:

Terminal panes. Every shell, monitor and subagent now carries a small terminal
beside it showing its last few lines: a shell's output, or a subagent's own
transcript, with what it said, the tools it called in blue and the first line of
each result dimmed. `T` turns them off and the answer is remembered.

Subagents spawned by subagents hang off the agent that spawned them, to any
depth, rather than all sitting on the session.

Much lower CPU while sessions are busy. The board rebuilds at most twice a
second, transcript tails are read incrementally and cached, panes are rendered
once and reused, and changing text no longer keeps the layout simulation awake.

Requires Windows 11.

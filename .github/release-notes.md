Every Claude Code session on this machine, drawn as the shape of the work.

A live session is a node, every workflow run and subagent under it is a smaller
node on a link, and the boxes hanging off a session are what it is doing right
now: the shell still writing, the monitor still armed, the tool call in flight,
the task list, the plan. The graph settles under repulsion and link tension, the
camera fits itself to whatever is on the canvas, and work pops in and out as it
starts and ends.

New in 1.3.0:

Notifications. When a session stops to wait on you, its disc throws off one red
ripple, the taskbar button flashes if the window is behind, and Windows shows a
toast naming it. `N` turns the toasts off.

Session state at a glance. A thin arc inside a session's rim shows how full its
context window is and turns amber near the limit. A session that has gone quiet
for ten minutes dims and breathes and says how long it has been idle. A session
that finishes leaves a fading dashed outline for three minutes instead of
vanishing.

Conflict edges. Two sessions that wrote the same file in the last fifteen
minutes are joined by a creeping red dashed line naming the file; two working in
the same repository are joined by a faint dotted one.

The tool call in flight carries a mark for its kind: shell, edit, read, search,
web, agent or MCP. A workflow run's ring has one segment per agent with its
count inside, and a failed agent's segment is dashed red.

Replay. `[` and `]` scrub back through the last hour of the hook log, `P` plays
it at 20x, and `L` returns to live.

New nodes come out on the side clear of text, and nothing settles under another
node's label, task list or terminal pane.

Requires Windows 11.

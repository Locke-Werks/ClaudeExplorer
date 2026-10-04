Every Claude Code session on this machine, drawn as the shape of the work.

A live session is a node, every workflow run and subagent under it is a smaller
node on a link, and the boxes hanging off a session are what it is doing right
now: the shell still writing, the monitor still armed, the tool call in flight,
the task list, the plan. The graph settles under repulsion and link tension, the
camera fits itself to whatever is on the canvas, and work pops in and out as it
starts and ends.

New in 1.1.0:

A tool log in the top-left corner. Every tool call any session makes appears as
one line, the tool and its whole argument, newest on top, and fades out after a
few seconds. The hook now records the full call for it rather than only the
120-character summary.

A session that started with no background shells now shows them once it has
some. Before, a session that was quiet on the first read never showed a shell
for as long as the window stayed open.

A monitor whose command has exited leaves the canvas straight away instead of
waiting out its timeout.

Requires Windows 11.

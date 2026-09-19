Every Claude Code session on this machine, drawn as the shape of the work.

A live session is a node, every workflow run and subagent under it is a smaller
node on a link, and the boxes hanging off a session are what it is doing right
now: the shell still writing, the monitor still armed, the tool call in flight,
the task list, the plan. The graph settles under repulsion and link tension, the
camera fits itself to whatever is on the canvas, and work pops in and out as it
starts and ends.

New in 1.0.0:

The first release. Extracted from ProjectMan's Node Explorer tab and shipped on
its own, with no projects root to configure and nothing to set up.

An on-screen key, drawn with the same primitives the canvas uses rather than
described in words. `K` hides it once you no longer need it and the answer is
remembered.

Fullscreen on `F11`, `Esc` to leave it. The window opens maximized, and the
counts and the key are painted onto the canvas rather than into a status bar, so
windowed and fullscreen are the same picture.

The installer registers the nine Claude Code hooks itself, as the logged-in user
rather than as the administrator, and takes them out again on uninstall. Both
are idempotent: an install that finds the registration already correct writes
nothing at all, one that finds it wrong repairs it in place, and neither touches
a hook that belongs to something else.

Requires Windows 11.

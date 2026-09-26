<div align="center">

<img src="assets/claudeexplorer.ico" width="96" alt="Claude Explorer">

# Claude Explorer

**Every Claude Code session on this machine, drawn as the shape of the work.**

[![release](https://img.shields.io/github/v/release/Locke-Werks/ClaudeExplorer?style=flat-square&color=d6262a)](https://github.com/Locke-Werks/ClaudeExplorer/releases)
[![license](https://img.shields.io/badge/license-GPLv3-d6262a?style=flat-square)](LICENSE)
[![platform](https://img.shields.io/badge/platform-Windows%2011-d6262a?style=flat-square)](#requirements)

</div>

---

[![Claude Explorer demo](https://img.youtube.com/vi/eFZSu8pUQnI/maxresdefault.jpg)](https://www.youtube.com/watch?v=eFZSu8pUQnI)

*Demo: every Claude Code session on one canvas (1:14)*

You have four Claude Code sessions open. One is running a workflow that spawned
eleven subagents. One is sitting on a permission prompt you have not noticed.
One finished twenty minutes ago and you have not gone back to it. One is
grinding through a task list you wrote an hour ago and can no longer remember.

Your terminal shows you one of them at a time, and only the last few lines of
it. `Ctrl+Tab` through the others and you are reconstructing the state of your
own machine from scrollback.

Claude Explorer puts the whole thing on one canvas. Every live session is a
node. Every workflow run and subagent under it is a smaller node on a link. The
boxes hanging off a session are what it is *doing* right now: the shell that is
still writing, the monitor that is still armed, the tool call in flight, the
task list, the plan it wrote. The whole thing settles under repulsion and link
tension, work pops in when it starts and pops out when it ends, and the camera
zooms itself to keep all of it in frame.

```
                          Explore
                             o
                              \
             CLAUDE-EXPLORER   O ---- o Explore
                              / \
                   Explore   o   [#] > Porting the GUI
                                       Make the icon
                                       Wire CI
```

It is a view and nothing else. There is no panning, no zoom control, no
selection and no menu, because there is nothing here to act on: the session is
where work gets done, and a viewer that let you poke at it from the side would
be a worse terminal. It answers one question, which is the one a terminal
cannot: what is the shape of what is running.

## Reading it

Two shapes and no more, because a legend nobody is shown has to be learnable by
looking at it once. **A disc is something that thinks. A box is work being
done.** Press `K` if you want the key on screen; it is on by default.

| | |
|---|---|
| large disc | a live session |
| medium disc | a workflow run, with an arc showing how far through its agents it is |
| small disc | a subagent |
| `>` box | a background shell, still writing |
| `~` box | a Monitor watch, still armed |
| `.` box | the tool call in flight this instant |
| `#` box | the session's task list, drawn as a block beside it |
| `=` box | the plan document it wrote |

**A solid rim means something is executing this instant. A broken rim means
armed or inert.** A monitor between two events is waiting, not working, and a
plan is a document that was written once. That is the only state those carry,
which is what lets one glance separate the things that are happening from the
things that are merely there.

A session with two hairline rings around it has stopped and is waiting for you.
It is the only red on the canvas.

The task list is drawn as one block rather than as a node per item, which is a
deliberate reversal. A node per item is the shape the data has and it read
badly: ten items became ten boxes scattered around the session by a force
layout with no reason to keep them in order, and the one thing a task list is
read for, what is being worked on and what is left, took longer to find than it
would have in a terminal. The dependency edges survive as indentation, so a
blocked item still sits under the thing blocking it and costs one glyph instead
of an edge the eye has to trace.

Sessions that have finished are not drawn at all.

## Keys

| | |
|---|---|
| `F11` | fullscreen on and off |
| `Esc` | leave fullscreen |
| `K` | show or hide the key |

There is no mouse input of any kind.

## Where the data comes from

Claude Explorer writes nothing to Claude Code's own files. It reads seven
sources, and what each one can and cannot say decides what ends up on screen.

| Source | What it owns |
|---|---|
| `~/.claude/sessions/<pid>.json` | Claude Code's live registry. The only thing that knows a session exists, and the only thing still true after one is killed without warning |
| `~/.claude/projects/<slug>/<id>/` | a subagent's type and description, and which workflow run and phase it belongs to |
| `%TEMP%\claude\<slug>\<id>\tasks\` | one file per background shell and monitor, holding its output as it arrives |
| `~/.claude/tasks/<id>/` | the task list, with real `blocks` / `blockedBy` edges between items |
| `~/.claude/jobs/`, `~/.claude/daemon/` | sessions running headless under the background daemon, which never write a registry file |
| `~/.claude/plans/` | plan documents |
| `%LOCALAPPDATA%\ClaudeExplorer\agents\events.jsonl` | what the hooks recorded: see below |

None of these is documented and none is stable. Every field is treated as
optional and every unrecognised value as information rather than as an error, so
a Claude Code that changes one of them costs a detail rather than the window.

Two of those sources answer a question the others cannot. A background shell
holds its output file open for exactly as long as it runs, so an exclusive open
says precisely whether it is still going. A monitor never holds the file at all,
so the only thing on the machine that says when one stops is the timeout it
declared when it started, which is why that number is carried through the hook
log.

The view does not poll on a timer. Two directory watches do the waking, with a
five second ceiling behind them in case the kernel drops a notification, and a
150ms debounce so one action costs one rebuild rather than six. Once the graph
has settled the canvas stops repainting entirely: a still picture costs nothing,
which matters for a window that lives on a second monitor for days.

## The hooks

The installer offers to register nine Claude Code hooks, and the checkbox is
ticked by default. They write one line per event to
`%LOCALAPPDATA%\ClaudeExplorer\agents\events.jsonl`, capped at 4 MB with one
back-file.

Declining them leaves a working view that says less. Sessions, subagents,
workflow runs, task lists, background jobs and the shells themselves are all
read straight off disk and do not need the hooks. What does need them:

- the tool call in flight
- a shell's command, as opposed to the fact that a shell is running
- a monitor's expiry, and therefore whether it is still armed
- telling a subagent's tool call apart from its parent session's
- joining a plan document to the session that wrote it

You can change your mind later. `cxhook install` and `cxhook uninstall` are on
PATH once the product is installed, and `cxhook status` says what is currently
registered.

Both are idempotent and neither is destructive. Entries are tagged
`"_claudeexplorer": true`, so uninstall removes exactly what install wrote and
leaves everyone else's hooks, matchers and settings untouched. An install that
finds the registration already correct writes nothing at all: no rewrite, no
backup, not even a touched timestamp. An install that finds it wrong, pointing
at an old path, carrying a broken timeout, or doubled up by a hand edit, repairs
it in place rather than adding a second copy beside it.

The receiver always exits 0, whatever it is handed. Claude Code reads a non-zero
exit from a `PreToolUse` hook as *block this tool call* and hands the hook's
stderr to the model, so a logger that failed loudly would stop your work over a
log line.

## Requirements

Windows 11. Nothing else: the installer carries its own Qt runtime, and there
is no configuration file, no account and no network access of any kind.

Claude Explorer and ProjectMan can be installed together. They keep separate
logs and tag their hook entries differently, so neither disturbs the other's
registration.

## Building

```
cmake --preset vs -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/msvc2022_64
cmake --build --preset vs
```

Qt 6.5 or newer, MSVC, x64. There is no third-party dependency to fetch and no
network step in the build.

`scripts/test-hooks.ps1` exercises install, repair, the receiver and uninstall
against a scratch settings file. It runs under `CX_HOOK_ROOT`, which moves both
the settings path and the event log into a temporary directory, so it cannot
reach the real `~/.claude/settings.json`.

## License

GPLv3. See [LICENSE](LICENSE).

Chakra Petch and Outfit are bundled under the SIL Open Font License 1.1; see
[assets/fonts](assets/fonts).

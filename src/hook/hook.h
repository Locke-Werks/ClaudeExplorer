#pragma once

#include "fs.h"

#include <cstdint>
#include <string>
#include <vector>

// cxhook.exe: Claude Code's hook receiver, and the commands that register it.
//
// With no argument this is the receiver. Claude Code runs it for every hook
// event in every session, feeding the payload on stdin. It reads that payload,
// appends one line to the event log, and exits. The viewer tails the log;
// nothing else reads it.
//
// Being on the critical path of every tool call sets the rules:
//
//   - No config, no network, nothing to resolve. Open, append, close. This is
//     also why it is its own binary rather than a switch on the viewer: it
//     links no Qt, so there is no DLL set to load before it can do the one
//     thing it is here for.
//   - It always exits 0. Claude Code reads a non-zero exit from a PreToolUse
//     hook as "block this tool call" and hands the hook's stderr to the model,
//     so a receiver that failed loudly would stop somebody's work over a log
//     line. Failures are dropped silently: the session registry still carries
//     the session, so a lost event costs detail, never the node.
//
// The management subcommands are not on that path and report failure normally.
namespace cx::hook {

// The record appended to events.jsonl, one compact JSON object per line:
//
//   {"ts":1789311216420,"event":"PreToolUse","session":"<uuid>",
//    "cwd":"C:\\Users\\you\\projects\\ClaudeExplorer",
//    "tool":"Edit","detail":"src/gui/node_graph.cpp"}
//
//   ts       int, milliseconds since the Unix epoch
//   event    the hook_event_name, verbatim
//   session  session_id
//   cwd      the session's working directory
//   tool     tool_name, on PreToolUse and PostToolUse; absent otherwise
//   detail   what the canvas should show, by event:
//              SessionStart       source ("startup", "resume", "compact")
//              UserPromptSubmit   the prompt, first line, cut to fit
//              PreToolUse         claude::toolSummary of tool_input
//              Notification       message: what it is asking for
//              Stop               empty
//              SubagentStart      empty; agentType carries it
//              SubagentStop       empty
//              SessionEnd         reason
//
//   agent      agent_id, present on anything a SUBAGENT did
//   agentType  agent_type: "Explore", "workflow-subagent", a named agent
//   toolUse    tool_use_id, PreToolUse and PostToolUse
//   task       backgroundTaskId or taskId, PostToolUse only
//   ttl        timeout_ms, PreToolUse on a Monitor only
//
// PostToolUse is a special case and does not mirror PreToolUse. A line is
// written ONLY when the tool response carries a background task id, which is
// the name of the file under %TEMP%\claude that the call writes its output
// into. Every other PostToolUse is dropped before anything is opened, so the
// log grows by a line per background task rather than a line per tool call.
// Such a line carries no detail of its own: `toolUse` points at the PreToolUse
// line that already holds the command.
//
// That id has two spellings and they are not interchangeable. Bash calls it
// backgroundTaskId and Monitor calls it taskId. Both are read, because they
// mean the same thing and a reader that knows only the first sees every shell
// and never a monitor. The wrapper has two names too: tool_response in a hook
// payload, toolUseResult in a transcript record, and which one a given CLI
// hands a hook is not documented.
//
// `ttl` is the only thing on the machine that says when a monitor stops. A
// background shell holds its output file open for as long as it runs, so an
// exclusive open answers the question exactly; a monitor never holds the file
// at all, so the timeout it declared when it started is the whole answer.
//
// agent, agentType and toolUse are why the graph can tell a session's own work
// from its subagents'. A subagent runs under its parent's session_id and
// reports its tool calls through the same hooks, so without agent_id a session
// shows whichever of five subagents called a tool last as its own work.
// Verified on CLI 2.1.276, where the parent's own spawn call is tool_name
// "Agent" and carries no agent_id.
//
// Keys are omitted when empty rather than written as "". src/gui/agent_board.h
// declares the reading half and the two must agree.

// Everything after the exe name. Empty is the receiver.
int run(const std::vector<std::string>& args);

// Set this to a directory and both paths below move inside it:
//
//   <dir>\settings.json            instead of %USERPROFILE%\.claude\settings.json
//   <dir>\agents\events.jsonl      instead of %LOCALAPPDATA%\ClaudeExplorer\...
//
// It exists because there is otherwise no way to exercise install, uninstall or
// the receiver without writing to the settings file Claude Code is reading, and
// a registration bug that eats that file is the one failure here that costs
// somebody real work. A test points this at a scratch directory holding a copy.
//
// Honoured by the receiver and by the management subcommands alike, so an
// install and the events it later produces land in the same scratch tree. Unset
// or empty means the real locations, which is every ordinary run.
constexpr const wchar_t* kTestRootVar = L"CX_HOOK_ROOT";

// %LOCALAPPDATA%\ClaudeExplorer\agents\events.jsonl. Empty when LocalAppData
// cannot be resolved.
fs::path eventLogPath();

// Hooks fire per tool call across every session at once, so the log is capped.
// Past kMaxLogBytes it is rolled to events.1.jsonl and started again, keeping
// one back-file. The viewer reads both.
constexpr std::int64_t kMaxLogBytes = 4 * 1024 * 1024;

} // namespace cx::hook

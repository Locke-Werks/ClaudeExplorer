<#
.SYNOPSIS
    Exercises cxhook install / status / uninstall against a scratch settings file.

.DESCRIPTION
    Everything runs under CX_HOOK_ROOT, which moves both the settings file and
    the event log into a temporary directory. Nothing here can reach the real
    ~/.claude/settings.json, which is the whole point: a registration bug that
    eats that file is the one failure in this product that costs somebody real
    work.

    The seed file is deliberately not empty. It carries a hook belonging to
    somebody else and a handful of unrelated top-level keys, because the failure
    worth catching is not "did it write nine entries", it is "did it write nine
    entries and quietly drop everything around them".

.PARAMETER Exe
    The cxhook.exe to test. Defaults to the Debug build.
#>

[CmdletBinding()]
param(
    [string] $Exe = 'build/vs/bin/Debug/cxhook.exe'
)

$ErrorActionPreference = 'Stop'
Set-Location (Split-Path $PSScriptRoot -Parent)

if (-not (Test-Path $Exe)) {
    throw "$Exe not found. Build first: cmake --build --preset vs"
}
$Exe = (Resolve-Path $Exe).Path

$expectedEvents = @(
    'SessionStart', 'SessionEnd', 'UserPromptSubmit', 'PreToolUse', 'PostToolUse',
    'Notification', 'Stop', 'SubagentStart', 'SubagentStop'
)

$root = Join-Path ([System.IO.Path]::GetTempPath()) "cxhook-test-$(Get-Random)"
New-Item -ItemType Directory -Force -Path $root | Out-Null
$settings = Join-Path $root 'settings.json'

$failures = New-Object System.Collections.Generic.List[string]
function Check([string] $what, [scriptblock] $test) {
    $ok = $false
    try { $ok = [bool] (& $test) } catch { $ok = $false }
    if ($ok) {
        Write-Host "  ok    $what"
    } else {
        Write-Host "  FAIL  $what" -ForegroundColor Red
        $failures.Add($what)
    }
}

function Ours([object] $doc) {
    $n = 0
    foreach ($ev in $doc.hooks.PSObject.Properties) {
        foreach ($group in @($ev.Value)) {
            foreach ($h in @($group.hooks)) {
                if ($h.PSObject.Properties.Name -contains '_claudeexplorer') { $n++ }
            }
        }
    }
    return $n
}

function Theirs([object] $doc) {
    $n = 0
    foreach ($ev in $doc.hooks.PSObject.Properties) {
        foreach ($group in @($ev.Value)) {
            foreach ($h in @($group.hooks)) {
                if ($h.PSObject.Properties.Name -notcontains '_claudeexplorer') { $n++ }
            }
        }
    }
    return $n
}

try {
    $env:CX_HOOK_ROOT = $root

    # A settings file with somebody else's hook in it and keys we must not touch.
    $seed = [ordered]@{
        model       = 'opus'
        permissions = [ordered]@{ defaultMode = 'plan' }
        hooks       = [ordered]@{
            SessionStart = @(
                [ordered]@{
                    matcher = 'startup|resume'
                    hooks   = @([ordered]@{
                        type    = 'command'
                        command = '"C:\Program Files\Somebody Else\tool.exe" resume'
                        timeout = 30
                    })
                }
            )
        }
    }
    $seedJson = $seed | ConvertTo-Json -Depth 12
    Set-Content -Path $settings -Value $seedJson -Encoding utf8

    Write-Host "root     $root"
    Write-Host "exe      $Exe"
    Write-Host ''

    # --- status on a file with nothing of ours in it -----------------------
    $out = & $Exe status 2>&1 | Out-String
    Check 'status reports not installed' { $out -match 'not installed' }

    # --- install -----------------------------------------------------------
    & $Exe install | Out-Null
    Check 'install exits 0' { $LASTEXITCODE -eq 0 }

    $doc = Get-Content $settings -Raw | ConvertFrom-Json
    Check "install wrote $($expectedEvents.Count) entries" { (Ours $doc) -eq $expectedEvents.Count }
    Check 'install left the other hook alone' { (Theirs $doc) -eq 1 }
    Check 'install kept unrelated top-level keys' {
        $doc.model -eq 'opus' -and $doc.permissions.defaultMode -eq 'plan'
    }
    Check 'every expected event is registered' {
        $have = $doc.hooks.PSObject.Properties.Name
        -not ($expectedEvents | Where-Object { $_ -notin $have })
    }
    Check 'a backup was written' { Test-Path "$settings.cx-backup" }

    # The bug this catches only appears when the exe sits in a path with no
    # space in it: Claude Code runs a hook through bash, where an unquoted
    # backslash is an escape character.
    $cmd = $null
    foreach ($group in @($doc.hooks.PreToolUse)) {
        foreach ($h in @($group.hooks)) {
            if ($h.PSObject.Properties.Name -contains '_claudeexplorer') { $cmd = $h.command }
        }
    }
    Check 'the command is quoted' { $cmd -match '^".*"$' }
    Check 'the command names cxhook.exe and nothing after it' {
        $cmd.Trim('"').EndsWith('cxhook.exe', [StringComparison]::OrdinalIgnoreCase)
    }
    Check 'the quoted path actually runs' {
        # Through a shell, the way Claude Code invokes it. It reads a payload on
        # stdin and must exit 0 whatever it is handed.
        '{}' | & cmd /c "$cmd" 2>&1 | Out-Null
        $LASTEXITCODE -eq 0
    }

    # --- install again -----------------------------------------------------
    # The point is not only that it does not double. It must not write at all:
    # this file is open in every running session, and rewriting it to produce
    # the bytes it already holds spends a backup and a rename for nothing.
    $before = Get-Content $settings -Raw
    $stamp  = (Get-Item $settings).LastWriteTimeUtc

    $out = & $Exe install 2>&1 | Out-String
    Check 'a second install reports nothing to do' { $out -match 'nothing to do' }

    $doc = Get-Content $settings -Raw | ConvertFrom-Json
    Check 'a second install does not duplicate' { (Ours $doc) -eq $expectedEvents.Count }
    Check 'a second install still spares the other hook' { (Theirs $doc) -eq 1 }
    Check 'a second install leaves the bytes alone' { (Get-Content $settings -Raw) -eq $before }
    Check 'a second install does not touch the file' {
        (Get-Item $settings).LastWriteTimeUtc -eq $stamp
    }

    # --- repair ------------------------------------------------------------
    # A registration that is present but wrong must be fixed, not left because
    # it looked installed. Break the timeout on one entry and install again.
    $doc = Get-Content $settings -Raw | ConvertFrom-Json
    foreach ($group in @($doc.hooks.Stop)) {
        foreach ($h in @($group.hooks)) {
            if ($h.PSObject.Properties.Name -contains '_claudeexplorer') { $h.timeout = 0 }
        }
    }
    Set-Content -Path $settings -Value ($doc | ConvertTo-Json -Depth 12) -Encoding utf8

    & $Exe install | Out-Null
    $doc = Get-Content $settings -Raw | ConvertFrom-Json
    Check 'install repairs a bad timeout' {
        $t = @()
        foreach ($group in @($doc.hooks.Stop)) {
            foreach ($h in @($group.hooks)) {
                if ($h.PSObject.Properties.Name -contains '_claudeexplorer') { $t += $h.timeout }
            }
        }
        $t.Count -eq 1 -and $t[0] -eq 5
    }
    Check 'repair did not duplicate' { (Ours $doc) -eq $expectedEvents.Count }
    Check 'repair still spared the other hook' { (Theirs $doc) -eq 1 }

    # And a hand-added duplicate is collapsed rather than left firing twice.
    $doc = Get-Content $settings -Raw | ConvertFrom-Json
    $dupe = @($doc.hooks.PreToolUse)[-1] | ConvertTo-Json -Depth 12 | ConvertFrom-Json
    $doc.hooks.PreToolUse = @($doc.hooks.PreToolUse) + @($dupe)
    Set-Content -Path $settings -Value ($doc | ConvertTo-Json -Depth 12) -Encoding utf8

    & $Exe install | Out-Null
    $doc = Get-Content $settings -Raw | ConvertFrom-Json
    Check 'install collapses a duplicate entry' { (Ours $doc) -eq $expectedEvents.Count }
    Check 'collapsing still spared the other hook' { (Theirs $doc) -eq 1 }

    $out = & $Exe status 2>&1 | Out-String
    Check 'status reports installed' { $out -match 'installed' -and $out -notmatch 'not installed' }

    # --- the receiver ------------------------------------------------------
    $payload = '{"hook_event_name":"PreToolUse","session_id":"test-session",' +
               '"cwd":"C:\\tmp","tool_name":"Read",' +
               '"tool_input":{"file_path":"C:\\tmp\\x.txt"},"tool_use_id":"toolu_1"}'
    $payload | & $Exe | Out-Null
    Check 'the receiver exits 0' { $LASTEXITCODE -eq 0 }

    $log = Join-Path $root 'agents/events.jsonl'
    Check 'the receiver wrote a line' { Test-Path $log }
    if (Test-Path $log) {
        $line = (Get-Content $log -Tail 1 | ConvertFrom-Json)
        Check 'the line carries the event'   { $line.event -eq 'PreToolUse' }
        Check 'the line carries the session' { $line.session -eq 'test-session' }
        Check 'the line carries the summary' { $line.detail -eq 'C:\tmp\x.txt' }
    }

    # Garbage on stdin is a dropped line, never a non-zero exit: Claude Code
    # reads exit 2 from a PreToolUse hook as "block this tool call".
    'not json at all' | & $Exe | Out-Null
    Check 'malformed input still exits 0' { $LASTEXITCODE -eq 0 }

    # --- uninstall ---------------------------------------------------------
    & $Exe uninstall | Out-Null
    Check 'uninstall exits 0' { $LASTEXITCODE -eq 0 }

    $doc = Get-Content $settings -Raw | ConvertFrom-Json
    Check 'uninstall removed every entry of ours' { (Ours $doc) -eq 0 }
    Check 'uninstall left the other hook alone' { (Theirs $doc) -eq 1 }
    Check 'uninstall kept unrelated top-level keys' {
        $doc.model -eq 'opus' -and $doc.permissions.defaultMode -eq 'plan'
    }
    Check 'the other hook still has its matcher' {
        @($doc.hooks.SessionStart)[0].matcher -eq 'startup|resume'
    }

    $out = & $Exe uninstall 2>&1 | Out-String
    Check 'a second uninstall reports nothing to do' { $out -match 'no Claude Explorer hooks' }

    Write-Host ''
    if ($failures.Count -gt 0) {
        throw "$($failures.Count) check(s) failed: $($failures -join '; ')"
    }
    Write-Host 'all checks passed' -ForegroundColor Green
}
finally {
    Remove-Item Env:\CX_HOOK_ROOT -ErrorAction SilentlyContinue
    Remove-Item $root -Recurse -Force -ErrorAction SilentlyContinue
}

<#
.SYNOPSIS
    Stages the installer payload and records which binaries are ours.

.DESCRIPTION
    Everything under the stage directory lands on the customer's disk byte for
    byte. Forge hashes payload members into the container and extracts them
    verbatim, so an unsigned binary going in stays unsigned on the installed
    machine: signing the finished installer does nothing for them.

    That is why this writes build/payload-ours.txt. The signing step takes a
    folder rather than a filter, and the stage tree is mostly Qt, which arrives
    signed by Qt. The list is what separates the two.

.PARAMETER BuildDir
    The configured build tree. Defaults to the release preset's.

.PARAMETER Config
    The MSVC configuration to install from.
#>

[CmdletBinding()]
param(
    [string] $BuildDir = 'build/release',
    [string] $Config   = 'Release',
    [string] $StageDir = 'stage'
)

$ErrorActionPreference = 'Stop'
Set-Location (Split-Path $PSScriptRoot -Parent)

if (Test-Path $StageDir) {
    Remove-Item $StageDir -Recurse -Force
}

cmake --install $BuildDir --config $Config --prefix $StageDir
if ($LASTEXITCODE -ne 0) {
    throw "cmake --install failed with $LASTEXITCODE"
}

# Our own binaries, named rather than inferred. A filter over the stage tree
# would sweep up the Qt DLLs, and re-signing somebody else's signed binary is
# not a thing to do by accident.
$ours = @('ClaudeExplorer.exe', 'cxhook.exe')

$found = @()
foreach ($name in $ours) {
    $path = Join-Path $StageDir $name
    if (-not (Test-Path $path)) {
        throw "$path is missing from the payload"
    }
    $found += (Resolve-Path $path).Path
}

# windeployqt puts the platform plugin in a subdirectory, and an installer that
# shipped without it produces a binary that exits on launch with no window and
# no message. Cheap to assert, expensive to discover.
$platform = Join-Path $StageDir 'platforms/qwindows.dll'
if (-not (Test-Path $platform)) {
    throw "$platform is missing; the staged tree is not self-contained"
}

New-Item -ItemType Directory -Force -Path 'build' | Out-Null
$found | Set-Content 'build/payload-ours.txt' -Encoding utf8

$files = Get-ChildItem $StageDir -Recurse -File
$mb    = [math]::Round(($files | Measure-Object Length -Sum).Sum / 1MB, 1)
Write-Host ("staged {0} files, {1} MB" -f $files.Count, $mb)
Write-Host ''
$files | Sort-Object Length -Descending | Select-Object -First 8 |
    ForEach-Object { '  {0,8:N0} KB  {1}' -f ($_.Length / 1KB), $_.Name }

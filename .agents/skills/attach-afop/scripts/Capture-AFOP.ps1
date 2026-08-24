[CmdletBinding()]
param(
    [string]$RepoRoot = "D:\renderdoc",
    [ValidateRange(0, 1000000000)]
    [int]$QueueFrame = 0
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

$statePath = Join-Path $env:LOCALAPPDATA "Temp\YeeCapture\AFOPAttachState.json"
$commandExe = Join-Path $RepoRoot "x64\Release\yeecapturecmd.exe"

if (-not (Test-Path -LiteralPath $statePath -PathType Leaf)) {
    throw "No AFOP attach state was found. Run Attach-AFOP.ps1 first."
}
if (-not (Test-Path -LiteralPath $commandExe -PathType Leaf)) {
    throw "YeeCapture command is missing: $commandExe"
}

$state = Get-Content -LiteralPath $statePath -Raw | ConvertFrom-Json
$afop = Get-Process -Id ([int]$state.ProcessId) -ErrorAction SilentlyContinue
if ($null -eq $afop -or $afop.ProcessName -ne "afop") {
    throw "The attached AFOP process is no longer running. Attach AFOP again first."
}

$captureArgs = @("targetcapture", "--target=$([uint32]$state.TargetIdent)")
if ($QueueFrame -gt 0) {
    $captureArgs += "--frame=$QueueFrame"
}

& $commandExe @captureArgs
exit $LASTEXITCODE

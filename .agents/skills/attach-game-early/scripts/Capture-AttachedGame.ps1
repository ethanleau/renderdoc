[CmdletBinding()]
param(
    [string]$StatePath = "",
    [ValidateRange(0, 1000000000)][int]$QueueFrame = 0,
    [ValidateRange(1, 3600)][int]$TimeoutSeconds = 120,
    [switch]$PreflightOnly
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "GameAttach.Common.ps1")

function Invoke-TargetCaptureCommand {
    param(
        [Parameter(Mandatory = $true)][string]$Command,
        [Parameter(Mandatory = $true)][string[]]$Arguments
    )

    # Windows PowerShell surfaces native stderr as ErrorRecord objects. Keep it
    # available for diagnostics without allowing ErrorActionPreference=Stop to
    # hide the command's real exit code.
    $savedErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        $output = @(& $Command @Arguments 2>&1)
        $exitCode = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $savedErrorActionPreference
    }

    $text = ($output | ForEach-Object { $_.ToString() }) -join [Environment]::NewLine
    if (-not [string]::IsNullOrWhiteSpace($text)) {
        Write-Host $text
    }

    return [pscustomobject]@{
        ExitCode = [int]$exitCode
        Text = $text
    }
}

if ([string]::IsNullOrWhiteSpace($StatePath)) {
    $StatePath = Join-Path (Get-AttachStateDirectory) "Latest.json"
}
if (-not (Test-Path -LiteralPath $StatePath -PathType Leaf)) {
    throw "No successful generic game attach state was found: $StatePath"
}

$state = Get-Content -LiteralPath $StatePath -Raw | ConvertFrom-Json
$profilePath = [string](Get-OptionalProperty $state "ProfilePath" "")
if ([string]::IsNullOrWhiteSpace($profilePath)) {
    throw "The attach state does not contain a profile path. Run attach again."
}
$profileInfo = Read-GameAttachProfile -ProfilePath $profilePath
$process = Get-Process -Id ([int]$state.ProcessId) -ErrorAction SilentlyContinue
if ($null -eq $process) {
    throw "The attached game process is no longer running."
}

$targetIdent = Assert-GameAttachStateProcess -State $state -ProfileInfo $profileInfo -Process $process

if ($PreflightOnly) {
    $connectResult = Invoke-TargetCaptureCommand -Command $profileInfo.Command `
        -Arguments @("targetcapture", "--target=$targetIdent", "--connect-only")
    if ($connectResult.ExitCode -ne 0) {
        [Console]::Error.WriteLine("targetcapture connection preflight failed with exit code $($connectResult.ExitCode).")
        exit $connectResult.ExitCode
    }

    Write-Host "GAME_CAPTURE_PREFLIGHT_OK"
    Write-Host "PID=$($process.Id)"
    Write-Host "TARGET_IDENT=$targetIdent"
    Write-Host "STATE=$StatePath"
    exit 0
}

$captureArgs = @("targetcapture", "--target=$targetIdent", "--timeout=$TimeoutSeconds")
if ($QueueFrame -gt 0) {
    $captureArgs += "--frame=$QueueFrame"
}

$captureResult = Invoke-TargetCaptureCommand -Command $profileInfo.Command -Arguments $captureArgs
if ($captureResult.ExitCode -ne 0) {
    [Console]::Error.WriteLine("targetcapture failed with exit code $($captureResult.ExitCode).")
}
exit $captureResult.ExitCode

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$ProfilePath,
    [ValidateRange(5, 180)][int]$ObserveSeconds = 30,
    [switch]$StopAfterObserve,
    [string]$ReportPath = ""
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "GameAttach.Common.ps1")

$profileInfo = Read-GameAttachProfile -ProfilePath $ProfilePath
Stop-ExistingProfileTarget -ProfileInfo $profileInfo

if ([string]::IsNullOrWhiteSpace($ReportPath)) {
    $reportDirectory = Join-Path $env:LOCALAPPDATA "Temp\YeeCapture\AttachGameEarly"
    New-Item -ItemType Directory -Path $reportDirectory -Force | Out-Null
    $ReportPath = Join-Path $reportDirectory ($profileInfo.Slug + "-launch-report.json")
}

$started = Get-Date
Start-ProfileProcess -ProcessConfig (Get-OptionalProperty $profileInfo.Profile "launch") | Out-Null
Write-Host "Observing $($profileInfo.ProcessName).exe for $ObserveSeconds seconds..."

$seen = @{}
$events = @()
$deadline = (Get-Date).AddSeconds($ObserveSeconds)
while ((Get-Date) -lt $deadline) {
    $now = Get-Date
    $running = @(Get-Process -Name $profileInfo.ProcessName -ErrorAction SilentlyContinue)
    $runningIds = @($running | ForEach-Object { $_.Id })

    foreach ($process in $running) {
        try {
            if ($process.StartTime -lt $started.AddSeconds(-1)) {
                continue
            }
        }
        catch { continue }

        $key = [string]$process.Id
        if (-not $seen.ContainsKey($key)) {
            $info = Get-ProcessLaunchInfo -Process $process
            $record = [pscustomobject]@{
                OffsetMilliseconds = [int](($now - $started).TotalMilliseconds)
                ProcessId = $process.Id
                ParentId = $info.ParentId
                ParentName = $info.ParentName
                Path = $info.Path
                CommandLine = $info.CommandLine
                SeenAt = $now
                Exited = $false
            }
            $seen[$key] = $record
            $events += [pscustomobject]@{
                event = "START"
                offsetMilliseconds = $record.OffsetMilliseconds
                processId = $record.ProcessId
                parentId = $record.ParentId
                parentName = $record.ParentName
                path = $record.Path
                commandLine = $record.CommandLine
            }
            Write-Host "START +$($record.OffsetMilliseconds)ms PID=$($record.ProcessId) parent=$($record.ParentName)/$($record.ParentId)"
        }
    }

    foreach ($key in @($seen.Keys)) {
        $record = $seen[$key]
        if (-not $record.Exited -and $runningIds -notcontains [int]$record.ProcessId) {
            $record.Exited = $true
            $lifetime = [int](($now - $record.SeenAt).TotalMilliseconds)
            $events += [pscustomobject]@{
                event = "EXIT"
                offsetMilliseconds = [int](($now - $started).TotalMilliseconds)
                processId = $record.ProcessId
                lifetimeMilliseconds = $lifetime
            }
            Write-Host "EXIT  +$([int](($now - $started).TotalMilliseconds))ms PID=$($record.ProcessId) lifetime=${lifetime}ms"
        }
    }

    Start-Sleep -Milliseconds 20
}

$report = [pscustomobject]@{
    profilePath = $profileInfo.Path
    observedAt = $started.ToString("o")
    observeSeconds = $ObserveSeconds
    events = @($events)
}
$report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $ReportPath -Encoding UTF8

if ($StopAfterObserve) {
    Stop-ExistingProfileTarget -ProfileInfo $profileInfo
}

Write-Host "GAME_LAUNCH_OBSERVE_OK"
Write-Host "REPORT=$ReportPath"

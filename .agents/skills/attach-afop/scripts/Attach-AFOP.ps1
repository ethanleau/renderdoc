[CmdletBinding()]
param(
    [string]$RepoRoot = "D:\renderdoc",
    [string]$CaptureDirectory = (Join-Path ([Environment]::GetFolderPath("MyDocuments")) "YeeCapture"),
    [ValidateRange(30, 300)]
    [int]$LaunchTimeoutSeconds = 120,
    [ValidateRange(10, 120)]
    [int]$AttachTimeoutSeconds = 45,
    [ValidateRange(500, 7000)]
    [int]$StableMilliseconds = 5000,
    [ValidateRange(5, 300)]
    [int]$PostAttachStabilitySeconds = 60,
    [ValidateRange(1, 2)]
    [int]$MaxAttempts = 2,
    [switch]$PreflightOnly
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

$steamAppId = 2840770
$commandExe = Join-Path $RepoRoot "x64\Release\yeecapturecmd.exe"
$logDirectory = Join-Path $env:LOCALAPPDATA "Temp\YeeCapture"
$statePath = Join-Path $logDirectory "AFOPAttachState.json"
$captureTemplate = Join-Path $CaptureDirectory "afop"

function Resolve-SteamExecutable {
    $candidates = New-Object System.Collections.Generic.List[string]
    $steamRegistry = Get-ItemProperty -LiteralPath "HKCU:\Software\Valve\Steam" -ErrorAction SilentlyContinue
    if ($null -ne $steamRegistry -and $steamRegistry.PSObject.Properties.Name -contains "SteamExe") {
        $candidates.Add(([string]$steamRegistry.SteamExe).Replace("/", "\"))
    }
    $candidates.Add("C:\Program Files (x86)\Steam\steam.exe")

    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path -LiteralPath $candidate -PathType Leaf)) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }

    throw "Steam executable was not found."
}

function Assert-Prerequisites {
    param([string]$SteamExecutable)

    $requiredFiles = @($SteamExecutable, $commandExe, (Join-Path $RepoRoot "x64\Release\yeecapture.dll"))
    foreach ($requiredFile in $requiredFiles) {
        if (-not (Test-Path -LiteralPath $requiredFile -PathType Leaf)) {
            throw "Required file is missing: $requiredFile"
        }
    }

    New-Item -ItemType Directory -Path $CaptureDirectory -Force | Out-Null
    New-Item -ItemType Directory -Path $logDirectory -Force | Out-Null

    $steamRoot = Split-Path -Parent $SteamExecutable
    $steamConfigs = @(Get-ChildItem -Path (Join-Path $steamRoot "userdata\*\config\localconfig.vdf") -File -ErrorAction SilentlyContinue)
    $overlayDisabled = $false
    foreach ($steamConfig in $steamConfigs) {
        $configText = Get-Content -LiteralPath $steamConfig.FullName -Raw
        $appBlocks = [regex]::Matches($configText, '"2840770"\s*\{(?<body>[^{}]*)\}')
        foreach ($appBlock in $appBlocks) {
            if ($appBlock.Groups["body"].Value -match '"OverlayAppEnable"\s*"0"') {
                $overlayDisabled = $true
                break
            }
        }
        if ($overlayDisabled) {
            break
        }
    }

    if (-not $overlayDisabled) {
        throw "Steam Overlay must be disabled for AFOP (app 2840770) before attaching YeeCapture."
    }
}

function Stop-ExistingAFOP {
    param([string]$SteamExecutable)

    $existing = @(Get-Process -Name "afop" -ErrorAction SilentlyContinue)
    if ($existing.Count -eq 0) {
        return
    }

    Write-Host "Stopping the existing AFOP instance so injection can happen before D3D12 initialization..."
    Start-Process -FilePath $SteamExecutable -ArgumentList "steam://stop/$steamAppId" -WindowStyle Hidden | Out-Null

    $graceDeadline = (Get-Date).AddSeconds(12)
    do {
        Start-Sleep -Milliseconds 250
        $existing = @(Get-Process -Name "afop" -ErrorAction SilentlyContinue)
    } while ($existing.Count -gt 0 -and (Get-Date) -lt $graceDeadline)

    foreach ($process in $existing) {
        Write-Host "AFOP did not exit cleanly; stopping PID $($process.Id)."
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
    }

    if ($existing.Count -gt 0) {
        Start-Sleep -Seconds 5
    }
}

function Wait-ForStableAttachedProcess {
    param(
        [System.Diagnostics.Process]$Process,
        [int]$StabilitySeconds
    )

    $processId = $Process.Id
    Write-Host "Attach markers verified; confirming AFOP remains alive for $StabilitySeconds seconds..."
    $deadline = (Get-Date).AddSeconds($StabilitySeconds)
    while ((Get-Date) -lt $deadline) {
        $Process.Refresh()
        if ($Process.HasExited) {
            $Process.WaitForExit()
            $exitCode = [int]$Process.ExitCode
            $exitBits = [System.BitConverter]::ToUInt32([System.BitConverter]::GetBytes($exitCode), 0)
            throw "AFOP PID $processId exited during the post-attach stability check with exit code 0x$($exitBits.ToString('X8')) ($exitCode)."
        }
        Start-Sleep -Milliseconds 250
    }
}

function Wait-ForRealAFOP {
    param(
        [datetime]$LaunchStarted,
        [int]$TimeoutSeconds,
        [int]$MinimumLifetimeMilliseconds,
        [string]$SteamExecutable
    )

    $firstSeen = @{}
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    $nextLaunchRetry = (Get-Date).AddSeconds(12)

    while ((Get-Date) -lt $deadline) {
        $processes = @(Get-Process -Name "afop" -ErrorAction SilentlyContinue | Sort-Object StartTime)
        if ($processes.Count -eq 0 -and (Get-Date) -ge $nextLaunchRetry) {
            Write-Host "AFOP has not appeared yet; reissuing the Steam launch request."
            Start-Process -FilePath $SteamExecutable -ArgumentList "-applaunch $steamAppId" -WindowStyle Hidden | Out-Null
            $nextLaunchRetry = (Get-Date).AddSeconds(12)
        }

        foreach ($process in $processes) {
            try {
                if ($process.StartTime -lt $LaunchStarted.AddSeconds(-2)) {
                    continue
                }
            }
            catch {
                continue
            }

            $pidKey = [string]$process.Id
            if (-not $firstSeen.ContainsKey($pidKey)) {
                $firstSeen[$pidKey] = Get-Date
                $parentName = $null
                $parentId = 0
                try {
                    $processInfo = Get-CimInstance -ClassName Win32_Process -Filter "ProcessId=$($process.Id)" -ErrorAction Stop
                    $parentId = [int]$processInfo.ParentProcessId
                    $parent = Get-Process -Id $parentId -ErrorAction Stop
                    $parentName = $parent.ProcessName
                }
                catch {
                    # Parent discovery is an optimisation. The lifetime fallback below
                    # remains available if the process disappears during inspection.
                }

                Write-Host "Observed AFOP candidate PID $($process.Id) (parent $parentName/$parentId)."

                # Steam launches a short-lived AFOP bootstrap process first. Ubisoft
                # launches the real game later. Selecting by parent lets injection
                # happen before Streamline maps its D3D12 proxy (about 2.4s after the
                # real process starts) instead of waiting out the old 5s heuristic.
                if ($parentName -and $parentName -ine "steam") {
                    $stillRunning = Get-Process -Id $process.Id -ErrorAction SilentlyContinue
                    if ($null -ne $stillRunning) {
                        Write-Host "Selected Ubisoft-launched AFOP immediately for pre-Streamline injection."
                        return $stillRunning
                    }
                }

                Write-Host "Waiting for this candidate to survive the bootstrap fallback window..."
            }

            $age = ((Get-Date) - [datetime]$firstSeen[$pidKey]).TotalMilliseconds
            if ($age -ge $MinimumLifetimeMilliseconds) {
                $stillRunning = Get-Process -Id $process.Id -ErrorAction SilentlyContinue
                if ($null -ne $stillRunning) {
                    return $stillRunning
                }
            }
        }

        Start-Sleep -Milliseconds 20
    }

    throw "The real AFOP process did not appear within $TimeoutSeconds seconds."
}

function Get-AFOPLog {
    param(
        [int]$ProcessId,
        [datetime]$NotBefore
    )

    $logs = @(Get-ChildItem -LiteralPath $logDirectory -Filter "RenderDoc_*.log" -File -ErrorAction SilentlyContinue |
        Where-Object { $_.LastWriteTime -ge $NotBefore.AddSeconds(-3) } |
        Sort-Object LastWriteTime -Descending)

    foreach ($log in $logs) {
        try {
            $content = Get-Content -LiteralPath $log.FullName -Raw -ErrorAction Stop
            $pidPattern = "RDOC\s+0*" + [regex]::Escape([string]$ProcessId) + ":"
            if ($content -match $pidPattern -and $content -match "Loading into .*\\afop\.exe") {
                return [pscustomobject]@{
                    Path = $log.FullName
                    Content = $content
                }
            }
        }
        catch {
            # The target may be writing this log while it is being read. Retry on the next poll.
        }
    }

    return $null
}

function Wait-ForVerifiedAttach {
    param(
        [System.Diagnostics.Process]$Process,
        [datetime]$InjectedAt,
        [int]$TimeoutSeconds
    )

    $processId = $Process.Id
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    $lastLog = $null

    while ((Get-Date) -lt $deadline) {
        $Process.Refresh()
        if ($Process.HasExited) {
            $Process.WaitForExit()
            $exitCode = [int]$Process.ExitCode
            $exitBits = [System.BitConverter]::ToUInt32([System.BitConverter]::GetBytes($exitCode), 0)
            throw "AFOP PID $processId exited before YeeCapture finished attaching with exit code 0x$($exitBits.ToString('X8')) ($exitCode)."
        }

        $lastLog = Get-AFOPLog -ProcessId $processId -NotBefore $InjectedAt
        if ($null -ne $lastLog) {
            $hasD3D12Device = $lastLog.Content.Contains("New D3D12 device created")
            $hasD3D12Capturer = $lastLog.Content.Contains("Adding D3D12 frame capturer")

            if ($hasD3D12Device -and $hasD3D12Capturer) {
                return $lastLog.Path
            }
        }

        Start-Sleep -Milliseconds 200
    }

    if ($null -ne $lastLog) {
        throw "YeeCapture injection did not reach the verified D3D12 capture-ready state. Log: $($lastLog.Path)"
    }
    throw "No YeeCapture log was created for AFOP PID $ProcessId."
}

function Save-AttachState {
    param(
        [int]$ProcessId,
        [uint32]$TargetIdent,
        [string]$LogPath
    )

    $state = [pscustomobject]@{
        ProcessId = $ProcessId
        TargetIdent = $TargetIdent
        CaptureTemplate = $captureTemplate
        LogPath = $LogPath
        AttachedAt = (Get-Date).ToString("o")
    }
    $state | ConvertTo-Json | Set-Content -LiteralPath $statePath -Encoding UTF8
}

$steamExe = Resolve-SteamExecutable
Assert-Prerequisites -SteamExecutable $steamExe

if ($PreflightOnly) {
    Write-Host "AFOP_PREFLIGHT_OK"
    Write-Host "Steam=$steamExe"
    Write-Host "YeeCapture=$commandExe"
    Write-Host "CaptureDirectory=$CaptureDirectory"
    exit 0
}

$lastError = $null
for ($attempt = 1; $attempt -le $MaxAttempts; $attempt++) {
    try {
        Write-Host "AFOP attach attempt $attempt of $MaxAttempts"
        Stop-ExistingAFOP -SteamExecutable $steamExe

        $launchStarted = Get-Date
        Start-Process -FilePath $steamExe -ArgumentList "-applaunch $steamAppId" -WindowStyle Hidden | Out-Null
        $afop = Wait-ForRealAFOP -LaunchStarted $launchStarted -TimeoutSeconds $LaunchTimeoutSeconds -MinimumLifetimeMilliseconds $StableMilliseconds -SteamExecutable $steamExe
        Write-Host "Selected real AFOP process PID $($afop.Id)."

        $injectedAt = Get-Date
        # yeecapturecmd writes its success line to stderr. Windows PowerShell wraps stderr
        # as ErrorRecord objects, so temporarily avoid promoting that stream to an exception.
        $savedErrorActionPreference = $ErrorActionPreference
        $ErrorActionPreference = "Continue"
        try {
            $injectOutput = @(& $commandExe "inject" "--PID=$($afop.Id)" "-c" $captureTemplate 2>&1)
            $injectExitCode = $LASTEXITCODE
        }
        finally {
            $ErrorActionPreference = $savedErrorActionPreference
        }
        $injectText = ($injectOutput | ForEach-Object { $_.ToString() }) -join [Environment]::NewLine
        Write-Host $injectText

        if ($injectText -notmatch "Launched as ID\s+(\d+)") {
            throw "yeecapturecmd did not return a target-control ID (exit code $injectExitCode)."
        }
        $targetIdent = [uint32]$Matches[1]
        if ($injectExitCode -ne 0 -and $injectExitCode -ne [int]$targetIdent) {
            throw "yeecapturecmd returned target ID $targetIdent but an unexpected exit code $injectExitCode."
        }

        $verifiedLog = Wait-ForVerifiedAttach -Process $afop -InjectedAt $injectedAt -TimeoutSeconds $AttachTimeoutSeconds
        Wait-ForStableAttachedProcess -Process $afop -StabilitySeconds $PostAttachStabilitySeconds
        Save-AttachState -ProcessId $afop.Id -TargetIdent $targetIdent -LogPath $verifiedLog

        Write-Host "AFOP_ATTACH_OK"
        Write-Host "PID=$($afop.Id)"
        Write-Host "TARGET_IDENT=$targetIdent"
        Write-Host "LOG=$verifiedLog"
        Write-Host "CAPTURE_TEMPLATE=$captureTemplate"
        Write-Host "STATE=$statePath"
        Write-Host "AFOP is ready. Focus the game and press F12 to capture a frame."
        exit 0
    }
    catch {
        $lastError = $_.Exception.Message
        Write-Warning "Attach attempt $attempt failed: $lastError"
        if ($attempt -lt $MaxAttempts) {
            Write-Host "Retrying with a clean AFOP launch..."
        }
    }
}

Write-Error "AFOP_ATTACH_FAILED after $MaxAttempts attempts: $lastError"
exit 1

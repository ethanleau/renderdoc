[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$ProfilePath,
    [ValidateRange(30, 300)][int]$LaunchTimeoutSeconds = 150,
    [ValidateRange(1, 2)][int]$MaxAttempts = 1,
    [switch]$PreflightOnly
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "GameAttach.Common.ps1")

function Wait-ForNamedProcess {
    param(
        [string]$ProcessName,
        [datetime]$NotBefore,
        [int]$TimeoutSeconds
    )

    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ((Get-Date) -lt $deadline) {
        foreach ($process in @(Get-Process -Name $ProcessName -ErrorAction SilentlyContinue |
                Sort-Object StartTime -Descending)) {
            try {
                if ($process.StartTime -ge $NotBefore.AddSeconds(-1)) {
                    return $process
                }
            }
            catch { }
        }
        Start-Sleep -Milliseconds 20
    }
    throw "Process '$ProcessName' did not appear within $TimeoutSeconds seconds."
}

function Wait-ForSelectedTarget {
    param(
        [object]$ProfileInfo,
        [datetime]$NotBefore,
        [int]$TimeoutSeconds,
        [string]$RequiredParentName = ""
    )

    $targetConfig = Get-OptionalProperty $ProfileInfo.Profile "target"
    $selection = Get-OptionalProperty $ProfileInfo.Profile "selection"
    $expectedPath = [string](Get-OptionalProperty $targetConfig "expectedPath" "")
    $acceptParents = @(ConvertTo-NameArray (Get-OptionalProperty $selection "acceptParentNames" @()))
    $rejectParents = @(ConvertTo-NameArray (Get-OptionalProperty $selection "rejectParentNames" @()))
    $stableMilliseconds = [int](Get-OptionalProperty $selection "stableMilliseconds" 1000)
    $firstSeen = @{}

    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ((Get-Date) -lt $deadline) {
        foreach ($process in @(Get-Process -Name $ProfileInfo.ProcessName -ErrorAction SilentlyContinue |
                Sort-Object StartTime)) {
            try {
                if ($process.StartTime -lt $NotBefore.AddSeconds(-1)) {
                    continue
                }
            }
            catch { continue }

            $key = [string]$process.Id
            if (-not $firstSeen.ContainsKey($key)) {
                $info = Get-ProcessLaunchInfo -Process $process
                $firstSeen[$key] = [pscustomobject]@{ SeenAt = Get-Date; Info = $info }
                Write-Host "Observed target candidate PID $($process.Id) parent=$($info.ParentName)/$($info.ParentId) path=$($info.Path)"
            }

            $record = $firstSeen[$key]
            $info = $record.Info
            if (-not (Test-ExpectedTargetPath -ActualPath $info.Path -ExpectedPath $expectedPath)) {
                continue
            }

            $parentName = ConvertTo-ProcessBaseName $info.ParentName
            if ($rejectParents -icontains $parentName) {
                continue
            }

            if (-not [string]::IsNullOrWhiteSpace($RequiredParentName)) {
                if ($parentName -ieq (ConvertTo-ProcessBaseName $RequiredParentName)) {
                    return $process
                }

                try {
                    $modules = @($process.Modules | ForEach-Object ModuleName)
                    if ($modules -icontains "yeecapture.dll") {
                        return $process
                    }
                }
                catch { }
                continue
            }

            if ($acceptParents.Count -gt 0) {
                if ($acceptParents -icontains $parentName) {
                    return $process
                }
                continue
            }

            if (((Get-Date) - $record.SeenAt).TotalMilliseconds -ge $stableMilliseconds) {
                $stillRunning = Get-Process -Id $process.Id -ErrorAction SilentlyContinue
                if ($null -ne $stillRunning) {
                    return $stillRunning
                }
            }
        }

        Start-Sleep -Milliseconds 20
    }

    throw "No target candidate satisfied the profile selection rules within $TimeoutSeconds seconds."
}

function Wait-ForAttachReadiness {
    param(
        [object]$ProfileInfo,
        [System.Diagnostics.Process]$Target,
        [datetime]$InjectedAt,
        [uint32]$TargetIdent,
        [int]$TimeoutSeconds
    )

    $capture = Get-OptionalProperty $ProfileInfo.Profile "capture"
    $requireReadyLog = [bool](Get-OptionalProperty $capture "requireReadyLog" $true)
    $patterns = @((Get-OptionalProperty $capture "readyLogPatterns" @()) | Where-Object { $_ })
    $lastLog = $null
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)

    while ((Get-Date) -lt $deadline) {
        $Target.Refresh()
        if ($Target.HasExited) {
            $Target.WaitForExit()
            throw "Target PID $($Target.Id) exited before attach verification completed (exit $($Target.ExitCode))."
        }

        $loaded = $false
        try {
            $loaded = @($Target.Modules | ForEach-Object ModuleName) -icontains "yeecapture.dll"
        }
        catch { }

        $targetControlReady = $false
        if ($TargetIdent -eq 0 -or
            -not (Test-TargetControlListener -ProcessId $Target.Id -TargetIdent $TargetIdent)) {
            $discoveredIdent = Get-TargetControlIdentForProcess -ProcessId $Target.Id
            if ($discoveredIdent -gt 0) {
                $TargetIdent = $discoveredIdent
            }
        }
        if ($TargetIdent -gt 0) {
            $targetControlReady = Test-TargetControlListener -ProcessId $Target.Id -TargetIdent $TargetIdent
        }

        $lastLog = Get-TargetCaptureLog -ProcessId $Target.Id -NotBefore $InjectedAt -ProcessName $ProfileInfo.ProcessName
        $readyLog = -not $requireReadyLog
        if ($requireReadyLog -and $null -ne $lastLog) {
            if ($patterns.Count -eq 0) {
                $readyLog = $true
            }
            else {
                foreach ($pattern in $patterns) {
                    if ($lastLog.Content -match [string]$pattern) {
                        $readyLog = $true
                        break
                    }
                }
            }
        }

        if ($loaded -and $targetControlReady -and $readyLog) {
            return [pscustomobject]@{
                TargetIdent = $TargetIdent
                LogPath = $(if ($null -ne $lastLog) { $lastLog.Path } else { "" })
            }
        }

        Start-Sleep -Milliseconds 200
    }

    throw "YeeCapture did not reach the profile's verified ready state within $TimeoutSeconds seconds."
}

function Wait-ForTargetStability {
    param(
        [System.Diagnostics.Process]$Target,
        [int]$Seconds
    )

    Write-Host "Attach ready; checking stability for $Seconds seconds..."
    $deadline = (Get-Date).AddSeconds($Seconds)
    while ((Get-Date) -lt $deadline) {
        $Target.Refresh()
        if ($Target.HasExited) {
            $Target.WaitForExit()
            throw "Target exited during stability verification (exit $($Target.ExitCode))."
        }
        Start-Sleep -Milliseconds 250
    }
}

$profileInfo = Read-GameAttachProfile -ProfilePath $ProfilePath
$capture = Get-OptionalProperty $profileInfo.Profile "capture"
$captureTemplate = [string](Get-OptionalProperty $capture "template" "")
if ([string]::IsNullOrWhiteSpace($captureTemplate)) {
    throw "capture.template is required."
}
$captureDirectory = Split-Path -Parent $captureTemplate
if ($captureDirectory) {
    New-Item -ItemType Directory -Path $captureDirectory -Force | Out-Null
}

if ($PreflightOnly) {
    Write-Host "GAME_ATTACH_PREFLIGHT_OK"
    Write-Host "PROFILE=$($profileInfo.Path)"
    Write-Host "MODE=$($profileInfo.Mode)"
    Write-Host "TARGET=$($profileInfo.ProcessName).exe"
    exit 0
}

$lastError = ""
for ($attempt = 1; $attempt -le $MaxAttempts; $attempt++) {
    try {
        Write-Host "Game attach attempt $attempt of $MaxAttempts"
        Stop-ExistingProfileTarget -ProfileInfo $profileInfo

        $target = $null
        $targetIdent = [uint32]0
        $injectedAt = Get-Date
        $launchStarted = Get-Date

        if ($profileInfo.Mode -eq "launcher-child") {
            $selection = Get-OptionalProperty $profileInfo.Profile "selection"
            $launcherName = ConvertTo-ProcessBaseName ([string](Get-OptionalProperty $selection "launcherProcessName" ""))
            $launcher = @(Get-Process -Name $launcherName -ErrorAction SilentlyContinue |
                Sort-Object StartTime -Descending | Select-Object -First 1)

            if ($launcher.Count -gt 0) {
                $launcher = $launcher[0]
                Write-Host "Using the existing dedicated launcher PID $($launcher.Id)."
            }
            else {
                $launchStarted = Get-Date
                Start-ProfileProcess -ProcessConfig (Get-OptionalProperty $profileInfo.Profile "launch") | Out-Null
                $launcher = Wait-ForNamedProcess -ProcessName $launcherName -NotBefore $launchStarted -TimeoutSeconds ([Math]::Min(60, $LaunchTimeoutSeconds))
            }

            Write-Host "Injecting child-process capture into dedicated launcher PID $($launcher.Id)..."

            $options = @((Get-OptionalProperty $capture "options" @()))
            if ($options -notcontains "--opt-hook-children") {
                $options += "--opt-hook-children"
            }
            Invoke-YeeCaptureInjection -Command $profileInfo.Command -ProcessId $launcher.Id `
                -CaptureTemplate $captureTemplate -Options $options | Out-Null
            $injectedAt = Get-Date

            if ($launcher.StartTime -lt $launchStarted.AddSeconds(-1)) {
                $launchStarted = Get-Date
                Start-ProfileProcess -ProcessConfig (Get-OptionalProperty $profileInfo.Profile "launch") | Out-Null
            }

            $target = Wait-ForSelectedTarget -ProfileInfo $profileInfo -NotBefore $launchStarted `
                -TimeoutSeconds $LaunchTimeoutSeconds -RequiredParentName $launcherName
        }
        else {
            $launchStarted = Get-Date
            Start-ProfileProcess -ProcessConfig (Get-OptionalProperty $profileInfo.Profile "launch") | Out-Null
            $target = Wait-ForSelectedTarget -ProfileInfo $profileInfo -NotBefore $launchStarted `
                -TimeoutSeconds $LaunchTimeoutSeconds
            Write-Host "Selected target PID $($target.Id)."
            $injectedAt = Get-Date
            $options = @((Get-OptionalProperty $capture "options" @()))
            $targetIdent = Invoke-YeeCaptureInjection -Command $profileInfo.Command -ProcessId $target.Id `
                -CaptureTemplate $captureTemplate -Options $options
        }

        $attachTimeout = [int](Get-OptionalProperty $capture "attachTimeoutSeconds" 45)
        $ready = Wait-ForAttachReadiness -ProfileInfo $profileInfo -Target $target `
            -InjectedAt $injectedAt -TargetIdent $targetIdent -TimeoutSeconds $attachTimeout
        $stabilitySeconds = [int](Get-OptionalProperty $capture "postAttachStabilitySeconds" 60)
        Wait-ForTargetStability -Target $target -Seconds $stabilitySeconds

        $statePath = Save-GameAttachState -ProfileInfo $profileInfo -ProcessId $target.Id `
            -TargetIdent $ready.TargetIdent -CaptureTemplate $captureTemplate -LogPath $ready.LogPath

        Write-Host "GAME_ATTACH_OK"
        Write-Host "PROFILE=$($profileInfo.Path)"
        Write-Host "PID=$($target.Id)"
        Write-Host "TARGET_IDENT=$($ready.TargetIdent)"
        Write-Host "LOG=$($ready.LogPath)"
        Write-Host "STATE=$statePath"
        exit 0
    }
    catch {
        $lastError = $_.Exception.Message
        Write-Warning "Attempt $attempt failed: $lastError"
    }
}

Write-Error "GAME_ATTACH_FAILED after $MaxAttempts attempt(s): $lastError"
exit 1

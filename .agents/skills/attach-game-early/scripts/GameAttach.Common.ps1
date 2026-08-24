Set-StrictMode -Version 2.0

$script:YeeCaptureFirstTargetControlPort = 38920
$script:YeeCaptureLastTargetControlPort = 38927

function Get-OptionalProperty {
    param(
        [object]$Object,
        [string]$Name,
        [object]$Default = $null
    )

    if ($null -eq $Object) {
        return $Default
    }

    $property = $Object.PSObject.Properties[$Name]
    if ($null -eq $property -or $null -eq $property.Value) {
        return $Default
    }

    return $property.Value
}

function ConvertTo-ProcessBaseName {
    param([string]$Name)

    if ([string]::IsNullOrWhiteSpace($Name)) {
        return ""
    }

    return [IO.Path]::GetFileNameWithoutExtension($Name.Trim())
}

function ConvertTo-NameArray {
    param([object]$Value)

    $result = New-Object System.Collections.Generic.List[string]
    foreach ($item in @($Value)) {
        if ($null -eq $item) {
            continue
        }
        $name = ConvertTo-ProcessBaseName ([string]$item)
        if (-not [string]::IsNullOrWhiteSpace($name)) {
            $result.Add($name)
        }
    }
    return @($result)
}

function Read-GameAttachProfile {
    param([Parameter(Mandatory = $true)][string]$ProfilePath)

    if (-not (Test-Path -LiteralPath $ProfilePath -PathType Leaf)) {
        throw "Game attach profile was not found: $ProfilePath"
    }

    $resolvedPath = (Resolve-Path -LiteralPath $ProfilePath).Path
    $profile = Get-Content -LiteralPath $resolvedPath -Raw | ConvertFrom-Json

    if ([int](Get-OptionalProperty $profile "profileVersion" 0) -ne 1) {
        throw "Unsupported or missing profileVersion in $resolvedPath"
    }

    $slug = [string](Get-OptionalProperty $profile "slug" "")
    if ($slug -notmatch '^[a-z0-9]+(?:-[a-z0-9]+)*$') {
        throw "Profile slug must use lowercase hyphen-case: $slug"
    }

    $mode = [string](Get-OptionalProperty $profile "mode" "direct")
    if ($mode -notin @("direct", "launcher-child")) {
        throw "Profile mode must be direct or launcher-child: $mode"
    }

    $target = Get-OptionalProperty $profile "target"
    $processName = ConvertTo-ProcessBaseName ([string](Get-OptionalProperty $target "processName" ""))
    if ([string]::IsNullOrWhiteSpace($processName)) {
        throw "Profile target.processName is required."
    }

    $launch = Get-OptionalProperty $profile "launch"
    $launchPath = [string](Get-OptionalProperty $launch "filePath" "")
    if ([string]::IsNullOrWhiteSpace($launchPath) -or
        -not (Test-Path -LiteralPath $launchPath -PathType Leaf)) {
        throw "Profile launch.filePath is missing or invalid: $launchPath"
    }

    $repoRoot = [string](Get-OptionalProperty $profile "repoRoot" "D:\renderdoc")
    $command = Join-Path $repoRoot "x64\Release\yeecapturecmd.exe"
    $captureDll = Join-Path $repoRoot "x64\Release\yeecapture.dll"
    if (-not (Test-Path -LiteralPath $command -PathType Leaf) -or
        -not (Test-Path -LiteralPath $captureDll -PathType Leaf)) {
        throw "The custom x64 Release YeeCapture build is missing under $repoRoot"
    }

    if ($mode -eq "launcher-child") {
        $selection = Get-OptionalProperty $profile "selection"
        $launcherName = ConvertTo-ProcessBaseName ([string](Get-OptionalProperty $selection "launcherProcessName" ""))
        if ([string]::IsNullOrWhiteSpace($launcherName)) {
            throw "selection.launcherProcessName is required for launcher-child mode."
        }

        $sharedLaunchers = @(
            "steam", "steamwebhelper", "epicgameslauncher", "upc", "ubisoftconnect",
            "eadesktop", "eabackgroundservice", "galaxyclient", "battle.net"
        )
        if ($sharedLaunchers -icontains $launcherName) {
            throw "launcher-child mode refuses shared launcher process '$launcherName'."
        }
    }

    return [pscustomobject]@{
        Path = $resolvedPath
        Profile = $profile
        Slug = $slug
        Mode = $mode
        ProcessName = $processName
        RepoRoot = $repoRoot
        Command = $command
        CaptureDll = $captureDll
    }
}

function Start-ProfileProcess {
    param([Parameter(Mandatory = $true)][object]$ProcessConfig)

    $filePath = [string](Get-OptionalProperty $ProcessConfig "filePath" "")
    if ([string]::IsNullOrWhiteSpace($filePath) -or
        -not (Test-Path -LiteralPath $filePath -PathType Leaf)) {
        throw "Configured executable is missing: $filePath"
    }

    $parameters = @{ FilePath = $filePath; PassThru = $true }
    $arguments = [string](Get-OptionalProperty $ProcessConfig "arguments" "")
    $workingDirectory = [string](Get-OptionalProperty $ProcessConfig "workingDirectory" "")
    if (-not [string]::IsNullOrWhiteSpace($arguments)) {
        $parameters.ArgumentList = $arguments
    }
    if (-not [string]::IsNullOrWhiteSpace($workingDirectory)) {
        $parameters.WorkingDirectory = $workingDirectory
    }

    return Start-Process @parameters
}

function Get-ProcessLaunchInfo {
    param([Parameter(Mandatory = $true)][System.Diagnostics.Process]$Process)

    $parentId = 0
    $parentName = ""
    $path = ""
    $commandLine = ""
    try {
        $info = Get-CimInstance -ClassName Win32_Process -Filter "ProcessId=$($Process.Id)" -ErrorAction Stop
        $parentId = [int]$info.ParentProcessId
        $path = [string]$info.ExecutablePath
        $commandLine = [string]$info.CommandLine
        $parent = Get-Process -Id $parentId -ErrorAction Stop
        $parentName = $parent.ProcessName
    }
    catch {
        try { $path = $Process.Path } catch { }
    }

    return [pscustomobject]@{
        Process = $Process
        ProcessId = $Process.Id
        ParentId = $parentId
        ParentName = $parentName
        Path = $path
        CommandLine = $commandLine
    }
}

function Test-ExpectedTargetPath {
    param(
        [string]$ActualPath,
        [string]$ExpectedPath
    )

    if ([string]::IsNullOrWhiteSpace($ExpectedPath)) {
        return $true
    }
    if ([string]::IsNullOrWhiteSpace($ActualPath)) {
        return $false
    }

    try {
        $expected = [IO.Path]::GetFullPath($ExpectedPath).TrimEnd('\')
        $actual = [IO.Path]::GetFullPath($ActualPath).TrimEnd('\')
        return $actual.Equals($expected, [StringComparison]::OrdinalIgnoreCase)
    }
    catch {
        return $false
    }
}

function Test-ProcessMatchesProfileTarget {
    param(
        [Parameter(Mandatory = $true)][System.Diagnostics.Process]$Process,
        [Parameter(Mandatory = $true)][object]$ProfileInfo
    )

    if ($Process.ProcessName -ine $ProfileInfo.ProcessName) {
        return $false
    }

    $targetConfig = Get-OptionalProperty $ProfileInfo.Profile "target"
    $expectedPath = [string](Get-OptionalProperty $targetConfig "expectedPath" "")
    if ([string]::IsNullOrWhiteSpace($expectedPath)) {
        return $true
    }

    $info = Get-ProcessLaunchInfo -Process $Process
    return (Test-ExpectedTargetPath -ActualPath $info.Path -ExpectedPath $expectedPath)
}

function Get-ProfileTargetProcesses {
    param([Parameter(Mandatory = $true)][object]$ProfileInfo)

    $matching = New-Object System.Collections.Generic.List[System.Diagnostics.Process]
    foreach ($process in @(Get-Process -Name $ProfileInfo.ProcessName -ErrorAction SilentlyContinue)) {
        if (Test-ProcessMatchesProfileTarget -Process $process -ProfileInfo $ProfileInfo) {
            $matching.Add($process)
        }
    }
    return @($matching)
}

function Get-LoadedCaptureDllPath {
    param([Parameter(Mandatory = $true)][System.Diagnostics.Process]$Process)

    try {
        foreach ($module in @($Process.Modules)) {
            if ($module.ModuleName -ieq "yeecapture.dll") {
                return [string]$module.FileName
            }
        }
    }
    catch { }

    return ""
}

function Stop-ExistingProfileTarget {
    param([Parameter(Mandatory = $true)][object]$ProfileInfo)

    $allNamedProcesses = @(Get-Process -Name $ProfileInfo.ProcessName -ErrorAction SilentlyContinue)
    $targetConfig = Get-OptionalProperty $ProfileInfo.Profile "target"
    $expectedPath = [string](Get-OptionalProperty $targetConfig "expectedPath" "")
    if ($allNamedProcesses.Count -gt 0 -and [string]::IsNullOrWhiteSpace($expectedPath)) {
        throw "target.expectedPath is required before stopping an existing same-named process."
    }

    $existing = @(Get-ProfileTargetProcesses -ProfileInfo $ProfileInfo)
    if ($existing.Count -eq 0) {
        if ($allNamedProcesses.Count -gt 0) {
            Write-Host "Ignoring $($allNamedProcesses.Count) same-named process(es) whose executable path does not match this profile."
        }
        return
    }

    $processIds = ($existing | ForEach-Object { [string]$_.Id }) -join ", "
    Write-Host "Early attach requires restarting the matching target process PID(s): $processIds"

    $stopConfig = Get-OptionalProperty $ProfileInfo.Profile "stop"
    $stopPath = [string](Get-OptionalProperty $stopConfig "filePath" "")
    if (-not [string]::IsNullOrWhiteSpace($stopPath)) {
        Write-Host "Requesting a clean stop for $($ProfileInfo.Profile.displayName)..."
        Start-ProfileProcess -ProcessConfig $stopConfig | Out-Null
    }

    $graceSeconds = [int](Get-OptionalProperty $stopConfig "graceSeconds" 15)
    $deadline = (Get-Date).AddSeconds([Math]::Max(1, $graceSeconds))
    while ((Get-Date) -lt $deadline -and
           @(Get-ProfileTargetProcesses -ProfileInfo $ProfileInfo).Count -gt 0) {
        Start-Sleep -Milliseconds 250
    }

    $remaining = @(Get-ProfileTargetProcesses -ProfileInfo $ProfileInfo)
    if ($remaining.Count -eq 0) {
        return
    }

    $force = [bool](Get-OptionalProperty $stopConfig "forceAfterGrace" $false)
    if (-not $force) {
        throw "The existing target is still running. Configure a clean stop command or explicitly set stop.forceAfterGrace."
    }

    foreach ($process in $remaining) {
        Stop-Process -Id $process.Id -Force -ErrorAction Stop
    }
    Start-Sleep -Seconds 2
}

function Invoke-YeeCaptureInjection {
    param(
        [Parameter(Mandatory = $true)][string]$Command,
        [Parameter(Mandatory = $true)][int]$ProcessId,
        [Parameter(Mandatory = $true)][string]$CaptureTemplate,
        [string[]]$Options = @()
    )

    foreach ($option in $Options) {
        if ($option -notmatch '^--opt-[a-z0-9-]+(?:=.*)?$') {
            throw "Unsupported capture option in profile: $option"
        }
    }

    $arguments = @("inject", "--PID=$ProcessId", "-c", $CaptureTemplate) + @($Options)
    $savedErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        $output = @(& $Command @arguments 2>&1)
        $exitCode = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $savedErrorActionPreference
    }

    $text = ($output | ForEach-Object { $_.ToString() }) -join [Environment]::NewLine
    Write-Host $text
    if ($text -notmatch "Launched as ID\s+(\d+)") {
        throw "YeeCapture injection did not return a target-control ID (exit $exitCode)."
    }

    return [uint32]$Matches[1]
}

function Get-TargetControlIdentForProcess {
    param([int]$ProcessId)

    try {
        $ports = @(Get-NetTCPConnection -State Listen -OwningProcess $ProcessId -ErrorAction Stop |
            Where-Object {
                $_.LocalPort -ge $script:YeeCaptureFirstTargetControlPort -and
                $_.LocalPort -le $script:YeeCaptureLastTargetControlPort
            } |
            Sort-Object LocalPort |
            Select-Object -ExpandProperty LocalPort -Unique)
        if ($ports.Count -gt 0) {
            return [uint32]$ports[0]
        }
    }
    catch { }

    return [uint32]0
}

function Test-TargetControlListener {
    param(
        [int]$ProcessId,
        [uint32]$TargetIdent
    )

    if ($TargetIdent -lt $script:YeeCaptureFirstTargetControlPort -or
        $TargetIdent -gt $script:YeeCaptureLastTargetControlPort) {
        return $false
    }

    try {
        $listeners = @(Get-NetTCPConnection -State Listen -OwningProcess $ProcessId -ErrorAction Stop |
            Where-Object { [uint32]$_.LocalPort -eq $TargetIdent })
        return ($listeners.Count -gt 0)
    }
    catch {
        return $false
    }
}

function Get-TargetCaptureLog {
    param(
        [int]$ProcessId,
        [datetime]$NotBefore,
        [string]$ProcessName
    )

    $logDirectory = Join-Path $env:LOCALAPPDATA "Temp\YeeCapture"
    foreach ($log in @(Get-ChildItem -LiteralPath $logDirectory -Filter "RenderDoc*.log" -File -ErrorAction SilentlyContinue |
            Where-Object { $_.LastWriteTime -ge $NotBefore.AddSeconds(-3) } |
            Sort-Object LastWriteTime -Descending)) {
        try {
            $content = Get-Content -LiteralPath $log.FullName -Raw -ErrorAction Stop
            $pidPattern = "RDOC\s+0*" + [regex]::Escape([string]$ProcessId) + ":"
            $processPattern = "Loading into .*[/\\]" + [regex]::Escape($ProcessName) + "\.exe"
            if ($content -match $pidPattern -and $content -match $processPattern) {
                return [pscustomobject]@{ Path = $log.FullName; Content = $content }
            }
        }
        catch { }
    }

    return $null
}

function Get-AttachStateDirectory {
    $directory = Join-Path $env:LOCALAPPDATA "Temp\YeeCapture\AttachGameEarly"
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
    return $directory
}

function Save-GameAttachState {
    param(
        [object]$ProfileInfo,
        [int]$ProcessId,
        [uint32]$TargetIdent,
        [string]$CaptureTemplate,
        [string]$LogPath
    )

    $process = Get-Process -Id $ProcessId -ErrorAction SilentlyContinue
    if ($null -eq $process -or $process.ProcessName -ine $ProfileInfo.ProcessName) {
        throw "The verified target process is no longer running."
    }

    $processInfo = Get-ProcessLaunchInfo -Process $process
    if ([string]::IsNullOrWhiteSpace($processInfo.Path)) {
        throw "Could not verify the target executable path before saving attach state."
    }

    $targetConfig = Get-OptionalProperty $ProfileInfo.Profile "target"
    $expectedPath = [string](Get-OptionalProperty $targetConfig "expectedPath" "")
    if (-not (Test-ExpectedTargetPath -ActualPath $processInfo.Path -ExpectedPath $expectedPath)) {
        throw "The verified target executable path no longer matches the profile."
    }

    $loadedCaptureDll = Get-LoadedCaptureDllPath -Process $process
    if ([string]::IsNullOrWhiteSpace($loadedCaptureDll) -or
        -not (Test-ExpectedTargetPath -ActualPath $loadedCaptureDll -ExpectedPath $ProfileInfo.CaptureDll)) {
        throw "The target is not using this profile's custom yeecapture.dll."
    }

    if (-not (Test-TargetControlListener -ProcessId $ProcessId -TargetIdent $TargetIdent)) {
        throw "Target-control ID $TargetIdent is not listening in target PID $ProcessId."
    }

    $stateDirectory = Get-AttachStateDirectory
    $state = [pscustomobject]@{
        StateVersion = 2
        ProfilePath = $ProfileInfo.Path
        Slug = $ProfileInfo.Slug
        DisplayName = [string](Get-OptionalProperty $ProfileInfo.Profile "displayName" $ProfileInfo.Slug)
        ProcessName = $ProfileInfo.ProcessName
        ProcessId = $ProcessId
        ProcessStartTimeUtc = $process.StartTime.ToUniversalTime().ToString("o")
        ProcessPath = $processInfo.Path
        TargetIdent = $TargetIdent
        CaptureDllPath = $loadedCaptureDll
        CaptureTemplate = $CaptureTemplate
        LogPath = $LogPath
        Mode = $ProfileInfo.Mode
        AttachedAt = (Get-Date).ToString("o")
    }

    $json = $state | ConvertTo-Json -Depth 5
    $slugPath = Join-Path $stateDirectory ($ProfileInfo.Slug + ".json")
    $latestPath = Join-Path $stateDirectory "Latest.json"
    $json | Set-Content -LiteralPath $slugPath -Encoding UTF8
    $json | Set-Content -LiteralPath $latestPath -Encoding UTF8
    return $slugPath
}


function Assert-GameAttachStateProcess {
    param(
        [Parameter(Mandatory = $true)][object]$State,
        [Parameter(Mandatory = $true)][object]$ProfileInfo,
        [Parameter(Mandatory = $true)][System.Diagnostics.Process]$Process
    )

    if ([int](Get-OptionalProperty $State "StateVersion" 0) -ne 2) {
        throw "The attach state predates identity verification. Run attach again to create a version 2 state."
    }
    if ([string](Get-OptionalProperty $State "Slug" "") -cne $ProfileInfo.Slug) {
        throw "The attach state does not belong to the referenced profile."
    }
    if ($Process.ProcessName -ine [string](Get-OptionalProperty $State "ProcessName" "")) {
        throw "The attached game process identity no longer matches the saved state."
    }

    $savedStartText = [string](Get-OptionalProperty $State "ProcessStartTimeUtc" "")
    try {
        $savedStart = [datetime]::Parse(
            $savedStartText,
            [Globalization.CultureInfo]::InvariantCulture,
            [Globalization.DateTimeStyles]::RoundtripKind).ToUniversalTime()
    }
    catch {
        throw "The attach state has an invalid process start time. Run attach again."
    }
    $actualStart = $Process.StartTime.ToUniversalTime()
    if ([Math]::Abs(($actualStart - $savedStart).TotalMilliseconds) -gt 1000) {
        throw "The saved PID has been reused by a different process instance. Run attach again."
    }

    $processInfo = Get-ProcessLaunchInfo -Process $Process
    $savedProcessPath = [string](Get-OptionalProperty $State "ProcessPath" "")
    if ([string]::IsNullOrWhiteSpace($savedProcessPath) -or
        -not (Test-ExpectedTargetPath -ActualPath $processInfo.Path -ExpectedPath $savedProcessPath)) {
        throw "The running executable path does not match the saved attach state."
    }
    if (-not (Test-ProcessMatchesProfileTarget -Process $Process -ProfileInfo $ProfileInfo)) {
        throw "The running executable no longer matches the profile target."
    }

    $loadedCaptureDll = Get-LoadedCaptureDllPath -Process $Process
    $savedCaptureDll = [string](Get-OptionalProperty $State "CaptureDllPath" "")
    if ([string]::IsNullOrWhiteSpace($savedCaptureDll) -or
        -not (Test-ExpectedTargetPath -ActualPath $loadedCaptureDll -ExpectedPath $savedCaptureDll) -or
        -not (Test-ExpectedTargetPath -ActualPath $loadedCaptureDll -ExpectedPath $ProfileInfo.CaptureDll)) {
        throw "The verified custom yeecapture.dll is no longer loaded in the target."
    }

    $targetIdent = [uint32](Get-OptionalProperty $State "TargetIdent" 0)
    if (-not (Test-TargetControlListener -ProcessId $Process.Id -TargetIdent $targetIdent)) {
        throw "Target-control ID $targetIdent is not listening in the saved target process."
    }

    return $targetIdent
}

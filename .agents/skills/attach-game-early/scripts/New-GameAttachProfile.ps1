[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][ValidatePattern('^[a-z0-9]+(?:-[a-z0-9]+)*$')][string]$Slug,
    [Parameter(Mandatory = $true)][string]$DisplayName,
    [Parameter(Mandatory = $true)][string]$TargetProcessName,
    [Parameter(Mandatory = $true)][string]$LaunchExecutable,
    [string]$LaunchArguments = "",
    [string]$LaunchWorkingDirectory = "",
    [string]$ExpectedPath = "",
    [ValidateSet("direct", "launcher-child")][string]$Mode = "direct",
    [string]$LauncherProcessName = "",
    [string[]]$AcceptParentNames = @(),
    [string[]]$RejectParentNames = @(),
    [ValidateRange(20, 10000)][int]$StableMilliseconds = 1000,
    [string]$StopExecutable = "",
    [string]$StopArguments = "",
    [ValidateRange(1, 120)][int]$StopGraceSeconds = 15,
    [switch]$ForceAfterGrace,
    [ValidateRange(5, 300)][int]$AttachTimeoutSeconds = 45,
    [ValidateRange(5, 300)][int]$PostAttachStabilitySeconds = 60,
    [switch]$NoReadyLog,
    [string]$RepoRoot = "D:\renderdoc",
    [string]$OutputPath = ""
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "GameAttach.Common.ps1")

if (-not (Test-Path -LiteralPath $LaunchExecutable -PathType Leaf)) {
    throw "Launch executable was not found: $LaunchExecutable"
}
if ($StopExecutable -and -not (Test-Path -LiteralPath $StopExecutable -PathType Leaf)) {
    throw "Stop executable was not found: $StopExecutable"
}
if ($Mode -eq "launcher-child" -and [string]::IsNullOrWhiteSpace($LauncherProcessName)) {
    throw "LauncherProcessName is required in launcher-child mode."
}

if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $profileDirectory = Join-Path $env:LOCALAPPDATA "YeeCapture\AttachProfiles"
    New-Item -ItemType Directory -Path $profileDirectory -Force | Out-Null
    $OutputPath = Join-Path $profileDirectory ($Slug + ".json")
}

$captureDirectory = Join-Path ([Environment]::GetFolderPath("MyDocuments")) "YeeCapture"
New-Item -ItemType Directory -Path $captureDirectory -Force | Out-Null

$profile = [ordered]@{
    profileVersion = 1
    slug = $Slug
    displayName = $DisplayName
    mode = $Mode
    repoRoot = $RepoRoot
    target = [ordered]@{
        processName = ConvertTo-ProcessBaseName $TargetProcessName
        expectedPath = $ExpectedPath
    }
    launch = [ordered]@{
        filePath = $LaunchExecutable
        arguments = $LaunchArguments
        workingDirectory = $LaunchWorkingDirectory
    }
    stop = [ordered]@{
        filePath = $StopExecutable
        arguments = $StopArguments
        graceSeconds = $StopGraceSeconds
        forceAfterGrace = [bool]$ForceAfterGrace
    }
    selection = [ordered]@{
        launcherProcessName = ConvertTo-ProcessBaseName $LauncherProcessName
        acceptParentNames = @(ConvertTo-NameArray $AcceptParentNames)
        rejectParentNames = @(ConvertTo-NameArray $RejectParentNames)
        stableMilliseconds = $StableMilliseconds
    }
    capture = [ordered]@{
        template = Join-Path $captureDirectory $Slug
        options = @()
        attachTimeoutSeconds = $AttachTimeoutSeconds
        postAttachStabilitySeconds = $PostAttachStabilitySeconds
        requireReadyLog = (-not $NoReadyLog)
        readyLogPatterns = @("Adding (D3D11|D3D12|Vulkan|OpenGL) frame capturer")
    }
}

$parent = Split-Path -Parent $OutputPath
if ($parent) {
    New-Item -ItemType Directory -Path $parent -Force | Out-Null
}
$profile | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $OutputPath -Encoding UTF8

Write-Host "GAME_ATTACH_PROFILE_CREATED"
Write-Host "PROFILE=$OutputPath"

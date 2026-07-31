<#
.SYNOPSIS
    Removes the F.E.A.R. VR installation.

.DESCRIPTION
    Deletes the install folder and the desktop shortcut. The retail
    installation was never written to and stays untouched; a Steam file
    verification is not needed.

    Saved games and profiles live in <InstallDir>\userdata and are kept
    unless -IncludeUserData is given.

    Without -Apply the run is a dry run.

.PARAMETER InstallDir
    Install folder to remove. Default: %USERPROFILE%\FearVR

    Example: -InstallDir "D:\Games\FearVR"

.PARAMETER IncludeUserData
    Also deletes <InstallDir>\userdata, i.e. saved games, profiles and
    screenshots.

.PARAMETER Apply
    Actually deletes. Without it nothing is changed.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File tools\release\uninstall.ps1

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File tools\release\uninstall.ps1 -Apply
#>
[CmdletBinding()]
param(
    [string]$InstallDir = (Join-Path $env:USERPROFILE 'FearVR'),

    [switch]$IncludeUserData,

    [switch]$Apply
)

$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\_fearvr-release.ps1"

$mode = if ($Apply) { 'APPLY' } else { 'DRY RUN' }
Write-Host "=== F.E.A.R. VR - Uninstall ($mode) ===" -ForegroundColor Cyan

if (-not (Test-Path -LiteralPath $InstallDir -PathType Container)) {
    Write-Host "No installation in '$InstallDir'."
    Write-Host 'If it was installed elsewhere, pass -InstallDir "<path>",'
    Write-Host '  for example: -InstallDir "D:\Games\FearVR"'
    return
}

$deploymentPath = Join-Path $InstallDir 'deployment.json'
$retailRoot = $null
$deploymentData = $null
if (Test-Path -LiteralPath $deploymentPath -PathType Leaf) {
    $deploymentData = Get-Content -Raw -LiteralPath $deploymentPath |
        ConvertFrom-Json
    $retailRoot = $deploymentData.retailRoot
}
$retailBefore = $null
if ($retailRoot -and (Test-Path -LiteralPath $retailRoot -PathType Container)) {
    $retailBefore = Assert-RetailFearExe $retailRoot
}

function Get-SizeMb([string]$Path) {
    $bytes = (Get-ChildItem -LiteralPath $Path -Recurse -File `
        -ErrorAction SilentlyContinue | Measure-Object -Property Length -Sum).Sum
    return [math]::Round(($bytes / 1MB), 1)
}

# Remove only the verified root-level proxies that belonged to this
# deployment. A foreign replacement remains untouched. Development installs
# may also have preserved a previous D3D9 wrapper for restoration.
if ($deploymentData -and $retailRoot) {
    foreach ($proxy in @(
        @{
            Path = $deploymentData.dinputProxy
            Hash = $deploymentData.dinputProxySha256
            Name = 'dinput8.dll'
        },
        @{
            Path = $deploymentData.d3d9Proxy
            Hash = $deploymentData.d3d9ProxySha256
            Name = 'd3d9.dll'
        }
    )) {
        if ([string]::IsNullOrWhiteSpace($proxy.Path) -or
            [string]::IsNullOrWhiteSpace($proxy.Hash) -or
            -not (Test-Path -LiteralPath $proxy.Path -PathType Leaf) -or
            (Get-FileSha256 $proxy.Path) -ne $proxy.Hash) {
            continue
        }
        $upstream = if ($proxy.Name -eq 'd3d9.dll') {
            Join-Path $retailRoot 'd3d9.fearvr-upstream.dll'
        } else {
            $null
        }
        if ($upstream -and
            (Test-Path -LiteralPath $upstream -PathType Leaf)) {
            Write-Host "  * restore previous $($proxy.Name)"
            if ($Apply) {
                Move-Item -LiteralPath $upstream `
                    -Destination $proxy.Path -Force
            }
        } else {
            Write-Host "  * remove root proxy $($proxy.Name)"
            if ($Apply) {
                Remove-Item -LiteralPath $proxy.Path -Force
            }
        }
    }
}

# userdata is the game's -userdirectory: saved games, profiles, screenshots.
# User data is never deleted without being asked for.
foreach ($entry in Get-ChildItem -LiteralPath $InstallDir -Force) {
    if ($entry.PSIsContainer -and $entry.Name -eq 'userdata' -and
        -not $IncludeUserData) {
        Write-Host ("  * keeping userdata (saved games, " +
                    "$(Get-SizeMb $entry.FullName) MB)")
        continue
    }
    $size = if ($entry.PSIsContainer) {
        Get-SizeMb $entry.FullName
    } else {
        [math]::Round(($entry.Length / 1MB), 1)
    }
    Write-Host "  * remove $($entry.Name) ($size MB)"
    if ($Apply) { Remove-Item -LiteralPath $entry.FullName -Recurse -Force }
}

if ($Apply -and -not $IncludeUserData) {
    Write-Host "  Note: '$InstallDir' is kept because of userdata."
} elseif ($Apply) {
    Remove-Item -LiteralPath $InstallDir -Recurse -Force -ErrorAction SilentlyContinue
}

# Only remove the shortcut that points at the folder being uninstalled. A
# second installation elsewhere keeps its own shortcut.
$shortcut = Join-Path ([Environment]::GetFolderPath('Desktop')) 'F.E.A.R. VR.lnk'
if (Test-Path -LiteralPath $shortcut -PathType Leaf) {
    $arguments = ''
    try {
        $arguments = (New-Object -ComObject WScript.Shell).CreateShortcut(
            $shortcut).Arguments
    } catch { }
    if ($arguments -like "*$InstallDir*") {
        Write-Host '  * remove desktop shortcut'
        if ($Apply) { Remove-Item -LiteralPath $shortcut -Force }
    } else {
        Write-Host '  * keeping desktop shortcut (points elsewhere)'
    }
}

if ($retailBefore) {
    $retailAfter = Assert-RetailFearExe $retailRoot
    if ($retailBefore.Sha256 -ne $retailAfter.Sha256) {
        throw 'SAFETY ABORT: the retail FEAR.exe was modified.'
    }
    Write-Host ''
    Write-Host 'Retail unchanged; a Steam file verification is not needed.'
}

Write-Host ''
if ($Apply) {
    Write-Host 'Uninstall complete.' -ForegroundColor Green
} else {
    Write-Host 'Dry run finished; nothing was changed.' -ForegroundColor Yellow
    Write-Host 'Re-run with -Apply to actually remove.'
}

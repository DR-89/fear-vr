<#
.SYNOPSIS
    Entfernt die F.E.A.R.-VR-Installation.

.DESCRIPTION
    Loescht den Installationsordner und die Desktop-Verknuepfung. Die
    Retail-Installation wurde nie beschrieben und bleibt unberuehrt; eine
    Steam-Dateipruefung ist nicht noetig.

    Spielstaende und Profile liegen in <InstallDir>\userdata und bleiben ohne
    -IncludeUserData erhalten.

    Ohne -Apply ist der Lauf ein Trockenlauf.
#>
[CmdletBinding()]
param(
    [string]$InstallDir = (Join-Path $env:USERPROFILE 'FearVR'),

    [switch]$IncludeUserData,

    [switch]$Apply
)

$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\_fearvr-release.ps1"

$mode = if ($Apply) { 'AUSFUEHRUNG' } else { 'TROCKENLAUF' }
Write-Host "=== F.E.A.R. VR - Deinstallation ($mode) ===" -ForegroundColor Cyan

if (-not (Test-Path -LiteralPath $InstallDir -PathType Container)) {
    Write-Host "Keine Installation in '$InstallDir'."
    return
}

$deploymentPath = Join-Path $InstallDir 'deployment.json'
$retailRoot = $null
if (Test-Path -LiteralPath $deploymentPath -PathType Leaf) {
    $retailRoot = (Get-Content -Raw -LiteralPath $deploymentPath |
        ConvertFrom-Json).retailRoot
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

# userdata ist das -userdirectory des Spiels: Spielstaende, Profile,
# Screenshots. Benutzerdaten werden nicht ungefragt geloescht.
foreach ($entry in Get-ChildItem -LiteralPath $InstallDir -Force) {
    if ($entry.PSIsContainer -and $entry.Name -eq 'userdata' -and
        -not $IncludeUserData) {
        Write-Host ("  * userdata bleibt erhalten (Spielstaende, " +
                    "$(Get-SizeMb $entry.FullName) MB)")
        continue
    }
    $size = if ($entry.PSIsContainer) {
        Get-SizeMb $entry.FullName
    } else {
        [math]::Round(($entry.Length / 1MB), 1)
    }
    Write-Host "  * $($entry.Name) entfernen ($size MB)"
    if ($Apply) { Remove-Item -LiteralPath $entry.FullName -Recurse -Force }
}

if ($Apply -and -not $IncludeUserData) {
    Write-Host "  Hinweis: '$InstallDir' bleibt wegen userdata bestehen."
} elseif ($Apply) {
    Remove-Item -LiteralPath $InstallDir -Recurse -Force -ErrorAction SilentlyContinue
}

$shortcut = Join-Path ([Environment]::GetFolderPath('Desktop')) 'F.E.A.R. VR.lnk'
if (Test-Path -LiteralPath $shortcut -PathType Leaf) {
    Write-Host '  * Desktop-Verknuepfung entfernen'
    if ($Apply) { Remove-Item -LiteralPath $shortcut -Force }
}

if ($retailBefore) {
    $retailAfter = Assert-RetailFearExe $retailRoot
    if ($retailBefore.Sha256 -ne $retailAfter.Sha256) {
        throw 'SICHERHEITSABBRUCH: Retail-FEAR.exe wurde veraendert.'
    }
    Write-Host ''
    Write-Host 'Retail unveraendert; eine Steam-Dateipruefung ist nicht noetig.'
}

Write-Host ''
if ($Apply) {
    Write-Host 'Deinstallation abgeschlossen.' -ForegroundColor Green
} else {
    Write-Host 'Trockenlauf beendet; es wurde nichts geaendert.' -ForegroundColor Yellow
    Write-Host 'Mit -Apply tatsaechlich ausfuehren.'
}

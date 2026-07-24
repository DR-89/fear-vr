<#
.SYNOPSIS
    Deaktiviert SteamVRs automatischen Theatermodus für Desktopspiele.

.DESCRIPTION
    Setzt ausschließlich steamvr.autoShowGameTheater in der persönlichen
    SteamVR-Konfiguration auf false. Vor der ersten Änderung wird eine
    datierte Sicherung unter dem Projekt-Logverzeichnis angelegt.
#>
[CmdletBinding()]
param(
    [string]$SettingsPath =
        'C:\Program Files (x86)\Steam\config\steamvr.vrsettings',

    [switch]$Quiet
)

$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\_fearvr-env.ps1"
$cfg = Get-FearVrConfig

if (-not (Test-Path -LiteralPath $SettingsPath -PathType Leaf)) {
    throw "SteamVR-Konfiguration fehlt: $SettingsPath"
}

$text = [IO.File]::ReadAllText($SettingsPath)
$valuePattern =
    '(?m)("autoShowGameTheater"\s*:\s*)(true|false)'
$changed = $false

if ([Text.RegularExpressions.Regex]::IsMatch($text, $valuePattern)) {
    $updated = [Text.RegularExpressions.Regex]::Replace(
        $text,
        $valuePattern,
        '${1}false'
    )
    $changed = $updated -cne $text
} else {
    $sectionPattern = '(?m)(^\s*"steamvr"\s*:\s*\{\s*\r?\n)'
    if (-not [Text.RegularExpressions.Regex]::IsMatch(
        $text,
        $sectionPattern
    )) {
        throw 'Abschnitt "steamvr" fehlt in steamvr.vrsettings.'
    }
    $updated = [Text.RegularExpressions.Regex]::Replace(
        $text,
        $sectionPattern,
        '${1}      "autoShowGameTheater" : false,' + [Environment]::NewLine,
        1
    )
    $changed = $true
}

if ($changed) {
    $backupDirectory = Assert-UnderProjectRoot (
        Join-Path $cfg.ProjectRoot 'logs\steamvr-settings-backups'
    )
    New-Item -ItemType Directory -Force -Path $backupDirectory |
        Out-Null
    $backupPath = Join-Path $backupDirectory (
        'steamvr-' +
        (Get-Date).ToUniversalTime().ToString('yyyyMMdd-HHmmss') +
        '.vrsettings'
    )
    Copy-Item -LiteralPath $SettingsPath -Destination $backupPath
    $utf8WithoutBom = New-Object Text.UTF8Encoding($false)
    [IO.File]::WriteAllText($SettingsPath, $updated, $utf8WithoutBom)
}

$verified =
    [IO.File]::ReadAllText($SettingsPath) |
    ConvertFrom-Json
if ($verified.steamvr.autoShowGameTheater -ne $false) {
    throw 'SteamVR-Theatermodus konnte nicht deaktiviert werden.'
}

if (-not $Quiet) {
    Write-Host 'SteamVR: automatischer Theatermodus ist deaktiviert.'
}

<#
.SYNOPSIS
    Entfernt alle lokalen F.E.A.R.-VR-Artefakte (ANWEISUNG.md §13, M6-Gate).

.DESCRIPTION
    Außerhalb der Projektwurzel entstehen genau drei Dateien:

      * steamvr.vrsettings — dort ausschließlich der Schlüssel
        steamvr.autoShowGameTheater. Er wird aus der ältesten Sicherung
        wiederhergestellt.
      * dinput8.dll und EchoPatch.ini im Retail-Verzeichnis, sofern EchoPatch
        installiert wurde. Beide werden wieder entfernt.

    Danach werden die projektinternen Arbeitsverzeichnisse gelöscht.

    Die Retail-EXE wird nur gelesen und vor sowie nach dem Lauf gegen ihren
    SHA-256 geprüft. Eine Steam-Dateiprüfung ist nicht nötig, weil an FEAR.exe
    nie geschrieben wurde.

    Ohne -Apply ist der Lauf ein reiner Trockenlauf und zeigt nur, was
    geschehen würde.

.PARAMETER Apply
    Führt die Änderungen tatsächlich aus.

.PARAMETER KeepLogs
    Behält logs\ einschließlich der SteamVR-Sicherungen.

.PARAMETER IncludeVendor
    Entfernt zusätzlich vendor-local\. Enthält die gepinnten
    Fremdquellen und die lokal installierten Public Tools; beides muss
    danach neu bereitgestellt werden.

.PARAMETER IncludeUserData
    Entfernt zusätzlich stage\userdata-*. Diese Verzeichnisse sind das
    -userdirectory des Spiels und enthalten Spielstände, Profile und
    Screenshots. Das sind Benutzerdaten und keine Moddateien, deshalb
    bleiben sie ohne diesen Schalter erhalten.

.PARAMETER Scope
    All          — SteamVR-Einstellung und Projektverzeichnisse (Standard).
    SteamVrOnly  — nur die SteamVR-Einstellung zurückstellen.
    ProjectOnly  — nur die Projektverzeichnisse entfernen. Sinnvoll, solange
                   SteamVR noch läuft und seine Konfiguration beim Beenden
                   ohnehin neu schreibt.
#>
[CmdletBinding()]
param(
    [switch]$Apply,

    [switch]$KeepLogs,

    [switch]$IncludeVendor,

    [switch]$IncludeUserData,

    [ValidateSet('All', 'SteamVrOnly', 'ProjectOnly')]
    [string]$Scope = 'All',

    [string]$SteamVrSettingsPath =
        'C:\Program Files (x86)\Steam\config\steamvr.vrsettings'
)

$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\_fearvr-env.ps1"
$cfg = Get-FearVrConfig

$mode = if ($Apply) { 'AUSFUEHRUNG' } else { 'TROCKENLAUF' }
Write-Host "=== F.E.A.R. VR — Deinstallation ($mode, Scope=$Scope) ===" `
    -ForegroundColor Cyan
$doSteamVr = $Scope -in @('All', 'SteamVrOnly')
$doProject = $Scope -in @('All', 'ProjectOnly')

function Step($text) { Write-Host "  * $text" }

# --- Retail nur lesen -------------------------------------------------------
$retailBefore = Assert-RetailFearExe
Write-Host "Retail verifiziert: $($retailBefore.Version) ($($retailBefore.Path))"

# --- 1. SteamVR-Theatermodus zurückstellen ----------------------------------
# Wiederhergestellt wird gezielt der eine Schlüssel, nicht die ganze Datei:
# SteamVR schreibt dort auch alles andere, was der Benutzer seither geändert
# hat. Ein Zurückkopieren der Sicherung würde diese Änderungen verwerfen.
Write-Host '--- SteamVR-Einstellung ---'
$backupDirectory = Join-Path $cfg.ProjectRoot 'logs\steamvr-settings-backups'
$oldestBackup = $null
if (Test-Path -LiteralPath $backupDirectory -PathType Container) {
    $oldestBackup = Get-ChildItem -LiteralPath $backupDirectory `
        -Filter 'steamvr-*.vrsettings' -File -ErrorAction SilentlyContinue |
        Sort-Object Name | Select-Object -First 1
}

if (-not $doSteamVr) {
    Step 'Übersprungen (Scope)'
} elseif (-not $oldestBackup) {
    Step 'Keine Sicherung vorhanden; autoShowGameTheater wird nicht angefasst.'
} elseif (-not (Test-Path -LiteralPath $SteamVrSettingsPath -PathType Leaf)) {
    Step "SteamVR-Konfiguration nicht vorhanden: $SteamVrSettingsPath"
} else {
    $originalJson = [IO.File]::ReadAllText($oldestBackup.FullName) |
        ConvertFrom-Json
    $originalValue = $null
    if ($originalJson.PSObject.Properties.Name -contains 'steamvr' -and
        $originalJson.steamvr.PSObject.Properties.Name -contains
            'autoShowGameTheater') {
        $originalValue = [bool]$originalJson.steamvr.autoShowGameTheater
    }

    $currentText = [IO.File]::ReadAllText($SteamVrSettingsPath)
    $valuePattern = '(?m)("autoShowGameTheater"\s*:\s*)(true|false)'
    $linePattern = '(?m)^[^\r\n]*"autoShowGameTheater"[^\r\n]*\r?\n'

    if ($null -eq $originalValue) {
        # Der Schlüssel existierte vorher nicht. Die von uns eingefügte Zeile
        # wird wieder entfernt, damit SteamVR seinen Standard verwendet.
        $updatedText = [Text.RegularExpressions.Regex]::Replace(
            $currentText, $linePattern, '', 1
        )
        $description = 'eingefügten Schlüssel autoShowGameTheater entfernen'
    } else {
        $updatedText = [Text.RegularExpressions.Regex]::Replace(
            $currentText,
            $valuePattern,
            ('${1}' + $originalValue.ToString().ToLowerInvariant())
        )
        $description =
            "autoShowGameTheater auf $($originalValue.ToString().ToLowerInvariant()) zurücksetzen"
    }

    if ($updatedText -ceq $currentText) {
        Step "Bereits im Ursprungszustand; keine Änderung nötig."
    } else {
        $steamVrRunning = @(
            Get-Process -Name 'vrserver', 'vrmonitor' -ErrorAction SilentlyContinue
        )
        if ($steamVrRunning.Count -gt 0) {
            Write-Host '    WARNUNG: SteamVR läuft und überschreibt die Datei beim Beenden. Erst SteamVR schließen.' `
                -ForegroundColor Yellow
        }
        Step "$description (Sicherung: $($oldestBackup.Name))"
        if ($Apply) {
            $utf8WithoutBom = New-Object Text.UTF8Encoding($false)
            [IO.File]::WriteAllText(
                $SteamVrSettingsPath, $updatedText, $utf8WithoutBom
            )
            $verify = [IO.File]::ReadAllText($SteamVrSettingsPath) |
                ConvertFrom-Json
            $now = $null
            if ($verify.steamvr.PSObject.Properties.Name -contains
                    'autoShowGameTheater') {
                $now = [bool]$verify.steamvr.autoShowGameTheater
            }
            if ($now -ne $originalValue) {
                throw 'autoShowGameTheater konnte nicht zurückgesetzt werden.'
            }
        }
    }
}

# --- 2. Stockmodule der Public Tools zurückstellen --------------------------
# Betrifft nur vendor-local\publictools, also die lokale SDK-Kopie des
# Benutzers, nicht die Retail-Installation.
Write-Host '--- Public-Tools-Module ---'
$stockBackupManifest =
    Join-Path $cfg.ProjectRoot 'stage\m0-stock-module-backup\manifest.json'
if (-not $doProject) {
    Step 'Übersprungen (Scope)'
} elseif (Test-Path -LiteralPath $stockBackupManifest -PathType Leaf) {
    Step 'Stockmodule aus stage\m0-stock-module-backup wiederherstellen'
    if ($Apply) {
        # Kein $LASTEXITCODE-Test: Der wird von & auf ein .ps1 nicht gesetzt.
        # Das gerufene Skript läuft mit ErrorActionPreference=Stop und wirft
        # bei jedem Fehler selbst.
        & "$PSScriptRoot\deploy-stock-game-modules.ps1" -Restore
    }
} else {
    Step 'Kein Modulbackup vorhanden; nichts wiederherzustellen.'
}

# --- 2b. EchoPatch aus dem Retail-Verzeichnis entfernen ---------------------
# Die beiden einzigen Dateien, die dieses Projekt in die Retail-Installation
# legt. FEAR.exe bleibt dabei unangetastet; eine fremde dinput8.dll fasst das
# gerufene Skript nicht an.
Write-Host '--- EchoPatch ---'
$echoDllPath = Join-Path $cfg.RetailRoot $cfg.EchoPatchDllName
if (-not $doProject) {
    Step 'Übersprungen (Scope)'
} elseif (Test-Path -LiteralPath $echoDllPath -PathType Leaf) {
    Step "EchoPatch aus $($cfg.RetailRoot) entfernen"
    if ($Apply) {
        & "$PSScriptRoot\install-echopatch.ps1" -Apply -Remove | Out-Null
    }
} else {
    Step 'Nicht installiert; nichts zu entfernen.'
}

# --- 3. Projektinterne Arbeitsverzeichnisse entfernen -----------------------
Write-Host '--- Projektverzeichnisse ---'
$removable = [ordered]@{
    'stage'         = $true
    'build'         = $true
    'dist'          = $true
    'local-runtime' = $true
    'logs'          = (-not $KeepLogs)
    'vendor-local'  = [bool]$IncludeVendor
}

function Get-SizeMb([string]$Path) {
    $bytes = (Get-ChildItem -LiteralPath $Path -Recurse -File `
        -ErrorAction SilentlyContinue |
        Measure-Object -Property Length -Sum).Sum
    return [math]::Round(($bytes / 1MB), 1)
}

foreach ($name in $removable.Keys) {
    $path = Assert-UnderProjectRoot (Join-Path $cfg.ProjectRoot $name)
    if (-not $doProject) {
        Step "$name bleibt erhalten (Scope)"
        continue
    }
    if (-not $removable[$name]) {
        Step "$name bleibt erhalten"
        continue
    }
    if (-not (Test-Path -LiteralPath $path -PathType Container)) {
        Step "$name ist nicht vorhanden"
        continue
    }

    # stage\userdata-* ist das -userdirectory des Spiels: Spielstände,
    # Profile und Screenshots. Benutzerdaten werden nicht ungefragt gelöscht,
    # deshalb wird stage\ eintragsweise geleert statt komplett entfernt.
    if ($name -eq 'stage' -and -not $IncludeUserData) {
        foreach ($entry in Get-ChildItem -LiteralPath $path -Force) {
            if ($entry.PSIsContainer -and $entry.Name -like 'userdata-*') {
                Step "stage\$($entry.Name) bleibt erhalten (Spielstände, $(Get-SizeMb $entry.FullName) MB)"
                continue
            }
            $size = if ($entry.PSIsContainer) {
                Get-SizeMb $entry.FullName
            } else {
                [math]::Round(($entry.Length / 1MB), 1)
            }
            Step "stage\$($entry.Name) entfernen ($size MB)"
            if ($Apply) {
                Remove-Item -LiteralPath $entry.FullName -Recurse -Force
            }
        }
        continue
    }

    Step "$name entfernen ($(Get-SizeMb $path) MB)"
    if ($Apply) {
        Remove-Item -LiteralPath $path -Recurse -Force
    }
}

# --- 4. Retail muss unverändert sein ----------------------------------------
$retailAfter = Assert-RetailFearExe
if ($retailBefore.Sha256 -ne $retailAfter.Sha256) {
    throw 'SICHERHEITSABBRUCH: Retail-FEAR.exe wurde verändert.'
}

Write-Host ''
if ($Apply) {
    Write-Host 'Deinstallation abgeschlossen; Retail unverändert.' `
        -ForegroundColor Green
    Write-Host 'Eine Steam-Dateiprüfung ist nicht nötig.'
} else {
    Write-Host 'Trockenlauf beendet; es wurde nichts geändert.' `
        -ForegroundColor Yellow
    Write-Host 'Mit -Apply tatsächlich ausführen.'
}
Write-Host 'Nicht entfernt: das Git-Arbeitsverzeichnis selbst und die Retail-Installation.'

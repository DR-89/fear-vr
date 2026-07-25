<#
.SYNOPSIS
    Richtet F.E.A.R. VR auf diesem Rechner ein.

.DESCRIPTION
    Legt eine isolierte Spielstage in einem eigenen Ordner an. Die
    Retail-Installation wird nur gelesen und nie beschrieben; eine
    Steam-Dateiprüfung bleibt deshalb sauber.

    Das Paket enthält nur eigene, MIT-lizenzierte Module. Die fünf
    Public-Tools-Module werden aus der lokalen Public-Tools-Installation
    dieses Rechners kopiert und liegen dem Paket bewusst nicht bei.

.PARAMETER InstallDir
    Zielordner. Standard: %USERPROFILE%\FearVR

    NICHT unterhalb von %LOCALAPPDATA% installieren: Die LithTech-Engine
    scheitert dort beim Laden der Archivkonfiguration mit
    "Failed to initialize client - unable to load game resources". Gemessen am
    25.07.2026 mit byteweise identischer archcfg an verschiedenen Orten; nur
    der Ort entscheidet. Das Skript lehnt solche Ziele deshalb ab.

.PARAMETER RetailRoot
    F.E.A.R.-Installationsordner. Wird sonst über die Steam-Bibliotheken
    gesucht.

.PARAMETER PublicToolsGame
    Verzeichnis Dev\Runtime\Game der Public Tools 1.08. Wird sonst an den
    üblichen Orten gesucht und über den Hash von GameClient.dll verifiziert.

.PARAMETER NoShortcut
    Legt keine Desktop-Verknüpfung an.
#>
[CmdletBinding()]
param(
    [string]$InstallDir = (Join-Path $env:USERPROFILE 'FearVR'),

    [string]$RetailRoot,

    [string]$PublicToolsGame,

    [switch]$NoShortcut
)

$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\_fearvr-release.ps1"
$cfg = Get-FearVrReleaseConfig
$packageRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path

Write-Host '=== F.E.A.R. VR - Installation ===' -ForegroundColor Cyan

# --- 0. Zielordner pruefen ---------------------------------------------------
# Liegt die Archivkonfiguration unterhalb von %LOCALAPPDATA%, bricht die
# Engine beim Start mit "unable to load game resources" ab. Nachgewiesen mit
# byteweise identischer archcfg an verschiedenen Orten: Nur der Ort
# entscheidet. %LOCALAPPDATA%\Temp funktioniert, andere Unterordner nicht.
$installFull = [IO.Path]::GetFullPath($InstallDir)
$localAppData = [IO.Path]::GetFullPath($env:LOCALAPPDATA)
if ($installFull.StartsWith($localAppData, [StringComparison]::OrdinalIgnoreCase)) {
    throw @"
Ungeeigneter Zielordner: $installFull

Unterhalb von %LOCALAPPDATA% scheitert die LithTech-Engine beim Laden der
Archivkonfiguration ("Failed to initialize client - unable to load game
resources"). Bitte einen anderen Ort waehlen, zum Beispiel:

  -InstallDir "$env:USERPROFILE\FearVR"
  -InstallDir "D:\Spiele\FearVR"
"@
}

# --- 1. Retail finden und verifizieren --------------------------------------
if ([string]::IsNullOrWhiteSpace($RetailRoot)) {
    $RetailRoot = Find-RetailRoot
    if (-not $RetailRoot) {
        throw @'
F.E.A.R.-Installation nicht gefunden.
Mit -RetailRoot "<Pfad zum Spielordner>" erneut aufrufen.
'@
    }
}
$retail = Assert-RetailFearExe $RetailRoot
Write-Host "  [OK] F.E.A.R. $($retail.Version)"
Write-Host "       $RetailRoot"

$retailArchCfg = Join-Path $RetailRoot 'Default.archcfg'
if (-not (Test-Path -LiteralPath $retailArchCfg -PathType Leaf)) {
    throw "Retail-Archivkonfiguration fehlt: $retailArchCfg"
}

# --- 2. Public Tools finden und verifizieren --------------------------------
if ([string]::IsNullOrWhiteSpace($PublicToolsGame)) {
    $PublicToolsGame = Find-PublicToolsGame
}
if (-not (Test-PublicToolsGame $PublicToolsGame)) {
    throw @'
Public Tools 1.08 nicht gefunden.

Die fuenf Module GameClient.dll, GameServer.dll, ClientFx.fxd, FEAR.dep und
FEARMod.Arch00s sind proprietaer und duerfen dem Paket nicht beiliegen. Sie
stammen aus dem offiziellen Installer "fear_publictools_108.exe", der bei der
Ultimate Shooter Edition unter extras\ mitgeliefert wird.

Hinweis zur Installation der Public Tools: Der Installer prueft
HKLM\SOFTWARE\WOW6432Node\Monolith Productions\FEAR\1.00.0000\Patch und
erwartet dort den Wert 8, waehrend Steam 10 setzt. Der Wert muss zur
Installation voruebergehend auf 8 stehen und danach wieder auf 10.

Danach erneut aufrufen, notfalls mit
  -PublicToolsGame "<Pfad>\Dev\Runtime\Game"
'@
}
Write-Host '  [OK] Public Tools 1.08'
Write-Host "       $PublicToolsGame"

# --- 3. Eigene Module aus dem Paket prüfen ----------------------------------
$manifestPath = Join-Path $packageRoot 'release-manifest.json'
if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
    throw "Paketmanifest fehlt: $manifestPath"
}
$package = Get-Content -Raw -LiteralPath $manifestPath | ConvertFrom-Json
foreach ($entry in $package.files) {
    $path = Join-Path $packageRoot $entry.path
    $actual = Get-FileSha256 $path
    if ($actual -ne $entry.sha256) {
        throw "Paketdatei fehlt oder wurde veraendert: $($entry.path)"
    }
}
Write-Host "  [OK] Paket $($package.version) ($($package.gitCommit)) unveraendert"

# --- 4. Stage aufbauen ------------------------------------------------------
$moduleDirectory = Join-Path $InstallDir 'game-modules'
$userDirectory = Join-Path $InstallDir 'userdata'
$logDirectory = Join-Path $InstallDir 'logs'
foreach ($directory in @($InstallDir, $moduleDirectory, $userDirectory, $logDirectory)) {
    New-Item -ItemType Directory -Force -Path $directory | Out-Null
}

$staged = [ordered]@{}
foreach ($target in $cfg.PublicToolsModules.Keys) {
    $source = Join-Path $PublicToolsGame $cfg.PublicToolsModules[$target]
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
        throw "Public-Tools-Modul fehlt: $source"
    }
    Copy-Item -LiteralPath $source -Destination (Join-Path $moduleDirectory $target) -Force
    $staged[$target] = 'public-tools'
}
foreach ($target in $cfg.BundledModules.Keys) {
    $source = Join-Path $packageRoot $cfg.BundledModules[$target]
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
        throw "Paketmodul fehlt: $source"
    }
    Copy-Item -LiteralPath $source -Destination (Join-Path $moduleDirectory $target) -Force
    $staged[$target] = 'fearvr'
}
Write-Host "  [OK] $($staged.Count) Module in $moduleDirectory"

# --- 5. Archivkonfiguration -------------------------------------------------
# Die lose archcfg-Schicht ist der offizielle Weg, ein eigenes Modulset zu
# laden, ohne eine einzige Retail-Datei anzufassen.
$archiveConfig = Join-Path $InstallDir 'fearvr.archcfg'
$archiveLines = @(
    Get-Content -LiteralPath $retailArchCfg -Encoding Default |
        Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
)
$archiveLines += $moduleDirectory
[IO.File]::WriteAllLines($archiveConfig, $archiveLines, [Text.Encoding]::ASCII)

# --- 6. Deployment-Manifest -------------------------------------------------
$records = foreach ($name in $staged.Keys) {
    $path = Join-Path $moduleDirectory $name
    [ordered]@{
        name = $name
        origin = $staged[$name]
        sha256 = Get-FileSha256 $path
        bytes = (Get-Item -LiteralPath $path).Length
    }
}
$deployment = Join-Path $InstallDir 'deployment.json'
[ordered]@{
    installedUtc = (Get-Date).ToUniversalTime().ToString('s') + 'Z'
    packageVersion = $package.version
    packageGitCommit = $package.gitCommit
    packageRoot = $packageRoot
    retailRoot = $RetailRoot
    runtimeExe = $retail.Path
    runtimeSha256 = $retail.Sha256
    steamAppId = $cfg.SteamAppId
    publicToolsGame = $PublicToolsGame
    moduleDirectory = $moduleDirectory
    archiveConfig = $archiveConfig
    archiveConfigSha256 = Get-FileSha256 $archiveConfig
    userDirectory = $userDirectory
    logDirectory = $logDirectory
    files = @($records)
} | ConvertTo-Json -Depth 5 | Out-File -Encoding utf8 -LiteralPath $deployment

# --- 7. Verknüpfung ---------------------------------------------------------
$playScript = Join-Path $PSScriptRoot 'play.ps1'
if (-not $NoShortcut) {
    $shortcut = Join-Path ([Environment]::GetFolderPath('Desktop')) 'F.E.A.R. VR.lnk'
    $shell = New-Object -ComObject WScript.Shell
    $link = $shell.CreateShortcut($shortcut)
    $link.TargetPath =
        'C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe'
    $link.Arguments =
        "-NoProfile -ExecutionPolicy Bypass -File `"$playScript`" " +
        "-InstallDir `"$InstallDir`""
    $link.WorkingDirectory = $packageRoot
    $link.IconLocation = "$($retail.Path),0"
    $link.Description = 'F.E.A.R. VR starten'
    $link.Save()
    Write-Host "  [OK] Verknuepfung: $shortcut"
}

# --- 8. Retail muss unverändert sein ----------------------------------------
$retailAfter = Assert-RetailFearExe $RetailRoot
if ($retail.Sha256 -ne $retailAfter.Sha256) {
    throw 'SICHERHEITSABBRUCH: Retail-FEAR.exe wurde veraendert.'
}

Write-Host ''
Write-Host 'Installation abgeschlossen; Retail unveraendert.' -ForegroundColor Green
Write-Host "Installationsordner: $InstallDir"
Write-Host ''
Write-Host 'Starten:'
Write-Host "  Desktop-Verknuepfung 'F.E.A.R. VR'"
Write-Host "  oder: powershell -ExecutionPolicy Bypass -File `"$playScript`""
Write-Host ''
Write-Host 'Deinstallieren:'
Write-Host ("  powershell -ExecutionPolicy Bypass -File " +
            "`"$(Join-Path $PSScriptRoot 'uninstall.ps1')`" -Apply")

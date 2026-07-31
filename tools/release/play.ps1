<#
.SYNOPSIS
    Startet F.E.A.R. VR.

.DESCRIPTION
    Startet den x64-OpenXR-Host, wartet auf XR-ready und ruft danach Steam mit
    der offiziellen App-ID auf. Geladen wird ausschliesslich ueber die lose
    archcfg-Schicht; die Retail-Installation bleibt unveraendert.

.PARAMETER Runtime
    active (Standard), steamvr, vdxr oder ein Pfad zu einem
    OpenXR-Runtime-Manifest. Erzwungen wird ueber XR_RUNTIME_JSON, nur fuer
    den Hostprozess; die systemweite Einstellung bleibt unangetastet.
#>
[CmdletBinding()]
param(
    [string]$InstallDir = (Join-Path $env:USERPROFILE 'FearVR'),

    [string]$Runtime = 'active',

    [switch]$Translation,

    [switch]$NoStereoHud,

    [switch]$NoHeadBob,

    # Diagnose: schaltet Gruppen unserer Schreibzugriffe auf Retail-Objekte ab,
    # um den Absturz an einer bestimmten Stelle einzugrenzen.
    [switch]$Safe,

    [switch]$NoFlashlight,

    [switch]$NoHandNodes,

    [switch]$NoWeaponTransform,

    [switch]$NoBodyHide,

    # Schaltet den Stereo-Doppelrender ab: Weltrender laeuft einmal pro Frame.
    [switch]$NoStereo,

    # Laesst den kompletten Client-Input-Hook weg: keine Controllersteuerung
    # und keine Kommando-Injektion. Spielbar bleibt es mit Maus und Tastatur.
    [switch]$NoInput,

    # Input-Hook bleibt installiert, schreibt aber keine Kommandobits mehr.
    [switch]$NoInject,

    # Laesst den Hook auf die Retail-Bindungsabfrage weg.
    [switch]$NoBindingHook,

    # Laesst die Arbeit im IClientShell::Update-Hook weg.
    [switch]$NoClientUpdate,

    # Misst nur die rohe Present-Rate. Das Spiel bleibt im Desktopfenster
    # sichtbar, aber es werden absichtlich keine Bilder ins Headset kopiert.
    [switch]$NoCapture,

    # Diagnose-Rollback für den verifizierten Jupiter-EX-HID-FPS-Fix.
    [switch]$NoHidFpsFix,

    # Laesst Weapon-Manager-, AimAt- und Fire-Vector-Hook ungesetzt.
    [switch]$NoAimHooks,

    # Laesst nur den AimAt-Node-Tracker ungesetzt.
    [switch]$NoAimAt,

    # AimAt-Hook bleibt gesetzt, ueberschreibt das Ziel aber nie.
    [switch]$AimAtPassthrough,

    [switch]$Wait
)

$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\_fearvr-release.ps1"
$packageRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path

$deploymentPath = Join-Path $InstallDir 'deployment.json'
if (-not (Test-Path -LiteralPath $deploymentPath -PathType Leaf)) {
    throw @"
Keine Installation in '$InstallDir'.
Zuerst install.ps1 ausfuehren.
"@
}
$deployment = Get-Content -Raw -LiteralPath $deploymentPath | ConvertFrom-Json

# --- Integritaet ------------------------------------------------------------
$retail = Assert-RetailFearExe $deployment.retailRoot
if (-not (Test-CompatibleRetailFearHashes `
        $deployment.runtimeSha256 $retail.Sha256)) {
    throw (
        'Die Retail-EXE hat sich seit der Installation in eine unbekannte ' +
        'Variante geaendert. Bitte neu installieren.'
    )
}
if ($retail.Sha256 -ne $deployment.runtimeSha256) {
    Write-Host (
        'FEAR.exe wechselte zwischen zwei bestaetigten Varianten; Start ' +
        "wird fortgesetzt: $($retail.Edition)"
    ) -ForegroundColor Yellow
}
if ((Get-FileSha256 $deployment.archiveConfig) -ne
    $deployment.archiveConfigSha256) {
    throw 'Die Archivkonfiguration wurde veraendert. Bitte neu installieren.'
}
foreach ($record in $deployment.files) {
    $path = Join-Path $deployment.moduleDirectory $record.name
    if ((Get-FileSha256 $path) -ne $record.sha256) {
        throw "Modul fehlt oder wurde veraendert: $($record.name)"
    }
}

$hostExe = Join-Path $packageRoot 'bin\x64\fearvr-host.exe'
if (-not (Test-Path -LiteralPath $hostExe -PathType Leaf)) {
    throw "Hostprogramm fehlt: $hostExe"
}

# Steam-Installationen brauchen den Umweg ueber steam.exe -applaunch. GOG-
# und DVD-Installationen starten direkt; die Argumente sind dieselben.
# Aeltere deployment.json ohne launchMode sind Steam-Installationen.
$launchMode = 'steam'
if ($deployment.PSObject.Properties.Name -contains 'launchMode' -and
    $deployment.launchMode) {
    $launchMode = $deployment.launchMode
}
$steamExe = $null
if ($launchMode -eq 'steam') {
    $steamExe = Get-SteamExecutable
    if (-not $steamExe) {
        throw @'
Steam nicht gefunden, die Installation ist aber im Steam-Startmodus
eingerichtet. Neu installieren mit -LaunchMode direct, wenn FEAR.exe ohne
Steam gestartet werden soll (GOG, Retail-DVD).
'@
    }
}

$running = @(Get-Process -Name 'FEAR' -ErrorAction SilentlyContinue)
if ($running.Count -gt 0) {
    throw "FEAR.exe laeuft bereits (PID: $($running.Id -join ', '))."
}

# --- Runtime ----------------------------------------------------------------
$runtimeInfo = Resolve-OpenXrRuntime $Runtime
$usesSteamVr = $runtimeInfo.Kind -eq 'steamvr'

$sessionId = [uint64]([DateTime]::UtcNow.Ticks) -bxor ([uint64]$PID -shl 32)
if ($sessionId -eq 0) { $sessionId = 1 }
$sessionText = '0x{0:X16}' -f $sessionId

$runLogDirectory = Join-Path $deployment.logDirectory (
    'fearvr-' + (Get-Date).ToUniversalTime().ToString('yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force -Path $runLogDirectory | Out-Null

Write-Host '=== F.E.A.R. VR ===' -ForegroundColor Cyan
Write-Host "Runtime: $($runtimeInfo.Name)$(if ($runtimeInfo.Override) { ' (erzwungen)' })"
Write-Host "Logs:    $runLogDirectory"

# SteamVR 2.x legt sonst eine Theaterflaeche ueber die laufende OpenXR-Szene.
# Andere Runtimes kennen das nicht; dort wird nichts angefasst.
$theaterScript = Join-Path $PSScriptRoot 'disable-steamvr-theater.ps1'
if ($usesSteamVr -and (Test-Path -LiteralPath $theaterScript -PathType Leaf)) {
    & $theaterScript -Quiet
}

# --- Host starten -----------------------------------------------------------
$hostArguments = @(
    '--ipc-session', $sessionText,
    '--exit-on-game-disconnect',
    '--log-dir', "`"$runLogDirectory`""
)
$previousRuntimeJson = $env:XR_RUNTIME_JSON
try {
    if ($runtimeInfo.Override) { $env:XR_RUNTIME_JSON = $runtimeInfo.Path }
    $hostProcess = Start-Process -FilePath $hostExe `
        -ArgumentList $hostArguments `
        -WorkingDirectory (Split-Path -Parent $hostExe) -PassThru
} finally {
    $env:XR_RUNTIME_JSON = $previousRuntimeJson
}

# Uebersetzt die Fehlermeldung des Hosts in einen brauchbaren Hinweis.
function Get-HostFailureHint([string]$LogDirectory) {
    $log = Get-ChildItem -LiteralPath $LogDirectory -Filter 'host-*.log' `
        -File -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
    if (-not $log) { return $null }
    $failure = Get-Content -LiteralPath $log.FullName |
        Where-Object { $_ -match '"event":"host_failure"' } |
        Select-Object -Last 1
    if (-not $failure) { return $null }
    $detail = $failure
    try { $detail = ($failure | ConvertFrom-Json).message } catch { }

    if ($detail -match 'XR_ERROR_FORM_FACTOR_UNAVAILABLE') {
        return @"
Es ist kein Headset verbunden.
Headset aufsetzen und die Verbindung herstellen (Virtual Desktop verbinden
bzw. SteamVR starten), dann erneut starten.
"@
    }
    if ($detail -match 'XR_ERROR_RUNTIME_UNAVAILABLE|XR_ERROR_INSTANCE') {
        return @"
Die OpenXR-Runtime ist nicht erreichbar.
Virtual Desktop Streamer bzw. SteamVR starten, oder mit
-Runtime vdxr / -Runtime steamvr eine bestimmte Runtime erzwingen.
"@
    }
    return $detail
}

$hostLog = $null
$ready = $false
$deadline = (Get-Date).AddSeconds(30)
do {
    Start-Sleep -Milliseconds 200
    $hostProcess.Refresh()
    if ($hostProcess.HasExited) {
        $hint = Get-HostFailureHint $runLogDirectory
        if ($hint) { throw $hint }
        throw ("OpenXR-Host endete vor XR-ready " +
               "(Exitcode $($hostProcess.ExitCode)). Headset und Runtime pruefen.")
    }
    $hostLog = Get-ChildItem -LiteralPath $runLogDirectory -Filter 'host-*.log' `
        -File -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
    $ready = $null -ne $hostLog -and
        (Get-Content -Raw -LiteralPath $hostLog.FullName) -match '"event":"xr_ready"'
} until ($ready -or (Get-Date) -ge $deadline)
if (-not $ready) {
    Stop-Process -Id $hostProcess.Id -Force -ErrorAction SilentlyContinue
    throw 'OpenXR-Host wurde nicht innerhalb von 30 Sekunden bereit.'
}

# --- Spiel starten ----------------------------------------------------------
$gameArguments = @(
    '-fearvr-session', $sessionText,
    '-fearvr-logdir', "`"$runLogDirectory`"",
    '-archcfg', "`"$($deployment.archiveConfig)`"",
    '-userdirectory', "`"$($deployment.userDirectory)`"",
    '-fearvr-stereo-toggle'
)
if (-not $NoInput) { $gameArguments += '-fearvr-input' }
if ($Translation) { $gameArguments += '-fearvr-translation' }
if (-not $NoStereoHud) { $gameArguments += '-fearvr-stereo-hud' }
if ($NoHeadBob) { $gameArguments += '-fearvr-no-headbob' }
if ($Safe) { $gameArguments += '-fearvr-safe' }
if ($NoFlashlight) { $gameArguments += '-fearvr-no-flashlight' }
if ($NoHandNodes) { $gameArguments += '-fearvr-no-handnodes' }
if ($NoWeaponTransform) { $gameArguments += '-fearvr-no-weapon-transform' }
if ($NoBodyHide) { $gameArguments += '-fearvr-no-body-hide' }
if ($NoStereo) { $gameArguments += '-fearvr-no-stereo' }
if ($NoInject) { $gameArguments += '-fearvr-no-inject' }
if ($NoBindingHook) { $gameArguments += '-fearvr-no-binding-hook' }
if ($NoClientUpdate) { $gameArguments += '-fearvr-no-client-update' }
if ($NoCapture) { $gameArguments += '-fearvr-no-capture' }
if ($NoHidFpsFix) { $gameArguments += '-fearvr-no-hid-fps-fix' }
if ($NoAimHooks) { $gameArguments += '-fearvr-no-aim-hooks' }
if ($NoAimAt) { $gameArguments += '-fearvr-no-aimat' }
if ($AimAtPassthrough) { $gameArguments += '-fearvr-aimat-passthrough' }

if ($launchMode -eq 'steam') {
    $steamArguments = @('-applaunch', $deployment.steamAppId) + $gameArguments
    Start-Process -FilePath $steamExe -ArgumentList ($steamArguments -join ' ') `
        -WorkingDirectory (Split-Path -Parent $steamExe) | Out-Null
} else {
    # GOG und Retail-DVD: Dieselben Argumente gehen direkt an FEAR.exe. Das
    # Arbeitsverzeichnis muss der Spielordner sein, sonst findet die Engine
    # ihre eigenen Ressourcen nicht. Geschrieben wird dort nichts: Alles
    # Veraenderliche liegt hinter -archcfg und -userdirectory.
    Start-Process -FilePath $retail.Path -ArgumentList ($gameArguments -join ' ') `
        -WorkingDirectory $deployment.retailRoot | Out-Null
}

$fear = $null
$deadline = (Get-Date).AddSeconds(25)
do {
    Start-Sleep -Milliseconds 200
    $fear = Get-Process -Name 'FEAR' -ErrorAction SilentlyContinue |
        Select-Object -First 1
} until ($null -ne $fear -or (Get-Date) -ge $deadline)
if ($null -eq $fear) {
    Stop-Process -Id $hostProcess.Id -Force -ErrorAction SilentlyContinue
    $startFailure = 'FEAR.exe war nach 25 Sekunden nicht gestartet.'
    if ($launchMode -eq 'steam') {
        $startFailure = 'Steam startete innerhalb von 25 Sekunden keine FEAR.exe.'
    }
    throw $startFailure
}

$theaterGuard = $null
$guardScript = Join-Path $PSScriptRoot 'hide-steamvr-theater.ps1'
if ($usesSteamVr -and (Test-Path -LiteralPath $guardScript -PathType Leaf)) {
    $theaterGuard = Start-Process `
        -FilePath 'C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe' `
        -ArgumentList @(
            '-NoProfile', '-ExecutionPolicy', 'Bypass',
            '-File', "`"$guardScript`"",
            '-GamePid', $fear.Id,
            '-LogPath', "`"$(Join-Path $runLogDirectory 'steamvr-theater-guard.log')`""
        ) -WindowStyle Hidden -PassThru
}

Write-Host "F.E.A.R. laeuft (PID $($fear.Id))." -ForegroundColor Green
Write-Host ''
Write-Host 'Steuerung: linker Stick bewegt, rechter Stick dreht; hoch springt, runter duckt.'
Write-Host 'A wechselt die Waffe; B laedt nach oder wirft gehalten eine Granate.'
Write-Host 'X schaltet die Lampe, Y pausiert.'
Write-Host 'Linker Trigger schaltet Zeitlupe; rechter Trigger feuert.'
Write-Host 'Linker Grip rennt, linker Stick-Klick nutzt Medkit, rechter Grip benutzt.'
Write-Host 'Rechter Stick-Klick greift in 3D im Nahkampf an.'
Write-Host 'Die linke Hand seitlich neigen lehnt um die Ecke.'
Write-Host 'F8 Stereo an/aus, F9 richtet nur 2D-Bildschirme neu aus, F10 Komfortbildschirm.'
Write-Host 'VR-Einstellungen stehen im ESC-Menue unter "VR SETTINGS".'

if ($Wait) {
    $fear.WaitForExit()
    foreach ($process in @($theaterGuard, $hostProcess)) {
        if ($null -eq $process) { continue }
        $process.WaitForExit(8000) | Out-Null
        $process.Refresh()
        if (-not $process.HasExited) {
            Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        }
    }
    Write-Host 'Spiel beendet.'
}

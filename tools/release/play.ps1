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

    [string]$RetailRoot,

    [string]$PublicToolsGame,

    [switch]$RefreshOverlay,

    [string]$Runtime = 'active',

    [Alias('LaunchMode')]
    [ValidateSet('auto', 'steam', 'direct')]
    [string]$GameLaunchMode = 'auto',

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

    # Diagnose-Rollback: FEAR nicht am OpenXR-Renderauftrag takten.
    [switch]$NoXrFramePacing,

    # Laesst Weapon-Manager-, AimAt- und Fire-Vector-Hook ungesetzt.
    [switch]$NoAimHooks,

    # Laesst nur den AimAt-Node-Tracker ungesetzt.
    [switch]$NoAimAt,

    # AimAt-Hook bleibt gesetzt, ueberschreibt das Ziel aber nie.
    [switch]$AimAtPassthrough,

    [switch]$Wait,

    # Interner, versteckter Startmodus desselben Launchers. So braucht das
    # Release keine separaten SteamVR-Theater-Hilfsskripte mehr.
    [Parameter(DontShow)]
    [switch]$InternalTheaterGuard,

    [Parameter(DontShow)]
    [int]$TheaterGamePid,

    [Parameter(DontShow)]
    [string]$TheaterVrcmd,

    [Parameter(DontShow)]
    [string]$TheaterLogPath
)

$ErrorActionPreference = 'Stop'

function Invoke-IntegratedSteamVrTheaterGuard {
    param(
        [Parameter(Mandatory)]
        [int]$GamePid,

        [Parameter(Mandatory)]
        [string]$VrCmd,

        [Parameter(Mandatory)]
        [string]$LogPath
    )

    $overlayKey = 'valve.steam.desktopgame.21090'
    $utf8WithoutBom = New-Object Text.UTF8Encoding($false)
    function Write-TheaterGuardLog([string]$Message) {
        try {
            $line = '{0:o} {1}{2}' -f (
                Get-Date
            ).ToUniversalTime(), $Message, [Environment]::NewLine
            [IO.File]::AppendAllText($LogPath, $line, $utf8WithoutBom)
        } catch {
            # Der Wächter ist eine Komfortfunktion; sein Log darf den Start nie
            # verhindern.
        }
    }

    if (-not (Test-Path -LiteralPath $VrCmd -PathType Leaf)) {
        Write-TheaterGuardLog "vrcmd_missing path=$VrCmd"
        return
    }

    # SteamVR 2.16 erzeugt die Fläche auf den beobachteten Systemen rund 3,5 s
    # nach dem Spielstart. Zwanzig Sekunden decken auch einen kalten Start ab,
    # ohne später im Spiel weiterhin vrcmd-Prozesse zu erzeugen.
    $deadline = (Get-Date).AddSeconds(20)
    Write-TheaterGuardLog "guard_started game_pid=$GamePid timeout=20"
    while ((Get-Date) -lt $deadline) {
        if ($null -eq (
                Get-Process -Id $GamePid -ErrorAction SilentlyContinue)) {
            Write-TheaterGuardLog 'game_exited'
            return
        }

        $steamVrRunning = $null -ne (
            Get-Process -Name 'vrserver' -ErrorAction SilentlyContinue |
                Select-Object -First 1)
        if (-not $steamVrRunning) {
            Start-Sleep -Milliseconds 500
            continue
        }

        $overlays = @(& $VrCmd --overlays 2>$null)
        $theaterVisible = @(
            $overlays | Where-Object {
                $_ -match [Regex]::Escape($overlayKey) -and
                $_ -match '\svisible\s'
            }
        ).Count -gt 0
        if (-not $theaterVisible) {
            Start-Sleep -Milliseconds 500
            continue
        }

        # Nur Valves Desktopfläche für App 21090 wurde als sichtbar erkannt.
        # Das normale SteamVR-Dashboard bleibt außerhalb dieses kurzen
        # Startfensters vollständig unangetastet.
        for ($attempt = 1; $attempt -le 3; ++$attempt) {
            & $VrCmd --compositorcmd disable_theater_mode 2>$null | Out-Null
            Start-Sleep -Milliseconds 100
            & $VrCmd --hidedashboard 2>$null | Out-Null
            Start-Sleep -Milliseconds 150
            $verification = @(& $VrCmd --overlays 2>$null)
            $stillVisible = @(
                $verification | Where-Object {
                    $_ -match [Regex]::Escape($overlayKey) -and
                    $_ -match '\svisible\s'
                }
            ).Count -gt 0
            if (-not $stillVisible) {
                Write-TheaterGuardLog (
                    "delayed_theater_hidden overlay=$overlayKey " +
                    "attempt=$attempt")
                return
            }
        }
        Write-TheaterGuardLog "theater_hide_failed overlay=$overlayKey"
        return
    }
    Write-TheaterGuardLog 'timeout_without_visible_theater'
}

if ($InternalTheaterGuard) {
    Invoke-IntegratedSteamVrTheaterGuard `
        -GamePid $TheaterGamePid `
        -VrCmd $TheaterVrcmd `
        -LogPath $TheaterLogPath
    exit 0
}

. "$PSScriptRoot\_fearvr-release.ps1"
$packageRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path

$packageManifestPath = Join-Path $packageRoot 'release-manifest.json'
if (Test-Path -LiteralPath $packageManifestPath -PathType Leaf) {
    $packageManifest =
        Get-Content -Raw -LiteralPath $packageManifestPath | ConvertFrom-Json
    if ($packageManifest.layout -eq 'retail-overlay') {
        $prepareArguments = @{
            InstallDir = $InstallDir
            Force = $RefreshOverlay
        }
        if (-not [string]::IsNullOrWhiteSpace($RetailRoot)) {
            $prepareArguments.RetailRoot = $RetailRoot
        }
        if (-not [string]::IsNullOrWhiteSpace($PublicToolsGame)) {
            $prepareArguments.PublicToolsGame = $PublicToolsGame
        }
        & (Join-Path $PSScriptRoot 'prepare-overlay.ps1') @prepareArguments
    }
}

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
if ([string]::IsNullOrWhiteSpace($deployment.dinputProxy) -or
    -not (Test-Path -LiteralPath $deployment.dinputProxy -PathType Leaf) -or
    (Get-FileSha256 $deployment.dinputProxy) -ne
        $deployment.dinputProxySha256) {
    throw (
        'Der frühe dinput8-HID-Fix neben FEAR.exe fehlt oder wurde ' +
        'verändert. Das Release-Overlay erneut entpacken.')
}
if ([string]::IsNullOrWhiteSpace($deployment.d3d9Proxy) -or
    -not (Test-Path -LiteralPath $deployment.d3d9Proxy -PathType Leaf) -or
    (Get-FileSha256 $deployment.d3d9Proxy) -ne
        $deployment.d3d9ProxySha256) {
    throw (
        'Die frühe d3d9-Transfer-Bridge neben FEAR.exe fehlt oder wurde ' +
        'verändert. Das Release-Overlay erneut entpacken.')
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
if ($GameLaunchMode -ne 'auto') {
    $launchMode = $GameLaunchMode
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

$sessionId = [uint64]([DateTime]::UtcNow.Ticks) -bxor ([uint64]$PID -shl 32)
if ($sessionId -eq 0) { $sessionId = 1 }
$sessionText = '0x{0:X16}' -f $sessionId

$runLogDirectory = Join-Path $deployment.logDirectory (
    'fearvr-' + (Get-Date).ToUniversalTime().ToString('yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force -Path $runLogDirectory | Out-Null
$launcherLog = Join-Path $runLogDirectory (
    'launcher-' + (Get-Date).ToUniversalTime().ToString('yyyyMMdd-HHmmss') +
    "-$PID.log")

function Write-LauncherEvent(
    [string]$Level,
    [string]$Event,
    [string]$Message
) {
    try {
        [ordered]@{
            time = (Get-Date).ToUniversalTime().ToString(
                'yyyy-MM-ddTHH:mm:ss.fffZ')
            level = $Level
            event = $Event
            message = $Message
        } | ConvertTo-Json -Compress |
            Add-Content -LiteralPath $launcherLog -Encoding UTF8
    } catch {
        # Diagnostics must never prevent the game from starting.
    }
}

function Find-SteamVrCommand([string]$SteamExecutable) {
    if ([string]::IsNullOrWhiteSpace($SteamExecutable)) {
        return $null
    }
    $steamRoot = Split-Path -Parent $SteamExecutable
    $libraries = New-Object Collections.Generic.List[string]
    $libraries.Add($steamRoot)
    $libraryConfig = Join-Path $steamRoot 'steamapps\libraryfolders.vdf'
    if (Test-Path -LiteralPath $libraryConfig -PathType Leaf) {
        foreach ($match in [Text.RegularExpressions.Regex]::Matches(
            [IO.File]::ReadAllText($libraryConfig),
            '"path"\s*"([^"]+)"')) {
            $libraries.Add(($match.Groups[1].Value -replace '\\\\', '\'))
        }
    }
    foreach ($library in $libraries) {
        $candidate = Join-Path $library (
            'steamapps\common\SteamVR\bin\win64\vrcmd.exe')
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            return $candidate
        }
    }
    return $null
}

function Disable-AutomaticSteamVrTheater(
    [string]$SteamExecutable,
    [string]$BackupDirectory
) {
    if ([string]::IsNullOrWhiteSpace($SteamExecutable)) {
        return $false
    }
    $settingsPath = Join-Path (
        Split-Path -Parent $SteamExecutable) 'config\steamvr.vrsettings'
    if (-not (Test-Path -LiteralPath $settingsPath -PathType Leaf)) {
        Write-LauncherEvent 'INFO' 'steamvr_theater_setting_absent' (
            "settings file not present: $settingsPath")
        return $false
    }

    try {
        $text = [IO.File]::ReadAllText($settingsPath)
        $valuePattern =
            '(?m)("autoShowGameTheater"\s*:\s*)(true|false)'
        if ([Text.RegularExpressions.Regex]::IsMatch(
                $text, $valuePattern)) {
            $updated = [Text.RegularExpressions.Regex]::Replace(
                $text, $valuePattern, '${1}false')
        } else {
            $sectionPattern = '(?m)(^\s*"steamvr"\s*:\s*\{\s*\r?\n)'
            if (-not [Text.RegularExpressions.Regex]::IsMatch(
                    $text, $sectionPattern)) {
                Write-LauncherEvent 'WARN' 'steamvr_theater_setting_failed' (
                    'section "steamvr" is absent from ' + $settingsPath)
                return $false
            }
            $updated = [Text.RegularExpressions.Regex]::Replace(
                $text, $sectionPattern,
                '${1}      "autoShowGameTheater" : false,' +
                    [Environment]::NewLine,
                1)
        }
        if ($updated -cne $text) {
            $backupPath = Join-Path $BackupDirectory (
                'steamvr-before-theater-fix.vrsettings')
            Copy-Item -LiteralPath $settingsPath -Destination $backupPath
            $utf8WithoutBom = New-Object Text.UTF8Encoding($false)
            [IO.File]::WriteAllText(
                $settingsPath, $updated, $utf8WithoutBom)
            Write-LauncherEvent 'INFO' 'steamvr_theater_setting_disabled' (
                "backup=$backupPath")
        } else {
            Write-LauncherEvent 'INFO' 'steamvr_theater_setting_verified' (
                'steamvr.autoShowGameTheater=false')
        }
        return $true
    } catch {
        Write-LauncherEvent 'WARN' 'steamvr_theater_setting_failed' (
            $_.Exception.Message)
        return $false
    }
}

Write-Host '=== F.E.A.R. VR ===' -ForegroundColor Cyan
Write-Host "Runtime: $($runtimeInfo.Name)$(if ($runtimeInfo.Override) { ' (erzwungen)' })"
Write-Host "Logs:    $runLogDirectory"
Write-LauncherEvent 'INFO' 'launcher_start' (
    "runtime=$($runtimeInfo.Name) kind=$($runtimeInfo.Kind) " +
    "launch_mode=$launchMode session=$sessionText")

$steamVrCommand = Find-SteamVrCommand $steamExe
if ($launchMode -eq 'steam') {
    Disable-AutomaticSteamVrTheater `
        -SteamExecutable $steamExe `
        -BackupDirectory $runLogDirectory | Out-Null
}

# --- Host starten -----------------------------------------------------------
$startupImage = Join-Path $packageRoot 'assets\fearvr-startup.jpg'
if (-not (Test-Path -LiteralPath $startupImage -PathType Leaf)) {
    throw "Startbild fehlt: $startupImage"
}
$hostArguments = @(
    '--ipc-session', $sessionText,
    '--exit-on-game-disconnect',
    '--log-dir', "`"$runLogDirectory`"",
    '--startup-image', "`"$startupImage`""
)
$previousRuntimeJson = $env:XR_RUNTIME_JSON
$previousPath = $env:Path
try {
    if ($runtimeInfo.Override) { $env:XR_RUNTIME_JSON = $runtimeInfo.Path }
    if ($runtimeInfo.Kind -eq 'steamvr') {
        # SteamVR 2.16 still starts the legacy vrmonitor.exe, which imports
        # the June-2010 D3DX10 helper. A normal DirectX installation places it
        # in System32. Private bundles also carry the redistributable x64 DLL,
        # so a broken/missing Steam common-redist install does not prevent the
        # monitor process from starting. Only the host's child environment is
        # extended; no system or SteamVR file is modified.
        $systemD3dx10 = Join-Path $env:WINDIR 'System32\d3dx10_43.dll'
        if (-not (Test-Path -LiteralPath $systemD3dx10 -PathType Leaf)) {
            $bundledD3dx10 = Join-Path $packageRoot (
                'redist\directx-jun2010\x64\d3dx10_43.dll')
            if (Test-Path -LiteralPath $bundledD3dx10 -PathType Leaf) {
                $bundledD3dxDirectory = Split-Path -Parent $bundledD3dx10
                $env:Path = if ([string]::IsNullOrWhiteSpace($env:Path)) {
                    $bundledD3dxDirectory
                } else {
                    "$bundledD3dxDirectory;$($env:Path)"
                }
                Write-LauncherEvent 'INFO' 'steamvr_legacy_d3dx_bundled' (
                    "system d3dx10_43.dll missing; child PATH includes " +
                    $bundledD3dxDirectory)
            } else {
                $warning = (
                    'SteamVRs vrmonitor.exe benötigt d3dx10_43.dll aus den ' +
                    'DirectX End-User Runtimes (June 2010), die auf diesem ' +
                    'System fehlt. SteamVR-Dateien prüfen bzw. die Legacy-' +
                    'DirectX-Laufzeit installieren.')
                Write-Warning $warning
                Write-LauncherEvent 'WARN' 'steamvr_legacy_d3dx_missing' $warning
            }
        }
    }
    $hostProcess = Start-Process -FilePath $hostExe `
        -ArgumentList $hostArguments `
        -WorkingDirectory (Split-Path -Parent $hostExe) -PassThru
    Write-LauncherEvent 'INFO' 'host_started' "pid=$($hostProcess.Id)"
} finally {
    $env:XR_RUNTIME_JSON = $previousRuntimeJson
    $env:Path = $previousPath
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
Write-LauncherEvent 'INFO' 'host_ready' "pid=$($hostProcess.Id)"

# --- Spiel starten ----------------------------------------------------------
$gameArguments = @(
    '-fearvr-session', $sessionText,
    '-fearvr-logdir', "`"$runLogDirectory`"",
    '-fearvr-bridge',
        "`"$(Join-Path $deployment.moduleDirectory 'fearvr-d3d9.dll')`"",
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
if ($NoXrFramePacing) { $gameArguments += '-fearvr-no-xr-frame-pacing' }
if ($NoAimHooks) { $gameArguments += '-fearvr-no-aim-hooks' }
if ($NoAimAt) { $gameArguments += '-fearvr-no-aimat' }
if ($AimAtPassthrough) { $gameArguments += '-fearvr-aimat-passthrough' }

$fear = $null
$proxyLog = $null
try {
    if ($launchMode -eq 'steam') {
        $steamArguments =
            @('-applaunch', $deployment.steamAppId) + $gameArguments
        $steamArgumentText = $steamArguments -join ' '
        $launchStarted = Get-Date
        $attemptOffsets = @(0, 3, 6)
        $attemptIndex = 0
        $deadline = $launchStarted.AddSeconds(9)
        $lastSteamError = $null

        # Steam can acknowledge -applaunch while its client is still starting
        # and then lose the actual app request. This leaves the OpenXR host
        # alive but never creates FEAR.exe. Retry the same idempotent app launch
        # twice while continuing to watch for the process. The old 0/10/20 s
        # schedule made a warm Steam client need up to 35 seconds. The measured
        # good path creates FEAR.exe in under one second, so short 0/3/6 s
        # retries preserve a cold-start fallback without delaying normal runs.
        do {
            $elapsed = ((Get-Date) - $launchStarted).TotalSeconds
            if ($attemptIndex -lt $attemptOffsets.Count -and
                $elapsed -ge $attemptOffsets[$attemptIndex]) {
                $attemptNumber = $attemptIndex + 1
                Write-LauncherEvent 'INFO' 'steam_launch_attempt' (
                    "attempt=$attemptNumber appid=$($deployment.steamAppId)")
                try {
                    Start-Process -FilePath $steamExe `
                        -ArgumentList $steamArgumentText `
                        -WorkingDirectory (Split-Path -Parent $steamExe) |
                        Out-Null
                    $lastSteamError = $null
                } catch {
                    $lastSteamError = $_.Exception.Message
                    Write-LauncherEvent 'WARN' 'steam_launch_failed' (
                        "attempt=$attemptNumber error=$lastSteamError")
                }
                $attemptIndex++
            }

            $fear = Get-Process -Name 'FEAR' -ErrorAction SilentlyContinue |
                Select-Object -First 1
            if ($null -eq $fear) { Start-Sleep -Milliseconds 200 }
        } until ($null -ne $fear -or (Get-Date) -ge $deadline)

        if ($null -eq $fear) {
            $detail = if ($lastSteamError) {
                " Letzter Steam-Fehler: $lastSteamError"
            } else {
                ''
            }
            Write-LauncherEvent 'WARN' 'steam_direct_fallback_attempt' (
                'Steam created no FEAR.exe after three attempts; starting ' +
                "the verified Retail executable directly.$detail")
            Write-Warning (
                'Steam hat nach drei Versuchen keine FEAR.exe erzeugt. ' +
                'Direkter Rückfallstart bei weiter laufendem Steam-Client ...')
            try {
                Start-Process -FilePath $retail.Path `
                    -ArgumentList ($gameArguments -join ' ') `
                    -WorkingDirectory $deployment.retailRoot | Out-Null
            } catch {
                throw (
                    'Steam startete keine FEAR.exe und der direkte ' +
                    "Rückfallstart schlug fehl: $($_.Exception.Message)$detail")
            }

            $fallbackDeadline = (Get-Date).AddSeconds(25)
            do {
                Start-Sleep -Milliseconds 200
                $fear =
                    Get-Process -Name 'FEAR' -ErrorAction SilentlyContinue |
                    Select-Object -First 1
            } until ($null -ne $fear -or
                     (Get-Date) -ge $fallbackDeadline)
            if ($null -eq $fear) {
                throw (
                    'Steam startete nach drei Versuchen keine FEAR.exe; auch ' +
                    'der direkte Rückfallstart erzeugte innerhalb von 25 ' +
                    "Sekunden keinen Prozess.$detail")
            }
            Write-LauncherEvent 'INFO' 'steam_direct_fallback_started' (
                "pid=$($fear.Id)")
        }
    } else {
        # GOG und Retail-DVD: Dieselben Argumente gehen direkt an FEAR.exe. Das
        # Arbeitsverzeichnis muss der Spielordner sein, sonst findet die Engine
        # ihre eigenen Ressourcen nicht. Geschrieben wird dort nichts: Alles
        # Veraenderliche liegt hinter -archcfg und -userdirectory.
        Write-LauncherEvent 'INFO' 'direct_launch_attempt' $retail.Path
        Start-Process -FilePath $retail.Path `
            -ArgumentList ($gameArguments -join ' ') `
            -WorkingDirectory $deployment.retailRoot | Out-Null

        $deadline = (Get-Date).AddSeconds(25)
        do {
            Start-Sleep -Milliseconds 200
            $fear = Get-Process -Name 'FEAR' -ErrorAction SilentlyContinue |
                Select-Object -First 1
        } until ($null -ne $fear -or (Get-Date) -ge $deadline)
        if ($null -eq $fear) {
            throw 'FEAR.exe war nach 25 Sekunden nicht gestartet.'
        }
    }

    Write-LauncherEvent 'INFO' 'game_process_started' "pid=$($fear.Id)"

    $theaterGuard = $null
    $steamVrIsRunning = $null -ne (
        Get-Process -Name 'vrserver' -ErrorAction SilentlyContinue |
            Select-Object -First 1)
    if ($steamVrCommand -and
        ($runtimeInfo.Kind -eq 'steamvr' -or $steamVrIsRunning)) {
        $guardLog = Join-Path $runLogDirectory 'steamvr-theater-guard.log'
        $powershellExe = Join-Path $PSHOME 'powershell.exe'
        if (-not (Test-Path -LiteralPath $powershellExe -PathType Leaf)) {
            $powershellExe = (Get-Process -Id $PID).Path
        }
        $theaterGuard = Start-Process -FilePath $powershellExe `
            -ArgumentList @(
                '-NoProfile', '-ExecutionPolicy', 'Bypass',
                '-File', "`"$PSCommandPath`"",
                '-InternalTheaterGuard',
                '-TheaterGamePid', $fear.Id,
                '-TheaterVrcmd', "`"$steamVrCommand`"",
                '-TheaterLogPath', "`"$guardLog`""
            ) -WindowStyle Hidden -PassThru
        Write-LauncherEvent 'INFO' 'steamvr_theater_guard_started' (
            "pid=$($theaterGuard.Id) game_pid=$($fear.Id)")
    }

    # A running FEAR.exe alone is not a successful VR start. Confirm that the
    # early d3d9 bridge received this session's command line and produced its
    # proxy log. Otherwise the old launcher falsely reported success while the
    # game continued flat and the headset stayed on the startup panel.
    $deadline = (Get-Date).AddSeconds(20)
    do {
        $hostProcess.Refresh()
        if ($hostProcess.HasExited) {
            throw (
                'Der OpenXR-Host endete, bevor sich die VR-Brücke verbinden ' +
                "konnte (Exitcode $($hostProcess.ExitCode)).")
        }
        $fear.Refresh()
        if ($fear.HasExited) {
            throw (
                'FEAR.exe endete, bevor die VR-Brücke geladen wurde ' +
                "(Exitcode $($fear.ExitCode)).")
        }
        $candidateProxyLog = Get-ChildItem -LiteralPath $runLogDirectory `
            -Filter 'proxy-*.log' -File -ErrorAction SilentlyContinue |
            Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
        $proxyLog = $null
        if ($null -ne $candidateProxyLog) {
            $proxyHeader = Get-Content -LiteralPath $candidateProxyLog.FullName `
                -TotalCount 1 -ErrorAction SilentlyContinue
            if (Test-FearVrProxySessionHeader `
                    $proxyHeader $sessionId) {
                $proxyLog = $candidateProxyLog
            }
        }
        if ($null -eq $proxyLog) { Start-Sleep -Milliseconds 200 }
    } until ($null -ne $proxyLog -or (Get-Date) -ge $deadline)

    if ($null -eq $proxyLog) {
        throw @'
FEAR.exe laeuft, aber die frühe VR-Brücke wurde nicht geladen.
Das Overlay erneut entpacken und prüfen, ob Antivirus dinput8.dll oder
d3d9.dll blockiert hat.
'@
    }
    Write-LauncherEvent 'INFO' 'proxy_ready' (
        "pid=$($fear.Id) log=$($proxyLog.Name)")
} catch {
    Write-LauncherEvent 'ERROR' 'launcher_failure' $_.Exception.Message
    if ($null -ne $fear) {
        try {
            $fear.Refresh()
            if (-not $fear.HasExited) {
                Stop-Process -Id $fear.Id -Force -ErrorAction SilentlyContinue
            }
        } catch { }
    }
    if ($null -ne $theaterGuard) {
        try {
            $theaterGuard.Refresh()
            if (-not $theaterGuard.HasExited) {
                Stop-Process -Id $theaterGuard.Id -Force `
                    -ErrorAction SilentlyContinue
            }
        } catch { }
    }
    try {
        $hostProcess.Refresh()
        if (-not $hostProcess.HasExited) {
            Stop-Process -Id $hostProcess.Id -Force -ErrorAction SilentlyContinue
        }
    } catch { }
    throw
}

Write-Host "F.E.A.R. laeuft (PID $($fear.Id))." -ForegroundColor Green
Write-Host ''
Write-Host 'Steuerung: linker Stick bewegt, rechter Stick dreht; hoch springt, runter duckt.'
Write-Host 'A wechselt die Waffe; B laedt nach oder wirft gehalten eine Granate.'
Write-Host 'X schaltet die Lampe, Y pausiert.'
Write-Host 'Trigger feuern die jeweilige Pistole; mit zwei Pistolen schaltet X Zeitlupe.'
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

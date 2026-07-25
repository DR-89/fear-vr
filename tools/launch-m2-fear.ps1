<#
.SYNOPSIS
    Startet F.E.A.R. M2 bis M5 über Steam und die isolierte archcfg-Stage.

.DESCRIPTION
    Startet den x64-OpenXR-Host, wartet auf XR-ready und ruft danach Steam mit
    App-ID 21090 auf. Über die offizielle lose archcfg-Schicht wird nur der
    ABI-neutrale GameClient-Loader geladen; Retail bleibt unverändert.

.PARAMETER Runtime
    Welche OpenXR-Runtime der Host verwenden soll:
      active   - die systemweit eingestellte (Standard)
      steamvr  - SteamVR erzwingen
      vdxr     - VirtualDesktopXR erzwingen
      <Pfad>   - beliebiges Runtime-Manifest (.json)
    Erzwungen wird über XR_RUNTIME_JSON, das nur für den Hostprozess gesetzt
    wird. Die systemweite Einstellung bleibt unverändert.

.PARAMETER Wait
    Wartet auf das Spielende. Der zugehörige Host wird danach beendet.
#>
[CmdletBinding()]
param(
    [ValidateSet('M2', 'M3', 'M4', 'M5')]
    [string]$Milestone = 'M2',

    [string]$Runtime = 'active',

    [switch]$Translation,

    [switch]$StereoHud,

    [switch]$NoHeadBob,

    [switch]$Wait
)

$ErrorActionPreference = 'Stop'
$milestoneLabel = $Milestone.ToUpperInvariant()
$milestoneSlug = $Milestone.ToLowerInvariant()
. "$PSScriptRoot\_fearvr-env.ps1"
$cfg = Get-FearVrConfig
$retailBefore = Assert-RetailFearExe

# --- OpenXR-Runtime bestimmen -----------------------------------------------
# Steam bleibt für den Spielstart nötig (offizieller -applaunch-Weg), die
# VR-Runtime ist davon aber unabhängig.
$runtimeInfo = Resolve-OpenXrRuntime $Runtime
$usesSteamVr = $runtimeInfo.Kind -eq 'steamvr'

# SteamVR 2.x legt beim offiziellen Start eines Nicht-VR-Spiels sonst
# automatisch eine Theaterfläche über die bereits laufende OpenXR-Szene.
# Andere Runtimes kennen dieses Verhalten nicht; dort wird die
# SteamVR-Konfiguration bewusst nicht angefasst.
if ($usesSteamVr) {
    & "$PSScriptRoot\disable-steamvr-theater.ps1" -Quiet
}

$manifestPath = Assert-UnderProjectRoot (
    Join-Path $cfg.ProjectRoot (
        "stage\$milestoneSlug-deployment.json"
    )
)
if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
    throw (
        "$milestoneLabel-Stage fehlt. Zuerst die zugehörige " +
        'prepare-Stage ausführen.'
    )
}
$manifest = Get-Content -Raw -LiteralPath $manifestPath | ConvertFrom-Json
if ((Get-FileSha256 $manifest.runtimeExe) -ne $manifest.runtimeSha256) {
    throw 'Manifestierte Retail-EXE stimmt nicht mehr.'
}
if ((Get-FileSha256 $manifest.archiveConfig) -ne
    $manifest.archiveConfigSha256) {
    throw "$milestoneLabel-Archivkonfiguration wurde verändert."
}
foreach ($record in $manifest.files) {
    $path = Join-Path $manifest.moduleDirectory $record.name
    if ((Get-FileSha256 $path) -ne $record.sha256) {
        throw (
            "$milestoneLabel-Stage-Datei fehlt oder wurde verändert: " +
            $record.name
        )
    }
}

$hostExe = Assert-UnderProjectRoot (
    Join-Path $cfg.ProjectRoot 'build\x64\src\host64\RelWithDebInfo\fearvr-host.exe'
)
$moduleProbe = Assert-UnderProjectRoot (
    Join-Path $cfg.ProjectRoot 'build\x86\src\launcher\RelWithDebInfo\fearvr-module-probe.exe'
)
foreach ($required in @($hostExe, $moduleProbe)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "$milestoneLabel-Laufzeitartefakt fehlt: $required"
    }
}
$existingFearIds = @(
    Get-Process -Name 'FEAR' -ErrorAction SilentlyContinue |
        ForEach-Object { $_.Id }
)
if ($existingFearIds.Count -gt 0) {
    throw "FEAR.exe läuft bereits (PID: $($existingFearIds -join ', '))."
}

$sessionId = [uint64]([DateTime]::UtcNow.Ticks)
$sessionId = $sessionId -bxor ([uint64]$PID -shl 32)
if ($sessionId -eq 0) {
    $sessionId = 1
}
$sessionText = '0x{0:X16}' -f $sessionId
$runLogDirectory = Assert-UnderProjectRoot (
    Join-Path $manifest.logDirectory (
        "$milestoneSlug-fear-" +
        (Get-Date).ToUniversalTime().ToString('yyyyMMdd-HHmmss')
    )
)
New-Item -ItemType Directory -Force -Path $runLogDirectory | Out-Null
New-Item -ItemType Directory -Force -Path $manifest.userDirectory | Out-Null

$hostArguments = @(
    '--ipc-session', $sessionText,
    '--exit-on-game-disconnect',
    '--log-dir', "`"$runLogDirectory`""
)
# XR_RUNTIME_JSON wirkt nur auf den erzeugten Kindprozess. Der Wert wird
# danach wieder auf den Ausgangszustand gesetzt, damit die aufrufende Shell
# unverändert bleibt.
$previousRuntimeJson = $env:XR_RUNTIME_JSON
try {
    if ($runtimeInfo.Override) {
        $env:XR_RUNTIME_JSON = $runtimeInfo.Path
    }
    $hostProcess = Start-Process -FilePath $hostExe `
        -ArgumentList $hostArguments `
        -WorkingDirectory (Split-Path -Parent $hostExe) `
        -PassThru
} finally {
    $env:XR_RUNTIME_JSON = $previousRuntimeJson
}

$hostLog = $null
$ready = $false
$deadline = (Get-Date).AddSeconds(30)
do {
    Start-Sleep -Milliseconds 200
    $hostProcess.Refresh()
    if ($hostProcess.HasExited) {
        throw "OpenXR-Host endete vor XR-ready (Exitcode $($hostProcess.ExitCode))."
    }
    $hostLog = Get-ChildItem -LiteralPath $runLogDirectory `
        -Filter 'host-*.log' -File -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTimeUtc -Descending |
        Select-Object -First 1
    $ready = $null -ne $hostLog -and
        (Get-Content -Raw -LiteralPath $hostLog.FullName) -match
            '"event":"xr_ready"'
} until ($ready -or (Get-Date) -ge $deadline)
if (-not $ready) {
    Stop-Process -Id $hostProcess.Id -Force -ErrorAction SilentlyContinue
    throw 'OpenXR-Host wurde nicht innerhalb von 30 Sekunden XR-ready.'
}

$steamArguments = @(
    '-applaunch', $manifest.steamAppId,
    '-fearvr-session', $sessionText,
    '-fearvr-logdir', "`"$runLogDirectory`"",
    '-archcfg', "`"$($manifest.archiveConfig)`"",
    '-userdirectory', "`"$($manifest.userDirectory)`""
)
if ($Milestone -ne 'M2') {
    $steamArguments += '-fearvr-stereo-toggle'
}
if ($Milestone -in @('M4', 'M5') -and $Translation) {
    $steamArguments += '-fearvr-translation'
}
if ($Milestone -in @('M4', 'M5') -and $StereoHud) {
    $steamArguments += '-fearvr-stereo-hud'
}
if ($Milestone -in @('M4', 'M5') -and $NoHeadBob) {
    $steamArguments += '-fearvr-no-headbob'
}
if ($Milestone -eq 'M5') {
    $steamArguments += '-fearvr-input'
}
$steamArguments = $steamArguments -join ' '
Write-Host "=== F.E.A.R. VR $milestoneLabel ===" -ForegroundColor Cyan
Write-Host "Session: $sessionText"
Write-Host "Stage:   $($manifest.moduleDirectory)"
Write-Host "Logs:    $runLogDirectory"
$runtimeSource = if ($runtimeInfo.Override) {
    'erzwungen über XR_RUNTIME_JSON'
} else {
    'systemweit aktiv'
}
Write-Host "Runtime: $($runtimeInfo.Name) ($runtimeSource)"
Start-Process -FilePath $manifest.steamExe `
    -ArgumentList $steamArguments `
    -WorkingDirectory (Split-Path -Parent $manifest.steamExe) |
    Out-Null

$fear = $null
$theaterGuard = $null
function Stop-StartedM2Processes {
    foreach ($process in @($theaterGuard, $fear, $hostProcess)) {
        if ($null -ne $process) {
            $process.Refresh()
            if (-not $process.HasExited) {
                Stop-Process -Id $process.Id -Force `
                    -ErrorAction SilentlyContinue
            }
        }
    }
}
$deadline = (Get-Date).AddSeconds(25)
do {
    Start-Sleep -Milliseconds 200
    $fear = Get-Process -Name 'FEAR' -ErrorAction SilentlyContinue |
        Where-Object { $_.Id -notin $existingFearIds } |
        Select-Object -First 1
} until ($null -ne $fear -or (Get-Date) -ge $deadline)
if ($null -eq $fear) {
    Stop-Process -Id $hostProcess.Id -Force -ErrorAction SilentlyContinue
    throw 'Steam startete innerhalb von 25 Sekunden keine FEAR.exe.'
}

# Der Theaterwächter ist reine SteamVR-Kosmetik und wird nur dort gebraucht.
if ($usesSteamVr) {
    $theaterGuardScript = Assert-UnderProjectRoot (
        Join-Path $cfg.ProjectRoot 'tools\hide-steamvr-theater.ps1'
    )
    $theaterGuardLog = Assert-UnderProjectRoot (
        Join-Path $runLogDirectory 'steamvr-theater-guard.log'
    )
    $theaterGuardArguments = @(
        '-NoProfile',
        '-ExecutionPolicy', 'Bypass',
        '-File', "`"$theaterGuardScript`"",
        '-GamePid', $fear.Id,
        '-LogPath', "`"$theaterGuardLog`""
    )
    $theaterGuard = Start-Process `
        -FilePath 'C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe' `
        -ArgumentList $theaterGuardArguments `
        -WindowStyle Hidden `
        -PassThru
}

$expectedModules = @{
    'GameClient.dll' = Join-Path $manifest.moduleDirectory 'GameClient.dll'
    'GameOrig.dll' = Join-Path $manifest.moduleDirectory 'GameOrig.dll'
    'fearvr-d3d9.dll' = Join-Path $manifest.moduleDirectory 'fearvr-d3d9.dll'
}
$loaded = @{}
$deadline = (Get-Date).AddSeconds(30)
do {
    Start-Sleep -Milliseconds 250
    $fear.Refresh()
    if ($fear.HasExited) {
        Stop-StartedM2Processes
        throw (
            "FEAR.exe endete während der " +
            "$milestoneLabel-Modulprüfung."
        )
    }
    $moduleLines = @(& $moduleProbe $fear.Id)
    if ($LASTEXITCODE -eq 0) {
        foreach ($line in $moduleLines) {
            $parts = $line -split "`t", 2
            if ($parts.Count -eq 2 -and
                $expectedModules.ContainsKey($parts[0])) {
                $loaded[$parts[0]] =
                    [IO.Path]::GetFullPath($parts[1])
            }
        }
    } else {
        $loaded.Clear()
    }
} until ($loaded.Count -eq $expectedModules.Count -or
         (Get-Date) -ge $deadline)
foreach ($name in $expectedModules.Keys) {
    $expected = [IO.Path]::GetFullPath($expectedModules[$name])
    if (-not $loaded.ContainsKey($name) -or $loaded[$name] -ne $expected) {
        $actual = if ($loaded.ContainsKey($name)) {
            $loaded[$name]
        } else {
            '<nicht geladen>'
        }
        Stop-StartedM2Processes
        throw (
            "$milestoneLabel-Modul wurde nicht aus der Stage geladen: " +
            "$name (ist: $actual)"
        )
    }
}

$bridgeReady = $false
$frameImported = $false
$hookReady = $Milestone -eq 'M2'
$stereoReady = $false
$proxyLog = $null
$deadline = (Get-Date).AddSeconds(
    $(if ($Milestone -ne 'M2') { 60 } else { 30 })
)
do {
    Start-Sleep -Milliseconds 250
    $fear.Refresh()
    if ($fear.HasExited) {
        Stop-StartedM2Processes
        throw (
            "FEAR.exe endete während der " +
            "$milestoneLabel-Bridgeprüfung."
        )
    }
    $proxyLog = Get-ChildItem -LiteralPath $runLogDirectory `
        -Filter 'proxy-*.log' -File -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTimeUtc -Descending |
        Select-Object -First 1
    if ($null -ne $proxyLog) {
        $proxyText = Get-Content -Raw -LiteralPath $proxyLog.FullName
        if ($proxyText -match '"event":"adapter_mismatch"') {
            Stop-StartedM2Processes
            throw 'D3D9 und OpenXR verwenden unterschiedliche GPU-Adapter.'
        }
        $bridgeReady =
            $proxyText -match '"event":"adapter_match"' -and
            $proxyText -match '"event":"shared_resources"' -and
            $proxyText -match '"event":"frame_ready"'
        if ($Milestone -ne 'M2') {
            $hookReady =
                $proxyText -match '"event":"engine_interfaces_found"' -and
                $proxyText -match '"event":"stereo_hook_armed"'
            $stereoReady =
                $proxyText -match '"event":"stereo_render_active"' -and
                $proxyText -match '"event":"stereo_frame_staged"'
        }
    }
    $hostText = Get-Content -Raw -LiteralPath $hostLog.FullName
    $frameImported = $hostText -match '"event":"ipc_frame"'
} until (($bridgeReady -and $frameImported -and $hookReady) -or
         (Get-Date) -ge $deadline)
if (-not $bridgeReady -or -not $frameImported -or -not $hookReady) {
    Stop-StartedM2Processes
    throw (
        "$milestoneLabel-Bildpfad wurde nicht rechtzeitig bereit. " +
        'Headset aufsetzen/fokussieren und Logs prüfen.'
    )
}

Write-Host (
    "F.E.A.R. läuft mit $milestoneLabel-Bridge (PID $($fear.Id))."
) `
    -ForegroundColor Green
if ($Milestone -ne 'M2') {
    if ($stereoReady) {
        Write-Host (
            'Nativer Stereo-Weltrender ist bereits aktiv. Im Headset ' +
            'muss Tiefenparallaxe sichtbar sein.'
        )
    } else {
        Write-Host (
            "$milestoneLabel-Hook ist bereit. Menü und Spielstart bleiben zunächst mono. " +
            'Erst in der 3D-Welt mit F8 Stereo einschalten.'
        )
    }
    if ($Milestone -in @('M4', 'M5')) {
        Write-Host (
            "$milestoneLabel`: HMD-Rotation ist aktiv. F9 setzt die aktuelle Blickrichtung " +
            'als Neutralpose; F8 schaltet jederzeit zurück auf mono.'
        )
        Write-Host (
            $(if ($Translation) {
                'Begrenzte HMD-Translation ist aktiv (maximal 25 cm).'
            } else {
                'HMD-Translation ist deaktiviert; mit -Translation opt-in aktivieren.'
            })
        )
        Write-Host (
            $(if ($NoHeadBob) {
                'Head-Bob ist für Kamera und Waffe deaktiviert.'
            } else {
                'Waffen-Bob ist aus; Kamera-Bob folgt fearvr.ini (Standard: aus).'
            })
        )
        Write-Host (
            'F10 schaltet den raumfesten Komfortbildschirm für ' +
            'Camera-Shakes und Zwischensequenzen.'
        )
        if ($Milestone -eq 'M5') {
            Write-Host (
                'M5-Controller: linker Stick bewegt, rechter Stick dreht; ' +
                'rechts hoch/runter wählt nächste/vorherige Waffe. ' +
                'A springt, B lädt nach, X duckt, Y schaltet Zeitlupe.'
            )
            Write-Host (
                'Linker Grip rennt, linker Stick-Klick öffnet Pause, ' +
                'rechter Grip benutzt, Trigger zielen/feuern; ' +
                'rechter Stick-Klick setzt Recenter.'
            )
            Write-Host (
                'Die linke Hand seitlich neigen lehnt um die Ecke: ' +
                'Oberseite nach links lehnt links, nach rechts lehnt rechts.'
            )
            Write-Host (
                'F11 zeigt die Player-Body-Pieces einzeln, um das ' +
                'Hand-Piece zu finden. Der Stand wird sofort in ' +
                'fearvr.ini gespeichert und beim Start automatisch ' +
                'angewandt; F11 ist danach nicht mehr nötig.'
            )
        }
    } else {
        Write-Host (
            'F8 schaltet jederzeit zurück auf mono. Headtracking folgt erst in M4.'
        )
    }
} else {
    Write-Host (
        'Im Headset muss das normale Spielbild mono in beiden Augen erscheinen.'
    )
}
Write-Host "Hostlog: $($hostLog.FullName)"
Write-Host "Proxylog: $($proxyLog.FullName)"

$retailAfter = Assert-RetailFearExe
if ($retailBefore.Sha256 -ne $retailAfter.Sha256) {
    throw 'SICHERHEITSABBRUCH: Retail-FEAR.exe wurde verändert.'
}

if ($Wait) {
    $fear.WaitForExit()
    $hostProcess.WaitForExit(8000) | Out-Null
    $hostProcess.Refresh()
    if (-not $hostProcess.HasExited) {
        Stop-Process -Id $hostProcess.Id -Force
    }
    Write-Host (
        "Spiel beendet; zugehöriger $milestoneLabel-Host beendet."
    )
}

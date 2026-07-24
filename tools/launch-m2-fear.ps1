<#
.SYNOPSIS
    Startet F.E.A.R. M2 offiziell über Steam und die isolierte archcfg-Stage.

.DESCRIPTION
    Startet den x64-OpenXR-Host, wartet auf XR-ready und ruft danach Steam mit
    App-ID 21090 auf. Über die offizielle lose archcfg-Schicht wird nur der
    ABI-neutrale GameClient-Loader geladen; Retail bleibt unverändert.

.PARAMETER Wait
    Wartet auf das Spielende. Der zugehörige Host wird danach beendet.
#>
[CmdletBinding()]
param(
    [switch]$Wait
)

$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\_fearvr-env.ps1"
$cfg = Get-FearVrConfig
$retailBefore = Assert-RetailFearExe

$manifestPath = Assert-UnderProjectRoot (
    Join-Path $cfg.ProjectRoot 'stage\m2-deployment.json'
)
if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
    throw 'M2-Stage fehlt. Zuerst tools\prepare-m2-stage.ps1 ausführen.'
}
$manifest = Get-Content -Raw -LiteralPath $manifestPath | ConvertFrom-Json
if ((Get-FileSha256 $manifest.runtimeExe) -ne $manifest.runtimeSha256) {
    throw 'Manifestierte Retail-EXE stimmt nicht mehr.'
}
if ((Get-FileSha256 $manifest.archiveConfig) -ne
    $manifest.archiveConfigSha256) {
    throw 'M2-Archivkonfiguration wurde verändert.'
}
foreach ($record in $manifest.files) {
    $path = Join-Path $manifest.moduleDirectory $record.name
    if ((Get-FileSha256 $path) -ne $record.sha256) {
        throw "M2-Stage-Datei fehlt oder wurde verändert: $($record.name)"
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
        throw "M2-Laufzeitartefakt fehlt: $required"
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
        'm2-fear-' +
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
$hostProcess = Start-Process -FilePath $hostExe `
    -ArgumentList $hostArguments `
    -WorkingDirectory (Split-Path -Parent $hostExe) `
    -PassThru

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
) -join ' '
Write-Host '=== F.E.A.R. VR M2 ===' -ForegroundColor Cyan
Write-Host "Session: $sessionText"
Write-Host "Stage:   $($manifest.moduleDirectory)"
Write-Host "Logs:    $runLogDirectory"
Start-Process -FilePath $manifest.steamExe `
    -ArgumentList $steamArguments `
    -WorkingDirectory (Split-Path -Parent $manifest.steamExe) |
    Out-Null

$fear = $null
function Stop-StartedM2Processes {
    foreach ($process in @($fear, $hostProcess)) {
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
        throw "FEAR.exe endete während der M2-Modulprüfung."
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
        throw "M2-Modul wurde nicht aus der Stage geladen: $name (ist: $actual)"
    }
}

$bridgeReady = $false
$frameImported = $false
$proxyLog = $null
$deadline = (Get-Date).AddSeconds(30)
do {
    Start-Sleep -Milliseconds 250
    $fear.Refresh()
    if ($fear.HasExited) {
        Stop-StartedM2Processes
        throw 'FEAR.exe endete während der M2-Bridgeprüfung.'
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
    }
    $hostText = Get-Content -Raw -LiteralPath $hostLog.FullName
    $frameImported = $hostText -match '"event":"ipc_frame"'
} until (($bridgeReady -and $frameImported) -or
         (Get-Date) -ge $deadline)
if (-not $bridgeReady -or -not $frameImported) {
    Stop-StartedM2Processes
    throw ('M2-Bildpfad wurde nicht innerhalb von 30 Sekunden bereit. ' +
           'Headset aufsetzen/fokussieren und Logs prüfen.')
}

Write-Host "F.E.A.R. läuft mit M2-Bridge (PID $($fear.Id))." `
    -ForegroundColor Green
Write-Host 'Im Headset muss das normale Spielbild mono in beiden Augen erscheinen.'
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
    Write-Host 'Spiel beendet; zugehöriger M2-Host beendet.'
}

<#
.SYNOPSIS
    Wertet ein Laufverzeichnis unter logs\ zu den in ANWEISUNG.md §14
    geforderten Kennzahlen aus.

.DESCRIPTION
    Liest ausschließlich die JSON-Zeilen der Host- und Proxylogs eines Laufs
    und schreibt nichts zurück. Ohne -Run wird das jüngste Laufverzeichnis
    verwendet.

.PARAMETER Run
    Name oder vollständiger Pfad eines Laufverzeichnisses unter logs\.

.PARAMETER AsJson
    Gibt das Ergebnis als JSON statt als Text aus.
#>
[CmdletBinding()]
param(
    [string]$Run,

    [switch]$AsJson
)

$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\_fearvr-env.ps1"
$cfg = Get-FearVrConfig

$logRoot = Join-Path $cfg.ProjectRoot 'logs'
if ([string]::IsNullOrWhiteSpace($Run)) {
    $runDirectory = Get-ChildItem -LiteralPath $logRoot -Directory |
        Where-Object { $_.Name -match '^m\d-fear-' } |
        Sort-Object Name -Descending | Select-Object -First 1
    if (-not $runDirectory) {
        throw "Kein Laufverzeichnis unter $logRoot gefunden."
    }
} elseif (Test-Path -LiteralPath $Run -PathType Container) {
    $runDirectory = Get-Item -LiteralPath $Run
} else {
    $candidate = Join-Path $logRoot $Run
    if (-not (Test-Path -LiteralPath $candidate -PathType Container)) {
        throw "Laufverzeichnis nicht gefunden: $Run"
    }
    $runDirectory = Get-Item -LiteralPath $candidate
}

function Read-Events([string]$Pattern) {
    $file = Get-ChildItem -LiteralPath $runDirectory.FullName `
        -Filter $Pattern -File | Sort-Object Name | Select-Object -First 1
    if (-not $file) { return @() }
    return @(
        Get-Content -LiteralPath $file.FullName |
            Where-Object { $_.TrimStart().StartsWith('{') } |
            ForEach-Object {
                try { $_ | ConvertFrom-Json } catch { }
            }
    )
}

function First-Message($events, [string]$EventName) {
    $hit = $events | Where-Object { $_.event -eq $EventName } |
        Select-Object -First 1
    if ($hit) { return $hit.message }
    return $null
}

function Field([string]$Text, [string]$Key) {
    if ([string]::IsNullOrWhiteSpace($Text)) { return $null }
    $match = [Text.RegularExpressions.Regex]::Match(
        $Text, [Text.RegularExpressions.Regex]::Escape($Key) + '=([^\s]+)'
    )
    if ($match.Success) { return $match.Groups[1].Value }
    return $null
}

$hostEvents = Read-Events 'host-*.log'
$proxyEvents = Read-Events 'proxy-*.log'
if ($hostEvents.Count -eq 0) {
    throw "Kein auswertbares Hostlog in $($runDirectory.FullName)."
}

# --- Statische Angaben ------------------------------------------------------
$hostStart = First-Message $hostEvents 'host_start'
$hostStop = First-Message $hostEvents 'host_stop'
$proxyStart = First-Message $proxyEvents 'proxy_start'

# --- Messfenster ------------------------------------------------------------
$perf = @($hostEvents | Where-Object { $_.event -eq 'perf_frame' })
$windows = @(
    foreach ($entry in $perf) {
        [ordered]@{
            xrFps = [double](Field $entry.message 'xr_fps')
            gameFps = [double](Field $entry.message 'game_fps')
            reused = [int](Field $entry.message 'reused')
            renderLeftAvgUs = [int](Field $entry.message 'render_left_avg_us')
            renderLeftMaxUs = [int](Field $entry.message 'render_left_max_us')
            renderRightAvgUs = [int](Field $entry.message 'render_right_avg_us')
            renderRightMaxUs = [int](Field $entry.message 'render_right_max_us')
            copyAvgUs = [int](Field $entry.message 'copy_avg_us')
            copyMaxUs = [int](Field $entry.message 'copy_max_us')
            handles = [int](Field $entry.message 'handles')
        }
    }
)

function Stat($values) {
    if (-not $values -or @($values).Count -eq 0) { return $null }
    $measured = $values | Measure-Object -Average -Maximum
    return [ordered]@{
        average = [math]::Round($measured.Average, 1)
        maximum = [math]::Round($measured.Maximum, 1)
    }
}

# Die letzte ring_full-Meldung trägt den kumulierten Zähler.
$dropped = 0
$ringFull = $proxyEvents | Where-Object { $_.event -eq 'ring_full' } |
    Select-Object -Last 1
if ($ringFull) { $dropped = [int](Field $ringFull.message 'dropped') }

$submitted = 0
$lastProgress = $hostEvents | Where-Object { $_.event -eq 'frame_progress' } |
    Select-Object -Last 1
if ($lastProgress) { $submitted = [int](Field $lastProgress.message 'submitted') }

$consumed = 0
$lastIpc = $hostEvents | Where-Object { $_.event -eq 'ipc_frame' } |
    Select-Object -Last 1
if ($lastIpc) { $consumed = [int](Field $lastIpc.message 'consumed') }

$firstTime = $hostEvents[0].time
$lastTime = $hostEvents[-1].time
$duration = [datetime]$lastTime - [datetime]$firstTime

# --- EXE-Hash aus dem Stage-Manifest des Meilensteins -----------------------
$milestone = 'm5'
if ($runDirectory.Name -match '^(m\d)-fear-') { $milestone = $Matches[1] }
$manifestPath = Join-Path $cfg.ProjectRoot "stage\$milestone-deployment.json"
$runtimeSha = $null
if (Test-Path -LiteralPath $manifestPath -PathType Leaf) {
    $runtimeSha = (Get-Content -LiteralPath $manifestPath -Raw |
        ConvertFrom-Json).runtimeSha256
}

$report = [ordered]@{
    run = $runDirectory.Name
    durationMinutes = [math]::Round($duration.TotalMinutes, 1)
    hostVersion = Field $hostStart 'version'
    hostGit = Field $hostStart 'git'
    proxyVersion = Field $proxyStart 'version'
    proxyGit = Field $proxyStart 'git'
    retailExeSha256 = $runtimeSha
    gpu = First-Message $hostEvents 'd3d11_adapter'
    openXrRuntime = First-Message $hostEvents 'runtime'
    swapchains = First-Message $hostEvents 'swapchains'
    sharedResources = First-Message $proxyEvents 'shared_resources'
    submittedFrames = $submitted
    consumedGameFrames = $consumed
    droppedFrames = $dropped
    reusedFrames = ($windows | ForEach-Object { $_.reused } |
        Measure-Object -Sum).Sum
    xrFps = Stat ($windows | ForEach-Object { $_.xrFps })
    gameFps = Stat ($windows | ForEach-Object { $_.gameFps })
    renderLeftUs = Stat ($windows | ForEach-Object { $_.renderLeftAvgUs })
    renderRightUs = Stat ($windows | ForEach-Object { $_.renderRightAvgUs })
    hostCopyUs = Stat ($windows | ForEach-Object { $_.copyAvgUs })
    handlesStart = [int](Field $hostStart 'handles')
    handlesEnd = [int](Field $hostStop 'handles')
    perfWindows = $windows.Count
}

if ($AsJson) {
    $report | ConvertTo-Json -Depth 5
    return
}

function Row($label, $value) {
    if ($null -eq $value -or "$value" -eq '') { $value = 'nicht erfasst' }
    Write-Host ("  {0,-26} {1}" -f $label, $value)
}

Write-Host "=== Performancebericht: $($report.run) ===" -ForegroundColor Cyan
Row 'Laufzeit (min)' $report.durationMinutes
Row 'Host-Version/Git' "$($report.hostVersion) / $($report.hostGit)"
Row 'Proxy-Version/Git' "$($report.proxyVersion) / $($report.proxyGit)"
Row 'FEAR.exe SHA-256' $report.retailExeSha256
Row 'GPU / Adapter-LUID' $report.gpu
Row 'OpenXR-Runtime' $report.openXrRuntime
Row 'Swapchains' $report.swapchains
Row 'Shared-Texture' $report.sharedResources
Write-Host '  --- Frames ---'
Row 'eingereicht (XR)' $report.submittedFrames
Row 'konsumiert (Spiel)' $report.consumedGameFrames
Row 'dropped (Ring voll)' $report.droppedFrames
Row 'reused (kein neues Bild)' $report.reusedFrames
Write-Host '  --- Raten und Zeiten ---'
if ($report.perfWindows -eq 0) {
    Write-Host '  Keine perf_frame-Fenster im Log. Der Lauf stammt aus einem' `
        -ForegroundColor Yellow
    Write-Host '  Build vor der Performanceinstrumentierung.' `
        -ForegroundColor Yellow
} else {
    Row 'XR-Displayrate (fps)' "Ø $($report.xrFps.average) / max $($report.xrFps.maximum)"
    Row 'Game-FPS' "Ø $($report.gameFps.average) / max $($report.gameFps.maximum)"
    Row 'Renderzeit links (us)' "Ø $($report.renderLeftUs.average) / max $($report.renderLeftUs.maximum)"
    Row 'Renderzeit rechts (us)' "Ø $($report.renderRightUs.average) / max $($report.renderRightUs.maximum)"
    Row 'Host-Copyzeit (us)' "Ø $($report.hostCopyUs.average) / max $($report.hostCopyUs.maximum)"
}
Write-Host '  --- Ressourcen ---'
Row 'Handles Start/Ende' "$($report.handlesStart) / $($report.handlesEnd)"

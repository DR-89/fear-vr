<#
.SYNOPSIS
    Schließt SteamVRs verzögert eingeblendete Theaterfläche einmalig.

.DESCRIPTION
    Beobachtet nach dem Steam-Start kurz das SteamVR-Hauptdashboard. Sobald
    Steam es für das laufende Desktopspiel sichtbar macht, wird es genau
    einmal geschlossen. Danach endet der Wächter, damit das Dashboard später
    normal benutzt werden kann.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [int]$GamePid,

    [Parameter(Mandatory)]
    [string]$LogPath,

    [string]$OverlayKey = 'valve.steam.desktopgame.21090',

    [ValidateRange(5, 600)]
    [int]$TimeoutSeconds = 300
)

$ErrorActionPreference = 'SilentlyContinue'
$vrCmd = 'C:\Program Files (x86)\Steam\steamapps\common\SteamVR' +
    '\bin\win64\vrcmd.exe'
$utf8WithoutBom = New-Object Text.UTF8Encoding($false)

function Write-GuardLog([string]$message) {
    $line = '{0:o} {1}{2}' -f (
        Get-Date
    ).ToUniversalTime(), $message, [Environment]::NewLine
    [IO.File]::AppendAllText($LogPath, $line, $utf8WithoutBom)
}

if (-not (Test-Path -LiteralPath $vrCmd -PathType Leaf)) {
    Write-GuardLog "vrcmd_missing path=$vrCmd"
    exit 1
}

Write-GuardLog "guard_started game_pid=$GamePid timeout=$TimeoutSeconds"
$deadline = (Get-Date).AddSeconds($TimeoutSeconds)
while ((Get-Date) -lt $deadline) {
    if ($null -eq (Get-Process -Id $GamePid -ErrorAction SilentlyContinue)) {
        Write-GuardLog 'game_exited'
        exit 0
    }

    $overlays = @(& $vrCmd --overlays 2>$null)
    $theaterOverlayVisible = @(
        $overlays | Where-Object {
            $_ -match (
                "^'" + [Regex]::Escape($OverlayKey) +
                "' -- '.*', \d+x\d+ visible "
            )
        }
    ).Count -gt 0
    if ($theaterOverlayVisible) {
        & $vrCmd --compositorcmd disable_theater_mode 2>$null |
            Out-Null
        Start-Sleep -Milliseconds 250
        & $vrCmd --hidedashboard 2>$null | Out-Null
        Start-Sleep -Milliseconds 250
        $verification = @(& $vrCmd --overlays 2>$null)
        $stillVisible = @(
            $verification | Where-Object {
                $_ -match (
                    "^'" + [Regex]::Escape($OverlayKey) +
                    "' -- '.*', \d+x\d+ visible "
                )
            }
        ).Count -gt 0
        if ($stillVisible) {
            Write-GuardLog "theater_hide_failed overlay=$OverlayKey"
            exit 2
        }
        Write-GuardLog "delayed_theater_hidden overlay=$OverlayKey"
        exit 0
    }
    Start-Sleep -Milliseconds 500
}

Write-GuardLog 'timeout_without_visible_theater'

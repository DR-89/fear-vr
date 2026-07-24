<#
.SYNOPSIS
    Startet den neu gebauten Stock-Client in der lokalen Public-Tools-Runtime.

.DESCRIPTION
    Verifiziert das Deploymentmanifest und alle drei Module, verwendet ein
    isoliertes Benutzerverzeichnis unter stage und startet die originale,
    hashverifizierte Steam-FEAR.exe über den offiziellen Steam-App-Start. Eine
    projektlokale -archcfg hängt die losen, selbst gebauten Module als letzte
    Archivschicht ein.

    FEARDevSP.exe wird wegen ihrer alten CD/DVD-Prüfung nicht verwendet.
    Die Steam-/Retail-Installation bleibt unverändert.

.PARAMETER Wait
    Wartet bis zum Ende des Spielprozesses und gibt dessen Exitcode zurück.
#>
[CmdletBinding()]
param(
    [switch]$Wait
)

$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\_fearvr-env.ps1"
$cfg = Get-FearVrConfig
Assert-RetailFearExe | Out-Null

$manifestPath = Assert-UnderProjectRoot (Join-Path $cfg.ProjectRoot 'stage\m0-stock-deployment.json')
if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
    throw 'Kein M0-Stock-Deployment. Zuerst tools\deploy-stock-game-modules.ps1 ausführen.'
}

$manifest = Get-Content -Raw -LiteralPath $manifestPath | ConvertFrom-Json
if (-not (Test-Path -LiteralPath $manifest.runtimeExe -PathType Leaf)) {
    throw "FEAR.exe fehlt: $($manifest.runtimeExe)"
}
if ((Get-FileSha256 $manifest.runtimeExe) -ne $cfg.ExpectedSha256) {
    throw 'Zu startende FEAR.exe stimmt nicht mit der verifizierten Steam-Fassung überein.'
}
if (-not (Test-Path -LiteralPath $manifest.steamExe -PathType Leaf)) {
    throw "Steam-Client fehlt: $($manifest.steamExe)"
}
if (-not (Test-Path -LiteralPath $manifest.archiveConfig -PathType Leaf)) {
    throw "M0-Archivkonfiguration fehlt: $($manifest.archiveConfig)"
}
if ((Get-FileSha256 $manifest.archiveConfig) -ne $manifest.archiveConfigSha256) {
    throw 'M0-Archivkonfiguration wurde nach dem Deployment verändert.'
}

$runtimeGame = $manifest.moduleDirectory
if (-not (Test-Path -LiteralPath $runtimeGame -PathType Container)) {
    throw "M0-Modulverzeichnis fehlt: $runtimeGame"
}
foreach ($record in $manifest.modules) {
    $path = Join-Path $runtimeGame $record.name
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Deploymentmodul fehlt: $path"
    }
    $actualHash = Get-FileSha256 $path
    if ($actualHash -ne $record.sha256) {
        throw "Deploymentmodul wurde nachträglich verändert: $($record.name)"
    }
}

$userDirectory = Assert-UnderProjectRoot $manifest.userDirectory
New-Item -ItemType Directory -Force -Path $userDirectory | Out-Null
$argumentLine = "-applaunch $($manifest.steamAppId) -archcfg `"$($manifest.archiveConfig)`" -userdirectory `"$userDirectory`""

$existingFearIds = @(
    Get-Process -Name 'FEAR' -ErrorAction SilentlyContinue |
        ForEach-Object { $_.Id }
)
if ($existingFearIds.Count -gt 0) {
    throw "Vor dem M0-Test läuft bereits FEAR.exe (PID: $($existingFearIds -join ', '))."
}

Write-Host '=== M0 Stock-Lauftest ===' -ForegroundColor Cyan
Write-Host "EXE:        $($manifest.runtimeExe)"
Write-Host "Steam:      $($manifest.steamExe) -applaunch $($manifest.steamAppId)"
Write-Host "WorkingDir: $($manifest.workingDirectory)"
Write-Host "ArchCfg:    $($manifest.archiveConfig)"
Write-Host "Userdata:   $userDirectory"
Write-Host 'Module:     lokal gebautes GameClient/GameServer/ClientFx'

$steamProcess = Start-Process -FilePath $manifest.steamExe `
    -ArgumentList $argumentLine `
    -WorkingDirectory (Split-Path -Parent $manifest.steamExe) `
    -PassThru
Write-Host "Steam-Aufruf gesendet (PID $($steamProcess.Id)); warte auf FEAR.exe ..."

$deadline = (Get-Date).AddSeconds(20)
$process = $null
do {
    Start-Sleep -Milliseconds 250
    $process = Get-Process -Name 'FEAR' -ErrorAction SilentlyContinue |
        Where-Object { $_.Id -notin $existingFearIds } |
        Select-Object -First 1
} until ($null -ne $process -or (Get-Date) -ge $deadline)

if ($null -eq $process) {
    throw 'Steam hat innerhalb von 20 Sekunden keinen FEAR.exe-Prozess gestartet. M0 ist nicht bestanden.'
}

Write-Host "FEAR.exe gestartet (PID $($process.Id)); prüfe das geladene GameClient-Modul ..."
$expectedGameClient = [IO.Path]::GetFullPath((Join-Path $runtimeGame 'GameClient.dll'))
$moduleDeadline = (Get-Date).AddSeconds(30)
$loadedGameClient = $null
do {
    Start-Sleep -Milliseconds 250
    $process.Refresh()
    if ($process.HasExited) {
        throw "FEAR.exe wurde während der Modulprüfung beendet (Exitcode $($process.ExitCode)). M0 ist nicht bestanden."
    }
    try {
        $loadedGameClient = $process.Modules |
            Where-Object { $_.ModuleName -ieq 'GameClient.dll' } |
            Select-Object -First 1
    } catch {
        $loadedGameClient = $null
    }
} until ($null -ne $loadedGameClient -or (Get-Date) -ge $moduleDeadline)

if ($null -eq $loadedGameClient) {
    throw 'GameClient.dll wurde innerhalb von 30 Sekunden nicht geladen. M0 ist nicht bestanden.'
}

$actualGameClient = [IO.Path]::GetFullPath($loadedGameClient.FileName)
if ($actualGameClient -ne $expectedGameClient) {
    throw "Falsches GameClient-Modul geladen: $actualGameClient"
}

Write-Host "M0-Modulprüfung bestanden: $actualGameClient" -ForegroundColor Green

if ($Wait) {
    $process.WaitForExit()
    Write-Host "FEAR.exe beendet (Exitcode $($process.ExitCode))."
    exit $process.ExitCode
}

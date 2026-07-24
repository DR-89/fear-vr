<#
.SYNOPSIS
    Erstellt die Retail-schonende M2-archcfg-Stage.

.DESCRIPTION
    Kopiert den ABI-neutralen GameClient-Loader, die M2-Bridge und das
    unveränderte originale VC7.1-GameClient-Modul ausschließlich nach
    stage\m2-game. Retail wird vor und nach dem Vorgang verifiziert.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\_fearvr-env.ps1"
$cfg = Get-FearVrConfig

$retailBefore = Assert-RetailFearExe
$retailArchCfg = Join-Path $cfg.RetailRoot 'Default.archcfg'
$steamExe = 'C:\Program Files (x86)\Steam\steam.exe'
$originalGameDirectory = Assert-UnderProjectRoot (
    Join-Path $cfg.ProjectRoot 'vendor-local\publictools\Dev\Runtime\Game'
)
$originalGameClient = Join-Path $originalGameDirectory 'GameClient.dll'
$loaderSource = Assert-UnderProjectRoot (
    Join-Path $cfg.ProjectRoot 'build\x86\src\gameclient_loader\RelWithDebInfo\GameClient.dll'
)
$bridgeSource = Assert-UnderProjectRoot (
    Join-Path $cfg.ProjectRoot 'build\x86\src\proxy32\RelWithDebInfo\fearvr-d3d9.dll'
)

foreach ($required in @(
    $retailArchCfg,
    $steamExe,
    $originalGameClient,
    $loaderSource,
    $bridgeSource
)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "M2-Eingabedatei fehlt: $required"
    }
}

$expectedOriginalHash =
    'B5F1F1976227FD0E6F1C32BD2BEEDFB117E68A87A07BB42D06BE489DD08A63BA'
$actualOriginalHash = Get-FileSha256 $originalGameClient
if ($actualOriginalHash -ne $expectedOriginalHash) {
    throw "Originales VC7.1-GameClient.dll ist nicht wiederhergestellt: $actualOriginalHash"
}

$stageRoot = Assert-UnderProjectRoot (
    Join-Path $cfg.ProjectRoot 'stage\m2-game'
)
$userDirectory = Assert-UnderProjectRoot (
    Join-Path $cfg.ProjectRoot 'stage\userdata-m2'
)
$logDirectory = Assert-UnderProjectRoot (
    Join-Path $cfg.ProjectRoot 'logs'
)
New-Item -ItemType Directory -Force -Path $stageRoot | Out-Null
New-Item -ItemType Directory -Force -Path $userDirectory | Out-Null
New-Item -ItemType Directory -Force -Path $logDirectory | Out-Null

$stagedFiles = [ordered]@{
    'GameClient.dll' = $loaderSource
    'GameOrig.dll' = $originalGameClient
    'fearvr-d3d9.dll' = $bridgeSource
    'GameServer.dll' = Join-Path $originalGameDirectory 'GameServer.dll'
    'ClientFx.fxd' = Join-Path $originalGameDirectory 'ClientFx.fxd'
    'FEAR.dep' = Join-Path $originalGameDirectory 'FEAR.dep'
    'FEARMod.Arch00s' = Join-Path $originalGameDirectory 'FEARMod.Arch00s'
}
foreach ($name in $stagedFiles.Keys) {
    if (-not (Test-Path -LiteralPath $stagedFiles[$name] -PathType Leaf)) {
        throw "Originale Modulset-Datei fehlt: $($stagedFiles[$name])"
    }
    Copy-Item -LiteralPath $stagedFiles[$name] `
        -Destination (Join-Path $stageRoot $name) -Force
}

$archiveConfig = Assert-UnderProjectRoot (
    Join-Path $cfg.ProjectRoot 'stage\m2.archcfg'
)
$archiveLines = @(
    Get-Content -LiteralPath $retailArchCfg -Encoding Default |
        Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
)
$archiveLines += $stageRoot
[IO.File]::WriteAllLines(
    $archiveConfig, $archiveLines, [Text.Encoding]::ASCII
)

$records = foreach ($name in $stagedFiles.Keys) {
    $path = Join-Path $stageRoot $name
    [ordered]@{
        name = $name
        sha256 = Get-FileSha256 $path
        bytes = (Get-Item -LiteralPath $path).Length
    }
}
$manifestPath = Assert-UnderProjectRoot (
    Join-Path $cfg.ProjectRoot 'stage\m2-deployment.json'
)
[ordered]@{
    preparedUtc = (Get-Date).ToUniversalTime().ToString('s') + 'Z'
    runtimeExe = $retailBefore.Path
    runtimeSha256 = $retailBefore.Sha256
    steamExe = $steamExe
    steamAppId = 21090
    workingDirectory = $cfg.RetailRoot
    moduleDirectory = $stageRoot
    originalModuleDirectory = $originalGameDirectory
    archiveConfig = $archiveConfig
    archiveConfigSha256 = Get-FileSha256 $archiveConfig
    userDirectory = $userDirectory
    logDirectory = $logDirectory
    files = @($records)
} | ConvertTo-Json -Depth 5 |
    Out-File -Encoding utf8 -LiteralPath $manifestPath

$retailAfter = Assert-RetailFearExe
if ($retailBefore.Sha256 -ne $retailAfter.Sha256) {
    throw 'SICHERHEITSABBRUCH: Retail-FEAR.exe wurde verändert.'
}

Write-Host 'M2-Stage bereit; Retail unverändert.' -ForegroundColor Green
Write-Host "Module:   $stageRoot"
Write-Host "ArchCfg:  $archiveConfig"
Write-Host "Manifest: $manifestPath"

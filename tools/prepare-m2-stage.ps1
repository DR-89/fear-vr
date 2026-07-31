<#
.SYNOPSIS
    Erstellt eine M2- bis M5-archcfg-Stage samt frühem dinput8-HID-Fix.

.DESCRIPTION
    Kopiert den ABI-neutralen GameClient-Loader, die VR-Bridge und das
    unveränderte originale VC7.1-GameClient-Modul nach stage\m2-game.
    Der eigene dinput8-Proxy wird neben FEAR.exe installiert, damit der
    bekannte F.E.A.R.-1.08-HID-Performancefehler bereits vor der
    DirectInput-Initialisierung abgeschaltet werden kann. FEAR.exe selbst
    wird vor und nach dem Vorgang verifiziert und nie verändert.
#>
[CmdletBinding()]
param(
    [ValidateSet('M2', 'M3', 'M4', 'M5')]
    [string]$Milestone = 'M2'
)

$ErrorActionPreference = 'Stop'
$milestoneLabel = $Milestone.ToUpperInvariant()
$milestoneSlug = $Milestone.ToLowerInvariant()
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
$dinputSource = Assert-UnderProjectRoot (
    Join-Path $cfg.ProjectRoot 'build\x86\src\dinput8_proxy\RelWithDebInfo\dinput8.dll'
)

foreach ($required in @(
    $retailArchCfg,
    $steamExe,
    $originalGameClient,
    $loaderSource,
    $bridgeSource,
    $dinputSource
)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "$milestoneLabel-Eingabedatei fehlt: $required"
    }
}

$dinputDestination = Join-Path $cfg.RetailRoot 'dinput8.dll'
if (Test-Path -LiteralPath $dinputDestination -PathType Leaf) {
    $existingDinputHash = Get-FileSha256 $dinputDestination
    $knownPreviousHashes = @(
        Get-ChildItem -LiteralPath (
            Join-Path $cfg.ProjectRoot 'stage') -Filter '*-deployment.json' `
            -File -ErrorAction SilentlyContinue |
            ForEach-Object {
                try {
                    (Get-Content -Raw -LiteralPath $_.FullName |
                        ConvertFrom-Json).dinputProxySha256
                } catch {
                    $null
                }
            }
    )
    $sourceHash = Get-FileSha256 $dinputSource
    if ($existingDinputHash -ne $sourceHash -and
        $existingDinputHash -notin $knownPreviousHashes) {
        throw (
            'Im Retail-Ordner liegt bereits eine fremde dinput8.dll. ' +
            'Sie wird nicht überschrieben: ' + $dinputDestination)
    }
}
Copy-Item -LiteralPath $dinputSource -Destination $dinputDestination -Force

# Der klassische Renderer wird vor GameClient und DirectInput initialisiert.
# Nur ein d3d9-Proxy direkt neben FEAR.exe kann daher CreateDevice garantiert
# vor der Geräteerzeugung sehen und D3DCREATE_MULTITHREADED setzen. Einen
# bereits vorhandenen Wrapper sichern wir wiederherstellbar und ketten ihn aus
# unserer Bridge weiter, statt ihn ersatzlos zu verlieren.
$d3d9Destination = Join-Path $cfg.RetailRoot 'd3d9.dll'
$d3d9UpstreamDestination =
    Join-Path $cfg.RetailRoot 'd3d9.fearvr-upstream.dll'
$bridgeHash = Get-FileSha256 $bridgeSource
if (Test-Path -LiteralPath $d3d9Destination -PathType Leaf) {
    $existingD3d9Hash = Get-FileSha256 $d3d9Destination
    if ($existingD3d9Hash -ne $bridgeHash) {
        $existingIsPriorFearVr = $false
        $previousManifestPath = Join-Path $cfg.ProjectRoot (
            "stage\$milestoneSlug-deployment.json")
        if (Test-Path -LiteralPath $previousManifestPath -PathType Leaf) {
            try {
                $previousManifest = Get-Content -Raw `
                    -LiteralPath $previousManifestPath | ConvertFrom-Json
                $existingIsPriorFearVr =
                    $previousManifest.d3d9ProxySha256 -eq
                        $existingD3d9Hash
            } catch {
                $existingIsPriorFearVr = $false
            }
        }
        if (-not $existingIsPriorFearVr -and
            (Test-Path -LiteralPath $d3d9UpstreamDestination -PathType Leaf)) {
            throw (
                'Ein fremder d3d9.dll-Wrapper und bereits eine andere ' +
                'F.E.A.R.-VR-Sicherung sind vorhanden. Aus Sicherheitsgründen ' +
                'wurde nichts überschrieben: ' + $d3d9Destination)
        }
        if (-not $existingIsPriorFearVr) {
            Copy-Item -LiteralPath $d3d9Destination `
                -Destination $d3d9UpstreamDestination
        }
    }
}
Copy-Item -LiteralPath $bridgeSource -Destination $d3d9Destination -Force

$expectedOriginalHash =
    'B5F1F1976227FD0E6F1C32BD2BEEDFB117E68A87A07BB42D06BE489DD08A63BA'
$actualOriginalHash = Get-FileSha256 $originalGameClient
if ($actualOriginalHash -ne $expectedOriginalHash) {
    throw "Originales VC7.1-GameClient.dll ist nicht wiederhergestellt: $actualOriginalHash"
}

$stageRoot = Assert-UnderProjectRoot (
    Join-Path $cfg.ProjectRoot "stage\$milestoneSlug-game"
)
$userDirectory = Assert-UnderProjectRoot (
    Join-Path $cfg.ProjectRoot "stage\userdata-$milestoneSlug"
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

# Body_Group enthaelt bei Retail Arme, Torso und Beine gemeinsam. Erzeuge aus
# dem lokal installierten Modell und der Textur einen Alpha-Test-Override,
# dessen rasterisierte Armgeometrie nur Ober-/Unterarme entfernt. Haende,
# Torso und Beine/Kicks bleiben sichtbar.
& "$PSScriptRoot\release\new-body-assets.ps1" `
    -SourceGame $originalGameDirectory `
    -DestinationGame $stageRoot

$archiveConfig = Assert-UnderProjectRoot (
    Join-Path $cfg.ProjectRoot "stage\$milestoneSlug.archcfg"
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
    Join-Path $cfg.ProjectRoot (
        "stage\$milestoneSlug-deployment.json"
    )
)
[ordered]@{
    milestone = $milestoneLabel
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
    dinputProxy = $dinputDestination
    dinputProxySha256 = Get-FileSha256 $dinputDestination
    d3d9Proxy = $d3d9Destination
    d3d9ProxySha256 = Get-FileSha256 $d3d9Destination
    d3d9Upstream = if (
        Test-Path -LiteralPath $d3d9UpstreamDestination -PathType Leaf
    ) { $d3d9UpstreamDestination } else { $null }
    files = @($records)
} | ConvertTo-Json -Depth 5 |
    Out-File -Encoding utf8 -LiteralPath $manifestPath

$retailAfter = Assert-RetailFearExe
if ($retailBefore.Sha256 -ne $retailAfter.Sha256) {
    throw 'SICHERHEITSABBRUCH: Retail-FEAR.exe wurde verändert.'
}

Write-Host "$milestoneLabel-Stage bereit; FEAR.exe unverändert." `
    -ForegroundColor Green
Write-Host "Module:   $stageRoot"
Write-Host "HID-Fix:  $dinputDestination"
Write-Host "D3D9:     $d3d9Destination"
Write-Host "ArchCfg:  $archiveConfig"
Write-Host "Manifest: $manifestPath"

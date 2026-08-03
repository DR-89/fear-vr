<#
.SYNOPSIS
    Schnuert ein weitergebbares F.E.A.R.-VR-Paket (ANWEISUNG.md §13, M6).

.DESCRIPTION
    Erzeugt unter dist\ einen Ordner samt ZIP mit ausschliesslich eigenen,
    MIT-lizenzierten Binaries, den Installations- und Startskripten sowie der
    Dokumentation.

    Enthaelt bewusst KEINE Retail- und KEINE Public-Tools-Dateien. Die fuenf
    proprietaeren Module holt install.ps1 auf dem Zielrechner aus dessen
    eigener Public-Tools-Installation.

.PARAMETER Configuration
    Buildkonfiguration der zu verpackenden Artefakte.

.PARAMETER SkipBuild
    Verwendet die vorhandenen Buildartefakte, statt vorher zu bauen.

.PARAMETER NoArchive
    Erzeugt nur den Ordner, kein ZIP.
#>
[CmdletBinding()]
param(
    [ValidateSet('RelWithDebInfo', 'Release')]
    [string]$Configuration = 'RelWithDebInfo',

    [switch]$SkipBuild,

    [switch]$NoArchive
)

$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\_fearvr-env.ps1"
$cfg = Get-FearVrConfig

Write-Host '=== F.E.A.R. VR - Release schnueren ===' -ForegroundColor Cyan

if (-not $SkipBuild) {
    & "$PSScriptRoot\build-all.ps1" -Configuration $Configuration
}

$gitCommit = (& git -C $cfg.ProjectRoot rev-parse --short HEAD).Trim()
$gitDirty = [bool](& git -C $cfg.ProjectRoot status --porcelain)
if ($gitDirty) {
    Write-Host 'WARNUNG: Der Arbeitsbaum ist nicht sauber. Das Paket ist damit keinem Commit eindeutig zuzuordnen.' `
        -ForegroundColor Yellow
}
# Version und Prerelease-Label kommen aus CMakeLists.txt, damit Paketname,
# Manifest und die in die Binaries kompilierte Versionszeichenkette nicht
# auseinanderlaufen koennen.
$cmakeLists = Get-Content -Raw -LiteralPath (Join-Path $cfg.ProjectRoot 'CMakeLists.txt')
if ($cmakeLists -notmatch '(?m)^\s*VERSION\s+(\d+\.\d+\.\d+)\s*$') {
    throw 'In CMakeLists.txt wurde keine project(VERSION x.y.z) gefunden.'
}
$semver = $Matches[1]
$label = ''
if ($cmakeLists -match '(?m)^\s*set\(FEARVR_VERSION_LABEL\s+"([^"]*)"\)') {
    $label = $Matches[1]
}
if ($label) { $semver = "$semver-$label" }
$version = "$semver+$gitCommit"

$distRoot = Assert-UnderProjectRoot (Join-Path $cfg.ProjectRoot 'dist')
$packageName = "fearvr-$version"
$packageRoot = Assert-UnderProjectRoot (Join-Path $distRoot $packageName)
if (Test-Path -LiteralPath $packageRoot) {
    Remove-Item -LiteralPath $packageRoot -Recurse -Force
}
foreach ($sub in @('bin\x64', 'bin\x86', 'tools', 'docs')) {
    New-Item -ItemType Directory -Force -Path (Join-Path $packageRoot $sub) | Out-Null
}

# --- Eigene Binaries. Nur MIT-lizenzierter Code, nichts Proprietaeres. -------
$binaries = [ordered]@{
    'bin\x64\fearvr-host.exe' =
        "build\x64\src\host64\$Configuration\fearvr-host.exe"
    'bin\x86\GameClient.dll' =
        "build\x86\src\gameclient_loader\$Configuration\GameClient.dll"
    'bin\x86\fearvr-d3d9.dll' =
        "build\x86\src\proxy32\$Configuration\fearvr-d3d9.dll"
}
foreach ($target in $binaries.Keys) {
    $source = Join-Path $cfg.ProjectRoot $binaries[$target]
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
        throw "Artefakt fehlt: $source. Zuerst tools\build-all.ps1 ausfuehren."
    }
    Copy-Item -LiteralPath $source -Destination (Join-Path $packageRoot $target) -Force
}

# --- Skripte ----------------------------------------------------------------
foreach ($script in @(
    'release\_fearvr-release.ps1',
    'release\install.ps1',
    'release\play.ps1',
    'release\uninstall.ps1',
    'release\new-body-assets.ps1',
    'disable-steamvr-theater.ps1',
    'hide-steamvr-theater.ps1'
)) {
    $source = Join-Path $cfg.ProjectRoot "tools\$script"
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
        throw "Skript fehlt: $source"
    }
    $destination = Join-Path $packageRoot (
        Join-Path 'tools' (Split-Path -Leaf $script))
    Copy-Item -LiteralPath $source -Destination $destination -Force
}

# --- Anklickbare Starter im Paketwurzelverzeichnis --------------------------
# Windows oeffnet eine .ps1 per Doppelklick im Editor statt sie auszufuehren.
# Diese beiden .cmd rufen die Skripte mit gelockerter Ausfuehrungsrichtlinie
# auf und halten das Fenster danach offen.
foreach ($launcher in @('Install.cmd', 'Uninstall.cmd')) {
    $source = Join-Path $cfg.ProjectRoot "tools\release\$launcher"
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
        throw "Starter fehlt: $source"
    }
    Copy-Item -LiteralPath $source `
        -Destination (Join-Path $packageRoot $launcher) -Force
}

# Die beiden SteamVR-Helfer dot-sourcen im Repo _fearvr-env.ps1. Im Paket
# gibt es das nicht, deshalb wird auf die Release-Fassung umgebogen.
foreach ($name in @('disable-steamvr-theater.ps1', 'hide-steamvr-theater.ps1')) {
    $path = Join-Path $packageRoot "tools\$name"
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { continue }
    $text = [IO.File]::ReadAllText($path)
    $text = $text -replace '_fearvr-env\.ps1', '_fearvr-release.ps1'
    $text = $text -replace 'Get-FearVrConfig', 'Get-FearVrReleaseConfig'
    [IO.File]::WriteAllText($path, $text, (New-Object Text.UTF8Encoding($true)))
}

# --- Dokumentation und Lizenzen ---------------------------------------------
foreach ($document in @(
    @{ Source = 'LICENSE'; Target = 'LICENSE' },
    @{ Source = 'THIRD_PARTY_NOTICES.md'; Target = 'THIRD_PARTY_NOTICES.md' },
    @{ Source = 'docs\OPENXR-INPUT.md'; Target = 'docs\OPENXR-INPUT.md' },
    @{ Source = 'docs\WEAPON_COLLISION.md'; Target = 'docs\WEAPON_COLLISION.md' }
)) {
    $source = Join-Path $cfg.ProjectRoot $document.Source
    if (Test-Path -LiteralPath $source -PathType Leaf) {
        Copy-Item -LiteralPath $source `
            -Destination (Join-Path $packageRoot $document.Target) -Force
    }
}
Copy-Item -LiteralPath (Join-Path $cfg.ProjectRoot 'tools\release\README-PACKAGE.md') `
    -Destination (Join-Path $packageRoot 'README.md') -Force

# --- Manifest ---------------------------------------------------------------
# install.ps1 prueft damit vor der Installation, dass das Paket unveraendert
# ist.
$files = foreach ($item in Get-ChildItem -LiteralPath $packageRoot -Recurse -File) {
    $relative = $item.FullName.Substring($packageRoot.Length + 1)
    [ordered]@{
        path = $relative
        sha256 = Get-FileSha256 $item.FullName
        bytes = $item.Length
    }
}
[ordered]@{
    product = 'F.E.A.R. VR'
    version = $version
    gitCommit = $gitCommit
    gitWorkingTreeDirty = $gitDirty
    builtUtc = (Get-Date).ToUniversalTime().ToString('s') + 'Z'
    configuration = $Configuration
    containsRetailContent = $false
    containsPublicToolsContent = $false
    files = @($files)
} | ConvertTo-Json -Depth 5 |
    Out-File -Encoding utf8 -LiteralPath (Join-Path $packageRoot 'release-manifest.json')

# --- Archiv -----------------------------------------------------------------
$archivePath = "$packageRoot.zip"
if (-not $NoArchive) {
    if (Test-Path -LiteralPath $archivePath) {
        Remove-Item -LiteralPath $archivePath -Force
    }
    Compress-Archive -Path (Join-Path $packageRoot '*') `
        -DestinationPath $archivePath
}

# --- Gegenprobe: nichts Proprietaeres im Paket ------------------------------
# Ein Public-Tools-Modul im Paket waere eine Lizenzverletzung. Der Test
# vergleicht gegen die bekannten Dateinamen und den Hash des Originalmoduls.
$forbiddenNames = @(
    'GameOrig.dll', 'GameServer.dll', 'ClientFx.fxd',
    'FEAR.dep', 'FEARMod.Arch00s', 'FEAR.exe'
)
$originalGameClient =
    'B5F1F1976227FD0E6F1C32BD2BEEDFB117E68A87A07BB42D06BE489DD08A63BA'
foreach ($item in Get-ChildItem -LiteralPath $packageRoot -Recurse -File) {
    if ($forbiddenNames -contains $item.Name) {
        throw "LIZENZABBRUCH: Propriataere Datei im Paket: $($item.Name)"
    }
    if ((Get-FileSha256 $item.FullName) -eq $originalGameClient) {
        throw "LIZENZABBRUCH: Originales Public-Tools-Modul im Paket: $($item.FullName)"
    }
}

$sizeMb = [math]::Round(((Get-ChildItem -LiteralPath $packageRoot -Recurse -File |
    Measure-Object -Property Length -Sum).Sum / 1MB), 1)

Write-Host ''
Write-Host "Paket erstellt: $packageName ($sizeMb MB)" -ForegroundColor Green
Write-Host "Ordner:  $packageRoot"
if (-not $NoArchive) { Write-Host "Archiv:  $archivePath" }
Write-Host 'Enthaelt keine Retail- und keine Public-Tools-Dateien.'

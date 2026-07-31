<#
.SYNOPSIS
    Erzeugt ein F.E.A.R.-VR-Overlay zum Entpacken über das vorhandene Spiel.

.DESCRIPTION
    Das ZIP besitzt keinen zusätzlichen Paket-Unterordner. Sein Inhalt wird
    direkt in den bestehenden F.E.A.R.-1.08-Ordner entpackt:

      Start F.E.A.R. VR.cmd
      Start F.E.A.R. VR - SteamVR.cmd
      dinput8.dll
      FEARVR\...

    Standardmäßig enthält es nur weitergebbare eigene Dateien. Beim ersten
    Start kopiert das Overlay die fünf benötigten proprietären Module aus der
    lokalen F.E.A.R.-Public-Tools-Installation des Besitzers.

    -PrivateBundle kopiert diese Module und die abgeleiteten Body-Assets
    bereits beim Packen hinein. Dieses Archiv ist sofort startbar, aber laut
    Public-Tools-EULA ausschließlich für den persönlichen Gebrauch bestimmt
    und darf nicht veröffentlicht oder weitergegeben werden.

.PARAMETER Configuration
    Buildkonfiguration der zu verpackenden Artefakte.

.PARAMETER SkipBuild
    Verwendet vorhandene Buildartefakte.

.PARAMETER PrivateBundle
    Erzeugt ein vollständiges privates Archiv mit lokalen Public-Tools-Dateien.

.PARAMETER PublicToolsGame
    Installationswurzel oder Dev\Runtime\Game der Public Tools 1.08 für
    -PrivateBundle. Ohne Angabe wird automatisch gesucht.

.PARAMETER NoArchive
    Erzeugt nur den Staging-Ordner unter dist\, kein ZIP.
#>
[CmdletBinding()]
param(
    [ValidateSet('RelWithDebInfo', 'Release')]
    [string]$Configuration = 'RelWithDebInfo',

    [switch]$SkipBuild,

    [switch]$PrivateBundle,

    [string]$PublicToolsGame,

    [switch]$NoArchive
)

$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\_fearvr-env.ps1"
. "$PSScriptRoot\release\_fearvr-release.ps1"
$cfg = Get-FearVrConfig
$releaseCfg = Get-FearVrReleaseConfig

Write-Host '=== F.E.A.R. VR - Retail-Overlay packen ===' -ForegroundColor Cyan

if (-not $SkipBuild) {
    & "$PSScriptRoot\build-all.ps1" -Configuration $Configuration
}

$gitCommit = (& git -C $cfg.ProjectRoot rev-parse --short HEAD).Trim()
$gitDirty = [bool](& git -C $cfg.ProjectRoot status --porcelain)
if ($gitDirty) {
    Write-Host 'WARNUNG: Der Arbeitsbaum ist nicht sauber.' -ForegroundColor Yellow
}

$cmakeLists = Get-Content -Raw -LiteralPath (
    Join-Path $cfg.ProjectRoot 'CMakeLists.txt')
if ($cmakeLists -notmatch '(?m)^\s*VERSION\s+(\d+\.\d+\.\d+)\s*$') {
    throw 'In CMakeLists.txt wurde keine project(VERSION x.y.z) gefunden.'
}
$semver = $Matches[1]
$label = ''
if ($cmakeLists -match
    '(?m)^\s*set\(FEARVR_VERSION_LABEL\s+"([^"]*)"\)') {
    $label = $Matches[1]
}
if ($label) { $semver = "$semver-$label" }
$version = "$semver+$gitCommit"

$distRoot = Assert-UnderProjectRoot (Join-Path $cfg.ProjectRoot 'dist')
$bundleSuffix = if ($PrivateBundle) { '-private' } else { '' }
$packageName = "fearvr-$version-overlay$bundleSuffix"
$packageRoot = Assert-UnderProjectRoot (Join-Path $distRoot $packageName)
$overlayRoot = Join-Path $packageRoot 'FEARVR'

if (Test-Path -LiteralPath $packageRoot) {
    Remove-Item -LiteralPath $packageRoot -Recurse -Force
}
foreach ($sub in @(
    'bin\x64',
    'game-modules',
    'tools',
    'docs',
    'assets'
)) {
    New-Item -ItemType Directory -Force -Path (
        Join-Path $overlayRoot $sub) | Out-Null
}

# Eigene x64/x86-Binaries landen bereits an ihrem endgültigen Overlay-Ort.
$startupImageSource =
    Join-Path $cfg.ProjectRoot 'assets\fearvr-startup.jpg'
if (-not (Test-Path -LiteralPath $startupImageSource -PathType Leaf)) {
    throw "Startbild fehlt: $startupImageSource"
}
Copy-Item -LiteralPath $startupImageSource `
    -Destination (Join-Path $overlayRoot 'assets\fearvr-startup.jpg') -Force

$binaries = [ordered]@{
    'bin\x64\fearvr-host.exe' =
        "build\x64\src\host64\$Configuration\fearvr-host.exe"
    'game-modules\GameClient.dll' =
        "build\x86\src\gameclient_loader\$Configuration\GameClient.dll"
    'game-modules\fearvr-d3d9.dll' =
        "build\x86\src\proxy32\$Configuration\fearvr-d3d9.dll"
}
foreach ($target in $binaries.Keys) {
    $source = Join-Path $cfg.ProjectRoot $binaries[$target]
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
        throw "Artefakt fehlt: $source. Zuerst tools\build-all.ps1 ausführen."
    }
    Copy-Item -LiteralPath $source `
        -Destination (Join-Path $overlayRoot $target) -Force
}

# Muss direkt neben FEAR.exe liegen, damit F.E.A.R.s redundante HID-
# Initialisierung noch vor dem ersten DirectInput8Create-Aufruf deaktiviert
# wird. Das ist ein eigenes, weitergebbares Mod-Binary.
$dinputSource = Join-Path $cfg.ProjectRoot (
    "build\x86\src\dinput8_proxy\$Configuration\dinput8.dll")
if (-not (Test-Path -LiteralPath $dinputSource -PathType Leaf)) {
    throw "Artefakt fehlt: $dinputSource. Zuerst tools\build-all.ps1 ausführen."
}
$dinputTarget = Join-Path $packageRoot 'dinput8.dll'
Copy-Item -LiteralPath $dinputSource -Destination $dinputTarget -Force
# Eine verifizierte zweite Kopie im Overlay erlaubt dem Starter, eine beim
# Entpacken blockierte oder von Sicherheitssoftware entfernte Root-DLL zu
# reparieren, bevor FEAR.exe gestartet wird.
Copy-Item -LiteralPath $dinputSource -Destination (
    Join-Path $overlayRoot 'game-modules\dinput8.dll') -Force

# Muss ebenfalls direkt neben FEAR.exe liegen: Nur so sieht die Bridge
# Direct3DCreate9/CreateDevice vor der Rendererinitialisierung und kann das
# klassische Gerät thread-sicher für den asynchronen Readback erzeugen.
$d3d9Target = Join-Path $packageRoot 'd3d9.dll'
Copy-Item -LiteralPath (
    Join-Path $cfg.ProjectRoot (
        "build\x86\src\proxy32\$Configuration\fearvr-d3d9.dll")) `
    -Destination $d3d9Target -Force

foreach ($script in @(
    'release\_fearvr-release.ps1',
    'release\prepare-overlay.ps1',
    'release\play.ps1',
    'release\new-body-assets.ps1'
)) {
    $source = Join-Path $cfg.ProjectRoot "tools\$script"
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
        throw "Skript fehlt: $source"
    }
    Copy-Item -LiteralPath $source `
        -Destination (Join-Path $overlayRoot (
            Join-Path 'tools' (Split-Path -Leaf $script))) -Force
}

# Doppelklick-Starter. Beide führen dasselbe x64-OpenXR-Programm aus; der
# SteamVR-Starter erzwingt lediglich Valves Runtime-Manifest.
$playLauncher = Join-Path $cfg.ProjectRoot 'tools\release\Play.cmd'
Copy-Item -LiteralPath $playLauncher `
    -Destination (Join-Path $packageRoot 'Start F.E.A.R. VR.cmd') -Force
$steamVrLauncher = Join-Path $packageRoot 'Start F.E.A.R. VR - SteamVR.cmd'
$steamVrText = [IO.File]::ReadAllText($playLauncher)
$steamVrText = $steamVrText -replace ' -RetailRoot "%~dp0\." %\*',
    ' -RetailRoot "%~dp0." -Runtime steamvr %*'
[IO.File]::WriteAllText(
    $steamVrLauncher, $steamVrText, [Text.Encoding]::ASCII)

foreach ($document in @(
    @{ Source = 'LICENSE'; Target = 'LICENSE' },
    @{ Source = 'THIRD_PARTY_NOTICES.md'; Target = 'THIRD_PARTY_NOTICES.md' },
    @{ Source = 'docs\OPENXR-INPUT.md'; Target = 'docs\OPENXR-INPUT.md' }
)) {
    $source = Join-Path $cfg.ProjectRoot $document.Source
    if (Test-Path -LiteralPath $source -PathType Leaf) {
        Copy-Item -LiteralPath $source `
            -Destination (Join-Path $overlayRoot $document.Target) -Force
    }
}
Copy-Item -LiteralPath (
    Join-Path $cfg.ProjectRoot 'tools\release\README-PACKAGE.md') `
    -Destination (Join-Path $packageRoot 'README - F.E.A.R. VR.md') -Force

$publicToolsSource = $null
$privateUpstreamD3d9Target = $null
if ($PrivateBundle) {
    if ([string]::IsNullOrWhiteSpace($PublicToolsGame)) {
        $publicToolsSource = Find-PublicToolsGame
    } else {
        $publicToolsSource = Resolve-PublicToolsGame $PublicToolsGame
    }
    if (-not (Test-PublicToolsGame $publicToolsSource)) {
        throw @'
-PrivateBundle benötigt eine lokale F.E.A.R.-Public-Tools-1.08-Installation.
Mit -PublicToolsGame kann deren Wurzel oder Dev\Runtime\Game angegeben werden.
'@
    }

    foreach ($target in $releaseCfg.PublicToolsModules.Keys) {
        $source = Join-Path $publicToolsSource (
            $releaseCfg.PublicToolsModules[$target])
        if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
            throw "Public-Tools-Modul fehlt: $source"
        }
        Copy-Item -LiteralPath $source `
            -Destination (Join-Path $overlayRoot "game-modules\$target") -Force
    }
    & (Join-Path $cfg.ProjectRoot 'tools\release\new-body-assets.ps1') `
        -SourceGame $publicToolsSource `
        -DestinationGame (Join-Path $overlayRoot 'game-modules')

    # SteamVR 2.16's x64 vrmonitor.exe still imports d3dx10_43.dll from the
    # redistributable DirectX End-User Runtimes (June 2010). Some clean Windows
    # systems miss that Steam prerequisite. The private bundle carries the
    # owner's installed redistributable and play.ps1 exposes it only through
    # the SteamVR child PATH; it never writes into Windows or SteamVR.
    $legacyD3dx10 = Join-Path $env:WINDIR 'System32\d3dx10_43.dll'
    if (Test-Path -LiteralPath $legacyD3dx10 -PathType Leaf) {
        $legacyD3dxTarget = Join-Path $overlayRoot (
            'redist\directx-jun2010\x64\d3dx10_43.dll')
        New-Item -ItemType Directory -Force -Path (
            Split-Path -Parent $legacyD3dxTarget) | Out-Null
        Copy-Item -LiteralPath $legacyD3dx10 `
            -Destination $legacyD3dxTarget -Force
    } else {
        Write-Warning (
            'd3dx10_43.dll fehlt auch auf dem Build-System; das private ' +
            'Archiv kann SteamVRs Legacy-vrmonitor-Abhängigkeit nicht bündeln.')
    }

    # Preserve parity with the owner's working retail installation. This
    # optional, pre-existing D3D9 compatibility wrapper is chained behind the
    # F.E.A.R. VR proxy. It handles old resolution/MSAA compatibility; it is
    # not a VR transport or performance component.
    $privateUpstreamD3d9 =
        Join-Path $cfg.RetailRoot 'd3d9.fearvr-upstream.dll'
    if (Test-Path -LiteralPath $privateUpstreamD3d9 -PathType Leaf) {
        $privateUpstreamD3d9Target =
            Join-Path $packageRoot 'd3d9.fearvr-upstream.dll'
        Copy-Item -LiteralPath $privateUpstreamD3d9 `
            -Destination $privateUpstreamD3d9Target -Force
    }

    @'
PRIVATE BUNDLE - DO NOT REDISTRIBUTE

This archive contains proprietary files copied from the locally installed
F.E.A.R. Public Tools 1.08 and may contain Microsoft's legacy DirectX
redistributable d3dx10_43.dll. Use this bundle only with your own legally
obtained copy of F.E.A.R. and do not publish it.
'@ | Out-File -Encoding ascii -LiteralPath (
        Join-Path $packageRoot 'PRIVATE-BUNDLE-NOT-FOR-REDISTRIBUTION.txt')
}

# Manifest relativ zu FEARVR\, weil dort auch die Laufzeitprüfung stattfindet.
# Das Manifest selbst wird anschließend geschrieben und ist nicht selbst-
# referenziell.
$files = foreach ($item in
    Get-ChildItem -LiteralPath $overlayRoot -Recurse -File) {
    $relative = $item.FullName.Substring($overlayRoot.Length + 1)
    [ordered]@{
        path = $relative
        sha256 = Get-FileSha256 $item.FullName
        bytes = $item.Length
    }
}
$rootFiles = @(
    [ordered]@{
        path = 'dinput8.dll'
        sha256 = Get-FileSha256 $dinputTarget
        bytes = (Get-Item -LiteralPath $dinputTarget).Length
    },
    [ordered]@{
        path = 'd3d9.dll'
        sha256 = Get-FileSha256 $d3d9Target
        bytes = (Get-Item -LiteralPath $d3d9Target).Length
    }
)
if ($privateUpstreamD3d9Target) {
    $rootFiles += [ordered]@{
        path = 'd3d9.fearvr-upstream.dll'
        sha256 = Get-FileSha256 $privateUpstreamD3d9Target
        bytes = (Get-Item -LiteralPath $privateUpstreamD3d9Target).Length
    }
}
[ordered]@{
    product = 'F.E.A.R. VR'
    version = $version
    layout = 'retail-overlay'
    gitCommit = $gitCommit
    gitWorkingTreeDirty = $gitDirty
    builtUtc = (Get-Date).ToUniversalTime().ToString('s') + 'Z'
    configuration = $Configuration
    steamVrCompatible = $true
    openXrApplication = $true
    redistributable = -not $PrivateBundle
    containsRetailContent = $false
    containsPublicToolsContent = [bool]$PrivateBundle
    publicToolsSource = if ($PrivateBundle) { 'local-owner-copy' } else { $null }
    rootFiles = $rootFiles
    files = @($files)
} | ConvertTo-Json -Depth 5 |
    Out-File -Encoding utf8 -LiteralPath (
        Join-Path $overlayRoot 'release-manifest.json')

# Öffentliche Pakete bleiben frei von proprietären Laufzeitdateien. Der
# private Modus hebt diese technische Paketsperre bewusst nur lokal auf; die
# EULA-Rechte werden dadurch nicht erweitert.
if (-not $PrivateBundle) {
    $forbiddenNames = @(
        'GameOrig.dll',
        'GameServer.dll',
        'ClientFx.fxd',
        'FEAR.dep',
        'FEARMod.Arch00s',
        'FEAR.exe'
    )
    foreach ($item in
        Get-ChildItem -LiteralPath $packageRoot -Recurse -File) {
        if ($forbiddenNames -contains $item.Name) {
            throw "LIZENZABBRUCH: Proprietäre Datei im öffentlichen Paket: $($item.Name)"
        }
    }
}

$archivePath = "$packageRoot.zip"
if (-not $NoArchive) {
    if (Test-Path -LiteralPath $archivePath) {
        Remove-Item -LiteralPath $archivePath -Force
    }
    Compress-Archive -Path (Join-Path $packageRoot '*') `
        -DestinationPath $archivePath -CompressionLevel Optimal
}

$sizeMb = [math]::Round(((
    Get-ChildItem -LiteralPath $packageRoot -Recurse -File |
        Measure-Object -Property Length -Sum).Sum / 1MB), 1)

Write-Host ''
Write-Host "Overlay erstellt: $packageName ($sizeMb MB)" -ForegroundColor Green
Write-Host "Ordner: $packageRoot"
if (-not $NoArchive) { Write-Host "Archiv: $archivePath" }
if ($PrivateBundle) {
    Write-Host 'PRIVAT: vollständig, aber nicht weiterverteilbar.' `
        -ForegroundColor Yellow
} else {
    Write-Host 'ÖFFENTLICH: weiterverteilbar; Public Tools werden lokal ergänzt.'
}
Write-Host 'SteamVR: über "Start F.E.A.R. VR - SteamVR.cmd" direkt auswählbar.'

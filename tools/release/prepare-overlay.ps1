<#
.SYNOPSIS
    Bereitet ein direkt in den F.E.A.R.-Ordner entpacktes VR-Overlay vor.

.DESCRIPTION
    Der Release liegt unter <RetailRoot>\FEARVR. Eigene Binaries sind bereits
    an ihrem endgültigen Ort. Falls die proprietären Public-Tools-Module nicht
    als privates Bundle beiliegen, werden sie einmalig aus der lokalen
    Public-Tools-Installation des Besitzers kopiert. Danach werden archcfg und
    deployment.json direkt im Overlay erzeugt.
#>
[CmdletBinding()]
param(
    [string]$InstallDir,

    [string]$RetailRoot,

    [string]$PublicToolsGame,

    [switch]$Force
)

$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\_fearvr-release.ps1"
$cfg = Get-FearVrReleaseConfig

if ([string]::IsNullOrWhiteSpace($InstallDir)) {
    $InstallDir = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
} else {
    $InstallDir = [IO.Path]::GetFullPath($InstallDir.Trim().Trim('"'))
}
if ([string]::IsNullOrWhiteSpace($RetailRoot)) {
    $RetailRoot = Split-Path -Parent $InstallDir
} else {
    $RetailRoot = [IO.Path]::GetFullPath($RetailRoot.Trim().Trim('"'))
}

$manifestPath = Join-Path $InstallDir 'release-manifest.json'
if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
    throw "Overlay-Manifest fehlt: $manifestPath"
}
$package = Get-Content -Raw -LiteralPath $manifestPath | ConvertFrom-Json
if ($package.layout -ne 'retail-overlay') {
    throw "Das Paket ist kein F.E.A.R.-VR-Retail-Overlay: $manifestPath"
}
$moduleDirectory = Join-Path $InstallDir 'game-modules'

# Nur Dateien verifizieren, die zum Archiv gehören. Lokal kopierte
# Public-Tools-Module eines öffentlichen Pakets und Nutzerdaten stehen nicht
# im Manifest und werden dadurch bei späteren Starts nicht beanstandet.
foreach ($entry in $package.files) {
    $path = Join-Path $InstallDir $entry.path
    if ((Get-FileSha256 $path) -ne $entry.sha256) {
        throw "Paketdatei fehlt oder wurde verändert: $($entry.path)"
    }
}
foreach ($entry in $package.rootFiles) {
    $path = Join-Path $RetailRoot $entry.path
    if ((Get-FileSha256 $path) -ne $entry.sha256) {
        $repairRelativePath = switch ($entry.path) {
            'dinput8.dll' { 'dinput8.dll' }
            'd3d9.dll' { 'fearvr-d3d9.dll' }
            default { $null }
        }
        $repairSource = if ($repairRelativePath) {
            Join-Path $moduleDirectory $repairRelativePath
        } else {
            $null
        }
        if ($repairSource -and
            (Get-FileSha256 $repairSource) -eq $entry.sha256) {
            Copy-Item -LiteralPath $repairSource -Destination $path -Force
        }
        if ((Get-FileSha256 $path) -ne $entry.sha256) {
            throw (
                'Frühes F.E.A.R.-VR-Laufzeitmodul fehlt oder wurde ' +
                "verändert und konnte nicht repariert werden: $($entry.path)")
        }
    }
}

$retail = Assert-RetailFearExe $RetailRoot
$retailArchCfg = Join-Path $RetailRoot 'Default.archcfg'
if (-not (Test-Path -LiteralPath $retailArchCfg -PathType Leaf)) {
    throw "Retail-Archivkonfiguration fehlt: $retailArchCfg"
}

# Veränderliche Spieldaten dürfen nicht unter dem entpackten Overlay liegen.
# Bei einer Steam-Installation unter Program Files kann FEAR.exe dort zwar
# lesen, seine settings.cfg aber nicht zuverlässig ersetzen. Dann wirkt ein
# Auflösungswechsel für die laufende Sitzung und fällt beim nächsten Laden
# wieder auf 640x480 zurück. Ein benutzereigener LocalAppData-Pfad ist sowohl
# für Steam/GOG/DVD als auch ohne Administratorrechte beschreibbar.
$localAppData = [Environment]::GetFolderPath('LocalApplicationData')
if ([string]::IsNullOrWhiteSpace($localAppData)) {
    $localAppData = Join-Path $env:USERPROFILE 'AppData\Local'
}
$userDirectory = Join-Path $localAppData 'F.E.A.R. VR\userdata'
$legacyUserDirectory = Join-Path $InstallDir 'userdata'
$logDirectory = Join-Path $InstallDir 'logs'
foreach ($directory in @(
    $InstallDir, $moduleDirectory, $userDirectory, $logDirectory
)) {
    New-Item -ItemType Directory -Force -Path $directory | Out-Null
}

# Übernimmt vorhandene Saves, Profile und Einstellungen aus älteren
# Overlay-Ständen. Bereits im neuen Benutzerpfad vorhandene Dateien gewinnen;
# dadurch ist die Migration bei jedem Start sicher und idempotent.
$migratedUserFiles = 0
if ((Test-Path -LiteralPath $legacyUserDirectory -PathType Container) -and
    [IO.Path]::GetFullPath($legacyUserDirectory).TrimEnd('\') -ne
        [IO.Path]::GetFullPath($userDirectory).TrimEnd('\')) {
    $legacyRoot = [IO.Path]::GetFullPath(
        $legacyUserDirectory).TrimEnd('\')
    foreach ($item in Get-ChildItem -LiteralPath $legacyRoot `
        -Recurse -Force -ErrorAction SilentlyContinue) {
        $relativePath =
            $item.FullName.Substring($legacyRoot.Length).TrimStart('\')
        $destination = Join-Path $userDirectory $relativePath
        if ($item.PSIsContainer) {
            New-Item -ItemType Directory -Force -Path $destination |
                Out-Null
        } elseif (-not (Test-Path -LiteralPath $destination -PathType Leaf)) {
            New-Item -ItemType Directory -Force -Path (
                Split-Path -Parent $destination) | Out-Null
            Copy-Item -LiteralPath $item.FullName `
                -Destination $destination
            ++$migratedUserFiles
        }
    }
}

# Früher und mit einer klaren Meldung abbrechen, statt F.E.A.R.s stillen
# settings.cfg-Fehler erst nach dem Verlassen des Optionsmenüs zu bemerken.
$writeProbe = Join-Path $userDirectory (
    '.fearvr-write-test-' + [Guid]::NewGuid().ToString('N') + '.tmp')
try {
    [IO.File]::WriteAllText(
        $writeProbe, 'write-test', [Text.Encoding]::ASCII)
} catch {
    throw (
        "F.E.A.R.-Benutzerdatenpfad ist nicht beschreibbar: " +
        "$userDirectory`n$($_.Exception.Message)")
} finally {
    if (Test-Path -LiteralPath $writeProbe -PathType Leaf) {
        Remove-Item -LiteralPath $writeProbe -Force
    }
}

foreach ($ownModule in @('GameClient.dll', 'fearvr-d3d9.dll')) {
    $path = Join-Path $moduleDirectory $ownModule
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "F.E.A.R.-VR-Modul fehlt im Overlay: $path"
    }
}
$dinputProxy = Join-Path $RetailRoot 'dinput8.dll'
if (-not (Test-Path -LiteralPath $dinputProxy -PathType Leaf)) {
    throw "F.E.A.R.-VR-HID-Fix fehlt neben FEAR.exe: $dinputProxy"
}
$d3d9Proxy = Join-Path $RetailRoot 'd3d9.dll'
if (-not (Test-Path -LiteralPath $d3d9Proxy -PathType Leaf)) {
    throw "F.E.A.R.-VR-D3D9-Bridge fehlt neben FEAR.exe: $d3d9Proxy"
}

$bodyDirectory = Join-Path $moduleDirectory 'fearvr'
$bodyTexture = Join-Path $bodyDirectory 'player_body_d.dds'
$bodyMaterial = Join-Path $bodyDirectory 'player_body.Mat00'
$missingPublicTools = @(
    $cfg.PublicToolsModules.Keys |
        Where-Object {
            -not (Test-Path -LiteralPath (
                Join-Path $moduleDirectory $_) -PathType Leaf)
        }
)
$needsBodyAssets =
    -not (Test-Path -LiteralPath $bodyTexture -PathType Leaf) -or
    -not (Test-Path -LiteralPath $bodyMaterial -PathType Leaf)
$needsPublicToolsSource =
    $Force -or $missingPublicTools.Count -gt 0 -or $needsBodyAssets

$resolvedPublicTools = $null
if ($needsPublicToolsSource) {
    $officialInstaller =
        Join-Path $RetailRoot 'extras\fear_publictools_108.exe'
    if ([string]::IsNullOrWhiteSpace($PublicToolsGame)) {
        $resolvedPublicTools = Find-PublicToolsGame
    } else {
        $resolvedPublicTools = Resolve-PublicToolsGame $PublicToolsGame
    }
    if (-not (Test-PublicToolsGame $resolvedPublicTools)) {
        $canPrompt = [Environment]::UserInteractive
        try {
            $canPrompt = $canPrompt -and
                -not [Console]::IsInputRedirected
        } catch { }
        if ($canPrompt) {
            $resolvedPublicTools =
                Request-PublicToolsGame $officialInstaller
        }
    }
    if (-not (Test-PublicToolsGame $resolvedPublicTools)) {
        $installerHint = if (Test-Path -LiteralPath $officialInstaller -PathType Leaf) {
            "`nOffizieller Installer dieser Spielkopie: $officialInstaller"
        } else {
            ''
        }
        throw @"
F.E.A.R. Public Tools 1.08 wurden nicht gefunden.

Das öffentliche Overlay darf die fünf proprietären Module laut ihrer EULA
nicht weiterverteilen. Installiere die mit deiner Spielkopie gelieferten
Public Tools einmal. Beim Start wird nach erfolgloser automatischer Suche
ausdrücklich nach ihrem Pfad gefragt. Alternativ direkt angeben mit:

  Start F.E.A.R. VR.cmd -PublicToolsGame "D:\Pfad\zu\Dev\Runtime\Game"
$installerHint

Ein mit tools\make-release.ps1 -PrivateBundle lokal erzeugtes Archiv enthält
diese Dateien bereits, darf aber nicht veröffentlicht werden.
"@
    }
}

if ($resolvedPublicTools) {
    foreach ($target in $cfg.PublicToolsModules.Keys) {
        $destination = Join-Path $moduleDirectory $target
        if (-not $Force -and
            (Test-Path -LiteralPath $destination -PathType Leaf)) {
            continue
        }
        $source = Join-Path $resolvedPublicTools $cfg.PublicToolsModules[$target]
        if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
            throw "Public-Tools-Modul fehlt: $source"
        }
        Copy-Item -LiteralPath $source -Destination $destination -Force
    }

    if ($Force -or $needsBodyAssets) {
        & (Join-Path $PSScriptRoot 'new-body-assets.ps1') `
            -SourceGame $resolvedPublicTools `
            -DestinationGame $moduleDirectory
    }
}

foreach ($target in $cfg.PublicToolsModules.Keys) {
    $path = Join-Path $moduleDirectory $target
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Benötigtes Laufzeitmodul fehlt: $path"
    }
}
if ((Get-FileSha256 (Join-Path $moduleDirectory 'GameOrig.dll')) -ne
    $cfg.PublicToolsGameClientSha256) {
    throw 'GameOrig.dll stammt nicht aus den F.E.A.R. Public Tools 1.08.'
}

$archiveConfig = Join-Path $InstallDir 'fearvr.archcfg'
$archiveLines = @(
    Get-Content -LiteralPath $retailArchCfg -Encoding Default |
        Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
)
$archiveLines += $moduleDirectory
[IO.File]::WriteAllLines(
    $archiveConfig, $archiveLines, [Text.Encoding]::ASCII)

$records = foreach ($name in @(
    'GameOrig.dll',
    'GameServer.dll',
    'ClientFx.fxd',
    'FEAR.dep',
    'FEARMod.Arch00s',
    'GameClient.dll',
    'fearvr-d3d9.dll'
)) {
    $path = Join-Path $moduleDirectory $name
    [ordered]@{
        name = $name
        origin = if ($name -in @(
            'GameClient.dll', 'fearvr-d3d9.dll')) {
            'fearvr'
        } else {
            'public-tools'
        }
        sha256 = Get-FileSha256 $path
        bytes = (Get-Item -LiteralPath $path).Length
    }
}

$deployment = Join-Path $InstallDir 'deployment.json'
[ordered]@{
    installedUtc = (Get-Date).ToUniversalTime().ToString('s') + 'Z'
    packageVersion = $package.version
    packageGitCommit = $package.gitCommit
    packageRoot = $InstallDir
    retailRoot = $RetailRoot
    runtimeExe = $retail.Path
    runtimeSha256 = $retail.Sha256
    runtimeVerified = $retail.Verified
    runtimeEdition = $retail.Edition
    launchMode = Get-RetailLaunchMode $RetailRoot
    steamAppId = $cfg.SteamAppId
    publicToolsGame = $resolvedPublicTools
    moduleDirectory = $moduleDirectory
    archiveConfig = $archiveConfig
    archiveConfigSha256 = Get-FileSha256 $archiveConfig
    userDirectory = $userDirectory
    legacyUserDirectory = $legacyUserDirectory
    migratedUserFiles = $migratedUserFiles
    logDirectory = $logDirectory
    dinputProxy = $dinputProxy
    dinputProxySha256 = Get-FileSha256 $dinputProxy
    d3d9Proxy = $d3d9Proxy
    d3d9ProxySha256 = Get-FileSha256 $d3d9Proxy
    files = @($records)
} | ConvertTo-Json -Depth 5 | Out-File -Encoding utf8 -LiteralPath $deployment

Write-Host "Overlay bereit: $InstallDir" -ForegroundColor Green
Write-Host "Retail:         $RetailRoot"
Write-Host "Module:         $moduleDirectory"
Write-Host "Benutzerdaten:  $userDirectory"
if ($migratedUserFiles -gt 0) {
    Write-Host (
        "Migriert:        $migratedUserFiles vorhandene Datei(en), " +
        'ohne Überschreiben.')
}

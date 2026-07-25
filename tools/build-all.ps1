<#
.SYNOPSIS
    Lokaler Ein-Schritt-Builder für F.E.A.R. VR (ANWEISUNG.md §13, M6).

.DESCRIPTION
    Prüft die gepinnten Abhängigkeiten, konfiguriert und baut x86 und x64,
    führt beide CTest-Suiten aus und schreibt ein Build-Manifest mit den
    SHA-256-Summen aller erzeugten Artefakte, dem Git-Stand und den
    verwendeten Werkzeugversionen.

    Das Skript liest die Retail-Installation nur zur Verifikation und
    schreibt ausschließlich unterhalb der Projektwurzel. Es enthält und
    verteilt keinerlei Retail- oder SDK-Inhalte.

.PARAMETER Configuration
    Buildkonfiguration, standardmäßig RelWithDebInfo.

.PARAMETER Clean
    Verwirft build\x86 und build\x64 vorher vollständig und konfiguriert neu.

.PARAMETER SkipTests
    Baut nur, ohne CTest auszuführen. Für einen Abnahmebuild nicht verwenden.
#>
[CmdletBinding()]
param(
    [ValidateSet('RelWithDebInfo', 'Debug', 'Release')]
    [string]$Configuration = 'RelWithDebInfo',

    [switch]$Clean,

    [switch]$SkipTests
)

$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\_fearvr-env.ps1"
$cfg = Get-FearVrConfig

function Get-ToolPath {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string[]]$Fallbacks
    )

    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }
    foreach ($candidate in $Fallbacks) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) { return $candidate }
    }
    throw "$Name wurde weder auf PATH noch an den bekannten Orten gefunden."
}

$cmake = Get-ToolPath -Name 'cmake' -Fallbacks @(
    'C:\Program Files\CMake\bin\cmake.exe'
)
$ctest = Get-ToolPath -Name 'ctest' -Fallbacks @(
    'C:\Program Files\CMake\bin\ctest.exe'
)

# Die Toolchain ist festgelegt: v141-kompatible x86-Module gegen Retail 1.08.
# Ohne -G würde CMake das neueste installierte Visual Studio wählen.
$generator = 'Visual Studio 17 2022'

# --- Architekturen (§4: getrennte Bitness, getrennte Buildbäume) -------------
$targets = @(
    [ordered]@{
        Name     = 'x86'
        Platform = 'Win32'
        Options  = @('-DFEARVR_BUILD_PROXY=ON', '-DFEARVR_BUILD_HOST=OFF')
    },
    [ordered]@{
        Name     = 'x64'
        Platform = 'x64'
        Options  = @('-DFEARVR_BUILD_PROXY=OFF', '-DFEARVR_BUILD_HOST=ON')
    }
)

Write-Host "=== F.E.A.R. VR — Ein-Schritt-Build ($Configuration) ===" `
    -ForegroundColor Cyan

# --- Retail nur lesen und verifizieren --------------------------------------
$retailBefore = Assert-RetailFearExe
Write-Host "Retail verifiziert: $($retailBefore.Version)"

# --- Gepinnte Abhängigkeiten ------------------------------------------------
# Kein $LASTEXITCODE-Test: Der wird von & auf ein .ps1 nicht gesetzt und
# stünde hier noch auf dem Ergebnis des letzten nativen Aufrufs. Das Skript
# läuft mit ErrorActionPreference=Stop und wirft bei jedem Fehler selbst.
& "$PSScriptRoot\prepare-dependencies.ps1"

# --- Konfigurieren und bauen ------------------------------------------------
$results = [ordered]@{}
foreach ($target in $targets) {
    $buildDir = Assert-UnderProjectRoot (
        Join-Path $cfg.ProjectRoot "build\$($target.Name)"
    )

    if ($Clean -and (Test-Path -LiteralPath $buildDir)) {
        Write-Host "--- $($target.Name): Buildbaum verwerfen ---"
        Remove-Item -LiteralPath $buildDir -Recurse -Force
    }

    # Ein vorhandener Cache mit anderem Generator lässt sich nicht
    # umkonfigurieren. Das passiert, sobald jemand cmake einmal ohne -G
    # aufgerufen hat und dabei ein neueres Visual Studio gewählt wurde.
    # Die Toolchain ist hier bewusst festgelegt, also wird der fremde
    # Buildbaum verworfen statt mit einem CMake-Fehler abzubrechen.
    $cachePath = Join-Path $buildDir 'CMakeCache.txt'
    if (Test-Path -LiteralPath $cachePath -PathType Leaf) {
        $cachedGenerator = Get-Content -LiteralPath $cachePath |
            Where-Object { $_ -like 'CMAKE_GENERATOR:INTERNAL=*' } |
            Select-Object -First 1
        if ($cachedGenerator -and
            $cachedGenerator -ne "CMAKE_GENERATOR:INTERNAL=$generator") {
            Write-Host ("--- {0}: fremder Generator im Cache ({1}); Buildbaum wird neu erzeugt ---" -f `
                $target.Name, ($cachedGenerator -replace '^.*=', '')) `
                -ForegroundColor Yellow
            Remove-Item -LiteralPath $buildDir -Recurse -Force
        }
    }

    Write-Host "--- $($target.Name): konfigurieren ---" -ForegroundColor Cyan
    & $cmake -S $cfg.ProjectRoot -B $buildDir `
        -G $generator -A $target.Platform `
        @($target.Options) '-DFEARVR_BUILD_TESTS=ON'
    if ($LASTEXITCODE -ne 0) {
        throw "CMake-Konfiguration für $($target.Name) schlug fehl."
    }

    Write-Host "--- $($target.Name): bauen ---" -ForegroundColor Cyan
    & $cmake --build $buildDir --config $Configuration
    if ($LASTEXITCODE -ne 0) {
        throw "Build für $($target.Name) schlug fehl."
    }

    $testSummary = 'übersprungen'
    if (-not $SkipTests) {
        Write-Host "--- $($target.Name): Tests ---" -ForegroundColor Cyan
        & $ctest --test-dir $buildDir -C $Configuration --output-on-failure
        if ($LASTEXITCODE -ne 0) {
            throw "CTest für $($target.Name) schlug fehl."
        }
        $testSummary = 'bestanden'
    }

    $results[$target.Name] = [ordered]@{
        platform = $target.Platform
        buildDirectory = $buildDir
        tests = $testSummary
    }
}

# --- Artefakte erfassen -----------------------------------------------------
# Nur selbst gebaute Dateien. Retail- und Public-Tools-Module gehören
# ausdrücklich NICHT dazu und werden erst beim Stagen lokal zusammengeführt.
$artifactCandidates = [ordered]@{
    'GameClient.dll (Loader, x86)' =
        "build\x86\src\gameclient_loader\$Configuration\GameClient.dll"
    'fearvr-d3d9.dll (Bridge, x86)' =
        "build\x86\src\proxy32\$Configuration\fearvr-d3d9.dll"
    'fearvr-launcher.exe (x86)' =
        "build\x86\src\launcher\$Configuration\fearvr-launcher.exe"
    'fearvr-host.exe (x64)' =
        "build\x64\src\host64\$Configuration\fearvr-host.exe"
}

$artifacts = @()
foreach ($label in $artifactCandidates.Keys) {
    $path = Assert-UnderProjectRoot (
        Join-Path $cfg.ProjectRoot $artifactCandidates[$label]
    )
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Erwartetes Artefakt fehlt nach dem Build: $path"
    }
    $item = Get-Item -LiteralPath $path
    $artifacts += [ordered]@{
        label = $label
        path = $path
        sha256 = Get-FileSha256 $path
        bytes = $item.Length
        modifiedUtc = $item.LastWriteTimeUtc.ToString('s') + 'Z'
    }
}

# --- Werkzeug- und Quellstand -----------------------------------------------
$gitCommit = (& git -C $cfg.ProjectRoot rev-parse HEAD).Trim()
$gitDirty = [bool](& git -C $cfg.ProjectRoot status --porcelain)
$cmakeVersion = (& $cmake --version | Select-Object -First 1)

$manifestPath = Assert-UnderProjectRoot (
    Join-Path $cfg.ProjectRoot 'stage\build-manifest.json'
)
New-Item -ItemType Directory -Force `
    -Path (Split-Path -Parent $manifestPath) | Out-Null

[ordered]@{
    builtUtc = (Get-Date).ToUniversalTime().ToString('s') + 'Z'
    configuration = $Configuration
    gitCommit = $gitCommit
    gitWorkingTreeDirty = $gitDirty
    cmake = $cmakeVersion
    retailExeSha256 = $retailBefore.Sha256
    retailExeVersion = $retailBefore.Version
    targets = $results
    artifacts = @($artifacts)
} | ConvertTo-Json -Depth 6 |
    Out-File -Encoding utf8 -LiteralPath $manifestPath

# --- Retail muss unverändert sein -------------------------------------------
$retailAfter = Assert-RetailFearExe
if ($retailBefore.Sha256 -ne $retailAfter.Sha256) {
    throw 'SICHERHEITSABBRUCH: Retail-FEAR.exe wurde verändert.'
}

Write-Host ''
Write-Host 'Build abgeschlossen; Retail unverändert.' -ForegroundColor Green
if ($gitDirty) {
    Write-Host 'Hinweis: Der Arbeitsbaum ist nicht sauber. Das Manifest ist damit nicht reproduzierbar an einen Commit gebunden.' `
        -ForegroundColor Yellow
}
foreach ($artifact in $artifacts) {
    Write-Host ("  {0,-30} {1}" -f $artifact.label, $artifact.sha256.Substring(0, 16))
}
Write-Host "Manifest: $manifestPath"

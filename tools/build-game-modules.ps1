<#
.SYNOPSIS
    Baut die lokalen F.E.A.R. Public-Tools-Spielmodule mit VS 2022/v141.

.DESCRIPTION
    Wendet zuerst die idempotenten, neu geschriebenen Build-Transformationen
    aus patches\apply-sdk-build-fixes.ps1 an. Danach baut MSBuild die bereits
    auf vcxproj migrierte Game.sln als Release|Win32. Proprietäre Quellen und
    erzeugte Module bleiben vollständig unter vendor-local.

.PARAMETER PublicToolsRoot
    Installationswurzel der Public Tools mit den Unterordnern Source und Dev.

.PARAMETER Configuration
    Zu bauende Konfiguration. Standard: Release.

.PARAMETER VerifyFixesOnly
    Prüft nur die Quelltransformationen und startet keinen Build.
#>
[CmdletBinding()]
param(
    [string]$PublicToolsRoot,
    [ValidateSet('Release', 'Debug')][string]$Configuration = 'Release',
    [switch]$VerifyFixesOnly
)

$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($PublicToolsRoot)) {
    $PublicToolsRoot = Join-Path $PSScriptRoot '..\vendor-local\publictools'
}
. "$PSScriptRoot\_fearvr-env.ps1"
$cfg = Get-FearVrConfig

$publicToolsRoot = [IO.Path]::GetFullPath($PublicToolsRoot)
$sourceRoot = Join-Path $publicToolsRoot 'Source'
$solution = Join-Path $sourceRoot 'Game\Game.sln'
$fixScript = Join-Path $cfg.ProjectRoot 'patches\apply-sdk-build-fixes.ps1'
$buildProps = Join-Path $cfg.ProjectRoot 'patches\gameclient-build.props'

foreach ($required in @($solution, $fixScript, $buildProps)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "Erforderliche Datei fehlt: $required"
    }
}

$solutionText = [IO.File]::ReadAllText($solution)
if ($solutionText.Contains('.vcproj"')) {
    throw @"
Game.sln verweist noch auf VS-2003-vcproj-Dateien. Migriere sie einmalig:
  devenv.com "$solution" /Upgrade
Danach dieses Skript erneut starten.
"@
}
if (-not $solutionText.Contains('.vcxproj"')) {
    throw "Game.sln enthält keine erwarteten vcxproj-Verweise: $solution"
}

Write-Host '=== Public-Tools-Quellfixes ===' -ForegroundColor Cyan
if ($VerifyFixesOnly) {
    & $fixScript -SdkSource $sourceRoot -VerifyOnly
    exit 0
}

& $fixScript -SdkSource $sourceRoot

$msbuild = 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe'
if (-not (Test-Path -LiteralPath $msbuild -PathType Leaf)) {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path -LiteralPath $vswhere -PathType Leaf)) {
        throw 'MSBuild und vswhere wurden nicht gefunden. Visual Studio 2022 mit C++ installieren.'
    }

    $vsPath = & $vswhere -latest -version '[17.0,18.0)' -products * `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath
    if (-not $vsPath) {
        throw 'Keine Visual-Studio-2022-Installation mit C++-Tools gefunden.'
    }
    $msbuild = Join-Path $vsPath 'MSBuild\Current\Bin\MSBuild.exe'
}

$v141 = Get-ChildItem 'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC' `
    -Directory -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -like '14.16.*' } |
    Select-Object -First 1
if (-not $v141) {
    Write-Warning 'Das v141-Toolset wurde am Standardort nicht gefunden; MSBuild wird die genaue Diagnose liefern.'
}

Write-Host '=== Game.sln Release|Win32 bauen ===' -ForegroundColor Cyan
Write-Host "MSBuild:  $msbuild"
Write-Host "Solution: $solution"
Write-Host "Props:    $buildProps"

$arguments = @(
    $solution
    "/p:Configuration=$Configuration"
    '/p:Platform=Win32'
    '/p:PlatformToolset=v141'
    '/p:WindowsTargetPlatformVersion=10.0.26100.0'
    "/p:ForceImportAfterCppTargets=$buildProps"
    '/m'
    '/nologo'
    '/verbosity:minimal'
)
& $msbuild @arguments
if ($LASTEXITCODE -ne 0) {
    throw "Game.sln-Build fehlgeschlagen (Exit $LASTEXITCODE)."
}

$builtRoot = Join-Path $sourceRoot "built\$Configuration"
$artifacts = @(
    [pscustomobject]@{ Name = 'GameClient.dll'; Path = (Join-Path $builtRoot 'GameClient.dll') }
    [pscustomobject]@{ Name = 'GameServer.dll'; Path = (Join-Path $builtRoot 'GameServer.dll') }
    [pscustomobject]@{ Name = 'ClientFx.fxd'; Path = (Join-Path $builtRoot 'ClientFx.fxd') }
)

Write-Host '=== Artefakte verifizieren ===' -ForegroundColor Cyan
$abiCompatible = $true
foreach ($artifact in $artifacts) {
    if (-not (Test-Path -LiteralPath $artifact.Path -PathType Leaf)) {
        throw "Erwartetes Buildartefakt fehlt: $($artifact.Path)"
    }

    $bytes = [IO.File]::ReadAllBytes($artifact.Path)
    if ($bytes.Length -lt 512) {
        throw "Buildartefakt ist unerwartet klein: $($artifact.Path)"
    }
    $peOffset = [BitConverter]::ToInt32($bytes, 0x3c)
    $machine = [BitConverter]::ToUInt16($bytes, $peOffset + 4)
    if ($machine -ne 0x014c) {
        throw ("Buildartefakt ist nicht PE32/x86: {0}, Machine=0x{1:X4}" -f $artifact.Path, $machine)
    }

    $item = Get-Item -LiteralPath $artifact.Path
    $sha = Get-FileSha256 $artifact.Path
    Write-Host ("[ OK ] {0,-16} {1,9} Bytes  SHA-256 {2}" -f $artifact.Name, $item.Length, $sha)

    $asciiImage = [Text.Encoding]::ASCII.GetString($bytes)
    $usesVc71Runtime = $asciiImage.Contains('MSVCR71.dll') -and
        $asciiImage.Contains('MSVCP71.dll')
    if (-not $usesVc71Runtime) {
        $abiCompatible = $false
        Write-Warning "$($artifact.Name) verwendet nicht MSVCR71.dll + MSVCP71.dll und ist daher nur ein Compile-Artefakt."
    }
}

Write-Host 'Public-Tools-Spielmodule erfolgreich kompiliert.' -ForegroundColor Green
if (-not $abiCompatible) {
    Write-Warning @"
LAUFZEITSPERRE: Die VS2022/v141-Artefakte sind nicht ABI-kompatibel mit
F.E.A.R. 1.08 (VC7.1). Der Live-Test beendet sich in MSVCR71.dll.
tools\deploy-stock-game-modules.ps1 wird diese Dateien nicht bereitstellen.
Für GameClient-Änderungen ist eine echte VC7.1-Toolchain erforderlich.
"@
}

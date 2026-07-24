<#
.SYNOPSIS
    Read-only-Prüfer für die F.E.A.R.-VR-Arbeitsumgebung (ANWEISUNG.md §2, §12).

.DESCRIPTION
    Prüft Retail-Pfad, FEAR.exe-Version/-Hash, OpenXR-Runtime, Registry,
    SteamVR-Manifest, Public-Tools-Installer und vorhandene Build-Tools.
    Ändert NICHTS. Exitcode 0, wenn die Retail-FEAR.exe korrekt verifiziert ist,
    sonst 1.

.PARAMETER DeepHash
    Zusätzlich den SHA-256 des ~671 MB großen SDK-Installers prüfen (langsam).
#>
[CmdletBinding()]
param(
    [switch]$DeepHash
)

$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\_fearvr-env.ps1"
$cfg = Get-FearVrConfig

$ok = $true
function Line($label, $value, $good) {
    $mark = if ($good) { '[ OK ]' } else { '[FAIL]' }
    Write-Host ("{0} {1,-34} {2}" -f $mark, $label, $value)
    if (-not $good) { $script:ok = $false }
}
function Info($label, $value) {
    Write-Host ("{0} {1,-34} {2}" -f '[info]', $label, $value)
}

Write-Host "=== F.E.A.R. VR — Umgebungsprüfung ($(Get-Date -Format s)) ===" -ForegroundColor Cyan

# --- Projektwurzel ---
Info 'Projektwurzel' $cfg.ProjectRoot

# --- Retail-FEAR.exe (Kernkriterium) ---
$exe = Join-Path $cfg.RetailRoot $cfg.FearExeName
if (Test-Path -LiteralPath $exe) {
    $ver = (Get-Item -LiteralPath $exe).VersionInfo.FileVersion
    Line 'FEAR.exe Version' "$ver (erwartet $($cfg.ExpectedVersion))" ($ver -eq $cfg.ExpectedVersion)
    $sha = Get-FileSha256 $exe
    Line 'FEAR.exe SHA-256' $(if ($sha -eq $cfg.ExpectedSha256) { 'passt' } else { $sha }) ($sha -eq $cfg.ExpectedSha256)
} else {
    Line 'FEAR.exe' "NICHT gefunden: $exe" $false
}

# --- OpenXR-Runtime (x64 aktiv, WOW6432Node fehlt erwartungsgemäß) ---
try {
    $rt = (Get-ItemProperty 'HKLM:\SOFTWARE\Khronos\OpenXR\1' -ErrorAction Stop).ActiveRuntime
    Line 'OpenXR x64 ActiveRuntime' $rt ([bool]$rt)
} catch { Line 'OpenXR x64 ActiveRuntime' 'fehlt' $false }

$wow = $null
try { $wow = (Get-ItemProperty 'HKLM:\SOFTWARE\WOW6432Node\Khronos\OpenXR\1' -ErrorAction Stop).ActiveRuntime } catch {}
Info 'OpenXR 32-bit (WOW6432Node)' $(if ($wow) { "$wow (unerwartet vorhanden)" } else { 'fehlt (erwartet -> separater x64-Host)' })

Info 'SteamVR-Manifest' $(if (Test-Path -LiteralPath $cfg.SteamVrManifest) { 'vorhanden' } else { 'nicht gefunden' })

# --- Public-Tools-Installer ---
$sdk = Join-Path $cfg.RetailRoot $cfg.SdkInstallerRel
if (Test-Path -LiteralPath $sdk) {
    $size = (Get-Item -LiteralPath $sdk).Length
    Line 'Public-Tools-Installer Größe' "$size (erwartet $($cfg.SdkInstallerSize))" ($size -eq $cfg.SdkInstallerSize)
    if ($DeepHash) {
        Write-Host '      (berechne SDK-Installer-Hash ~671 MB ...)'
        $sh = Get-FileSha256 $sdk
        Line 'Public-Tools-Installer SHA-256' $(if ($sh -eq $cfg.SdkInstallerSha256) { 'passt' } else { $sh }) ($sh -eq $cfg.SdkInstallerSha256)
    } else {
        Info 'Public-Tools-Installer SHA-256' 'übersprungen (mit -DeepHash prüfen)'
    }
} else {
    Line 'Public-Tools-Installer' "NICHT gefunden: $sdk" $false
}

# --- Build-Tools ---
Write-Host "--- Build-Tools ---"
$g = Get-Command git -ErrorAction SilentlyContinue
Info 'git' $(if ($g) { $g.Source } else { 'NICHT gefunden' })

# cmake: PATH oder Standardinstallation
$cm = Get-Command cmake -ErrorAction SilentlyContinue
if ($cm) {
    Info 'cmake' ("$($cm.Source)  (" + ((& $cm.Source --version | Select-Object -First 1)) + ")")
} elseif (Test-Path 'C:\Program Files\CMake\bin\cmake.exe') {
    $v = (& 'C:\Program Files\CMake\bin\cmake.exe' --version | Select-Object -First 1)
    Info 'cmake' "C:\Program Files\CMake\bin\cmake.exe ($v; neue Shell nötig für PATH)"
} else { Info 'cmake' 'NICHT gefunden' }

# Visual Studio / MSVC via vswhere (cl/msbuild liegen nicht global auf PATH).
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vswhere) {
    $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>$null
    if ($vsPath) {
        $vsName = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property displayName 2>$null
        $vsVer  = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property catalog_productDisplayVersion 2>$null
        Info 'Visual Studio (C++ Tools)' "$vsName $vsVer"
        $v141 = @(Get-ChildItem (Join-Path $vsPath 'VC\Tools\MSVC') -Directory -ErrorAction SilentlyContinue | Where-Object { $_.Name -like '14.16.*' })
        Info 'MSVC v141-Toolset' $(if ($v141.Count) { $v141[0].Name } else { 'NICHT installiert' })
    } else {
        Info 'Visual Studio (C++ Tools)' 'NICHT gefunden (VC.Tools fehlt)'
    }
} else {
    Info 'Visual Studio' 'kein vswhere / nicht installiert'
}

# Windows 10/11 SDK
$sdkInc = "${env:ProgramFiles(x86)}\Windows Kits\10\Include"
if (Test-Path $sdkInc) {
    Info 'Windows SDK' (((Get-ChildItem $sdkInc -Directory | Select-Object -ExpandProperty Name) -join ', '))
} else { Info 'Windows SDK' 'NICHT gefunden' }

# --- Fazit ---
Write-Host ("=== Ergebnis: {0} ===" -f $(if ($ok) { 'Retail verifiziert' } else { 'Abweichungen gefunden' })) -ForegroundColor $(if ($ok) { 'Green' } else { 'Red' })
if (-not $ok) {
    Write-Host 'Bei falscher/fehlender FEAR.exe werden versionsabhängige Hooks NICHT aktiviert.' -ForegroundColor Yellow
    exit 1
}
exit 0

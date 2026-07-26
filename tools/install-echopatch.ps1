<#
.SYNOPSIS
    Legt EchoPatch neben die Retail-FEAR.exe oder entfernt es wieder.

.DESCRIPTION
    EchoPatch (Wemino) ist ein dinput8.dll-Wrapper und muss deshalb im
    Retail-Verzeichnis liegen — der einzige Punkt, an dem dieses Projekt in die
    Retail-Installation schreibt. Installiert werden genau zwei Dateien:

        dinput8.dll     unveraendert aus dem gepinnten Release
        EchoPatch.ini   die versionierte VR-Fassung aus patches\echopatch

    FEAR.exe wird nur gelesen und vor sowie nach dem Lauf gegen ihren SHA-256
    geprueft. Sie bleibt unveraendert, solange CheckLAAPatch = 0 in der
    EchoPatch.ini steht — der LAA-Patch waere die einzige EchoPatch-Funktion,
    die die EXE beschreibt, und wuerde jeden Start unserer Kette blockieren.

    Ohne -Apply ist der Lauf ein reiner Trockenlauf und zeigt nur den Ist-Stand
    und was geschehen wuerde.

.PARAMETER Apply
    Fuehrt die Aenderung tatsaechlich aus.

.PARAMETER Remove
    Entfernt beide Dateien wieder. Eine fremde dinput8.dll — also eine, deren
    Hash nicht zum gepinnten Release passt — wird nicht angefasst.

.PARAMETER ZipPath
    Abweichender Pfad zum EchoPatch-Release-ZIP. Standard ist der in
    _fearvr-env.ps1 gepinnte Pfad unter vendor-local\.
#>
[CmdletBinding()]
param(
    [switch]$Apply,

    [switch]$Remove,

    [string]$ZipPath
)

$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\_fearvr-env.ps1"
$cfg = Get-FearVrConfig

$retailBefore = Assert-RetailFearExe
$dllTarget = Join-Path $cfg.RetailRoot $cfg.EchoPatchDllName
$iniTarget = Join-Path $cfg.RetailRoot $cfg.EchoPatchIniName

function Get-InstallState {
    $dllHash = Get-FileSha256 $dllTarget
    return [pscustomobject]@{
        DllPresent = $null -ne $dllHash
        DllMatches = $dllHash -eq $cfg.EchoPatchDllSha256
        DllHash    = $dllHash
        IniPresent = Test-Path -LiteralPath $iniTarget -PathType Leaf
    }
}

$state = Get-InstallState
Write-Host '=== EchoPatch im Retail-Verzeichnis ===' -ForegroundColor Cyan
Write-Host "Retail:  $($cfg.RetailRoot)"
Write-Host "FEAR.exe: $($retailBefore.Version) (Hash unveraendert)"
$dllStatus = if (-not $state.DllPresent) {
    'nicht vorhanden'
} elseif ($state.DllMatches) {
    "vorhanden, Release $($cfg.EchoPatchVersion)"
} else {
    "vorhanden, FREMDE Datei (SHA-256 $($state.DllHash))"
}
Write-Host "$($cfg.EchoPatchDllName): $dllStatus"
Write-Host ("$($cfg.EchoPatchIniName): " + $(
    if ($state.IniPresent) { 'vorhanden' } else { 'nicht vorhanden' }))

if ($Remove) {
    if (-not $Apply) {
        Write-Host ''
        Write-Host 'Trockenlauf. Mit -Apply -Remove wuerden entfernt:' `
            -ForegroundColor Yellow
        if ($state.DllMatches) { Write-Host "  $dllTarget" }
        if ($state.IniPresent) { Write-Host "  $iniTarget" }
        if (-not $state.DllMatches -and $state.DllPresent) {
            Write-Host (
                "  (uebersprungen: $dllTarget gehoert nicht zum gepinnten " +
                'Release)') -ForegroundColor Yellow
        }
        return
    }
    if ($state.DllPresent -and -not $state.DllMatches) {
        Write-Warning (
            "$($cfg.EchoPatchDllName) stammt nicht aus dem gepinnten " +
            'Release und bleibt unangetastet.')
    } elseif ($state.DllMatches) {
        Remove-Item -LiteralPath $dllTarget -Force
        Write-Host "Entfernt: $dllTarget"
    }
    if ($state.IniPresent) {
        Remove-Item -LiteralPath $iniTarget -Force
        Write-Host "Entfernt: $iniTarget"
    }
    $retailAfter = Assert-RetailFearExe
    if ($retailBefore.Sha256 -ne $retailAfter.Sha256) {
        throw 'SICHERHEITSABBRUCH: Retail-FEAR.exe wurde veraendert.'
    }
    Write-Host 'Retail ist wieder im Auslieferungszustand.' `
        -ForegroundColor Green
    return
}

# --- Installieren ------------------------------------------------------------
$zip = if ([string]::IsNullOrWhiteSpace($ZipPath)) {
    Join-Path $cfg.ProjectRoot $cfg.EchoPatchZipRel
} else {
    [IO.Path]::GetFullPath($ZipPath)
}
if (-not (Test-Path -LiteralPath $zip -PathType Leaf)) {
    throw @"
EchoPatch-Release nicht gefunden: $zip
Herunterladen von $($cfg.EchoPatchReleaseUrl) und dorthin legen.
"@
}
$zipItem = Get-Item -LiteralPath $zip
if ($zipItem.Length -ne $cfg.EchoPatchZipSize) {
    throw (
        "EchoPatch-ZIP hat $($zipItem.Length) Byte, erwartet " +
        "$($cfg.EchoPatchZipSize).")
}
$zipHash = Get-FileSha256 $zip
if ($zipHash -ne $cfg.EchoPatchZipSha256) {
    throw "EchoPatch-ZIP hat den falschen SHA-256: $zipHash"
}
$iniSource = Assert-UnderProjectRoot (
    Join-Path $cfg.ProjectRoot $cfg.EchoPatchIniRel)
if (-not (Test-Path -LiteralPath $iniSource -PathType Leaf)) {
    throw "Versionierte EchoPatch.ini fehlt: $iniSource"
}

# Die VR-kritischen Werte werden hier geprueft und nicht nur dokumentiert:
# eine versehentlich zurueckgesetzte Vorlage faellt sonst erst im Headset auf.
$iniText = Get-Content -Raw -LiteralPath $iniSource
$requiredSettings = [ordered]@{
    'CheckLAAPatch' = '0'
    'SDLGamepadSupport' = '0'
    'EnableCrashHandler' = '0'
    'DynamicVsync' = '0'
    'HUDScaling' = '0'
    'AutoResolution' = '0'
    'CustomFOV' = '0'
    'SSAAScale' = '1.0'
}
foreach ($name in $requiredSettings.Keys) {
    $expected = $requiredSettings[$name]
    if ($iniText -notmatch "(?m)^\s*$name\s*=\s*$([regex]::Escape($expected))\s*$") {
        throw (
            "EchoPatch.ini: '$name' muss fuer den VR-Betrieb auf " +
            "$expected stehen. Siehe docs\ECHOPATCH.md.")
    }
}

if (-not $Apply) {
    Write-Host ''
    Write-Host 'Trockenlauf. Mit -Apply wuerden geschrieben:' `
        -ForegroundColor Yellow
    Write-Host "  $dllTarget   (aus $zip)"
    Write-Host "  $iniTarget   (aus $iniSource)"
    Write-Host 'FEAR.exe wird dabei nicht angefasst.'
    return
}

Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive = [IO.Compression.ZipFile]::OpenRead($zip)
try {
    # Bewusst nur der Wurzeleintrag: FEARXP\ und FEARXP2\ enthalten dieselbe
    # DLL fuer die Erweiterungen, die dieses Projekt nicht startet.
    $entry = $archive.Entries |
        Where-Object { $_.FullName -eq $cfg.EchoPatchDllName } |
        Select-Object -First 1
    if ($null -eq $entry) {
        throw "Im ZIP fehlt der Eintrag $($cfg.EchoPatchDllName)."
    }
    [IO.Compression.ZipFileExtensions]::ExtractToFile(
        $entry, $dllTarget, $true)
} finally {
    $archive.Dispose()
}
$installedHash = Get-FileSha256 $dllTarget
if ($installedHash -ne $cfg.EchoPatchDllSha256) {
    Remove-Item -LiteralPath $dllTarget -Force
    throw "Entpackte $($cfg.EchoPatchDllName) hat den falschen SHA-256."
}
Copy-Item -LiteralPath $iniSource -Destination $iniTarget -Force

$retailAfter = Assert-RetailFearExe
if ($retailBefore.Sha256 -ne $retailAfter.Sha256) {
    throw 'SICHERHEITSABBRUCH: Retail-FEAR.exe wurde veraendert.'
}
Write-Host ''
Write-Host "EchoPatch $($cfg.EchoPatchVersion) ist installiert." `
    -ForegroundColor Green
Write-Host "  $dllTarget"
Write-Host "  $iniTarget"
Write-Host 'FEAR.exe unveraendert. Entfernen: -Apply -Remove.'

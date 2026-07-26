# =============================================================================
# Gemeinsame Konstanten & Helfer (ANWEISUNG.md §2). Per Dot-Sourcing genutzt:
#   . "$PSScriptRoot\_fearvr-env.ps1"
# Enthält KEINE ausführbare Logik außer Definitionen.
# =============================================================================

# --- Verifizierter Ausgangszustand (§2, geprüft 2026-07-24) ---
$script:FearVr = [ordered]@{
    ProjectRoot   = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
    RetailRoot    = 'C:\Program Files (x86)\Steam\steamapps\common\FEAR Ultimate Shooter Edition'
    FearExeName   = 'FEAR.exe'
    ExpectedVersion = '1.08.282.0'
    ExpectedSha256  = 'D5EBC38A4F12B772C9112A2811C290ADB6C5052D3BC2F817302D38CF55BB2CBE'
    SdkInstallerRel = 'extras\fear_publictools_108.exe'
    SdkInstallerSize = 671441087
    SdkInstallerSha256 = '11AAA4128528403F7BC9EA5119C68051C62B92A99E6411DFD749AF55E9B19DF8'
    # --- EchoPatch (Wemino), als dinput8.dll neben FEAR.exe ------------------
    # Die einzige Fremdsoftware, die bewusst IN der Retail-Installation liegt.
    # Sie besteht aus genau zwei Dateien und wird von tools\install-echopatch.ps1
    # gesetzt und wieder entfernt. FEAR.exe selbst bleibt unangetastet, solange
    # CheckLAAPatch = 0 in der EchoPatch.ini steht.
    EchoPatchVersion = '4.2.1'
    EchoPatchZipRel = 'vendor-local\echopatch\EchoPatch-4.2.1.zip'
    EchoPatchZipSize = 1978793
    EchoPatchZipSha256 = '5AE9BF8F4D549B0F1CD682D63B4123C2BFF2622BD2035779DF263183C61BF9AE'
    EchoPatchDllName = 'dinput8.dll'
    EchoPatchDllSha256 = '3B01CD16228C1A85585037B87D0C6C41E99F18AE0823821518242382B854BEFA'
    EchoPatchIniName = 'EchoPatch.ini'
    EchoPatchIniRel = 'patches\echopatch\EchoPatch.ini'
    EchoPatchReleaseUrl = 'https://github.com/Wemino/EchoPatch/releases/download/4.2.1/EchoPatch.zip'
    SteamVrManifest = 'C:\Program Files (x86)\Steam\steamapps\common\SteamVR\steamxr_win64.json'
    VdxrManifest    = 'C:\Program Files\Virtual Desktop Streamer\OpenXR\virtualdesktop-openxr.json'
    UserDataRel     = 'stage\userdata'
}

function Get-FearVrConfig { return $script:FearVr }

function Get-FileSha256([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path)) { return $null }

    # Get-FileHash bindet Dateipfade über den PowerShell-Provider. Unter einem
    # geerbten -WhatIf kann PowerShell 5.1 dabei $null statt eines Hashobjekts
    # liefern. Direkter, read-only Dateizugriff vermeidet diese Nebenwirkung.
    $stream = [IO.File]::OpenRead([IO.Path]::GetFullPath($Path))
    $sha256 = [Security.Cryptography.SHA256]::Create()
    try {
        return [BitConverter]::ToString($sha256.ComputeHash($stream)).Replace('-', '')
    } finally {
        $sha256.Dispose()
        $stream.Dispose()
    }
}

# Prüft, dass ein Zielpfad NUR unterhalb der Projektwurzel liegt (§12).
function Assert-UnderProjectRoot([string]$Path) {
    $root = (Get-FearVrConfig).ProjectRoot
    $full = [System.IO.Path]::GetFullPath($Path)
    $rootFull = [System.IO.Path]::GetFullPath($root)
    if (-not $full.StartsWith($rootFull, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "SICHERHEITSABBRUCH: '$full' liegt nicht unter der Projektwurzel '$rootFull'."
    }
    return $full
}

# --- OpenXR-Runtime-Auswahl --------------------------------------------------
# Der Mod ist an keine bestimmte Runtime gebunden: Der x64-Host spricht nur
# OpenXR. SteamVR und VirtualDesktopXR (VDXR) funktionieren beide.
#
# Umgeschaltet wird NICHT über HKLM\...\Khronos\OpenXR\1\ActiveRuntime — das
# ist eine systemweite Einstellung. Stattdessen wird XR_RUNTIME_JSON nur für
# den gestarteten Hostprozess gesetzt. Das wirkt ausschließlich auf diesen
# Prozess und lässt die Systemeinstellung unangetastet.

# Liest den Namen aus einem OpenXR-Runtime-Manifest.
function Get-OpenXrRuntimeName([string]$ManifestPath) {
    if (-not (Test-Path -LiteralPath $ManifestPath -PathType Leaf)) {
        return $null
    }
    try {
        return ([IO.File]::ReadAllText($ManifestPath) |
            ConvertFrom-Json).runtime.name
    } catch {
        return $null
    }
}

# steamvr | vdxr | other
function Get-OpenXrRuntimeKind([string]$ManifestPath) {
    $name = Get-OpenXrRuntimeName $ManifestPath
    if ($null -eq $name) { return 'other' }
    if ($name -match 'SteamVR') { return 'steamvr' }
    if ($name -match 'VirtualDesktop') { return 'vdxr' }
    return 'other'
}

# Die systemweit aktive Runtime, rein lesend.
function Get-ActiveOpenXrRuntime {
    $path = $null
    try {
        $path = (Get-ItemProperty 'HKLM:\SOFTWARE\Khronos\OpenXR\1' `
            -ErrorAction Stop).ActiveRuntime
    } catch {
        return $null
    }
    if ([string]::IsNullOrWhiteSpace($path)) { return $null }
    return [pscustomobject]@{
        Path = $path
        Name = Get-OpenXrRuntimeName $path
        Kind = Get-OpenXrRuntimeKind $path
    }
}

# Löst die -Runtime-Auswahl eines Launchers auf.
#   active            -> keine Überschreibung, Systemeinstellung gilt
#   steamvr | vdxr    -> bekanntes Manifest
#   <Pfad zur .json>  -> beliebiges Manifest
# Rückgabe: Objekt mit Path (oder $null bei 'active'), Name, Kind und
# Override (bool).
function Resolve-OpenXrRuntime([string]$Runtime) {
    $cfg = Get-FearVrConfig
    $active = Get-ActiveOpenXrRuntime

    if ([string]::IsNullOrWhiteSpace($Runtime) -or $Runtime -eq 'active') {
        if ($null -eq $active) {
            throw @'
Keine aktive OpenXR-Runtime gefunden.
SteamVR oder den Virtual Desktop Streamer starten und dort als OpenXR-Runtime
setzen, oder den Launcher mit -Runtime steamvr bzw. -Runtime vdxr aufrufen.
'@
        }
        return [pscustomobject]@{
            Path = $null; Name = $active.Name
            Kind = $active.Kind; Override = $false
        }
    }

    $manifest = switch ($Runtime) {
        'steamvr' { $cfg.SteamVrManifest }
        'vdxr'    { $cfg.VdxrManifest }
        default   { $Runtime }
    }
    if (-not (Test-Path -LiteralPath $manifest -PathType Leaf)) {
        throw "OpenXR-Runtime-Manifest nicht gefunden: $manifest"
    }
    return [pscustomobject]@{
        Path = [IO.Path]::GetFullPath($manifest)
        Name = Get-OpenXrRuntimeName $manifest
        Kind = Get-OpenXrRuntimeKind $manifest
        Override = $true
    }
}

# Verifiziert die Retail-FEAR.exe gegen Version + SHA-256. Wirft bei Abweichung.
function Assert-RetailFearExe {
    $cfg = Get-FearVrConfig
    $exe = Join-Path $cfg.RetailRoot $cfg.FearExeName
    if (-not (Test-Path -LiteralPath $exe)) {
        throw "FEAR.exe nicht gefunden: $exe"
    }
    # Get-Item unterstützt ShouldProcess. Ein -WhatIf eines aufrufenden Skripts
    # würde deshalb bis hierher vererbt und statt eines FileInfo-Objekts $null
    # liefern. Die .NET-Abfrage ist read-only und funktioniert unabhängig davon.
    $ver = [System.Diagnostics.FileVersionInfo]::GetVersionInfo($exe).FileVersion
    $sha = Get-FileSha256 $exe
    if ($ver -ne $cfg.ExpectedVersion) {
        throw "Falsche FEAR.exe-Version: '$ver' (erwartet '$($cfg.ExpectedVersion)'). Versionsabhängige Hooks bleiben DEAKTIVIERT."
    }
    if ($sha -ne $cfg.ExpectedSha256) {
        throw "Falscher FEAR.exe-Hash: '$sha' (erwartet '$($cfg.ExpectedSha256)'). Versionsabhängige Hooks bleiben DEAKTIVIERT."
    }
    return [pscustomobject]@{ Path = $exe; Version = $ver; Sha256 = $sha }
}

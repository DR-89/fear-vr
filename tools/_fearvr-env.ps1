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
    SteamVrManifest = 'C:\Program Files (x86)\Steam\steamapps\common\SteamVR\steamxr_win64.json'
    UserDataRel     = 'stage\userdata'
}

function Get-FearVrConfig { return $script:FearVr }

function Get-FileSha256([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path)) { return $null }
    return (Get-FileHash -Algorithm SHA256 -LiteralPath $Path).Hash.ToUpperInvariant()
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

# Verifiziert die Retail-FEAR.exe gegen Version + SHA-256. Wirft bei Abweichung.
function Assert-RetailFearExe {
    $cfg = Get-FearVrConfig
    $exe = Join-Path $cfg.RetailRoot $cfg.FearExeName
    if (-not (Test-Path -LiteralPath $exe)) {
        throw "FEAR.exe nicht gefunden: $exe"
    }
    $ver = (Get-Item -LiteralPath $exe).VersionInfo.FileVersion
    $sha = Get-FileSha256 $exe
    if ($ver -ne $cfg.ExpectedVersion) {
        throw "Falsche FEAR.exe-Version: '$ver' (erwartet '$($cfg.ExpectedVersion)'). Versionsabhängige Hooks bleiben DEAKTIVIERT."
    }
    if ($sha -ne $cfg.ExpectedSha256) {
        throw "Falscher FEAR.exe-Hash: '$sha' (erwartet '$($cfg.ExpectedSha256)'). Versionsabhängige Hooks bleiben DEAKTIVIERT."
    }
    return [pscustomobject]@{ Path = $exe; Version = $ver; Sha256 = $sha }
}

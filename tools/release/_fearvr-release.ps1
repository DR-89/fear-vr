# =============================================================================
# Gemeinsame Definitionen für das ausgelieferte F.E.A.R.-VR-Paket.
# Absichtlich unabhängig vom Entwicklungs-Repository: Das Paket bringt nur
# eigene Binaries mit und holt die Public-Tools-Module lokal beim Nutzer.
# Enthält KEINE ausführbare Logik außer Definitionen.
# =============================================================================

$script:FearVrRelease = [ordered]@{
    # Retail-Zielversion. Versionsabhängige Hooks bleiben bei Abweichung aus.
    ExpectedVersion = '1.08.282.0'
    ExpectedSha256  = 'D5EBC38A4F12B772C9112A2811C290ADB6C5052D3BC2F817302D38CF55BB2CBE'

    # Unverändertes VC7.1-GameClient.dll aus den Public Tools 1.08. Dient als
    # Erkennungsmerkmal für ein gültiges Public-Tools-Runtime-Verzeichnis.
    PublicToolsGameClientSha256 =
        'B5F1F1976227FD0E6F1C32BD2BEEDFB117E68A87A07BB42D06BE489DD08A63BA'

    SteamAppId = 21090

    # Module, die aus der lokalen Public-Tools-Installation geholt werden.
    # Schlüssel = Zielname in der Stage, Wert = Quellname im Runtime\Game.
    PublicToolsModules = [ordered]@{
        'GameOrig.dll'    = 'GameClient.dll'
        'GameServer.dll'  = 'GameServer.dll'
        'ClientFx.fxd'    = 'ClientFx.fxd'
        'FEAR.dep'        = 'FEAR.dep'
        'FEARMod.Arch00s' = 'FEARMod.Arch00s'
    }

    # Eigene Module, die das Paket mitbringt.
    BundledModules = [ordered]@{
        'GameClient.dll'  = 'bin\x86\GameClient.dll'
        'fearvr-d3d9.dll' = 'bin\x86\fearvr-d3d9.dll'
    }

    SteamVrManifest =
        'C:\Program Files (x86)\Steam\steamapps\common\SteamVR\steamxr_win64.json'
    VdxrManifest =
        'C:\Program Files\Virtual Desktop Streamer\OpenXR\virtualdesktop-openxr.json'
}

function Get-FearVrReleaseConfig { return $script:FearVrRelease }

function Get-FileSha256([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return $null }
    $stream = [IO.File]::OpenRead([IO.Path]::GetFullPath($Path))
    $sha256 = [Security.Cryptography.SHA256]::Create()
    try {
        return [BitConverter]::ToString(
            $sha256.ComputeHash($stream)).Replace('-', '')
    } finally {
        $sha256.Dispose()
        $stream.Dispose()
    }
}

# Verifiziert eine Retail-FEAR.exe. Wirft bei Abweichung, weil alle
# versionsabhängigen Hooks sonst ohnehin deaktiviert blieben.
function Assert-RetailFearExe([string]$RetailRoot) {
    $cfg = Get-FearVrReleaseConfig
    $exe = Join-Path $RetailRoot 'FEAR.exe'
    if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) {
        throw "FEAR.exe nicht gefunden: $exe"
    }
    $version = [Diagnostics.FileVersionInfo]::GetVersionInfo($exe).FileVersion
    if ($version -ne $cfg.ExpectedVersion) {
        throw ("Falsche FEAR.exe-Version: '$version' " +
               "(erwartet '$($cfg.ExpectedVersion)').")
    }
    $sha = Get-FileSha256 $exe
    if ($sha -ne $cfg.ExpectedSha256) {
        throw "Falscher FEAR.exe-Hash: '$sha'."
    }
    return [pscustomobject]@{ Path = $exe; Version = $version; Sha256 = $sha }
}

# Sucht die Retail-Installation über die Steam-Bibliotheken.
function Find-RetailRoot {
    $candidates = New-Object Collections.Generic.List[string]
    $steamRoot = $null
    foreach ($key in @(
        'HKLM:\SOFTWARE\WOW6432Node\Valve\Steam',
        'HKLM:\SOFTWARE\Valve\Steam',
        'HKCU:\SOFTWARE\Valve\Steam'
    )) {
        try {
            $value = (Get-ItemProperty $key -ErrorAction Stop)
            foreach ($name in @('InstallPath', 'SteamPath')) {
                if ($value.PSObject.Properties.Name -contains $name -and
                    $value.$name) {
                    $steamRoot = $value.$name
                    break
                }
            }
        } catch { }
        if ($steamRoot) { break }
    }
    if (-not $steamRoot) { $steamRoot = 'C:\Program Files (x86)\Steam' }

    $libraries = New-Object Collections.Generic.List[string]
    $libraries.Add($steamRoot)
    $vdf = Join-Path $steamRoot 'steamapps\libraryfolders.vdf'
    if (Test-Path -LiteralPath $vdf -PathType Leaf) {
        foreach ($match in [Text.RegularExpressions.Regex]::Matches(
            [IO.File]::ReadAllText($vdf), '"path"\s*"([^"]+)"')) {
            # Die Klammern sind noetig: In einem Methodenaufruf wuerde das
            # Komma von -replace sonst als Argumenttrenner gelesen.
            $libraries.Add(($match.Groups[1].Value -replace '\\\\', '\'))
        }
    }
    foreach ($library in $libraries) {
        $candidates.Add(
            (Join-Path $library 'steamapps\common\FEAR Ultimate Shooter Edition'))
        $candidates.Add((Join-Path $library 'steamapps\common\FEAR'))
    }

    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath (Join-Path $candidate 'FEAR.exe') -PathType Leaf) {
            return $candidate
        }
    }
    return $null
}

# Sucht ein Public-Tools-Runtime-Verzeichnis und verifiziert es über den
# Hash des unveränderten VC7.1-GameClient.dll.
function Find-PublicToolsGame {
    $cfg = Get-FearVrReleaseConfig
    $roots = @(
        'C:\Program Files (x86)\Monolith Productions\FEAR Public Tools',
        'C:\Program Files\Monolith Productions\FEAR Public Tools',
        'C:\FEAR Public Tools',
        'C:\Program Files (x86)\FEAR Public Tools'
    )
    foreach ($root in $roots) {
        $game = Join-Path $root 'Dev\Runtime\Game'
        if (Test-PublicToolsGame $game) { return $game }
    }
    return $null
}

function Test-PublicToolsGame([string]$GameDirectory) {
    $cfg = Get-FearVrReleaseConfig
    if ([string]::IsNullOrWhiteSpace($GameDirectory)) { return $false }
    $client = Join-Path $GameDirectory 'GameClient.dll'
    if (-not (Test-Path -LiteralPath $client -PathType Leaf)) { return $false }
    return (Get-FileSha256 $client) -eq $cfg.PublicToolsGameClientSha256
}

# --- OpenXR-Runtime ----------------------------------------------------------
# Umgeschaltet wird über XR_RUNTIME_JSON nur für den Hostprozess. Die
# systemweite Einstellung unter HKLM\...\Khronos\OpenXR\1\ActiveRuntime wird
# nie geschrieben.
function Get-OpenXrRuntimeName([string]$ManifestPath) {
    if (-not (Test-Path -LiteralPath $ManifestPath -PathType Leaf)) { return $null }
    try {
        return ([IO.File]::ReadAllText($ManifestPath) |
            ConvertFrom-Json).runtime.name
    } catch { return $null }
}

function Get-OpenXrRuntimeKind([string]$ManifestPath) {
    $name = Get-OpenXrRuntimeName $ManifestPath
    if ($null -eq $name) { return 'other' }
    if ($name -match 'SteamVR') { return 'steamvr' }
    if ($name -match 'VirtualDesktop') { return 'vdxr' }
    return 'other'
}

function Resolve-OpenXrRuntime([string]$Runtime) {
    $cfg = Get-FearVrReleaseConfig
    if ([string]::IsNullOrWhiteSpace($Runtime) -or $Runtime -eq 'active') {
        $path = $null
        try {
            $path = (Get-ItemProperty 'HKLM:\SOFTWARE\Khronos\OpenXR\1' `
                -ErrorAction Stop).ActiveRuntime
        } catch { }
        if ([string]::IsNullOrWhiteSpace($path)) {
            throw @'
Keine aktive OpenXR-Runtime gefunden.
SteamVR oder den Virtual Desktop Streamer starten und dort als OpenXR-Runtime
setzen, oder mit -Runtime steamvr bzw. -Runtime vdxr starten.
'@
        }
        return [pscustomobject]@{
            Path = $null; Name = Get-OpenXrRuntimeName $path
            Kind = Get-OpenXrRuntimeKind $path; Override = $false
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

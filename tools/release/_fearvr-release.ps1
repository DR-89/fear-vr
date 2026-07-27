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

    # Bekannte, tatsächlich getestete FEAR.exe-Builds. Jede andere 1.08-EXE
    # (GOG, Retail-DVD) kann laufen, ist hier aber nicht bestätigt: Alle
    # versionsabhängigen Signaturen dieses Mods liegen in GameOrig.dll aus den
    # Public Tools, nicht in FEAR.exe. Deshalb wird ein unbekannter Hash nur
    # noch gemeldet und nicht mehr als Abbruchgrund behandelt.
    KnownRetailHashes = [ordered]@{
        'D5EBC38A4F12B772C9112A2811C290ADB6C5052D3BC2F817302D38CF55BB2CBE' =
            'Steam, Ultimate Shooter Edition 1.08'
        'D662DCCDB2EBD17D1ACED7C725A8724060010718146E0C0074DA5E8EF89B82B4' =
            'Steam 1.08 + HDTextures4FEAR/XP v2.0.2'
    }

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

# Verifiziert eine Retail-FEAR.exe. Die Version 1.08 ist Bedingung — die
# Public-Tools-Module passen zu keiner anderen. Der Hash entscheidet dagegen
# nur noch darüber, ob dieser Build getestet ist: Steam, GOG und die
# Retail-DVD liefern verschiedene 1.08-EXEn, während jede Bytesignatur dieses
# Mods in GameOrig.dll steht.
function Assert-RetailFearExe([string]$RetailRoot) {
    $cfg = Get-FearVrReleaseConfig
    $exe = Join-Path $RetailRoot 'FEAR.exe'
    if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) {
        throw "FEAR.exe not found: $exe"
    }
    $version = [Diagnostics.FileVersionInfo]::GetVersionInfo($exe).FileVersion
    if ($version -notlike '1.08*') {
        throw ("Wrong FEAR.exe version: '$version' (1.08 required). " +
               "Path: $exe")
    }
    $sha = Get-FileSha256 $exe
    $known = $cfg.KnownRetailHashes[$sha]
    return [pscustomobject]@{
        Path = $exe
        Version = $version
        Sha256 = $sha
        Verified = [bool]$known
        Edition = if ($known) { $known } else { "unknown 1.08 build ($version)" }
    }
}

# Ein nachtraegliches Installieren oder Entfernen des bestaetigten
# HDTextures4FEAR-Patches darf eine vorhandene VR-Installation weiter nutzen.
# Unbekannte Hashwechsel bleiben dagegen ein Integritaetsfehler.
function Test-CompatibleRetailFearHashes(
    [string]$RecordedHash,
    [string]$CurrentHash
) {
    if ($RecordedHash -eq $CurrentHash) { return $true }
    $cfg = Get-FearVrReleaseConfig
    return (
        [bool]$cfg.KnownRetailHashes[$RecordedHash] -and
        [bool]$cfg.KnownRetailHashes[$CurrentHash]
    )
}

# Alle festen lokalen Laufwerke, damit ein Spiel auf D:\ oder E:\ genauso
# gefunden wird wie auf C:\. Netzlaufwerke bleiben draußen: Die Suche würde
# dort spürbar hängen.
function Get-LocalDriveRoots {
    $roots = New-Object Collections.Generic.List[string]
    try {
        foreach ($drive in [IO.DriveInfo]::GetDrives()) {
            if ($drive.DriveType -eq [IO.DriveType]::Fixed -and $drive.IsReady) {
                $roots.Add($drive.Name.TrimEnd('\'))
            }
        }
    } catch { }
    if ($roots.Count -eq 0) { $roots.Add('C:') }
    return $roots
}

# Installationsorte aus der Uninstall-Registry. Deckt GOG, die alte
# Retail-DVD und jede Neuinstallation an einem ungewöhnlichen Ort ab, ohne
# das Dateisystem durchsuchen zu müssen.
function Get-RegistryInstallLocations([string]$NamePattern) {
    $found = New-Object Collections.Generic.List[string]
    $keys = @(
        'HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\*',
        'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\*',
        'HKCU:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\*',
        'HKLM:\SOFTWARE\WOW6432Node\GOG.com\Games\*'
    )
    foreach ($key in $keys) {
        try { $entries = Get-ItemProperty $key -ErrorAction Stop } catch { continue }
        foreach ($entry in $entries) {
            $names = $entry.PSObject.Properties.Name
            $title = ''
            foreach ($field in @('DisplayName', 'gameName')) {
                if ($names -contains $field -and $entry.$field) {
                    $title = [string]$entry.$field
                    break
                }
            }
            if ($title -notlike $NamePattern) { continue }
            foreach ($field in @('InstallLocation', 'path', 'InstallPath')) {
                if ($names -contains $field -and $entry.$field) {
                    $found.Add(([string]$entry.$field).Trim('"'))
                }
            }
        }
    }
    return $found
}

# Sucht die Retail-Installation: erst Steam-Bibliotheken, dann die
# Uninstall-Registry (GOG, DVD), zuletzt die üblichen Ordnernamen auf allen
# festen Laufwerken. Ein Kandidat mit passendem Hash gewinnt immer; sonst
# kommt der erste Ordner mit einer FEAR.exe zurück, damit
# `Assert-RetailFearExe` die Abweichung benennen kann statt zu schweigen.
function Find-RetailRoot {
    $cfg = Get-FearVrReleaseConfig
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
    $gameFolders = @(
        'FEAR Ultimate Shooter Edition',
        'FEAR',
        'F.E.A.R',
        'F.E.A.R.',
        'FEAR Platinum Collection'
    )
    foreach ($library in $libraries) {
        foreach ($folder in $gameFolders) {
            $candidates.Add((Join-Path $library "steamapps\common\$folder"))
        }
    }

    # GOG, DVD-Installation und alles, was sich ordentlich registriert.
    foreach ($location in (Get-RegistryInstallLocations '*F.E.A.R*')) {
        $candidates.Add($location)
    }
    foreach ($location in (Get-RegistryInstallLocations '*FEAR*')) {
        $candidates.Add($location)
    }

    # Die üblichen Ordnernamen auf jedem festen Laufwerk.
    $parents = @(
        'Program Files (x86)',
        'Program Files',
        'Games',
        'GOG Games',
        'SteamLibrary\steamapps\common',
        'Games\steamapps\common',
        ''
    )
    $vendors = @('', 'Sierra\', 'Monolith Productions\', 'Vivendi Games\')
    foreach ($drive in (Get-LocalDriveRoots)) {
        foreach ($parent in $parents) {
            foreach ($vendor in $vendors) {
                foreach ($folder in $gameFolders) {
                    $candidates.Add(
                        [IO.Path]::Combine($drive + '\', $parent, "$vendor$folder"))
                }
            }
        }
    }

    # Ein getesteter Build gewinnt, danach jede andere 1.08-EXE (GOG,
    # Retail-DVD), zuletzt irgendeine FEAR.exe — die lehnt
    # `Assert-RetailFearExe` dann mit Begründung ab.
    $version108 = $null
    $anyExe = $null
    foreach ($candidate in $candidates) {
        if ([string]::IsNullOrWhiteSpace($candidate)) { continue }
        $exe = Join-Path $candidate 'FEAR.exe'
        if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) { continue }
        if ($cfg.KnownRetailHashes[(Get-FileSha256 $exe)]) { return $candidate }
        if (-not $version108) {
            $version = [Diagnostics.FileVersionInfo]::GetVersionInfo($exe).FileVersion
            if ($version -like '1.08*') { $version108 = $candidate }
        }
        if (-not $anyExe) { $anyExe = $candidate }
    }
    if ($version108) { return $version108 }
    return $anyExe
}

# Wie das Spiel gestartet wird. Steam verlangt den Umweg über
# `steam.exe -applaunch`, weil FEAR.exe dort ohne laufenden Client abbricht.
# GOG- und DVD-Installationen werden direkt gestartet; die Argumente sind
# dieselben, sie gehen nur nicht durch Steam.
function Get-RetailLaunchMode([string]$RetailRoot) {
    if ($RetailRoot -match '(?i)\\steamapps\\common\\' -and
        (Get-SteamExecutable)) {
        return 'steam'
    }
    return 'direct'
}

function Get-SteamExecutable {
    foreach ($key in @(
        'HKLM:\SOFTWARE\WOW6432Node\Valve\Steam',
        'HKLM:\SOFTWARE\Valve\Steam',
        'HKCU:\SOFTWARE\Valve\Steam'
    )) {
        try { $value = Get-ItemProperty $key -ErrorAction Stop } catch { continue }
        foreach ($name in @('InstallPath', 'SteamPath')) {
            if ($value.PSObject.Properties.Name -contains $name -and $value.$name) {
                $exe = Join-Path $value.$name 'steam.exe'
                if (Test-Path -LiteralPath $exe -PathType Leaf) { return $exe }
            }
        }
    }
    $default = Join-Path ${env:ProgramFiles(x86)} 'Steam\steam.exe'
    if (Test-Path -LiteralPath $default -PathType Leaf) { return $default }
    return $null
}

# Sucht ein Public-Tools-Runtime-Verzeichnis und verifiziert es über den
# Hash des unveränderten VC7.1-GameClient.dll. Der Installer der Public Tools
# lässt den Zielordner frei wählen, deshalb werden neben den Standardorten
# auch die Registry und die üblichen Ordnernamen je Laufwerk geprüft.
function Find-PublicToolsGame {
    $roots = New-Object Collections.Generic.List[string]
    foreach ($location in (Get-RegistryInstallLocations '*Public Tools*')) {
        $roots.Add($location)
    }
    $folders = @(
        'Monolith Productions\FEAR Public Tools',
        'FEAR Public Tools',
        'F.E.A.R. Public Tools'
    )
    $parents = @('Program Files (x86)', 'Program Files', 'Games', '')
    foreach ($drive in (Get-LocalDriveRoots)) {
        foreach ($parent in $parents) {
            foreach ($folder in $folders) {
                $roots.Add([IO.Path]::Combine($drive + '\', $parent, $folder))
            }
        }
    }
    foreach ($root in $roots) {
        if ([string]::IsNullOrWhiteSpace($root)) { continue }
        # Ein Nutzer, der den Pfad selbst angibt, zeigt mal auf die
        # Installationswurzel und mal direkt auf Dev\Runtime\Game.
        foreach ($suffix in @('Dev\Runtime\Game', '')) {
            $game = if ($suffix) { Join-Path $root $suffix } else { $root }
            if (Test-PublicToolsGame $game) { return $game }
        }
    }
    return $null
}

# Nimmt eine Nutzereingabe entgegen — Installationswurzel oder direkt das
# Runtime-Verzeichnis — und liefert das verifizierte Dev\Runtime\Game zurück.
function Resolve-PublicToolsGame([string]$Path) {
    if ([string]::IsNullOrWhiteSpace($Path)) { return $null }
    $trimmed = $Path.Trim().Trim('"')
    foreach ($suffix in @('', 'Dev\Runtime\Game', 'Runtime\Game', 'Game')) {
        $game = if ($suffix) { Join-Path $trimmed $suffix } else { $trimmed }
        if (Test-PublicToolsGame $game) {
            return [IO.Path]::GetFullPath($game)
        }
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

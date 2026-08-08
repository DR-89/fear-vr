<#
.SYNOPSIS
    Sets up F.E.A.R. VR on this machine.

.DESCRIPTION
    Builds an isolated game stage in a folder of its own. The retail
    installation is only ever read, never written, so a Steam file
    verification stays clean.

    Updating is the same command: run this script from the new package. An
    existing installation is detected, its paths are reused, its modules are
    replaced, and saved games under userdata are kept. Uninstalling first is
    not necessary.

    The package ships our own MIT-licensed modules only. The five Public
    Tools modules are copied from this machine's local Public Tools
    installation; they are deliberately not part of the package.

    Both the game folder and the Public Tools folder are detected
    automatically. If a path cannot be found, the script asks for it and
    shows examples.

.PARAMETER InstallDir
    Target folder. Default: %USERPROFILE%\FearVR

    Do NOT install below %LOCALAPPDATA%: the LithTech engine fails to load
    its archive configuration there and aborts with "Failed to initialize
    client - unable to load game resources". Measured on 2026-07-25 with a
    byte-identical archcfg in different locations; only the location makes
    the difference. The script therefore rejects such targets.

    Example: -InstallDir "D:\Games\FearVR"

.PARAMETER RetailRoot
    F.E.A.R. installation folder, i.e. the folder that contains FEAR.exe.
    Detected automatically when omitted.

    Example: -RetailRoot "C:\Program Files (x86)\Steam\steamapps\common\FEAR Ultimate Shooter Edition"

.PARAMETER PublicToolsGame
    Public Tools 1.08 folder. Either the installation root or its
    Dev\Runtime\Game subfolder. Detected automatically when omitted and
    verified against the hash of the stock GameClient.dll.

    Example: -PublicToolsGame "C:\Program Files (x86)\Monolith Productions\FEAR Public Tools"

.PARAMETER LaunchMode
    How the game is started later on.

    auto    (default) steam for a copy under steamapps\common, otherwise
            direct
    steam   through steam.exe -applaunch 21090
    direct  FEAR.exe is started directly, with the same arguments. This is
            the mode for GOG and retail-disc installations.

.PARAMETER NoShortcut
    Does not create a desktop shortcut.

.PARAMETER NonInteractive
    Never prompts. A path that cannot be detected becomes an error instead
    of a question — use this for unattended installs.

.PARAMETER Clean
    Only meaningful when updating: wipes the install folder first (except
    userdata) instead of replacing the modules in place.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File tools\release\install.ps1

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File tools\release\install.ps1 `
        -InstallDir "D:\Games\FearVR" `
        -RetailRoot "D:\SteamLibrary\steamapps\common\FEAR Ultimate Shooter Edition"
#>
[CmdletBinding()]
param(
    [string]$InstallDir = (Join-Path $env:USERPROFILE 'FearVR'),

    [string]$RetailRoot,

    [string]$PublicToolsGame,

    [ValidateSet('auto', 'steam', 'direct')]
    [string]$LaunchMode = 'auto',

    [switch]$NoShortcut,

    [switch]$NonInteractive,

    [switch]$Clean
)

$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\_fearvr-release.ps1"
$cfg = Get-FearVrReleaseConfig
$packageRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path

$script:CanPrompt = -not $NonInteractive -and [Environment]::UserInteractive

# Asks for a path and keeps asking until the validator accepts it. Empty
# input aborts with the same guidance the non-interactive run would print,
# so nobody ends up in a loop they cannot leave.
function Request-Path {
    param(
        [Parameter(Mandatory)][string]$Title,
        [Parameter(Mandatory)][string]$Explanation,
        [Parameter(Mandatory)][string[]]$Examples,
        [Parameter(Mandatory)][scriptblock]$Validate,
        [Parameter(Mandatory)][string]$ParameterName,
        [string]$Hint
    )

    $guidance = @()
    $guidance += $Explanation
    if ($Hint) { $guidance += '', $Hint }
    $guidance += '', 'Examples:'
    foreach ($example in $Examples) { $guidance += "  $example" }
    $guidanceText = ($guidance -join [Environment]::NewLine)

    if (-not $script:CanPrompt) {
        throw ($guidanceText + [Environment]::NewLine + [Environment]::NewLine +
            "Re-run with -$ParameterName ""<path>"".")
    }

    Write-Host ''
    Write-Host "  $Title" -ForegroundColor Yellow
    foreach ($line in $guidance) { Write-Host "  $line" }
    Write-Host ''

    while ($true) {
        $answer = Read-Host '  Path (empty to abort)'
        if ([string]::IsNullOrWhiteSpace($answer)) {
            throw ($guidanceText + [Environment]::NewLine + [Environment]::NewLine +
                "Aborted. Re-run with -$ParameterName ""<path>"".")
        }
        # Paths pasted from Explorer or from a shell often carry quotes.
        $answer = $answer.Trim().Trim('"')
        $resolved = & $Validate $answer
        if ($resolved) { return $resolved }
        Write-Host '  That path does not work. Please try again.' -ForegroundColor Yellow
    }
}

# --- 0. Check the target folder ---------------------------------------------
# With the archive configuration below %LOCALAPPDATA%, the engine aborts on
# start with "unable to load game resources". Proven with a byte-identical
# archcfg in different locations: only the location decides.
# %LOCALAPPDATA%\Temp works, other subfolders do not.
$localAppData = [IO.Path]::GetFullPath($env:LOCALAPPDATA)
while ($true) {
    $installFull = [IO.Path]::GetFullPath($InstallDir)
    if (-not $installFull.StartsWith($localAppData, [StringComparison]::OrdinalIgnoreCase)) {
        break
    }
    $InstallDir = Request-Path `
        -Title 'Unsuitable target folder.' `
        -ParameterName 'InstallDir' `
        -Explanation @"
$installFull lies below %LOCALAPPDATA%. There the LithTech engine fails to
load its archive configuration ("Failed to initialize client - unable to
load game resources"). Please pick another location.
"@ `
        -Examples @(
            "$env:USERPROFILE\FearVR",
            'D:\Games\FearVR',
            'C:\FearVR'
        ) `
        -Validate {
            param($value)
            $full = [IO.Path]::GetFullPath($value)
            if ($full.StartsWith($localAppData, [StringComparison]::OrdinalIgnoreCase)) {
                return $null
            }
            return $full
        }
}
$InstallDir = [IO.Path]::GetFullPath($InstallDir)

# --- Existing installation --------------------------------------------------
# Updating is the same command as installing. An earlier deployment.json
# supplies the paths that were used last time, so an update needs no arguments
# at all, and saved games under userdata are never touched.
$previous = $null
$previousPath = Join-Path $InstallDir 'deployment.json'
if (Test-Path -LiteralPath $previousPath -PathType Leaf) {
    try {
        $previous = Get-Content -Raw -LiteralPath $previousPath | ConvertFrom-Json
    } catch {
        # An unreadable manifest is treated as a fresh install; everything it
        # would have provided is detected again anyway.
        $previous = $null
    }
}

Write-Host (
    '=== F.E.A.R. VR - ' +
    $(if ($previous) { 'Update' } else { 'Installation' }) +
    ' ===') -ForegroundColor Cyan

if ($previous) {
    Write-Host ("  ..   Existing installation found in $InstallDir " +
        "(package $($previous.packageVersion))")
    # The running game holds the staged modules open; replacing them would
    # fail halfway through and leave a mixed module set behind.
    $running = @(Get-Process -Name 'FEAR' -ErrorAction SilentlyContinue)
    if ($running.Count -gt 0) {
        throw ("F.E.A.R. is running (PID $($running.Id -join ', ')). " +
               'Close the game, then run this again.')
    }
    if ([string]::IsNullOrWhiteSpace($RetailRoot) -and $previous.retailRoot) {
        $RetailRoot = $previous.retailRoot
    }
    if ([string]::IsNullOrWhiteSpace($PublicToolsGame) -and
        $previous.publicToolsGame) {
        $PublicToolsGame = $previous.publicToolsGame
    }
    if ($LaunchMode -eq 'auto' -and $previous.launchMode) {
        $LaunchMode = $previous.launchMode
    }
    if ($Clean) {
        # Everything except userdata: saved games and profiles stay.
        foreach ($entry in Get-ChildItem -LiteralPath $InstallDir -Force) {
            if ($entry.PSIsContainer -and $entry.Name -eq 'userdata') { continue }
            Remove-Item -LiteralPath $entry.FullName -Recurse -Force
        }
        Write-Host '  [OK] Removed the previous installation (userdata kept)'
    }
}

# --- 1. Locate and verify the retail installation ---------------------------
if ([string]::IsNullOrWhiteSpace($RetailRoot)) {
    Write-Host '  ..   Looking for the F.E.A.R. installation'
    $RetailRoot = Find-RetailRoot
}
if ([string]::IsNullOrWhiteSpace($RetailRoot) -or
    -not (Test-Path -LiteralPath (Join-Path $RetailRoot 'FEAR.exe') -PathType Leaf)) {
    $RetailRoot = Request-Path `
        -Title 'F.E.A.R. installation not found.' `
        -ParameterName 'RetailRoot' `
        -Explanation @'
Please enter the folder that contains FEAR.exe. The mod requires F.E.A.R.
1.08; older versions are rejected because the Public Tools modules do not
match them. Steam, GOG and the retail disc all work.
'@ `
        -Examples @(
            'C:\Program Files (x86)\Steam\steamapps\common\FEAR Ultimate Shooter Edition',
            'D:\SteamLibrary\steamapps\common\FEAR Ultimate Shooter Edition',
            'C:\GOG Games\FEAR',
            'C:\Program Files (x86)\Sierra\FEAR'
        ) `
        -Validate {
            param($value)
            if (Test-Path -LiteralPath (Join-Path $value 'FEAR.exe') -PathType Leaf) {
                return [IO.Path]::GetFullPath($value)
            }
            return $null
        }
}
$retail = Assert-RetailFearExe $RetailRoot
Write-Host "  [OK] F.E.A.R. $($retail.Version) - $($retail.Edition)"
Write-Host "       $RetailRoot"
if (-not $retail.Verified) {
    # Kein Abbruchgrund: Die versionsabhängigen Signaturen dieses Mods liegen
    # in GameOrig.dll aus den Public Tools, nicht in FEAR.exe.
    Write-Host @"
  [!]  This FEAR.exe build has not been tested with the mod.
       The listed Steam and GOG Preservation 1.08 builds are confirmed; other
       1.08 builds (other GOG revisions, retail disc) remain unverified.
       Please report success or failure so the build can be listed.
       SHA-256: $($retail.Sha256)
"@ -ForegroundColor Yellow
}

# Steam refuses to hand FEAR.exe its command line without the client running,
# so a Steam copy is launched through steam.exe -applaunch. Any other copy is
# started directly, with the same arguments.
$launchMode = if ($LaunchMode -eq 'auto') {
    Get-RetailLaunchMode $RetailRoot
} else {
    $LaunchMode
}
if ($launchMode -eq 'steam' -and -not (Get-SteamExecutable)) {
    throw @'
Launch mode "steam" was requested, but steam.exe was not found. Install
Steam, or use -LaunchMode direct to start FEAR.exe directly (GOG, retail
disc).
'@
}
Write-Host "  [OK] Launch mode: $launchMode"

$retailArchCfg = Join-Path $RetailRoot 'Default.archcfg'
if (-not (Test-Path -LiteralPath $retailArchCfg -PathType Leaf)) {
    throw "Retail archive configuration is missing: $retailArchCfg"
}

# --- 2. Locate and verify the Public Tools ----------------------------------
if ([string]::IsNullOrWhiteSpace($PublicToolsGame)) {
    Write-Host '  ..   Looking for the Public Tools 1.08 installation'
    $PublicToolsGame = Find-PublicToolsGame
} else {
    $PublicToolsGame = Resolve-PublicToolsGame $PublicToolsGame
}
if (-not (Test-PublicToolsGame $PublicToolsGame)) {
    $PublicToolsGame = Request-Path `
        -Title 'Public Tools 1.08 not found.' `
        -ParameterName 'PublicToolsGame' `
        -Explanation @'
The five modules GameClient.dll, GameServer.dll, ClientFx.fxd, FEAR.dep and
FEARMod.Arch00s are proprietary and must not ship with this package. They
come from the official installer "fear_publictools_108.exe", which is part
of the Ultimate Shooter Edition under extras\.

Please enter either the Public Tools installation folder or its
Dev\Runtime\Game subfolder.
'@ `
        -Hint @'
Note on installing the Public Tools: the installer reads
HKLM\SOFTWARE\WOW6432Node\Monolith Productions\FEAR\1.00.0000\Patch and
expects the value 8, while Steam sets 10. Set it to 8 for the installation
and back to 10 afterwards.
'@ `
        -Examples @(
            'C:\Program Files (x86)\Monolith Productions\FEAR Public Tools',
            'C:\Program Files (x86)\Monolith Productions\FEAR Public Tools\Dev\Runtime\Game',
            'D:\FEAR Public Tools'
        ) `
        -Validate {
            param($value)
            return (Resolve-PublicToolsGame $value)
        }
}
Write-Host '  [OK] Public Tools 1.08'
Write-Host "       $PublicToolsGame"

# --- 3. Check our own modules in the package --------------------------------
$manifestPath = Join-Path $packageRoot 'release-manifest.json'
if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
    throw "Package manifest is missing: $manifestPath"
}
$package = Get-Content -Raw -LiteralPath $manifestPath | ConvertFrom-Json
foreach ($entry in $package.files) {
    $path = Join-Path $packageRoot $entry.path
    $actual = Get-FileSha256 $path
    if ($actual -ne $entry.sha256) {
        throw "Package file is missing or was modified: $($entry.path)"
    }
}
Write-Host "  [OK] Package $($package.version) ($($package.gitCommit)) unmodified"

# --- 4. Build the stage -----------------------------------------------------
$moduleDirectory = Join-Path $InstallDir 'game-modules'
$userDirectory = Join-Path $InstallDir 'userdata'
$logDirectory = Join-Path $InstallDir 'logs'
foreach ($directory in @($InstallDir, $moduleDirectory, $userDirectory, $logDirectory)) {
    New-Item -ItemType Directory -Force -Path $directory | Out-Null
}

$staged = [ordered]@{}
foreach ($target in $cfg.PublicToolsModules.Keys) {
    $source = Join-Path $PublicToolsGame $cfg.PublicToolsModules[$target]
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
        throw "Public Tools module is missing: $source"
    }
    Copy-Item -LiteralPath $source -Destination (Join-Path $moduleDirectory $target) -Force
    $staged[$target] = 'public-tools'
}
foreach ($target in $cfg.BundledModules.Keys) {
    $source = Join-Path $packageRoot $cfg.BundledModules[$target]
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
        throw "Package module is missing: $source"
    }
    Copy-Item -LiteralPath $source -Destination (Join-Path $moduleDirectory $target) -Force
    $staged[$target] = 'fearvr'
}

# Die abgeleitete Textur wird erst hier aus der Public-Tools-Installation des
# Besitzers erzeugt und ist deshalb kein Bestandteil des Release-Pakets.
& (Join-Path $PSScriptRoot 'new-body-assets.ps1') `
    -SourceGame $PublicToolsGame `
    -DestinationGame $moduleDirectory

# An older package may have staged a module this one no longer uses. The
# archcfg layer would still load it, so anything not staged now goes.
foreach ($stale in Get-ChildItem -LiteralPath $moduleDirectory -File) {
    if ($staged.Contains($stale.Name)) { continue }
    Remove-Item -LiteralPath $stale.FullName -Force
    Write-Host "  [OK] Removed stale module $($stale.Name)"
}
Write-Host "  [OK] $($staged.Count) modules in $moduleDirectory"

# --- 5. Archive configuration -----------------------------------------------
# The loose archcfg layer is the official way to load a module set of our
# own without touching a single retail file.
$archiveConfig = Join-Path $InstallDir 'fearvr.archcfg'
$archiveLines = @(
    Get-Content -LiteralPath $retailArchCfg -Encoding Default |
        Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
)
$archiveLines += $moduleDirectory
[IO.File]::WriteAllLines($archiveConfig, $archiveLines, [Text.Encoding]::ASCII)

# --- 6. Deployment manifest -------------------------------------------------
$records = foreach ($name in $staged.Keys) {
    $path = Join-Path $moduleDirectory $name
    [ordered]@{
        name = $name
        origin = $staged[$name]
        sha256 = Get-FileSha256 $path
        bytes = (Get-Item -LiteralPath $path).Length
    }
}
$deployment = Join-Path $InstallDir 'deployment.json'
[ordered]@{
    installedUtc = (Get-Date).ToUniversalTime().ToString('s') + 'Z'
    packageVersion = $package.version
    packageGitCommit = $package.gitCommit
    packageRoot = $packageRoot
    retailRoot = $RetailRoot
    runtimeExe = $retail.Path
    runtimeSha256 = $retail.Sha256
    runtimeVerified = $retail.Verified
    runtimeEdition = $retail.Edition
    launchMode = $launchMode
    steamAppId = $cfg.SteamAppId
    publicToolsGame = $PublicToolsGame
    moduleDirectory = $moduleDirectory
    archiveConfig = $archiveConfig
    archiveConfigSha256 = Get-FileSha256 $archiveConfig
    userDirectory = $userDirectory
    logDirectory = $logDirectory
    files = @($records)
} | ConvertTo-Json -Depth 5 | Out-File -Encoding utf8 -LiteralPath $deployment

# --- 7. Shortcut ------------------------------------------------------------
$playScript = Join-Path $PSScriptRoot 'play.ps1'
if (-not $NoShortcut) {
    $shortcut = Join-Path ([Environment]::GetFolderPath('Desktop')) 'F.E.A.R. VR.lnk'
    $shell = New-Object -ComObject WScript.Shell
    $link = $shell.CreateShortcut($shortcut)
    $link.TargetPath =
        'C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe'
    $link.Arguments =
        "-NoProfile -ExecutionPolicy Bypass -File `"$playScript`" " +
        "-InstallDir `"$InstallDir`""
    $link.WorkingDirectory = $packageRoot
    $link.IconLocation = "$($retail.Path),0"
    $link.Description = 'Start F.E.A.R. VR'
    $link.Save()
    Write-Host "  [OK] Shortcut: $shortcut"
}

# --- 8. Retail must be unchanged --------------------------------------------
$retailAfter = Assert-RetailFearExe $RetailRoot
if ($retail.Sha256 -ne $retailAfter.Sha256) {
    throw 'SAFETY ABORT: the retail FEAR.exe was modified.'
}

Write-Host ''
if ($previous) {
    Write-Host ("Update complete ($($previous.packageVersion) -> " +
        "$($package.version)); retail untouched.") -ForegroundColor Green
    Write-Host 'Saved games and profiles were kept.'
} else {
    Write-Host 'Installation complete; retail untouched.' -ForegroundColor Green
}
Write-Host "Install folder: $InstallDir"
Write-Host ''
Write-Host 'Play:'
Write-Host "  desktop shortcut 'F.E.A.R. VR'"
Write-Host "  or: powershell -ExecutionPolicy Bypass -File `"$playScript`""
Write-Host ''
Write-Host 'Uninstall:'
Write-Host ("  powershell -ExecutionPolicy Bypass -File " +
            "`"$(Join-Path $PSScriptRoot 'uninstall.ps1')`" -Apply")

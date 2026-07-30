[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$PackageDir,

    [string]$OutputDir = (Join-Path $PSScriptRoot '..\dist'),

    [string]$Version,

    [string]$InnoSetupCompiler
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Resolve-FullPath {
    param([Parameter(Mandatory = $true)][string]$Path)

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }

    return [System.IO.Path]::GetFullPath((Join-Path (Get-Location) $Path))
}

function Find-InnoSetupCompiler {
    param([string]$RequestedPath)

    if ($RequestedPath) {
        $resolved = Resolve-FullPath $RequestedPath
        if (-not (Test-Path -LiteralPath $resolved -PathType Leaf)) {
            throw "Inno Setup compiler not found: $resolved"
        }
        return $resolved
    }

    $candidates = @(
        (Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 6\ISCC.exe'),
        (Join-Path $env:ProgramFiles 'Inno Setup 6\ISCC.exe')
    ) | Where-Object { $_ -and (Test-Path -LiteralPath $_ -PathType Leaf) }

    if ($candidates.Count -eq 0) {
        throw @'
Inno Setup 6 was not found.
Install it from https://jrsoftware.org/isinfo.php or pass -InnoSetupCompiler with the full path to ISCC.exe.
'@
    }

    return $candidates[0]
}

function Read-ProjectVersion {
    param([string]$RepositoryRoot)

    $cmakePath = Join-Path $RepositoryRoot 'CMakeLists.txt'
    if (-not (Test-Path -LiteralPath $cmakePath -PathType Leaf)) {
        return 'dev'
    }

    $cmake = Get-Content -LiteralPath $cmakePath -Raw
    $major = [regex]::Match($cmake, 'project\([^\)]*VERSION\s+(\d+)\.(\d+)\.(\d+)', 'IgnoreCase, Singleline')
    $label = [regex]::Match($cmake, 'set\(FEARVR_VERSION_LABEL\s+"([^"]*)"\)', 'IgnoreCase')

    if (-not $major.Success) {
        return 'dev'
    }

    $baseVersion = '{0}.{1}.{2}' -f $major.Groups[1].Value, $major.Groups[2].Value, $major.Groups[3].Value
    if ($label.Success -and -not [string]::IsNullOrWhiteSpace($label.Groups[1].Value)) {
        return "$baseVersion-$($label.Groups[1].Value)"
    }

    return $baseVersion
}

$repositoryRoot = Resolve-FullPath (Join-Path $PSScriptRoot '..')
$packagePath = Resolve-FullPath $PackageDir
$outputPath = Resolve-FullPath $OutputDir
$issPath = Join-Path $repositoryRoot 'installer\FearVR.iss'

if (-not (Test-Path -LiteralPath $packagePath -PathType Container)) {
    throw "Release package directory not found: $packagePath"
}

$requiredFiles = @(
    'tools\install.ps1',
    'tools\play.ps1'
)

foreach ($relativePath in $requiredFiles) {
    $candidate = Join-Path $packagePath $relativePath
    if (-not (Test-Path -LiteralPath $candidate -PathType Leaf)) {
        throw "The release package is missing required file: $relativePath"
    }
}

if (-not (Test-Path -LiteralPath $issPath -PathType Leaf)) {
    throw "Inno Setup script not found: $issPath"
}

if (-not $Version) {
    $Version = Read-ProjectVersion $repositoryRoot
}

$compiler = Find-InnoSetupCompiler $InnoSetupCompiler
New-Item -ItemType Directory -Path $outputPath -Force | Out-Null

Write-Host '=== F.E.A.R. VR graphical installer ==='
Write-Host "Package: $packagePath"
Write-Host "Output:  $outputPath"
Write-Host "Version: $Version"
Write-Host "Compiler: $compiler"

$arguments = @(
    "/DPackageDir=$packagePath",
    "/DOutputDir=$outputPath",
    "/DAppVersion=$Version",
    $issPath
)

& $compiler @arguments
if ($LASTEXITCODE -ne 0) {
    throw "Inno Setup failed with exit code $LASTEXITCODE."
}

$expectedInstaller = Join-Path $outputPath "FearVR-Setup-$Version.exe"
if (-not (Test-Path -LiteralPath $expectedInstaller -PathType Leaf)) {
    throw "Inno Setup completed, but the expected installer was not produced: $expectedInstaller"
}

Write-Host ''
Write-Host "Created: $expectedInstaller"

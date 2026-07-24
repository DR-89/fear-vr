[CmdletBinding()]
param(
    [switch]$VerifyOnly
)

$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$vendorRoot = Join-Path $projectRoot "vendor-local"

$dependencies = @(
    @{
        Name = "Khronos OpenXR-SDK"
        Url = "https://github.com/KhronosGroup/OpenXR-SDK.git"
        Tag = "release-1.1.59"
        Commit = "e5df31de6c15b4900aee3092273194e51282000d"
        Directory = Join-Path $vendorRoot "openxr-sdk"
    },
    @{
        Name = "Khronos OpenXR-SDK-Source"
        Url = "https://github.com/KhronosGroup/OpenXR-SDK-Source.git"
        Tag = "release-1.1.59"
        Commit = "04e92820192a6eec490e5eb8ffbd8211bafb0551"
        Directory = Join-Path $vendorRoot "openxr-sdk-source"
    }
)

function Invoke-Git {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$Arguments
    )

    & git @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "git $($Arguments -join ' ') failed with exit code $LASTEXITCODE."
    }
}

if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    throw "Git was not found on PATH."
}

if (-not (Test-Path -LiteralPath $vendorRoot)) {
    if ($VerifyOnly) {
        throw "Dependency directory is missing: $vendorRoot"
    }
    New-Item -ItemType Directory -Path $vendorRoot | Out-Null
}

foreach ($dependency in $dependencies) {
    $directory = $dependency.Directory
    if (-not (Test-Path -LiteralPath $directory)) {
        if ($VerifyOnly) {
            throw "$($dependency.Name) is missing: $directory"
        }

        Write-Host "Cloning $($dependency.Name) at $($dependency.Tag)..."
        Invoke-Git -Arguments @(
            "clone",
            "--branch", $dependency.Tag,
            "--depth", "1",
            $dependency.Url,
            $directory
        )
    }

    if (-not (Test-Path -LiteralPath (Join-Path $directory ".git"))) {
        throw "$directory exists but is not a Git checkout. It was not modified."
    }

    $actualOrigin = (& git -C $directory remote get-url origin).Trim()
    if ($LASTEXITCODE -ne 0) {
        throw "Could not read Git origin for $directory."
    }
    if ($actualOrigin -ne $dependency.Url) {
        throw "$($dependency.Name) has unexpected origin '$actualOrigin'. Expected '$($dependency.Url)'."
    }

    $actualCommit = (& git -C $directory rev-parse HEAD).Trim()
    if ($LASTEXITCODE -ne 0) {
        throw "Could not read Git commit for $directory."
    }
    if ($actualCommit -ne $dependency.Commit) {
        throw "$($dependency.Name) is at $actualCommit, expected $($dependency.Commit). The checkout was not changed."
    }

    Write-Host "[OK] $($dependency.Name) $($dependency.Tag) ($actualCommit)"
}

Write-Host "All M1 dependencies are present and pinned."

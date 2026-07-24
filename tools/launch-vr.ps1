<#
.SYNOPSIS
    Startet die isolierte F.E.A.R.-Stage — flat (M0) oder mit VR-Host (ab M1).

.DESCRIPTION
    Liest stage\launch-info.json (von prepare-stage.ps1 erzeugt) und startet die
    kopierte FEAR.exe aus stage\bin mit dem Retail-Verzeichnis als Working
    Directory und getrenntem -userdirectory. Der VR-Pfad (Host zuerst starten,
    auf "XR ready" warten) ist ab M1 aktiv.

.PARAMETER Flat
    Ohne VR-Host starten (Flat-Screen). Für M0 der einzige unterstützte Modus.
#>
[CmdletBinding()]
param(
    [switch]$Flat
)

$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\_fearvr-env.ps1"
$cfg = Get-FearVrConfig

# Retail vor jedem Start verifizieren (§2, §14).
Assert-RetailFearExe | Out-Null

$infoPath = Join-Path $cfg.ProjectRoot 'stage\launch-info.json'
if (-not (Test-Path -LiteralPath $infoPath)) {
    Write-Host "Keine Stage gefunden. Zuerst vorbereiten:" -ForegroundColor Yellow
    Write-Host "  pwsh -File tools\prepare-stage.ps1"
    exit 1
}
$info = Get-Content -Raw -LiteralPath $infoPath | ConvertFrom-Json

if (-not (Test-Path -LiteralPath $info.fearExe)) {
    throw "Staged FEAR.exe fehlt: $($info.fearExe). prepare-stage.ps1 erneut ausführen."
}

if (-not $Flat) {
    # VR-Pfad (ab M1): fearvr-host.exe starten, auf XR-ready warten, dann Spiel.
    Write-Host 'VR-Modus ist erst ab M1 verfügbar. Für M0 mit -Flat starten.' -ForegroundColor Yellow
    Write-Host '  pwsh -File tools\launch-vr.ps1 -Flat'
    exit 2
}

# --- Flat-Start (M0-Spike) ---
New-Item -ItemType Directory -Force -Path $info.userDirectory | Out-Null
$fearArgs = @('-userdirectory', $info.userDirectory)

Write-Host '=== Flat-Start (M0) ===' -ForegroundColor Cyan
Write-Host "EXE:        $($info.fearExe)"
Write-Host "WorkingDir: $($info.workingDir)"
Write-Host "Args:       $($fearArgs -join ' ')"

# Working Directory = Retail (read-only), Schreibzugriffe -> stage\userdata.
$p = Start-Process -FilePath $info.fearExe -ArgumentList $fearArgs `
        -WorkingDirectory $info.workingDir -PassThru
Write-Host "Gestartet (PID $($p.Id)). Retail-Verzeichnis wird nur gelesen."

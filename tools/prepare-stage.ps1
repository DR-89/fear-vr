<#
.SYNOPSIS
    Bereitet die isolierte, Retail-schonende Stage vor (ANWEISUNG.md §12).

.DESCRIPTION
    - Prüft Retail-Pfad, FEAR.exe-Version und SHA-256 (Abbruch bei Abweichung).
    - Erfasst eine Baseline der Hashes von Retail-EXE und zentralen DLLs.
    - Löst 'stage' AUSSCHLIESSLICH unter der Projektwurzel auf und validiert dies.
    - Kopiert (nie verschieben, nie hardlinken) nur die für den Start nötigen
      Binärdateien nach stage\bin.
    - Setzt das isolierte Benutzerverzeichnis stage\userdata.
    - Vergleicht die Retail-Hashes NACH der Vorbereitung erneut und bricht ab,
      falls sich eine Retail-Datei verändert hätte.
    Es werden NIEMALS Retail-Dateien gelöscht, verschoben oder überschrieben.

.PARAMETER Force
    Überschreibt vorhandene Dateien in stage\bin (nur innerhalb der Stage).
#>
[CmdletBinding(SupportsShouldProcess = $true)]
param(
    [switch]$Force
)

$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\_fearvr-env.ps1"
$cfg = Get-FearVrConfig

Write-Host '=== prepare-stage: Retail verifizieren ===' -ForegroundColor Cyan
$fear = Assert-RetailFearExe   # wirft bei falscher Version/Hash
Write-Host "FEAR.exe OK: $($fear.Version) / $($fear.Sha256)"

# --- Baseline der Retail-Hashes (EXE + zentrale DLLs im Wurzelverzeichnis) ---
function Get-RetailBaseline {
    $files = @()
    $files += Get-Item -LiteralPath (Join-Path $cfg.RetailRoot $cfg.FearExeName)
    $files += Get-ChildItem -LiteralPath $cfg.RetailRoot -Filter *.dll -File -ErrorAction SilentlyContinue
    $map = [ordered]@{}
    foreach ($f in $files) { $map[$f.Name] = (Get-FileSha256 $f.FullName) }
    return $map
}
Write-Host '=== Baseline der Retail-Hashes erfassen ===' -ForegroundColor Cyan
$baseline = Get-RetailBaseline
Write-Host ("Baseline: {0} Datei(en) gehasht." -f $baseline.Count)

# --- Stage-Pfade auflösen & absichern (§12.2) ---
$stageRoot = Assert-UnderProjectRoot (Join-Path $cfg.ProjectRoot 'stage')
$stageBin  = Assert-UnderProjectRoot (Join-Path $stageRoot 'bin')
$userData  = Assert-UnderProjectRoot (Join-Path $cfg.ProjectRoot $cfg.UserDataRel)
Write-Host "Stage:     $stageRoot"
Write-Host "Bin:       $stageBin"
Write-Host "Userdata:  $userData"

if ($PSCmdlet.ShouldProcess($stageRoot, 'Stage-Struktur anlegen')) {
    New-Item -ItemType Directory -Force -Path $stageBin  | Out-Null
    New-Item -ItemType Directory -Force -Path $userData | Out-Null
}

# --- FEAR.exe in die Stage KOPIEREN (nie verschieben/hardlinken) ---
$destExe = Join-Path $stageBin $cfg.FearExeName
if ((Test-Path -LiteralPath $destExe) -and -not $Force) {
    Write-Host "stage\bin\FEAR.exe existiert bereits (mit -Force überschreiben)." -ForegroundColor Yellow
} elseif ($PSCmdlet.ShouldProcess($destExe, 'FEAR.exe nach stage kopieren')) {
    Copy-Item -LiteralPath $fear.Path -Destination $destExe -Force:$Force
    Write-Host "Kopiert -> $destExe"
}

# --- Start-Info schreiben (Working Dir = Retail; -userdirectory = stage) ---
# Staging-Modell (§12-Spike): kopierte FEAR.exe startet mit dem Retail-Verzeichnis
# als Working Directory, damit Archive/DLLs read-only dort gefunden werden;
# alle Schreibzugriffe gehen ins isolierte stage\userdata.
$launchInfo = [ordered]@{
    fearExe        = $destExe
    workingDir     = $cfg.RetailRoot
    userDirectory  = $userData
    preparedUtc    = (Get-Date).ToUniversalTime().ToString('s') + 'Z'
    retailSha256   = $fear.Sha256
    note           = 'Archive/DLLs werden read-only aus workingDir referenziert. archcfg-/Mod-Pfad ist M0-TODO (Public-Tools-Doku).'
}
$infoPath = Join-Path $stageRoot 'launch-info.json'
if ($PSCmdlet.ShouldProcess($infoPath, 'launch-info.json schreiben')) {
    $launchInfo | ConvertTo-Json -Depth 4 | Out-File -Encoding utf8 -LiteralPath $infoPath
    $baselinePath = Join-Path $stageRoot 'retail-baseline.json'
    $baseline | ConvertTo-Json -Depth 4 | Out-File -Encoding utf8 -LiteralPath $baselinePath
    Write-Host "Geschrieben: $infoPath"
    Write-Host "Geschrieben: $baselinePath"
}

# --- Retail-Hashes NACH der Vorbereitung erneut vergleichen (§12.8) ---
Write-Host '=== Retail-Integrität nach Vorbereitung prüfen ===' -ForegroundColor Cyan
$after = Get-RetailBaseline
$changed = @()
foreach ($k in $baseline.Keys) {
    if ($baseline[$k] -ne $after[$k]) { $changed += $k }
}
if ($changed.Count -gt 0) {
    throw "SICHERHEITSABBRUCH: Retail-Datei(en) verändert: $($changed -join ', ')"
}
Write-Host 'Retail unverändert. Stage bereit.' -ForegroundColor Green
Write-Host ''
Write-Host 'Nächster Schritt (M0-Spike): Flat-Start testen ->'
Write-Host '  pwsh -File tools\launch-vr.ps1 -Flat'

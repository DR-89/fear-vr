<#
.SYNOPSIS
    Stellt die lokal gebauten Stock-Module in der Public-Tools-Dev-Runtime bereit.

.DESCRIPTION
    Sichert GameClient.dll, GameServer.dll und ClientFx.fxd aus der lokalen
    Dev-Runtime genau einmal unter stage und kopiert danach die selbst gebauten
    x86-Module nach vendor-local\publictools\Dev\Runtime\Game. Zusätzlich wird
    unter stage eine Archivkonfiguration aus der Retail-Konfiguration plus dem
    losen Game-Verzeichnis erzeugt. Gestartet wird die originale Steam-FEAR.exe
    in ihrem Installationsverzeichnis.

    FEARDevSP.exe wird wegen ihrer veralteten CD/DVD-Prüfung ausdrücklich nicht
    verwendet. Eine kopierte Steam-FEAR.exe wird wegen Steams Pfadprüfung
    ebenfalls nicht gestartet.

    Die Steam-/Retail-Installation wird nur gelesen und per Hash verifiziert;
    sie wird nicht beschrieben.

.PARAMETER Restore
    Stellt die beim ersten Deploy gesicherten Public-Tools-Module wieder her.
#>
[CmdletBinding(SupportsShouldProcess = $true)]
param(
    [string]$PublicToolsRoot,
    [ValidateSet('Release', 'Debug')][string]$Configuration = 'Release',
    [switch]$Restore
)

$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($PublicToolsRoot)) {
    $PublicToolsRoot = Join-Path $PSScriptRoot '..\vendor-local\publictools'
}

. "$PSScriptRoot\_fearvr-env.ps1"
$cfg = Get-FearVrConfig
$retailBefore = Assert-RetailFearExe

$publicToolsRoot = [IO.Path]::GetFullPath($PublicToolsRoot)
$builtRoot = Join-Path $publicToolsRoot "Source\built\$Configuration"
$runtimeRoot = Join-Path $publicToolsRoot 'Dev\Runtime'
$runtimeGame = Join-Path $runtimeRoot 'Game'
$runtimeExe = $retailBefore.Path
$steamExe = 'C:\Program Files (x86)\Steam\steam.exe'
$steamAppId = 21090
$retailArchCfg = Join-Path $cfg.RetailRoot 'Default.archcfg'
$backupRoot = Assert-UnderProjectRoot (Join-Path $cfg.ProjectRoot 'stage\m0-stock-module-backup')
$manifestPath = Join-Path $backupRoot 'manifest.json'
$deploymentPath = Assert-UnderProjectRoot (Join-Path $cfg.ProjectRoot 'stage\m0-stock-deployment.json')
$archiveConfigPath = Assert-UnderProjectRoot (Join-Path $cfg.ProjectRoot 'stage\m0-stock.archcfg')
$moduleNames = @('GameClient.dll', 'GameServer.dll', 'ClientFx.fxd')

foreach ($requiredDirectory in @($builtRoot, $runtimeGame)) {
    if (-not (Test-Path -LiteralPath $requiredDirectory -PathType Container)) {
        throw "Erforderliches Verzeichnis fehlt: $requiredDirectory"
    }
}
if (-not (Test-Path -LiteralPath $retailArchCfg -PathType Leaf)) {
    throw "Retail-Archivkonfiguration fehlt: $retailArchCfg"
}
if (-not (Test-Path -LiteralPath $steamExe -PathType Leaf)) {
    throw "Steam-Client fehlt: $steamExe"
}

function Assert-X86Pe {
    param([Parameter(Mandatory = $true)][string]$Path)

    $bytes = [IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -lt 512) {
        throw "PE-Datei ist unerwartet klein: $Path"
    }
    $peOffset = [BitConverter]::ToInt32($bytes, 0x3c)
    $machine = [BitConverter]::ToUInt16($bytes, $peOffset + 4)
    if ($machine -ne 0x014c) {
        throw ("Datei ist nicht PE32/x86: {0}, Machine=0x{1:X4}" -f $Path, $machine)
    }
}

function Assert-Vc71RuntimeImports {
    param([Parameter(Mandatory = $true)][string]$Path)

    $asciiImage = [Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($Path))
    if (-not $asciiImage.Contains('MSVCR71.dll') -or
        -not $asciiImage.Contains('MSVCP71.dll')) {
        throw @"
ABI-SICHERHEITSABBRUCH: $Path
Das Modul importiert nicht MSVCR71.dll und MSVCP71.dll. VS2022/v141 erzeugt
zwar linkbare PE32-Dateien, diese sind aber nicht binärkompatibel mit F.E.A.R.
1.08 und stürzen beim Start ab. Nur VC7.1-kompatible Module deployen.
"@
    }
}

if ($Restore) {
    if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
        throw "Kein Modulbackup vorhanden: $manifestPath"
    }

    $manifest = Get-Content -Raw -LiteralPath $manifestPath | ConvertFrom-Json
    foreach ($name in $moduleNames) {
        $source = Join-Path $backupRoot $name
        $destination = Join-Path $runtimeGame $name
        if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
            throw "Backupmodul fehlt: $source"
        }

        $record = $manifest.modules | Where-Object { $_.name -eq $name }
        if (-not $record) {
            throw "Backupmanifest enthält keinen Eintrag für $name"
        }
        $sourceHash = Get-FileSha256 $source
        if ($sourceHash -ne $record.sha256) {
            throw "Backuphash stimmt nicht für $name"
        }

        if ($PSCmdlet.ShouldProcess($destination, "Originales Public-Tools-Modul $name wiederherstellen")) {
            Copy-Item -LiteralPath $source -Destination $destination -Force
            Write-Host "[RESTORE] $name"
        }
    }
} else {
    foreach ($name in $moduleNames) {
        $builtPath = Join-Path $builtRoot $name
        $runtimePath = Join-Path $runtimeGame $name
        if (-not (Test-Path -LiteralPath $builtPath -PathType Leaf)) {
            throw "Buildartefakt fehlt: $builtPath. Zuerst tools\build-game-modules.ps1 ausführen."
        }
        if (-not (Test-Path -LiteralPath $runtimePath -PathType Leaf)) {
            throw "Public-Tools-Stockmodul fehlt: $runtimePath"
        }
        Assert-X86Pe $builtPath
        Assert-Vc71RuntimeImports $builtPath
    }

    if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
        if ($PSCmdlet.ShouldProcess($backupRoot, 'Unveränderte Public-Tools-Module einmalig sichern')) {
            New-Item -ItemType Directory -Force -Path $backupRoot | Out-Null
            $records = @()
            foreach ($name in $moduleNames) {
                $source = Join-Path $runtimeGame $name
                $destination = Join-Path $backupRoot $name
                Copy-Item -LiteralPath $source -Destination $destination
                $records += [pscustomobject]@{
                    name = $name
                    sha256 = Get-FileSha256 $destination
                    bytes = (Get-Item -LiteralPath $destination).Length
                }
            }
            [ordered]@{
                createdUtc = (Get-Date).ToUniversalTime().ToString('s') + 'Z'
                publicToolsRoot = $publicToolsRoot
                modules = $records
            } | ConvertTo-Json -Depth 4 | Out-File -Encoding utf8 -LiteralPath $manifestPath
            Write-Host "Stockbackup angelegt: $backupRoot"
        }
    } else {
        $manifest = Get-Content -Raw -LiteralPath $manifestPath | ConvertFrom-Json
        foreach ($name in $moduleNames) {
            $backupPath = Join-Path $backupRoot $name
            $record = $manifest.modules | Where-Object { $_.name -eq $name }
            if (-not $record -or -not (Test-Path -LiteralPath $backupPath -PathType Leaf)) {
                throw "Vorhandenes Stockbackup ist unvollständig: $name"
            }
            if ((Get-FileSha256 $backupPath) -ne $record.sha256) {
                throw "Vorhandenes Stockbackup hat einen falschen Hash: $name"
            }
        }
        Write-Host "Vorhandenes Stockbackup verifiziert: $backupRoot"
    }

    $deployed = @()
    foreach ($name in $moduleNames) {
        $source = Join-Path $builtRoot $name
        $destination = Join-Path $runtimeGame $name
        if ($PSCmdlet.ShouldProcess($destination, "Gebautes Stockmodul $name bereitstellen")) {
            Copy-Item -LiteralPath $source -Destination $destination -Force
            $sourceHash = Get-FileSha256 $source
            $destinationHash = Get-FileSha256 $destination
            if ($sourceHash -ne $destinationHash) {
                throw "Deploy-Verifikation fehlgeschlagen: $name"
            }
            $deployed += [pscustomobject]@{
                name = $name
                sha256 = $destinationHash
                bytes = (Get-Item -LiteralPath $destination).Length
            }
            Write-Host "[DEPLOY] $name  $destinationHash"
        }
    }

    if (-not $WhatIfPreference) {
        $archiveLines = @(
            Get-Content -LiteralPath $retailArchCfg -Encoding Default |
                Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
        )
        $archiveLines += $runtimeGame
        [IO.File]::WriteAllLines($archiveConfigPath, $archiveLines, [Text.Encoding]::ASCII)

        [ordered]@{
            deployedUtc = (Get-Date).ToUniversalTime().ToString('s') + 'Z'
            runtimeExe = $runtimeExe
            steamExe = $steamExe
            steamAppId = $steamAppId
            workingDirectory = $cfg.RetailRoot
            moduleDirectory = $runtimeGame
            archiveConfig = $archiveConfigPath
            archiveConfigSha256 = (Get-FileSha256 $archiveConfigPath)
            userDirectory = (Assert-UnderProjectRoot (Join-Path $cfg.ProjectRoot 'stage\userdata-m0'))
            modules = $deployed
        } | ConvertTo-Json -Depth 4 | Out-File -Encoding utf8 -LiteralPath $deploymentPath
        Write-Host "Archivkonfiguration: $archiveConfigPath"
        Write-Host "Deploymentmanifest: $deploymentPath"
    }
}

$retailAfter = Assert-RetailFearExe
if ($retailBefore.Sha256 -ne $retailAfter.Sha256) {
    throw 'SICHERHEITSABBRUCH: Retail-FEAR.exe hat sich während des Deployments verändert.'
}

Write-Host 'Retail-FEAR.exe unverändert.' -ForegroundColor Green
if ($WhatIfPreference) {
    Write-Host 'Trockenlauf abgeschlossen; es wurden keine Module kopiert.' -ForegroundColor Green
} elseif ($Restore) {
    Write-Host 'Public-Tools-Stockmodule wurden wiederhergestellt.' -ForegroundColor Green
} else {
    Write-Host 'Gebauter Stock-Client ist für den M0-Lauftest bereit.' -ForegroundColor Green
}

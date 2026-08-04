<#
.SYNOPSIS
    Erzeugt eine VR-lesbare lokalisierte F.E.A.R.-Datenbank.

.DESCRIPTION
    F.E.A.R. liest HUDSwap nicht als lose .record-Datei, sondern aus
    DatabaseLocalized\FEARL.Gamdb00p. Dieses Skript kopiert die originale
    Public-Tools-Datenbank und ersetzt darin ausschliesslich die drei
    eindeutig identifizierten Werte des Waffen-/Pickup-Hinweises.

    Die strikten Kontext- und Trefferpruefungen verhindern, dass eine andere
    Version oder ein bereits veraendertes Paket unbemerkt beschaedigt wird.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$SourceGame,

    [Parameter(Mandatory)]
    [string]$DestinationGame
)

$ErrorActionPreference = 'Stop'

$relativeRecord =
    'DatabaseLocalized\Interface\HUD\HUDSwap.record'
$relativeDatabase =
    'DatabaseLocalized\FEARL.Gamdb00p'
$sourceRecord = Join-Path $SourceGame $relativeRecord
$sourceDatabase = Join-Path $SourceGame $relativeDatabase
foreach ($source in @($sourceRecord, $sourceDatabase)) {
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
        throw "Public-Tools-HUD-Daten fehlen: $source"
    }
}

# Die menschenlesbare Quelle ist eine zweite Versionssicherung. Dadurch wird
# nicht allein aufgrund zufaellig gleicher Bytefolgen in einer fremden
# Datenbank gepatcht.
$recordText = [IO.File]::ReadAllText($sourceRecord)
foreach ($expectedLine in @(
    'Value.0000=12',
    'Value.0000=160, 227, 245, 242',
    'Value.0000=255, 19, 236, 30'
)) {
    if (-not [Regex]::IsMatch(
            $recordText,
            '(?m)^' + [Regex]::Escape($expectedLine) + '\r?$')) {
        throw (
            'HUDSwap entspricht nicht der erwarteten Public-Tools-' +
            "Version 1.08: $sourceRecord")
    }
}

function Find-UniqueBytePattern(
    [byte[]]$Data,
    [byte[]]$Pattern,
    [string]$Description
) {
    $foundOffset = -1
    $matchCount = 0
    for ($offset = 0; $offset -le $Data.Length - $Pattern.Length; ++$offset) {
        $matches = $true
        for ($index = 0; $index -lt $Pattern.Length; ++$index) {
            if ($Data[$offset + $index] -ne $Pattern[$index]) {
                $matches = $false
                break
            }
        }
        if ($matches) {
            $foundOffset = $offset
            ++$matchCount
        }
    }
    if ($matchCount -ne 1) {
        throw (
            "HUDSwap-Binaerfeld '$Description' wurde $matchCount-mal " +
            "statt genau einmal gefunden: $sourceDatabase")
    }
    return $foundOffset
}

function Set-UniqueBytePattern(
    [byte[]]$Data,
    [byte[]]$Expected,
    [byte[]]$Replacement,
    [string]$Description
) {
    if ($Expected.Length -ne $Replacement.Length) {
        throw "Interner Patchlaengenfehler fuer '$Description'."
    }
    $offset = Find-UniqueBytePattern $Data $Expected $Description
    [Array]::Copy(
        $Replacement, 0, $Data, $offset, $Replacement.Length)
    return $offset
}

$database = [IO.File]::ReadAllBytes($sourceDatabase)

# Im gepackten Format folgt auf Typ, Flags und Elementzahl der int32-Wert.
# Die davor liegende eindeutige Retail-Textfarbe bindet die sonst haeufige
# Zahl 12 zweifelsfrei an HUDSwap.TextSize.
[byte[]]$oldTextSize = @(
    0xF2, 0xF5, 0xE3, 0xA0,
    0x5C, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00,
    0x88, 0x00, 0x00, 0x00,
    0x70, 0x00, 0x00, 0x00,
    0x03, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00,
    0x0C, 0x00, 0x00, 0x00,
    0xF4, 0x03, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0x00
)
[byte[]]$newTextSize = $oldTextSize.Clone()
$newTextSize[40] = 0x1A # 26 Pixel statt 12

# Farben sind im gepackten Datensatz als little-endian Int_ARGB abgelegt.
# Normaler Hinweis: deckendes Weiss. Notwendiger Waffenwechsel: deckendes
# warmes Gelb statt des schwer lesbaren Retail-Gruens.
$offsets = [ordered]@{}
$offsets.TextSize = Set-UniqueBytePattern `
    $database $oldTextSize $newTextSize 'TextSize=12'
$offsets.TextColor = Set-UniqueBytePattern `
    $database `
    ([byte[]]@(0xF2, 0xF5, 0xE3, 0xA0)) `
    ([byte[]]@(0xFF, 0xFF, 0xFF, 0xFF)) `
    'TextColor=A0E3F5F2'
$offsets.AdditionalColor = Set-UniqueBytePattern `
    $database `
    ([byte[]]@(0x1E, 0xEC, 0x13, 0xFF)) `
    ([byte[]]@(0x50, 0xE0, 0xFF, 0xFF)) `
    'AdditionalColor=FF13EC1E'

$destination = Join-Path $DestinationGame $relativeDatabase
New-Item -ItemType Directory -Force -Path (
    Split-Path -Parent $destination) | Out-Null
[IO.File]::WriteAllBytes($destination, $database)

if ((Get-Item -LiteralPath $destination).Length -ne
    (Get-Item -LiteralPath $sourceDatabase).Length) {
    throw "Die erzeugte HUD-Datenbank hat eine falsche Laenge: $destination"
}

Write-Host (
    "VR-Pickup-Hinweis in FEARL.Gamdb00p erzeugt " +
    "(TextSize@$($offsets.TextSize + 40), " +
    "TextColor@$($offsets.TextColor), " +
    "SwapColor@$($offsets.AdditionalColor)): $destination")

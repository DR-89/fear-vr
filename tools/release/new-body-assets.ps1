<#
.SYNOPSIS
    Erzeugt die lokalen VR-Body-Overrides aus der installierten Public-Tools-
    Textur.

.DESCRIPTION
    player.Model00p legt Torso, Beine und beide Arme gemeinsam in Body_Group
    ab. Das komplette Piece auszublenden entfernt deshalb auch die Kick-
    Animationen. Dieses Skript liest Positionen, UVs und Dreiecke direkt aus
    dem lokalen Player-Modell. Es erkennt die sechs Arm-Meshes (drei LODs pro
    Seite) anhand ihrer Bind-Pose-Geometrie und stanzt nur deren UV-Dreiecke
    aus der DXT3-Textur. Die getrennten Hand-Meshes werden vor dem Schreiben
    ausdrücklich auf Überschneidungen geprüft.

    Die abgeleiteten Dateien werden ausschließlich auf dem Rechner des
    Besitzers aus dessen Public-Tools-Installation erzeugt. Sie gehören nicht
    in ein Release-Paket.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$SourceGame,

    [Parameter(Mandatory = $true)]
    [string]$DestinationGame
)

$ErrorActionPreference = 'Stop'

function Get-DisjointSetRoot {
    param(
        [int[]]$Parents,
        [int]$Index
    )

    $root = $Index
    while ($Parents[$root] -ne $root) {
        $root = $Parents[$root]
    }
    while ($Parents[$Index] -ne $Index) {
        $next = $Parents[$Index]
        $Parents[$Index] = $root
        $Index = $next
    }
    return $root
}

function Join-DisjointSet {
    param(
        [int[]]$Parents,
        [int[]]$Sizes,
        [int]$Left,
        [int]$Right
    )

    $leftRoot = Get-DisjointSetRoot $Parents $Left
    $rightRoot = Get-DisjointSetRoot $Parents $Right
    if ($leftRoot -eq $rightRoot) {
        return
    }
    if ($Sizes[$leftRoot] -lt $Sizes[$rightRoot]) {
        $temporary = $leftRoot
        $leftRoot = $rightRoot
        $rightRoot = $temporary
    }
    $Parents[$rightRoot] = $leftRoot
    $Sizes[$leftRoot] += $Sizes[$rightRoot]
}

function Find-PlayerMeshDirectory {
    param([byte[]]$ModelBytes)

    # FEARs Model00p-Container hat vor dem eigentlichen Render-Mesh mehrere
    # variable Verzeichnisse. Das Mesh selbst ist eindeutig: 64 Byte je
    # Vertex, danach Dreiecksindizes als UInt16. Durch die vollständige
    # Indexprüfung vermeiden wir versionsabhängige, fest codierte Offsets.
    $searchLimit = [Math]::Min($ModelBytes.Length - 8, 131072)
    for ($offset = 0; $offset -lt $searchLimit; ++$offset) {
        $vertexLength = [BitConverter]::ToInt32($ModelBytes, $offset)
        if ($vertexLength -lt 64000 -or $vertexLength -gt 1280000 -or
            ($vertexLength % 64) -ne 0) {
            continue
        }
        $faceLength = [BitConverter]::ToInt32($ModelBytes, $offset + 4)
        if ($faceLength -lt 6000 -or $faceLength -gt 600000 -or
            ($faceLength % 6) -ne 0 -or
            $offset + 8 + $vertexLength + $faceLength -gt
                $ModelBytes.Length) {
            continue
        }

        $vertexCount = [int]($vertexLength / 64)
        $vertexData = $offset + 8
        $sampleValid = $true
        for ($sample = 0; $sample -lt 24; ++$sample) {
            $vertex = $vertexData +
                [int]($sample * ($vertexCount - 1) / 23) * 64
            $normalX = [BitConverter]::ToSingle($ModelBytes, $vertex + 12)
            $normalY = [BitConverter]::ToSingle($ModelBytes, $vertex + 16)
            $normalZ = [BitConverter]::ToSingle($ModelBytes, $vertex + 20)
            $u = [BitConverter]::ToSingle($ModelBytes, $vertex + 24)
            $v = [BitConverter]::ToSingle($ModelBytes, $vertex + 28)
            if ([Single]::IsNaN($normalX) -or
                [Single]::IsNaN($normalY) -or
                [Single]::IsNaN($normalZ) -or
                [Math]::Abs($normalX) -gt 1.01 -or
                [Math]::Abs($normalY) -gt 1.01 -or
                [Math]::Abs($normalZ) -gt 1.01 -or
                $u -lt -0.01 -or $u -gt 1.01 -or
                $v -lt -0.01 -or $v -gt 1.01) {
                $sampleValid = $false
                break
            }
        }
        if (-not $sampleValid) {
            continue
        }

        $indexData = $vertexData + $vertexLength
        $indexCount = [int]($faceLength / 2)
        $usedVertices = [Collections.Generic.HashSet[int]]::new()
        $maximumIndex = 0
        $indicesValid = $true
        for ($index = 0; $index -lt $indexCount; ++$index) {
            $value = [int][BitConverter]::ToUInt16(
                $ModelBytes, $indexData + $index * 2)
            if ($value -ge $vertexCount) {
                $indicesValid = $false
                break
            }
            if ($value -gt $maximumIndex) {
                $maximumIndex = $value
            }
            [void]$usedVertices.Add($value)
        }
        if ($indicesValid -and
            $maximumIndex -eq $vertexCount - 1 -and
            $usedVertices.Count -ge [int]($vertexCount * 0.95)) {
            return [pscustomobject]@{
                VertexData = $vertexData
                VertexCount = $vertexCount
                IndexData = $indexData
                IndexCount = $indexCount
            }
        }
    }

    throw 'The render-mesh directory in player.Model00p was not found.'
}

function Set-TriangleMaskPixels {
    param(
        [object[]]$Vertices,
        [int[]]$Triangle,
        [int]$Width,
        [int]$Height,
        [bool[]]$Mask
    )

    $x = [double[]]::new(3)
    $y = [double[]]::new(3)
    for ($corner = 0; $corner -lt 3; ++$corner) {
        $vertex = $Vertices[$Triangle[$corner]]
        $x[$corner] = $vertex.U * $Width
        $y[$corner] = $vertex.V * $Height
    }

    # UV-Dreiecke dürfen über den Wrap-Rand laufen. Für den Flächentest
    # werden sie zunächst in einen zusammenhängenden Bereich verschoben.
    if ((($x | Measure-Object -Maximum).Maximum -
            ($x | Measure-Object -Minimum).Minimum) -gt $Width / 2) {
        for ($corner = 0; $corner -lt 3; ++$corner) {
            if ($x[$corner] -lt $Width / 2) {
                $x[$corner] += $Width
            }
        }
    }
    if ((($y | Measure-Object -Maximum).Maximum -
            ($y | Measure-Object -Minimum).Minimum) -gt $Height / 2) {
        for ($corner = 0; $corner -lt 3; ++$corner) {
            if ($y[$corner] -lt $Height / 2) {
                $y[$corner] += $Height
            }
        }
    }

    $denominator =
        ($y[1] - $y[2]) * ($x[0] - $x[2]) +
        ($x[2] - $x[1]) * ($y[0] - $y[2])
    if ([Math]::Abs($denominator) -lt 0.000001) {
        return
    }

    $minimumX = [int][Math]::Floor(
        ($x | Measure-Object -Minimum).Minimum)
    $maximumX = [int][Math]::Ceiling(
        ($x | Measure-Object -Maximum).Maximum)
    $minimumY = [int][Math]::Floor(
        ($y | Measure-Object -Minimum).Minimum)
    $maximumY = [int][Math]::Ceiling(
        ($y | Measure-Object -Maximum).Maximum)

    for ($pixelY = $minimumY; $pixelY -le $maximumY; ++$pixelY) {
        for ($pixelX = $minimumX; $pixelX -le $maximumX; ++$pixelX) {
            $centerX = $pixelX + 0.5
            $centerY = $pixelY + 0.5
            $a = (
                ($y[1] - $y[2]) * ($centerX - $x[2]) +
                ($x[2] - $x[1]) * ($centerY - $y[2])
            ) / $denominator
            $b = (
                ($y[2] - $y[0]) * ($centerX - $x[2]) +
                ($x[0] - $x[2]) * ($centerY - $y[2])
            ) / $denominator
            $c = 1.0 - $a - $b
            if ($a -lt -0.000001 -or $b -lt -0.000001 -or
                $c -lt -0.000001) {
                continue
            }

            $wrappedX = (($pixelX % $Width) + $Width) % $Width
            $wrappedY = (($pixelY % $Height) + $Height) % $Height
            $Mask[$wrappedY * $Width + $wrappedX] = $true
        }
    }
}

function Write-AsciiString16 {
    param(
        [System.IO.BinaryWriter]$Writer,
        [string]$Value
    )

    $bytes = [Text.Encoding]::ASCII.GetBytes($Value)
    if ($bytes.Length -gt [UInt16]::MaxValue) {
        throw "Material string is too long: $Value"
    }
    $Writer.Write([UInt16]$bytes.Length)
    $Writer.Write($bytes)
}

function Write-PlayerBodyMaterial {
    param([string]$Path)

    $stream = [IO.File]::Open(
        $Path, [IO.FileMode]::Create, [IO.FileAccess]::Write,
        [IO.FileShare]::None)
    try {
        $writer = [IO.BinaryWriter]::new(
            $stream, [Text.Encoding]::ASCII, $true)
        try {
            $writer.Write([Text.Encoding]::ASCII.GetBytes('LTMI'))
            $writer.Write([UInt32]1)
            Write-AsciiString16 $writer `
                'Shaders\skeletal\Solid\specular_alphatest.fx'
            $writer.Write([UInt32]8)

            # Integer material parameter.
            $writer.Write([UInt32]4)
            Write-AsciiString16 $writer 'SurfaceFlags'
            $writer.Write([UInt32]2)

            foreach ($floatParameter in @(
                @{ Name = 'DefaultWidth'; Value = 100.0 },
                @{ Name = 'DefaultHeight'; Value = 100.0 },
                @{ Name = 'fMaxSpecularPower'; Value = 64.0 }
            )) {
                $writer.Write([UInt32]5)
                Write-AsciiString16 $writer $floatParameter.Name
                $writer.Write([Single]$floatParameter.Value)
            }

            foreach ($textureParameter in @(
                @{ Name = 'tDiffuseMap'; Value = 'fearvr\player_body_d.dds' },
                @{ Name = 'tEmissiveMap'; Value = 'Tex\Engineering\black.dds' },
                @{ Name = 'tSpecularMap'; Value = 'chars\skins\player_new_s.dds' },
                @{ Name = 'tNormalMap'; Value = 'chars\skins\player_new_n.dds' }
            )) {
                $writer.Write([UInt32]1)
                Write-AsciiString16 $writer $textureParameter.Name
                Write-AsciiString16 $writer $textureParameter.Value
            }
        }
        finally {
            $writer.Dispose()
        }
    }
    finally {
        $stream.Dispose()
    }
}

$sourceTexture = Join-Path $SourceGame 'chars\skins\player_new_d.dds'
if (-not (Test-Path -LiteralPath $sourceTexture -PathType Leaf)) {
    throw "Public-Tools player texture is missing: $sourceTexture"
}

$textureBytes = [IO.File]::ReadAllBytes($sourceTexture)
if ($textureBytes.Length -lt 128 -or
    [Text.Encoding]::ASCII.GetString($textureBytes, 0, 4) -ne 'DDS ') {
    throw "Player texture is not a DDS file: $sourceTexture"
}
$height = [BitConverter]::ToInt32($textureBytes, 12)
$width = [BitConverter]::ToInt32($textureBytes, 16)
$fourCc = [Text.Encoding]::ASCII.GetString($textureBytes, 84, 4)
if ($width -ne 1024 -or $height -ne 512 -or $fourCc -ne 'DXT3') {
    throw (
        "Unsupported player texture layout: ${width}x${height} $fourCc. " +
        'No body override was generated.')
}

$sourceModel = Join-Path $SourceGame 'chars\models\player.Model00p'
if (-not (Test-Path -LiteralPath $sourceModel -PathType Leaf)) {
    throw "Public-Tools player model is missing: $sourceModel"
}
$modelBytes = [IO.File]::ReadAllBytes($sourceModel)
if ($modelBytes.Length -lt 128 -or
    [Text.Encoding]::ASCII.GetString($modelBytes, 0, 5) -ne 'MODL!') {
    throw "Player model is not a FEAR Model00p file: $sourceModel"
}

$mesh = Find-PlayerMeshDirectory $modelBytes
$vertices = [object[]]::new($mesh.VertexCount)
for ($vertexIndex = 0; $vertexIndex -lt $mesh.VertexCount;
    ++$vertexIndex) {
    $vertexOffset = $mesh.VertexData + $vertexIndex * 64
    $vertices[$vertexIndex] = [pscustomobject]@{
        X = [BitConverter]::ToSingle($modelBytes, $vertexOffset)
        Y = [BitConverter]::ToSingle($modelBytes, $vertexOffset + 4)
        Z = [BitConverter]::ToSingle($modelBytes, $vertexOffset + 8)
        U = [BitConverter]::ToSingle($modelBytes, $vertexOffset + 24)
        V = [BitConverter]::ToSingle($modelBytes, $vertexOffset + 28)
    }
}

$indices = [int[]]::new($mesh.IndexCount)
for ($index = 0; $index -lt $mesh.IndexCount; ++$index) {
    $indices[$index] = [int][BitConverter]::ToUInt16(
        $modelBytes, $mesh.IndexData + $index * 2)
}

# Verbundene Mesh-Komponenten trennen. Body, Arme und Hände benutzen zwar
# dasselbe Texturatlas, sind im Modell aber jeweils getrennte Komponenten.
$parents = [int[]]::new($mesh.VertexCount)
$componentSizes = [int[]]::new($mesh.VertexCount)
for ($vertexIndex = 0; $vertexIndex -lt $mesh.VertexCount;
    ++$vertexIndex) {
    $parents[$vertexIndex] = $vertexIndex
    $componentSizes[$vertexIndex] = 1
}
for ($index = 0; $index -lt $mesh.IndexCount; $index += 3) {
    Join-DisjointSet $parents $componentSizes `
        $indices[$index] $indices[$index + 1]
    Join-DisjointSet $parents $componentSizes `
        $indices[$index] $indices[$index + 2]
}

$components = @{}
for ($vertexIndex = 0; $vertexIndex -lt $mesh.VertexCount;
    ++$vertexIndex) {
    $root = Get-DisjointSetRoot $parents $vertexIndex
    if (-not $components.ContainsKey($root)) {
        $components[$root] = [Collections.Generic.List[int]]::new()
    }
    $components[$root].Add($vertexIndex)
}

$armRoots = [Collections.Generic.HashSet[int]]::new()
$handRoots = [Collections.Generic.HashSet[int]]::new()
foreach ($entry in $components.GetEnumerator()) {
    $minimumX = [double]::MaxValue
    $maximumX = [double]::MinValue
    $minimumY = [double]::MaxValue
    $maximumY = [double]::MinValue
    foreach ($vertexIndex in $entry.Value) {
        $vertex = $vertices[$vertexIndex]
        $minimumX = [Math]::Min($minimumX, $vertex.X)
        $maximumX = [Math]::Max($maximumX, $vertex.X)
        $minimumY = [Math]::Min($minimumY, $vertex.Y)
        $maximumY = [Math]::Max($maximumY, $vertex.Y)
    }

    $liesOnOneSide = $minimumX -gt 15 -or $maximumX -lt -15
    $outerReach = [Math]::Max(
        [Math]::Abs($minimumX), [Math]::Abs($maximumX))
    if ($entry.Value.Count -ge 75 -and $liesOnOneSide -and
        $minimumY -gt -5 -and $maximumY -gt 50 -and
        $outerReach -gt 40) {
        [void]$armRoots.Add([int]$entry.Key)
    }

    # Alle Hand- und Handgelenk-Komponenten, auch die kleinen Naht-Meshes.
    if ($entry.Value.Count -ge 4 -and
        ($minimumX -gt 30 -or $maximumX -lt -30) -and
        $minimumY -gt -12 -and $maximumY -lt 12) {
        [void]$handRoots.Add([int]$entry.Key)
    }
}

if ($armRoots.Count -ne 6) {
    throw (
        "Expected six player arm meshes, found $($armRoots.Count). " +
        'No body override was generated.')
}
if ($handRoots.Count -lt 12) {
    throw (
        "Expected separate player hand meshes, found $($handRoots.Count). " +
        'No body override was generated.')
}

$armMask = [bool[]]::new($width * $height)
$handMask = [bool[]]::new($width * $height)
$armTriangleCount = 0
for ($index = 0; $index -lt $mesh.IndexCount; $index += 3) {
    $triangle = [int[]]@(
        $indices[$index],
        $indices[$index + 1],
        $indices[$index + 2]
    )
    $root = Get-DisjointSetRoot $parents $triangle[0]
    if ($armRoots.Contains($root)) {
        Set-TriangleMaskPixels `
            $vertices $triangle $width $height $armMask
        ++$armTriangleCount
    }
    if ($handRoots.Contains($root)) {
        Set-TriangleMaskPixels `
            $vertices $triangle $width $height $handMask
    }
}
if ($armTriangleCount -lt 700 -or $armTriangleCount -gt 900) {
    throw (
        "Unexpected player arm topology: $armTriangleCount triangles. " +
        'No body override was generated.')
}

# Zwei Pixel Sicherheitsrand verhindern Alpha-Test-Fransen bei bilinearer
# Filterung. Die folgende Prüfung garantiert, dass dieser Rand keine Hand-UV
# berührt.
$dilatedArmMask = [bool[]]$armMask.Clone()
for ($pixel = 0; $pixel -lt $armMask.Length; ++$pixel) {
    if (-not $armMask[$pixel]) {
        continue
    }
    $sourceX = $pixel % $width
    $sourceY = [int][Math]::Floor($pixel / $width)
    for ($offsetY = -2; $offsetY -le 2; ++$offsetY) {
        for ($offsetX = -2; $offsetX -le 2; ++$offsetX) {
            $targetX = (($sourceX + $offsetX) % $width + $width) % $width
            $targetY = (($sourceY + $offsetY) % $height + $height) % $height
            $dilatedArmMask[$targetY * $width + $targetX] = $true
        }
    }
}

$transparentPixelCount = 0
for ($pixel = 0; $pixel -lt $dilatedArmMask.Length; ++$pixel) {
    if (-not $dilatedArmMask[$pixel]) {
        continue
    }
    if ($handMask[$pixel]) {
        throw (
            "Generated arm mask overlaps a hand UV at pixel $pixel. " +
            'No body override was generated.')
    }
    ++$transparentPixelCount
}
if ($transparentPixelCount -lt 40000 -or
    $transparentPixelCount -gt 60000) {
    throw (
        "Unexpected player arm mask size: $transparentPixelCount pixels. " +
        'No body override was generated.')
}

$blocksWide = [int]($width / 4)
# Retails Solid-Shader ignoriert den Alpha-Kanal; die Quelldatei enthält dort
# deshalb großflächig Nullwerte. Der Alpha-Test-Override braucht eine explizite
# Grundlage: zunächst jedes DXT3-Pixel vollständig sichtbar machen, danach
# ausschließlich die Arm-Inseln ausstanzen.
$blockCount = [int](($width / 4) * ($height / 4))
for ($block = 0; $block -lt $blockCount; ++$block) {
    $alphaStart = 128 + $block * 16
    for ($alphaByte = 0; $alphaByte -lt 8; ++$alphaByte) {
        $textureBytes[$alphaStart + $alphaByte] = 0xFF
    }
}

for ($maskPixel = 0; $maskPixel -lt $dilatedArmMask.Length; ++$maskPixel) {
    if (-not $dilatedArmMask[$maskPixel]) {
        continue
    }
    $x = $maskPixel % $width
    $y = [int][Math]::Floor($maskPixel / $width)
    $block = [int](($y -shr 2) * $blocksWide + ($x -shr 2))
    $pixel = [int](($y -band 3) * 4 + ($x -band 3))
    $alphaOffset = 128 + $block * 16 + ($pixel -shr 1)
    if (($pixel -band 1) -eq 0) {
        $textureBytes[$alphaOffset] =
            $textureBytes[$alphaOffset] -band 0xF0
    }
    else {
        $textureBytes[$alphaOffset] =
            $textureBytes[$alphaOffset] -band 0x0F
    }
}

$textureDirectory = Join-Path $DestinationGame 'fearvr'
New-Item -ItemType Directory -Force -Path $textureDirectory |
    Out-Null

$destinationTexture = Join-Path $textureDirectory 'player_body_d.dds'
$destinationMaterial = Join-Path $textureDirectory 'player_body.Mat00'
[IO.File]::WriteAllBytes($destinationTexture, $textureBytes)
Write-PlayerBodyMaterial $destinationMaterial

# Die erste Fassung verliess sich auf das Ueberschreiben des Retail-Pfads.
# Die Archiv-Suchreihenfolge kann dann trotzdem das Originalmaterial liefern.
$legacyMaterial = Join-Path $DestinationGame 'chars\materials\player_new.Mat00'
if (Test-Path -LiteralPath $legacyMaterial -PathType Leaf) {
    Remove-Item -LiteralPath $legacyMaterial -Force
}

Write-Host (
    '  [OK] VR body material: torso/legs/hands visible, ' +
    "$transparentPixelCount arm pixels transparent")

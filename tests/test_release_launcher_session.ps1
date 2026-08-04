param(
    [string]$HelperPath
)

$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($HelperPath)) {
    $HelperPath =
        Join-Path $projectRoot 'tools\release\_fearvr-release.ps1'
}
. ([IO.Path]::GetFullPath($HelperPath))

$launcherSession =
    [Convert]::ToUInt64('08DEE33320DE5F5C', 16)
$nativeHeader = @'
{"time":"2026-07-31T02:55:13.111Z","level":"INFO","event":"proxy_start","message":"version=1.0.0-beta.7 git=3617e32 pid=7956 session=0x8DEE33320DE5F5C"}
'@

if (-not (Test-FearVrProxySessionHeader `
        $nativeHeader $launcherSession)) {
    throw 'A native session without the launcher leading zero was rejected.'
}
if (Test-FearVrProxySessionHeader `
        $nativeHeader ([uint64]($launcherSession + 1))) {
    throw 'A different proxy session was accepted.'
}
if (Test-FearVrProxySessionHeader 'not json' $launcherSession) {
    throw 'A malformed proxy header was accepted.'
}
if (Test-FearVrProxySessionHeader `
        '{"message":"proxy without session"}' $launcherSession) {
    throw 'A proxy header without a session was accepted.'
}

$publicToolsCandidates = @(
    Get-PublicToolsDefaultPaths -DriveRoots @('C:', 'D:'))
$expectedPublicToolsPaths = @(
    'C:\Program Files (x86)\Sierra\FEAR Public Tools',
    'D:\Program Files (x86)\Sierra\FEAR Public Tools',
    'D:\Sierra\FEAR Public Tools'
)
foreach ($expectedPath in $expectedPublicToolsPaths) {
    if ($expectedPath -notin $publicToolsCandidates) {
        throw "Public Tools search omits standard path: $expectedPath"
    }
}

$playScript = Join-Path $projectRoot 'tools\release\play.ps1'
$playText = [IO.File]::ReadAllText($playScript)
foreach ($requiredLauncherText in @(
    '[switch]$InternalTheaterGuard',
    'valve.steam.desktopgame.21090',
    '--compositorcmd disable_theater_mode',
    '$attemptOffsets = @(0, 3, 6)',
    '$deadline = $launchStarted.AddSeconds(9)'
)) {
    if (-not $playText.Contains($requiredLauncherText)) {
        throw "Release launcher omits: $requiredLauncherText"
    }
}
foreach ($removedHelper in @(
    'disable-steamvr-theater.ps1',
    'hide-steamvr-theater.ps1'
)) {
    if ($playText.Contains($removedHelper)) {
        throw "Standalone theater helper returned: $removedHelper"
    }
}

$testRoot = Join-Path ([IO.Path]::GetTempPath()) (
    'fearvr-ui-test-' + [Guid]::NewGuid().ToString('N'))
try {
    $sourceGame = Join-Path $testRoot 'source'
    $destinationGame = Join-Path $testRoot 'destination'
    $relativeRecord =
        'DatabaseLocalized\Interface\HUD\HUDSwap.record'
    $relativeDatabase =
        'DatabaseLocalized\FEARL.Gamdb00p'
    $sourceRecord = Join-Path $sourceGame $relativeRecord
    New-Item -ItemType Directory -Force -Path (
        Split-Path -Parent $sourceRecord) | Out-Null
    @'
[Record]
Name=HUDSwap

[Attrib.TextSize]
Value.0000=12

[Attrib.TextColor]
Value.0000=160, 227, 245, 242

[Attrib.AdditionalColor]
Value.0000=255, 19, 236, 30
'@ | Out-File -LiteralPath $sourceRecord -Encoding ascii

    # Minimale synthetische Packdatei mit denselben eindeutigen Feldern wie
    # die Public-Tools-1.08-Datenbank. So bleibt der Test frei von
    # proprietaeren Testartefakten.
    [byte[]]$packedDatabase = @(
        0xAA, 0xBB,
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
        0x0A, 0x00, 0x00, 0x00,
        0xCC,
        0x1E, 0xEC, 0x13, 0xFF,
        0xEE
    )
    $sourceDatabase = Join-Path $sourceGame $relativeDatabase
    New-Item -ItemType Directory -Force -Path (
        Split-Path -Parent $sourceDatabase) | Out-Null
    [IO.File]::WriteAllBytes($sourceDatabase, $packedDatabase)

    & (Join-Path $projectRoot 'tools\release\new-vr-ui-assets.ps1') `
        -SourceGame $sourceGame `
        -DestinationGame $destinationGame
    $generated = [IO.File]::ReadAllBytes(
        (Join-Path $destinationGame $relativeDatabase))
    $generatedHex = [BitConverter]::ToString($generated).Replace('-', '')
    foreach ($requiredPatch in @(
        '0300000000000000010000001A000000F40300000A000000',
        'FFFFFFFF',
        '50E0FFFF'
    )) {
        if (-not $generatedHex.Contains($requiredPatch)) {
            throw "VR pickup-prompt database omits patch: $requiredPatch"
        }
    }
    foreach ($obsoleteValue in @('F2F5E3A0', '1EEC13FF')) {
        if ($generatedHex.Contains($obsoleteValue)) {
            throw "VR pickup-prompt database retains: $obsoleteValue"
        }
    }
} finally {
    if (Test-Path -LiteralPath $testRoot -PathType Container) {
        Remove-Item -LiteralPath $testRoot -Recurse -Force
    }
}

# Die HUD-Shader werden absichtlich erst zur Laufzeit kompiliert. Ein normaler
# C++-Build entdeckt daher nicht, wenn eine neue Kontur das alte ps_2_0-Limit
# von 64 Arithmetik-Slots überschreitet. Genau das soll dieser Test abfangen.
$bridgeText = [IO.File]::ReadAllText(
    (Join-Path $projectRoot 'src\proxy32\bridge.cpp'))
$shaderMatch = [Text.RegularExpressions.Regex]::Match(
    $bridgeText,
    '(?s)constexpr char kHudCompositeShader\[\] = R"\((.*?)\)";')
if (-not $shaderMatch.Success) {
    throw 'GPU HUD composite shader source was not found.'
}
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class FearVrHudShaderTest
{
    [DllImport("d3dcompiler_47.dll", CallingConvention = CallingConvention.StdCall)]
    private static extern int D3DCompile(
        IntPtr data, UIntPtr size, string name, IntPtr defines,
        IntPtr include, string entry, string target, uint flags1,
        uint flags2, out IntPtr code, out IntPtr errors);

    public static bool CompileShaderModel2(string source)
    {
        byte[] bytes = System.Text.Encoding.ASCII.GetBytes(source);
        GCHandle pin = GCHandle.Alloc(bytes, GCHandleType.Pinned);
        IntPtr code = IntPtr.Zero;
        IntPtr errors = IntPtr.Zero;
        try
        {
            int result = D3DCompile(
                pin.AddrOfPinnedObject(), (UIntPtr)bytes.Length,
                "fearvr_stereo_hud", IntPtr.Zero, IntPtr.Zero,
                "main", "ps_2_0", 0, 0, out code, out errors);
            return result >= 0 && code != IntPtr.Zero;
        }
        finally
        {
            if (code != IntPtr.Zero) Marshal.Release(code);
            if (errors != IntPtr.Zero) Marshal.Release(errors);
            pin.Free();
        }
    }
}
'@
if (-not [FearVrHudShaderTest]::CompileShaderModel2(
        $shaderMatch.Groups[1].Value)) {
    throw 'GPU HUD composite shader exceeds or violates ps_2_0.'
}

Write-Host 'release launcher session matching passed'

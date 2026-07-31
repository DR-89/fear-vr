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

Write-Host 'release launcher session matching passed'

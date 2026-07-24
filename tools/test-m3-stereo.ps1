<#
.SYNOPSIS
    Testet den M3-Stereo-Transport isoliert mit verschiedenfarbigen Augen.

.DESCRIPTION
    Verwendet die sichere M2-Test-Stage, rendert links rot und rechts blau
    und verlangt die M3-Ereignisse in Proxy und Host. Retail bleibt
    unverändert.
#>
[CmdletBinding()]
param(
    [ValidateRange(120, 10000)]
    [int]$Frames = 600,

    [switch]$ClassicD3D9
)

$arguments = @{
    Frames = $Frames
    Stereo = $true
}
if ($ClassicD3D9) {
    $arguments.ClassicD3D9 = $true
}

& "$PSScriptRoot\test-m2-bridge.ps1" @arguments
exit $LASTEXITCODE

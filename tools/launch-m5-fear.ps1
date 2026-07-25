<#
.SYNOPSIS
    Startet M5 mit semantischer OpenXR-Controllersteuerung.
#>
[CmdletBinding()]
param(
    [switch]$Translation,

    [switch]$NoStereoHud,

    [switch]$NoHeadBob,

    [switch]$Wait
)

& "$PSScriptRoot\launch-m2-fear.ps1" -Milestone M5 `
    -Translation:$Translation -StereoHud:(-not $NoStereoHud) `
    -NoHeadBob:$NoHeadBob -Wait:$Wait

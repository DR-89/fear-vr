<#
.SYNOPSIS
    Startet den M4-Headtracking-Pfad mit F.E.A.R. über Steam.
#>
[CmdletBinding()]
param(
    [switch]$Translation,

    [switch]$NoStereoHud,

    [switch]$NoHeadBob,

    [switch]$Wait
)

& "$PSScriptRoot\launch-m2-fear.ps1" -Milestone M4 `
    -Translation:$Translation -StereoHud:(-not $NoStereoHud) `
    -NoHeadBob:$NoHeadBob -Wait:$Wait

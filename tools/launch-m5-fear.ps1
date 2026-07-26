<#
.SYNOPSIS
    Startet M5 mit semantischer OpenXR-Controllersteuerung.

.PARAMETER Runtime
    OpenXR-Runtime: active (Standard), steamvr, vdxr oder ein Manifestpfad.
    Erzwungen wird über XR_RUNTIME_JSON, nur für den Hostprozess.
#>
[CmdletBinding()]
param(
    [switch]$Translation,

    [switch]$NoStereoHud,

    [switch]$NoHeadBob,

    [switch]$NoGpuHud,

    [string]$Runtime = 'active',

    [switch]$Wait
)

& "$PSScriptRoot\launch-m2-fear.ps1" -Milestone M5 `
    -Translation:$Translation -StereoHud:(-not $NoStereoHud) `
    -NoHeadBob:$NoHeadBob -NoGpuHud:$NoGpuHud `
    -Runtime $Runtime -Wait:$Wait

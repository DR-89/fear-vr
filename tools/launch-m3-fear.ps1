<#
.SYNOPSIS
    Startet den nativen M3-Stereopfad mit F.E.A.R. über Steam.
#>
[CmdletBinding()]
param(
    [switch]$Wait
)

& "$PSScriptRoot\launch-m2-fear.ps1" -Milestone M3 -Wait:$Wait

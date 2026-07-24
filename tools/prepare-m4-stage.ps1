<#
.SYNOPSIS
    Erstellt die Retail-schonende M4-Headtracking-Stage.
#>
[CmdletBinding()]
param()

& "$PSScriptRoot\prepare-m2-stage.ps1" -Milestone M4

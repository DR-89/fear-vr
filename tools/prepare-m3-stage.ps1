<#
.SYNOPSIS
    Erstellt die Retail-schonende M3-Stage.
#>
[CmdletBinding()]
param()

& "$PSScriptRoot\prepare-m2-stage.ps1" -Milestone M3

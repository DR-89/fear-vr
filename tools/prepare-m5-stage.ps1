<#
.SYNOPSIS
    Erstellt die Retail-schonende M5-OpenXR-Eingabe-Stage.
#>
[CmdletBinding()]
param()

& "$PSScriptRoot\prepare-m2-stage.ps1" -Milestone M5

<#
.SYNOPSIS
    Applies the minimal source fixes needed to build F.E.A.R. Public Tools 1.08
    with the modern MSVC toolchain.

.DESCRIPTION
    The official SDK source remains local and is never copied into this
    repository. This script performs small, deterministic transformations on a
    user-supplied SDK source tree. Every transformation accepts exactly one of
    two states:

      1. the original SDK text, which is changed once; or
      2. the expected patched text, which is left untouched.

    Any third state fails closed so that an unknown SDK revision is not edited.

.PARAMETER SdkSource
    Path to the Public Tools "Source" directory.

.PARAMETER VerifyOnly
    Do not edit. Fail unless every expected fix is already present.
#>
[CmdletBinding(SupportsShouldProcess = $true)]
param(
    [string]$SdkSource,
    [switch]$VerifyOnly
)

$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($SdkSource)) {
    $SdkSource = Join-Path $PSScriptRoot '..\vendor-local\publictools\Source'
}
$sourceRoot = [IO.Path]::GetFullPath($SdkSource)

function Get-LiteralCount {
    param(
        [Parameter(Mandatory = $true)][string]$Text,
        [Parameter(Mandatory = $true)][string]$Needle
    )

    $count = 0
    $offset = 0
    while ($true) {
        $index = $Text.IndexOf($Needle, $offset, [StringComparison]::Ordinal)
        if ($index -lt 0) {
            return $count
        }

        $count++
        $offset = $index + $Needle.Length
    }
}

function Get-SourcePath {
    param([Parameter(Mandatory = $true)][string]$RelativePath)

    $path = [IO.Path]::GetFullPath((Join-Path $sourceRoot $RelativePath))
    $rootWithSeparator = $sourceRoot.TrimEnd('\') + '\'
    if (-not $path.StartsWith($rootWithSeparator, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Path escapes the SDK source root: $RelativePath"
    }
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required SDK source file is missing: $path"
    }
    return $path
}

function Write-SourceText {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Text
    )

    # The affected SDK files are ASCII-compatible Windows source files.
    # Encoding.Default preserves their original Windows code page.
    [IO.File]::WriteAllText($Path, $Text, [Text.Encoding]::Default)
}

function Ensure-LiteralReplacement {
    param(
        [Parameter(Mandatory = $true)][string]$RelativePath,
        [Parameter(Mandatory = $true)][string]$Original,
        [Parameter(Mandatory = $true)][string]$Patched,
        [int]$ExpectedCount = 1,
        [Parameter(Mandatory = $true)][string]$Description
    )

    $path = Get-SourcePath $RelativePath
    $text = [IO.File]::ReadAllText($path, [Text.Encoding]::Default)
    $originalCount = Get-LiteralCount $text $Original
    $patchedCount = Get-LiteralCount $text $Patched

    if (($originalCount -eq 0) -and ($patchedCount -eq $ExpectedCount)) {
        Write-Host ("[PASS] {0}" -f $Description)
        return
    }

    if (($originalCount -ne $ExpectedCount) -or ($patchedCount -ne 0)) {
        throw ("Unexpected source state for {0}: original={1}, patched={2}, expected={3}. File: {4}" -f
            $Description, $originalCount, $patchedCount, $ExpectedCount, $path)
    }

    if ($VerifyOnly) {
        throw "Fix is not applied: $Description ($path)"
    }

    if ($PSCmdlet.ShouldProcess($path, $Description)) {
        Write-SourceText $path ($text.Replace($Original, $Patched))
        Write-Host ("[EDIT] {0}" -f $Description)
    }
}

function Ensure-CTypeInclude {
    $relativePath = 'engine\sdk\inc\ltstrutils.h'
    $path = Get-SourcePath $relativePath
    $text = [IO.File]::ReadAllText($path, [Text.Encoding]::Default)
    $marker = '// F.E.A.R-VR build fix: modern UCRT no longer includes <ctype.h> transitively;'

    if ((Get-LiteralCount $text $marker) -eq 1) {
        Write-Host '[PASS] Explicit ctype.h include'
        return
    }
    if ((Get-LiteralCount $text $marker) -ne 0) {
        throw "Unexpected ctype.h marker count in $path"
    }
    if ($VerifyOnly) {
        throw "Fix is not applied: explicit ctype.h include ($path)"
    }

    $anchor = '#ifndef __STDLIB_H__'
    if ((Get-LiteralCount $text $anchor) -ne 1) {
        throw "Expected stdlib include anchor exactly once in $path"
    }

    $newline = if ($text.Contains("`r`n")) { "`r`n" } else { "`n" }
    $block = @(
        '// F.E.A.R-VR build fix: modern UCRT no longer includes <ctype.h> transitively;'
        '// toupper/tolower (LTStrUpr/LTStrLwr below) need it explicitly.'
        '#ifndef __CTYPE_H__'
        '#include <ctype.h>'
        '#define __CTYPE_H__'
        '#endif'
        ''
    ) -join $newline

    if ($PSCmdlet.ShouldProcess($path, 'Add explicit ctype.h include')) {
        Write-SourceText $path ($text.Replace($anchor, $block + $anchor))
        Write-Host '[EDIT] Explicit ctype.h include'
    }
}

if (-not (Test-Path -LiteralPath (Join-Path $sourceRoot 'Game\Game.sln') -PathType Leaf)) {
    throw "Not a F.E.A.R. Public Tools Source tree (Game\Game.sln missing): $sourceRoot"
}

Write-Host "SDK source: $sourceRoot"
Write-Host ("Mode: {0}" -f $(if ($VerifyOnly) { 'verify only' } else { 'apply idempotently' }))

Ensure-CTypeInclude

Ensure-LiteralReplacement `
    -RelativePath 'Game\ClientFxDLL\dynalightfx.cpp' `
    -Original 'Texture Animation Files (*."RESEXT_TEXTUREANIM")|*."RESEXT_TEXTUREANIM"|All Files' `
    -Patched 'Texture Animation Files (*." RESEXT_TEXTUREANIM ")|*." RESEXT_TEXTUREANIM "|All Files' `
    -Description 'Separate texture-animation macro tokens'

$polyOriginal = 'for(uint32 nValidNode = 0; nValidNode < m_nNumTrackedNodes; nValidNode++)'
$polyPatched = "uint32 nValidNode = 0;`r`n`t`t`tfor(; nValidNode < m_nNumTrackedNodes; nValidNode++)"
$polyPath = Get-SourcePath 'Game\ClientFxDLL\PolyTrailFX.cpp'
$polyText = [IO.File]::ReadAllText($polyPath, [Text.Encoding]::Default)
if (-not $polyText.Contains("`r`n")) {
    $polyPatched = $polyPatched.Replace("`r`n", "`n")
}
Ensure-LiteralReplacement `
    -RelativePath 'Game\ClientFxDLL\PolyTrailFX.cpp' `
    -Original $polyOriginal `
    -Patched $polyPatched `
    -Description 'Hoist nValidNode out of legacy for-scope'

Ensure-LiteralReplacement `
    -RelativePath 'Game\ObjectDLL\AI.cpp' `
    -Original '"."RESEXT_MATERIAL' `
    -Patched '"." RESEXT_MATERIAL' `
    -ExpectedCount 2 `
    -Description 'Separate material-extension macro tokens'

Ensure-LiteralReplacement `
    -RelativePath 'Game\ObjectDLL\ServerSpecialFX.cpp' `
    -Original '*."RESEXT_EFFECT_PACKED' `
    -Patched '*." RESEXT_EFFECT_PACKED' `
    -Description 'Separate packed-effect macro token'

Ensure-LiteralReplacement `
    -RelativePath 'Game\ClientShellDLL\HUDChatInput.cpp' `
    -Original "m_szChatHistory[i][0] = L'';" `
    -Patched "m_szChatHistory[i][0] = L'\0';" `
    -Description 'Replace invalid empty wide character literal'

Ensure-LiteralReplacement `
    -RelativePath 'Game\ClientShellDLL\Game_ClientShell.rc' `
    -Original '#include "afxres.h"' `
    -Patched '#include "winres.h"' `
    -Description 'Use Windows SDK resource header for GameClient'

Ensure-LiteralReplacement `
    -RelativePath 'Game\ObjectDLL\Game_ServerShell.rc' `
    -Original '#include "afxres.h"' `
    -Patched '#include "winres.h"' `
    -Description 'Use Windows SDK resource header for GameServer'

Ensure-LiteralReplacement `
    -RelativePath 'Game\ClientShellDLL\ModelDecalMgr.h' `
    -Original 'inline uint32 GetDecalType' `
    -Patched 'uint32 GetDecalType' `
    -ExpectedCount 2 `
    -Description 'Emit externally linked GetDecalType overloads'

Write-Host 'All Public Tools source fixes are in the expected state.'

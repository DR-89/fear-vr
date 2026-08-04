# Graphical installer

This directory contains an Inno Setup 6 wrapper for the current F.E.A.R. VR
`retail-overlay` release package.

The generated installer asks for a legal F.E.A.R. 1.08 installation and a
F.E.A.R. Public Tools 1.08 installation. It then copies the overlay directly
into the folder containing `FEAR.exe`, runs
`FEARVR\tools\prepare-overlay.ps1`, and optionally creates a desktop shortcut.
It does not replace `FEAR.exe` or the retail game archives. Existing root-level
`dinput8.dll` and `d3d9.dll` proxy mods are replaced by F.E.A.R. VR's proxies.

Gameplay and comfort settings remain available through the in-game VR menu and
`fearvr.ini` where applicable.

## Prerequisites

Install Inno Setup 6. The build script automatically checks these locations:

```text
C:\Program Files (x86)\Inno Setup 6\ISCC.exe
C:\Program Files\Inno Setup 6\ISCC.exe
%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe
```

## Build

First create an unpacked public retail-overlay package:

```powershell
pwsh -File tools\make-release.ps1 -NoArchive
```

Then build the installer from the generated directory below `dist`:

```powershell
pwsh -File tools\build-graphical-installer.ps1 `
  -PackageDir "dist\fearvr-<version>+<commit>-overlay"
```

The result is `dist\FearVR-Setup-<version>.exe`. `-OutputDir`, `-Version`, and
`-InnoSetupCompiler` may be supplied when non-default values are needed.

The build script rejects legacy staged-install packages and incomplete overlay
packages before invoking Inno Setup.

## Installation flow

The setup program validates:

1. `FEAR.exe` and `Default.archcfg` in the selected retail directory.
2. `GameClient.dll` in the selected Public Tools directory or its
   `Dev\Runtime\Game` child.
3. The packaged overlay manifest and required root proxy files.

After extraction, it runs the package's preparation script with explicit
`-InstallDir`, `-RetailRoot`, `-PublicToolsGame`, and `-Force` arguments. That
script verifies the release manifest, copies the proprietary modules only from
the user's local Public Tools installation, and creates the isolated runtime
configuration below `FEARVR`.

Before publishing, clean-install and update testing should still be performed
on each supported Windows/store combination. Code signing and project artwork
can be added independently of the packaging format.

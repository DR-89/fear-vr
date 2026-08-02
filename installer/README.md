# Graphical installer

This directory contains an Inno Setup wrapper around the existing
`tools/install.ps1` workflow.

The graphical installer does **not** reimplement the installation logic and does
not expose gameplay settings. It only gathers installation paths and options,
then invokes `install.ps1` in non-interactive mode.

## User experience

The generated installer guides users through:

1. Locating the retail F.E.A.R. 1.08 installation.
2. Locating F.E.A.R. Public Tools 1.08.
3. Choosing a separate F.E.A.R. VR installation folder.
4. Choosing whether to create a desktop shortcut.
5. Optionally cleaning an existing staged installation.
6. Running the existing installation script and displaying its output.

Gameplay and comfort options remain available through the in-game VR settings
menu and `fearvr.ini` where applicable.

## Prerequisites for maintainers

Install Inno Setup 6 so `ISCC.exe` is available. The default locations checked
by the build script are:

```text
C:\Program Files (x86)\Inno Setup 6\ISCC.exe
C:\Program Files\Inno Setup 6\ISCC.exe
```

## Build the installer

First create a normal release package using the existing release process:

```powershell
pwsh -File tools\make-release.ps1
```

Then point the graphical-installer build script at the prepared package folder:

```powershell
pwsh -File tools\build-graphical-installer.ps1 `
  -PackageDir "<path-to-prepared-release-package>"
```

The generated executable is written to `dist` by default:

```text
dist\FearVR-Setup-<version>.exe
```

Use a custom output directory when needed:

```powershell
pwsh -File tools\build-graphical-installer.ps1 `
  -PackageDir "<path-to-prepared-release-package>" `
  -OutputDir "D:\Builds\FearVR"
```

Pass the compiler path explicitly when Inno Setup is installed elsewhere:

```powershell
pwsh -File tools\build-graphical-installer.ps1 `
  -PackageDir "<path-to-prepared-release-package>" `
  -InnoSetupCompiler "D:\Tools\Inno Setup 6\ISCC.exe"
```

## Architecture

The final setup executable embeds the prepared release package in a temporary
folder. During installation it runs:

```powershell
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass `
  -File tools\install.ps1 `
  -RetailRoot "<selected-retail-folder>" `
  -PublicToolsGame "<selected-public-tools-folder>" `
  -InstallDir "<selected-install-folder>" `
  -NonInteractive
```

Depending on the wizard selections, it may also pass:

```text
-NoShortcut
-Clean
```

The retail game folder remains unmodified. The existing PowerShell installer
continues to own validation, package integrity checks, staging, update handling,
and shortcut creation.

## Recommended follow-up work

Before publishing the graphical installer as the default release path:

- Confirm the exact output directory produced by `make-release.ps1` and optionally
  make `build-graphical-installer.ps1` discover it automatically.
- Test clean installs and updates on Windows 10 and Windows 11.
- Test Steam, GOG, and retail-disc paths where available.
- Add project artwork and an application icon.
- Add code signing for the setup executable when a certificate is available.
- Add a GitHub Actions job that installs Inno Setup and produces the installer
  as a release artifact.

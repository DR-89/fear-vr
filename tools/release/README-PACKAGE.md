# F.E.A.R. VR – Overlay release

This archive is installed by extracting it **directly over an existing legal
F.E.A.R. 1.08 installation**. It adds a `FEARVR` folder, two launchers and
F.E.A.R. VR's own `dinput8.dll` and `d3d9.dll`; it does not replace `FEAR.exe`
or any retail archive. The DirectInput proxy fixes the original game's
accumulating HID slowdown. The D3D9 proxy activates the low-latency
asynchronous frame transport before the game creates its renderer.

If the game folder already contains a third-party `d3d9.dll`, preserve it
before extracting the archive. The development installer chains such a wrapper
as `d3d9.fearvr-upstream.dll`, but a plain ZIP extraction cannot back up a file
that it overwrites.

## Install and play

1. Extract the archive into the folder that contains `FEAR.exe`.
2. Start the headset runtime.
3. Double-click `Start F.E.A.R. VR.cmd`.

To force Valve's runtime, use
`Start F.E.A.R. VR - SteamVR.cmd`. The regular launcher uses the system's
active OpenXR runtime. Both launchers run the same 64-bit OpenXR host; SteamVR
is an OpenXR runtime and therefore does not require a separate renderer build.

The overlay keeps logs below `FEARVR\`. Saves, profiles and configuration use
the writable per-user directory
`%LOCALAPPDATA%\F.E.A.R. VR\userdata`; older data from `FEARVR\userdata` is
copied there once without overwriting newer files. This allows the retail game
to persist display settings even when Steam is installed below Program Files.
Updating is the same operation: extract the newer archive over the same game
folder.
The launcher retries a Steam app hand-off that produced no `FEAR.exe` and
then falls back to the verified executable directly while Steam remains
running. It only reports a successful start after the matching VR bridge has
loaded.
Startup diagnostics are written as `launcher-*.log` beside the host and proxy
logs for that run.

## Public and private archives

The normal release contains only redistributable F.E.A.R.-VR files. On its
first start it finds the owner's local **F.E.A.R. Public Tools 1.08**
installation and copies the five required runtime modules into `FEARVR`.
It checks registry entries and common folders on every local drive, including
`C:\Program Files (x86)\Sierra\FEAR Public Tools`. If nothing valid is found,
the visible launcher explicitly asks for the installation folder and verifies
the selected Public Tools 1.08 files before continuing.

If they are installed at a custom location:

```powershell
.\Start F.E.A.R. VR.cmd -PublicToolsGame "D:\FEAR Public Tools\Dev\Runtime\Game"
```

The official installer `fear_publictools_108.exe` is included with the Steam
Ultimate Shooter Edition under `extras\`. Its EULA does not grant permission
to redistribute the proprietary runtime modules.

A developer can create an immediately playable personal archive from a local
Public Tools installation:

```powershell
pwsh -File tools\make-release.ps1 -PrivateBundle
```

That private archive contains the proprietary modules, generated body assets
and the local x64 `d3dx10_43.dll` legacy DirectX redistributable required by
SteamVR's `vrmonitor.exe` on systems with an incomplete Steam prerequisite
installation. The DLL is exposed only through the SteamVR child-process path;
no file is copied into Windows or SteamVR. The archive is marked
`NOT FOR REDISTRIBUTION` and must not be published.

## Runtime selection

```powershell
.\Start F.E.A.R. VR.cmd -Runtime steamvr
.\Start F.E.A.R. VR.cmd -Runtime vdxr
.\Start F.E.A.R. VR.cmd -Runtime "D:\Runtime\manifest.json"
```

Runtime selection is process-local through `XR_RUNTIME_JSON`; no global OpenXR
setting is changed.

## Controls

- Right trigger fires the right-hand pistol.
- With a second pistol equipped, left trigger fires the left-hand pistol and
  X toggles slow motion. Otherwise X controls the selected light.
- Grip supports a two-handed weapon at its original foregrip location.
- The wrist HUD appears above the support wrist when its inner side is viewed.
- Lowering the headset crouches physically when `Physical duck` is enabled;
  stick crouch always remains available.
- VR options, including light placement, handedness and physical duck, are under
  `ESC > VR SETTINGS`.

Mouse, keyboard and gamepad remain usable in parallel.

## Removal

Delete the added `dinput8.dll`, `d3d9.dll`, `FEARVR` folder and the two
`Start F.E.A.R. VR*.cmd` launchers. Original retail files are not replaced, so
a Steam file verification is unnecessary.

## License

F.E.A.R.-VR's own source and binaries are under the MIT license; see
`FEARVR\LICENSE` and `FEARVR\THIRD_PARTY_NOTICES.md`. F.E.A.R. and the Public
Tools remain subject to their owners' licenses.

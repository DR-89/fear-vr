# F.E.A.R. VR

Open-source, locally buildable VR mod for the **single-player base version of
F.E.A.R. 1.08** (`FEAR.exe`, LithTech Jupiter EX, Direct3D 9).

> **Note:**
> This repo was created entirely with AI assistance. I've been a software
> developer for 11 years, but something like this is beyond me without AI.
> I guided the AI to the best of my knowledge. Nevertheless, the AI will
> certainly have made mistakes. PRs are welcome — help improve the mod!

> **Status:** M6 (Packaging and regression). M5 is complete and confirmed
> in-game: native stereo world rendering, relative HMD headtracking,
> F9 recenter, readable world-locked menu and stereo HUD are all confirmed
> with real F.E.A.R. on Quest 3/SteamVR. OpenXR controllers fully drive the
> game: movement, turning, weapon selection, jumping, reloading, crouching,
> slow-mo, sprinting, use, aim/fire, recenter and pause menu; leaning uses
> left-hand tilt. First-person view shows only hands and weapon, and the
> ESC menu includes a native VR settings page. The classic D3D9 path still
> uses a flagged CPU compatibility fallback; translation remains opt-in
> without world collision. Details: `docs/TESTING.md`.

## Demo

[![F.E.A.R. VR in action](https://img.youtube.com/vi/QTsNeLT8Pn8/maxresdefault.jpg)](https://youtu.be/QTsNeLT8Pn8)

▶️ **Watch the demo on YouTube:** https://youtu.be/QTsNeLT8Pn8

## Features

Everything listed here is implemented and has run in the actual game. Where
something is built but not yet verified in-game, it's noted.

### Rendering

- **Native stereo world rendering.** The LithTech camera renders twice per
  frame with its own per-eye matrix — not a duplicated mono image. Activates
  automatically after loading; F8 toggles at any time.
- **Relative headtracking.** HMD rotation rotates the game camera relative
  to the neutral pose; pitch and roll are preserved. F9 or right stick click
  sets the current gaze direction as the neutral pose.
- **Optional HMD translation** up to 25 cm (`-Translation`). Without world
  collision, deliberately opt-in.
- **Stereo HUD.** Ammo, health and hints are overlaid into both eyes instead
  of flat over the left image. The HUD is evenly compressed toward the screen
  center (5/4) so edge elements stay within the comfortable field of view.
- **Flat-screen mode for fullscreen UI.** Menus, loading screens, movies and
  the mission briefing appear as a world-locked 2.4 × 1.8 m panel at 2 m
  distance. Detection is based on the retail game state
  `CInterfaceMgr::m_eGameState`, not pixel heuristics. Right stick click
  re-anchors the panel to the current gaze direction.
- **Comfort screen (F10).** World-locked rendering for camera shakes and
  cutscenes, so forced camera movement doesn't pull at the head.
- **Quiet camera.** Weapon bob and camera recoil are disabled; head bob is
  off by default and only toggleable via `fearvr.ini`.
- **First-person body corrected.** Upper and lower arms are hidden; hands
  and weapon remain visible. The classic crosshair is off, since the red
  aim laser takes over its role.

### Motion Controls

- **Full game control via OpenXR controllers** — move, turn, jump, crouch,
  sprint, weapon switch, reload, grenade, slow-mo, use, aim, fire, pause and
  recenter. Mouse, keyboard and gamepad remain usable in parallel.
- **Weapon follows the right hand.** Shot origin and fire vectors come from
  the muzzle transform, not from gaze direction.
- **Red aim laser** from the muzzle, toggleable on/off.
- **Point instead of look.** Activate and pick-up follow the weapon laser
  with approx. 1.5 m range, instead of head direction.
- **Hand flashlight in the left hand.** It follows the hand's position and aim
  direction and toggles with a click on the left trigger. The second,
  non-toggleable retail flashlight is removed.
- **Leaning via hand tilt.** Tilting the left hand sideways leans around
  corners; inverted and hanging hands are handled.
- **Haptics per shot**, including full-auto — triggered on the retail shot,
  not on the trigger edge. Empty magazine = no vibration.

### Menu and Settings

- **Native VR settings page** in the ESC menu ("VR SETTINGS"), usable with
  the controller: Stereo rendering, Stereo HUD, Turn speed, Red aim
  guide, Controller vibration, Recenter view, Reset VR defaults.
- **Persistence in `fearvr.ini`**, including settings not exposed in the menu:
  HMD translation, head bob and comfort screen.
- **F11 calibration** for player body pieces, in case the default arm piece
  doesn't fit. The result is saved immediately.

### Operation

- **Two processes by bitness:** x64 OpenXR host and x86 D3D9 bridge over a
  versioned shared-memory protocol with frame ring and heartbeat monitoring.
  If the host crashes, the game continues flat.
- **Runtime selection at launch:** SteamVR and VirtualDesktopXR are confirmed;
  `-Runtime` sets `XR_RUNTIME_JSON` only for the host process.
- **SteamVR Desktop Theater** is automatically disabled and monitored; under
  VDXR this is skipped entirely.
- **Structured JSON logs** for host and bridge with perf counters; each run
  writes to its own directory under `logs\`.
- **Version-bound with fail-safe.** All retail hooks check timestamp, image
  size and expected byte patterns. If something doesn't match, the hook stays
  disabled and the game continues. Diagnostic switches like `-fearvr-safe`,
  `-fearvr-no-interaction` or `-fearvr-no-gamestate` are available.
- **Distributable package** (`tools\make-release.ps1`) with installer, desktop
  shortcut and uninstaller. It contains only our own MIT-licensed binaries;
  the proprietary modules are pulled from the local Public Tools installation.

## Core Principles

- **Retail stays untouched.** Nothing is written into the Steam installation
  and no original EXE/DLL/archive file is overwritten. All work happens in an
  isolated stage under the project root (`stage/`) with its own
  `-userdirectory`.
- **No retail/SDK/asset files in Git.** See `.gitignore`.
- **Separate processes by bitness:** an x64 OpenXR host owns the OpenXR
  session; the x86 `FEAR.exe` renders via a `d3d9.dll` bridge and a locally
  rebuilt GameClient module. Reason: the 32-bit OpenXR runtime registry entry
  is missing on this machine (see `docs/ENVIRONMENT.md`).

## Architecture (Overview)

```text
SteamVR / OpenXR (x64)
   ^  OpenXR + XR_KHR_D3D11_enable
fearvr-host.exe (x64, D3D11)
   ^  versioned IPC (poses, FOV, shared texture handles)
FEAR.exe (x86, D3D9) + d3d9.dll bridge + GameClient module (x86)
   v  LithTech RenderCamera, twice per frame
```

Details: `docs/ARCHITECTURE.md`.

## Repository Structure

| Path | Contents |
|---|---|
| `docs/` | Environment, architecture, coordinates, stereo research, tests |
| `src/common/` | shared IPC contract (`protocol.h`) + math |
| `src/host64/` | x64 OpenXR host (`fearvr-host.exe`) |
| `src/proxy32/` | x86 D3D9 proxy/bridge |
| `src/gameclient_loader/` | ABI-neutral loader for the real `archcfg` stage |
| `src/launcher/` | Launcher (starts host, then isolated `FEAR.exe`) |
| `game-source-overlay/` | only **newly written** GameClient project files |
| `patches/` | minimal, license-checked diffs / transform scripts |
| `shaders/` | host fullscreen/composite shaders |
| `tests/` | automated tests (protocol, math, state machine …) |
| `tools/` | `verify-install.ps1`, `prepare-m5-stage.ps1`, `launch-m5-fear.ps1` … |
| `vendor-local/`, `build/`, `stage/`, `logs/` | local, **not** in Git |

## Prerequisites

See `docs/ENVIRONMENT.md` for the verified current state and still-missing
components. In short:

- F.E.A.R. 1.08 (Ultimate Shooter Edition), legally installed
- Visual Studio 2022 with "Desktop Development with C++" (+ v141 toolset for
  compile/source analysis; runtime Public Tools modules need VC7.1)
- CMake, Git
- SteamVR as active OpenXR runtime + headset
- Local official Public Tools installer 1.08

## Quick Start (Verify Environment)

```bash
pwsh -File tools/verify-install.ps1
```

Checks retail path, `FEAR.exe` hash/version, OpenXR runtime, registry and
available build tools, and reports missing components — without changing
anything.

## Build

One command checks pinned dependencies, builds x86 and x64, runs both test
suites and writes `stage\build-manifest.json` with SHA-256 sums of all
artifacts:

```powershell
pwsh -File tools\build-all.ps1
```

Individual builds are still possible; x86 (proxy) and x64 (host) are built
**separately**:

```powershell
pwsh -File tools\prepare-dependencies.ps1

cmake -S . -B build\x86 -A Win32 -DFEARVR_BUILD_PROXY=ON -DFEARVR_BUILD_HOST=OFF
cmake --build build\x86 --config RelWithDebInfo

cmake -S . -B build\x64 -A x64 -DFEARVR_BUILD_PROXY=OFF -DFEARVR_BUILD_HOST=ON
cmake --build build\x64 --config RelWithDebInfo
```

`-G "Visual Studio 17 2022"` is part of this: without `-G`, CMake picks the
newest installed Visual Studio, and the x86 modules must remain v141/VC7.1
compatible. `build-all.ps1` detects a build tree created with a foreign
generator and recreates it.

The artifacts are **process-reproducible, not bit-identical**: MSVC embeds
timestamps and PDB GUIDs, so two builds of the same sources produce different
hashes. The manifest records the Git state and warns if the working tree is
dirty.

Verify M1 host against the active OpenXR runtime:

```powershell
build\x64\src\host64\RelWithDebInfo\fearvr-host.exe --validate-only
build\x64\src\host64\RelWithDebInfo\fearvr-host.exe --max-frames 120
```

M2 bridge and real stage:

```powershell
pwsh -File tools\test-m2-bridge.ps1
pwsh -File tools\test-m2-bridge.ps1 -ClassicD3D9
pwsh -File tools\test-m2-bridge.ps1 -AbortHost
pwsh -File tools\prepare-m2-stage.ps1
pwsh -File tools\launch-m2-fear.ps1
```

Playable M4 build:

```powershell
pwsh -File tools\prepare-m4-stage.ps1
pwsh -File tools\launch-m4-fear.ps1
```

M5 with Motion Controls:

```powershell
pwsh -File tools\prepare-m5-stage.ps1
pwsh -File tools\launch-m5-fear.ps1
```

## VR Runtime: SteamVR or Virtual Desktop

The mod is not bound to any specific runtime — the x64 host only speaks
OpenXR. **SteamVR** and **VirtualDesktopXR (VDXR)** are confirmed.

```powershell
pwsh -File tools\launch-m5-fear.ps1                    # active runtime
pwsh -File tools\launch-m5-fear.ps1 -Runtime vdxr      # Virtual Desktop
pwsh -File tools\launch-m5-fear.ps1 -Runtime steamvr   # SteamVR
```

`-Runtime` sets `XR_RUNTIME_JSON` **only for the host process**. The
system-wide setting under
`HKLM\SOFTWARE\Khronos\OpenXR\1\ActiveRuntime` is not changed; to change it
permanently, do so in the Virtual Desktop Streamer or SteamVR respectively.
`tools\verify-install.ps1` shows the active runtime and which ones are
installed.

Only under SteamVR are SteamVR-specific steps executed (disable
`autoShowGameTheater`, theater watchdog). Under VDXR they are skipped
entirely, and no SteamVR file is touched.

**Steam is still required** — but only as the store: F.E.A.R. is officially
launched via `steam.exe -applaunch 21090`. This is independent of which VR
runtime renders. SteamVR itself does not need to run under VDXR.

Bindings: left stick moves, left grip sprints; left stick click is free.
Right stick turns; at 80% deflection it jumps up and crouches down, stick
click recenters the view. A switches weapons, B reloads (short press) or
throws a grenade (hold), X toggles slow-mo, Y opens pause. Right grip uses,
triggers aim and fire. Tilting the left hand sideways leans around corners.
The flashlight is in the left hand, follows its position and aim direction,
and toggles with a click on the left trigger. Every shot vibrates. Mouse,
keyboard and gamepad remain usable in parallel. Details:
`docs/OPENXR-INPUT.md`.

In first-person view, only hands and weapon are visible; upper and lower arms
are hidden.

The M5 launch enables the confirmed stereo HUD by default and closes SteamVR's
delayed F.E.A.R. Desktop Theater automatically. Options:

- `-Translation`: limited HMD translation up to 25 cm, without world
  collision;
- Head bob is off by default; `HeadBob=1` in `fearvr.ini` enables only the
  camera movement while the weapon stays steady for stable aiming;
- `-NoHeadBob`: forces head bob off, even if the INI enables it;
- `-NoStereoHud`: for comparison/troubleshooting only.

Keys in-game:

- F8: toggle native stereo world rendering on/off;
- F9: recenter current HMD orientation;
- F10: toggle world-locked comfort screen for camera shakes and cutscenes;
- F11: isolate player body pieces one by one to recalibrate the arm piece.
  Only needed if the default doesn't fit.

The ESC menu in M5 contains the English-labeled entry "VR SETTINGS" directly
after "Options". The page is deliberately short and single-page: Stereo
rendering, Stereo HUD, Turn speed, Red aim guide, Controller vibration,
Recenter view, Reset VR defaults and BACK. HMD translation, head bob and
comfort screen remain configurable in `fearvr.ini` without cluttering the
native menu. Selection is saved to `stage/userdata-m5/fearvr.ini`. Stick
navigates, A or trigger confirms and B goes back.

## Uninstall

Outside the project root, the mod writes exactly **one** file:
`steamvr.vrsettings`, and there only the key
`steamvr.autoShowGameTheater`. There is no registry change, no write to the
retail installation, and no file outside the project folder.

```powershell
pwsh -File tools\uninstall-fearvr.ps1          # dry run, changes nothing
pwsh -File tools\uninstall-fearvr.ps1 -Apply   # actually remove
```

Removes `stage\`, `build\`, `dist\`, `local-runtime\` and `logs\`.
Beforehand, `autoShowGameTheater` is specifically restored from the oldest
backup — only that one key, so later custom SteamVR settings are preserved.

**Save games are not removed.** `stage\userdata-*` is the game's
`-userdirectory` and contains saves, profiles and screenshots. Those are user
data, not mod files; they are only removed with `-IncludeUserData`.
Additional switches: `-KeepLogs`, `-IncludeVendor` and
`-Scope SteamVrOnly|ProjectOnly`.

A Steam file integrity check is not needed, because retail was never written
to. The script verifies the SHA-256 of `FEAR.exe` before and after.

SteamVR should be closed during uninstall: it rewrites its configuration on
shutdown and would otherwise overwrite the restoration. The script warns if
it sees SteamVR running.

## Known Limitations

- The classic D3D9 path requires a CPU readback per frame
  (`FEARVR_BF_CPU_FALLBACK`), as does the stereo HUD compositor. Both are
  flagged as proof-of-concept and are not a release performance path.
- HMD translation has no world collision and therefore remains opt-in
  (`-Translation`).
- The version-dependent hooks apply to **F.E.A.R. 1.08.282.0**. On mismatched
  hash or signature they stay disabled and the game continues flat.
- The left system/menu button cannot be bound: SteamVR captures it for its
  own system menu.
- The weapon jump when climbing stairs is not conclusively solved and
  deliberately deferred.
- "Motion-controlled aiming" is verified via aim laser and hit point; a
  general "6DoF weapon" is not claimed.

## License

The self-written components are under the **MIT License** (see `LICENSE`).
For the license boundaries of dependencies and the official F.E.A.R. Client
and Public Tools components, see `THIRD_PARTY_NOTICES.md`.

## Legal Notice

This mod contains **no** retail files, no proprietary SDK source code and no
extracted assets. Building and running it requires your own, legally purchased
F.E.A.R. installation and the official Public Tools installer. "VR playable"
is claimed earliest from M4, "Motion Controls" from M5.

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
> readable world-locked menu and stereo HUD are all confirmed
> with real F.E.A.R. on Quest 3/SteamVR. OpenXR controllers fully drive the
> game: movement, turning, weapon selection, jumping, reloading, crouching,
> slow-mo, sprinting, use, aim/fire, melee and pause menu; leaning uses
> left-hand tilt. First-person view keeps hands, torso and legs visible while
> hiding only the arms by default, and the
> ESC menu includes a native VR settings page. The classic D3D9 path still
> uses a flagged CPU compatibility fallback; translation remains opt-in
> without world collision. Details: `docs/TESTING.md`.

## Demo

[![F.E.A.R. VR in action](https://img.youtube.com/vi/QTsNeLT8Pn8/maxresdefault.jpg)](https://youtu.be/QTsNeLT8Pn8)

▶️ **Watch the demo on YouTube:** https://youtu.be/QTsNeLT8Pn8

## Installation

The installer finds the usual paths on its own. When it cannot, it asks for
them and shows examples — nothing has to be edited by hand.

**What you need**

- F.E.A.R. **1.08**, legally installed. Only the Steam Ultimate Shooter
  Edition is confirmed in-game; GOG and retail-disc copies of 1.08 install and
  launch, but are untested — see [Game editions](#game-editions). Versions
  below 1.08 are rejected, because the Public Tools modules do not match them.
- The official **Public Tools 1.08**, installed on this machine (see step 1).
- A headset with **SteamVR** or **Virtual Desktop (VDXR)** as the active
  OpenXR runtime.
- Windows 10/11, 64-bit.

### Must-have mod: F.E.A.R. HD Textures v2.0.2

For the best experience, install
[F.E.A.R. HD Textures v2.0.2 from ModDB](https://www.moddb.com/downloads/fear-hd-textures-v202).
Download it, run its installer, select your F.E.A.R. folder and the correct
Steam/non-Steam option, then click **Install**. That's it — install it and be
happy. F.E.A.R. VR recognizes both the original Steam executable and the
executable patched by this texture mod.

### 1. Install the Public Tools 1.08

Five modules (`GameClient.dll`, `GameServer.dll`, `ClientFx.fxd`, `FEAR.dep`,
`FEARMod.Arch00s`) are proprietary and must not ship with this mod. They come
from Monolith's own installer `fear_publictools_108.exe`, which the Ultimate
Shooter Edition includes under `extras\`. The installer copies them out of
your local Public Tools installation.

The Public Tools installer refuses the Steam version: it reads
`HKLM\SOFTWARE\WOW6432Node\Monolith Productions\FEAR\1.00.0000\Patch` and
expects the value **8**, while Steam sets **10**. Set it to 8, install, set it
back to 10:

```powershell
$key = 'HKLM:\SOFTWARE\WOW6432Node\Monolith Productions\FEAR\1.00.0000'
Set-ItemProperty $key -Name Patch -Value 8      # before installing
Set-ItemProperty $key -Name Patch -Value 10     # after installing
```

### 2. Get the package

Either download a release archive and unpack it anywhere, or build one
yourself from this repository:

```powershell
pwsh -File tools\make-release.ps1
```

### 3. Run the installer

In the unpacked package, double-click **`Install.cmd`**. The window stays open
at the end so the result stays readable. From a shell it is:

```powershell
powershell -ExecutionPolicy Bypass -File tools\install.ps1
```

That is the whole command for a normal setup. It detects the game and the
Public Tools, builds an isolated stage under `%USERPROFILE%\FearVR`, and
creates a desktop shortcut named **F.E.A.R. VR**. The retail installation is
only read, never written — a Steam file verification stays clean.

If a path cannot be detected, the installer prints examples and asks for it.
You can also pass paths up front:

```powershell
powershell -ExecutionPolicy Bypass -File tools\install.ps1 `
  -InstallDir "D:\Games\FearVR" `
  -RetailRoot "D:\SteamLibrary\steamapps\common\FEAR Ultimate Shooter Edition" `
  -PublicToolsGame "C:\Program Files (x86)\Monolith Productions\FEAR Public Tools"
```

**Path examples**

| Option | What to enter | Examples |
|---|---|---|
| `-RetailRoot` | The folder containing `FEAR.exe` | `C:\Program Files (x86)\Steam\steamapps\common\FEAR Ultimate Shooter Edition`<br>`D:\SteamLibrary\steamapps\common\FEAR Ultimate Shooter Edition`<br>`C:\GOG Games\FEAR`<br>`C:\Program Files (x86)\Sierra\FEAR` |
| `-PublicToolsGame` | The Public Tools folder, or its `Dev\Runtime\Game` subfolder | `C:\Program Files (x86)\Monolith Productions\FEAR Public Tools`<br>`C:\Program Files (x86)\Monolith Productions\FEAR Public Tools\Dev\Runtime\Game` |
| `-InstallDir` | Where the mod's own files go (default `%USERPROFILE%\FearVR`) | `C:\Users\You\FearVR`<br>`D:\Games\FearVR` |

Quotes are required around paths with spaces, and pasted quotes are stripped
automatically. Further options: `-NoShortcut` (no desktop shortcut) and
`-NonInteractive` (never prompt — fail instead, for unattended installs).

> **Do not install below `%LOCALAPPDATA%`.** The LithTech engine then fails to
> load its archive configuration and aborts with "Failed to initialize client -
> unable to load game resources". This was measured with a byte-identical
> configuration in different locations; only the location decides. The
> installer rejects such targets and asks for another one.

### 4. Updating

Double-click `Install.cmd` in the new package — there is nothing to uninstall
first.

It detects the existing installation, reuses the paths and launch mode from
last time (so no arguments are needed even if you passed some originally),
replaces the modules, and drops any module a newer package no longer uses.
**Saved games and profiles under `userdata` are kept.** Close the game first —
the installer refuses to run while `FEAR.exe` is open, because the staged
modules are locked then.

Add `-Clean` to wipe the install folder except `userdata` before staging
again, if an installation ever ends up in a strange state.

### 5. Play

Start SteamVR or the Virtual Desktop Streamer first, then use the desktop
shortcut, or:

```powershell
powershell -ExecutionPolicy Bypass -File tools\play.ps1
```

Steam has to be running as the store front (`-applaunch 21090`); SteamVR
itself is optional if you use VDXR. To pick a runtime explicitly, add
`-Runtime steamvr` or `-Runtime vdxr`.

### 6. Uninstall

Double-click `Uninstall.cmd`. It lists what would be removed and deletes only
after you confirm with **Y**. From a shell:

```powershell
powershell -ExecutionPolicy Bypass -File tools\uninstall.ps1          # dry run
powershell -ExecutionPolicy Bypass -File tools\uninstall.ps1 -Apply   # remove
```

Saved games and profiles under `<InstallDir>\userdata` are kept unless you add
`-IncludeUserData`. Since nothing was ever written into the retail folder,
there is nothing to repair there.

### Game editions

The mod is not bound to a specific store. Every byte signature it relies on
lives in `GameOrig.dll`, the stock client that comes from the Public Tools —
`FEAR.exe` itself is only hooked through its import table, which no build-
specific address is derived from. What differs between editions is how the
game is started:

| Edition | Launch | Status |
|---|---|---|
| Steam (Ultimate Shooter Edition 1.08) | `steam.exe -applaunch 21090` | Confirmed in-game |
| Steam 1.08 + HDTextures4FEAR/XP v2.0.2 | `steam.exe -applaunch 21090` | Recognized; exact patched EXE hash confirmed |
| GOG (1.08) | `FEAR.exe` directly, same arguments | Should work, untested |
| Retail disc, patched to 1.08 | `FEAR.exe` directly, same arguments | Should work, untested |

The installer picks the launch mode itself: a copy under `steamapps\common`
goes through Steam, anything else is started directly. Override it with
`-LaunchMode steam` or `-LaunchMode direct`.

An unknown `FEAR.exe` build is no longer an error. The installer prints its
SHA-256 with a warning that this build is untested and continues. If you run
a GOG or disc copy, that hash plus a note whether it worked is exactly what's
needed to list the build as confirmed.

The HDTextures4FEAR/XP v2.0.2 installer replaces the Steam executable. Its
patched hash and the original Steam hash are both recognized. Installing or
removing that texture patch after F.E.A.R. VR was installed therefore does not
require reinstalling the VR mod; unknown executable changes are still rejected.

### If something goes wrong

| Message | Cause and fix |
|---|---|
| `Wrong FEAR.exe version` | Not patched to 1.08, or `-RetailRoot` points at a different installation. |
| `This FEAR.exe build has not been tested` | A 1.08 build other than Steam's (GOG, disc). Installation continues; please report whether it works. |
| `Public Tools 1.08 not found` | Not installed, or installed to a custom folder — pass `-PublicToolsGame` (see step 1 for the `Patch=8` trick). |
| `unable to load game resources` at startup | The install folder is below `%LOCALAPPDATA%`; reinstall with `-InstallDir "D:\Games\FearVR"`. |
| `Package file is missing or was modified` | The package was altered after it was built; unpack it again. |

## Features

Everything listed here is implemented and has run in the actual game. Where
something is built but not yet verified in-game, it's noted.

### Rendering

- **Native stereo world rendering.** The LithTech camera renders twice per
  frame with its own per-eye matrix — not a duplicated mono image. Activates
  automatically after loading; F8 toggles at any time.
- **Relative headtracking.** HMD rotation rotates the game camera relative
  to the neutral horizontal direction; physical pitch and roll are always
  preserved. The 3D world has no manual recenter; its stable camera basis is
  initialized automatically.
- **Optional HMD translation** up to 25 cm (`-Translation`). Without world
  collision, deliberately opt-in.
- **Stereo HUD.** Ammo, health and hints are overlaid into both eyes instead
  of flat over the left image. The HUD is evenly compressed toward the screen
  center (5/4) so edge elements stay within the comfortable field of view.
- **Flat-screen mode for fullscreen UI.** Menus, loading screens, movies and
  the mission briefing appear as a world-locked 2.4 × 1.8 m panel at 2 m
  distance. Detection is based on the retail game state
  `CInterfaceMgr::m_eGameState`, not pixel heuristics. Right stick click, F9
  and `Recenter 2D panel` re-anchor the panel to the current gaze direction.
- **Comfort screen (F10).** World-locked rendering for camera shakes and
  cutscenes, so forced camera movement doesn't pull at the head.
- **Quiet camera.** Weapon bob and camera recoil are disabled; head bob is off
  by default and only toggleable via `fearvr.ini`. Retail's original vertical
  smoothing remains active to blend discrete stair-height steps.
- **Stable collision camera.** Native VR uses Retail's raycast anti-clipping
  fallback instead of its moving camera-collision model, avoiding contact
  wobble without adding a delayed post-process position filter.
- **Stable slide-kick view.** The body and height animation remain visible,
  but its scripted camera rotation cannot tilt the HMD-controlled view or
  leave it pointing downward afterward.
- **First-person body corrected.** Upper and lower arms are hidden by default;
  hands, torso, legs and weapon remain visible, so kick animations can be
  seen. Arms can be toggled in the VR menu. The classic crosshair is off,
  since the red aim laser takes over its role.

### Motion Controls

- **Full game control via OpenXR controllers** — move, turn, jump, crouch,
  sprint, weapon switch, reload, grenade, slow-mo, use, aim, fire, pause and
  melee. Mouse, keyboard and gamepad remain usable in parallel.
- **Weapon follows the right hand.** Shot origin and fire vectors come from
  the muzzle transform, not from gaze direction. View, hands and weapon share
  one directional camera-height basis: Retail smoothing remains for upward
  steps and crouching, while descending/airborne motion bypasses its trailing
  height until it catches up.
- **Red aim laser** from the muzzle, toggleable on/off.
- **Point instead of look.** Activate and pick-up follow the weapon laser
  with approx. 1.5 m range, instead of head direction.
- **Hand flashlight in the left hand.** It follows the hand's position and aim
  direction and toggles with X. The second,
  non-toggleable retail flashlight is removed.
- **Full gesture melee.** A forward thrust of either available hand produces
  the weapon or off-hand strike. The same thrust becomes a jump kick during a
  player-initiated jump. Sprinting forward plus a physical 25 cm crouch or
  stick-down enables the guarded slide kick. Direction is measured against
  each hand's own pointing direction, so sideways swings and pulling back do
  not count.
- **Ladder climbing by hand**, switchable in the VR settings page
  ("Ladder climbing: HANDS / CLASSIC", classic by default). On a ladder a grab
  button grabs the rung and pulling the hand down climbs up. It drives the
  same commands the game itself evaluates on a ladder, so no write into the
  player physics is involved, and the grab buttons keep their usual meaning
  everywhere else.
- **Physical lean with world collision.** Retail's lean only rolls the camera;
  the viewpoint never leaves the player position. Physical head movement now
  moves the viewpoint too, limited by a ray against the world so you cannot
  lean through a wall. Hands, weapon, muzzle and shot origin follow the same
  offset. The visible body follows its horizontal component during rendering,
  keeping the viewer naturally above the torso, while Retail's player object
  and collision capsule remain at the player position. No locomotion axes are
  injected, preventing counter-steering or oscillation. Switchable as
  `Physical lean` in the VR settings.
- **Leaning via hand tilt.** Tilting the left hand sideways leans around
  corners; inverted and hanging hands are handled.
- **Haptics per shot**, including full-auto — triggered on the retail shot,
  not on the trigger edge. Empty magazine = no vibration.

### Menu and Settings

- **Categorized native VR settings** in the ESC menu ("VR Settings"), usable
  with keyboard, mouse, or controller. Six submenus cover Display & HUD,
  Movement & Comfort, Controls, Weapons, Melee, and Advanced settings. B/Back
  returns to the category list before leaving VR settings.
- **Floating live-tuning panel during gameplay.** Hold both grip buttons and
  press B to place the panel in front of your current view. Its Recoil, Weight,
  Weapon, Move, Melee, and VR tabs expose the settings that are safe and useful
  to change during play. Point at a tab or row and press trigger or A; the right
  stick navigates rows and switches tabs. B closes the panel. Changes apply
  immediately and save to `fearvr.ini`; controller gameplay commands are
  captured while it is open. The VR tab can change stereo world render scale
  live from 100% through 200% without changing the desktop resolution.
- **All normal VR preferences are editable in game** and persist immediately
  to `fearvr.ini`, including HMD translation, head bob, comfort screen,
  physical leaning, all four individual melee gestures, and simulated weapon
  weight. The Weapons page is split into Handling & appearance, Simulated
  weight, and Recoil pages. The Advanced page contains diagnostics and the
  global defaults reset.
- **Per-weapon weight and recoil profiles** can be tuned from the native menu
  or floating tool panel. Recoil, Weight, and Weapon tabs show the equipped
  weapon in their header. Choose Current or Default, then adjust weight,
  positional follow, rotational follow, catch-up, recoil strength (up to
  500%), muzzle rise, and recovery through safe presets. Exact values remain
  available in `fearvr.ini` under `[VR]`, `[WeaponWeight.<model-name>]`, or
  `[WeaponRecoil.<model-name>]`. Weapons without overrides use the `[VR]`
  defaults. Recoil remains active when simulated weight is off; per-weapon
  weight still scales the kick when enabled. Empty trigger pulls do not recoil.
  `WeaponWeightDiagnostics=1` emits rate-limited raw/filter error and velocity
  telemetry, including the current recoil offsets.
  Recommended ranges are `0.10-4.00`, `2.0-40.0`, `2.0-40.0`, and
  `0.0-4.0`, respectively.
- **Body visibility switch** saved as `ShowArms` (`0` by default). F11 remains
  a developer diagnostic for isolating Retail body pieces, not the arm-hiding
  mechanism.

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

## EchoPatch (tried, then removed)

[EchoPatch](https://github.com/Wemino/EchoPatch) was evaluated as a companion
patch and is **not** installed. Its crash handler is a vectored exception
handler and killed the game on startup, because this mod deliberately probes
retail internals behind `__try/__except`. With that disabled, EchoPatch aborted
on a missing signature — it patches the game modules, and this mod renames the
retail client to `GameOrig.dll`. Everything EchoPatch is known for (FOV, HUD
scaling, SSAA, gamepad) had to be switched off for VR anyway. Full reasoning
and a reproducible install script, should anyone want to retry:
`docs/ECHOPATCH.md`.

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

Bindings: left stick moves, left grip sprints — or, with that hand placed on
the weapon, holds it as a second hand; left stick click uses a medkit.
Movement is body-relative by default, so looking around does not steer a held
movement direction. `Move direction: Head` opts into steering forward from
the HMD's current horizontal facing direction. The locomotion
stick uses a circular deadzone that preserves diagonal direction and speed.
Right stick turns; at 80% deflection it jumps up and crouches down, stick
click performs a melee attack in the 3D world and re-anchors the panel in 2D.
A switches weapons, B reloads (short press) or
throws a grenade (hold), X toggles the flashlight, Y opens pause. Right grip
uses, the left trigger toggles slow-mo, and the right trigger
fires. Tilting the left hand sideways leans around corners.
Holding the weapon with both hands steers longer weapons along the line
between the hands; sprinting and leaning rest while that grab is held.
`Controls: Left-handed` in the VR menu mirrors every binding between the
hands.
The flashlight is in the left hand, follows its position and aim direction,
and toggles with X. Every shot vibrates. Mouse,
keyboard and gamepad remain usable in parallel. Details:
`docs/OPENXR-INPUT.md`.

In first-person view, hands, torso, legs and weapon remain visible; only upper
and lower arms are hidden by default. `Show arms: On / Off` in the VR menu
switches immediately between the original Retail material and the VR arm mask.

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
- F9: re-anchor menus and other 2D panels; no function in the 3D world;
- F10: toggle world-locked comfort screen for camera shakes and cutscenes;
- F11: developer diagnostic that isolates player body pieces one by one.

The ESC menu in M5 contains the English-labeled entry "VR Settings" directly
after "Options". Six short native submenus cover Display & HUD, Movement &
Comfort, Controls, Weapons, Melee and Advanced settings without overflowing
the Retail frame. This includes HMD translation, head bob, comfort screen,
individual melee switches, physical lean and simulated weapon-weight profiles.
Changes are applied immediately and saved to `fearvr.ini`. Stick navigates,
A or trigger confirms, and B returns to the category list before leaving VR
settings.

For faster testing without pausing, hold both grips and press B during gameplay
to open the world-space live-tuning panel. The right-controller ray selects a
tab or row, trigger or A confirms/changes it, right-stick left/right switches
tabs, right-stick up/down selects rows, and B closes it.

## Uninstall

Outside the project root, the mod writes exactly **one** file:
`steamvr.vrsettings`, and there only the key
`steamvr.autoShowGameTheater`. There is no registry change and no write to the
retail installation. (`tools\install-echopatch.ps1` would place two files in
the retail folder, but EchoPatch is not installed — see above.)

```powershell
pwsh -File tools\uninstall-fearvr.ps1          # dry run, changes nothing
pwsh -File tools\uninstall-fearvr.ps1 -Apply   # actually remove
```

Removes `stage\`, `build\`, `dist\`, `local-runtime\`, `logs\` and EchoPatch.
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

- The classic D3D9 path still requires a CPU readback per eye and frame
  (`FEARVR_BF_CPU_FALLBACK`). F.E.A.R. creates a plain `IDirect3DDevice9`, and
  D3D9 can only share surfaces across processes from a D3D9Ex device — so this
  is the one remaining copy. The zero-copy `DirectShared` path already exists
  and engages the moment the device is an Ex device; getting there needs a
  wrapper for textures and buffers, because `D3DPOOL_MANAGED` does not exist on
  Ex devices. **The stereo HUD compositor no longer reads back**: the pixel
  comparison runs as a `ps_2_0` shader on the GPU, and its coverage heuristic
  reads a few kilobytes one frame late instead of a full frame. That removed
  one of three readbacks and all per-pixel CPU work. `-fearvr-no-gpu-hud`
  forces the old CPU compositor back.
- HMD translation has no world collision and therefore remains opt-in
  (`-Translation`).
- The version-dependent hooks verify byte signatures, image size and timestamp
  of the **F.E.A.R. 1.08** Public Tools modules. On a mismatch the affected
  hook stays disabled and the game continues flat. `FEAR.exe` itself is only
  hooked through its import table, which is why other 1.08 builds are expected
  to work (see [Game editions](#game-editions)).
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

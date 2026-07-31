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

The release is a binary overlay: extract it directly into the existing game
folder that contains `FEAR.exe`.

**What you need**

- F.E.A.R. **1.08**, legally installed. Only the Steam Ultimate Shooter
  Edition is confirmed in-game; GOG and retail-disc copies of 1.08 install and
  launch, but are untested — see [Game editions](#game-editions). Versions
  below 1.08 are rejected, because the Public Tools modules do not match them.
- A public archive needs the official **Public Tools 1.08** installed locally.
  A personal `-PrivateBundle` already contains the required runtime modules.
- A headset with **SteamVR** or **Virtual Desktop (VDXR)** as the active
  OpenXR runtime. Native SteamVR hardware is covered by explicit Valve Index
  and HTC Vive Wand controller profiles; Lighthouse headsets without bundled
  controllers use whichever of those controllers is connected.
- Windows 10/11, 64-bit.

### Must-have mod: F.E.A.R. HD Textures v2.0.2

For the best experience, install
[F.E.A.R. HD Textures v2.0.2 from ModDB](https://www.moddb.com/downloads/fear-hd-textures-v202).
Download it, run its installer, select your F.E.A.R. folder and the correct
Steam/non-Steam option, then click **Install**. That's it — install it and be
happy. F.E.A.R. VR recognizes both the original Steam executable and the
executable patched by this texture mod.

### 1. Get the overlay

Download a release ZIP, or build one:

```powershell
pwsh -File tools\make-release.ps1
```

Developers who own the Public Tools can build an immediately playable
personal archive:

```powershell
pwsh -File tools\make-release.ps1 -PrivateBundle
```

That private archive contains proprietary Public Tools runtime files. It is
marked **not for redistribution** because their EULA does not grant public
redistribution rights.

### 2. Extract over the game

Extract the ZIP directly into the folder containing `FEAR.exe`. It adds:

```text
dinput8.dll
d3d9.dll
FEARVR\
Start F.E.A.R. VR.cmd
Start F.E.A.R. VR - SteamVR.cmd
```

No retail executable or archive is replaced. `dinput8.dll` is F.E.A.R. VR's
own early DirectInput proxy; it disables the base game's known accumulating
HID performance bug before input initialization, then forwards to Windows'
real DirectInput implementation. `d3d9.dll` is the early renderer bridge that
enables the non-blocking transfer worker before the game creates its graphics
device. If another graphics wrapper already uses that filename, preserve it
before extraction; the development installer chains such a preserved wrapper
as `d3d9.fearvr-upstream.dll`. Updating is the same operation: extract the
newer ZIP over the same folder. Saves, configuration and logs stay below
`FEARVR\`.

### 3. Public Tools for the public archive

On first launch, the public archive copies five proprietary runtime modules
from the owner's local Public Tools installation. The official
`fear_publictools_108.exe` is included with the Steam Ultimate Shooter Edition
under `extras\`.

The old installer expects registry value `Patch=8`, while Steam normally uses
10. Set it to 8 for installation and restore 10 afterwards:

```powershell
$key = 'HKLM:\SOFTWARE\WOW6432Node\Monolith Productions\FEAR\1.00.0000'
Set-ItemProperty $key -Name Patch -Value 8
# run extras\fear_publictools_108.exe
Set-ItemProperty $key -Name Patch -Value 10
```

For a custom location, pass its root or `Dev\Runtime\Game` folder:

```powershell
.\Start F.E.A.R. VR.cmd -PublicToolsGame "D:\FEAR Public Tools\Dev\Runtime\Game"
```

### 4. Play

Start the headset runtime, then double-click `Start F.E.A.R. VR.cmd`. To force
Valve's runtime, use `Start F.E.A.R. VR - SteamVR.cmd`.

SteamVR is an OpenXR runtime, so this uses the same x64 host binary with
Valve's runtime manifest—not a separate renderer or a compatibility wrapper.
VDXR can be selected with `-Runtime vdxr`; the system-wide OpenXR setting is
never changed.

Steam still has to run as the store front for a Steam copy
(`-applaunch 21090`). This is independent of which VR runtime renders.

### 5. Remove

Delete `dinput8.dll`, `d3d9.dll`, the `FEARVR` folder and the two
`Start F.E.A.R. VR*.cmd` files. No original retail file was replaced, so a
Steam file verification is unnecessary.

### Game editions

The mod is not bound to a specific store. Most byte signatures live in
`GameOrig.dll`, the stock client from the Public Tools. The early HID fix also
checks two F.E.A.R. 1.08 code regions in the in-memory `FEAR.exe`; it applies
only when timestamp, image size and every still-unpatched byte match. A
supported wrapper may already have safely patched either region; the proxy
then completes only the other verified region. It never edits the executable
on disk. What differs between editions is how the game starts:

| Edition | Launch | Status |
|---|---|---|
| Steam (Ultimate Shooter Edition 1.08) | `steam.exe -applaunch 21090` | Confirmed in-game |
| Steam 1.08 + HDTextures4FEAR/XP v2.0.2 | `steam.exe -applaunch 21090` | Recognized; exact patched EXE hash confirmed |
| GOG (1.08) | `FEAR.exe` directly, same arguments | Should work, untested |
| Retail disc, patched to 1.08 | `FEAR.exe` directly, same arguments | Should work, untested |

The overlay picks the launch mode itself: a copy under `steamapps\common`
goes through Steam, anything else is started directly.

An unknown `FEAR.exe` build is no longer an error. The overlay records its
SHA-256 with a warning and continues; on a byte mismatch the HID patch remains
off rather than touching unknown code. If you run a GOG or disc copy, that hash
plus the `fear_hid_fix` result from the log is what is needed to confirm it.

The HDTextures4FEAR/XP v2.0.2 installer replaces the Steam executable. Its
patched hash and the original Steam hash are both recognized. Installing or
removing that texture patch after F.E.A.R. VR was installed therefore does not
require reinstalling the VR mod; unknown executable changes are still rejected.

### If something goes wrong

| Message | Cause and fix |
|---|---|
| `Wrong FEAR.exe version` | Not patched to 1.08, or `-RetailRoot` points at a different installation. |
| `This FEAR.exe build has not been tested` | A 1.08 build other than Steam's (GOG, disc). Installation continues; please report whether it works. |
| `Public Tools 1.08 not found` | The public archive could not find them—install the copy under `extras\`, pass `-PublicToolsGame`, or use a local `-PrivateBundle`. |
| `Package file is missing or was modified` | The package was altered after it was built; unpack it again. |
| Steam creates no `FEAR.exe` after three launch attempts | The launcher now falls back to the verified executable directly while Steam remains running, then still verifies that the matching VR bridge loaded. |
| `vrmonitor.exe` reports missing `d3dx10_43.dll` | SteamVR's legacy status window is missing the DirectX End-User Runtimes (June 2010). A private bundle supplies the local x64 redistributable through the child-process path without modifying SteamVR. |

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
- **Support-wrist status HUD.** Looking directly at the inside of the support
  wrist opens a compact technical line display for health, armor, total ammo,
  frag grenades, proximity mines, remote charges, medkits and air. It renders
  in front of the hand without depth occlusion and stays closed while the
  support hand holds the weapon. The classic status panels are hidden only
  during active stereo gameplay and restored before pause, ESC and fullscreen
  menus render.
- **Stereo overlay for remaining HUD messages.** Hints and other non-status
  overlays are still lifted into both eyes instead of being left flat over
  one eye.
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
  melee. Touch and Valve Index controllers expose every dedicated button.
  Vive Wands map their two menu buttons to pause and reload/grenade; their
  digital grip clicks are converted to the squeeze actions used by analog
  controllers. Mouse, keyboard and gamepad remain usable in parallel.
- **Weapon follows the right hand.** Shot origin and fire vectors come from
  the muzzle transform, not from gaze direction. View, hands and weapon share
  one directional camera-height basis: Retail smoothing remains for upward
  steps and crouching, while descending/airborne motion bypasses its trailing
  height until it catches up.
- **Red aim laser** from the muzzle, toggleable on/off.
- **Point instead of look.** Activate and pick-up follow the weapon laser
  with approx. 1.5 m range, instead of head direction.
- **Configurable VR flashlight.** It can follow the left hand, the headset or
  the weapon (default) and toggles with X. Head mode keeps world shadows but suppresses
  shadows from the player's body, hands and weapon so they cannot block the
  head-mounted beam. The second, non-toggleable retail flashlight is removed.
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
- **Physical duck.** Lowering the headset by 26 cm engages Retail crouch and
  releases above an 18 cm hysteresis threshold. It is independently
  switchable in VR settings; the classic stick crouch always remains active.
- **Leaning via hand tilt.** Tilting the left hand sideways leans around
  corners; inverted and hanging hands are handled.
- **Haptics per shot**, including full-auto — triggered on the retail shot,
  not on the trigger edge. Empty magazine = no vibration; Dual Pistols pulse
  the controller whose pistol actually fired.
- **Native Dual Pistols in both hands.** The left pistol follows the support
  controller with its own muzzle and aim ray. Left trigger fires left, right
  trigger fires right; holding both lets Retail alternate its dual-pistol
  cadence. While they are equipped, X controls slow-mo and leaves the current
  flashlight state unchanged.

### Menu and Settings

- **Native VR settings page** in the ESC menu ("VR SETTINGS"), usable with
  the controller: Stereo rendering, Stereo HUD, Turn speed, Red aim
  guide, Controller vibration, Flashlight mount (left hand/head/weapon),
  Controls (right/left-handed), Ladder climbing (hands or classic), Physical
  lean, Physical duck, Melee (gestures or classic), Show arms, FOV scale,
  Recenter 2D panel, Reset VR defaults.
- **Persistence in `fearvr.ini`**, including settings not exposed in the menu:
  HMD translation, head bob, comfort screen and four individual melee moves.
- **Body visibility switch** saved as `ShowArms` (`0` by default). The
  generated Alpha-Test material hides only upper and lower arms; hands, torso
  and legs remain visible, while the head is hidden through its separate
  material slot. F11 remains
  a developer diagnostic for isolating Retail body pieces, not the arm-hiding
  mechanism.

### Operation

- **Two processes by bitness:** x64 OpenXR host and x86 D3D9 bridge over a
  versioned shared-memory protocol with frame ring and heartbeat monitoring.
  If the host crashes, the game continues flat.
- **Runtime selection at launch:** SteamVR and VirtualDesktopXR are confirmed;
  `-Runtime` sets `XR_RUNTIME_JSON` only for the host process.
- **No SteamVR configuration helpers:** launching through SteamVR no longer
  edits `steamvr.vrsettings` or starts a theater watchdog.
- **Structured JSON logs** for launcher, host and bridge with perf counters;
  each run writes to its own directory under `logs\`.
- **Version-bound with fail-safe.** All retail hooks check timestamp, image
  size and expected byte patterns. If something doesn't match, the hook stays
  disabled and the game continues. Diagnostic switches like `-fearvr-safe`,
  `-fearvr-no-interaction` or `-fearvr-no-gamestate` are available.
- **Distributable package** (`tools\make-release.ps1`) with installer, desktop
  shortcut and uninstaller. It contains only our own MIT-licensed binaries;
  the proprietary modules are pulled from the local Public Tools installation.

## Core Principles

- **Original retail files stay untouched.** The development and release
  launchers add their own `dinput8.dll` and `d3d9.dll` proxies beside
  `FEAR.exe`; no original EXE/archive is overwritten. Game modules, saves and
  logs remain in the isolated stage or `FEARVR\` overlay.
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
FEAR.exe (x86) + early dinput8 HID fix + D3D9 bridge + GameClient module
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
| `src/dinput8_proxy/` | early x86 DirectInput forwarder and guarded HID fix |
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

## Base-game HID slowdown fix

The original game can initialize general HID devices as controllers and then
degrade permanently after several minutes until the process is restarted.
F.E.A.R. VR now includes only the isolated correction for those redundant
input blocks plus the legacy input-hook latency block. The patch validates all
original bytes before replacing them with NOPs and records `fear_hid_fix` in
each run log. The full EchoPatch remains uninstalled because its unrelated
module hooks and exception handler conflict with this VR mod. Details:
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

For native SteamVR hardware the host supplies dedicated bindings for Valve
Index controllers and HTC Vive Wands. When SteamVR advertises
`XR_KHR_generic_controller`, the host also enables that profile so SteamVR can
automatically remap newer driver-provided controllers. The selected profile is
written to the host log as `input_interaction_profile` for each hand.

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

Runtime selection only affects the host's OpenXR manifest. Neither SteamVR nor
VDXR launch edits SteamVR settings or starts an auxiliary theater process.

**Steam is still required** — but only as the store: F.E.A.R. is officially
launched via `steam.exe -applaunch 21090`. This is independent of which VR
runtime renders. SteamVR itself does not need to run under VDXR.

Touch/Index bindings: left stick moves, left grip sprints — or, with that hand placed on
the weapon, holds it as a second hand; left stick click uses a medkit.
Right stick turns; at 80% deflection it jumps up and crouches down, stick
click performs a melee attack in the 3D world and re-anchors the panel in 2D.
A switches weapons, B reloads (short press) or
throws a grenade (hold), X toggles the flashlight, Y opens pause. Right grip
uses, the left trigger toggles slow-mo, and the right trigger fires. With Dual
Pistols equipped, left/right trigger fire the matching pistol and X controls
slow-mo without changing the flashlight. Tilting the left hand sideways leans
around corners.
On Vive Wands, the trackpads replace the sticks, trackpad clicks replace the
primary face buttons, grip clicks provide sprint/use, the left menu button
opens pause and the right menu button reloads on a short press or throws a
grenade when held. Vive Wands have no physical stick clicks: medkit remains
available on the keyboard, while the normal motion gesture provides melee.
Holding the weapon with both hands steers longer weapons along the line
between the hands. The visible support hand snaps to the weapon-specific grip
from Retail's original animation instead of freezing at the controller's grab
position; the physical controller still steers the weapon. Sprinting and
leaning rest while that grab is held.
`Controls: LEFT-HANDED` in the VR menu mirrors every binding between the
hands.
The flashlight mount can be switched between left hand, head and weapon in the
VR menu and toggles with X, except while Dual Pistols reserve X for slow-mo.
Every shot vibrates. Mouse,
keyboard and gamepad remain usable in parallel. Details:
`docs/OPENXR-INPUT.md`.

In first-person view, hands, torso, legs and weapon remain visible; only upper
and lower arms are hidden by default. `Show arms: ON / OFF` in the VR menu
switches immediately between the original Retail material and the VR arm mask.

The M5 launch enables the stereo overlay and support-wrist status HUD by
default.
Options:

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

The ESC menu in M5 contains the English-labeled entry "VR SETTINGS" directly
after "Options". The page is deliberately short and single-page: Stereo
rendering, Stereo HUD, Turn speed, Red aim guide, Controller vibration,
flashlight mount, right/left-handed controls, ladder climbing, physical lean,
gesture/classic melee, Show arms, FOV scale, Recenter 2D panel, Reset VR
defaults and BACK. HMD translation, head bob, comfort screen and the four
individual melee switches remain configurable in `fearvr.ini`
without cluttering the native menu. Selection is saved to
`stage/userdata-m5/fearvr.ini`. Stick navigates, A or trigger confirms and B
goes back.

## Uninstall

The development launcher does not change the registry or SteamVR configuration.
Stage preparation installs only F.E.A.R. VR's own guarded `dinput8.dll` beside
`FEAR.exe`; original retail files remain unchanged.

```powershell
pwsh -File tools\uninstall-fearvr.ps1          # dry run, changes nothing
pwsh -File tools\uninstall-fearvr.ps1 -Apply   # actually remove
```

Removes `stage\`, `build\`, `dist\`, `local-runtime\`, `logs\` and the verified
F.E.A.R.-VR `dinput8.dll`.
For installations made by older revisions, the uninstaller can still restore
the legacy `autoShowGameTheater` backup without replacing other SteamVR
settings.

**Save games are not removed.** `stage\userdata-*` is the game's
`-userdirectory` and contains saves, profiles and screenshots. Those are user
data, not mod files; they are only removed with `-IncludeUserData`.
Additional switches: `-KeepLogs`, `-IncludeVendor` and
`-Scope SteamVrOnly|ProjectOnly`.

A Steam file integrity check is not needed, because no original retail file is
replaced. The script verifies the SHA-256 of `FEAR.exe` before and after.

SteamVR should be closed during uninstall: it rewrites its configuration on
shutdown and would otherwise overwrite the restoration. The script warns if
it sees SteamVR running.

## Known Limitations

- The classic D3D9 path still requires a CPU readback per eye and frame
  (`FEARVR_BF_CPU_FALLBACK`). F.E.A.R. creates a plain `IDirect3DDevice9`, and
  D3D9 can only share surfaces across processes from a D3D9Ex device — so this
  is the one remaining copy. The readback no longer stalls F.E.A.R.'s
  Present thread: the early root-level `d3d9.dll` creates the game device
  multithread-safe, and a latest-frame worker performs readback plus D3D9Ex
  upload in the background. Existing queued images are superseded instead of
  building latency. The zero-copy `DirectShared` path already exists
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
  of the **F.E.A.R. 1.08** Public Tools modules. The early HID fix performs the
  same full validation on its two small `FEAR.exe` regions. On a mismatch the
  affected hook stays disabled instead of patching unknown code.
- On Touch controllers SteamVR captures the left system/menu button; Y remains
  the pause binding. Vive Wand application-menu buttons remain available and
  are mapped explicitly.
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

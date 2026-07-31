# F.E.A.R. VR

VR mod for the single-player base version of **F.E.A.R. 1.08**
(LithTech Jupiter EX, Direct3D 9). Native stereo rendering, headtracking and
OpenXR motion controls.

## Requirements

1. **F.E.A.R. 1.08**, legally installed. The Steam Ultimate Shooter Edition
   (1.08.282.0) and its HDTextures4FEAR/XP v2.0.2 patched executable are
   recognized; GOG and retail-disc copies of 1.08 install and launch as well,
   but are untested. An unknown build is reported
   with its SHA-256 and installation continues — every byte signature this mod
   uses lives in `GameOrig.dll` from the Public Tools, not in `FEAR.exe`.
   Versions below 1.08 are rejected.
2. **F.E.A.R. Public Tools 1.08.** The official installer
   `fear_publictools_108.exe` ships with the Ultimate Shooter Edition under
   `extras\`.
3. A headset with **SteamVR** or **Virtual Desktop**. Both runtimes are
   confirmed.
4. Windows 10/11, 64-bit.

### Note on installing the Public Tools

The installer reads
`HKLM\SOFTWARE\WOW6432Node\Monolith Productions\FEAR\1.00.0000\Patch` and
expects the value **8**, while Steam sets **10**. Set it to 8 for the
installation and back to 10 afterwards — without that step the installer
rejects the Steam edition.

```powershell
$key = 'HKLM:\SOFTWARE\WOW6432Node\Monolith Productions\FEAR\1.00.0000'
Set-ItemProperty $key -Name Patch -Value 8      # before installing
Set-ItemProperty $key -Name Patch -Value 10     # after installing
```

These five modules are proprietary and must not ship with this package. They
are copied from **your own** Public Tools installation during setup:

`GameClient.dll`, `GameServer.dll`, `ClientFx.fxd`, `FEAR.dep`,
`FEARMod.Arch00s`

## Installation

Double-click **`Install.cmd`** in this folder. That is all — the window stays
open at the end so the result stays readable.

Equivalent from a shell, if you prefer typing:

```powershell
powershell -ExecutionPolicy Bypass -File tools\install.ps1
```

The default target is `%USERPROFILE%\FearVR`. The game folder and the Public
Tools are detected automatically and verified by their hashes. If a path
cannot be found, the installer shows examples and asks for it.

> **Do not install below `%LOCALAPPDATA%`.** The engine then fails to load its
> archive configuration and aborts with "Failed to initialize client - unable
> to load game resources". The installer rejects such targets.

Options:

```powershell
tools\install.ps1 -InstallDir "D:\Games\FearVR"
tools\install.ps1 -RetailRoot "D:\SteamLibrary\steamapps\common\FEAR Ultimate Shooter Edition"
tools\install.ps1 -PublicToolsGame "C:\Program Files (x86)\Monolith Productions\FEAR Public Tools"
tools\install.ps1 -LaunchMode direct  # start FEAR.exe without Steam (GOG, disc)
tools\install.ps1 -NoShortcut       # no desktop shortcut
tools\install.ps1 -NonInteractive   # never prompt; fail instead
```

`Install.cmd` passes any extra arguments straight through, so
`Install.cmd -InstallDir "D:\Games\FearVR"` works the same way.

A copy under `steamapps\common` is launched through
`steam.exe -applaunch 21090`; any other copy is started directly with the
same arguments. `-LaunchMode` overrides that choice.

`-PublicToolsGame` accepts either the installation folder or its
`Dev\Runtime\Game` subfolder. Quote any path that contains spaces.

## Updating

Double-click `Install.cmd` in the new package. There is nothing to uninstall
first.

An existing installation is detected, the paths and launch mode from last
time are reused, the modules are replaced, and modules a newer package no
longer uses are removed. **Saved games and profiles stay.** Close the game
first — while `FEAR.exe` runs, its modules are locked and the installer
refuses to continue.

`-Clean` wipes the install folder except `userdata` before staging again.

## Playing

Desktop shortcut **F.E.A.R. VR**, or:

```powershell
powershell -ExecutionPolicy Bypass -File tools\play.ps1
```

Options:

```powershell
tools\play.ps1 -Runtime vdxr      # force Virtual Desktop
tools\play.ps1 -Runtime steamvr   # force SteamVR
tools\play.ps1 -Translation       # limited HMD translation (opt-in)
tools\play.ps1 -NoHeadBob         # force head bob off (already off by default)
tools\play.ps1 -NoStereoHud       # troubleshooting only
tools\play.ps1 -NoCapture         # raw Present-rate diagnosis; headset image off
tools\play.ps1 -NoHidFpsFix       # diagnostic rollback of the Jupiter EX HID fix
tools\play.ps1 -NoXrFramePacing   # allow duplicate XR requests for A/B testing
tools\play.ps1 -RenderScale 150   # supersample native stereo world rendering
```

`-Runtime` sets `XR_RUNTIME_JSON` for the host process only. The system-wide
runtime setting is never changed.

`-RenderScale` accepts 100 through 200 percent and applies only to native
stereo gameplay. Retail menus and videos keep their original backbuffer.
Start with 125 or 150 percent: classic-D3D9 CPU readback cost grows with the
number of pixels.

A Steam copy needs the Steam client running, because F.E.A.R. officially
starts through `steam.exe -applaunch 21090`. GOG and disc copies do not need
Steam at all. Either way this is independent of which VR runtime renders;
SteamVR itself does not have to run when using Virtual Desktop.

## Controls

| Input | Action |
|---|---|
| left stick | move |
| left grip | sprint — or, with the hand on the weapon, hold it two-handed |
| left stick click | use medkit |
| left trigger | slow-mo |
| right stick left/right | turn |
| right stick up/down | jump / crouch (from 80 % deflection) |
| right stick click | melee attack in 3D / re-anchor panel in 2D |
| right grip | use |
| right trigger | fire |
| A | weapon switch |
| B | short: reload — held: throw grenade |
| X | flashlight |
| Y | pause menu |
| tilt left hand sideways | lean around corners (only with *Physical lean: OFF*) |
| physically lean your head | move viewpoint and visible body together, stopped by walls (*Physical lean: ON*) |
| thrust either free hand forward | weapon/off-hand strike |
| jump, then thrust either free hand | jump kick (never injects a jump) |
| sprint forward, crouch physically or with stick, then thrust | slide kick |
| grab on a ladder, pull down | climb up (set *Ladder climbing: HANDS*) |

Keyboard: **F8** stereo on/off, **F9** re-anchor 2D panel, **F10** world-locked comfort
screen, **F11** developer body-piece diagnostic.

Mouse, keyboard and gamepad remain usable in parallel. The VR options live in
the ESC menu under **VR SETTINGS**, including
`Controls: RIGHT-HANDED / LEFT-HANDED`, which mirrors the whole layout, and
`Ladder climbing: HANDS / CLASSIC`, which decides whether ladders are climbed
by grabbing the rungs or with the stick as in the original game.
`Melee: GESTURES / CLASSIC` enables or disables all motion melee. Fresh
configurations default to `GESTURES`.
`Show arms: ON / OFF` switches between the original Retail arms and the VR
mask. It defaults to `OFF`; hands, torso and legs remain visible. The choice
is saved as `ShowArms` in `fearvr.ini`.

The four moves can also be disabled independently in `fearvr.ini` under
`[VR]`: `MeleeWeaponStrike`, `MeleeOffHandStrike`, `MeleeJumpKick` and
`MeleeSlideKick` (`1` = enabled, the default; `0` = disabled).

The left system/menu button cannot be bound: SteamVR grabs it for its own
system menu.

## Uninstall

Double-click **`Uninstall.cmd`**. It first lists what would be removed and
only deletes after you confirm with **Y**.

```powershell
powershell -ExecutionPolicy Bypass -File tools\uninstall.ps1          # dry run
powershell -ExecutionPolicy Bypass -File tools\uninstall.ps1 -Apply   # remove
```

**Saved games are kept.** They live in `<InstallDir>\userdata` and are only
removed with `-IncludeUserData`.

The retail installation is never written to at any point. A Steam file
verification is not needed; as far as Steam is concerned the installation is
unmodified.

## Known limits

- The classic D3D9 path still needs one CPU readback per eye and frame
  (`FEARVR_BF_CPU_FALLBACK` in the log). F.E.A.R. creates a plain
  `IDirect3DDevice9`, and D3D9 can only share surfaces between processes from
  a D3D9Ex device — so this is the one remaining copy. The zero-copy
  `DirectShared` path exists and engages as soon as the device is an Ex
  device.
- The stereo HUD compositor, by contrast, **no longer reads back**: the pixel
  comparison runs as a `ps_2_0` shader on the GPU, and the coverage heuristic
  that separates the HUD from fullscreen effects reads a few kilobytes one
  frame late instead of a full frame. That removed one of three readbacks and
  all per-pixel CPU work. `-fearvr-no-gpu-hud` forces the old CPU compositor
  back for troubleshooting.
- HMD translation has no world collision and therefore stays opt-in.
- The version-dependent hooks check byte signatures in the Public Tools
  modules of **F.E.A.R. 1.08**. If one does not match, that hook stays
  disabled and the game keeps running flat.
- The weapon jump when climbing stairs is not fully understood yet.
- "Motion-controlled aiming" is backed by the aim ray and the hit point; a
  general "6DoF weapon" is not claimed.

## License

Our own parts are under the **MIT license** (see `LICENSE`). For the
dependencies see `THIRD_PARTY_NOTICES.md`.

This package contains **no** retail files, no proprietary SDK sources and no
extracted assets. Running it requires your own, legally obtained F.E.A.R.
installation and the official Public Tools installer.

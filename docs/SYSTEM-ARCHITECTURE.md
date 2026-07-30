# F.E.A.R. VR System Architecture

This document explains how the current VR mod is assembled and how data moves
through it at runtime. It describes the implementation in this repository as of
29 July 2026. Historical choices, experiments, and rejected alternatives remain
in [ARCHITECTURE.md](ARCHITECTURE.md); subsystem-specific details are linked
throughout this document.

## 1. Architectural drivers

The design is shaped by five constraints:

1. `FEAR.exe` and the LithTech client are 32-bit Direct3D 9 code, while the
   OpenXR host and the supported runtime path are 64-bit Direct3D 11.
2. The retail installation must remain unchanged. The mod is loaded through
   F.E.A.R.'s official `-archcfg` module layer from an isolated directory.
3. Only world rendering may run twice. Simulation, input, AI, sound, particles,
   and game time must still advance once per game frame.
4. Hooks are valid only for the verified Retail 1.08 layout. Unknown code or
   interface layouts disable the affected feature instead of being guessed.
5. Losing the host, losing the D3D device, filling the transport ring, or
   disabling stereo must leave the normal flat game path usable.

These constraints produce a two-process system joined by a versioned shared
memory contract and shared GPU textures.

## 2. System context

```text
                         OpenXR calls and layers
                 +---------------------------------+
                 |                                 v
+------------------------+              +------------------------+
| OpenXR runtime (x64)   |<-------------| fearvr-host.exe (x64) |
| SteamVR, VDXR, etc.    |              | OpenXR + D3D11        |
+------------------------+              +-----------+------------+
                                                    ^
                                      shared memory | shared D3D textures
                                      + events      | poses/input/haptics
                                                    v
+------------------------+              +------------------------+
| Retail FEAR.exe (x86)  |<------------>| fearvr-d3d9.dll (x86) |
| LithTech + D3D9        | Present/Reset| bridge and transport   |
|                        |              +------------+-----------+
|  +------------------+  |                           ^
|  | GameClient.dll   |--+---- C ABI: render/input  |
|  | VR loader/hooks  |  |                           |
|  +--------+---------+  |                           |
|           v            |                           |
|  +------------------+  |                           |
|  | GameOrig.dll     |  |                           |
|  | stock game client|  |                           |
|  +------------------+  |                           |
+------------------------+                           |
        ^                                            |
        +--------- launch/session configuration -----+
                    tools/release/play.ps1
```

The host owns everything that must talk to OpenXR. The game process owns
LithTech integration and D3D9 rendering. The D3D9 bridge is the boundary
adapter: it exposes a small C ABI to the game hooks and transports completed
eye images to the host.

## 3. Component responsibilities

| Component | Runtime | Responsibility | Primary implementation |
|---|---:|---|---|
| Release launcher | PowerShell | Validates the installed stage, selects an OpenXR runtime, creates a per-run session ID and logs, starts the host, waits for `xr_ready`, then starts F.E.A.R. | `tools/release/play.ps1`, `tools/_fearvr-env.ps1` |
| OpenXR host | x64 process | Owns the OpenXR instance, action set, session, local space, two swapchains, frame loop, D3D11 device, layer submission, texture import, and haptic output | `src/host64/` |
| Shared contract | x86/x64 header-only | Defines fixed-width IPC structures, flags, seqlocks, ring-slot states, object names, math, and independently testable gameplay algorithms | `src/common/` |
| D3D9 bridge | x86 DLL | Hooks D3D9 creation/`Present`/`Reset`, captures eye and final back-buffer images, composites the HUD, manages shared textures and the ring, and exposes the GameClient C ABI | `src/proxy32/` |
| GameClient loader and hooks | x86 DLL | Preserves the stock GameClient ABI, loads `GameOrig.dll`, verifies Retail 1.08 interfaces/signatures, repeats only `RenderCamera`, maps tracking/input into game behavior, and adds VR settings | `src/gameclient_loader/` |
| Stock game modules | x86 DLLs/data | Supply the original Public Tools GameClient/GameServer and resource modules. They are copied locally by the installer and are not stored in this repository | local `game-modules/` stage |
| Installer and packager | PowerShell | Builds an isolated deployment without proprietary content and generates/verifies manifests | `tools/release/install.ps1`, `tools/make-release.ps1` |
| Native launcher stub | x86 executable | Early launcher scaffold; it is not the production orchestration path described above | `src/launcher/` |

### 3.1 Why the GameClient is a loader

The stage presents the project's `GameClient.dll` to LithTech. It implements the
two expected exports:

- `GetBuildNumber` loads and delegates to the stock `GameOrig.dll`.
- `SetMasterDatabase` delegates to the stock module, then installs VR hooks
  using the engine's initialized interface database.

The same loader lazily loads `fearvr-d3d9.dll` and asks it to install the D3D9
IAT and late-device hooks. `DllMain` in both DLLs only disables thread
notifications; OpenXR, D3D, IPC, logging, and hook initialization happen later,
outside the loader lock.

This arrangement preserves the original client behavior while putting the
smallest practical compatibility boundary in front of it.

## 4. Deployment and startup

The installer creates an isolated tree containing:

```text
<install>/
  fearvr.archcfg       retail archive entries plus game-modules/
  deployment.json      paths, hashes, version, and launch mode
  game-modules/
    GameClient.dll     project loader/hook DLL
    fearvr-d3d9.dll    project D3D9 bridge
    GameOrig.dll       verified stock Public Tools GameClient
    GameServer.dll     stock Public Tools module
    ClientFx.fxd       stock Public Tools data
    FEAR.dep           stock Public Tools data
    FEARMod.Arch00s    stock Public Tools archive
  userdata/            saves and mutable game state
  logs/                one directory per run
```

The five proprietary Public Tools files are copied from the user's own
installation. They are deliberately absent from release packages. The retail
game directory is read but never modified.

Startup proceeds as follows:

1. `play.ps1` validates `deployment.json`, the archive configuration, and all
   staged module hashes.
2. It resolves `active`, `steamvr`, `vdxr`, or an explicit runtime manifest.
   An `XR_RUNTIME_JSON` override is scoped only to the new host process.
3. It creates a non-zero 64-bit session ID and a new run log directory.
4. It starts `fearvr-host.exe --ipc-session <ID>` and waits up to 30 seconds
   for the structured `xr_ready` event.
5. The host creates its OpenXR instance and input actions, chooses the runtime's
   required graphics adapter, creates D3D11, then creates the session, local
   reference space, and one swapchain per eye.
6. The launcher starts F.E.A.R. with the same session ID, the isolated
   `-archcfg`, the isolated `-userdirectory`, and feature switches.
7. LithTech loads the staged GameClient loader. The loader delegates to
   `GameOrig.dll`, loads the bridge, and installs only the hooks whose
   signatures and interfaces validate.
8. The bridge creates the session-named shared mapping and events on its first
   relevant game call. The already-running host polls for and opens them.
9. Heartbeats and adapter LUIDs are exchanged. Image transfer is enabled only
   after the protocol and adapter match.

For Steam installations, Steam remains the store/activation launcher. SteamVR
is not required when another OpenXR runtime is selected. SteamVR theater
suppression is only run when the selected runtime is actually SteamVR.

## 5. The frame pipeline

The game and OpenXR run at independent rates. A request produced during an XR
frame can be rendered by a later game frame, and the resulting image can be
submitted by a still later XR frame. Frame IDs preserve causality across those
boundaries.

```text
OpenXR host                 GameClient hooks           D3D9 bridge
     |                            |                         |
     | xrWaitFrame               |                         |
     | xrLocateViews             |                         |
     | publish request N --------+------------------------>|
     |                            | read request N          |
     |                            | render left world       |
     |                            |------------------------> capture left
     |                            | render right world      |
     |                            |------------------------> capture right
     |                            |                         |
     |                            | Retail draws HUD once   |
     |                            | Retail calls Present -->|
     |                            |                         | merge HUD/final UI
     |                            |                         | fill slot pair N
     |                            |                         | D3D9 query -> READY
     | claim newest READY pair N <-------------------------|
     | CopyResource to private D3D11 eye textures          |
     | D3D11 query -> release slots                         |
     | render textures into XR swapchains                  |
     | submit using the stored pose/FOV for request N      |
     | xrEndFrame                                           |
```

### 5.1 Host-side frame work

Each running OpenXR frame:

1. waits and begins the frame;
2. synchronizes the OpenXR action set and publishes a physical controller
   snapshot;
3. consumes at most one new haptic request;
4. locates the two eye views at `predictedDisplayTime`;
5. scales FOV if requested and publishes a `FearVrRenderRequest`;
6. imports the newest complete image pair without blocking for the game;
7. draws the host's private eye textures into the OpenXR swapchains with a
   fullscreen D3D11 shader; and
8. submits either a stereo projection layer or a world-locked mono quad.

The host keeps a 256-entry render-pose history keyed by request frame ID. When
image `N` arrives, its projection layer is submitted with the pose and FOV used
to render `N`, not the newest pose. This gives the OpenXR compositor correct
information for timewarp. If the XR display rate exceeds the game rate, the
host deliberately reuses the last imported image.

### 5.2 Game-side stereo rendering

The verified Retail 1.08 `ILTRenderer` layout has player-camera `RenderCamera`
in VTable slot 17 forwarding to the render implementation in slot 19. The
loader replaces only slot 17 after validating the exact forwarder bytes.

For a valid stereo request, the hook:

1. snapshots the original camera transform and FOV;
2. computes a shared symmetric FOV and relative eye poses;
3. composes HMD rotation and, when enabled, translation onto the game's
   existing camera basis;
4. sets the left-eye camera, clears only the render target, calls the original
   slot-19 world renderer, and captures the D3D9 back buffer;
5. repeats those rendering operations for the right eye;
6. restores the original camera transform and FOV; and
7. marks the pair complete with the request frame ID.

No game update is repeated. The duplicate call is confined to the world camera
render function. HUD, menus, simulation, animation updates, AI, audio, and
input remain on their original once-per-frame paths.

The hook falls back to the original mono call when stereo is disabled, the host
or request is stale, FOV/IPD validation fails, a technique override is active,
the call is recursive, or an eye render fails. Structured exception handling
restores camera state before the mono fallback if a Retail call faults.

## 6. Capturing, HUD compositing, and presentation

World images are captured immediately after each eye render. F.E.A.R. then
continues its normal frame and draws the HUD, menus, and post-world effects once
before calling `Present`.

At `Present`, the bridge chooses among three presentation modes:

| Mode | Source sent to host | OpenXR layer |
|---|---|---|
| Native stereo gameplay | Captured left/right worlds, optionally with post-world HUD deltas composed into both | Stereo projection |
| Menu, loading, comfort, or large full-screen effect | Final mono back buffer duplicated as needed | World-locked quad |
| No valid stereo frame | Final mono back buffer | World-locked quad |

### 6.1 HUD separation

The default `GpuHudCompositor` compares the final presented image with the
captured right-eye world using D3D9 pixel shaders. Pixels changed after the
world render are treated as candidate HUD pixels and are composed identically
over both eye images. HUD coordinates are continuously compressed toward the
center for a comfortable field of view.

A reduction chain estimates changed-pixel coverage. Its small result is read
one frame later to avoid a full-frame synchronization point:

- safe, limited coverage is treated as HUD;
- explicit menu state or high coverage is treated as a flat/full-screen frame;
- GPU compositor failure automatically selects the older CPU compositor.

This is a heuristic, not a native UI layer. Transparent UI edges already
contain right-eye background pixels, and full-screen effects must be kept flat
to avoid accidentally replacing most of the stereo world with mono content.

### 6.2 D3D9-to-D3D11 image transport

There are three shared texture slots per eye. Left and right slots with the same
index always form a pair and carry the same frame ID and generation.

For a D3D9Ex game device, the bridge can copy directly into D3D9 shared
render-target textures. Retail F.E.A.R. normally creates a classic D3D9 device,
so the current compatibility path:

1. reads each completed eye surface into system memory with
   `GetRenderTargetData`;
2. copies rows into a system-memory surface owned by a helper D3D9Ex device;
3. uploads that surface into a shared D3D9Ex texture; and
4. exposes its shared handle to D3D11.

The bridge marks this path with `FEARVR_BF_CPU_FALLBACK`. The GPU HUD compositor
removes HUD pixel processing from the CPU, but it cannot remove the classic
D3D9 readback. Eliminating that remaining per-eye readback requires making the
game device effectively D3D9Ex-compatible, including emulation of resources
that Retail creates in `D3DPOOL_MANAGED`.

## 7. IPC and synchronization

All named objects use:

```text
Local\FearVr.M2.<16-digit-session-id>.<suffix>
```

The mapping contains one `FearVrSharedHeader` from `src/common/protocol.h`.
Protocol version 5 currently carries:

| Direction | Payload | Synchronization |
|---|---|---|
| Host to game | Per-eye pose/FOV render request, predicted time, recenter generation, render flags | `requestSequence` seqlock |
| Host to game | Physical buttons, sticks, triggers, squeeze values, aim/grip poses, focus and validity | `inputSequence` seqlock |
| Game to host | Haptic hand mask, duration, amplitude, and frequency | `hapticSequence` seqlock |
| Game to host | Shared texture handles, dimensions, format, frame ID, generation, slot state | Atomic slot state machine plus D3D queries |
| Both directions | Heartbeats, process IDs, adapter LUIDs, feature/diagnostic flags | Atomic fields |

Only fixed-width POD fields cross the bitness boundary. The protocol does not
share pointers, native-width handles, STL objects, or Direct3D enums. Compile
time size assertions and runtime magic/version/structure-size checks protect
the layout.

### 7.1 Ring state machine

```text
EMPTY --game claims--> WRITING --D3D9 query complete--> READY
  ^                                                   |
  |                                                   |
  +--host D3D11 query complete-- CONSUMING <--host claims
```

The game claims both eyes atomically enough to preserve a pair; if either slot
is unavailable, it releases the other. The host sorts ready candidates by
frame ID, claims the newest matching pair, validates its handles and metadata,
then copies both shared sources into host-private D3D11 textures. Private
textures prevent a released ring slot from being overwritten while OpenXR is
still sampling the submitted image.

Neither render loop waits indefinitely:

- a full ring drops the new game frame;
- an unfinished D3D9 or D3D11 query is polled on later ticks;
- heartbeat timeouts disconnect the peer and recover slot states;
- `Present` allows only a short best-effort query poll before returning.

## 8. Input, motion controls, and haptics

`XrInput` owns one OpenXR action set and suggests bindings for Oculus Touch,
Valve Index, Microsoft Motion, HTC Vive, and the KHR simple controller. It
publishes physical state rather than game commands. This keeps runtime/profile
interpretation on the host and game semantics inside the GameClient.

The GameClient polls input from verified `IClientShell::Update` VTable slot 20:

1. reject invalid, unfocused, or stale samples and neutralize all VR controls;
2. mirror the complete input snapshot once when left-handed mode is enabled;
3. convert sticks, buttons, trigger, squeeze, and hand poses into F.E.A.R.
   semantic commands;
4. merge those values through the Retail binding path, leaving keyboard,
   mouse, and existing gamepad input available;
5. update gestures, climbing, menu navigation, weapon presentation, and
   interaction state; then
6. call the original client update exactly once.

The weapon and interaction hooks use the same controller-derived aim basis for
the visible weapon, projectile fire vectors, activation/pickup traces, and the
diagnostic aim guide. The support hand can latch onto a weapon and blend its
direction according to weapon length. Other extracted algorithms handle
weapon inertia, physical lean and collision, body follow, gesture melee,
hand-over-hand ladder input, and vertical camera smoothing.

Game events publish haptic requests in the opposite direction. The host accepts
each request ID once and calls `xrApplyHapticFeedback` only while the session is
focused. Left-handed input is mirrored back to physical hands when a haptic
mask is produced.

Detailed bindings and hook behavior are documented in
[OPENXR-INPUT.md](OPENXR-INPUT.md).

## 9. Head tracking and coordinate ownership

OpenXR poses remain in metres and OpenXR coordinates while crossing IPC. The
GameClient:

- derives a yaw-only recenter pose so recentering cannot tilt the world;
- converts from OpenXR's right-handed, negative-Z-forward basis to LithTech's
  game basis;
- applies per-eye pose relative to the recenter pose;
- composes tracking onto the Retail camera rather than replacing the game's
  camera; and
- optionally limits translation against world geometry while leaving the
  player's collision capsule under Retail control.

The OpenXR host owns final layer-space poses. The game owns conversion into
LithTech camera transforms. The complete axis and quaternion derivation is in
[COORDINATE-SYSTEM.md](COORDINATE-SYSTEM.md).

## 10. Menus and comfort mode

Verified Retail menu hooks add `VR Settings` to the native pause menu. Its root
opens six bounded subpages: Display & HUD, Movement & Comfort, Controls,
Weapons, Melee, and Advanced. The page/back state and numeric-preset selection
are isolated in `src/common/vr_menu_model.h`; the Retail adapter creates and
shows the native controls in `stereo_hook.cpp`. Settings are applied immediately
and persisted in `fearvr.ini`. The bridge receives explicit menu/comfort state
so it can clear `FEARVR_BF_STEREO_ACTIVE`.

When that flag is clear, the host renders the latest mono image into one eye
swapchain and submits it as a 2.4 m by 1.8 m quad anchored 2 m in front of the
view. The anchor is yaw-only and remains world-locked until a panel recenter is
requested. This same path covers startup, loading screens, normal menus,
comfort-mode cutscenes, and stereo failure.

## 11. Lifecycle, ownership, and recovery

| Event | Owner and response |
|---|---|
| OpenXR focus/session change | Host state machine begins/ends/restarts the session and neutralizes input when unfocused |
| Game starts after host | Bridge creates IPC; host opens it on a 250 ms retry loop |
| Host disappears | Game heartbeat check disables transport, recovers ring slots, and continues flat |
| Game disappears | Host heartbeat check disconnects; production launch uses `--exit-on-game-disconnect` |
| D3D9 `Reset` or resolution change | Bridge releases all default-pool, capture, compositor, and shared resources, then lazily recreates them |
| Shared handle changes | Host's per-slot cache detects the new handle and reopens only that source |
| Adapter mismatch | Shared-texture transfer remains disabled; the mismatch is logged |
| Protocol mismatch | IPC is rejected and the protocol error flag is set |
| Unsupported Retail build/layout | The specific hook remains untouched; less invasive features continue where possible |
| Ring pressure | The producer drops frames; neither process blocks the other's render loop |

## 12. Code organization and test boundaries

`src/common` is more than an IPC include directory. It is the portability and
testability boundary for logic that does not need LithTech, D3D, or OpenXR:

- protocol validation and ring selection;
- OpenXR session transitions;
- stereo FOV/IPD and head-tracking math;
- controller mapping and input freshness;
- HUD coverage/coordinate math;
- two-handed grip and weapon-weight filters;
- melee gesture/action classification;
- ladder grip motion;
- physical lean collision/body follow; and
- vertical camera height selection.

Each of these areas has a corresponding native unit test in `tests/`. The x86
test build additionally contains a synthetic D3D9 producer and an IAT-hook test.
Headset/runtime tests, lifecycle tests, and performance acceptance procedures
are described in [TESTING.md](TESTING.md).

Builds are intentionally split by bitness:

```text
build/x86: fearvr-d3d9.dll, GameClient.dll, x86 tools/tests
build/x64: fearvr-host.exe, x64 tests
```

The root CMake configuration rejects a host in an x86 build or a bridge in an
x64 build.

## 13. Safety and compatibility boundaries

The main defensive boundaries are:

- no heavy work in `DllMain`;
- no writes to the retail installation;
- fixed-width and runtime-validated cross-process structures;
- adapter LUID equality before opening shared images;
- heartbeat and focus freshness checks;
- byte signatures and executable-address checks before Retail hooks;
- camera/FOV restoration on normal, failed, and exception paths;
- no duplicate game update; and
- flat/mono fallback at every external boundary.

The GameClient hook implementation is necessarily large because many Retail
features share version-specific objects and lifecycle boundaries. New
calculation-heavy behavior should continue to be extracted into `src/common`
and covered by tests; `stereo_hook.cpp` should remain the adapter that reads
verified Retail state and applies those tested decisions.

## 14. Known architectural debt

1. Retail's classic D3D9 device still requires two per-frame CPU readbacks for
   stereo transport. This is the largest structural performance limitation.
2. HUD separation is image-difference based. A native HUD render target or
   OpenXR UI layer would avoid right-eye contamination at transparent edges.
3. Most GameClient integrations are tied to verified Retail 1.08 byte patterns,
   RVAs, and VTable layouts. Supporting another executable requires a separate
   validated layout, not a relaxed check.
4. `src/gameclient_loader/stereo_hook.cpp` contains several integration
   concerns in one translation unit. The pure algorithms are separated, but
   the Retail adapters could be split further without changing runtime
   boundaries.
5. Production startup is implemented in PowerShell. `src/launcher` remains an
   early scaffold and should not be mistaken for the shipped orchestrator.

## 15. Where to make changes

| Change | Start here |
|---|---|
| OpenXR instance/session/frame/layer behavior | `src/host64/openxr_host.cpp` |
| OpenXR actions and interaction-profile bindings | `src/host64/xr_input.cpp` |
| D3D11 swapchain texture drawing | `src/host64/texture_renderer.cpp` |
| Host side of shared-texture import | `src/host64/ipc_bridge.cpp` |
| Protocol fields or ring layout | `src/common/protocol.h`, then both IPC implementations and `tests/test_protocol.cpp` |
| D3D9 hooks, capture, HUD, or shared-texture production | `src/proxy32/bridge.cpp` |
| Game camera, weapon, interaction, menu, or input hooks | `src/gameclient_loader/stereo_hook.cpp` |
| Coordinate/FOV/tracking rules | `src/common/head_tracking_math.h`, `src/common/stereo_math.h` |
| Controller or gesture behavior | the relevant header in `src/common/`, its test, then the GameClient adapter |
| Installed file layout and launch sequence | `tools/release/install.ps1`, `tools/release/play.ps1` |
| Design rationale and measured alternatives | `docs/ARCHITECTURE.md` |

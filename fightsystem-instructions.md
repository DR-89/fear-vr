# Melee combat on motion controllers — implementation plan

Status: **M1, M3, M4 and M5 implemented; M2's static probes are implemented
and await one in-game validation session.** The forward-thrust detector lives in
`src/common/melee_thrust.h`; the two-hand classifier, shared cooldown and
250 ms jump queue live in `src/common/melee_actions.h`. Jump kick only rides a
real Retail jump and never injects `JUMP`. Slide kick accepts either hand and
either physical crouch or stick-DUCK, behind the documented hard guards.

Goal: reach F.E.A.R.'s four close-combat moves — **weapon strike**, **punch**,
**jump kick** and **slide kick** — from hand and head motion instead of from a
key, and keep every one of them switchable off.

---

## 1. What the retail game actually does

This is the part that decides the whole design, so it is stated first and with
sources. All four moves come from **one** command. F.E.A.R. does not have a
"jump kick button".

`COMMAND_ID_ALT_FIRING` (19) sets `BC_CFLG_ALT_FIRING`
(`CMoveMgr.cpp:1137`). `CPlayerMgr::CanAltFireWeapon()`
(`PlayerMgr.cpp:6746`) turns that into the weapon state `W_ALT_FIRING`, which
plays the weapon's alt-fire animation — **that is the weapon strike**.

The same state raises the animation property `kAP_ACT_FireSecondary`, and
`CPlayerBodyMgr::UpdateActionProperties()`
(`PlayerBodyMgr.cpp:2655`) then picks the *variant* purely from the player's
current movement state:

| Retail condition (all with alt-fire raised) | Resulting move |
|---|---|
| crouching **and** moving forward **and** `InPostureDownWindow()` | `kAP_ACT_SlideKick` |
| jumping or falling, no movement direction | `kAP_ACT_JumpIdleKick` |
| jumping or falling, direction forward | `kAP_ACT_JumpRunKick` |
| anything else | weapon's own alt-fire animation (strike) |

Two timing facts matter:

- `InPostureDownWindow()` is true only inside `PostureDownTime` after the
  **rising edge** of `BC_CFLG_DUCK` (`CMoveMgr.cpp:1206`, `CMoveMgr.h:174`).
  The duration comes from the player-movement database record, so it must be
  measured in-game, not guessed.
- The jump variants need the player to actually be airborne
  (`kAP_MOV_Jump` / `kAP_MOV_Fall`), which starts one frame after the rising
  edge of `BC_CFLG_JUMP` (`CMoveMgr.cpp:1199`).

**Consequence for this mod:** we cannot request a specific kick. We can only
put the player into the matching movement state and raise alt-fire inside the
right window. Every kick is therefore a small, timed command sequence, not a
single injection.

### The punch is the one move retail does not have

There is no unarmed state in F.E.A.R.'s single-player inventory, and
`IsMeleeWeapon()` (`ClientWeapon.h:348`) only checks whether the *current*
weapon deals `DT_MELEE` damage. A separate fist attack does not exist as its
own action. Realistically we have two options, and this needs a decision
before implementation starts:

- **A (recommended):** the off-hand thrust triggers the same secondary attack.
  Mechanically identical to the weapon strike, but it lets the player throw a
  punch with the free hand and get a hit. Honest framing in the docs: "off-hand
  strike", not "punch".
- **B:** switch to a melee-damage weapon on the gesture and back afterwards.
  Two extra weapon switches per punch, visible in the HUD, and it fights with
  the weapon-switch gesture. Not recommended.

---

## 2. What VR can observe

Available every frame in `g_currentInput` and `g_headTracking`:

- both hand grip and aim poses, in room space, in meters;
- both grip/trigger values and all buttons;
- HMD pose, therefore head **height** and head velocity;
- the existing derived state: two-handed grip, ladder state, weapon disabled.

Not available: feet, knees, hips. **Both kicks must be inferred**, either from
a hand gesture plus a movement state, or from head motion. This is the second
design constraint, and it is why the gesture catalogue below never asks the
player to actually kick.

---

## 3. Gesture catalogue

| Move | Gesture | Retail sequence to produce |
|---|---|---|
| Weapon strike | forward thrust of the weapon hand (**already implemented**) | pulse `ALT_FIRING` (200 ms) |
| Off-hand strike ("punch") | forward thrust of the off-hand, grab button **not** held | pulse `ALT_FIRING` |
| Jump kick | thrust while airborne, or thrust within ~250 ms before becoming airborne | wait for Retail's existing airborne state, then pulse `ALT_FIRING`; never inject `JUMP` |
| Slide kick | crouch (real or stick) while moving forward, then thrust | `DUCK` edge + hold `FORWARD` + pulse `ALT_FIRING` inside `PostureDownTime` |
| Manual melee | right stick click in the 3D world | pulse `ALT_FIRING` for 200 ms; Retail selects strike, jump kick or slide kick from the current movement state |

Notes on the two kicks:

- **Jump kick** should not inject a jump on its own. If the player is already
  airborne, the thrust is enough. If they thrust and *then* press jump within
  a short window, the sequencer may hold the alt-fire pulse until the airborne
  state arrives — that is a queued pulse, not an injected jump.
- **Slide kick** is the only move that needs an injected `DUCK` edge, because
  the posture window opens on that edge. Restrict it hard: only while
  sprinting forward, only when the player is on the ground, and only when the
  crouch gesture (physical squat, or the existing stick-down) was performed.
  Without those guards the mod would randomly duck during a firefight.

---

## 4. Architecture

Follow the pattern that worked for `melee_thrust.h` and `climb_grip.h`:
**pure, headset-free logic in `src/common/`, plumbing in the GameClient.**

### New file: `src/common/melee_actions.h`

Pure state machine, no Windows, no engine types. Input per frame:

```
struct MeleeActionInput {
    FearVrInputState input;      // already handedness-mirrored
    uint64_t nowNs;              // caller's monotonic clock
    float headHeightMeters;      // for the crouch gesture
    bool headHeightValid;
    bool movementAvailable;      // all kicks fail closed without Retail state
    bool airborne;               // from retail movement state
    bool onGround;
    bool movingForward;          // stick or physical
    bool sprinting;
    bool postureDownWindow;
    bool stickCrouch;
    bool aimingDownSights;
    bool onLadder;
    bool weaponDisabled;         // no melee while retail disabled the weapon
    bool offHandHoldingWeapon;
    bool weaponStrikeEnabled;
    bool offHandStrikeEnabled;
    bool jumpKickEnabled;
    bool slideKickEnabled;
};

enum class MeleeAction { None, WeaponStrike, OffHandStrike, JumpKick, SlideKick };

struct MeleeActionRequest {
    MeleeAction action;
    bool needsDuckEdge;          // slide kick only
    bool needsForwardHold;       // slide kick only
};
```

Reuse `MeleeThrustDetector` per hand — it already handles direction, speed
threshold, re-arming and cooldown. The new logic on top is only:

1. classify which hand thrust;
2. read the movement state and pick the variant;
3. emit a request with the required accompanying commands.

Keep one shared cooldown across all four moves so a single lunge cannot fire a
strike and a kick.

### New file: `src/gameclient_loader/` plumbing (in `stereo_hook.cpp`)

A small sequencer, because the kicks are multi-frame:

- `UpdateMeleeActions()` in `HookClientShellUpdate`, next to
  `PrepareMeleeThrustPulse()` (which it replaces);
- pulse windows per command, like the existing `g_meleePulseUntil`;
- `ResolveInjectedCommand()` already exists and is the single place where
  `FORWARD`, `REVERSE`, `DUCK`, `JUMP` and `ALT_FIRING` are decided — extend
  it, do not add a second injection path.

### Retail state used by the sequencer

The two required facts were located with byte patterns and independent
cross-checks, the same way `LadderMgr` was (see `docs/OPENXR-INPUT.md`):

1. **airborne** — `CMoveMgr::m_bJumped` and `m_bFalling`, verified at offsets
   `+0x78` and `+0x66`.
2. **posture-down window** — the real DUCK edge from `m_dwControlFlags` plus
   the initialized `PostureDownTime` console variable; the duration is never
   assumed.

All probes fail closed. **Airborne is never faked from HMD motion** — if the
Retail state cannot be verified, normal strikes remain available and kicks
stay off.

---

## 5. Milestones

Each one ends in a state that can be built, tested and shipped.

**M1 — Off-hand strike.** Second `MeleeThrustDetector` for the off-hand,
suppressed while that hand holds the weapon (two-handed grip) or grabs a
ladder. Shared cooldown with the weapon hand. Fully unit-testable.
*Risk: low. No new retail knowledge needed.*

**M2 — Crouch and airborne state.** Locate the two retail facts above, verify
by pattern, log them under a diagnostic event for one play session. No
behaviour change yet.
*Risk: medium, pure reverse engineering.*

**M3 — Jump kick.** Queue an alt-fire pulse when a thrust happens airborne, or
shortly before becoming airborne. No injected jump. **Implemented:** a ground
thrust waits at most 250 ms; it becomes a jump kick if Retail reports airborne
inside that window, otherwise the original hand's normal strike is emitted.
*Risk: low once M2 is in.*

**M4 — Slide kick.** The full sequence: crouch gesture (head height drop of
≥ 25 cm within 400 ms, or stick-down) while `movingForward` and `onGround`,
inject `DUCK` edge, hold `FORWARD`, pulse `ALT_FIRING` inside the measured
window. Hard guards: not while aiming down sights, not on a ladder, not while
the weapon is disabled, and a cooldown of at least a second.
**Implemented:** physical crouch injects the short DUCK edge; stick-DUCK must
already be inside Retail's measured posture-down window. Both paths require
Retail RUN + FORWARD + on-ground, accept a thrust from either free hand, hold
FORWARD and ALT_FIRING for 200 ms, and share a 1 s cooldown.
The GameClient preserves the local camera rotation captured immediately
before the action and also overwrites Retail's cached target-attach rotation.
Retail normally carries that final animated camera-socket pitch into the
permanent view after a slide kick; VR suppresses only that animation rotation
while retaining the animated body, camera height and body yaw. Stabilization
holds the original pitch basis for five seconds after the latest slide kick.
Overlapping kicks extend that interval without capturing a new basis, so an
intermediate animated downward pitch can never become the next neutral view.
Only pitch and roll are preserved; Retail's current local yaw is merged back
in every frame so snap-turn remains responsive during the hold. The view
therefore remains controlled by the HMD throughout the move and the animation
exit cannot restore a downward pitch.
*Risk: highest — this one injects movement commands.*

**M5 — Options and docs.** One VR-menu entry `Melee: GESTURES / CLASSIC`, plus
`fearvr.ini` keys for the individual moves so a player can keep strikes but
disable kicks. Update `docs/OPENXR-INPUT.md` and both READMEs.
**Implemented:** `GESTURES` is the fresh-config default. The master menu switch
and the four default-on keys `MeleeWeaponStrike`, `MeleeOffHandStrike`,
`MeleeJumpKick` and `MeleeSlideKick` are persisted independently.

---

## 6. Test plan

**Without a headset** (`tests/test_melee_actions.cpp`, run in CI):

- each gesture produces exactly the expected action, once;
- a thrust while airborne yields JumpKick; the same thrust on the ground waits
  250 ms, then yields WeaponStrike unless a real jump upgrades it;
- slide kick is refused when not moving forward, when airborne, on a ladder,
  and while the weapon is disabled;
- off-hand thrust is refused while that hand holds the weapon;
- the shared cooldown blocks a second action inside the window;
- replayed frames (same timestamp) and frame gaps > 100 ms produce nothing —
  the failure mode that made the first melee gesture never fire.

**In the headset**, per move: does it trigger, does it trigger *only* when
intended, and does the animation match the move. Specifically watch for the
two known false-positive sources: reloading motions producing a strike, and
combat crouching producing an unwanted slide.

For jump and slide kick, also verify first-person body visibility with
`Show arms: OFF`: upper and lower arms are absent, while hands, torso and both
legs remain visible throughout the animation. Toggle `Show arms` to `ON` and
back to `OFF` once, then relaunch and verify that the last choice persists.
**Validated in-headset on 27 July 2026:** both toggle directions worked; with
arms off, hands, torso and legs remained visible, and `ShowArms=0` persisted.
Also verify after several slide kicks that looking straight ahead before the
move still means looking straight ahead afterwards. The body and height motion
must remain visible, but the animation must never pitch the HMD view.

**Diagnostics:** keep the `melee_thrust_peak` pattern — every 3 seconds log
peak speed, sample count and which action the classifier would have chosen.
Without it, a gesture that does not fire cannot be told apart from a command
that does nothing, which cost two full test rounds already.

---

## 7. Risks and mitigations

| Risk | Mitigation |
|---|---|
| Injected `DUCK`/`FORWARD` fights the player's own input | Only inject on an explicit gesture, never continuously; release immediately after the pulse |
| Wrong kick variant plays (state mistimed) | Read the real retail state (M2) instead of guessing; if unreadable, disable kicks |
| Gestures fire during normal play (reload, hand raise) | Reuse the proven direction + speed + alignment test, one shared cooldown, and keep `Melee: CLASSIC` available |
| Retail structure offsets drift | Same rule as everywhere in this project: verify by byte pattern, and leave the feature off on mismatch |
| Motion sickness from injected movement | Slide kick is the only one that moves the body; keep it individually switchable |
| Slide-kick animation leaves the view pitched downward | Preserve Retail's pre-kick local rotation and cached target-attach rotation for five seconds after the latest kick; overlapping kicks extend the hold without recapturing |
| Camera wobbles while the player collides with geometry | In native VR use Retail's raycast anti-clipping path instead of its moving camera-collision model; restore the original path in 2D/mono |
| Camera twitches after locomotion or on stairs | Do not inject lean-body locomotion axes; keep Retail's original 275 ms height smoothing for upward/duck changes, but visually bypass its trailing height while descending or airborne |
| Lean-body following counter-steers, oscillates or leaves the view behind the body | Do not inject locomotion axes for physical lean; move only view, hands, weapon and muzzle through the world-limited head offset |
| Hands and weapon shift vertically after jumping or ducking | In native 3D VR build tracked-hand transforms from the final camera-object position after Retail height/collision correction; leave Retail's own weapon-update base unchanged |

---

## 8. Design decisions

1. Off-hand strike uses option A: the same Retail secondary attack, without a
   visible weapon switch.
2. Jump kick only rides an existing player jump and never injects `JUMP`.
3. A physical squat and stick-down both qualify for slide kick; the sprint,
   forward, ground, ADS, ladder and weapon guards remain mandatory.
4. Either free hand may trigger kicks.
5. Fresh configurations default to `Melee: GESTURES`, with all four individual
   actions enabled.
6. `Show arms` uses option A: the menu label describes visibility, defaults to
   `OFF`, and persists. The OFF material masks only upper/lower arms; hands,
   torso and legs stay visible so kick animations remain observable.
7. Slide-kick camera handling uses option A: body and height animation remain,
   while animation-driven camera rotation is suppressed. HMD tracking alone
   controls the view direction and Retail cannot retain the final downward
   pitch.
8. Repeated slide kicks use the original pre-sequence pitch basis and extend
   its five-second hold instead of capturing an intermediate animation frame.
9. Right stick click uses Retail's secondary attack as manual melee in the 3D
   world, independent of gesture detection. In menus and other 2D views the
   same click re-anchors the flat panel instead; manual 3D recenter is removed.
10. Every manual melee click captures the stable camera basis before pulsing
    Retail. The slide-kick posture becomes observable only after that input
    frame, so waiting to classify the variant would capture the animated pitch
    too late. Ground strikes and jump kicks use the same harmless protection.
11. Camera stabilization locks only the pre-attack pitch and roll. Current
    Retail yaw is retained every frame, allowing immediate left/right turning
    throughout the five-second protection interval.
12. Native 3D VR sets `CameraCollisionUseObject=0`, selecting Retail's existing
    raycast anti-clipping path. Menus, comfort view and mono restore the
    captured original value. No post-render camera lag filter is introduced.
13. Superseded by decision 19: native 3D VR initially disabled
    `CameraSmoothingEnabled`, before the actual post-movement feedback was
    traced to lean-body locomotion axes.
14. Superseded by decision 18: the initial lean-body controller used a 300 ms
    post-locomotion grace period.
15. Superseded by decision 18: its view compensation was changed from filtered
    to same-frame.
16. Superseded by decision 18: its return path temporarily allowed the view
    behind the returning body and is no longer active.
17. Jump/duck hand stability uses option A: OpenXR hand and weapon transforms
    use the final rendered camera-object position in native 3D VR. Retail's
    pre-correction `CPlayerCamera::m_vPos` still goes to Retail's own update,
    and 2D/mono behavior remains untouched.
18. Physical lean body handling uses the later option A: no movement axes are
    injected and Retail's body/collision capsule stay at the player position.
    The world-limited physical offset still moves the view, both hands, weapon
    and muzzle. This removes controller feedback, return oscillation and every
    path that could leave the view behind the body.
19. Stair handling uses option A: F.E.A.R.'s original
    `CameraSmoothingEnabled` value is left untouched. Its 275 ms interpolation
    is specifically gated to grounded movement/duck height changes and smooths
    discrete stair steps. The earlier locomotion twitch cannot return through
    the removed lean-body axis path.
20. Directional stair handling uses the follow-up option A: grounded upward
    steps and ducking render Retail's final, smoothed camera height. Descending
    steps and airborne motion instead render the raw `CPlayerCamera::m_vPos`
    height until Retail catches it within 0.25 game units. A difference above
    35 units is treated as a possible floor/ceiling collision and keeps the
    final anti-clipping height for that whole event. Eyes, hands and weapon all
    consume the same selected visual height; Retail's internal smoothing and
    weapon-update input remain unchanged.

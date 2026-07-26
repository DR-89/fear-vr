#pragma once

#include <algorithm>
#include <cmath>

#include "head_tracking_math.h"
#include "input_state.h"
#include "protocol.h"

namespace fearvr {

// Holding the weapon with the off-hand: the left grab button engages only
// while the left hand actually sits on the weapon, i.e. in front of the right
// hand and close to its aim axis. Everywhere else the same button keeps its
// usual meaning (sprint), so a grab in empty air never steers the weapon.

// The grab engages above this and releases below the second value. The gap
// keeps a hand resting at the trigger point from flickering between sprint
// and two-handed hold.
constexpr float kTwoHandEngageSqueeze = 0.65F;
constexpr float kTwoHandReleaseSqueeze = 0.45F;
// Reach along the right hand's aim axis, in meters. A support hand behind the
// firing hand is not a fore-end grip, and beyond 60 cm no F.E.A.R. weapon has
// anything left to hold.
constexpr float kTwoHandMinForwardMeters = 0.05F;
constexpr float kTwoHandMaxForwardMeters = 0.60F;
// Perpendicular distance from that axis. Wider than the widest weapon so a
// natural, slightly offset support hand still counts.
constexpr float kTwoHandMaxLateralMeters = 0.22F;
// Below this hand separation the line between both hands is dominated by
// tracking noise and must not steer anything.
constexpr float kTwoHandMinSteerSeparationMeters = 0.12F;
// cos(50 degrees). The support direction has to stay roughly on the barrel;
// a wilder angle means the hands are not on one weapon.
constexpr float kTwoHandMinSteerAlignment = 0.64F;

// How much the line between both hands takes over the aim direction, by
// weapon length. A pistol keeps following the right hand alone; a rifle is
// carried by both hands, which is what makes the off-hand worth using.
//
// Die Grenzen stammen aus gemessenen Werten, nicht aus einer Schaetzung: Das
// Log `two_handed_grip_active` und die Muendungsdiagnose ergaben am 26.07.2026
// rund 13 cm fuer eine Pistole und 37 cm fuer ein Gewehr. Die erste Fassung
// setzte 28 bis 55 cm an und gab dem Gewehr deshalb nur 0.19 — der Benutzer
// merkte von der Stuetzhand fast nichts. Die Rampe liegt jetzt zwischen
// Pistole und Gewehr, damit ein Gewehr den vollen Anteil bekommt.
constexpr float kTwoHandShortWeaponMeters = 0.20F;
constexpr float kTwoHandLongWeaponMeters = 0.35F;
// 0.85 war zu viel, solange die Waffe beim Zugreifen noch sprang: Die rechte
// Hand hatte dann keine Wirkung mehr. Mit dem gemerkten Winkelversatz gibt es
// diesen Sprung nicht mehr, und 0.75 laesst die Stuetzhand deutlich fuehren,
// ohne der Waffenhand den Drehpunkt zu nehmen.
constexpr float kTwoHandMaximumBlend = 0.75F;

// Position of the left grip in the right hand's aim frame.
struct TwoHandedSupportOffset {
    float forwardMeters{0.0F};
    float lateralMeters{0.0F};
    bool valid{false};
};

inline TwoHandedSupportOffset LeftHandSupportOffset(
    const FearVrInputState& state) noexcept {
    constexpr std::uint32_t kBothHands =
        FEARVR_HAND_MASK_LEFT | FEARVR_HAND_MASK_RIGHT;
    if ((state.activeHands & kBothHands) != kBothHands ||
        (state.gripPoseValidHands & kBothHands) != kBothHands ||
        (state.aimPoseValidHands & FEARVR_HAND_MASK_RIGHT) == 0) {
        return {};
    }
    const FearVrPose& leftGrip = state.handGripPose[FEARVR_HAND_LEFT];
    const FearVrPose& rightGrip = state.handGripPose[FEARVR_HAND_RIGHT];
    const FearVrPose& rightAim = state.handAimPose[FEARVR_HAND_RIGHT];
    if (!IsValidPose(leftGrip) || !IsValidPose(rightGrip) ||
        !IsValidPose(rightAim)) {
        return {};
    }

    // OpenXR poses are right-handed with -Z forward, so the forward reach is
    // the negated Z component once the offset is expressed in the aim frame.
    const TrackingVector offset = Rotate(
        Conjugate(PoseRotation(rightAim)),
        {leftGrip.px - rightGrip.px, leftGrip.py - rightGrip.py,
         leftGrip.pz - rightGrip.pz});
    if (!IsFinite(offset)) {
        return {};
    }
    const float lateral =
        std::sqrt(offset.x * offset.x + offset.y * offset.y);
    if (!std::isfinite(lateral)) {
        return {};
    }
    return {-offset.z, lateral, true};
}

// True while the left hand is placed where a fore-end grip would be.
inline bool IsLeftHandOnWeapon(
    const FearVrInputState& state) noexcept {
    const TwoHandedSupportOffset offset = LeftHandSupportOffset(state);
    return offset.valid &&
           offset.forwardMeters >= kTwoHandMinForwardMeters &&
           offset.forwardMeters <= kTwoHandMaxForwardMeters &&
           offset.lateralMeters <= kTwoHandMaxLateralMeters;
}

// Der Griff ist ein Riegel, keine Dauerpruefung.
//
// Zuerst wurde die Handgeometrie in jedem Bild neu bewertet. Driftete die Hand
// an der Zonengrenze, schaltete der Griff im Wechsel an und aus — und mit ihm
// die Sperre fuer Sprint und Lehnen. Beim beidhaendigen Halten ist die linke
// Hand fast immer geneigt, also lehnte sich der Spieler bei jedem Aussetzer
// kurz zur Seite: die Sicht verschob sich. Benutzerbefund vom 26.07.2026.
//
// Gegriffen wird deshalb einmal beim Druecken; danach haelt der Griff, bis der
// Knopf losgelassen wird. So greift man auch im Echten.
inline bool ShouldEngageTwoHandedGrip(
    const FearVrInputState& state) noexcept {
    if ((state.flags & FEARVR_IF_FOCUSED) == 0) {
        return false;
    }
    const float squeeze = state.squeeze[FEARVR_HAND_LEFT];
    return std::isfinite(squeeze) &&
           squeeze >= kTwoHandEngageSqueeze &&
           IsLeftHandOnWeapon(state);
}

// Losgelassen wird allein ueber den Knopf — die Hand darf sich waehrend des
// Haltens frei bewegen, sonst waere der Griff wieder unstet.
inline bool ShouldReleaseTwoHandedGrip(
    const FearVrInputState& state) noexcept {
    if ((state.flags & FEARVR_IF_FOCUSED) == 0) {
        return true;
    }
    const float squeeze = state.squeeze[FEARVR_HAND_LEFT];
    return !std::isfinite(squeeze) ||
           squeeze < kTwoHandReleaseSqueeze;
}

// Weight of the hand-to-hand line in the aim direction, from the distance
// between the weapon's origin and its muzzle.
inline float TwoHandedAimBlend(float barrelLengthMeters) noexcept {
    if (!std::isfinite(barrelLengthMeters) ||
        barrelLengthMeters <= kTwoHandShortWeaponMeters) {
        return 0.0F;
    }
    const float span =
        kTwoHandLongWeaponMeters - kTwoHandShortWeaponMeters;
    const float ramp =
        (barrelLengthMeters - kTwoHandShortWeaponMeters) / span;
    return kTwoHandMaximumBlend *
           std::clamp(ramp, 0.0F, 1.0F);
}

} // namespace fearvr

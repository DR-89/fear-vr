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
// Wie weit die Handlinie von der Waffenachse abweichen darf.
//
// Vorher war das ein einziger harter Wert bei 50 Grad: Wanderte die
// Stuetzhand darueber hinaus, blieb die Waffe schlagartig stehen — eine Wand
// mitten in der Bewegung, obwohl niemand beim Halten eines Gewehrs zentimeter-
// genau arbeitet. Benutzerbefund vom 28.07.2026.
//
// Jetzt sind es zwei Werte. Bis `Soft` folgt die Waffe eins zu eins. Darueber
// wird der Ueberschuss weich zusammengedrueckt und laeuft asymptotisch gegen
// `Max`, ohne ihn je zu erreichen. Schlechtes Halten wirkt dadurch immer noch
// weniger, blockiert aber nichts mehr, und es gibt keinen Punkt, an dem die
// Waffe stehenbleibt oder zurueckspringt.
constexpr float kTwoHandSoftSteerRadians = 0.96F;  // rund 55 Grad
constexpr float kTwoHandMaxSteerRadians = 1.57F;   // rund 90 Grad
// A Retail reload/weapon-switch animation can move the animated off-hand far
// below or even beyond the muzzle. Such a transient pose must never become the
// rigid support attachment. The real fore-end points broadly toward the muzzle
// and ends no more than a small model-space tolerance beyond it.
constexpr float kTwoHandGripMinimumMuzzleAlignment = 0.35F;

// Position of the left grip in the right hand's aim frame.
struct TwoHandedSupportOffset {
    float forwardMeters{0.0F};
    float lateralMeters{0.0F};
    bool valid{false};
};

struct TwoHandedPivotTranslation {
    TrackingVector primaryPosition;
    TrackingVector correction;
    bool valid{false};
};

inline bool IsPlausibleSecondaryGripGeometry(
    const TrackingVector& supportOffset,
    const TrackingVector& muzzleOffset,
    float beyondMuzzleTolerance) noexcept {
    if (!IsFinite(supportOffset) || !IsFinite(muzzleOffset) ||
        !std::isfinite(beyondMuzzleTolerance) ||
        beyondMuzzleTolerance < 0.0F) {
        return false;
    }
    const float supportLengthSquared =
        supportOffset.x * supportOffset.x +
        supportOffset.y * supportOffset.y +
        supportOffset.z * supportOffset.z;
    const float muzzleLengthSquared =
        muzzleOffset.x * muzzleOffset.x +
        muzzleOffset.y * muzzleOffset.y +
        muzzleOffset.z * muzzleOffset.z;
    if (!(supportLengthSquared > 0.0F) ||
        !(muzzleLengthSquared > 0.0F)) {
        return false;
    }
    const float supportLength = std::sqrt(supportLengthSquared);
    const float muzzleLength = std::sqrt(muzzleLengthSquared);
    const float dot =
        supportOffset.x * muzzleOffset.x +
        supportOffset.y * muzzleOffset.y +
        supportOffset.z * muzzleOffset.z;
    const float alignment = dot / (supportLength * muzzleLength);
    return std::isfinite(alignment) &&
           alignment >= kTwoHandGripMinimumMuzzleAlignment &&
           supportLength <= muzzleLength + beyondMuzzleTolerance;
}

// Translate a rigid two-grab object so its stored secondary attachment lands
// on the tracked secondary hand. Rotation is solved separately; once that
// rotated attachment offset is known, this makes the support hand the pivot
// without scaling the object or changing the distance between its grips.
// Units are deliberately arbitrary as long as all three inputs match.
inline TwoHandedPivotTranslation SolveSecondaryGripPivot(
    const TrackingVector& primaryPosition,
    const TrackingVector& secondaryPosition,
    const TrackingVector& rotatedSecondaryOffset,
    float influence = 1.0F) noexcept {
    TwoHandedPivotTranslation result{};
    if (!IsFinite(primaryPosition) || !IsFinite(secondaryPosition) ||
        !IsFinite(rotatedSecondaryOffset) || !std::isfinite(influence)) {
        return result;
    }
    const float amount = std::clamp(influence, 0.0F, 1.0F);
    const TrackingVector predictedSecondary{
        primaryPosition.x + rotatedSecondaryOffset.x,
        primaryPosition.y + rotatedSecondaryOffset.y,
        primaryPosition.z + rotatedSecondaryOffset.z};
    result.correction = {
        (secondaryPosition.x - predictedSecondary.x) * amount,
        (secondaryPosition.y - predictedSecondary.y) * amount,
        (secondaryPosition.z - predictedSecondary.z) * amount};
    result.primaryPosition = {
        primaryPosition.x + result.correction.x,
        primaryPosition.y + result.correction.y,
        primaryPosition.z + result.correction.z};
    result.valid = IsFinite(result.primaryPosition) &&
        IsFinite(result.correction);
    return result;
}

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

// Der zulaessige Lenkwinkel zu einem gemessenen Winkel zwischen Handlinie und
// Waffenachse. Unterhalb der weichen Grenze bleibt der Winkel unveraendert,
// darueber waechst er nur noch gedaempft weiter — die Steigung am Uebergang
// ist genau 1, der Verlauf also knickfrei.
inline float SoftLimitedSteerAngle(float angleRadians) noexcept {
    if (!std::isfinite(angleRadians)) {
        return kTwoHandSoftSteerRadians;
    }
    if (angleRadians <= kTwoHandSoftSteerRadians) {
        return angleRadians;
    }
    const float range =
        kTwoHandMaxSteerRadians - kTwoHandSoftSteerRadians;
    const float excess =
        (angleRadians - kTwoHandSoftSteerRadians) / range;
    return kTwoHandSoftSteerRadians +
           range * (1.0F - std::exp(-excess));
}

} // namespace fearvr

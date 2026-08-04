#pragma once

#include <cmath>

namespace fearvr {

// A two-handed player-body pose may move the animated camera socket while
// running, firing or jumping even though the physical player has not moved by
// that extra amount. Capture the normal eye-to-body offset outside a two-hand
// grip and reconstruct eye height from the physical player body's world Y for
// the entire grip. This preserves real locomotion and the jump arc while
// excluding presentation-only socket motion.
constexpr float kTwoHandedJumpCollisionGuardUnits = 35.0F;

struct TwoHandedJumpCameraState {
    float groundedEyeOffset{0.0F};
    bool groundedEyeOffsetValid{false};
    bool anchorActive{false};
};

struct TwoHandedJumpCameraOutput {
    float visualHeight{0.0F};
    bool anchorActive{false};
    bool collisionGuarded{false};
};

inline void ResetTwoHandedJumpCamera(
    TwoHandedJumpCameraState& state) noexcept {
    state = TwoHandedJumpCameraState{};
}

inline TwoHandedJumpCameraOutput UpdateTwoHandedJumpCamera(
    TwoHandedJumpCameraState& state,
    float bodyHeight,
    bool bodyHeightValid,
    float finalCameraHeight,
    bool onGround,
    bool airborne,
    bool ducking,
    bool twoHandedGripActive,
    bool movementStateAvailable) noexcept {
    TwoHandedJumpCameraOutput output;
    output.visualHeight = finalCameraHeight;
    if (!bodyHeightValid || !std::isfinite(bodyHeight) ||
        !std::isfinite(finalCameraHeight) || !movementStateAvailable) {
        ResetTwoHandedJumpCamera(state);
        return output;
    }

    // Learn only from a stable standing frame outside the affected pose.
    // Crouch changes the intended eye offset, and a two-handed frame may
    // already contain the socket displacement we are trying to reject.
    if (onGround && !airborne && !ducking && !twoHandedGripActive) {
        state.groundedEyeOffset = finalCameraHeight - bodyHeight;
        state.groundedEyeOffsetValid =
            std::isfinite(state.groundedEyeOffset);
    }

    const bool shouldAnchor =
        twoHandedGripActive && !ducking &&
        state.groundedEyeOffsetValid;
    state.anchorActive = shouldAnchor;
    if (!shouldAnchor) {
        return output;
    }

    const float anchoredHeight =
        bodyHeight + state.groundedEyeOffset;
    // A large downward correction is likely a real ceiling collision. Never
    // lift the view through geometry merely to preserve the animation filter.
    if (finalCameraHeight <
        anchoredHeight - kTwoHandedJumpCollisionGuardUnits) {
        output.collisionGuarded = true;
        return output;
    }

    output.visualHeight = anchoredHeight;
    output.anchorActive = true;
    return output;
}

} // namespace fearvr

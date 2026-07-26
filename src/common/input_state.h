#pragma once

#include <cmath>
#include <cstdint>

#include "protocol.h"

namespace fearvr {

inline bool IsInputStateUsable(
    const FearVrInputState& state, bool sampleFresh) noexcept {
    return sampleFresh &&
           (state.flags & FEARVR_IF_VALID) != 0 &&
           (state.flags & FEARVR_IF_FOCUSED) != 0;
}

inline void NeutralizeInputState(
    FearVrInputState& state) noexcept {
    state.moveX = 0.0F;
    state.moveY = 0.0F;
    state.turnX = 0.0F;
    state.turnY = 0.0F;
    for (std::uint32_t hand = 0;
         hand < FEARVR_HAND_COUNT; ++hand) {
        state.trigger[hand] = 0.0F;
        state.squeeze[hand] = 0.0F;
    }
    state.buttons = 0;
    state.activeHands = 0;
    state.aimPoseValidHands = 0;
    state.gripPoseValidHands = 0;
    for (std::uint32_t hand = 0;
         hand < FEARVR_HAND_COUNT; ++hand) {
        state.handAimPose[hand] = {};
        state.handGripPose[hand] = {};
    }
    state.flags &= ~FEARVR_IF_FOCUSED;
}

// Left-handed play: every binding swaps hands exactly once, here, right after
// the state arrives. Downstream the weapon hand stays FEARVR_HAND_RIGHT, so
// weapon aim, flashlight, leaning, hand nodes and the command mapping all
// mirror with a single swap instead of a handedness case each.
//
// The only value that must be mirrored back is the haptic request's hand mask,
// which addresses a physical controller again.
inline void MirrorInputHandedness(
    FearVrInputState& state) noexcept {
    const auto SwapFloat = [](float& left, float& right) {
        const float saved = left;
        left = right;
        right = saved;
    };
    const auto SwapHandMasks = [](std::uint32_t& mask) {
        const std::uint32_t left = mask & FEARVR_HAND_MASK_LEFT;
        const std::uint32_t right = mask & FEARVR_HAND_MASK_RIGHT;
        mask &= ~static_cast<std::uint32_t>(
            FEARVR_HAND_MASK_LEFT | FEARVR_HAND_MASK_RIGHT);
        if (left != 0) {
            mask |= FEARVR_HAND_MASK_RIGHT;
        }
        if (right != 0) {
            mask |= FEARVR_HAND_MASK_LEFT;
        }
    };

    SwapFloat(state.moveX, state.turnX);
    SwapFloat(state.moveY, state.turnY);
    SwapFloat(
        state.trigger[FEARVR_HAND_LEFT],
        state.trigger[FEARVR_HAND_RIGHT]);
    SwapFloat(
        state.squeeze[FEARVR_HAND_LEFT],
        state.squeeze[FEARVR_HAND_RIGHT]);
    for (std::uint32_t index = 0; index < 4U; ++index) {
        const std::uint32_t leftBit = FEARVR_IB_LEFT_PRIMARY << index;
        const std::uint32_t rightBit = FEARVR_IB_RIGHT_PRIMARY << index;
        const bool leftSet = (state.buttons & leftBit) != 0;
        const bool rightSet = (state.buttons & rightBit) != 0;
        state.buttons &= ~(leftBit | rightBit);
        if (leftSet) {
            state.buttons |= rightBit;
        }
        if (rightSet) {
            state.buttons |= leftBit;
        }
    }
    SwapHandMasks(state.activeHands);
    SwapHandMasks(state.aimPoseValidHands);
    SwapHandMasks(state.gripPoseValidHands);
    const FearVrPose savedAim = state.handAimPose[FEARVR_HAND_LEFT];
    state.handAimPose[FEARVR_HAND_LEFT] =
        state.handAimPose[FEARVR_HAND_RIGHT];
    state.handAimPose[FEARVR_HAND_RIGHT] = savedAim;
    const FearVrPose savedGrip = state.handGripPose[FEARVR_HAND_LEFT];
    state.handGripPose[FEARVR_HAND_LEFT] =
        state.handGripPose[FEARVR_HAND_RIGHT];
    state.handGripPose[FEARVR_HAND_RIGHT] = savedGrip;
}

// Signed roll of a pose about its own forward (-Z) axis, in radians.
// OpenXR spaces are Y-up, so the roll follows from the world-up components
// of the pose's local +X and +Y axes. A positive result means the top of the
// hand is tipped towards the user's left.
inline float PoseRollRadians(const FearVrPose& pose) noexcept {
    const float x = pose.qx;
    const float y = pose.qy;
    const float z = pose.qz;
    const float w = pose.qw;
    const float lengthSqr = x * x + y * y + z * z + w * w;
    if (!std::isfinite(lengthSqr) || lengthSqr < 1.0e-6F) {
        return 0.0F;
    }
    // Both terms carry the same positive |q|^2 factor, which atan2 cancels,
    // so an unnormalised quaternion still yields the correct angle.
    const float rightAxisUp = 2.0F * (x * y + z * w);
    const float upAxisUp = w * w - x * x + y * y - z * z;
    return std::atan2(rightAxisUp, upAxisUp);
}

// |cos(pitch)| of a pose: how much of its own right and up axes still lies in
// the world horizontal plane. Roll about the forward axis loses all meaning as
// this approaches zero, which is exactly when the pose points straight up or
// down — a hand hanging at the side produces wild roll values otherwise.
inline float PoseLevelness(const FearVrPose& pose) noexcept {
    const float x = pose.qx;
    const float y = pose.qy;
    const float z = pose.qz;
    const float w = pose.qw;
    const float lengthSqr = x * x + y * y + z * z + w * w;
    if (!std::isfinite(lengthSqr) || lengthSqr < 1.0e-6F) {
        return 0.0F;
    }
    const float rightAxisUp = 2.0F * (x * y + z * w);
    const float upAxisUp = w * w - x * x + y * y - z * z;
    const float magnitude = std::sqrt(
        rightAxisUp * rightAxisUp + upAxisUp * upAxisUp);
    return magnitude / lengthSqr;
}

// Roll of the left aim pose, or zero while that hand is unusable.
inline float LeftHandLeanRollRadians(
    const FearVrInputState& state) noexcept {
    if ((state.activeHands & FEARVR_HAND_MASK_LEFT) == 0 ||
        (state.aimPoseValidHands & FEARVR_HAND_MASK_LEFT) == 0) {
        return 0.0F;
    }
    return PoseRollRadians(state.handAimPose[FEARVR_HAND_LEFT]);
}

inline float ApplyInputDeadzone(
    float value, float deadzone) noexcept {
    if (!std::isfinite(value) ||
        !std::isfinite(deadzone) ||
        deadzone < 0.0F || deadzone >= 1.0F) {
        return 0.0F;
    }
    if (value > -deadzone && value < deadzone) {
        return 0.0F;
    }
    const float magnitude =
        (std::fabs(value) - deadzone) / (1.0F - deadzone);
    const float clamped = magnitude > 1.0F ? 1.0F : magnitude;
    return value < 0.0F ? -clamped : clamped;
}

} // namespace fearvr

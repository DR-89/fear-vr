#pragma once

#include <cstdint>

#include "input_state.h"

namespace fearvr {

// Public F.E.A.R. 1.08 command IDs used by CBindMgr.
enum FearVrGameCommand : std::uint32_t {
    FEARVR_CMD_FORWARD = 0,
    FEARVR_CMD_REVERSE = 1,
    FEARVR_CMD_FORWARD_AXIS = 2,
    FEARVR_CMD_STRAFE_LEFT = 3,
    FEARVR_CMD_STRAFE_RIGHT = 4,
    FEARVR_CMD_STRAFE_AXIS = 5,
    FEARVR_CMD_YAW_POS = 9,
    FEARVR_CMD_YAW_NEG = 10,
    FEARVR_CMD_MENU = 13,
    FEARVR_CMD_DUCK = 14,
    FEARVR_CMD_JUMP = 15,
    FEARVR_CMD_RUN = 16,
    FEARVR_CMD_FIRING = 17,
    FEARVR_CMD_LEAN_LEFT = 20,
    FEARVR_CMD_LEAN_RIGHT = 21,
    FEARVR_CMD_YAW_ACCEL = 23,
    FEARVR_CMD_FOCUS = 27,
    FEARVR_CMD_PREV_WEAPON = 76,
    FEARVR_CMD_NEXT_WEAPON = 77,
    FEARVR_CMD_ACTIVATE = 87,
    FEARVR_CMD_RELOAD = 88,
    FEARVR_CMD_SLOWMO = 106
};

struct FearVrCommandValue {
    float value;
    bool active;
};

// ~24 degrees of left-hand roll before leaning engages. Far enough above the
// tilt that normal walking and aiming produce.
constexpr float kLeanRollThresholdRadians = 0.42F;
// ~100 degrees. Beyond this the hand is turned over rather than tilted, and a
// roll near +-180 degrees must not read as a full lean.
constexpr float kLeanMaxRollRadians = 1.75F;
// cos(60 degrees): the aim pose must stay reasonably horizontal, otherwise its
// roll is numerically meaningless. A hand hanging at the side is rejected here.
constexpr float kLeanMinLevelness = 0.5F;

// -1 leans right, +1 leans left, 0 does not lean.
inline int LeftHandLeanDirection(
    const FearVrInputState& input) noexcept {
    if ((input.activeHands & FEARVR_HAND_MASK_LEFT) == 0 ||
        (input.aimPoseValidHands & FEARVR_HAND_MASK_LEFT) == 0) {
        return 0;
    }
    const FearVrPose& pose = input.handAimPose[FEARVR_HAND_LEFT];
    if (PoseLevelness(pose) < kLeanMinLevelness) {
        return 0;
    }
    const float roll = PoseRollRadians(pose);
    if (roll >= kLeanRollThresholdRadians &&
        roll <= kLeanMaxRollRadians) {
        return 1;
    }
    if (roll <= -kLeanRollThresholdRadians &&
        roll >= -kLeanMaxRollRadians) {
        return -1;
    }
    return 0;
}

inline FearVrCommandValue MapControllerCommand(
    const FearVrInputState& input,
    std::uint32_t command) noexcept {
    constexpr float kStickDeadzone = 0.22F;
    constexpr float kTriggerThreshold = 0.55F;
    constexpr float kSqueezeThreshold = 0.65F;

    const bool leftActive =
        (input.activeHands & FEARVR_HAND_MASK_LEFT) != 0;
    const bool rightActive =
        (input.activeHands & FEARVR_HAND_MASK_RIGHT) != 0;
    const float moveX =
        leftActive
            ? ApplyInputDeadzone(input.moveX, kStickDeadzone)
            : 0.0F;
    const float moveY =
        leftActive
            ? ApplyInputDeadzone(input.moveY, kStickDeadzone)
            : 0.0F;
    const float turnX =
        rightActive
            ? ApplyInputDeadzone(input.turnX, kStickDeadzone)
            : 0.0F;
    const float turnY =
        rightActive
            ? ApplyInputDeadzone(input.turnY, kStickDeadzone)
            : 0.0F;

    switch (command) {
    case FEARVR_CMD_FORWARD_AXIS:
        return {moveY, moveY != 0.0F};
    case FEARVR_CMD_STRAFE_AXIS:
        return {moveX, moveX != 0.0F};
    case FEARVR_CMD_YAW_ACCEL:
        return {turnX, turnX != 0.0F};
    case FEARVR_CMD_YAW_POS:
        return {1.0F, turnX > 0.0F};
    case FEARVR_CMD_YAW_NEG:
        return {1.0F, turnX < 0.0F};
    case FEARVR_CMD_FORWARD:
        return {1.0F, moveY > 0.0F};
    case FEARVR_CMD_REVERSE:
        return {1.0F, moveY < 0.0F};
    case FEARVR_CMD_STRAFE_LEFT:
        return {1.0F, moveX < 0.0F};
    case FEARVR_CMD_STRAFE_RIGHT:
        return {1.0F, moveX > 0.0F};
    case FEARVR_CMD_FIRING:
        return {
            1.0F,
            rightActive &&
                input.trigger[FEARVR_HAND_RIGHT] >=
                    kTriggerThreshold};
    case FEARVR_CMD_FOCUS:
        return {
            1.0F,
            leftActive &&
                input.trigger[FEARVR_HAND_LEFT] >=
                    kTriggerThreshold};
    case FEARVR_CMD_NEXT_WEAPON:
        return {1.0F, turnY >= 0.65F};
    case FEARVR_CMD_PREV_WEAPON:
        return {1.0F, turnY <= -0.65F};
    case FEARVR_CMD_RUN:
        return {
            1.0F,
            leftActive &&
                input.squeeze[FEARVR_HAND_LEFT] >=
                    kSqueezeThreshold};
    case FEARVR_CMD_ACTIVATE:
        return {
            1.0F,
            rightActive &&
                input.squeeze[FEARVR_HAND_RIGHT] >=
                    kSqueezeThreshold};
    case FEARVR_CMD_JUMP:
        return {
            1.0F,
            rightActive &&
                (input.buttons & FEARVR_IB_RIGHT_PRIMARY) != 0};
    case FEARVR_CMD_DUCK:
        return {
            1.0F,
            leftActive &&
                (input.buttons & FEARVR_IB_LEFT_PRIMARY) != 0};
    case FEARVR_CMD_RELOAD:
        return {
            1.0F,
            rightActive &&
                (input.buttons & FEARVR_IB_RIGHT_SECONDARY) != 0};
    case FEARVR_CMD_SLOWMO:
        return {
            1.0F,
            leftActive &&
                (input.buttons & FEARVR_IB_LEFT_SECONDARY) != 0};
    case FEARVR_CMD_LEAN_LEFT:
        return {1.0F, LeftHandLeanDirection(input) > 0};
    case FEARVR_CMD_LEAN_RIGHT:
        return {1.0F, LeftHandLeanDirection(input) < 0};
    case FEARVR_CMD_MENU:
        return {
            1.0F,
            leftActive &&
                (input.buttons & FEARVR_IB_LEFT_STICK) != 0};
    default:
        return {0.0F, false};
    }
}

} // namespace fearvr

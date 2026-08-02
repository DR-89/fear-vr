#include <cmath>
#include <cstdio>
#include <limits>

#include "input_state.h"

namespace {

int Fail(const char* message) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    return 1;
}

bool Near(float left, float right) {
    return std::fabs(left - right) < 0.0001F;
}

} // namespace

int main() {
    FearVrInputState state{};
    state.sampleId = 9;
    state.flags = FEARVR_IF_VALID | FEARVR_IF_FOCUSED;
    state.activeHands =
        FEARVR_HAND_MASK_LEFT | FEARVR_HAND_MASK_RIGHT;
    state.buttons =
        FEARVR_IB_LEFT_PRIMARY | FEARVR_IB_RIGHT_STICK;
    state.moveX = 0.7F;
    state.moveY = -0.8F;
    state.turnX = 0.5F;
    state.turnY = -0.4F;
    state.trigger[0] = 1.0F;
    state.squeeze[1] = 0.6F;
    state.aimPoseValidHands = FEARVR_HAND_MASK_RIGHT;
    state.handAimPose[FEARVR_HAND_RIGHT].px = 0.25F;
    state.handAimPose[FEARVR_HAND_RIGHT].qw = 1.0F;
    state.gripPoseValidHands = FEARVR_HAND_MASK_RIGHT;
    state.handGripPose[FEARVR_HAND_RIGHT].py = -0.15F;
    state.handGripPose[FEARVR_HAND_RIGHT].qw = 1.0F;

    if (!fearvr::IsInputStateUsable(state, true) ||
        fearvr::IsInputStateUsable(state, false)) {
        return Fail("fresh and focused validity is incorrect");
    }
    fearvr::NeutralizeInputState(state);
    if (state.sampleId != 9 ||
        (state.flags & FEARVR_IF_VALID) == 0 ||
        (state.flags & FEARVR_IF_FOCUSED) != 0 ||
        state.activeHands != 0 || state.buttons != 0 ||
        state.aimPoseValidHands != 0 ||
        state.gripPoseValidHands != 0 ||
        state.handAimPose[FEARVR_HAND_RIGHT].px != 0.0F ||
        state.handAimPose[FEARVR_HAND_RIGHT].qw != 0.0F ||
        state.handGripPose[FEARVR_HAND_RIGHT].py != 0.0F ||
        state.handGripPose[FEARVR_HAND_RIGHT].qw != 0.0F ||
        state.moveX != 0.0F || state.moveY != 0.0F ||
        state.turnX != 0.0F || state.turnY != 0.0F ||
        state.trigger[0] != 0.0F ||
        state.squeeze[1] != 0.0F) {
        return Fail("focus loss must publish a complete neutral state");
    }

    if (!Near(fearvr::ApplyInputDeadzone(0.1F, 0.2F), 0.0F) ||
        !Near(fearvr::ApplyInputDeadzone(0.6F, 0.2F), 0.5F) ||
        !Near(fearvr::ApplyInputDeadzone(-0.6F, 0.2F), -0.5F) ||
        !Near(fearvr::ApplyInputDeadzone(2.0F, 0.2F), 1.0F) ||
        fearvr::ApplyInputDeadzone(
            std::numeric_limits<float>::quiet_NaN(), 0.2F) != 0.0F) {
        return Fail("deadzone normalization is incorrect");
    }

    const fearvr::InputVector2 radialDiagonal =
        fearvr::ApplyRadialInputDeadzone(0.3F, 0.8F, 0.2F);
    const float sourceRatio = 0.3F / 0.8F;
    if (!(radialDiagonal.x > 0.0F) ||
        !(radialDiagonal.y > radialDiagonal.x) ||
        !Near(radialDiagonal.x / radialDiagonal.y, sourceRatio)) {
        return Fail("radial deadzone must preserve diagonal direction");
    }
    const fearvr::InputVector2 radialFull =
        fearvr::ApplyRadialInputDeadzone(1.0F, 1.0F, 0.2F);
    if (!Near(
            std::sqrt(radialFull.x * radialFull.x +
                      radialFull.y * radialFull.y),
            1.0F)) {
        return Fail("radial deadzone must normalize full diagonals");
    }
    const fearvr::InputVector2 radialCenter =
        fearvr::ApplyRadialInputDeadzone(0.1F, 0.1F, 0.2F);
    if (radialCenter.x != 0.0F || radialCenter.y != 0.0F) {
        return Fail("radial deadzone must neutralize the center circle");
    }

    // A roll about the pose's own forward axis must come back signed, with
    // positive meaning the top of the hand is tipped to the user's left.
    FearVrPose upright{};
    upright.qw = 1.0F;
    if (!Near(fearvr::PoseRollRadians(upright), 0.0F)) {
        return Fail("an upright pose must report zero roll");
    }
    FearVrPose zeroQuaternion{};
    if (fearvr::PoseRollRadians(zeroQuaternion) != 0.0F) {
        return Fail("a degenerate quaternion must report zero roll");
    }
    for (int degrees = -80; degrees <= 80; degrees += 20) {
        const float angle =
            static_cast<float>(degrees) * 3.14159265F / 180.0F;
        FearVrPose rolled{};
        rolled.qz = std::sin(angle * 0.5F);
        rolled.qw = std::cos(angle * 0.5F);
        if (!Near(fearvr::PoseRollRadians(rolled), angle)) {
            return Fail("roll extraction must match the applied angle");
        }
        // An unnormalised quaternion must yield the same angle.
        FearVrPose scaled{};
        scaled.qz = rolled.qz * 3.0F;
        scaled.qw = rolled.qw * 3.0F;
        if (!Near(fearvr::PoseRollRadians(scaled), angle)) {
            return Fail("roll extraction must ignore quaternion scale");
        }
    }

    // Levelness is 1 for any pure roll and collapses to 0 when the pose points
    // straight down, which is what makes the roll unusable there.
    if (!Near(fearvr::PoseLevelness(upright), 1.0F)) {
        return Fail("an upright pose must be fully level");
    }
    FearVrPose rolledFlat{};
    rolledFlat.qz = std::sin(0.7853981F);
    rolledFlat.qw = std::cos(0.7853981F);
    if (!Near(fearvr::PoseLevelness(rolledFlat), 1.0F)) {
        return Fail("a pure roll must not reduce levelness");
    }
    FearVrPose pitchedDown{};
    pitchedDown.qx = std::sin(-0.7853981F);
    pitchedDown.qw = std::cos(-0.7853981F);
    if (!Near(fearvr::PoseLevelness(pitchedDown), 0.0F)) {
        return Fail("a pose pointing straight down must not be level");
    }
    if (fearvr::PoseLevelness(zeroQuaternion) != 0.0F) {
        return Fail("a degenerate quaternion must report zero levelness");
    }

    FearVrInputState lean{};
    lean.handAimPose[FEARVR_HAND_LEFT].qz = std::sin(0.5F * 0.5F);
    lean.handAimPose[FEARVR_HAND_LEFT].qw = std::cos(0.5F * 0.5F);
    if (fearvr::LeftHandLeanRollRadians(lean) != 0.0F) {
        return Fail("lean roll must be zero while the left hand is idle");
    }
    lean.activeHands = FEARVR_HAND_MASK_LEFT;
    if (fearvr::LeftHandLeanRollRadians(lean) != 0.0F) {
        return Fail("lean roll must be zero without a valid aim pose");
    }
    lean.aimPoseValidHands = FEARVR_HAND_MASK_LEFT;
    if (!Near(fearvr::LeftHandLeanRollRadians(lean), 0.5F)) {
        return Fail("lean roll must follow the left aim pose");
    }

    // Linkshaenderbelegung: ein einziger Tausch dreht Stoecke, Trigger,
    // Griffe, Tasten, Handmasken und Posen.
    FearVrInputState handed{};
    handed.moveX = 0.25F;
    handed.moveY = -0.5F;
    handed.turnX = 0.75F;
    handed.turnY = 1.0F;
    handed.trigger[FEARVR_HAND_LEFT] = 0.1F;
    handed.trigger[FEARVR_HAND_RIGHT] = 0.9F;
    handed.squeeze[FEARVR_HAND_LEFT] = 0.2F;
    handed.squeeze[FEARVR_HAND_RIGHT] = 0.8F;
    handed.buttons =
        FEARVR_IB_LEFT_PRIMARY | FEARVR_IB_RIGHT_SECONDARY |
        FEARVR_IB_RIGHT_STICK;
    handed.activeHands = FEARVR_HAND_MASK_LEFT;
    handed.aimPoseValidHands =
        FEARVR_HAND_MASK_LEFT | FEARVR_HAND_MASK_RIGHT;
    handed.gripPoseValidHands = FEARVR_HAND_MASK_RIGHT;
    handed.handAimPose[FEARVR_HAND_LEFT].px = 1.0F;
    handed.handAimPose[FEARVR_HAND_RIGHT].px = 2.0F;
    handed.handGripPose[FEARVR_HAND_LEFT].pz = 3.0F;
    handed.handGripPose[FEARVR_HAND_RIGHT].pz = 4.0F;

    const FearVrInputState original = handed;
    fearvr::MirrorInputHandedness(handed);
    if (handed.moveX != original.turnX ||
        handed.turnX != original.moveX ||
        handed.moveY != original.turnY ||
        handed.turnY != original.moveY) {
        return Fail("mirroring must swap both sticks");
    }
    if (handed.trigger[FEARVR_HAND_RIGHT] != 0.1F ||
        handed.squeeze[FEARVR_HAND_RIGHT] != 0.2F) {
        return Fail("mirroring must swap triggers and grips");
    }
    if (handed.buttons !=
        (FEARVR_IB_RIGHT_PRIMARY | FEARVR_IB_LEFT_SECONDARY |
         FEARVR_IB_LEFT_STICK)) {
        return Fail("mirroring must swap the button bits per hand");
    }
    if (handed.activeHands != FEARVR_HAND_MASK_RIGHT ||
        handed.gripPoseValidHands != FEARVR_HAND_MASK_LEFT ||
        handed.aimPoseValidHands !=
            (FEARVR_HAND_MASK_LEFT | FEARVR_HAND_MASK_RIGHT)) {
        return Fail("mirroring must swap the hand masks");
    }
    if (handed.handAimPose[FEARVR_HAND_RIGHT].px != 1.0F ||
        handed.handGripPose[FEARVR_HAND_RIGHT].pz != 3.0F) {
        return Fail("mirroring must swap the tracked poses");
    }

    // Zweimal gespiegelt ist der Ausgangszustand.
    fearvr::MirrorInputHandedness(handed);
    if (handed.moveX != original.moveX ||
        handed.turnY != original.turnY ||
        handed.buttons != original.buttons ||
        handed.activeHands != original.activeHands ||
        handed.aimPoseValidHands != original.aimPoseValidHands ||
        handed.gripPoseValidHands != original.gripPoseValidHands ||
        handed.trigger[FEARVR_HAND_LEFT] !=
            original.trigger[FEARVR_HAND_LEFT] ||
        handed.handAimPose[FEARVR_HAND_LEFT].px !=
            original.handAimPose[FEARVR_HAND_LEFT].px ||
        handed.handGripPose[FEARVR_HAND_LEFT].pz !=
            original.handGripPose[FEARVR_HAND_LEFT].pz) {
        return Fail("mirroring twice must restore the original state");
    }

    std::puts("test_input_state: OK");
    return 0;
}

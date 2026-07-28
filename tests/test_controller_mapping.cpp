#include <cassert>
#include <cmath>

#include "controller_mapping.h"

namespace {

bool NearlyEqual(float left, float right) {
    return std::fabs(left - right) < 0.0001F;
}

} // namespace

int main() {
    FearVrInputState input{};
    input.flags = FEARVR_IF_VALID | FEARVR_IF_FOCUSED;
    input.activeHands =
        FEARVR_HAND_MASK_LEFT | FEARVR_HAND_MASK_RIGHT;

    input.moveY = 0.61F;
    const fearvr::FearVrCommandValue forwardAxis =
        fearvr::MapControllerCommand(
            input, fearvr::FEARVR_CMD_FORWARD_AXIS);
    assert(forwardAxis.active);
    assert(forwardAxis.value > 0.0F);
    assert(fearvr::MapControllerCommand(
               input, fearvr::FEARVR_CMD_FORWARD)
               .active);
    assert(!fearvr::MapControllerCommand(
                input, fearvr::FEARVR_CMD_REVERSE)
                .active);

    input.moveX = -1.0F;
    const fearvr::FearVrCommandValue strafeAxis =
        fearvr::MapControllerCommand(
            input, fearvr::FEARVR_CMD_STRAFE_AXIS);
    assert(strafeAxis.active);
    assert(NearlyEqual(strafeAxis.value, -1.0F));
    assert(fearvr::MapControllerCommand(
               input, fearvr::FEARVR_CMD_STRAFE_LEFT)
               .active);

    input.turnX = 0.7F;
    assert(fearvr::MapControllerCommand(
               input, fearvr::FEARVR_CMD_YAW_ACCEL)
               .value > 0.0F);
    assert(fearvr::MapControllerCommand(
               input, fearvr::FEARVR_CMD_YAW_POS)
               .active);
    assert(!fearvr::MapControllerCommand(
                input, fearvr::FEARVR_CMD_YAW_NEG)
                .active);
    // Der rechte Stick springt und duckt erst im Vollausschlag; ein Teilweg
    // beim Drehen darf beides nicht ausloesen.
    input.turnY = 0.79F;
    assert(!fearvr::MapControllerCommand(
                input, fearvr::FEARVR_CMD_JUMP)
                .active);
    input.turnY = 0.8F;
    assert(fearvr::MapControllerCommand(
               input, fearvr::FEARVR_CMD_JUMP)
               .active);
    assert(!fearvr::MapControllerCommand(
                input, fearvr::FEARVR_CMD_DUCK)
                .active);
    input.turnY = -0.8F;
    assert(fearvr::MapControllerCommand(
               input, fearvr::FEARVR_CMD_DUCK)
               .active);
    assert(!fearvr::MapControllerCommand(
                input, fearvr::FEARVR_CMD_JUMP)
                .active);
    // Waffenwechsel, Nachladen und Granate entstehen als Puls im GameClient
    // und sind in der zustandslosen Zuordnung deshalb nie aktiv.
    input.turnY = 0.0F;
    assert(!fearvr::MapControllerCommand(
                input, fearvr::FEARVR_CMD_NEXT_WEAPON)
                .active);
    assert(!fearvr::MapControllerCommand(
                input, fearvr::FEARVR_CMD_PREV_WEAPON)
                .active);
    assert(!fearvr::MapControllerCommand(
                input, fearvr::FEARVR_CMD_THROW_GRENADE)
                .active);

    input.trigger[FEARVR_HAND_RIGHT] = 0.54F;
    assert(!fearvr::MapControllerCommand(
                input, fearvr::FEARVR_CMD_FIRING)
                .active);
    input.trigger[FEARVR_HAND_RIGHT] = 0.55F;
    assert(fearvr::MapControllerCommand(
               input, fearvr::FEARVR_CMD_FIRING)
               .active);

    input.squeeze[FEARVR_HAND_LEFT] = 0.8F;
    input.squeeze[FEARVR_HAND_RIGHT] = 0.8F;
    input.buttons =
        FEARVR_IB_RIGHT_PRIMARY |
        FEARVR_IB_RIGHT_SECONDARY |
        FEARVR_IB_LEFT_PRIMARY |
        FEARVR_IB_LEFT_SECONDARY |
        FEARVR_IB_LEFT_STICK;
    assert(fearvr::MapControllerCommand(
               input, fearvr::FEARVR_CMD_RUN)
               .active);
    assert(fearvr::MapControllerCommand(
               input, fearvr::FEARVR_CMD_ACTIVATE)
               .active);
    assert(!fearvr::MapControllerCommand(
                input, fearvr::FEARVR_CMD_JUMP)
                .active);
    // Ducken haengt allein am rechten Stick; der linke Stick-Klick ist frei.
    assert(!fearvr::MapControllerCommand(
                input, fearvr::FEARVR_CMD_DUCK)
                .active);
    assert(!fearvr::MapControllerCommand(
                input, fearvr::FEARVR_CMD_RELOAD)
                .active);
    assert(fearvr::MapControllerCommand(
               input, fearvr::FEARVR_CMD_MENU)
               .active);
    // X ist jetzt allein der Taschenlampenschalter. Zeitlupe folgt dem linken
    // Trigger und darf durch X nicht mehr als Spielkommando aktiviert werden.
    input.trigger[FEARVR_HAND_LEFT] = 0.54F;
    assert(!fearvr::MapControllerCommand(
                input, fearvr::FEARVR_CMD_SLOWMO)
                .active);
    input.trigger[FEARVR_HAND_LEFT] = 0.55F;
    assert(fearvr::MapControllerCommand(
               input, fearvr::FEARVR_CMD_SLOWMO)
               .active);
    input.buttons &= ~static_cast<std::uint32_t>(
        FEARVR_IB_LEFT_PRIMARY);
    assert(fearvr::MapControllerCommand(
               input, fearvr::FEARVR_CMD_SLOWMO)
               .active);
    // Der linke Stick-Klick benutzt einen Medkit; Ducken liegt weiterhin
    // allein auf dem rechten Stick.
    assert(fearvr::MapControllerCommand(
               input, fearvr::FEARVR_CMD_MEDKIT)
               .active);
    input.buttons &= ~static_cast<std::uint32_t>(
        FEARVR_IB_LEFT_STICK);
    assert(!fearvr::MapControllerCommand(
                input, fearvr::FEARVR_CMD_MEDKIT)
                .active);
    input.buttons |= FEARVR_IB_LEFT_STICK;

    input.squeeze[FEARVR_HAND_LEFT] = 0.0F;
    assert(!fearvr::MapControllerCommand(
                input, fearvr::FEARVR_CMD_RUN)
                .active);
    assert(fearvr::MapControllerCommand(
               input, fearvr::FEARVR_CMD_MENU)
               .active);

    // Ohne rechte Hand gibt es weder Springen noch Ducken.
    input.turnY = -1.0F;
    input.activeHands = FEARVR_HAND_MASK_RIGHT;
    assert(fearvr::MapControllerCommand(
               input, fearvr::FEARVR_CMD_DUCK)
               .active);

    input.activeHands = FEARVR_HAND_MASK_LEFT;
    assert(!fearvr::MapControllerCommand(
                input, fearvr::FEARVR_CMD_FIRING)
                .active);
    assert(!fearvr::MapControllerCommand(
                input, fearvr::FEARVR_CMD_JUMP)
                .active);
    assert(!fearvr::MapControllerCommand(
                input, fearvr::FEARVR_CMD_DUCK)
                .active);

    // Tilting the left hand sideways leans around corners.
    FearVrInputState lean{};
    lean.flags = FEARVR_IF_VALID | FEARVR_IF_FOCUSED;
    lean.activeHands = FEARVR_HAND_MASK_LEFT;
    lean.aimPoseValidHands = FEARVR_HAND_MASK_LEFT;
    // Pure roll about the aim pose's own forward axis, no residual pitch.
    const auto SetLeftRoll = [&lean](float radians) {
        lean.handAimPose[FEARVR_HAND_LEFT] = FearVrPose{};
        lean.handAimPose[FEARVR_HAND_LEFT].qz =
            std::sin(radians * 0.5F);
        lean.handAimPose[FEARVR_HAND_LEFT].qw =
            std::cos(radians * 0.5F);
    };

    SetLeftRoll(0.0F);
    assert(!fearvr::MapControllerCommand(
                lean, fearvr::FEARVR_CMD_LEAN_LEFT)
                .active);
    assert(!fearvr::MapControllerCommand(
                lean, fearvr::FEARVR_CMD_LEAN_RIGHT)
                .active);

    // A small tilt from ordinary aiming must not trigger a lean.
    SetLeftRoll(0.3F);
    assert(!fearvr::MapControllerCommand(
                lean, fearvr::FEARVR_CMD_LEAN_LEFT)
                .active);

    SetLeftRoll(0.8F);
    assert(fearvr::MapControllerCommand(
               lean, fearvr::FEARVR_CMD_LEAN_LEFT)
               .active);
    assert(!fearvr::MapControllerCommand(
                lean, fearvr::FEARVR_CMD_LEAN_RIGHT)
                .active);

    SetLeftRoll(-0.8F);
    assert(fearvr::MapControllerCommand(
               lean, fearvr::FEARVR_CMD_LEAN_RIGHT)
               .active);
    assert(!fearvr::MapControllerCommand(
                lean, fearvr::FEARVR_CMD_LEAN_LEFT)
                .active);

    // A hand turned over must not read as a full lean. Rolls near +-180
    // degrees would otherwise pass the plain threshold comparison.
    SetLeftRoll(3.0F);
    assert(!fearvr::MapControllerCommand(
                lean, fearvr::FEARVR_CMD_LEAN_LEFT)
                .active);
    SetLeftRoll(-3.0F);
    assert(!fearvr::MapControllerCommand(
                lean, fearvr::FEARVR_CMD_LEAN_RIGHT)
                .active);

    // A hand hanging at the side points the aim pose straight down, where the
    // roll about the forward axis is numerically meaningless.
    lean.handAimPose[FEARVR_HAND_LEFT] = FearVrPose{};
    lean.handAimPose[FEARVR_HAND_LEFT].qx = std::sin(-0.785398F);
    lean.handAimPose[FEARVR_HAND_LEFT].qw = std::cos(-0.785398F);
    assert(fearvr::LeftHandLeanDirection(lean) == 0);
    assert(!fearvr::MapControllerCommand(
                lean, fearvr::FEARVR_CMD_LEAN_LEFT)
                .active);
    assert(!fearvr::MapControllerCommand(
                lean, fearvr::FEARVR_CMD_LEAN_RIGHT)
                .active);

    // Losing tracking must release the lean instead of latching it.
    SetLeftRoll(-0.8F);
    assert(fearvr::LeftHandLeanDirection(lean) < 0);
    lean.aimPoseValidHands = 0;
    assert(fearvr::LeftHandLeanDirection(lean) == 0);
    assert(!fearvr::MapControllerCommand(
                lean, fearvr::FEARVR_CMD_LEAN_RIGHT)
                .active);

    input.activeHands = 0;
    assert(!fearvr::MapControllerCommand(
                input, fearvr::FEARVR_CMD_FORWARD_AXIS)
                .active);
    assert(!fearvr::MapControllerCommand(
                input, fearvr::FEARVR_CMD_MENU)
                .active);

    return 0;
}

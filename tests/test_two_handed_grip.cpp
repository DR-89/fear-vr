#include <cassert>
#include <cmath>
#include <limits>

#include "controller_mapping.h"
#include "two_handed_grip.h"

namespace {

// Beide Haende in OpenXR-LOCAL: die rechte im Ursprung mit Blick nach -Z,
// die linke um `forward` davor und um `lateral` daneben.
FearVrInputState TwoHandInput(
    float forwardMeters, float lateralMeters,
    float leftSqueeze) {
    FearVrInputState input{};
    input.flags = FEARVR_IF_VALID | FEARVR_IF_FOCUSED;
    input.activeHands =
        FEARVR_HAND_MASK_LEFT | FEARVR_HAND_MASK_RIGHT;
    input.aimPoseValidHands = input.activeHands;
    input.gripPoseValidHands = input.activeHands;
    for (std::uint32_t hand = 0; hand < FEARVR_HAND_COUNT; ++hand) {
        input.handAimPose[hand].qw = 1.0F;
        input.handGripPose[hand].qw = 1.0F;
    }
    input.handGripPose[FEARVR_HAND_LEFT].px = lateralMeters;
    input.handGripPose[FEARVR_HAND_LEFT].pz = -forwardMeters;
    input.squeeze[FEARVR_HAND_LEFT] = leftSqueeze;
    return input;
}

} // namespace

int main() {
    // Die Stuetzhand liegt vor der Waffenhand und nah an der Zielachse.
    const FearVrInputState onWeapon = TwoHandInput(0.35F, 0.03F, 0.9F);
    const fearvr::TwoHandedSupportOffset offset =
        fearvr::LeftHandSupportOffset(onWeapon);
    assert(offset.valid);
    assert(std::fabs(offset.forwardMeters - 0.35F) < 0.001F);
    assert(std::fabs(offset.lateralMeters - 0.03F) < 0.001F);
    assert(fearvr::IsLeftHandOnWeapon(onWeapon));
    assert(fearvr::ShouldEngageTwoHandedGrip(onWeapon));
    assert(!fearvr::ShouldReleaseTwoHandedGrip(onWeapon));

    // Greifen ohne gedrueckten Grabknopf haelt nichts.
    assert(!fearvr::ShouldEngageTwoHandedGrip(
        TwoHandInput(0.35F, 0.03F, 0.2F)));
    // Hysterese: ein gehaltener Griff loest erst unterhalb des zweiten
    // Schwellwerts, ein neuer Griff braucht den vollen Druck.
    const FearVrInputState halfSqueeze =
        TwoHandInput(0.35F, 0.03F, 0.55F);
    assert(!fearvr::ShouldReleaseTwoHandedGrip(halfSqueeze));
    assert(!fearvr::ShouldEngageTwoHandedGrip(halfSqueeze));

    // Der Riegel: Waehrend des Haltens darf die Hand die Zone verlassen,
    // ohne dass der Griff aufgeht. Nur der Knopf loest.
    const FearVrInputState movedAway = TwoHandInput(0.90F, 0.40F, 0.9F);
    assert(!fearvr::IsLeftHandOnWeapon(movedAway));
    assert(!fearvr::ShouldEngageTwoHandedGrip(movedAway));
    assert(!fearvr::ShouldReleaseTwoHandedGrip(movedAway));

    // Die Hand haengt weit neben oder hinter der Waffe: das ist ein Sprint.
    assert(!fearvr::IsLeftHandOnWeapon(
        TwoHandInput(0.35F, 0.40F, 0.9F)));
    assert(!fearvr::IsLeftHandOnWeapon(
        TwoHandInput(-0.30F, 0.03F, 0.9F)));
    assert(!fearvr::IsLeftHandOnWeapon(
        TwoHandInput(0.90F, 0.03F, 0.9F)));

    // Ohne verfolgte Haende gibt es keinen Griff.
    FearVrInputState untracked = TwoHandInput(0.35F, 0.03F, 0.9F);
    untracked.gripPoseValidHands = FEARVR_HAND_MASK_RIGHT;
    assert(!fearvr::LeftHandSupportOffset(untracked).valid);
    assert(!fearvr::ShouldEngageTwoHandedGrip(untracked));

    // Verlorener Fokus loest den Griff, statt ihn haengen zu lassen.
    FearVrInputState unfocused = TwoHandInput(0.35F, 0.03F, 0.9F);
    unfocused.flags = FEARVR_IF_VALID;
    assert(!fearvr::ShouldEngageTwoHandedGrip(unfocused));
    assert(fearvr::ShouldReleaseTwoHandedGrip(unfocused));

    // Gemessene Waffenlaengen: Pistole rund 13 cm, Gewehr rund 37 cm. Die
    // eine bleibt an der rechten Hand, das andere fuehren beide Haende voll.
    assert(fearvr::TwoHandedAimBlend(0.13F) == 0.0F);
    assert(fearvr::TwoHandedAimBlend(0.25F) > 0.0F);
    assert(fearvr::TwoHandedAimBlend(0.25F) <
           fearvr::TwoHandedAimBlend(0.30F));
    assert(std::fabs(
               fearvr::TwoHandedAimBlend(0.37F) -
               fearvr::kTwoHandMaximumBlend) < 0.0001F);
    assert(fearvr::TwoHandedAimBlend(2.0F) <= 1.0F);
    assert(std::fabs(
               fearvr::TwoHandedAimBlend(2.0F) -
               fearvr::kTwoHandMaximumBlend) < 0.0001F);
    assert(fearvr::TwoHandedAimBlend(
               std::numeric_limits<float>::quiet_NaN()) == 0.0F);

    // Solange die Waffe mitgehalten wird, gehoeren linker Grabknopf und
    // linke Handneigung der Waffe.
    FearVrInputState leaning = TwoHandInput(0.35F, 0.03F, 0.9F);
    leaning.handAimPose[FEARVR_HAND_LEFT] = FearVrPose{};
    leaning.handAimPose[FEARVR_HAND_LEFT].qz = std::sin(0.4F);
    leaning.handAimPose[FEARVR_HAND_LEFT].qw = std::cos(0.4F);
    assert(fearvr::MapControllerCommand(
               leaning, fearvr::FEARVR_CMD_LEAN_LEFT, false)
               .active);
    assert(!fearvr::MapControllerCommand(
                leaning, fearvr::FEARVR_CMD_LEAN_LEFT, true)
                .active);
    assert(fearvr::MapControllerCommand(
               leaning, fearvr::FEARVR_CMD_RUN, false)
               .active);
    assert(!fearvr::MapControllerCommand(
                leaning, fearvr::FEARVR_CMD_RUN, true)
                .active);
    // Alles andere bleibt unveraendert.
    leaning.trigger[FEARVR_HAND_RIGHT] = 0.9F;
    assert(fearvr::MapControllerCommand(
               leaning, fearvr::FEARVR_CMD_FIRING, true)
               .active);

    return 0;
}

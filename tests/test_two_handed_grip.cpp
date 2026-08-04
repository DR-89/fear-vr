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

    // With rotation already solved, translation pins the stored support
    // attachment to the tracked left hand instead of leaving the right grip
    // as a permanent origin.
    const fearvr::TwoHandedPivotTranslation pivot =
        fearvr::SolveSecondaryGripPivot(
            {0.20F, 1.20F, -0.10F},
            {-0.10F, 1.25F, -0.50F},
            {-0.25F, 0.05F, -0.35F});
    assert(pivot.valid);
    assert(std::fabs(pivot.correction.x + 0.05F) < 0.0001F);
    assert(std::fabs(pivot.correction.y) < 0.0001F);
    assert(std::fabs(pivot.correction.z + 0.05F) < 0.0001F);
    assert(std::fabs(
        pivot.primaryPosition.x - 0.15F) < 0.0001F);
    const fearvr::TrackingVector pinnedSupport{
        pivot.primaryPosition.x - 0.25F,
        pivot.primaryPosition.y + 0.05F,
        pivot.primaryPosition.z - 0.35F};
    assert(std::fabs(pinnedSupport.x + 0.10F) < 0.0001F);
    assert(std::fabs(pinnedSupport.y - 1.25F) < 0.0001F);
    assert(std::fabs(pinnedSupport.z + 0.50F) < 0.0001F);
    assert(!fearvr::SolveSecondaryGripPivot(
        {}, {}, {}, std::numeric_limits<float>::quiet_NaN()).valid);

    // Only a fore-end attachment that points toward and remains close to the
    // measured muzzle may be cached. Reload poses below the weapon or beyond
    // its barrel caused later support grabs to pitch the gun downward.
    assert(fearvr::IsPlausibleSecondaryGripGeometry(
        {30.0F, 2.0F, 1.0F}, {55.0F, 0.0F, 0.0F}, 8.0F));
    assert(!fearvr::IsPlausibleSecondaryGripGeometry(
        {2.0F, -35.0F, 1.0F}, {55.0F, 0.0F, 0.0F}, 8.0F));
    assert(!fearvr::IsPlausibleSecondaryGripGeometry(
        {70.0F, 0.0F, 0.0F}, {55.0F, 0.0F, 0.0F}, 8.0F));
    assert(!fearvr::IsPlausibleSecondaryGripGeometry(
        {30.0F, 0.0F, 0.0F}, {}, 8.0F));

    // Der Lenkwinkel: bis zur weichen Grenze eins zu eins, darueber gedaempft
    // weiterlaufend statt an einer Wand stehenzubleiben.
    assert(fearvr::SoftLimitedSteerAngle(0.0F) == 0.0F);
    assert(std::fabs(fearvr::SoftLimitedSteerAngle(0.5F) - 0.5F) < 0.0001F);
    assert(std::fabs(
               fearvr::SoftLimitedSteerAngle(
                   fearvr::kTwoHandSoftSteerRadians) -
               fearvr::kTwoHandSoftSteerRadians) < 0.0001F);
    // Knickfrei: direkt hinter der weichen Grenze bleibt die Steigung bei 1.
    const float justAbove = fearvr::kTwoHandSoftSteerRadians + 0.001F;
    assert(std::fabs(
               fearvr::SoftLimitedSteerAngle(justAbove) - justAbove) <
           0.0001F);
    // Darueber steigt der Winkel weiter an, aber langsamer als die Hand.
    const float wide = fearvr::kTwoHandSoftSteerRadians + 0.4F;
    assert(fearvr::SoftLimitedSteerAngle(wide) >
           fearvr::kTwoHandSoftSteerRadians);
    assert(fearvr::SoftLimitedSteerAngle(wide) < wide);
    assert(fearvr::SoftLimitedSteerAngle(wide) <
           fearvr::SoftLimitedSteerAngle(wide + 0.2F));
    // Und laeuft nie ueber die harte Grenze hinaus, auch nicht bei 180 Grad.
    assert(fearvr::SoftLimitedSteerAngle(3.14159F) <
           fearvr::kTwoHandMaxSteerRadians);
    // Ein ungueltiger Winkel lenkt nicht weiter als die weiche Grenze.
    assert(fearvr::SoftLimitedSteerAngle(
               std::numeric_limits<float>::quiet_NaN()) <=
           fearvr::kTwoHandSoftSteerRadians);

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

#include <cassert>
#include <cmath>
#include <limits>

#include "arm_ik.h"

namespace {

float Distance(
    const fearvr::ArmIkVector& left,
    const fearvr::ArmIkVector& right) {
    return std::sqrt(fearvr::ArmIkLengthSquared(
        fearvr::ArmIkSubtract(left, right)));
}

} // namespace

int main() {
    using fearvr::ArmIkDot;
    using fearvr::ArmIkVector;
    using fearvr::SolveTwoBoneElbow;

    const fearvr::ArmIkTuning defaults =
        fearvr::SanitizeArmIkTuning({});
    assert(defaults.elbowOutward == 1.0F);
    assert(defaults.elbowDown == 0.45F);
    assert(defaults.elbowBack == 0.15F);
    assert(defaults.preserveElbowContinuity);
    assert(defaults.leftHandPitchDegrees == 0.0F);

    fearvr::ArmIkTuning unsafe{};
    unsafe.elbowOutward = -10.0F;
    unsafe.elbowDown = 20.0F;
    unsafe.elbowBack = std::numeric_limits<float>::quiet_NaN();
    unsafe.leftHandRightMeters = 3.0F;
    unsafe.leftHandUpMeters = -3.0F;
    unsafe.leftHandPitchDegrees = 900.0F;
    unsafe.leftHandYawDegrees =
        std::numeric_limits<float>::infinity();
    const fearvr::ArmIkTuning sanitized =
        fearvr::SanitizeArmIkTuning(unsafe);
    assert(sanitized.elbowOutward == 0.20F);
    assert(sanitized.elbowDown == 1.5F);
    assert(sanitized.elbowBack == 0.15F);
    assert(sanitized.leftHandRightMeters == 0.20F);
    assert(sanitized.leftHandUpMeters == -0.20F);
    assert(sanitized.leftHandPitchDegrees == 180.0F);
    assert(sanitized.leftHandYawDegrees == 0.0F);

    const ArmIkVector shoulder{0.0F, 0.0F, 0.0F};
    const ArmIkVector target{1.0F, 0.0F, 0.0F};
    const auto down = SolveTwoBoneElbow(
        shoulder, target, 1.0F, 1.0F,
        {0.0F, -1.0F, 0.0F}, {}, false,
        {0.0F, 0.0F, 1.0F});
    assert(down.valid);
    assert(!down.targetClamped);
    assert(std::fabs(down.elbow.x - 0.5F) < 0.0001F);
    assert(down.elbow.y < -0.86F);
    assert(std::fabs(Distance(shoulder, down.elbow) - 1.0F) < 0.0001F);
    assert(std::fabs(Distance(target, down.elbow) - 1.0F) < 0.0001F);

    // Mirrored anatomical poles produce mirrored elbows.
    const auto up = SolveTwoBoneElbow(
        shoulder, target, 1.0F, 1.0F,
        {0.0F, 1.0F, 0.0F}, {}, false,
        {0.0F, 0.0F, 1.0F});
    assert(up.valid);
    assert(std::fabs(up.elbow.x - down.elbow.x) < 0.0001F);
    assert(std::fabs(up.elbow.y + down.elbow.y) < 0.0001F);

    // An unreachable controller is clamped to the arm's measured reach.
    const auto far = SolveTwoBoneElbow(
        shoulder, {10.0F, 0.0F, 0.0F}, 1.0F, 1.0F,
        {0.0F, -1.0F, 0.0F}, {}, false,
        {0.0F, -1.0F, 0.0F});
    assert(far.valid);
    assert(far.targetClamped);
    assert(far.solvedDistance < 2.0F);
    const ArmIkVector clampedHand{far.solvedDistance, 0.0F, 0.0F};
    assert(std::fabs(Distance(shoulder, far.elbow) - 1.0F) < 0.0001F);
    assert(std::fabs(Distance(clampedHand, far.elbow) - 1.0F) < 0.0001F);

    // Crossing the pole-axis singularity must retain the prior bend
    // hemisphere instead of flipping the elbow to the other side.
    const ArmIkVector previousBend{0.0F, 0.0F, 1.0F};
    const auto singularCrossing = SolveTwoBoneElbow(
        shoulder, {0.01F, -1.0F, 0.0F}, 1.0F, 1.0F,
        {0.0F, -1.0F, 0.0F}, previousBend, true,
        {0.0F, 0.0F, -1.0F});
    assert(singularCrossing.valid);
    assert(ArmIkDot(
        singularCrossing.bendDirection, previousBend) > 0.0F);

    const auto invalid = SolveTwoBoneElbow(
        shoulder, target,
        std::numeric_limits<float>::quiet_NaN(), 1.0F,
        {0.0F, -1.0F, 0.0F}, {}, false, {});
    assert(!invalid.valid);
    assert(!SolveTwoBoneElbow(
        shoulder, shoulder, 1.0F, 1.0F,
        {0.0F, -1.0F, 0.0F}, {}, false, {}).valid);

    return 0;
}

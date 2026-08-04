#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>

#include "weapon_weight.h"

namespace {

constexpr float kPi = 3.14159265358979323846F;

fearvr::WeaponWeightPose Pose(float x, float angleRadians = 0.0F) {
    const float halfAngle = 0.5F * angleRadians;
    return {
        {x, 0.0F, 0.0F},
        {0.0F, std::sin(halfAngle), 0.0F, std::cos(halfAngle)}};
}

float QuaternionLength(const fearvr::WeaponWeightQuaternion& value) {
    return std::sqrt(
        value.x * value.x + value.y * value.y +
        value.z * value.z + value.w * value.w);
}

} // namespace

int main() {
    using namespace fearvr;

    const WeaponWeightProfile defaults{};
    assert(defaults.weight == 1.0F);
    assert(defaults.positionalFollow == 18.0F);
    assert(defaults.rotationalFollow == 20.0F);
    assert(defaults.catchUpStrength == 1.5F);
    assert(WeaponWeightPresetMultiplier(WeaponWeightPreset::none) == 0.0F);
    assert(WeaponWeightPresetMultiplier(WeaponWeightPreset::light) == 0.5F);
    assert(WeaponWeightPresetMultiplier(WeaponWeightPreset::medium) == 1.0F);
    assert(WeaponWeightPresetMultiplier(WeaponWeightPreset::heavy) == 2.0F);
    assert(SanitizeWeaponWeightPreset(-4) == WeaponWeightPreset::none);
    assert(SanitizeWeaponWeightPreset(9) == WeaponWeightPreset::heavy);
    assert(ApplyWeaponWeightPreset(
               defaults, WeaponWeightPreset::light).weight == 0.5F);
    assert(ApplyWeaponWeightPreset(
               defaults, WeaponWeightPreset::medium).weight == 1.0F);
    assert(ApplyWeaponWeightPreset(
               defaults, WeaponWeightPreset::heavy).weight == 2.0F);

    WeaponWeightFilterState disabled;
    WeaponWeightPose output;
    assert(UpdateWeaponWeightFilter(
        disabled, Pose(1.0F, 0.5F), true, 1'000'000'000ULL,
        false, defaults, output));
    assert(output.position.x == 1.0F);
    assert(output.orientation.y == Pose(1.0F, 0.5F).orientation.y);
    assert(disabled.linearVelocity.x == 0.0F);

    WeaponWeightFilterState state;
    assert(UpdateWeaponWeightFilter(
        state, Pose(0.0F), true, 1'000'000'000ULL,
        true, defaults, output));
    assert(UpdateWeaponWeightFilter(
        state, Pose(1.0F, kPi * 0.5F), true, 1'011'111'111ULL,
        true, defaults, output));
    assert(output.position.x > 0.0F && output.position.x < 1.0F);
    assert(QuaternionLength(output.orientation) > 0.9999F);
    assert(QuaternionLength(output.orientation) < 1.0001F);

    float previous = output.position.x;
    for (std::uint64_t frame = 2; frame < 240; ++frame) {
        assert(UpdateWeaponWeightFilter(
            state, Pose(1.0F, kPi * 0.5F), true,
            1'000'000'000ULL + frame * 11'111'111ULL,
            true, defaults, output));
        assert(output.position.x >= previous - 1.0e-6F);
        assert(output.position.x <= 1.0F + 1.0e-6F);
        previous = output.position.x;
    }
    assert(std::fabs(output.position.x - 1.0F) < 1.0e-4F);

    WeaponWeightFilterState at90Hz;
    WeaponWeightFilterState at72Hz;
    WeaponWeightPose out90;
    WeaponWeightPose out72;
    UpdateWeaponWeightFilter(
        at90Hz, Pose(0.0F), true, 1'000'000'000ULL,
        true, defaults, out90);
    UpdateWeaponWeightFilter(
        at72Hz, Pose(0.0F), true, 1'000'000'000ULL,
        true, defaults, out72);
    for (std::uint64_t frame = 1; frame <= 90; ++frame) {
        UpdateWeaponWeightFilter(
            at90Hz, Pose(0.5F, 1.0F), true,
            1'000'000'000ULL + frame * 10'000'000ULL,
            true, defaults, out90);
    }
    for (std::uint64_t frame = 1; frame <= 72; ++frame) {
        UpdateWeaponWeightFilter(
            at72Hz, Pose(0.5F, 1.0F), true,
            1'000'000'000ULL + frame * 12'500'000ULL,
            true, defaults, out72);
    }
    assert(std::fabs(out90.position.x - out72.position.x) < 1.0e-4F);

    WeaponWeightDiagnostics diagnostics;
    assert(UpdateWeaponWeightFilter(
        state, Pose(5.0F), true, 5'000'000'000ULL,
        true, defaults, output, &diagnostics));
    assert(output.position.x == 5.0F);
    assert(diagnostics.resetReason ==
           WeaponWeightResetReason::excessiveDeltaTime);
    assert(UpdateWeaponWeightFilter(
        state, Pose(6.0F), true, 5'000'000'000ULL,
        true, defaults, output, &diagnostics));
    assert(diagnostics.resetReason ==
           WeaponWeightResetReason::nonPositiveDeltaTime);
    assert(!UpdateWeaponWeightFilter(
        state, Pose(6.0F), false, 5'010'000'000ULL,
        true, defaults, output, &diagnostics));
    assert(!state.poseValid);
    assert(UpdateWeaponWeightFilter(
        state, Pose(9.0F), true, 5'020'000'000ULL,
        true, defaults, output, &diagnostics));
    assert(output.position.x == 9.0F);
    assert(diagnostics.resetReason ==
           WeaponWeightResetReason::trackingReacquired);

    WeaponWeightPose invalid = Pose(0.0F);
    invalid.position.x = std::numeric_limits<float>::infinity();
    assert(!UpdateWeaponWeightFilter(
        state, invalid, true, 5'030'000'000ULL,
        true, defaults, output));
    assert(!state.poseValid);
    state.linearVelocity.x =
        std::numeric_limits<float>::quiet_NaN();
    assert(UpdateWeaponWeightFilter(
        state, Pose(3.0F), true, 5'040'000'000ULL,
        true, defaults, output));
    assert(std::isfinite(output.position.x));

    assert(WeaponWeightCatchUpMultiplier(0.0F, 1.5F) == 1.0F);
    assert(WeaponWeightCatchUpMultiplier(0.001F, 1.5F) > 1.0F);
    assert(WeaponWeightCatchUpMultiplier(10000.0F, 1.5F) < 2.5001F);

    const WeaponWeightQuaternion q = Pose(0.0F, 0.75F).orientation;
    const WeaponWeightVector positive = QuaternionToRotationVector(q);
    const WeaponWeightVector negative =
        QuaternionToRotationVector({-q.x, -q.y, -q.z, -q.w});
    assert(std::fabs(positive.y - negative.y) < 1.0e-6F);

    return 0;
}

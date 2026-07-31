#include <cassert>
#include <cmath>
#include <cstdint>

#include "weapon_recoil.h"

namespace {

constexpr float kPi = 3.14159265358979323846F;

fearvr::WeaponRecoilOffset FireAtWeight(float weight) {
    fearvr::WeaponWeightProfile profile{};
    profile.weight = weight;
    fearvr::WeaponRecoilState state;
    fearvr::WeaponRecoilOffset output;
    fearvr::UpdateWeaponRecoil(
        state, 1'000'000'000ULL, true, profile, 0, output);
    assert(fearvr::UpdateWeaponRecoil(
        state, 1'011'111'111ULL, true, profile, 1, output));
    return output;
}

} // namespace

int main() {
    using namespace fearvr;

    WeaponWeightProfile profile{};
    WeaponRecoilState state;
    WeaponRecoilOffset output;
    assert(!UpdateWeaponRecoil(
        state, 1'000'000'000ULL, true, profile, 0, output));
    assert(output.backwardMeters == 0.0F);
    assert(output.pitchRadians == 0.0F);

    assert(UpdateWeaponRecoil(
        state, 1'011'111'111ULL, true, profile, 1, output));
    assert(output.backwardMeters > 0.0F);
    assert(output.pitchRadians > 0.0F);
    assert(output.backwardMeters < 0.025F);
    assert(output.pitchRadians < 9.0F * kPi / 180.0F);

    const WeaponRecoilOffset light = FireAtWeight(0.25F);
    const WeaponRecoilOffset normal = FireAtWeight(1.0F);
    const WeaponRecoilOffset heavy = FireAtWeight(4.0F);
    assert(light.backwardMeters > normal.backwardMeters);
    assert(normal.backwardMeters > heavy.backwardMeters);
    assert(light.pitchRadians > normal.pitchRadians);
    assert(normal.pitchRadians > heavy.pitchRadians);

    float peakPitch = output.pitchRadians;
    for (std::uint64_t frame = 2; frame < 240; ++frame) {
        UpdateWeaponRecoil(
            state,
            1'000'000'000ULL + frame * 11'111'111ULL,
            true, profile, 0, output);
        peakPitch = (std::max)(peakPitch, output.pitchRadians);
    }
    assert(peakPitch > normal.pitchRadians);
    assert(output.backwardMeters < 1.0e-5F);
    assert(output.pitchRadians < 1.0e-5F);

    WeaponRecoilState singleState;
    WeaponRecoilState burstState;
    WeaponRecoilOffset single;
    WeaponRecoilOffset burst;
    UpdateWeaponRecoil(
        singleState, 2'000'000'000ULL, true, profile, 0, single);
    UpdateWeaponRecoil(
        burstState, 2'000'000'000ULL, true, profile, 0, burst);
    UpdateWeaponRecoil(
        singleState, 2'010'000'000ULL, true, profile, 1, single);
    UpdateWeaponRecoil(
        burstState, 2'010'000'000ULL, true, profile, 3, burst);
    assert(burst.backwardMeters > single.backwardMeters);
    assert(burst.pitchRadians > single.pitchRadians);

    assert(!UpdateWeaponRecoil(
        burstState, 2'020'000'000ULL, false, profile, 1, burst));
    assert(!burstState.initialized);
    assert(!UpdateWeaponRecoil(
        state, 5'000'000'000ULL, true, profile, 1, output));
    assert(state.backwardMeters == 0.0F);
    assert(state.pitchRadians == 0.0F);

    WeaponWeightPose aim{
        {1.0F, 2.0F, 3.0F}, {0.0F, 0.0F, 0.0F, 1.0F}};
    WeaponWeightPose grip{
        {1.0F, 1.9F, 3.1F}, {0.0F, 0.0F, 0.0F, 1.0F}};
    const WeaponWeightVector separationBefore =
        grip.position - aim.position;
    ApplyWeaponRecoil({0.01F, 0.10F}, aim, grip);
    assert(std::fabs(aim.position.z - 3.01F) < 1.0e-6F);
    assert(std::fabs(grip.position.z - 3.11F) < 1.0e-6F);
    assert(std::fabs(aim.orientation.x - std::sin(0.05F)) < 1.0e-6F);
    const WeaponWeightVector separationAfter =
        grip.position - aim.position;
    assert(std::fabs(separationAfter.x - separationBefore.x) < 1.0e-6F);
    assert(std::fabs(separationAfter.y - separationBefore.y) < 1.0e-6F);
    assert(std::fabs(separationAfter.z - separationBefore.z) < 1.0e-6F);

    const float halfYaw = 0.25F * kPi;
    aim = {{0.0F, 0.0F, 0.0F},
           {0.0F, std::sin(halfYaw), 0.0F, std::cos(halfYaw)}};
    grip = aim;
    ApplyWeaponRecoil({0.01F, 0.0F}, aim, grip);
    assert(std::fabs(aim.position.x - 0.01F) < 1.0e-6F);
    assert(std::fabs(aim.position.z) < 1.0e-6F);

    return 0;
}

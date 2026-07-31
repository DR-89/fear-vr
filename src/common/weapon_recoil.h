#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "weapon_weight.h"

namespace fearvr {

// Recoil is expressed in the weapon's OpenXR aim space: +Z is backwards
// (OpenXR points forward along -Z) and a positive rotation around +X raises
// the muzzle. The offsets are layered after the simulated-weight filter so
// controller motion cannot immediately erase a shot impulse.
struct WeaponRecoilOffset {
    float backwardMeters{0.0F};
    float pitchRadians{0.0F};
};

struct WeaponRecoilState {
    bool initialized{false};
    std::uint64_t lastUpdateTimestampNs{0};
    float backwardMeters{0.0F};
    float backwardVelocity{0.0F};
    float pitchRadians{0.0F};
    float pitchVelocity{0.0F};
};

inline void ResetWeaponRecoil(WeaponRecoilState& state) noexcept {
    state = {};
}

inline bool WeaponRecoilStateIsFinite(
    const WeaponRecoilState& state) noexcept {
    return std::isfinite(state.backwardMeters) &&
           std::isfinite(state.backwardVelocity) &&
           std::isfinite(state.pitchRadians) &&
           std::isfinite(state.pitchVelocity);
}

// Advances a critically damped recoil spring and folds successful shots into
// its velocity. Weight acts as mass (impulse / mass), while the same follow
// rates as the pose filter govern recovery. This makes every existing weapon
// profile useful for both handling inertia and firing response.
inline bool UpdateWeaponRecoil(
    WeaponRecoilState& state, std::uint64_t timestampNs, bool enabled,
    const WeaponWeightProfile& requestedProfile,
    std::uint32_t successfulShots,
    WeaponRecoilOffset& output) noexcept {
    output = {};
    if (!enabled || timestampNs == 0) {
        ResetWeaponRecoil(state);
        return false;
    }

    float deltaSeconds = 0.0F;
    if (!state.initialized) {
        state.initialized = true;
        state.lastUpdateTimestampNs = timestampNs;
    } else {
        if (!WeaponRecoilStateIsFinite(state) ||
            timestampNs <= state.lastUpdateTimestampNs ||
            timestampNs - state.lastUpdateTimestampNs > 100'000'000ULL) {
            ResetWeaponRecoil(state);
            state.initialized = true;
            state.lastUpdateTimestampNs = timestampNs;
            return false;
        }
        deltaSeconds = static_cast<float>(
            static_cast<double>(timestampNs - state.lastUpdateTimestampNs) /
            1'000'000'000.0);
        state.lastUpdateTimestampNs = timestampNs;
    }

    const WeaponWeightProfile profile =
        SanitizeWeaponWeightProfile(requestedProfile);
    const float mass = profile.weight;
    const float massScale = std::sqrt(mass);
    const std::uint32_t shots = (std::min)(successfulShots, 8U);
    if (shots != 0) {
        constexpr float kBackwardImpulseMetersPerSecond = 0.22F;
        constexpr float kPitchImpulseRadiansPerSecond = 2.20F;
        state.backwardVelocity +=
            kBackwardImpulseMetersPerSecond *
            static_cast<float>(shots) / mass;
        state.pitchVelocity +=
            kPitchImpulseRadiansPerSecond *
            static_cast<float>(shots) / mass;
        // Bound automatic-fire accumulation without clipping ordinary shots.
        state.backwardVelocity =
            (std::min)(state.backwardVelocity, 0.65F);
        state.pitchVelocity =
            (std::min)(state.pitchVelocity, 6.5F);
    }

    if (deltaSeconds > 0.0F) {
        const float positionOmega =
            profile.positionalFollow / massScale;
        const float rotationOmega =
            profile.rotationalFollow / massScale;
        SolveCriticallyDampedComponent(
            state.backwardMeters, state.backwardVelocity,
            positionOmega, deltaSeconds);
        SolveCriticallyDampedComponent(
            state.pitchRadians, state.pitchVelocity,
            rotationOmega, deltaSeconds);
    }

    constexpr float kMaximumBackwardMeters = 0.025F;
    constexpr float kMaximumPitchRadians =
        9.0F * 3.14159265358979323846F / 180.0F;
    state.backwardMeters = std::clamp(
        state.backwardMeters, 0.0F, kMaximumBackwardMeters);
    state.pitchRadians = std::clamp(
        state.pitchRadians, 0.0F, kMaximumPitchRadians);
    if (!WeaponRecoilStateIsFinite(state)) {
        ResetWeaponRecoil(state);
        return false;
    }

    if (std::fabs(state.backwardMeters) < 1.0e-6F &&
        std::fabs(state.backwardVelocity) < 1.0e-5F) {
        state.backwardMeters = 0.0F;
        state.backwardVelocity = 0.0F;
    }
    if (std::fabs(state.pitchRadians) < 1.0e-6F &&
        std::fabs(state.pitchVelocity) < 1.0e-5F) {
        state.pitchRadians = 0.0F;
        state.pitchVelocity = 0.0F;
    }

    output.backwardMeters = state.backwardMeters;
    output.pitchRadians = state.pitchRadians;
    return state.backwardMeters != 0.0F ||
           state.pitchRadians != 0.0F ||
           state.backwardVelocity != 0.0F ||
           state.pitchVelocity != 0.0F;
}

inline WeaponWeightVector RotateWeaponWeightVector(
    const WeaponWeightQuaternion& requestedRotation,
    const WeaponWeightVector& value) noexcept {
    const WeaponWeightQuaternion rotation =
        Normalize(requestedRotation);
    const WeaponWeightVector quaternionVector{
        rotation.x, rotation.y, rotation.z};
    const WeaponWeightVector twiceCross{
        2.0F * (quaternionVector.y * value.z -
                quaternionVector.z * value.y),
        2.0F * (quaternionVector.z * value.x -
                quaternionVector.x * value.z),
        2.0F * (quaternionVector.x * value.y -
                quaternionVector.y * value.x)};
    return {
        value.x + rotation.w * twiceCross.x +
            quaternionVector.y * twiceCross.z -
            quaternionVector.z * twiceCross.y,
        value.y + rotation.w * twiceCross.y +
            quaternionVector.z * twiceCross.x -
            quaternionVector.x * twiceCross.z,
        value.z + rotation.w * twiceCross.z +
            quaternionVector.x * twiceCross.y -
            quaternionVector.y * twiceCross.x};
}

// Applies one rigid recoil offset to the independently filtered aim and grip
// poses. Their separation is preserved, preventing the visible gun and hand
// socket from pulling apart during recovery.
inline void ApplyWeaponRecoil(
    const WeaponRecoilOffset& recoil, WeaponWeightPose& aimPose,
    WeaponWeightPose& gripPose) noexcept {
    const WeaponWeightVector backward = RotateWeaponWeightVector(
        aimPose.orientation, {0.0F, 0.0F, recoil.backwardMeters});
    aimPose.position = aimPose.position + backward;
    gripPose.position = gripPose.position + backward;

    const WeaponWeightQuaternion pitch =
        RotationVectorToQuaternion({recoil.pitchRadians, 0.0F, 0.0F});
    aimPose.orientation = Normalize(Multiply(aimPose.orientation, pitch));
    gripPose.orientation = Normalize(Multiply(gripPose.orientation, pitch));
}

} // namespace fearvr

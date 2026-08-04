#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace fearvr {

struct WeaponWeightVector {
    float x{0.0F};
    float y{0.0F};
    float z{0.0F};
};

struct WeaponWeightQuaternion {
    float x{0.0F};
    float y{0.0F};
    float z{0.0F};
    float w{1.0F};
};

struct WeaponWeightPose {
    WeaponWeightVector position;
    WeaponWeightQuaternion orientation;
};

struct WeaponWeightProfile {
    float weight{1.0F};
    float positionalFollow{18.0F};
    float rotationalFollow{20.0F};
    float catchUpStrength{1.5F};
};

enum class WeaponWeightPreset : std::uint8_t {
    none = 0,
    light = 1,
    medium = 2,
    heavy = 3
};

inline WeaponWeightPreset SanitizeWeaponWeightPreset(int value) noexcept {
    return static_cast<WeaponWeightPreset>(std::clamp(value, 0, 3));
}

inline float WeaponWeightPresetMultiplier(
    WeaponWeightPreset preset) noexcept {
    switch (preset) {
    case WeaponWeightPreset::light:
        return 0.5F;
    case WeaponWeightPreset::medium:
        return 1.0F;
    case WeaponWeightPreset::heavy:
        return 2.0F;
    case WeaponWeightPreset::none:
    default:
        return 0.0F;
    }
}

enum class WeaponWeightResetReason : std::uint8_t {
    none,
    firstValidPose,
    enabledChanged,
    weaponChanged,
    owningHandChanged,
    trackingLost,
    trackingReacquired,
    referenceSpaceChanged,
    teleportedOrRecentered,
    sceneLoaded,
    objectRecreated,
    nonPositiveDeltaTime,
    excessiveDeltaTime,
    nonFiniteValue
};

struct WeaponWeightDiagnostics {
    WeaponWeightPose rawPose;
    WeaponWeightPose filteredPose;
    WeaponWeightVector linearVelocity;
    WeaponWeightVector angularVelocity;
    float positionalErrorMeters{0.0F};
    float angularErrorRadians{0.0F};
    float effectivePositionOmega{0.0F};
    float effectiveRotationOmega{0.0F};
    WeaponWeightResetReason resetReason{WeaponWeightResetReason::none};
    bool poseValid{false};
};

// One writer owns this state. Publish the completed pose as one immutable
// snapshot when gameplay and rendering run on separate threads.
struct WeaponWeightFilterState {
    bool initialized{false};
    WeaponWeightVector position;
    WeaponWeightVector linearVelocity;
    WeaponWeightQuaternion orientation;
    WeaponWeightVector angularVelocity;
    std::uint64_t lastUpdateTimestampNs{0};
    bool poseValid{false};
    bool enabledOnLastUpdate{false};
    WeaponWeightResetReason lastResetReason{
        WeaponWeightResetReason::none};
};

struct WeaponWeightPairState {
    WeaponWeightFilterState aim;
    WeaponWeightFilterState grip;
};

inline bool IsFinite(const WeaponWeightVector& value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) &&
           std::isfinite(value.z);
}

inline bool IsFinite(const WeaponWeightQuaternion& value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) &&
           std::isfinite(value.z) && std::isfinite(value.w);
}

inline bool IsFinite(const WeaponWeightPose& value) noexcept {
    return IsFinite(value.position) && IsFinite(value.orientation);
}

inline WeaponWeightVector operator+(
    const WeaponWeightVector& left,
    const WeaponWeightVector& right) noexcept {
    return {left.x + right.x, left.y + right.y, left.z + right.z};
}

inline WeaponWeightVector operator-(
    const WeaponWeightVector& left,
    const WeaponWeightVector& right) noexcept {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

inline WeaponWeightVector operator*(
    const WeaponWeightVector& value, float scale) noexcept {
    return {value.x * scale, value.y * scale, value.z * scale};
}

inline float Length(const WeaponWeightVector& value) noexcept {
    return std::sqrt(
        value.x * value.x + value.y * value.y + value.z * value.z);
}

inline WeaponWeightQuaternion Normalize(
    const WeaponWeightQuaternion& value) noexcept {
    const float lengthSquared =
        value.x * value.x + value.y * value.y +
        value.z * value.z + value.w * value.w;
    if (!std::isfinite(lengthSquared) ||
        lengthSquared <= std::numeric_limits<float>::min()) {
        return {};
    }
    const float inverseLength = 1.0F / std::sqrt(lengthSquared);
    return {
        value.x * inverseLength, value.y * inverseLength,
        value.z * inverseLength, value.w * inverseLength};
}

inline WeaponWeightQuaternion Conjugate(
    const WeaponWeightQuaternion& value) noexcept {
    return {-value.x, -value.y, -value.z, value.w};
}

inline WeaponWeightQuaternion Multiply(
    const WeaponWeightQuaternion& left,
    const WeaponWeightQuaternion& right) noexcept {
    return {
        left.w * right.x + left.x * right.w +
            left.y * right.z - left.z * right.y,
        left.w * right.y - left.x * right.z +
            left.y * right.w + left.z * right.x,
        left.w * right.z + left.x * right.y -
            left.y * right.x + left.z * right.w,
        left.w * right.w - left.x * right.x -
            left.y * right.y - left.z * right.z};
}

inline WeaponWeightVector QuaternionToRotationVector(
    WeaponWeightQuaternion value) noexcept {
    value = Normalize(value);
    if (value.w < 0.0F) {
        value.x = -value.x;
        value.y = -value.y;
        value.z = -value.z;
        value.w = -value.w;
    }
    const float vectorLength =
        std::sqrt(value.x * value.x + value.y * value.y +
                  value.z * value.z);
    if (vectorLength < 1.0e-6F) {
        return {2.0F * value.x, 2.0F * value.y, 2.0F * value.z};
    }
    const float angle = 2.0F * std::atan2(
        vectorLength, std::clamp(value.w, -1.0F, 1.0F));
    const float scale = angle / vectorLength;
    return {value.x * scale, value.y * scale, value.z * scale};
}

inline WeaponWeightQuaternion RotationVectorToQuaternion(
    const WeaponWeightVector& value) noexcept {
    const float angle = Length(value);
    if (!std::isfinite(angle)) {
        return {};
    }
    if (angle < 1.0e-6F) {
        return Normalize(
            WeaponWeightQuaternion{0.5F * value.x, 0.5F * value.y, 0.5F * value.z, 1.0F});
    }
    const float halfAngle = 0.5F * angle;
    const float scale = std::sin(halfAngle) / angle;
    return Normalize(WeaponWeightQuaternion{
        value.x * scale, value.y * scale, value.z * scale,
        std::cos(halfAngle)});
}

inline WeaponWeightProfile SanitizeWeaponWeightProfile(
    const WeaponWeightProfile& profile) noexcept {
    WeaponWeightProfile result = profile;
    result.weight = std::isfinite(result.weight)
        ? std::clamp(result.weight, 0.10F, 4.00F) : 1.0F;
    result.positionalFollow = std::isfinite(result.positionalFollow)
        ? std::clamp(result.positionalFollow, 2.0F, 40.0F) : 18.0F;
    result.rotationalFollow = std::isfinite(result.rotationalFollow)
        ? std::clamp(result.rotationalFollow, 2.0F, 40.0F) : 20.0F;
    result.catchUpStrength = std::isfinite(result.catchUpStrength)
        ? std::clamp(result.catchUpStrength, 0.0F, 4.0F) : 1.5F;
    return result;
}

inline WeaponWeightProfile ApplyWeaponWeightPreset(
    const WeaponWeightProfile& profile,
    WeaponWeightPreset preset) noexcept {
    WeaponWeightProfile result = profile;
    const float multiplier = WeaponWeightPresetMultiplier(preset);
    if (multiplier > 0.0F) {
        result.weight *= multiplier;
    }
    return SanitizeWeaponWeightProfile(result);
}

inline float WeaponWeightCatchUpMultiplier(
    float normalizedError, float strength) noexcept {
    if (!std::isfinite(normalizedError) || !std::isfinite(strength)) {
        return 1.0F;
    }
    const float xSquared = normalizedError * normalizedError;
    const float response = std::isfinite(xSquared)
        ? xSquared / (1.0F + xSquared) : 1.0F;
    return 1.0F + std::clamp(strength, 0.0F, 4.0F) * response;
}

inline void SolveCriticallyDampedComponent(
    float& error, float& velocity, float omega, float deltaSeconds) noexcept {
    if (error * velocity < 0.0F &&
        std::fabs(velocity) > omega * std::fabs(error)) {
        velocity = -omega * error;
    }
    const float j = velocity + error * omega;
    const float decay = std::exp(-omega * deltaSeconds);
    error = (error + j * deltaSeconds) * decay;
    velocity = (velocity - j * omega * deltaSeconds) * decay;
}

inline void ClearWeaponWeightFilter(
    WeaponWeightFilterState& state,
    WeaponWeightResetReason reason) noexcept {
    state = {};
    state.lastResetReason = reason;
}

inline void SnapWeaponWeightFilter(
    WeaponWeightFilterState& state, const WeaponWeightPose& target,
    std::uint64_t timestampNs, bool enabled,
    WeaponWeightResetReason reason) noexcept {
    state.initialized = true;
    state.position = target.position;
    state.linearVelocity = {};
    state.orientation = Normalize(target.orientation);
    state.angularVelocity = {};
    state.lastUpdateTimestampNs = timestampNs;
    state.poseValid = true;
    state.enabledOnLastUpdate = enabled;
    state.lastResetReason = reason;
}

inline bool WeaponWeightFilterStateIsFinite(
    const WeaponWeightFilterState& state) noexcept {
    return IsFinite(state.position) && IsFinite(state.linearVelocity) &&
           IsFinite(state.orientation) && IsFinite(state.angularVelocity);
}

inline bool UpdateWeaponWeightFilter(
    WeaponWeightFilterState& state, const WeaponWeightPose& targetPose,
    bool targetValid, std::uint64_t timestampNs, bool enabled,
    const WeaponWeightProfile& requestedProfile,
    WeaponWeightPose& output,
    WeaponWeightDiagnostics* diagnostics = nullptr) noexcept {
    WeaponWeightDiagnostics localDiagnostics{};
    localDiagnostics.rawPose = targetPose;

    const float targetOrientationLengthSquared =
        targetPose.orientation.x * targetPose.orientation.x +
        targetPose.orientation.y * targetPose.orientation.y +
        targetPose.orientation.z * targetPose.orientation.z +
        targetPose.orientation.w * targetPose.orientation.w;
    if (!targetValid || !IsFinite(targetPose) ||
        !std::isfinite(targetOrientationLengthSquared) ||
        targetOrientationLengthSquared <= std::numeric_limits<float>::min()) {
        ClearWeaponWeightFilter(
            state, targetValid
                ? WeaponWeightResetReason::nonFiniteValue
                : WeaponWeightResetReason::trackingLost);
        localDiagnostics.resetReason = state.lastResetReason;
        if (diagnostics != nullptr) {
            *diagnostics = localDiagnostics;
        }
        return false;
    }

    WeaponWeightPose target = targetPose;
    target.orientation = Normalize(target.orientation);
    const float normalizedLength =
        target.orientation.x * target.orientation.x +
        target.orientation.y * target.orientation.y +
        target.orientation.z * target.orientation.z +
        target.orientation.w * target.orientation.w;
    if (!IsFinite(target.orientation) || normalizedLength < 0.5F) {
        ClearWeaponWeightFilter(
            state, WeaponWeightResetReason::nonFiniteValue);
        localDiagnostics.resetReason = state.lastResetReason;
        if (diagnostics != nullptr) {
            *diagnostics = localDiagnostics;
        }
        return false;
    }

    WeaponWeightResetReason snapReason = WeaponWeightResetReason::none;
    if (!state.initialized || !state.poseValid) {
        snapReason = state.lastResetReason ==
                WeaponWeightResetReason::trackingLost
            ? WeaponWeightResetReason::trackingReacquired
            : WeaponWeightResetReason::firstValidPose;
    } else if (state.enabledOnLastUpdate != enabled) {
        snapReason = WeaponWeightResetReason::enabledChanged;
    } else if (!WeaponWeightFilterStateIsFinite(state)) {
        snapReason = WeaponWeightResetReason::nonFiniteValue;
    } else if (timestampNs <= state.lastUpdateTimestampNs) {
        snapReason = WeaponWeightResetReason::nonPositiveDeltaTime;
    } else if (timestampNs - state.lastUpdateTimestampNs >
               100'000'000ULL) {
        snapReason = WeaponWeightResetReason::excessiveDeltaTime;
    }

    if (snapReason != WeaponWeightResetReason::none || !enabled) {
        SnapWeaponWeightFilter(
            state, target, timestampNs, enabled, snapReason);
        output = target;
        localDiagnostics.filteredPose = output;
        localDiagnostics.resetReason = snapReason;
        localDiagnostics.poseValid = true;
        if (diagnostics != nullptr) {
            *diagnostics = localDiagnostics;
        }
        return true;
    }

    const WeaponWeightProfile profile =
        SanitizeWeaponWeightProfile(requestedProfile);
    const float deltaSeconds = static_cast<float>(
        static_cast<double>(timestampNs - state.lastUpdateTimestampNs) /
        1'000'000'000.0);
    const float massScale = std::sqrt(profile.weight);
    const WeaponWeightVector positionError =
        state.position - target.position;
    const WeaponWeightQuaternion rotationErrorQuaternion = Multiply(
        Conjugate(target.orientation), Normalize(state.orientation));
    const WeaponWeightVector rotationError =
        QuaternionToRotationVector(rotationErrorQuaternion);
    const float positionErrorLength = Length(positionError);
    const float rotationErrorLength = Length(rotationError);
    const float positionOmega =
        profile.positionalFollow / massScale *
        WeaponWeightCatchUpMultiplier(
            positionErrorLength / 0.12F, profile.catchUpStrength);
    constexpr float kThirtyFiveDegreesRadians =
        35.0F * 3.14159265358979323846F / 180.0F;
    const float rotationOmega =
        profile.rotationalFollow / massScale *
        WeaponWeightCatchUpMultiplier(
            rotationErrorLength / kThirtyFiveDegreesRadians,
            profile.catchUpStrength);

    WeaponWeightVector solvedPositionError = positionError;
    SolveCriticallyDampedComponent(
        solvedPositionError.x, state.linearVelocity.x,
        positionOmega, deltaSeconds);
    SolveCriticallyDampedComponent(
        solvedPositionError.y, state.linearVelocity.y,
        positionOmega, deltaSeconds);
    SolveCriticallyDampedComponent(
        solvedPositionError.z, state.linearVelocity.z,
        positionOmega, deltaSeconds);

    WeaponWeightVector solvedRotationError = rotationError;
    SolveCriticallyDampedComponent(
        solvedRotationError.x, state.angularVelocity.x,
        rotationOmega, deltaSeconds);
    SolveCriticallyDampedComponent(
        solvedRotationError.y, state.angularVelocity.y,
        rotationOmega, deltaSeconds);
    SolveCriticallyDampedComponent(
        solvedRotationError.z, state.angularVelocity.z,
        rotationOmega, deltaSeconds);

    state.position = target.position + solvedPositionError;
    state.orientation = Normalize(Multiply(
        target.orientation,
        RotationVectorToQuaternion(solvedRotationError)));
    state.lastUpdateTimestampNs = timestampNs;
    state.poseValid = true;
    state.enabledOnLastUpdate = true;
    state.lastResetReason = WeaponWeightResetReason::none;

    if (!WeaponWeightFilterStateIsFinite(state)) {
        SnapWeaponWeightFilter(
            state, target, timestampNs, enabled,
            WeaponWeightResetReason::nonFiniteValue);
    }

    output = {state.position, state.orientation};
    localDiagnostics.filteredPose = output;
    localDiagnostics.linearVelocity = state.linearVelocity;
    localDiagnostics.angularVelocity = state.angularVelocity;
    localDiagnostics.positionalErrorMeters =
        Length(state.position - target.position);
    localDiagnostics.angularErrorRadians = Length(
        QuaternionToRotationVector(Multiply(
            Conjugate(target.orientation), state.orientation)));
    localDiagnostics.effectivePositionOmega = positionOmega;
    localDiagnostics.effectiveRotationOmega = rotationOmega;
    localDiagnostics.resetReason = state.lastResetReason;
    localDiagnostics.poseValid = true;
    if (diagnostics != nullptr) {
        *diagnostics = localDiagnostics;
    }
    return true;
}

inline void ResetWeaponWeightPair(
    WeaponWeightPairState& state,
    WeaponWeightResetReason reason) noexcept {
    ClearWeaponWeightFilter(state.aim, reason);
    ClearWeaponWeightFilter(state.grip, reason);
}

} // namespace fearvr

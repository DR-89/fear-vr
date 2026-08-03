#pragma once

#include <algorithm>
#include <cmath>

namespace fearvr {

// Small engine-independent vector used by the arm solver. Keeping the
// geometric part independent from LithTech lets it be tested without loading
// the Retail model interfaces.
struct ArmIkVector {
    float x{0.0F};
    float y{0.0F};
    float z{0.0F};
};

struct TwoBoneElbowSolution {
    ArmIkVector elbow;
    ArmIkVector bendDirection;
    float solvedDistance{0.0F};
    bool targetClamped{false};
    bool valid{false};
};

// User-facing tuning values for the visible Retail arms and off-hand. The
// elbow components form a body-relative pole direction. Hand translation is
// in the controller's local frame and hand rotation is applied after the raw
// OpenXR grip orientation.
struct ArmIkTuning {
    float elbowOutward{1.0F};
    float elbowDown{0.45F};
    float elbowBack{0.15F};
    bool preserveElbowContinuity{true};
    float leftHandRightMeters{0.0F};
    float leftHandUpMeters{0.0F};
    float leftHandForwardMeters{0.0F};
    float leftHandPitchDegrees{0.0F};
    float leftHandYawDegrees{0.0F};
    float leftHandRollDegrees{0.0F};
};

inline ArmIkTuning SanitizeArmIkTuning(ArmIkTuning tuning) noexcept {
    const auto sane = [](float value, float fallback,
                         float minimum, float maximum) noexcept {
        return std::isfinite(value)
            ? (std::max)(minimum, (std::min)(maximum, value))
            : fallback;
    };
    tuning.elbowOutward = sane(tuning.elbowOutward, 1.0F, 0.20F, 2.0F);
    tuning.elbowDown = sane(tuning.elbowDown, 0.45F, 0.0F, 1.5F);
    tuning.elbowBack = sane(tuning.elbowBack, 0.15F, -1.0F, 1.0F);
    tuning.leftHandRightMeters = sane(
        tuning.leftHandRightMeters, 0.0F, -0.20F, 0.20F);
    tuning.leftHandUpMeters = sane(
        tuning.leftHandUpMeters, 0.0F, -0.20F, 0.20F);
    tuning.leftHandForwardMeters = sane(
        tuning.leftHandForwardMeters, 0.0F, -0.20F, 0.20F);
    tuning.leftHandPitchDegrees = sane(
        tuning.leftHandPitchDegrees, 0.0F, -180.0F, 180.0F);
    tuning.leftHandYawDegrees = sane(
        tuning.leftHandYawDegrees, 0.0F, -180.0F, 180.0F);
    tuning.leftHandRollDegrees = sane(
        tuning.leftHandRollDegrees, 0.0F, -180.0F, 180.0F);
    return tuning;
}

inline ArmIkVector ArmIkAdd(
    const ArmIkVector& left, const ArmIkVector& right) noexcept {
    return {left.x + right.x, left.y + right.y, left.z + right.z};
}

inline ArmIkVector ArmIkSubtract(
    const ArmIkVector& left, const ArmIkVector& right) noexcept {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

inline ArmIkVector ArmIkScale(
    const ArmIkVector& value, float scale) noexcept {
    return {value.x * scale, value.y * scale, value.z * scale};
}

inline float ArmIkDot(
    const ArmIkVector& left, const ArmIkVector& right) noexcept {
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

inline float ArmIkLengthSquared(const ArmIkVector& value) noexcept {
    return ArmIkDot(value, value);
}

inline bool ArmIkNormalize(
    ArmIkVector value, ArmIkVector& normalized) noexcept {
    const float lengthSquared = ArmIkLengthSquared(value);
    if (!std::isfinite(lengthSquared) || lengthSquared < 0.000001F) {
        return false;
    }
    const float inverseLength = 1.0F / std::sqrt(lengthSquared);
    normalized = ArmIkScale(value, inverseLength);
    return true;
}

inline bool ArmIkProjectOntoPlane(
    const ArmIkVector& value, const ArmIkVector& planeNormal,
    ArmIkVector& projected) noexcept {
    return ArmIkNormalize(
        ArmIkSubtract(
            value, ArmIkScale(planeNormal, ArmIkDot(value, planeNormal))),
        projected);
}

// Analytic two-bone elbow solve with a stable pole plane.
//
// The pole is body-relative (outward/downward for an arm). A remembered bend
// direction selects the same hemisphere when the target passes close to the
// pole axis, where the mathematical solution otherwise has no stable side.
// The animated direction remains only as a final fallback for malformed or
// singular pole inputs; animation can no longer steer the elbow every frame.
inline TwoBoneElbowSolution SolveTwoBoneElbow(
    const ArmIkVector& shoulder, const ArmIkVector& target,
    float upperLength, float lowerLength,
    const ArmIkVector& poleDirection,
    const ArmIkVector& previousBendDirection,
    bool previousBendValid,
    const ArmIkVector& animatedUpperDirection) noexcept {
    TwoBoneElbowSolution solution{};
    if (!std::isfinite(upperLength) || !std::isfinite(lowerLength) ||
        upperLength < 0.01F || lowerLength < 0.01F) {
        return solution;
    }

    ArmIkVector targetDirection;
    const ArmIkVector shoulderToTarget = ArmIkSubtract(target, shoulder);
    if (!ArmIkNormalize(shoulderToTarget, targetDirection)) {
        return solution;
    }
    const float targetDistance =
        std::sqrt(ArmIkLengthSquared(shoulderToTarget));
    const float minimumReach =
        std::fabs(upperLength - lowerLength) + 0.01F;
    const float maximumReach = upperLength + lowerLength - 0.01F;
    if (minimumReach > maximumReach) {
        return solution;
    }
    const float solvedDistance = (std::max)(
        minimumReach, (std::min)(maximumReach, targetDistance));
    const float along =
        (upperLength * upperLength - lowerLength * lowerLength +
         solvedDistance * solvedDistance) /
        (2.0F * solvedDistance);
    const float bendHeight = std::sqrt((std::max)(
        0.0F, upperLength * upperLength - along * along));

    ArmIkVector previousProjected;
    const bool hasPrevious = previousBendValid &&
        ArmIkProjectOntoPlane(
            previousBendDirection, targetDirection, previousProjected);
    const ArmIkVector rawPoleProjection = ArmIkSubtract(
        poleDirection,
        ArmIkScale(
            targetDirection, ArmIkDot(poleDirection, targetDirection)));
    ArmIkVector poleProjected;
    const bool hasPole = ArmIkNormalize(
        rawPoleProjection, poleProjected);
    const float poleLengthSquared = ArmIkLengthSquared(poleDirection);
    const float poleStrength = hasPole && poleLengthSquared > 0.000001F
        ? std::sqrt(
              ArmIkLengthSquared(rawPoleProjection) /
              poleLengthSquared)
        : 0.0F;

    ArmIkVector bendDirection{};
    if (hasPole) {
        bendDirection = poleProjected;
        // A pole defines a plane, but at the parallel singularity its
        // projected sign reverses. Retain the prior hemisphere across it.
        if (hasPrevious && ArmIkDot(bendDirection, previousProjected) < 0.0F) {
            bendDirection = ArmIkScale(bendDirection, -1.0F);
        }
        if (hasPrevious) {
            // Fade spatially from the remembered plane while the pole is
            // nearly parallel to the reach direction. This avoids a hard
            // turn as the target enters or leaves the singular cone.
            const float poleWeight = (std::max)(
                0.0F, (std::min)(
                    1.0F, (poleStrength - 0.05F) / 0.30F));
            ArmIkVector blended;
            if (ArmIkNormalize(
                    ArmIkAdd(
                        ArmIkScale(previousProjected, 1.0F - poleWeight),
                        ArmIkScale(bendDirection, poleWeight)),
                    blended)) {
                bendDirection = blended;
            }
        }
    } else if (hasPrevious) {
        bendDirection = previousProjected;
    } else if (!ArmIkProjectOntoPlane(
                   animatedUpperDirection, targetDirection,
                   bendDirection)) {
        // Deterministic final fallback for a completely straight bind pose.
        const ArmIkVector reference =
            std::fabs(targetDirection.y) < 0.9F
                ? ArmIkVector{0.0F, -1.0F, 0.0F}
                : ArmIkVector{1.0F, 0.0F, 0.0F};
        if (!ArmIkProjectOntoPlane(
                reference, targetDirection, bendDirection)) {
            return solution;
        }
    }

    solution.elbow = ArmIkAdd(
        shoulder,
        ArmIkAdd(
            ArmIkScale(targetDirection, along),
            ArmIkScale(bendDirection, bendHeight)));
    solution.bendDirection = bendDirection;
    solution.solvedDistance = solvedDistance;
    solution.targetClamped =
        std::fabs(solvedDistance - targetDistance) > 0.0001F;
    solution.valid = std::isfinite(solution.elbow.x) &&
        std::isfinite(solution.elbow.y) &&
        std::isfinite(solution.elbow.z);
    return solution;
}

} // namespace fearvr

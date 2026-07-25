#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "protocol.h"

namespace fearvr {

struct TrackingVector {
    float x{0.0F};
    float y{0.0F};
    float z{0.0F};
};

struct TrackingQuaternion {
    float x{0.0F};
    float y{0.0F};
    float z{0.0F};
    float w{1.0F};
};

struct RelativeEyePose {
    TrackingVector positionMeters;
    TrackingQuaternion rotation;
    bool valid{false};
};

inline bool IsFinite(const TrackingVector& value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) &&
           std::isfinite(value.z);
}

inline bool IsFinite(const TrackingQuaternion& value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) &&
           std::isfinite(value.z) && std::isfinite(value.w);
}

inline TrackingQuaternion Normalize(
    const TrackingQuaternion& value) noexcept {
    const float magnitudeSquared =
        value.x * value.x + value.y * value.y +
        value.z * value.z + value.w * value.w;
    if (!IsFinite(value) || !std::isfinite(magnitudeSquared) ||
        magnitudeSquared < 0.000001F) {
        return {};
    }
    const float inverseMagnitude = 1.0F / std::sqrt(magnitudeSquared);
    return {value.x * inverseMagnitude, value.y * inverseMagnitude,
            value.z * inverseMagnitude, value.w * inverseMagnitude};
}

inline float Dot(const TrackingQuaternion& left,
                 const TrackingQuaternion& right) noexcept {
    return left.x * right.x + left.y * right.y +
           left.z * right.z + left.w * right.w;
}

inline TrackingQuaternion Conjugate(
    const TrackingQuaternion& value) noexcept {
    return {-value.x, -value.y, -value.z, value.w};
}

inline TrackingQuaternion Multiply(
    const TrackingQuaternion& left,
    const TrackingQuaternion& right) noexcept {
    return Normalize({
        left.w * right.x + left.x * right.w +
            left.y * right.z - left.z * right.y,
        left.w * right.y - left.x * right.z +
            left.y * right.w + left.z * right.x,
        left.w * right.z + left.x * right.y -
            left.y * right.x + left.z * right.w,
        left.w * right.w - left.x * right.x -
            left.y * right.y - left.z * right.z});
}

inline TrackingVector Rotate(
    const TrackingQuaternion& rotation,
    const TrackingVector& value) noexcept {
    const TrackingQuaternion unit = Normalize(rotation);
    const TrackingVector quaternionVector{unit.x, unit.y, unit.z};
    const TrackingVector twiceCross{
        2.0F * (quaternionVector.y * value.z -
                quaternionVector.z * value.y),
        2.0F * (quaternionVector.z * value.x -
                quaternionVector.x * value.z),
        2.0F * (quaternionVector.x * value.y -
                quaternionVector.y * value.x)};
    return {
        value.x + unit.w * twiceCross.x +
            quaternionVector.y * twiceCross.z -
            quaternionVector.z * twiceCross.y,
        value.y + unit.w * twiceCross.y +
            quaternionVector.z * twiceCross.x -
            quaternionVector.x * twiceCross.z,
        value.z + unit.w * twiceCross.z +
            quaternionVector.x * twiceCross.y -
            quaternionVector.y * twiceCross.x};
}

inline TrackingVector PosePosition(const FearVrPose& pose) noexcept {
    return {pose.px, pose.py, pose.pz};
}

inline TrackingQuaternion PoseRotation(const FearVrPose& pose) noexcept {
    return Normalize({pose.qx, pose.qy, pose.qz, pose.qw});
}

inline bool IsValidPose(const FearVrPose& pose) noexcept {
    const TrackingQuaternion rotation{
        pose.qx, pose.qy, pose.qz, pose.qw};
    const float magnitudeSquared =
        rotation.x * rotation.x + rotation.y * rotation.y +
        rotation.z * rotation.z + rotation.w * rotation.w;
    return IsFinite(PosePosition(pose)) && IsFinite(rotation) &&
           std::isfinite(magnitudeSquared) &&
           magnitudeSquared >= 0.25F && magnitudeSquared <= 4.0F;
}

inline FearVrPose CenterHeadPose(
    const FearVrRenderRequest& request) noexcept {
    const FearVrPose& left = request.eye[FEARVR_EYE_LEFT].pose;
    const FearVrPose& right = request.eye[FEARVR_EYE_RIGHT].pose;
    if (!IsValidPose(left) || !IsValidPose(right)) {
        return {};
    }
    const TrackingQuaternion leftRotation = PoseRotation(left);
    TrackingQuaternion rightRotation = PoseRotation(right);
    if (Dot(leftRotation, rightRotation) < 0.0F) {
        rightRotation = {-rightRotation.x, -rightRotation.y,
                         -rightRotation.z, -rightRotation.w};
    }
    const TrackingQuaternion centerRotation = Normalize({
        leftRotation.x + rightRotation.x,
        leftRotation.y + rightRotation.y,
        leftRotation.z + rightRotation.z,
        leftRotation.w + rightRotation.w});
    FearVrPose center{};
    center.px = (left.px + right.px) * 0.5F;
    center.py = (left.py + right.py) * 0.5F;
    center.pz = (left.pz + right.pz) * 0.5F;
    center.qx = centerRotation.x;
    center.qy = centerRotation.y;
    center.qz = centerRotation.z;
    center.qw = centerRotation.w;
    return center;
}

// OpenXR is right-handed with -Z forward. LithTech uses +X right, +Y up and
// +Z forward with its left-handed cross-product convention.
inline TrackingVector OpenXrToLithTech(
    const TrackingVector& value) noexcept {
    return {value.x, value.y, -value.z};
}

inline TrackingQuaternion OpenXrToLithTech(
    const TrackingQuaternion& value) noexcept {
    const TrackingQuaternion unit = Normalize(value);
    return {-unit.x, -unit.y, unit.z, unit.w};
}

inline TrackingVector ClampMagnitude(
    const TrackingVector& value, float maximum) noexcept {
    const float magnitudeSquared =
        value.x * value.x + value.y * value.y + value.z * value.z;
    if (!IsFinite(value) || !std::isfinite(maximum) || maximum < 0.0F ||
        !std::isfinite(magnitudeSquared)) {
        return {};
    }
    if (magnitudeSquared <= maximum * maximum ||
        magnitudeSquared < 0.000001F) {
        return value;
    }
    const float scale = maximum / std::sqrt(magnitudeSquared);
    return {value.x * scale, value.y * scale, value.z * scale};
}

inline RelativeEyePose TrackedPoseRelativeToRecenter(
    const FearVrPose& recenter,
    const FearVrPose& trackedPose) noexcept {
    if (!IsValidPose(recenter) || !IsValidPose(trackedPose)) {
        return {};
    }

    const TrackingQuaternion inverseRecenter =
        Conjugate(PoseRotation(recenter));
    const TrackingVector relativePosition = Rotate(
        inverseRecenter,
        {trackedPose.px - recenter.px,
         trackedPose.py - recenter.py,
         trackedPose.pz - recenter.pz});
    const TrackingQuaternion relativeRotation = Multiply(
        inverseRecenter, PoseRotation(trackedPose));
    const TrackingVector lithTechPosition =
        OpenXrToLithTech(relativePosition);
    const TrackingQuaternion lithTechRotation =
        OpenXrToLithTech(relativeRotation);
    return {
        lithTechPosition, lithTechRotation,
        IsFinite(lithTechPosition) && IsFinite(lithTechRotation)};
}

inline RelativeEyePose EyePoseRelativeToRecenter(
    const FearVrPose& recenter, const FearVrPose& currentCenter,
    const FearVrPose& eye, bool translationEnabled,
    float maximumTranslationMeters = 0.25F) noexcept {
    if (!IsValidPose(recenter) || !IsValidPose(currentCenter) ||
        !IsValidPose(eye) || !std::isfinite(maximumTranslationMeters) ||
        maximumTranslationMeters < 0.0F) {
        return {};
    }

    const TrackingQuaternion inverseRecenter =
        Conjugate(PoseRotation(recenter));
    TrackingVector centerDelta = Rotate(
        inverseRecenter,
        {currentCenter.px - recenter.px,
         currentCenter.py - recenter.py,
         currentCenter.pz - recenter.pz});
    if (translationEnabled) {
        centerDelta =
            ClampMagnitude(centerDelta, maximumTranslationMeters);
    } else {
        centerDelta = {};
    }
    const TrackingVector eyeOffset = Rotate(
        inverseRecenter,
        {eye.px - currentCenter.px,
         eye.py - currentCenter.py,
         eye.pz - currentCenter.pz});
    const TrackingVector relativePosition{
        centerDelta.x + eyeOffset.x,
        centerDelta.y + eyeOffset.y,
        centerDelta.z + eyeOffset.z};
    const TrackingQuaternion relativeRotation = Multiply(
        inverseRecenter, PoseRotation(eye));
    const TrackingVector lithTechPosition =
        OpenXrToLithTech(relativePosition);
    const TrackingQuaternion lithTechRotation =
        OpenXrToLithTech(relativeRotation);
    return {
        lithTechPosition, lithTechRotation,
        IsFinite(lithTechPosition) && IsFinite(lithTechRotation)};
}

} // namespace fearvr

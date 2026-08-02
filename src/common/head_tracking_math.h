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

struct HeadRelativeMovement {
    float x{0.0F};
    float y{0.0F};
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

// A VR recenter must only choose which horizontal direction is "forward".
// Treating the current pitch and roll as neutral tilts the room with the head:
// returning to a physically level pose then produces the opposite pitch/roll.
// Derive yaw from the projected HMD forward vector and retain the position for
// the existing translation origin. Near a vertical gaze, where forward has no
// useful azimuth, the projected right vector provides a stable fallback.
inline FearVrPose YawOnlyRecenterPose(
    const FearVrPose& pose) noexcept {
    if (!IsValidPose(pose)) {
        return {};
    }

    const TrackingQuaternion rotation = PoseRotation(pose);
    const TrackingVector forward =
        Rotate(rotation, {0.0F, 0.0F, -1.0F});
    const float forwardHorizontalSquared =
        forward.x * forward.x + forward.z * forward.z;

    float yaw = 0.0F;
    if (std::isfinite(forwardHorizontalSquared) &&
        forwardHorizontalSquared > 0.000001F) {
        yaw = std::atan2(-forward.x, -forward.z);
    } else {
        const TrackingVector right =
            Rotate(rotation, {1.0F, 0.0F, 0.0F});
        const float rightHorizontalSquared =
            right.x * right.x + right.z * right.z;
        if (!std::isfinite(rightHorizontalSquared) ||
            rightHorizontalSquared <= 0.000001F) {
            return {};
        }
        yaw = std::atan2(-right.z, right.x);
    }
    if (!std::isfinite(yaw)) {
        return {};
    }

    const float halfYaw = yaw * 0.5F;
    FearVrPose recenter = pose;
    recenter.qx = 0.0F;
    recenter.qy = std::sin(halfYaw);
    recenter.qz = 0.0F;
    recenter.qw = std::cos(halfYaw);
    return recenter;
}

// Rotate a horizontal movement stick from the user's current HMD-facing
// frame into Retail's body-relative strafe/forward frame. Pitch and roll are
// deliberately removed: looking up must not reduce forward speed, and head
// roll must never introduce strafing. The input magnitude is preserved.
inline HeadRelativeMovement RotateMovementByHeadYaw(
    const FearVrPose& recenter, const FearVrPose& currentCenter,
    float moveX, float moveY) noexcept {
    if (!IsValidPose(recenter) || !IsValidPose(currentCenter) ||
        !std::isfinite(moveX) || !std::isfinite(moveY)) {
        return {};
    }
    const FearVrPose currentYaw = YawOnlyRecenterPose(currentCenter);
    if (!IsValidPose(currentYaw)) {
        return {};
    }
    const TrackingQuaternion relativeYaw = Multiply(
        Conjugate(PoseRotation(recenter)), PoseRotation(currentYaw));
    const TrackingVector right =
        Rotate(relativeYaw, {1.0F, 0.0F, 0.0F});
    const TrackingVector forward =
        Rotate(relativeYaw, {0.0F, 0.0F, -1.0F});
    const float x = moveX * right.x + moveY * forward.x;
    const float y = -(moveX * right.z + moveY * forward.z);
    if (!std::isfinite(x) || !std::isfinite(y)) {
        return {};
    }
    return {x, y, true};
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

// Der reine Kopfversatz gegenueber der Recenter-Position, in
// LithTech-Achsen und Metern. Getrennt abrufbar, weil das physische Lehnen
// ihn gegen die Weltgeometrie pruefen muss, bevor er auf die Augen kommt:
// Ohne diese Pruefung schoebe ein Lehnen den Blickpunkt in die Wand hinein.
inline TrackingVector HeadTranslationRelativeToRecenter(
    const FearVrPose& recenter, const FearVrPose& currentCenter,
    float maximumTranslationMeters = 0.25F) noexcept {
    if (!IsValidPose(recenter) || !IsValidPose(currentCenter) ||
        !std::isfinite(maximumTranslationMeters) ||
        maximumTranslationMeters < 0.0F) {
        return {};
    }
    const TrackingVector centerDelta = ClampMagnitude(
        Rotate(
            Conjugate(PoseRotation(recenter)),
            {currentCenter.px - recenter.px,
             currentCenter.py - recenter.py,
             currentCenter.pz - recenter.pz}),
        maximumTranslationMeters);
    return OpenXrToLithTech(centerDelta);
}

// `translationScale` kuerzt den Kopfversatz, ohne die Augenbasis anzutasten:
// 1 laesst ihn ganz zu, 0 haelt den Blickpunkt an der Spielerposition fest.
// Damit begrenzt der Aufrufer das physische Lehnen an Waenden, ohne dass
// dieser Header die Spielwelt kennen muss.
inline RelativeEyePose EyePoseRelativeToRecenter(
    const FearVrPose& recenter, const FearVrPose& currentCenter,
    const FearVrPose& eye, bool translationEnabled,
    float maximumTranslationMeters = 0.25F,
    float translationScale = 1.0F) noexcept {
    if (!IsValidPose(recenter) || !IsValidPose(currentCenter) ||
        !IsValidPose(eye) || !std::isfinite(maximumTranslationMeters) ||
        maximumTranslationMeters < 0.0F ||
        !std::isfinite(translationScale)) {
        return {};
    }
    if (translationScale < 0.0F) {
        translationScale = 0.0F;
    } else if (translationScale > 1.0F) {
        translationScale = 1.0F;
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
        centerDelta = {
            centerDelta.x * translationScale,
            centerDelta.y * translationScale,
            centerDelta.z * translationScale};
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

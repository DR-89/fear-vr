#include <cmath>
#include <cstdlib>
#include <iostream>

#include "head_tracking_math.h"

namespace {

constexpr float kHalfSqrtTwo = 0.70710678F;

bool Near(float actual, float expected, float tolerance = 0.0002F) {
    return std::fabs(actual - expected) <= tolerance;
}

int Fail(const char* message) {
    std::cerr << "test_head_tracking_math: FAIL: " << message << '\n';
    return EXIT_FAILURE;
}

FearVrPose Pose(float px, float py, float pz, float qx, float qy,
                float qz, float qw) {
    return {px, py, pz, qx, qy, qz, qw};
}

FearVrRenderRequest Head(const FearVrPose& center,
                         float halfIpd = 0.032F) {
    FearVrRenderRequest request{};
    request.eye[FEARVR_EYE_LEFT].pose = center;
    request.eye[FEARVR_EYE_RIGHT].pose = center;
    request.eye[FEARVR_EYE_LEFT].pose.px -= halfIpd;
    request.eye[FEARVR_EYE_RIGHT].pose.px += halfIpd;
    return request;
}

} // namespace

int main() {
    const FearVrPose identity = Pose(0.0F, 0.0F, 0.0F,
                                     0.0F, 0.0F, 0.0F, 1.0F);
    const FearVrRenderRequest neutral = Head(identity);
    const FearVrPose neutralCenter = fearvr::CenterHeadPose(neutral);
    if (!fearvr::IsValidPose(neutralCenter) ||
        !Near(neutralCenter.px, 0.0F)) {
        return Fail("neutral eye poses must produce a valid head center");
    }

    const fearvr::RelativeEyePose left =
        fearvr::EyePoseRelativeToRecenter(
            neutralCenter, neutralCenter,
            neutral.eye[FEARVR_EYE_LEFT].pose, false);
    const fearvr::RelativeEyePose right =
        fearvr::EyePoseRelativeToRecenter(
            neutralCenter, neutralCenter,
            neutral.eye[FEARVR_EYE_RIGHT].pose, false);
    if (!left.valid || !right.valid ||
        !Near(left.positionMeters.x, -0.032F) ||
        !Near(right.positionMeters.x, 0.032F)) {
        return Fail("neutral IPD must remain left/right after recenter");
    }

    const fearvr::TrackingQuaternion xrYawLeft{
        0.0F, kHalfSqrtTwo, 0.0F, kHalfSqrtTwo};
    const fearvr::TrackingQuaternion ltYawLeft =
        fearvr::OpenXrToLithTech(xrYawLeft);
    const fearvr::TrackingVector yawForward = fearvr::Rotate(
        ltYawLeft, {0.0F, 0.0F, 1.0F});
    if (!Near(yawForward.x, -1.0F) ||
        !Near(yawForward.z, 0.0F)) {
        return Fail("turning the HMD left must turn LithTech forward left");
    }

    const FearVrPose headLeft = Pose(
        0.0F, 0.0F, 0.0F,
        xrYawLeft.x, xrYawLeft.y, xrYawLeft.z, xrYawLeft.w);
    const fearvr::HeadRelativeMovement forwardWhileLookingLeft =
        fearvr::RotateMovementByHeadYaw(identity, headLeft, 0.0F, 1.0F);
    if (!forwardWhileLookingLeft.valid ||
        !Near(forwardWhileLookingLeft.x, -1.0F) ||
        !Near(forwardWhileLookingLeft.y, 0.0F)) {
        return Fail(
            "head-relative forward must follow horizontal HMD yaw");
    }
    const fearvr::HeadRelativeMovement strafeWhileLookingLeft =
        fearvr::RotateMovementByHeadYaw(identity, headLeft, 1.0F, 0.0F);
    if (!strafeWhileLookingLeft.valid ||
        !Near(strafeWhileLookingLeft.x, 0.0F) ||
        !Near(strafeWhileLookingLeft.y, 1.0F)) {
        return Fail(
            "head-relative strafe must remain perpendicular to forward");
    }

    const fearvr::TrackingQuaternion xrPitchUp{
        kHalfSqrtTwo, 0.0F, 0.0F, kHalfSqrtTwo};
    const fearvr::TrackingQuaternion ltPitchUp =
        fearvr::OpenXrToLithTech(xrPitchUp);
    const fearvr::TrackingVector pitchForward = fearvr::Rotate(
        ltPitchUp, {0.0F, 0.0F, 1.0F});
    if (!Near(pitchForward.y, 1.0F) ||
        !Near(pitchForward.z, 0.0F)) {
        return Fail("looking up must turn LithTech forward upward");
    }

    FearVrPose movedCenter = identity;
    movedCenter.pz = -0.5F;
    FearVrRenderRequest moved = Head(movedCenter);
    const FearVrPose currentCenter = fearvr::CenterHeadPose(moved);
    const fearvr::RelativeEyePose translationOff =
        fearvr::EyePoseRelativeToRecenter(
            neutralCenter, currentCenter,
            moved.eye[FEARVR_EYE_LEFT].pose, false);
    const fearvr::RelativeEyePose translationOn =
        fearvr::EyePoseRelativeToRecenter(
            neutralCenter, currentCenter,
            moved.eye[FEARVR_EYE_LEFT].pose, true,
            fearvr::kPhysicalLeanMaxTranslationMeters);
    if (!Near(translationOff.positionMeters.z, 0.0F) ||
        !Near(translationOn.positionMeters.z, 0.25F)) {
        return Fail("forward translation must be opt-in and clamped to 25cm");
    }

    const fearvr::RelativeEyePose roomScaleTranslation =
        fearvr::EyePoseRelativeToRecenter(
            neutralCenter, currentCenter,
            moved.eye[FEARVR_EYE_LEFT].pose, true,
            fearvr::kRoomScaleMaxTranslationMeters);
    if (!Near(roomScaleTranslation.positionMeters.z, 0.5F)) {
        return Fail("room-scale translation must preserve play-area movement");
    }

    FearVrPose farCenter = identity;
    farCenter.pz = -3.0F;
    const FearVrRenderRequest far = Head(farCenter);
    const fearvr::RelativeEyePose boundedRoomScale =
        fearvr::EyePoseRelativeToRecenter(
            neutralCenter, fearvr::CenterHeadPose(far),
            far.eye[FEARVR_EYE_LEFT].pose, true,
            fearvr::kRoomScaleMaxTranslationMeters);
    if (!Near(boundedRoomScale.positionMeters.z, 2.0F)) {
        return Fail("room-scale translation must reject implausible pose jumps");
    }

    const FearVrPose movedRecenter =
        fearvr::YawOnlyRecenterPose(currentCenter);
    const fearvr::RelativeEyePose recenteredPosition =
        fearvr::EyePoseRelativeToRecenter(
            movedRecenter, currentCenter,
            moved.eye[FEARVR_EYE_LEFT].pose, true,
            fearvr::kRoomScaleMaxTranslationMeters);
    if (!Near(recenteredPosition.positionMeters.x, -0.032F) ||
        !Near(recenteredPosition.positionMeters.z, 0.0F)) {
        return Fail("recentering must zero translation while preserving IPD");
    }

    const FearVrPose recenteredYaw = Pose(
        0.0F, 0.0F, 0.0F,
        0.0F, kHalfSqrtTwo, 0.0F, kHalfSqrtTwo);
    const FearVrRenderRequest sameYaw = Head(recenteredYaw);
    const FearVrPose sameYawCenter = fearvr::CenterHeadPose(sameYaw);
    const fearvr::RelativeEyePose recentered =
        fearvr::EyePoseRelativeToRecenter(
            recenteredYaw, sameYawCenter,
            sameYaw.eye[FEARVR_EYE_LEFT].pose, false);
    if (!recentered.valid ||
        !Near(recentered.rotation.x, 0.0F) ||
        !Near(recentered.rotation.y, 0.0F) ||
        !Near(recentered.rotation.z, 0.0F) ||
        !Near(recentered.rotation.w, 1.0F)) {
        return Fail("the recenter orientation must become neutral");
    }

    const FearVrPose pitchedDown = Pose(
        0.0F, 0.0F, 0.0F,
        -0.38268343F, 0.0F, 0.0F, 0.92387953F);
    const fearvr::TrackingQuaternion yawWithPitchRotation =
        fearvr::Multiply(
            {0.0F, kHalfSqrtTwo, 0.0F, kHalfSqrtTwo},
            {-0.38268343F, 0.0F, 0.0F, 0.92387953F});
    const FearVrPose yawWithPitch = Pose(
        0.0F, 0.0F, 0.0F,
        yawWithPitchRotation.x, yawWithPitchRotation.y,
        yawWithPitchRotation.z, yawWithPitchRotation.w);
    const FearVrPose mixedRecenter =
        fearvr::YawOnlyRecenterPose(yawWithPitch);
    if (!fearvr::IsValidPose(mixedRecenter) ||
        !Near(mixedRecenter.qx, 0.0F) ||
        !Near(mixedRecenter.qy, kHalfSqrtTwo) ||
        !Near(mixedRecenter.qz, 0.0F) ||
        !Near(mixedRecenter.qw, kHalfSqrtTwo)) {
        return Fail(
            "yaw-only recenter must isolate yaw from simultaneous pitch");
    }

    const FearVrPose pitchedRecenter =
        fearvr::YawOnlyRecenterPose(pitchedDown);
    const fearvr::RelativeEyePose pitchAtReset =
        fearvr::TrackedPoseRelativeToRecenter(
            pitchedRecenter, pitchedDown);
    const fearvr::RelativeEyePose levelAfterPitchReset =
        fearvr::TrackedPoseRelativeToRecenter(
            pitchedRecenter, identity);
    const fearvr::TrackingVector pitchAtResetForward = fearvr::Rotate(
        pitchAtReset.rotation, {0.0F, 0.0F, 1.0F});
    const fearvr::TrackingVector levelAfterPitchResetForward = fearvr::Rotate(
        levelAfterPitchReset.rotation, {0.0F, 0.0F, 1.0F});
    if (!pitchAtReset.valid || !levelAfterPitchReset.valid ||
        !(pitchAtResetForward.y < -0.7F) ||
        !Near(levelAfterPitchResetForward.y, 0.0F) ||
        !Near(levelAfterPitchResetForward.z, 1.0F)) {
        return Fail(
            "yaw-only recenter must preserve physical pitch instead of "
            "inverting it after the head returns level");
    }

    const FearVrPose rolled = Pose(
        0.0F, 0.0F, 0.0F,
        0.0F, 0.0F, 0.38268343F, 0.92387953F);
    const FearVrPose rolledRecenter =
        fearvr::YawOnlyRecenterPose(rolled);
    if (!fearvr::IsValidPose(rolledRecenter) ||
        !Near(rolledRecenter.qx, 0.0F) ||
        !Near(rolledRecenter.qy, 0.0F) ||
        !Near(rolledRecenter.qz, 0.0F) ||
        !Near(rolledRecenter.qw, 1.0F)) {
        return Fail(
            "yaw-only recenter must not absorb physical head roll");
    }

    const FearVrPose rightHandAim = Pose(
        0.25F, -0.15F, -0.45F,
        0.0F, kHalfSqrtTwo, 0.0F, kHalfSqrtTwo);
    const fearvr::RelativeEyePose hand =
        fearvr::TrackedPoseRelativeToRecenter(identity, rightHandAim);
    const fearvr::TrackingVector handForward = fearvr::Rotate(
        hand.rotation, {0.0F, 0.0F, 1.0F});
    if (!hand.valid ||
        !Near(hand.positionMeters.x, 0.25F) ||
        !Near(hand.positionMeters.y, -0.15F) ||
        !Near(hand.positionMeters.z, 0.45F) ||
        !Near(handForward.x, -1.0F) ||
        !Near(handForward.z, 0.0F)) {
        return Fail(
            "controller aim pose must preserve position and pointing axes");
    }

    if (!Near(fearvr::SanitizeEyeHeightMeters(-1.0F), 0.0F) ||
        !Near(fearvr::SanitizeEyeHeightMeters(0.2F), 0.5F) ||
        !Near(fearvr::SanitizeEyeHeightMeters(3.0F), 2.4F)) {
        return Fail("eye-height settings must preserve auto and clamp bad values");
    }
    if (!Near(
            fearvr::CalibratedVerticalOffsetMeters(1.20F, 1.70F),
            -0.50F) ||
        !Near(
            fearvr::CalibratedVerticalOffsetMeters(0.05F, 1.70F),
            -1.45F) ||
        !Near(
            fearvr::CalibratedVerticalOffsetMeters(2.60F, 1.70F),
            0.50F)) {
        return Fail(
            "floor-relative height must allow crouching while retaining clearance");
    }

    FearVrPose floorCenter = identity;
    floorCenter.py = 1.20F;
    floorCenter.pz = -0.10F;
    FearVrRenderRequest floorHead = Head(floorCenter);
    const float crouchOffset =
        fearvr::CalibratedVerticalOffsetMeters(1.20F, 1.70F);
    const fearvr::TrackingVector independentHeight =
        fearvr::HeadTranslationRelativeToRecenter(
            identity, floorCenter,
            fearvr::kPhysicalLeanMaxTranslationMeters,
            true, crouchOffset);
    const fearvr::RelativeEyePose independentHeightEye =
        fearvr::EyePoseRelativeToRecenter(
            identity, floorCenter,
            floorHead.eye[FEARVR_EYE_LEFT].pose, true,
            fearvr::kPhysicalLeanMaxTranslationMeters, 0.5F,
            true, crouchOffset);
    if (!Near(independentHeight.y, crouchOffset) ||
        !Near(independentHeight.z, 0.10F) ||
        !independentHeightEye.valid ||
        !Near(independentHeightEye.positionMeters.y, crouchOffset) ||
        !Near(independentHeightEye.positionMeters.z, 0.05F)) {
        return Fail(
            "calibrated height must be independent of lean caps and wall scale");
    }

    FearVrPose invalid = identity;
    invalid.qw = NAN;
    if (fearvr::EyePoseRelativeToRecenter(
            invalid, neutralCenter,
            neutral.eye[FEARVR_EYE_LEFT].pose, false).valid) {
        return Fail("non-finite recenter poses must fail closed");
    }

    std::cout << "test_head_tracking_math: OK\n";
    return EXIT_SUCCESS;
}

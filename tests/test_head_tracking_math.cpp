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
            moved.eye[FEARVR_EYE_LEFT].pose, true);
    if (!Near(translationOff.positionMeters.z, 0.0F) ||
        !Near(translationOn.positionMeters.z, 0.25F)) {
        return Fail("forward translation must be opt-in and clamped to 25cm");
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

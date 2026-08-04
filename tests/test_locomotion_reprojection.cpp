#include <cmath>
#include <cstdlib>
#include <iostream>

#include "locomotion_reprojection.h"

namespace {

constexpr float kHalfSqrtTwo = 0.70710678F;

bool Near(float actual, float expected, float tolerance = 0.0002F) {
    return std::fabs(actual - expected) <= tolerance;
}

int Fail(const char* message) {
    std::cerr << "test_locomotion_reprojection: FAIL: "
              << message << '\n';
    return EXIT_FAILURE;
}

FearVrGameCameraSample Camera(
    std::uint64_t frameId, float x, float y, float z,
    float qx = 0.0F, float qy = 0.0F, float qz = 0.0F,
    float qw = 1.0F) {
    FearVrGameCameraSample sample{};
    sample.frameId = frameId;
    sample.pose = {x, y, z, qx, qy, qz, qw};
    sample.flags = FEARVR_GCF_VALID;
    return sample;
}

} // namespace

int main() {
    const FearVrGameCameraSample origin =
        Camera(10, 0.0F, 1.7F, 0.0F);

    const auto forward = fearvr::ComputeLocomotionReprojection(
        origin, Camera(13, 0.0F, 1.7F, 0.12F));
    if (!forward.valid || forward.ageFrames != 3 ||
        !Near(forward.distanceMeters, 0.12F) ||
        !Near(forward.sourcePoseOffsetMeters.x, 0.0F) ||
        !Near(forward.sourcePoseOffsetMeters.y, 0.0F) ||
        !Near(forward.sourcePoseOffsetMeters.z, 0.12F)) {
        return Fail(
            "forward LithTech movement must move the old XR source back");
    }

    const auto right = fearvr::ComputeLocomotionReprojection(
        origin, Camera(12, 0.08F, 1.7F, 0.0F));
    if (!right.valid ||
        !Near(right.sourcePoseOffsetMeters.x, -0.08F) ||
        !Near(right.sourcePoseOffsetMeters.z, 0.0F)) {
        return Fail(
            "rightward movement must move the old XR source left");
    }

    const auto vertical = fearvr::ComputeLocomotionReprojection(
        origin, Camera(11, 0.0F, 2.1F, 0.0F));
    if (!vertical.valid || !Near(vertical.distanceMeters, 0.0F) ||
        !Near(vertical.sourcePoseOffsetMeters.y, 0.0F)) {
        return Fail("head-bob and stairs must not shift the XR layer");
    }

    const FearVrGameCameraSample turned =
        Camera(20, 0.0F, 1.7F, 0.0F,
               0.0F, kHalfSqrtTwo, 0.0F, kHalfSqrtTwo);
    const auto turnedForward =
        fearvr::ComputeLocomotionReprojection(
            turned,
            Camera(22, 0.1F, 1.7F, 0.0F,
                   0.0F, kHalfSqrtTwo, 0.0F, kHalfSqrtTwo));
    if (!turnedForward.valid ||
        !Near(turnedForward.sourcePoseOffsetMeters.x, 0.0F) ||
        !Near(turnedForward.sourcePoseOffsetMeters.z, 0.1F)) {
        return Fail(
            "world movement must be transformed through the old base yaw");
    }

    const auto teleport = fearvr::ComputeLocomotionReprojection(
        origin, Camera(11, 0.0F, 1.7F, 0.75F));
    if (teleport.valid) {
        return Fail("teleports must not be translated into compositor motion");
    }

    const auto tooOld = fearvr::ComputeLocomotionReprojection(
        origin, Camera(30, 0.0F, 1.7F, 0.1F));
    if (tooOld.valid) {
        return Fail("very old camera samples must fail closed");
    }

    const auto snapTurn = fearvr::ComputeLocomotionReprojection(
        origin,
        Camera(11, 0.0F, 1.7F, 0.1F,
               0.0F, kHalfSqrtTwo, 0.0F, kHalfSqrtTwo));
    if (snapTurn.valid) {
        return Fail(
            "translation-only reprojection must reject a simultaneous turn");
    }

    std::cout << "test_locomotion_reprojection: OK\n";
    return EXIT_SUCCESS;
}

#include <cmath>
#include <cstdlib>
#include <iostream>

#include "stereo_math.h"

namespace {

bool Near(float actual, float expected, float tolerance = 0.0001F) {
    return std::fabs(actual - expected) <= tolerance;
}

int Fail(const char* message) {
    std::cerr << "test_stereo_math: FAIL: " << message << '\n';
    return EXIT_FAILURE;
}

} // namespace

int main() {
    FearVrRenderRequest request{};
    request.eye[FEARVR_EYE_LEFT].pose.px = -0.032F;
    request.eye[FEARVR_EYE_RIGHT].pose.px = 0.032F;

    if (!Near(fearvr::InterpupillaryDistanceMeters(request), 0.064F)) {
        return Fail("IPD must be taken from the two OpenXR eye poses");
    }
    if (!Near(fearvr::EyeOffsetGameUnits(
                  request, FEARVR_EYE_LEFT), -3.2F) ||
        !Near(fearvr::EyeOffsetGameUnits(
                  request, FEARVR_EYE_RIGHT), 3.2F)) {
        return Fail("left/right eye offsets are swapped or scaled incorrectly");
    }

    request.eye[FEARVR_EYE_LEFT].pose.px = 1.968F;
    request.eye[FEARVR_EYE_RIGHT].pose.px = 2.032F;
    if (!Near(fearvr::EyeOffsetGameUnits(
                  request, FEARVR_EYE_LEFT), -3.2F) ||
        !Near(fearvr::EyeOffsetGameUnits(
                  request, FEARVR_EYE_RIGHT), 3.2F)) {
        return Fail("absolute tracking-space translation must cancel");
    }

    request.eye[FEARVR_EYE_LEFT].pose.px = 0.0F;
    request.eye[FEARVR_EYE_RIGHT].pose.px = 0.0F;
    request.eye[FEARVR_EYE_LEFT].pose.pz = -0.032F;
    request.eye[FEARVR_EYE_RIGHT].pose.pz = 0.032F;
    if (!Near(fearvr::InterpupillaryDistanceMeters(request), 0.064F) ||
        !Near(fearvr::EyeOffsetGameUnits(
                  request, FEARVR_EYE_LEFT), -3.2F) ||
        !Near(fearvr::EyeOffsetGameUnits(
                  request, FEARVR_EYE_RIGHT), 3.2F)) {
        return Fail("rotated OpenXR eye baseline must preserve the IPD");
    }

    const FearVrFov left{-0.90F, 0.82F, 0.88F, -0.84F};
    const FearVrFov right{-0.81F, 0.91F, 0.86F, -0.83F};
    const fearvr::SymmetricFov symmetric =
        fearvr::SharedSymmetricFov(left, right);
    if (!symmetric.valid ||
        !Near(symmetric.halfHorizontal, 0.81F) ||
        !Near(symmetric.halfVertical, 0.83F)) {
        return Fail("shared conservative symmetric FOV is incorrect");
    }
    const FearVrFov protocol = fearvr::ToProtocolFov(symmetric);
    if (!Near(protocol.angleLeft, -0.81F) ||
        !Near(protocol.angleRight, 0.81F) ||
        !Near(protocol.angleUp, 0.83F) ||
        !Near(protocol.angleDown, -0.83F)) {
        return Fail("symmetric FOV conversion is incorrect");
    }

    const FearVrFov invalid{};
    if (fearvr::SharedSymmetricFov(invalid, right).valid) {
        return Fail("invalid FOV must be rejected");
    }

    request.eye[FEARVR_EYE_LEFT].pose.px = NAN;
    if (!Near(fearvr::EyeOffsetGameUnits(
                  request, FEARVR_EYE_LEFT), 0.0F)) {
        return Fail("non-finite eye poses must fail closed");
    }

    std::cout << "test_stereo_math: OK\n";
    return EXIT_SUCCESS;
}

#include <cmath>
#include <cstdlib>
#include <iostream>

#include "stereo_math.h"
#include "vertical_camera_height.h"

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
    const fearvr::SymmetricFov scaled =
        fearvr::ScaleSymmetricFov(symmetric, 1.2F);
    if (!scaled.valid ||
        !Near(scaled.halfHorizontal,
              std::atan(std::tan(0.81F) * 1.2F)) ||
        !Near(scaled.halfVertical,
              std::atan(std::tan(0.83F) * 1.2F))) {
        return Fail("FOV scale must multiply projection tangents");
    }
    if (fearvr::ScaleSymmetricFov(symmetric, NAN).valid ||
        fearvr::ScaleSymmetricFov(
            fearvr::SymmetricFov{}, 1.2F).valid) {
        return Fail("invalid FOV scale inputs must fail closed");
    }

    if (fearvr::SharedSymmetricFov(invalid, right).valid) {
        return Fail("invalid FOV must be rejected");
    }

    request.eye[FEARVR_EYE_LEFT].pose.px = NAN;
    if (!Near(fearvr::EyeOffsetGameUnits(
                  request, FEARVR_EYE_LEFT), 0.0F)) {
        return Fail("non-finite eye poses must fail closed");
    }

    {
        fearvr::VerticalCameraHeightState height;
        fearvr::UpdateVerticalCameraHeight(
            height, 100.0F, 100.0F, false, false, true);
        const auto upward = fearvr::UpdateVerticalCameraHeight(
            height, 110.0F, 104.0F, false, false, true);
        if (upward.bypassActive ||
            !Near(upward.visualHeight, 104.0F)) {
            return Fail("upward stair steps must keep Retail smoothing");
        }
    }
    {
        fearvr::VerticalCameraHeightState height;
        fearvr::UpdateVerticalCameraHeight(
            height, 100.0F, 100.0F, false, true, true);
        const auto duck = fearvr::UpdateVerticalCameraHeight(
            height, 90.0F, 96.0F, false, true, true);
        if (duck.bypassActive || !Near(duck.visualHeight, 96.0F)) {
            return Fail("ducking must keep Retail smoothing");
        }
    }
    {
        fearvr::VerticalCameraHeightState height;
        fearvr::UpdateVerticalCameraHeight(
            height, 100.0F, 100.0F, false, false, true);
        const auto downward = fearvr::UpdateVerticalCameraHeight(
            height, 90.0F, 97.0F, false, false, true);
        if (!downward.bypassActive ||
            !Near(downward.visualHeight, 90.0F)) {
            return Fail("downward stair steps must use raw height");
        }
        const auto waiting = fearvr::UpdateVerticalCameraHeight(
            height, 90.0F, 92.0F, false, false, true);
        if (!waiting.bypassActive ||
            !Near(waiting.visualHeight, 90.0F)) {
            return Fail("downward bypass must wait for Retail catch-up");
        }
        const auto caughtUp = fearvr::UpdateVerticalCameraHeight(
            height, 90.0F, 90.1F, false, false, true);
        if (caughtUp.bypassActive ||
            !Near(caughtUp.visualHeight, 90.1F)) {
            return Fail("caught-up height must return to Retail seamlessly");
        }
    }
    {
        fearvr::VerticalCameraHeightState height;
        fearvr::UpdateVerticalCameraHeight(
            height, 100.0F, 100.0F, false, false, true);
        const auto airborne = fearvr::UpdateVerticalCameraHeight(
            height, 120.0F, 110.0F, true, false, true);
        if (!airborne.bypassActive ||
            !Near(airborne.visualHeight, 120.0F)) {
            return Fail("airborne camera must ignore an old smoothing tail");
        }
    }
    {
        fearvr::VerticalCameraHeightState height;
        fearvr::UpdateVerticalCameraHeight(
            height, 100.0F, 100.0F, false, false, true);
        const auto guarded = fearvr::UpdateVerticalCameraHeight(
            height, 50.0F, 90.0F, false, false, true);
        if (guarded.bypassActive ||
            !Near(guarded.visualHeight, 90.0F)) {
            return Fail("large collision correction must keep final height");
        }
    }

    std::cout << "test_stereo_math: OK\n";
    return EXIT_SUCCESS;
}

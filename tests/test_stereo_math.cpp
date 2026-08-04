#include <cmath>
#include <cstdlib>
#include <iostream>

#include "camera_collision.h"
#include "physical_duck.h"
#include "stereo_math.h"
#include "vertical_camera_height.h"
#include "two_handed_jump_camera.h"
#include "wrist_hud_visibility.h"

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
    if (!fearvr::UpdatePhysicalDuck(
            true, true, 1.43F, 1.70F, false) ||
        !fearvr::UpdatePhysicalDuck(
            true, true, 1.51F, 1.70F, true) ||
        fearvr::UpdatePhysicalDuck(
            true, true, 1.53F, 1.70F, true) ||
        fearvr::UpdatePhysicalDuck(
            false, true, 1.40F, 1.70F, true)) {
        return Fail("physical duck hysteresis is incorrect");
    }

    if (!fearvr::IsLookingAtWrist(0.88F) ||
        !fearvr::IsLookingAtWrist(0.84F, true) ||
        fearvr::IsLookingAtWrist(0.87F) ||
        fearvr::IsLookingAtWrist(0.83F, true) ||
        fearvr::IsLookingAtWrist(NAN) ||
        !fearvr::IsWristBackTilted(0.72F) ||
        !fearvr::IsWristBackTilted(0.58F, true) ||
        fearvr::IsWristBackTilted(0.71F) ||
        fearvr::IsWristBackTilted(0.57F, true) ||
        fearvr::IsWristBackTilted(NAN)) {
        return Fail(
            "wrist HUD must require gaze plus a pronounced watch pose");
    }
    if (!fearvr::IsDualPistolWristReadingPose(
            0.20F, 0.80F, 0.0F) ||
        !fearvr::IsDualPistolWristReadingPose(
            0.60F, 0.58F, 0.54F, true) ||
        fearvr::IsDualPistolWristReadingPose(
            0.75F, 0.80F, 0.0F) ||
        fearvr::IsDualPistolWristReadingPose(
            -0.75F, 0.80F, 0.0F) ||
        fearvr::IsDualPistolWristReadingPose(
            0.20F, 0.71F, 0.0F) ||
        fearvr::IsDualPistolWristReadingPose(
            0.20F, 0.80F, 0.55F)) {
        return Fail(
            "dual-pistol HUD must require a deliberate wrist-reading pose");
    }
    std::uint64_t wristCandidateSince = 0;
    if (fearvr::UpdateDualPistolWristDwell(
            true, 1000, wristCandidateSince) ||
        wristCandidateSince != 1000 ||
        fearvr::UpdateDualPistolWristDwell(
            true,
            1000 + fearvr::kDualPistolWristDwellMilliseconds - 1,
            wristCandidateSince) ||
        !fearvr::UpdateDualPistolWristDwell(
            true,
            1000 + fearvr::kDualPistolWristDwellMilliseconds,
            wristCandidateSince) ||
        fearvr::UpdateDualPistolWristDwell(
            false, 1200, wristCandidateSince) ||
        wristCandidateSince != 0) {
        return Fail(
            "dual-pistol wrist-reading dwell timing is incorrect");
    }

    if (fearvr::IsCharacterCollisionObject(0) ||
        !fearvr::IsCharacterCollisionObject(
            fearvr::kCharacterUserFlag) ||
        !fearvr::IsCharacterCollisionObject(
            fearvr::kCharacterHitBoxUserFlag) ||
        !fearvr::IsCharacterCollisionObject(
            fearvr::kCharacterUserFlag |
            fearvr::kCharacterHitBoxUserFlag)) {
        return Fail(
            "VR camera character collision filtering is incorrect");
    }

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
        fearvr::TwoHandedJumpCameraState jump;
        const auto grounded = fearvr::UpdateTwoHandedJumpCamera(
            jump, 20.0F, true, 100.0F,
            true, false, false, false, true);
        if (grounded.anchorActive ||
            !jump.groundedEyeOffsetValid ||
            !Near(jump.groundedEyeOffset, 80.0F)) {
            return Fail(
                "grounded frame must capture eye-to-body jump anchor");
        }
        const auto airborne = fearvr::UpdateTwoHandedJumpCamera(
            jump, 35.0F, true, 130.0F,
            false, true, false, true, true);
        if (!airborne.anchorActive ||
            !Near(airborne.visualHeight, 115.0F)) {
            return Fail(
                "two-handed jump must follow physical body height only");
        }
        const auto firingOnGround = fearvr::UpdateTwoHandedJumpCamera(
            jump, 20.0F, true, 112.0F,
            true, false, false, true, true);
        if (!firingOnGround.anchorActive ||
            !Near(firingOnGround.visualHeight, 100.0F)) {
            return Fail(
                "two-handed firing and running must ignore camera socket lift");
        }
        const auto ducking = fearvr::UpdateTwoHandedJumpCamera(
            jump, 20.0F, true, 72.0F,
            true, false, true, true, true);
        if (ducking.anchorActive ||
            !Near(ducking.visualHeight, 72.0F)) {
            return Fail(
                "crouch height must retain Retail camera correction");
        }
        const auto oneHanded = fearvr::UpdateTwoHandedJumpCamera(
            jump, 35.0F, true, 130.0F,
            false, true, false, false, true);
        if (oneHanded.anchorActive ||
            !Near(oneHanded.visualHeight, 130.0F)) {
            return Fail(
                "ordinary jump must retain Retail camera height");
        }
        const auto ceiling = fearvr::UpdateTwoHandedJumpCamera(
            jump, 75.0F, true, 110.0F,
            false, true, false, true, true);
        if (ceiling.anchorActive || !ceiling.collisionGuarded ||
            !Near(ceiling.visualHeight, 110.0F)) {
            return Fail(
                "large ceiling correction must override jump anchor");
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
        const auto heldWeaponJump = fearvr::UpdateVerticalCameraHeight(
            height, 122.0F, 109.0F, true, false, true, true);
        if (heldWeaponJump.bypassActive ||
            !Near(heldWeaponJump.visualHeight, 109.0F)) {
            return Fail(
                "two-handed airborne animation must keep final camera height");
        }
        const auto landing = fearvr::UpdateVerticalCameraHeight(
            height, 105.0F, 107.0F, false, false, true);
        if (landing.bypassActive ||
            !Near(landing.visualHeight, 107.0F)) {
            return Fail(
                "stabilized jump must not become a downward-step bypass");
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

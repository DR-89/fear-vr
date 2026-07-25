#include <iostream>

#include "stereo_hud_math.h"

namespace {

bool Expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
    }
    return condition;
}

} // namespace

int main() {
    bool ok = true;
    ok &= Expect(
        !fearvr::IsPostWorldPixel(0xff123456u, 0xff123456u),
        "identical world pixels must stay stereo");
    ok &= Expect(
        !fearvr::IsPostWorldPixel(0x00123456u, 0xff123456u),
        "alpha-only differences must be ignored");
    ok &= Expect(
        !fearvr::IsPostWorldPixel(0xff123658u, 0xff123456u),
        "differences at the threshold must be ignored");
    ok &= Expect(
        fearvr::IsPostWorldPixel(0xff123756u, 0xff123456u),
        "visible post-world colour differences must be detected");
    ok &= Expect(
        fearvr::IsSafePostWorldCoverage(1, 100),
        "sparse HUD coverage must be accepted");
    ok &= Expect(
        fearvr::IsSafePostWorldCoverage(65, 100),
        "the coverage boundary must be accepted");
    ok &= Expect(
        !fearvr::IsSafePostWorldCoverage(66, 100),
        "full-screen-like coverage must be rejected");
    ok &= Expect(
        !fearvr::IsSafePostWorldCoverage(0, 100),
        "an empty delta must not enter the compositor");
    ok &= Expect(
        !fearvr::IsFlatPanelCoverage(79, 100),
        "a full-screen gameplay effect must keep the stereo world");
    ok &= Expect(
        fearvr::IsFlatPanelCoverage(82, 100),
        "the observed menu coverage boundary must select the flat panel");
    ok &= Expect(
        fearvr::IsFlatPanelCoverage(95, 100),
        "a full-screen menu delta must select the flat panel");
    ok &= Expect(
        !fearvr::IsFlatPanelCoverage(2, 100),
        "a sparse HUD must not select the flat panel");
    ok &= Expect(
        fearvr::StereoHudSourceRow(300, 768) == 300,
        "the upper half and crosshair region must stay in place");
    ok &= Expect(
        fearvr::StereoHudSourceRow(450, 768) == 450,
        "the complete reticle region must stay in place");
    ok &= Expect(
        fearvr::StereoHudSourceRow(500, 768) == 596,
        "the lower HUD must move up by one eighth");
    ok &= Expect(
        fearvr::StereoHudSourceRow(700, 768) == 768,
        "rows beyond the shifted source must be clipped");
    ok &= Expect(
        fearvr::StereoHudSourceColumn(132, 600, 1024, 768) == 100,
        "the lower-left HUD must move slightly right");
    ok &= Expect(
        fearvr::StereoHudSourceColumn(200, 300, 1024, 768) == 72,
        "the left-side weapon HUD must move substantially inward");
    ok &= Expect(
        fearvr::StereoHudSourceColumn(748, 400, 1024, 768) == 850,
        "the right-side HUD must move toward the reticle");
    ok &= Expect(
        fearvr::StereoHudSourceColumn(512, 384, 1024, 768) == 512,
        "the reticle column must remain unchanged");
    return ok ? 0 : 1;
}

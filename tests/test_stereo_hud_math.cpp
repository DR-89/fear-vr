#include <cstdint>
#include <iostream>

#include "stereo_hud_math.h"
#include "vr_menu_scroll.h"
#include "vr_render_resolution.h"

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
        fearvr::IsSafePostWorldCoverage(3, 100),
        "the sparse HUD coverage boundary must be accepted");
    ok &= Expect(
        !fearvr::IsSafePostWorldCoverage(4, 100),
        "fullscreen-effect coverage must be rejected");
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
        fearvr::StereoHudSourceRow(384, 768) == 384,
        "the image centre must remain unchanged");
    ok &= Expect(
        fearvr::StereoHudSourceColumn(512, 384, 1024, 768) == 512,
        "the reticle column must remain unchanged");
    ok &= Expect(
        fearvr::StereoHudSourceRow(500, 768) == 500,
        "post-world text rows must retain exact source pixels");
    ok &= Expect(
        fearvr::StereoHudSourceColumn(200, 300, 1024, 768) == 200,
        "post-world text columns must retain exact source pixels");
    ok &= Expect(
        fearvr::StereoHudSourceColumn(748, 400, 1024, 768) == 748,
        "right-side text must retain exact source pixels");
    ok &= Expect(
        fearvr::StereoHudSourceRow(20, 768) == 20,
        "top rows must remain visible without shrink clipping");

    // Der eigentliche Fehler der frueheren Zonenverschiebung: Ein HUD-Element,
    // das eine Zonengrenze kreuzte, wurde zerschnitten. Eine stetige und
    // monotone Abbildung kann das grundsaetzlich nicht — benachbarte
    // Ausgabespalten duerfen in der Quelle nie auseinanderspringen.
    std::uint32_t previous = 0;
    bool contiguous = true;
    bool monotonic = true;
    for (std::uint32_t column = 1; column < 1024; ++column) {
        const std::uint32_t source =
            fearvr::StereoHudSourceColumn(column, 384, 1024, 768);
        if (source >= 1024) {
            continue;
        }
        if (previous != 0) {
            if (source < previous) {
                monotonic = false;
            }
            if (source - previous > 2) {
                contiguous = false;
            }
        }
        previous = source;
    }
    ok &= Expect(
        monotonic, "the column mapping must never run backwards");
    ok &= Expect(
        contiguous,
        "neighbouring columns must stay neighbours, so no HUD element is "
        "torn apart");

    const fearvr::VrRenderResolution desktop{2560, 1440};
    const fearvr::VrRenderResolution none{};
    fearvr::VrRenderResolution resolved =
        fearvr::ResolveVrRenderResolution(
            {640, 480}, none, desktop);
    ok &= Expect(
        resolved.width == 2560 && resolved.height == 1440,
        "a first-start VGA fallback must use the current desktop mode");
    resolved = fearvr::ResolveVrRenderResolution(
        {640, 480}, {1920, 1080}, desktop);
    ok &= Expect(
        resolved.width == 1920 && resolved.height == 1080,
        "a temporary VGA reset must reuse the last verified VR mode");
    resolved = fearvr::ResolveVrRenderResolution(
        {1600, 900}, {1920, 1080}, desktop);
    ok &= Expect(
        resolved.width == 1600 && resolved.height == 900,
        "every user-selected HD mode must remain untouched");
    resolved = fearvr::ResolveVrRenderResolution(
        {640, 480}, none, {800, 600});
    ok &= Expect(
        resolved.width == 640 && resolved.height == 480,
        "an unusable desktop mode must not invent a fixed resolution");
    ok &= Expect(
        fearvr::VrMenuScrollStart(0, 0, 16, 11) == 0,
        "the first VR menu row must keep the viewport at the top");
    ok &= Expect(
        fearvr::VrMenuScrollStart(0, 10, 16, 11) == 0,
        "the last row fitting in the viewport must remain visible");
    ok &= Expect(
        fearvr::VrMenuScrollStart(0, 11, 16, 11) == 1,
        "selecting row twelve must scroll the viewport by one row");
    ok &= Expect(
        fearvr::VrMenuScrollStart(1, 15, 16, 11) == 5,
        "the last VR menu row must expose the final five entries");
    ok &= Expect(
        fearvr::VrMenuScrollStart(5, 4, 16, 11) == 4,
        "moving upward must reveal the selected preceding row");
    ok &= Expect(
        fearvr::VrMenuScrollStart(4, 0, 16, 11) == 0,
        "returning to the first row must restore the top viewport");
    ok &= Expect(
        fearvr::VrMenuScrollStart(5, 7, 8, 11) == 0,
        "a menu shorter than the viewport must never scroll");
    return ok ? 0 : 1;
}

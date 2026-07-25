#include <cstdint>
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
        fearvr::IsSafePostWorldCoverage(20, 100),
        "the sparse HUD coverage boundary must be accepted");
    ok &= Expect(
        !fearvr::IsSafePostWorldCoverage(21, 100),
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
        fearvr::StereoHudSourceRow(500, 768) == 529,
        "the lower HUD must move up toward the centre");
    ok &= Expect(
        fearvr::StereoHudSourceColumn(200, 300, 1024, 768) == 122,
        "the left-side HUD must move inward");
    ok &= Expect(
        fearvr::StereoHudSourceColumn(748, 400, 1024, 768) == 807,
        "the right-side HUD must move inward");
    ok &= Expect(
        fearvr::StereoHudSourceRow(20, 768) == 768,
        "rows whose source lies outside the image must be clipped");

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
    return ok ? 0 : 1;
}

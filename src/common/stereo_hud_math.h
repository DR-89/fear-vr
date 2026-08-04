#pragma once

#include <cstdint>

namespace fearvr {

inline bool IsPostWorldPixel(std::uint32_t presented,
                             std::uint32_t rightWorld,
                             std::uint8_t threshold = 2) noexcept {
    const auto channelChanged = [threshold](std::uint32_t left,
                                            std::uint32_t right,
                                            unsigned shift) noexcept {
        const int a = static_cast<int>((left >> shift) & 0xffu);
        const int b = static_cast<int>((right >> shift) & 0xffu);
        return (a > b ? a - b : b - a) >
               static_cast<int>(threshold);
    };
    return channelChanged(presented, rightWorld, 0) ||
           channelChanged(presented, rightWorld, 8) ||
           channelChanged(presented, rightWorld, 16);
}

inline bool IsSafePostWorldCoverage(std::uint64_t changedPixels,
                                     std::uint64_t totalPixels) noexcept {
    // Sparse HUD elements can be lifted into stereo. Large deltas are usually
    // fullscreen effects (for example Slow-Mo) and must stay out of the HUD
    // compositor, otherwise their mono filter gets warped into both eyes.
    //
    // Retail text and subtitles stay below three percent in measured gameplay.
    // A more generous limit used to admit moving world fragments (15--18
    // percent) as HUD and briefly paste them into both eyes.
    return totalPixels != 0 && changedPixels != 0 &&
           changedPixels * 100u <= totalPixels * 3u;
}

inline bool IsFlatPanelCoverage(std::uint64_t changedPixels,
                                std::uint64_t totalPixels) noexcept {
    return totalPixels != 0 &&
           changedPixels * 100u > totalPixels * 81u;
}

// Das alte Retail-HUD wurde als Ganzes auf 80 Prozent gestaucht, damit seine
// Randblöcke ins bequeme Sichtfeld rücken. Seit Leben, Rüstung, Munition und
// Inventar auf dem Wrist-HUD liegen, bleiben im Post-World-Pfad vor allem
// dünne Texteinblendungen. Deren 5:4-Rückabtastung ließ bei Point-Sampling
// einzelne Glyphenpixel aus und machte Hinweise sowie Untertitel unscharf.
//
// Deshalb gilt jetzt eine exakte 1:1-Abbildung. Sie erhält jeden Pixel der
// 1080p-Retail-Schrift; VR-Statusblöcke brauchen hier nicht mehr verschoben
// zu werden. Menü und ESC-Ansicht laufen weiterhin als eigener Flat-Panel-
// Pfad und werden von dieser Abbildung nicht berührt.
constexpr std::uint32_t kStereoHudShrinkNumerator = 1;
constexpr std::uint32_t kStereoHudShrinkDenominator = 1;

// Rechnet eine Ausgabekoordinate auf die Quellkoordinate zurück. Liegt die
// Quelle außerhalb des Bildes, kommt `extent` als "hier ist kein HUD" zurück —
// derselbe Wert, den die Aufrufer bereits als ungültig behandeln.
inline std::uint32_t StereoHudSourceAxis(
    std::uint32_t outputCoordinate, std::uint32_t extent) noexcept {
    if (extent == 0) {
        return outputCoordinate;
    }
    const std::int64_t center = static_cast<std::int64_t>(extent) / 2;
    const std::int64_t offset =
        static_cast<std::int64_t>(outputCoordinate) - center;
    const std::int64_t source =
        center +
        offset * static_cast<std::int64_t>(kStereoHudShrinkNumerator) /
            static_cast<std::int64_t>(kStereoHudShrinkDenominator);
    if (source < 0 || source >= static_cast<std::int64_t>(extent)) {
        return extent;
    }
    return static_cast<std::uint32_t>(source);
}

inline std::uint32_t StereoHudSourceRow(
    std::uint32_t outputRow, std::uint32_t height) noexcept {
    return StereoHudSourceAxis(outputRow, height);
}

inline std::uint32_t StereoHudSourceColumn(
    std::uint32_t outputColumn, std::uint32_t outputRow,
    std::uint32_t width, std::uint32_t height) noexcept {
    (void)outputRow;
    (void)height;
    return StereoHudSourceAxis(outputColumn, width);
}

} // namespace fearvr

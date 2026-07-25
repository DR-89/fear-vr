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
    return totalPixels != 0 && changedPixels != 0 &&
           changedPixels * 100u <= totalPixels * 20u;
}

inline bool IsFlatPanelCoverage(std::uint64_t changedPixels,
                                std::uint64_t totalPixels) noexcept {
    return totalPixels != 0 &&
           changedPixels * 100u > totalPixels * 81u;
}

// Das HUD wird als Ganzes zur Bildmitte hin gestaucht, damit die Randblöcke
// im Headset im bequemen Sichtfeld liegen.
//
// Frühere Fassungen verschoben stattdessen einzelne Zonen um feste Beträge —
// links oben um 1/8 Breite, links unten um 1/32, rechts um 1/10, die Mitte gar
// nicht, dazu ein Zeilensprung bei 3/5 Höhe. Jedes HUD-Element, das eine
// dieser Grenzen kreuzte, wurde dadurch zerschnitten: eine Hälfte wanderte,
// die andere blieb stehen. Betroffen waren unter anderem die mittigen
// Aktivierungshinweise, die genau über der Grenze bei 3/8 Breite liegen.
//
// Eine gleichmäßige Skalierung um den Bildmittelpunkt hat diesen Fehler
// grundsätzlich nicht: Sie ist stetig und monoton, benachbarte Quellpixel
// bleiben also immer benachbart.
constexpr std::uint32_t kStereoHudShrinkNumerator = 5;
constexpr std::uint32_t kStereoHudShrinkDenominator = 4;

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

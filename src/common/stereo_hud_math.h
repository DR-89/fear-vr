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

inline std::uint32_t StereoHudSourceRow(
    std::uint32_t outputRow, std::uint32_t height) noexcept {
    if (height == 0 || outputRow < height * 3u / 5u) {
        return outputRow;
    }
    const std::uint32_t raisedRow = outputRow + height / 8u;
    return raisedRow < height ? raisedRow : height;
}

inline std::uint32_t StereoHudSourceColumn(
    std::uint32_t outputColumn, std::uint32_t outputRow,
    std::uint32_t width, std::uint32_t height) noexcept {
    if (width == 0 || height == 0) {
        return outputColumn;
    }

    // Move the lower-left status block slightly inward.
    if (outputRow >= height * 3u / 5u &&
        outputColumn < width / 2u) {
        const std::uint32_t shift = width / 32u;
        return outputColumn >= shift
            ? outputColumn - shift
            : width;
    }

    // Weapon-selection elements live at the upper/middle left edge and need
    // a stronger inward shift to remain comfortable in the headset.
    if (outputRow < height * 3u / 5u &&
        outputColumn < width * 3u / 8u) {
        const std::uint32_t shift = width / 8u;
        return outputColumn >= shift
            ? outputColumn - shift
            : width;
    }

    // Pull the right-side status block substantially toward the reticle.
    if (outputColumn >= width * 5u / 8u) {
        const std::uint32_t shifted =
            outputColumn + width / 10u;
        return shifted < width ? shifted : width;
    }
    return outputColumn;
}

} // namespace fearvr

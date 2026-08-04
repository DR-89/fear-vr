#pragma once

#include <algorithm>
#include <cstdint>

namespace fearvr {

constexpr std::uint32_t kRenderScaleMinimumPercent = 100;
constexpr std::uint32_t kRenderScaleMaximumPercent = 200;

struct RenderScaleSize {
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::uint32_t percent{kRenderScaleMinimumPercent};
};

inline constexpr std::uint32_t NormalizeRenderScalePercent(
    std::uint32_t percent) noexcept {
    return percent < kRenderScaleMinimumPercent
        ? kRenderScaleMinimumPercent
        : (percent > kRenderScaleMaximumPercent
               ? kRenderScaleMaximumPercent
               : percent);
}

inline constexpr RenderScaleSize CalculateRenderScaleSize(
    std::uint32_t sourceWidth, std::uint32_t sourceHeight,
    std::uint32_t requestedPercent, std::uint32_t maximumWidth,
    std::uint32_t maximumHeight) noexcept {
    if (sourceWidth == 0 || sourceHeight == 0 ||
        maximumWidth < sourceWidth || maximumHeight < sourceHeight) {
        return {};
    }

    std::uint32_t percent =
        NormalizeRenderScalePercent(requestedPercent);
    percent = (std::min)(
        percent,
        static_cast<std::uint32_t>(
            (static_cast<std::uint64_t>(maximumWidth) * 100U) /
            sourceWidth));
    percent = (std::min)(
        percent,
        static_cast<std::uint32_t>(
            (static_cast<std::uint64_t>(maximumHeight) * 100U) /
            sourceHeight));
    percent = (std::max)(percent, kRenderScaleMinimumPercent);

    return {
        static_cast<std::uint32_t>(
            (static_cast<std::uint64_t>(sourceWidth) * percent + 50U) /
            100U),
        static_cast<std::uint32_t>(
            (static_cast<std::uint64_t>(sourceHeight) * percent + 50U) /
            100U),
        percent};
}

} // namespace fearvr

#pragma once

#include <cstdint>

namespace fearvr {

struct VrRenderResolution {
    std::uint32_t width{0};
    std::uint32_t height{0};
};

inline bool IsUsableVrRenderResolution(
    const VrRenderResolution& resolution) noexcept {
    return resolution.width >= 1024 && resolution.height >= 720;
}

// F.E.A.R. can silently fall back to 640x480 even after the display menu has
// selected another mode.  Keep every usable user choice.  Only a VGA fallback
// is replaced: first by a source mode already proven in this process, otherwise
// by the current desktop mode.  There is deliberately no fixed resolution.
inline VrRenderResolution ResolveVrRenderResolution(
    const VrRenderResolution& requested,
    const VrRenderResolution& preferred,
    const VrRenderResolution& desktop) noexcept {
    if (requested.width > 800 || requested.height > 600) {
        return requested;
    }
    if (IsUsableVrRenderResolution(preferred)) {
        return preferred;
    }
    if (IsUsableVrRenderResolution(desktop)) {
        return desktop;
    }
    return requested;
}

} // namespace fearvr

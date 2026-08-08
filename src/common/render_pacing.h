#pragma once

#include <cstdint>

namespace fearvr {

// FEAR may reach its camera render more than once before OpenXR publishes a
// new request. Only that duplicate render should wait for the next request.
// A request that already differs from the last rendered one is fresh and must
// be consumed immediately.
inline constexpr bool ShouldWaitForNewRenderRequest(
    std::uint64_t lastRenderedFrameId,
    std::uint64_t currentFrameId) noexcept {
    return lastRenderedFrameId != 0 && currentFrameId != 0 &&
           currentFrameId == lastRenderedFrameId;
}

} // namespace fearvr

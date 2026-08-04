#pragma once

#include <cstdint>
#include <filesystem>

namespace fearvr {

struct OpenXrHostOptions {
    std::filesystem::path logDirectory{"logs"};
    std::filesystem::path startupImage;
    std::uint64_t maxFrames{0};
    std::uint64_t ipcSessionId{0};
    bool validateOnly{false};
    bool enableD3dDebug{false};
    bool exitOnGameDisconnect{false};
};

int RunOpenXrHost(const OpenXrHostOptions& options);
void RequestOpenXrHostStop() noexcept;

} // namespace fearvr

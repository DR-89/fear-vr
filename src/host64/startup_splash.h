#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

#include <d3d11.h>
#include <wrl/client.h>

namespace fearvr {

class StartupSplash {
public:
    bool Load(ID3D11Device* device, const std::filesystem::path& path,
              std::string& error) noexcept;

    ID3D11ShaderResourceView* View() const noexcept {
        return view_.Get();
    }
    std::uint32_t Width() const noexcept {
        return width_;
    }
    std::uint32_t Height() const noexcept {
        return height_;
    }

private:
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> view_;
    std::uint32_t width_{0};
    std::uint32_t height_{0};
};

} // namespace fearvr

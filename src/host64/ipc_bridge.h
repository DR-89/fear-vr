#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include <d3d11.h>
#include <wrl/client.h>

#include "protocol.h"

namespace fearvr {

using IpcLogFunction =
    std::function<void(const char* level, const char* event,
                       const std::string& message)>;

class IpcBridge {
public:
    IpcBridge(std::uint64_t sessionId, ID3D11Device* device,
              ID3D11DeviceContext* context, std::uint64_t adapterLuid,
              IpcLogFunction log);
    ~IpcBridge();

    IpcBridge(const IpcBridge&) = delete;
    IpcBridge& operator=(const IpcBridge&) = delete;

    void Tick();
    void PublishRenderRequest(const FearVrRenderRequest& request);
    bool ConsumeLatestPair();

    [[nodiscard]] bool Enabled() const noexcept;
    [[nodiscard]] bool GameConnected() const noexcept;
    [[nodiscard]] bool GameWasConnected() const noexcept;
    [[nodiscard]] bool HasImage(std::uint32_t eye) const noexcept;
    [[nodiscard]] ID3D11ShaderResourceView* ImageView(
        std::uint32_t eye) const noexcept;
    [[nodiscard]] std::uint64_t LatestFrameId() const noexcept;

private:
    struct PrivateEye;
    struct PendingCopy;

    bool TryConnect();
    void Disconnect() noexcept;
    void UpdateAdapterMatch();
    void UpdateGameHeartbeat();
    bool FinishPendingCopy();
    bool FindAndClaimPair(std::uint32_t& slotIndex,
                          std::uint64_t& frameId,
                          std::uint64_t& generation);
    bool ValidateAndOpenSource(std::uint32_t eye,
                               std::uint32_t slotIndex);
    bool EnsurePrivateTexture(std::uint32_t eye,
                              ID3D11Texture2D* source);
    void ReleaseClaim(std::uint32_t slotIndex) noexcept;
    void LogHresult(const char* event, HRESULT result);

    std::uint64_t sessionId_{0};
    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    std::uint64_t adapterLuid_{0};
    IpcLogFunction log_;
    HANDLE mapping_{nullptr};
    HANDLE frameReadyEvent_{nullptr};
    HANDLE slotConsumedEvent_{nullptr};
    HANDLE gameProcessHandle_{nullptr};
    DWORD gameProcessId_{0};
    FearVrSharedHeader* shared_{nullptr};
    PrivateEye* privateEye_{nullptr};
    PendingCopy* pending_{nullptr};
    ULONGLONG nextConnectAttempt_{0};
    ULONGLONG lastGameHeartbeatTick_{0};
    std::uint64_t lastGameHeartbeat_{0};
    std::uint64_t latestFrameId_{0};
    std::uint64_t consumedFrames_{0};
    bool gameConnected_{false};
    bool gameWasConnected_{false};
    bool protocolRejected_{false};
    bool adapterMatchLogged_{false};
    bool adapterMismatchLogged_{false};
};

} // namespace fearvr

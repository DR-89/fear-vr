#include "ipc_bridge.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <utility>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include "ipc_names.h"
#include "locomotion_reprojection.h"
#include "protocol_utils.h"

namespace fearvr {
namespace {

using Microsoft::WRL::ComPtr;

volatile LONG* AtomicState(FearVrSlot& slot) noexcept {
    return reinterpret_cast<volatile LONG*>(&slot.state);
}

volatile LONG* AtomicFlags(FearVrSharedHeader& header) noexcept {
    return reinterpret_cast<volatile LONG*>(&header.bridgeFlags);
}

volatile LONG* Atomic32(std::uint32_t& value) noexcept {
    return reinterpret_cast<volatile LONG*>(&value);
}

volatile LONG64* Atomic64(std::uint64_t& value) noexcept {
    return reinterpret_cast<volatile LONG64*>(&value);
}

std::uint64_t ReadAtomic64(std::uint64_t& value) noexcept {
    return static_cast<std::uint64_t>(
        InterlockedCompareExchange64(Atomic64(value), 0, 0));
}

} // namespace

struct IpcBridge::PrivateEye {
    ComPtr<ID3D11Texture2D> texture;
    ComPtr<ID3D11ShaderResourceView> view;
    UINT width{0};
    UINT height{0};
    DXGI_FORMAT format{DXGI_FORMAT_UNKNOWN};
};

// Eine geoeffnete Shared-Texture, gemerkt unter dem Handle, aus dem sie
// stammt. Aendert der Proxy seine Slots (Geraetereset, neue Aufloesung),
// bekommt derselbe Slot ein neues Handle — der Vergleich unten oeffnet dann
// von selbst neu.
struct IpcBridge::SharedSource {
    std::uint64_t handle{0};
    ComPtr<ID3D11Texture2D> texture;
};

struct IpcBridge::PendingCopy {
    bool active{false};
    std::uint32_t slotIndex{0};
    std::uint64_t frameId{0};
    std::uint64_t generation{0};
    std::array<ComPtr<ID3D11Texture2D>, FEARVR_EYE_COUNT> source;
    ComPtr<ID3D11Query> completion;
};

IpcBridge::IpcBridge(std::uint64_t sessionId, ID3D11Device* device,
                     ID3D11DeviceContext* context,
                     std::uint64_t adapterLuid, IpcLogFunction log)
    : sessionId_(sessionId), device_(device), context_(context),
      adapterLuid_(adapterLuid), log_(std::move(log)),
      privateEye_(new PrivateEye[FEARVR_EYE_COUNT]),
      pending_(new PendingCopy),
      sharedSource_(
          new SharedSource[FEARVR_EYE_COUNT * FEARVR_SLOTS_PER_EYE]) {
    if (Enabled()) {
        std::ostringstream message;
        message << "session=0x" << std::hex << std::uppercase << sessionId_;
        log_("INFO", "ipc_enabled", message.str());
    }
}

IpcBridge::~IpcBridge() {
    Disconnect();
    delete[] privateEye_;
    delete pending_;
    delete[] sharedSource_;
}

bool IpcBridge::Enabled() const noexcept {
    return sessionId_ != 0;
}

void IpcBridge::Tick() {
    if (!Enabled()) {
        return;
    }
    if (shared_ == nullptr && !TryConnect()) {
        return;
    }
    InterlockedIncrement64(Atomic64(shared_->hostHeartbeat));
    FinishPendingCopy();
    UpdateGameHeartbeat();
    UpdateAdapterMatch();
    // Shared frames are independent of OpenXR visibility. Import them while
    // the session is only SYNCHRONIZED so the ring keeps moving and the
    // newest game image is already available when the headset focuses.
    ConsumeLatestPair();
}

bool IpcBridge::TryConnect() {
    if (protocolRejected_) {
        return false;
    }
    const ULONGLONG now = GetTickCount64();
    if (now < nextConnectAttempt_) {
        return false;
    }
    nextConnectAttempt_ = now + 250;

    const std::wstring mappingName =
        MakeIpcObjectName(sessionId_, L"Mapping");
    mapping_ = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE,
                                mappingName.c_str());
    if (mapping_ == nullptr) {
        return false;
    }
    shared_ = static_cast<FearVrSharedHeader*>(
        MapViewOfFile(mapping_, FILE_MAP_ALL_ACCESS, 0, 0,
                      sizeof(FearVrSharedHeader)));
    if (shared_ == nullptr) {
        CloseHandle(mapping_);
        mapping_ = nullptr;
        return false;
    }
    if (!IsProtocolHeaderValid(*shared_)) {
        log_("ERROR", "protocol_mismatch",
             "Magic/version/structure sizes rejected.");
        protocolRejected_ = true;
        Disconnect();
        return false;
    }

    const std::wstring frameReadyName =
        MakeIpcObjectName(sessionId_, L"FrameReady");
    const std::wstring consumedName =
        MakeIpcObjectName(sessionId_, L"SlotConsumed");
    const std::wstring renderRequestName =
        MakeIpcObjectName(sessionId_, L"RenderRequest");
    frameReadyEvent_ =
        OpenEventW(SYNCHRONIZE, FALSE, frameReadyName.c_str());
    slotConsumedEvent_ =
        OpenEventW(EVENT_MODIFY_STATE, FALSE, consumedName.c_str());
    renderRequestEvent_ =
        OpenEventW(EVENT_MODIFY_STATE, FALSE, renderRequestName.c_str());
    if (frameReadyEvent_ == nullptr || slotConsumedEvent_ == nullptr ||
        renderRequestEvent_ == nullptr) {
        Disconnect();
        return false;
    }

    shared_->hostProcessId = GetCurrentProcessId();
    shared_->hostAdapterLuid = adapterLuid_;
    if ((ReadAtomic64(shared_->requestSequence) & 1ULL) != 0) {
        InterlockedIncrement64(Atomic64(shared_->requestSequence));
    }
    if ((ReadAtomic64(shared_->inputSequence) & 1ULL) != 0) {
        InterlockedIncrement64(Atomic64(shared_->inputSequence));
    }
    if ((ReadAtomic64(shared_->hapticSequence) & 1ULL) != 0) {
        InterlockedIncrement64(Atomic64(shared_->hapticSequence));
    }
    if ((ReadAtomic64(shared_->cameraSequence) & 1ULL) != 0) {
        InterlockedIncrement64(Atomic64(shared_->cameraSequence));
    }
    InterlockedOr(AtomicFlags(*shared_), FEARVR_BF_HOST_READY);
    log_("INFO", "ipc_connected",
         "Proxy mapping opened and protocol accepted.");
    return true;
}

void IpcBridge::Disconnect() noexcept {
    ReleaseSharedSources();
    if (shared_ != nullptr) {
        InterlockedAnd(AtomicFlags(*shared_),
                       static_cast<LONG>(~FEARVR_BF_HOST_READY));
        UnmapViewOfFile(shared_);
        shared_ = nullptr;
    }
    if (mapping_ != nullptr) {
        CloseHandle(mapping_);
        mapping_ = nullptr;
    }
    if (frameReadyEvent_ != nullptr) {
        CloseHandle(frameReadyEvent_);
        frameReadyEvent_ = nullptr;
    }
    if (slotConsumedEvent_ != nullptr) {
        CloseHandle(slotConsumedEvent_);
        slotConsumedEvent_ = nullptr;
    }
    if (renderRequestEvent_ != nullptr) {
        CloseHandle(renderRequestEvent_);
        renderRequestEvent_ = nullptr;
    }
    if (gameProcessHandle_ != nullptr) {
        CloseHandle(gameProcessHandle_);
        gameProcessHandle_ = nullptr;
    }
    gameProcessId_ = 0;
    latestFrameId_ = 0;
    latestImageCamera_ = {};
}

void IpcBridge::UpdateAdapterMatch() {
    if (shared_->gameAdapterLuid == 0) {
        return;
    }
    if (shared_->gameAdapterLuid == adapterLuid_) {
        InterlockedOr(AtomicFlags(*shared_), FEARVR_BF_ADAPTER_MATCH);
        if (!adapterMatchLogged_) {
            log_("INFO", "adapter_match",
                 "D3D9 and OpenXR adapter LUIDs match.");
            adapterMatchLogged_ = true;
        }
    } else {
        InterlockedAnd(AtomicFlags(*shared_),
                       static_cast<LONG>(~FEARVR_BF_ADAPTER_MATCH));
        if (!adapterMismatchLogged_) {
            std::ostringstream message;
            message << "game=0x" << std::hex << std::uppercase
                    << shared_->gameAdapterLuid << " host=0x"
                    << adapterLuid_;
            log_("ERROR", "adapter_mismatch", message.str());
            adapterMismatchLogged_ = true;
        }
    }
}

void IpcBridge::UpdateGameHeartbeat() {
    const std::uint64_t heartbeat =
        ReadAtomic64(shared_->gameHeartbeat);
    const ULONGLONG now = GetTickCount64();
    if (heartbeat != lastGameHeartbeat_) {
        lastGameHeartbeat_ = heartbeat;
        lastGameHeartbeatTick_ = now;
    }
    const DWORD processId = shared_->gameProcessId;
    if (processId != 0 && processId != gameProcessId_) {
        if (gameProcessHandle_ != nullptr) {
            CloseHandle(gameProcessHandle_);
        }
        gameProcessHandle_ =
            OpenProcess(SYNCHRONIZE, FALSE, processId);
        gameProcessId_ = gameProcessHandle_ == nullptr ? 0 : processId;
    }
    const bool processStateKnown = gameProcessHandle_ != nullptr;
    const bool processAlive =
        processStateKnown &&
        WaitForSingleObject(gameProcessHandle_, 0) == WAIT_TIMEOUT;
    const bool heartbeatFresh =
        heartbeat != 0 && now - lastGameHeartbeatTick_ <= 2000;
    const bool connected =
        heartbeat != 0 &&
        (shared_->bridgeFlags & FEARVR_BF_GAME_READY) != 0 &&
        (processStateKnown ? processAlive : heartbeatFresh);
    if (connected != gameConnected_) {
        gameConnected_ = connected;
        gameWasConnected_ = gameWasConnected_ || connected;
        log_(connected ? "INFO" : "WARN",
             connected ? "game_connected" : "game_disconnected",
             connected ? "Proxy heartbeat active."
                       : "Proxy heartbeat timed out; last image retained.");
    }
}

bool IpcBridge::GameConnected() const noexcept {
    return gameConnected_;
}

bool IpcBridge::GameWasConnected() const noexcept {
    return gameWasConnected_;
}

bool IpcBridge::StereoActive() const noexcept {
    return shared_ != nullptr &&
           (shared_->bridgeFlags & FEARVR_BF_STEREO_ACTIVE) != 0;
}

std::uint32_t IpcBridge::PanelRecenterGeneration() const noexcept {
    if (shared_ == nullptr) {
        return 0;
    }
    return static_cast<std::uint32_t>(
        InterlockedCompareExchange(
            Atomic32(shared_->panelRecenterGeneration), 0, 0));
}

std::uint32_t IpcBridge::FovScalePercent() const noexcept {
    if (shared_ == nullptr) {
        return FEARVR_FOV_SCALE_DEFAULT_PERCENT;
    }
    const auto percent = static_cast<std::uint32_t>(
        InterlockedCompareExchange(
            Atomic32(shared_->fovScalePercent), 0, 0));
    return NormalizeFovScalePercent(percent);
}

void IpcBridge::PublishRenderRequest(
    const FearVrRenderRequest& request) {
    if (shared_ == nullptr) {
        return;
    }
    InterlockedIncrement64(Atomic64(shared_->requestSequence));
    MemoryBarrier();
    shared_->request = request;
    MemoryBarrier();
    InterlockedIncrement64(Atomic64(shared_->requestSequence));
    SetEvent(renderRequestEvent_);
}

void IpcBridge::PublishInputState(const FearVrInputState& input) {
    if (shared_ == nullptr) {
        return;
    }
    InterlockedIncrement64(Atomic64(shared_->inputSequence));
    MemoryBarrier();
    shared_->input = input;
    MemoryBarrier();
    InterlockedIncrement64(Atomic64(shared_->inputSequence));
}

bool IpcBridge::ConsumeHapticRequest(FearVrHapticRequest& request) {
    if (shared_ == nullptr) {
        return false;
    }
    for (int attempt = 0; attempt < 4; ++attempt) {
        const std::uint64_t before =
            ReadAtomic64(shared_->hapticSequence);
        if ((before & 1ULL) != 0 || before == 0) {
            continue;
        }
        const FearVrHapticRequest snapshot = shared_->haptic;
        MemoryBarrier();
        const std::uint64_t after =
            ReadAtomic64(shared_->hapticSequence);
        if (before != after || (after & 1ULL) != 0 ||
            (snapshot.flags & FEARVR_HF_VALID) == 0 ||
            snapshot.requestId == 0 ||
            snapshot.requestId == lastHapticRequestId_) {
            continue;
        }
        lastHapticRequestId_ = snapshot.requestId;
        request = snapshot;
        return true;
    }
    return false;
}

bool IpcBridge::ConsumeLatestPair() {
    if (shared_ == nullptr || !gameConnected_ ||
        (shared_->bridgeFlags & FEARVR_BF_ADAPTER_MATCH) == 0 ||
        (shared_->bridgeFlags & FEARVR_BF_SHARED_SUPPORTED) == 0) {
        return false;
    }
    if (!FinishPendingCopy()) {
        return false;
    }

    std::uint32_t slotIndex = 0;
    std::uint64_t frameId = 0;
    std::uint64_t generation = 0;
    FearVrGameCameraSample camera{};
    if (!FindAndClaimPair(
            slotIndex, frameId, generation, camera)) {
        return false;
    }

    const auto copyStart = std::chrono::steady_clock::now();

    bool valid = true;
    for (std::uint32_t eye = 0; eye < FEARVR_EYE_COUNT; ++eye) {
        if (!ValidateAndOpenSource(eye, slotIndex) ||
            !EnsurePrivateTexture(eye, pending_->source[eye].Get())) {
            valid = false;
            break;
        }
    }
    if (!valid) {
        ++consecutiveOpenFailures_;
        ReleaseClaim(slotIndex);
        for (auto& source : pending_->source) {
            source.Reset();
        }
        return false;
    }
    consecutiveOpenFailures_ = 0;

    for (std::uint32_t eye = 0; eye < FEARVR_EYE_COUNT; ++eye) {
        context_->CopyResource(privateEye_[eye].texture.Get(),
                               pending_->source[eye].Get());
    }
    if (!pending_->completion) {
        D3D11_QUERY_DESC queryDescription{};
        queryDescription.Query = D3D11_QUERY_EVENT;
        const HRESULT result = device_->CreateQuery(
            &queryDescription,
            pending_->completion.ReleaseAndGetAddressOf());
        if (FAILED(result)) {
            LogHresult("d3d11_query_create_failed", result);
            ReleaseClaim(slotIndex);
            return false;
        }
    }

    pending_->active = true;
    pending_->slotIndex = slotIndex;
    pending_->frameId = frameId;
    pending_->generation = generation;
    context_->End(pending_->completion.Get());
    context_->Flush();

    const auto copyMicroseconds =
        static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - copyStart)
                .count());
    ++copyStats_.samples;
    copyStats_.totalMicroseconds += copyMicroseconds;
    copyStats_.maxMicroseconds =
        (std::max)(copyStats_.maxMicroseconds, copyMicroseconds);

    latestFrameId_ = frameId;
    latestImageCamera_ = camera;
    latestGeneration_ = generation;
    ++consumedFrames_;
    if (consumedFrames_ == 1 || consumedFrames_ % 300 == 0) {
        std::ostringstream message;
        message << "frame=" << frameId << " generation=" << generation
                << " slot=" << slotIndex
                << " consumed=" << consumedFrames_;
        log_("INFO", "ipc_frame", message.str());
    }
    FinishPendingCopy();
    return true;
}

bool IpcBridge::FinishPendingCopy() {
    if (!pending_->active) {
        return true;
    }
    const HRESULT result = context_->GetData(
        pending_->completion.Get(), nullptr, 0, 0);
    if (result == S_FALSE) {
        return false;
    }
    if (FAILED(result)) {
        LogHresult("d3d11_query_failed", result);
    }
    ReleaseClaim(pending_->slotIndex);
    for (auto& source : pending_->source) {
        source.Reset();
    }
    pending_->active = false;
    SetEvent(slotConsumedEvent_);
    return true;
}

bool IpcBridge::FindAndClaimPair(std::uint32_t& slotIndex,
                                 std::uint64_t& frameId,
                                 std::uint64_t& generation,
                                 FearVrGameCameraSample& camera) {
    std::array<std::uint32_t, FEARVR_SLOTS_PER_EYE> order{};
    for (std::uint32_t index = 0; index < FEARVR_SLOTS_PER_EYE; ++index) {
        order[index] = index;
    }
    std::sort(order.begin(), order.end(),
              [this](std::uint32_t left, std::uint32_t right) {
                  return shared_->slot[FEARVR_EYE_LEFT][left].frameId >
                         shared_->slot[FEARVR_EYE_LEFT][right].frameId;
              });

    for (const std::uint32_t candidate : order) {
        FearVrSlot& left =
            shared_->slot[FEARVR_EYE_LEFT][candidate];
        FearVrSlot& right =
            shared_->slot[FEARVR_EYE_RIGHT][candidate];
        if (InterlockedCompareExchange(AtomicState(left),
                                       FEARVR_SLOT_CONSUMING,
                                       FEARVR_SLOT_READY) !=
            FEARVR_SLOT_READY) {
            continue;
        }
        if (InterlockedCompareExchange(AtomicState(right),
                                       FEARVR_SLOT_CONSUMING,
                                       FEARVR_SLOT_READY) !=
            FEARVR_SLOT_READY) {
            InterlockedExchange(AtomicState(left), FEARVR_SLOT_READY);
            continue;
        }
        MemoryBarrier();
        if (left.frameId == 0 || left.frameId != right.frameId ||
            left.generation == 0 ||
            left.generation != right.generation) {
            ReleaseClaim(candidate);
            continue;
        }
        slotIndex = candidate;
        frameId = left.frameId;
        generation = left.generation;
        camera = {};
        if (left.camera.frameId == frameId &&
            right.camera.frameId == frameId &&
            std::memcmp(
                &left.camera, &right.camera,
                sizeof(FearVrGameCameraSample)) == 0 &&
            IsValidGameCameraSample(left.camera)) {
            camera = left.camera;
        }
        return true;
    }
    return false;
}

bool IpcBridge::ValidateAndOpenSource(std::uint32_t eye,
                                      std::uint32_t slotIndex) {
    const FearVrSlot& slot = shared_->slot[eye][slotIndex];
    if (slot.sharedHandle == 0 || slot.width == 0 || slot.height == 0 ||
        slot.width > 16384 || slot.height > 16384 ||
        slot.format != FEARVR_FMT_B8G8R8A8) {
        log_("ERROR", "slot_rejected",
             "Invalid handle, dimensions, or format.");
        return false;
    }
    // Dasselbe Handle wie beim letzten Mal: die Textur ist bereits offen.
    //
    // Vorher wurde hier jedes Spielbild zweimal `OpenSharedResource`
    // aufgerufen und die Textur danach sofort wieder freigegeben. Das ist ein
    // Kernelaufruf mit Treiber-Sperre, kein Zeigerkopieren: im Log vom
    // 28.07.2026 lag `copy_avg_us` deshalb bei 310–440 µs mit Spitzen bis
    // 3772 µs, und die XR-Displayrate brach in einzelnen Fenstern auf 72,8
    // statt 90 ein — die kurzen Ruckler, bei denen die Welt sichtbar
    // nachhing. Der Inhalt der Textur aendert sich davon nicht: Wer welchen
    // Slot lesen darf, klaert allein das Claim-Protokoll.
    SharedSource& cached =
        sharedSource_[eye * FEARVR_SLOTS_PER_EYE + slotIndex];
    if (cached.texture && cached.handle == slot.sharedHandle) {
        pending_->source[eye] = cached.texture;
        return true;
    }
    cached.texture.Reset();
    cached.handle = 0;

    const HANDLE handle = reinterpret_cast<HANDLE>(
        static_cast<std::uintptr_t>(slot.sharedHandle));
    ComPtr<ID3D11Resource> resource;
    HRESULT result = device_->OpenSharedResource(
        handle, IID_PPV_ARGS(resource.ReleaseAndGetAddressOf()));
    if (FAILED(result)) {
        std::ostringstream message;
        message << "HRESULT=0x" << std::hex << std::uppercase
                << static_cast<std::uint32_t>(result)
                << " frame=" << std::dec << slot.frameId
                << " slot=" << slotIndex;
        log_(
            consecutiveOpenFailures_ == 0 ? "WARN" : "ERROR",
            consecutiveOpenFailures_ == 0
                ? "shared_resource_stale"
                : "open_shared_resource_failed",
            message.str());
        return false;
    }
    result = resource.As(&pending_->source[eye]);
    if (FAILED(result)) {
        LogHresult("shared_texture_interface_failed", result);
        return false;
    }
    D3D11_TEXTURE2D_DESC description{};
    pending_->source[eye]->GetDesc(&description);
    if (description.Width != slot.width ||
        description.Height != slot.height ||
        description.MipLevels != 1 || description.ArraySize != 1 ||
        description.SampleDesc.Count != 1 ||
        description.Format != DXGI_FORMAT_B8G8R8A8_UNORM) {
        log_("ERROR", "shared_texture_rejected",
             "D3D11 resource description does not match protocol.");
        pending_->source[eye].Reset();
        return false;
    }
    cached.texture = pending_->source[eye];
    cached.handle = slot.sharedHandle;
    return true;
}

// Die gemerkten Texturen gehoeren zum Proxyprozess. Faellt die Verbindung
// weg, sind die Handles nichts mehr wert und muessen vor einem neuen Aufbau
// vergessen werden.
void IpcBridge::ReleaseSharedSources() noexcept {
    if (sharedSource_ == nullptr) {
        return;
    }
    for (std::uint32_t index = 0;
         index < FEARVR_EYE_COUNT * FEARVR_SLOTS_PER_EYE; ++index) {
        sharedSource_[index].texture.Reset();
        sharedSource_[index].handle = 0;
    }
}

bool IpcBridge::EnsurePrivateTexture(std::uint32_t eye,
                                     ID3D11Texture2D* source) {
    D3D11_TEXTURE2D_DESC sourceDescription{};
    source->GetDesc(&sourceDescription);
    PrivateEye& destination = privateEye_[eye];
    if (destination.texture &&
        destination.width == sourceDescription.Width &&
        destination.height == sourceDescription.Height &&
        destination.format == sourceDescription.Format) {
        return true;
    }

    destination = {};
    D3D11_TEXTURE2D_DESC privateDescription = sourceDescription;
    // FEAR's D3D9 backbuffer stores display-referred sRGB colour values even
    // though the shared resource is exposed as plain UNORM. Sampling that
    // resource as linear and then writing it into an sRGB OpenXR swapchain
    // gamma-encodes it a second time, making shaded surfaces much brighter in
    // VR than in the game window.
    //
    // Keep the copied bits unchanged in a typeless resource and interpret
    // them as sRGB only in the shader view. CopyResource permits this within
    // the BGRA8 format family, while the shader receives linear-light values.
    privateDescription.Format = DXGI_FORMAT_B8G8R8A8_TYPELESS;
    privateDescription.Usage = D3D11_USAGE_DEFAULT;
    privateDescription.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    privateDescription.CPUAccessFlags = 0;
    privateDescription.MiscFlags = 0;
    HRESULT result = device_->CreateTexture2D(
        &privateDescription, nullptr,
        destination.texture.ReleaseAndGetAddressOf());
    if (FAILED(result)) {
        LogHresult("private_texture_create_failed", result);
        return false;
    }
    D3D11_SHADER_RESOURCE_VIEW_DESC viewDescription{};
    viewDescription.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    viewDescription.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    viewDescription.Texture2D.MostDetailedMip = 0;
    viewDescription.Texture2D.MipLevels = 1;
    result = device_->CreateShaderResourceView(
        destination.texture.Get(), &viewDescription,
        destination.view.ReleaseAndGetAddressOf());
    if (FAILED(result)) {
        LogHresult("private_srv_create_failed", result);
        destination = {};
        return false;
    }
    destination.width = sourceDescription.Width;
    destination.height = sourceDescription.Height;
    destination.format = sourceDescription.Format;
    std::ostringstream message;
    message << "eye=" << eye << " size=" << destination.width << 'x'
            << destination.height << " format="
            << static_cast<unsigned>(destination.format)
            << " sampling=srgb";
    log_("INFO", "private_texture", message.str());
    return true;
}

void IpcBridge::ReleaseClaim(std::uint32_t slotIndex) noexcept {
    if (shared_ == nullptr) {
        return;
    }
    for (std::uint32_t eye = 0; eye < FEARVR_EYE_COUNT; ++eye) {
        InterlockedExchange(
            AtomicState(shared_->slot[eye][slotIndex]),
            FEARVR_SLOT_EMPTY);
    }
}

bool IpcBridge::HasImage(std::uint32_t eye) const noexcept {
    return eye < FEARVR_EYE_COUNT && privateEye_[eye].view;
}

ID3D11ShaderResourceView* IpcBridge::ImageView(
    std::uint32_t eye) const noexcept {
    return HasImage(eye) ? privateEye_[eye].view.Get() : nullptr;
}

std::uint64_t IpcBridge::LatestFrameId() const noexcept {
    return latestFrameId_;
}

bool IpcBridge::LatestImageCamera(
    FearVrGameCameraSample& camera) const noexcept {
    if (!IsValidGameCameraSample(latestImageCamera_) ||
        latestImageCamera_.frameId != latestFrameId_) {
        return false;
    }
    camera = latestImageCamera_;
    return true;
}

bool IpcBridge::LatestGameCamera(
    FearVrGameCameraSample& camera) const noexcept {
    if (shared_ == nullptr) {
        return false;
    }
    for (int attempt = 0; attempt < 4; ++attempt) {
        const std::uint64_t before =
            ReadAtomic64(shared_->cameraSequence);
        if ((before & 1ULL) != 0 || before == 0) {
            continue;
        }
        const FearVrGameCameraSample snapshot =
            shared_->latestCamera;
        MemoryBarrier();
        const std::uint64_t after =
            ReadAtomic64(shared_->cameraSequence);
        if (before == after && (after & 1ULL) == 0 &&
            IsValidGameCameraSample(snapshot)) {
            camera = snapshot;
            return true;
        }
    }
    return false;
}

std::uint64_t IpcBridge::LatestGeneration() const noexcept {
    return latestGeneration_;
}

BridgeCopyStats IpcBridge::TakeCopyStats() noexcept {
    const BridgeCopyStats snapshot = copyStats_;
    copyStats_ = BridgeCopyStats{};
    return snapshot;
}

void IpcBridge::LogHresult(const char* event, HRESULT result) {
    std::ostringstream message;
    message << "HRESULT=0x" << std::hex << std::uppercase
            << static_cast<std::uint32_t>(result);
    log_("ERROR", event, message.str());
}

} // namespace fearvr

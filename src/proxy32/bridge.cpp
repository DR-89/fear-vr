#include "bridge.h"

#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

#include <Shellapi.h>
#include <d3dcompiler.h>
#include <dxgi1_2.h>
#include <MinHook.h>
#include <wrl/client.h>

#include "fearvr-version.h"
#include "ipc_names.h"
#include "locomotion_reprojection.h"
#include "protocol_utils.h"
#include "stereo_hud_math.h"
#include "system_d3d9.h"

namespace fearvr {
namespace {

using Microsoft::WRL::ComPtr;

std::string JsonEscape(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size() + 8);
    for (const char character : value) {
        switch (character) {
        case '\\':
            escaped += "\\\\";
            break;
        case '"':
            escaped += "\\\"";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            escaped += character;
            break;
        }
    }
    return escaped;
}

std::string UtcTimestamp(bool fileSafe) {
    SYSTEMTIME time{};
    GetSystemTime(&time);
    std::ostringstream output;
    output << std::setfill('0') << std::setw(4) << time.wYear;
    if (fileSafe) {
        output << std::setw(2) << time.wMonth << std::setw(2) << time.wDay
               << '-' << std::setw(2) << time.wHour << std::setw(2)
               << time.wMinute << std::setw(2) << time.wSecond << '-'
               << GetCurrentProcessId();
    } else {
        output << '-' << std::setw(2) << time.wMonth << '-' << std::setw(2)
               << time.wDay << 'T' << std::setw(2) << time.wHour << ':'
               << std::setw(2) << time.wMinute << ':' << std::setw(2)
               << time.wSecond << '.' << std::setw(3) << time.wMilliseconds
               << 'Z';
    }
    return output.str();
}

class Logger {
public:
    void Open(const std::filesystem::path& directory) noexcept {
        try {
            std::error_code error;
            std::filesystem::create_directories(directory, error);
            if (error) {
                return;
            }
            path_ = directory /
                    ("proxy-" + UtcTimestamp(true) + ".log");
            stream_.open(path_, std::ios::out | std::ios::trunc);
        } catch (...) {
        }
    }

    void Write(const char* level, const char* event,
               const std::string& message) noexcept {
        try {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!stream_.is_open()) {
                return;
            }
            stream_ << "{\"time\":\"" << UtcTimestamp(false)
                    << "\",\"level\":\"" << JsonEscape(level)
                    << "\",\"event\":\"" << JsonEscape(event)
                    << "\",\"message\":\"" << JsonEscape(message)
                    << "\"}\n";
            stream_.flush();
        } catch (...) {
        }
    }

private:
    std::mutex mutex_;
    std::filesystem::path path_;
    std::ofstream stream_;
};

struct CommandLineConfig {
    std::uint64_t sessionId{0};
    std::filesystem::path logDirectory;
    bool stereoEnabled{false};
    bool stereoToggleAllowed{false};
    bool translationEnabled{false};
    bool stereoHudEnabled{false};
    // Notausstieg: laesst den GPU-Kompositor weg und mischt das HUD wieder
    // Pixel fuer Pixel auf der CPU. Der GPU-Weg zeichnet in das Geraet des
    // Spiels; bleibt danach etwas schwarz, trennt dieser Schalter die Ursache.
    bool disableGpuHud{false};
};

CommandLineConfig ReadConfig() noexcept {
    CommandLineConfig config;
    int argumentCount = 0;
    wchar_t** arguments =
        CommandLineToArgvW(GetCommandLineW(), &argumentCount);
    if (arguments == nullptr) {
        return config;
    }
    for (int index = 1; index < argumentCount; ++index) {
        if (_wcsicmp(arguments[index], L"-fearvr-session") == 0 &&
            index + 1 < argumentCount) {
            wchar_t* end = nullptr;
            const unsigned long long parsed =
                _wcstoui64(arguments[++index], &end, 0);
            if (end != arguments[index] && *end == L'\0' && parsed != 0) {
                config.sessionId = static_cast<std::uint64_t>(parsed);
            }
        } else if (_wcsicmp(arguments[index], L"-fearvr-logdir") == 0 &&
                   index + 1 < argumentCount) {
            config.logDirectory = arguments[++index];
        } else if (_wcsicmp(
                       arguments[index], L"-fearvr-stereo") == 0) {
            config.stereoEnabled = true;
        } else if (_wcsicmp(
                       arguments[index], L"-fearvr-stereo-toggle") == 0) {
            config.stereoToggleAllowed = true;
        } else if (_wcsicmp(
                       arguments[index], L"-fearvr-translation") == 0) {
            config.translationEnabled = true;
        } else if (_wcsicmp(
                       arguments[index], L"-fearvr-stereo-hud") == 0) {
            config.stereoHudEnabled = true;
        } else if (_wcsicmp(
                       arguments[index], L"-fearvr-no-gpu-hud") == 0) {
            config.disableGpuHud = true;
        }
    }
    LocalFree(arguments);
    return config;
}

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

struct SlotResource {
    ComPtr<IDirect3DTexture9> texture;
    ComPtr<IDirect3DSurface9> surface;
    ComPtr<IDirect3DQuery9> completion;
    HANDLE sharedHandle{nullptr};
};

enum class TransferMode {
    None,
    DirectShared,
    CpuViaD3D9Ex
};

// Keep the user's game/monitor resolution untouched, but bound the
// system-memory bridge. At 2560x1440 a stereo pair is almost 30 MiB and field
// logs showed 19 ms spent in readback/upload alone. StretchRect reduces the
// image on the GPU before that expensive copy.
constexpr UINT kMaxTransportWidth = 1920;
constexpr UINT kMaxTransportHeight = 1080;

struct TransportExtent {
    UINT width{0};
    UINT height{0};
};

TransportExtent ComputeTransportExtent(UINT sourceWidth,
                                       UINT sourceHeight) noexcept {
    if (sourceWidth == 0 || sourceHeight == 0 ||
        (sourceWidth <= kMaxTransportWidth &&
         sourceHeight <= kMaxTransportHeight)) {
        return {sourceWidth, sourceHeight};
    }

    const std::uint64_t widthLimitedHeight =
        (static_cast<std::uint64_t>(sourceHeight) *
             kMaxTransportWidth +
         sourceWidth / 2U) /
        sourceWidth;
    if (widthLimitedHeight <= kMaxTransportHeight) {
        return {
            kMaxTransportWidth,
            static_cast<UINT>(
                (std::max)(std::uint64_t{1}, widthLimitedHeight))};
    }

    const std::uint64_t heightLimitedWidth =
        (static_cast<std::uint64_t>(sourceWidth) *
             kMaxTransportHeight +
         sourceHeight / 2U) /
        sourceHeight;
    return {
        static_cast<UINT>(
            (std::max)(std::uint64_t{1}, heightLimitedWidth)),
        kMaxTransportHeight};
}

// ============================================================================
// GPU-Kompositor für das Stereo-HUD
//
// Die erste Fassung verglich das Present-Bild und das rechte Weltbild Pixel für
// Pixel auf der CPU. Das kostete pro Bild drei volle Readbacks über den Bus und
// drei Durchläufe über alle Pixel — bei 1080p rund sechs Millionen Iterationen.
// Dieselbe Entscheidung trifft ein Pixelshader auf der GPU, wo die Bilder
// ohnehin schon liegen.
//
// Die Mathematik ist bewusst dieselbe wie in `stereo_hud_math.h`: Schwelle über
// den Farbkanälen, Stauchung um die Bildmitte, und die Auswahl zwischen
// Weltbild und Present. Der Deckungsgrad, der Vollbildeffekte vom HUD trennt,
// wird bis auf ein 1x1-Texel reduziert. Der Composite-Shader liest dieses Texel
// noch im selben Bild und kann daher auch eine plötzlich große Änderung sicher
// abweisen. Die verzögerte CPU-Lesung dient nur der Diagnose.
// ============================================================================

constexpr UINT kHudMaxReduceLevels = 8;

constexpr char kHudMaskShader[] = R"(
sampler2D presented : register(s0);
sampler2D rightWorld : register(s1);
float4 params : register(c0);

float4 main(float2 uv : TEXCOORD0) : COLOR0 {
    float3 difference = abs(tex2D(presented, uv).rgb -
                            tex2D(rightWorld, uv).rgb);
    float changed = step(
        params.x, max(max(difference.r, difference.g), difference.b));
    return float4(changed, changed, changed, 1.0);
}
)";

constexpr char kHudReduceShader[] = R"(
sampler2D source : register(s0);
float4 texel : register(c0);

float4 main(float2 uv : TEXCOORD0) : COLOR0 {
    // Vier bilineare Abgriffe, jeder in der Mitte eines 2x2-Quadranten: das
    // ergibt exakt den Mittelwert der 4x4 Quelltexel dieses Zieltexels.
    float4 total = tex2D(source, uv + float2(-texel.x, -texel.y)) +
                   tex2D(source, uv + float2( texel.x, -texel.y)) +
                   tex2D(source, uv + float2(-texel.x,  texel.y)) +
                   tex2D(source, uv + float2( texel.x,  texel.y));
    return total * 0.25;
}
)";

constexpr char kHudCompositeShader[] = R"(
sampler2D presented : register(s0);
sampler2D rightWorld : register(s1);
sampler2D eyeWorld : register(s2);
sampler2D coverageMask : register(s3);
float4 params : register(c0);
float4 size : register(c1);
float4 shrink : register(c2);

float4 main(float2 uv : TEXCOORD0) : COLOR0 {
    float4 world = tex2D(eyeWorld, uv);
    float4 flatImage = tex2D(presented, uv);

    // Ganzzahlige Rückrechnung wie StereoHudSourceAxis: Ablage von der Mitte
    // mal 5/4, zur Null hin abgeschnitten.
    float2 outputPixel = floor(uv * size.xy);
    float2 center = floor(size.xy * 0.5);
    float2 scaled = (outputPixel - center) * shrink.x;
    float2 sourcePixel = center + sign(scaled) * floor(abs(scaled));
    float2 sourceUv = (sourcePixel + 0.5) * size.zw;
    float inside = step(0.0, sourcePixel.x) * step(0.0, sourcePixel.y) *
                   step(sourcePixel.x, size.x - 1.0) *
                   step(sourcePixel.y, size.y - 1.0);

    float4 overlayColor = tex2D(presented, sourceUv);
    float3 difference = abs(overlayColor.rgb -
                            tex2D(rightWorld, sourceUv).rgb);
    float changed = step(
        params.x, max(max(difference.r, difference.g), difference.b));

    // Die 1x1-Abdeckung gehört zum aktuellen Bild. Damit kann kein plötzlicher
    // Welt-/Posteffekt mehr aufgrund der CPU-Auswertung des Vorbilds einen
    // einzelnen Frame lang als HUD in beide Augen gelangen.
    float currentCoverage =
        tex2D(coverageMask, float2(0.5, 0.5)).r;
    float sparse = 1.0 - step(params.w, currentCoverage);
    float overlay = params.y * inside * changed * sparse;
    return lerp(lerp(world, overlayColor, overlay), flatImage, params.z);
}
)";

struct HudQuadVertex {
    float x, y, z, rhw;
    float u, v;
};

using D3DCompileFunction = HRESULT(WINAPI*)(
    LPCVOID, SIZE_T, LPCSTR, const D3D_SHADER_MACRO*, ID3DInclude*,
    LPCSTR, LPCSTR, UINT, UINT, ID3DBlob**, ID3DBlob**);

class GpuHudCompositor {
public:
    // Legt Shader, Zwischenziele und den Zustandsblock an. Schlägt irgendetwas
    // davon fehl, bleibt der Kompositor einfach aus und der Aufrufer mischt
    // weiter auf der CPU.
    bool Initialize(IDirect3DDevice9* device, UINT width, UINT height,
                    Logger& logger) noexcept {
        Release();
        width_ = width;
        height_ = height;

        D3DCAPS9 caps{};
        if (FAILED(device->GetDeviceCaps(&caps)) ||
            caps.PixelShaderVersion < D3DPS_VERSION(2, 0)) {
            logger.Write(
                "WARN", "stereo_hud_gpu_unsupported",
                "The game device reports less than pixel shader 2.0; "
                "HUD compositing stays on the CPU.");
            return false;
        }
        if (!LoadCompiler(logger)) {
            return false;
        }
        if (!CompilePixelShader(device, kHudMaskShader, maskShader_,
                                "stereo_hud_mask_shader_failed", logger) ||
            !CompilePixelShader(device, kHudReduceShader, reduceShader_,
                                "stereo_hud_reduce_shader_failed", logger) ||
            !CompilePixelShader(device, kHudCompositeShader,
                                compositeShader_,
                                "stereo_hud_composite_shader_failed",
                                logger)) {
            return false;
        }

        for (std::uint32_t eye = 0; eye < FEARVR_EYE_COUNT; ++eye) {
            if (!CreateRenderTargetTexture(
                    device, width, height, composite_[eye], logger)) {
                return false;
            }
        }
        if (!CreateRenderTargetTexture(
                device, width, height, mask_, logger)) {
            return false;
        }

        UINT levelWidth = width;
        UINT levelHeight = height;
        while (reduceLevelCount_ < kHudMaxReduceLevels &&
               (levelWidth > 1U || levelHeight > 1U)) {
            levelWidth = (levelWidth + 3U) / 4U;
            levelHeight = (levelHeight + 3U) / 4U;
            if (!CreateRenderTargetTexture(
                    device, levelWidth, levelHeight,
                    reduce_[reduceLevelCount_], logger)) {
                return false;
            }
            ++reduceLevelCount_;
        }
        coverageWidth_ = levelWidth;
        coverageHeight_ = levelHeight;
        if (reduceLevelCount_ == 0) {
            // Ein sehr kleines Bild braucht keine Reduktion; dann wird die
            // Maske selbst gelesen.
            coverageWidth_ = width;
            coverageHeight_ = height;
        }
        HRESULT result = device->CreateOffscreenPlainSurface(
            coverageWidth_, coverageHeight_, D3DFMT_A8R8G8B8,
            D3DPOOL_SYSTEMMEM, coverageReadback_.ReleaseAndGetAddressOf(),
            nullptr);
        if (FAILED(result)) {
            LogHresult(logger, "stereo_hud_coverage_surface_failed", result);
            return false;
        }
        result = device->CreateStateBlock(
            D3DSBT_ALL, stateBlock_.ReleaseAndGetAddressOf());
        if (FAILED(result)) {
            LogHresult(logger, "stereo_hud_state_block_failed", result);
            return false;
        }

        ready_ = true;
        coveragePending_ = false;
        std::ostringstream message;
        message << "reduce_levels=" << reduceLevelCount_
                << " coverage=" << coverageWidth_ << 'x' << coverageHeight_;
        logger.Write("INFO", "stereo_hud_gpu_ready", message.str());
        return true;
    }

    void Release() noexcept {
        ready_ = false;
        coveragePending_ = false;
        coverageSource_ = nullptr;
        coverageTexture_ = nullptr;
        coverageRatio_ = 1.0;
        reduceLevelCount_ = 0;
        stateBlock_.Reset();
        coverageReadback_.Reset();
        for (auto& level : reduce_) {
            level = {};
        }
        mask_ = {};
        for (auto& target : composite_) {
            target = {};
        }
        compositeShader_.Reset();
        reduceShader_.Reset();
        maskShader_.Reset();
    }

    bool ready() const noexcept { return ready_; }

    IDirect3DSurface9* CompositeSurface(std::uint32_t eye) const noexcept {
        return composite_[eye].surface.Get();
    }

    // Anteil geänderter Pixel aus dem *vorherigen* Bild. Der Wert dient nur
    // noch der Diagnose. Die Bildausgabe wird im Shader mit der aktuellen
    // 1x1-Abdeckung geschützt.
    double coverageRatio() const noexcept { return coverageRatio_; }

    bool Compose(IDirect3DDevice9* device,
                 IDirect3DTexture9* presented,
                 IDirect3DTexture9* rightWorld,
                 IDirect3DTexture9* const eyeWorld[FEARVR_EYE_COUNT],
                 bool compositeEnabled, bool flatPanel,
                 Logger& logger) noexcept {
        if (!ready_ || stateBlock_->Capture() != D3D_OK) {
            return false;
        }
        ComPtr<IDirect3DSurface9> previousTarget;
        ComPtr<IDirect3DSurface9> previousDepth;
        device->GetRenderTarget(0, previousTarget.ReleaseAndGetAddressOf());
        // Ohne Tiefenpuffer zeichnen: Ein fremdes Ziel anderer Größe würde
        // sonst den Zeichenaufruf ablehnen.
        device->GetDepthStencilSurface(
            previousDepth.ReleaseAndGetAddressOf());

        // Der Present-Hook läuft nach EndScene; ein Zeichenaufruf braucht aber
        // eine offene Szene. Schlägt BeginScene fehl, sind wir bereits in
        // einer — dann darf sie hier auch nicht geschlossen werden.
        const bool beganScene = SUCCEEDED(device->BeginScene());
        bool succeeded = ApplyCommonState(device);
        if (succeeded) {
            ReadPreviousCoverage(logger);
            succeeded = RenderMaskAndReduce(device, presented, rightWorld);
        }
        for (std::uint32_t eye = 0; succeeded && eye < FEARVR_EYE_COUNT;
             ++eye) {
            succeeded = RenderComposite(
                device, presented, rightWorld, eyeWorld[eye],
                composite_[eye], compositeEnabled, flatPanel);
        }
        if (beganScene) {
            device->EndScene();
        }
        if (succeeded && coverageSource_ != nullptr) {
            // Der Kopiervorgang läuft ab hier neben dem Spiel her; gelesen
            // wird er erst im nächsten Bild.
            coveragePending_ = SUCCEEDED(device->GetRenderTargetData(
                coverageSource_, coverageReadback_.Get()));
            coverageSource_ = nullptr;
        }

        device->SetDepthStencilSurface(previousDepth.Get());
        if (previousTarget) {
            device->SetRenderTarget(0, previousTarget.Get());
        }
        stateBlock_->Apply();
        if (!succeeded) {
            logger.Write(
                "WARN", "stereo_hud_gpu_compose_failed",
                "A GPU HUD pass failed; the CPU compositor takes over.");
            ready_ = false;
        }
        return succeeded;
    }

private:
    struct RenderTargetTexture {
        ComPtr<IDirect3DTexture9> texture;
        ComPtr<IDirect3DSurface9> surface;
        UINT width{0};
        UINT height{0};
    };

    static void LogHresult(Logger& logger, const char* event,
                           HRESULT result) noexcept {
        std::ostringstream message;
        message << "HRESULT=0x" << std::hex << std::uppercase
                << static_cast<std::uint32_t>(result);
        logger.Write("ERROR", event, message.str());
    }

    bool LoadCompiler(Logger& logger) noexcept {
        if (compile_ != nullptr) {
            return true;
        }
        // Bewusst dynamisch: Fehlt der Compiler, soll die Bridge weiterlaufen
        // und nicht schon beim Laden der DLL scheitern.
        const HMODULE module = LoadLibraryW(L"d3dcompiler_47.dll");
        if (module == nullptr) {
            logger.Write(
                "WARN", "stereo_hud_compiler_missing",
                "d3dcompiler_47.dll is unavailable; HUD compositing "
                "stays on the CPU.");
            return false;
        }
        compile_ = reinterpret_cast<D3DCompileFunction>(
            reinterpret_cast<void*>(
                GetProcAddress(module, "D3DCompile")));
        if (compile_ == nullptr) {
            logger.Write(
                "WARN", "stereo_hud_compiler_missing",
                "d3dcompiler_47.dll exports no D3DCompile.");
            return false;
        }
        return true;
    }

    bool CompilePixelShader(IDirect3DDevice9* device, const char* source,
                            ComPtr<IDirect3DPixelShader9>& shader,
                            const char* failureEvent,
                            Logger& logger) noexcept {
        ComPtr<ID3DBlob> code;
        ComPtr<ID3DBlob> errors;
        HRESULT result = compile_(
            source, std::strlen(source), "fearvr_stereo_hud", nullptr,
            nullptr, "main", "ps_2_0", 0, 0,
            code.ReleaseAndGetAddressOf(),
            errors.ReleaseAndGetAddressOf());
        if (FAILED(result) || !code) {
            if (errors) {
                logger.Write(
                    "ERROR", failureEvent,
                    std::string(
                        static_cast<const char*>(errors->GetBufferPointer()),
                        errors->GetBufferSize()));
            } else {
                LogHresult(logger, failureEvent, result);
            }
            return false;
        }
        result = device->CreatePixelShader(
            static_cast<const DWORD*>(code->GetBufferPointer()),
            shader.ReleaseAndGetAddressOf());
        if (FAILED(result)) {
            LogHresult(logger, failureEvent, result);
            return false;
        }
        return true;
    }

    static bool CreateRenderTargetTexture(IDirect3DDevice9* device,
                                          UINT width, UINT height,
                                          RenderTargetTexture& target,
                                          Logger& logger) noexcept {
        target = {};
        HRESULT result = device->CreateTexture(
            width, height, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
            D3DPOOL_DEFAULT, target.texture.ReleaseAndGetAddressOf(),
            nullptr);
        if (FAILED(result)) {
            LogHresult(logger, "stereo_hud_target_create_failed", result);
            return false;
        }
        result = target.texture->GetSurfaceLevel(
            0, target.surface.ReleaseAndGetAddressOf());
        if (FAILED(result)) {
            LogHresult(logger, "stereo_hud_target_surface_failed", result);
            return false;
        }
        target.width = width;
        target.height = height;
        return true;
    }

    bool ApplyCommonState(IDirect3DDevice9* device) noexcept {
        device->SetVertexShader(nullptr);
        device->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1);
        device->SetRenderState(D3DRS_ZENABLE, FALSE);
        device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
        device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
        device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
        device->SetRenderState(D3DRS_FOGENABLE, FALSE);
        device->SetRenderState(D3DRS_CLIPPLANEENABLE, 0);
        device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        device->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
        device->SetRenderState(D3DRS_COLORWRITEENABLE,
                               D3DCOLORWRITEENABLE_RED |
                                   D3DCOLORWRITEENABLE_GREEN |
                                   D3DCOLORWRITEENABLE_BLUE |
                                   D3DCOLORWRITEENABLE_ALPHA);
        for (DWORD sampler = 0; sampler < 4; ++sampler) {
            device->SetSamplerState(
                sampler, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
            device->SetSamplerState(
                sampler, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
            device->SetSamplerState(
                sampler, D3DSAMP_SRGBTEXTURE, FALSE);
            device->SetSamplerState(
                sampler, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
        }
        return true;
    }

    static void SetPointFilter(IDirect3DDevice9* device,
                               DWORD sampler) noexcept {
        device->SetSamplerState(sampler, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
        device->SetSamplerState(sampler, D3DSAMP_MINFILTER, D3DTEXF_POINT);
    }

    static void SetLinearFilter(IDirect3DDevice9* device,
                                DWORD sampler) noexcept {
        device->SetSamplerState(sampler, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
        device->SetSamplerState(sampler, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    }

    static bool DrawFullscreenQuad(IDirect3DDevice9* device, UINT width,
                                   UINT height) noexcept {
        // Die halbe Texelverschiebung ist die D3D9-Regel für 1:1-Abbildung
        // zwischen Pixelmitte und Texelmitte.
        const float right = static_cast<float>(width) - 0.5F;
        const float bottom = static_cast<float>(height) - 0.5F;
        const HudQuadVertex vertices[4] = {
            {-0.5F, -0.5F, 0.0F, 1.0F, 0.0F, 0.0F},
            {right, -0.5F, 0.0F, 1.0F, 1.0F, 0.0F},
            {right, bottom, 0.0F, 1.0F, 1.0F, 1.0F},
            {-0.5F, bottom, 0.0F, 1.0F, 0.0F, 1.0F}};
        return SUCCEEDED(device->DrawPrimitiveUP(
            D3DPT_TRIANGLEFAN, 2, vertices, sizeof(HudQuadVertex)));
    }

    bool RenderMaskAndReduce(IDirect3DDevice9* device,
                             IDirect3DTexture9* presented,
                             IDirect3DTexture9* rightWorld) noexcept {
        const float threshold[4] = {kHudPixelThreshold, 0.0F, 0.0F, 0.0F};
        if (FAILED(device->SetRenderTarget(0, mask_.surface.Get())) ||
            FAILED(device->SetDepthStencilSurface(nullptr)) ||
            FAILED(device->SetPixelShader(maskShader_.Get())) ||
            FAILED(device->SetPixelShaderConstantF(0, threshold, 1)) ||
            FAILED(device->SetTexture(0, presented)) ||
            FAILED(device->SetTexture(1, rightWorld))) {
            return false;
        }
        SetPointFilter(device, 0);
        SetPointFilter(device, 1);
        if (!DrawFullscreenQuad(device, mask_.width, mask_.height)) {
            return false;
        }

        const RenderTargetTexture* source = &mask_;
        if (FAILED(device->SetPixelShader(reduceShader_.Get())) ||
            FAILED(device->SetTexture(1, nullptr))) {
            return false;
        }
        SetLinearFilter(device, 0);
        for (std::uint32_t level = 0; level < reduceLevelCount_; ++level) {
            const RenderTargetTexture& target = reduce_[level];
            const float texel[4] = {
                1.0F / static_cast<float>(source->width),
                1.0F / static_cast<float>(source->height), 0.0F, 0.0F};
            if (FAILED(device->SetRenderTarget(0, target.surface.Get())) ||
                FAILED(device->SetTexture(0, source->texture.Get())) ||
                FAILED(device->SetPixelShaderConstantF(0, texel, 1)) ||
                !DrawFullscreenQuad(device, target.width, target.height)) {
                return false;
            }
            source = &target;
        }
        // Gelesen wird erst nach EndScene: GetRenderTargetData gehört nicht
        // zwischen BeginScene und EndScene.
        coverageSource_ = source->surface.Get();
        coverageTexture_ = source->texture.Get();
        return true;
    }

    bool RenderComposite(IDirect3DDevice9* device,
                         IDirect3DTexture9* presented,
                         IDirect3DTexture9* rightWorld,
                         IDirect3DTexture9* eyeWorld,
                         const RenderTargetTexture& target,
                         bool compositeEnabled, bool flatPanel) noexcept {
        const float params[4] = {
            kHudPixelThreshold, compositeEnabled ? 1.0F : 0.0F,
            flatPanel ? 1.0F : 0.0F, kHudCoverageThreshold};
        const float size[4] = {
            static_cast<float>(width_), static_cast<float>(height_),
            1.0F / static_cast<float>(width_),
            1.0F / static_cast<float>(height_)};
        const float shrink[4] = {
            static_cast<float>(kStereoHudShrinkNumerator) /
                static_cast<float>(kStereoHudShrinkDenominator),
            0.0F, 0.0F, 0.0F};
        if (FAILED(device->SetRenderTarget(0, target.surface.Get())) ||
            FAILED(device->SetPixelShader(compositeShader_.Get())) ||
            FAILED(device->SetPixelShaderConstantF(0, params, 1)) ||
            FAILED(device->SetPixelShaderConstantF(1, size, 1)) ||
            FAILED(device->SetPixelShaderConstantF(2, shrink, 1)) ||
            FAILED(device->SetTexture(0, presented)) ||
            FAILED(device->SetTexture(1, rightWorld)) ||
            FAILED(device->SetTexture(2, eyeWorld)) ||
            FAILED(device->SetTexture(3, coverageTexture_))) {
            return false;
        }
        SetPointFilter(device, 0);
        SetPointFilter(device, 1);
        SetPointFilter(device, 2);
        SetPointFilter(device, 3);
        return DrawFullscreenQuad(device, target.width, target.height);
    }

    void ReadPreviousCoverage(Logger& logger) noexcept {
        if (!coveragePending_) {
            return;
        }
        coveragePending_ = false;
        D3DLOCKED_RECT locked{};
        const HRESULT result = coverageReadback_->LockRect(
            &locked, nullptr, D3DLOCK_READONLY);
        if (FAILED(result)) {
            LogHresult(logger, "stereo_hud_coverage_lock_failed", result);
            return;
        }
        std::uint64_t total = 0;
        for (UINT row = 0; row < coverageHeight_; ++row) {
            const auto* pixels = reinterpret_cast<const std::uint32_t*>(
                static_cast<const std::uint8_t*>(locked.pBits) +
                static_cast<std::size_t>(row) *
                    static_cast<std::size_t>(locked.Pitch));
            for (UINT column = 0; column < coverageWidth_; ++column) {
                total += pixels[column] & 0xffu;
            }
        }
        coverageReadback_->UnlockRect();
        const std::uint64_t maximum =
            static_cast<std::uint64_t>(coverageWidth_) * coverageHeight_ *
            255ull;
        coverageRatio_ = maximum == 0
            ? 0.0
            : static_cast<double>(total) / static_cast<double>(maximum);
    }

    // 2 von 255 Stufen, wie IsPostWorldPixel: der Vergleich dort ist echt
    // größer, deshalb liegt die Schwelle hier zwischen 2 und 3.
    static constexpr float kHudPixelThreshold = 2.5F / 255.0F;
    static constexpr float kHudCoverageThreshold = 3.0F / 100.0F;

    D3DCompileFunction compile_{nullptr};
    ComPtr<IDirect3DPixelShader9> maskShader_;
    ComPtr<IDirect3DPixelShader9> reduceShader_;
    ComPtr<IDirect3DPixelShader9> compositeShader_;
    ComPtr<IDirect3DStateBlock9> stateBlock_;
    ComPtr<IDirect3DSurface9> coverageReadback_;
    std::array<RenderTargetTexture, FEARVR_EYE_COUNT> composite_{};
    RenderTargetTexture mask_{};
    std::array<RenderTargetTexture, kHudMaxReduceLevels> reduce_{};
    IDirect3DSurface9* coverageSource_{nullptr};
    IDirect3DTexture9* coverageTexture_{nullptr};
    std::uint32_t reduceLevelCount_{0};
    UINT width_{0};
    UINT height_{0};
    UINT coverageWidth_{0};
    UINT coverageHeight_{0};
    double coverageRatio_{1.0};
    bool coveragePending_{false};
    bool ready_{false};
};

class Bridge {
public:
    Bridge() : config_(ReadConfig()) {
        if (config_.sessionId != 0) {
            if (config_.logDirectory.empty()) {
                wchar_t temporary[MAX_PATH]{};
                if (GetTempPathW(MAX_PATH, temporary) != 0) {
                    config_.logDirectory =
                        std::filesystem::path(temporary) / "FearVr";
                }
            }
            logger_.Open(config_.logDirectory);
            std::ostringstream message;
            message << "version=" << FEARVR_VERSION_STRING
                    << " git=" << FEARVR_GIT_HASH
                    << " pid=" << GetCurrentProcessId()
                    << " session=0x" << std::hex << std::uppercase
                    << config_.sessionId;
            logger_.Write("INFO", "proxy_start", message.str());
            std::ostringstream d3d9Message;
            d3d9Message
                << "system="
                << std::filesystem::path(SystemD3D9Path()).u8string()
                << " upstream="
                << std::filesystem::path(UpstreamD3D9Path()).u8string()
                << " upstream_status="
                << (UpstreamD3D9Loaded()
                        ? "loaded"
                        : (UpstreamD3D9Present() ? "load_failed"
                                                : "absent"))
                << " upstream_error=" << UpstreamD3D9LoadError();
            logger_.Write(
                UpstreamD3D9Present() && !UpstreamD3D9Loaded()
                    ? "WARN"
                    : "INFO",
                "d3d9_chain", d3d9Message.str());
            if (config_.stereoToggleAllowed) {
                logger_.Write(
                    "INFO", "stereo_toggle_ready",
                    "Stereo starts disabled; press F8 in the 3D world. "
                    "Press F9 to recenter head tracking.");
            }
        }
    }

    ~Bridge() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            completionWorkerStop_ = true;
        }
        completionWorkerWake_.notify_all();
        if (completionWorker_.joinable()) {
            completionWorker_.join();
        }
        ReleaseResources();
        if (shared_ != nullptr) {
            UnmapViewOfFile(shared_);
        }
        if (mapping_ != nullptr) {
            CloseHandle(mapping_);
        }
        if (frameReadyEvent_ != nullptr) {
            CloseHandle(frameReadyEvent_);
        }
        if (slotConsumedEvent_ != nullptr) {
            CloseHandle(slotConsumedEvent_);
        }
    }

    void LogHookStatus(const char* level, const char* event,
                       const std::string& message) noexcept {
        logger_.Write(level, event, message);
    }

    void NoteMultithreadedDevice(IDirect3DDevice9* device) noexcept {
        if (device == nullptr) {
            return;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        multithreadedDevice_ = device;
        if (!multithreadedDeviceLogged_) {
            logger_.Write(
                "INFO", "game_device_multithreaded",
                "Retail D3D9 device was created with "
                "D3DCREATE_MULTITHREADED; asynchronous readback is safe.");
            multithreadedDeviceLogged_ = true;
        }
    }

    void CapturePresent(IDirect3DDevice9* device) noexcept {
        if (device == nullptr || config_.sessionId == 0) {
            return;
        }
        PollStereoToggle();
        std::lock_guard<std::mutex> lock(mutex_);
        if (!EnsureIpc()) {
            return;
        }

        InterlockedIncrement64(Atomic64(shared_->gameHeartbeat));
        PollPending();
        UpdateHostConnection();
        EnsureDeviceMetadata(device);
        UpdateAdapterMatch();

        if (!hostConnected_ ||
            (shared_->bridgeFlags & FEARVR_BF_ADAPTER_MATCH) == 0) {
            return;
        }

        ComPtr<IDirect3DSurface9> backBuffer;
        HRESULT result = device->GetBackBuffer(
            0, 0, D3DBACKBUFFER_TYPE_MONO,
            backBuffer.ReleaseAndGetAddressOf());
        if (FAILED(result)) {
            if (result == D3DERR_DEVICELOST) {
                InterlockedOr(AtomicFlags(*shared_), FEARVR_BF_DEVICE_LOST);
            }
            LogHresult("get_backbuffer_failed", result);
            return;
        }

        D3DSURFACE_DESC description{};
        result = backBuffer->GetDesc(&description);
        if (FAILED(result)) {
            LogHresult("get_backbuffer_desc_failed", result);
            return;
        }
        if (!EnsureResources(device, description.Width, description.Height)) {
            return;
        }
        if (pending_.active) {
            return;
        }

        std::uint32_t slotIndex = 0;
        if (!ClaimWritablePair(slotIndex)) {
            ++droppedFrames_;
            if (droppedFrames_ == 1 || droppedFrames_ % 30000 == 0) {
                logger_.Write(
                    "WARN", "ring_full",
                    "dropped=" + std::to_string(droppedFrames_));
            }
            return;
        }

        const bool stereo =
            stereoFrameReady_ &&
            stereoEyeCaptured_[FEARVR_EYE_LEFT] &&
            stereoEyeCaptured_[FEARVR_EYE_RIGHT];
        const std::uint64_t frameId =
            stereo ? stereoFrameId_ : ++frameId_;
        if (stereo && frameId > frameId_) {
            frameId_ = frameId;
        }
        const std::uint64_t generation = ++generation_;
        for (std::uint32_t eye = 0; eye < FEARVR_EYE_COUNT; ++eye) {
            FearVrSlot& slot = shared_->slot[eye][slotIndex];
            slot.frameId = frameId;
            slot.generation = generation;
            slot.camera = stereo
                ? stereoCameraSample_
                : FearVrGameCameraSample{};
        }

        if (asyncTransferEnabled_ &&
            config_.stereoHudEnabled &&
            !hudCompositor_.ready()) {
            // Compose can fail closed at runtime. Drain the worker before the
            // synchronous fallback reuses its upload surface.
            ReleaseAsyncTransferResources();
        }
        const bool asyncQueuedPath =
            asyncTransferEnabled_ &&
            transferMode_ == TransferMode::CpuViaD3D9Ex;
        const auto transferStart =
            std::chrono::steady_clock::now();
        bool copied = false;
        stereoHudFlatFrame_ = false;
        if (asyncQueuedPath) {
            copied = QueueFrameAsync(
                device, backBuffer.Get(), slotIndex, frameId,
                generation, stereo);
        } else if (stereo) {
            copied = transferMode_ == TransferMode::CpuViaD3D9Ex
                ? CopyStereoFrameViaCpu(
                      device, backBuffer.Get(), slotIndex)
                : CopyStereoFrameDirect(device, slotIndex);
        } else {
            copied = transferMode_ == TransferMode::CpuViaD3D9Ex
                ? CopyFrameViaCpu(device, backBuffer.Get(), slotIndex)
                : CopyFrameDirect(device, backBuffer.Get(), slotIndex);
        }

        if (!copied) {
            ReleaseClaimedPair(slotIndex);
            if (stereo) {
                ClearStereoFrame();
            }
            return;
        }
        const std::uint64_t transferMicroseconds =
            static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - transferStart)
                    .count());
        ++transferSamples_;
        transferTotalMicroseconds_ += transferMicroseconds;
        transferMaxMicroseconds_ =
            (std::max)(transferMaxMicroseconds_, transferMicroseconds);
        if (stereo) {
            if (stereoHudFlatFrame_) {
                InterlockedAnd(
                    AtomicFlags(*shared_),
                    static_cast<LONG>(~FEARVR_BF_STEREO_ACTIVE));
            } else {
                InterlockedOr(
                    AtomicFlags(*shared_), FEARVR_BF_STEREO_ACTIVE);
            }
            ++stereoFrames_;
            if (stereoFrames_ == 1 || stereoFrames_ % 300 == 0) {
                logger_.Write(
                    "INFO", "stereo_frame_staged",
                    "request_frame=" + std::to_string(frameId) +
                        " stereo_frames=" +
                        std::to_string(stereoFrames_) +
                        (asyncQueuedPath
                             ? " enqueue_avg_us="
                             : " transfer_avg_us=") +
                        std::to_string(
                            transferSamples_ == 0
                                ? 0
                                : transferTotalMicroseconds_ /
                                      transferSamples_) +
                        " transfer_max_us=" +
                        std::to_string(transferMaxMicroseconds_));
                transferSamples_ = 0;
                transferTotalMicroseconds_ = 0;
                transferMaxMicroseconds_ = 0;
            }
            ClearStereoFrame();
        } else {
            InterlockedAnd(
                AtomicFlags(*shared_),
                static_cast<LONG>(~FEARVR_BF_STEREO_ACTIVE));
        }

        if (asyncQueuedPath) {
            return;
        }
        pending_.active = true;
        pending_.slotIndex = slotIndex;
        pending_.frameId = frameId;
        pending_.generation = generation;

        // UpdateSurface/query completion used to spin here for up to 3 ms.
        // Present is FEAR's main render thread, so an occasional late query
        // directly became a visible world/body hitch. A multithread-safe
        // D3D9Ex helper now completes the same query off-thread and publishes
        // the slot as soon as it is ready. Unlike simply deferring PollPending
        // to the next Present, this does not add an entire game-frame of
        // image latency.
        if (transferMode_ == TransferMode::CpuViaD3D9Ex &&
            EnsureCompletionWorker()) {
            completionWorkerWake_.notify_one();
        } else if (transferMode_ == TransferMode::CpuViaD3D9Ex) {
            // Thread creation can fail under extreme resource pressure.
            // Keep the bridge fail-open and at least publish immediately when
            // the driver has already completed the upload.
            PollPending();
        } else {
            // A native D3D9Ex application owns this query on its own device,
            // which is not guaranteed to have been created multithread-safe.
            // Keep polling it on the application's render thread.
            const ULONGLONG deadline = GetTickCount64() + 3;
            do {
                if (PollPending()) {
                    break;
                }
                SwitchToThread();
            } while (GetTickCount64() < deadline);
        }
    }

    void BeforeReset(
        const D3DPRESENT_PARAMETERS* parameters) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        std::ostringstream message;
        message << "D3DPOOL_DEFAULT bridge resources released.";
        if (parameters != nullptr) {
            message << " requested="
                    << parameters->BackBufferWidth << 'x'
                    << parameters->BackBufferHeight
                    << " format="
                    << static_cast<unsigned int>(
                           parameters->BackBufferFormat)
                    << " windowed="
                    << (parameters->Windowed ? 1 : 0)
                    << " refresh="
                    << parameters->FullScreen_RefreshRateInHz;
        } else {
            message << " requested=<null>";
        }
        logger_.Write(
            "INFO", "device_reset_begin", message.str());
        ReleaseResources();
        if (shared_ != nullptr) {
            InterlockedOr(AtomicFlags(*shared_), FEARVR_BF_DEVICE_LOST);
        }
        device_ = nullptr;
        deviceMetadataReady_ = false;
    }

    void AfterReset(
        IDirect3DDevice9* device, HRESULT result) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        if (shared_ == nullptr) {
            return;
        }
        if (SUCCEEDED(result)) {
            InterlockedAnd(
                AtomicFlags(*shared_),
                static_cast<LONG>(~FEARVR_BF_DEVICE_LOST));
            std::ostringstream message;
            message << "Reset successful";
            ComPtr<IDirect3DSurface9> backBuffer;
            D3DSURFACE_DESC description{};
            if (device != nullptr &&
                SUCCEEDED(device->GetBackBuffer(
                    0, 0, D3DBACKBUFFER_TYPE_MONO,
                    backBuffer.ReleaseAndGetAddressOf())) &&
                backBuffer &&
                SUCCEEDED(backBuffer->GetDesc(&description))) {
                message << "; active="
                        << description.Width << 'x'
                        << description.Height
                        << " format="
                        << static_cast<unsigned int>(
                               description.Format);
            } else {
                message << "; active=<unavailable>";
            }
            message << "; resources will be recreated.";
            logger_.Write(
                "INFO", "device_reset_complete", message.str());
        } else {
            LogHresult("device_reset_failed", result);
        }
    }

    BOOL IsConnected() noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!EnsureIpc()) {
            return FALSE;
        }
        UpdateHostConnection();
        return hostConnected_ ? TRUE : FALSE;
    }

    BOOL StereoEnabled() noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return config_.stereoEnabled ? TRUE : FALSE;
    }

    void SetStereoEnabled(BOOL enabled) noexcept {
        StereoToggleCallback callback = nullptr;
        bool changed = false;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            const bool requested = enabled != FALSE;
            if (config_.stereoEnabled != requested) {
                config_.stereoEnabled = requested;
                ClearStereoFrame();
                if (shared_ != nullptr && !requested) {
                    InterlockedAnd(
                        AtomicFlags(*shared_),
                        static_cast<LONG>(
                            ~FEARVR_BF_STEREO_ACTIVE));
                }
                callback = stereoToggleCallback_;
                changed = true;
                logger_.Write(
                    "INFO", "stereo_set",
                    requested
                        ? "Native stereo enabled automatically after loading."
                        : "Native stereo disabled programmatically.");
            }
        }
        if (changed && callback != nullptr) {
            callback(enabled != FALSE ? TRUE : FALSE);
        }
    }

    void SetFovScalePercent(std::uint32_t percent) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        const std::uint32_t requested =
            NormalizeFovScalePercent(percent);
        if (fovScalePercent_ == requested) {
            return;
        }
        fovScalePercent_ = requested;
        if (shared_ != nullptr) {
            InterlockedExchange(
                Atomic32(shared_->fovScalePercent),
                static_cast<LONG>(fovScalePercent_));
        }
        logger_.Write(
            "INFO", "fov_scale_set",
            "Stereo FOV scale set to " +
                std::to_string(fovScalePercent_) + "%.");
    }

    BOOL TranslationEnabled() noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return config_.translationEnabled ? TRUE : FALSE;
    }

    void SetTranslationEnabled(BOOL enabled) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        config_.translationEnabled = enabled != FALSE;
        logger_.Write(
            "INFO", "translation_set",
            config_.translationEnabled
                ? "Bounded HMD translation enabled from the VR menu."
                : "HMD translation disabled from the VR menu.");
    }

    BOOL StereoHudEnabled() noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return config_.stereoHudEnabled ? TRUE : FALSE;
    }

    void SetStereoHudEnabled(BOOL enabled) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        config_.stereoHudEnabled = enabled != FALSE;
        ClearStereoFrame();
        logger_.Write(
            "INFO", "stereo_hud_set",
            config_.stereoHudEnabled
                ? "Stereo HUD enabled from the VR menu."
                : "Stereo HUD disabled from the VR menu.");
    }

    BOOL ComfortModeEnabled() noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return comfortModeEnabled_ ? TRUE : FALSE;
    }

    void SetComfortModeEnabled(BOOL enabled) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        const bool requested = enabled != FALSE;
        if (comfortModeEnabled_ == requested) {
            return;
        }
        comfortModeEnabled_ = requested;
        ClearStereoFrame();
        if (shared_ != nullptr) {
            InterlockedAnd(
                AtomicFlags(*shared_),
                static_cast<LONG>(~FEARVR_BF_STEREO_ACTIVE));
        }
        if (!comfortModeEnabled_) {
            IncrementRecenterGeneration();
        }
        logger_.Write(
            "INFO", "comfort_mode_set",
            comfortModeEnabled_
                ? "World-locked comfort panel enabled from the VR menu."
                : "Comfort panel disabled from the VR menu; stereo resumes.");
    }

    void SetMenuActive(BOOL active) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        const bool requested = active != FALSE;
        if (menuActive_ == requested) {
            return;
        }
        menuActive_ = requested;
        ClearStereoFrame();
        if (shared_ != nullptr) {
            InterlockedAnd(
                AtomicFlags(*shared_),
                static_cast<LONG>(~FEARVR_BF_STEREO_ACTIVE));
        }
        logger_.Write(
            "INFO", "menu_render_mode",
            menuActive_
                ? "Pause/menu detected; native stereo disabled immediately."
                : "Gameplay detected; native stereo may resume on the next frame.");
    }

    void RequestRecenter() noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        IncrementPanelRecenterGeneration();
        logger_.Write(
            "INFO", "panel_recenter_requested",
            "The in-game VR menu requested a new 2D panel anchor.");
    }

    BOOL StereoAvailable() const noexcept {
        return config_.stereoEnabled ||
               config_.stereoToggleAllowed ? TRUE : FALSE;
    }

    BOOL FlatPanelActive() noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return stereoHudFlatFrame_ ? TRUE : FALSE;
    }

    void RegisterStereoToggle(
        StereoToggleCallback callback) noexcept {
        BOOL enableNow = FALSE;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stereoToggleCallback_ = callback;
            enableNow = config_.stereoEnabled ? TRUE : FALSE;
        }
        if (callback != nullptr && enableNow) {
            callback(TRUE);
        }
    }

    BOOL ReadRenderRequest(FearVrRenderRequest* output) noexcept {
        if (output == nullptr) {
            return FALSE;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        if (!EnsureIpc()) {
            return FALSE;
        }
        for (int attempt = 0; attempt < 4; ++attempt) {
            const std::uint64_t before =
                ReadAtomic64(shared_->requestSequence);
            if ((before & 1ULL) != 0 || before == 0) {
                continue;
            }
            FearVrRenderRequest snapshot = shared_->request;
            MemoryBarrier();
            const std::uint64_t after =
                ReadAtomic64(shared_->requestSequence);
            if (before == after && (after & 1ULL) == 0 &&
                (snapshot.flags & FEARVR_RF_VALID) != 0) {
                snapshot.recenterGeneration = recenterGeneration_;
                if (config_.translationEnabled) {
                    snapshot.flags |= FEARVR_RF_TRANSLATION_ON;
                }
                if (comfortModeEnabled_) {
                    snapshot.flags |= FEARVR_RF_FLATSCREEN;
                }
                if (menuActive_) {
                    snapshot.flags |= FEARVR_RF_FLATSCREEN;
                }
                *output = snapshot;
                return TRUE;
            }
        }
        return FALSE;
    }

    BOOL ReadInputState(FearVrInputState* output) noexcept {
        if (output == nullptr) {
            return FALSE;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        if (!EnsureIpc()) {
            return FALSE;
        }
        for (int attempt = 0; attempt < 4; ++attempt) {
            const std::uint64_t before =
                ReadAtomic64(shared_->inputSequence);
            if ((before & 1ULL) != 0 || before == 0) {
                continue;
            }
            const FearVrInputState snapshot = shared_->input;
            MemoryBarrier();
            const std::uint64_t after =
                ReadAtomic64(shared_->inputSequence);
            if (before == after && (after & 1ULL) == 0 &&
                (snapshot.flags & FEARVR_IF_VALID) != 0) {
                *output = snapshot;
                return TRUE;
            }
        }
        return FALSE;
    }

    BOOL WriteHapticRequest(
        const FearVrHapticRequest* request) noexcept {
        if (request == nullptr ||
            (request->flags & FEARVR_HF_VALID) == 0 ||
            request->requestId == 0) {
            return FALSE;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        if (!EnsureIpc() || !hostConnected_) {
            return FALSE;
        }
        InterlockedIncrement64(Atomic64(shared_->hapticSequence));
        MemoryBarrier();
        shared_->haptic = *request;
        MemoryBarrier();
        InterlockedIncrement64(Atomic64(shared_->hapticSequence));
        return TRUE;
    }

    void BeginStereoEye(std::uint32_t eye) noexcept {
        if (eye >= FEARVR_EYE_COUNT) {
            return;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        if (eye == FEARVR_EYE_LEFT) {
            if (stereoFrameReady_) {
                stereoAccepting_ = false;
                return;
            }
            stereoEyeCaptured_.fill(false);
            stereoFrameId_ = 0;
            stereoAccepting_ = true;
        }
    }

    void CaptureStereoEye(std::uint32_t eye) noexcept {
        if (eye >= FEARVR_EYE_COUNT || config_.sessionId == 0) {
            return;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        if (!stereoAccepting_ || device_ == nullptr ||
            !resourcesReady_ || !stereoCapture_[eye]) {
            return;
        }

        ComPtr<IDirect3DSurface9> backBuffer;
        HRESULT result = device_->GetBackBuffer(
            0, 0, D3DBACKBUFFER_TYPE_MONO,
            backBuffer.ReleaseAndGetAddressOf());
        if (FAILED(result)) {
            LogHresult("stereo_get_backbuffer_failed", result);
            return;
        }
        D3DSURFACE_DESC description{};
        result = backBuffer->GetDesc(&description);
        if (FAILED(result) ||
            description.Width != sourceWidth_ ||
            description.Height != sourceHeight_) {
            logger_.Write(
                "WARN", "stereo_capture_size_changed",
                "Stereo capture deferred until resources are recreated.");
            return;
        }
        result = device_->StretchRect(
            backBuffer.Get(), nullptr, stereoCapture_[eye].Get(), nullptr,
            D3DTEXF_NONE);
        if (FAILED(result)) {
            LogHresult("stereo_stage_copy_failed", result);
            return;
        }
        stereoEyeCaptured_[eye] = true;
    }

    void EndStereoFrame(
        std::uint64_t frameId,
        const FearVrGameCameraSample* camera) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!stereoAccepting_) {
            return;
        }
        stereoAccepting_ = false;
        if (frameId == 0 ||
            !stereoEyeCaptured_[FEARVR_EYE_LEFT] ||
            !stereoEyeCaptured_[FEARVR_EYE_RIGHT]) {
            if (!stereoIncompleteLogged_) {
                logger_.Write(
                    "WARN", "stereo_frame_incomplete",
                    "Both eye captures are required; mono fallback remains active.");
                stereoIncompleteLogged_ = true;
            }
            stereoEyeCaptured_.fill(false);
            stereoFrameId_ = 0;
            return;
        }
        stereoFrameId_ = frameId;
        stereoCameraSample_ = {};
        if (camera != nullptr && camera->frameId == frameId &&
            IsValidGameCameraSample(*camera)) {
            stereoCameraSample_ = *camera;
            if (shared_ != nullptr) {
                InterlockedIncrement64(
                    Atomic64(shared_->cameraSequence));
                MemoryBarrier();
                shared_->latestCamera = stereoCameraSample_;
                MemoryBarrier();
                InterlockedIncrement64(
                    Atomic64(shared_->cameraSequence));
            }
        }
        stereoFrameReady_ = true;
    }

private:
    struct PendingFrame {
        bool active{false};
        std::uint32_t slotIndex{0};
        std::uint64_t frameId{0};
        std::uint64_t generation{0};
    };

    enum class AsyncJobState : std::uint8_t {
        Free,
        Preparing,
        Queued,
        Processing,
        Discarded
    };

    struct AsyncTransferJob {
        AsyncJobState state{AsyncJobState::Free};
        std::array<ComPtr<IDirect3DTexture9>, FEARVR_EYE_COUNT>
            captureTexture{};
        std::array<ComPtr<IDirect3DSurface9>, FEARVR_EYE_COUNT>
            capture{};
        std::array<ComPtr<IDirect3DSurface9>, FEARVR_EYE_COUNT>
            readback{};
        ComPtr<IDirect3DQuery9> captureComplete;
        std::uint32_t slotIndex{0};
        std::uint64_t frameId{0};
        std::uint64_t generation{0};
        std::chrono::steady_clock::time_point queuedAt{};
    };

    void ClearStereoFrame() noexcept {
        stereoEyeCaptured_.fill(false);
        stereoFrameId_ = 0;
        stereoCameraSample_ = {};
        stereoFrameReady_ = false;
        stereoAccepting_ = false;
    }

    void IncrementRecenterGeneration() noexcept {
        ++recenterGeneration_;
        if (recenterGeneration_ == 0) {
            ++recenterGeneration_;
        }
    }

    void IncrementPanelRecenterGeneration() noexcept {
        if (shared_ == nullptr) {
            return;
        }
        LONG generation = InterlockedIncrement(
            Atomic32(shared_->panelRecenterGeneration));
        if (generation == 0) {
            InterlockedIncrement(
                Atomic32(shared_->panelRecenterGeneration));
        }
    }

    void PollStereoToggle() noexcept {
        if (!config_.stereoToggleAllowed) {
            return;
        }
        StereoToggleCallback callback = nullptr;
        BOOL enabled = FALSE;
        bool changed = false;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            const bool keyDown =
                (GetAsyncKeyState(VK_F8) & 0x8000) != 0;
            if (keyDown && !stereoKeyWasDown_) {
                config_.stereoEnabled = !config_.stereoEnabled;
                ClearStereoFrame();
                if (shared_ != nullptr &&
                    !config_.stereoEnabled) {
                    InterlockedAnd(
                        AtomicFlags(*shared_),
                        static_cast<LONG>(
                            ~FEARVR_BF_STEREO_ACTIVE));
                }
                callback = stereoToggleCallback_;
                enabled = config_.stereoEnabled ? TRUE : FALSE;
                changed = true;
                logger_.Write(
                    "INFO", "stereo_toggle",
                    config_.stereoEnabled
                        ? "Native stereo enabled with F8."
                        : "Native stereo disabled with F8; mono fallback active.");
            }
            stereoKeyWasDown_ = keyDown;

            const bool recenterKeyDown =
                (GetAsyncKeyState(VK_F9) & 0x8000) != 0;
            if (recenterKeyDown && !recenterKeyWasDown_) {
                const bool flatView =
                    menuActive_ || comfortModeEnabled_ ||
                    !config_.stereoEnabled;
                if (flatView) {
                    IncrementPanelRecenterGeneration();
                    logger_.Write(
                        "INFO", "panel_recenter_requested",
                        "F9 requested a new 2D panel anchor.");
                } else {
                    logger_.Write(
                        "INFO", "world_recenter_ignored",
                        "F9 has no recenter function in the 3D world.");
                }
            }
            recenterKeyWasDown_ = recenterKeyDown;

            const bool comfortKeyDown =
                (GetAsyncKeyState(VK_F10) & 0x8000) != 0;
            if (comfortKeyDown && !comfortKeyWasDown_) {
                comfortModeEnabled_ = !comfortModeEnabled_;
                ClearStereoFrame();
                if (shared_ != nullptr) {
                    InterlockedAnd(
                        AtomicFlags(*shared_),
                        static_cast<LONG>(
                            ~FEARVR_BF_STEREO_ACTIVE));
                }
                if (!comfortModeEnabled_) {
                    IncrementRecenterGeneration();
                }
                logger_.Write(
                    "INFO", "comfort_mode_toggle",
                    comfortModeEnabled_
                        ? "World-locked comfort panel enabled with F10."
                        : "Comfort panel disabled with F10; stereo resumes.");
            }
            comfortKeyWasDown_ = comfortKeyDown;
        }
        if (changed && callback != nullptr) {
            callback(enabled);
        }
    }

    bool EnsureIpc() noexcept {
        if (shared_ != nullptr) {
            return true;
        }

        const std::wstring mappingName =
            MakeIpcObjectName(config_.sessionId, L"Mapping");
        mapping_ = CreateFileMappingW(
            INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
            static_cast<DWORD>(sizeof(FearVrSharedHeader)),
            mappingName.c_str());
        if (mapping_ == nullptr) {
            LogWin32("mapping_create_failed", GetLastError());
            return false;
        }
        const bool existed = GetLastError() == ERROR_ALREADY_EXISTS;
        shared_ = static_cast<FearVrSharedHeader*>(
            MapViewOfFile(mapping_, FILE_MAP_ALL_ACCESS, 0, 0,
                          sizeof(FearVrSharedHeader)));
        if (shared_ == nullptr) {
            LogWin32("mapping_view_failed", GetLastError());
            CloseHandle(mapping_);
            mapping_ = nullptr;
            return false;
        }
        if (existed) {
            if (!IsProtocolHeaderValid(*shared_)) {
                logger_.Write("ERROR", "protocol_mismatch",
                              "Existing mapping has incompatible header.");
                InterlockedOr(AtomicFlags(*shared_),
                              FEARVR_BF_PROTOCOL_ERROR);
                return false;
            }
        } else {
            InitializeProtocolHeader(*shared_);
        }

        const std::wstring frameReadyName =
            MakeIpcObjectName(config_.sessionId, L"FrameReady");
        const std::wstring consumedName =
            MakeIpcObjectName(config_.sessionId, L"SlotConsumed");
        frameReadyEvent_ =
            CreateEventW(nullptr, FALSE, FALSE, frameReadyName.c_str());
        slotConsumedEvent_ =
            CreateEventW(nullptr, FALSE, FALSE, consumedName.c_str());
        if (frameReadyEvent_ == nullptr || slotConsumedEvent_ == nullptr) {
            LogWin32("event_create_failed", GetLastError());
            return false;
        }

        shared_->gameProcessId = GetCurrentProcessId();
        InterlockedExchange(
            Atomic32(shared_->fovScalePercent),
            static_cast<LONG>(fovScalePercent_));
        InterlockedOr(AtomicFlags(*shared_), FEARVR_BF_GAME_READY);
        logger_.Write("INFO", "ipc_created",
                      "Named mapping and bounded ring ready.");
        return true;
    }

    void EnsureDeviceMetadata(IDirect3DDevice9* device) noexcept {
        if (deviceMetadataReady_ && device_ == device) {
            return;
        }
        if (device_ != device) {
            ReleaseResources();
            device_ = device;
        }

        D3DDEVICE_CREATION_PARAMETERS creation{};
        if (FAILED(device->GetCreationParameters(&creation))) {
            return;
        }
        ComPtr<IDirect3D9> direct3D;
        if (FAILED(device->GetDirect3D(
                direct3D.ReleaseAndGetAddressOf()))) {
            return;
        }
        bool found = false;
        ComPtr<IDirect3D9Ex> direct3DEx;
        if (SUCCEEDED(direct3D.As(&direct3DEx))) {
            LUID adapterLuid{};
            if (SUCCEEDED(direct3DEx->GetAdapterLUID(
                    creation.AdapterOrdinal, &adapterLuid))) {
                gameAdapterLuid_ = PackLuid(
                    static_cast<std::uint32_t>(adapterLuid.HighPart),
                    adapterLuid.LowPart);
                found = true;
            }
        }

        // Auf Hybrid-GPUs kann der D3D9-Renderadapter keine eigene Ausgabe
        // besitzen. Geräte-IDs sind dort zuverlässiger als der Monitor.
        if (!found) {
            D3DADAPTER_IDENTIFIER9 d3d9Identifier{};
            if (SUCCEEDED(direct3D->GetAdapterIdentifier(
                    creation.AdapterOrdinal, 0, &d3d9Identifier))) {
                ComPtr<IDXGIFactory1> factory;
                if (SUCCEEDED(CreateDXGIFactory1(
                        IID_PPV_ARGS(
                            factory.ReleaseAndGetAddressOf())))) {
                    for (UINT adapterIndex = 0;; ++adapterIndex) {
                        ComPtr<IDXGIAdapter1> adapter;
                        if (factory->EnumAdapters1(
                                adapterIndex,
                                adapter.ReleaseAndGetAddressOf()) ==
                            DXGI_ERROR_NOT_FOUND) {
                            break;
                        }
                        DXGI_ADAPTER_DESC1 description{};
                        if (SUCCEEDED(adapter->GetDesc1(&description)) &&
                            description.VendorId ==
                                d3d9Identifier.VendorId &&
                            description.DeviceId ==
                                d3d9Identifier.DeviceId &&
                            description.SubSysId ==
                                d3d9Identifier.SubSysId &&
                            description.Revision ==
                                d3d9Identifier.Revision) {
                            gameAdapterLuid_ = PackLuid(
                                static_cast<std::uint32_t>(
                                    description.AdapterLuid.HighPart),
                                description.AdapterLuid.LowPart);
                            found = true;
                            break;
                        }
                    }
                }
            }
        }

        // Klassische IDirect3D9-Objekte besitzen keine LUID-Abfrage.
        // Dort bleibt die Monitor-zu-DXGI-Zuordnung der sichere Fallback.
        if (!found) {
            const HMONITOR monitor =
                direct3D->GetAdapterMonitor(creation.AdapterOrdinal);
            ComPtr<IDXGIFactory1> factory;
            if (FAILED(CreateDXGIFactory1(
                    IID_PPV_ARGS(factory.ReleaseAndGetAddressOf())))) {
                logger_.Write("ERROR", "adapter_luid_failed",
                              "CreateDXGIFactory1 failed.");
                return;
            }

            for (UINT adapterIndex = 0; !found; ++adapterIndex) {
                ComPtr<IDXGIAdapter1> adapter;
                if (factory->EnumAdapters1(
                        adapterIndex,
                        adapter.ReleaseAndGetAddressOf()) ==
                    DXGI_ERROR_NOT_FOUND) {
                    break;
                }
                for (UINT outputIndex = 0;; ++outputIndex) {
                    ComPtr<IDXGIOutput> output;
                    const HRESULT enumResult = adapter->EnumOutputs(
                        outputIndex, output.ReleaseAndGetAddressOf());
                    if (enumResult == DXGI_ERROR_NOT_FOUND) {
                        break;
                    }
                    if (FAILED(enumResult)) {
                        continue;
                    }
                    DXGI_OUTPUT_DESC outputDescription{};
                    if (SUCCEEDED(output->GetDesc(&outputDescription)) &&
                        outputDescription.Monitor == monitor) {
                        DXGI_ADAPTER_DESC1 adapterDescription{};
                        if (SUCCEEDED(adapter->GetDesc1(
                                &adapterDescription))) {
                            gameAdapterLuid_ = PackLuid(
                                static_cast<std::uint32_t>(
                                    adapterDescription.AdapterLuid.HighPart),
                                adapterDescription.AdapterLuid.LowPart);
                            found = true;
                        }
                        break;
                    }
                }
            }
        }
        if (!found) {
            logger_.Write("ERROR", "adapter_luid_failed",
                          "No DXGI adapter matched the D3D9 monitor.");
            return;
        }

        shared_->gameAdapterLuid = gameAdapterLuid_;
        deviceMetadataReady_ = true;
        std::ostringstream message;
        message << "luid=0x" << std::hex << std::uppercase
                << LuidHigh(gameAdapterLuid_) << ':'
                << LuidLow(gameAdapterLuid_);
        logger_.Write("INFO", "d3d9_adapter", message.str());
    }

    void UpdateAdapterMatch() noexcept {
        if (!deviceMetadataReady_ || shared_->hostAdapterLuid == 0) {
            return;
        }
        if (shared_->hostAdapterLuid == gameAdapterLuid_) {
            InterlockedOr(AtomicFlags(*shared_), FEARVR_BF_ADAPTER_MATCH);
            if (!adapterMatchLogged_) {
                logger_.Write("INFO", "adapter_match",
                              "D3D9 and OpenXR adapter LUIDs match.");
                adapterMatchLogged_ = true;
            }
        } else {
            InterlockedAnd(
                AtomicFlags(*shared_),
                static_cast<LONG>(~FEARVR_BF_ADAPTER_MATCH));
            if (!adapterMismatchLogged_) {
                std::ostringstream message;
                message << "game=0x" << std::hex << std::uppercase
                        << gameAdapterLuid_ << " host=0x"
                        << shared_->hostAdapterLuid;
                logger_.Write("ERROR", "adapter_mismatch", message.str());
                adapterMismatchLogged_ = true;
            }
        }
    }

    void UpdateHostConnection() noexcept {
        const std::uint64_t heartbeat =
            ReadAtomic64(shared_->hostHeartbeat);
        const ULONGLONG now = GetTickCount64();
        if (heartbeat != lastHostHeartbeat_) {
            lastHostHeartbeat_ = heartbeat;
            lastHostHeartbeatTick_ = now;
        }
        const bool connected =
            heartbeat != 0 &&
            (shared_->bridgeFlags & FEARVR_BF_HOST_READY) != 0 &&
            now - lastHostHeartbeatTick_ <= 2000;
        if (connected != hostConnected_) {
            hostConnected_ = connected;
            logger_.Write(connected ? "INFO" : "WARN",
                          connected ? "host_connected"
                                    : "host_disconnected",
                          connected
                              ? "Host heartbeat active."
                              : "Host heartbeat timed out; flat screen continues.");
            if (!connected) {
                RecoverSlotsAfterHostLoss();
            }
        }
    }

    void RecoverSlotsAfterHostLoss() noexcept {
        for (std::uint32_t eye = 0; eye < FEARVR_EYE_COUNT; ++eye) {
            for (std::uint32_t slotIndex = 0;
                 slotIndex < FEARVR_SLOTS_PER_EYE; ++slotIndex) {
                FearVrSlot& slot = shared_->slot[eye][slotIndex];
                if (slot.state == FEARVR_SLOT_READY ||
                    slot.state == FEARVR_SLOT_CONSUMING) {
                    InterlockedExchange(AtomicState(slot),
                                        FEARVR_SLOT_EMPTY);
                }
            }
        }
    }

    bool CreateSharedSlots(IDirect3DDevice9* resourceDevice, UINT width,
                           UINT height) noexcept {
        for (std::uint32_t eye = 0; eye < FEARVR_EYE_COUNT; ++eye) {
            for (std::uint32_t slotIndex = 0;
                 slotIndex < FEARVR_SLOTS_PER_EYE; ++slotIndex) {
                SlotResource& resource = resources_[eye][slotIndex];
                HANDLE sharedHandle = nullptr;
                HRESULT result = resourceDevice->CreateTexture(
                    width, height, 1, D3DUSAGE_RENDERTARGET,
                    D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT,
                    resource.texture.ReleaseAndGetAddressOf(),
                    &sharedHandle);
                if (FAILED(result) || sharedHandle == nullptr) {
                    LogHresult("shared_texture_create_failed", result);
                    return false;
                }
                resource.sharedHandle = sharedHandle;
                result = resource.texture->GetSurfaceLevel(
                    0, resource.surface.ReleaseAndGetAddressOf());
                if (FAILED(result)) {
                    LogHresult("shared_surface_failed", result);
                    return false;
                }
                result = resourceDevice->CreateQuery(
                    D3DQUERYTYPE_EVENT,
                    resource.completion.ReleaseAndGetAddressOf());
                if (FAILED(result) || !resource.completion) {
                    LogHresult("d3d9_query_create_failed", result);
                    return false;
                }

                FearVrSlot& slot = shared_->slot[eye][slotIndex];
                slot.sharedHandle = static_cast<std::uint64_t>(
                    reinterpret_cast<std::uintptr_t>(sharedHandle));
                slot.frameId = 0;
                slot.width = width;
                slot.height = height;
                slot.format = FEARVR_FMT_B8G8R8A8;
                slot.generation = 0;
                InterlockedExchange(AtomicState(slot),
                                    FEARVR_SLOT_EMPTY);
            }
        }
        return true;
    }

    bool CreateStereoCaptureSurfaces(IDirect3DDevice9* gameDevice,
                                     UINT width,
                                     UINT height) noexcept {
        // Render-Target-*Texturen* statt reiner Surfaces: Der GPU-Kompositor
        // muss die beiden Weltbilder in einem Shader abtasten können, und für
        // StretchRect und GetRenderTargetData ist Ebene 0 einer solchen Textur
        // dasselbe wie ein Render-Target-Surface.
        for (std::uint32_t eye = 0; eye < FEARVR_EYE_COUNT; ++eye) {
            HRESULT result = gameDevice->CreateTexture(
                width, height, 1, D3DUSAGE_RENDERTARGET,
                D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT,
                stereoCaptureTexture_[eye].ReleaseAndGetAddressOf(),
                nullptr);
            if (FAILED(result)) {
                LogHresult("stereo_capture_create_failed", result);
                return false;
            }
            result = stereoCaptureTexture_[eye]->GetSurfaceLevel(
                0, stereoCapture_[eye].ReleaseAndGetAddressOf());
            if (FAILED(result)) {
                LogHresult("stereo_capture_surface_failed", result);
                return false;
            }
        }
        return true;
    }

    bool CreateAsyncTransferResources(IDirect3DDevice9* gameDevice,
                                      UINT width,
                                      UINT height) noexcept {
        if (gameDevice == nullptr ||
            multithreadedDevice_ != gameDevice) {
            return false;
        }
        for (AsyncTransferJob& job : asyncJobs_) {
            for (std::uint32_t eye = 0;
                 eye < FEARVR_EYE_COUNT; ++eye) {
                HRESULT result = gameDevice->CreateTexture(
                    width, height, 1, D3DUSAGE_RENDERTARGET,
                    D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT,
                    job.captureTexture[eye].ReleaseAndGetAddressOf(),
                    nullptr);
                if (FAILED(result)) {
                    LogHresult("async_capture_create_failed", result);
                    return false;
                }
                result = job.captureTexture[eye]->GetSurfaceLevel(
                    0, job.capture[eye].ReleaseAndGetAddressOf());
                if (FAILED(result)) {
                    LogHresult(
                        "async_capture_surface_failed", result);
                    return false;
                }
                result = gameDevice->CreateOffscreenPlainSurface(
                    width, height, D3DFMT_A8R8G8B8,
                    D3DPOOL_SYSTEMMEM,
                    job.readback[eye].ReleaseAndGetAddressOf(),
                    nullptr);
                if (FAILED(result)) {
                    LogHresult("async_readback_create_failed", result);
                    return false;
                }
            }
            const HRESULT queryResult = gameDevice->CreateQuery(
                D3DQUERYTYPE_EVENT,
                job.captureComplete.ReleaseAndGetAddressOf());
            if (FAILED(queryResult) || !job.captureComplete) {
                LogHresult(
                    "async_capture_query_failed", queryResult);
                return false;
            }
            job.state = AsyncJobState::Free;
        }
        asyncGameDevice_ = gameDevice;
        return true;
    }

    bool CreateCpuInteropResources(IDirect3DDevice9* gameDevice,
                                   UINT width, UINT height) noexcept {
        using Direct3DCreate9ExFunction =
            HRESULT(WINAPI*)(UINT, IDirect3D9Ex**);
        const auto createDirect3DEx =
            ResolveSystemD3D9<Direct3DCreate9ExFunction>(
                "Direct3DCreate9Ex");
        if (createDirect3DEx == nullptr) {
            logger_.Write("ERROR", "cpu_bridge_d3d9ex_missing",
                          "System Direct3DCreate9Ex is unavailable.");
            return false;
        }

        IDirect3D9Ex* direct3DEx = nullptr;
        HRESULT result =
            createDirect3DEx(D3D_SDK_VERSION, &direct3DEx);
        bridgeDirect3DEx_.Attach(direct3DEx);
        if (FAILED(result) || !bridgeDirect3DEx_) {
            LogHresult("cpu_bridge_d3d9ex_failed", result);
            return false;
        }

        UINT bridgeAdapter = D3DADAPTER_DEFAULT;
        bool adapterFound = false;
        for (UINT index = 0;
             index < bridgeDirect3DEx_->GetAdapterCount(); ++index) {
            LUID luid{};
            if (SUCCEEDED(bridgeDirect3DEx_->GetAdapterLUID(
                    index, &luid)) &&
                PackLuid(
                    static_cast<std::uint32_t>(luid.HighPart),
                    luid.LowPart) == gameAdapterLuid_) {
                bridgeAdapter = index;
                adapterFound = true;
                break;
            }
        }
        if (!adapterFound) {
            logger_.Write(
                "ERROR", "cpu_bridge_adapter_failed",
                "No D3D9Ex adapter matched the game adapter LUID.");
            return false;
        }

        companionWindow_ = CreateWindowExW(
            0, L"STATIC", L"FearVrD3D9CpuBridge", WS_OVERLAPPED,
            0, 0, 32, 32, nullptr, nullptr, GetModuleHandleW(nullptr),
            nullptr);
        if (companionWindow_ == nullptr) {
            LogWin32("cpu_bridge_window_failed", GetLastError());
            return false;
        }

        D3DPRESENT_PARAMETERS parameters{};
        parameters.Windowed = TRUE;
        parameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
        parameters.hDeviceWindow = companionWindow_;
        parameters.BackBufferWidth = 32;
        parameters.BackBufferHeight = 32;
        parameters.BackBufferFormat = D3DFMT_UNKNOWN;
        result = bridgeDirect3DEx_->CreateDeviceEx(
            bridgeAdapter, D3DDEVTYPE_HAL, companionWindow_,
            D3DCREATE_HARDWARE_VERTEXPROCESSING |
                D3DCREATE_FPU_PRESERVE |
                D3DCREATE_MULTITHREADED,
            &parameters, nullptr,
            bridgeDeviceEx_.ReleaseAndGetAddressOf());
        if (FAILED(result)) {
            result = bridgeDirect3DEx_->CreateDeviceEx(
                bridgeAdapter, D3DDEVTYPE_HAL, companionWindow_,
                D3DCREATE_SOFTWARE_VERTEXPROCESSING |
                    D3DCREATE_FPU_PRESERVE |
                    D3DCREATE_MULTITHREADED,
                &parameters, nullptr,
                bridgeDeviceEx_.ReleaseAndGetAddressOf());
        }
        if (FAILED(result) || !bridgeDeviceEx_) {
            LogHresult("cpu_bridge_device_failed", result);
            return false;
        }

        result = gameDevice->CreateTexture(
            width, height, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
            D3DPOOL_DEFAULT, gameCaptureTexture_.ReleaseAndGetAddressOf(),
            nullptr);
        if (FAILED(result)) {
            LogHresult("cpu_bridge_capture_failed", result);
            return false;
        }
        result = gameCaptureTexture_->GetSurfaceLevel(
            0, gameCapture_.ReleaseAndGetAddressOf());
        if (FAILED(result)) {
            LogHresult("cpu_bridge_capture_surface_failed", result);
            return false;
        }
        result = gameDevice->CreateOffscreenPlainSurface(
            width, height, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM,
            gameReadback_.ReleaseAndGetAddressOf(), nullptr);
        if (FAILED(result)) {
            LogHresult("cpu_bridge_readback_failed", result);
            return false;
        }
        if (config_.stereoHudEnabled) {
            result = gameDevice->CreateOffscreenPlainSurface(
                width, height, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM,
                rightWorldReadback_.ReleaseAndGetAddressOf(), nullptr);
            if (FAILED(result)) {
                LogHresult("stereo_hud_right_readback_failed", result);
                return false;
            }
            result = gameDevice->CreateOffscreenPlainSurface(
                width, height, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM,
                presentedReadback_.ReleaseAndGetAddressOf(), nullptr);
            if (FAILED(result)) {
                LogHresult("stereo_hud_present_readback_failed", result);
                return false;
            }
        }
        result = bridgeDeviceEx_->CreateOffscreenPlainSurface(
            width, height, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM,
            bridgeUpload_.ReleaseAndGetAddressOf(), nullptr);
        if (FAILED(result)) {
            LogHresult("cpu_bridge_upload_failed", result);
            return false;
        }
        if (!CreateSharedSlots(bridgeDeviceEx_.Get(), width, height)) {
            return false;
        }
        transferMode_ = TransferMode::CpuViaD3D9Ex;
        InterlockedOr(AtomicFlags(*shared_), FEARVR_BF_CPU_FALLBACK);
        if (config_.stereoHudEnabled) {
            logger_.Write(
                "INFO", "stereo_hud_ready",
                "Post-world HUD compositing enabled for CPU transfer.");
        }
        return true;
    }

    bool EnsureResources(IDirect3DDevice9* device, UINT sourceWidth,
                         UINT sourceHeight) noexcept {
        if (resourcesReady_ &&
            sourceWidth_ == sourceWidth &&
            sourceHeight_ == sourceHeight &&
            device_ == device) {
            return true;
        }
        const ULONGLONG now = GetTickCount64();
        if (now < nextResourceRetryTick_) {
            return false;
        }

        ReleaseResources();
        device_ = device;
        sourceWidth_ = sourceWidth;
        sourceHeight_ = sourceHeight;
        const TransportExtent transport =
            ComputeTransportExtent(sourceWidth, sourceHeight);
        width_ = transport.width;
        height_ = transport.height;

        ComPtr<IDirect3DDevice9Ex> deviceEx;
        bool created = false;
        if (SUCCEEDED(device->QueryInterface(
                IID_PPV_ARGS(deviceEx.ReleaseAndGetAddressOf())))) {
            created = CreateSharedSlots(device, width_, height_);
            if (created) {
                transferMode_ = TransferMode::DirectShared;
            }
        } else {
            created = CreateCpuInteropResources(device, width_, height_);
        }
        if (created) {
            created =
                CreateStereoCaptureSurfaces(device, width_, height_);
        }
        if (created && config_.stereoHudEnabled &&
            !config_.disableGpuHud) {
            // Scheitert der Kompositor, bleibt das HUD trotzdem: dann mischt
            // wieder die CPU. Kein Grund, den ganzen Bildpfad aufzugeben.
            hudCompositor_.Initialize(device, width_, height_, logger_);
        }
        if (created &&
            transferMode_ == TransferMode::CpuViaD3D9Ex &&
            multithreadedDevice_ == device &&
            (!config_.stereoHudEnabled || hudCompositor_.ready())) {
            if (CreateAsyncTransferResources(device, width_, height_) &&
                EnsureAsyncTransferWorker()) {
                asyncTransferEnabled_ = true;
                logger_.Write(
                    "INFO", "async_cpu_transfer_ready",
                    "Low-latency game-device readback and D3D9Ex upload "
                    "run outside FEAR's Present thread; obsolete frames "
                    "are dropped instead of queued.");
            } else {
                ReleaseAsyncTransferResources();
                logger_.Write(
                    "WARN", "async_cpu_transfer_unavailable",
                    "Asynchronous transfer setup failed; the proven "
                    "synchronous CPU bridge remains active.");
            }
        }
        if (!created) {
            ReleaseResources();
            nextResourceRetryTick_ = now + 1000;
            return false;
        }

        nextResourceRetryTick_ = 0;
        resourcesReady_ = true;
        InterlockedOr(AtomicFlags(*shared_),
                      FEARVR_BF_SHARED_SUPPORTED);
        std::ostringstream message;
        message << "source=" << sourceWidth_ << 'x' << sourceHeight_
                << " transport=" << width_ << 'x' << height_
                << " format=B8G8R8A8 slots="
                << FEARVR_SLOTS_PER_EYE << "x2 path="
                << (transferMode_ == TransferMode::DirectShared
                        ? "direct"
                        : "cpu_d3d9ex");
        logger_.Write(
            transferMode_ == TransferMode::DirectShared
                ? "INFO"
                : "WARN",
            "shared_resources", message.str());
        return true;
    }

    bool CopyFrameDirect(IDirect3DDevice9* device,
                         IDirect3DSurface9* backBuffer,
                         std::uint32_t slotIndex) noexcept {
        for (std::uint32_t eye = 0; eye < FEARVR_EYE_COUNT; ++eye) {
            SlotResource& resource = resources_[eye][slotIndex];
            HRESULT result = device->StretchRect(
                backBuffer, nullptr, resource.surface.Get(), nullptr,
                D3DTEXF_NONE);
            if (FAILED(result)) {
                LogHresult("stretch_rect_failed", result);
                return false;
            }
            result = resource.completion->Issue(D3DISSUE_END);
            if (FAILED(result)) {
                LogHresult("d3d9_query_issue_failed", result);
                return false;
            }
        }
        return true;
    }

    bool CopyStereoFrameDirect(IDirect3DDevice9* device,
                               std::uint32_t slotIndex) noexcept {
        for (std::uint32_t eye = 0; eye < FEARVR_EYE_COUNT; ++eye) {
            SlotResource& resource = resources_[eye][slotIndex];
            HRESULT result = device->StretchRect(
                stereoCapture_[eye].Get(), nullptr,
                resource.surface.Get(), nullptr, D3DTEXF_NONE);
            if (FAILED(result)) {
                LogHresult("stereo_stretch_rect_failed", result);
                return false;
            }
            result = resource.completion->Issue(D3DISSUE_END);
            if (FAILED(result)) {
                LogHresult("stereo_query_issue_failed", result);
                return false;
            }
        }
        return true;
    }

    bool CopyReadbackToBridgeUpload(
        IDirect3DSurface9* readback) noexcept {
        if (readback == nullptr || !bridgeUpload_) {
            return false;
        }
        D3DLOCKED_RECT source{};
        D3DLOCKED_RECT destination{};
        HRESULT result = readback->LockRect(
            &source, nullptr, D3DLOCK_READONLY);
        if (FAILED(result)) {
            LogHresult("cpu_bridge_readback_lock_failed", result);
            return false;
        }
        result = bridgeUpload_->LockRect(&destination, nullptr, 0);
        if (FAILED(result)) {
            readback->UnlockRect();
            LogHresult("cpu_bridge_upload_lock_failed", result);
            return false;
        }

        const std::size_t rowBytes =
            static_cast<std::size_t>(width_) * 4U;
        auto* sourceBytes =
            static_cast<const std::uint8_t*>(source.pBits);
        auto* destinationBytes =
            static_cast<std::uint8_t*>(destination.pBits);
        for (UINT row = 0; row < height_; ++row) {
            std::memcpy(destinationBytes, sourceBytes, rowBytes);
            sourceBytes += source.Pitch;
            destinationBytes += destination.Pitch;
        }
        bridgeUpload_->UnlockRect();
        readback->UnlockRect();
        return true;
    }

    bool StageSurfaceViaCpu(IDirect3DDevice9* device,
                            IDirect3DSurface9* sourceSurface) noexcept {
        const HRESULT result = device->GetRenderTargetData(
            sourceSurface, gameReadback_.Get());
        if (FAILED(result)) {
            LogHresult("cpu_bridge_readback_copy_failed", result);
            return false;
        }
        return CopyReadbackToBridgeUpload(gameReadback_.Get());
    }

    bool UploadCpuSurface(std::uint32_t eye,
                          std::uint32_t slotIndex) noexcept {
        SlotResource& resource = resources_[eye][slotIndex];
        HRESULT result = bridgeDeviceEx_->UpdateSurface(
            bridgeUpload_.Get(), nullptr, resource.surface.Get(), nullptr);
        if (FAILED(result)) {
            LogHresult("cpu_bridge_update_failed", result);
            return false;
        }
        result = resource.completion->Issue(D3DISSUE_END);
        if (FAILED(result)) {
            LogHresult("cpu_bridge_query_failed", result);
            return false;
        }
        return true;
    }

    bool CopyFrameViaCpu(IDirect3DDevice9* device,
                         IDirect3DSurface9* backBuffer,
                         std::uint32_t slotIndex) noexcept {
        HRESULT result = device->StretchRect(
            backBuffer, nullptr, gameCapture_.Get(), nullptr,
            D3DTEXF_NONE);
        if (FAILED(result)) {
            LogHresult("cpu_bridge_stretch_failed", result);
            return false;
        }
        if (!StageSurfaceViaCpu(device, gameCapture_.Get())) {
            return false;
        }

        for (std::uint32_t eye = 0; eye < FEARVR_EYE_COUNT; ++eye) {
            if (!UploadCpuSurface(eye, slotIndex)) {
                return false;
            }
        }
        return true;
    }

    bool CopyStereoFrameViaCpu(IDirect3DDevice9* device,
                               IDirect3DSurface9* backBuffer,
                               std::uint32_t slotIndex) noexcept {
        if (config_.stereoHudEnabled && hudCompositor_.ready()) {
            if (CopyStereoHudFrameViaGpu(device, backBuffer, slotIndex)) {
                return true;
            }
            // Compose() hat sich beim Fehlschlag selbst abgeschaltet; der
            // nächste Frame nimmt dann direkt den CPU-Weg.
        }
        if (config_.stereoHudEnabled &&
            rightWorldReadback_ && presentedReadback_) {
            return CopyStereoHudFrameViaCpu(
                device, backBuffer, slotIndex);
        }
        for (std::uint32_t eye = 0; eye < FEARVR_EYE_COUNT; ++eye) {
            if (!StageSurfaceViaCpu(
                    device, stereoCapture_[eye].Get()) ||
                !UploadCpuSurface(eye, slotIndex)) {
                return false;
            }
        }
        return true;
    }

    bool ReadSurfaceViaCpu(IDirect3DDevice9* device,
                           IDirect3DSurface9* source,
                           IDirect3DSurface9* destination,
                           const char* failureEvent) noexcept {
        const HRESULT result =
            device->GetRenderTargetData(source, destination);
        if (FAILED(result)) {
            LogHresult(failureEvent, result);
            return false;
        }
        return true;
    }

    // Derselbe Bildaufbau wie CopyStereoHudFrameViaCpu, nur entscheidet ein
    // Pixelshader statt einer Schleife. Übrig bleibt der Transfer-Readback,
    // den das klassische D3D9-Gerät des Spiels erzwingt.
    bool CopyStereoHudFrameViaGpu(IDirect3DDevice9* device,
                                  IDirect3DSurface9* backBuffer,
                                  std::uint32_t slotIndex) noexcept {
        HRESULT result = device->StretchRect(
            backBuffer, nullptr, gameCapture_.Get(), nullptr,
            D3DTEXF_NONE);
        if (FAILED(result)) {
            LogHresult("stereo_hud_present_stretch_failed", result);
            return false;
        }

        const std::uint64_t totalPixels =
            static_cast<std::uint64_t>(width_) * height_;
        // Der CPU-Wert stammt aus dem vorherigen Bild und dient nur dem Log.
        // Die aktuelle 1x1-Abdeckung entscheidet ohne Readback im Shader.
        const std::uint64_t changedPixels = static_cast<std::uint64_t>(
            hudCompositor_.coverageRatio() *
                static_cast<double>(totalPixels) + 0.5);
        const bool flatPanel = menuActive_;
        const bool composite = !flatPanel;
        stereoHudFlatFrame_ = flatPanel;

        IDirect3DTexture9* const eyeWorld[FEARVR_EYE_COUNT] = {
            stereoCaptureTexture_[FEARVR_EYE_LEFT].Get(),
            stereoCaptureTexture_[FEARVR_EYE_RIGHT].Get()};
        if (!hudCompositor_.Compose(
                device, gameCaptureTexture_.Get(),
                stereoCaptureTexture_[FEARVR_EYE_RIGHT].Get(), eyeWorld,
                composite, flatPanel, logger_)) {
            return false;
        }

        for (std::uint32_t eye = 0; eye < FEARVR_EYE_COUNT; ++eye) {
            if (!StageSurfaceViaCpu(
                    device, hudCompositor_.CompositeSurface(eye)) ||
                !UploadCpuSurface(eye, slotIndex)) {
                return false;
            }
        }

        ++stereoHudFrames_;
        if (stereoHudFrames_ == 1 || stereoHudFrames_ % 300 == 0) {
            std::ostringstream message;
            message << "previous_changed_pixels=" << changedPixels
                    << " previous_coverage_percent="
                    << (totalPixels == 0
                            ? 0
                            : changedPixels * 100u / totalPixels)
                    << " mode="
                    << (flatPanel
                            ? "flat_panel"
                            : "current_frame_gpu_guard")
                    << " path=gpu";
            logger_.Write(
                "INFO",
                flatPanel
                    ? "stereo_hud_flat_panel"
                    : "stereo_hud_gpu_guarded",
                message.str());
        }
        return true;
    }

    bool CopyStereoHudFrameViaCpu(IDirect3DDevice9* device,
                                  IDirect3DSurface9* backBuffer,
                                  std::uint32_t slotIndex) noexcept {
        HRESULT result = device->StretchRect(
            backBuffer, nullptr, gameCapture_.Get(), nullptr,
            D3DTEXF_NONE);
        if (FAILED(result)) {
            LogHresult("stereo_hud_present_stretch_failed", result);
            return false;
        }
        if (!ReadSurfaceViaCpu(
                device, gameCapture_.Get(), presentedReadback_.Get(),
                "stereo_hud_present_readback_failed") ||
            !ReadSurfaceViaCpu(
                device, stereoCapture_[FEARVR_EYE_RIGHT].Get(),
                rightWorldReadback_.Get(),
                "stereo_hud_right_readback_failed")) {
            return false;
        }

        D3DLOCKED_RECT presented{};
        D3DLOCKED_RECT rightWorld{};
        result = presentedReadback_->LockRect(
            &presented, nullptr, D3DLOCK_READONLY);
        if (FAILED(result)) {
            LogHresult("stereo_hud_present_lock_failed", result);
            return false;
        }
        result = rightWorldReadback_->LockRect(
            &rightWorld, nullptr, D3DLOCK_READONLY);
        if (FAILED(result)) {
            presentedReadback_->UnlockRect();
            LogHresult("stereo_hud_right_lock_failed", result);
            return false;
        }

        std::uint64_t changedPixels = 0;
        for (UINT row = 0; row < height_; ++row) {
            const auto* presentedPixels =
                reinterpret_cast<const std::uint32_t*>(
                    static_cast<const std::uint8_t*>(presented.pBits) +
                    static_cast<std::size_t>(row) *
                        static_cast<std::size_t>(presented.Pitch));
            const auto* rightPixels =
                reinterpret_cast<const std::uint32_t*>(
                    static_cast<const std::uint8_t*>(rightWorld.pBits) +
                    static_cast<std::size_t>(row) *
                        static_cast<std::size_t>(rightWorld.Pitch));
            for (UINT column = 0; column < width_; ++column) {
                changedPixels += IsPostWorldPixel(
                    presentedPixels[column], rightPixels[column])
                    ? 1u
                    : 0u;
            }
        }
        const std::uint64_t totalPixels =
            static_cast<std::uint64_t>(width_) * height_;
        // Menu state is supplied by the verified Retail menu/freshness hooks.
        // Pixel coverage alone cannot distinguish a menu from a fullscreen
        // gameplay effect such as Slow-Mo, so it must not select mono video
        // mode anymore.
        const bool flatPanel = menuActive_;
        const bool composite =
            !flatPanel && IsSafePostWorldCoverage(changedPixels, totalPixels);
        stereoHudFlatFrame_ = flatPanel;

        for (std::uint32_t eye = 0; eye < FEARVR_EYE_COUNT; ++eye) {
            D3DLOCKED_RECT source{};
            bool sourceLocked = false;
            if (flatPanel) {
                source = presented;
            } else if (eye == FEARVR_EYE_RIGHT) {
                source = rightWorld;
            } else {
                if (!ReadSurfaceViaCpu(
                        device, stereoCapture_[eye].Get(),
                        gameReadback_.Get(),
                        "stereo_hud_left_readback_failed")) {
                    rightWorldReadback_->UnlockRect();
                    presentedReadback_->UnlockRect();
                    return false;
                }
                result = gameReadback_->LockRect(
                    &source, nullptr, D3DLOCK_READONLY);
                if (FAILED(result)) {
                    rightWorldReadback_->UnlockRect();
                    presentedReadback_->UnlockRect();
                    LogHresult("stereo_hud_left_lock_failed", result);
                    return false;
                }
                sourceLocked = true;
            }

            D3DLOCKED_RECT destination{};
            result = bridgeUpload_->LockRect(
                &destination, nullptr, 0);
            if (FAILED(result)) {
                if (sourceLocked) {
                    gameReadback_->UnlockRect();
                }
                rightWorldReadback_->UnlockRect();
                presentedReadback_->UnlockRect();
                LogHresult("stereo_hud_upload_lock_failed", result);
                return false;
            }

            for (UINT row = 0; row < height_; ++row) {
                const UINT hudSourceRow =
                    StereoHudSourceRow(row, height_);
                const auto* sourcePixels =
                    reinterpret_cast<const std::uint32_t*>(
                        static_cast<const std::uint8_t*>(source.pBits) +
                        static_cast<std::size_t>(row) *
                            static_cast<std::size_t>(source.Pitch));
                const auto* presentedPixels =
                    reinterpret_cast<const std::uint32_t*>(
                        static_cast<const std::uint8_t*>(presented.pBits) +
                        static_cast<std::size_t>(
                            hudSourceRow < height_ ? hudSourceRow : row) *
                            static_cast<std::size_t>(presented.Pitch));
                const auto* rightPixels =
                    reinterpret_cast<const std::uint32_t*>(
                        static_cast<const std::uint8_t*>(rightWorld.pBits) +
                        static_cast<std::size_t>(
                            hudSourceRow < height_ ? hudSourceRow : row) *
                            static_cast<std::size_t>(rightWorld.Pitch));
                auto* destinationPixels =
                    reinterpret_cast<std::uint32_t*>(
                        static_cast<std::uint8_t*>(destination.pBits) +
                        static_cast<std::size_t>(row) *
                            static_cast<std::size_t>(destination.Pitch));
                for (UINT column = 0; column < width_; ++column) {
                    const UINT hudSourceColumn =
                        StereoHudSourceColumn(
                            column, row, width_, height_);
                    const bool overlayPixel =
                        composite && hudSourceRow < height_ &&
                        hudSourceColumn < width_ &&
                        IsPostWorldPixel(
                            presentedPixels[hudSourceColumn],
                            rightPixels[hudSourceColumn]);
                    destinationPixels[column] = flatPanel
                        ? sourcePixels[column]
                        : (overlayPixel
                               ? presentedPixels[hudSourceColumn]
                                        : sourcePixels[column]);
                }
            }
            bridgeUpload_->UnlockRect();
            if (sourceLocked) {
                gameReadback_->UnlockRect();
            }
            if (!UploadCpuSurface(eye, slotIndex)) {
                rightWorldReadback_->UnlockRect();
                presentedReadback_->UnlockRect();
                return false;
            }
        }
        rightWorldReadback_->UnlockRect();
        presentedReadback_->UnlockRect();

        ++stereoHudFrames_;
        if (stereoHudFrames_ == 1 ||
            stereoHudFrames_ % 300 == 0) {
            std::ostringstream message;
            message << "changed_pixels=" << changedPixels
                    << " coverage_percent="
                    << (totalPixels == 0
                            ? 0
                            : changedPixels * 100u / totalPixels)
                    << " mode="
                    << (flatPanel
                            ? "flat_panel"
                            : (composite ? "raised_hud"
                                         : "world_only"));
            logger_.Write(
                composite || flatPanel ? "INFO" : "WARN",
                flatPanel
                    ? "stereo_hud_flat_panel"
                    : (composite ? "stereo_hud_composited"
                                 : "stereo_hud_rejected"),
                message.str());
        }
        return true;
    }

    int AcquireAsyncJob() noexcept {
        std::lock_guard<std::mutex> lock(asyncTransferMutex_);
        std::size_t freeIndex = asyncJobs_.size();
        for (std::size_t index = 0;
             index < asyncJobs_.size(); ++index) {
            if (asyncJobs_[index].state == AsyncJobState::Free) {
                freeIndex = index;
                break;
            }
        }
        if (freeIndex == asyncJobs_.size()) {
            return -1;
        }

        // Keep at most one pending image behind the active readback. If a
        // newer frame arrives while one is queued, retire the obsolete
        // pending frame and queue the newest capture in the free third
        // surface. The worker later waits only for that retired GPU copy,
        // never performs its expensive readback/upload.
        if (asyncQueueCount_ != 0) {
            const std::size_t obsoleteIndex =
                asyncQueue_[asyncQueueHead_];
            asyncQueueHead_ =
                (asyncQueueHead_ + 1) % asyncQueue_.size();
            --asyncQueueCount_;
            AsyncTransferJob& obsolete =
                asyncJobs_[obsoleteIndex];
            obsolete.state = AsyncJobState::Discarded;
            if (shared_ != nullptr) {
                ReleaseClaimedPair(obsolete.slotIndex);
            }
            ++asyncSupersededFrames_;
        }
        asyncJobs_[freeIndex].state =
            AsyncJobState::Preparing;
        return static_cast<int>(freeIndex);
    }

    void CancelAsyncJob(std::size_t index) noexcept {
        std::lock_guard<std::mutex> lock(asyncTransferMutex_);
        if (index < asyncJobs_.size()) {
            asyncJobs_[index].state = AsyncJobState::Free;
        }
    }

    bool EnqueueAsyncJob(std::size_t index) noexcept {
        {
            std::lock_guard<std::mutex> lock(asyncTransferMutex_);
            if (index >= asyncJobs_.size() ||
                asyncJobs_[index].state !=
                    AsyncJobState::Preparing ||
                asyncQueueCount_ >= asyncQueue_.size()) {
                return false;
            }
            asyncJobs_[index].state = AsyncJobState::Queued;
            asyncQueue_[asyncQueueTail_] =
                static_cast<std::uint32_t>(index);
            asyncQueueTail_ =
                (asyncQueueTail_ + 1) % asyncQueue_.size();
            ++asyncQueueCount_;
        }
        asyncTransferWake_.notify_one();
        return true;
    }

    bool QueueFrameAsync(
        IDirect3DDevice9* device, IDirect3DSurface9* backBuffer,
        std::uint32_t slotIndex, std::uint64_t frameId,
        std::uint64_t generation, bool stereo) noexcept {
        const int acquired = AcquireAsyncJob();
        if (acquired < 0) {
            ++asyncQueueDrops_;
            if (asyncQueueDrops_ == 1 ||
                asyncQueueDrops_ % 300 == 0) {
                logger_.Write(
                    "WARN", "async_transfer_backpressure",
                    "dropped=" +
                        std::to_string(asyncQueueDrops_));
            }
            return false;
        }
        const std::size_t jobIndex =
            static_cast<std::size_t>(acquired);
        AsyncTransferJob& job = asyncJobs_[jobIndex];

        IDirect3DSurface9* sources[FEARVR_EYE_COUNT] = {};
        if (!stereo) {
            const HRESULT result = device->StretchRect(
                backBuffer, nullptr, gameCapture_.Get(), nullptr,
                D3DTEXF_NONE);
            if (FAILED(result)) {
                LogHresult("async_mono_stage_failed", result);
                CancelAsyncJob(jobIndex);
                return false;
            }
            sources[FEARVR_EYE_LEFT] = gameCapture_.Get();
            sources[FEARVR_EYE_RIGHT] = gameCapture_.Get();
        } else {
            sources[FEARVR_EYE_LEFT] =
                stereoCapture_[FEARVR_EYE_LEFT].Get();
            sources[FEARVR_EYE_RIGHT] =
                stereoCapture_[FEARVR_EYE_RIGHT].Get();
        }
        if (stereo && config_.stereoHudEnabled) {
            if (!hudCompositor_.ready()) {
                CancelAsyncJob(jobIndex);
                return false;
            }
            HRESULT result = device->StretchRect(
                backBuffer, nullptr, gameCapture_.Get(), nullptr,
                D3DTEXF_NONE);
            if (FAILED(result)) {
                LogHresult(
                    "async_hud_present_stretch_failed", result);
                CancelAsyncJob(jobIndex);
                return false;
            }

            const std::uint64_t totalPixels =
                static_cast<std::uint64_t>(width_) * height_;
            const std::uint64_t changedPixels =
                static_cast<std::uint64_t>(
                    hudCompositor_.coverageRatio() *
                        static_cast<double>(totalPixels) +
                    0.5);
            const bool flatPanel = menuActive_;
            const bool composite = !flatPanel;
            stereoHudFlatFrame_ = flatPanel;
            IDirect3DTexture9* const eyeWorld[
                FEARVR_EYE_COUNT] = {
                stereoCaptureTexture_[
                    FEARVR_EYE_LEFT].Get(),
                stereoCaptureTexture_[
                    FEARVR_EYE_RIGHT].Get()};
            if (!hudCompositor_.Compose(
                    device, gameCaptureTexture_.Get(),
                    stereoCaptureTexture_[
                        FEARVR_EYE_RIGHT].Get(),
                    eyeWorld, composite, flatPanel, logger_)) {
                CancelAsyncJob(jobIndex);
                return false;
            }
            for (std::uint32_t eye = 0;
                 eye < FEARVR_EYE_COUNT; ++eye) {
                sources[eye] =
                    hudCompositor_.CompositeSurface(eye);
            }

            ++stereoHudFrames_;
            if (stereoHudFrames_ == 1 ||
                stereoHudFrames_ % 300 == 0) {
                std::ostringstream message;
                message
                    << "previous_changed_pixels=" << changedPixels
                    << " previous_coverage_percent="
                    << (totalPixels == 0
                            ? 0
                            : changedPixels * 100u /
                                  totalPixels)
                    << " mode="
                    << (flatPanel
                            ? "flat_panel"
                            : "current_frame_gpu_guard")
                    << " path=gpu_async";
                logger_.Write(
                    "INFO",
                    flatPanel
                        ? "stereo_hud_flat_panel"
                        : "stereo_hud_gpu_guarded",
                    message.str());
            }
        }

        for (std::uint32_t eye = 0;
             eye < FEARVR_EYE_COUNT; ++eye) {
            const HRESULT result = device->StretchRect(
                sources[eye], nullptr, job.capture[eye].Get(),
                nullptr, D3DTEXF_NONE);
            if (FAILED(result)) {
                LogHresult("async_stage_copy_failed", result);
                CancelAsyncJob(jobIndex);
                return false;
            }
        }
        const HRESULT queryResult =
            job.captureComplete->Issue(D3DISSUE_END);
        if (FAILED(queryResult)) {
            LogHresult("async_stage_query_failed", queryResult);
            CancelAsyncJob(jobIndex);
            return false;
        }

        job.slotIndex = slotIndex;
        job.frameId = frameId;
        job.generation = generation;
        job.queuedAt = std::chrono::steady_clock::now();
        if (!EnqueueAsyncJob(jobIndex)) {
            CancelAsyncJob(jobIndex);
            return false;
        }
        return true;
    }

    bool WaitForAsyncQuery(IDirect3DQuery9* query,
                           const char* failureEvent) noexcept {
        if (query == nullptr) {
            return false;
        }
        const ULONGLONG deadline = GetTickCount64() + 250;
        for (;;) {
            const HRESULT result =
                query->GetData(nullptr, 0, D3DGETDATA_FLUSH);
            if (result == S_OK) {
                return true;
            }
            if (FAILED(result) || GetTickCount64() >= deadline) {
                LogHresult(
                    failureEvent,
                    FAILED(result) ? result : E_ABORT);
                return false;
            }
            std::this_thread::sleep_for(
                std::chrono::microseconds(250));
        }
    }

    bool ProcessAsyncJob(std::size_t jobIndex) noexcept {
        if (jobIndex >= asyncJobs_.size() ||
            !asyncGameDevice_ || shared_ == nullptr) {
            return false;
        }
        AsyncTransferJob& job = asyncJobs_[jobIndex];
        if (!WaitForAsyncQuery(
                job.captureComplete.Get(),
                "async_capture_wait_failed")) {
            return false;
        }

        const auto readbackStart =
            std::chrono::steady_clock::now();
        for (std::uint32_t eye = 0;
             eye < FEARVR_EYE_COUNT; ++eye) {
            const HRESULT result =
                asyncGameDevice_->GetRenderTargetData(
                    job.capture[eye].Get(),
                    job.readback[eye].Get());
            if (FAILED(result)) {
                LogHresult("async_readback_failed", result);
                return false;
            }
            if (!CopyReadbackToBridgeUpload(
                    job.readback[eye].Get()) ||
                !UploadCpuSurface(eye, job.slotIndex)) {
                return false;
            }
        }
        const std::uint64_t readbackMicroseconds =
            static_cast<std::uint64_t>(
                std::chrono::duration_cast<
                    std::chrono::microseconds>(
                    std::chrono::steady_clock::now() -
                    readbackStart)
                    .count());

        for (std::uint32_t eye = 0;
             eye < FEARVR_EYE_COUNT; ++eye) {
            if (!WaitForAsyncQuery(
                    resources_[eye][job.slotIndex]
                        .completion.Get(),
                    "async_upload_wait_failed")) {
                return false;
            }
        }

        bool slotsStillClaimed = true;
        for (std::uint32_t eye = 0;
             eye < FEARVR_EYE_COUNT; ++eye) {
            FearVrSlot& slot =
                shared_->slot[eye][job.slotIndex];
            if (InterlockedCompareExchange(
                    AtomicState(slot), FEARVR_SLOT_WRITING,
                    FEARVR_SLOT_WRITING) !=
                FEARVR_SLOT_WRITING) {
                slotsStillClaimed = false;
            }
        }
        if (!slotsStillClaimed) {
            return false;
        }

        MemoryBarrier();
        for (std::uint32_t eye = 0;
             eye < FEARVR_EYE_COUNT; ++eye) {
            InterlockedExchange(
                AtomicState(
                    shared_->slot[eye][job.slotIndex]),
                FEARVR_SLOT_READY);
        }
        SetEvent(frameReadyEvent_);
        if (job.frameId == 1 || job.frameId % 300 == 0) {
            std::ostringstream message;
            message << "frame=" << job.frameId
                    << " generation=" << job.generation
                    << " slot=" << job.slotIndex
                    << " path=async";
            logger_.Write(
                "INFO", "frame_ready", message.str());
        }

        const std::uint64_t totalMicroseconds =
            static_cast<std::uint64_t>(
                std::chrono::duration_cast<
                    std::chrono::microseconds>(
                    std::chrono::steady_clock::now() -
                    job.queuedAt)
                    .count());
        ++asyncTransferSamples_;
        asyncTransferTotalMicroseconds_ += totalMicroseconds;
        asyncTransferMaxMicroseconds_ =
            (std::max)(asyncTransferMaxMicroseconds_,
                       totalMicroseconds);
        asyncReadbackTotalMicroseconds_ +=
            readbackMicroseconds;
        asyncReadbackMaxMicroseconds_ =
            (std::max)(asyncReadbackMaxMicroseconds_,
                       readbackMicroseconds);
        if (asyncTransferSamples_ == 1 ||
            asyncTransferSamples_ % 300 == 0) {
            std::ostringstream message;
            message
                << "frames=" << asyncTransferSamples_
                << " total_avg_us="
                << asyncTransferTotalMicroseconds_ /
                       asyncTransferSamples_
                << " total_max_us="
                << asyncTransferMaxMicroseconds_
                << " readback_upload_avg_us="
                << asyncReadbackTotalMicroseconds_ /
                       asyncTransferSamples_
                << " readback_upload_max_us="
                << asyncReadbackMaxMicroseconds_;
            logger_.Write(
                "INFO", "async_transfer_complete",
                message.str());
        }
        return true;
    }

    bool EnsureAsyncTransferWorker() noexcept {
        if (asyncTransferWorker_.joinable()) {
            return true;
        }
        try {
            asyncTransferStop_ = false;
            asyncTransferWorker_ = std::thread(
                [this]() noexcept {
                    AsyncTransferWorkerLoop();
                });
            return true;
        } catch (...) {
            return false;
        }
    }

    void AsyncTransferWorkerLoop() noexcept {
        for (;;) {
            std::size_t jobIndex = 0;
            {
                std::unique_lock<std::mutex> lock(
                    asyncTransferMutex_);
                asyncTransferWake_.wait(
                    lock, [this]() noexcept {
                        return asyncTransferStop_ ||
                               asyncQueueCount_ != 0;
                    });
                if (asyncTransferStop_) {
                    return;
                }
                jobIndex = asyncQueue_[asyncQueueHead_];
                asyncQueueHead_ =
                    (asyncQueueHead_ + 1) %
                    asyncQueue_.size();
                --asyncQueueCount_;
                asyncJobs_[jobIndex].state =
                    AsyncJobState::Processing;
            }

            if (!ProcessAsyncJob(jobIndex) &&
                shared_ != nullptr) {
                ReleaseClaimedPair(
                    asyncJobs_[jobIndex].slotIndex);
            }
            {
                std::lock_guard<std::mutex> lock(
                    asyncTransferMutex_);
                asyncJobs_[jobIndex].state =
                    AsyncJobState::Free;
            }
            ReapDiscardedAsyncJobs();
        }
    }

    void ReapDiscardedAsyncJobs() noexcept {
        for (std::size_t index = 0;
             index < asyncJobs_.size(); ++index) {
            bool discarded = false;
            {
                std::lock_guard<std::mutex> lock(
                    asyncTransferMutex_);
                discarded =
                    asyncJobs_[index].state ==
                    AsyncJobState::Discarded;
            }
            if (!discarded) {
                continue;
            }
            // The discarded copy was submitted before the newest queued
            // copy, so this query normally completes immediately.
            WaitForAsyncQuery(
                asyncJobs_[index].captureComplete.Get(),
                "async_discard_wait_failed");
            {
                std::lock_guard<std::mutex> lock(
                    asyncTransferMutex_);
                if (asyncJobs_[index].state ==
                    AsyncJobState::Discarded) {
                    asyncJobs_[index].state =
                        AsyncJobState::Free;
                }
            }
        }
    }

    void ReleaseAsyncTransferResources() noexcept {
        {
            std::lock_guard<std::mutex> lock(
                asyncTransferMutex_);
            asyncTransferStop_ = true;
        }
        asyncTransferWake_.notify_all();
        if (asyncTransferWorker_.joinable()) {
            asyncTransferWorker_.join();
        }
        {
            std::lock_guard<std::mutex> lock(
                asyncTransferMutex_);
            if (shared_ != nullptr) {
                for (AsyncTransferJob& job : asyncJobs_) {
                    if (job.state != AsyncJobState::Free) {
                        ReleaseClaimedPair(job.slotIndex);
                    }
                }
            }
            asyncQueueHead_ = 0;
            asyncQueueTail_ = 0;
            asyncQueueCount_ = 0;
            for (AsyncTransferJob& job : asyncJobs_) {
                job.state = AsyncJobState::Free;
                job.captureComplete.Reset();
                for (std::uint32_t eye = 0;
                     eye < FEARVR_EYE_COUNT; ++eye) {
                    job.readback[eye].Reset();
                    job.capture[eye].Reset();
                    job.captureTexture[eye].Reset();
                }
            }
        }
        asyncGameDevice_.Reset();
        asyncTransferEnabled_ = false;
        asyncTransferStop_ = false;
    }

    void ReleaseResources() noexcept {
        ReleaseAsyncTransferResources();
        pending_ = {};
        resourcesReady_ = false;
        transferMode_ = TransferMode::None;
        // Zuerst der Kompositor: Er hält Render-Targets auf demselben Gerät.
        hudCompositor_.Release();
        for (std::uint32_t eye = 0; eye < FEARVR_EYE_COUNT; ++eye) {
            stereoCaptureTexture_[eye].Reset();
            stereoCapture_[eye].Reset();
            for (std::uint32_t slotIndex = 0;
                 slotIndex < FEARVR_SLOTS_PER_EYE; ++slotIndex) {
                SlotResource& resource = resources_[eye][slotIndex];
                resource.completion.Reset();
                resource.surface.Reset();
                resource.texture.Reset();
                resource.sharedHandle = nullptr;
                if (shared_ != nullptr) {
                    FearVrSlot& slot = shared_->slot[eye][slotIndex];
                    slot.sharedHandle = 0;
                    slot.frameId = 0;
                    slot.width = 0;
                    slot.height = 0;
                    slot.format = FEARVR_FMT_UNKNOWN;
                    slot.generation = 0;
                    slot.camera = {};
                    InterlockedExchange(AtomicState(slot),
                                        FEARVR_SLOT_EMPTY);
                }
            }
        }
        bridgeUpload_.Reset();
        presentedReadback_.Reset();
        rightWorldReadback_.Reset();
        gameReadback_.Reset();
        gameCapture_.Reset();
        gameCaptureTexture_.Reset();
        ClearStereoFrame();
        bridgeDeviceEx_.Reset();
        bridgeDirect3DEx_.Reset();
        if (companionWindow_ != nullptr) {
            DestroyWindow(companionWindow_);
            companionWindow_ = nullptr;
        }
        if (shared_ != nullptr) {
            InterlockedAnd(
                AtomicFlags(*shared_),
                static_cast<LONG>(
                    ~(FEARVR_BF_SHARED_SUPPORTED |
                      FEARVR_BF_CPU_FALLBACK |
                      FEARVR_BF_STEREO_ACTIVE)));
        }
    }

    bool ClaimWritablePair(std::uint32_t& slotIndex) noexcept {
        for (std::uint32_t attempt = 0;
             attempt < FEARVR_SLOTS_PER_EYE; ++attempt) {
            const std::uint32_t candidate =
                (nextSlot_ + attempt) % FEARVR_SLOTS_PER_EYE;
            FearVrSlot& left =
                shared_->slot[FEARVR_EYE_LEFT][candidate];
            FearVrSlot& right =
                shared_->slot[FEARVR_EYE_RIGHT][candidate];
            if (InterlockedCompareExchange(
                    AtomicState(left), FEARVR_SLOT_WRITING,
                    FEARVR_SLOT_EMPTY) != FEARVR_SLOT_EMPTY) {
                continue;
            }
            if (InterlockedCompareExchange(
                    AtomicState(right), FEARVR_SLOT_WRITING,
                    FEARVR_SLOT_EMPTY) != FEARVR_SLOT_EMPTY) {
                InterlockedExchange(AtomicState(left),
                                    FEARVR_SLOT_EMPTY);
                continue;
            }
            nextSlot_ = (candidate + 1) % FEARVR_SLOTS_PER_EYE;
            slotIndex = candidate;
            return true;
        }
        return false;
    }

    void ReleaseClaimedPair(std::uint32_t slotIndex) noexcept {
        for (std::uint32_t eye = 0; eye < FEARVR_EYE_COUNT; ++eye) {
            InterlockedExchange(
                AtomicState(shared_->slot[eye][slotIndex]),
                FEARVR_SLOT_EMPTY);
        }
    }

    bool PollPending() noexcept {
        if (!pending_.active) {
            return true;
        }
        for (std::uint32_t eye = 0; eye < FEARVR_EYE_COUNT; ++eye) {
            SlotResource& resource =
                resources_[eye][pending_.slotIndex];
            if (!resource.completion ||
                resource.completion->GetData(
                    nullptr, 0, D3DGETDATA_FLUSH) != S_OK) {
                return false;
            }
        }

        MemoryBarrier();
        for (std::uint32_t eye = 0; eye < FEARVR_EYE_COUNT; ++eye) {
            InterlockedExchange(
                AtomicState(shared_->slot[eye][pending_.slotIndex]),
                FEARVR_SLOT_READY);
        }
        SetEvent(frameReadyEvent_);
        if (pending_.frameId == 1 ||
            pending_.frameId % 300 == 0) {
            std::ostringstream message;
            message << "frame=" << pending_.frameId
                    << " generation=" << pending_.generation
                    << " slot=" << pending_.slotIndex;
            logger_.Write("INFO", "frame_ready", message.str());
        }
        pending_ = {};
        return true;
    }

    bool EnsureCompletionWorker() noexcept {
        if (completionWorker_.joinable()) {
            return true;
        }
        try {
            completionWorkerStop_ = false;
            completionWorker_ =
                std::thread([this]() noexcept { CompletionWorkerLoop(); });
            logger_.Write(
                "INFO", "async_frame_completion",
                "D3D9Ex upload queries complete outside FEAR's Present "
                "thread.");
            return true;
        } catch (...) {
            logger_.Write(
                "WARN", "async_frame_completion_failed",
                "Could not create the D3D9Ex completion worker; frame "
                "publication falls back to Present polling.");
            return false;
        }
    }

    void CompletionWorkerLoop() noexcept {
        std::unique_lock<std::mutex> lock(mutex_);
        for (;;) {
            completionWorkerWake_.wait(
                lock, [this]() noexcept {
                    return completionWorkerStop_ || pending_.active;
                });
            if (completionWorkerStop_) {
                return;
            }
            while (pending_.active && !completionWorkerStop_) {
                if (PollPending()) {
                    break;
                }
                // Release the bridge lock between short query polls so Reset
                // and the next Present can proceed. 250 us is well below one
                // 90-Hz interval but avoids a busy CPU core.
                completionWorkerWake_.wait_for(
                    lock, std::chrono::microseconds(250));
            }
        }
    }

    void LogHresult(const char* event, HRESULT result) noexcept {
        std::ostringstream message;
        message << "HRESULT=0x" << std::hex << std::uppercase
                << static_cast<std::uint32_t>(result);
        logger_.Write("ERROR", event, message.str());
    }

    void LogWin32(const char* event, DWORD error) noexcept {
        logger_.Write("ERROR", event,
                      "Win32=" + std::to_string(error));
    }

    CommandLineConfig config_;
    Logger logger_;
    std::mutex mutex_;
    std::condition_variable completionWorkerWake_;
    std::thread completionWorker_;
    bool completionWorkerStop_{false};
    std::mutex asyncTransferMutex_;
    std::condition_variable asyncTransferWake_;
    std::thread asyncTransferWorker_;
    bool asyncTransferStop_{false};
    HANDLE mapping_{nullptr};
    HANDLE frameReadyEvent_{nullptr};
    HANDLE slotConsumedEvent_{nullptr};
    FearVrSharedHeader* shared_{nullptr};
    IDirect3DDevice9* device_{nullptr};
    IDirect3DDevice9* multithreadedDevice_{nullptr};
    ComPtr<IDirect3D9Ex> bridgeDirect3DEx_;
    ComPtr<IDirect3DDevice9Ex> bridgeDeviceEx_;
    ComPtr<IDirect3DDevice9> asyncGameDevice_;
    ComPtr<IDirect3DTexture9> gameCaptureTexture_;
    ComPtr<IDirect3DSurface9> gameCapture_;
    ComPtr<IDirect3DSurface9> gameReadback_;
    ComPtr<IDirect3DSurface9> rightWorldReadback_;
    ComPtr<IDirect3DSurface9> presentedReadback_;
    ComPtr<IDirect3DSurface9> bridgeUpload_;
    std::array<ComPtr<IDirect3DTexture9>, FEARVR_EYE_COUNT>
        stereoCaptureTexture_{};
    std::array<ComPtr<IDirect3DSurface9>, FEARVR_EYE_COUNT>
        stereoCapture_{};
    GpuHudCompositor hudCompositor_;
    std::array<std::array<SlotResource, FEARVR_SLOTS_PER_EYE>,
               FEARVR_EYE_COUNT>
        resources_{};
    std::array<AsyncTransferJob, FEARVR_SLOTS_PER_EYE>
        asyncJobs_{};
    std::array<std::uint32_t, FEARVR_SLOTS_PER_EYE>
        asyncQueue_{};
    std::size_t asyncQueueHead_{0};
    std::size_t asyncQueueTail_{0};
    std::size_t asyncQueueCount_{0};
    PendingFrame pending_{};
    UINT width_{0};
    UINT height_{0};
    UINT sourceWidth_{0};
    UINT sourceHeight_{0};
    std::uint32_t nextSlot_{0};
    std::uint64_t frameId_{0};
    std::uint64_t generation_{0};
    std::uint64_t droppedFrames_{0};
    std::uint64_t transferSamples_{0};
    std::uint64_t transferTotalMicroseconds_{0};
    std::uint64_t transferMaxMicroseconds_{0};
    std::uint64_t asyncQueueDrops_{0};
    std::uint64_t asyncSupersededFrames_{0};
    std::uint64_t asyncTransferSamples_{0};
    std::uint64_t asyncTransferTotalMicroseconds_{0};
    std::uint64_t asyncTransferMaxMicroseconds_{0};
    std::uint64_t asyncReadbackTotalMicroseconds_{0};
    std::uint64_t asyncReadbackMaxMicroseconds_{0};
    std::uint64_t stereoFrameId_{0};
    FearVrGameCameraSample stereoCameraSample_{};
    std::uint64_t stereoFrames_{0};
    std::uint64_t stereoHudFrames_{0};
    std::uint64_t gameAdapterLuid_{0};
    std::uint64_t lastHostHeartbeat_{0};
    ULONGLONG lastHostHeartbeatTick_{0};
    ULONGLONG nextResourceRetryTick_{0};
    HWND companionWindow_{nullptr};
    TransferMode transferMode_{TransferMode::None};
    bool resourcesReady_{false};
    bool asyncTransferEnabled_{false};
    bool deviceMetadataReady_{false};
    bool hostConnected_{false};
    bool adapterMatchLogged_{false};
    bool adapterMismatchLogged_{false};
    bool multithreadedDeviceLogged_{false};
    std::array<bool, FEARVR_EYE_COUNT> stereoEyeCaptured_{};
    bool stereoFrameReady_{false};
    bool stereoHudFlatFrame_{false};
    bool stereoAccepting_{false};
    bool stereoIncompleteLogged_{false};
    bool stereoKeyWasDown_{false};
    bool recenterKeyWasDown_{false};
    bool comfortKeyWasDown_{false};
    bool comfortModeEnabled_{false};
    bool menuActive_{false};
    std::uint32_t recenterGeneration_{0};
    std::uint32_t fovScalePercent_{
        FEARVR_FOV_SCALE_DEFAULT_PERCENT};
    StereoToggleCallback stereoToggleCallback_{nullptr};
};

Bridge& GetBridge() {
    // Absichtlich prozesslebenslang: CRT-Destruktoren laufen bei DLL_DETACH
    // unter dem Loader-Lock. D3D/IPC-Cleanup darf dort nicht stattfinden;
    // Windows gibt die Prozessressourcen beim Prozessende frei.
    static Bridge* bridge = new Bridge;
    return *bridge;
}

using CreateDeviceFunction = HRESULT(STDMETHODCALLTYPE*)(
    IDirect3D9*, UINT, D3DDEVTYPE, HWND, DWORD,
    D3DPRESENT_PARAMETERS*, IDirect3DDevice9**);
using CreateDeviceExFunction = HRESULT(STDMETHODCALLTYPE*)(
    IDirect3D9Ex*, UINT, D3DDEVTYPE, HWND, DWORD,
    D3DPRESENT_PARAMETERS*, D3DDISPLAYMODEEX*, IDirect3DDevice9Ex**);
using ResetFunction =
    HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*,
                                D3DPRESENT_PARAMETERS*);
using PresentFunction =
    HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice9*, const RECT*,
                                const RECT*, HWND, const RGNDATA*);

void ForceHighQualitySource(D3DPRESENT_PARAMETERS* parameters) noexcept {
    // Keep Retail's own display mode. The game menu relies on that mode's
    // coordinate system; overriding it breaks the flat VR menu projection.
    (void)parameters;
}

struct D3D9VtableRecord {
    void** vtable{nullptr};
    CreateDeviceFunction createDevice{nullptr};
    CreateDeviceExFunction createDeviceEx{nullptr};
};

struct DeviceVtableRecord {
    void** vtable{nullptr};
    ResetFunction reset{nullptr};
    PresentFunction present{nullptr};
};

SRWLOCK g_hookLock = SRWLOCK_INIT;
std::array<D3D9VtableRecord, 8> g_d3d9Records{};
std::array<DeviceVtableRecord, 8> g_deviceRecords{};
INIT_ONCE g_lateHookOnce = INIT_ONCE_STATIC_INIT;
volatile LONG g_lateHooksActive = FALSE;
BOOL g_lateHookResult = FALSE;
ResetFunction g_lateReset = nullptr;
PresentFunction g_latePresent = nullptr;

HRESULT STDMETHODCALLTYPE HookCreateDevice(
    IDirect3D9* self, UINT adapter, D3DDEVTYPE deviceType,
    HWND focusWindow, DWORD behaviorFlags,
    D3DPRESENT_PARAMETERS* parameters,
    IDirect3DDevice9** output);
HRESULT STDMETHODCALLTYPE HookCreateDeviceEx(
    IDirect3D9Ex* self, UINT adapter, D3DDEVTYPE deviceType,
    HWND focusWindow, DWORD behaviorFlags,
    D3DPRESENT_PARAMETERS* parameters,
    D3DDISPLAYMODEEX* fullscreenMode,
    IDirect3DDevice9Ex** output);
HRESULT STDMETHODCALLTYPE HookReset(IDirect3DDevice9* self,
                                     D3DPRESENT_PARAMETERS* parameters);
HRESULT STDMETHODCALLTYPE HookPresent(IDirect3DDevice9* self,
                                       const RECT* source,
                                       const RECT* destination,
                                       HWND overrideWindow,
                                       const RGNDATA* dirtyRegion);
HRESULT STDMETHODCALLTYPE HookLateReset(
    IDirect3DDevice9* self, D3DPRESENT_PARAMETERS* parameters);
HRESULT STDMETHODCALLTYPE HookLatePresent(
    IDirect3DDevice9* self, const RECT* source,
    const RECT* destination, HWND overrideWindow,
    const RGNDATA* dirtyRegion);

bool ReplaceVtableEntry(void** vtable, std::size_t index,
                        void* replacement) noexcept {
    DWORD oldProtection = 0;
    if (!VirtualProtect(&vtable[index], sizeof(void*),
                        PAGE_EXECUTE_READWRITE, &oldProtection)) {
        return false;
    }
    InterlockedExchangePointer(
        reinterpret_cast<void* volatile*>(&vtable[index]), replacement);
    DWORD ignored = 0;
    VirtualProtect(&vtable[index], sizeof(void*), oldProtection, &ignored);
    FlushInstructionCache(GetCurrentProcess(), &vtable[index],
                          sizeof(void*));
    return true;
}

void PatchDevice(IDirect3DDevice9* device) noexcept {
    if (device == nullptr) {
        return;
    }
    if (InterlockedCompareExchange(&g_lateHooksActive, FALSE, FALSE) !=
        FALSE) {
        return;
    }
    void** vtable = *reinterpret_cast<void***>(device);
    AcquireSRWLockExclusive(&g_hookLock);
    for (const DeviceVtableRecord& record : g_deviceRecords) {
        if (record.vtable == vtable) {
            ReleaseSRWLockExclusive(&g_hookLock);
            return;
        }
    }
    for (DeviceVtableRecord& record : g_deviceRecords) {
        if (record.vtable == nullptr) {
            record.vtable = vtable;
            record.reset =
                reinterpret_cast<ResetFunction>(vtable[16]);
            record.present =
                reinterpret_cast<PresentFunction>(vtable[17]);
            ReplaceVtableEntry(vtable, 16,
                               reinterpret_cast<void*>(&HookReset));
            ReplaceVtableEntry(vtable, 17,
                               reinterpret_cast<void*>(&HookPresent));
            break;
        }
    }
    ReleaseSRWLockExclusive(&g_hookLock);
}

void PatchD3D9(IDirect3D9* direct3D, bool hasEx) noexcept {
    if (direct3D == nullptr) {
        return;
    }
    void** vtable = *reinterpret_cast<void***>(direct3D);
    AcquireSRWLockExclusive(&g_hookLock);
    for (const D3D9VtableRecord& record : g_d3d9Records) {
        if (record.vtable == vtable) {
            ReleaseSRWLockExclusive(&g_hookLock);
            return;
        }
    }
    for (D3D9VtableRecord& record : g_d3d9Records) {
        if (record.vtable == nullptr) {
            record.vtable = vtable;
            record.createDevice = reinterpret_cast<CreateDeviceFunction>(
                vtable[16]);
            ReplaceVtableEntry(vtable, 16,
                               reinterpret_cast<void*>(&HookCreateDevice));
            if (hasEx) {
                record.createDeviceEx =
                    reinterpret_cast<CreateDeviceExFunction>(vtable[20]);
                ReplaceVtableEntry(
                    vtable, 20,
                    reinterpret_cast<void*>(&HookCreateDeviceEx));
            }
            break;
        }
    }
    ReleaseSRWLockExclusive(&g_hookLock);
}

D3D9VtableRecord FindD3D9Record(void** vtable) noexcept {
    D3D9VtableRecord result;
    AcquireSRWLockShared(&g_hookLock);
    for (const D3D9VtableRecord& record : g_d3d9Records) {
        if (record.vtable == vtable) {
            result = record;
            break;
        }
    }
    ReleaseSRWLockShared(&g_hookLock);
    return result;
}

DeviceVtableRecord FindDeviceRecord(void** vtable) noexcept {
    DeviceVtableRecord result;
    AcquireSRWLockShared(&g_hookLock);
    for (const DeviceVtableRecord& record : g_deviceRecords) {
        if (record.vtable == vtable) {
            result = record;
            break;
        }
    }
    ReleaseSRWLockShared(&g_hookLock);
    return result;
}

HRESULT STDMETHODCALLTYPE HookCreateDevice(
    IDirect3D9* self, UINT adapter, D3DDEVTYPE deviceType,
    HWND focusWindow, DWORD behaviorFlags,
    D3DPRESENT_PARAMETERS* parameters,
    IDirect3DDevice9** output) {
    const D3D9VtableRecord record =
        FindD3D9Record(*reinterpret_cast<void***>(self));
    if (record.createDevice == nullptr) {
        return D3DERR_INVALIDCALL;
    }
    ForceHighQualitySource(parameters);
    behaviorFlags |= D3DCREATE_MULTITHREADED;
    const HRESULT result = record.createDevice(
        self, adapter, deviceType, focusWindow, behaviorFlags, parameters,
        output);
    if (SUCCEEDED(result) && output != nullptr && *output != nullptr) {
        GetBridge().NoteMultithreadedDevice(*output);
        PatchDevice(*output);
    }
    return result;
}

HRESULT STDMETHODCALLTYPE HookCreateDeviceEx(
    IDirect3D9Ex* self, UINT adapter, D3DDEVTYPE deviceType,
    HWND focusWindow, DWORD behaviorFlags,
    D3DPRESENT_PARAMETERS* parameters,
    D3DDISPLAYMODEEX* fullscreenMode,
    IDirect3DDevice9Ex** output) {
    const D3D9VtableRecord record =
        FindD3D9Record(*reinterpret_cast<void***>(self));
    if (record.createDeviceEx == nullptr) {
        return D3DERR_INVALIDCALL;
    }
    ForceHighQualitySource(parameters);
    behaviorFlags |= D3DCREATE_MULTITHREADED;
    const HRESULT result = record.createDeviceEx(
        self, adapter, deviceType, focusWindow, behaviorFlags, parameters,
        fullscreenMode, output);
    if (SUCCEEDED(result) && output != nullptr && *output != nullptr) {
        GetBridge().NoteMultithreadedDevice(*output);
        PatchDevice(*output);
    }
    return result;
}

HRESULT STDMETHODCALLTYPE HookReset(IDirect3DDevice9* self,
                                     D3DPRESENT_PARAMETERS* parameters) {
    const DeviceVtableRecord record =
        FindDeviceRecord(*reinterpret_cast<void***>(self));
    if (record.reset == nullptr) {
        return D3DERR_INVALIDCALL;
    }
    ForceHighQualitySource(parameters);
    GetBridge().BeforeReset(parameters);
    const HRESULT result = record.reset(self, parameters);
    GetBridge().AfterReset(self, result);
    return result;
}

HRESULT STDMETHODCALLTYPE HookPresent(IDirect3DDevice9* self,
                                       const RECT* source,
                                       const RECT* destination,
                                       HWND overrideWindow,
                                       const RGNDATA* dirtyRegion) {
    const DeviceVtableRecord record =
        FindDeviceRecord(*reinterpret_cast<void***>(self));
    if (record.present == nullptr) {
        return D3DERR_INVALIDCALL;
    }
    GetBridge().CapturePresent(self);
    return record.present(self, source, destination, overrideWindow,
                          dirtyRegion);
}

HRESULT STDMETHODCALLTYPE HookLateReset(
    IDirect3DDevice9* self, D3DPRESENT_PARAMETERS* parameters) {
    if (g_lateReset == nullptr) {
        return D3DERR_INVALIDCALL;
    }
    ForceHighQualitySource(parameters);
    GetBridge().BeforeReset(parameters);
    const HRESULT result = g_lateReset(self, parameters);
    GetBridge().AfterReset(self, result);
    return result;
}

HRESULT STDMETHODCALLTYPE HookLatePresent(
    IDirect3DDevice9* self, const RECT* source,
    const RECT* destination, HWND overrideWindow,
    const RGNDATA* dirtyRegion) {
    if (g_latePresent == nullptr) {
        return D3DERR_INVALIDCALL;
    }
    GetBridge().CapturePresent(self);
    return g_latePresent(self, source, destination, overrideWindow,
                         dirtyRegion);
}

BOOL CALLBACK InstallLateHooksOnce(PINIT_ONCE, PVOID, PVOID*) {
    auto& bridge = GetBridge();
    using Direct3DCreate9Function = IDirect3D9*(WINAPI*)(UINT);
    const auto createDirect3D =
        ResolveSystemD3D9<Direct3DCreate9Function>("Direct3DCreate9");
    if (createDirect3D == nullptr) {
        bridge.LogHookStatus(
            "ERROR", "late_hook_system_d3d9_failed",
            "Direct3DCreate9 could not be resolved from System32.");
        return TRUE;
    }

    const HWND window = CreateWindowExW(
        0, L"STATIC", L"FearVrD3D9HookProbe", WS_OVERLAPPED,
        0, 0, 32, 32, nullptr, nullptr, GetModuleHandleW(nullptr),
        nullptr);
    if (window == nullptr) {
        bridge.LogHookStatus(
            "ERROR", "late_hook_window_failed",
            "Win32=" + std::to_string(GetLastError()));
        return TRUE;
    }

    ComPtr<IDirect3D9> direct3D;
    direct3D.Attach(createDirect3D(D3D_SDK_VERSION));
    if (!direct3D) {
        DestroyWindow(window);
        bridge.LogHookStatus(
            "ERROR", "late_hook_d3d9_failed",
            "System Direct3DCreate9 returned null.");
        return TRUE;
    }

    D3DPRESENT_PARAMETERS parameters{};
    parameters.Windowed = TRUE;
    parameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
    parameters.hDeviceWindow = window;
    parameters.BackBufferWidth = 32;
    parameters.BackBufferHeight = 32;
    parameters.BackBufferFormat = D3DFMT_UNKNOWN;

    ComPtr<IDirect3DDevice9> device;
    const HRESULT createResult = direct3D->CreateDevice(
        D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE,
        &parameters, device.GetAddressOf());
    if (FAILED(createResult) || !device) {
        DestroyWindow(window);
        std::ostringstream message;
        message << "HRESULT=0x" << std::hex << std::uppercase
                << static_cast<std::uint32_t>(createResult);
        bridge.LogHookStatus(
            "ERROR", "late_hook_device_failed", message.str());
        return TRUE;
    }

    void** vtable = *reinterpret_cast<void***>(device.Get());
    void* const resetTarget = vtable[16];
    void* const presentTarget = vtable[17];
    if (resetTarget == reinterpret_cast<void*>(&HookReset) &&
        presentTarget == reinterpret_cast<void*>(&HookPresent)) {
        g_lateHookResult = TRUE;
        bridge.LogHookStatus(
            "INFO", "late_hooks_not_needed",
            "The device vtable is already hooked by the proxy path.");
        device.Reset();
        direct3D.Reset();
        DestroyWindow(window);
        return TRUE;
    }

    const MH_STATUS initialize = MH_Initialize();
    if (initialize != MH_OK &&
        initialize != MH_ERROR_ALREADY_INITIALIZED) {
        bridge.LogHookStatus(
            "ERROR", "late_hook_initialize_failed",
            MH_StatusToString(initialize));
        device.Reset();
        direct3D.Reset();
        DestroyWindow(window);
        return TRUE;
    }

    MH_STATUS status = MH_CreateHook(
        resetTarget, reinterpret_cast<void*>(&HookLateReset),
        reinterpret_cast<void**>(&g_lateReset));
    if (status != MH_OK) {
        bridge.LogHookStatus(
            "ERROR", "late_hook_reset_create_failed",
            MH_StatusToString(status));
        device.Reset();
        direct3D.Reset();
        DestroyWindow(window);
        return TRUE;
    }

    status = MH_CreateHook(
        presentTarget, reinterpret_cast<void*>(&HookLatePresent),
        reinterpret_cast<void**>(&g_latePresent));
    if (status != MH_OK) {
        MH_RemoveHook(resetTarget);
        g_lateReset = nullptr;
        bridge.LogHookStatus(
            "ERROR", "late_hook_present_create_failed",
            MH_StatusToString(status));
        device.Reset();
        direct3D.Reset();
        DestroyWindow(window);
        return TRUE;
    }

    status = MH_QueueEnableHook(resetTarget);
    if (status == MH_OK) {
        status = MH_QueueEnableHook(presentTarget);
    }
    if (status == MH_OK) {
        status = MH_ApplyQueued();
    }
    if (status != MH_OK) {
        MH_RemoveHook(presentTarget);
        MH_RemoveHook(resetTarget);
        g_latePresent = nullptr;
        g_lateReset = nullptr;
        bridge.LogHookStatus(
            "ERROR", "late_hook_enable_failed",
            MH_StatusToString(status));
        device.Reset();
        direct3D.Reset();
        DestroyWindow(window);
        return TRUE;
    }

    InterlockedExchange(&g_lateHooksActive, TRUE);
    g_lateHookResult = TRUE;
    bridge.LogHookStatus(
        "INFO", "late_hooks_installed",
        "System D3D9 Reset and Present hooks are active.");
    device.Reset();
    direct3D.Reset();
    DestroyWindow(window);
    return TRUE;
}

} // namespace

void OnDirect3D9Created(IDirect3D9* direct3D) noexcept {
    PatchD3D9(direct3D, false);
}

void OnDirect3D9ExCreated(IDirect3D9Ex* direct3D) noexcept {
    PatchD3D9(direct3D, true);
}

BOOL InstallLateD3D9Hooks() noexcept {
    if (!InitOnceExecuteOnce(
            &g_lateHookOnce, InstallLateHooksOnce, nullptr, nullptr)) {
        return FALSE;
    }
    return g_lateHookResult;
}

BOOL IsHostConnected() noexcept {
    return GetBridge().IsConnected();
}

BOOL IsStereoAvailable() noexcept {
    return GetBridge().StereoAvailable();
}

BOOL IsStereoEnabled() noexcept {
    return GetBridge().StereoEnabled();
}

void SetStereoEnabled(BOOL enabled) noexcept {
    GetBridge().SetStereoEnabled(enabled);
}

void SetFovScalePercent(std::uint32_t percent) noexcept {
    GetBridge().SetFovScalePercent(percent);
}

BOOL IsTranslationEnabled() noexcept {
    return GetBridge().TranslationEnabled();
}

void SetTranslationEnabled(BOOL enabled) noexcept {
    GetBridge().SetTranslationEnabled(enabled);
}

BOOL IsStereoHudEnabled() noexcept {
    return GetBridge().StereoHudEnabled();
}

void SetStereoHudEnabled(BOOL enabled) noexcept {
    GetBridge().SetStereoHudEnabled(enabled);
}

BOOL IsComfortModeEnabled() noexcept {
    return GetBridge().ComfortModeEnabled();
}

void SetComfortModeEnabled(BOOL enabled) noexcept {
    GetBridge().SetComfortModeEnabled(enabled);
}

void SetMenuActive(BOOL active) noexcept {
    GetBridge().SetMenuActive(active);
}

void RequestRecenter() noexcept {
    GetBridge().RequestRecenter();
}

BOOL IsFlatPanelActive() noexcept {
    return GetBridge().FlatPanelActive();
}

void RegisterStereoToggleCallback(
    StereoToggleCallback callback) noexcept {
    GetBridge().RegisterStereoToggle(callback);
}

BOOL GetRenderRequest(FearVrRenderRequest* request) noexcept {
    return GetBridge().ReadRenderRequest(request);
}

BOOL GetInputState(FearVrInputState* input) noexcept {
    return GetBridge().ReadInputState(input);
}

BOOL SubmitHapticRequest(
    const FearVrHapticRequest* request) noexcept {
    return GetBridge().WriteHapticRequest(request);
}

void BeginEye(std::uint32_t eye) noexcept {
    GetBridge().BeginStereoEye(eye);
}

void CaptureEye(std::uint32_t eye) noexcept {
    GetBridge().CaptureStereoEye(eye);
}

void EndStereoFrame(
    std::uint64_t frameId,
    const FearVrGameCameraSample* camera) noexcept {
    GetBridge().EndStereoFrame(frameId, camera);
}

void ReportHookStatus(const char* level, const char* event,
                      const char* message) noexcept {
    GetBridge().LogHookStatus(
        level == nullptr ? "INFO" : level,
        event == nullptr ? "stereo_hook" : event,
        message == nullptr ? "" : message);
}

} // namespace fearvr

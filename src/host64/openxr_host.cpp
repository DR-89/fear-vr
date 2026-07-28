#include "openxr_host.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <Windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include "fearvr-version.h"
#include "head_tracking_math.h"
#include "ipc_bridge.h"
#include "protocol_utils.h"
#include "stereo_math.h"
#include "texture_renderer.h"
#include "xr_input.h"
#include "xr_session_state.h"

namespace fearvr {
namespace {

using Microsoft::WRL::ComPtr;

std::atomic_bool g_stopRequested{false};

// Quest 3's native panel target is 2064x2208 per eye. Some OpenXR runtimes
// advertise a lower recommendation (for example when their quality slider is
// below 100%). Request at least this target while respecting the runtime's
// advertised maximum dimensions; runtimes already offering a higher target
// (such as VDXR's 2688x2880) remain untouched.
constexpr std::uint32_t kQuest3EyeWidth = 2064;
constexpr std::uint32_t kQuest3EyeHeight = 2208;

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
               << time.wMinute << std::setw(2) << time.wSecond;
    } else {
        output << '-' << std::setw(2) << time.wMonth << '-' << std::setw(2)
               << time.wDay << 'T' << std::setw(2) << time.wHour << ':'
               << std::setw(2) << time.wMinute << ':' << std::setw(2)
               << time.wSecond << '.' << std::setw(3) << time.wMilliseconds
               << 'Z';
    }
    return output.str();
}

// Geschrieben wird auf einem eigenen Faden.
//
// Vorher stand in `Write` beides: ein `flush` auf die Logdatei und ein
// `std::endl` auf die Konsole — zwei synchrone Ein-/Ausgaben mitten im
// 90-Hz-Takt, denn der Bildfaden loggt selbst (Eingabeproben, Sitzungs-
// wechsel, Slotimporte). Ein Konsolenschreibvorgang unter Windows kostet
// Millisekunden, wenn die Gegenstelle gerade nicht liest. Im Lauf vom
// 28.07.2026 blieben nach dem Beseitigen der Treiberaufrufe noch 216 Bilder
// ueber 8 ms uebrig, bei 335 Logzeilen im selben Zeitraum.
//
// Jetzt baut der Aufrufer nur die Zeile und haengt sie an; geschrieben und
// geleert wird hinten. Der Faden leert nach jedem Schwung, ein harter
// Absturz verliert also hoechstens die letzten Millisekunden.
class Logger {
public:
    explicit Logger(const std::filesystem::path& directory) {
        std::error_code error;
        std::filesystem::create_directories(directory, error);
        if (error) {
            throw std::runtime_error("Logverzeichnis konnte nicht erstellt "
                                     "werden: " +
                                     error.message());
        }
        path_ = directory / ("host-" + UtcTimestamp(true) + ".log");
        stream_.open(path_, std::ios::out | std::ios::trunc);
        if (!stream_) {
            throw std::runtime_error("Hostlog konnte nicht geoeffnet werden.");
        }
        worker_ = std::thread([this] { Drain(); });
    }

    ~Logger() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stopping_ = true;
        }
        signal_.notify_one();
        if (worker_.joinable()) {
            worker_.join();
        }
    }

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void Write(const char* level, const char* event,
               const std::string& message) {
        // Der Zeitstempel gehoert hierher, nicht auf den Schreibfaden: sonst
        // stuende in der Zeile, wann sie geschrieben wurde, statt wann das
        // Ereignis eintrat.
        Entry entry;
        entry.file = "{\"time\":\"" + UtcTimestamp(false) +
                     "\",\"level\":\"" + JsonEscape(level) +
                     "\",\"event\":\"" + JsonEscape(event) +
                     "\",\"message\":\"" + JsonEscape(message) + "\"}";
        entry.console = std::string("[") + level + "] " + event + ": " +
                        message;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            pending_.push_back(std::move(entry));
        }
        signal_.notify_one();
    }

    [[nodiscard]] const std::filesystem::path& Path() const noexcept {
        return path_;
    }

private:
    struct Entry {
        std::string file;
        std::string console;
    };

    void Drain() {
        std::vector<Entry> batch;
        for (;;) {
            {
                std::unique_lock<std::mutex> lock(mutex_);
                signal_.wait(lock, [this] {
                    return stopping_ || !pending_.empty();
                });
                if (pending_.empty() && stopping_) {
                    return;
                }
                batch.swap(pending_);
            }
            for (const Entry& entry : batch) {
                stream_ << entry.file << '\n';
                std::cout << entry.console << '\n';
            }
            batch.clear();
            stream_.flush();
            std::cout.flush();
        }
    }

    std::filesystem::path path_;
    std::ofstream stream_;
    std::mutex mutex_;
    std::condition_variable signal_;
    std::vector<Entry> pending_;
    std::thread worker_;
    bool stopping_{false};
};

class XrException final : public std::runtime_error {
public:
    XrException(XrResult result, std::string message)
        : std::runtime_error(std::move(message)), result_(result) {}

    [[nodiscard]] XrResult Result() const noexcept { return result_; }

private:
    XrResult result_;
};

std::string XrResultText(XrInstance instance, XrResult result) {
    if (instance != XR_NULL_HANDLE) {
        std::array<char, XR_MAX_RESULT_STRING_SIZE> text{};
        if (XR_SUCCEEDED(xrResultToString(instance, result, text.data()))) {
            return text.data();
        }
    }
    switch (result) {
    case XR_ERROR_RUNTIME_UNAVAILABLE:
        return "XR_ERROR_RUNTIME_UNAVAILABLE";
    case XR_ERROR_API_VERSION_UNSUPPORTED:
        return "XR_ERROR_API_VERSION_UNSUPPORTED";
    case XR_ERROR_INITIALIZATION_FAILED:
        return "XR_ERROR_INITIALIZATION_FAILED";
    case XR_ERROR_FORM_FACTOR_UNAVAILABLE:
        return "XR_ERROR_FORM_FACTOR_UNAVAILABLE";
    case XR_ERROR_GRAPHICS_DEVICE_INVALID:
        return "XR_ERROR_GRAPHICS_DEVICE_INVALID";
    default:
        break;
    }
    return "XrResult(" + std::to_string(static_cast<std::int32_t>(result)) +
           ')';
}

void CheckXr(XrInstance instance, XrResult result, const char* operation) {
    if (XR_FAILED(result)) {
        throw XrException(result, std::string(operation) + " fehlgeschlagen: " +
                                     XrResultText(instance, result));
    }
}

void CheckHr(HRESULT result, const char* operation) {
    if (FAILED(result)) {
        std::ostringstream message;
        message << operation << " fehlgeschlagen: HRESULT=0x" << std::hex
                << std::uppercase << static_cast<std::uint32_t>(result);
        throw std::runtime_error(message.str());
    }
}

std::string WideToUtf8(const wchar_t* value) {
    if (value == nullptr || value[0] == L'\0') {
        return {};
    }
    const int sourceLength = lstrlenW(value);
    const int required = WideCharToMultiByte(CP_UTF8, 0, value, sourceLength,
                                             nullptr, 0, nullptr, nullptr);
    if (required <= 0) {
        return {};
    }
    std::string converted(static_cast<std::size_t>(required), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value, sourceLength, converted.data(),
                        required, nullptr, nullptr);
    return converted;
}

std::string LuidText(const LUID& luid) {
    std::ostringstream output;
    output << "0x" << std::hex << std::uppercase
           << static_cast<std::uint32_t>(luid.HighPart) << ':'
           << static_cast<std::uint32_t>(luid.LowPart);
    return output.str();
}

const char* SessionStateName(XrSessionState state) {
    switch (state) {
    case XR_SESSION_STATE_UNKNOWN:
        return "UNKNOWN";
    case XR_SESSION_STATE_IDLE:
        return "IDLE";
    case XR_SESSION_STATE_READY:
        return "READY";
    case XR_SESSION_STATE_SYNCHRONIZED:
        return "SYNCHRONIZED";
    case XR_SESSION_STATE_VISIBLE:
        return "VISIBLE";
    case XR_SESSION_STATE_FOCUSED:
        return "FOCUSED";
    case XR_SESSION_STATE_STOPPING:
        return "STOPPING";
    case XR_SESSION_STATE_LOSS_PENDING:
        return "LOSS_PENDING";
    case XR_SESSION_STATE_EXITING:
        return "EXITING";
    default:
        return "UNRECOGNIZED";
    }
}

XrLifecycleState ToLifecycleState(XrSessionState state) {
    switch (state) {
    case XR_SESSION_STATE_IDLE:
        return XrLifecycleState::Idle;
    case XR_SESSION_STATE_READY:
        return XrLifecycleState::Ready;
    case XR_SESSION_STATE_SYNCHRONIZED:
        return XrLifecycleState::Synchronized;
    case XR_SESSION_STATE_VISIBLE:
        return XrLifecycleState::Visible;
    case XR_SESSION_STATE_FOCUSED:
        return XrLifecycleState::Focused;
    case XR_SESSION_STATE_STOPPING:
        return XrLifecycleState::Stopping;
    case XR_SESSION_STATE_LOSS_PENDING:
        return XrLifecycleState::LossPending;
    case XR_SESSION_STATE_EXITING:
        return XrLifecycleState::Exiting;
    default:
        return XrLifecycleState::Unknown;
    }
}

struct Swapchain {
    XrSwapchain handle{XR_NULL_HANDLE};
    std::int32_t width{0};
    std::int32_t height{0};
    std::vector<XrSwapchainImageD3D11KHR> images;
    // Je Swapchain-Bild eine Rendersicht, einmal angelegt. Die Bildmenge
    // steht nach dem Aufzaehlen fest; sie pro Bild neu zu erzeugen waere eine
    // Treiberallokation im 90-Hz-Takt.
    std::vector<ComPtr<ID3D11RenderTargetView>> renderTargets;
};

struct RenderPoseSample {
    std::uint64_t frameId{0};
    std::array<XrPosef, FEARVR_EYE_COUNT> pose{};
    std::array<XrFovf, FEARVR_EYE_COUNT> fov{};
};

constexpr std::size_t kRenderPoseHistorySize = 256;

// Ab hier ist ein Bild verloren: 8 ms von 11,1 ms bei 90 Hz. Was darueber
// liegt, schafft den naechsten Takt nicht mehr.
constexpr std::uint64_t kLongFrameMicroseconds = 8000;

using SteadyClock = std::chrono::steady_clock;

std::uint64_t MicrosecondsSince(
    const SteadyClock::time_point start) noexcept {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            SteadyClock::now() - start)
            .count());
}

enum class LoopResult {
    Exit,
    RestartSession,
};

class Host {
public:
    explicit Host(const OpenXrHostOptions& options)
        : options_(options), logger_(options.logDirectory) {
        logger_.Write("INFO", "host_start",
                      std::string("version=") + FEARVR_VERSION_STRING +
                          " git=" + FEARVR_GIT_HASH +
                          " log=" + logger_.Path().u8string() +
                          " handles=" + std::to_string(CurrentHandleCount()));
    }

    ~Host() {
        DestroySessionResources();
        xrInput_.reset();
        ipcBridge_.reset();
        deviceContext_.Reset();
        device_.Reset();
        adapter_.Reset();
        if (instance_ != XR_NULL_HANDLE) {
            xrDestroyInstance(instance_);
            instance_ = XR_NULL_HANDLE;
        }
    }

    int Run() {
        try {
            CreateInstance();
            xrInput_ = std::make_unique<XrInput>(
                [this](const char* level, const char* event,
                       const std::string& message) {
                    logger_.Write(level, event, message);
                });
            xrInput_->Initialize(
                instance_, genericControllerExtensionEnabled_);
            CreateSystemAndDevice();

            bool restartSession = false;
            do {
                restartSession = false;
                CreateSessionResources();
                logger_.Write(
                    "INFO", "xr_ready",
                    "Session, Reference-Space und zwei Swapchains bereit.");

                if (options_.validateOnly) {
                    logger_.Write("INFO", "validation_complete",
                                  "OpenXR-/D3D11-Initialisierung erfolgreich.");
                    return 0;
                }

                const LoopResult result = RunSessionLoop();
                restartSession = result == LoopResult::RestartSession &&
                                 !g_stopRequested.load();
                DestroySessionResources();
                if (restartSession) {
                    logger_.Write("WARN", "session_restart",
                                  "XR_SESSION_LOSS_PENDING: Session wird neu "
                                  "erstellt.");
                    std::this_thread::sleep_for(
                        std::chrono::milliseconds(500));
                }
            } while (restartSession);

            logger_.Write("INFO", "host_stop",
                          "OpenXR-Host sauber beendet. handles=" +
                              std::to_string(CurrentHandleCount()));
            return 0;
        } catch (const std::exception& error) {
            logger_.Write("ERROR", "host_failure", error.what());
            throw;
        }
    }

private:
    void CreateInstance() {
        std::uint32_t extensionCount = 0;
        CheckXr(XR_NULL_HANDLE,
                xrEnumerateInstanceExtensionProperties(
                    nullptr, 0, &extensionCount, nullptr),
                "xrEnumerateInstanceExtensionProperties");
        std::vector<XrExtensionProperties> extensions(extensionCount);
        for (auto& extension : extensions) {
            extension.type = XR_TYPE_EXTENSION_PROPERTIES;
            extension.next = nullptr;
        }
        CheckXr(XR_NULL_HANDLE,
                xrEnumerateInstanceExtensionProperties(
                    nullptr, extensionCount, &extensionCount,
                    extensions.data()),
                "xrEnumerateInstanceExtensionProperties");

        const auto d3d11Extension =
            std::find_if(extensions.begin(), extensions.end(), [](const auto& p) {
                return std::string(p.extensionName) ==
                       XR_KHR_D3D11_ENABLE_EXTENSION_NAME;
            });
        if (d3d11Extension == extensions.end()) {
            throw std::runtime_error(
                "Die aktive OpenXR-Runtime bietet XR_KHR_D3D11_enable nicht "
                "an.");
        }

        genericControllerExtensionEnabled_ =
            std::find_if(
                extensions.begin(), extensions.end(), [](const auto& p) {
                    return std::string(p.extensionName) ==
                           XR_KHR_GENERIC_CONTROLLER_EXTENSION_NAME;
                }) != extensions.end();
        std::vector<const char*> enabledExtensions{
            XR_KHR_D3D11_ENABLE_EXTENSION_NAME};
        if (genericControllerExtensionEnabled_) {
            enabledExtensions.push_back(
                XR_KHR_GENERIC_CONTROLLER_EXTENSION_NAME);
        }
        XrInstanceCreateInfo createInfo{XR_TYPE_INSTANCE_CREATE_INFO};
        strncpy_s(createInfo.applicationInfo.applicationName, "F.E.A.R. VR",
                  _TRUNCATE);
        createInfo.applicationInfo.applicationVersion = 1;
        strncpy_s(createInfo.applicationInfo.engineName, "fearvr-host",
                  _TRUNCATE);
        createInfo.applicationInfo.engineVersion = 1;
        // OpenXR 1.0 is sufficient for M1 and accepted by older runtimes.
        // Khronos hello_xr deliberately makes the same compatibility choice.
        createInfo.applicationInfo.apiVersion = XR_API_VERSION_1_0;
        createInfo.enabledExtensionCount =
            static_cast<std::uint32_t>(enabledExtensions.size());
        createInfo.enabledExtensionNames = enabledExtensions.data();

        CheckXr(XR_NULL_HANDLE, xrCreateInstance(&createInfo, &instance_),
                "xrCreateInstance");

        XrInstanceProperties properties{XR_TYPE_INSTANCE_PROPERTIES};
        CheckXr(instance_, xrGetInstanceProperties(instance_, &properties),
                "xrGetInstanceProperties");
        std::ostringstream runtime;
        runtime << properties.runtimeName << ' '
                << XR_VERSION_MAJOR(properties.runtimeVersion) << '.'
                << XR_VERSION_MINOR(properties.runtimeVersion) << '.'
                << XR_VERSION_PATCH(properties.runtimeVersion);
        logger_.Write("INFO", "runtime", runtime.str());
        logger_.Write(
            "INFO", "controller_fallback",
            genericControllerExtensionEnabled_
                ? "XR_KHR_generic_controller enabled."
                : "XR_KHR_generic_controller unavailable; hardware-specific "
                  "profiles remain active.");
    }

    void CreateSystemAndDevice() {
        XrSystemGetInfo systemInfo{XR_TYPE_SYSTEM_GET_INFO};
        systemInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
        CheckXr(instance_, xrGetSystem(instance_, &systemInfo, &systemId_),
                "xrGetSystem(HMD)");

        XrSystemProperties properties{XR_TYPE_SYSTEM_PROPERTIES};
        CheckXr(instance_, xrGetSystemProperties(instance_, systemId_,
                                                 &properties),
                "xrGetSystemProperties");
        std::ostringstream system;
        system << "name=" << properties.systemName
               << " vendorId=" << properties.vendorId
               << " orientationTracking="
               << static_cast<unsigned>(
                      properties.trackingProperties.orientationTracking)
               << " positionTracking="
               << static_cast<unsigned>(
                      properties.trackingProperties.positionTracking);
        logger_.Write("INFO", "xr_system", system.str());

        PFN_xrGetD3D11GraphicsRequirementsKHR getRequirements = nullptr;
        CheckXr(
            instance_,
            xrGetInstanceProcAddr(
                instance_, "xrGetD3D11GraphicsRequirementsKHR",
                reinterpret_cast<PFN_xrVoidFunction*>(&getRequirements)),
            "xrGetInstanceProcAddr(xrGetD3D11GraphicsRequirementsKHR)");
        if (getRequirements == nullptr) {
            throw std::runtime_error(
                "xrGetD3D11GraphicsRequirementsKHR ist nicht verfuegbar.");
        }

        XrGraphicsRequirementsD3D11KHR requirements{
            XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR};
        CheckXr(instance_, getRequirements(instance_, systemId_, &requirements),
                "xrGetD3D11GraphicsRequirementsKHR");

        ComPtr<IDXGIFactory1> factory;
        CheckHr(CreateDXGIFactory1(IID_PPV_ARGS(factory.ReleaseAndGetAddressOf())),
                "CreateDXGIFactory1");

        for (UINT index = 0;; ++index) {
            ComPtr<IDXGIAdapter1> candidate;
            const HRESULT result = factory->EnumAdapters1(
                index, candidate.ReleaseAndGetAddressOf());
            if (result == DXGI_ERROR_NOT_FOUND) {
                break;
            }
            CheckHr(result, "IDXGIFactory1::EnumAdapters1");

            DXGI_ADAPTER_DESC1 description{};
            CheckHr(candidate->GetDesc1(&description),
                    "IDXGIAdapter1::GetDesc1");
            if (description.AdapterLuid.HighPart ==
                    requirements.adapterLuid.HighPart &&
                description.AdapterLuid.LowPart ==
                    requirements.adapterLuid.LowPart) {
                adapter_ = candidate;
                adapterDescription_ = description;
                break;
            }
        }
        if (!adapter_) {
            throw std::runtime_error(
                "Der von OpenXR verlangte D3D11-Adapter wurde nicht gefunden: " +
                LuidText(requirements.adapterLuid));
        }

        const std::array<D3D_FEATURE_LEVEL, 6> allFeatureLevels{
            D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0,
            D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0};
        std::vector<D3D_FEATURE_LEVEL> featureLevels;
        for (const D3D_FEATURE_LEVEL level : allFeatureLevels) {
            if (level >= requirements.minFeatureLevel) {
                featureLevels.push_back(level);
            }
        }
        if (featureLevels.empty()) {
            throw std::runtime_error(
                "OpenXR fordert ein nicht unterstuetztes D3D-Feature-Level.");
        }

        UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
        if (options_.enableD3dDebug) {
            flags |= D3D11_CREATE_DEVICE_DEBUG;
        }
        D3D_FEATURE_LEVEL selectedLevel{};
        HRESULT createResult = D3D11CreateDevice(
            adapter_.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, flags,
            featureLevels.data(),
            static_cast<UINT>(featureLevels.size()), D3D11_SDK_VERSION,
            device_.ReleaseAndGetAddressOf(), &selectedLevel,
            deviceContext_.ReleaseAndGetAddressOf());
        if (createResult == DXGI_ERROR_SDK_COMPONENT_MISSING &&
            (flags & D3D11_CREATE_DEVICE_DEBUG) != 0) {
            logger_.Write(
                "WARN", "d3d_debug_unavailable",
                "D3D11-Debug-Layer fehlt; erneuter Versuch ohne Debug-Layer.");
            flags &= ~D3D11_CREATE_DEVICE_DEBUG;
            createResult = D3D11CreateDevice(
                adapter_.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, flags,
                featureLevels.data(), static_cast<UINT>(featureLevels.size()),
                D3D11_SDK_VERSION, device_.ReleaseAndGetAddressOf(),
                &selectedLevel, deviceContext_.ReleaseAndGetAddressOf());
        }
        CheckHr(createResult, "D3D11CreateDevice(OpenXR-Adapter)");

        std::ostringstream adapterMessage;
        adapterMessage << "name=" << WideToUtf8(adapterDescription_.Description)
                       << " luid=" << LuidText(adapterDescription_.AdapterLuid)
                       << " dedicatedVideoMemory="
                       << adapterDescription_.DedicatedVideoMemory
                       << " minFeatureLevel=0x" << std::hex
                       << static_cast<unsigned>(requirements.minFeatureLevel)
                       << " selectedFeatureLevel=0x"
                       << static_cast<unsigned>(selectedLevel);
        logger_.Write("INFO", "d3d11_adapter", adapterMessage.str());

        textureRenderer_.Initialize(device_.Get());
        if (options_.ipcSessionId != 0) {
            const LUID& luid = adapterDescription_.AdapterLuid;
            const std::uint64_t packedLuid =
                PackLuid(static_cast<std::uint32_t>(luid.HighPart),
                         luid.LowPart);
            ipcBridge_ = std::make_unique<IpcBridge>(
                options_.ipcSessionId, device_.Get(), deviceContext_.Get(),
                packedLuid,
                [this](const char* level, const char* event,
                       const std::string& message) {
                    logger_.Write(level, event, message);
                });
        }
    }

    void CreateSessionResources() {
        lifecycle_ = XrSessionStateMachine{};
        submittedFrames_ = 0;
        exitRequested_ = false;

        XrGraphicsBindingD3D11KHR binding{
            XR_TYPE_GRAPHICS_BINDING_D3D11_KHR};
        binding.device = device_.Get();

        XrSessionCreateInfo sessionInfo{XR_TYPE_SESSION_CREATE_INFO};
        sessionInfo.next = &binding;
        sessionInfo.systemId = systemId_;
        CheckXr(instance_, xrCreateSession(instance_, &sessionInfo, &session_),
                "xrCreateSession");
        xrInput_->Attach(session_);

        XrReferenceSpaceCreateInfo spaceInfo{
            XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
        spaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
        spaceInfo.poseInReferenceSpace.orientation.w = 1.0F;
        CheckXr(instance_,
                xrCreateReferenceSpace(session_, &spaceInfo, &appSpace_),
                "xrCreateReferenceSpace(LOCAL)");
        std::uint32_t blendModeCount = 0;
        CheckXr(instance_,
                xrEnumerateEnvironmentBlendModes(
                    instance_, systemId_,
                    XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0,
                    &blendModeCount, nullptr),
                "xrEnumerateEnvironmentBlendModes(count)");
        if (blendModeCount == 0) {
            throw std::runtime_error(
                "Die Runtime meldet keinen Environment-Blend-Modus.");
        }
        std::vector<XrEnvironmentBlendMode> blendModes(blendModeCount);
        CheckXr(instance_,
                xrEnumerateEnvironmentBlendModes(
                    instance_, systemId_,
                    XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, blendModeCount,
                    &blendModeCount, blendModes.data()),
                "xrEnumerateEnvironmentBlendModes");
        const auto opaque =
            std::find(blendModes.begin(), blendModes.end(),
                      XR_ENVIRONMENT_BLEND_MODE_OPAQUE);
        blendMode_ =
            opaque != blendModes.end() ? *opaque : blendModes.front();

        std::uint32_t viewCount = 0;
        CheckXr(instance_,
                xrEnumerateViewConfigurationViews(
                    instance_, systemId_,
                    XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &viewCount,
                    nullptr),
                "xrEnumerateViewConfigurationViews(count)");
        if (viewCount != 2) {
            throw std::runtime_error(
                "M1 erwartet genau zwei PRIMARY_STEREO-Ansichten; Runtime "
                "meldet " +
                std::to_string(viewCount) + '.');
        }
        viewConfiguration_.resize(viewCount);
        for (auto& view : viewConfiguration_) {
            view.type = XR_TYPE_VIEW_CONFIGURATION_VIEW;
            view.next = nullptr;
        }
        CheckXr(instance_,
                xrEnumerateViewConfigurationViews(
                    instance_, systemId_,
                    XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, viewCount,
                    &viewCount, viewConfiguration_.data()),
                "xrEnumerateViewConfigurationViews");

        std::uint32_t formatCount = 0;
        CheckXr(instance_,
                xrEnumerateSwapchainFormats(session_, 0, &formatCount, nullptr),
                "xrEnumerateSwapchainFormats(count)");
        std::vector<std::int64_t> formats(formatCount);
        CheckXr(instance_,
                xrEnumerateSwapchainFormats(session_, formatCount, &formatCount,
                                            formats.data()),
                "xrEnumerateSwapchainFormats");

        const std::array<DXGI_FORMAT, 4> preferredFormats{
            DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
            DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,
            DXGI_FORMAT_R8G8B8A8_UNORM,
            DXGI_FORMAT_B8G8R8A8_UNORM};
        bool foundFormat = false;
        for (const DXGI_FORMAT candidate : preferredFormats) {
            if (std::find(formats.begin(), formats.end(),
                          static_cast<std::int64_t>(candidate)) !=
                formats.end()) {
                swapchainFormat_ = candidate;
                foundFormat = true;
                break;
            }
        }
        if (!foundFormat) {
            throw std::runtime_error(
                "Keine unterstuetzte RGBA8/BGRA8-OpenXR-Swapchain.");
        }

        swapchains_.resize(viewCount);
        locatedViews_.resize(viewCount);
        projectionViews_.resize(viewCount);
        for (std::uint32_t eye = 0; eye < viewCount; ++eye) {
            locatedViews_[eye].type = XR_TYPE_VIEW;
            projectionViews_[eye].type =
                XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;

            const XrViewConfigurationView& configuration =
                viewConfiguration_[eye];
            Swapchain& swapchain = swapchains_[eye];
            const std::uint32_t requestedWidth = (std::max)(
                configuration.recommendedImageRectWidth,
                kQuest3EyeWidth);
            const std::uint32_t requestedHeight = (std::max)(
                configuration.recommendedImageRectHeight,
                kQuest3EyeHeight);
            const std::uint32_t width = (std::min)(
                requestedWidth, configuration.maxImageRectWidth);
            const std::uint32_t height = (std::min)(
                requestedHeight, configuration.maxImageRectHeight);
            swapchain.width = static_cast<std::int32_t>(width);
            swapchain.height = static_cast<std::int32_t>(height);

            XrSwapchainCreateInfo createInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO};
            createInfo.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
            createInfo.format = static_cast<std::int64_t>(swapchainFormat_);
            createInfo.sampleCount = 1;
            createInfo.width = width;
            createInfo.height = height;
            createInfo.faceCount = 1;
            createInfo.arraySize = 1;
            createInfo.mipCount = 1;
            CheckXr(instance_,
                    xrCreateSwapchain(session_, &createInfo,
                                      &swapchain.handle),
                    "xrCreateSwapchain");

            std::uint32_t imageCount = 0;
            CheckXr(instance_,
                    xrEnumerateSwapchainImages(swapchain.handle, 0, &imageCount,
                                               nullptr),
                    "xrEnumerateSwapchainImages(count)");
            swapchain.images.resize(imageCount);
            for (auto& image : swapchain.images) {
                image.type = XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR;
                image.next = nullptr;
                image.texture = nullptr;
            }
            CheckXr(
                instance_,
                xrEnumerateSwapchainImages(
                    swapchain.handle, imageCount, &imageCount,
                    reinterpret_cast<XrSwapchainImageBaseHeader*>(
                        swapchain.images.data())),
                "xrEnumerateSwapchainImages");
            swapchain.renderTargets.clear();
            swapchain.renderTargets.resize(imageCount);

            XrCompositionLayerProjectionView& projection =
                projectionViews_[eye];
            projection.subImage.swapchain = swapchain.handle;
            projection.subImage.imageRect.offset = {0, 0};
            projection.subImage.imageRect.extent = {swapchain.width,
                                                    swapchain.height};
            projection.subImage.imageArrayIndex = 0;
        }

        std::ostringstream swapchainMessage;
        swapchainMessage << "format=" << static_cast<int>(swapchainFormat_)
                         << " left=" << swapchains_[0].width << 'x'
                         << swapchains_[0].height
                         << " right=" << swapchains_[1].width << 'x'
                         << swapchains_[1].height;
        logger_.Write("INFO", "swapchains", swapchainMessage.str());
    }

    void DestroySessionResources() noexcept {
        if (xrInput_) {
            xrInput_->ResetSession();
        }
        projectionViews_.clear();
        locatedViews_.clear();
        viewConfiguration_.clear();
        for (Swapchain& swapchain : swapchains_) {
            // Erst die Sichten, dann die Bilder: eine Rendersicht haelt eine
            // Referenz auf die Swapchain-Textur.
            swapchain.renderTargets.clear();
            swapchain.images.clear();
            if (swapchain.handle != XR_NULL_HANDLE) {
                xrDestroySwapchain(swapchain.handle);
                swapchain.handle = XR_NULL_HANDLE;
            }
        }
        swapchains_.clear();
        if (appSpace_ != XR_NULL_HANDLE) {
            xrDestroySpace(appSpace_);
            appSpace_ = XR_NULL_HANDLE;
        }
        if (session_ != XR_NULL_HANDLE) {
            xrDestroySession(session_);
            session_ = XR_NULL_HANDLE;
        }
    }

    LoopResult PollEvents(bool& exitLoop) {
        XrEventDataBuffer event{XR_TYPE_EVENT_DATA_BUFFER};
        for (;;) {
            const XrResult pollResult = xrPollEvent(instance_, &event);
            if (pollResult == XR_EVENT_UNAVAILABLE) {
                break;
            }
            CheckXr(instance_, pollResult, "xrPollEvent");
            if (event.type == XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING) {
                logger_.Write(
                    "ERROR", "instance_loss_pending",
                    "OpenXR-Instance geht verloren; Host wird beendet.");
                exitLoop = true;
                return LoopResult::Exit;
            }
            if (event.type == XR_TYPE_EVENT_DATA_EVENTS_LOST) {
                const auto* lost =
                    reinterpret_cast<const XrEventDataEventsLost*>(&event);
                logger_.Write("WARN", "events_lost",
                              "count=" + std::to_string(lost->lostEventCount));
            }
            if (event.type ==
                    XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED &&
                xrInput_) {
                const auto* changed =
                    reinterpret_cast<
                        const XrEventDataInteractionProfileChanged*>(
                        &event);
                if (changed->session == session_) {
                    xrInput_->MarkInteractionProfileChanged();
                    xrInput_->LogInteractionProfiles(session_);
                }
            }
            if (event.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
                const auto* changed =
                    reinterpret_cast<const XrEventDataSessionStateChanged*>(
                        &event);
                if (changed->session == session_) {
                    logger_.Write("INFO", "session_state",
                                  SessionStateName(changed->state));
                    const XrLifecycleTransition transition =
                        lifecycle_.OnStateChanged(
                            ToLifecycleState(changed->state));
                    if (transition.previous ==
                            XrLifecycleState::Focused &&
                        transition.current !=
                            XrLifecycleState::Focused &&
                        ipcBridge_ && xrInput_) {
                        FearVrInputState neutral{};
                        xrInput_->Sync(
                            session_, XR_NULL_HANDLE, false, 0, neutral);
                        ipcBridge_->PublishInputState(neutral);
                        logger_.Write(
                            "INFO", "input_focus_cleared",
                            "Neutral controller state published.");
                    }
                    switch (transition.action) {
                    case XrLifecycleAction::BeginSession: {
                        XrSessionBeginInfo beginInfo{
                            XR_TYPE_SESSION_BEGIN_INFO};
                        beginInfo.primaryViewConfigurationType =
                            XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                        CheckXr(instance_,
                                xrBeginSession(session_, &beginInfo),
                                "xrBeginSession");
                        break;
                    }
                    case XrLifecycleAction::EndSession:
                        CheckXr(instance_, xrEndSession(session_),
                                "xrEndSession");
                        if (exitRequested_) {
                            exitLoop = true;
                        }
                        break;
                    case XrLifecycleAction::RestartSession:
                        exitLoop = true;
                        return LoopResult::RestartSession;
                    case XrLifecycleAction::ExitHost:
                        exitLoop = true;
                        return LoopResult::Exit;
                    case XrLifecycleAction::None:
                        break;
                    }
                }
            }
            event = {XR_TYPE_EVENT_DATA_BUFFER};
        }
        return LoopResult::Exit;
    }

    bool RenderFrame() {
        XrFrameWaitInfo waitInfo{XR_TYPE_FRAME_WAIT_INFO};
        XrFrameState frameState{XR_TYPE_FRAME_STATE};
        CheckXr(instance_, xrWaitFrame(session_, &waitInfo, &frameState),
                "xrWaitFrame");

        XrFrameBeginInfo beginInfo{XR_TYPE_FRAME_BEGIN_INFO};
        CheckXr(instance_, xrBeginFrame(session_, &beginInfo), "xrBeginFrame");

        // Ab hier gehoert die Zeit uns: `xrWaitFrame` blockiert absichtlich
        // bis zum naechsten Takt, alles danach ist eigene Arbeit. Genau die
        // muss unter dem Bildbudget bleiben, sonst faellt ein Frame aus und
        // der Kompositor zeigt das letzte Bild ein zweites Mal.
        const auto frameCpuStart = std::chrono::steady_clock::now();
        frameSwapWaitMicroseconds_ = 0;

        const auto inputStart = SteadyClock::now();
        if (ipcBridge_ && xrInput_) {
            FearVrInputState input{};
            xrInput_->Sync(
                session_, appSpace_,
                lifecycle_.State() == XrLifecycleState::Focused,
                frameState.predictedDisplayTime,
                input);
            ipcBridge_->PublishInputState(input);
            const bool rightStickDown =
                (input.buttons & FEARVR_IB_RIGHT_STICK) != 0;
            if (rightStickDown && !rightStickWasDown_ &&
                !ipcBridge_->StereoActive()) {
                monoQuadAnchored_ = false;
                logger_.Write(
                    "INFO", "mono_quad_recenter_requested",
                    "Right stick click will re-anchor the loading/menu "
                    "panel at the current view direction.");
            }
            rightStickWasDown_ = rightStickDown;

            FearVrHapticRequest haptic{};
            if (lifecycle_.State() == XrLifecycleState::Focused &&
                ipcBridge_->ConsumeHapticRequest(haptic)) {
                xrInput_->ApplyHaptic(session_, haptic);
            }
        }

        inputMaxMicroseconds_ =
            (std::max)(inputMaxMicroseconds_, MicrosecondsSince(inputStart));

        const XrCompositionLayerBaseHeader* layers[1]{};
        std::uint32_t layerCount = 0;
        XrCompositionLayerProjection layer{
            XR_TYPE_COMPOSITION_LAYER_PROJECTION};
        XrCompositionLayerQuad quad{
            XR_TYPE_COMPOSITION_LAYER_QUAD};

        if (frameState.shouldRender == XR_TRUE) {
            XrViewLocateInfo locateInfo{XR_TYPE_VIEW_LOCATE_INFO};
            locateInfo.viewConfigurationType =
                XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
            locateInfo.displayTime = frameState.predictedDisplayTime;
            locateInfo.space = appSpace_;
            XrViewState viewState{XR_TYPE_VIEW_STATE};
            std::uint32_t viewCount = 0;
            const auto locateStart = SteadyClock::now();
            CheckXr(instance_,
                    xrLocateViews(session_, &locateInfo, &viewState,
                                  static_cast<std::uint32_t>(
                                      locatedViews_.size()),
                                  &viewCount, locatedViews_.data()),
                    "xrLocateViews");
            locateMaxMicroseconds_ = (std::max)(
                locateMaxMicroseconds_, MicrosecondsSince(locateStart));

            const XrViewStateFlags requiredFlags =
                XR_VIEW_STATE_POSITION_VALID_BIT |
                XR_VIEW_STATE_ORIENTATION_VALID_BIT;
            if (viewCount == 2 &&
                (viewState.viewStateFlags & requiredFlags) == requiredFlags) {
                std::array<XrFovf, FEARVR_EYE_COUNT> submittedFov{
                    locatedViews_[FEARVR_EYE_LEFT].fov,
                    locatedViews_[FEARVR_EYE_RIGHT].fov};
                const bool nativeStereo =
                    ipcBridge_ && ipcBridge_->StereoActive();
                const std::uint32_t panelRecenterGeneration =
                    ipcBridge_
                    ? ipcBridge_->PanelRecenterGeneration()
                    : 0;
                if (panelRecenterGeneration !=
                    panelRecenterGeneration_) {
                    panelRecenterGeneration_ =
                        panelRecenterGeneration;
                    if (!nativeStereo &&
                        panelRecenterGeneration != 0) {
                        monoQuadAnchored_ = false;
                        logger_.Write(
                            "INFO", "mono_quad_recenter_requested",
                            "The 2D menu action or F9 will re-anchor the "
                            "panel at the current view direction.");
                    }
                }
                if (nativeStereo) {
                    const FearVrFov runtimeLeft{
                        submittedFov[FEARVR_EYE_LEFT].angleLeft,
                        submittedFov[FEARVR_EYE_LEFT].angleRight,
                        submittedFov[FEARVR_EYE_LEFT].angleUp,
                        submittedFov[FEARVR_EYE_LEFT].angleDown};
                    const FearVrFov runtimeRight{
                        submittedFov[FEARVR_EYE_RIGHT].angleLeft,
                        submittedFov[FEARVR_EYE_RIGHT].angleRight,
                        submittedFov[FEARVR_EYE_RIGHT].angleUp,
                        submittedFov[FEARVR_EYE_RIGHT].angleDown};
                    const std::uint32_t fovScalePercent =
                        ipcBridge_->FovScalePercent();
                    const SymmetricFov symmetric = ScaleSymmetricFov(
                        SharedSymmetricFov(runtimeLeft, runtimeRight),
                        static_cast<float>(fovScalePercent) / 100.0F);
                    if (symmetric.valid) {
                        const FearVrFov protocolFov =
                            ToProtocolFov(symmetric);
                        for (std::uint32_t eye = 0;
                             eye < FEARVR_EYE_COUNT; ++eye) {
                            submittedFov[eye] = {
                                protocolFov.angleLeft,
                                protocolFov.angleRight,
                                protocolFov.angleUp,
                                protocolFov.angleDown};
                        }
                        if (loggedFovScalePercent_ != fovScalePercent) {
                            std::ostringstream message;
                            message << "horizontal="
                                    << symmetric.halfHorizontal * 2.0F
                                    << " vertical="
                                    << symmetric.halfVertical * 2.0F
                                    << " scale=" << fovScalePercent
                                    << "%";
                            logger_.Write(
                                "INFO", "symmetric_stereo_fov",
                                message.str());
                            loggedFovScalePercent_ = fovScalePercent;
                        }
                    }
                }
                if (ipcBridge_) {
                    FearVrRenderRequest request{};
                    request.frameId = ++requestFrameId_;
                    request.predictedDisplayTimeNs =
                        static_cast<std::uint64_t>(
                            frameState.predictedDisplayTime);
                    request.flags = FEARVR_RF_VALID;
                    for (std::uint32_t eye = 0;
                         eye < FEARVR_EYE_COUNT; ++eye) {
                        const XrPosef& pose = locatedViews_[eye].pose;
                        const XrFovf& fov = submittedFov[eye];
                        FearVrEyeView& output = request.eye[eye];
                        output.pose.px = pose.position.x;
                        output.pose.py = pose.position.y;
                        output.pose.pz = pose.position.z;
                        output.pose.qx = pose.orientation.x;
                        output.pose.qy = pose.orientation.y;
                        output.pose.qz = pose.orientation.z;
                        output.pose.qw = pose.orientation.w;
                        output.fov.angleLeft = fov.angleLeft;
                        output.fov.angleRight = fov.angleRight;
                        output.fov.angleUp = fov.angleUp;
                        output.fov.angleDown = fov.angleDown;
                    }
                    RenderPoseSample& sample =
                        renderPoseHistory_[
                            request.frameId % kRenderPoseHistorySize];
                    sample.frameId = request.frameId;
                    for (std::uint32_t eye = 0;
                         eye < FEARVR_EYE_COUNT; ++eye) {
                        sample.pose[eye] = locatedViews_[eye].pose;
                        sample.fov[eye] = submittedFov[eye];
                    }
                    ipcBridge_->PublishRenderRequest(request);
                    ipcBridge_->ConsumeLatestPair();
                }
                const RenderPoseSample* imagePose = nullptr;
                if (nativeStereo && ipcBridge_) {
                    const std::uint64_t imageFrameId =
                        ipcBridge_->LatestFrameId();
                    const RenderPoseSample& candidate =
                        renderPoseHistory_[
                            imageFrameId % kRenderPoseHistorySize];
                    if (imageFrameId != 0 &&
                        candidate.frameId == imageFrameId) {
                        imagePose = &candidate;
                        if (!imagePoseMatchLogged_) {
                            const std::uint64_t framesBehind =
                                requestFrameId_ >= imageFrameId
                                    ? requestFrameId_ - imageFrameId
                                    : 0;
                            logger_.Write(
                                "INFO", "image_pose_matched",
                                "image_frame=" +
                                    std::to_string(imageFrameId) +
                                    " request_age_frames=" +
                                    std::to_string(framesBehind) +
                                    "; compositor timewarp can correct "
                                    "the rendered pose.");
                            imagePoseMatchLogged_ = true;
                        }
                    }
                    if (imagePose == nullptr && imageFrameId != 0) {
                        // Ohne Treffer bekommt das alte Bild die *aktuelle*
                        // Pose aufgedrueckt. Der Kompositor haelt es dann
                        // faelschlich fuer frisch und korrigiert nichts mehr
                        // — die Welt haengt sichtbar nach. Zaehlen, damit
                        // dieser Fall nicht unbemerkt bleibt.
                        ++poseFallbacks_;
                    }
                }
                if (nativeStereo) {
                    monoQuadAnchored_ = false;
                    for (std::uint32_t eye = 0; eye < 2; ++eye) {
                        RenderEye(eye);
                        projectionViews_[eye].pose =
                            imagePose == nullptr
                                ? locatedViews_[eye].pose
                                : imagePose->pose[eye];
                        projectionViews_[eye].fov =
                            imagePose == nullptr
                                ? submittedFov[eye]
                                : imagePose->fov[eye];
                    }
                    layer.space = appSpace_;
                    layer.viewCount =
                        static_cast<std::uint32_t>(
                            projectionViews_.size());
                    layer.views = projectionViews_.data();
                    layers[0] =
                        reinterpret_cast<
                            const XrCompositionLayerBaseHeader*>(
                            &layer);
                } else {
                    RenderEye(FEARVR_EYE_LEFT);
                    if (!monoQuadAnchored_) {
                        TrackingQuaternion leftRotation{
                            locatedViews_[FEARVR_EYE_LEFT]
                                .pose.orientation.x,
                            locatedViews_[FEARVR_EYE_LEFT]
                                .pose.orientation.y,
                            locatedViews_[FEARVR_EYE_LEFT]
                                .pose.orientation.z,
                            locatedViews_[FEARVR_EYE_LEFT]
                                .pose.orientation.w};
                        TrackingQuaternion rightRotation{
                            locatedViews_[FEARVR_EYE_RIGHT]
                                .pose.orientation.x,
                            locatedViews_[FEARVR_EYE_RIGHT]
                                .pose.orientation.y,
                            locatedViews_[FEARVR_EYE_RIGHT]
                                .pose.orientation.z,
                            locatedViews_[FEARVR_EYE_RIGHT]
                                .pose.orientation.w};
                        leftRotation = Normalize(leftRotation);
                        rightRotation = Normalize(rightRotation);
                        if (Dot(leftRotation, rightRotation) < 0.0F) {
                            rightRotation = {
                                -rightRotation.x, -rightRotation.y,
                                -rightRotation.z, -rightRotation.w};
                        }
                        const TrackingQuaternion centerRotation =
                            Normalize({
                                leftRotation.x + rightRotation.x,
                                leftRotation.y + rightRotation.y,
                                leftRotation.z + rightRotation.z,
                                leftRotation.w + rightRotation.w});
                        const TrackingVector forward = Rotate(
                            centerRotation, {0.0F, 0.0F, -1.0F});
                        FearVrPose centerPose{};
                        centerPose.px =
                            (locatedViews_[FEARVR_EYE_LEFT]
                                 .pose.position.x +
                             locatedViews_[FEARVR_EYE_RIGHT]
                                 .pose.position.x) *
                            0.5F;
                        centerPose.py =
                            (locatedViews_[FEARVR_EYE_LEFT]
                                 .pose.position.y +
                             locatedViews_[FEARVR_EYE_RIGHT]
                                 .pose.position.y) *
                            0.5F;
                        centerPose.pz =
                            (locatedViews_[FEARVR_EYE_LEFT]
                                 .pose.position.z +
                             locatedViews_[FEARVR_EYE_RIGHT]
                                 .pose.position.z) *
                            0.5F;
                        centerPose.qx = centerRotation.x;
                        centerPose.qy = centerRotation.y;
                        centerPose.qz = centerRotation.z;
                        centerPose.qw = centerRotation.w;
                        const FearVrPose levelAnchor =
                            YawOnlyRecenterPose(centerPose);
                        monoQuadPose_ = {};
                        monoQuadPose_.orientation = {
                            levelAnchor.qx, levelAnchor.qy,
                            levelAnchor.qz, levelAnchor.qw};
                        monoQuadPose_.position = {
                            centerPose.px + forward.x * 2.0F,
                            centerPose.py + forward.y * 2.0F,
                            centerPose.pz + forward.z * 2.0F};
                        monoQuadAnchored_ = true;
                        logger_.Write(
                            "INFO", "mono_quad_anchored",
                            "Menu panel centered at the current gaze point "
                            "with a level, yaw-only orientation.");
                    }
                    quad.space = appSpace_;
                    quad.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
                    quad.subImage = projectionViews_[
                        FEARVR_EYE_LEFT].subImage;
                    quad.pose = monoQuadPose_;
                    quad.size = {2.4F, 1.8F};
                    layers[0] =
                        reinterpret_cast<
                            const XrCompositionLayerBaseHeader*>(
                            &quad);
                    if (!monoQuadLogged_) {
                        logger_.Write(
                            "INFO", "mono_quad_layer",
                            "Flat game/menu image is shown as a "
                            "2.4x1.8m world-locked panel at 2m.");
                        monoQuadLogged_ = true;
                    }
                }
                layerCount = 1;
                // Ein Frame gilt als wiederverwendet, wenn seit der letzten
                // Einreichung kein neues Spielbild importiert wurde. Das
                // passiert regulär, sobald die XR-Displayrate über der
                // Spiel-FPS liegt.
                const std::uint64_t imageFrameId =
                    ipcBridge_ ? ipcBridge_->LatestFrameId() : 0;
                if (imageFrameId != 0 &&
                    imageFrameId == lastSubmittedImageFrameId_) {
                    ++reusedFrames_;
                }
                lastSubmittedImageFrameId_ = imageFrameId;
                ++submittedFrames_;
            } else {
                logger_.Write("WARN", "tracking_invalid",
                              "Pose ungueltig; Frame ohne Layer eingereicht.");
            }
        }

        // Einmal pro Bild statt einmal pro Auge: `Flush` reicht die
        // Befehlsliste an den Treiber weiter, und zwei Uebergaben pro Bild
        // sind zwei Synchronisationspunkte, wo einer genuegt. Wichtig ist
        // allein, dass alles vor `xrEndFrame` eingereicht ist.
        if (layerCount != 0) {
            // Verdaechtig Nummer eins: `Flush` kann blockieren, wenn die
            // Befehlswarteschlange des Treibers voll ist — und der grosse
            // GPU-Verbraucher auf diesem Rechner ist das Spiel selbst.
            const auto flushStart = SteadyClock::now();
            deviceContext_->Flush();
            flushMaxMicroseconds_ = (std::max)(
                flushMaxMicroseconds_, MicrosecondsSince(flushStart));
        }

        XrFrameEndInfo endInfo{XR_TYPE_FRAME_END_INFO};
        endInfo.displayTime = frameState.predictedDisplayTime;
        endInfo.environmentBlendMode = blendMode_;
        endInfo.layerCount = layerCount;
        endInfo.layers = layerCount == 0 ? nullptr : layers;
        const auto endFrameStart = SteadyClock::now();
        CheckXr(instance_, xrEndFrame(session_, &endInfo), "xrEndFrame");
        const std::uint64_t endFrameMicroseconds =
            MicrosecondsSince(endFrameStart);
        endFrameMaxMicroseconds_ =
            (std::max)(endFrameMaxMicroseconds_, endFrameMicroseconds);

        const auto frameTotalMicroseconds =
            static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - frameCpuStart)
                    .count());
        // Fremde Wartezeit gehoert nicht in die eigene Bilanz. Neben dem
        // Swapchain-Warten faellt auch `xrEndFrame` darunter: Dort nimmt der
        // Kompositor das Bild entgegen und drosselt uns auf seinen Takt.
        //
        // Der Lauf vom 28.07.2026 hat das entschieden: In 102 Spielfenstern
        // lag die groesste *eigene* Spitze bei 576 µs, im Mittel 180 µs —
        // und jedes der 19 Fenster mit einem Bild ueber 11,1 ms ging zu ueber
        // 90 % auf `xrEndFrame` zurueck. Wuerde man das mitzaehlen, meldete
        // `long_frames` genau das als Ruckler, was ein VR-Programm tun soll,
        // wenn es frueher fertig ist als der Takt.
        const std::uint64_t foreignMicroseconds =
            frameSwapWaitMicroseconds_ + endFrameMicroseconds;
        const std::uint64_t frameCpuMicroseconds =
            frameTotalMicroseconds > foreignMicroseconds
                ? frameTotalMicroseconds - foreignMicroseconds
                : 0;
        frameCpuMaxMicroseconds_ =
            (std::max)(frameCpuMaxMicroseconds_, frameCpuMicroseconds);
        swapWaitMaxMicroseconds_ =
            (std::max)(swapWaitMaxMicroseconds_, frameSwapWaitMicroseconds_);
        if (frameCpuMicroseconds > kLongFrameMicroseconds) {
            ++longFrames_;
        }

        if (submittedFrames_ != 0 && submittedFrames_ % 300 == 0) {
            logger_.Write("INFO", "frame_progress",
                          "submitted=" + std::to_string(submittedFrames_));
            LogPerformanceWindow();
        }
        return options_.maxFrames != 0 &&
               submittedFrames_ >= options_.maxFrames;
    }

    // Fasst ein Messfenster zu genau einer Zeile zusammen (ANWEISUNG.md §14:
    // Game-FPS/XR-Displayrate, reused frames, Renderzeit links/rechts und
    // Host-Copyzeit). Alle Zähler werden danach zurückgesetzt, damit die
    // Zeilen unabhängig voneinander auswertbar bleiben.
    void LogPerformanceWindow() {
        const auto now = std::chrono::steady_clock::now();
        const auto windowMicroseconds =
            std::chrono::duration_cast<std::chrono::microseconds>(
                now - perfWindowStart_)
                .count();
        perfWindowStart_ = now;
        if (windowMicroseconds <= 0) {
            return;
        }

        const auto average = [](const std::uint64_t total,
                                const std::uint64_t samples) {
            return samples == 0 ? 0ULL : total / samples;
        };

        const BridgeCopyStats copy =
            ipcBridge_ ? ipcBridge_->TakeCopyStats() : BridgeCopyStats{};
        const EyeStats left = eyeStats_[FEARVR_EYE_LEFT];
        const EyeStats right = eyeStats_[FEARVR_EYE_RIGHT];
        eyeStats_ = {};

        const double windowSeconds =
            static_cast<double>(windowMicroseconds) / 1'000'000.0;
        const double xrFps = 300.0 / windowSeconds;
        const double gameFps =
            static_cast<double>(copy.samples) / windowSeconds;

        std::ostringstream message;
        message.setf(std::ios::fixed);
        message.precision(1);
        message << "window_frames=300"
                << " xr_fps=" << xrFps
                << " game_fps=" << gameFps
                << " reused=" << reusedFrames_
                << " render_left_avg_us="
                << average(left.totalMicroseconds, left.samples)
                << " render_left_max_us=" << left.maxMicroseconds
                << " render_right_avg_us="
                << average(right.totalMicroseconds, right.samples)
                << " render_right_max_us=" << right.maxMicroseconds
                << " copy_avg_us="
                << average(copy.totalMicroseconds, copy.samples)
                << " copy_max_us=" << copy.maxMicroseconds
                << " frame_cpu_max_us=" << frameCpuMaxMicroseconds_
                << " swap_wait_max_us=" << swapWaitMaxMicroseconds_
                << " input_max_us=" << inputMaxMicroseconds_
                << " locate_max_us=" << locateMaxMicroseconds_
                << " flush_max_us=" << flushMaxMicroseconds_
                << " endframe_max_us=" << endFrameMaxMicroseconds_
                << " long_frames=" << longFrames_
                << " pose_fallback=" << poseFallbacks_
                << " handles=" << CurrentHandleCount();
        logger_.Write("INFO", "perf_frame", message.str());
        reusedFrames_ = 0;
        frameCpuMaxMicroseconds_ = 0;
        swapWaitMaxMicroseconds_ = 0;
        inputMaxMicroseconds_ = 0;
        locateMaxMicroseconds_ = 0;
        flushMaxMicroseconds_ = 0;
        endFrameMaxMicroseconds_ = 0;
        longFrames_ = 0;
        poseFallbacks_ = 0;
    }

    static std::uint32_t CurrentHandleCount() noexcept {
        DWORD handles = 0;
        if (GetProcessHandleCount(GetCurrentProcess(), &handles) == 0) {
            return 0;
        }
        return static_cast<std::uint32_t>(handles);
    }

    void RenderEye(std::uint32_t eye) {
        const auto renderStart = std::chrono::steady_clock::now();
        RenderEyeInner(eye);
        const auto microseconds =
            static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - renderStart)
                    .count());
        EyeStats& stats = eyeStats_.at(eye);
        ++stats.samples;
        stats.totalMicroseconds += microseconds;
        stats.maxMicroseconds = (std::max)(stats.maxMicroseconds, microseconds);
    }

    void RenderEyeInner(std::uint32_t eye) {
        Swapchain& swapchain = swapchains_.at(eye);
        XrSwapchainImageAcquireInfo acquireInfo{
            XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
        std::uint32_t imageIndex = 0;
        CheckXr(instance_,
                xrAcquireSwapchainImage(swapchain.handle, &acquireInfo,
                                        &imageIndex),
                "xrAcquireSwapchainImage");

        // Dieses Warten ist fremde Zeit: Es haengt daran, wann der Kompositor
        // das Bild freigibt, nicht an unserer Arbeit. Getrennt gemessen,
        // damit `frame_cpu_max_us` nicht faelschlich uns anlastet, was die
        // Gegenseite verursacht.
        const auto waitStart = std::chrono::steady_clock::now();
        XrSwapchainImageWaitInfo waitInfo{
            XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
        waitInfo.timeout = 1'000'000'000;
        CheckXr(instance_, xrWaitSwapchainImage(swapchain.handle, &waitInfo),
                "xrWaitSwapchainImage(1s)");
        frameSwapWaitMicroseconds_ +=
            static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - waitStart)
                    .count());

        if (imageIndex >= swapchain.images.size() ||
            imageIndex >= swapchain.renderTargets.size() ||
            swapchain.images[imageIndex].texture == nullptr) {
            throw std::runtime_error(
                "OpenXR lieferte einen ungueltigen Swapchain-Image-Index.");
        }

        ComPtr<ID3D11RenderTargetView>& renderTarget =
            swapchain.renderTargets[imageIndex];
        if (!renderTarget) {
            D3D11_RENDER_TARGET_VIEW_DESC viewDescription{};
            viewDescription.Format = swapchainFormat_;
            viewDescription.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
            viewDescription.Texture2D.MipSlice = 0;
            CheckHr(device_->CreateRenderTargetView(
                        swapchain.images[imageIndex].texture,
                        &viewDescription,
                        renderTarget.ReleaseAndGetAddressOf()),
                    "ID3D11Device::CreateRenderTargetView");
        }

        if (ipcBridge_ && ipcBridge_->HasImage(eye)) {
            textureRenderer_.Draw(
                deviceContext_.Get(), renderTarget.Get(),
                ipcBridge_->ImageView(eye),
                static_cast<float>(swapchain.width),
                static_cast<float>(swapchain.height));
        } else {
            constexpr std::array<float, 4> leftColor{
                0.90F, 0.03F, 0.03F, 1.0F};
            constexpr std::array<float, 4> rightColor{
                0.03F, 0.12F, 0.90F, 1.0F};
            const auto& color = eye == 0 ? leftColor : rightColor;
            deviceContext_->ClearRenderTargetView(renderTarget.Get(),
                                                  color.data());
        }

        XrSwapchainImageReleaseInfo releaseInfo{
            XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
        CheckXr(instance_,
                xrReleaseSwapchainImage(swapchain.handle, &releaseInfo),
                "xrReleaseSwapchainImage");
    }

    LoopResult RunSessionLoop() {
        bool exitLoop = false;
        auto exitDeadline = std::chrono::steady_clock::time_point::max();
        while (!exitLoop && !g_stopRequested.load()) {
            if (ipcBridge_) {
                ipcBridge_->Tick();
            }
            if (options_.exitOnGameDisconnect && !exitRequested_ &&
                ipcBridge_ &&
                ipcBridge_->GameWasConnected() &&
                !ipcBridge_->GameConnected()) {
                logger_.Write(
                    "INFO", "game_disconnect_exit",
                    "Game-Heartbeat beendet; OpenXR-Host wird beendet.");
                if (lifecycle_.IsSessionRunning()) {
                    CheckXr(instance_, xrRequestExitSession(session_),
                            "xrRequestExitSession(game disconnect)");
                    exitRequested_ = true;
                    exitDeadline =
                        std::chrono::steady_clock::now() +
                        std::chrono::seconds(5);
                } else {
                    return LoopResult::Exit;
                }
            }
            const LoopResult eventResult = PollEvents(exitLoop);
            if (exitLoop) {
                return eventResult;
            }

            if (lifecycle_.IsSessionRunning()) {
                if (RenderFrame() && !exitRequested_) {
                    CheckXr(instance_, xrRequestExitSession(session_),
                            "xrRequestExitSession");
                    exitRequested_ = true;
                    exitDeadline =
                        std::chrono::steady_clock::now() +
                        std::chrono::seconds(5);
                    logger_.Write(
                        "INFO", "frame_limit",
                        "submitted=" + std::to_string(submittedFrames_) +
                            "; Session-Ende angefordert.");
                }
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }

            if (exitRequested_ &&
                std::chrono::steady_clock::now() >= exitDeadline) {
                logger_.Write(
                    "WARN", "exit_timeout",
                    "Runtime meldete innerhalb von 5s keinen STOPPING-State.");
                return LoopResult::Exit;
            }
        }

        if (g_stopRequested.load() && lifecycle_.IsSessionRunning() &&
            !exitRequested_) {
            const XrResult requestResult = xrRequestExitSession(session_);
            if (XR_FAILED(requestResult)) {
                logger_.Write("WARN", "request_exit_failed",
                              XrResultText(instance_, requestResult));
            }
        }
        return LoopResult::Exit;
    }

    OpenXrHostOptions options_;
    Logger logger_;
    XrInstance instance_{XR_NULL_HANDLE};
    XrSystemId systemId_{XR_NULL_SYSTEM_ID};
    XrSession session_{XR_NULL_HANDLE};
    XrSpace appSpace_{XR_NULL_HANDLE};
    XrEnvironmentBlendMode blendMode_{XR_ENVIRONMENT_BLEND_MODE_OPAQUE};
    DXGI_FORMAT swapchainFormat_{DXGI_FORMAT_UNKNOWN};
    ComPtr<IDXGIAdapter1> adapter_;
    DXGI_ADAPTER_DESC1 adapterDescription_{};
    ComPtr<ID3D11Device> device_;
    ComPtr<ID3D11DeviceContext> deviceContext_;
    TextureRenderer textureRenderer_;
    std::unique_ptr<IpcBridge> ipcBridge_;
    std::unique_ptr<XrInput> xrInput_;
    bool genericControllerExtensionEnabled_{false};
    std::vector<XrViewConfigurationView> viewConfiguration_;
    std::vector<XrView> locatedViews_;
    std::vector<XrCompositionLayerProjectionView> projectionViews_;
    std::vector<Swapchain> swapchains_;
    // Renderzeit je Auge (ANWEISUNG.md §14), zwischen zwei perf_frame-
    // Meldungen gesammelt und danach zurückgesetzt.
    struct EyeStats {
        std::uint64_t samples{0};
        std::uint64_t totalMicroseconds{0};
        std::uint64_t maxMicroseconds{0};
    };

    std::array<RenderPoseSample, kRenderPoseHistorySize>
        renderPoseHistory_{};
    XrSessionStateMachine lifecycle_;
    std::array<EyeStats, FEARVR_EYE_COUNT> eyeStats_{};
    std::chrono::steady_clock::time_point perfWindowStart_{
        std::chrono::steady_clock::now()};
    std::uint64_t reusedFrames_{0};
    std::uint64_t frameCpuMaxMicroseconds_{0};
    std::uint64_t frameSwapWaitMicroseconds_{0};
    std::uint64_t swapWaitMaxMicroseconds_{0};
    // Die vier bisher unbeobachteten Abschnitte des Bildes. Render- und
    // Copyzeit sind mit wenigen hundert Mikrosekunden zu klein fuer die
    // gemessenen Spitzen — die Zeit muss in einem von diesen sitzen.
    std::uint64_t inputMaxMicroseconds_{0};
    std::uint64_t locateMaxMicroseconds_{0};
    std::uint64_t flushMaxMicroseconds_{0};
    std::uint64_t endFrameMaxMicroseconds_{0};
    std::uint64_t longFrames_{0};
    std::uint64_t poseFallbacks_{0};
    std::uint64_t lastSubmittedImageFrameId_{0};
    std::uint64_t submittedFrames_{0};
    std::uint64_t requestFrameId_{0};
    std::uint32_t loggedFovScalePercent_{0};
    bool imagePoseMatchLogged_{false};
    bool monoQuadLogged_{false};
    bool monoQuadAnchored_{false};
    bool rightStickWasDown_{false};
    std::uint32_t panelRecenterGeneration_{0};
    XrPosef monoQuadPose_{{0.0F, 0.0F, 0.0F, 1.0F},
                         {0.0F, 0.0F, -2.0F}};
    bool exitRequested_{false};
};

} // namespace

void RequestOpenXrHostStop() noexcept {
    g_stopRequested.store(true);
}

int RunOpenXrHost(const OpenXrHostOptions& options) {
    g_stopRequested.store(false);
    try {
        Host host(options);
        return host.Run();
    } catch (const XrException& error) {
        std::cerr << "OpenXR-Fehler: " << error.what() << std::endl;
        if (error.Result() == XR_ERROR_RUNTIME_UNAVAILABLE) {
            std::cerr
                << "Keine aktive OpenXR-Runtime erreichbar. SteamVR starten "
                   "und als OpenXR-Runtime festlegen."
                << std::endl;
            return 10;
        }
        if (error.Result() == XR_ERROR_FORM_FACTOR_UNAVAILABLE) {
            std::cerr << "Kein betriebsbereites Headset gefunden. SteamVR-"
                         "Status und Headset-Verbindung pruefen."
                      << std::endl;
            return 11;
        }
        return 12;
    } catch (const std::exception& error) {
        std::cerr << "Hostinitialisierung fehlgeschlagen: " << error.what()
                  << std::endl;
        return 13;
    }
}

} // namespace fearvr

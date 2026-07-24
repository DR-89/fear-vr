#include "stereo_hook.h"

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cwchar>

#include <iltclient.h>
#include <iltrenderer.h>

#include "head_tracking_math.h"
#include "protocol.h"
#include "stereo_math.h"

namespace fearvr {
namespace {

using IsHostConnectedFunction = BOOL(__cdecl*)();
using IsStereoAvailableFunction = BOOL(__cdecl*)();
using IsStereoEnabledFunction = BOOL(__cdecl*)();
using StereoToggleCallback = void(__cdecl*)(BOOL);
using RegisterStereoToggleFunction =
    void(__cdecl*)(StereoToggleCallback);
using GetRenderRequestFunction =
    BOOL(__cdecl*)(FearVrRenderRequest*);
using BeginEyeFunction = void(__cdecl*)(std::uint32_t);
using CaptureEyeFunction = void(__cdecl*)(std::uint32_t);
using EndStereoFrameFunction = void(__cdecl*)(std::uint64_t);
using ReportHookStatusFunction =
    void(__cdecl*)(const char*, const char*, const char*);
using RenderPlayerCameraFunction =
    LTRESULT(__thiscall*)(ILTRenderer*, HLOCALOBJ);
using RenderCameraWithOverrideFunction =
    LTRESULT(__thiscall*)(ILTRenderer*, HLOCALOBJ, const char*);

ILTClient* g_client = nullptr;
ILTRenderer* g_renderer = nullptr;
IsHostConnectedFunction g_isHostConnected = nullptr;
IsStereoAvailableFunction g_isStereoAvailable = nullptr;
IsStereoEnabledFunction g_isStereoEnabled = nullptr;
RegisterStereoToggleFunction g_registerStereoToggle = nullptr;
GetRenderRequestFunction g_getRenderRequest = nullptr;
BeginEyeFunction g_beginEye = nullptr;
CaptureEyeFunction g_captureEye = nullptr;
EndStereoFrameFunction g_endStereoFrame = nullptr;
ReportHookStatusFunction g_reportHookStatus = nullptr;
RenderPlayerCameraFunction g_renderPlayerCamera = nullptr;
RenderCameraWithOverrideFunction g_renderCameraWithOverride = nullptr;
void** g_renderCameraSlot = nullptr;
SRWLOCK g_hookLock = SRWLOCK_INIT;
bool g_hookInstalled = false;
thread_local bool g_inStereoRender = false;
thread_local const char* g_stereoStep = "idle";
struct StereoRecoveryState {
    LTRigidTransform transform;
    float fovX{0.0F};
    float fovY{0.0F};
    bool valid{false};
};
thread_local StereoRecoveryState g_stereoRecovery;
struct HeadTrackingState {
    FearVrPose recenter{};
    std::uint64_t lastFrameId{0};
    ULONGLONG lastFreshFrameTick{0};
    std::uint32_t recenterGeneration{0};
    LONG resetGeneration{0};
    bool centered{false};
    bool trackingLost{false};
};
thread_local HeadTrackingState g_headTracking;
volatile LONG g_firstHookCallLogged = 0;
volatile LONG g_playerHookCallLogged = 0;
volatile LONG g_firstStereoFrameLogged = 0;
volatile LONG g_stereoFallbackLogged = 0;
volatile LONG g_headTrackingActiveLogged = 0;
volatile LONG g_trackingResetGeneration = 0;

// The retail VC7.1 VTable groups the RenderCamera overloads differently
// from their declaration order in the public header. The one-argument
// player-camera alias in slot 17 forwards to the exact two-argument
// RenderCamera implementation in slot 19 (call [vtable + 0x4c]).
constexpr std::size_t kRenderPlayerCameraSlot = 17;
constexpr std::size_t kRenderCameraWithOverrideSlot = 19;
constexpr unsigned char kRetailPlayerCameraForwarder[] = {
    0x8B, 0x54, 0x24, 0x04, // mov edx,[esp+4]
    0x8B, 0x01,             // mov eax,[ecx]
    0x6A, 0x00,             // push 0 (technique override)
    0x52,                   // push edx (camera)
    0xFF, 0x50, 0x4C,       // call [eax+0x4c] (slot 19)
    0xC2, 0x04, 0x00        // ret 4
};
static_assert(
    0x4C / sizeof(void*) == kRenderCameraWithOverrideSlot,
    "Retail RenderCamera forwarding slot changed.");

void Report(const char* level, const char* event,
            const char* message) noexcept {
    if (g_reportHookStatus != nullptr) {
        g_reportHookStatus(level, event, message);
    }
    OutputDebugStringA("F.E.A.R. VR: ");
    OutputDebugStringA(event);
    OutputDebugStringA(": ");
    OutputDebugStringA(message);
    OutputDebugStringA("\n");
}

bool IsExecutableAddress(const void* address) noexcept {
    MEMORY_BASIC_INFORMATION info{};
    if (address == nullptr ||
        VirtualQuery(address, &info, sizeof(info)) != sizeof(info) ||
        info.State != MEM_COMMIT) {
        return false;
    }
    const DWORD protection =
        info.Protect & ~(PAGE_GUARD | PAGE_NOCACHE | PAGE_WRITECOMBINE);
    return protection == PAGE_EXECUTE ||
           protection == PAGE_EXECUTE_READ ||
           protection == PAGE_EXECUTE_READWRITE ||
           protection == PAGE_EXECUTE_WRITECOPY;
}

bool MatchesRetailPlayerCameraForwarder(const void* target) noexcept {
    if (!IsExecutableAddress(target)) {
        return false;
    }
    __try {
        return std::memcmp(
                   target, kRetailPlayerCameraForwarder,
                   sizeof(kRetailPlayerCameraForwarder)) == 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool CommandLineContains(const wchar_t* option) noexcept {
    const wchar_t* const commandLine = GetCommandLineW();
    return commandLine != nullptr && option != nullptr &&
           std::wcsstr(commandLine, option) != nullptr;
}

void ConfigureComfortOptions() noexcept {
    if (g_client == nullptr ||
        !CommandLineContains(L"-fearvr-no-headbob")) {
        return;
    }

    static const char* const amplitudeVariables[] = {
        "HeadBobCameraOffsetXAmp",
        "HeadBobCameraOffsetYAmp",
        "HeadBobCameraOffsetZAmp",
        "HeadBobCameraRotationXAmp",
        "HeadBobCameraRotationYAmp",
        "HeadBobCameraRotationZAmp",
        "HeadBobWeaponOffsetXAmp",
        "HeadBobWeaponOffsetYAmp",
        "HeadBobWeaponOffsetZAmp",
        "HeadBobWeaponRotationXAmp",
        "HeadBobWeaponRotationYAmp",
        "HeadBobWeaponRotationZAmp",
    };

    bool configured = true;
    __try {
        configured =
            g_client->SetConsoleVariableFloat(
                "HeadBobDebugMode", 1.0F) == LT_OK;
        for (const char* variable : amplitudeVariables) {
            configured =
                g_client->SetConsoleVariableFloat(
                    variable, 0.0F) == LT_OK &&
                configured;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        configured = false;
    }
    Report(
        configured ? "INFO" : "WARN",
        configured ? "headbob_disabled"
                   : "headbob_disable_failed",
        configured
            ? "Camera and weapon head bob are disabled by opt-in."
            : "The opt-in head-bob console variables could not be set.");
}

static_assert(sizeof(void*) == 4,
              "The GameClient loader must remain x86.");

// Read-only Abbildung der in ltmodule.h/ltmodule.cpp dokumentierten
// VC7.1-Layouts. Wir registrieren keine zusätzlichen Holder im Engine-
// Datenbestand, sondern lesen ausschließlich die bereits ausgewählte
// Default-Implementierung.
struct InterfaceArrayAbi {
    std::uint32_t count;
    std::uint32_t capacity;
    void** items;
};

struct InterfaceDatabaseAbi {
    void** vtable;
    void* trackedPointers;
    InterfaceArrayAbi* interfaces;
};

struct InterfaceNameManagerAbi {
    void** vtable;
    const char* name;
    std::int32_t version;
    void* implementations;
    void* holders;
    void* currentInterface;
};

static_assert(sizeof(InterfaceArrayAbi) == 12,
              "Official database_array ABI changed.");
static_assert(sizeof(InterfaceDatabaseAbi) == 12,
              "Official CInterfaceDatabase ABI changed.");
static_assert(sizeof(InterfaceNameManagerAbi) == 24,
              "Official CInterfaceNameMgr ABI changed.");

void* FindCurrentInterface(void* masterDatabase, const char* name,
                           std::int32_t version) noexcept {
    __try {
        auto* database =
            static_cast<InterfaceDatabaseAbi*>(masterDatabase);
        InterfaceArrayAbi* array = database->interfaces;
        if (array == nullptr || array->count > array->capacity ||
            array->count > 4096 || array->items == nullptr) {
            return nullptr;
        }
        for (std::uint32_t index = 0; index < array->count; ++index) {
            auto* manager = static_cast<InterfaceNameManagerAbi*>(
                array->items[index]);
            if (manager != nullptr && manager->name != nullptr &&
                manager->version == version &&
                std::strcmp(manager->name, name) == 0) {
                return manager->currentInterface;
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
    return nullptr;
}

class StereoRenderGuard {
public:
    StereoRenderGuard() noexcept {
        g_inStereoRender = true;
    }
    ~StereoRenderGuard() {
        g_inStereoRender = false;
    }

    StereoRenderGuard(const StereoRenderGuard&) = delete;
    StereoRenderGuard& operator=(const StereoRenderGuard&) = delete;
};

bool PrepareTrackedEyePoses(
    const FearVrRenderRequest& request,
    RelativeEyePose (&eyePose)[FEARVR_EYE_COUNT]) noexcept {
    const FearVrPose currentCenter = CenterHeadPose(request);
    if (!IsValidPose(currentCenter)) {
        return false;
    }

    const ULONGLONG now = GetTickCount64();
    const LONG resetGeneration = InterlockedCompareExchange(
        &g_trackingResetGeneration, 0, 0);
    if (request.frameId == g_headTracking.lastFrameId) {
        if (g_headTracking.lastFreshFrameTick != 0 &&
            now - g_headTracking.lastFreshFrameTick > 250) {
            if (!g_headTracking.trackingLost) {
                Report(
                    "WARN", "head_tracking_lost",
                    "No fresh OpenXR pose for 250ms; mono fallback active.");
            }
            g_headTracking.trackingLost = true;
            g_headTracking.centered = false;
            return false;
        }
    } else {
        g_headTracking.lastFrameId = request.frameId;
        g_headTracking.lastFreshFrameTick = now;
        const bool recenter =
            !g_headTracking.centered ||
            g_headTracking.trackingLost ||
            g_headTracking.recenterGeneration !=
                request.recenterGeneration ||
            g_headTracking.resetGeneration != resetGeneration;
        if (recenter) {
            g_headTracking.recenter = currentCenter;
            g_headTracking.recenterGeneration =
                request.recenterGeneration;
            g_headTracking.resetGeneration = resetGeneration;
            g_headTracking.centered = true;
            g_headTracking.trackingLost = false;
            Report(
                "INFO", "head_tracking_recentered",
                "Current HMD pose is the neutral camera orientation.");
        }
    }
    if (!g_headTracking.centered) {
        return false;
    }

    const bool translationEnabled =
        (request.flags & FEARVR_RF_TRANSLATION_ON) != 0;
    for (std::uint32_t eye = 0; eye < FEARVR_EYE_COUNT; ++eye) {
        eyePose[eye] = EyePoseRelativeToRecenter(
            g_headTracking.recenter, currentCenter,
            request.eye[eye].pose, translationEnabled);
        if (!eyePose[eye].valid) {
            return false;
        }
    }
    if (InterlockedCompareExchange(
            &g_headTrackingActiveLogged, 1, 0) == 0) {
        Report(
            "INFO", "head_tracking_active",
            translationEnabled
                ? "Relative HMD rotation and bounded translation are active."
                : "Relative HMD rotation is active; translation is disabled.");
    }
    return true;
}

LTRESULT RenderStereo(ILTRenderer* renderer, HLOCALOBJ camera,
                      const char* techniqueOverride) {
    g_stereoStep = "check_arguments";
    if (renderer == nullptr || camera == nullptr) {
        g_stereoStep = "fallback_invalid_arguments";
        return g_renderCameraWithOverride(
            renderer, camera, techniqueOverride);
    }
    g_stereoStep = "check_technique_override";
    if (techniqueOverride != nullptr) {
        g_stereoStep = "fallback_technique_override";
        return g_renderCameraWithOverride(
            renderer, camera, techniqueOverride);
    }
    g_stereoStep = "check_recursion";
    if (g_inStereoRender) {
        g_stereoStep = "fallback_recursive_render";
        return g_renderCameraWithOverride(
            renderer, camera, techniqueOverride);
    }
    g_stereoStep = "check_dependencies";
    if (g_client == nullptr ||
        g_isHostConnected == nullptr || g_isStereoEnabled == nullptr ||
        g_getRenderRequest == nullptr ||
        g_beginEye == nullptr || g_captureEye == nullptr ||
        g_endStereoFrame == nullptr) {
        g_stereoStep = "fallback_missing_dependency";
        return g_renderCameraWithOverride(
            renderer, camera, techniqueOverride);
    }
    g_stereoStep = "query_stereo_enabled";
    if (!g_isStereoEnabled()) {
        return g_renderCameraWithOverride(
            renderer, camera, techniqueOverride);
    }
    g_stereoStep = "query_host_connected";
    if (!g_isHostConnected()) {
        return g_renderCameraWithOverride(
            renderer, camera, techniqueOverride);
    }

    g_stereoStep = "read_render_request";
    FearVrRenderRequest request{};
    if (!g_getRenderRequest(&request) ||
        (request.flags & FEARVR_RF_VALID) == 0 ||
        (request.flags & FEARVR_RF_FLATSCREEN) != 0 ||
        request.frameId == 0) {
        return g_renderCameraWithOverride(
            renderer, camera, techniqueOverride);
    }

    g_stereoStep = "validate_fov_and_ipd";
    const SymmetricFov symmetric = SharedSymmetricFov(
        request.eye[FEARVR_EYE_LEFT].fov,
        request.eye[FEARVR_EYE_RIGHT].fov);
    const float ipd = InterpupillaryDistanceMeters(request);
    if (!symmetric.valid || !std::isfinite(ipd) ||
        ipd < 0.02F || ipd > 0.12F ||
        g_client->GetCameraFOV == nullptr ||
        g_client->SetCameraFOV == nullptr) {
        return g_renderCameraWithOverride(
            renderer, camera, techniqueOverride);
    }

    g_stereoStep = "prepare_head_tracking";
    RelativeEyePose trackedEye[FEARVR_EYE_COUNT]{};
    if (!PrepareTrackedEyePoses(request, trackedEye)) {
        return g_renderCameraWithOverride(
            renderer, camera, techniqueOverride);
    }

    g_stereoStep = "get_camera_transform";
    LTRigidTransform originalTransform;
    if (g_client->GetObjectTransform(
            camera, &originalTransform) != LT_OK) {
        return g_renderCameraWithOverride(
            renderer, camera, techniqueOverride);
    }

    g_stereoStep = "get_camera_fov";
    float originalFovX = 0.0F;
    float originalFovY = 0.0F;
    g_client->GetCameraFOV(
        camera, &originalFovX, &originalFovY);
    if (!std::isfinite(originalFovX) ||
        !std::isfinite(originalFovY)) {
        return g_renderCameraWithOverride(
            renderer, camera, techniqueOverride);
    }
    g_stereoRecovery.transform = originalTransform;
    g_stereoRecovery.fovX = originalFovX;
    g_stereoRecovery.fovY = originalFovY;
    g_stereoRecovery.valid = true;

    const float stereoFovX = symmetric.halfHorizontal * 2.0F;
    const float stereoFovY = symmetric.halfVertical * 2.0F;
    LTRESULT eyeResult[FEARVR_EYE_COUNT]{LT_ERROR, LT_ERROR};
    std::uint32_t renderedEyes = 0;

    StereoRenderGuard guard;
    for (std::uint32_t eye = 0; eye < FEARVR_EYE_COUNT; ++eye) {
        g_stereoStep =
            eye == FEARVR_EYE_LEFT
                ? "set_left_transform"
                : "set_right_transform";
        LTRigidTransform eyeTransform = originalTransform;
        const RelativeEyePose& tracked = trackedEye[eye];
        const LTVector localOffset(
            tracked.positionMeters.x * kGameUnitsPerMeter,
            tracked.positionMeters.y * kGameUnitsPerMeter,
            tracked.positionMeters.z * kGameUnitsPerMeter);
        eyeTransform.m_vPos +=
            originalTransform.m_rRot.RotateVector(localOffset);
        const LTRotation headRotation(
            tracked.rotation.x, tracked.rotation.y,
            tracked.rotation.z, tracked.rotation.w);
        eyeTransform.m_rRot =
            originalTransform.m_rRot * headRotation;
        if (g_client->SetObjectTransform(
                camera, eyeTransform) != LT_OK) {
            break;
        }

        g_stereoStep =
            eye == FEARVR_EYE_LEFT ? "set_left_fov" : "set_right_fov";
        g_client->SetCameraFOV(camera, stereoFovX, stereoFovY);
        g_stereoStep =
            eye == FEARVR_EYE_LEFT
                ? "clear_left_target"
                : "clear_right_target";
        if (renderer->ClearRenderTarget(
                CLEARRTARGET_ALL, 0) != LT_OK) {
            break;
        }
        g_stereoStep =
            eye == FEARVR_EYE_LEFT ? "begin_left_eye" : "begin_right_eye";
        g_beginEye(eye);
        g_stereoStep =
            eye == FEARVR_EYE_LEFT
                ? "render_left_eye"
                : "render_right_eye";
        eyeResult[eye] = g_renderCameraWithOverride(
            renderer, camera, nullptr);
        if (eyeResult[eye] == LT_OK) {
            g_stereoStep =
                eye == FEARVR_EYE_LEFT
                    ? "capture_left_eye"
                    : "capture_right_eye";
            g_captureEye(eye);
        }
        ++renderedEyes;
    }

    g_stereoStep = "restore_camera_transform";
    g_client->SetObjectTransform(camera, originalTransform);
    g_stereoStep = "restore_camera_fov";
    g_client->SetCameraFOV(camera, originalFovX, originalFovY);
    g_stereoRecovery.valid = false;
    g_stereoStep = "end_stereo_frame";
    g_endStereoFrame(request.frameId);

    if (renderedEyes != FEARVR_EYE_COUNT ||
        eyeResult[FEARVR_EYE_LEFT] != LT_OK ||
        eyeResult[FEARVR_EYE_RIGHT] != LT_OK) {
        if (InterlockedCompareExchange(
                &g_stereoFallbackLogged, 1, 0) == 0) {
            Report(
                "WARN", "stereo_render_fallback",
                "An eye render failed; restored camera and rendered mono.");
        }
        g_stereoStep = "clear_mono_fallback";
        renderer->ClearRenderTarget(CLEARRTARGET_ALL, 0);
        g_stereoStep = "render_mono_fallback";
        return g_renderCameraWithOverride(
            renderer, camera, techniqueOverride);
    }
    if (InterlockedCompareExchange(
            &g_firstStereoFrameLogged, 1, 0) == 0) {
        Report(
            "INFO", "stereo_render_active",
            "Player camera world render is running once per eye.");
    }
    g_stereoStep = "complete";
    return eyeResult[renderedEyes - 1];
}

LTRESULT InvokeStereoProtected(
    ILTRenderer* renderer, HLOCALOBJ camera,
    const char* techniqueOverride) {
    if (g_renderCameraWithOverride == nullptr) {
        return LT_ERROR;
    }
    if (InterlockedCompareExchange(
            &g_firstHookCallLogged, 1, 0) == 0) {
        Report(
            "INFO", "rendercamera_hook_called",
            "The confirmed RenderCamera path entered native stereo.");
    }

    volatile DWORD exceptionCode = 0;
    __try {
        return RenderStereo(
            renderer, camera, techniqueOverride);
    } __except (
        exceptionCode = GetExceptionCode(),
        EXCEPTION_EXECUTE_HANDLER) {
    }

    const char* const failedStep = g_stereoStep;
    g_inStereoRender = false;
    if (g_stereoRecovery.valid && g_client != nullptr &&
        camera != nullptr) {
        __try {
            g_client->SetObjectTransform(
                camera, g_stereoRecovery.transform);
            g_client->SetCameraFOV(
                camera, g_stereoRecovery.fovX,
                g_stereoRecovery.fovY);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
    }
    g_stereoRecovery.valid = false;

    char message[192]{};
    std::snprintf(
        message, sizeof(message),
        "Structured exception 0x%08lX at %s; camera restored and mono resumed.",
        static_cast<unsigned long>(exceptionCode), failedStep);
    Report("ERROR", "stereo_render_exception", message);
    g_stereoStep = "mono_after_exception";
    return g_renderCameraWithOverride(
        renderer, camera, techniqueOverride);
}

LTRESULT __fastcall HookRenderPlayerCamera(
    ILTRenderer* renderer, void* ignoredEdx, HLOCALOBJ camera) {
    (void)ignoredEdx;
    if (InterlockedCompareExchange(
            &g_playerHookCallLogged, 1, 0) == 0) {
        Report(
            "INFO", "player_rendercamera_called",
            "Retail RenderCamera slot 17 entered native stereo.");
    }
    return InvokeStereoProtected(renderer, camera, nullptr);
}

bool ExchangeVtableSlot(void** slot, void* expected,
                        void* replacement) noexcept {
    if (slot == nullptr || expected == nullptr ||
        replacement == nullptr) {
        return false;
    }
    DWORD oldProtection = 0;
    if (!VirtualProtect(
            slot, sizeof(*slot), PAGE_READWRITE,
            &oldProtection)) {
        Report(
            "ERROR", "stereo_vtable_protect_failed",
            "An ILTRenderer VTable slot could not be made writable.");
        return false;
    }
    void* const observed = InterlockedCompareExchangePointer(
        reinterpret_cast<PVOID volatile*>(slot),
        replacement, expected);
    DWORD ignoredProtection = 0;
    const BOOL protectionRestored = VirtualProtect(
        slot, sizeof(*slot), oldProtection,
        &ignoredProtection);
    if (observed != expected) {
        Report(
            "ERROR", "stereo_vtable_target_changed",
            "An ILTRenderer VTable slot changed unexpectedly.");
        return false;
    }
    if (!protectionRestored) {
        Report(
            "ERROR", "stereo_vtable_restore_protection_failed",
            "ILTRenderer VTable protection could not be restored.");
        return false;
    }
    return true;
}

bool TryInstallRendererHook() noexcept {
    AcquireSRWLockExclusive(&g_hookLock);
    if (g_hookInstalled) {
        ReleaseSRWLockExclusive(&g_hookLock);
        return true;
    }
    if (g_client == nullptr || g_renderer == nullptr) {
        ReleaseSRWLockExclusive(&g_hookLock);
        return false;
    }

    void** const vtable = *reinterpret_cast<void***>(g_renderer);
    void** const playerSlot =
        vtable == nullptr
            ? nullptr
            : &vtable[kRenderPlayerCameraSlot];
    void* const playerTarget =
        playerSlot == nullptr ? nullptr : *playerSlot;
    void* const overrideTarget =
        vtable == nullptr ? nullptr
                          : vtable[kRenderCameraWithOverrideSlot];
    if (!MatchesRetailPlayerCameraForwarder(playerTarget) ||
        !IsExecutableAddress(overrideTarget)) {
        Report(
            "ERROR", "stereo_hook_layout_mismatch",
            "The retail RenderCamera 17-to-19 forwarding layout "
            "does not match; the VTable remains untouched.");
        ReleaseSRWLockExclusive(&g_hookLock);
        return false;
    }

    g_renderPlayerCamera =
        reinterpret_cast<RenderPlayerCameraFunction>(playerTarget);
    g_renderCameraWithOverride =
        reinterpret_cast<RenderCameraWithOverrideFunction>(
            overrideTarget);
    if (!ExchangeVtableSlot(
            playerSlot, playerTarget,
            reinterpret_cast<void*>(
                &HookRenderPlayerCamera))) {
        g_renderPlayerCamera = nullptr;
        g_renderCameraWithOverride = nullptr;
        ReleaseSRWLockExclusive(&g_hookLock);
        return false;
    }
    g_hookInstalled = true;
    g_renderCameraSlot = playerSlot;
    InterlockedIncrement(&g_trackingResetGeneration);
    Report(
        "INFO", "stereo_hook_installed",
        "ILTClient 105 and ILTRenderer 0 connected; "
        "retail player-camera slot 17 is hooked and renders through "
        "the exact slot 19 overload once per eye.");
    ReleaseSRWLockExclusive(&g_hookLock);
    return true;
}

bool TryRemoveRendererHook() noexcept {
    AcquireSRWLockExclusive(&g_hookLock);
    if (!g_hookInstalled ||
        g_renderCameraSlot == nullptr ||
        g_renderPlayerCamera == nullptr ||
        g_renderCameraWithOverride == nullptr) {
        ReleaseSRWLockExclusive(&g_hookLock);
        return true;
    }

    const bool slotRestored = ExchangeVtableSlot(
        g_renderCameraSlot,
        reinterpret_cast<void*>(
            &HookRenderPlayerCamera),
        reinterpret_cast<void*>(
            g_renderPlayerCamera));
    g_hookInstalled = !slotRestored;
    if (g_hookInstalled) {
        ReleaseSRWLockExclusive(&g_hookLock);
        return false;
    }
    Report(
        "INFO", "stereo_hook_removed",
        "Original player-camera VTable slot 17 restored.");
    ReleaseSRWLockExclusive(&g_hookLock);
    return true;
}

void __cdecl SetStereoHookEnabled(BOOL enabled) {
    if (enabled) {
        TryInstallRendererHook();
    } else {
        TryRemoveRendererHook();
    }
}

template <typename Function>
Function Resolve(HMODULE module, const char* name) noexcept {
    return module == nullptr
        ? nullptr
        : reinterpret_cast<Function>(GetProcAddress(module, name));
}

} // namespace

bool InstallStereoHook(void* masterDatabase, HMODULE bridge) noexcept {
    if (masterDatabase == nullptr || bridge == nullptr) {
        return false;
    }

    g_isHostConnected =
        Resolve<IsHostConnectedFunction>(
            bridge, "FearVr_IsHostConnected");
    g_isStereoAvailable =
        Resolve<IsStereoAvailableFunction>(
            bridge, "FearVr_IsStereoAvailable");
    g_isStereoEnabled =
        Resolve<IsStereoEnabledFunction>(
            bridge, "FearVr_IsStereoEnabled");
    g_registerStereoToggle =
        Resolve<RegisterStereoToggleFunction>(
            bridge, "FearVr_RegisterStereoToggleCallback");
    g_getRenderRequest =
        Resolve<GetRenderRequestFunction>(
            bridge, "FearVr_GetRenderRequest");
    g_beginEye =
        Resolve<BeginEyeFunction>(bridge, "FearVr_BeginEye");
    g_captureEye =
        Resolve<CaptureEyeFunction>(bridge, "FearVr_CaptureEye");
    g_endStereoFrame =
        Resolve<EndStereoFrameFunction>(
            bridge, "FearVr_EndStereoFrame");
    g_reportHookStatus =
        Resolve<ReportHookStatusFunction>(
            bridge, "FearVr_ReportHookStatus");
    if (g_isHostConnected == nullptr ||
        g_isStereoAvailable == nullptr ||
        g_isStereoEnabled == nullptr ||
        g_registerStereoToggle == nullptr ||
        g_getRenderRequest == nullptr || g_beginEye == nullptr ||
        g_captureEye == nullptr || g_endStereoFrame == nullptr) {
        Report(
            "ERROR", "stereo_bridge_exports_missing",
            "The staged bridge lacks one or more M3 exports.");
        return false;
    }
    if (!g_isStereoAvailable()) {
        Report(
            "INFO", "stereo_hook_not_requested",
            "M3 was not requested; the renderer VTable remains untouched.");
        return true;
    }

    g_client = static_cast<ILTClient*>(FindCurrentInterface(
        masterDatabase, "ILTClient.Default", 105));
    ILTRenderer* const heldRenderer =
        static_cast<ILTRenderer*>(FindCurrentInterface(
            masterDatabase, "ILTRenderer.Default", 0));
    if (g_client == nullptr || heldRenderer == nullptr) {
        Report(
            "ERROR", "engine_interfaces_missing",
            "Versioned ILTClient/ILTRenderer defaults were not found.");
        return false;
    }

    __try {
        g_renderer = g_client->GetRenderer();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_renderer = nullptr;
    }
    if (g_renderer == nullptr) {
        Report(
            "ERROR", "active_renderer_missing",
            "ILTClient::GetRenderer did not return the active renderer.");
        return false;
    }
    Report(
        "INFO",
        g_renderer == heldRenderer
            ? "active_renderer_matches_holder"
            : "active_renderer_differs_from_holder",
        g_renderer == heldRenderer
            ? "ILTClient::GetRenderer matches ILTRenderer.Default."
            : "ILTClient::GetRenderer differs from ILTRenderer.Default; "
              "the active PlayerCamera renderer will be hooked.");
    Report(
        "INFO", "engine_interfaces_found",
        "Read-only lookup found ILTClient 105 and ILTRenderer 0; "
        "the active renderer was resolved through ILTClient.");
    ConfigureComfortOptions();
    g_registerStereoToggle(&SetStereoHookEnabled);
    Report(
        "INFO", "stereo_hook_armed",
        "Renderer VTable remains original until F8 enables native stereo.");
    return true;
}

} // namespace fearvr

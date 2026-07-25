#include "bridge.h"
#include "iat_hook.h"
#include "system_d3d9.h"

#include <d3d9on12.h>

namespace {

template <typename Function>
Function Required(const char* name) noexcept {
    return fearvr::ResolveSystemD3D9<Function>(name);
}

} // namespace

extern "C" IDirect3D9* WINAPI Direct3DCreate9(UINT sdkVersion) {
    using Function = IDirect3D9*(WINAPI*)(UINT);
    const Function real = Required<Function>("Direct3DCreate9");
    if (real == nullptr) {
        SetLastError(fearvr::SystemD3D9LoadError());
        return nullptr;
    }
    IDirect3D9* direct3D = real(sdkVersion);
    fearvr::OnDirect3D9Created(direct3D);
    return direct3D;
}

extern "C" HRESULT WINAPI Direct3DCreate9Ex(UINT sdkVersion,
                                             IDirect3D9Ex** output) {
    using Function = HRESULT(WINAPI*)(UINT, IDirect3D9Ex**);
    const Function real = Required<Function>("Direct3DCreate9Ex");
    if (real == nullptr) {
        return HRESULT_FROM_WIN32(fearvr::SystemD3D9LoadError());
    }
    const HRESULT result = real(sdkVersion, output);
    if (SUCCEEDED(result) && output != nullptr) {
        fearvr::OnDirect3D9ExCreated(*output);
    }
    return result;
}

extern "C" IDirect3D9* WINAPI Direct3DCreate9On12(
    UINT sdkVersion, D3D9ON12_ARGS* overrideList, UINT overrideCount) {
    using Function =
        IDirect3D9*(WINAPI*)(UINT, D3D9ON12_ARGS*, UINT);
    const Function real = Required<Function>("Direct3DCreate9On12");
    if (real == nullptr) {
        return nullptr;
    }
    IDirect3D9* direct3D =
        real(sdkVersion, overrideList, overrideCount);
    fearvr::OnDirect3D9Created(direct3D);
    return direct3D;
}

extern "C" HRESULT WINAPI Direct3DCreate9On12Ex(
    UINT sdkVersion, D3D9ON12_ARGS* overrideList, UINT overrideCount,
    IDirect3D9Ex** output) {
    using Function =
        HRESULT(WINAPI*)(UINT, D3D9ON12_ARGS*, UINT, IDirect3D9Ex**);
    const Function real = Required<Function>("Direct3DCreate9On12Ex");
    if (real == nullptr) {
        return HRESULT_FROM_WIN32(fearvr::SystemD3D9LoadError());
    }
    const HRESULT result =
        real(sdkVersion, overrideList, overrideCount, output);
    if (SUCCEEDED(result) && output != nullptr) {
        fearvr::OnDirect3D9ExCreated(*output);
    }
    return result;
}

extern "C" int WINAPI D3DPERF_BeginEvent(D3DCOLOR color,
                                          LPCWSTR name) {
    using Function = int(WINAPI*)(D3DCOLOR, LPCWSTR);
    const Function real = Required<Function>("D3DPERF_BeginEvent");
    return real == nullptr ? -1 : real(color, name);
}

extern "C" int WINAPI D3DPERF_EndEvent() {
    using Function = int(WINAPI*)();
    const Function real = Required<Function>("D3DPERF_EndEvent");
    return real == nullptr ? -1 : real();
}

extern "C" DWORD WINAPI D3DPERF_GetStatus() {
    using Function = DWORD(WINAPI*)();
    const Function real = Required<Function>("D3DPERF_GetStatus");
    return real == nullptr ? 0 : real();
}

extern "C" BOOL WINAPI D3DPERF_QueryRepeatFrame() {
    using Function = BOOL(WINAPI*)();
    const Function real = Required<Function>("D3DPERF_QueryRepeatFrame");
    return real == nullptr ? FALSE : real();
}

extern "C" void WINAPI D3DPERF_SetMarker(D3DCOLOR color,
                                          LPCWSTR name) {
    using Function = void(WINAPI*)(D3DCOLOR, LPCWSTR);
    const Function real = Required<Function>("D3DPERF_SetMarker");
    if (real != nullptr) {
        real(color, name);
    }
}

extern "C" void WINAPI D3DPERF_SetOptions(DWORD options) {
    using Function = void(WINAPI*)(DWORD);
    const Function real = Required<Function>("D3DPERF_SetOptions");
    if (real != nullptr) {
        real(options);
    }
}

extern "C" void WINAPI D3DPERF_SetRegion(D3DCOLOR color,
                                          LPCWSTR name) {
    using Function = void(WINAPI*)(D3DCOLOR, LPCWSTR);
    const Function real = Required<Function>("D3DPERF_SetRegion");
    if (real != nullptr) {
        real(color, name);
    }
}

extern "C" BOOL FearVr_IsHostConnected() {
    return fearvr::IsHostConnected();
}

extern "C" BOOL FearVr_IsStereoAvailable() {
    return fearvr::IsStereoAvailable();
}

extern "C" BOOL FearVr_IsStereoEnabled() {
    return fearvr::IsStereoEnabled();
}

extern "C" void FearVr_SetStereoEnabled(BOOL enabled) {
    fearvr::SetStereoEnabled(enabled);
}

extern "C" BOOL FearVr_IsTranslationEnabled() {
    return fearvr::IsTranslationEnabled();
}

extern "C" void FearVr_SetTranslationEnabled(BOOL enabled) {
    fearvr::SetTranslationEnabled(enabled);
}

extern "C" BOOL FearVr_IsStereoHudEnabled() {
    return fearvr::IsStereoHudEnabled();
}

extern "C" void FearVr_SetStereoHudEnabled(BOOL enabled) {
    fearvr::SetStereoHudEnabled(enabled);
}

extern "C" BOOL FearVr_IsComfortModeEnabled() {
    return fearvr::IsComfortModeEnabled();
}

extern "C" void FearVr_SetComfortModeEnabled(BOOL enabled) {
    fearvr::SetComfortModeEnabled(enabled);
}

extern "C" void FearVr_RequestRecenter() {
    fearvr::RequestRecenter();
}

extern "C" BOOL FearVr_IsFlatPanelActive() {
    return fearvr::IsFlatPanelActive();
}

extern "C" void FearVr_RegisterStereoToggleCallback(
    fearvr::StereoToggleCallback callback) {
    fearvr::RegisterStereoToggleCallback(callback);
}

extern "C" BOOL FearVr_GetRenderRequest(FearVrRenderRequest* request) {
    return fearvr::GetRenderRequest(request);
}

extern "C" BOOL FearVr_GetInputState(FearVrInputState* input) {
    return fearvr::GetInputState(input);
}

extern "C" BOOL FearVr_SubmitHapticRequest(
    const FearVrHapticRequest* request) {
    return fearvr::SubmitHapticRequest(request);
}

extern "C" void FearVr_BeginEye(std::uint32_t eye) {
    fearvr::BeginEye(eye);
}

extern "C" void FearVr_CaptureEye(std::uint32_t eye) {
    fearvr::CaptureEye(eye);
}

extern "C" void FearVr_EndStereoFrame(std::uint64_t frameId) {
    fearvr::EndStereoFrame(frameId);
}

extern "C" void FearVr_ReportHookStatus(
    const char* level, const char* event, const char* message) {
    fearvr::ReportHookStatus(level, event, message);
}

extern "C" BOOL FearVr_InstallIatHook() {
    const BOOL iatInstalled =
        fearvr::InstallDirect3DCreate9IatHook(
            reinterpret_cast<void*>(&Direct3DCreate9));
    const BOOL lateHooksInstalled = fearvr::InstallLateD3D9Hooks();
    return iatInstalled || lateHooksInstalled;
}

#if !defined(_M_IX86)
#error The generic D3D9 ordinal thunks require the x86 MSVC inline assembler.
#endif

extern "C" FARPROC __cdecl FearVr_ResolveOrdinal(unsigned ordinal) {
    return fearvr::ResolveSystemD3D9Ordinal(ordinal);
}

extern "C" __declspec(naked) void FearVr_OrdinalDispatch() {
    __asm {
        pushad
        push eax
        call FearVr_ResolveOrdinal
        add esp, 4
        mov dword ptr [esp + 28], eax
        popad
        jmp eax
    }
}

extern "C" __declspec(naked) void FearVr_Ordinal16() {
    __asm mov eax, 16
    __asm jmp FearVr_OrdinalDispatch
}
extern "C" __declspec(naked) void FearVr_Ordinal17() {
    __asm mov eax, 17
    __asm jmp FearVr_OrdinalDispatch
}
extern "C" __declspec(naked) void FearVr_Ordinal18() {
    __asm mov eax, 18
    __asm jmp FearVr_OrdinalDispatch
}
extern "C" __declspec(naked) void FearVr_Ordinal19() {
    __asm mov eax, 19
    __asm jmp FearVr_OrdinalDispatch
}
extern "C" __declspec(naked) void FearVr_Ordinal22() {
    __asm mov eax, 22
    __asm jmp FearVr_OrdinalDispatch
}
extern "C" __declspec(naked) void FearVr_Ordinal23() {
    __asm mov eax, 23
    __asm jmp FearVr_OrdinalDispatch
}
extern "C" __declspec(naked) void FearVr_Ordinal24() {
    __asm mov eax, 24
    __asm jmp FearVr_OrdinalDispatch
}
extern "C" __declspec(naked) void FearVr_Ordinal25() {
    __asm mov eax, 25
    __asm jmp FearVr_OrdinalDispatch
}
extern "C" __declspec(naked) void FearVr_Ordinal26() {
    __asm mov eax, 26
    __asm jmp FearVr_OrdinalDispatch
}
extern "C" __declspec(naked) void FearVr_Ordinal34() {
    __asm mov eax, 34
    __asm jmp FearVr_OrdinalDispatch
}
extern "C" __declspec(naked) void FearVr_Ordinal35() {
    __asm mov eax, 35
    __asm jmp FearVr_OrdinalDispatch
}
extern "C" __declspec(naked) void FearVr_Ordinal36() {
    __asm mov eax, 36
    __asm jmp FearVr_OrdinalDispatch
}

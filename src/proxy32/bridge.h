#pragma once

#include <cstdint>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <d3d9.h>

#include "protocol.h"

namespace fearvr {

using StereoToggleCallback = void(__cdecl*)(BOOL enabled);

void OnDirect3D9Created(IDirect3D9* direct3D) noexcept;
void OnDirect3D9ExCreated(IDirect3D9Ex* direct3D) noexcept;
void ApplyEngineFixes() noexcept;
BOOL InstallLateD3D9Hooks() noexcept;

BOOL IsHostConnected() noexcept;
BOOL IsStereoAvailable() noexcept;
BOOL IsStereoEnabled() noexcept;
void SetStereoEnabled(BOOL enabled) noexcept;
void SetFovScalePercent(std::uint32_t percent) noexcept;
BOOL IsTranslationEnabled() noexcept;
void SetTranslationEnabled(BOOL enabled) noexcept;
BOOL IsStereoHudEnabled() noexcept;
void SetStereoHudEnabled(BOOL enabled) noexcept;
BOOL IsComfortModeEnabled() noexcept;
void SetComfortModeEnabled(BOOL enabled) noexcept;
void SetMenuActive(BOOL active) noexcept;
void RequestRecenter() noexcept;
BOOL IsFlatPanelActive() noexcept;
void RegisterStereoToggleCallback(
    StereoToggleCallback callback) noexcept;
BOOL GetRenderRequest(FearVrRenderRequest* request) noexcept;
BOOL WaitForNewRenderRequest(
    std::uint64_t previousFrameId,
    std::uint32_t timeoutMilliseconds,
    FearVrRenderRequest* request) noexcept;
BOOL GetInputState(FearVrInputState* input) noexcept;
BOOL SubmitHapticRequest(const FearVrHapticRequest* request) noexcept;
void BeginEye(std::uint32_t eye) noexcept;
void CaptureEye(std::uint32_t eye) noexcept;
void EndStereoFrame(std::uint64_t frameId) noexcept;
void ReportHookStatus(const char* level, const char* event,
                      const char* message) noexcept;

} // namespace fearvr

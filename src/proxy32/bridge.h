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
BOOL InstallLateD3D9Hooks() noexcept;

BOOL IsHostConnected() noexcept;
BOOL IsStereoAvailable() noexcept;
BOOL IsStereoEnabled() noexcept;
void RegisterStereoToggleCallback(
    StereoToggleCallback callback) noexcept;
BOOL GetRenderRequest(FearVrRenderRequest* request) noexcept;
void BeginEye(std::uint32_t eye) noexcept;
void CaptureEye(std::uint32_t eye) noexcept;
void EndStereoFrame(std::uint64_t frameId) noexcept;
void ReportHookStatus(const char* level, const char* event,
                      const char* message) noexcept;

} // namespace fearvr

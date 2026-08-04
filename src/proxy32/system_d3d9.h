#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

namespace fearvr {

FARPROC ResolveSystemD3D9Name(const char* name) noexcept;
FARPROC ResolveSystemD3D9Ordinal(unsigned ordinal) noexcept;
const wchar_t* SystemD3D9Path() noexcept;
DWORD SystemD3D9LoadError() noexcept;
const wchar_t* UpstreamD3D9Path() noexcept;
bool UpstreamD3D9Present() noexcept;
bool UpstreamD3D9Loaded() noexcept;
DWORD UpstreamD3D9LoadError() noexcept;

template <typename Function>
Function ResolveSystemD3D9(const char* name) noexcept {
    return reinterpret_cast<Function>(ResolveSystemD3D9Name(name));
}

} // namespace fearvr

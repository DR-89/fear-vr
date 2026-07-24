#include "system_d3d9.h"

#include <array>
#include <cwchar>

namespace fearvr {
namespace {

INIT_ONCE g_initOnce = INIT_ONCE_STATIC_INIT;
HMODULE g_systemD3D9 = nullptr;
DWORD g_loadError = ERROR_SUCCESS;
std::array<wchar_t, MAX_PATH> g_systemPath{};

BOOL CALLBACK LoadSystemD3D9(PINIT_ONCE, PVOID, PVOID*) {
    const UINT length = GetSystemDirectoryW(
        g_systemPath.data(), static_cast<UINT>(g_systemPath.size()));
    if (length == 0 || length >= g_systemPath.size()) {
        g_loadError = GetLastError();
        return TRUE;
    }
    constexpr wchar_t suffix[] = L"\\d3d9.dll";
    if (length + (sizeof(suffix) / sizeof(suffix[0])) >
        g_systemPath.size()) {
        g_loadError = ERROR_INSUFFICIENT_BUFFER;
        return TRUE;
    }
    wcscat_s(g_systemPath.data(), g_systemPath.size(), suffix);
    g_systemD3D9 =
        LoadLibraryExW(g_systemPath.data(), nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (g_systemD3D9 == nullptr) {
        g_loadError = GetLastError();
    }
    return TRUE;
}

HMODULE SystemModule() noexcept {
    InitOnceExecuteOnce(&g_initOnce, LoadSystemD3D9, nullptr, nullptr);
    return g_systemD3D9;
}

} // namespace

FARPROC ResolveSystemD3D9Name(const char* name) noexcept {
    const HMODULE module = SystemModule();
    return module == nullptr ? nullptr : GetProcAddress(module, name);
}

FARPROC ResolveSystemD3D9Ordinal(unsigned ordinal) noexcept {
    const HMODULE module = SystemModule();
    return module == nullptr
               ? nullptr
               : GetProcAddress(module, MAKEINTRESOURCEA(ordinal));
}

const wchar_t* SystemD3D9Path() noexcept {
    (void)SystemModule();
    return g_systemPath.data();
}

DWORD SystemD3D9LoadError() noexcept {
    (void)SystemModule();
    return g_loadError;
}

} // namespace fearvr

#include "system_d3d9.h"

#include <array>
#include <cwchar>

namespace fearvr {
namespace {

INIT_ONCE g_initOnce = INIT_ONCE_STATIC_INIT;
HMODULE g_systemD3D9 = nullptr;
HMODULE g_upstreamD3D9 = nullptr;
DWORD g_loadError = ERROR_SUCCESS;
std::array<wchar_t, MAX_PATH> g_systemPath{};
std::array<wchar_t, MAX_PATH> g_upstreamPath{};
DWORD g_upstreamLoadError = ERROR_SUCCESS;
bool g_upstreamPresent = false;

BOOL CALLBACK LoadSystemD3D9(PINIT_ONCE, PVOID, PVOID*) {
    HMODULE self = nullptr;
    std::array<wchar_t, MAX_PATH> siblingPath{};
    if (GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&LoadSystemD3D9), &self)) {
        const DWORD selfLength = GetModuleFileNameW(
            self, siblingPath.data(),
            static_cast<DWORD>(siblingPath.size()));
        if (selfLength != 0 &&
            selfLength < siblingPath.size()) {
            wchar_t* separator =
                wcsrchr(siblingPath.data(), L'\\');
            if (separator != nullptr) {
                *(separator + 1) = L'\0';
                constexpr wchar_t upstreamName[] =
                    L"d3d9.fearvr-upstream.dll";
                if (wcslen(siblingPath.data()) +
                        wcslen(upstreamName) <
                    siblingPath.size()) {
                    wcscat_s(
                        siblingPath.data(), siblingPath.size(),
                        upstreamName);
                    wcscpy_s(
                        g_upstreamPath.data(), g_upstreamPath.size(),
                        siblingPath.data());
                    if (GetFileAttributesW(siblingPath.data()) !=
                        INVALID_FILE_ATTRIBUTES) {
                        g_upstreamPresent = true;
                        g_upstreamD3D9 = LoadLibraryExW(
                            siblingPath.data(), nullptr,
                            LOAD_WITH_ALTERED_SEARCH_PATH);
                        if (g_upstreamD3D9 == nullptr) {
                            g_upstreamLoadError = GetLastError();
                        }
                    }
                }
            }
        }
    }

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
    if (g_upstreamD3D9 != nullptr) {
        const FARPROC upstream =
            GetProcAddress(g_upstreamD3D9, name);
        if (upstream != nullptr) {
            return upstream;
        }
    }
    return module == nullptr ? nullptr
                             : GetProcAddress(module, name);
}

FARPROC ResolveSystemD3D9Ordinal(unsigned ordinal) noexcept {
    const HMODULE module = SystemModule();
    if (g_upstreamD3D9 != nullptr) {
        const FARPROC upstream = GetProcAddress(
            g_upstreamD3D9, MAKEINTRESOURCEA(ordinal));
        if (upstream != nullptr) {
            return upstream;
        }
    }
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

const wchar_t* UpstreamD3D9Path() noexcept {
    (void)SystemModule();
    return g_upstreamPath.data();
}

bool UpstreamD3D9Present() noexcept {
    (void)SystemModule();
    return g_upstreamPresent;
}

bool UpstreamD3D9Loaded() noexcept {
    (void)SystemModule();
    return g_upstreamD3D9 != nullptr;
}

DWORD UpstreamD3D9LoadError() noexcept {
    (void)SystemModule();
    return g_upstreamLoadError;
}

} // namespace fearvr

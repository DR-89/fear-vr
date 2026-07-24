#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

extern "C" int FearVrGameClientCompatData = 0;

namespace {

INIT_ONCE g_originalOnce = INIT_ONCE_STATIC_INIT;
INIT_ONCE g_bridgeOnce = INIT_ONCE_STATIC_INIT;
HMODULE g_original = nullptr;
HMODULE g_bridge = nullptr;

bool ModuleSiblingPath(const wchar_t* fileName,
                       wchar_t (&path)[MAX_PATH]) noexcept {
    HMODULE self = nullptr;
    if (!GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&ModuleSiblingPath), &self)) {
        return false;
    }
    const DWORD length = GetModuleFileNameW(self, path, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return false;
    }
    wchar_t* separator = wcsrchr(path, L'\\');
    if (separator == nullptr) {
        return false;
    }
    *(separator + 1) = L'\0';
    return wcscat_s(path, fileName) == 0;
}

BOOL CALLBACK LoadBridge(PINIT_ONCE once, PVOID parameter,
                         PVOID* context) {
    (void)once;
    (void)parameter;
    (void)context;
    wchar_t path[MAX_PATH]{};
    if (!ModuleSiblingPath(L"fearvr-d3d9.dll", path)) {
        return TRUE;
    }
    g_bridge = LoadLibraryExW(
        path, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (g_bridge != nullptr) {
        using InstallFunction = BOOL(__cdecl*)();
        const auto install = reinterpret_cast<InstallFunction>(
            GetProcAddress(g_bridge, "FearVr_InstallIatHook"));
        if (install != nullptr) {
            install();
        }
    }
    return TRUE;
}

void EnsureBridge() noexcept {
    InitOnceExecuteOnce(&g_bridgeOnce, LoadBridge, nullptr, nullptr);
}

BOOL CALLBACK LoadOriginal(PINIT_ONCE once, PVOID parameter,
                           PVOID* context) {
    (void)once;
    (void)parameter;
    (void)context;

    wchar_t path[MAX_PATH]{};
    if (!ModuleSiblingPath(L"GameOrig.dll", path)) {
        return TRUE;
    }
    g_original = LoadLibraryExW(
        path, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    return TRUE;
}

HMODULE OriginalModule() noexcept {
    InitOnceExecuteOnce(&g_originalOnce, LoadOriginal, nullptr, nullptr);
    return g_original;
}

} // namespace

extern "C" unsigned long GetBuildNumber() {
    EnsureBridge();
    using Function = unsigned long(__cdecl*)();
    HMODULE original = OriginalModule();
    const auto function = original == nullptr
        ? nullptr
        : reinterpret_cast<Function>(
              GetProcAddress(original, "GetBuildNumber"));
    return function == nullptr ? 0UL : function();
}

extern "C" void SetMasterDatabase(void* masterDatabase) {
    EnsureBridge();
    using Function = void(__cdecl*)(void*);
    HMODULE original = OriginalModule();
    const auto function = original == nullptr
        ? nullptr
        : reinterpret_cast<Function>(
              GetProcAddress(original, "SetMasterDatabase"));
    if (function != nullptr) {
        function(masterDatabase);
    }
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
    }
    return TRUE;
}

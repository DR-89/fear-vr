#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <Shellapi.h>
#include <d3d9.h>
#include <dinput.h>

#include "fear_hid_fix.h"
#include "../proxy32/iat_hook.h"

#include <cstdio>
#include <cwchar>

namespace {

INIT_ONCE g_systemDinputOnce = INIT_ONCE_STATIC_INIT;
INIT_ONCE g_earlyBridgeOnce = INIT_ONCE_STATIC_INIT;
HMODULE g_systemDinput = nullptr;
HMODULE g_earlyBridge = nullptr;
fearvr::FearHidFixResult g_hidFixResult =
    fearvr::FearHidFixResult::NotFear108;
volatile LONG g_diagnosticWritten = 0;

using DirectInput8CreateFunction = HRESULT(WINAPI*)(
    HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);
using DllCanUnloadNowFunction = HRESULT(WINAPI*)();
using DllGetClassObjectFunction = HRESULT(WINAPI*)(
    REFCLSID, REFIID, LPVOID*);
using DllRegisterServerFunction = HRESULT(WINAPI*)();
using DllUnregisterServerFunction = HRESULT(WINAPI*)();
using GetdfDIJoystickFunction = LPCDIDATAFORMAT(WINAPI*)();

bool ReadCommandLineValue(const wchar_t* option,
                          wchar_t (&value)[MAX_PATH]) noexcept {
    int count = 0;
    wchar_t** arguments =
        CommandLineToArgvW(GetCommandLineW(), &count);
    if (arguments == nullptr) {
        return false;
    }
    bool found = false;
    for (int index = 1; index + 1 < count; ++index) {
        if (_wcsicmp(arguments[index], option) == 0) {
            found = wcscpy_s(value, arguments[index + 1]) == 0;
            break;
        }
    }
    LocalFree(arguments);
    return found;
}

BOOL CALLBACK LoadEarlyBridge(PINIT_ONCE, PVOID,
                              PVOID*) noexcept {
    wchar_t path[MAX_PATH]{};
    if (!ReadCommandLineValue(L"-fearvr-bridge", path)) {
        return TRUE;
    }
    g_earlyBridge = LoadLibraryExW(
        path, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (g_earlyBridge == nullptr) {
        OutputDebugStringA(
            "F.E.A.R. VR early D3D9 bridge load failed.\n");
        return TRUE;
    }
    OutputDebugStringA(
        "F.E.A.R. VR early D3D9 bridge loaded.\n");
    return TRUE;
}

FARPROC EarlyBridgeExport(const char* name) noexcept {
    InitOnceExecuteOnce(
        &g_earlyBridgeOnce, LoadEarlyBridge, nullptr, nullptr);
    return g_earlyBridge == nullptr
        ? nullptr
        : GetProcAddress(g_earlyBridge, name);
}

IDirect3D9* SystemDirect3DCreate9(UINT sdkVersion) noexcept {
    wchar_t path[MAX_PATH]{};
    const UINT length = GetSystemDirectoryW(path, MAX_PATH);
    if (length == 0 || length >= MAX_PATH ||
        wcscat_s(path, L"\\d3d9.dll") != 0) {
        return nullptr;
    }
    const HMODULE module = LoadLibraryW(path);
    if (module == nullptr) {
        return nullptr;
    }
    using Function = IDirect3D9*(WINAPI*)(UINT);
    const auto function = reinterpret_cast<Function>(
        GetProcAddress(module, "Direct3DCreate9"));
    return function == nullptr ? nullptr : function(sdkVersion);
}

BOOL CALLBACK LoadSystemDinput(PINIT_ONCE once, PVOID parameter,
                               PVOID* context) noexcept {
    (void)once;
    (void)parameter;
    (void)context;

    wchar_t path[MAX_PATH]{};
    const UINT length = GetSystemDirectoryW(path, MAX_PATH);
    if (length == 0 || length >= MAX_PATH ||
        wcscat_s(path, L"\\dinput8.dll") != 0) {
        return TRUE;
    }
    g_systemDinput = LoadLibraryW(path);
    return TRUE;
}

FARPROC SystemExport(const char* name) noexcept {
    InitOnceExecuteOnce(
        &g_systemDinputOnce, LoadSystemDinput, nullptr, nullptr);
    return g_systemDinput == nullptr
        ? nullptr
        : GetProcAddress(g_systemDinput, name);
}

bool ReadLogDirectory(wchar_t (&directory)[MAX_PATH]) noexcept {
    int count = 0;
    wchar_t** arguments =
        CommandLineToArgvW(GetCommandLineW(), &count);
    if (arguments == nullptr) {
        return false;
    }
    bool found = false;
    for (int index = 1; index + 1 < count; ++index) {
        if (_wcsicmp(arguments[index], L"-fearvr-logdir") == 0) {
            found = wcscpy_s(directory, arguments[index + 1]) == 0;
            break;
        }
    }
    LocalFree(arguments);
    return found;
}

void WriteDiagnostic(const HRESULT createResult) noexcept {
    if (InterlockedCompareExchange(
            &g_diagnosticWritten, 1, 0) != 0) {
        return;
    }

    wchar_t directory[MAX_PATH]{};
    if (!ReadLogDirectory(directory)) {
        return;
    }
    wchar_t path[MAX_PATH]{};
    if (swprintf_s(
            path, L"%s\\dinput-%lu.log", directory,
            static_cast<unsigned long>(GetCurrentProcessId())) < 0) {
        return;
    }

    const HANDLE file = CreateFileW(
        path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }

    SYSTEMTIME now{};
    GetSystemTime(&now);
    char line[768]{};
    const int length = snprintf(
        line, sizeof(line),
        "{\"time\":\"%04u-%02u-%02uT%02u:%02u:%02u.%03uZ\","
        "\"level\":\"INFO\",\"event\":\"fear_hid_fix\","
        "\"message\":\"result=%s direct_input_create=0x%08lX "
        "legacy_input_hook_bytes=%zu redundant_hid_bytes=%zu\"}\r\n",
        now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute,
        now.wSecond, now.wMilliseconds,
        fearvr::FearHidFixResultName(g_hidFixResult),
        static_cast<unsigned long>(createResult),
        fearvr::kLegacyInputHookBytes.size(),
        fearvr::kRedundantHidDeviceBytes.size());
    if (length > 0) {
        DWORD written = 0;
        WriteFile(file, line, static_cast<DWORD>(length), &written, nullptr);
    }
    CloseHandle(file);
}

} // namespace

extern "C" IDirect3D9* WINAPI FearVrEarlyDirect3DCreate9(
    UINT sdkVersion) {
    using Function = IDirect3D9*(WINAPI*)(UINT);
    const auto function = reinterpret_cast<Function>(
        EarlyBridgeExport("Direct3DCreate9"));
    return function == nullptr
        ? SystemDirect3DCreate9(sdkVersion)
        : function(sdkVersion);
}

extern "C" HRESULT WINAPI DirectInput8Create(
    HINSTANCE instance, DWORD version, REFIID interfaceId,
    LPVOID* output, LPUNKNOWN outer) {
    const auto function = reinterpret_cast<DirectInput8CreateFunction>(
        SystemExport("DirectInput8Create"));
    const HRESULT result = function == nullptr
        ? E_FAIL
        : function(instance, version, interfaceId, output, outer);
    WriteDiagnostic(result);
    return result;
}

extern "C" HRESULT WINAPI DllCanUnloadNow() {
    const auto function = reinterpret_cast<DllCanUnloadNowFunction>(
        SystemExport("DllCanUnloadNow"));
    return function == nullptr ? S_FALSE : function();
}

extern "C" HRESULT WINAPI DllGetClassObject(
    REFCLSID classId, REFIID interfaceId, LPVOID* output) {
    const auto function = reinterpret_cast<DllGetClassObjectFunction>(
        SystemExport("DllGetClassObject"));
    return function == nullptr
        ? CLASS_E_CLASSNOTAVAILABLE
        : function(classId, interfaceId, output);
}

extern "C" HRESULT WINAPI DllRegisterServer() {
    const auto function = reinterpret_cast<DllRegisterServerFunction>(
        SystemExport("DllRegisterServer"));
    return function == nullptr ? E_FAIL : function();
}

extern "C" HRESULT WINAPI DllUnregisterServer() {
    const auto function = reinterpret_cast<DllUnregisterServerFunction>(
        SystemExport("DllUnregisterServer"));
    return function == nullptr ? E_FAIL : function();
}

extern "C" LPCDIDATAFORMAT WINAPI GetdfDIJoystick() {
    const auto function = reinterpret_cast<GetdfDIJoystickFunction>(
        SystemExport("GetdfDIJoystick"));
    return function == nullptr ? nullptr : function();
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        g_hidFixResult = fearvr::ApplyFear108HidFix();
        // dinput8.dll is loaded before Retail creates its renderer. Loading
        // the bridge at the first Direct3DCreate9 call is then outside the
        // loader lock, but still guaranteed to happen before CreateDevice.
        // The bridge can therefore add D3DCREATE_MULTITHREADED safely.
        fearvr::InstallDirect3DCreate9IatHook(
            reinterpret_cast<void*>(
                &FearVrEarlyDirect3DCreate9));
        char message[160]{};
        snprintf(
            message, sizeof(message),
            "F.E.A.R. VR early HID fix: %s\n",
            fearvr::FearHidFixResultName(g_hidFixResult));
        OutputDebugStringA(message);
    }
    return TRUE;
}

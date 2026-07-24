#include <cstdio>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <d3d9.h>

int wmain(int argumentCount, wchar_t** arguments) {
    if (argumentCount != 2) {
        std::fprintf(stderr,
                     "usage: iat_hook_test <fearvr-d3d9.dll>\n");
        return 2;
    }
    HMODULE bridge = LoadLibraryExW(
        arguments[1], nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (bridge == nullptr) {
        std::fprintf(stderr, "Bridge load failed: Win32=%lu\n",
                     GetLastError());
        return 3;
    }
    using InstallFunction = BOOL(__cdecl*)();
    const auto install = reinterpret_cast<InstallFunction>(
        GetProcAddress(bridge, "FearVr_InstallIatHook"));
    if (install == nullptr || !install()) {
        std::fprintf(stderr, "Direct3DCreate9 IAT hook failed.\n");
        return 4;
    }
    IDirect3D9* direct3D = Direct3DCreate9(D3D_SDK_VERSION);
    if (direct3D == nullptr) {
        std::fprintf(stderr,
                     "Forwarded Direct3DCreate9 returned null.\n");
        return 5;
    }

    const HWND window = CreateWindowExW(
        0, L"STATIC", L"FearVrIatHookTest", WS_OVERLAPPED,
        0, 0, 64, 64, nullptr, nullptr, GetModuleHandleW(nullptr),
        nullptr);
    if (window == nullptr) {
        std::fprintf(stderr, "Test window creation failed: Win32=%lu\n",
                     GetLastError());
        direct3D->Release();
        return 6;
    }
    D3DPRESENT_PARAMETERS parameters{};
    parameters.Windowed = TRUE;
    parameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
    parameters.hDeviceWindow = window;
    parameters.BackBufferWidth = 64;
    parameters.BackBufferHeight = 64;
    parameters.BackBufferFormat = D3DFMT_UNKNOWN;

    IDirect3DDevice9* device = nullptr;
    HRESULT result = direct3D->CreateDevice(
        D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE,
        &parameters, &device);
    if (FAILED(result) || device == nullptr) {
        std::fprintf(stderr,
                     "Forwarded CreateDevice failed: HRESULT=0x%08lX\n",
                     static_cast<unsigned long>(result));
        DestroyWindow(window);
        direct3D->Release();
        return 7;
    }

    result = device->Present(nullptr, nullptr, nullptr, nullptr);
    if (FAILED(result)) {
        std::fprintf(stderr,
                     "Late-hooked Present failed: HRESULT=0x%08lX\n",
                     static_cast<unsigned long>(result));
        device->Release();
        DestroyWindow(window);
        direct3D->Release();
        return 8;
    }

    result = device->Reset(&parameters);
    if (FAILED(result)) {
        std::fprintf(stderr,
                     "Late-hooked Reset failed: HRESULT=0x%08lX\n",
                     static_cast<unsigned long>(result));
        device->Release();
        DestroyWindow(window);
        direct3D->Release();
        return 9;
    }
    device->Release();
    DestroyWindow(window);
    direct3D->Release();
    std::printf("iat_hook_test: OK (IAT + late Present/Reset)\n");
    return 0;
}

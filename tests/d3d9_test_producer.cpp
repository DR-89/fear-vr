#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <d3d9.h>

namespace {

struct Options {
    std::uint64_t frameLimit{900};
    std::uint64_t adapterLuid{0};
    bool classicD3D9{false};
    bool stereo{false};
};

using BeginEyeFunction = void(__cdecl*)(std::uint32_t);
using CaptureEyeFunction = void(__cdecl*)(std::uint32_t);
using EndStereoFrameFunction = void(__cdecl*)(std::uint64_t);

LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM wparam,
                                 LPARAM lparam) {
    if (message == WM_CLOSE) {
        DestroyWindow(window);
        return 0;
    }
    if (message == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

Options ParseOptions(int argumentCount, char** arguments) {
    Options options;
    for (int index = 1; index < argumentCount; ++index) {
        const std::string argument = arguments[index];
        if (argument == "--classic-d3d9") {
            options.classicD3D9 = true;
            continue;
        }
        if (argument == "--stereo") {
            options.stereo = true;
            continue;
        }
        if (index + 1 >= argumentCount) {
            continue;
        }
        const int radix = argument == "--adapter-luid" ? 0 : 10;
        if (argument != "--frames" && argument != "--adapter-luid") {
            continue;
        }
        char* end = nullptr;
        const unsigned long long parsed =
            std::strtoull(arguments[index + 1], &end, radix);
        if (end != arguments[index + 1] && *end == '\0' && parsed != 0) {
            if (argument == "--frames") {
                options.frameLimit = static_cast<std::uint64_t>(parsed);
            } else {
                options.adapterLuid =
                    static_cast<std::uint64_t>(parsed);
            }
        }
    }
    return options;
}

bool PumpMessages() {
    MSG message{};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
        if (message.message == WM_QUIT) {
            return false;
        }
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return true;
}

D3DCOLOR FrameColor(std::uint64_t frame) {
    const auto red = static_cast<unsigned>((frame * 5U) & 0xFFU);
    const auto green = static_cast<unsigned>((frame * 3U + 85U) & 0xFFU);
    const auto blue = static_cast<unsigned>((frame * 7U + 170U) & 0xFFU);
    return D3DCOLOR_XRGB(red, green, blue);
}

D3DCOLOR StereoEyeColor(std::uint64_t frame, std::uint32_t eye) {
    const auto pulse = static_cast<unsigned>((frame * 3U) & 0x7FU);
    return eye == 0
        ? D3DCOLOR_XRGB(128U + pulse, 24U, 16U)
        : D3DCOLOR_XRGB(16U, 24U, 128U + pulse);
}

HRESULT ResetDevice(IDirect3DDevice9* device,
                    D3DPRESENT_PARAMETERS& parameters, HWND window) {
    constexpr LONG width = 800;
    constexpr LONG height = 450;
    SetWindowPos(window, nullptr, 0, 0, width, height,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    parameters.BackBufferWidth = width;
    parameters.BackBufferHeight = height;
    const HRESULT result = device->Reset(&parameters);
    std::printf("M2 reset: HRESULT=0x%08lX size=%ldx%ld\n",
                static_cast<unsigned long>(result), width, height);
    return result;
}

} // namespace

extern "C" {
__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001UL;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

int main(int argumentCount, char** arguments) {
    const Options options = ParseOptions(argumentCount, arguments);
    const std::uint64_t frameLimit = options.frameLimit;

    HINSTANCE instance = GetModuleHandleW(nullptr);
    const wchar_t className[] = L"FearVrM2D3d9Producer";
    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = WindowProcedure;
    windowClass.hInstance = instance;
    windowClass.hCursor =
        LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    windowClass.lpszClassName = className;
    if (RegisterClassW(&windowClass) == 0) {
        std::fprintf(stderr, "RegisterClassW failed: %lu\n", GetLastError());
        return 2;
    }

    HWND window = CreateWindowExW(
        0, className, L"F.E.A.R. VR M2 D3D9 Producer",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 960,
        540, nullptr, nullptr, instance, nullptr);
    if (window == nullptr) {
        std::fprintf(stderr, "CreateWindowExW failed: %lu\n", GetLastError());
        return 3;
    }

    IDirect3D9Ex* d3dEx = nullptr;
    const HRESULT createD3dResult =
        Direct3DCreate9Ex(D3D_SDK_VERSION, &d3dEx);
    if (FAILED(createD3dResult) || d3dEx == nullptr) {
        std::fprintf(stderr, "Direct3DCreate9Ex failed: 0x%08lX\n",
                     static_cast<unsigned long>(createD3dResult));
        DestroyWindow(window);
        return 4;
    }
    IDirect3D9* d3d = options.classicD3D9
        ? Direct3DCreate9(D3D_SDK_VERSION)
        : static_cast<IDirect3D9*>(d3dEx);
    if (d3d == nullptr) {
        std::fprintf(stderr, "Direct3DCreate9 failed.\n");
        d3dEx->Release();
        DestroyWindow(window);
        return 4;
    }

    UINT selectedAdapter = D3DADAPTER_DEFAULT;
    bool selectedAdapterFound = options.adapterLuid == 0;
    for (UINT adapter = 0; adapter < d3dEx->GetAdapterCount(); ++adapter) {
        LUID luid{};
        D3DADAPTER_IDENTIFIER9 identifier{};
        if (FAILED(d3dEx->GetAdapterLUID(adapter, &luid)) ||
            FAILED(d3dEx->GetAdapterIdentifier(
                adapter, 0, &identifier))) {
            continue;
        }
        const std::uint64_t packed =
            (static_cast<std::uint64_t>(
                 static_cast<std::uint32_t>(luid.HighPart))
             << 32U) |
            luid.LowPart;
        std::printf("M2 D3D9 adapter=%u luid=0x%016llX name=%s\n", adapter,
                    static_cast<unsigned long long>(packed),
                    identifier.Description);
        if (options.adapterLuid != 0 && packed == options.adapterLuid) {
            selectedAdapter = adapter;
            selectedAdapterFound = true;
        }
    }
    if (!selectedAdapterFound) {
        std::fprintf(stderr,
                     "Requested adapter LUID 0x%016llX is unavailable.\n",
                     static_cast<unsigned long long>(
                         options.adapterLuid));
        if (options.classicD3D9) {
            d3d->Release();
        }
        d3dEx->Release();
        DestroyWindow(window);
        return 6;
    }

    D3DPRESENT_PARAMETERS parameters{};
    parameters.BackBufferWidth = 960;
    parameters.BackBufferHeight = 540;
    parameters.BackBufferFormat = D3DFMT_X8R8G8B8;
    parameters.BackBufferCount = 1;
    parameters.MultiSampleType = D3DMULTISAMPLE_NONE;
    parameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
    parameters.hDeviceWindow = window;
    parameters.Windowed = TRUE;
    parameters.EnableAutoDepthStencil = FALSE;
    parameters.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

    IDirect3DDevice9* device = nullptr;
    HRESULT result = D3DERR_INVALIDCALL;
    if (options.classicD3D9) {
        result = d3d->CreateDevice(
            selectedAdapter, D3DDEVTYPE_HAL, window,
            D3DCREATE_HARDWARE_VERTEXPROCESSING |
                D3DCREATE_MULTITHREADED,
            &parameters, &device);
        if (FAILED(result)) {
            result = d3d->CreateDevice(
                selectedAdapter, D3DDEVTYPE_HAL, window,
                D3DCREATE_SOFTWARE_VERTEXPROCESSING |
                    D3DCREATE_MULTITHREADED,
                &parameters, &device);
        }
    } else {
        IDirect3DDevice9Ex* deviceEx = nullptr;
        result = d3dEx->CreateDeviceEx(
            selectedAdapter, D3DDEVTYPE_HAL, window,
            D3DCREATE_HARDWARE_VERTEXPROCESSING |
                D3DCREATE_MULTITHREADED,
            &parameters, nullptr, &deviceEx);
        if (FAILED(result)) {
            result = d3dEx->CreateDeviceEx(
                selectedAdapter, D3DDEVTYPE_HAL, window,
                D3DCREATE_SOFTWARE_VERTEXPROCESSING |
                    D3DCREATE_MULTITHREADED,
                &parameters, nullptr, &deviceEx);
        }
        device = deviceEx;
    }
    if (FAILED(result)) {
        std::fprintf(stderr, "CreateDevice failed: 0x%08lX\n",
                     static_cast<unsigned long>(result));
        if (options.classicD3D9) {
            d3d->Release();
        }
        d3dEx->Release();
        DestroyWindow(window);
        return 5;
    }

    std::printf("M2 D3D9 producer started: frames=%llu mode=%s\n",
                static_cast<unsigned long long>(frameLimit),
                options.classicD3D9 ? "classic" : "ex");
    HMODULE proxy = GetModuleHandleW(L"d3d9.dll");
    const auto beginEye = proxy == nullptr
        ? nullptr
        : reinterpret_cast<BeginEyeFunction>(
              GetProcAddress(proxy, "FearVr_BeginEye"));
    const auto captureEye = proxy == nullptr
        ? nullptr
        : reinterpret_cast<CaptureEyeFunction>(
              GetProcAddress(proxy, "FearVr_CaptureEye"));
    const auto endStereoFrame = proxy == nullptr
        ? nullptr
        : reinterpret_cast<EndStereoFrameFunction>(
              GetProcAddress(proxy, "FearVr_EndStereoFrame"));
    if (options.stereo &&
        (beginEye == nullptr || captureEye == nullptr ||
         endStereoFrame == nullptr)) {
        std::fprintf(stderr, "M3 stereo bridge exports are unavailable.\n");
        device->Release();
        if (options.classicD3D9) {
            d3d->Release();
        }
        d3dEx->Release();
        DestroyWindow(window);
        return 7;
    }
    if (options.stereo) {
        std::printf(
            "M3 stereo producer active: left=red right=blue\n");
    }

    bool resetDone = false;
    bool running = true;
    for (std::uint64_t frame = 1; running && frame <= frameLimit; ++frame) {
        running = PumpMessages();

        if (frame == frameLimit / 3U) {
            ShowWindow(window, SW_MINIMIZE);
            Sleep(100);
            ShowWindow(window, SW_RESTORE);
            std::printf("M2 minimize/restore completed at frame=%llu\n",
                        static_cast<unsigned long long>(frame));
        }
        if (!resetDone && frame >= frameLimit / 2U) {
            resetDone = SUCCEEDED(ResetDevice(device, parameters, window));
        }

        if (options.stereo) {
            result = D3D_OK;
            for (std::uint32_t eye = 0; eye < 2; ++eye) {
                result = device->Clear(
                    0, nullptr, D3DCLEAR_TARGET,
                    StereoEyeColor(frame, eye), 1.0F, 0);
                if (FAILED(result)) {
                    break;
                }
                beginEye(eye);
                captureEye(eye);
            }
            endStereoFrame(frame);
        } else {
            result = device->Clear(
                0, nullptr, D3DCLEAR_TARGET,
                FrameColor(frame), 1.0F, 0);
        }
        if (SUCCEEDED(result)) {
            result = device->Present(nullptr, nullptr, nullptr, nullptr);
        }
        if (result == D3DERR_DEVICELOST) {
            Sleep(10);
            continue;
        }
        if (FAILED(result)) {
            std::fprintf(stderr,
                         "Frame %llu failed: HRESULT=0x%08lX\n",
                         static_cast<unsigned long long>(frame),
                         static_cast<unsigned long>(result));
            break;
        }
        if (frame == 1 || frame % 300U == 0) {
            if (options.stereo) {
                std::printf(
                    "M3 producer frame=%llu left=0x%08lX right=0x%08lX\n",
                    static_cast<unsigned long long>(frame),
                    static_cast<unsigned long>(
                        StereoEyeColor(frame, 0)),
                    static_cast<unsigned long>(
                        StereoEyeColor(frame, 1)));
            } else {
                std::printf("M2 producer frame=%llu color=0x%08lX\n",
                            static_cast<unsigned long long>(frame),
                            static_cast<unsigned long>(
                                FrameColor(frame)));
            }
        }
        Sleep(11);
    }

    device->Release();
    if (options.classicD3D9) {
        d3d->Release();
    }
    d3dEx->Release();
    DestroyWindow(window);
    UnregisterClassW(className, instance);
    std::printf("M2 D3D9 producer stopped.\n");
    return 0;
}

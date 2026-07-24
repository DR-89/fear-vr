#include <cerrno>
#include <cstdio>
#include <cstdlib>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <TlHelp32.h>

int wmain(int argumentCount, wchar_t** arguments) {
    if (argumentCount != 2 || arguments[1][0] == L'-') {
        std::fprintf(stderr, "usage: fearvr-module-probe <pid>\n");
        return 2;
    }
    wchar_t* end = nullptr;
    errno = 0;
    const unsigned long parsed =
        ::wcstoul(arguments[1], &end, 10);
    if (errno != 0 || end == arguments[1] || *end != L'\0' ||
        parsed == 0) {
        std::fprintf(stderr, "invalid pid\n");
        return 2;
    }

    HANDLE snapshot = CreateToolhelp32Snapshot(
        TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32,
        static_cast<DWORD>(parsed));
    if (snapshot == INVALID_HANDLE_VALUE) {
        std::fprintf(stderr, "CreateToolhelp32Snapshot: Win32=%lu\n",
                     GetLastError());
        return 3;
    }
    MODULEENTRY32W module{};
    module.dwSize = sizeof(module);
    if (!Module32FirstW(snapshot, &module)) {
        const DWORD error = GetLastError();
        CloseHandle(snapshot);
        std::fprintf(stderr, "Module32FirstW: Win32=%lu\n", error);
        return 4;
    }
    do {
        ::wprintf(L"%ls\t%ls\n", module.szModule,
                  module.szExePath);
        module.dwSize = sizeof(module);
    } while (Module32NextW(snapshot, &module));
    CloseHandle(snapshot);
    return 0;
}

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <limits>
#include <string>

#include <Windows.h>

#include "fearvr-version.h"
#include "openxr_host.h"
#include "protocol.h"

namespace {

BOOL WINAPI ConsoleControlHandler(DWORD controlType) {
    switch (controlType) {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
    case CTRL_CLOSE_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
        fearvr::RequestOpenXrHostStop();
        return TRUE;
    default:
        return FALSE;
    }
}

void PrintUsage() {
    std::printf(
        "Aufruf: fearvr-host [Optionen]\n"
        "  --log-dir <Pfad>     Logverzeichnis (Standard: .\\logs)\n"
        "  --max-frames <N>     Nach N eingereichten XR-Frames sauber beenden\n"
        "  --ipc-session <ID>   M2-IPC-ID (dezimal oder 0x-hexadezimal)\n"
        "  --startup-image <Pfad>  Startbild bis zum ersten Spielframe\n"
        "  --exit-on-game-disconnect  Nach Game-Heartbeat-Timeout beenden\n"
        "  --validate-only      Instance/System/D3D11/Session/Swapchains pruefen\n"
        "  --d3d-debug          D3D11-Debug-Layer anfordern (falls installiert)\n"
        "  --help               Diese Hilfe anzeigen\n");
}

bool ParseUnsigned(const char* text, std::uint64_t& value) {
    if (text == nullptr || text[0] == '\0' || text[0] == '-') {
        return false;
    }
    char* end = nullptr;
    errno = 0;
    const unsigned long long parsed = std::strtoull(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0') {
        return false;
    }
    if (parsed > (std::numeric_limits<std::uint64_t>::max)()) {
        return false;
    }
    value = static_cast<std::uint64_t>(parsed);
    return true;
}

bool ParseSessionId(const char* text, std::uint64_t& value) {
    if (text == nullptr || text[0] == '\0' || text[0] == '-') {
        return false;
    }
    char* end = nullptr;
    errno = 0;
    const unsigned long long parsed = std::strtoull(text, &end, 0);
    if (errno != 0 || end == text || *end != '\0' || parsed == 0) {
        return false;
    }
    value = static_cast<std::uint64_t>(parsed);
    return true;
}

} // namespace

int main(int argc, char** argv) {
    std::printf("fearvr-host %s (%s)\n", FEARVR_VERSION_STRING,
                FEARVR_GIT_HASH);
    std::printf("Protokoll: magic=0x%08X version=%u header=%zu bytes\n",
                static_cast<unsigned>(FEARVR_PROTOCOL_MAGIC),
                static_cast<unsigned>(FEARVR_PROTOCOL_VERSION),
                sizeof(FearVrSharedHeader));

    fearvr::OpenXrHostOptions options;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--help" || argument == "-h") {
            PrintUsage();
            return 0;
        }
        if (argument == "--validate-only") {
            options.validateOnly = true;
            continue;
        }
        if (argument == "--d3d-debug") {
            options.enableD3dDebug = true;
            continue;
        }
        if (argument == "--exit-on-game-disconnect") {
            options.exitOnGameDisconnect = true;
            continue;
        }
        if (argument == "--log-dir") {
            if (++index >= argc) {
                std::fprintf(stderr, "--log-dir benoetigt einen Pfad.\n");
                return 2;
            }
            options.logDirectory = std::filesystem::u8path(argv[index]);
            continue;
        }
        if (argument == "--startup-image") {
            if (++index >= argc) {
                std::fprintf(
                    stderr, "--startup-image benoetigt einen Pfad.\n");
                return 2;
            }
            options.startupImage = std::filesystem::u8path(argv[index]);
            continue;
        }
        if (argument == "--max-frames") {
            if (++index >= argc ||
                !ParseUnsigned(argv[index], options.maxFrames) ||
                options.maxFrames == 0) {
                std::fprintf(stderr,
                             "--max-frames benoetigt eine positive Ganzzahl.\n");
                return 2;
            }
            continue;
        }
        if (argument == "--ipc-session") {
            if (++index >= argc ||
                !ParseSessionId(argv[index], options.ipcSessionId)) {
                std::fprintf(
                    stderr,
                    "--ipc-session benoetigt eine von Null verschiedene ID.\n");
                return 2;
            }
            continue;
        }

        std::fprintf(stderr, "Unbekannte Option: %s\n", argument.c_str());
        PrintUsage();
        return 2;
    }

    if (!SetConsoleCtrlHandler(ConsoleControlHandler, TRUE)) {
        std::fprintf(stderr,
                     "Warnung: Console-Control-Handler konnte nicht gesetzt "
                     "werden (Win32=%lu).\n",
                     GetLastError());
    }

    try {
        return fearvr::RunOpenXrHost(options);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Unbehandelter Hostfehler: %s\n", error.what());
        return 1;
    }
}

// fearvr-host.exe — x64 OpenXR-Host (ANWEISUNG.md §5.1)
//
// M0-Stub: Nur Versionsausgabe und ein klarer Hinweis, dass der eigentliche
// OpenXR-/D3D11-Lebenszyklus in M1 implementiert wird (nach hello_xr-Vorbild).
// KEINE OpenXR-/D3D-Initialisierung hier, solange die Abhängigkeiten (M1) nicht
// mit festem Pin eingebunden sind.
#include <cstdio>

#include "protocol.h"
#include "fearvr-version.h"

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    std::printf("fearvr-host %s (%s)\n", FEARVR_VERSION_STRING, FEARVR_GIT_HASH);
    std::printf("Protokoll: magic=0x%08X version=%u header=%zu bytes\n",
                (unsigned)FEARVR_PROTOCOL_MAGIC,
                (unsigned)FEARVR_PROTOCOL_VERSION,
                sizeof(FearVrSharedHeader));
    std::printf("M0-Stub: OpenXR-/D3D11-Lebenszyklus folgt in M1.\n");
    return 0;
}

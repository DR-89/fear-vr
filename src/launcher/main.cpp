// fearvr-launcher.exe — ANWEISUNG.md §12
//
// M0-Stub. Endziel:
//   1. fearvr-host.exe starten;
//   2. mit Timeout auf "XR ready" warten;
//   3. isolierte FEAR.exe mit -userdirectory "...\stage\userdata" starten;
//   4. nur benötigte Pfade/Session-ID übergeben;
//   5. bei Spielende den zugehörigen Host beenden;
//   6. auf Wunsch Flat-Start ohne VR-Host;
//   7. klare Meldung bei fehlender Runtime, falschem Adapter oder falscher EXE.
//
// Für M0 wird das Staging/Starten über tools/prepare-stage.ps1 und
// tools/launch-vr.ps1 abgebildet; dieser native Launcher wird ab M1/M2 aktiv.
#include <cstdio>

#include "fearvr-version.h"

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    std::printf("fearvr-launcher %s (%s) — M0-Stub.\n",
                FEARVR_VERSION_STRING, FEARVR_GIT_HASH);
    std::printf("Nutze für M0 tools/verify-install.ps1, prepare-stage.ps1, "
                "launch-vr.ps1.\n");
    return 0;
}

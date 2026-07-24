// d3d9.dll Proxy/Bridge (x86) — ANWEISUNG.md §5.2 / §3
//
// WICHTIG: In DllMain nur minimale, loader-lock-sichere Arbeit. KEINE OpenXR-,
// D3D-, Thread- oder IPC-Initialisierung hier. Die echte Bridge-Initialisierung
// erfolgt lazy beim ersten Direct3DCreate9-Aufruf (M2), nicht im Loader-Lock.
//
// M0-Stub: nur die DllMain-Hülle. Export-Weiterleitung und Hooks folgen in M2.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "protocol.h"

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID reserved) {
    (void)hModule;
    (void)reserved;
    switch (reason) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hModule);
            // Bewusst nichts Weiteres hier (Loader-Lock).
            break;
        case DLL_PROCESS_DETACH:
            break;
        default:
            break;
    }
    return TRUE;
}

// ---- Optionale C-ABI der Bridge (Platzhalter, §5.2) -------------------------
// Die tatsächliche Implementierung kommt in M2. Das GameClient-Modul löst diese
// Symbole dynamisch via GetModuleHandleW(L"d3d9.dll") + GetProcAddress auf;
// fehlen sie, bleibt der Original-Renderpfad unverändert.
extern "C" __declspec(dllexport) BOOL FearVr_IsHostConnected(void) {
    return FALSE; // M0: noch keine Host-Verbindung.
}

#pragma once

#include <Windows.h>

namespace fearvr {

// Patcht ausschließlich den Direct3DCreate9-IAT-Eintrag des Hauptmoduls.
// Kein Laden von DLLs, kein D3D- oder IPC-Aufruf; dadurch auch aus dem
// ABI-neutralen GameClient-Loader während dessen PROCESS_ATTACH nutzbar.
BOOL InstallDirect3DCreate9IatHook(void* replacement) noexcept;

} // namespace fearvr

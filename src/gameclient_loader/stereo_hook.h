#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

namespace fearvr {

bool InstallStereoHook(void* masterDatabase, HMODULE bridge) noexcept;

} // namespace fearvr

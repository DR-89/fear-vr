# x86 d3d9.dll Proxy/Bridge — ANWEISUNG.md §5.2
#
# Baut die Bridge unter dem eindeutigen Namen fearvr-d3d9.dll. Der synthetische
# Test kopiert sie ausschließlich in sein eigenes Ausgabeverzeichnis als
# d3d9.dll. Die echte FEAR-Stage lädt den eindeutigen Namen über den
# ABI-neutralen archcfg-Loader. NIEMALS in die Retail-Installation kopieren.
set(FEARVR_MINHOOK_ROOT
  "${CMAKE_SOURCE_DIR}/vendor-local/minhook"
  CACHE PATH "Local pinned checkout of MinHook")
if(NOT EXISTS "${FEARVR_MINHOOK_ROOT}/CMakeLists.txt")
  message(FATAL_ERROR
    "MinHook missing at ${FEARVR_MINHOOK_ROOT}. "
    "Run tools/prepare-dependencies.ps1 first.")
endif()
add_subdirectory(
  "${FEARVR_MINHOOK_ROOT}"
  "${CMAKE_BINARY_DIR}/third_party/minhook"
  EXCLUDE_FROM_ALL
)

add_library(fearvr-proxy SHARED
  bridge.cpp
  bridge.h
  d3d9.def
  dllmain.cpp
  iat_hook.cpp
  iat_hook.h
  proxy_exports.cpp
  system_d3d9.cpp
  system_d3d9.h
)
target_link_libraries(fearvr-proxy PRIVATE
  fearvr-common
  fearvr-version
  fearvr-warnings
  minhook
  dxgi
  shell32
)

# Eindeutiger Modulname für die offizielle archcfg-Stage. Der isolierte
# D3D9-Test kopiert dieselbe Binärdatei gezielt als d3d9.dll neben seine EXE.
set_target_properties(fearvr-proxy PROPERTIES
  OUTPUT_NAME "fearvr-d3d9"
  PREFIX ""
)

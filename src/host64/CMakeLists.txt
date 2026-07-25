# x64 OpenXR host (fearvr-host.exe) - ANWEISUNG.md section 5.1
set(FEARVR_OPENXR_SDK_ROOT
    "${CMAKE_SOURCE_DIR}/vendor-local/openxr-sdk"
    CACHE PATH "Local pinned checkout of the Khronos OpenXR-SDK")

if(NOT EXISTS "${FEARVR_OPENXR_SDK_ROOT}/CMakeLists.txt")
  message(FATAL_ERROR
    "OpenXR-SDK missing at ${FEARVR_OPENXR_SDK_ROOT}. "
    "Run tools/prepare-dependencies.ps1 first.")
endif()

# Build only the static loader. This avoids shipping openxr_loader.dll beside
# the host; runtime selection remains with the official system loader.
set(BUILD_LOADER ON CACHE BOOL "" FORCE)
set(DYNAMIC_LOADER OFF CACHE BOOL "" FORCE)
set(BUILD_API_LAYERS OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_CONFORMANCE_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_SDK_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_ALL_EXTENSIONS OFF CACHE BOOL "" FORCE)
set(INSTALL_SYMBOLS OFF CACHE BOOL "" FORCE)
add_subdirectory(
  "${FEARVR_OPENXR_SDK_ROOT}"
  "${CMAKE_BINARY_DIR}/third_party/openxr-sdk"
  EXCLUDE_FROM_ALL)

add_executable(fearvr-host
  ipc_bridge.cpp
  ipc_bridge.h
  main.cpp
  openxr_host.cpp
  openxr_host.h
  xr_input.cpp
  xr_input.h
  texture_renderer.cpp
  texture_renderer.h
)
target_compile_definitions(fearvr-host PRIVATE
  XR_USE_PLATFORM_WIN32
  XR_USE_GRAPHICS_API_D3D11
  WIN32_LEAN_AND_MEAN
  NOMINMAX
)
target_link_libraries(fearvr-host PRIVATE
  fearvr-common
  fearvr-version
  fearvr-warnings
  OpenXR::openxr_loader
  d3d11
  d3dcompiler
  dxgi
)
set_target_properties(fearvr-host PROPERTIES OUTPUT_NAME "fearvr-host")

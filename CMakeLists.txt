# =============================================================================
# F.E.A.R. VR — Root-CMake (ANWEISUNG.md §11)
#
# Ein Buildverzeichnis ist entweder Win32 ODER x64, nie beides. Getrennt bauen:
#
#   cmake -S . -B build/x86 -A Win32 -DFEARVR_BUILD_PROXY=ON  -DFEARVR_BUILD_HOST=OFF
#   cmake --build build/x86 --config RelWithDebInfo
#
#   cmake -S . -B build/x64 -A x64   -DFEARVR_BUILD_PROXY=OFF -DFEARVR_BUILD_HOST=ON
#   cmake --build build/x64 --config RelWithDebInfo
# =============================================================================
cmake_minimum_required(VERSION 3.21)

project(FearVr
  VERSION 1.0.0
  DESCRIPTION "Open-source VR mod for F.E.A.R. 1.08 (Singleplayer)"
  LANGUAGES C CXX)

# Vorabkennzeichnung nach SemVer. CMake selbst kennt keine Prerelease-Anhaenge,
# deshalb steht das Label hier daneben und wird in fearvr-version.cmake zur
# vollstaendigen Versionszeichenkette zusammengesetzt. Leer lassen fuer ein
# endgueltiges Release. tools/make-release.ps1 liest beide Werte aus dieser
# Datei, damit es nur eine Quelle der Wahrheit gibt.
set(FEARVR_VERSION_LABEL "beta.7")

# ---- Optionen ---------------------------------------------------------------
option(FEARVR_BUILD_HOST  "Build x64 OpenXR host (fearvr-host.exe)" OFF)
option(FEARVR_BUILD_PROXY "Build x86 d3d9.dll proxy/bridge"          OFF)
option(FEARVR_BUILD_TESTS "Build automated tests"                    ON)

# ---- Nur MSVC / Windows -----------------------------------------------------
if(NOT WIN32 OR NOT MSVC)
  message(FATAL_ERROR
    "F.E.A.R. VR baut nur mit MSVC unter Windows (siehe docs/ENVIRONMENT.md).")
endif()

# ---- Architektur-Guard (§11) -----------------------------------------------
# CMAKE_SIZEOF_VOID_P: 4 = Win32/x86, 8 = x64.
if(FEARVR_BUILD_PROXY AND NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
  message(FATAL_ERROR
    "FEARVR_BUILD_PROXY erfordert einen x86-Build: -A Win32.")
endif()
if(FEARVR_BUILD_HOST AND NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR
    "FEARVR_BUILD_HOST erfordert einen x64-Build: -A x64.")
endif()

# ---- Globale Einstellungen --------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
endif()

# PDBs immer lokal erzeugen (auch in Release-ähnlichen Configs).
add_compile_options($<$<CONFIG:Release>:/Zi>)
add_link_options($<$<CONFIG:Release>:/DEBUG>)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/fearvr-warnings.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/fearvr-version.cmake)

# ---- Gemeinsamer Vertrag (header-only, x86 & x64) ---------------------------
add_library(fearvr-common INTERFACE)
target_include_directories(fearvr-common INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/src/common)

add_library(fearvr-hid-fix STATIC
  src/common/fear_hid_fix.cpp
  src/common/fear_hid_fix.h
)
target_link_libraries(fearvr-hid-fix PRIVATE
  fearvr-common
  fearvr-warnings
)
target_compile_definitions(fearvr-hid-fix PRIVATE
  WIN32_LEAN_AND_MEAN
  NOMINMAX
)

message(STATUS "FearVr ${PROJECT_VERSION} | ptr=${CMAKE_SIZEOF_VOID_P}*8 bit "
               "| HOST=${FEARVR_BUILD_HOST} PROXY=${FEARVR_BUILD_PROXY} "
               "TESTS=${FEARVR_BUILD_TESTS}")

# ---- Subprojekte ------------------------------------------------------------
if(FEARVR_BUILD_HOST)
  add_subdirectory(src/host64)
endif()
if(FEARVR_BUILD_PROXY)
  add_subdirectory(src/dinput8_proxy)
  add_subdirectory(src/proxy32)
  add_subdirectory(src/gameclient_loader)
  add_subdirectory(src/launcher)   # Launcher ist x86-nah, baut mit dem Proxy
endif()
if(FEARVR_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

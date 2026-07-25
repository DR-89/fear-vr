# Versionsinformationen in Host, Proxy, Protokoll und Logs (§11).
# Erzeugt einen generierten Header aus fearvr-version.h.in.
set(FEARVR_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(FEARVR_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(FEARVR_VERSION_PATCH ${PROJECT_VERSION_PATCH})

# Vollstaendige SemVer-Zeichenkette inklusive Prerelease-Anhang aus
# FEARVR_VERSION_LABEL (in CMakeLists.txt gesetzt, darf leer sein).
if(FEARVR_VERSION_LABEL STREQUAL "")
  set(FEARVR_VERSION_FULL "${PROJECT_VERSION}")
else()
  set(FEARVR_VERSION_FULL "${PROJECT_VERSION}-${FEARVR_VERSION_LABEL}")
endif()

# Git-Kurz-Hash (falls verfügbar) für Log-/Diagnosezwecke.
find_package(Git QUIET)
set(FEARVR_GIT_HASH "unknown")
if(GIT_FOUND)
  execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE _fearvr_git_hash
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
  if(_fearvr_git_hash)
    set(FEARVR_GIT_HASH "${_fearvr_git_hash}")
  endif()
endif()

set(FEARVR_GENERATED_DIR ${CMAKE_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${FEARVR_GENERATED_DIR})
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/fearvr-version.h.in
  ${FEARVR_GENERATED_DIR}/fearvr-version.h
  @ONLY)

# Header-only INTERFACE-Ziel für den generierten Versionsheader.
add_library(fearvr-version INTERFACE)
target_include_directories(fearvr-version INTERFACE ${FEARVR_GENERATED_DIR})

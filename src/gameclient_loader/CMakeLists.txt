# ABI-neutraler Loader für die offizielle archcfg-Modulschicht.
add_library(fearvr-gameclient-loader SHARED
  GameClient.def
  dllmain.cpp
  stereo_hook.cpp
  stereo_hook.h
)
set(FEARVR_PUBLIC_TOOLS_ROOT
  "${CMAKE_SOURCE_DIR}/vendor-local/publictools"
  CACHE PATH "Local F.E.A.R. Public Tools tree")
set(FEARVR_PUBLIC_TOOLS_SDK
  "${FEARVR_PUBLIC_TOOLS_ROOT}/Source/engine/sdk/inc")
if(NOT EXISTS "${FEARVR_PUBLIC_TOOLS_SDK}/iltclient.h")
  message(FATAL_ERROR
    "F.E.A.R. Public Tools headers missing at "
    "${FEARVR_PUBLIC_TOOLS_SDK}.")
endif()
target_compile_definitions(fearvr-gameclient-loader PRIVATE
  DISABLE_ASSERTS
  WIN32_LEAN_AND_MEAN
  NOMINMAX
)
# Die VC7.1-SDK-Header verlassen sich auf das damalige, einphasige
# Template-Lookup-Verhalten von MSVC.
target_compile_options(fearvr-gameclient-loader PRIVATE /Zc:twoPhase-)
target_include_directories(fearvr-gameclient-loader SYSTEM PRIVATE
  "${FEARVR_PUBLIC_TOOLS_SDK}"
)
target_link_libraries(fearvr-gameclient-loader PRIVATE
  fearvr-common
  fearvr-warnings
  minhook
)
add_dependencies(fearvr-gameclient-loader fearvr-proxy)
set_target_properties(fearvr-gameclient-loader PROPERTIES
  OUTPUT_NAME "GameClient"
  PREFIX ""
)

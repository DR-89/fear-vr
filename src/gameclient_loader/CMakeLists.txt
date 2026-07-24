# ABI-neutraler Loader für die offizielle archcfg-Modulschicht.
add_library(fearvr-gameclient-loader SHARED
  GameClient.def
  dllmain.cpp
)
target_compile_definitions(fearvr-gameclient-loader PRIVATE
  WIN32_LEAN_AND_MEAN
  NOMINMAX
)
target_link_libraries(fearvr-gameclient-loader PRIVATE
  fearvr-warnings
)
add_dependencies(fearvr-gameclient-loader fearvr-proxy)
set_target_properties(fearvr-gameclient-loader PROPERTIES
  OUTPUT_NAME "GameClient"
  PREFIX ""
)

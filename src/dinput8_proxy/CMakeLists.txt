add_library(fearvr-dinput8 SHARED
  dinput8.def
  dinput8_proxy.cpp
  ../proxy32/iat_hook.cpp
  ../proxy32/iat_hook.h
)
target_compile_definitions(fearvr-dinput8 PRIVATE
  DIRECTINPUT_VERSION=0x0800
  WIN32_LEAN_AND_MEAN
  NOMINMAX
)
target_link_libraries(fearvr-dinput8 PRIVATE
  fearvr-common
  fearvr-hid-fix
  fearvr-warnings
  shell32
)
set_target_properties(fearvr-dinput8 PROPERTIES
  OUTPUT_NAME "dinput8"
  PREFIX ""
)

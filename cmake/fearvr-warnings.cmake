# Hohe Warnstufe; neue Projektquellen sollen warning-clean sein (§11).
# Als INTERFACE-Target, das eigene Ziele per target_link_libraries erben.
add_library(fearvr-warnings INTERFACE)

target_compile_options(fearvr-warnings INTERFACE
  /W4        # hohe Warnstufe
  /permissive- # striktere Standardkonformität
  /utf-8
)

# Optional: neue Ziele können Warnings als Fehler behandeln.
option(FEARVR_WARNINGS_AS_ERRORS "Treat warnings as errors for new targets" OFF)
if(FEARVR_WARNINGS_AS_ERRORS)
  target_compile_options(fearvr-warnings INTERFACE /WX)
endif()

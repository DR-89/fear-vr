# Launcher — ANWEISUNG.md §12
# Startet zuerst fearvr-host.exe, wartet mit Timeout auf "XR ready", startet
# dann die isolierte FEAR.exe mit getrenntem -userdirectory. M0: Gerüst.
add_executable(fearvr-launcher
  main.cpp
)
target_link_libraries(fearvr-launcher PRIVATE
  fearvr-common
  fearvr-version
  fearvr-warnings
)
set_target_properties(fearvr-launcher PROPERTIES OUTPUT_NAME "fearvr-launcher")

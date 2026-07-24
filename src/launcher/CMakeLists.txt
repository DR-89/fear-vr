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

# PowerShell x64 kann die Modulliste eines alten x86-Prozesses je nach
# Windows-Build mit ERROR_PARTIAL_COPY nicht zuverlässig lesen. Dieser kleine
# gleichbittige Probe wird vom sicheren M2-Startskript verwendet.
add_executable(fearvr-module-probe module_probe.cpp)
target_link_libraries(fearvr-module-probe PRIVATE fearvr-warnings)

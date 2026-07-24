# x64 OpenXR-Host (fearvr-host.exe) — ANWEISUNG.md §5.1
# M0: nur Gerüst. Der OpenXR-/D3D11-Lebenszyklus folgt in M1 (hello_xr-Vorbild).
add_executable(fearvr-host
  main.cpp
)
target_link_libraries(fearvr-host PRIVATE
  fearvr-common
  fearvr-version
  fearvr-warnings
)
set_target_properties(fearvr-host PROPERTIES OUTPUT_NAME "fearvr-host")

# In M1 hinzuzufügen (mit festem Pin, siehe THIRD_PARTY_NOTICES.md):
#   - OpenXR-Loader (XR_KHR_D3D11_enable)
#   - d3d11, dxgi, d3dcompiler

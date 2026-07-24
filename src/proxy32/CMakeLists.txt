# x86 d3d9.dll Proxy/Bridge — ANWEISUNG.md §5.2
#
# Erzeugt eine DLL mit dem Ausgabenamen "d3d9.dll", die NUR in der isolierten
# Stage neben einer kopierten FEAR.exe liegt — NIEMALS in der Retail-Installation.
# M0: Gerüst. Weiterleitung aller D3D9-Exports + Reset/Present-Hooks folgen in M2.
add_library(fearvr-proxy SHARED
  dllmain.cpp
)
target_link_libraries(fearvr-proxy PRIVATE
  fearvr-common
  fearvr-version
  fearvr-warnings
)

# Ausgabename exakt d3d9.dll (der Loader von FEAR.exe sucht nach diesem Namen).
set_target_properties(fearvr-proxy PROPERTIES
  OUTPUT_NAME "d3d9"
  PREFIX ""
)

# In M2 hinzuzufügen:
#   - Vollständige Export-Weiterleitung an %SystemRoot%\SysWOW64\d3d9.dll
#     (absolut aufgelöster Systempfad), z. B. via .def-Datei mit Forwardern.
#   - Hooks: Direct3DCreate9, IDirect3D9::CreateDevice,
#            IDirect3DDevice9::Reset, Present.
#   - MinHook nur, falls COM-Wrapper nicht ausreicht (fester Pin).

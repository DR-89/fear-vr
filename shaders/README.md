# shaders/

Fullscreen-/Composite-Shader des x64-Hosts (ANWEISUNG.md §5.1).

Ab **M1**: kleiner Fullscreen-Shader, der die zwei geöffneten D3D9-Shared-
Texturen in die OpenXR-Swapchainbilder zeichnet. Behandelt Formatkonvertierung,
Gamma, Skalierung und nötigenfalls Orientierung.

Noch leer (M0): Der Host-Shader liegt als Rohstring in
`src/host64/texture_renderer.cpp`.

Ebenso die drei D3D9-Shader des Stereo-HUD-Kompositors (Maske, Reduktion,
Komposition) in `src/proxy32/bridge.cpp`. Sie stehen bewusst neben dem Code,
der ihre Konstanten setzt — dieselbe Formel wie `src/common/stereo_hud_math.h`,
und die beiden dürfen nicht auseinanderlaufen.

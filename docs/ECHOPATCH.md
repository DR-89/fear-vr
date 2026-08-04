# EchoPatch-Abgrenzung und übernommener HID-Fix

Der vollständige [EchoPatch](https://github.com/Wemino/EchoPatch) wird nicht
mit F.E.A.R. VR installiert. Frühere Tests zeigten zwei strukturelle Konflikte:

- Sein Vectored Exception Handler beendet Ausnahmen, die unser Loader beim
  vorsichtigen Sondieren von Retail-Interna per `__try/__except` abfängt.
- Mehrere Signaturen erwarten das originale Spielmodul unter
  `GameClient.dll`. Bei F.E.A.R. VR heißt dieses Modul `GameOrig.dll`, weil
  `GameClient.dll` unser ABI-neutraler Loader ist.

## Warum trotzdem ein eigener `dinput8.dll`-Proxy existiert

Das von Spielern beschriebene Muster – zunächst normale Leistung, nach etwa
5–15 Minuten dauerhaft niedrige Bildrate bis zum Neustart – ist ein bekannter
Fehler des Basisspiels. EchoPatch beschreibt als Ursache, dass F.E.A.R.
allgemeine HID-Geräte fälschlich als Controller initialisiert.

F.E.A.R. VR implementiert ausschließlich diese kleine, frühe Korrektur selbst:

1. Der eigene x86-Proxy wird als `dinput8.dll` direkt neben `FEAR.exe` geladen.
2. Er prüft PE-Zeitstempel, Architektur, Imagegröße und jeden Originalbyte der
   beiden bekannten F.E.A.R.-1.08-Codeblöcke.
3. Nur bei vollständiger Übereinstimmung werden der redundante
   HID-Initialisierungsblock und der alte Windows-Input-Hook im
   **Prozessspeicher** übersprungen.
4. Danach werden `DirectInput8Create` und die übrigen Standardexporte an die
   echte Windows-`dinput8.dll` weitergereicht.
5. `FEAR.exe` auf dem Datenträger bleibt bytegleich.

Bei einer unbekannten EXE oder abweichenden Signatur bleibt der Patch aus und
das Spiel startet unverändert weiter. Das Laufprotokoll enthält dafür das
Ereignis `fear_hid_fix` mit einem der Ergebnisse `applied`,
`already_applied`, `not_fear_108`, `image_too_small`,
`signature_mismatch` oder `protection_failed`.

## Was ausdrücklich nicht übernommen wurde

HUD-Skalierung, FOV, SSAA, Controller-/Gyro-Unterstützung, Crash-Handler,
High-FPS-Spielphysikpatches und alle GameClient-/GameServer-Hooks von EchoPatch
sind nicht Teil dieses Ports. Dadurch gibt es keine zweite Hook-Suite und
keinen zusätzlichen Eingabeschreiber neben OpenXR.

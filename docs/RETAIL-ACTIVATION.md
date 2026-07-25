# Retail-Aktivierung und Objekterkennung — Binärbefunde

Grundlage für die strahlbasierte VR-Interaktion (Schalter/Taster aktivieren,
Items aufnehmen). Alle Adressen sind RVAs in der Retail-Client-DLL
`GameOrig.dll` (F.E.A.R. 1.08, `ImageBase 0x10000000`,
`SizeOfImage 0x00315000` — derselbe Abbildcheck wie in `stereo_hook.cpp`).

Ermittelt durch statische Analyse der Retail-DLL, abgeglichen mit den
Public-Tools-Quellen `Source/Game/ClientShellDLL/TargetMgr.cpp`,
`PlayerMgr.cpp` und `objectdetector.cpp`.

## Warum das der richtige Angriffspunkt ist

Beide Retail-Mechanismen hängen an der **Kamera**, nicht am Spielerkörper:

- **Schalter/Taster:** `CTargetMgr::CheckForIntersect` schießt den
  Aktivierungsstrahl von Kameraposition und -rotation aus. Reichweitengrenze
  ist die Konsolenvariable `ActivationDistance`.
- **Items:** `CPlayerMgr` unterhält einen `ObjectDetector` als FOV-Kegel
  (30°/120°, Reichweite `PickupDistance`) um das **Kameraobjekt**.
  `COMMAND_ID_ACTIVATE` schickt für das erkannte Objekt
  `MID_PICKUPITEM_ACTIVATE` an den Server. Aufsammeln per Aktivierung ist im
  Retail-Client also bereits vorgesehen und braucht keine Serveränderung —
  der Detektor muss nur der Hand statt dem Kopf folgen.

## Adressen

| Symbol | RVA | Aufruf |
|---|---|---|
| `CTargetMgr::Update` | `0x001CC980` | thiscall, keine Argumente |
| `CTargetMgr::CheckForIntersect` | `0x001CC150` | thiscall, `float&` |
| `CPlayerMgr`-Objekterkennungsupdate | `0x00152EF0` | thiscall, keine Argumente |
| `ObjectDetector::Update` | `0x001205A0` | thiscall, `float` |
| `ObjectDetector::SetTransform` | `0x0011F140` | thiscall, `HOBJECT` |
| `ObjectDetector::GetObject` | `0x0011FB50` | thiscall, keine Argumente |
| `ObjectDetector::SetParamsFOV` | `0x0011F5F0` | thiscall, 7 floats |
| `ObjectDetector::SetBehaviorFlags` | `0x0011F180` | thiscall, `uint32` |

Alle Aufrufstellen laufen über die Inkrementell-Link-Thunks
(`0xA8F8 → 0x1CC150`, `0xD954 → 0x1205A0`, `0x2FAE → 0x11F140`,
`0x1099C → 0x11FB50`, `0xE61F → 0x11F5F0`, `0x11BB7 → 0x11F180`).
Gegenprobe: Thunk `0xA902` zeigt auf `0x00066F90`, die bereits im Projekt
verifizierte `SetWeaponTransform` — die Thunk-Tabelle ist also dieselbe
Binärversion.

## Globale Daten und Layouts

| Name | RVA | Bedeutung |
|---|---|---|
| `g_pPlayerMgr` | `0x002E2C3C` | Zeiger auf `CPlayerMgr` |
| `g_vtActivationDistance` | `0x002E2FAC` | VarTrack, 12 Byte |
| `g_vtTargetDistance` | `0x002E2FB8` | VarTrack |
| `g_vtPickupDistance` | `0x002E2FC4` | VarTrack |

`CPlayerMgr`:

- `+0x28` — `CPlayerCamera*`
- `+0x3CC` — `m_PickupObjectDetector`

`CPlayerCamera`:

- `+0x0C` — Kamera-`HOBJECT` (das Objekt, das der Detektor verfolgt und das
  der Taschenlampenpfad in `stereo_hook.cpp` bereits umsetzt)
- `+0x10` — `LTVector` Kameraposition (drei Floats)
- `+0x1C` — `LTRotation`, zweiter Faktor
- `+0xB8` — `LTRotation`, erster Faktor

Die Blickrotation entsteht in `CheckForIntersect` als Produkt
`(*(cam+0xB8)) * (*(cam+0x1C))`; Retail 1.08 rechnet sie dort aus zwei
Teilrotationen aus, statt einen fertigen Member zu liefern.

`CTargetMgr` (`m_ActivationData` ab `+0xB4`):

- `+0x10` — `m_hTarget`
- `+0xA4` — `m_fTargetRange`
- `+0xB4` — `m_ActivationData.m_vPos`
- `+0xC0` — `m_ActivationData.m_rRot`

## Umsetzung (in `stereo_hook.cpp`)

`CheckForIntersect` liest die Kamera **ausschließlich** in den ersten
Instruktionen (`0x1CC1E3` und `0x1CC20D`) und arbeitet danach nur noch mit
Kopien. Ein Wrapper kann die drei Kameramember also gefahrlos für die Dauer
des Originalaufrufs auf die Waffen-/Handpose setzen und danach zurückschreiben
— dasselbe Muster wie beim Taschenlampenpfad, nur auf Membern statt auf der
Objekttransformation.

Für Items genügt ein Wrapper um `ObjectDetector::Update`, der nur auf den
Pickup-Detektor des Spielers wirkt (`this == *g_pPlayerMgr + 0x3CC`) und
währenddessen die Transformation des Kameraobjekts auf die Handpose setzt.

Die Reichweiten (`ActivationDistance`, `PickupDistance`) bleiben
Konsolenvariablen und werden auf eine realistische Armlänge angehoben, statt
sie im Code zu verdrahten.

Umgesetzt als:

- `HookRetailCheckForIntersect` — setzt Position und beide Teilrotationen der
  Kamera für die Dauer des Originalaufrufs auf die Mündungstransformation und
  schreibt sie danach unverändert zurück. Der zweite Rotationsfaktor wird zur
  Identität, damit das Produkt genau die Waffendrehung ergibt.
- `HookRetailObjectDetectorUpdate` — wirkt ausschließlich auf den
  Pickup-Detektor des Spielers und gibt dem Kameraobjekt währenddessen die
  Mündungstransformation.
- `UpdateInteractionReachOverride` — hebt beide Reichweiten auf 60 Einheiten
  (rund 1,5 m) an, sobald Stereo aktiv ist, und stellt die Ausgangswerte
  wieder her. Ein größerer Ausgangswert wird nie gesenkt.
- `ResolveRetailInteractionTargets` — prüft Zeitstempel und Abbildgröße der
  Retail-DLL sowie die Anfangsbytes beider Funktionen. Passt etwas nicht,
  bleibt die Strahlinteraktion aus und das Spiel läuft unverändert weiter.

Der Strahl ist bewusst dieselbe Mündungstransformation, aus der auch der
sichtbare rote Zielstrahl und die Fire-Vectors entstehen — was anvisiert wird,
wird auch aktiviert. Beides zusammen ist über `-fearvr-no-interaction`
(und über `-fearvr-safe`) abschaltbar.

Beide Overrides greifen nur bei aktivem Stereo und außerhalb von
Zwischensequenzen und Komfortpanel — dieselbe Grenze, an der auch der
Taschenlampenpfad zurückweicht, weil das Entführen der Kamera in geskripteten
Szenen bereits einmal zu Abstürzen geführt hat.

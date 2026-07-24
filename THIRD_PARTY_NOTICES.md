# THIRD_PARTY_NOTICES.md

Dieses Projekt bindet Fremdkomponenten ein. Jede Abhängigkeit wird auf einen
festen **Commit oder Tag** festgeschrieben (ANWEISUNG.md §4, §11) und hier
dokumentiert. Es werden **keine** Retail-Dateien, proprietäre SDK-Quellen oder
extrahierten Assets mitgeliefert.

> Status: Gerüst. Pins werden eingetragen, sobald die jeweilige Abhängigkeit
> tatsächlich integriert wird (M1 für OpenXR, M2 für MinHook usw.).

## Geplante / eingebundene Abhängigkeiten

| Komponente | Verwendung | Bezug | Lizenz | Pin (Commit/Tag) |
|---|---|---|---|---|
| Khronos OpenXR-SDK | x64-Host: Header/Loader | https://github.com/KhronosGroup/OpenXR-SDK | Apache-2.0 | _TBD_ |
| Khronos OpenXR-SDK-Source (`hello_xr` als Referenz) | Host-Lebenszyklus-Vorlage | https://github.com/KhronosGroup/OpenXR-SDK-Source | Apache-2.0 | _TBD_ |
| MinHook | gezielte x86-Hooks | https://github.com/TsudaKageyu/minhook | BSD-2-Clause | _TBD_ |
| DirectXMath | Mathe (Posen, Projektion) | Teil des Windows 10/11 SDK | MIT | via Windows SDK |

## Offizielle F.E.A.R.-Bestandteile (NICHT in diesem Repo)

Die folgenden Bestandteile werden **lokal** vom Benutzer bereitgestellt und
liegen **nicht** im Repository:

- **F.E.A.R. 1.08 Retail** (`FEAR.exe`, Archive, DLLs) — proprietär,
  Monolith Productions / WB Games. Nur lokal, legal erworben.
- **F.E.A.R. Public Tools 1.08** (`fear_publictools_108.exe`) — offizielle
  SDK-/Client-Quellen. Eigene Lizenzbedingungen des Herstellers; werden nach
  `vendor-local/` installiert und nicht committet.

## Sekundäre Referenz (nur zur Orientierung, nicht übernommen)

- **FEAR-MORE** — https://github.com/SendoTarget/FEAR-MORE — demonstriert einen
  VS-2022/v141-x86-Build der offiziellen F.E.A.R.-1.08-Clientmodule sowie eine
  Retail-schonende Stagingstrategie. **Komponentenspezifische Lizenzgrenzen
  (MIT / GPL / proprietär)** beachten. Es wird **weder Code noch Binärdatei**
  ohne konkrete Lizenzprüfung übernommen.

## Regeln (ANWEISUNG.md §4, §10, §11)

- Jede Abhängigkeit auf Tag/Commit festschreiben, `FetchContent` nur mit
  festem Pin, kein Download zur Laufzeit des Spiels.
- Patches gegen das offizielle SDK minimal halten; vor jeder Veröffentlichung
  prüfen, ob die SDK-Lizenz das Verteilen des konkreten Diffs erlaubt.
  Sichere Alternative: lokales Transformationsskript auf benutzerbereitgestellte
  SDK-Quelle (`patches/`, `game-source-overlay/`).

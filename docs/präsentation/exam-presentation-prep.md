# Prüfungspräsentation am 23.06. – Vorbereitung (AALeC Quiz)

Diese Datei sammelt (a) was der Dozent für die Präsentation verlangt, (b) einen
Vorschlag für den Aufbau/die Story der Präsentation, und (c) die dafür
recherchierten Fakten aus diesem Projekt. **Noch keine fertigen Folien** –
das hier ist die Stoffsammlung, aus der die Folien später gebaut werden.

---

## 1. Rahmenbedingungen (aus der E-Mail des Dozenten)

| Punkt | Details |
|---|---|
| Termin | 23. Juni, Beginn 11:30 Uhr |
| Format | Projekt-Präsentation in Gruppen |
| Dauer | ca. 15 Minuten pro Gruppe |
| Zweck | Erklärt was umgesetzt wurde **und** dient gleichzeitig als Projekt-Dokumentation |

### Pflicht-Inhalte der Präsentation
1. **Use Case & Projektidee** – welches Problem wird gelöst, welche Anwendung wurde umgesetzt?
2. **Hardware** – verwendete Komponenten, Sensoren/Aktoren, gewählter Chip/Controller, Begründung der Wahl
3. **Verkabelung & Schnittstellen** – Beschreibung, idealerweise mit Diagramm, verwendete Schnittstellen erklärt
4. **Software-Stack & Code** – Struktur der Software, zentrale Code-Teile, verwendete Bibliotheken, wichtige Funktionen
5. **Netzwerkanbindung & Backend** – z.B. WiFi, HTTP, MQTT, API, lokales Backend, externe Services
6. **Setup & Konfiguration** – wie wird installiert, konfiguriert, gestartet?
7. **Probleme & Workarounds** – welche Schwierigkeiten kamen auf, wie gelöst?

### Abgabe (direkt nach der Präsentation)
- [ ] Folien, bevorzugt als PDF
- [ ] Source Code, bevorzugt als Git-Repository
- [ ] Kurzes README / Setup-Guide
- [ ] Fotos der Verkabelung
- [ ] Fotos vom Prototyp und der Anwendung in Aktion
- [ ] Nötige Konfigurationshinweise — **keine** privaten Credentials/Passwörter

> Anforderung des Dozenten: Mit Folien + Code + Setup-Anleitung muss jemand
> Fremdes das Projekt verstehen **und wieder in Betrieb nehmen** können.

### Hardware-Vorführung
- Hardware muss **funktionsfähig live vorgeführt** werden.
- Standard-/Laborhardware (unser Fall: AALeC-Controller, Wemos D1 Mini-Basis)
  muss nach der Präsentation **abgegeben** werden.
- Eigene PCBs/Custom-Builds müssten das nicht, müssen aber klar gezeigt/dokumentiert sein.
  *(Bei uns: kein Custom-PCB, AALeC-V3-Plattform ist Laborhardware → wird wohl abgegeben.)*

### Netzwerk-Angebot des Dozenten
- Er bringt einen **Router** zu den nächsten Lab-Sessions mit, an dem Projekte
  mit Netzwerkbedarf getestet werden können.
- Wenn passend, soll die Anwendung so vorbereitet werden, dass sie **von
  anderen Geräten im selben lokalen Netz** erreichbar/testbar ist (Dashboards,
  Webseiten, Steuer-Interfaces).
- **Das passt sehr gut zu uns**: Beamer-Frontend + Admin-Panel sind genau
  solche Web-Dashboards. Siehe Abschnitt 5 "Offene Punkte" — Achtung,
  `localhost`-Konfiguration muss vor der Demo auf die echte LAN-IP des
  Routers/Demo-Rechners umgestellt werden, sonst funktioniert es von anderen
  Geräten aus nicht.

---

## 2. Vorschlag: roter Faden (Lehrer-Stil — für Laien verständlich)

Ziel: Jemand ohne Vorwissen soll am Ende verstehen, **was** das Projekt ist
und **wie** es grob funktioniert — nicht jedes Detail, aber das Gesamtbild.

Empfohlene Erzähl-Logik (vom Vertrauten zum Technischen):

1. **Einstieg mit Vergleich:** "Kennt ihr Kahoot? Genau sowas, nur dass die
   Spieler keine Handy-App nutzen, sondern selbstgebaute Hardware-Buzzer mit
   Drehregler, kleinem Display und LEDs."
2. **Live-Demo-Teaser** (kurz, 1-2 Fragen durchspielen) — schafft sofort ein
   konkretes Bild, bevor es technisch wird.
3. **Drei Bausteine erklären** (wie drei Charaktere, die miteinander reden):
   - Die **Geräte** (AALeC-Controller) — was Spieler in der Hand halten
   - Der **Spielleiter** (Backend) — verwaltet Fragen, Punkte, Spielzustand
   - Die **Leinwand** (Beamer-Frontend) — zeigt Fragen & Ergebnisse für alle
   - Verbindendes Element: die **Post** zwischen ihnen (MQTT-Broker) — "ein
     Postamt, das Nachrichten zwischen den dreien zustellt, ohne dass sie
     sich direkt kennen müssen."
4. **Unter die Haube schauen:** Hardware → Verkabelung/Schnittstellen →
   Software-Aufbau → Netzwerk im Detail.
5. **Setup zeigen:** wie man das alles an- und ausschaltet (sehr kurz, eher
   "schaut wie einfach das ist" als Kommandozeile vorlesen).
6. **Ehrliche Geschichte der Probleme:** 2-3 konkrete Stolpersteine + Lösung
   — das macht eine Präsentation glaubwürdig und zeigt eigenständiges Arbeiten.
7. **Abschluss:** kurzer Rückblick "was kann das Ding jetzt", Frage ans Publikum.

---

## 3. Vorschlag: Folienstruktur mit Timing (Ziel ~15 Min)

| # | Folie | Inhalt | Zeit |
|---|---|---|---|
| 1 | Titel + Hook | Projektname, "Kahoot mit eigener Hardware", 1 Satz Use Case | 1 min |
| 2 | Live-Demo (kurz) | 1-2 Fragen live durchspielen, Beamer + 1-2 Geräte | 2 min |
| 3 | Use Case & Idee | Problem, Zielgruppe (z.B. Vorlesungs-Quiz), Spielablauf-Diagramm (State Machine) | 1.5 min |
| 4 | Hardware | AALeC-V3 / Wemos D1 Mini (ESP8266), Komponenten-Tabelle, warum dieser Chip | 2 min |
| 5 | Verkabelung & Schnittstellen | GPIO-Tabelle, Foto/Diagramm, I²C/WLAN erklärt | 1.5 min |
| 6 | Software-Stack | 3 Module (Firmware/Backend/Frontend), Architektur-Diagramm, Kern-Bibliotheken | 2.5 min |
| 7 | Netzwerk & Backend | MQTT-Topics, REST-API, Diagramm Broker↔Geräte↔Backend↔Frontend | 2 min |
| 8 | Setup & Konfiguration | docker-compose / podman-compose, config.h, 1 Befehl zum Starten | 1 min |
| 9 | Probleme & Workarounds | 2-3 Top-Stories (siehe Abschnitt 4.7) | 1.5 min |
| 10 | Abschluss | Zusammenfassung, Stand/Ausblick, Fragen | 1 min |

(Summe ≈ 16 min — ein wenig Puffer einplanen/kürzen je nach Gruppengröße.)

---

## 4. Recherchierte Inhalte je Pflichtpunkt

### 4.1 Use Case & Projektidee

- **Problem:** Klassische Live-Quiz-Tools (Kahoot etc.) brauchen Smartphones/Laptops
  pro Spieler. Hier: eigene, selbstgebaute Hardware-Buzzer statt Handy-Apps —
  praxisnäher für ein Elektronik/Embedded-Fach, jeder Spieler nutzt sein eigenes Gerät.
- **Anwendung:** MQTT-basiertes Multiplayer-Quiz für die AALeC-Hardware-Plattform.
  Ein "Game Master" (Backend) steuert den Ablauf zentral, ein Beamer zeigt
  Fragen/Ergebnisse für die ganze Klasse, jeder Spieler antwortet über sein
  eigenes Gerät.
- **Spielablauf (State Machine):**
  `WAITING → QUESTION → VOTING → REVEAL → SCORES → (nächste Frage oder ENDED)`
- **Fragetypen** (eigene Stärke ggü. simplen MC-Quiz-Tools): klassisches Multiple
  Choice, Schätzfragen, Höher/Niedriger, **Poti-Challenge** (Potentiometer auf
  Zielwert drehen) und **Temperatur-Challenge** (Sensor auf Zieltemperatur
  bringen) — nutzt die physische Hardware aktiv statt nur Knopfdruck.

### 4.2 Hardware

| Komponente | Einsatz im Projekt |
|---|---|
| **Wemos D1 Mini (ESP8266)** | Hauptcontroller, WLAN + Rechenleistung für MQTT/JSON |
| OLED-Display | Zeigt Frage, Antwortoptionen, Status ("Bereit!", Name) |
| Drehgeber + Taster | Antwort auswählen (A–D) und bestätigen |
| WS2812B-LED-Streifen (5×) | Visuelles Feedback: richtig=grün, falsch=rot, Status-Pulsieren |
| Potentiometer | Eingabe für "Poti-Challenge"-Fragen |
| BME280/BME680-Sensor | Auf der Platine vorhanden, **in diesem Quiz-Modus ungenutzt** außer für die Temperatur-Challenge |
| Piezo-Beeper | Sound-Feedback bei Antwort-Abgabe, richtig/falsch, Gewinner-Melodie |

**Warum ESP8266 (Wemos D1 Mini)?**
- Eingebautes WLAN → keine Zusatz-Hardware fürs Netzwerk nötig
- Günstig, breite Library-Unterstützung (Arduino-Framework), genug Leistung
  für JSON-Parsing + MQTT + OLED gleichzeitig
- Plattform war als "AALeC-V3" bereits als Hochschul-Hardware vorgegeben/etabliert

> ⚠️ **Zu prüfen:** Im Code wird der OLED-Treiber `SH1106Wire` verwendet
> (Datei `lib/AALeC-V3/src/AALeC-V3.h`). Die ältere Doku nennt das Display
> "SSD1306" — beides läuft über dieselbe Bibliotheksfamilie
> (`esp8266-oled-ssd1306`), die mehrere Chip-Varianten unterstützt. Für die
> Präsentation: lieber **SH1106** sagen, das ist was im Code tatsächlich
> instanziiert wird.

### 4.3 Verkabelung & Schnittstellen

GPIO-Belegung (AALeC-V3, aus `lib/AALeC-V3/src/AALeC-V3.h`):

| GPIO | Funktion |
|---|---|
| 0 | Taster (Drehgeber-Knopf, Antwort bestätigen) |
| 2 | WS2812B LED-Kette |
| 4 | I²C SDA (OLED) |
| 5 | I²C SCL (OLED) |
| 12 | Drehgeber Kanal A |
| 14 | Drehgeber Kanal B |
| 15 | Piezo-Beeper |
| A0 (analog) | Potentiometer (Schätz-/Poti-Fragen) |

**Schnittstellen:**
- **I²C** fürs OLED-Display (Adresse `0x3c`)
- **GPIO digital** für Taster, Drehgeber, LED-Datenleitung, Beeper
- **Analog (ADC)** für das Potentiometer
- **WLAN (802.11)** für die Netzwerkanbindung — kein Kabel zum PC nötig im Betrieb,
  nur zum Flashen über USB/Seriell

→ Für die Folie: ein **Foto der echten Verkabelung** + die obige Tabelle reicht,
ein vollständiges Schaltbild ist bei einer vorgefertigten Hardware-Plattform
(AALeC-V3) nicht nötig — kurz auf die Pin-Tabelle verweisen.

### 4.4 Software-Stack & Code

Drei unabhängige Teile, die nur über MQTT/HTTP miteinander reden:

**a) Firmware (`src/firmware/Controller`, C++/Arduino, PlatformIO, Board `esp12e`)**
- Bibliotheken: `PubSubClient` (MQTT), `ArduinoJson`, `Adafruit NeoPixel`,
  vendored `AALeC-V3` (Hardware-Abstraktion: Display/LEDs/Encoder/Sensoren),
  `esp8266-oled-ssd1306` (OLED-Treiber, vendored)
- Struktur: `main.cpp` (Setup/Loop/State-Dispatch), `display.cpp` (ein
  `show...()` pro Spielzustand, z.B. `showWaiting()`, `showVoting()`,
  `showReveal()`), `network.cpp` (WLAN/MQTT-Verbindung, Reconnect-Logik),
  `storage.cpp` (Name im EEPROM speichern), `audio.cpp` (Beeper-Melodien)
- Zentrale Funktion: pro Spielzustand eine Anzeige-Funktion, die in einer
  `while`-Schleife läuft solange dieser Zustand aktiv ist und dabei
  `mqtt.loop()` aufruft, um Nachrichten zu verarbeiten

**b) Backend / "Game Master" (`src/backend`, Python 3.11, `uv` für Dependencies)**
- Bibliotheken: `Flask` (REST-API), `paho-mqtt` (MQTT-Client)
- Eine Klasse `GameMaster` verwaltet: Spielerliste, Zustandsautomat,
  Punkteberechnung (Basis-Punkte + Zeitbonus + Streak-Bonus), Frage-Sets
- REST-API (Port 8080) fürs Admin-Panel: Fragensets lesen/schreiben/aktivieren
- Persistiert Spielstand in `game_state.json` (Crash-Recovery)
- 75 automatisierte Tests (`pytest`) für die Punkteberechnung/Validierung

**c) Frontend (`src/frontend`, Next.js 14 + TypeScript + React 18)**
- Bibliothek: `mqtt.js` (MQTT über WebSockets direkt im Browser)
- Zwei Seiten:
  - `/` — **Beamer-Ansicht** für den Projektor (Lobby, Frage, Live-Voting,
    Auflösung, Punktestand)
  - `/admin` — **Admin-Panel**: Fragensets verwalten, Quiz starten/beenden/
    neustarten, Teilnehmer sehen/umbenennen/entfernen
- Reagiert in Echtzeit auf MQTT-Nachrichten, kein eigener State-Server nötig

### 4.5 Netzwerkanbindung & Backend

- **Transportweg:** WLAN (802.11) für alle Geräte und den Beamer-Rechner
- **MQTT-Broker:** Eclipse Mosquitto
  - Port `1883` (TCP) — für die ESP8266-Firmware
  - Port `9001` (WebSockets) — für den Browser (Frontend), da Browser kein
    rohes TCP-MQTT können
- **Topics** (Auszug, vollständig in `docs/asyncapi.yaml` dokumentiert):
  `quiz/state`, `quiz/question`, `quiz/reveal`, `quiz/scores`, `quiz/players`
  (Server → alle), `quiz/answer/<device_id>`, `quiz/connect/<device_id>`
  (Geräte → Server), `quiz/control` (Start/Stop/Restart-Befehle)
- **REST-API** (Flask, Port 8080): separat vom Echtzeit-MQTT-Pfad, nur für
  administrative Aufgaben (Fragensets bearbeiten) — bewusste Trennung:
  Spielzustand = MQTT/Echtzeit, Konfiguration = HTTP/REST
- **Deployment:** alle drei Teile (Broker, Backend, Frontend) laufen als
  Container, orchestriert mit `docker-compose`/`podman-compose`. Es gibt
  fertige Images auf GitHub Container Registry (GHCR) **und** eine
  Dev-Override-Datei zum lokalen Bauen aus dem Source-Code.
- **CI/CD:** GitHub Actions baut bei jedem Push die Firmware (PlatformIO) und
  die beiden Docker-Images; bei einem Versions-Tag (`vX.Y.Z`) werden die
  Images nach GHCR gepusht und automatisch ein GitHub-Release mit der
  fertigen `firmware.bin` erstellt.

### 4.6 Setup & Konfiguration

1. WLAN-Zugangsdaten + Broker-Adresse in `config.h` eintragen (Vorlage:
   `config.h.example`, ist `.gitignore`d — keine Secrets im Repo)
2. `podman-compose up -d` (oder `docker-compose`) startet Broker + Backend +
   Frontend in einem Schritt
3. Firmware separat über PlatformIO/USB flashen (`upload.sh`)
4. Browser auf `http://<rechner>:3000` für den Beamer, `/admin` fürs
   Admin-Panel

### 4.7 Probleme & Workarounds (für die Folie: 2-3 davon auswählen)

1. **ESP8266-Toolchain auf Apple-Silicon-Mac:** PlatformIOs Compiler für den
   ESP8266 (`xtensa-lx106-elf`) gibt es nur als x86_64-Build, und Rosetta 2
   war auf der Build-Maschine nicht verfügbar → kompiliert wird stattdessen in
   einem **arm64-Linux-Container** (lädt dort automatisch den nativen
   `linux_aarch64`-Toolchain), geflasht wird danach direkt vom Host aus mit
   reinem Python-`esptool.py` (architekturunabhängig).
2. **Admin-Panel zunächst unerreichbar:** Die REST-API lief im Backend-Container
   auf Port 8080, der aber in `docker-compose.yml` nicht nach außen freigegeben
   war → Port-Mapping ergänzt.
3. **Neues Fragenset ließ sich nicht anlegen:** Beim Erstellen schickt das
   Frontend zunächst eine leere Fragenliste, das Backend hat *jede* leere
   Liste pauschal abgelehnt (422) → Validierung angepasst: leeres Set als
   Entwurf erlaubt, aber **Aktivieren/Spielen** eines leeren Sets bleibt
   weiterhin blockiert.
4. **Geräte-Umbenennung per Doppelklick am Gerät unzuverlässig:** Die Geste
   (Doppelklick + 1.2s halten) hatte ein zu enges Zeitfenster, wodurch der
   "gehaltene" zweite Klick oft fälschlich wieder als neuer "erster Klick"
   gewertet wurde → nach mehreren Tuning-Versuchen letztlich entschieden, das
   lokale Umbenennen ganz zu entfernen und stattdessen zuverlässig über das
   Admin-Panel zu lösen (einfacher, weniger Fehlerquellen am Gerät selbst).

---

## 5. Offene Punkte — vor der Präsentation klären

- [ ] **Netzwerk-Setup für die Demo klären:** `docs/howto.md` beschreibt einen
  separaten "Arduino Uno (WiFi-Hotspot)" als Access Point — dieser Hotspot-Teil
  wurde aber aus dem Repo entfernt (`refactor: remove unused Hotspot firmware
  folder`). Aktuell verbindet sich der Controller als normaler WLAN-Client zu
  einer existierenden SSID `AALeC-Quiz`. Für die Prüfung: voraussichtlich der
  **Router des Dozenten** übernimmt diese Rolle — vorher testen!

Antwort: Hier wäre es cool, das man erwähnt, dass man in dem gleichen lokalen Netzwerk sein muss! Das mit dem Arudino ist nur für die Präsentatio und den Tests gedacht!


- [ ] **`NEXT_PUBLIC_MQTT_URL` / API-URL für das Demo-Netzwerk anpassen:**
  Diese Werte werden beim Frontend-Build fest einkompiliert und stehen aktuell
  auf `localhost`. Wenn andere Geräte im Dozenten-Netzwerk das Dashboard
  ansehen sollen (wie in der E-Mail angeboten), muss vor der Demo mit der
  echten LAN-IP/Hostnamen des Vorführ-Rechners neu gebaut werden — sonst
  funktioniert die Seite nur auf dem Vorführ-Rechner selbst.

Antwort: Ist beim Backend halt so, dass es nur LOCALHOST ist!

- [ ] **Test-Fragenset "Joa" (0 Fragen) im Admin-Panel aufräumen** — Restartefakt
  aus dem Testen, vor der Demo löschen oder mit echten Fragen befüllen.
- [ ] **Höher/Niedriger-Fragen:** alle drei haben aktuell "HIGHER" als Lösung
  (inhaltlich korrekt, aber genauso erratbar wie das B-Problem) — ggf. noch
  ausgleichen, optional.
- [ ] Welche Fragensets/Inhalte werden in der Live-Demo tatsächlich gezeigt?
  Eigene kurze Vorlesungs-Quiz-Fragen vorbereiten, die zum Vortrag passen.
- [ ] Fotos für die Abgabe machen: Verkabelung, Prototyp, App in Aktion (siehe
  Checkliste Abschnitt 1).

- [ ] Klären, ob die AALeC-Hardware nach der Prüfung abgegeben werden muss
  (laut Ansage: ja, da Laborhardware). Antwort: Muss abgegeben werden!

---

## 6. Was noch fehlt, bevor die Folien gebaut werden können

- Diagramme: Architektur-Übersicht (Geräte ↔ Broker ↔ Backend ↔ Frontend),
  evtl. ein einfaches Verkabelungsfoto mit Pin-Beschriftung
- Screenshots: Beamer-Lobby, Frage-Screen, Admin-Panel
- Finale Auswahl der 2-3 "Probleme & Workarounds"-Stories für die Folie
- Entscheidung: wer präsentiert welchen Teil (falls Gruppenarbeit)

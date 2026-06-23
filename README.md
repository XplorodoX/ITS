# AALeC Quiz — ITS

Lokales Echtzeit-Multiplayer-Quizspiel für Vorlesungen. Physische ESP8266-Controller (AALeC-V3) dienen als Eingabegeräte — Antworten per Drehgeber, Rückmeldung via OLED-Display und WS2812B-LEDs.

## Systemübersicht

| Komponente | Technologie | Port | Funktion |
|---|---|---|---|
| Firmware | C++ / PlatformIO | — | Controller-Logik, MQTT-Client |
| Broker | Eclipse Mosquitto | 1883 / WS 9001 | Nachrichtenverteilung |
| Backend | Python / Flask | 8080 | Spielsteuerung, REST-API |
| Frontend | Next.js / React | 3000 | Beamer-Ansicht, Admin-Panel |

**Kommunikationsfluss:** Controller → MQTT (1883) → Broker → Backend (Game Master) → Broker → Frontend (WebSocket 9001)

---

## Voraussetzungen

| Tool | Zweck | Download |
|---|---|---|
| Python 3.10+ | flash.py, Backend | python.org |
| Podman oder Docker | Container | podman.io / docker.com |
| PlatformIO | Firmware kompilieren (optional) | platformio.org |

---

## Schnellstart

### 1. WLAN-Netzwerk bereitstellen

Die Controller verbinden sich mit dem WLAN **`AALeC-Quiz`** (Passwort: `12345678`). Dieses Netz muss existieren — entweder über den AALeC-WLAN-Router oder einen eigenen Hotspot mit diesen Zugangsdaten.

Der Mac/PC, auf dem die Container laufen, muss im **selben Netzwerk** erreichbar sein.

### 2. Firmware konfigurieren

`config.h` ist nicht im Repository enthalten und muss manuell angelegt werden:

```bash
cp src/firmware/Controller/src/config.h.example src/firmware/Controller/src/config.h
```

Hostnamen des Broker-Rechners eintragen (mDNS, kein feste IP nötig):

```bash
# Eigenen Hostnamen ermitteln (macOS)
scutil --get LocalHostName
# → z. B. MacBook-Pro-von-Florian
```

```cpp
// src/firmware/Controller/src/config.h
#define WIFI_SSID     "AALeC-Quiz"
#define WIFI_PASSWORD "12345678"
#define MQTT_BROKER   "MacBook-Pro-von-Florian.local"   // eigenen Hostnamen eintragen
#define MQTT_PORT     1883
```

### 3. Container starten

```bash
podman-compose up -d
# oder: docker-compose up -d
```

Erreichbar nach dem Start:

| URL | Funktion |
|---|---|
| `http://localhost:3000` | Beamer-Ansicht (Lobby, Fragen, Scores) |
| `http://localhost:3000/admin` | Admin-Panel (Quiz steuern) |
| `http://localhost:8080` | Backend REST-API |

### 4. Firmware flashen

Controller per USB anschließen, dann:

```bash
python3 flash.py
```

Das Skript lädt automatisch die neueste `firmware.bin` aus dem GitHub-Release, erkennt den seriellen Port und flasht den Controller. Kein PlatformIO erforderlich.

---

## Quiz starten

Sobald Controller in der Lobby (`http://localhost:3000`) erscheinen:

- **Frontend:** Grünen **"Quiz starten"**-Button klicken
- **Terminal:**
  ```bash
  mosquitto_pub -h 127.0.0.1 -p 1883 -t "quiz/control" -m '{"action":"start"}'
  ```

Spiel zurücksetzen:
```bash
mosquitto_pub -h 127.0.0.1 -p 1883 -t "quiz/control" -m '{"action":"restart"}'
```

---

## Fragen anpassen

Quizfragen befinden sich in `src/backend/questions.json`. Unterstützte Fragetypen:

| Typ | Beschreibung |
|---|---|
| *(Standard)* | Multiple Choice (A–D) |
| `higher_lower` | Ist Wert X größer oder kleiner als Y? |
| `estimate` | Zahlenschätzer mit Slider |
| `poti_target` | Hardware-Challenge: Poti auf Zielwert einstellen |
| `temp_target` | Hardware-Challenge: Sensor auf Zieltemperatur bringen |

Änderungen an `questions.json` erfordern einen Neustart des `game-master`-Containers:
```bash
podman-compose restart game-master
```

---

## REST API

Der Game Master stellt eine REST-API auf Port **8080** bereit. Alle Endpunkte geben JSON zurück und akzeptieren CORS von beliebigen Origins.

### `GET /api/question-sets`

Gibt alle verfügbaren Fragensets im Backend-Verzeichnis zurück.

```json
[
  { "name": "questions", "count": 15, "active": true },
  { "name": "advanced",  "count":  8, "active": false }
]
```

### `GET /api/question-sets/<name>`

Liest ein einzelnes Fragenset als JSON-Array.

### `PUT /api/question-sets/<name>`

Erstellt oder überschreibt ein Fragenset. Body muss ein JSON-Array von Fragen sein.  
Validierungsfehler werden mit `422` und `{ "errors": [...] }` zurückgegeben.  
Name darf nur `[A-Za-z0-9_-]` enthalten.

```bash
curl -X PUT http://localhost:8080/api/question-sets/meinset \
  -H "Content-Type: application/json" \
  -d '[{"text":"...", "options":{"A":"...","B":"...","C":"...","D":"..."}, "correct":"A", "time_limit_s":20}]'
```

### `DELETE /api/question-sets/<name>`

Löscht ein Fragenset. Schlägt mit `409` fehl wenn das Set gerade aktiv ist.

### `GET /api/active-set`

Gibt das aktuell geladene Fragenset zurück.

```json
{ "active": "questions" }
```

### `POST /api/active-set`

Wechselt das aktive Fragenset (nur im Zustand `WAITING` möglich).

```bash
curl -X POST http://localhost:8080/api/active-set \
  -H "Content-Type: application/json" \
  -d '{"name": "advanced"}'
```

---

## WebSocket / MQTT

Das Frontend verbindet sich direkt per **WebSocket auf Port 9001** mit dem Mosquitto-Broker — kein eigener WebSocket-Server nötig. Mosquitto spricht MQTT nativ über WebSocket.

```
Browser  ──WS:9001──►  Mosquitto  ◄──TCP:1883──  Backend / Controller
```

### Topics (Broker → Frontend)

| Topic | Inhalt |
|---|---|
| `quiz/state` | Spielzustand (`WAITING` / `QUESTION` / `VOTING` / `REVEAL` / `SCORES` / `ENDED`), `question_id`, `remaining_s` |
| `quiz/question` | Aktuelle Frage inkl. Typ, Text, Optionen, Zeitlimit |
| `quiz/reveal` | Richtige Antwort, Antwortverteilung, Einzelergebnisse |
| `quiz/scores` | Aktuelles Leaderboard (`device_id`, `name`, `score`, `streak`) |
| `quiz/answer_count` | Live-Zähler: wie viele haben bereits geantwortet (`count` / `total`) |
| `quiz/players` | Spielerliste mit Online-Status für die Lobby |

### Topics (Frontend → Broker)

| Topic | Payload | Funktion |
|---|---|---|
| `quiz/control` | `{"action": "start"}` | Quiz starten |
| `quiz/control` | `{"action": "restart"}` | Zurück zur Lobby, Punkte reset |
| `quiz/control` | `{"action": "end"}` | Quiz vorzeitig beenden |
| `quiz/control` | `{"action": "reset_names"}` | Namenswahl aller Controller zurücksetzen |
| `quiz/rename/set` | `{"device_id": "...", "name": "..."}` | Gerät umbenennen |
| `quiz/remove/set` | `{"device_id": "..."}` | Gerät aus Spielerliste entfernen |

### Verbindung aus eigenem Code

```js
import mqtt from 'mqtt';
const client = mqtt.connect('ws://localhost:9001');
client.on('connect', () => client.subscribe(['quiz/state', 'quiz/scores']));
client.on('message', (topic, payload) => console.log(topic, JSON.parse(payload)));
```

---

## Lokale Entwicklung

```bash
# Container lokal aus Source bauen
podman-compose -f docker-compose.yml -f docker-compose.dev.yml up --build -d

# Firmware lokal kompilieren und flashen (PlatformIO)
cd src/firmware/Controller
pio run --target upload
```

---

## Bilder

`Bilder/` enthält Fotos und Screenshots des laufenden Systems:

| Datei | Inhalt |
|---|---|
| `AALeC_hardware.jpg` | AALeC-V3 Controller (Hardware) |
| `AdminPanel.png` | Admin-Panel im Browser |
| `WaitingScreen.png` | Lobby-Ansicht auf dem Beamer |

---

## Präsentation

```bash
# Im Browser öffnen (Pfeiltasten / Leertaste blättern, F = Vollbild)
open docs/präsentation/index.html

# Als PDF exportieren (Playwright wird beim ersten Start installiert)
python3 docs/präsentation/export_pdf.py
```

---

## CI/CD

- **Push / PR:** Firmware kompiliert, Docker-Images gebaut
- **Version-Tag `v*`:** Images nach GHCR gepusht, GitHub Release mit `firmware.bin` als Download-Asset

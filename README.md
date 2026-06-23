# AALeC Quiz — ITS

Lokales Echtzeit-Multiplayer-Quizspiel mit physischen ESP8266-Buzzern als Eingabegeräte.

## Systemübersicht

| Komponente | Technologie | Funktion |
|---|---|---|
| Firmware | C++ / PlatformIO | Buzzer-Logik, MQTT-Client |
| Backend | Python / Flask | Spielsteuerung, REST-API |
| Frontend | Next.js / React | Beamer-Ansicht, Admin-Panel |
| Broker | Eclipse Mosquitto | MQTT auf Port 1883 / WS 9001 |

---

## Schnellstart

### 1. WLAN & mDNS konfigurieren

```bash
# Hostnamen des Macs ermitteln
scutil --get LocalHostName
# → z. B. MacBook-Pro-von-Florian

# config.h anlegen
cp src/firmware/Controller/src/config.h.example src/firmware/Controller/src/config.h
```

In `src/firmware/Controller/src/config.h` anpassen:

```cpp
#define WIFI_SSID     "AALeC-Quiz"
#define WIFI_PASSWORD "12345678"
#define MQTT_BROKER   "MacBook-Pro-von-Florian.local"
```

### 2. Container starten

```bash
podman-compose up -d
# oder: docker-compose up -d
```

Danach erreichbar:
- Frontend / Beamer: `http://localhost:3000`
- Admin-Panel: `http://localhost:3000/admin`

### 3. Firmware flashen

Controller per USB anschließen, dann:

```bash
python3 flash.py
```

Das Skript lädt automatisch die neueste `firmware.bin` von GitHub und erkennt den seriellen Port.

---

## Lokale Entwicklung (aus Source bauen)

```bash
# Container lokal bauen
podman-compose -f docker-compose.yml -f docker-compose.dev.yml up --build -d

# Firmware mit PlatformIO kompilieren
cd src/firmware/Controller
pio run --target upload
```

---

## Spielsteuerung

Sobald die Geräte in der Lobby sichtbar sind:

```bash
# Spiel starten
mosquitto_pub -h 127.0.0.1 -p 1883 -t "quiz/control" -m '{"action":"start"}'

# Zurücksetzen
mosquitto_pub -h 127.0.0.1 -p 1883 -t "quiz/control" -m '{"action":"restart"}'
```

Alternativ direkt im Frontend über den **Quiz starten**-Button.

---

## Präsentation

Datei: `docs/präsentation/index.html` — im Browser öffnen.

Steuerung: `Pfeiltasten` / `Leertaste` blättern, `F` Vollbild.

PDF exportieren:

```bash
python3 docs/präsentation/export_pdf.py
```

*(Playwright + Chromium werden beim ersten Start automatisch installiert.)*

---

## CI/CD

- **Push / PR:** Firmware kompilieren, Docker-Images bauen
- **Version-Tag `v*`:** Images nach GHCR pushen, GitHub Release mit `firmware.bin` erstellen

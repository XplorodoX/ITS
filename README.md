# AALeC Quiz — Intelligent Transport Systems (ITS)

Dieses Repository enthält das **AALeC Quiz**, ein lokales Echtzeit-Multiplayer-Quizspiel (ähnlich wie Kahoot), das physische Hardware-Buzzer (ESP8266-Controller) als Eingabegeräte nutzt.

---

## 🏗️ Systemarchitektur

Das System teilt sich in drei Hauptkomponenten auf:

1. **Hardware & Firmware (`src/firmware/Controller`)**:
   * ESP8266-basierte AALeC-V3 Controller, die sich mit dem WLAN-Hotspot verbinden und Eingaben (Drehgeber, Potentiometer, Temperatursensor) erfassen.
   * Kommunikation erfolgt in Echtzeit über **MQTT**.
2. **Backend / Game Master (`src/backend`)**:
   * Ein in Python geschriebener Spielleiter (Game Master), der den Zustandsautomaten (`WAITING` ──► `QUESTION` ──► `VOTING` ──► `REVEAL` ──► `SCORES`) verwaltet.
   * Beinhaltet eine REST-API zur Verwaltung von Fragensets.
3. **Frontend / Beamer-Projektor (`src/frontend`)**:
   * Eine Next.js-Webanwendung, die als Lobby und Beamer-Projektionsfläche dient. Sie aktualisiert ihren Zustand vollautomatisch per WebSockets über den MQTT-Broker.

---

## 📁 Repository-Struktur

* [src/backend](file:///Users/merluee/ITS/src/backend) — Python Game Master, REST-API und Fragensets.
* [src/frontend](file:///Users/merluee/ITS/src/frontend) — Next.js Beamer-Projektor-Anwendung.
* [src/firmware/Controller](file:///Users/merluee/ITS/src/firmware/Controller) — PlatformIO C++ Firmware für die AALeC-Clients.
* [.github/workflows](file:///Users/merluee/ITS/.github/workflows) — CI/CD Workflows für automatisierte Builds und Releases.
* [docs](file:///Users/merluee/ITS/docs) — Weiterführende Dokumentationen und Anleitungen.

---

## 📦 Schnellstart mit Pre-built Releases (Ohne Kompilieren)

Du musst die Anwendung und die Firmware nicht selbst kompilieren. Du kannst direkt die vorgefertigten Container-Images und die kompilierte Firmware aus den GitHub Releases nutzen:

### 1. Pre-compiled Controller-Firmware flashen
Das Flashen kann plattformübergreifend (Windows, macOS, Linux) vollautomatisch über das mitgelieferte Python-Skript `flash.py` im Hauptverzeichnis durchgeführt werden:
1. Verbinde den AALeC-Controller per USB mit deinem Computer.
2. Führe das Skript aus:
   * **macOS / Linux**: `python3 flash.py`
   * **Windows**: `python flash.py`
   *(Das Skript lädt automatisch die neueste `firmware.bin` von GitHub herunter, sucht den passenden seriellen USB-Port und flasht die Firmware mit der robusten Standard-Baudrate von 115200).*

### 2. Pre-built Docker/Podman Container starten
Die `docker-compose.yml` im Hauptverzeichnis ist standardmäßig so konfiguriert, dass sie die fertigen Images direkt aus der GitHub Container Registry (GHCR) herunterlädt. Starte alle Services einfach mit:
```bash
podman-compose up -d  # oder docker-compose up -d
```

### 🛠️ Lokale Entwicklung (Kompilieren aus dem Source-Code)
Falls du Änderungen am Code vornimmst und diese lokal bauen und ausführen möchtest:
1. **Container lokal bauen & starten**:
   ```bash
   podman-compose -f docker-compose.yml -f docker-compose.dev.yml up --build -d
   # bzw. docker-compose
   ```
2. **Firmware lokal kompilieren**: Folge den regulären Schritten unter [Setup & Ausführung](#🚀-setup--ausführung).

---

## 🚀 Setup & Ausführung

Folge diesen Schritten, um das Quiz-System von Grund auf lokal aufzusetzen und zu starten:

### Schritt 1: Netzwerk & mDNS konfigurieren (Wichtig!)
Damit du den Code auf den AALeC-Geräten nicht bei jedem Wechsel der IP-Adresse neu flashen musst, wird **mDNS** zur Broker-Suche genutzt.
1. Ermittle den lokalen Hostnamen deines Macs im Terminal:
   ```bash
   scutil --get LocalHostName
   # Beispielausgabe: dein-pc-name.local
   ```
2. Kopiere die Konfigurationsdatei im Firmware-Ordner:
   ```bash
   cp src/firmware/Controller/src/config.h.example src/firmware/Controller/src/config.h
   ```
3. Passe in der neuen `config.h` den Hostnamen an (füge `.local` an deinen Mac-Namen an):
   ```cpp
   #define MQTT_BROKER   "dein-pc-name.local"
   ```

### Schritt 2: Docker-Container (Backend, Frontend & Broker) starten
Starte alle nötigen Services im Hauptverzeichnis mit Podman (oder Docker):
```bash
podman-compose up --build -d
```
Dies startet:
* Den **Mosquitto MQTT-Broker** auf Ports `1883` (TCP) und `9001` (WebSockets).
* Den **Game Master** im Hintergrund.
* Das **Frontend** unter `http://localhost:3000`.

### Schritt 3: Firmware flashen
1. Verbinde das AALeC-Gerät per USB mit deinem Computer.
2. Führe das automatische Flash-Skript aus:
   * **macOS / Linux**: `python3 flash.py`
   * **Windows**: `python flash.py`
   *(Alternativ kannst du in VS Code mit PlatformIO das Projekt `Controller` öffnen und direkt den **Upload**-Befehl ausführen).*

---

## 🎮 Spielsteuerung

Nachdem das System läuft und die Geräte im Beamer-Frontend (`http://localhost:3000`) in der Lobby sichtbar sind, kannst du das Spiel steuern:

* **Über das Frontend:** Klicke einfach auf den grünen **"Quiz starten"**-Button in der Beamer-Lobby.
* **Per MQTT-Befehl (Terminal):**
  ```bash
  # Spiel starten:
  mosquitto_pub -h 127.0.0.1 -p 1883 -t "quiz/control" -m '{"action":"start"}'

  # Spiel zurücksetzen / neu starten:
  mosquitto_pub -h 127.0.0.1 -p 1883 -t "quiz/control" -m '{"action":"restart"}'
  ```

---

## 📊 Projekt-Präsentation

Die Abschlusspräsentation für das Projekt befindet sich direkt im Repository:
* **Pfad:** [docs/präsentation/index.html](file:///Users/merluee/ITS/docs/präsentation/index.html) (lokal im Browser öffnen)
* **Steuerung:** 
  * `Pfeiltasten` oder `Leertaste` zum Blättern
  * `F` zum Umschalten in den Vollbildmodus
* **PDF-Export (Druckversion):** 
  * Navigiere zur letzten Folie („Referenzen & Ressourcen“) und klicke auf den Button **„PDF-Version generieren (Drucken)“** (oder drücke `Cmd + P` / `Strg + P` im Browser).
  * Wähle als Ziel **„Als PDF speichern“**, stelle das Layout auf **Querformat** und aktiviere in den Einstellungen **„Hintergrundgrafiken“**, damit das dunkle Theme korrekt gedruckt wird.

---

## 🛠️ CI/CD Pipeline

Das Repository verfügt über eine automatisierte GitHub Actions Pipeline:
* **Bei jedem Push/PR:** Die Firmware wird kompiliert (Syntaxprüfung) und die Docker-Images werden gebaut.
* **Bei einem Version-Tag (z. B. `v1.0.0`):**
  * Die Docker-Images werden automatisch nach GHCR gepusht (`ghcr.io/xplorodox/its-backend` & `ghcr.io/xplorodox/its-frontend`).
  * Es wird automatisch ein **GitHub Release** erstellt, an dem die fertig kompilierte `firmware.bin` direkt als Download-Asset angehängt wird.

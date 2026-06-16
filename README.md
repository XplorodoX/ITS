# AALeC Quiz — Intelligent Transport Systems (ITS)

Dieses Repository enthält das **AALeC Quiz**, ein lokales Echtzeit-Multiplayer-Quizspiel (ähnlich wie Kahoot), das physische Hardware-Buzzer (ESP8266-Controller) als Eingabegeräte nutzt.

---

## 🏗️ Systemarchitektur

Das System teilt sich in drei Hauptkomponenten auf:

1. **Hardware & Firmware (`src/firmware/Joahatunrecht`)**:
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
* [src/firmware/Joahatunrecht](file:///Users/merluee/ITS/src/firmware/Joahatunrecht) — PlatformIO C++ Firmware für die AALeC-Clients.
* [src/firmware/Hotspot](file:///Users/merluee/ITS/src/firmware/Hotspot) — Firmware für den Arduino Uno R4, der den WLAN-Hotspot aufspannt.
* [.github/workflows](file:///Users/merluee/ITS/.github/workflows) — CI/CD Workflows für automatisierte Builds und Releases.
* [docs](file:///Users/merluee/ITS/docs) — Weiterführende Dokumentationen und Anleitungen.

---

## 🚀 Setup & Ausführung

Folge diesen Schritten, um das Quiz-System lokal aufzusetzen und zu starten:

### Schritt 1: Netzwerk & mDNS konfigurieren (Wichtig!)
Damit du den Code auf den AALeC-Geräten nicht bei jedem Wechsel der IP-Adresse neu flashen musst, wird **mDNS** zur Broker-Suche genutzt.
1. Ermittle den lokalen Hostnamen deines Macs im Terminal:
   ```bash
   scutil --get LocalHostName
   # Beispielausgabe: MacBook-Pro-von-Florian
   ```
2. Kopiere die Konfigurationsdatei im Firmware-Ordner:
   ```bash
   cp src/firmware/Joahatunrecht/src/config.h.example src/firmware/Joahatunrecht/src/config.h
   ```
3. Passe in der neuen `config.h` den Hostnamen an (füge `.local` an deinen Mac-Namen an):
   ```cpp
   #define MQTT_BROKER   "MacBook-Pro-von-Florian.local"
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
1. Verbinde das AALeC-Gerät per USB mit deinem Mac.
2. Verbinde deinen Mac mit dem WLAN-Hotspot **`AALeC-Quiz`** (Passwort: `12345678`).
3. Öffne PlatformIO und führe den **Upload** für das Projekt `Joahatunrecht` aus.

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

## 🛠️ CI/CD Pipeline

Das Repository verfügt über eine automatisierte GitHub Actions Pipeline:
* **Bei jedem Push/PR:** Die Firmware wird kompiliert (Syntaxprüfung) und die Docker-Images werden gebaut.
* **Bei einem Version-Tag (z. B. `v1.0.0`):**
  * Die Docker-Images werden automatisch nach GHCR gepusht (`ghcr.io/xplorodox/its-backend` & `ghcr.io/xplorodox/its-frontend`).
  * Es wird automatisch ein **GitHub Release** erstellt, an dem die fertig kompilierte `firmware.bin` direkt als Download-Asset angehängt wird.

# AALeC Quiz — Frontend / Beamer-Projektor

Das Frontend des AALeC Quiz ist eine Next.js-Webanwendung, die als Lobby und Beamer-Projektionsfläche dient. Sie aktualisiert ihren Zustand vollautomatisch per MQTT über WebSockets und erzeugt spielerische Soundeffekte.

Zudem enthält das Frontend ein vollwertiges Admin-Panel unter `/admin` zur komfortablen Verwaltung und Erstellung von Fragensets.

---

## 🛠️ Stack & Technologien

- **Framework**: Next.js (Page-Routing/App-Router)
- **Sprache**: TypeScript
- **Styling**: Vanilla CSS Modules (für vollkommen anpassbare, performante Animationen & Responsive-Design)
- **Kommunikation**: `mqtt` (npm-Bibliothek für MQTT-over-WebSockets im Browser)
- **Soundeffekte**: Web Audio API (Synthesizer im Browser generiert Töne dynamisch; benötigt keine Mediendateien!)

---

## 📂 Dateistruktur

```text
src/
├── app/
│   ├── admin/
│   │   ├── admin.module.css  # Styling des Admin-Panels
│   │   └── page.tsx          # Admin-Editor für Fragensets
│   ├── globals.css           # Globales Farbschema, Fonts und Resets
│   ├── layout.tsx            # HTML Root-Struktur
│   └── page.tsx              # Hauptseite (Beamer-Lobby und automatisches Routing)
├── components/
│   ├── Confetti.tsx          # Canvas-basiertes Konfetti-Partikelsystem
│   └── screens/
│       ├── LobbyScreen.module.css # Styles für die Lobby
│       ├── QuestionScreen.tsx     # Anzeige der Fragen & Countdown
│       ├── RevealScreen.tsx       # Diagramme der Antworten & Auflösung
│       ├── ScoresScreen.tsx       # Leaderboard und Gewinnermedaillen
│       ├── WaitingScreen.tsx      # Lobby-Wartebereich & Set-Auswahl
│       └── screens.module.css     # Allgemeine Animations- & Screen-Klassen
├── hooks/
│   ├── useMqtt.ts            # Verbindungsaufbau, State-Synchronisation & Publish-Methoden
│   └── useSound.ts           # Web-Audio Synthesizer-Steuerung für State-Übergänge
└── types/
    └── quiz.ts               # TypeScript-Typdefinitionen für Fragen, Antworten und Spieler
```

---

## 🔄 Client-Zustand & Ablauf

1. Das Haupt-Layout bindet den React Hook `useMqtt()` ein. Dieser baut eine WebSocket-Verbindung zum MQTT-Broker auf (standardmäßig über `ws://localhost:9001`).
2. Der Hook subskribiert alle relevanten `quiz/`-Topics und speichert den eingehenden Spielzustand im lokalen React-State.
3. Die Hauptseite (`src/app/page.tsx`) führt ein bedingtes Rendering (Routing) basierend auf `gameState.state` aus:
   - **`WAITING`** ──► `<WaitingScreen />` (Lobby & Verbindungssignal)
   - **`QUESTION`** ──► `<QuestionScreen voting={false} />` (Fragenankündigung, noch keine Antwortabgabe erlaubt)
   - **`VOTING`** ──► `<QuestionScreen voting={true} />` (Abstimmung geöffnet, Countdown läuft)
   - **`REVEAL`** ──► `<RevealScreen />` (Ergebnispräsentation & Diagrammzeichnung)
   - **`SCORES`** ──► `<ScoresScreen ended={false} />` (Aktuelles Leaderboard)
   - **`ENDED`** ──► `<ScoresScreen ended={true} />` (Endergebnis mit `<Confetti />` & Restart-Button)
4. Jedes Mal, wenn sich der Zustand ändert, spielt der Hook `useSound(state)` den passenden Akkord oder Arpeggio-Effekt ab.

---

## 🎹 Sound-Synthese (`useSound.ts`)

Die Soundeffekte werden vollkommen clientseitig erzeugt:
- **Fragenankündigung (`QUESTION`)**: Eine einfache abwechselnde Sinuswellen-Melodie.
- **Auflösung (`REVEAL`)**: Ein stimmungsvoller C5-Dur-Dreiklang (Dreieckswelle, leicht verzögert angespielt für Fülle).
- **Leaderboard (`SCORES`)**: Ein aufsteigendes Arpeggio (C-D-E-G) mit sanft abklingendem Gain-Verlauf.
- **Spielende (`ENDED`)**: Eine klassische Fanfare (C-E-G-C) mit Dreieckswelle.

---

## 🛡️ Admin-Panel (`/admin`)

Das Admin-Panel ist unter `http://localhost:3000/admin` erreichbar. Es kommuniziert mit der REST-API des Python Game Masters auf Port `8080` und bietet folgende Funktionen:
1. **Fragen-Sets verwalten**: Neue Sets anlegen, vorhandene JSON-Dateien laden oder löschen.
2. **Aktives Set setzen**: Über ein Play-Symbol wird dem Game Master signalisiert, welches Set für das nächste Spiel geladen werden soll.
3. **Fragen-Editor**:
   - Fragentexte und Antwortoptionen anpassen.
   - Zeitlimits bearbeiten.
   - Zwischen verschiedenen Fragentypen wählen: *Multiple Choice*, *Schätzfragen*, *Höher/Niedriger*, *Poti-Challenge* und *Temperatur-Challenge*.
4. **Drag & Drop Sortierung**: Fragen können einfach per Drag-Handle in eine neue Reihenfolge gezogen werden.
5. **Validierung vor dem Speichern**: Erkennt fehlende Werte vor dem Übermitteln an den Server.

---

## 🚀 Lokale Ausführung (Entwicklung)

1. **Abhängigkeiten installieren**:
   ```bash
   npm install
   ```
2. **Entwicklungsserver starten**:
   ```bash
   npm run dev
   ```
3. **Im Browser öffnen**:
   - Beamer-Präsentation: `http://localhost:3000`
   - Admin-Bereich: `http://localhost:3000/admin`

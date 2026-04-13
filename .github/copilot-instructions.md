# Copilot Instructions — AALeC Multiplayer Quiz

This repository contains the software for an **MQTT-based multiplayer quiz
system** built around the **AALeC** (Aalener Lern-Computer) hardware platform,
developed at Hochschule Aalen.

---

## System Overview

Students participate in a live quiz using their AALeC devices. A central server
manages the game state, sends questions to all clients, collects answers, and
calculates scores. A beamer view (browser-based) displays the question and live
results to the room.

```
                        MQTT Broker
                            │
          ┌─────────────────┼─────────────────┐
          │                 │                 │
   Game Master         AALeC #1 … #N      Beamer View
  (Python backend)    (ESP8266 firmware)  (Browser / HTML)
          │                 │                 │
   publishes questions   shows A/B/C/D    shows question +
   collects answers      on OLED display    live bar chart
   calculates scores     rotary = select    after reveal
```

---

## Hardware: AALeC V3

Each AALeC is a **Wemos D1 Mini** (ESP8266) with:

| Component | Purpose in quiz |
|-----------|-----------------|
| OLED display (SSD1306, I²C) | Shows A / B / C / D answer options |
| Rotary encoder + push button | Navigate and confirm answer selection |
| 5× WS2812B LEDs | Visual feedback (correct = green, wrong = red) |
| BME280/BME680 sensor | Not used in quiz mode |
| WiFi (ESP8266 built-in) | MQTT over TCP/IP |

**GPIO mapping relevant for quiz firmware:**

| GPIO | Function |
|------|----------|
| 0 | Rotary encoder button (confirm answer) |
| 2 | WS2812B LED chain |
| 4 | SDA (OLED) |
| 5 | SCL (OLED) |
| 12 | Rotary encoder channel A |
| 14 | Rotary encoder channel B |

**Library dependencies (Arduino):**
- `Adafruit_NeoPixel` — LED chain
- `ESP8266 and ESP32 OLED Driver for SSD1306 display` — OLED
- `PubSubClient` — MQTT client
- `ArduinoJson` — JSON message parsing

---

## MQTT Topic Structure

All topics are prefixed with `quiz/`.

### Server → Clients (publish)

| Topic | Payload | Description |
|-------|---------|-------------|
| `quiz/state` | JSON | Current game state (see below) |
| `quiz/question` | JSON | Active question with answer options |
| `quiz/reveal` | JSON | Correct answer + per-option counts |
| `quiz/scores` | JSON | Current scoreboard |

### Clients → Server (publish)

| Topic | Payload | Description |
|-------|---------|-------------|
| `quiz/answer/<device_id>` | JSON | Player's answer submission |
| `quiz/connect/<device_id>` | JSON | Device registration on startup |
| `quiz/disconnect/<device_id>` | — | LWT (Last Will and Testament) topic |

### Beamer (subscribe-only)

The beamer view subscribes to `quiz/state`, `quiz/question`, `quiz/reveal`, and
`quiz/scores`. It never publishes.

---

## Message Formats

### `quiz/question`
```json
{
  "id": 3,
  "text": "Was ist die Hauptstadt von Frankreich?",
  "options": {
    "A": "Berlin",
    "B": "Paris",
    "C": "Madrid",
    "D": "Rom"
  },
  "time_limit_s": 20
}
```

### `quiz/answer/<device_id>`
```json
{
  "question_id": 3,
  "answer": "B",
  "elapsed_ms": 4200
}
```

### `quiz/state`
```json
{
  "state": "VOTING",
  "question_id": 3,
  "remaining_s": 14
}
```
Valid states: `WAITING` | `QUESTION` | `VOTING` | `REVEAL` | `SCORES` | `ENDED`

### `quiz/reveal`
```json
{
  "question_id": 3,
  "correct": "B",
  "counts": { "A": 2, "B": 11, "C": 1, "D": 0 }
}
```

### `quiz/scores`
```json
{
  "scores": [
    { "device_id": "aAlec-42", "name": "Max", "score": 1850 },
    { "device_id": "aAlec-07", "name": "Lisa", "score": 1600 }
  ]
}
```

---

## Game Master (Python Backend)

The server is a Python application responsible for the full game loop.

### State machine

```
WAITING ──► QUESTION ──► VOTING ──► REVEAL ──► SCORES
               ▲                                  │
               └──────────── (next question) ◄────┘
                                                  │ (last question)
                                               ENDED
```

### Scoring

- Base score per correct answer: **1000 points**
- Time bonus: linear, up to **+500 points** for instant answer
  - `bonus = round(500 * (1 - elapsed_ms / (time_limit_s * 1000)))`
- No points for wrong answers or late submissions (after `REVEAL`)

### Device lifecycle

- On connect: device publishes to `quiz/connect/<id>` with `{"name": "...", "device_id": "..."}`
- LWT: device registers `quiz/disconnect/<id>` with the broker on connect
- Server tracks connected devices and excludes disconnected ones from scoring

### Key responsibilities

- Load question set from JSON file
- Maintain game state and publish `quiz/state` on every transition
- Accept registrations during `WAITING` state only
- Open answer window during `VOTING`, reject late answers
- Compute scores, publish `quiz/reveal` and `quiz/scores`
- Advance game loop (timed transitions via `asyncio` or threading)

---

## AALeC Firmware Behavior

### Display layout (OLED 128×64)

```
┌──────────────────────────────┐
│  Frage 3 / 10      [14s]     │  ← question number + timer
├──────────────────────────────┤
│  > A  Berlin                 │  ← answer options, rotary selects
│    B  Paris                  │
│    C  Madrid                 │
│    D  Rom                    │
└──────────────────────────────┘
```

After submission the display shows `Antwort: B` and locks input until `REVEAL`.

### LED feedback

| Event | LED pattern |
|-------|-------------|
| Connected, waiting | Slow blue pulse |
| Question active | White, dim |
| Answer submitted | Cyan, solid |
| Correct answer revealed | Green, bright |
| Wrong answer revealed | Red, brief flash → off |
| Disconnected / error | Red, solid |

### Rotary encoder

- Rotate: cycle through A / B / C / D
- Push: confirm selection (only valid during `VOTING` state)
- Double-push during `WAITING`: register device name (if name entry is implemented)

---

## Beamer View

A single-page HTML/JS app served by the game master (or opened as a local file).
Subscribes to MQTT via WebSocket (e.g. using `mqtt.js` against the broker's WS
port).

### Screens

| State | What is shown |
|-------|---------------|
| `WAITING` | Connected player list, waiting message |
| `QUESTION` | Question text, countdown timer |
| `VOTING` | Question text + animated countdown, answer count (no options revealed) |
| `REVEAL` | Bar chart of answer distribution, correct answer highlighted |
| `SCORES` | Scoreboard (top N players) |
| `ENDED` | Final scoreboard, winner highlight |

---

## Project Structure

```
AALeC-Quiz/
├── server/
│   ├── game_master.py      # Main loop, state machine
│   ├── mqtt_client.py      # MQTT publish/subscribe helpers
│   ├── scoring.py          # Score calculation
│   ├── questions.json      # Question bank
│   ├── pyproject.toml      # uv project definition + dependencies
│   └── uv.lock             # Locked dependency tree (commit this)
├── firmware/
│   ├── aAlec_quiz/
│   │   ├── aAlec_quiz.ino  # Arduino sketch (main)
│   │   ├── config.h        # WiFi credentials + broker IP (do not commit)
│   │   ├── display.h/.cpp  # OLED rendering
│   │   ├── leds.h/.cpp     # WS2812B feedback
│   │   └── mqtt.h/.cpp     # MQTT connection + message handling
│   └── lib/                # Local library copies if needed
├── beamer/
│   └── index.html          # Single-page beamer view (self-contained)
├── mosquitto.conf          # Broker config (TCP 1883 + WS 9001)
├── docs/
│   └── mqtt-topics.md      # Extended topic documentation
└── README.md
```

---

## Development Setup

### Broker

Any MQTT broker works. For local development use the included config:
```bash
docker run -d -p 1883:1883 -p 9001:9001 \
  -v $(pwd)/mosquitto.conf:/mosquitto/config/mosquitto.conf \
  eclipse-mosquitto
```
`mosquitto.conf` enables TCP on 1883 **and** WebSockets on 9001 (required by the
beamer). Minimal config:
```
listener 1883
listener 9001
protocol websockets
allow_anonymous true
```

### Python server

Uses [uv](https://docs.astral.sh/uv/) for dependency management — same tooling
as in the Locust teaching repo.

```bash
cd server
uv sync                          # install deps from uv.lock
uv run game_master.py --broker localhost --questions questions.json
```

To add a dependency:
```bash
uv add paho-mqtt
```

Never edit `uv.lock` by hand and always commit it alongside `pyproject.toml`.

### Firmware

Open `firmware/aAlec_quiz/aAlec_quiz.ino` in the Arduino IDE. Copy
`config.h.example` to `config.h` and fill in your WiFi credentials and broker
IP before flashing. `config.h` is gitignored.

### Beamer

`beamer/index.html` is self-contained — no build step, no separate JS file.
Open it directly in a browser (full-screen on the beamer):
```bash
xdg-open beamer/index.html      # Linux
open beamer/index.html          # macOS
```
The broker WebSocket URL is configured at the top of the `<script>` block
(default: `ws://localhost:9001`). Change it to match your broker IP before
the session.

---

## Coding Conventions

- **Python**: PEP 8, type hints where practical, `paho-mqtt>=2.0` for broker communication
- **Package management**: `uv` — use `uv add <pkg>` (never `pip install`), always commit `uv.lock`
- **Arduino/C++**: Follow AALeC library style; use the AALeC library for display and LEDs
- **Beamer**: `beamer/index.html` is a single self-contained file — no bundler, no separate JS. Keep it that way.
- **JSON messages**: always include `question_id` to allow late-message detection
- **Device IDs**: use the ESP8266 chip ID, formatted as `aAlec-<hex>` (6 digits)
- **State transitions**: only the game master changes state; clients are always receivers of state
- **`config.h` is gitignored** — never commit WiFi credentials or broker IPs

---

## AI Diary

Every AI-assisted contribution is documented in `diary/<branch>/NNN-title.md`.

```markdown
# NNN — Short Title

**Date**: YYYY-MM-DD
**Tool**: [Claude / Copilot / Cursor / ...]
**Model**: [model name]
**Iterations**: [number of follow-up prompts]

## Prompt

**YYYY-MM-DD HH:MM**

[Full prompt text.]
```

Commit convention:
```
[diary] NNN — Short description of what was prompted
```

Every AI-assisted code change **must** have a corresponding diary entry
committed alongside it.

---

## Do NOT Modify

- `lib/` inside `firmware/` — vendored Arduino library copies
- `uv.lock` by hand — always go through `uv add` / `uv remove`
- `config.h` — local secrets, gitignored, each developer has their own
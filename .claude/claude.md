# Copilot Instructions — AALeC Multiplayer Quiz

## Repository Status (Current Phase)

As of **2026-04-13**, this repository is in a **bootstrap/preparation phase**.

- Focus currently on repository structure, process documentation, and DevOps
  readiness.
- Do **not** implement production backend, firmware, or frontend logic unless the
  prompt explicitly requests coding those components.
- Prefer placeholders, scaffolding, CI quality gates, and architecture docs.
- Canonical source layout is `src/backend`, `src/frontend`, and `src/firmware`.

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

## Frontend View

A Node.js-based frontend (Next.js) for projector/beamer usage.
Consumes MQTT state via a suitable bridge/client strategy for browser contexts
(for example via broker WebSocket support).

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
├── .github/
│   ├── workflows/           # CI workflows (bootstrap checks first)
│   └── copilot-instructions.md
├── diary/
│   ├── README.md
│   └── <branch>/NNN-title.md
├── src/
│   ├── backend/
│   │   └── README.md        # Placeholder during bootstrap
│   ├── firmware/
│   │   ├── aAlec_quiz/
│   │   │   └── README.md    # Placeholder during bootstrap
│   │   └── lib/             # Local library copies if needed
│   └── frontend/
│       └── README.md        # Placeholder during bootstrap (Node/Next.js)
├── docs/
│   └── devops/
│       ├── README.md
│       └── ci-cd-plan.md
├── docs/mqtt-topics.md      # Extended topic documentation
├── mosquitto.conf           # Broker config (optional in later phase)
├── LICENSE
└── README.md
```

Planned implementation files listed in this document remain targets for later
phases and may not exist yet.

---

## Development Setup

In bootstrap phase, prefer setting up CI pipelines and repository standards
before introducing runtime components.

### Broker

Any MQTT broker works. For local development use the included config:
```bash
podman run -d -p 1883:1883 -p 9001:9001 \
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

### Backend (Python)

Uses [uv](https://docs.astral.sh/uv/) for dependency management.

```bash
cd src/backend
uv sync                          # install deps from uv.lock
uv run game_master.py --broker localhost --questions questions.json
```

To add a dependency:
```bash
uv add paho-mqtt
```

Never edit `uv.lock` by hand and always commit it alongside `pyproject.toml`.

If `src/backend/pyproject.toml` does not yet exist, prepare only scaffolding and CI
checks first.

### Firmware

Open `src/firmware/aAlec_quiz/aAlec_quiz.ino` in the Arduino IDE. Copy
`config.h.example` to `config.h` and fill in your WiFi credentials and broker
IP before flashing. `config.h` is gitignored.

### Frontend (Node/Next.js)

Frontend implementation should live in `src/frontend` as a Next.js app.
In bootstrap phase, keep it as scaffolding only. Once implemented, standard
local development flow should be:
```bash
cd src/frontend
npm ci
npm run dev
```
Keep broker configuration environment-based (for example via `.env.local`) and
never commit secrets.

---

## Coding Conventions

- **Python**: PEP 8, type hints where practical, `paho-mqtt>=2.0` for broker communication
- **Package management**: `uv` — use `uv add <pkg>` (never `pip install`), always commit `uv.lock`
- **Arduino/C++**: Follow AALeC library style; use the AALeC library for display and LEDs
- **Frontend**: use Node.js + Next.js in `src/frontend`; prefer TypeScript and
  keep architecture modular.
- **JSON messages**: always include `question_id` to allow late-message detection
- **Device IDs**: use the ESP8266 chip ID, formatted as `aAlec-<hex>` (6 digits)
- **State transitions**: only the game master changes state; clients are always receivers of state
- **`config.h` is gitignored** — never commit WiFi credentials or broker IPs

## DevOps Baseline (Bootstrap)

- Keep `.github/workflows/tests.yml` passing at all times.
- CI in current phase should validate repository quality gates, not product
  runtime behavior.
- Minimum expected checks:
  - workflow linting (`actionlint`)
  - markdown linting
  - repository structure sanity checks for `src/backend`, `src/frontend`, and
    `src/firmware`
- Only add runtime test jobs (Python, Arduino, frontend) when the respective
  code actually exists.

---

## AI Diary

Every AI-assisted contribution is documented in `diary/<branch>/NNN-title.md`.

```markdown
# NNN — Short Title

**Date**: YYYY-MM-DD
**Tool**: [Claude / Copilot / Cursor / ...]
**Model**: [model name, e.g., GPT-5.3-Codex]
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

- `lib/` inside `src/firmware/` — vendored Arduino library copies
- `uv.lock` by hand — always go through `uv add` / `uv remove`
- `config.h` — local secrets, gitignored, each developer has their own
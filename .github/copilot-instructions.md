# Copilot Instructions — AALeC Multiplayer Quiz

## Repository Status

This repository contains a **working implementation** of an **MQTT-based
multiplayer quiz system** built around the **AALeC** (Aalener Lern-Computer)
hardware platform, developed at Hochschule Aalen.

The bootstrap phase is over. All three components are implemented and run
together (locally or via `docker compose`):

- **Backend** — `src/backend/game_master.py`: full game loop, 5 question types,
  streak scoring, state persistence, and an admin REST API.
- **Frontend** — `src/frontend/`: a Next.js beamer view (TypeScript) that
  subscribes to the broker over WebSockets and an admin page for question sets.
- **Firmware** — `src/firmware/Joahatunrecht/`: the AALeC V3 quiz client
  (ESP8266); `src/firmware/Hotspot/`: an Arduino UNO R4 WiFi access point.

When changing one component, keep the **MQTT contract** below in sync across
backend, frontend, and firmware — it is the single source of truth that ties
them together. Keep `uv.lock` committed and CI green.

---

## System Overview

Students participate in a live quiz using their AALeC devices. The game master
manages state, sends questions to all clients, collects answers, and calculates
scores. A beamer view (browser-based) displays the question and live results.

```text
                        MQTT Broker
                            │
          ┌─────────────────┼─────────────────┐
          │                 │                 │
   Game Master         AALeC #1 … #N      Beamer View
  (Python backend)    (ESP8266 firmware)  (Next.js / browser)
          │                 │                 │
   publishes questions   shows A/B/C/D    shows question +
   collects answers      / poti / temp     live results +
   calculates scores     rotary = select   admin controls
```

---

## Hardware: AALeC V3

Each AALeC is a **Wemos D1 Mini** (ESP8266) with:

| Component | Purpose in quiz |
|-----------|-----------------|
| OLED display (SSD1306, I²C) | Shows A / B / C / D answer options, estimates, targets |
| Rotary encoder + push button | Navigate and confirm answer selection |
| 5× WS2812B LEDs | Visual feedback (correct = green, wrong = red) |
| BME280/BME680 sensor | Used by `temp_target` questions (heat the sensor) |
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

**Library dependencies** are managed via PlatformIO (`platformio.ini`), not the
Arduino IDE library manager. Key libs: `AALeC-V3`, `Adafruit NeoPixel`,
`Adafruit SSD1306` + `GFX`, `PubSubClient`, `ArduinoJson`, sensor libs.

---

## MQTT Topic Structure

All topics are prefixed with `quiz/`. Retained topics are marked **(retain)** —
they let late-joining clients (beamer, reconnecting devices) get current state.

### Server → Clients / Beamer (publish)

| Topic | Retain | Description |
|-------|--------|-------------|
| `quiz/state` | ✅ | Current game state (state name, question id, remaining_s) |
| `quiz/question` | — | Active question (shape depends on `type`, see below) |
| `quiz/reveal` | — | Correct answer + per-type result data |
| `quiz/scores` | — | Current scoreboard (sorted, includes streak) |
| `quiz/players` | ✅ | Lobby player list with online/offline + `min_players` |
| `quiz/ack/<device_id>` | — | Registration confirmation for a device |
| `quiz/answer_count` | — | Live answered/total counter for the beamer |
| `quiz/question_sets` | ✅ | Available question sets + active set name |
| `quiz/namelist` | ✅ | Pre-defined name list relayed to devices |
| `quiz/name/reset` | — | Tells devices to clear their stored name |

### Clients (AALeC) → Server (publish)

| Topic | Payload | Description |
|-------|---------|-------------|
| `quiz/answer/<device_id>` | JSON | Player's answer submission |
| `quiz/connect/<device_id>` | JSON | Device registration on startup (`{"name": "..."}`) |
| `quiz/disconnect/<device_id>` | — | LWT (Last Will and Testament) topic |

### Frontend / Beamer → Server (publish)

| Topic | Payload | Description |
|-------|---------|-------------|
| `quiz/control` | JSON | Game control: `{"action": "start" \| "restart" \| "reset_names" \| "load_set", "name"?: "..."}` |
| `quiz/namelist/set` | JSON | `{"names": [...]}` — name list to relay to devices (max 20, ≤15 chars each) |

The beamer also subscribes (read-only) to `quiz/state`, `quiz/question`,
`quiz/reveal`, `quiz/scores`, `quiz/answer_count`, `quiz/players`,
`quiz/question_sets`.

---

## Question Types

Questions are stored in a JSON array. A question's `type` field selects the
format (default `"mcq"` if omitted). All questions have `text` and
`time_limit_s`. Valid types: `mcq`, `estimate`, `higher_lower`, `poti_target`,
`temp_target`.

### `mcq` (default)

```json
{
  "type": "mcq",
  "text": "Was ist die Hauptstadt von Frankreich?",
  "options": { "A": "Berlin", "B": "Paris", "C": "Madrid", "D": "Rom" },
  "correct": "B",
  "time_limit_s": 20
}
```

`options` must have exactly keys `A B C D`; `correct` is one of them.

### `estimate`

```json
{
  "type": "estimate",
  "text": "Wie viele Einwohner hat Stuttgart? (Tausend)",
  "min": 0, "max": 1000, "unit": "Tsd.",
  "correct": 626,
  "time_limit_s": 30
}
```

Player submits a number; scored by relative error against `correct` (see Scoring).

### `higher_lower`

```json
{
  "type": "higher_lower",
  "text": "Ist der Eiffelturm höher oder niedriger als 400 m?",
  "reference": 400, "unit": "m",
  "correct": "LOWER", "actual": 330,
  "time_limit_s": 20
}
```

`correct` is `HIGHER` or `LOWER`; `actual` is revealed to the room.

### `poti_target`

```json
{
  "type": "poti_target",
  "text": "Dreh das Poti auf genau 75%!",
  "target": 75, "tolerance": 5,
  "time_limit_s": 20
}
```

Player turns the rotary/poti to a target percentage. `tolerance` defaults to `5`.

### `temp_target`

```json
{
  "type": "temp_target",
  "text": "Wärme den Sensor auf 30°C!",
  "target": 30.0, "tolerance": 1.5,
  "time_limit_s": 45
}
```

Player heats the BME sensor to a target °C. `tolerance` defaults to `1.5`.

---

## Message Formats

### `quiz/question` (published)

Common fields plus type-specific extras:

```jsonc
{
  "id": 3,              // running question id (1-based)
  "total": 7,           // number of questions in the set
  "type": "mcq",
  "text": "...",
  "time_limit_s": 20,
  // mcq:          "options": {"A":..,"B":..,"C":..,"D":..}
  // estimate:     "min", "max", "unit"
  // higher_lower: "reference", "unit"
  // poti_target:  "target", "tolerance"
  // temp_target:  "target", "tolerance"
}
```

### `quiz/answer/<device_id>`

```json
{ "question_id": 3, "answer": "B", "elapsed_ms": 4200 }
```

`answer` is `A/B/C/D` (mcq), `HIGHER/LOWER` (higher_lower), or a number
(estimate / poti_target / temp_target).

### `quiz/state`

```json
{ "state": "VOTING", "question_id": 3, "remaining_s": 14 }
```

Valid states: `WAITING` | `QUESTION` | `VOTING` | `REVEAL` | `SCORES` | `ENDED`

### `quiz/reveal` (shape depends on type)

```jsonc
// mcq
{ "question_id": 3, "correct": "B", "counts": {"A":2,"B":11,"C":1,"D":0} }

// higher_lower
{ "question_id": 4, "type": "higher_lower", "correct": "LOWER",
  "actual": 330, "unit": "m", "counts": {"HIGHER":3,"LOWER":9} }

// estimate / poti_target / temp_target
{ "question_id": 5, "type": "estimate", "correct": 626, "unit": "Tsd.",
  "answers": [ {"device_id":"aAlec-42","name":"Max","value":600,"delta":26} ] }
```

### `quiz/scores`

```json
{ "scores": [ {"device_id":"aAlec-42","name":"Max","score":1850,"streak":3} ] }
```

### `quiz/players`

```json
{ "players": [ {"device_id":"aAlec-42","name":"Max","online":true} ],
  "min_players": 1 }
```

---

## Game Master (Python Backend)

`src/backend/game_master.py` is the single authority for game state.

### State machine

```text
WAITING ──► QUESTION ──► VOTING ──► REVEAL ──► SCORES
               ▲           │                      │
               │           │ (all online players  │
               │           │  answered → early)   │
               └──────── (next question) ◄────────┘
                                                  │ (last question)
                                               ENDED ──► (restart) ──► WAITING
```

Timed transitions use `threading.Timer`: QUESTION shows for 3 s, VOTING runs for
`time_limit_s` (or ends early once every online player has answered), REVEAL and
SCORES each last 5 s.

### Concurrency

State (`gs`, `gs.answers`, `players`, `questions`) is touched by three thread
groups: the MQTT callback thread, the Flask admin thread, and `threading.Timer`
callbacks. All entry points that mutate shared state are wrapped with the
`@_locked` decorator backed by a **re-entrant** `threading.RLock`, making each
state transition atomic. When adding a new mutating method, decorate it with
`@_locked` rather than hand-rolling lock blocks.

### Scoring

- **mcq / higher_lower** — correct answer: `1000` base + time bonus + streak bonus.
  - Time bonus (linear): `round(500 * (1 - elapsed_ms / (time_limit_s*1000)))`.
  - Streak bonus: `+200` per consecutive-correct level, capped at level 3
    (`streak_level = min(streak-1, 3)` → max `+600`).
- **estimate** — tiered by relative error `|guess-correct| / (max-min)`:
  `≤0%→1000`, `≤5%→800`, `≤10%→600`, `≤20%→400`, `≤30%→200`, beyond → `0`.
  Time bonus added when within a scoring tier.
- **poti_target / temp_target** — linear decay inside tolerance:
  `round(1000 * (1 - delta/tolerance))`; `0` beyond tolerance. No time bonus.
- Wrong / no / late answer → `0` points and the player's streak resets to `0`.

### Persistence

State is saved to `--state-file` (default `game_state.json`) after each SCORES
and on ENDED/restart: players, scores, streaks, and `question_index`. On
startup it is restored; restored players are marked **offline** until they
reconnect. `game_state.json` is a runtime artifact and is gitignored.

### Admin REST API

A Flask server (default port `8080`, CORS open) manages question sets. Set names
must match `[A-Za-z0-9_-]+`. Sets are JSON files in `--questions-dir`.

| Method & path | Description |
|---------------|-------------|
| `GET /api/question-sets` | List sets: `[{name, count, active}]` |
| `GET /api/question-sets/<name>` | Return a set's question array |
| `PUT /api/question-sets/<name>` | Create/replace a set (validated; 422 on errors) |
| `DELETE /api/question-sets/<name>` | Delete a set (409 if it is the active set) |
| `GET /api/active-set` | `{active}` |
| `POST /api/active-set` | `{name}` → switch active set (only while `WAITING`) |

> The API has **no authentication** and CORS is `*` — intended for a trusted
> classroom LAN only. Do not expose it publicly without adding auth.

### Key responsibilities

- Load and validate question sets; switch sets only while in `WAITING`.
- Maintain game state and publish `quiz/state` (retained) on every transition.
- Accept new registrations during `WAITING`; allow reconnects any time.
- Open the answer window during `VOTING`, reject stale/duplicate/late answers.
- Compute scores, publish `quiz/reveal` and `quiz/scores`.

---

## AALeC Firmware Behavior

(`src/firmware/Joahatunrecht/` — ESP8266 quiz client.)

### Player name

The device stores a 5-char player name in EEPROM. A long press of the rotary
button clears it; the backend can also broadcast `quiz/name/reset`. Names can be
pre-seeded from the beamer via `quiz/namelist`.

### Display layout (OLED 128×64)

```text
┌──────────────────────────────┐
│  Frage 3 / 10      [14s]     │  ← question number + timer
├──────────────────────────────┤
│  > A  Berlin                 │  ← answer options, rotary selects
│    B  Paris                  │
│    C  Madrid                 │
│    D  Rom                    │
└──────────────────────────────┘
```

MCQ shows A/B/C/D; `poti_target` shows a percentage; `temp_target` shows the
live sensor temperature; `estimate`/`higher_lower` use the appropriate input UI.
After submission the display locks input until `REVEAL`.

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

- Rotate: cycle/adjust the current input (option, estimate value, target).
- Push: confirm selection (only valid during `VOTING`).
- Long press during name entry: reset the stored name.

---

## Frontend View (Next.js / Beamer)

`src/frontend/` is a Next.js + TypeScript app. It connects to the broker over
WebSockets via `mqtt.js` (`useMqtt` hook) and renders one screen per game state.
The MQTT WebSocket URL is read from `NEXT_PUBLIC_MQTT_URL` (build-time, inlined).

### Screens

| State | What is shown |
|-------|---------------|
| `WAITING` | Lobby: connected players, name-list editor, start button |
| `QUESTION` | Question text, countdown timer |
| `VOTING` | Question + countdown, live answered/total count |
| `REVEAL` | Result view (bar chart for mcq/higher_lower; ranked list for estimate/target) |
| `SCORES` | Scoreboard (top players, with streaks) |
| `ENDED` | Final scoreboard, winner highlight |

There is also an **admin page** (`/admin`) that talks to the REST API to create,
edit, delete, and activate question sets.

---

## Project Structure

```text
ITS/
├── .github/
│   ├── workflows/                 # CI workflows
│   ├── ISSUE_TEMPLATE/
│   └── copilot-instructions.md
├── docs/
│   ├── asyncapi.yaml              # AsyncAPI spec of the MQTT contract
│   ├── howto.md
│   └── devops/
├── src/
│   ├── backend/
│   │   ├── game_master.py         # game loop + admin REST API
│   │   ├── questions.json         # default question set
│   │   ├── tests/test_scoring.py
│   │   ├── pyproject.toml / uv.lock
│   │   └── Dockerfile
│   ├── firmware/
│   │   ├── Joahatunrecht/         # AALeC V3 quiz client (ESP8266, PlatformIO)
│   │   │   ├── src/main.cpp
│   │   │   ├── src/config.h.example
│   │   │   └── lib/               # vendored Arduino libraries (do not modify)
│   │   └── Hotspot/               # Arduino UNO R4 WiFi access point
│   └── frontend/                  # Next.js beamer + admin (TypeScript)
│       ├── src/
│       ├── Dockerfile
│       └── .env.local             # NEXT_PUBLIC_MQTT_URL (dev)
├── docker-compose.yml             # mosquitto + game-master + frontend
├── mosquitto.conf
├── LICENSE
└── README.md
```

---

## Development Setup

### Everything at once (Docker)

```bash
docker compose up --build
```
Starts the broker (1883 TCP, 9001 WebSockets), game master, and frontend
(`http://localhost:3000`). The frontend's broker URL is baked at build time via
the `NEXT_PUBLIC_MQTT_URL` build arg — it must be reachable **from the beamer's
browser** (e.g. `ws://localhost:9001`, or `ws://<host-ip>:9001` for a remote
beamer), *not* the internal `mosquitto` service name.

### Broker only

```bash
podman run -d -p 1883:1883 -p 9001:9001 \
  -v $(pwd)/mosquitto.conf:/mosquitto/config/mosquitto.conf \
  eclipse-mosquitto
```
`mosquitto.conf` enables TCP on 1883 **and** WebSockets on 9001 (required by the
beamer): `listener 1883` / `listener 9001` + `protocol websockets` /
`allow_anonymous true`.

### Backend (Python, uv)

```bash
cd src/backend
uv sync
uv run game_master.py --broker localhost --questions questions.json --api-port 8080
uv run pytest                      # run the scoring tests
```
Add deps with `uv add <pkg>` (never `pip install`); always commit `uv.lock`.

### Firmware (PlatformIO)

```bash
cd src/firmware/Joahatunrecht
cp src/config.h.example src/config.h   # fill in WiFi + broker IP (gitignored)
pio run                                # compile
pio run -t upload                      # flash
```

### Frontend (Node/Next.js)

```bash
cd src/frontend
npm ci
npm run dev
```
Set `NEXT_PUBLIC_MQTT_URL` in `.env.local` (e.g. `ws://localhost:9001`). Never
commit secrets.

---

## Coding Conventions

- **Python**: PEP 8, type hints where practical, `paho-mqtt>=2.0`. Mutating
  `GameMaster` methods must be `@_locked`. Validate question payloads via
  `_collect_question_errors` so the REST API and startup share one validator.
- **Package management**: `uv` — use `uv add <pkg>` (never `pip install`), always commit `uv.lock`.
- **Arduino/C++**: PlatformIO; use the AALeC library for display and LEDs.
- **Frontend**: Next.js + TypeScript in `src/frontend`; keep architecture modular.
- **JSON messages**: always include `question_id` to allow late-message detection.
- **Device IDs**: ESP8266 chip ID formatted as `aAlec-<hex>`.
- **State transitions**: only the game master changes state; clients always receive state.
- **MQTT contract**: when you change a topic or payload, update **all** of
  backend, frontend, firmware, and `docs/asyncapi.yaml` together.
- **`config.h` is gitignored** — never commit WiFi credentials or broker IPs.

## DevOps Baseline

- Keep `.github/workflows/tests.yml` passing at all times.
- CI runs: `actionlint`, markdown linting, and repository structure checks.
- **Add runtime test jobs as code grows** — at minimum the backend has a pytest
  suite (`src/backend/tests/`) that should run in CI; frontend `build`/lint and a
  firmware PlatformIO compile check are good next additions.

---

## Do NOT Modify

- `lib/` inside `src/firmware/` — vendored Arduino library copies
- `uv.lock` by hand — always go through `uv add` / `uv remove`
- `config.h` — local secrets, gitignored, each developer has their own

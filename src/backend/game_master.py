"""
AALeC Quiz — Game Master (MQTT Backend)

Usage:
    uv run game_master.py --broker localhost --questions questions.json

State machine:
    WAITING ──► QUESTION ──► VOTING ──► REVEAL ──► SCORES
                   ▲                                  │
                   └──────────── (next question) ◄────┘
                                                      │ (last question)
                                                   ENDED
"""

import argparse
import json
import logging
import os
import re
import sys
import threading
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional

import paho.mqtt.client as mqtt
from flask import Flask, jsonify, request, current_app

# ── Topics ────────────────────────────────────────────────────────────────────
T_STATE        = "quiz/state"
T_QUESTION     = "quiz/question"
T_REVEAL       = "quiz/reveal"
T_SCORES       = "quiz/scores"
T_PLAYERS      = "quiz/players"       # lobby player list (online/offline)
T_ACK          = "quiz/ack"           # registration confirmed → quiz/ack/<device_id>
T_ANSWER_COUNT = "quiz/answer_count"  # live counter for beamer
T_ANSWER       = "quiz/answer/#"      # subscribe pattern
T_CONNECT      = "quiz/connect/#"     # subscribe pattern
T_DISCONNECT   = "quiz/disconnect/#"  # subscribe pattern (LWT)
T_CONTROL      = "quiz/control"       # start command from frontend
T_NAMELIST     = "quiz/namelist"      # name list for devices
T_NAMELIST_SET = "quiz/namelist/set"  # name list from frontend
T_NAME_RESET   = "quiz/name/reset"    # reset command for device-side name selection

MIN_PLAYERS = 1

# ── Scoring constants ─────────────────────────────────────────────────────────
BASE_SCORE           = 1000
TIME_BONUS           = 500
STREAK_BONUS_PER_LVL = 200   # extra points per consecutive correct answer
MAX_STREAK_LEVELS    = 3      # cap at level 3 → max +600

# Estimate: tiered score by relative error  (|guess - correct| / range)
ESTIMATE_TIERS = [
    (0.00, 1000),  # exact (≤0%)
    (0.05,  800),  # within 5 % of range
    (0.10,  600),  # within 10 %
    (0.20,  400),  # within 20 %
    (0.30,  200),  # within 30 %
]                  # beyond 30 % → 0 points

VALID_TYPES = {"mcq", "estimate", "higher_lower", "poti_target", "temp_target"}


def _collect_question_errors(questions: list[dict]) -> list[str]:
    """Return validation error strings. Empty list = valid. Never calls sys.exit."""
    errors: list[str] = []
    if not questions:
        return ["questions list is empty — need at least one question"]
    for i, q in enumerate(questions):
        label  = f"  question {i + 1}"
        q_type = q.get("type", "mcq")
        if q_type not in VALID_TYPES:
            errors.append(
                f"{label}: unknown type {q_type!r}"
                f" (valid: {', '.join(sorted(VALID_TYPES))})"
            )
            continue
        missing: list[str] = []
        if "text" not in q:
            missing.append("text")
        if "time_limit_s" not in q:
            missing.append("time_limit_s")
        if q_type == "mcq":
            opts = q.get("options", {})
            if not isinstance(opts, dict) or set(opts.keys()) != {"A", "B", "C", "D"}:
                missing.append("options (dict with keys A B C D)")
            if str(q.get("correct", "")).upper() not in ("A", "B", "C", "D"):
                missing.append("correct (A | B | C | D)")
        elif q_type == "estimate":
            for field_name in ("min", "max", "correct"):
                if field_name not in q:
                    missing.append(field_name)
        elif q_type == "higher_lower":
            for field_name in ("reference", "actual"):
                if field_name not in q:
                    missing.append(field_name)
            if str(q.get("correct", "")).upper() not in ("HIGHER", "LOWER"):
                missing.append("correct (HIGHER | LOWER)")
        elif q_type in ("poti_target", "temp_target"):
            if "target" not in q:
                missing.append("target")
        if missing:
            errors.append(f"{label} (type={q_type!r}): missing/invalid: {', '.join(missing)}")
    return errors


def validate_questions(questions: list[dict]) -> None:
    """Validate question list at startup. Exits with a clear message on failure."""
    errors = _collect_question_errors(questions)
    if errors:
        print("ERROR: question validation failed:")
        for e in errors:
            print(e)
        sys.exit(1)
    print(f"[questions] {len(questions)} question(s) validated OK")


# ── Admin REST API ─────────────────────────────────────────────────────────────

logging.getLogger("werkzeug").setLevel(logging.ERROR)
_flask_app = Flask(__name__)


@_flask_app.after_request
def _add_cors(response):
    response.headers.update({
        "Access-Control-Allow-Origin":  "*",
        "Access-Control-Allow-Methods": "GET, PUT, POST, DELETE, OPTIONS",
        "Access-Control-Allow-Headers": "Content-Type",
    })
    return response


@_flask_app.route("/api/question-sets", methods=["GET", "OPTIONS"])
def _api_list_sets():
    if request.method == "OPTIONS":
        return "", 204
    return jsonify(current_app.config["gm"]._scan_question_sets())


@_flask_app.route("/api/question-sets/<name>", methods=["GET", "PUT", "DELETE", "OPTIONS"])
def _api_question_set(name: str):
    if request.method == "OPTIONS":
        return "", 204
    if not re.fullmatch(r"[A-Za-z0-9_-]+", name):
        return jsonify({"error": "invalid name"}), 400
    gm   = current_app.config["gm"]
    path = Path(gm.questions_dir) / f"{name}.json"

    if request.method == "GET":
        if not path.exists():
            return jsonify({"error": "not found"}), 404
        return jsonify(json.loads(path.read_text(encoding="utf-8")))

    if request.method == "PUT":
        qs = request.get_json(silent=True)
        if not isinstance(qs, list):
            return jsonify({"error": "body must be a JSON array"}), 400
        errs = _collect_question_errors(qs)
        if errs:
            return jsonify({"errors": errs}), 422
        tmp = str(path) + ".tmp"
        with open(tmp, "w", encoding="utf-8") as f:
            json.dump(qs, f, indent=2, ensure_ascii=False)
        os.replace(tmp, str(path))
        gm._publish_question_sets()
        return jsonify({"ok": True, "count": len(qs)})

    if request.method == "DELETE":
        if not path.exists():
            return jsonify({"error": "not found"}), 404
        if name == gm.active_set:
            return jsonify({"error": "cannot delete the active set"}), 409
        path.unlink()
        gm._publish_question_sets()
        return jsonify({"ok": True})


@_flask_app.route("/api/active-set", methods=["GET", "POST", "OPTIONS"])
def _api_active_set():
    if request.method == "OPTIONS":
        return "", 204
    gm = current_app.config["gm"]
    if request.method == "GET":
        return jsonify({"active": gm.active_set})
    data = request.get_json(silent=True) or {}
    name = str(data.get("name", ""))
    if not re.fullmatch(r"[A-Za-z0-9_-]+", name):
        return jsonify({"error": "invalid name"}), 400
    err = gm.load_set(name)
    if err:
        return jsonify({"error": err}), 400
    return jsonify({"ok": True, "active": gm.active_set})


@dataclass
class Player:
    device_id: str
    name: str
    score: int   = 0
    streak: int  = 0
    online: bool = True


@dataclass
class GameState:
    state: str = "WAITING"
    question_index: int = 0
    question_id: int = 0
    question_start: float = 0.0
    answers: dict = field(default_factory=dict)  # device_id → answer payload


class GameMaster:
    def __init__(
        self,
        broker: str,
        port: int,
        questions: list[dict],
        state_file: str    = "game_state.json",
        questions_dir: str = ".",
        active_set: str    = "questions",
        api_port: int      = 8080,
    ):
        self.broker        = broker
        self.port          = port
        self.questions     = questions
        self.state_file    = state_file
        self.questions_dir = questions_dir
        self.active_set    = active_set
        self.api_port      = api_port
        self.players: dict[str, Player] = {}
        self.gs            = GameState()
        self._timer: Optional[threading.Timer] = None
        self._lock         = threading.Lock()

        self._load_persisted_state()

        self.client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
        self.client.reconnect_delay_set(min_delay=1, max_delay=30)
        self.client.on_connect    = self._on_connect
        self.client.on_message    = self._on_message
        self.client.on_disconnect = self._on_disconnect

    # ── Question sets ─────────────────────────────────────────────────────────

    def _scan_question_sets(self) -> list[dict]:
        sets = []
        try:
            for path in sorted(Path(self.questions_dir).glob("*.json")):
                try:
                    qs = json.loads(path.read_text(encoding="utf-8"))
                    if not isinstance(qs, list):
                        continue  # skip non-array files (e.g. game_state.json)
                    sets.append({
                        "name":   path.stem,
                        "count":  len(qs),
                        "active": path.stem == self.active_set,
                    })
                except Exception:
                    pass
        except OSError:
            pass
        return sets

    def _publish_question_sets(self) -> None:
        self._publish("quiz/question_sets", {
            "sets":   self._scan_question_sets(),
            "active": self.active_set,
        }, retain=True)

    def load_set(self, name: str) -> str | None:
        """Switch to a different question set. Returns an error string or None on success."""
        if self.gs.state != "WAITING":
            return f"cannot switch set in {self.gs.state} state"
        path = Path(self.questions_dir) / f"{name}.json"
        if not path.exists():
            return f"set {name!r} not found"
        try:
            qs = json.loads(path.read_text(encoding="utf-8"))
        except (json.JSONDecodeError, OSError) as e:
            return f"could not load {name!r}: {e}"
        errs = _collect_question_errors(qs)
        if errs:
            return f"validation error: {errs[0]}"
        self.questions  = qs
        self.active_set = name
        print(f"[questions] switched to {name!r} ({len(qs)} questions)")
        self._publish_question_sets()
        return None

    def _start_api_server(self) -> None:
        _flask_app.config["gm"] = self
        t = threading.Thread(
            target=lambda: _flask_app.run(
                host="0.0.0.0", port=self.api_port,
                debug=False, use_reloader=False, threaded=True,
            ),
            daemon=True,
        )
        t.start()
        print(f"[api] admin API on http://0.0.0.0:{self.api_port}")

    # ── Persistence ───────────────────────────────────────────────────────────

    def _load_persisted_state(self) -> None:
        if not os.path.exists(self.state_file):
            return
        try:
            with open(self.state_file, encoding="utf-8") as f:
                data = json.load(f)
        except (json.JSONDecodeError, OSError) as e:
            print(f"[persist] could not read {self.state_file}: {e} — starting fresh")
            return

        for p in data.get("players", []):
            self.players[p["device_id"]] = Player(
                device_id=p["device_id"],
                name=p["name"],
                score=p.get("score", 0),
                streak=p.get("streak", 0),
                online=False,  # everyone is offline until they reconnect
            )
        self.gs.question_index = data.get("question_index", 0)
        self.gs.question_id    = data.get("question_id", 0)
        print(
            f"[persist] restored {len(self.players)} player(s) from '{self.state_file}'"
            f" — resuming at question {self.gs.question_index + 1}/{len(self.questions)}"
        )

    def _save_state(self) -> None:
        data = {
            "saved_at":       time.strftime("%Y-%m-%dT%H:%M:%S"),
            "state":          self.gs.state,
            "question_index": self.gs.question_index,
            "question_id":    self.gs.question_id,
            "players": [
                {"device_id": p.device_id, "name": p.name, "score": p.score, "streak": p.streak}
                for p in self.players.values()
            ],
        }
        tmp = self.state_file + ".tmp"
        try:
            with open(tmp, "w", encoding="utf-8") as f:
                json.dump(data, f, indent=2, ensure_ascii=False)
            os.replace(tmp, self.state_file)
        except OSError as e:
            print(f"[persist] failed to save state: {e}")

    # ── MQTT callbacks ────────────────────────────────────────────────────────

    def _on_connect(self, client, userdata, flags, reason_code, properties):
        if reason_code == 0:
            print(f"[broker] connected to {self.broker}:{self.port}")
        else:
            print(f"[broker] connection failed: {reason_code}")
            return
        client.subscribe(T_ANSWER)
        client.subscribe(T_CONNECT)
        client.subscribe(T_DISCONNECT)
        client.subscribe(T_CONTROL)
        client.subscribe(T_NAMELIST_SET)
        self._publish_state()
        self._publish_players()
        self._publish_question_sets()

    def _on_disconnect(self, client, userdata, flags, reason_code, properties):
        if reason_code == 0:
            print("[broker] disconnected cleanly")
        else:
            print(f"[broker] lost connection (rc={reason_code}), reconnecting with backoff …")

    def _on_message(self, client, userdata, msg: mqtt.MQTTMessage):
        topic   = msg.topic
        payload = msg.payload.decode("utf-8", errors="replace")

        try:
            data = json.loads(payload) if payload else {}
        except json.JSONDecodeError:
            data = {}

        if topic.startswith("quiz/connect/"):
            self._handle_connect(topic, data)
        elif topic.startswith("quiz/disconnect/"):
            self._handle_disconnect(topic)
        elif topic.startswith("quiz/answer/"):
            self._handle_answer(topic, data)
        elif topic == T_CONTROL:
            self._handle_control(data)
        elif topic == T_NAMELIST_SET:
            self._handle_namelist(data)

    # ── Device lifecycle ──────────────────────────────────────────────────────

    def _handle_connect(self, topic: str, data: dict):
        device_id = topic.split("/")[-1]
        name      = data.get("name", device_id)
        with self._lock:
            if device_id in self.players:
                self.players[device_id].online = True
                self.players[device_id].name   = name
                print(f"[reconnect] {device_id} is back as '{name}'  ({self._online_count()} online)")
            elif self.gs.state == "WAITING":
                self.players[device_id] = Player(device_id=device_id, name=name)
                print(f"[register] {device_id} → '{name}'  ({self._online_count()} online)")
                self._publish(f"{T_ACK}/{device_id}", {"status": "registered", "name": name})
            else:
                print(f"[register] {device_id} ignored — game already running")
        self._publish_players()

    def _handle_disconnect(self, topic: str):
        device_id = topic.split("/")[-1]
        with self._lock:
            if device_id in self.players:
                self.players[device_id].online = False
                print(f"[disconnect] {device_id} offline  ({self._online_count()} online)")
        self._publish_players()

    def _online_count(self) -> int:
        """Must be called with self._lock held."""
        return sum(1 for p in self.players.values() if p.online)

    # ── Control (frontend start button) ──────────────────────────────────────

    def _handle_control(self, data: dict):
        action = data.get("action")
        if action == "start":
            print("[control] start requested via MQTT")
            self.start_game()
        elif action == "restart":
            print("[control] restart requested via MQTT")
            self.restart_game()
        elif action == "reset_names":
            print("[control] device name reset requested via MQTT")
            self._publish(T_NAME_RESET, {"reset": True, "source": "beamer"})
        elif action == "load_set":
            name = str(data.get("name", ""))
            err  = self.load_set(name)
            if err:
                print(f"[control] load_set failed: {err}")

    def _handle_namelist(self, data: dict):
        """Relay the name list from the frontend to all devices (retained)."""
        names = data.get("names", [])
        if not isinstance(names, list) or len(names) == 0:
            print("[namelist] empty or invalid — ignored")
            return
        names = [str(n)[:15] for n in names[:20]]
        print(f"[namelist] relaying {len(names)} names to devices: {names}")
        # Publish on the device topic and retain it so late-joining firmware receives it once.
        self.client.publish(T_NAMELIST, json.dumps({"names": names}), retain=True)

    # ── Answer handling ───────────────────────────────────────────────────────

    def _handle_answer(self, topic: str, data: dict):
        if self.gs.state != "VOTING":
            return
        device_id   = topic.split("/")[-1]
        question_id = data.get("question_id")
        answer      = data.get("answer", "").upper()
        elapsed_ms  = data.get("elapsed_ms", 0)

        if question_id != self.gs.question_id:
            return  # stale answer
        if device_id in self.gs.answers:
            return  # already answered

        self.gs.answers[device_id] = {"answer": answer, "elapsed_ms": elapsed_ms}
        print(f"[answer] {device_id}: {answer}  ({elapsed_ms} ms)")
        with self._lock:
            total = self._online_count()
        self._publish(T_ANSWER_COUNT, {
            "question_id": self.gs.question_id,
            "count":       len(self.gs.answers),
            "total":       total,
        })

        # Wenn alle aktuell online Spieler geantwortet haben, Reveal sofort zeigen.
        if total > 0 and len(self.gs.answers) >= total:
            print("[voting] all online players answered -> reveal now")
            self._finish_voting_early()

    # ── State machine ─────────────────────────────────────────────────────────

    def _publish_state(self, remaining_s: int = 0):
        self._publish(T_STATE, {
            "state":       self.gs.state,
            "question_id": self.gs.question_id,
            "remaining_s": remaining_s,
        }, retain=True)

    def _publish_players(self):
        with self._lock:
            players = [
                {"device_id": p.device_id, "name": p.name, "online": p.online}
                for p in self.players.values()
            ]
        self._publish(T_PLAYERS, {"players": players, "min_players": MIN_PLAYERS}, retain=True)

    def _publish(self, topic: str, payload: dict, retain: bool = False):
        self.client.publish(topic, json.dumps(payload), retain=retain)

    def start_game(self):
        """Advance from WAITING → first QUESTION."""
        if self.gs.state != "WAITING":
            print("[game] already running")
            return
        with self._lock:
            online = self._online_count()
        if online < MIN_PLAYERS:
            print(f"[game] only {online} player(s) online, need {MIN_PLAYERS}")
            return
        print(f"[game] starting with {online} online players")
        self.gs.question_index = 0
        self._transition_to_question()

    def _transition_to_question(self):
        if self.gs.question_index >= len(self.questions):
            self._transition_to_ended()
            return

        q = self.questions[self.gs.question_index]
        self.gs.state          = "QUESTION"
        self.gs.question_id   += 1
        self.gs.question_start = time.monotonic()
        self.gs.answers        = {}

        q_type  = q.get("type", "mcq")
        payload: dict = {
            "id":           self.gs.question_id,
            "total":        len(self.questions),
            "type":         q_type,
            "text":         q["text"],
            "time_limit_s": q["time_limit_s"],
        }
        if q_type == "estimate":
            payload["min"]  = q["min"]
            payload["max"]  = q["max"]
            payload["unit"] = q.get("unit", "")
        elif q_type == "higher_lower":
            payload["reference"] = q["reference"]
            payload["unit"]      = q.get("unit", "")
        elif q_type == "poti_target":
            payload["target"]    = q["target"]
            payload["tolerance"] = q.get("tolerance", 5)
        elif q_type == "temp_target":
            payload["target"]    = q["target"]
            payload["tolerance"] = q.get("tolerance", 1.5)
        else:
            payload["options"] = q["options"]
        self._publish(T_QUESTION, payload)
        self._publish_state(remaining_s=q["time_limit_s"])
        print(f"[question {self.gs.question_id}] {q['text']}")

        self._set_timer(3, self._transition_to_voting)

    def _transition_to_voting(self):
        q = self.questions[self.gs.question_index]
        self.gs.state          = "VOTING"
        self.gs.question_start = time.monotonic()
        self._publish_state(remaining_s=q["time_limit_s"])
        print(f"[voting] open for {q['time_limit_s']}s")
        self._set_timer(q["time_limit_s"], self._transition_to_reveal)

    def _finish_voting_early(self):
        if self.gs.state != "VOTING":
            return
        if self._timer:
            self._timer.cancel()
            self._timer = None
        self._transition_to_reveal()

    def _transition_to_reveal(self):
        if self.gs.state != "VOTING":
            return
        q            = self.questions[self.gs.question_index]
        q_type       = q.get("type", "mcq")
        time_limit_ms = q["time_limit_s"] * 1000
        with self._lock:
            players_snapshot = dict(self.players)

        # ── mark no-answer players streak=0 ──────────────────────────────────
        for device_id, player in players_snapshot.items():
            if device_id not in self.gs.answers:
                player.streak = 0

        self.gs.state = "REVEAL"

        if q_type == "estimate":
            self._reveal_estimate(q, time_limit_ms, players_snapshot)
        elif q_type == "higher_lower":
            self._reveal_higher_lower(q, time_limit_ms, players_snapshot)
        elif q_type == "poti_target":
            self._reveal_poti_target(q, players_snapshot)
        elif q_type == "temp_target":
            self._reveal_temp_target(q, players_snapshot)
        else:
            self._reveal_mcq(q, time_limit_ms, players_snapshot)

        self._publish_state()
        self._set_timer(5, self._transition_to_scores)

    # ── MCQ reveal ────────────────────────────────────────────────────────────
    def _reveal_mcq(self, q: dict, time_limit_ms: int, players: dict):
        correct = q["correct"].upper()
        counts: dict[str, int] = {k: 0 for k in q["options"]}
        for ans in self.gs.answers.values():
            if ans["answer"] in counts:
                counts[ans["answer"]] += 1

        for device_id, ans in self.gs.answers.items():
            if device_id not in players:
                continue
            player = players[device_id]
            if ans["answer"] == correct:
                player.streak += 1
                elapsed      = min(ans["elapsed_ms"], time_limit_ms)
                time_bonus   = round(TIME_BONUS * (1 - elapsed / time_limit_ms))
                streak_level = min(player.streak - 1, MAX_STREAK_LEVELS)
                streak_bonus = streak_level * STREAK_BONUS_PER_LVL
                player.score += BASE_SCORE + time_bonus + streak_bonus
                if streak_bonus:
                    print(f"  streak ×{player.streak} for {device_id} → +{streak_bonus} bonus")
            else:
                player.streak = 0

        self._publish(T_REVEAL, {
            "question_id": self.gs.question_id,
            "correct":     correct,
            "counts":      counts,
        })
        print(f"[reveal/mcq] correct={correct}  counts={counts}")

    # ── Estimate reveal ───────────────────────────────────────────────────────
    def _reveal_estimate(self, q: dict, time_limit_ms: int, players: dict):
        correct  = int(q["correct"])
        rng      = max(q["max"] - q["min"], 1)
        answers_out = []

        for device_id, ans in self.gs.answers.items():
            if device_id not in players:
                continue
            player = players[device_id]
            try:
                guess = int(ans["answer"])
            except (ValueError, TypeError):
                player.streak = 0
                continue

            delta        = abs(guess - correct)
            rel_err      = delta / rng
            base         = 0
            for threshold, pts in ESTIMATE_TIERS:
                if rel_err <= threshold:
                    base = pts
                    break
            if base > 0:
                elapsed    = min(ans["elapsed_ms"], time_limit_ms)
                time_bonus = round(TIME_BONUS * (1 - elapsed / time_limit_ms))
                player.score += base + time_bonus
                player.streak += 1
            else:
                player.streak = 0

            answers_out.append({
                "device_id": device_id,
                "name":      player.name,
                "value":     guess,
                "delta":     delta,
            })
            print(f"  {device_id}: guess={guess}  delta={delta}  pts={base}")

        answers_out.sort(key=lambda x: x["delta"])
        self._publish(T_REVEAL, {
            "question_id": self.gs.question_id,
            "type":        "estimate",
            "correct":     correct,
            "unit":        q.get("unit", ""),
            "answers":     answers_out,
        })
        print(f"[reveal/estimate] correct={correct}")

    # ── Higher / Lower reveal ─────────────────────────────────────────────────
    def _reveal_higher_lower(self, q: dict, time_limit_ms: int, players: dict):
        correct = q["correct"].upper()   # "HIGHER" or "LOWER"
        counts  = {"HIGHER": 0, "LOWER": 0}

        for ans in self.gs.answers.values():
            key = ans["answer"].upper()
            if key in counts:
                counts[key] += 1

        for device_id, ans in self.gs.answers.items():
            if device_id not in players:
                continue
            player = players[device_id]
            if ans["answer"].upper() == correct:
                player.streak += 1
                elapsed      = min(ans["elapsed_ms"], time_limit_ms)
                time_bonus   = round(TIME_BONUS * (1 - elapsed / time_limit_ms))
                streak_level = min(player.streak - 1, MAX_STREAK_LEVELS)
                streak_bonus = streak_level * STREAK_BONUS_PER_LVL
                player.score += BASE_SCORE + time_bonus + streak_bonus
            else:
                player.streak = 0

        self._publish(T_REVEAL, {
            "question_id": self.gs.question_id,
            "type":        "higher_lower",
            "correct":     correct,
            "actual":      q["actual"],
            "unit":        q.get("unit", ""),
            "counts":      counts,
        })
        print(f"[reveal/higher_lower] correct={correct}  actual={q['actual']}  counts={counts}")

    # ── Poti Target reveal ────────────────────────────────────────────────────
    def _reveal_poti_target(self, q: dict, players: dict):
        correct   = int(q["target"])
        tolerance = int(q.get("tolerance", 5))
        answers_out = []

        for device_id, ans in self.gs.answers.items():
            if device_id not in players:
                continue
            player = players[device_id]
            try:
                guess = int(ans["answer"])
            except (ValueError, KeyError):
                player.streak = 0
                continue
            delta  = abs(guess - correct)
            earned = 0
            if delta <= tolerance:
                # Score: full points at exact, linear decay to 0 at tolerance boundary
                earned = round(BASE_SCORE * (1 - delta / max(tolerance, 1)))
                player.streak += 1
            else:
                player.streak = 0
            player.score += earned
            answers_out.append({
                "device_id": device_id,
                "name":      player.name,
                "value":     guess,
                "delta":     delta,
            })

        answers_out.sort(key=lambda x: x["delta"])
        self._publish(T_REVEAL, {
            "question_id": self.gs.question_id,
            "type":        "poti_target",
            "correct":     correct,
            "tolerance":   tolerance,
            "answers":     answers_out,
        })
        print(f"[reveal/poti_target] correct={correct}%  tolerance=±{tolerance}%")

    # ── Temp Target reveal ────────────────────────────────────────────────────
    def _reveal_temp_target(self, q: dict, players: dict):
        correct   = float(q["target"])
        tolerance = float(q.get("tolerance", 1.5))
        answers_out = []

        for device_id, ans in self.gs.answers.items():
            if device_id not in players:
                continue
            player = players[device_id]
            try:
                guess = float(ans["answer"])
            except (ValueError, KeyError):
                player.streak = 0
                continue
            delta  = abs(guess - correct)
            earned = 0
            if delta <= tolerance:
                earned = round(BASE_SCORE * (1 - delta / max(tolerance, 1)))
                player.streak += 1
            else:
                player.streak = 0
            player.score += earned
            answers_out.append({
                "device_id": device_id,
                "name":      player.name,
                "value":     round(guess, 1),
                "delta":     round(delta, 1),
            })

        answers_out.sort(key=lambda x: x["delta"])
        self._publish(T_REVEAL, {
            "question_id": self.gs.question_id,
            "type":        "temp_target",
            "correct":     correct,
            "tolerance":   tolerance,
            "answers":     answers_out,
        })
        print(f"[reveal/temp_target] correct={correct}°C  tolerance=±{tolerance}°C")

    def _transition_to_scores(self):
        self.gs.state = "SCORES"
        with self._lock:
            scoreboard = sorted(
                [{"device_id": p.device_id, "name": p.name, "score": p.score, "streak": p.streak}
                 for p in self.players.values()],
                key=lambda x: x["score"],
                reverse=True,
            )
        self._publish(T_SCORES, {"scores": scoreboard})
        self._publish_state()
        print(f"[scores] {scoreboard}")

        self.gs.question_index += 1
        self._save_state()
        if self.gs.question_index < len(self.questions):
            self._set_timer(5, self._transition_to_question)
        else:
            self._set_timer(5, self._transition_to_ended)

    def _transition_to_ended(self):
        self.gs.state = "ENDED"
        self._publish_state()
        self._save_state()
        print("[game] ended")

    def restart_game(self):
        """Reset scores and state back to WAITING so a new game can start."""
        if self.gs.state not in ("ENDED", "WAITING"):
            print(f"[game] restart ignored — state is {self.gs.state}")
            return
        if self._timer:
            self._timer.cancel()
            self._timer = None
        with self._lock:
            for p in self.players.values():
                p.score  = 0
                p.streak = 0
        self.gs = GameState()  # fresh state, question_index=0, question_id=0
        print("[game] restarted — back to WAITING")
        self._publish_state()
        self._publish_players()
        self._save_state()

    # ── Timer helpers ─────────────────────────────────────────────────────────

    def _set_timer(self, seconds: float, callback):
        if self._timer:
            self._timer.cancel()
        self._timer = threading.Timer(seconds, callback)
        self._timer.daemon = True
        self._timer.start()

    # ── Run ───────────────────────────────────────────────────────────────────

    def run(self):
        self._start_api_server()
        print("AALeC Quiz — Game Master")
        print(f"  Broker   : {self.broker}:{self.port}")
        print(f"  Questions: {len(self.questions)}")
        print(f"  Min players to start: {MIN_PLAYERS}")
        print("Start the game via the beamer lobby UI or publish to quiz/control.")
        print("Press Ctrl+C to quit.")

        try:
            self.client.connect(self.broker, self.port, keepalive=60)
        except (OSError, ConnectionRefusedError) as e:
            print(f"[broker] initial connection failed ({e}) — will retry automatically …")

        try:
            # loop_forever blocks and handles reconnection on unexpected disconnects.
            # retry_first_connection=True also covers a failed initial connect().
            self.client.loop_forever(retry_first_connection=True)
        except KeyboardInterrupt:
            pass

        if self._timer:
            self._timer.cancel()
        self.client.disconnect()
        print("Bye.")


# ── Entry point ───────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(description="AALeC Quiz Game Master")
    parser.add_argument("--broker",        default="localhost",       help="MQTT broker host")
    parser.add_argument("--port",          default=1883,  type=int,   help="MQTT broker port")
    parser.add_argument("--questions",     default="questions.json",  help="Path to questions JSON file")
    parser.add_argument("--questions-dir", default=None,              help="Directory of question-set JSON files (default: same dir as --questions)")
    parser.add_argument("--state-file",    default="game_state.json", help="Path to persisted game state")
    parser.add_argument("--api-port",      default=8080,  type=int,   help="Admin REST API port")
    args = parser.parse_args()

    q_path        = Path(args.questions)
    questions_dir = args.questions_dir if args.questions_dir else str(q_path.parent or ".")
    active_set    = q_path.stem

    try:
        with open(q_path, encoding="utf-8") as f:
            questions = json.load(f)
    except FileNotFoundError:
        sys.exit(f"ERROR: questions file not found: {str(q_path)!r}")
    except json.JSONDecodeError as e:
        sys.exit(f"ERROR: questions file is not valid JSON: {e}")

    validate_questions(questions)
    GameMaster(
        broker=args.broker,
        port=args.port,
        questions=questions,
        state_file=args.state_file,
        questions_dir=questions_dir,
        active_set=active_set,
        api_port=args.api_port,
    ).run()


if __name__ == "__main__":
    main()

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
import threading
import time
from dataclasses import dataclass, field
from typing import Optional

import paho.mqtt.client as mqtt

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
    def __init__(self, broker: str, port: int, questions: list[dict]):
        self.broker    = broker
        self.port      = port
        self.questions = questions
        self.players: dict[str, Player] = {}
        self.gs        = GameState()
        self._timer: Optional[threading.Timer] = None
        self._lock     = threading.Lock()

        self.client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
        self.client.on_connect    = self._on_connect
        self.client.on_message    = self._on_message
        self.client.on_disconnect = self._on_disconnect

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
        self._publish_state()
        self._publish_players()

    def _on_disconnect(self, client, userdata, flags, reason_code, properties):
        print(f"[broker] disconnected (rc={reason_code}), reconnecting …")

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

    # ── Device lifecycle ──────────────────────────────────────────────────────

    def _handle_connect(self, topic: str, data: dict):
        device_id = topic.split("/")[-1]
        name      = data.get("name", device_id)
        with self._lock:
            if device_id in self.players:
                self.players[device_id].online = True
                print(f"[reconnect] {device_id} is back  ({self._online_count()} online)")
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
        if self.gs.question_index < len(self.questions):
            self._set_timer(5, self._transition_to_question)
        else:
            self._set_timer(5, self._transition_to_ended)

    def _transition_to_ended(self):
        self.gs.state = "ENDED"
        self._publish_state()
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

    # ── Timer helpers ─────────────────────────────────────────────────────────

    def _set_timer(self, seconds: float, callback):
        if self._timer:
            self._timer.cancel()
        self._timer = threading.Timer(seconds, callback)
        self._timer.daemon = True
        self._timer.start()

    # ── Run ───────────────────────────────────────────────────────────────────

    def run(self):
        self.client.connect(self.broker, self.port, keepalive=60)
        self.client.loop_start()

        print("AALeC Quiz — Game Master")
        print(f"  Broker   : {self.broker}:{self.port}")
        print(f"  Questions: {len(self.questions)}")
        print(f"  Min players to start: {MIN_PLAYERS}")
        print("Start the game via the beamer lobby UI or publish to quiz/control.")
        print("Press Ctrl+C to quit.")

        try:
            while True:
                time.sleep(1)
        except KeyboardInterrupt:
            pass

        if self._timer:
            self._timer.cancel()
        self.client.loop_stop()
        self.client.disconnect()
        print("Bye.")


# ── Entry point ───────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(description="AALeC Quiz Game Master")
    parser.add_argument("--broker",    default="localhost",      help="MQTT broker host")
    parser.add_argument("--port",      default=1883, type=int,   help="MQTT broker port")
    parser.add_argument("--questions", default="questions.json", help="Path to questions JSON")
    args = parser.parse_args()

    with open(args.questions, encoding="utf-8") as f:
        questions = json.load(f)

    GameMaster(broker=args.broker, port=args.port, questions=questions).run()


if __name__ == "__main__":
    main()

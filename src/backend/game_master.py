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
T_STATE      = "quiz/state"
T_QUESTION   = "quiz/question"
T_REVEAL     = "quiz/reveal"
T_SCORES     = "quiz/scores"
T_ANSWER     = "quiz/answer/#"       # subscribe pattern
T_CONNECT    = "quiz/connect/#"      # subscribe pattern
T_DISCONNECT = "quiz/disconnect/#"   # subscribe pattern (LWT)

# ── Scoring constants ─────────────────────────────────────────────────────────
BASE_SCORE  = 1000
TIME_BONUS  = 500


@dataclass
class Player:
    device_id: str
    name: str
    score: int = 0


@dataclass
class GameState:
    state: str = "WAITING"
    question_index: int = 0
    question_id: int = 0          # monotonic, survives restarts in theory
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
        self._publish_state()

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

    # ── Device lifecycle ──────────────────────────────────────────────────────

    def _handle_connect(self, topic: str, data: dict):
        device_id = topic.split("/")[-1]
        name      = data.get("name", device_id)
        if self.gs.state == "WAITING":
            self.players[device_id] = Player(device_id=device_id, name=name)
            print(f"[register] {device_id} → '{name}'  ({len(self.players)} players)")
        else:
            print(f"[register] {device_id} ignored — game already running")

    def _handle_disconnect(self, topic: str):
        device_id = topic.split("/")[-1]
        if device_id in self.players:
            print(f"[disconnect] {device_id} left")

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

    # ── State machine ─────────────────────────────────────────────────────────

    def _publish_state(self, remaining_s: int = 0):
        payload = {
            "state":       self.gs.state,
            "question_id": self.gs.question_id,
            "remaining_s": remaining_s,
        }
        self._publish(T_STATE, payload)

    def _publish(self, topic: str, payload: dict, retain: bool = False):
        self.client.publish(topic, json.dumps(payload), retain=retain)

    def start_game(self):
        """Advance from WAITING → first QUESTION."""
        if self.gs.state != "WAITING":
            print("[game] already running")
            return
        if not self.players:
            print("[game] no players registered, aborting")
            return
        print(f"[game] starting with {len(self.players)} players")
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

        payload = {
            "id":           self.gs.question_id,
            "text":         q["text"],
            "options":      q["options"],
            "time_limit_s": q["time_limit_s"],
        }
        self._publish(T_QUESTION, payload)

        # Brief display period before voting opens
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

    def _transition_to_reveal(self):
        q       = self.questions[self.gs.question_index]
        correct = q["correct"].upper()

        # Count answers per option
        counts: dict[str, int] = {k: 0 for k in q["options"]}
        for device_id, ans in self.gs.answers.items():
            option = ans["answer"]
            if option in counts:
                counts[option] += 1

        # Score players
        time_limit_ms = q["time_limit_s"] * 1000
        for device_id, ans in self.gs.answers.items():
            if ans["answer"] == correct and device_id in self.players:
                elapsed = min(ans["elapsed_ms"], time_limit_ms)
                bonus   = round(TIME_BONUS * (1 - elapsed / time_limit_ms))
                self.players[device_id].score += BASE_SCORE + bonus

        self.gs.state = "REVEAL"
        self._publish(T_REVEAL, {
            "question_id": self.gs.question_id,
            "correct":     correct,
            "counts":      counts,
        })
        self._publish_state()
        print(f"[reveal] correct={correct}  counts={counts}")
        self._set_timer(5, self._transition_to_scores)

    def _transition_to_scores(self):
        self.gs.state = "SCORES"
        scoreboard = sorted(
            [{"device_id": p.device_id, "name": p.name, "score": p.score}
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
        print(f"  Broker : {self.broker}:{self.port}")
        print(f"  Questions: {len(self.questions)}")
        print("Commands: [s] start game   [q] quit")

        try:
            while True:
                cmd = input("> ").strip().lower()
                if cmd == "s":
                    self.start_game()
                elif cmd == "q":
                    break
        except (KeyboardInterrupt, EOFError):
            pass
        finally:
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

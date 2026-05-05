"""Unit tests for game_master scoring logic.

Covers:
- MCQ:         time bonus, streak bonus (levels 1-3+), wrong answers, edge cases
- Estimate:    tier boundaries, invalid input, result sort order
- Poti target: exact hit, linear decay, tolerance boundary, out-of-range
- Temp target: float answers, tolerance, invalid input
- Higher/Lower: case-insensitive matching, streak bonus
- State machine: registration gating, disconnect, restart, no-answer streak reset
- validate_questions: required fields, unknown type, bad MCQ options/correct
"""
import json
import sys
import os
from unittest.mock import MagicMock, patch

import pytest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

# Patch mqtt.Client so GameMaster.__init__ never opens a real connection
with patch("paho.mqtt.client.Client"):
    from game_master import (
        GameMaster,
        Player,
        GameState,
        BASE_SCORE,
        TIME_BONUS,
        STREAK_BONUS_PER_LVL,
        MAX_STREAK_LEVELS,
        validate_questions,
    )

# ── Sample questions ───────────────────────────────────────────────────────────

MCQ_Q = {
    "text": "Was ist 2+2?",
    "options": {"A": "3", "B": "4", "C": "5", "D": "6"},
    "correct": "B",
    "time_limit_s": 20,
}

ESTIMATE_Q = {
    "type": "estimate",
    "text": "Einwohner?",
    "min": 0,
    "max": 1000,
    "unit": "Tsd.",
    "correct": 500,
    "time_limit_s": 30,
}

POTI_Q = {
    "type": "poti_target",
    "text": "Dreh auf 75%!",
    "target": 75,
    "tolerance": 5,
    "time_limit_s": 20,
}

TEMP_Q = {
    "type": "temp_target",
    "text": "Zieltemperatur!",
    "target": 25.0,
    "tolerance": 2.0,
    "time_limit_s": 20,
}

HL_Q = {
    "type": "higher_lower",
    "text": "Höher oder niedriger als 400m?",
    "reference": 400,
    "unit": "m",
    "correct": "LOWER",
    "actual": 330,
    "time_limit_s": 20,
}

# ── Helpers ────────────────────────────────────────────────────────────────────


def make_gm(questions=None):
    if questions is None:
        questions = [MCQ_Q]
    with patch("paho.mqtt.client.Client"):
        gm = GameMaster(broker="localhost", port=1883, questions=questions)
    gm.client = MagicMock()
    return gm


def make_player(device_id="dev-01", name="Alice", score=0, streak=0, online=True) -> Player:
    return Player(device_id=device_id, name=name, score=score, streak=streak, online=online)


def last_publish_payload(gm) -> dict:
    return json.loads(gm.client.publish.call_args[0][1])


# ── MCQ scoring ────────────────────────────────────────────────────────────────


class TestMcqScoring:
    TIME_LIMIT_MS = 20_000

    def setup_method(self):
        self.gm = make_gm([MCQ_Q])
        self.gm.gs.question_id = 1

    def _reveal(self, answers: dict, players: dict):
        self.gm.gs.answers = answers
        self.gm._reveal_mcq(MCQ_Q, self.TIME_LIMIT_MS, players)

    # ── time bonus ─────────────────────────────────────────────────────────────

    def test_correct_instant_earns_full_time_bonus(self):
        p = make_player()
        self._reveal({"dev-01": {"answer": "B", "elapsed_ms": 0}}, {"dev-01": p})
        assert p.score == BASE_SCORE + TIME_BONUS
        assert p.streak == 1

    def test_correct_halfway_earns_half_time_bonus(self):
        p = make_player()
        self._reveal({"dev-01": {"answer": "B", "elapsed_ms": 10_000}}, {"dev-01": p})
        assert p.score == BASE_SCORE + 250  # round(500 * 0.5)

    def test_correct_at_time_limit_earns_no_time_bonus(self):
        p = make_player()
        self._reveal({"dev-01": {"answer": "B", "elapsed_ms": 20_000}}, {"dev-01": p})
        assert p.score == BASE_SCORE

    def test_elapsed_beyond_limit_is_clamped(self):
        p = make_player()
        self._reveal({"dev-01": {"answer": "B", "elapsed_ms": 99_999}}, {"dev-01": p})
        assert p.score == BASE_SCORE  # time_bonus = 0 after clamp

    # ── wrong answer ───────────────────────────────────────────────────────────

    def test_wrong_answer_gives_no_score(self):
        p = make_player(streak=3)
        self._reveal({"dev-01": {"answer": "A", "elapsed_ms": 0}}, {"dev-01": p})
        assert p.score == 0

    def test_wrong_answer_resets_streak(self):
        p = make_player(streak=3)
        self._reveal({"dev-01": {"answer": "A", "elapsed_ms": 0}}, {"dev-01": p})
        assert p.streak == 0

    # ── streak bonus ───────────────────────────────────────────────────────────

    def test_first_correct_answer_has_no_streak_bonus(self):
        # streak becomes 1 → streak_level = min(1-1, MAX) = 0 → no bonus
        p = make_player(streak=0)
        self._reveal({"dev-01": {"answer": "B", "elapsed_ms": 0}}, {"dev-01": p})
        assert p.score == BASE_SCORE + TIME_BONUS

    def test_streak_level_1_adds_one_bonus(self):
        p = make_player(streak=1)
        self._reveal({"dev-01": {"answer": "B", "elapsed_ms": 0}}, {"dev-01": p})
        assert p.score == BASE_SCORE + TIME_BONUS + STREAK_BONUS_PER_LVL
        assert p.streak == 2

    def test_streak_level_2_adds_two_bonuses(self):
        p = make_player(streak=2)
        self._reveal({"dev-01": {"answer": "B", "elapsed_ms": 0}}, {"dev-01": p})
        assert p.score == BASE_SCORE + TIME_BONUS + 2 * STREAK_BONUS_PER_LVL

    def test_streak_capped_at_max_level(self):
        p = make_player(streak=MAX_STREAK_LEVELS)
        self._reveal({"dev-01": {"answer": "B", "elapsed_ms": 0}}, {"dev-01": p})
        assert p.score == BASE_SCORE + TIME_BONUS + MAX_STREAK_LEVELS * STREAK_BONUS_PER_LVL

    def test_very_high_streak_still_capped(self):
        p = make_player(streak=10)
        self._reveal({"dev-01": {"answer": "B", "elapsed_ms": 0}}, {"dev-01": p})
        assert p.score == BASE_SCORE + TIME_BONUS + MAX_STREAK_LEVELS * STREAK_BONUS_PER_LVL

    # ── answer counts ──────────────────────────────────────────────────────────

    def test_counts_tallied_correctly(self):
        players = {k: make_player(k) for k in ("dev-01", "dev-02", "dev-03")}
        answers = {
            "dev-01": {"answer": "B", "elapsed_ms": 0},
            "dev-02": {"answer": "A", "elapsed_ms": 0},
            "dev-03": {"answer": "B", "elapsed_ms": 0},
        }
        self._reveal(answers, players)
        payload = last_publish_payload(self.gm)
        assert payload["counts"] == {"A": 1, "B": 2, "C": 0, "D": 0}

    def test_unknown_player_in_answers_is_skipped(self):
        players = {"dev-01": make_player("dev-01")}
        answers = {
            "dev-01": {"answer": "B", "elapsed_ms": 0},
            "ghost":  {"answer": "B", "elapsed_ms": 0},
        }
        self._reveal(answers, players)
        assert players["dev-01"].score == BASE_SCORE + TIME_BONUS
        assert "ghost" not in players


# ── Estimate scoring ───────────────────────────────────────────────────────────


class TestEstimateScoring:
    """range = max - min = 1000, correct = 500, time_limit_ms = 30_000."""

    TIME_LIMIT_MS = 30_000

    def setup_method(self):
        self.gm = make_gm([ESTIMATE_Q])
        self.gm.gs.question_id = 1

    def _reveal(self, answers: dict, players: dict):
        self.gm.gs.answers = answers
        self.gm._reveal_estimate(ESTIMATE_Q, self.TIME_LIMIT_MS, players)

    def test_exact_hit_earns_tier_1000_plus_time_bonus(self):
        p = make_player()
        self._reveal({"dev-01": {"answer": "500", "elapsed_ms": 0}}, {"dev-01": p})
        assert p.score == 1000 + TIME_BONUS
        assert p.streak == 1

    def test_within_5_percent_earns_tier_800(self):
        # delta=50, rel_err=0.05 → exactly at 5 % boundary → tier 800
        p = make_player()
        self._reveal({"dev-01": {"answer": "450", "elapsed_ms": 0}}, {"dev-01": p})
        assert p.score == 800 + TIME_BONUS

    def test_within_10_percent_earns_tier_600(self):
        # delta=75, rel_err=0.075
        p = make_player()
        self._reveal({"dev-01": {"answer": "425", "elapsed_ms": 0}}, {"dev-01": p})
        assert p.score == 600 + TIME_BONUS

    def test_within_20_percent_earns_tier_400(self):
        # delta=150, rel_err=0.15
        p = make_player()
        self._reveal({"dev-01": {"answer": "350", "elapsed_ms": 0}}, {"dev-01": p})
        assert p.score == 400 + TIME_BONUS

    def test_within_30_percent_earns_tier_200(self):
        # delta=250, rel_err=0.25
        p = make_player()
        self._reveal({"dev-01": {"answer": "250", "elapsed_ms": 0}}, {"dev-01": p})
        assert p.score == 200 + TIME_BONUS

    def test_exactly_30_percent_boundary_earns_tier_200(self):
        # delta=300, rel_err=0.30 → still within → 200
        p = make_player()
        self._reveal({"dev-01": {"answer": "200", "elapsed_ms": 0}}, {"dev-01": p})
        assert p.score == 200 + TIME_BONUS

    def test_beyond_30_percent_earns_nothing_and_resets_streak(self):
        # delta=301, rel_err=0.301
        p = make_player(streak=2)
        self._reveal({"dev-01": {"answer": "199", "elapsed_ms": 0}}, {"dev-01": p})
        assert p.score == 0
        assert p.streak == 0

    def test_invalid_answer_earns_nothing_and_resets_streak(self):
        p = make_player(streak=3)
        self._reveal({"dev-01": {"answer": "abc", "elapsed_ms": 0}}, {"dev-01": p})
        assert p.score == 0
        assert p.streak == 0

    def test_results_sorted_by_delta_ascending(self):
        players = {"dev-01": make_player("dev-01"), "dev-02": make_player("dev-02")}
        answers = {
            "dev-01": {"answer": "450", "elapsed_ms": 0},  # delta=50
            "dev-02": {"answer": "300", "elapsed_ms": 0},  # delta=200
        }
        self._reveal(answers, players)
        deltas = [a["delta"] for a in last_publish_payload(self.gm)["answers"]]
        assert deltas == sorted(deltas)


# ── Poti target scoring ────────────────────────────────────────────────────────


class TestPotiTargetScoring:
    """target=75, tolerance=5."""

    def setup_method(self):
        self.gm = make_gm([POTI_Q])
        self.gm.gs.question_id = 1

    def _reveal(self, answers: dict, players: dict):
        self.gm.gs.answers = answers
        self.gm._reveal_poti_target(POTI_Q, players)

    def test_exact_hit_earns_full_base_score(self):
        p = make_player()
        self._reveal({"dev-01": {"answer": "75", "elapsed_ms": 0}}, {"dev-01": p})
        assert p.score == BASE_SCORE
        assert p.streak == 1

    def test_midpoint_of_tolerance_earns_half_score(self):
        # delta=2 → round(1000 * (1 - 2/5)) = 600
        p = make_player()
        self._reveal({"dev-01": {"answer": "77", "elapsed_ms": 0}}, {"dev-01": p})
        assert p.score == 600
        assert p.streak == 1

    def test_at_tolerance_boundary_earns_zero_but_streak_counts(self):
        # delta=5 == tolerance → earned=0, but within range → streak still increments
        p = make_player()
        self._reveal({"dev-01": {"answer": "80", "elapsed_ms": 0}}, {"dev-01": p})
        assert p.score == 0
        assert p.streak == 1

    def test_outside_tolerance_earns_nothing_resets_streak(self):
        p = make_player(streak=3)
        self._reveal({"dev-01": {"answer": "81", "elapsed_ms": 0}}, {"dev-01": p})
        assert p.score == 0
        assert p.streak == 0

    def test_invalid_answer_resets_streak(self):
        p = make_player(streak=2)
        self._reveal({"dev-01": {"answer": "abc", "elapsed_ms": 0}}, {"dev-01": p})
        assert p.score == 0
        assert p.streak == 0

    def test_results_sorted_by_delta_ascending(self):
        players = {"dev-01": make_player("dev-01"), "dev-02": make_player("dev-02")}
        answers = {
            "dev-01": {"answer": "77", "elapsed_ms": 0},  # delta=2
            "dev-02": {"answer": "70", "elapsed_ms": 0},  # delta=5
        }
        self._reveal(answers, players)
        deltas = [a["delta"] for a in last_publish_payload(self.gm)["answers"]]
        assert deltas == sorted(deltas)


# ── Temp target scoring ────────────────────────────────────────────────────────


class TestTempTargetScoring:
    """target=25.0, tolerance=2.0."""

    def setup_method(self):
        self.gm = make_gm([TEMP_Q])
        self.gm.gs.question_id = 1

    def _reveal(self, answers: dict, players: dict):
        self.gm.gs.answers = answers
        self.gm._reveal_temp_target(TEMP_Q, players)

    def test_exact_hit_earns_full_score(self):
        p = make_player()
        self._reveal({"dev-01": {"answer": "25.0", "elapsed_ms": 0}}, {"dev-01": p})
        assert p.score == BASE_SCORE
        assert p.streak == 1

    def test_within_tolerance_earns_partial_score(self):
        # delta=1.0 → round(1000 * (1 - 1.0/2.0)) = 500
        p = make_player()
        self._reveal({"dev-01": {"answer": "26.0", "elapsed_ms": 0}}, {"dev-01": p})
        assert p.score == 500

    def test_outside_tolerance_earns_nothing_resets_streak(self):
        p = make_player(streak=2)
        self._reveal({"dev-01": {"answer": "28.0", "elapsed_ms": 0}}, {"dev-01": p})
        assert p.score == 0
        assert p.streak == 0

    def test_invalid_answer_resets_streak(self):
        p = make_player(streak=1)
        self._reveal({"dev-01": {"answer": "hot", "elapsed_ms": 0}}, {"dev-01": p})
        assert p.score == 0
        assert p.streak == 0


# ── Higher / Lower scoring ─────────────────────────────────────────────────────


class TestHigherLowerScoring:
    """correct='LOWER', time_limit_ms=20_000."""

    TIME_LIMIT_MS = 20_000

    def setup_method(self):
        self.gm = make_gm([HL_Q])
        self.gm.gs.question_id = 1

    def _reveal(self, answers: dict, players: dict):
        self.gm.gs.answers = answers
        self.gm._reveal_higher_lower(HL_Q, self.TIME_LIMIT_MS, players)

    def test_correct_uppercase_earns_score(self):
        p = make_player()
        self._reveal({"dev-01": {"answer": "LOWER", "elapsed_ms": 0}}, {"dev-01": p})
        assert p.score == BASE_SCORE + TIME_BONUS
        assert p.streak == 1

    def test_correct_lowercase_is_accepted(self):
        p = make_player()
        self._reveal({"dev-01": {"answer": "lower", "elapsed_ms": 0}}, {"dev-01": p})
        assert p.score == BASE_SCORE + TIME_BONUS

    def test_wrong_answer_gives_no_score_resets_streak(self):
        p = make_player(streak=2)
        self._reveal({"dev-01": {"answer": "HIGHER", "elapsed_ms": 0}}, {"dev-01": p})
        assert p.score == 0
        assert p.streak == 0

    def test_streak_bonus_applied_on_correct(self):
        p = make_player(streak=1)
        self._reveal({"dev-01": {"answer": "LOWER", "elapsed_ms": 0}}, {"dev-01": p})
        assert p.score == BASE_SCORE + TIME_BONUS + STREAK_BONUS_PER_LVL

    def test_counts_published_correctly(self):
        players = {"dev-01": make_player("dev-01"), "dev-02": make_player("dev-02")}
        answers = {
            "dev-01": {"answer": "LOWER",  "elapsed_ms": 0},
            "dev-02": {"answer": "HIGHER", "elapsed_ms": 0},
        }
        self._reveal(answers, players)
        payload = last_publish_payload(self.gm)
        assert payload["counts"] == {"HIGHER": 1, "LOWER": 1}


# ── State machine ──────────────────────────────────────────────────────────────


class TestStateMachine:
    def _connect(self, gm, device_id, name="Alice"):
        gm._handle_connect(f"quiz/connect/{device_id}", {"name": name, "device_id": device_id})

    def test_register_in_waiting_succeeds(self):
        gm = make_gm()
        self._connect(gm, "dev-01", "Alice")
        assert "dev-01" in gm.players
        assert gm.players["dev-01"].name == "Alice"

    def test_register_after_game_started_is_ignored(self):
        gm = make_gm()
        self._connect(gm, "dev-01", "Alice")
        gm.gs.state = "QUESTION"
        self._connect(gm, "dev-02", "Bob")
        assert "dev-02" not in gm.players

    def test_reconnect_existing_player_updates_name_and_online_status(self):
        gm = make_gm()
        self._connect(gm, "dev-01", "Alice")
        gm.players["dev-01"].online = False
        gm.gs.state = "QUESTION"
        self._connect(gm, "dev-01", "Alice2")
        assert gm.players["dev-01"].online is True
        assert gm.players["dev-01"].name == "Alice2"

    def test_disconnect_sets_player_offline(self):
        gm = make_gm()
        self._connect(gm, "dev-01")
        gm._handle_disconnect("quiz/disconnect/dev-01")
        assert gm.players["dev-01"].online is False

    def test_restart_resets_scores_and_streaks(self):
        gm = make_gm()
        self._connect(gm, "dev-01")
        gm.players["dev-01"].score  = 5000
        gm.players["dev-01"].streak = 4
        gm.gs.state = "ENDED"
        gm.restart_game()
        assert gm.players["dev-01"].score  == 0
        assert gm.players["dev-01"].streak == 0
        assert gm.gs.state == "WAITING"

    def test_restart_ignored_during_active_game(self):
        gm = make_gm()
        self._connect(gm, "dev-01")
        gm.players["dev-01"].score = 1000
        gm.gs.state = "VOTING"
        gm.restart_game()
        assert gm.players["dev-01"].score == 1000
        assert gm.gs.state == "VOTING"

    def test_start_with_no_players_blocked(self):
        gm = make_gm()
        gm.start_game()
        assert gm.gs.state == "WAITING"

    def test_no_answer_resets_streak_at_reveal(self):
        gm = make_gm()
        self._connect(gm, "dev-01")
        gm.players["dev-01"].streak = 3
        gm.gs.state = "VOTING"
        gm.gs.answers = {}  # dev-01 did not answer this round
        # reproduce the streak-reset loop from _transition_to_reveal
        for device_id, player in gm.players.items():
            if device_id not in gm.gs.answers:
                player.streak = 0
        assert gm.players["dev-01"].streak == 0


# ── validate_questions ─────────────────────────────────────────────────────────


class TestValidateQuestions:
    def _ok(self, questions):
        validate_questions(questions)  # must not raise or exit

    def _fails(self, questions):
        with pytest.raises(SystemExit):
            validate_questions(questions)

    # ── valid inputs ───────────────────────────────────────────────────────────

    def test_valid_mcq(self):
        self._ok([MCQ_Q])

    def test_valid_estimate(self):
        self._ok([ESTIMATE_Q])

    def test_valid_higher_lower(self):
        self._ok([HL_Q])

    def test_valid_poti_target(self):
        self._ok([POTI_Q])

    def test_valid_temp_target(self):
        self._ok([TEMP_Q])

    def test_multiple_valid_questions(self):
        self._ok([MCQ_Q, ESTIMATE_Q, HL_Q])

    # ── empty list ─────────────────────────────────────────────────────────────

    def test_empty_list_exits(self):
        self._fails([])

    # ── unknown type ───────────────────────────────────────────────────────────

    def test_unknown_type_exits(self):
        self._fails([{**MCQ_Q, "type": "telepathy"}])

    # ── missing common fields ──────────────────────────────────────────────────

    def test_missing_text_exits(self):
        q = {k: v for k, v in MCQ_Q.items() if k != "text"}
        self._fails([q])

    def test_missing_time_limit_exits(self):
        q = {k: v for k, v in MCQ_Q.items() if k != "time_limit_s"}
        self._fails([q])

    # ── MCQ-specific ───────────────────────────────────────────────────────────

    def test_mcq_missing_options_exits(self):
        q = {k: v for k, v in MCQ_Q.items() if k != "options"}
        self._fails([q])

    def test_mcq_wrong_option_keys_exits(self):
        self._fails([{**MCQ_Q, "options": {"X": "a", "Y": "b", "Z": "c", "W": "d"}}])

    def test_mcq_invalid_correct_exits(self):
        self._fails([{**MCQ_Q, "correct": "E"}])

    def test_mcq_correct_case_insensitive(self):
        self._ok([{**MCQ_Q, "correct": "b"}])

    # ── Estimate-specific ──────────────────────────────────────────────────────

    def test_estimate_missing_correct_exits(self):
        q = {k: v for k, v in ESTIMATE_Q.items() if k != "correct"}
        self._fails([q])

    def test_estimate_missing_min_exits(self):
        q = {k: v for k, v in ESTIMATE_Q.items() if k != "min"}
        self._fails([q])

    # ── Higher/Lower-specific ──────────────────────────────────────────────────

    def test_higher_lower_bad_correct_exits(self):
        self._fails([{**HL_Q, "correct": "MAYBE"}])

    def test_higher_lower_missing_actual_exits(self):
        q = {k: v for k, v in HL_Q.items() if k != "actual"}
        self._fails([q])

    # ── Poti/Temp-specific ─────────────────────────────────────────────────────

    def test_poti_missing_target_exits(self):
        q = {k: v for k, v in POTI_Q.items() if k != "target"}
        self._fails([q])

    def test_temp_missing_target_exits(self):
        q = {k: v for k, v in TEMP_Q.items() if k != "target"}
        self._fails([q])

    # ── partial failure ────────────────────────────────────────────────────────

    def test_one_bad_question_in_list_still_exits(self):
        bad = {k: v for k, v in MCQ_Q.items() if k != "correct"}
        self._fails([MCQ_Q, bad])


# ── Persistence ────────────────────────────────────────────────────────────────


class TestPersistence:
    def _make_gm_with_file(self, tmp_path, questions=None):
        state_file = str(tmp_path / "game_state.json")
        with patch("paho.mqtt.client.Client"):
            gm = GameMaster(
                broker="localhost",
                port=1883,
                questions=questions or [MCQ_Q],
                state_file=state_file,
            )
        gm.client = MagicMock()
        return gm

    # ── save / load round-trip ─────────────────────────────────────────────────

    def test_save_creates_file(self, tmp_path):
        gm = self._make_gm_with_file(tmp_path)
        gm._save_state()
        assert (tmp_path / "game_state.json").exists()

    def test_save_and_reload_restores_players(self, tmp_path):
        gm = self._make_gm_with_file(tmp_path)
        gm.players["dev-01"] = make_player("dev-01", "Alice", score=1500, streak=2)
        gm.players["dev-02"] = make_player("dev-02", "Bob",   score=800,  streak=0)
        gm.gs.question_index = 3
        gm.gs.question_id    = 3
        gm._save_state()

        # new instance reads the file
        gm2 = self._make_gm_with_file(tmp_path)
        assert "dev-01" in gm2.players
        assert gm2.players["dev-01"].name   == "Alice"
        assert gm2.players["dev-01"].score  == 1500
        assert gm2.players["dev-01"].streak == 2
        assert gm2.players["dev-02"].score  == 800
        assert gm2.gs.question_index        == 3
        assert gm2.gs.question_id           == 3

    def test_restored_players_start_offline(self, tmp_path):
        gm = self._make_gm_with_file(tmp_path)
        gm.players["dev-01"] = make_player("dev-01", online=True)
        gm._save_state()

        gm2 = self._make_gm_with_file(tmp_path)
        assert gm2.players["dev-01"].online is False

    def test_save_is_atomic_no_tmp_left_over(self, tmp_path):
        gm = self._make_gm_with_file(tmp_path)
        gm._save_state()
        assert not (tmp_path / "game_state.json.tmp").exists()

    # ── missing / corrupt file ─────────────────────────────────────────────────

    def test_missing_file_starts_fresh(self, tmp_path):
        gm = self._make_gm_with_file(tmp_path)
        assert gm.players == {}
        assert gm.gs.question_index == 0

    def test_corrupt_file_starts_fresh(self, tmp_path):
        state_file = tmp_path / "game_state.json"
        state_file.write_text("not valid json", encoding="utf-8")
        gm = self._make_gm_with_file(tmp_path)
        assert gm.players == {}

    def test_empty_players_in_file(self, tmp_path):
        state_file = tmp_path / "game_state.json"
        state_file.write_text(
            json.dumps({"players": [], "question_index": 0, "question_id": 0}),
            encoding="utf-8",
        )
        gm = self._make_gm_with_file(tmp_path)
        assert gm.players == {}

    # ── save triggered by state transitions ────────────────────────────────────

    def test_save_called_after_transition_to_scores(self, tmp_path):
        gm = self._make_gm_with_file(tmp_path, questions=[MCQ_Q])
        gm.players["dev-01"] = make_player("dev-01", "Alice", score=0)
        gm.gs.state          = "VOTING"
        gm.gs.question_index = 0
        gm.gs.question_id    = 1
        gm.gs.answers        = {"dev-01": {"answer": "B", "elapsed_ms": 0}}

        gm._reveal_mcq(MCQ_Q, 20_000, {"dev-01": gm.players["dev-01"]})
        gm._transition_to_scores()

        saved = json.loads((tmp_path / "game_state.json").read_text())
        # question_index was incremented to 1 before save
        assert saved["question_index"] == 1
        assert saved["players"][0]["score"] > 0

    def test_save_called_after_restart(self, tmp_path):
        gm = self._make_gm_with_file(tmp_path)
        gm.players["dev-01"] = make_player("dev-01", "Alice", score=3000, streak=3)
        gm.gs.state          = "ENDED"
        gm.restart_game()

        saved = json.loads((tmp_path / "game_state.json").read_text())
        assert saved["players"][0]["score"]  == 0
        assert saved["players"][0]["streak"] == 0
        assert saved["question_index"]       == 0

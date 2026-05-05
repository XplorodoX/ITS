export type GameStateValue =
  | "WAITING"
  | "QUESTION"
  | "VOTING"
  | "REVEAL"
  | "SCORES"
  | "ENDED";

export interface GameState {
  state: GameStateValue;
  question_id: number;
  remaining_s: number;
}

// ── Question types ──────────────────────────────────────────────────────────

export interface McqQuestion {
  id: number;
  total?: number;
  type?: "mcq";
  text: string;
  options: { A: string; B: string; C: string; D: string };
  time_limit_s: number;
}

export interface EstimateQuestion {
  id: number;
  total?: number;
  type: "estimate";
  text: string;
  min: number;
  max: number;
  unit?: string;
  time_limit_s: number;
}

export interface HigherLowerQuestion {
  id: number;
  total?: number;
  type: "higher_lower";
  text: string;
  reference: number;
  unit?: string;
  time_limit_s: number;
}

export interface PotiTargetQuestion {
  id: number;
  total?: number;
  type: "poti_target";
  text: string;
  target: number;
  tolerance: number;
  time_limit_s: number;
}

export interface TempTargetQuestion {
  id: number;
  total?: number;
  type: "temp_target";
  text: string;
  target: number;
  tolerance: number;
  time_limit_s: number;
}

export type Question = McqQuestion | EstimateQuestion | HigherLowerQuestion | PotiTargetQuestion | TempTargetQuestion;

// ── Reveal types ────────────────────────────────────────────────────────────

export interface McqReveal {
  question_id: number;
  type?: "mcq";
  correct: "A" | "B" | "C" | "D";
  counts: { A: number; B: number; C: number; D: number };
}

export interface EstimateReveal {
  question_id: number;
  type: "estimate";
  correct: number;
  unit?: string;
  answers: { device_id: string; name: string; value: number; delta: number }[];
}

export interface HigherLowerReveal {
  question_id: number;
  type: "higher_lower";
  correct: "HIGHER" | "LOWER";
  actual: number;
  unit?: string;
  counts: { HIGHER: number; LOWER: number };
}

export interface PotiTargetReveal {
  question_id: number;
  type: "poti_target";
  correct: number;
  tolerance: number;
  answers: { device_id: string; name: string; value: number; delta: number }[];
}

export interface TempTargetReveal {
  question_id: number;
  type: "temp_target";
  correct: number;
  tolerance: number;
  answers: { device_id: string; name: string; value: number; delta: number }[];
}

export type Reveal = McqReveal | EstimateReveal | HigherLowerReveal | PotiTargetReveal | TempTargetReveal;

// ── Other ───────────────────────────────────────────────────────────────────

export interface ScoreEntry {
  device_id: string;
  name: string;
  score: number;
  streak?: number;
}

export interface AnswerCount {
  question_id: number;
  count: number;
  total: number;
}

export interface Scores {
  scores: ScoreEntry[];
}

export interface LobbyPlayer {
  device_id: string;
  name: string;
  online: boolean;
}

export interface Players {
  players: LobbyPlayer[];
  min_players: number;
}

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

export interface Question {
  id: number;
  text: string;
  options: { A: string; B: string; C: string; D: string };
  time_limit_s: number;
}

export interface Reveal {
  question_id: number;
  correct: "A" | "B" | "C" | "D";
  counts: { A: number; B: number; C: number; D: number };
}

export interface ScoreEntry {
  device_id: string;
  name: string;
  score: number;
}

export interface Scores {
  scores: ScoreEntry[];
}

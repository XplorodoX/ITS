"use client";

import type { ScoreEntry } from "@/types/quiz";
import styles from "./screens.module.css";

const MEDALS = ["🥇", "🥈", "🥉"];

interface Props {
  scores: ScoreEntry[];
  ended?: boolean;
  onRestart?: () => void;
}

export default function ScoresScreen({ scores, ended = false, onRestart }: Props) {
  const maxScore = scores[0]?.score || 1;

  return (
    <div className={styles.screen}>
      <h2 className={styles.scoresTitle}>{ended ? "Endergebnis" : "Zwischenstand"}</h2>

      <div className={styles.scoresList}>
        {scores.map((entry, i) => (
          <div
            key={entry.device_id}
            className={`${styles.scoreRow} ${i === 0 ? styles.scoreRowFirst : ""}`}
          >
            <span className={styles.scoreRank}>
              {i < 3 ? MEDALS[i] : `${i + 1}.`}
            </span>
            <span className={styles.scoreName}>
              {entry.name}
              {entry.streak != null && entry.streak >= 2 && (
                <span className={styles.streakBadge} title={`${entry.streak}× Streak`}>
                  {"🔥".repeat(Math.min(entry.streak, 3))}
                </span>
              )}
            </span>
            <div className={styles.scoreBarTrack}>
              <div
                className={styles.scoreBarFill}
                style={{ width: `${(entry.score / maxScore) * 100}%` }}
              />
            </div>
            <span className={styles.scorePoints}>{entry.score.toLocaleString("de-DE")}</span>
          </div>
        ))}
      </div>

      {ended && onRestart && (
        <button className={styles.restartBtn} onClick={onRestart}>
          Neu starten
        </button>
      )}
    </div>
  );
}

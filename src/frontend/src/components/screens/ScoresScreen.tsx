"use client";

import type { ScoreEntry } from "@/types/quiz";
import styles from "./screens.module.css";
import Confetti from "../Confetti";

// Medaillen-Symbole für das Podium (Top 3)
const MEDALS = ["🥇", "🥈", "🥉"];

interface Props {
  /** Das aktuelle Leaderboard-Array, sortiert nach absteigendem Score */
  scores: ScoreEntry[];
  /** Gibt an, ob das Spiel beendet ist (wenn true, startet das Konfetti) */
  ended?: boolean;
  /** Callback zur Auslösung eines Quiz-Neustarts */
  onRestart?: () => void;
}

/**
 * ScoresScreen Komponente.
 *
 * Zeigt das Leaderboard/Scoreboard (Zwischenstand oder Endergebnis) an.
 * Visualisiert die relativen Abstände mit animierten Balken und zeichnet
 * bei Spielende die Konfetti-Animation.
 */
export default function ScoresScreen({ scores, ended = false, onRestart }: Props) {
  // Ermittelt die Höchstpunktzahl, um die Breiten der Punkteleisten (0-100%) relativ zu normalisieren
  const maxScore = scores[0]?.score || 1;

  return (
    <div className={styles.screen}>
      {/* Konfetti rendern, wenn das Spiel zu Ende ist */}
      {ended && <Confetti />}
      <h2 className={styles.scoresTitle}>{ended ? "Endergebnis" : "Zwischenstand"}</h2>

      <div className={styles.scoresList}>
        {scores.map((entry, i) => (
          <div
            key={entry.device_id}
            className={`${styles.scoreRow} ${i === 0 ? styles.scoreRowFirst : ""}`}
            style={{ "--i": i } as React.CSSProperties}
          >
            {/* Medaille für Platz 1-3, andernfalls Rangnummer */}
            <span className={styles.scoreRank}>
              {i < 3 ? MEDALS[i] : `${i + 1}.`}
            </span>
            <span className={styles.scoreName}>
              {entry.name}
              {/* Feuersymbole bei aktiver Streak (ab 2 korrekten Antworten in Folge) */}
              {entry.streak != null && entry.streak >= 2 && (
                <span className={styles.streakBadge} title={`${entry.streak}× Streak`}>
                  {"🔥".repeat(Math.min(entry.streak, 3))}
                </span>
              )}
            </span>
            <div className={styles.scoreBarTrack}>
              {/* Breite proportional zum Führenden */}
              <div
                className={styles.scoreBarFill}
                style={{ width: `${(entry.score / maxScore) * 100}%` }}
              />
            </div>
            {/* Lokale deutsche Formatierung für Tausendertrennzeichen */}
            <span className={styles.scorePoints}>{entry.score.toLocaleString("de-DE")}</span>
          </div>
        ))}
      </div>

      {/* Button zum Zurückkehren in die Lobby am Spielende */}
      {ended && onRestart && (
        <button className={styles.restartBtn} onClick={onRestart}>
          Neu starten
        </button>
      )}
    </div>
  );
}

"use client";

import type { Question, Reveal } from "@/types/quiz";
import styles from "./screens.module.css";

const LABELS = ["A", "B", "C", "D"] as const;
const COLORS = ["#6c63ff", "#22c55e", "#eab308", "#ef4444"];

interface Props {
  question: Question;
  reveal: Reveal;
}

export default function RevealScreen({ question, reveal }: Props) {
  const total = Object.values(reveal.counts).reduce((a, b) => a + b, 0) || 1;

  return (
    <div className={styles.screen}>
      <h1 className={styles.questionText}>{question.text}</h1>

      <div className={styles.barChart}>
        {LABELS.map((key, i) => {
          const count = reveal.counts[key];
          const pct   = Math.round((count / total) * 100);
          const correct = key === reveal.correct;

          return (
            <div key={key} className={styles.barRow}>
              <span
                className={`${styles.barKey} ${correct ? styles.barKeyCorrect : ""}`}
                style={{ "--opt-color": COLORS[i] } as React.CSSProperties}
              >
                {key}
              </span>
              <div className={styles.barTrack}>
                <div
                  className={`${styles.barFill} ${correct ? styles.barFillCorrect : ""}`}
                  style={{
                    width: `${pct}%`,
                    "--opt-color": COLORS[i],
                  } as React.CSSProperties}
                />
              </div>
              <span className={styles.barCount}>{count}</span>
              {correct && <span className={styles.correctBadge}>✓</span>}
            </div>
          );
        })}
      </div>

      <p className={styles.correctAnswer}>
        Richtige Antwort: <strong>{reveal.correct} — {question.options[reveal.correct]}</strong>
      </p>
    </div>
  );
}

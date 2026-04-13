"use client";

import type { Question } from "@/types/quiz";
import styles from "./screens.module.css";

const LABELS = ["A", "B", "C", "D"] as const;
const COLORS = ["#6c63ff", "#22c55e", "#eab308", "#ef4444"];

interface Props {
  question: Question;
  remainingS: number;
  voting: boolean;
}

export default function QuestionScreen({ question, remainingS, voting }: Props) {
  const pct = Math.max(0, remainingS / question.time_limit_s);

  return (
    <div className={styles.screen}>
      <div className={styles.questionHeader}>
        <span className={styles.questionLabel}>Frage</span>
        {voting && (
          <span className={styles.timerBadge} style={{ "--pct": pct } as React.CSSProperties}>
            {remainingS}s
          </span>
        )}
      </div>

      <h1 className={styles.questionText}>{question.text}</h1>

      <div className={styles.optionsGrid}>
        {LABELS.map((key, i) => (
          <div
            key={key}
            className={styles.optionCard}
            style={{ "--opt-color": COLORS[i] } as React.CSSProperties}
          >
            <span className={styles.optionKey}>{key}</span>
            <span className={styles.optionText}>{question.options[key]}</span>
          </div>
        ))}
      </div>
    </div>
  );
}

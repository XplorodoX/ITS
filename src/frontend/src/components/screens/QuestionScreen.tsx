"use client";

import { useEffect, useState } from "react";
import type { Question, AnswerCount } from "@/types/quiz";
import styles from "./screens.module.css";

const LABELS = ["A", "B", "C", "D"] as const;
const COLORS = ["#6c63ff", "#22c55e", "#eab308", "#ef4444"];

interface Props {
  question: Question;
  remainingS: number;
  voting: boolean;
  answerCount: AnswerCount | null;
}

export default function QuestionScreen({ question, remainingS, voting, answerCount }: Props) {
  // Local countdown — ticks independently of MQTT updates
  const [countdown, setCountdown] = useState(remainingS);

  // Sync when a new question arrives (remainingS jumps to a new value)
  useEffect(() => {
    setCountdown(remainingS);
  }, [remainingS, question.id]);

  // Tick every second while voting is open
  useEffect(() => {
    if (!voting) return;
    const id = setInterval(() => {
      setCountdown(prev => {
        if (prev <= 1) {
          clearInterval(id);
          return 0;
        }
        return prev - 1;
      });
    }, 1000);
    return () => clearInterval(id);
  }, [voting, question.id]);

  const pct = Math.max(0, countdown / question.time_limit_s);

  // Only show answer count if it belongs to the current question
  const count = answerCount?.question_id === question.id ? answerCount : null;

  return (
    <div className={styles.screen}>
      <div className={styles.questionHeader}>
        <span className={styles.questionLabel}>Frage</span>
        {voting && (
          <span className={styles.timerBadge} style={{ "--pct": pct } as React.CSSProperties}>
            {countdown}s
          </span>
        )}
        {voting && count && (
          <span className={styles.answerCounter}>
            {count.count} / {count.total} geantwortet
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

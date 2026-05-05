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
  const [countdown, setCountdown] = useState(remainingS);

  useEffect(() => { setCountdown(remainingS); }, [remainingS, question.id]);

  useEffect(() => {
    if (!voting) return;
    const id = setInterval(() => {
      setCountdown(prev => { if (prev <= 1) { clearInterval(id); return 0; } return prev - 1; });
    }, 1000);
    return () => clearInterval(id);
  }, [voting, question.id]);

  const pct   = Math.max(0, countdown / question.time_limit_s);
  const count = answerCount?.question_id === question.id ? answerCount : null;
  const qtype = question.type ?? "mcq";

  const header = (
    <div className={styles.questionHeader}>
      <span className={styles.questionLabel}>
        {qtype === "estimate" ? "Schätzfrage"
        : qtype === "higher_lower" ? "Höher / Niedriger?"
        : qtype === "poti_target" ? "Poti-Challenge"
        : qtype === "temp_target" ? "Temperatur-Challenge"
        : "Frage"}
      </span>
      {voting && (
        <span className={styles.timerBadge} style={{ "--pct": pct } as React.CSSProperties}>
          {countdown}s
        </span>
      )}
      {voting && count && (
        <span className={styles.answerCounter}>{count.count} / {count.total} geantwortet</span>
      )}
    </div>
  );

  if (qtype === "estimate") {
    const q = question as Extract<typeof question, { type: "estimate" }>;
    return (
      <div className={styles.screen}>
        {header}
        <h1 className={styles.questionText}>{q.text}</h1>
        <div className={styles.estimateHint}>
          <span className={styles.estimateRange}>
            {q.min} – {q.max}{q.unit ? ` ${q.unit}` : ""}
          </span>
          <p className={styles.estimateInstr}>Drehknopf zum Schätzen, Drücken zum Bestätigen</p>
        </div>
      </div>
    );
  }

  if (qtype === "higher_lower") {
    const q = question as Extract<typeof question, { type: "higher_lower" }>;
    return (
      <div className={styles.screen}>
        {header}
        <h1 className={styles.questionText}>{q.text}</h1>
        <div className={styles.hlReference}>
          <span className={styles.hlValue}>
            {q.reference}{q.unit ? ` ${q.unit}` : ""}
          </span>
        </div>
        <div className={styles.hlChoices}>
          <div className={`${styles.hlCard} ${styles.hlHigher}`}>↑ Höher</div>
          <div className={`${styles.hlCard} ${styles.hlLower}`}>↓ Niedriger</div>
        </div>
      </div>
    );
  }

  if (qtype === "poti_target") {
    const q = question as Extract<typeof question, { type: "poti_target" }>;
    return (
      <div className={styles.screen}>
        {header}
        <h1 className={styles.questionText}>{q.text}</h1>
        <div className={styles.estimateHint}>
          <span className={styles.estimateRange} style={{ fontSize: "2.5rem", fontWeight: 700 }}>
            Ziel: {q.target}%
          </span>
          <p className={styles.estimateInstr}>Poti drehen bis zum Zielwert, dann Knopf drücken</p>
          <p className={styles.estimateInstr}>Toleranz: ±{q.tolerance}%</p>
        </div>
      </div>
    );
  }

  if (qtype === "temp_target") {
    const q = question as Extract<typeof question, { type: "temp_target" }>;
    return (
      <div className={styles.screen}>
        {header}
        <h1 className={styles.questionText}>{q.text}</h1>
        <div className={styles.estimateHint}>
          <span className={styles.estimateRange} style={{ fontSize: "2.5rem", fontWeight: 700 }}>
            Ziel: {q.target} °C
          </span>
          <p className={styles.estimateInstr}>Sensor auf die Zieltemperatur bringen, dann Knopf drücken</p>
          <p className={styles.estimateInstr}>Toleranz: ±{q.tolerance} °C</p>
        </div>
      </div>
    );
  }

  // Default: MCQ
  return (
    <div className={styles.screen}>
      {header}
      <h1 className={styles.questionText}>{question.text}</h1>
      <div className={styles.optionsGrid}>
        {LABELS.map((key, i) => (
          <div
            key={key}
            className={styles.optionCard}
            style={{ "--opt-color": COLORS[i] } as React.CSSProperties}
          >
            <span className={styles.optionKey}>{key}</span>
            <span className={styles.optionText}>{(question as Extract<typeof question, { options: unknown }>).options[key]}</span>
          </div>
        ))}
      </div>
    </div>
  );
}

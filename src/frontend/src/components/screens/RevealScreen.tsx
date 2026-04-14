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

  // ── Estimate ──────────────────────────────────────────────────────────────
  if (reveal.type === "estimate") {
    const maxDelta = Math.max(...reveal.answers.map(a => a.delta), 1);
    return (
      <div className={styles.screen}>
        <h1 className={styles.questionText}>{question.text}</h1>
        <div className={styles.estimateCorrect}>
          Richtige Antwort: <strong>{reveal.correct}{reveal.unit ? ` ${reveal.unit}` : ""}</strong>
        </div>
        <div className={styles.estimateList}>
          {reveal.answers.slice(0, 8).map((a, i) => (
            <div key={a.device_id} className={styles.estimateRow}>
              <span className={styles.estimateRank}>{i + 1}.</span>
              <span className={styles.estimateName}>{a.name}</span>
              <span className={styles.estimateValue}>{a.value}{reveal.unit ? ` ${reveal.unit}` : ""}</span>
              <div className={styles.barTrack}>
                <div
                  className={styles.barFill}
                  style={{ width: `${Math.round((1 - a.delta / maxDelta) * 100)}%`, "--opt-color": "#6c63ff" } as React.CSSProperties}
                />
              </div>
              <span className={styles.estimateDelta}>±{a.delta}</span>
            </div>
          ))}
        </div>
      </div>
    );
  }

  // ── Higher / Lower ────────────────────────────────────────────────────────
  if (reveal.type === "higher_lower") {
    const total   = (reveal.counts.HIGHER + reveal.counts.LOWER) || 1;
    const unit    = reveal.unit ? ` ${reveal.unit}` : "";
    return (
      <div className={styles.screen}>
        <h1 className={styles.questionText}>{question.text}</h1>
        <div className={styles.hlRevealActual}>
          Tatsächlich: <strong>{reveal.actual}{unit}</strong>
        </div>
        <div className={styles.barChart}>
          {(["HIGHER", "LOWER"] as const).map((key) => {
            const correct = key === reveal.correct;
            const count   = reveal.counts[key];
            const pct     = Math.round((count / total) * 100);
            return (
              <div key={key} className={styles.barRow}>
                <span className={`${styles.barKey} ${correct ? styles.barKeyCorrect : ""}`}
                  style={{ "--opt-color": key === "HIGHER" ? "#22c55e" : "#ef4444" } as React.CSSProperties}>
                  {key === "HIGHER" ? "↑" : "↓"}
                </span>
                <div className={styles.barTrack}>
                  <div
                    className={`${styles.barFill} ${correct ? styles.barFillCorrect : ""}`}
                    style={{ width: `${pct}%`, "--opt-color": key === "HIGHER" ? "#22c55e" : "#ef4444" } as React.CSSProperties}
                  />
                </div>
                <span className={styles.barCount}>{count}</span>
                {correct && <span className={styles.correctBadge}>✓</span>}
              </div>
            );
          })}
        </div>
        <p className={styles.correctAnswer}>
          Richtig: <strong>{reveal.correct === "HIGHER" ? "↑ Höher" : "↓ Niedriger"}</strong>
          {" "}({reveal.actual}{unit})
        </p>
      </div>
    );
  }

  // ── MCQ (default) ─────────────────────────────────────────────────────────
  const total = Object.values(reveal.counts).reduce((a, b) => a + b, 0) || 1;
  return (
    <div className={styles.screen}>
      <h1 className={styles.questionText}>{question.text}</h1>
      <div className={styles.barChart}>
        {LABELS.map((key, i) => {
          const count   = reveal.counts[key];
          const pct     = Math.round((count / total) * 100);
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
                  style={{ width: `${pct}%`, "--opt-color": COLORS[i] } as React.CSSProperties}
                />
              </div>
              <span className={styles.barCount}>{count}</span>
              {correct && <span className={styles.correctBadge}>✓</span>}
            </div>
          );
        })}
      </div>
      <p className={styles.correctAnswer}>
        Richtige Antwort: <strong>{reveal.correct} — {(question as Extract<typeof question, { options: unknown }>).options[reveal.correct]}</strong>
      </p>
    </div>
  );
}

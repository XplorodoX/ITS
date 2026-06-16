"use client";

import type { Question, Reveal } from "@/types/quiz";
import styles from "./screens.module.css";

// Antwort-Buchstaben und Farbcodes für MCQ-Diagramme
const LABELS = ["A", "B", "C", "D"] as const;
const COLORS = ["#6c63ff", "#22c55e", "#eab308", "#ef4444"];

interface Props {
  /** Die aktuell behandelte Frage */
  question: Question;
  /** Die Auflösungsdaten vom Game Master (z. B. richtige Option, abgegebene Schätzwerte) */
  reveal: Reveal;
}

/**
 * RevealScreen Komponente.
 *
 * Präsentiert die Auflösung einer Frage. Zeichnet ein Diagramm für Multiple Choice /
 * Höher-Niedriger Fragen oder zeigt eine Rangliste mit Abweichungen (Deltas) für
 * Schätz- und Sensor-Challenges an.
 */
export default function RevealScreen({ question, reveal }: Props) {

  // ── 1. Schätzfragen Auflösung (Estimate) ──
  if (reveal.type === "estimate") {
    // Ermittelt die maximale Abweichung, um die relative Balkenbreite (0-100%) zu skalieren
    const maxDelta = Math.max(...reveal.answers.map(a => a.delta), 1);
    return (
      <div className={styles.screen}>
        <h1 className={styles.questionText}>{question.text}</h1>
        <div className={styles.estimateCorrect}>
          Richtige Antwort: <strong>{reveal.correct}{reveal.unit ? ` ${reveal.unit}` : ""}</strong>
        </div>
        <div className={styles.estimateList}>
          {/* Zeigt die Top 8 Schätzungen, sortiert nach geringster Abweichung */}
          {reveal.answers.slice(0, 8).map((a, i) => (
            <div
              key={a.device_id}
              className={styles.estimateRow}
              style={{ "--i": i } as React.CSSProperties}
            >
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

  // ── 2. Poti-Challenge Auflösung (Poti Target) ──
  if (reveal.type === "poti_target") {
    const maxDelta = Math.max(...reveal.answers.map(a => a.delta), 1);
    return (
      <div className={styles.screen}>
        <h1 className={styles.questionText}>{question.text}</h1>
        <div className={styles.estimateCorrect}>
          Zielwert: <strong>{reveal.correct}%</strong>
          <span style={{ marginLeft: "1rem", fontSize: "0.9rem", opacity: 0.7 }}>±{reveal.tolerance}% Toleranz</span>
        </div>
        <div className={styles.estimateList}>
          {reveal.answers.slice(0, 8).map((a, i) => (
            <div
              key={a.device_id}
              className={styles.estimateRow}
              style={{ "--i": i } as React.CSSProperties}
            >
              <span className={styles.estimateRank}>{i + 1}.</span>
              <span className={styles.estimateName}>{a.name}</span>
              <span className={styles.estimateValue}>{a.value}%</span>
              <div className={styles.barTrack}>
                <div
                  className={styles.barFill}
                  style={{ width: `${Math.round((1 - a.delta / maxDelta) * 100)}%`, "--opt-color": "#6c63ff" } as React.CSSProperties}
                />
              </div>
              <span className={styles.estimateDelta}>±{a.delta}%</span>
            </div>
          ))}
        </div>
      </div>
    );
  }

  // ── 3. Temperatur-Challenge Auflösung (Temp Target) ──
  if (reveal.type === "temp_target") {
    const maxDelta = Math.max(...reveal.answers.map(a => a.delta), 1);
    return (
      <div className={styles.screen}>
        <h1 className={styles.questionText}>{question.text}</h1>
        <div className={styles.estimateCorrect}>
          Zieltemperatur: <strong>{reveal.correct} °C</strong>
          <span style={{ marginLeft: "1rem", fontSize: "0.9rem", opacity: 0.7 }}>±{reveal.tolerance} °C Toleranz</span>
        </div>
        <div className={styles.estimateList}>
          {reveal.answers.slice(0, 8).map((a, i) => (
            <div
              key={a.device_id}
              className={styles.estimateRow}
              style={{ "--i": i } as React.CSSProperties}
            >
              <span className={styles.estimateRank}>{i + 1}.</span>
              <span className={styles.estimateName}>{a.name}</span>
              <span className={styles.estimateValue}>{a.value.toFixed(1)} °C</span>
              <div className={styles.barTrack}>
                <div
                  className={styles.barFill}
                  style={{ width: `${Math.round((1 - a.delta / maxDelta) * 100)}%`, "--opt-color": "#f97316" } as React.CSSProperties}
                />
              </div>
              <span className={styles.estimateDelta}>±{a.delta.toFixed(1)} °C</span>
            </div>
          ))}
        </div>
      </div>
    );
  }

  // ── 4. Höher / Niedriger Auflösung (Higher / Lower) ──
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
              <div
                key={key}
                className={styles.barRow}
                style={{ "--i": key === "HIGHER" ? 0 : 1 } as React.CSSProperties}
              >
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

  // ── 5. Standard MCQ Auflösung (Multiple Choice) ──
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
            <div
              key={key}
              className={styles.barRow}
              style={{ "--i": i } as React.CSSProperties}
            >
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

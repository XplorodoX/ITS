"use client";

import type { ScoreEntry } from "@/types/quiz";
import styles from "./screens.module.css";

interface Props {
  players: ScoreEntry[];
}

export default function WaitingScreen({ players }: Props) {
  return (
    <div className={styles.screen}>
      <div className={styles.waitingLogo}>AALeC Quiz</div>
      <p className={styles.waitingSubtitle}>Warte auf Spieler…</p>

      {players.length > 0 && (
        <div className={styles.playerGrid}>
          {players.map((p) => (
            <div key={p.device_id} className={styles.playerChip}>
              {p.name}
            </div>
          ))}
        </div>
      )}

      {players.length === 0 && (
        <p className={styles.muted}>Noch keine Geräte verbunden.</p>
      )}
    </div>
  );
}

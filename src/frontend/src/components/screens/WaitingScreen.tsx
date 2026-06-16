"use client";

import type { LobbyPlayer, QuestionSets } from "@/types/quiz";
import styles from "./screens.module.css";
import lobbyStyles from "./LobbyScreen.module.css";

interface Props {
  players: LobbyPlayer[];
  minPlayers: number;
  gameState: string;
  questionSets: QuestionSets | null;
  onStart: () => void;
  onLoadSet: (name: string) => void;
}

export default function WaitingScreen({ players, minPlayers, gameState, questionSets, onStart, onLoadSet }: Props) {
  const activePlayers = players.filter((p) => p.online);
  const onlineCount   = activePlayers.length;
  const missing       = Math.max(0, minPlayers - onlineCount);
  const canStart      = gameState === "WAITING" && missing === 0;

  return (
    <div className={styles.screen}>
      <div className={styles.waitingLogo}>AALeC Quiz</div>
      <p className={styles.waitingSubtitle}>
        {onlineCount === 0
          ? "Warte auf Geräte…"
          : `${onlineCount} ${onlineCount === 1 ? "Gerät" : "Geräte"} verbunden`}
      </p>

      {/* ── Teilnehmer:innen ── */}
      <div className={styles.playerGrid}>
        {activePlayers.length === 0 ? (
          <p className={styles.muted}>Noch keine Geräte verbunden.</p>
        ) : (
          activePlayers.map((p, i) => (
            <div
              key={p.device_id}
              className={`${styles.playerChip} ${lobbyStyles.online}`}
              style={{ "--i": i } as React.CSSProperties}
            >
              <span className={lobbyStyles.dot} />
              <span className={lobbyStyles.playerName}>{p.name}</span>
            </div>
          ))
        )}
      </div>

      {/* ── Fragen-Set ── */}
      {gameState === "WAITING" && questionSets && (
        <div className={lobbyStyles.setSelector}>
          <p className={lobbyStyles.setLabel}>
            Fragen-Set:
            <strong style={{ marginLeft: "0.5rem", color: "var(--accent)" }}>
              {questionSets.active}
            </strong>
          </p>
          {questionSets.sets.length > 1 && (
            <div className={lobbyStyles.setChips}>
              {questionSets.sets.map(s => (
                <button
                  key={s.name}
                  className={`${lobbyStyles.setChip} ${s.active ? lobbyStyles.setChipActive : ""}`}
                  onClick={() => !s.active && onLoadSet(s.name)}
                  disabled={s.active}
                >
                  {s.name}
                  <span className={lobbyStyles.setChipCount}>{s.count}</span>
                </button>
              ))}
            </div>
          )}
        </div>
      )}

      {/* ── Start-Aktionen ── */}
      {gameState === "WAITING" && (
        <div className={lobbyStyles.actions}>
          {missing > 0 && (
            <p className={lobbyStyles.hint}>
              Noch <strong>{missing}</strong> {missing === 1 ? "Spieler fehlt" : "Spieler fehlen"} zum Starten.
            </p>
          )}
          {missing === 0 && players.length > 0 && (
            <p className={lobbyStyles.hint}>
              <strong>{onlineCount}</strong> Spieler bereit — los geht&apos;s!
            </p>
          )}
          <button
            className={lobbyStyles.startBtn}
            disabled={!canStart}
            onClick={onStart}
          >
            Quiz starten
          </button>
        </div>
      )}

      {gameState !== "WAITING" && (
        <p className={styles.waitingSubtitle}>Spiel läuft…</p>
      )}
    </div>
  );
}

"use client";

import type { LobbyPlayer } from "@/types/quiz";
import styles from "./screens.module.css";
import lobbyStyles from "./LobbyScreen.module.css";

interface Props {
  players: LobbyPlayer[];
  minPlayers: number;
  gameState: string;
  onStart: () => void;
}

export default function WaitingScreen({ players, minPlayers, gameState, onStart }: Props) {
  const onlineCount = players.filter((p) => p.online).length;
  const missing     = Math.max(0, minPlayers - onlineCount);
  const canStart    = gameState === "WAITING" && missing === 0;

  return (
    <div className={styles.screen}>
      <div className={styles.waitingLogo}>AALeC Quiz</div>

      <div className={lobbyStyles.playerList}>
        {players.length === 0 ? (
          <p className={styles.muted}>Noch keine Geräte verbunden.</p>
        ) : (
          players.map((p) => (
            <div
              key={p.device_id}
              className={`${lobbyStyles.playerRow} ${p.online ? lobbyStyles.online : lobbyStyles.offline}`}
            >
              <span className={lobbyStyles.dot} />
              <span className={lobbyStyles.playerName}>{p.name}</span>
            </div>
          ))
        )}
      </div>

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

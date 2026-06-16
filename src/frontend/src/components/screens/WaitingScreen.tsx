"use client";

import type { LobbyPlayer } from "@/types/quiz";
import styles from "./screens.module.css";
import lobbyStyles from "./LobbyScreen.module.css";

interface Props {
  /** Liste aller registrierten Spieler */
  players: LobbyPlayer[];
  /** Mindestanzahl Spieler, um das Quiz zu starten */
  minPlayers: number;
  /** Der aktuelle Zustand des Spiels (muss WAITING sein) */
  gameState: string;
  /** Callback zum Starten des Spiels (Übergang zu QUESTION) */
  onStart: () => void;
}

/**
 * WaitingScreen Komponente (Lobby).
 *
 * Wartet auf die Verbindung der Controller. Zeigt die verbundenen Spieler
 * als Chips mit Online-Statuspunkten an. Ermöglicht dem Spielleiter, das Quiz zu starten.
 */
export default function WaitingScreen({ players, minPlayers, gameState, onStart }: Props) {
  // Filtern nach Spielern, die gerade aktiv online sind
  const activePlayers = players.filter((p) => p.online);
  const onlineCount   = activePlayers.length;
  // Berechnen, wie viele Spieler noch fehlen, um das konfigurierte Minimum zu erreichen
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
          {/* Start-Button ist deaktiviert, solange nicht genügend Spieler online sind */}
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

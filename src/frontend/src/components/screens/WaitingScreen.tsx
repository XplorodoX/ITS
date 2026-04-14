"use client";

import { useState, useRef } from "react";
import type { LobbyPlayer } from "@/types/quiz";
import styles from "./screens.module.css";
import lobbyStyles from "./LobbyScreen.module.css";

interface Props {
  players: LobbyPlayer[];
  minPlayers: number;
  gameState: string;
  onStart: () => void;
  onSendNameList: (names: string[]) => void;
  onResetNames: () => void;
}

export default function WaitingScreen({ players, minPlayers, gameState, onStart, onSendNameList, onResetNames }: Props) {
  const onlineCount = players.filter((p) => p.online).length;
  const missing     = Math.max(0, minPlayers - onlineCount);
  const canStart    = gameState === "WAITING" && missing === 0;

  const [names, setNames]       = useState<string[]>([]);
  const [input, setInput]       = useState("");
  const [sent, setSent]         = useState(false);
  const inputRef                = useRef<HTMLInputElement>(null);

  const addName = () => {
    const trimmed = input.trim().slice(0, 15);
    if (!trimmed || names.includes(trimmed) || names.length >= 20) return;
    setNames(prev => [...prev, trimmed]);
    setInput("");
    setSent(false);
    inputRef.current?.focus();
  };

  const removeName = (name: string) => {
    setNames(prev => prev.filter(n => n !== name));
    setSent(false);
  };

  const handleSend = () => {
    if (names.length === 0) return;
    onSendNameList(names);
    setSent(true);
  };

  return (
    <div className={styles.screen}>
      <div className={styles.waitingLogo}>AALeC Quiz</div>

      {/* ── Spielerliste ── */}
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

      {/* ── Namenslisten-Editor ── */}
      {gameState === "WAITING" && (
        <div className={lobbyStyles.nameListEditor}>
          <p className={lobbyStyles.nameListTitle}>Namensliste für Geräte:</p>

          <div className={lobbyStyles.nameListTags}>
            {names.map(name => (
              <span key={name} className={lobbyStyles.nameTag}>
                {name}
                <button
                  className={lobbyStyles.nameTagRemove}
                  onClick={() => removeName(name)}
                  aria-label={`${name} entfernen`}
                >×</button>
              </span>
            ))}
          </div>

          <div className={lobbyStyles.nameListInputRow}>
            <input
              ref={inputRef}
              className={lobbyStyles.nameInput}
              value={input}
              maxLength={15}
              placeholder="Name eingeben…"
              onChange={e => setInput(e.target.value)}
              onKeyDown={e => { if (e.key === "Enter") addName(); }}
            />
            <button
              className={lobbyStyles.addBtn}
              onClick={addName}
              disabled={!input.trim() || names.length >= 20}
            >+</button>
          </div>

          <button
            className={`${lobbyStyles.sendBtn} ${sent ? lobbyStyles.sentBtn : ""}`}
            onClick={handleSend}
            disabled={names.length === 0}
          >
            {sent ? "✓ Gesendet" : "▶ Liste senden"}
          </button>

          <button
            className={`${lobbyStyles.sendBtn} ${lobbyStyles.resetBtn}`}
            onClick={onResetNames}
          >
            Namen auf Geraeten zuruecksetzen
          </button>
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

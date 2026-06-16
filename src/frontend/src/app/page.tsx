"use client";

import { useCallback, useMemo } from "react";
import { useMqtt } from "@/hooks/useMqtt";
import { useSound } from "@/hooks/useSound";

import WaitingScreen  from "@/components/screens/WaitingScreen";
import QuestionScreen from "@/components/screens/QuestionScreen";
import RevealScreen   from "@/components/screens/RevealScreen";
import ScoresScreen   from "@/components/screens/ScoresScreen";
import styles from "@/components/screens/screens.module.css";

/**
 * BeamerPage Komponente.
 *
 * Der primäre Einstiegspunkt für die Projektionsfläche (Beamer).
 * Baut die Verbindung zum Broker über `useMqtt` auf und rendert
 * abhängig vom übertragenen Zustand den passenden Screen.
 * Spielt Sounds über `useSound` bei Zustandsänderungen ab.
 */
export default function BeamerPage() {
  // MQTT-Verbindung aufbauen und Zustände abfragen
  const { connected, gameState, question, reveal, scores, answerCount, players, questionSets, publish } = useMqtt();

  const lobbyPlayers  = players?.players    ?? [];
  const minPlayers    = players?.min_players ?? 2;
  const currentState  = gameState?.state     ?? "WAITING";

  // Tonsynthesizer triggern bei Zustandswechseln
  useSound(currentState);

  /**
   * Sendet das Start-Signal an den Game Master, um das Spiel zu beginnen.
   */
  const handleStart = useCallback(() => {
    publish("quiz/control", { action: "start" });
  }, [publish]);

  /**
   * Setzt das Spiel zurück, um ein neues Quiz in der Lobby vorzubereiten.
   */
  const handleRestart = useCallback(() => {
    publish("quiz/control", { action: "restart" });
  }, [publish]);

  /**
   * Lädt ein anderes Fragenset über den Game Master.
   */
  const handleLoadSet = useCallback((name: string) => {
    publish("quiz/control", { action: "load_set", name });
  }, [publish]);

  // Standard-Lobby-Screen als Fallback-Element
  const waitingScreen = (
    <WaitingScreen
      players={lobbyPlayers}
      minPlayers={minPlayers}
      gameState={currentState}
      questionSets={questionSets}
      onStart={handleStart}
      onLoadSet={handleLoadSet}
    />
  );

  // Selektiert die anzuzeigende Komponente basierend auf dem Spielzustand.
  // Optimiert mit useMemo, um unnötige Neu-Rendervorgänge zu verhindern.
  const screen = useMemo(() => {
    switch (currentState) {
      case "WAITING":
        return waitingScreen;

      case "QUESTION":
        return question
          ? <QuestionScreen question={question} remainingS={gameState?.remaining_s ?? 0} voting={false} answerCount={null} />
          : waitingScreen;

      case "VOTING":
        return question
          ? <QuestionScreen question={question} remainingS={gameState?.remaining_s ?? 0} voting={true} answerCount={answerCount} />
          : waitingScreen;

      case "REVEAL":
        return question && reveal
          ? <RevealScreen question={question} reveal={reveal} />
          : waitingScreen;

      case "SCORES":
        return scores
          ? <ScoresScreen scores={scores.scores} />
          : waitingScreen;

      case "ENDED":
        return scores
          ? <ScoresScreen scores={scores.scores} ended onRestart={handleRestart} />
          : waitingScreen;

      default:
        return waitingScreen;
    }
  // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [currentState, question, reveal, scores, answerCount, lobbyPlayers, minPlayers]);

  return (
    <>
      {/* Kleiner Verbindungsindikator in der Ecke (grün = verbunden, rot/unsichtbar = getrennt) */}
      <div className={`${styles.connectionDot} ${connected ? styles.connected : ""}`} />
      {screen}
    </>
  );
}

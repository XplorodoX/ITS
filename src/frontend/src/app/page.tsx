"use client";

import { useCallback, useMemo } from "react";
import { useMqtt } from "@/hooks/useMqtt";

import WaitingScreen  from "@/components/screens/WaitingScreen";
import QuestionScreen from "@/components/screens/QuestionScreen";
import RevealScreen   from "@/components/screens/RevealScreen";
import ScoresScreen   from "@/components/screens/ScoresScreen";
import styles from "@/components/screens/screens.module.css";

export default function BeamerPage() {
  const { connected, gameState, question, reveal, scores, answerCount, players, publish } = useMqtt();

  const lobbyPlayers  = players?.players    ?? [];
  const minPlayers    = players?.min_players ?? 2;
  const currentState  = gameState?.state     ?? "WAITING";

  const handleStart = useCallback(() => {
    publish("quiz/control", { action: "start" });
  }, [publish]);

  const handleRestart = useCallback(() => {
    publish("quiz/control", { action: "restart" });
  }, [publish]);

  const handleSendNameList = useCallback((names: string[]) => {
    publish("quiz/namelist/set", { names });
  }, [publish]);

  const handleResetNames = useCallback(() => {
    publish("quiz/control", { action: "reset_names" });
  }, [publish]);

  const waitingScreen = (
    <WaitingScreen
      players={lobbyPlayers}
      minPlayers={minPlayers}
      gameState={currentState}
      onStart={handleStart}
      onSendNameList={handleSendNameList}
      onResetNames={handleResetNames}
    />
  );

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
      <div className={`${styles.connectionDot} ${connected ? styles.connected : ""}`} />
      {screen}
    </>
  );
}

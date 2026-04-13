"use client";

import { useMemo, useRef } from "react";
import { useMqtt } from "@/hooks/useMqtt";
import type { ScoreEntry } from "@/types/quiz";

import WaitingScreen  from "@/components/screens/WaitingScreen";
import QuestionScreen from "@/components/screens/QuestionScreen";
import RevealScreen   from "@/components/screens/RevealScreen";
import ScoresScreen   from "@/components/screens/ScoresScreen";
import styles from "@/components/screens/screens.module.css";

export default function BeamerPage() {
  const { connected, gameState, question, reveal, scores, answerCount } = useMqtt();

  // Keep track of registered players from scores payloads
  const playersRef = useRef<ScoreEntry[]>([]);
  if (scores) playersRef.current = scores.scores;

  const screen = useMemo(() => {
    const state = gameState?.state ?? "WAITING";

    switch (state) {
      case "WAITING":
        return <WaitingScreen players={playersRef.current} />;

      case "QUESTION":
        return question
          ? <QuestionScreen question={question} remainingS={gameState?.remaining_s ?? 0} voting={false} answerCount={null} />
          : <WaitingScreen players={playersRef.current} />;

      case "VOTING":
        return question
          ? <QuestionScreen question={question} remainingS={gameState?.remaining_s ?? 0} voting={true} answerCount={answerCount} />
          : <WaitingScreen players={playersRef.current} />;

      case "REVEAL":
        return question && reveal
          ? <RevealScreen question={question} reveal={reveal} />
          : <WaitingScreen players={playersRef.current} />;

      case "SCORES":
        return scores
          ? <ScoresScreen scores={scores.scores} />
          : <WaitingScreen players={playersRef.current} />;

      case "ENDED":
        return scores
          ? <ScoresScreen scores={scores.scores} ended />
          : <WaitingScreen players={playersRef.current} />;

      default:
        return <WaitingScreen players={playersRef.current} />;
    }
  // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [gameState, question, reveal, scores, answerCount]);

  return (
    <>
      <div className={`${styles.connectionDot} ${connected ? styles.connected : ""}`} />
      {screen}
    </>
  );
}

"use client";

import { useEffect, useRef, useState } from "react";
import mqtt, { MqttClient } from "mqtt";
import type { GameState, Question, Reveal, Scores, AnswerCount, Players } from "@/types/quiz";

export interface QuizData {
  gameState: GameState | null;
  question: Question | null;
  reveal: Reveal | null;
  scores: Scores | null;
  answerCount: AnswerCount | null;
  players: Players | null;
  connected: boolean;
  publish: (topic: string, payload: object) => void;
}

const BROKER_WS_URL =
  process.env.NEXT_PUBLIC_MQTT_URL ?? "ws://localhost:9001";

export function useMqtt(): QuizData {
  const clientRef = useRef<MqttClient | null>(null);

  const [connected, setConnected]     = useState(false);
  const [gameState, setGameState]     = useState<GameState | null>(null);
  const [question,  setQuestion]      = useState<Question | null>(null);
  const [reveal,    setReveal]        = useState<Reveal | null>(null);
  const [scores,    setScores]        = useState<Scores | null>(null);
  const [answerCount, setAnswerCount] = useState<AnswerCount | null>(null);
  const [players,   setPlayers]       = useState<Players | null>(null);

  useEffect(() => {
    const client = mqtt.connect(BROKER_WS_URL);
    clientRef.current = client;

    client.on("connect", () => {
      setConnected(true);
      client.subscribe([
        "quiz/state",
        "quiz/question",
        "quiz/reveal",
        "quiz/scores",
        "quiz/answer_count",
        "quiz/players",
      ]);
    });

    client.on("disconnect", () => setConnected(false));
    client.on("error", () => setConnected(false));

    client.on("message", (topic: string, payload: Buffer) => {
      try {
        const data = JSON.parse(payload.toString());
        if (topic === "quiz/state")        setGameState(data);
        if (topic === "quiz/question")     setQuestion(data);
        if (topic === "quiz/reveal")       setReveal(data);
        if (topic === "quiz/scores")       setScores(data);
        if (topic === "quiz/answer_count") setAnswerCount(data);
        if (topic === "quiz/players")      setPlayers(data);
      } catch {
        // malformed message — ignore
      }
    });

    return () => { client.end(); };
  }, []);

  const publish = (topic: string, payload: object) => {
    clientRef.current?.publish(topic, JSON.stringify(payload));
  };

  return { connected, gameState, question, reveal, scores, answerCount, players, publish };
}

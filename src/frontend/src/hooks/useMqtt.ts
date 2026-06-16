"use client";

import { useEffect, useRef, useState } from "react";
import mqtt, { MqttClient } from "mqtt";
import type { GameState, Question, Reveal, Scores, AnswerCount, Players } from "@/types/quiz";

/**
 * Schnittstelle für das vom useMqtt-Hook bereitgestellte Spielzustands-Objekt.
 */
export interface QuizData {
  /** Der aktuelle Status der Spiel-State-Machine (z. B. WAITING, VOTING) */
  gameState: GameState | null;
  /** Die Struktur der aktuellen Frage */
  question: Question | null;
  /** Die Antwortstatistiken und richtigen Antworten */
  reveal: Reveal | null;
  /** Das aktuelle Leaderboard / Punkteliste */
  scores: Scores | null;
  /** Anzahl der abgegebenen Stimmen relativ zur Gesamtteilnehmerzahl */
  answerCount: AnswerCount | null;
  /** Die Liste der aktuell registrierten/online Spieler in der Lobby */
  players: Players | null;
  /** Gibt an, ob der Web-Client mit dem MQTT-Broker verbunden ist */
  connected: boolean;
  /** Methode zum Veröffentlichen von Steuerbefehlen auf dem Broker */
  publish: (topic: string, payload: object) => void;
}

// WebSocket-Adresse des MQTT-Brokers (Fallback auf localhost:9001 für Dev-Umgebungen)
const BROKER_WS_URL =
  process.env.NEXT_PUBLIC_MQTT_URL ?? "ws://localhost:9001";

/**
 * Ein React Hook zur Verwaltung der Echtzeit-MQTT-Kommunikation.
 * Baut die WebSocket-Verbindung auf, subskribiert alle relevanten Themen
 * und synchronisiert die Daten mit dem lokalen React-State.
 *
 * @returns QuizData mit dem aktuellen Spielzustand und Steuerungsmethoden.
 */
export function useMqtt(): QuizData {
  const clientRef = useRef<MqttClient | null>(null);

  const [connected, setConnected]     = useState(false);
  const [gameState, setGameState]     = useState<GameState | null>(null);
  const [question,  setQuestion]      = useState<Question | null>(null);
  const [reveal,    setReveal]        = useState<Reveal | null>(null);
  const [scores,    setScores]        = useState<Scores | null>(null);
  const [answerCount,   setAnswerCount]   = useState<AnswerCount | null>(null);
  const [players,       setPlayers]       = useState<Players | null>(null);

  useEffect(() => {
    // 1. Verbindung zum MQTT-Broker über WebSockets herstellen
    const client = mqtt.connect(BROKER_WS_URL);
    clientRef.current = client;

    // 2. Event-Handler für erfolgreiche Verbindung: Kanäle abonnieren
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

    // 3. Event-Handler bei Verbindungsverlust
    client.on("disconnect", () => setConnected(false));
    client.on("error", () => setConnected(false));

    // 4. Zentraler Nachrichtenverteiler
    client.on("message", (topic: string, payload: Buffer) => {
      try {
        const data = JSON.parse(payload.toString());
        if (topic === "quiz/state")        setGameState(data);
        if (topic === "quiz/question")     setQuestion(data);
        if (topic === "quiz/reveal")       setReveal(data);
        if (topic === "quiz/scores")       setScores(data);
        if (topic === "quiz/answer_count")   setAnswerCount(data);
        if (topic === "quiz/players")        setPlayers(data);
      } catch {
        // Ignoriere fehlerhafte JSON-Nachrichten
      }
    });

    // 5. Cleanup bei Demontage der React-Komponente
    return () => { client.end(); };
  }, []);

  /**
   * Hilfsfunktion zum Senden einer Nachricht als JSON-Payload an den Broker.
   */
  const publish = (topic: string, payload: object) => {
    clientRef.current?.publish(topic, JSON.stringify(payload));
  };

  return { connected, gameState, question, reveal, scores, answerCount, players, publish };
}

"use client";

import { useEffect, useRef } from "react";
import type { GameStateValue } from "@/types/quiz";

// Definiert die Events, für die Audio-Synthese-Muster hinterlegt sind
type SoundEvent = "question" | "reveal" | "scores" | "ended";

/**
 * Erzeugt einen einzelnen synthetischen Ton über die Web Audio API.
 *
 * @param ctx Der AudioContext des Browsers
 * @param freq Frequenz in Hertz (z. B. 440 Hz für Kammerton A)
 * @param startTime Startzeitpunkt relativ zum AudioContext-Zeitgeber
 * @param duration Spieldauer des Tons in Sekunden
 * @param type Art des Oszillators (sine, triangle, sawtooth, square)
 * @param volume Lautstärke (Gain-Faktor zwischen 0.0 und 1.0)
 */
function playNote(
  ctx: AudioContext,
  freq: number,
  startTime: number,
  duration: number,
  type: OscillatorType = "triangle",
  volume = 0.25,
) {
  // 1. Oszillator (Klangerzeuger) und GainNode (Lautstärkeregler) instanziieren
  const osc  = ctx.createOscillator();
  const gain = ctx.createGain();

  // 2. Nodes verknüpfen: Oszillator -> Gain -> Lautsprecher
  osc.connect(gain);
  gain.connect(ctx.destination);

  // 3. Eigenschaften setzen
  osc.type = type;
  osc.frequency.setValueAtTime(freq, startTime);
  gain.gain.setValueAtTime(volume, startTime);

  // 4. Exponentialer Lautstärkeabfall für ein natürlicheres Ausklingen (Decay/Release)
  gain.gain.exponentialRampToValueAtTime(0.001, startTime + duration);

  // 5. Wiedergabe starten und automatisch beenden
  osc.start(startTime);
  osc.stop(startTime + duration + 0.05);
}

/**
 * Registrierte Synthesizer-Muster für verschiedene Ereignisse im Quizspiel.
 */
const SOUNDS: Record<SoundEvent, (ctx: AudioContext) => void> = {
  // Eine einfache ansteigende Zweiton-Melodie bei Fragenankündigung
  question: (ctx) => {
    const t = ctx.currentTime;
    playNote(ctx, 440, t,        0.18, "sine", 0.2);
    playNote(ctx, 554, t + 0.18, 0.25, "sine", 0.2);
  },
  // Ein voller C-Dur Dreiklang bei der Auflösung der Antwort
  reveal: (ctx) => {
    const t = ctx.currentTime;
    // C5 Major Akkord: C (523 Hz), E (659 Hz), G (784 Hz)
    // Leicht versetztes Starten für einen reicheren Chorus-artigen Effekt
    playNote(ctx, 523, t,        0.9, "triangle", 0.22);
    playNote(ctx, 659, t + 0.04, 0.9, "triangle", 0.18);
    playNote(ctx, 784, t + 0.08, 0.9, "triangle", 0.15);
  },
  // Ein aufsteigendes Arpeggio bei der Anzeige des Leaderboards
  scores: (ctx) => {
    const t = ctx.currentTime;
    // Tonfolge: C (523 Hz) -> D (587 Hz) -> E (659 Hz) -> G (784 Hz)
    [523, 587, 659, 784].forEach((f, i) => playNote(ctx, f, t + i * 0.1, 0.35, "sine", 0.22));
  },
  // Eine feierliche Fanfare zum Abschluss des Spiels
  ended: (ctx) => {
    const t = ctx.currentTime;
    // Fanfare: C -> E -> G -> C (eine Oktave höher)
    const notes = [523, 659, 784, 1047];
    const times = [0, 0.2, 0.4, 0.7];
    notes.forEach((f, i) => playNote(ctx, f, t + times[i], 0.55, "triangle", 0.25));
  },
};

/**
 * Zuordnung von Spielzuständen zu Audio-Events.
 */
const STATE_SOUND: Partial<Record<GameStateValue, SoundEvent>> = {
  QUESTION: "question",
  REVEAL:   "reveal",
  SCORES:   "scores",
  ENDED:    "ended",
};

/**
 * React Hook zur automatischen Klangerzeugung bei Zustandsübergängen.
 *
 * Beachtet Browser-Sicherheitsrichtlinien und reaktiviert einen eventuell
 * suspendierten AudioContext erst bei Interaktion.
 *
 * @param currentState Der aktuelle State aus dem MQTT-Empfänger
 */
export function useSound(currentState: GameStateValue) {
  const prevStateRef = useRef<GameStateValue | null>(null);
  const ctxRef       = useRef<AudioContext | null>(null);

  useEffect(() => {
    const prev = prevStateRef.current;
    prevStateRef.current = currentState;

    // Kein Ton beim ersten Render-Vorgang oder wenn der Zustand unverändert blieb
    if (prev === null || prev === currentState) return;

    // Sucht das zugeordnete Soundmuster
    const event = STATE_SOUND[currentState];
    if (!event) return;

    try {
      // Lazy-Instanziierung des globalen Audio-Contexts
      if (!ctxRef.current) {
        ctxRef.current = new AudioContext();
      }
      const ctx = ctxRef.current;
      const play = () => SOUNDS[event](ctx);

      // Browser blockieren AudioContext oft im Zustand 'suspended', bis der User klickt.
      // Falls suspendiert, reaktivieren wir ihn per Promise, andernfalls spielen wir direkt.
      ctx.state === "suspended" ? ctx.resume().then(play) : play();
    } catch {
      // Stummes Fehlschlagen, falls Web Audio API vom Browser nicht unterstützt wird
    }
  }, [currentState]);
}

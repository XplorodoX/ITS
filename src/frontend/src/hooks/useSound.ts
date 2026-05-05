"use client";

import { useEffect, useRef } from "react";
import type { GameStateValue } from "@/types/quiz";

type SoundEvent = "question" | "reveal" | "scores" | "ended";

function playNote(
  ctx: AudioContext,
  freq: number,
  startTime: number,
  duration: number,
  type: OscillatorType = "triangle",
  volume = 0.25,
) {
  const osc  = ctx.createOscillator();
  const gain = ctx.createGain();
  osc.connect(gain);
  gain.connect(ctx.destination);
  osc.type = type;
  osc.frequency.setValueAtTime(freq, startTime);
  gain.gain.setValueAtTime(volume, startTime);
  gain.gain.exponentialRampToValueAtTime(0.001, startTime + duration);
  osc.start(startTime);
  osc.stop(startTime + duration + 0.05);
}

const SOUNDS: Record<SoundEvent, (ctx: AudioContext) => void> = {
  question: (ctx) => {
    const t = ctx.currentTime;
    playNote(ctx, 440, t,        0.18, "sine", 0.2);
    playNote(ctx, 554, t + 0.18, 0.25, "sine", 0.2);
  },
  reveal: (ctx) => {
    const t = ctx.currentTime;
    // C5 major chord stab, slightly staggered for richness
    playNote(ctx, 523, t,        0.9, "triangle", 0.22);
    playNote(ctx, 659, t + 0.04, 0.9, "triangle", 0.18);
    playNote(ctx, 784, t + 0.08, 0.9, "triangle", 0.15);
  },
  scores: (ctx) => {
    const t = ctx.currentTime;
    // Ascending arpeggio: C D E G
    [523, 587, 659, 784].forEach((f, i) => playNote(ctx, f, t + i * 0.1, 0.35, "sine", 0.22));
  },
  ended: (ctx) => {
    const t = ctx.currentTime;
    // Fanfare: C E G C (one octave up)
    const notes = [523, 659, 784, 1047];
    const times = [0, 0.2, 0.4, 0.7];
    notes.forEach((f, i) => playNote(ctx, f, t + times[i], 0.55, "triangle", 0.25));
  },
};

const STATE_SOUND: Partial<Record<GameStateValue, SoundEvent>> = {
  QUESTION: "question",
  REVEAL:   "reveal",
  SCORES:   "scores",
  ENDED:    "ended",
};

export function useSound(currentState: GameStateValue) {
  const prevStateRef = useRef<GameStateValue | null>(null);
  const ctxRef       = useRef<AudioContext | null>(null);

  useEffect(() => {
    const prev = prevStateRef.current;
    prevStateRef.current = currentState;

    if (prev === null || prev === currentState) return;

    const event = STATE_SOUND[currentState];
    if (!event) return;

    try {
      if (!ctxRef.current) {
        ctxRef.current = new AudioContext();
      }
      const ctx = ctxRef.current;
      const play = () => SOUNDS[event](ctx);
      ctx.state === "suspended" ? ctx.resume().then(play) : play();
    } catch {
      // Web Audio not supported — fail silently
    }
  }, [currentState]);
}

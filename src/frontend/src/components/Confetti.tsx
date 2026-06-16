"use client";

import { useEffect, useRef } from "react";

interface ConfettiPiece {
  x: number;
  y: number;
  size: number;
  color: string;
  rotation: number;
  rotationSpeed: number;
  speedX: number;
  speedY: number;
  opacity: number;
}

const COLORS = ["#6c63ff", "#22c55e", "#eab308", "#ef4444", "#3b82f6", "#ec4899", "#f97316"];

export default function Confetti() {
  const canvasRef = useRef<HTMLCanvasElement>(null);

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;

    const ctx = canvas.getContext("2d");
    if (!ctx) return;

    let animationFrameId: number;
    let pieces: ConfettiPiece[] = [];

    const resizeCanvas = () => {
      canvas.width = window.innerWidth;
      canvas.height = window.innerHeight;
    };

    window.addEventListener("resize", resizeCanvas);
    resizeCanvas();

    // Create initial pieces
    const createPiece = (x: number, y: number, isInitial = false): ConfettiPiece => {
      return {
        x,
        y: isInitial ? Math.random() * canvas.height : y,
        size: Math.random() * 8 + 6,
        color: COLORS[Math.floor(Math.random() * COLORS.length)],
        rotation: Math.random() * 360,
        rotationSpeed: (Math.random() - 0.5) * 5,
        speedX: (Math.random() - 0.5) * 6,
        speedY: Math.random() * 4 + 2,
        opacity: 1,
      };
    };

    for (let i = 0; i < 150; i++) {
      pieces.push(createPiece(Math.random() * canvas.width, -20, true));
    }

    const animate = () => {
      ctx.clearRect(0, 0, canvas.width, canvas.height);

      for (let i = pieces.length - 1; i >= 0; i--) {
        const p = pieces[i];

        p.x += p.speedX;
        p.y += p.speedY;
        p.rotation += p.rotationSpeed;

        // Apply slight gravity/drift
        p.speedY += 0.05;
        p.speedX += Math.sin(p.y / 30) * 0.05;

        // Render confetti piece (small rotated rectangle)
        ctx.save();
        ctx.translate(p.x, p.y);
        ctx.rotate((p.rotation * Math.PI) / 180);
        ctx.fillStyle = p.color;
        ctx.globalAlpha = p.opacity;
        ctx.fillRect(-p.size / 2, -p.size / 2, p.size, p.size);
        ctx.restore();

        // Recycle or remove pieces that are offscreen
        if (p.y > canvas.height) {
          if (pieces.length < 180) {
            pieces[i] = createPiece(Math.random() * canvas.width, -10);
          } else {
            pieces.splice(i, 1);
          }
        }
      }

      animationFrameId = requestAnimationFrame(animate);
    };

    animate();

    return () => {
      window.removeEventListener("resize", resizeCanvas);
      cancelAnimationFrame(animationFrameId);
    };
  }, []);

  return (
    <canvas
      ref={canvasRef}
      style={{
        position: "fixed",
        top: 0,
        left: 0,
        width: "100%",
        height: "100%",
        pointerEvents: "none",
        zIndex: 9999,
      }}
    />
  );
}

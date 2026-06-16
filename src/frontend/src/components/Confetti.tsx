"use client";

import { useEffect, useRef } from "react";

// Strukturdefinition für ein einzelnes Konfetti-Partikel (Schnipsel)
interface ConfettiPiece {
  /** X-Position auf dem Canvas */
  x: number;
  /** Y-Position auf dem Canvas */
  y: number;
  /** Kantenlänge des Schnipsels in Pixeln */
  size: number;
  /** Füllfarbe des Schnipsels (Hex-Code) */
  color: string;
  /** Aktueller Rotationswinkel in Grad */
  rotation: number;
  /** Rotationsgeschwindigkeit (Änderung pro Frame) */
  rotationSpeed: number;
  /** Driftgeschwindigkeit in X-Richtung */
  speedX: number;
  /** Fallgeschwindigkeit in Y-Richtung */
  speedY: number;
  /** Transparenzfaktor (0.0 bis 1.0) */
  opacity: number;
}

// Harmonische Farbpalette für die Konfetti-Schnipsel
const COLORS = ["#6c63ff", "#22c55e", "#eab308", "#ef4444", "#3b82f6", "#ec4899", "#f97316"];

/**
 * Confetti-Komponente.
 *
 * Rendert ein bildschirmfüllendes HTML5-Canvas Element im Vordergrund
 * und simuliert darauf ein physikalisches Partikelsystem mit Schwerkraft und Winddrift.
 */
export default function Confetti() {
  const canvasRef = useRef<HTMLCanvasElement>(null);

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;

    const ctx = canvas.getContext("2d");
    if (!ctx) return;

    let animationFrameId: number;
    let pieces: ConfettiPiece[] = [];

    // Passt die Canvas-Auflösung dynamisch an das Browserfenster an
    const resizeCanvas = () => {
      canvas.width = window.innerWidth;
      canvas.height = window.innerHeight;
    };

    window.addEventListener("resize", resizeCanvas);
    resizeCanvas();

    /**
     * Erstellt ein neues Konfetti-Partikel mit zufälligen physikalischen Werten.
     *
     * @param x Start-X-Koordinate
     * @param y Start-Y-Koordinate
     * @param isInitial Gibt an, ob das Partikel beim ersten Laden (auf dem ganzen Screen verteilt) erzeugt wird
     */
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

    // Initialen Partikelpool anlegen (verteilt über den Bildschirm)
    for (let i = 0; i < 150; i++) {
      pieces.push(createPiece(Math.random() * canvas.width, -20, true));
    }

    /**
     * Die zentrale Animationsschleife (Frame-Loop)
     */
    const animate = () => {
      // Canvas säubern
      ctx.clearRect(0, 0, canvas.width, canvas.height);

      // Partikel rückwärts durchlaufen, damit Löschen per splice keine Indexfehler wirft
      for (let i = pieces.length - 1; i >= 0; i--) {
        const p = pieces[i];

        // 1. Positionen basierend auf Geschwindigkeiten anpassen
        p.x += p.speedX;
        p.y += p.speedY;
        p.rotation += p.rotationSpeed;

        // 2. Physikalische Kräfte simulieren: Konstante Schwerkraft und sinusförmige Winddrift
        p.speedY += 0.05;
        p.speedX += Math.sin(p.y / 30) * 0.05;

        // 3. Transformationsmatrix anwenden und Schnipsel rendern
        ctx.save();
        ctx.translate(p.x, p.y);
        ctx.rotate((p.rotation * Math.PI) / 180);
        ctx.fillStyle = p.color;
        ctx.globalAlpha = p.opacity;
        // Zeichnet ein kleines gedrehtes Quadrat um den Ursprung
        ctx.fillRect(-p.size / 2, -p.size / 2, p.size, p.size);
        ctx.restore();

        // 4. Aus dem Bildschirm gefallene Partikel recyceln oder entfernen
        if (p.y > canvas.height) {
          if (pieces.length < 180) {
            // Recyceln: Oben neu einwerfen
            pieces[i] = createPiece(Math.random() * canvas.width, -10);
          } else {
            // Zu viele Partikel: Löschen
            pieces.splice(i, 1);
          }
        }
      }

      // Nächsten Frame anfordern
      animationFrameId = requestAnimationFrame(animate);
    };

    animate();

    // Event-Listener entfernen und Animationsschleife stoppen bei Component-Unmount
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
        pointerEvents: "none", // Keine Interaktion blockieren
        zIndex: 9999,          // Über allen anderen Elementen anzeigen
      }}
    />
  );
}

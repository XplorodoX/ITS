#!/usr/bin/env python3
"""
Präsentation → PDF (Screenshot-Methode, pixel-perfekt)

Jede Folie wird bei nativem 1920×1080 aufgenommen — das JS-Scaling
greift nicht (Viewport = Canvas-Größe → scale = 1.0, kein Transform).

Verwendung:
    python3 export_pdf.py
    python3 export_pdf.py --out ~/Desktop/ITS.pdf
    python3 export_pdf.py --dpi 200
"""

import subprocess
import sys
import io
import argparse
from pathlib import Path


def ensure_deps():
    # playwright
    try:
        from playwright.sync_api import sync_playwright  # noqa: F401
    except ImportError:
        print("Installiere playwright …")
        subprocess.check_call([sys.executable, "-m", "pip", "install", "playwright"])
        print("Installiere Chromium …")
        subprocess.check_call([sys.executable, "-m", "playwright", "install", "chromium"])

    # Pillow
    try:
        from PIL import Image  # noqa: F401
    except ImportError:
        print("Installiere Pillow …")
        subprocess.check_call([sys.executable, "-m", "pip", "install", "Pillow"])

    from playwright.sync_api import sync_playwright
    from PIL import Image
    return sync_playwright, Image


def main():
    parser = argparse.ArgumentParser(description="Präsentation → PDF")
    parser.add_argument("--out", default=None, help="Ausgabepfad (Standard: präsentation.pdf)")
    parser.add_argument("--dpi", type=int, default=150, help="PDF-Auflösung (Standard: 150)")
    args = parser.parse_args()

    here = Path(__file__).resolve().parent
    html = here / "index.html"
    out  = Path(args.out).resolve() if args.out else here / "präsentation.pdf"

    if not html.exists():
        print(f"Fehler: {html} nicht gefunden.")
        sys.exit(1)

    sync_playwright, Image = ensure_deps()

    print(f"  Quelle : {html.name}")
    print(f"  Ziel   : {out}")
    print(f"  DPI    : {args.dpi}\n")

    images = []

    with sync_playwright() as p:
        browser = p.chromium.launch(args=["--no-sandbox"])

        # Viewport = 1920×1080 → JS-scale() ergibt s=1.0, kein Transform
        page = browser.new_page(viewport={"width": 1920, "height": 1080})

        print("Lade Präsentation …")
        page.goto(html.as_uri(), wait_until="networkidle")
        page.wait_for_timeout(1500)  # Fonts, SVGs, Animationen

        # Anzahl Folien und IDs direkt aus dem DOM lesen
        slide_ids = page.evaluate("""
            () => [...document.querySelectorAll('.slide')].map(s => s.id || '')
        """)
        total = len(slide_ids)
        print(f"Folien gefunden: {total}\n")

        for i, sid in enumerate(slide_ids):
            label = sid if sid else f"Folie {i+1}"
            print(f"  [{i+1:02d}/{total}] #{label}", end="", flush=True)

            # Direkte DOM-Manipulation — go() ist in IIFE und nicht global erreichbar
            page.evaluate(f"""
                () => {{
                    document.querySelectorAll('.slide').forEach(s => s.classList.remove('active'));
                    const s = document.querySelectorAll('.slide')[{i}];
                    if (s) s.classList.add('active');
                }}
            """)
            page.wait_for_timeout(150)  # CSS-Transition abwarten

            raw = page.screenshot(full_page=False)
            images.append(Image.open(io.BytesIO(raw)).convert("RGB"))
            print(" ✓")

        browser.close()

    print(f"\nErstelle PDF mit {len(images)} Seiten …")
    images[0].save(
        str(out),
        save_all=True,
        append_images=images[1:],
        resolution=args.dpi,
    )

    size = out.stat().st_size
    print(f"\n✓  Fertig!  {out.name}  ({size // 1024} KB)")


if __name__ == "__main__":
    main()

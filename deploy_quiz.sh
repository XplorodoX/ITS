#!/usr/bin/env bash
# ==============================================================================
# AALeC Quiz — Automated Installer & Flashing Script
# ==============================================================================
# Dieses Skript automatisiert das Starten der Docker/Podman Container (Frontend,
# Backend und MQTT-Broker) und flasht die neueste Firmware direkt aus den
# GitHub Releases auf den ESP8266 AALeC Controller.
#
# Voraussetzungen:
# - Docker / Podman (inkl. Compose)
# - Python 3
# - curl
# ==============================================================================

set -euo pipefail

# ANSI Farbcodes für schöne Ausgaben
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}==============================================================================${NC}"
echo -e "${BLUE}             AALeC Quiz — Automatischer Installer & Flasher                 ${NC}"
echo -e "${BLUE}==============================================================================${NC}"

# --- 1. Container-Dienste starten ---
echo -e "\n${YELLOW}[1/4] Starte Docker/Podman Container...${NC}"

COMPOSE_CMD=""
if command -v podman-compose &> /dev/null; then
  COMPOSE_CMD="podman-compose"
elif command -v docker-compose &> /dev/null; then
  COMPOSE_CMD="docker-compose"
elif docker compose version &> /dev/null; then
  COMPOSE_CMD="docker compose"
fi

if [ -n "$COMPOSE_CMD" ]; then
  echo -e "Gefundenes Compose-Tool: ${GREEN}$COMPOSE_CMD${NC}"
  echo "Führe '$COMPOSE_CMD up -d' aus..."
  $COMPOSE_CMD up -d
  echo -e "${GREEN}✓ Container wurden gestartet/aktualisiert!${NC}"
else
  echo -e "${RED}⚠️ Kein Compose-Tool (podman-compose/docker-compose) gefunden.${NC}"
  echo "Verzeichnis-Dienste konnten nicht gestartet werden. Überspringe diesen Schritt..."
fi

# --- 2. Neueste Firmware von GitHub herunterladen ---
echo -e "\n${YELLOW}[2/4] Lade neueste Firmware herunter...${NC}"

REPO="XplorodoX/ITS"
API_URL="https://api.github.com/repos/$REPO/releases/latest"

echo -e "Rufe Release-Informationen von GitHub ab: ${BLUE}$API_URL${NC}"

# Hole die Download-URL für firmware.bin über Python, um fehleranfälliges Greppen zu vermeiden
DOWNLOAD_URL=$(curl -sL "$API_URL" | python3 -c "
import sys, json
try:
    data = json.load(sys.stdin)
    assets = data.get('assets', [])
    url = next(a['browser_download_url'] for a in assets if a['name'] == 'firmware.bin')
    print(url)
except Exception as e:
    print('ERROR', file=sys.stderr)
    sys.exit(1)
" 2>/dev/null || echo "")

if [ -z "$DOWNLOAD_URL" ]; then
  echo -e "${RED}❌ Fehler: Die 'firmware.bin' konnte nicht im neuesten Release gefunden werden.${NC}"
  echo "Bitte stelle sicher, dass Releases auf GitHub existieren und eine 'firmware.bin' angehängt ist."
  exit 1
fi

echo -e "Lade Firmware herunter von: ${GREEN}$DOWNLOAD_URL${NC}"
curl -L -o firmware.bin "$DOWNLOAD_URL"
echo -e "${GREEN}✓ 'firmware.bin' erfolgreich heruntergeladen!${NC}"

# --- 3. Seriellen Port des Controllers ermitteln ---
echo -e "\n${YELLOW}[3/4] Suche angeschlossenen AALeC Controller...${NC}"

PORT=""
while true; do
  PORTS=()
  # Suche nach gängigen USB-zu-UART Bridge-Dateien auf macOS
  for p in /dev/cu.usbserial-* /dev/cu.wchusbserial* /dev/cu.usbmodem*; do
    if [ -e "$p" ]; then
      PORTS+=("$p")
    fi
  done

  if [ ${#PORTS[@]} -gt 0 ]; then
    break
  fi

  echo -e "${YELLOW}⚠️ Kein angeschlossener Controller über USB gefunden.${NC}"
  echo "Bitte stelle sicher, dass das Gerät angeschlossen und eingeschaltet ist."
  read -p "Drücke [Enter] um erneut zu scannen, oder tippe 'skip' zum Überspringen: " choice
  if [ "$choice" = "skip" ]; then
    echo -e "${YELLOW}⏭️ Firmware-Flashen übersprungen. Lösche temporäre Dateien...${NC}"
    rm -f firmware.bin
    exit 0
  fi
done

# Port auswählen falls mehrere Geräte angeschlossen sind
if [ ${#PORTS[@]} -gt 1 ]; then
  echo -e "${YELLOW}Mehrere serielle Ports gefunden:${NC}"
  for i in "${!PORTS[@]}"; do
    echo "  [$i] ${PORTS[$i]}"
  done
  while true; do
    read -p "Wähle den passenden Port (0-$((${#PORTS[@]} - 1))): " idx
    if [[ "$idx" =~ ^[0-9]+$ ]] && [ "$idx" -lt ${#PORTS[@]} ]; then
      PORT="${PORTS[$idx]}"
      break
    fi
    echo -e "${RED}Ungültige Auswahl.${NC}"
  done
else
  PORT="${PORTS[0]}"
fi

echo -e "Verwende seriellen Port: ${GREEN}$PORT${NC}"

# --- 4. Firmware flashen mit virtuellem Python-Environment ---
echo -e "\n${YELLOW}[4/4] Flashe Firmware auf den AALeC Controller...${NC}"

VENV_DIR=".venv_flash"

echo "Erstelle temporäre virtuelle Python-Umgebung in '$VENV_DIR'..."
python3 -m venv "$VENV_DIR"

# Aktiviere die venv temporär
# shellcheck disable=SC1091
source "$VENV_DIR"/bin/activate

echo "Installiere Flash-Tool 'esptool' über pip..."
pip install -q --upgrade pip
pip install -q esptool

echo -e "${BLUE}Starte Flash-Vorgang...${NC}"
# Führe das installierte esptool aus
esptool.py --chip esp8266 --port "$PORT" --baud 115200 write_flash 0x0 firmware.bin

# Deaktiviere und lösche die venv
deactivate
echo "Lösche temporäre Umgebungs- und Firmware-Dateien..."
rm -rf "$VENV_DIR"
rm -f firmware.bin

echo -e "\n${GREEN}==============================================================================${NC}"
echo -e "${GREEN}🎉 Fertig! Die Container laufen und der Controller wurde erfolgreich geflasht!${NC}"
echo -e "${GREEN}==============================================================================${NC}"

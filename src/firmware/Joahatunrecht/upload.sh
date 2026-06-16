#!/usr/bin/env bash
# Flashes the already-built firmware.bin (from ./build.sh) to the device.
# Runs on the HOST, not in the container — esptool.py is pure Python and
# needs no Rosetta. We call it directly instead of `pio run -t upload`
# because PlatformIO's SCons build-signature includes the compiler's
# absolute path, which differs between the container (Linux toolchain under
# the cached podman volume) and the host (broken macOS x86_64 toolchain) —
# so `pio run -t upload` always re-triggers a host-side compile and hits the
# Bad CPU type error, even though uploading itself never needs a compiler.
#
# Usage: ./upload.sh [serial-port]
# Defaults to /dev/cu.usbserial-110 if no port is given.

set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")"

PORT="${1:-/dev/cu.usbserial-110}"
BIN=.pio/build/esp12e/firmware.bin
ESPTOOL=~/.platformio/packages/tool-esptoolpy/esptool.py
PYTHON=~/.platformio/penv/bin/python

if [ ! -f "$BIN" ]; then
  echo "error: $BIN not found — run ./build.sh first" >&2
  exit 1
fi

"$PYTHON" "$ESPTOOL" \
  --before default_reset --after hard_reset \
  --chip esp8266 --port "$PORT" --baud 115200 \
  write_flash 0x0 "$BIN"

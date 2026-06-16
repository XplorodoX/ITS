#!/usr/bin/env bash
# Builds this PlatformIO project inside a native arm64 Linux container via
# podman, avoiding the macOS x86_64 xtensa-lx106-elf toolchain (which needs
# Rosetta 2, no longer available on this machine).
#
# Usage: ./build.sh [pio run args...]
# Examples:
#   ./build.sh                  # pio run
#   ./build.sh -t clean
#
# Only compiles. Upload afterwards from the host with upload.sh — NOT with
# `pio run -t upload`. PlatformIO's SCons build signature embeds the
# compiler's absolute path, and that path is necessarily different here (the
# container's Linux toolchain lives under the cached podman volume, the
# host's lives under ~/.platformio and is the broken macOS x86_64 one), so
# `pio run -t upload` on the host always considers the build stale and
# re-triggers a host-side compile — hitting Bad CPU type again. Matching the
# *project* mount path to the host's doesn't fix this (verified): the
# mismatch is in the toolchain path, not the project path.

set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")"

IMAGE=joahatunrecht-builder
VOLUME=joahatunrecht-pio-cache

podman image exists "$IMAGE" || podman build -t "$IMAGE" -f Containerfile .
podman volume exists "$VOLUME" || podman volume create "$VOLUME"

podman run --rm \
  -v "$PWD":/project:Z \
  -v "$VOLUME":/root/.platformio \
  "$IMAGE" \
  pio run "$@"

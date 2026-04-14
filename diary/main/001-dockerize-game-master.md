# 001 — Dockerize Game Master

**Date**: 2026-04-14
**Tool**: Claude Code
**Model**: claude-sonnet-4-6
**Iterations**: 1

## Prompt

**2026-04-14**

kannst du den quiz master auch noch dockerizieren?? damit alles dienste auf docker laufen?

## What was done

- Created `src/backend/Dockerfile` using `python:3.11-slim` + `uv` for dependency installation.
- Created `docker-compose.yml` at the repository root orchestrating two services:
  - `mosquitto` — eclipse-mosquitto:2 with healthcheck, exposes 1883 (TCP) and 9001 (WebSockets)
  - `game-master` — builds from `src/backend`, waits for mosquitto to be healthy before starting
- The broker hostname defaults to `mosquitto` (Docker service name) so no config change is needed.
- `BROKER` and `QUESTIONS` can be overridden via environment variables or a `.env` file.

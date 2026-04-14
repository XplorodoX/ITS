FROM python:3.11-slim

# Install uv
COPY --from=ghcr.io/astral-sh/uv:latest /uv /usr/local/bin/uv

WORKDIR /app

# Copy dependency files first for layer caching
COPY pyproject.toml uv.lock* ./

# Install dependencies (no project itself, just deps)
RUN uv sync --no-install-project

# Copy source
COPY game_master.py ./
COPY questions.json ./

ENV PATH="/app/.venv/bin:$PATH" \
    PYTHONUNBUFFERED=1

ENTRYPOINT ["python", "game_master.py"]
CMD ["--broker", "mosquitto", "--questions", "questions.json"]

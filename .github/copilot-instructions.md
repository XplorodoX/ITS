# Copilot Instructions for the Locust Teaching Repository

This is a **teaching copy** of [locustio/locust](https://github.com/locustio/locust),
used in "AI-Supported Software Development" at Hochschule Aalen. GitHub Actions
are disabled. Branch protection is on `master` (1 review required).

## Collaboration Principles

- Be direct and technically accurate. If something is wrong or could be better, say so constructively.
- Understand existing code before proposing changes. Locust has intentional design decisions — look at how similar things are already done before suggesting a different approach.
- Keep changes minimal. One logical change per commit, one purpose per PR. No drive-by refactors.
- Follow the workflow: research the relevant modules/tests, plan the approach, implement focused changes, then validate with `make test` and `uv run ruff check . && uv run ruff format --check`.

## Project Overview

Locust is a **load/performance testing framework**. Users define virtual user
behavior in Python classes, and Locust spawns thousands of concurrent gevent
greenlets to simulate traffic. It has a real-time web UI (Flask + React) and
supports distributed execution via ZeroMQ.

### Architecture

```
main.py → Environment → Runner → Users (greenlets)
               │             │           │
               │             │           ├── @task methods
               │             │           └── HttpSession / FastHttpSession
               │             ├── UsersDispatcher (allocates users to workers)
               │             └── stats.RequestStats (collects metrics)
               ├── Events (pub/sub: request, test_start, test_stop, ...)
               ├── WebUI (Flask REST API + React frontend on :8089)
               └── LoadTestShape (optional programmatic load curves)
```

**Data flow:** `@task` calls `self.client.get(url)` → `HttpSession` measures
response time → fires `environment.events.request` → `RequestStats` logs it →
web UI polls `/api/stats`.

**Runner hierarchy:** `Runner` (abstract) → `LocalRunner` (single process) /
`MasterRunner` + `WorkerRunner` (distributed via ZMQ).

### Key Modules

- `locust/__init__.py` — Public API exports + gevent monkey-patching (must run before stdlib imports)
- `locust/user/users.py` — `User`, `HttpUser` base classes. Lifecycle: `on_start()` → task loop → `on_stop()`
- `locust/user/task.py` — `@task`, `@tag` decorators, `TaskSet`. Core execution loop: pick task → execute → wait → repeat
- `locust/event.py` — `EventHook` pub/sub system. Key events: `init`, `test_start`, `test_stop`, `request`, `spawning_complete`, `quitting`
- `locust/env.py` — `Environment` class. Central object holding user classes, runner, web UI, stats, events
- `locust/runners.py` — `LocalRunner`, `MasterRunner`/`WorkerRunner`. Controls spawning, stats, shape worker
- `locust/stats.py` — `RequestStats`, `StatsEntry`. Per-request metrics with histogram bucketing and percentiles
- `locust/web.py` — Flask web UI. REST endpoints: `/api/stats`, `/api/start`, `/api/stop`, etc.
- `locust/clients.py` — `HttpSession` (extends `requests.Session`). Wraps HTTP calls with timing + `request` event
- `locust/main.py` — CLI entry point. Parses args, loads locustfiles, creates Environment, starts runner + web UI
- `locust/dispatch.py` — User-to-worker distribution using Kullback-Leibler divergence
- `locust/shape.py` — `LoadTestShape` base class for custom load profiles
- `locust/argument_parser.py` — CLI argument definitions
- `locust/exception.py` — `StopUser`, `InterruptTaskSet`, `RescheduleTask`, `StopTest`

### Key Directories

- `locust/user/` — User model, task system, wait times, sequential/markov tasksets
- `locust/contrib/` — Protocol-specific users: `fasthttp.py`, `mqtt.py`, `socketio.py`, `postgres.py`, `mongodb.py`
- `locust/rpc/` — ZeroMQ master-worker communication (msgpack serialization)
- `locust/webui/` — React + TypeScript frontend (Vite, Yarn)
- `locust/test/` — All tests (~700+ methods, 28 files). Each test file matches a source module.
- `examples/` — 40+ example locustfiles
- `docs/` — Sphinx/RST documentation

## Coding Conventions

- **Line length**: 120 characters
- **Formatter/linter**: Ruff (handles both). Run `uv run ruff check . && uv run ruff format --check`
- **Import order**: `locust` imports MUST come first (before stdlib) because `__init__.py` does gevent monkey-patching. Ruff isort is configured for this.
- **Type checking**: mypy, `python_version = "3.10"`, `ignore_missing_imports = true`
- **Tests**: pytest with `unittest.TestCase` style. Extend `LocustTestCase` or `WebserverTestCase` from `locust/test/testcases.py`. No conftest.py — tests use base classes for setup/teardown.
- **Gevent awareness**: All I/O is cooperative (greenlets). Use `gevent.sleep()`. CPU-bound code blocks all greenlets.

## Common Patterns

**User definition:**
```python
from locust import HttpUser, task, between

class MyUser(HttpUser):
    wait_time = between(1, 3)
    @task(3)
    def browse(self):
        self.client.get("/items")
```

**Event hooks:**
```python
@events.init.add_listener
def on_init(environment, **kwargs):
    pass
```

**Response validation with `catch_response`:**
```python
with self.client.get("/api/data", catch_response=True) as response:
    if response.json().get("status") != "ok":
        response.failure("Bad status")
```

**Control flow exceptions:** `StopUser` (stop user), `InterruptTaskSet` (return to parent), `RescheduleTask` (skip wait), `StopTest` (abort test).

**Wait times:** `between(min, max)`, `constant(sec)`, `constant_pacing(sec)`, `constant_throughput(rate)`.

**Metaclass task collection:** `UserMeta`/`TaskSetMeta` scan for `@task`-decorated methods and build the `tasks` list automatically from `locust_task_weight` attributes.

## Testing

- Run all tests: `make test` or `pytest -vv locust/test`
- Single file: `pytest locust/test/test_stats.py`
- Single method: `pytest locust/test/test_stats.py::TestClass::test_method`
- No CI in this repo — run tests locally before pushing
- Never fix a test by changing a correct assertion — fix the code instead
- When multiple tests fail, focus on the first one (later failures often cascade)

**Writing a test:**
```python
from locust import User, task, constant
from locust.test.testcases import LocustTestCase

class TestMyFeature(LocustTestCase):
    def test_something(self):
        class MyUser(User):
            wait_time = constant(0)
            @task
            def my_task(self):
                pass
        user = MyUser(self.environment)
        # ... assertions
```

**Test utilities** in `locust/test/util.py`: `temporary_file()`, `patch_env()`, `get_free_tcp_port()`, `mock_locustfile()`.

## Benchmarks

Run `make benchmark` (or `uv run python benchmarks/run_benchmarks.py`) to measure
ops/sec for refactoring exercise targets. Students must include before/after
numbers in refactoring PRs.

| Benchmark | Package | What it measures |
| --------- | ------- | ---------------- |
| `setup_logging()` | A (Utilities) | Logging config dict construction |
| `StatsEntry.log()` | B (Statistics) | Per-request stats recording (hot path) |
| `StatsEntry.extend()` | B (Statistics) | Stats entry merging |
| `filter_tasks_by_tags()` | C (User Model) | Task filtering by tag sets |

## Git Workflow

1. Branch from `master`: `git checkout -b feature/<short-description>` (or `fix/`, `refactor/`)
2. Make focused, atomic commits. Imperative mood ("Add retry logic", not "Added").
3. Validate: `make test && uv run ruff check . && uv run ruff format --check`
4. Push and open a PR against `master`. PR title under 70 chars, imperative. Description explains *what* and *why*.
5. Get 1 review (required by branch protection). No force pushes.

## Do NOT Modify

- `.github/workflows/` — Actions are disabled; edits have no effect
- `locust/webui/dist/` — Built artifacts, regenerated by `yarn webui:build`
- `locust/_version.py` — Auto-generated by hatch-vcs
- `Dockerfile*`, Makefile release targets — Not relevant for course work

## AI Diary

Every AI-assisted contribution to this repository is documented in an **AI
Diary** — a transparent log of prompts and the artifacts they produced. The
goal is traceability: reviewers, instructors, and future contributors can see
exactly what was asked, what was generated, and how many iterations it took.

### Why

In a course about AI-supported software development, making AI usage visible
matters. The diary ensures that AI contributions are traceable in the git
history, supports honest reflection on how AI tools are used, and helps the
team learn from each other's prompting strategies.

### Where entries live

```text
diary/<branch-name>/NNN-short-title.md
```

Each feature branch gets its own subfolder under `diary/`. Inside, entries are
numbered markdown files (e.g., `001-add-retry-logic.md`).

### Entry template

```markdown
# NNN — Short Title

**Date**: YYYY-MM-DD
**Tool**: [tool name, e.g., Claude Code, GitHub Copilot, Cursor]
**Model**: [model name, e.g., Claude Opus 4.6, GPT-4.1]
**Iterations**: [number of follow-up prompts needed]

## Prompt

**YYYY-MM-DD HH:MM**

[The full prompt text as given by the user.]

If there were follow-up prompts, add each with its own timestamp:

**YYYY-MM-DD HH:MM**

[Follow-up prompt text.]
```

### Commit convention

Every prompt that creates or modifies code artifacts results in its own commit
containing both the code changes and the corresponding diary entry:

```text
[diary] NNN — Short description of what was prompted
```

This way, `git log` shows a clear trail of AI interactions, and
`git show <hash>` reveals both what was produced and what was asked.

### Rule

Every AI-assisted code change **must** have a corresponding diary entry
committed alongside it. No exceptions — this is how we keep AI usage
transparent in this teaching repository.


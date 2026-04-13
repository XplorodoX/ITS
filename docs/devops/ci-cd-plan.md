# CI/CD Bootstrap Plan

## Goal
Prepare a reliable DevOps baseline before product code is introduced.

## Current State
- Repository contains process docs, issue templates, and initial workflows.
- Product implementation is intentionally deferred.
- Target architecture is split under `src/` into `src/backend` and
	`src/frontend`.

## Baseline Pipeline (Phase 0)
1. Workflow linting (`actionlint`)
2. Markdown linting (`markdownlint-cli2`)
3. Repository structure sanity checks

## Next Phases
1. Phase 1 (Backend starts):
- Add Python formatting and lint checks (`ruff`)
- Add type checks (`mypy`)
- Add unit tests (`pytest`)

2. Phase 2 (Firmware starts):
- Add Arduino compile smoke checks for ESP8266
- Validate board/toolchain versions in CI

3. Phase 3 (Frontend starts, Node/Next.js):
- Add Node version pinning in CI
- Add install/build checks (`npm ci`, `npm run build`)
- Add linting/type checks (`next lint`, TypeScript)
- Add browser smoke test (headless)

4. Phase 4 (Release readiness):
- Add semantic versioning and release artifacts
- Add SBOM and dependency vulnerability scan

## Technical Debt Tracking
- Keep a dedicated `tech-debt` label for backlog items.
- Require CI green status before merge.
- Periodically prune stale items via automation.

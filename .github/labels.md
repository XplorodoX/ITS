# Label Strategy

This repository uses a small, explicit label set to manage technical debt and delivery flow.

## Core Workflow Labels

- `tech-debt`: Non-urgent quality debt that should be tracked and prioritized.
- `blocked`: Work cannot continue due to missing dependency, decision, or access.
- `ready-for-implementation`: Planning and requirements are clear; implementation can start.
- `needs-info`: Issue or PR requires clarifications before progress.

## Type Labels

- `bug`: Incorrect behavior.
- `feature`: New capability request.
- `docs`: Documentation/process changes.
- `ci`: CI/CD workflow or automation change.

## Priority Labels

- `priority:high`
- `priority:medium`
- `priority:low`

## Usage Rules

- Every open item should have exactly one type label.
- Use at most one priority label per item.
- If work is waiting, add `blocked` and explain blocker in a comment.
- If debt is created, link or create a `tech-debt` issue before merge.
- Move planning items to `ready-for-implementation` only when acceptance criteria are explicit.

## Suggested Initial Label Set

Create these labels in GitHub UI:

- `tech-debt` (color `#A371F7`)
- `blocked` (color `#D73A4A`)
- `ready-for-implementation` (color `#0E8A16`)
- `needs-info` (color `#FBCA04`)
- `docs` (color `#1D76DB`)
- `ci` (color `#5319E7`)
- `priority:high` (color `#B60205`)
- `priority:medium` (color `#D93F0B`)
- `priority:low` (color `#FBCA04`)

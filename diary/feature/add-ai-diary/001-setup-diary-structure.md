# 001 — Set up AI Diary structure

**Date**: 2026-03-28
**Tool**: Claude Code
**Model**: Claude Opus 4.6
**Iterations**: 1

## Prompt

**2026-03-28 19:17**

We are introducing an **AI Diary** approach to this teaching repository. The goal is to make AI-assisted contributions transparent and traceable: every prompt that produces a code artifact gets its own commit with a diary entry documenting the interaction.

### What to set up

**1. Create the diary structure:**

Create a `diary/` folder at the repo root with a `README.md` explaining the approach. The diary is organized by feature branch: each branch gets a subfolder named after the branch (e.g., `diary/feature/add-retry-logic/`). Inside each subfolder, diary entries are numbered markdown files.

**2. Diary entry template:**

Each entry follows this format:

```markdown
# NNN — Short Title

**Date**: YYYY-MM-DD
**Tool**: [tool name, e.g., Claude Code, GitHub Copilot, Cursor]
**Model**: [model name, e.g., Claude Opus 4.6, GPT-4.1]
**Iterations**: [number of follow-up prompts needed]

## Prompt

**YYYY-MM-DD HH:MM**

[The full prompt text as given by the user.]

If there were follow-up prompts (corrections, clarifications), add each as a separate entry with its own timestamp:

**YYYY-MM-DD HH:MM**

[Follow-up prompt text.]
```

**3. Commit convention:**

Every prompt that creates or modifies code artifacts should result in its own commit containing:
- The code changes produced by the AI
- A new diary entry documenting the prompt

Commit message format: `[diary] NNN — Short description of what was prompted`

This ensures `git log` shows a clear trail of AI interactions, and `git show <hash>` reveals both what was produced and what was asked.

**4. Update CLAUDE.md:**

Add a "Diary" section to the existing CLAUDE.md explaining:
- What the diary is and why we use it (transparency, traceability, learning)
- Where entries live (`diary/<branch-name>/NNN-short-title.md`)
- The template format
- The commit convention
- That every AI-assisted code change should have a corresponding diary entry

**5. Update `.github/copilot-instructions.md`:**

This file already exists in the repo. Add the same Diary section to it so that Copilot users get the same diary conventions.

### How to do this

1. Create a feature branch: `feature/add-ai-diary`
2. Set up the `diary/` folder and its README
3. Update CLAUDE.md with the Diary section
4. Update `.github/copilot-instructions.md` with the same Diary section
5. **Use the diary approach for this very setup** — create `diary/feature/add-ai-diary/` and add diary entries for each prompt in this conversation that produces artifacts. This is the first use of the system, documenting itself.
6. Commit each step with its diary entry following the convention above.
7. Push the branch and open a PR against `master`.

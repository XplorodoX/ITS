# AI Diary

This directory contains a transparent log of every AI-assisted contribution to
this repository. Each prompt that produces a code artifact gets its own commit
with a diary entry documenting the interaction.

## Why

In a course about AI-supported software development, it matters that we can
trace *what was asked*, *what was produced*, and *how many iterations it took*.
The diary makes AI usage visible in the git history so that reviewers,
instructors, and future contributors can understand the role AI played in each
change.

## Structure

Entries are organized by feature branch:

```
diary/
  feature/add-retry-logic/
    001-add-retry-logic.md
    002-fix-retry-edge-case.md
  fix/stats-percentile-bug/
    001-investigate-percentile-bug.md
```

Each branch gets a subfolder named after the branch. Inside, diary entries are
numbered markdown files (`NNN-short-title.md`).

## Entry Template

```markdown
# NNN — Short Title

**Date**: YYYY-MM-DD
**Tool**: [tool name, e.g., Claude Code, GitHub Copilot, Cursor]
**Model**: [model name, e.g., Claude Opus 4.6, GPT-4.1]
**Iterations**: [number of follow-up prompts needed]

## Prompt

**YYYY-MM-DD HH:MM**

[The full prompt text as given by the user.]

If there were follow-up prompts (corrections, clarifications), add each as a
separate entry with its own timestamp:

**YYYY-MM-DD HH:MM**

[Follow-up prompt text.]
```

## Commit Convention

Every prompt that creates or modifies code artifacts results in its own commit
containing both the code changes and the diary entry:

```
[diary] NNN — Short description of what was prompted
```

This ensures `git log` shows a clear trail of AI interactions, and
`git show <hash>` reveals both what was produced and what was asked.

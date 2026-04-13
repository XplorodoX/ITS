# 001 — Add benchmark infrastructure for refactoring exercises

**Date**: 2026-03-28
**Tool**: Claude Code
**Model**: Claude Opus 4.6
**Iterations**: 1

## Prompt

**2026-03-28 15:00**

We need a simple performance benchmark infrastructure for the teaching copy of
Locust. Students work in teams assigned to one of three exercise packages. Each
package includes a refactoring target. Students run benchmarks before and after
refactoring to confirm they haven't degraded performance, and include the
numbers in their PR description.

The benchmarks must cover all three refactoring targets plus the stats hot path
(`StatsEntry.log()`) since Package B touches the core stats module.

Requirements:
1. Create a `make benchmark` target
2. Output format grouped by package, copy-pasteable into PR descriptions
3. Use `timeit` or `time.perf_counter()` — no pytest-benchmark dependency
4. Live at `benchmarks/run_benchmarks.py`
5. Run in under 10 seconds, produce stable numbers (multiple iterations, report median)
6. Realistic fixtures (~20 tasks with mixed tag sets, populated StatsEntry objects)
7. Update CLAUDE.md and copilot-instructions.md with a Benchmarks section
8. Use the diary approach on `feature/add-benchmarks` branch, open PR against master

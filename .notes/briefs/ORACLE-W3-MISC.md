# ORACLE-W3-MISC — leftover A/B: GrassRenderer, 2dEffect, platform (+ PostEffects/Renderer if still open)

Rules: .notes/agent_rules/v4.md. Worktree via tools/standalone/windows/new_worktree.sh oracle-w3-misc.
## Evidence
WIN_HANDOFF_4 "Next candidates": GrassRenderer 9, 2dEffect 9, platform 8, PostEffects 9, Renderer 10 uncovered A/B (counts as of
10-21; ORACLE-W3-RENDER / POSTFX-RASTER may have taken some — recount with `py tools/standalone/oracle_coverage.py --json`, use `py`,
`python` is the Store stub). Grass: plant placement maths (which plant, where, colour/scale from the seeds); 2dEffect: effect lookup,
sizes, copy/remove; platform: whatever is pure/global (time, paths, string helpers) — OS calls stubbed or class D, say which.
Lessons: TRAP-AUDIT.md (W2_PORT_FAULT_IS_DIFF=1), ORACLE-C-VEH.md continuation 2 (fixture links with /SAFESEH:NO when exe EH frames unwind).
## Do not touch
tools/standalone/gameplay_regress*.sh and its routes (REGRESS-SETUP works there).
## Task
Rows for the A+B functions; fix every mismatch (one commit per fix, asm address and pre-fix failure count; consumer audit; soak before
landing a fix). Target < 5 min per test target. HARD budget under 250k tokens / 100 min.
## Acceptance
Coverage before/after per file and game-wide; 0 PC24 mismatches; report ≤15 lines with start/end time, full report in .notes/reports/ORACLE-W3-MISC.md.

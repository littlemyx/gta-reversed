# ORACLE-W3-MISC — Localisation.cpp + TagManager.cpp classes A+B

Rules: .notes/agent_rules/v3.md. Worktree via tools/standalone/windows/new_worktree.sh oracle-w3-misc.
## Evidence
Coverage JSON: Localisation.cpp 17 uncovered A/B (region-dependent rules the game checks everywhere: blood, nastiness, metric units,
German/French game differences, ...); TagManager.cpp 14 (gang graffiti tags: spray progress, alpha, tag counts, territory hooks).
Lessons: ORACLE-W3-CARENV.md, ORACLE-W3-VIS.md, ORACLE-W3-SCRIPTS.md. Note: the run build must behave like the exe for the version
we target (FrontEndMenuManager language/region settings as the exe reads them) — test with the settings values the exe can have.
## Task
Rows for both files with real fixtures (TagManager's tag array at its exe address with entities/atomics, the localisation globals);
fix every mismatch (one commit per fix, asm address and pre-fix failure count in the subject) or skip with reason. Target < 5 min.
HARD budget under 200k tokens.
## Acceptance
Coverage before/after; 0 PC24 mismatches; report ≤15 lines with start/end time.

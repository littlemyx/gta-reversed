# ORACLE-W3-INTERIOR — exe-oracle coverage for game_sa/Interior classes A+B

Rules: .notes/agent_rules/v3.md. Worktree via tools/standalone/windows/new_worktree.sh oracle-w3-int.
## Evidence
.notes/ORACLE_COVERAGE.md: Interior 113 ported / 0 covered, uncovered A 34, B 8 (furniture placement, interior groups, ped/info
placement inside shops/houses/police stations). Playtest 2 found PopulateInterior node position wrong (U15, cops spawning above CJ in
the LSPD) — this code had no oracle rows at all. Lessons: ORACLE-W3-ENTITY.md, ORACLE-W3-MODELS.md, ORACLE-W3-AUDIO.md (CRT initialisers).
## Task
Rows for the A+B Interior functions with real fixtures (CInterior_c with its tile grid, furniture groups, interior info);
fix every mismatch (one commit per fix, asm address and pre-fix failure count in the subject) or skip with reason. Aim under 300k tokens.
## Acceptance
Coverage before/after; 0 PC24 mismatches; run the new target + any target linking the changed TUs on the rebased tree (the orchestrator
runs the full gate); report ≤15 lines with start/end time.

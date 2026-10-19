# ORACLE-W3-ENTITY — exe-oracle coverage for game_sa/Entity (CEntity/CPhysical/CPlaceable base) classes A+B

Rules: .notes/agent_rules/v3.md. Worktree via tools/standalone/windows/new_worktree.sh oracle-w3-entity.
## Evidence
.notes/ORACLE_COVERAGE.md: game_sa/Entity 150 ported / 38 covered, uncovered A 19, B 30 (base physics/placement code every
entity runs). Template/lessons: ORACLE-W3-MODELS.md, ORACLE-W3-AUDIO.md (+ "open rows": run the exe's CRT initialisers the function
reads), ORACLE-W3-COLL.md (x87 precision), ORACLE-W3-TASKS.md continuation 2.
## Task
Rows for the A+B uncovered Entity functions with real entity fixtures (matrix, col model, model info at the exe addresses); fix every
mismatch (one commit per fix, asm address in the subject, row fails pre-fix with the count) or skip with reason. Keep reads targeted:
aim under 300k tokens.
## Acceptance
Coverage before/after; 0 PC24 mismatches; run_all_tests green on the rebased tree; report ≤15 lines with start/end time.

# ORACLE-W3-EVENTS — exe-oracle coverage for game_sa/Events classes A+B

Rules: .notes/agent_rules/v2.md. Worktree via tools/standalone/windows/new_worktree.sh oracle-w3-ev; configure with
-DGTASA_COMPILE_CACHE=OFF -DGTASA_NO_EDIT_AND_CONTINUE=ON.
## Evidence
.notes/ORACLE_COVERAGE.md: Events 480 ported / 292 covered; uncovered A 12, B 31 (C 65 optional if time allows — fake vtables).
ORACLE-W2 covered many events already (wave2_oracle_test); extend that infrastructure or add wave3_events_oracle_test.
Lessons: .notes/reports/W3-VEH-OPEN.md, W3-PED-OPEN.md (edge-value rows, fixture wiring, CleanX87, no `_exit`).
## Task
Rows for the A+B uncovered Events functions; fix every mismatch (one commit per fix, asm address in the subject; row fails pre-fix) or skip with reason.
## Acceptance
Coverage before/after; 0 PC24 mismatches; run_all_tests green (count stated); Release builds. Report ≤15 lines.

# ORACLE-W3-SCRIPTS — exe-oracle coverage for game_sa/Scripts classes A+B (non-opcode functions)

Rules: .notes/agent_rules/v3.md. Worktree via tools/standalone/windows/new_worktree.sh oracle-w3-scripts.
## Evidence
.notes/ORACLE_COVERAGE.md: Scripts 114 ported / 23 covered, uncovered A 21, B 29. Opcode handlers are covered by opcode in the
script_oracle_* tests; these are the CRunningScript / CTheScripts / script-object helpers (parameter reading, collect/store, local/global
variable access, mission cleanup, script brain, sphere/text helpers) that every mission runs through. The playtest-1 ResetIP stack bug
(U10) lived here. Lessons: ORACLE-W3-CAMERA.md, ORACLE-W3-INTERIOR.md, ORACLE-W3-AUDIO.md ("open rows"), script_oracle_* fixtures.
## Task
Rows for the A+B uncovered Scripts functions with a real script fixture (a CRunningScript with a script buffer holding valid encoded
params, CTheScripts::ScriptSpace at its exe address); fix every mismatch (one commit per fix, asm address and pre-fix failure count in the
subject) or skip with reason. Aim under 300k tokens.
## Acceptance
Coverage before/after; 0 PC24 mismatches; run the new target + targets linking the changed TUs (the orchestrator runs the full gate);
report ≤15 lines with start/end time.

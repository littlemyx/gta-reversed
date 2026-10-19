# ORACLE-W3-TASKS — exe-oracle coverage for game_sa/Tasks/TaskTypes classes A+B

Rules: .notes/agent_rules/v2.md. Worktree via tools/standalone/windows/new_worktree.sh oracle-w3-tasks; cmake flags
-DGTASA_COMPILE_CACHE=OFF -DGTASA_NO_EDIT_AND_CONTINUE=ON.
## Evidence
.notes/ORACLE_COVERAGE.md: Tasks/TaskTypes 2090 ported / 1269 covered (wave2); uncovered A 49, B 120. Ped behaviour is what the
playtests hit most. Infrastructure: wave2 (oracle_alloc.h arena, gen_w2_*.py), wave3 generators (gen_w3_*.py), lessons in
.notes/reports/W3-VEH-OPEN.md, W3-PED-OPEN.md, ORACLE-W3-GAMEPLAY.md (per-class fixtures, edge values, hand rows for out-params).
## Task
Class A first (49), then B in order of the remaining count; new target wave3_tasks_oracle_test or extend wave2. Fix every mismatch
(one commit per fix, asm address in the subject, row fails pre-fix) or skip with reason. Partial landing + state file at the cap is fine.
## Acceptance
Coverage before/after; 0 PC24 mismatches; run_all_tests green (count stated); report ≤15 lines.

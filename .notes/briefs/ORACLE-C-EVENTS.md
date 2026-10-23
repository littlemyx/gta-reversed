# ORACLE-C-EVENTS — class C for game_sa/Events (64) and game_sa/Entity/Ped (38 C + 1 A + 4 B)

Rules: .notes/agent_rules/v4.md. Worktree via tools/standalone/windows/new_worktree.sh oracle-c-events.
## Evidence
ORACLE_COVERAGE.md: Events 480 / 337 covered, uncovered C 64 (S 33, D 46); Entity/Ped 232 / 102, uncovered A 1 B 4 C 38.
Tools: the class-C vtable swap (W3FuncCase(..., vtCls), tests/standalone/wave3_oracle_veh.inc) and the task variant from ORACLE-C-TASKS
(task `this` + ped under test get the exe or port vftable around each call; gen_w3_tasks.py --classes A,B,C --prev); /SAFESEH:NO on
the test target (exe C++ EH frames). Reports: ORACLE-C-VEH.md, ORACLE-C-TASKS.md. `py`, not `python`.
Events: AffectsPed / AffectsPedGroup / TakesPriorityOver / Clone / IsValid / CanBeInterruptedBy / ReportCriminalEvent — they decide
which AI reaction wins, so a mismatch = peds that react differently. Ped: the C functions that call vehicle/task/event virtuals.
## Task
Rows for the uncovered A/B/C functions; fix every mismatch (one commit per fix, asm address, pre-fix failure count, consumer audit;
soak before landing a gameplay fix). Any port-only fault or CReferences difference = a bug: fix it like the exe.
Target < 5 min per test target. HARD budget 250k tokens / 100 min.
## Do not touch
game_sa/Tasks/TaskTypes oracle tests and w3_tasks_skip.tsv (ORACLE-C-TASKS-2 works there).
## Acceptance
Coverage before/after; 0 PC24 mismatches; soak OK; report ≤15 lines with start/end time; full report .notes/reports/ORACLE-C-EVENTS.md.

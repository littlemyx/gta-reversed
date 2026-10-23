# ORACLE-C-TASKS — class C (virtual calls) for game_sa/Tasks/TaskTypes, first wave

Rules: .notes/agent_rules/v4.md. Worktree via tools/standalone/windows/new_worktree.sh oracle-c-tasks.
## Evidence
ORACLE_COVERAGE.md: game_sa/Tasks/TaskTypes 2090 functions, 1355 covered, uncovered A 24 / B 61 / C 143 / S 137 / D 370.
Tool: the class-C vtable-swap helper W3FuncCase(..., vtCls) in tests/standalone/wave3_oracle_veh.inc (ORACLE-C-VEH,
.notes/reports/ORACLE-C-VEH.md incl. continuation 2: link the fixture with /SAFESEH:NO when exe C++ EH frames unwind).
Recount with `py tools/standalone/oracle_coverage.py --json` (`python` is the Store stub).
## Task
Start with the 24 A + 61 B left in TaskTypes (cheap), then class C: task MakeAbortable / CreateNextSubTask / CreateFirstSubTask /
ControlSubTask / ProcessPed bodies whose callees are virtual (ped, vehicle, entity, event vtables). Fixtures: real CPed/CVehicle
objects from the existing wave3 fixtures, task trees built with the real allocators. Fix every mismatch (one commit per fix, asm
address, pre-fix failure count, consumer audit; soak before landing a gameplay fix — wait for build.lock/gpu.lock, never remove them).
Target < 5 min per test target. HARD budget under 250k tokens / 100 min.
## Do not touch
CTaskSimpleFall*, Animation/ (CAnimManager, CAnimBlendAssocGroup) — ANIM-CRASH works there.
## Acceptance
Coverage before/after (TaskTypes A/B/C and game-wide); 0 PC24 mismatches; report ≤15 lines with start/end time; full report .notes/reports/ORACLE-C-TASKS.md.

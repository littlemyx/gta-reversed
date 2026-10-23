# ORACLE-C-TASKS-2 — the TaskTypes skip rows that point at real port bugs

Rules: .notes/agent_rules/v4.md. Worktree via tools/standalone/windows/new_worktree.sh oracle-c-tasks-2.
## Evidence
.notes/reports/ORACLE-C-TASKS.md + tests/standalone/tools/w3_tasks_skip.tsv (70 rows). Priority order:
1. CReferences list difference — the exe registers a reference (RegisterReference / CleanUpOldReference) and the port does not:
   CTaskSimpleIKLookAt::Clone, CTaskComplexEvasiveDive::Clone, CTaskComplexSeekCoverUntilTargetDead::CreateFirstSubTask. A missing
   reference = a dangling entity pointer after the entity is deleted = a real crash candidate in game. grep every task for the same
   pattern (ctor/Clone storing an entity pointer without RegisterReference vs the exe) — consumer audit.
2. Port-only faults where the exe survives: CTaskSimpleSayDeathSample (SayDeathSample), PassObject::CreateFirstSubTask,
   DuckToggle::ProcessPed; assert exceptions in BeInGroup x3, Sunbathe::Destructor, AffectSecondaryBehaviour::ProcessPed,
   GoPickUpEntity::Destructor (an assert that the exe does not have = a port deviation; follow the exe).
3. Created-task byte differences: FollowPedFootsteps, TrackEntity x2, WanderCop.
4. FollowPointRoute::Constructor still kills the process.
## Task
Fix as the exe does (one commit per fix, asm address, pre-fix failure count, consumer audit; soak before landing gameplay fixes).
Un-skip the rows you fix. HARD budget 250k tokens / 100 min.
## Do not touch
CTaskSimpleFall*, Animation/, Shadows/StencilShadows, Collision data (CRASH-HUNT works there).
## Acceptance
Rows fixed / un-skipped, 0 PC24 mismatches, soak OK; report ≤15 lines with start/end time; full report .notes/reports/ORACLE-C-TASKS-2.md.

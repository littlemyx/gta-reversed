# ORACLE-C-MODELS — class C for game_sa/Models (56) and game_sa/Entity (22)

Rules: .notes/agent_rules/v4.md. Worktree via tools/standalone/windows/new_worktree.sh oracle-c-models.
## Evidence
ORACLE_COVERAGE.md: Models 202 / 105 covered, uncovered C 56 (S 13, D 28); Entity (root: CEntity/CPhysical/CPlaceable...) 150 / 87,
uncovered C 22. Tools: the class-C vtable swap (W3FuncCase(..., vtCls), tests/standalone/wave3_oracle_veh.inc), the task variant from
ORACLE-C-TASKS (gen_w3_tasks.py --classes A,B,C --prev; it now recognises EntityRef members — ORACLE-C-TASKS-2), /SAFESEH:NO on the
test target. Reports: ORACLE-C-VEH.md, ORACLE-C-TASKS.md, ORACLE-C-TASKS-2.md (fixture artefacts vs real bugs). `py`, not `python`.
Models: CBaseModelInfo / CClumpModelInfo / CVehicleModelInfo / CPedModelInfo virtuals (SetClump, CreateInstance, DeleteRwObject,
Shutdown, SetAnimFile, ConvertAnimFileIndex, GetTimeInfo...). Entity: CEntity/CPhysical virtual bodies.
## Task
Rows for the uncovered A/B/C; fix every mismatch (one commit per fix, asm address, pre-fix failure count, consumer audit; soak before
landing a gameplay fix). Distinguish fixture artefacts from real bugs before fixing. Target < 5 min per test target.
HARD budget 250k tokens / 100 min — at 200k tokens stop starting new rows and land.
## Do not touch
Events/ and Entity/Ped oracle tests (ORACLE-C-EVENTS works there).
## Acceptance
Coverage before/after; 0 PC24 mismatches; soak OK; report ≤15 lines with start/end time; full report .notes/reports/ORACLE-C-MODELS.md.

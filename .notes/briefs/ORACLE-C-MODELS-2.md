# ORACLE-C-MODELS-2 — RW-bound Models class C + Entity root class C (22)

Rules: .notes/agent_rules/v4.md. Worktree via tools/standalone/windows/new_worktree.sh oracle-c-models-2.
## Evidence
.notes/reports/ORACLE-C-MODELS.md: 21 rows landed (bfafeb81), all pass. Left: CClumpModelInfo frame walkers + SetClump,
CAtomicModelInfo::SetAtomic, CWeaponModelInfo::SetClump, CVehicleModelInfo RW callbacks — recipe in the report (patch
RwFrameForAllChildren 0x7F0DC0 on both sides to one host stub, set the frame-plugin offset global 0x8D6090; never draw the CBaseModelInfo
vftable: _purecall in GetModelType aborts the process). Entity root (CEntity / CPhysical / CPlaceable virtual bodies): 22 class C, not started.
## Task
Rows for these; fix every real mismatch (one commit per fix, asm address, pre-fix failure count, consumer audit; soak before landing).
Distinguish fixture artefacts from bugs. HARD budget 250k tokens / 100 min — at 200k stop starting new rows and land.
## Do not touch
Events/ and Entity/Ped oracle tests (ORACLE-C-EVENTS-2 works there).
## Acceptance
Coverage before/after; 0 PC24 mismatches; report ≤15 lines with start/end time; full report appended to .notes/reports/ORACLE-C-MODELS.md.

# ORACLE-C-ENTITY — Entity root class C (22) + the Models leftovers

Rules: .notes/agent_rules/v4.md. Worktree via tools/standalone/windows/new_worktree.sh oracle-c-entity.
## Evidence
ORACLE_COVERAGE.md: game_sa/Entity (root: CEntity / CPhysical / CPlaceable / CObject-free parts) 150 / 87, uncovered C 22.
Models leftovers from .notes/reports/ORACLE-C-MODELS.md (both sections): CClumpModelInfo::SetClump, CWeaponModelInfo::SetClump, the
CVehicleModelInfo RW callbacks. Tools: class-C vtable swap (W3FuncCase(..., vtCls)), W3M_ONLY=<substring> row filter, the Models target
links PipelinePlugin / 2dEffect / CustomBuilding* / CarFXRenderer / RwHelper; /SAFESEH:NO. `py`, not `python`.
## Task
Rows for these; fix every real mismatch (one commit per fix, asm address, pre-fix failure count, consumer audit; soak before landing).
HARD budget 250k tokens / 100 min — at 200k stop starting new rows and land.
## Acceptance
Coverage before/after (measure "before" first); 0 PC24 mismatches; report ≤15 lines with start/end time; full report .notes/reports/ORACLE-C-ENTITY.md.
Data dirs for soaks: put them under C:/gta-scratch/<id>/ (not C:/), and leave them; never rm -rf a dir that holds junctions.

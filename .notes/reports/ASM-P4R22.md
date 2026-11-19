# ASM-P4R22: exe-commented Entity/root functions with no verdict, re-verified against the exe

Result TSV: .notes/asm_sweep/P4R22.result.tsv. Candidate list (354 rows, exe-comment bodies, zone-filtered): C:/gta-scratch/p4r22/cand.tsv.

| item | count |
|---|---|
| compared | 21 |
| MATCH | 20 |
| FIXED | 1 |
| NOT_REACHED | ~330 (budget) |

Fix: CAutomobile::GetCarPitch (exe 0x6A6050) - the 2D magnitude was rounded to float before fpatan; the exe keeps the sqrt on the x87 stack (same as GetCarRoll). Commit d73ddaff9.

Observation: unlike the Task functions in P4R18, the Entity/root bodies with exe-address comments were mostly genuinely audited (1 real deviation in 21).
Remaining: root-file rows (CWeapon/CCarCtrl/...) go to P4R23 per the orchestrator zone split; Entity rows left: CEntity wind matrices, CVehicle::DoBladeCollision.

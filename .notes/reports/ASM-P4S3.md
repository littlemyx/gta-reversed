# ASM-P4S3: ped tasks + CPed/CPlayerPed (oracle-only, 25 rows)

Compared 17 of 25 (plus the GetWeaponSkill(type) overload behind a symbol-mismatch row): 16 MATCH, 2 FIXED, 8 NOT_REACHED (budget).
Table: .notes/asm_sweep/P4S3.result.tsv.

FIXED
- 879b523f4 CTaskComplexDiveFromAttachedEntityAndGetUp::CreateFirstSubTask: vehicle dive dir = entity right/left dir, not bbox plane normal.
- 3a9cda6d8 CPed::GetWeaponSkill(type): player POOR only when stat < req (not <=).

NOT_REACHED: ProcessPlayerWeapon, KillPedOnFootMelee::CreateNextSubTask, FollowNodeRoute::CalcGoToTaskType, RoadRage::ControlSubTask,
FollowPedFootsteps::CreateSubTask, TrackEntity::CalcMoveRatio, CPed::CalculateNewVelocity, HoldEntity::DropEntity.
Regress left to the gate.

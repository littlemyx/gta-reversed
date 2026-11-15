# ASM-P4S3b (continuation of P4S3, 8 rows)

7 of 8 compared (asmsig annot, instruction by instruction): 2 MATCH, 5 FIXED, 1 NOT_REACHED (ProcessPlayerWeapon, 1771 insns, budget).
Build StandaloneRelease ok. StandaloneRW / run_all_tests skipped (fresh worktree has no StandaloneRW); gameplay_regress left to the gate.

| function | verdict | commit |
|---|---|---|
| CTaskComplexKillPedOnFootMelee::CreateNextSubTask | FIXED | 4872230d5 |
| CTaskComplexFollowNodeRoute::CalcGoToTaskType | FIXED | b28dd62f9 |
| CTaskComplexRoadRage::ControlSubTask | FIXED | 66253316f |
| CTaskComplexFollowPedFootsteps::CreateSubTask | MATCH | - |
| CTaskComplexTrackEntity::CalcMoveRatio | MATCH | - |
| CPed::CalculateNewVelocity | FIXED | ecef593fc |
| CTaskSimpleHoldEntity::DropEntity | FIXED | e385cb66a |

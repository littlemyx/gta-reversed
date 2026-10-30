# ASM-S60b - 2026-10-30
Table: S60b.result.tsv (17 rows: the 14 CTaskAllocator* rows, CTask::operator delete, CTaskComplex scalar deleting dtor, CTaskManager dtor).
Result: 14 MATCH + 3 FIXED (the TSV row "CTask::operator new" at 0x61A5B0 is the exe's delete; operator new 0x61A5A0 is outside the slice).

Fixes (one commit each):
- CTaskAllocatorKillOnFoot::AllocateTasks (0x69BCA0, 37fec9f74): the look-about that follows the armed kill task lasts 1000 + rand*1000 ms (exe: 1000 - (int)(rand * -1000)); the port used 0..999 ms. The "already killing" skip test also covers TASK_COMPLEX_KILL_PED_ON_FOOT_STEALTH (0x404; the exe skips 0x404 and 0x405).
- CTaskAllocatorAttack::ProcessGroup (0x69D0C0, 8882115b8): the periodic re-allocation is a direct call of CTaskAllocatorAttack::AllocateTasks (also in the inlined copy at 0x69D110). The port called the virtual AllocateTasks, so a CTaskAllocatorPlayerCommandAttack re-ran its own ped-type allocation every 3 s instead of the KillOnFoot allocation.
- CTaskAllocatorPlayerCommandAttack::AllocateTasks (0x69C4D0, b36248f7a): the single-target path is taken for a negative ped type (`cmp [this+0x1C], 0; jl`, PED_TYPE_NONE), the port used IsPedTypePlayer (types 0 / 1, which the exe sends down the ped-type path). The formation target pointer starts null (exe: stack garbage when the formation has no target for the ped).

MATCH details: slot ranges: all member loops run slots 0..7 (the leader is slot 7) with a player skip where the exe has one; ComputeClosestPeds (extended-precision dz2+dx2+dy2, strict < greedy pairing, FLT_MAX wipes, leader / first-7 fallback), KillThreatsBasic / RandomGroup (melee seek-cover test) / Driveby (BE_IN_GROUP in primary slot 3, driver skip, GangDriveBy args), both SetTask slot -1 / force 0 forms, destructors, IsFinished (8 pairs), CTask delete (slot flag 0x80, first-free lowered), CTaskComplex dtor, CTaskManager dtor (primary 0..4, secondary 0..5).
Open: none.
Tests: wave2_oracle_test (RW_EXE_ORACLE = GTA_EXE = compact exe, rebuilt after the last edit): 1142 functions, mismatches PC24 0 / 0, PC53 0 / 0. gameplay_regress combat: PASS=14. v6 pre-commit grep clean. PortableWin64: TaskAllocatorAttack / KillOnFoot / PlayerCommandAttack compile with 0 errors (cl /Zs with the main tree's PortableWin64 compile commands pointed at the worktree).

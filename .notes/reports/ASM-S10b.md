# ASM-S10b (simple car tasks: CarDrive, CarAlign, CarCloseDoor, CarFallOut, CarGetIn, BeHitWhileMoving tail)

Scope: .notes/asm_sweep/S10b.tsv, 26 rows. Compared 26, fixed 4 functions (5 deviations), match 22, not reached 0. Result table: S10b.result.tsv.

| Function | Verdict | Commit |
|---|---|---|
| CarGetIn::ProcessPed (0x64DB30) | FIXED: BMX move force is forward * (timestep*mass*0.01), scalar first | a96d3729 |
| CarCloseDoorFromOutside::StartAnim (0x64B080) | FIXED: strict `>` in the ped-in-air check; AddAnimation instead of BlendAnimation(1000); MakeAbortable passes the ped to ProcessOpenDoor | 1f67419b |
| CarCloseDoorFromInside::ProcessPed (0x64D9A0) | FIXED: ProcessOpenDoor gets the ped (port helper always passed null; also MakeAbortable 0x64AF99) | 5bc63553 |
| CarFallOut::ProcessPed (0x64E2C0) | FIXED: ProcessOpenDoor time = anim current time (was 1.0) | d2fadad7 |
| All 10 CarDrive rows, CarAlign (3), CarCloseDoor ComputeAnimID/StartAnim(Inside)/SetPedPosition, CarFallOut StartAnim/SetPedPosition, CarGetIn SetPedPosition, BeHitWhileMoving::StartAnim tail | MATCH | - |

Jump tables of both CloseDoor ComputeAnimID decoded from the exe (not enum order). Tests: see the end of this file.

Tests (worktree build of this branch): wave2_oracle_test with RW_EXE_ORACLE=GTA_EXE=gta_sa_compact.exe: 1142 functions, PC24 0 / 0, PC53 0 / 0 mismatches. gameplay_regress combat (StandaloneRelease): PASS=14. StandaloneRelease build clean.

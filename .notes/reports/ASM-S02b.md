# ASM-S02b (rest of S02) - 2026-10-28
Result: 11 of 11 compared (7 MATCH, 4 FIXED in 4 commits, table S02b.result.tsv). CopInCar::ControlSubTask now fully covered (0x68FD50-0x6901E9).
Fixes:
- 80dd3585 CTaskComplexCopInCar::ControlSubTask (0x68FDC3-0x6901E9): POLICE_PURSUIT case returned when m_IsDriver (exe: when NOT driver) and lacked the CVehicle::IsUpsideDown check; SIMPLE_CAR_DRIVE case fell through to creating the drive mission when Timer1 was not out of time (exe returns the sub task), and the mission creation did not clear Timer1.started; NaN-form health / distance compares.
- 4b1f19ec CTaskComplexDestroyCarMelee::CreateFirstSubTask (0x62DB20): arrive-range sphere test sums (dz^2+dx^2)+dy^2, NaN counts as inside.
- d646d464 CTaskComplexDie::CreateFirstSubTask (0x6302D0): drowning in a plane/heli went to return null (exe continues to the common die path); CTaskComplexDieInCar got WEAPON_UNARMED instead of the task's weapon type.
- b29ebbac CTaskComplexDieInCar::CreateSubTask (0x62FD50): LEAVE_CAR_AND_DIE now builds CTaskComplexLeaveCarAndDie (new ctor = exe 0x62F1A0; task type 0x2C1, PEDSTATE_DIE in CreateFirstSubTask) instead of plain CTaskComplexLeaveCar, so DieInCar::CreateNextSubTask's 0x2C1 case is reachable again.
MATCH (7): DestroyCar::ControlSubTask, DestroyCarMelee CreateSubTask / CalculateSearchPositionAndRanges, DieInCar ControlSubTask / CreateFirstSubTask / CreateNextSubTask / PreparePedVehicleForPedDeath.
Notes: CTaskTimer::Start vs the exe's unguarded store in CopInCar::ControlSubTask is equivalent (intervals are non-negative). Other DestroyCarMelee fns (CreateNext/Control) still use IsPointInSphere (x,y,z sum order) - not in this slice.
Scratch: C:/gta-scratch/asm-s02b/.
Gate: StandaloneRelease build OK; wave2_oracle_test (StandaloneRW, exe oracle): 1142 checks, 0 failures, 0 mismatches; gameplay_regress combat (NOVSYNC, own build): TOTAL PASS=14.

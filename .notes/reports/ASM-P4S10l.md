# ASM-P4S10l
Part A: P4S10k CPhysical::ApplyAirResistance fix (e84e192c6) CONFIRMED (exe 0x544CE1 tests +0x594 sub type; CMonsterTruck ctor sets only +0x594=1).
Part B (8 rows incl. Part A; 4 MATCH, 3 FIXED):
- CTaskInteriorGoToInfo::CreateFirstSubTask (0x676640-0x67684E): FollowNodeRoute start node m_Int node -> node of ped's current interior (56a8a1978)
- CTaskComplexEnterAnyCarAsDriver::CreateFirstSubTask (0x643510-0x6436C1): fallback closest loop: no stealable test -> requires IsVehicleStealable (d6df483f1)
- CTaskComplexHitPedWithCar::CreateSubTask (0x6560E0-0x6562E0): type 414 returned nullptr -> new CTaskSimpleHitFromBehind (005b0077c)
MATCH: CTaskGoToVehicleAndLean::CreateNextSubTask, CTaskGangHassleVehicle::CreateNextSubTask, CAutomobile::PlayHornIfNecessary, CTaskComplexEvasiveStep::CreateSubTask.
Not reached: remaining P4S10.queue.tsv rows (CTaskInteriorGoToInfo::CreateNextSubTask 0x675890, CPed::Look, CPed::ClearAll, CTaskComplexDriveToPoint::IsTargetBlocked 0x6452C0, ...).

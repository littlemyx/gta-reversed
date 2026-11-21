# ASM-Z19: behaviour-heavy CTask*/CEvent* methods among the oracle-only rows (oracle: gta_sa_compact.exe via exedis)

Compared 118 functions + 15 sampled constructors: the 66 behaviour rows of the brief (MakeAbortable / ControlSubTask / CreateNext/First/SubTask / AffectsPed / IsValid / TakesPriorityOver / ProcessPed / IsCriminalEvent, listed in Z19.result.tsv) plus 52 further CTask*/CEvent* non-ctor rows (FinishAnimCBs, SetSubTask, Sequence DeRegister/SetCanBeEmptied, GetPartnerSequence x3, PlayerPassiveControlGun, GetSourceEntity x4, AffectsPedGroup x2, PrepareVehicleForPedExit, ...) (118 rows in total), and 15 constructors sampled. Result: .notes/asm_sweep/Z19.result.tsv.

## Fixed (1)
- CEventLeanOnVehicle::IsValid (exe 0x4B16C0), commit 2397079fb: the `ped == null` path called `IsValid(ped)` (itself, virtual) -> unbounded recursion. The exe inlines the base CEvent::IsValid (`m_bValid || m_nTimeActive <= GetLifeTime()`); the port now calls `CEvent::IsValid(ped)`. (CEventPassObject / CEventInteriorUseInfo already did this correctly.)

## MATCH (everything else)
Checked per body: abort-priority switches (URGENT=1, IMMEDIATE=2), the forwarders, -1000.0 blend delta (0xC47A0000) stores, flag bits (CPed 0x478 bit 9 IgnoreWeaponRange, 0x46C bit 22 KindaStayInSamePlace, CPed::IsPlayer type guard `(flags&7)==3`), CTaskSimpleIKChain::BlendOut inline, CTaskSimpleJump flag 4 + -4.0, GetPartnerSequence x3 (x87 sum order, float store of 1-t, anim ids 287/288/164/142, 0.001/0.02 turn tolerances), PlayerPassiveControlGun (signed `< 2` on both commands), PrepareVehicleForPedExit (status 4 vs 9), CEventPlayerCommandToGroup::AffectsPedGroup (7 followers, area code byte), CTaskAllocatorKillThreatsDriveby::ProcessGroup (inherited Basic logic is identical), CEventHandlerHistory field order.

Notes without behaviour effect: CTaskComplexFallAndStayDown ctor with dir > 3 leaves the anim id untouched in the exe (port: 0), CTaskComplexDrivePointRoute::CTaskComplexDrivePointRoute does not register the vehicle (documented in source, same as exe).

## Constructor sample (15)
KillCriminal, WanderCop, Wander, LeaveCarAndFlee, EnterCarAsPassengerTimed, WalkRoundCar (nibble packing of byte +0xE), UseMobilePhone, ReactToAttack, FleeShooting, KillAllThreats, FollowLeaderInFormation, GoToPointAiming, StealCar, LeaveCarAsPassengerWait, RunAnimLoopedMiddle (flag byte). Misses: none.

## Rows left
The ~566 CTask*/CEvent* Constructor/Clone/Destructor rows are mostly done only as a sample; the CTask*/CEvent* non-constructor oracle-only rows of the behaviour class are now exhausted (52 + 66 compared). Remaining no-verdict rows are CarCtrl steering, CCheat, Shopping, VehicleModelInfo, Entity, Object, Pickup, PedGroup helpers.

Link: see below.
Link: StandaloneRelease (STATEHASH=OFF) rebuilt EventLeanOnVehicle.cpp and linked gta_reversed.exe, exit 0.

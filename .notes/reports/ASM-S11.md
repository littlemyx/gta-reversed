# ASM-S11 (Tasks/TaskTypes: car get-in/out family, Choking, Climb; 48 functions) - 2026-10-26 23:16 .. 23:40 (soak RESULT OK, Release build 1186/1186)
Result: 39 compared (30 MATCH, 9 FIXED in 9 commits incl. 1 extra function outside the tsv), 0 UNSURE, 9 NOT_REACHED (token budget; Climb::ProcessPed/ScanToGrab/ScanToGrabSectorList/CreateColModel/StartAnim/GetCameraStickModifier, CarSetPedInAsDriver/InAsPassenger/SetPedSlowDraggedOut ProcessPed). Table: S11.result.tsv.
Fixes (exe address, one line each):
- CTaskSimpleCarOpenLockedDoorFromOutside::StartAnim (0x64AE00) doors 10 AND 11 use DOORLOCKED_LHS (port: only 10); ::ProcessPed (0x64D8E0) returns true when the vehicle is gone.
- CTaskSimpleCarShuffle::SetPedPosition (0x646920) line-up only for blend amount strictly > 0.9 (port >=; differs only at exactly 0.9f).
- CTaskSimpleCarGetOut::ComputeAnimID (0x647330) m_isUpsideDown is ASSIGNED IsCarUpsideDown() for automobiles (port only raised it); ::ProcessPed (0x64DC70) returns true without a vehicle (port false) and the Smash'n'Boom vehicle reset needs ped->IsPlayer().
- CTaskSimpleCarSlowBeDraggedOut::ProcessPed (0x64E180) self-dragged door opening uses the anim's current time, not 1.0.
- CTaskSimpleCarSlowDragPedOut::ProcessPed (0x64E060) jacker/jacked speech roles were swapped in the rand>0x200 half (exe: SayJacking(ped,jacked) then SayJacked(jacked)).
- CTaskSimpleCarOpenDoorFromOutside::ProcessPed (0x64D710) vehicle-type test inverted: exe uses OpenDoor(ratio) for an AUTOMOBILE before the open timing, ProcessOpenDoor otherwise; port did the opposite.
- CTaskSimpleCarJumpOut::ProcessPed (0x64DD60) sets CPedIK 0x8 (bSlopePitch) not bUseArm (0x4) at 0x64DE1F; door opened through SetDoorOpen_Component (m_door is a TARGET_DOOR 8..11; SetDoorOpen(eDoors) was a no-op).
- CTaskSimpleCarSlowDragPedOut::FinishAnimCarSlowDragPedOutCB (0x648180, not in the tsv): same SetDoorOpen((eDoors)TARGET_DOOR) no-op -> SetDoorOpen_Component.
OTHER SITES with the same `SetDoorOpen((eDoors)targetDoor)` pattern, NOT touched (other slices/files): TaskSimpleCarCloseDoorFromOutside.cpp:118, TaskComplexEnterCar.cpp:730 - exe uses the *_Component accessors (0x6C2250/0x6C21E0); the former is likely the same no-op bug.
MATCH: SetTempAction (ctor-dtor-ProcessPed), OpenDoorFromOutside (ComputeAnimID/CB/StartAnim/SetPedPosition), SlowDragPedOut helpers, JumpOut helpers + SetPedPosition, SetPedOut ProcessPed + PositionPedOutOfCollision, Choking ProcessPed + UpdateChoke, GoToPointNearDoorUntilDoorNotInUse, Climb Shutdown/StartSpeech/TestForStandUp/TestForVault/TestForClimb.
Notes: TestForClimb keeps the transformed heading unrounded for sin/cos in the exe (port rounds first, sub-ulp). Consumer audit: no layout/signature change; JumpOut's door-open now really marks the damage-manager door state like the exe.
Gate: Release build OK (1186/1186); no oracle target covers these tasks; soak.sh 3 min NOVSYNC new game: RESULT OK, 0 errors (C:/gta-scratch/asm-s11/soak).

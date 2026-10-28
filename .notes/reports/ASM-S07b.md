# ASM-S07b (Tasks/TaskTypes, 30 functions) - 2026-10-28 - COMPLETE 30/30
Result: 30 compared: 22 MATCH, 8 FIXED (5 commits), 0 UNSURE, 0 NOT_REACHED. Tables in S07b.result.tsv.
Fixes (all in the run build, no FIX_BUGS guard needed):
- ReactToGunAimedAt (f34264bb): ControlSubTask aborted the wander with LEISURE, exe pushes URGENT(1) (0x6985B9); CreateNextSubTask missed Say(CTX_GLOBAL_GUN_RUN, 0x73) when the cower finishes (0x69840C); the flee task's tolerance is `push [0xC18CF0]` (runtime global) not 1.0; the pause rand needs m_pVehicle as well as bInVehicle.
- SeekEntityAiming::ControlSubTask (8458a6f2): the radius compares are `!(d2 < r2)` (>=, == and NaN take the branch); a new TaskSimpleUseGun is created only when the secondary attack slot is EMPTY (port also replaced a non-gun task there).
- ShuffleSeats (a30bc8e3): CreateSubTask(SET_PED_IN_AS_DRIVER) calls RemovePassenger(ped) first (was missing: ped stayed in the passenger list); SET_PED_OUT uses ped->m_pVehicle for the task and the door (exe 0x63D3B5), not the task's vehicle; CreateNextSubTask: SET_PED_OUT finishes in non-abort mode (was unreachable/null).
- SmartFleeEntity::ControlSubTask (db5ec7ad): the pos-check timer is restarted (unguarded) only when the target moved strictly farther than tolerance (port restarted it every period, before the test, and used >=); sum order z,y,x.
- SmartFleePoint::ControlSubTask (385c3407): distance sums z,y,x (float rounding).
MATCH: RoadRage::CreateFirstSubTask, ScreamInCarThenLeave x4, SeekCoverUntilTargetDead::ControlSubTask, SeekEntityAiming CreateNext/CreateFirst, ShuffleSeats dtor/CreateTaskUtilityLineUpPedWithCar/CreateFirst, SignalAtPed x3, SitDownThenIdleThenStandUp x2, SmartFleeEntity CreateNext/CreateFirst, SmartFleePoint ComputeFleeDir/CreateNext/CreateFirst, ReactToGunAimedAt::CreateFirstSubTask.
Carry-over from S07: CCamera::m_nWhoIsInControlOfTheCamera is at +0xB0 (verified by a compile-time offsetof probe against the port header: 176 = 0xB0), so ProstituteSolicit::ControlSubTask's read at exe 0xB6F0D8 (= 0xB6F028 + 0xB0) is the right field. (The header has no static_assert for it.)
Notes: SmartFleeEntity::CreateSubTask (0x65C530) and SmartFleePoint::CreateSubTask (0x65BE80) were not in this scope; the sequence/RunAnim part of the latter was read in passing and agrees (group 0, anim 0xA5, 4.0, tired 2000).
Open range: none.

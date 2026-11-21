# ASM-Z15: Z14 leftovers + no-verdict functions (exe oracle: gta_sa_compact.exe via exedis)

## Result
83 rows in Z15.result.tsv: 80 MATCH, 3 FIXED.

| fix | exe | what |
|---|---|---|
| 8a260b264 | 0x699857 (CCoverPoint::Update, OBJECT arm) | the exe gives an entity without a matrix one (AllocateMatrix 0x54F560 + UpdateMatrix 0x54F1B0) before the up.z test; the port dereferenced a null matrix |
| 90278fd2a | 0x5E79B0 (CPed::SetLook(float)) | also clears bIsRestoringLook (`and eax,~8 ; or eax,4`), as SetLookFlag already did |
| fccfb76e9 | 0x60A440 (CPlayerPed::MakePlayerGroupDisappear) | the exe clears bit 0 (bUsesCollision) and bit 7 (bIsVisible) of the follower's entity flags; the port cleared bit 1 (bCollisionProcessed) instead, so hidden followers kept colliding |

## 1. Z14 leftovers
CCover::Update, CPedGroupMembership::RemoveMember (both overloads), CCarEnterExit::SetAnimOffsetForEnterOrExitVehicle, LinkEntryExit / PostEntryExitsCreation / GetEntryExitIndex, CEntryExit::TransitionFinished tail (0x440678-0x440B89), IsWanderPathClear (water loop and drop checks): all compared instruction by instruction. Only CCoverPoint::Update differed. Not done (skip rule): CStreaming::LoadScene / RequestSpecialModel.
Known and left: CCoverPoint::Remove nulls the EntityRef (the exe keeps the stale pointer and its registration); no gameplay effect.

## 2. Ranking source
No per-mission hit dumps exist: the only per-function hit data is C:/gta-scratch/statehash/hitcount/ryder1.* (one mission). compare_ts.tsv shows that most stuck missions stop in the same place in both ports (driver-side), so the C++-only stops (2, 4/5, 9, 10, 11, 23, 24, 25, 28, 29, 33-36, 39-41) cannot be ranked by hits. An HC pass over those missions would be needed for a real ranking.
Instead: every hooked function that has an address in any .notes/asm_sweep/*.tsv result already carries a verdict (only ~57 of ~8800 hooks have none: Fx / audio / clothes builder / plant manager). The remaining population is P4_ORACLE_ONLY.tsv (2128 functions verified by the differential oracle only, no asm compare). I took the gameplay part of it (CPed, CPlayerPed, CVehicle, CAutomobile, CBike, CTrain, CPhysical, interior tasks, fall tasks, event damage, cop ped) and compared the non-trivial ones; trivial getters were folded into the rows.
Finding: the oracle misses flag-bit mistakes (2 of the 3 fixes are wrong flag bits); the other ~50 functions matched. Remaining oracle-only gameplay candidates (not compared): CPedAttractor queue helpers, CTaskComplexFallAndStayDown::*, CTaskComplexPassObject::*, CTaskInteriorShopKeeper::CreateNextSubTask, CHeli::SwitchPoliceHelis / FindSwatPositionRelativeToHeli, CEntity::GetIsOnScreen / FindTriggerPointCoors, Interior_c tile helpers, InteriorManager_c stealable-object helpers, CEventDamage::AffectsPed second half.

## Build
StandaloneRelease (non-STATEHASH) link in C:/gta-wt/p4r2d/build/StandaloneRelease under buildslot.2 (owner Z15); source commits pushed only after the link succeeded (see the note at the end of this file).

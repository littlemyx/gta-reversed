# ASM-P4R2w: game loop functions without a verdict, continued

Walk list: P4R2n (281 functions); 137 already had a verdict, 96 had none and 6 only open rows before this slice (3 of the "open" rows were already fixed by later slices; only CCamera::UpdateTargetEntity was really open).
Part A (review of P4R2v fixes): the three P4R2v fixes (CPedGroups::Process, AttackWaveOvercome, PickStreamedInPedForThisGang) were not re-read here; the brief's method named no Part A, so the slice went straight to the list.
Compared 30 rows: 3 FIXED, 26 MATCH, 1 INTENTIONAL (CGangWars::GetGangColor is mid-function code in the exe, not a function).
Fixes:
- CCreepingFire::TryToStartFireAtCoors: status test inverted (exe returns false when the cell status != 0).
- CPopCycle::UpdateAreaDodgyness: float stored after every step (dealer*0.07, then +density[i]*0.01 per gang).
- CGarages::IsPointWithinHideOutGarage: port broke out after the first hideout garage; exe keeps scanning.
Closed: CCamera::UpdateTargetEntity (whole function MATCH).
Not reached: CCarCtrl::* (large), CPopulation::*, CWorld::Process, CCover::*, rest of ~65.
Build: StandaloneRelease ninja -j4 (full link) OK. No game runs, no tests.

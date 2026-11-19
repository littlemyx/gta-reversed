# ASM-P4R2t: review of P4R2s fix + game loop functions without a verdict, continued

Walk list: P4R2n (281 functions); recomputed against all result TSVs: 94 without a MATCH/FIXED/INTENTIONAL row (largest-first list in C:/gta-scratch/p4r2n/rem.tsv).
Part A: P4R2s fix (cd91bdbe2: SetPedsOnFire invulnerable test, TriggerExplosion scan code) re-checked in the exe: confirmed.
Part B: compared 18 functions: 13 MATCH, 5 FIXED in 3 commits.
Fixes: CFireManager::ExtinguishPoint/ExtinguishPointWithWater radius test strict (distSq < r*r); CFire::ExtinguishWithWater particle offset from integer rand&0xFF steps (x,y *0.01, z *0.005) instead of continuous range; CGameLogic::LaRiotsActiveHere strict rectangle tests.
MATCH: FindSmallestZoneForPosition, GetZoneInfo, FindNearestFire, GetNextFreeFire, SetStatValue, CheckForThreshold, CPlane::{CountPlanesAndHelis,AreWeInNoPlaneZone,AreWeInNoBigPlaneZone}, RemoveAllExplosionsInArea, FindDirFromVector, ShouldCarStopForBridge, FindSafeHouseIndexForGarageType.
Not reached (budget): CCamera::*, CStats::UpdateRespectStat/DisplayScriptStatUpdateMessage/others, CGangWars::*, CRestart::FindClosest*, CPointLights::AddLight and the rest.
Build: StandaloneRelease ninja -j4 linked. No game runs, no tests.

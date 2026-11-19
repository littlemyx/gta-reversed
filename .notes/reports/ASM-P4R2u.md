# ASM-P4R2u: review of P4R2t fixes + game loop functions without a verdict, continued

Walk list: P4R2n (281 functions; 94 without a verdict after P4R2t). The "open DEVIATION" rows of the list are stale (their fixes landed in later rows, e.g. UpdateRespectStat b5a99235a).
Part A: P4R2t fixes re-checked in the exe: ExtinguishPoint*, LaRiotsActiveHere confirmed; CFire::ExtinguishWithWater was wrong on z: the exe adds (rand&0xFF)*0.005 with no -128 (0..1.275), fixed (0801f4fd3).
Part B: compared 12 more functions: 11 MATCH, 1 FIXED (e30a57539: CWorld::RemoveVehicleAndItsOccupants nulls m_pDriver as the exe does; the port left it dangling and ~CVehicle then called FlagToDestroyWhenNextProcessed on the deleted driver).
MATCH: CRestart::FindClosest{Hospital,Police}RestartPoint, CWorld::ClearExcitingStuffFromArea, CCheat::AddToCheatString, CStats::{DecrementStat,UpdateStatsOnRespawn,UpdateStatsAddToHealth,ProcessReactionStatsOnIncrement,UpdateRespectStat}.
Not reached (budget): CCamera::*, CPointLights::AddLight, CGangWars::*, CWeapon::GenerateDamageEvent, CVehicleRecording::SaveOrRetrieveDataForThisFrame and the rest.
Build: StandaloneRelease ninja -j4 linked OK. No game runs, no tests.

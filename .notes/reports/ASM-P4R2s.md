# ASM-P4R2s: review of P4R2r fix + game loop functions without a verdict, continued

Walk list: P4R2n (281 functions). Recomputed against all result TSVs: 165 already MATCH/FIXED/INTENTIONAL, 116 without (13 of those are DEVIATION rows that already carry a fix commit).
Part A: CPedScriptedTaskRecord::Process fix (b1f8ff71d) re-checked in the exe: confirmed.
Part B: compared 15 functions: 11 MATCH, 4 FIXED (one commit, World.cpp: cd91bdbe2).
Fixes: SetPedsOnFire started fires only for INVULNERABLE peds (exe: not invulnerable, no creator, or player creator); SetPedsOnFire/SetPedsChoking/SetCarsOnFire area test is strict |dz|<5,|dx|<r,|dy|<r on float differences (port: inclusive min/max box); TriggerExplosion does not advance the scan code.
MATCH: GetIsLineOfSightClear (loop order differs, result does not), ChooseCivilianCoupleOccupations, SprayPaintWorld, UpdateDealerStrengths, PickRiotRoadBlockCar, EvaluateTargetForHeatSeekingMissile, FillZonesWithGangColours, SetWorldOnFire, GetCurrentPercOther_Peds, ChooseGangPedModel, IsStatCapped.
Not reached (budget): the rest of the 116 (CCamera::Init, CPointLights::AddLight, CRestart::FindClosest*, CWorld::ClearExcitingStuffFromArea, CCheat::AddToCheatString skipped per brief).
Build: StandaloneRelease ninja -j4 linked (see log). No game runs, no tests.

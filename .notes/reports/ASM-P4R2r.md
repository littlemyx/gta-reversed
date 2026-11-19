# ASM-P4R2r: review of P4R2q fixes + game loop functions without a verdict, continued

Walk list: P4R2n (281 functions, CGame::Process 3 levels). Recomputed against all result TSVs: 158 already MATCH/FIXED/INTENTIONAL, 123 without such a verdict (before this slice).
Part A: both P4R2q fixes (ProcessLineOfSight ascending same-X/Y loops, AddExplosion grenade-victim fx + PLAYER1-only fires stat) re-checked in the exe: confirmed.
Part B: compared 9 new functions: 8 MATCH, 1 FIXED. Two open-list entries (UpdateTerritory..., UpdatePercentages) already had MATCH from P4R2o/p.

Fix: CPedScriptedTaskRecord::Process (d68b14f65): exe's "primary task" test = first non-null of slots 0..2 compared as one value, plus slots 3, 4 (not HasTaskPrimary over all 5), so a task in slot 1/2 behind a non-null earlier slot is not PRIMARY (then SECONDARY test, else Flush).
MATCH: CWeapon::CheckForShootingVehicleOccupant, CPopulation::ChooseCivilianOccupation, CCamera::RestoreWithJumpCut, CCrime::ReportCrime, CPopulation::PedMICanBeCreatedAtThisAttractor, CCamera::ProcessMusicFade.
Not reached (budget): CCamera::Init (skipped per brief), CPointLights::AddLight, CRestart::FindClosest*, CWorld::ClearExcitingStuffFromArea, CCheat::AddToCheatString and the rest of the list.
Build: StandaloneRelease ninja -j4 linked. No game runs, no run_all_tests, no regress.

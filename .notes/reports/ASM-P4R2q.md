# ASM-P4R2q: review of P4R2o/P4R2p fixes + game loop functions without a verdict, continued

Part A: the 4 fix commits of P4R2o/P4R2p (BulletInfo, Explosion::Update, ProjectileInfo::Update, Cover::AddCoverPoint) re-checked against the exe: all confirmed.
Part B: walk list from P4R2n (281 functions, 127 still without a verdict after P4R2o/p). Compared 5: 2 FIXED, 2 MATCH, 1 UNSURE (GetIsLineOfSightClear, order-only).

Fixes
- CWorld::ProcessLineOfSight (853baa026): same-X / same-Y lines with origin < target scan ascending from the origin (0x56BB8D/0x56BD8B), the port scanned descending from the target.
- CExplosion::AddExplosion (22ff473ba): grenade with a victim creates explosion_small on the victim matrix (0x736C97), the port did nothing; fires-started stat only for PED_TYPE_PLAYER1 (0x737401).
MATCH: CPopCycle::FindNewPedType, CTrafficLights::ShouldCarStopForLight.
Not reached (budget): CCamera::Init, CWeapon::CheckForShootingVehicleOccupant, CPopulation::ChooseCivilianOccupation, CCamera::RestoreWithJumpCut, CCrime::ReportCrime and the rest.
Build: StandaloneRelease ninja -j4 linked. No game runs, no regress, no run_all_tests.

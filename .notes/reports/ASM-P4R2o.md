# ASM-P4R2o: per-frame game loop functions without a verdict, continued

Walked list: P4R2n (281 functions under CGame::Process, 137 with a verdict, 132 without, 12 open DEVIATION/NOT_REACHED; the 12 open rows already carry fix commits). Compared 4 functions, 2 FIXED, 2 MATCH.

Fixes
- CBulletInfo::Update (daf6537f8): a bullet that hit something was never switched off (exe 0x7367FF); creator ped not validated with CPed::IsPointerValid (0x7360F6); dead-ped blood anim lacked the PLAYING flag (0x7363C4); sniper dot product summed x,y,z instead of x,z,y (0x7366FB).
- CExplosion::Update (fcd1e9d45): car-explosion creeping fire drew the random offset x before y (exe draws y first) and omitted the +2.0 z lift (0x7377A9).

MATCH: CGangWars::UpdateTerritoryUnderControlPercentage, CStats::IncrementStat (both already carry exe audits in source).
Not reached (budget): CProjectileInfo::Update, CWorld::ProcessLineOfSight, CCover::AddCoverPoint, CPopCycle::UpdatePercentages and the rest of the list.
Build: StandaloneRelease ninja -j4 (see report line in the hand-back). No game runs, no regress.

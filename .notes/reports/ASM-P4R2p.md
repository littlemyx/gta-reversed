# ASM-P4R2p: per-frame game loop functions without a verdict, continued

Walked list: P4R2n (281 functions under CGame::Process, 137 with a verdict, 132 without; the 12 open DEVIATION rows already carry fixes). Compared 4 functions (+RemoveProjectile as callee): 2 FIXED, 2 MATCH.

Fixes
- CProjectileInfo::Update: molotov/bomb "close to creator" distance summed x,y,z, exe sums x,z,y (0x7397A4). Everything else (rocket smoke trail, heat-seeking homing, flare, satchel, RemoveProjectile) matches.
- CCover::AddCoverPoint: near-point test used the entity position when both entity and pos were given, exe prefers pos (0x698FA5); the port left m_CoverEntity null for IPL-index-0 buildings while the exe stores the pointer (only RegisterReference is skipped, 0x6990D6), so CCover::Update dropped every such POINTONMAP point at once.

MATCH: CPopCycle::UpdatePercentages, CProjectileInfo::RemoveProjectile.
Not reached (budget): CWorld::ProcessLineOfSight, CWorld::GetIsLineOfSightClear, CPopCycle::FindNewPedType and the rest of the list.
Build: StandaloneRelease ninja -j4, 9/9 linked. No game runs, no regress, no run_all_tests.

# ASM-S27b (World.cpp, 15 functions) - 2026-10-27
Result: 10 compared (4 FIXED in 4 commits + 1 out-of-slice fix, 6 MATCH), 0 UNSURE, 5 NOT REACHED (token budget; table in S27b.result.tsv).
Fixes:
- ShutDown (0x564050): calls CIplStore::Shutdown first (0x564055); the port never did (3a9fdba7).
- RemoveFallenCars (0x565E80, 0x565F72): a kept car goes to the nearest VEHICLE path node (type 0), the port searched ped nodes; strict z < limit test (2ee1845e).
- RemoveFallenPeds (0x565D02): only z < -100 handled, z == limit is skipped (306e3d81).
- RepositionOneObject (0x569A88/0x569AC6/0x569AB0): street lights with no ColData stay put; ColData with neither one box nor spheres repositions at the object's own position; single-box point uses box min z, not centre z (ce873dd6).
- FindGroundZFor3DCoord (0x5696DB, out of slice): ProcessVerticalLine tests buildings only, the port also tested dummies (FindRoofZ really has dummies on); every caller is affected (9dc80d60).
MATCH: ProcessAttachedEntities, FindObjectsKindaColliding, FindObjectsIntersectingCube, FindObjectsIntersectingAngledCollisionBox, FindMissionEntitiesIntersectingCube, TestSphereAgainstWorld.
NOT REACHED: TestSphereAgainstSectorList, TriggerExplosionSectorList, Process, ProcessLineOfSightSector, ProcessLineOfSightSectorList.
Note: GetSectorX uses x/50+60 while the exe uses x*0.02f+60 (border-only float noise, left as is).
Consumer audit: no signature change; FindGroundZFor3DCoord callers (GameCommands, Pickups, Population, ...) now get the exe's building-only ground height.
Gate: Release build OK (1186/1186), no oracle row covers these functions (FindGroundZ is scripted in the oracle tests), soak RESULT: OK (NOVSYNC, 0 errors, route 600 s).

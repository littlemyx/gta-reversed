# ASM-S28 (Streaming.cpp, 73 functions) - 2026-10-29
Result: 19 compared (4 FIXED in 4 commits, 15 MATCH, 0 UNSURE), 54 NOT_REACHED (budget; request/priority/removal logic first, file/IMG setup and trivial helpers untouched). Table: S28.result.tsv.
Fixes:
- 81b3e565 RequestFilesInChannel (0x409050): kept a reference into the channel slot, which RemoveModel overwrites with -1 (re-request of model -1), and read the flags after RemoveModel cleared PRIORITY_REQUEST; exe copies id and flags first.
- 9ed8dbcf RemoveLoadedVehicle (0x40C020): boats removed only when count > (boatsNeeded ? 2 : 0) (port >=); a removed boat returns true (port false, so RemoveLeastUsedModel went on to remove more).
- c692f518 DeleteLeastUsedEntityRwObject (0x409760): camera distance summed z,y,x (unrounded) instead of x,y,z.
- 2e8a5179 ProcessEntitiesInSectorList radius overload (0x40C270): strict rect/circle compares, dist^2 < min(drawDist^2, radius^2) summed y then x.
MATCH: RequestFile, RemoveLeastUsedModel (incl. the 3-block fallthrough), RemoveLoadedZoneModel, MakeSpaceFor, RemoveReferencedTxds, PurgeRequestList, RemoveAllUnusedModels, SetModelIsDeletable, RemoveBigBuildings, RemoveDodgyPedsFromRandomSlots (jump table 87/244/246/256/257), RemoveInappropriatePedModels, ClearSlots, StreamVehiclesAndPeds, StreamVehiclesAndPeds_Always, StreamZoneModels (control flow and calls; mod/idiv setup not re-read).
Open (next agent): LoadAllRequestedModels, ProcessLoadingChannel, RequestModelStream, GetNextFileOnCd, StreamOneNewCar, StreamZoneModels_Gangs, StreamCopModels, StreamPedsForInterior, LoadZoneVehicle, ReInit, the *InSectorList helpers, then IMG/file setup rows.
Consumer audit: no signature/layout change; the 4 edits only change rounding/compare edge cases and the boat-removal return value (RemoveLeastUsedModel / MakeSpaceFor callers).
Gate: Release + StandaloneRW builds OK; wave2_oracle_test (RW_EXE_ORACLE+GTA_EXE = compact exe) 1142 functions, PC24 0 / PC53 0 mismatches; combat regress PASS=14; soak.sh 3 min RESULT: OK (errors 0).

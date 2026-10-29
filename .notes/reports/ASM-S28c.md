# ASM-S28c (Streaming.cpp, 34 rows) - 2026-10-29
Result: 11 compared (4 DEVIATION fixed + Task 0, 7 MATCH, 0 UNSURE), 23 NOT_REACHED (budget). Table: S28c.result.tsv.
Task 0 (080e6ac4, ac3ee544): DeleteRwObjectsBehindCameraInSectorList / NotInFrustumInSectorList (0x409940/0x4099E0) and RemoveReferencedTxds (0x40D2F0) compare memoryUsed < goal SIGNED (jl); the third "InSectorList" helper (DeleteRwObjectsInSectorList) has no compare. StreamZoneModels_Gangs CanDeleteModel: the port-only m_DefaultModelIDForUnusedSlot guard is now under FIX_BUGS; the exe instead skips marker 2000 while walking the other gangs' 23-slot car groups (0x40ADC0), the port now does the same.
Fixes:
- a16a4d03 AddModelsToRequestList (0x40D3F0): the repeat-sector list processed with Buildings/Dummies is Objects (+8); port streamed Peds (+4).
- edc3592e DeleteRwObjectsBehindCamera (0x40D7C0): entry test and final RemoveLeastUsedModel loop compare signed (MakeSpaceFor passes avail - size, which can wrap negative); forward.y NaN takes the <= 0 path.
- 0aeebc92 PossiblyStreamCarOutAfterCreation (0x40BA70): tests CVehicleModelInfo::m_nFlags (+0x4E) bit 0, port tested CBaseModelInfo::m_nFlags (+0x12).
- 30ad919c Update (0x40E670): LOD branch also taken for NaN camera height (jp).
MATCH: ConvertBufferToObject (3rd arg, the channel index, is never read in the exe), AddLodsToRequestList (exe truncates the LOD sector, port floors: differs only off-map for sector floats in (-1,0)), AreAnimsUsedByRequestedModels, AddEntity, DeleteAllRwObjects, DeleteRwObjectsAfterDeath, GetDefaultCabDriverModel.
Open (next agent): AddImageToList, Load/Save, LoadCdDirectory x2, LoadInitialPeds/Weapons, LoadScene, LoadSceneCollision, RequestBigBuildings, RequestTxdModel, RequestVehicleUpgrade, RequestSpecialChar/Model, ReadIniFile, RemoveCurrentZonesModels, RemoveTxdModel, Init2, InitImageList, Shutdown, StreamAmbulanceAndMedic, StreamFireEngineAndFireman, StreamPedsIntoRandomSlots.
Consumer audit: no signature/layout change; edits change only edge cases (signed memory compares), the repeat-sector list streamed near the player, the vehicle flag test.
Gate: Release + StandaloneRW (wave2_oracle_test) builds OK; wave2_oracle_test (RW_EXE_ORACLE+GTA_EXE = compact exe) 1142 functions, PC24 0 / PC53 0 mismatches; combat regress PASS=14; soak.sh route 406 s RESULT: OK.

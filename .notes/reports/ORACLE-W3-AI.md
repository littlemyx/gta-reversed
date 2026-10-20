# ORACLE-W3-AI report

Agent ORACLE-W3-AI (rules v3), start 07:19, end ~08:45 (2026-10-20), worktree C:/gta-wt/oracle-w3-ai (branch oracle-w3-ai, base 4b7c7468).
New target `wave3_ai_oracle_test` (W2 harness: deterministic arena, whole exe data image + a 128 KB scratch block + the fake double-link / txd pools + the reference world compared after every case), rows in `tests/standalone/wave3_oracle_ai*.inc`
(`_ped` PedIntelligence.cpp, `_obj` Object.cpp, `_car` / `_car2` / `_car3` CarCtrl.cpp, `_cover` Cover.cpp). Development switches: `W3A_DIV=n` divides the case counts, `W3A_SCALE=n` multiplies them, `W3A_PENDING=1` also runs the two pending Cover rows, `W2_FAULTLOG=1`.
**58 active rows (CarCtrl 22, PedIntelligence 17, Object 13, Cover 6) + 2 pending Cover rows (off by default, see below), PC24 0 mismatches, PC53 74 (informational, x87-order rows), exit 0**.

| | before | after |
|---|---:|---:|
| exe-oracle coverage, whole game | 4275 | **4335** (2 of the 60 literals are the guarded pending Cover rows: their address literals count) |
| CarCtrl.cpp uncovered A / B | 23 | **1** (DealWithBend_Racing) |
| PedIntelligence.cpp uncovered A / B | 17 | **0** |
| Cover.cpp uncovered A / B | 11 | **3** (CalculateHorizontalSize, DoLineCheckWithinObject, FindVectorFromFirstToMissingVertex: port bodies are NOTSA_UNREACHABLE / unused, nothing to compare) |
| Object.cpp uncovered A / B | 16 | **3** (SetupLighting, RemoveLighting, ProcessGarageDoorBehaviour) |
| port bugs fixed (one commit each, asm address + pre-fix failing count in the subject) | | **9 commits** |

## Fixtures
- Scratch block (0x20000): two peds with CPedIntelligence behind `m_pIntelligence` (+0x47C), acquaintance masks with shared bits, scanner arrays, interesting entities, mental-state timer; two vehicles (type 2, matrix blocks, model index 400 / 401 with car-sized col models behind `ms_modelInfoPtrs`, autopilot missions / node addresses, speeds, scan codes); objects (type 4) with matrices and col models; double-link list nodes; output floats.
- Pools: the CPtrNodeDoubleLink pool (0xB74488) and the CTxdPool (0xC8800C) as `PoolFx` fakes (random occupancy, garbage storage) for the control-code list, the processed-building list, `RefModelInfo` / `SetRemapTexture` refcounts; the entity reference world of `logic_oracle_refs.inc` (ped / vehicle / object pools, registered references) for `RemoveAllInterestingEntities`, `SetRelatedDummy`, `IsObjectPointerValid*`, `IsAnyoneParking`, and every cover-point row (entities / covered peds are registered through `RegisterReference` on the cover point fields).
- Exe globals refilled per case: the CarCtrl counters and the vehicles-of-interest list (0x969070.., 0x969084), `ThePaths.m_pPathNodes`, `CTimer` time / frame counter, `CWorld` scan code and player slot, the 8 ropes (0xB768B8), the ModelIndices used by the weave / slow-down code, the 11 loaded-car groups.
- Calls follow the exe's ABI as the asm shows it (CarCtrl / Cover: cdecl statics, a by-value `CVector` is pushed as 3 floats; CObject / CPedIntelligence members: thiscall). An ABI mismatch shows up as an ESP check failure.
- Added to the target besides the 4 TUs: CoverPoint, Physical, PedType, Acquaintance, TaskManager, TimeCycle, Collision, PtrNodeDouble/SingleLink, BaseModelInfo, TxdStore, LoadedCarGroup, ModelIndices, Ropes, Rope.

## Port bugs fixed (rows failed pre-fix; counts = failing PC24 cases of the first run on the unfixed code)
| function | asm | bug | pre-fix failing |
|---|---|---|---|
| CPedIntelligence::GetActivePrimaryTask | 0x4B85B0 | the loop variable was unused: the physical-response slot was read three times (exe: slots 0, 1, 2) | 142/400 |
| CPedIntelligence::CanSeeEntityWithLights | 0x605550 | x87 order: `((dist / 30) * L) * L` (port: `dist / 30 * L^2`), dist = sqrt((dz^2 + dx^2) + dy^2) in extended precision, `<= 0` and NaN return 0.0 (fcom + test ah,0x41) | 115/2000 |
| CObject::SetMatrixForTrainCrossing | 0x59F200 | fcos / fsin stay unrounded in the products; x and y get `+ 0.0f * sin`, z the float-rounded sine | 788/2000 |
| CCarCtrl::RemoveFromInterestingVehicleList | 0x423ED0 | no early exit: every slot holding the vehicle is cleared (port broke after the first) | 36/400 |
| CCarCtrl::SlowCarDownForObject | 0x426220 | factor is `min(*arg3, (1 - (heading - 7) / 13) * speedMult)` (port multiplied the minimum by the speed multiplier); dot products summed z first | 871/3000 |
| CCarCtrl::ReInit (+ CPopulation::m_LoadedGangCars) | 0x4213B0 | the exe clears 11 loaded-car groups (0xC0EA28..0xC0EC22, also CCarCtrl::Init and CStreaming::RemoveCarModel); the port's array had 10 | 1/100 (first case only; later cases start from the exe's result) |
| CCover::Find2HighestPoints | 0x6988E0 | every branch writes both outputs; the port repeated an exe "bug" that does not exist (the second store goes to outPoint2: pops shift esp) | 1194/1500 |
| CCover::Init | 0x698710 | only the type byte of each point is cleared; the port assigned `CCoverPoint{}` which also cleaned up the entity / ped references | 200/200 |
| CObject::GrabObjectToCarryWithRope | 0x5A1AB0 | the grabbed entity belongs in `m_pRopeAttachObject` (+0x318, reference registered there), the HOOK object (`m_pAttachedEntity`, +0x314) is moved to the transformed (0, 0, pickup height) and loses collision; the port swapped both and multiplied the point with the transformed one | 2500/2500 (port faulted) |

## Rows that pass without a change
PedIntelligence: GetPedEntities, Set/GetHearingRange / SeeingRange, Get/SetEffectInUse, FindRespectedFriendInInformRange, IsFriendlyWith, IsThreatenedBy, Respects, AreFriends, RecordEventForScript, HasInterestingEntites, IsInterestingEntity, RemoveAllInterestingEntities, IncrementAngerAtPlayer.
Object: SetIsStatic, CanBeDeleted, Set/CanBeTargetted, RefModelInfo, SetRemapTexture, Add/RemoveFromControlCodeList, SetRelatedDummy, IsObjectPointerValid(_NotInWorld).
CarCtrl: Clear/Register/IsThis.../PruneVehiclesOfInterest, UpdateCarCount, StopCarIfNodesAreInvalid, IsAnyoneParking, SteerAIPlane/HeliToCrashAndBurn, TestWhetherToFirePlaneGuns, SlowCarDownForOtherCar, SlowCarDownForCarsSectorList, SlowCarDownForObjectsSectorList, WeaveForObject, WeaveThrough{Cars,Peds,Objects}SectorList (these include WeaveForOtherCar / WeaveForPed), SteerAICarBlockingPlayerForwardAndBack, SteerAIBoatWithPhysicsCirclingPlayer.
Cover: RemoveCoverPointIfEntityLost, RemoveCoverPointsForThisEntity, DoesCoverPointStillProvideCover, ShouldThisBuildingHaveItsCoverPointsCreated.

## Pending / not done
- **CCover::FindAndReserveCoverPoint 0x6992B0 (483/2500) and CCover::FindCoordinatesCoverPoint 0x699570 (113/2500)**: rows are written and kept behind `W3A_PENDING=1`. The port is a structural reconstruction, not the exe: the exe's loop first drops lost-entity points, then tests height (|ped.z - point.z| < constant), 2D distance, the direction dot product for POINTONMAP points and keeps the nearest/farthest by its own rules, whereas the port filters with `CanAccommodateAnotherPed` and picks `max_element` of squared distance and requires the ped to be among the covered peds. Needs a rewrite from the asm (`exedis.py 0x6992B0 0x699570`), then enable the rows.
- DealWithBend_Racing 0x428040 (710 asm lines, 15 arguments: link addresses + node tables), Object SetupLighting / RemoveLighting (RW lighting calls), ProcessGarageDoorBehaviour (garage table + CGarage::BuildRotatedDoorMatrix): not started (time).
- Cover CalculateHorizontalSize / DoLineCheckWithinObject / FindVectorFromFirstToMissingVertex: skipped, the port bodies are unreachable / unused (nothing to run on the port side).

## Observed, not changed
- WeaveFor* / SlowCarDown* rows use entities with matrices (GetMatrix() would allocate from the matrix list); the null-matrix case is not covered.
- ModelIndices.cpp must be linked: with unresolved `ModelIndices::MI_*` the port compared against garbage while the exe read its own words (cost a false 980-mismatch round).
- The counts above are of the first run on the unfixed code with the final fixture, except ReInit (measured with the first fixture, see table).

## Gates
Built and run in this worktree on the final tree (RW_EXE_ORACLE = the main checkout's gta_sa_compact.exe): `wave3_ai_oracle_test` 58 functions PC24 0 (PC53 74, informational), exit 0 (~11 min; `W3A_DIV` for quick runs); wave3_entity_oracle_test (Object.cpp) 52 functions PC24 0; wave3_ped_oracle_test (PedIntelligence.cpp) 72 functions PC24 0; x87_oracle_test + fixed_oracle_test (Cover.cpp / CoverPoint.cpp) PC24 0 / PC24 0;
physics_oracle_test (CarCtrl.cpp) 108 functions exit 0, hard PC24 0 (74 strict = NaN-payload-only rows, not touched); gameplay_oracle_test (PedIntelligence.cpp) 313 functions exit 0, hard PC24 0 (57 strict = NaN-payload-only rows as before). NOT run here (also link changed TUs, left to the orchestrator's run_all_tests):
logic_oracle_test (CarCtrl.cpp / Population.h), review_oracle_test (CoverPoint.cpp), the gp / tasks / events wave3 targets (PedIntelligence.cpp).

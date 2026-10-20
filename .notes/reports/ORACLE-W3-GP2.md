# ORACLE-W3-GP2 report

Agent ORACLE-W3-GP2 (rules v3), start 20:08, end ~20:50 WEST 2026-10-20 (the agent shell clock), worktree C:/gta-wt/oracle-w3-gp2 (branch oracle-w3-gp2, base d1936749). New target `wave3_gp2_oracle_test` (W2 harness: deterministic arena, whole exe data image + 16 KB fixture block compared after every case), rows in `tests/standalone/wave3_oracle_gp2.inc` + `wave3_oracle_gp2_b.inc`; case counts divided by `W3G_DIV` (default 2). **34 functions, PC24 0 mismatches, 3 min 45 s** (rc 0). Gate also run: `physics_oracle_test` (links TrafficLights.cpp), rc 0.

| | before | after |
|---|---:|---:|
| exe-oracle coverage, whole game | 4615 | **4649** |
| TrafficLights.cpp uncovered A/B | 9 | **0** |
| Restart.cpp uncovered A/B | 9 | **0** |
| Shopping.cpp uncovered A/B | 8 | **0** |
| PedAttractor.cpp uncovered A/B | 8 | **0** |
| Vehicle.cpp (9) / World.cpp (12) | | not done (token budget, see below) |
| port bugs fixed (one commit each, asm address + pre-fix count in the subject) | | **5** |

## Port bugs fixed (counts = failing PC24 cases of the first run on the unfixed code)
| function | asm | bug | pre-fix |
|---|---|---|---|
| CTrafficLights::LightForPeds | 0x49D400 | green while masked time < 15384 (`cmp 0x3C18`); port < 15383 | 100/3000 |
| CTrafficLights::FindTrafficLightTypeFromOrientation | 0x49D4D0 | north-south only for 60 < h < 150 or 240 < h < 330, NaN -> west-east; the port's inverted test gave north-south for NaN (user FindTrafficLightType failed 12/3000) | 5/2500 |
| CPedAttractor::IsAtHeadOfQueue | 0x5EB530 | compares slot 0 of the vector without a size check (the stale ped of an emptied queue is still "head"); the port had an empty() guard | 212/3000 |
| CPedAttractor::GetQueueSlot | 0x5EB550 | -1 when the ped is not in the queue; the port returned the queue size | 2260/3000 |
| CRestart::FindClosestHospitalRestartPoint | 0x460850 | fcomp / test ah branches: a NaN radius or NaN distance takes the closest-point search, a NaN city-unlocked stat does not skip a point; the port's `<=` / `>=` forms went the other way | 56/3000 |

## Rows that pass without a change
LightForCars1/2 (+ _Visual), IsMITrafficLight, FindOrientationForTrafficLightType, CRestart::Initialise / AddHospitalRestartPoint / AddPoliceRestartPoint / OverrideNextRestart / CancelOverrideRestart / Set- and ClearRespawnPointForDurationOfMission / FindClosestPoliceRestartPoint, CShopping::FindItem / GetItemIndex / SetPlayerHasBought / HasPlayerBought / AddPriceModifier(key) / RemovePriceModifier(key) / UpdateStats, CPedAttractor::IsRegisteredWithPed / GetHeadOfQueue / GetTailOfQueue / HasEmptySlot / ComputeAttractTime / GetTaskForPed.

## Fixture
Game clock (0xB7CB84) at the phase boundaries (5000 / 6000 / 11000 / 12000 / 15383) of the 16384-tick cycle, the wrap and huge values; the nine traffic-light model indices (repeats allowed); entities as raw blobs with a matrix forward vector at headings 60 / 150 / 240 / 330 +- 1 ulp and NaN / inf. Restart tables (10 points each, when-to-use 0..3, override / extra points, NaN specials). Shopping price table (300 items, key repeats), 560 keys, 20 price modifiers, stat modifiers (index -1..5). PedAttractor: raw object with MSVC6 vectors (SArray) over fake ped pointers, stale slots, a never-allocated vector; task-pair table + the `ms_tasks` list. Host stubs, the same body behind the exe address and the port symbol: CGameLogic::LaRiotsActiveHere 0x441C10, CTheZones::GetLevelFromPosition 0x572300 (position -> level bands), CStats::GetStatValue 0x558E40, CStats::ModifyStat 0x55D090 (recording).

## Observed, not changed / skips
- CShopping::SetCurrentProperty 0x49B1F0: in the oracle (address-globals) build the port's `gpCurrentProperty` is a TU-local `static inline` variable, not the exe cell; the row checks that the exe stores its argument (the port side only runs the call).
- FindItem / GetItemIndex with an absent key: the port traps (NOTSA_UNREACHABLE), the exe returns -1; the rows use present keys only. IsAtHeadOfQueue with a never-allocated queue: the exe dereferences null (not exercised).
- Not done (stopped at the ~250k-token budget, not the clock): Vehicle.cpp A/B (SetupLighting, RemoveLighting, UpdateClumpAlpha, DoHeadLightEffect, DoTailLightEffect, SetVehicleAtomicVisibilityCB-Object, SetCompAlphaCB, FindUpgradeCB, FindReplacementUpgradeCB) and the World.cpp A/B leftovers (StopAllLawEnforcersInTheirTracks, SetAllCarsCanBeDamaged, FindUnsuspectingTargetCar, FindLodOfTypeInRange, FindObjectsKindaCollidingSectorList, CallOffChaseForAreaSectorListVehicles, TestForUnusedModels x2, TestForBuildingsOnTopOfEachOther x2, CastShadowSectorList, FindPlayerCentreOfWorldForMap). Plan in ORACLE-W3-GP2-state.md. CONTINUE: ORACLE-W3-GP2-state.md

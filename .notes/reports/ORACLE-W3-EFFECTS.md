# ORACLE-W3-EFFECTS report

Agent ORACLE-W3-EFFECTS (rules v3), start 04:47, end ~06:00 (2026-10-20), worktree C:/gta-wt/oracle-w3-fxr (branch oracle-w3-fxr, base bc68c592). New target `wave3_fx_oracle_test` (W2 harness: deterministic arena, whole exe data image + a 128 KB scratch block compared after every case), rows in `tests/standalone/wave3_oracle_fx*.inc` (a Bridge, b Cranes / Explosion / Fire / FireManager / CreepingFire, c Glass / Birds / Skidmarks / ProjectileInfo / WaterCannon(s) / Ropes / Rope, d Clouds / PlantMgr / PlantSurfPropMgr / PPTriPlantBuffer, e Coronas / cloud distance, f fire-manager class C rows, g projectile table rows), skip list `tests/standalone/tools/w3_fx_skip.tsv`.
**71 functions, PC24 0 mismatches.** Development switches: `W3F_DIV=n` divides the case counts, `W3F_SCALE=n` multiplies them; `./wave3_fx_oracle_test.exe <substring>...` runs matching rows only.

| | before | after |
|---|---:|---:|
| exe-oracle coverage, whole game | 4134 | **4206** |
| game_sa root files covered | 1196 | **1267** |
| uncovered A / B in the root files | 288 / 394 | **254 / 366** |
| slice files (Explosion ... PPTriPlantBuffer): uncovered A / B | 34 / 28 (62) | **0 / 0** |
| port bugs fixed (one commit each) | | **7 commits** |

## Harness
- Globals under test live at their exe addresses on both sides (NOTSA_ORACLE_ADDRESS_GLOBALS); fixtures: the 60-slot fire table with random flag combinations / script ids / positions (all active / script / extinguishing / first-generation combinations, free-slot counts 0..8), the 45 glass panes, the rope table (random types / ids / 32 segments each) and a standalone rope object for `UpdateWeightInRope` (target near the anchor so both the stretched and the slack branch run), the three water cannons (garbage section words, ids matching or not), the projectile tables (32 fake objects with and without matrix), plant location triangle / collision-entity lists (doubly linked lists of 2..8 nodes in the scratch block), coronas (the fixture of world_oracle_hud), fog / volumetric cloud slot tables.
- Audio and camera callees are recorded by host stubs (the exe address is `oracle::Patch`ed, the port method of the unlinked TU is defined in the test with the same body, the call log is part of the compared scratch block): `CAudioEngine::ReportGlassCollisionEvent` (0x506EE0), `CAEWaterCannonAudioEntity::Initialise` (0x503060), `CSprite::CalcScreenCoors` (0x70CE30, DoSunAndMoon).
- Port-side definitions for TUs that are not linked: `CGame::CanSeeOutSideFromCurrArea`. Private port members (`CPlantMgr::_ColEntityCache_FindInCache`, `CFireManager::Constructor` / `Destructor`) are reached with the W2Rob access trick. Rows with fire clean-up (`CFire::Extinguish` and its callers) keep the entity / FX pointers of every fire null, so Extinguish is pure state on both sides.

## Port bugs fixed (rows failed pre-fix; counts = failing PC24 cases of the first run on the unfixed code, logs C:/gta-scratch/w3fxr/r2.log, r1.log)
| function | asm | bug | pre-fix failing |
|---|---|---|---|
| CGlass::Init / FindFreePane (+ data) | 0x71A8D0 / 0x71ACA0 | the pane polygon table has FIVE polygons in the exe (the fifth, (0.7,0) (1,1) (1,0), was missing: a shattered window lacked its last piece; GeneratePanesForWindow loops 5 pieces, `cmp ebx,5` at 0x71BBA1), the pane array has 45 entries (ends at LastColCheckMS 0xC72FA8; the loops stop at 0xC73015), the centre is (v0+v1+v2) * 0.33333334f, the port divided the sum by 4 | Init 200/200, FindFreePane 153/600 |
| CGlass::CalcAlphaWithNormal | 0x71ACF0 | `std::pow(specular, 6)` and a different operation order; the exe computes dot in (y, z, x) order, the reflection from rounded 2*dot*N, `(rx + ry) * 0.57 - rz * 0.57` and specular^6 by five multiplications (game-mode x87 rounds each to float) | 46/3000 (alpha byte off by one) |
| CPlantMgr::UpdateAmbientColor | 0x5DB310 | `std::max(value, 255)` cut to a byte (the exe clamps to 255 with an unsigned 16 bit compare; the product is truncated by _ftol2 after `64 - ...`): every ambient colour of the plants was garbage | 1500/1500 |
| CClouds::MovingFogInit | 0x713660 | `ms_mf = {}` cleared the whole fog struct; the exe clears only the 350 slot flags (rep stosd) and sets wind + quad indices | 100/100 |
| CWaterCannon (Init, Update_NewInput, CWaterCannons::Init / UpdateOne) | 0x728B40 / 0x728C20 / 0x728C80 / 0x728CB0 | `m_nSectionsCount` is a 16 bit word at +4 in the exe (movsx word, Init writes one word); the port's int32 cleared the word at +6 too and a non-zero upper word indexed out of range | 72/400, 101/600, 102/200, 343/2000 |
| CFireManager::GetNextFreeFire | 0x539E50 | with no free slot the port took the fire with `firstGen \|\| script`; the exe takes the first fire that is NOT a first-generation script fire (`!script \|\| !firstGen`) and extinguishes it | 37/2000 |
| CClouds::VolumetricCloudsGetMaxDistance | 0x713630 | `std::min(far, 600)` returns NaN for a NaN far plane; the exe (`fcomp` + `jp`) returns 600 | 1/600 |

Header changes: Glass.h (`PanePolyPositions` [5][3], `aGlassPanes` 45 entries), WaterCannon.h (`int16 m_nSectionsCount` + `int16 field_6`).

## Rows that pass without a change
Bridge (Init, Update, FindBridgeEntities, ShouldLightsBeFlashing), Cranes (InitCranes, UpdateCranes, IsThisCarBeingCarriedByAnyCrane, IsThisCarBeingTargettedByAnyCrane, IsThisCarPickedUp), CExplosion::Shutdown, CFire (Constructor, Initialise, Extinguish), CFireManager (Init, GetNumOfNonScriptFires, PlentyFiresAvailable, IsScriptFireExtinguished, SetScriptFireAudio, GetScriptFireCoords, RemoveScriptFire, RemoveAllScriptFires, Shutdown, Constructor, Destructor), CCreepingFire (SetReadyToBurn, Update), CGlass (AskForObjectToBeRenderedInGlass, WindowRespondsToSoftCollision with the recorded audio call), CBirds (Init, Shutdown), CSkidmarks::Clear, CProjectileInfo (GetProjectileInfo, IsProjectileInRange without destroy, RemoveIfThisIsAProjectile not-found path), CRopes (Init, FindRope, IsCarriedByRope, SetSpeedOfTopNode, FindCoorsAlongRope, FindPickupHeight), CRope (ReleasePickedUpObject, UpdateWeightInRope: stretched and slack branch, 72% of the cases return true), CClouds (MovingFog_Delete / GetFirstFreeSlot / GetFXIntensity / GetWind, VolumetricCloudsInit, VolumetricClouds_Delete / GetFirstFreeSlot, SetUpOneSkyPoly), CPlantMgr (_ColEntityCache_FindInCache, MoveLocTriToList, MoveColEntToList, SetPlantFriendlyFlagInAtomicMI), CPPTriPlantBuffer (Get/SetPlantModelsTab), CPlantSurfPropMgr (AllocSurfProperties, GetSurfProperties), CCoronas (RegisterCorona with a corona type, DoSunAndMoon with the recorded CalcScreenCoors).

## Observed, not changed
- Fire ids >= 60 (CFireManager script id functions): the port's `std::array` asserts, the exe reads the neighbouring memory; script ids are slot indices, rows use 0..59.
- `CClouds::VolumetricClouds_GetFirstFreeSlot`: the exe compares `m_VolumetricCloudsUsedNum` signed (`jle`), the port unsigned; the counter never exceeds 180.
- `CFireManager::~CFireManager` (0x538BB0) is a bare `ret` in the exe, the port's wrapper returns `this`: the return register is not compared.
- `CGlass::CalcAlphaWithNormal`: the exe reads the camera matrix at a fixed address, the port through `TheCamera.m_matrix`; the fixture sets `m_matrix = &m_mCameraMatrix` as the game does. FIX_BUGS keeps the normalised light direction (deliberate deviation).
- `VolumetricCloudsGetMaxDistance`: the far plane lives at +0x84 of the exe's RwCamera and at librw's offset in the port's one; the fixture stores the value at both places.
- `CRope::UpdateWeightInRope` and `CGlass::CalcAlphaWithNormal` differ at PC53 (CRT default precision, informational; the game runs PC24). The rope row uses game-range magnitudes (no denormal / tiny operands: x87 keeps a wider exponent range than float for results near FLT_MIN; a denormal segment length gave 2/4000 one-ulp differences before the generator was cleaned).
- `Rubbish.cpp` has no hooked function (only `SetVisibility`, not an exe-address site): nothing to cover.

## Not covered (tests/standalone/tools/w3_fx_skip.tsv)
89 functions, none of class A or B: class C (RenderWare draw calls and virtual calls: Birds / Coronas / Clouds / Glass / Skidmarks / Rope / FallingGlassPane Render*, CGlass::IsObjectGlass, FindWindowSectorList), S (CPtrList / object-pool allocations: CGlass::BreakGlassPhysically / WindowRespondsToCollision / CarWindscreenShatters, CRopes::RegisterRope, CRope::CreateHookObjectForRope, CPlantMgr location-triangle builders, CWaterCannon::PushPeds / Update_OncePerFrame) and D (D3D / file IO / audio). The destroying paths of IsProjectileInRange / RemoveIfThisIsAProjectile (CRadar::ClearBlipForEntity, CWorld::Remove, delete) need an object pool and world fixture.

## Gates
`wave3_fx_oracle_test` 71 functions PC24 0 mismatches, exit 0 (final tree, log C:/gta-scratch/w3fxr/final_fx2.log). Targets linking the changed TUs, built and run in this worktree: world_oracle_test (Clouds, Coronas; 117 functions, PC24 0), wave3_entity_oracle_test (Glass; 52, PC24 0), wave3_audio_oracle_test (AEWaterCannonAudioEntity uses CWaterCannon; 164, PC24 0), gameplay_oracle_test (FireManager; 313, PC24 0 excluding NaN payloads). The orchestrator runs the full run_all_tests.

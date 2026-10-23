# ORACLE-C-MODELS report

Agent ORACLE-C-MODELS (rules v4), worktree C:/gta-wt/oracle-c-models, 2026-10-23 23:21 WEST start; stopped at the ~200k token budget (wall clock well under the 100 min cap), nothing in progress.

| | before | after |
|---|---:|---:|
| `wave3_models_oracle_test` rows | 95 | **116** (PC24 **0** mismatches, PC53 7 informational as before, exit 0, 3 min 48 s cold) |
| whole-game covered (oracle_coverage.py, same tree) | 4930 | **4951** (+21) |
| game_sa/Models covered | 105 | 126 |
| port bugs found | | **0** (all 21 new rows pass on the unchanged port) |

## What was added (tests/standalone/wave3_oracle_models5.inc, included by wave3_oracle_models.inc)
* Class-C helper for model infos, the Models twin of W3FuncCase: the exe vftable (RH_ScopedVirtualClass VA) is written into word 0 of the info object before the exe call, the port's
  vftable (`??_7Cls@@6B@` through /alternatename, `w3mvt_*`) before the port call, cleared after. Compilers emit a vftable only where a constructor is used: `W3M_ForceVftables` constructs one of each
  class (never executed) so the five missing vftables (CDamageAtomic / CTime / CLodTime / CWeapon / CPed) exist; `TestW3MVtInfra` prints slot 4 and the unresolved slots (none).
* 11 rows `CModelInfo::Is{Boat,Train,Heli,Plane,Bike,FakePlane,MonsterTruck,QuadBike,Bmx,Trailer}Model` + `IsVehicleModelType`: 16 info objects of random classes (60% vehicle infos, vehicle type 0..11 or garbage, some null table entries).
* 9 rows `CModelInfo::Add{Atomic,DamageAtomic,LodAtomic,Time,LodTime,Weapon,Clump,Vehicle,Ped}Model`: store count 0..2, vptr written into the store item that the exe's virtual Init (slot 6) is called on.
* `CBaseModelInfo::SetColModel` (0x4C4BC0): time-model "other model" chain through ms_modelInfoPtrs with real GetTimeInfo vtable calls.
* Pitfall found: a CBaseModelInfo vftable has `_purecall` in the GetModelType slot on both sides; the call aborts the whole test process (exit 3, no message) - the base class is not drawn.

## Not done (remaining class C of Models, all RW-bound)
CClumpModelInfo::{SetClump, FindFrameFromIdCB, FillFrameArrayCB, GetFrameFromId, GetFrameFromName, FillFrameArray, SetFrameIds, SetHierarchyForSkinAtomic, AtomicSetupLightingCB, CreateInstance-mat},
CAtomicModelInfo::SetAtomic, CDamageAtomicModelInfo::SetDamagedAtomic, CWeaponModelInfo::SetClump, the CVehicleModelInfo RW callbacks (SetAtomicRendererCB*, SetVehicleComponentFlags, GetWheelPosn,
GetOriginalCompPosition, SetEditableMaterials, SetEnvironmentMapCB, PreprocessHierarchy, ...). Recipe for the frame walkers: patch the exe's RwFrameForAllChildren (0x7F0DC0) and define the
port's fakerw one to the same host stub over a hand-built frame tree (child/next at fixed offsets), set the frame-plugin offset global 0x8D6090 (GetFrameHierarchyId = [off + frame]) and
reuse BuildHideDamaged's node-name frame; GetFrameFromId/Name, FillFrameArray, FindFrameFromIdCB, FillFrameArrayCB, SetFrameIds are then ~10 lines each. Entity root (22 class C) not started.

## Consumer audit / soak
No port source changed (test files only), so no gameplay soak was needed.

## ORACLE-C-MODELS-2 (agent ORACLE-C-MODELS-2, rules v4, worktree C:/gta-wt/ocm2, 2026-10-23 23:50 - 2026-10-24 ~01:00 WEST; stopped at the token budget, not the wall clock)

| | before | after |
|---|---:|---:|
| `wave3_models_oracle_test` rows | 116 | **131** (PC24 **0** mismatches, PC53 7 informational as before, no port-only faults left except the known IsVehicleModelType one) |
| whole-game covered (oracle_coverage.py, rebased tree) | - | 5039 (includes rows other agents landed meanwhile) |

New rows (tests/standalone/wave3_oracle_models6.inc): FindFrameFromNameCB 0x4C52A0, FindFrameFromNameWithoutIdCB 0x4C52F0, FindFrameFromIdCB 0x4C5350, FillFrameArrayCB 0x4C5390, GetFrameFromId 0x4C53C0, GetFrameFromName 0x4C5400, FillFrameArray 0x4C5440,
SetFrameIds 0x4C5460, SetHierarchyForSkinAtomic 0x4C4EF0, CreateInstance(mat) 0x4C5110, AtomicSetupLightingCB 0x4C4F30, CDamageAtomicModelInfo::SetDamagedAtomic 0x4C48D0, CAtomicModelInfo::SetAtomic 0x4C4360,
CCustomBuildingRenderer::IsCBPCPipelineAttached 0x5D7F40, CCarFXRenderer::IsCCPCPipelineAttached 0x5D5B80.
Infra: the exe's RwFrameForAllChildren (0x7F0DC0), RpHAnimFrameGetHierarchy (0x7C5160), RpSkinAtomicSetHAnimHierarchy (0x7C7520) patched to host stubs, the port's fakerw entry points of the same names defined to the same bodies; the exe's
_stricmp (0x8229B6) patched to a C-locale host body (its locale/lock plumbing recurses without CRT init); hand-built frame trees; every atomic/geometry field written at both the exe offset and the librw offset (the port's structs are librw's);
a fake vftable for the virtual CreateInstance(); the Models oracle target now also links PipelinePlugin, 2dEffect, CustomBuildingRenderer/DNPipeline, CarFXRenderer, RwHelper (they were unresolved = port faults). `W3M_ONLY=<substring>` runs only the matching rows.

### Port bugs found and fixed (one commit each)
| function | exe | bug | pre-fix failures (PC24) |
|---|---|---|---|
| CClumpModelInfo::FindFrameFromNameCB / FindFrameFromNameWithoutIdCB (-> GetFrameFromName, SetFrameIds) | 0x4C52A0 / 0x4C52F0 | exe calls the CRT `_stricmp` (0x8229B6); the port used `strcmp` (case-sensitive name match) | 40/400, 18/400, GetFrameFromName 54/400, SetFrameIds 25/500 |
| CClumpModelInfo::CreateInstance(RwMatrix*) | 0x4C5110 | exe copies the matrix to frame+0x10 (modelling); the port memcpy'd over the frame start (object header) | 148/200 (all non-null cases) |
| CCustomBuildingRenderer::IsCBPCPipelineAttached | 0x5D7F40 | exe tests the night-colours pointer [geometry+plugin offset]; the port tested the plugin data address (never null) -> true for every pre-lit geometry, then ran the building pipeline set-up | 122/600 (+562-586/1200 port-only faults downstream) |

Consumer audit: no layout/index/size change; callers of each function listed in the commit messages (clump/vehicle component lookups; Entity.cpp:333, Object.cpp:111 and the three model-info set-up callbacks for the predicate).

### Not done (token budget)
CClumpModelInfo::SetClump (0x4C4F70: needs RpClumpForAllAtomics/RpSkinGeometryGetSkin/RpSkinGetVertexBoneWeights stubs and a librw+RW3.6 pair of geometries; asm read: sphere radius *= 1.2, weights normalised with 1/(w0+w1+w2+w3), hierarchy flags 0x3000 - port matches by reading),
CWeaponModelInfo::SetClump, the CVehicleModelInfo RW callbacks (SetVehicleComponentFlags, GetWheelPosn, GetOriginalCompPosition, SetEnvironmentMapCB, PreprocessHierarchy ...), Entity root class C (22).

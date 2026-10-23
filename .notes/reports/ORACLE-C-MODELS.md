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

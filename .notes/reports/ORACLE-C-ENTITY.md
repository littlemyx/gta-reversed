# ORACLE-C-ENTITY report

Agent ORACLE-C-ENTITY (rules v4), worktree C:/gta-wt/oracle-c-entity, 2026-10-24 01:15 - ~02:15 WEST. Stopped at the token budget (not the wall clock); nothing in progress.

| | before | after |
|---|---:|---:|
| whole-game covered (oracle_coverage.py, same tree) | 5042 | **5049** (+7) |
| game_sa/Entity covered / uncovered class C | 87 / 22 | **94 / 15** |
| wave3_entity_oracle_test rows | 52 | **59** (PC24 0 mismatches) |
| port bugs fixed | | 1 |

## Rows (tests/standalone/wave3_oracle_entity_e.inc, ROW filter env `W3E_ONLY=a|b` added to the entity harness)
CEntity::HasPreRenderEffects 0x532B70 (special model-index globals, glass/pickup/traffic-light, 2dfx light scan), GetIsOnScreen 0x534540, IsVisible 0x536BC0, GetIsBoundingBoxOnScreen 0x5345D0,
FindTriggerPointCoors 0x533380, RenderEffects 0x5342B0 (road-sign effects mostly fault identically on a garbage atomic: loop logic compared, the roadsign render itself not), RemoveEscalatorsForEntity 0x717900.
Infra: the entity target now links Camera.cpp, Escalator(s).cpp, TrafficLights.cpp, BaseModelInfo.cpp, standalone/rw/math.cpp (+tables); CModelInfo::Get2dEffectStore defined in the test (ModelInfo.cpp not linked);
the exe's RwV3dTransformPoints (0x7EDD90) / RwV3dTransformVector (0x7EDDC0) are Patch()ed to the port's own functions (the exe's dispatch through the RW engine table is not initialised); a fake model-info
vftable (all slots return 0) serves the first virtual call both Get2dEffect bodies make (GetRwModelType); VtGuard writes the exe / port CEntity vftable into the entity word 0 around a call; CGame::m_pWorkingMatrix1/2 get
fixture buffers for the TransformVectors path.

## Port bug fixed
| function | exe | bug | pre-fix failures (PC24) |
|---|---|---|---|
| CEntity::RemoveEscalatorsForEntity | 0x717900 (loop 0x717910..0x717933) | the exe walks all 32 escalators and switches off every one that uses the entity; the port returned after the first match | 587/600 |

Consumer audit: no layout / meaning change. Callers: entity destruction paths (CWorld/CBuilding/CObject removal) call it to drop the escalator that owns the entity; the only observable change is that a second
escalator sharing the entity is now also removed, as in the exe. CEscalators::GetAllExists() (filter view) tolerates m_Exist being cleared during iteration.
Soak (Release build of the worktree, NOVSYNC, 3 min, new game): RESULT OK, 0 errors, 45300 frames (C:/gta-scratch/oce/soak_out). No script / streaming / population / path / police code touched, gameplay_regress not run.

## Not done (token budget)
Entity class C left (15): CEntity::SetModelIndex (needs a fake vtable with recording stubs: slots 6/7), SetModelIndexNoCreate, Render, UpdateRpHAnim, ModifyMatrixForCrane, SetRwObjectAlpha, SetAtomicAlpha,
MaterialUpdateUVAnimCB (all RW-bound: need hand-built atomic/geometry/material fixtures as in wave3_oracle_models6.inc), CPhysical::{ApplyCollision-1 0x5435C0, ApplySoftCollision-1 0x543890,
DettachAutoAttachedEntity 0x5446A0 (virtual SetIsStatic via vptr: VtGuard with w3evt_CPhysical / 0x863BA0 is ready), ApplyCollisionAlt 0x544D50, ProcessShiftSectorList 0x546670, ApplySpeed 0x547B80,
PlacePhysicalRelativeToOtherPhysical 0x546DB0 (CWorld::Remove/Add)}. Models leftovers (CClumpModelInfo::SetClump, CWeaponModelInfo::SetClump, CVehicleModelInfo RW callbacks) not started.

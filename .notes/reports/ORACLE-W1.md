# ORACLE-W1: exe-oracle coverage inventory + wave 1 (Windows host, 2026-10-17)

Worktree C:/gta-scratch/wto1 (StandaloneRW + StandaloneRelease), scratch C:/gta-scratch/ow1. Branch reverse/interior_c, base 66d98061.

## Result
* **Inventory**: `tools/standalone/oracle_coverage.py` + `.notes/ORACLE_COVERAGE.md` (9240 ported functions = RH_Scoped*Install sites; covered by an address literal in tests/standalone: **1139 -> 1715**; the uncovered rest classified A pure / B globals / C vtable / D imports per subsystem, with the reason for D and a "next candidates" list).
* **Wave 1**: 577 new exe-oracle rows (141 hand-written in `wave1_oracle_test`, 436 generated in `wave1_consts_oracle_test`), 4000 cases x PC24 + PC53 per hand-written row, 16 per generated row (constants).
* **11 real port bugs found and fixed** (one commit each, asm addresses in the messages), 0 mismatches left at PC24 in both new tests.

## Rows added
| area (inc) | rows | what |
|---|---:|---|
| surface | 46 | SurfaceInfos_c: 43 property queries (bit-field positions, signedness, wet multiplier), GetAdhesiveLimit, GetSurfaceIdFromName (all real names + mutated) |
| camera | 51 | CCamera queries (FOV, look direction, fade status/direction, cut-scene finish, spline pos ...), Fade / ProcessFade, Vector{Move,Track}Linear, LerpFOV, AddShakeSimple, CamShakeNoPos, TakeControl{NoEntity,WithSpline}, widescreen, weapon-mode, collision-set helpers (SetColVars*, SetCamCollisionVarDataSet), special-case helpers, IsExtraEntityToIgnore |
| peds | 14 | CPed state predicates (IsPedInControl, CanBeArrested, CanSetPedState, CanPedReturnToState, CanStrafeOrMouseControl, UseGroundColModel, CanUseTorsoWhenLooking, IsAlive), ClearAimFlag, SetIdle, ClearAll, SetWeaponSkill, SetLookTimer, SetPedStats |
| ai | 19 | CPlayerRelationshipRecorder (6), CStuckCarCheck (5), CPedGroupMembership (5), CCarAI::FindSwitchDistanceClose/Far, FindPoliceBikeMission... |
| entities | 11 | CPlaceable::SetPosn x2 / IsWithinArea x2 (script "is in area"), CEvent{Danger,CarUpsideDown,ChatPartner,DraggedOutCar,GotKnockedOverByCar,CopCarBeingStolen}::AffectsPed, CEventBuildingCollision::IsHeadOnCollision |
| consts (generated) | 436 | the tiny constant-returning overrides: GetTaskType (~200), GetEventType/Priority/LifeTime, GetLocalSoundLevel, GetWanderType, GetModelType, CanBeInterruptedBySameEvent, ControlSubTask/CreateNextSubTask/MakeAbortable/IsInterruptable stubs |

## Mismatches -> root cause -> fix
| # | function (exe) | cause | commit |
|---|---|---|---|
| 1 | CCamera::ProcessFade 0x50B5D0 | duration 0: exe SETS alpha (0 / 255), port added 0 / 255: a zero-length fade-out never ended | bdb41739 |
| 2 | CCamera::IsExtraEntityToIgnore 0x50CE80 | exe scans the first `count` slots, port the whole array | f23cc3ce |
| 3 | CCamera::SetZoomValueFollowPedScript 0x50C160 | values other than 0/1/2 only set the flag in the exe; port stored 0.25 | 3498d220 |
| 4 | CCamera::SetColVarsPed 0x50CC50 | non-player ped types return in the exe; port switched to collision set 0 | 2ed03239 |
| 5 | CCamera::CameraVehicleModeSpecialCases 0x50CDE0 | slow threshold constant 0.010000001f (port 0.1f), squared-speed compare | 8627e74f |
| 6 | CCamera VectorMoveLinear/TrackLinear 0x50D160/0x50D1D0, LerpFOV 0x50D280, AddShakeSimple 0x50D240, ProcessVector*Linear 0x5164A0/0x516440, ProcessFOVLerp 0x516500 | exe keeps `fild` of the ms timer unrounded (and rounds the +2^32 correction at PC24): port converted to float first (second rounding) -> 1-ulp differences once the timer > 2^24 ms or >= 2^31 | eff7ee9b |
| 7 | CPed::ClearAll 0x5E5320 | exe clears bits 4/31 = bKnockedUpIntoAir/bKnockedOffBike; port cleared bHitSteepSlope/bCrouchWhenScared | a27df6c0 |
| 8 | CStuckCarCheck::HasCarBeenStuckForAWhile 0x463C00 | first record with the handle decides (port: any stuck duplicate) | e8123b4a |
| 9 | CPedGroupMembership::IsFollower 0x5F69E0 | exe scans follower slots 0..6 only | 20c257fb |
| 10 | CTaskInteriorBeInShop::GetTaskType 0x675450 | class had no `Type`: returned the base CTaskComplexWander::Type (912) instead of 1404 | baed7255 |

(1 hit per ~3 hand-written functions in the camera/ped/AI rows; the 436 generated rows found 1 bug.)
Non-bugs found while writing rows (documented in the row code): CCamera::CameraPedModeSpecialCases takes an ignored stack argument in the exe (`ret 4`); CCarAI::FindPoliceBikeMissionForWantedLevel and the GetModelType family return in AL only (upper EAX bits are garbage): rows compare the returned width.

## Masks / fixture rules (all deliberate)
* **Canonical bools**: fixtures keep plain `bool` members 0/1 (tests/standalone/tools/gen_bool_offsets.py -> wave1_oracle_bools.inc). The exe tests some flags with `cmp byte,0` and others with `cmp byte,1`/`sete`; a bool of 2..255 is UB for the port (found with SetWideScreenOff / ProcessFade).
* **No NaN/Inf** in game-state floats (FillGarbage turns an all-ones exponent into 0x3F; GF/GVf never produce NaN/inf/huge): x87 compare+fnstsw and C++ `>` / `max` treat unordered differently; no game state is NaN.
* Camera time step is never denormal (the port returns the step as a float, the exe keeps the product on the x87 stack).
* ePedState is drawn from 0..79 (out-of-range values are decoded differently by the exe's tables and never occur); ped in vehicle implies m_pVehicle (both sides dereference it); IsHeadOnCollision gets a ped matrix.
* Event/ped/vehicle pointers are small pools of real buffers (or null), never random.
* `GetModelType`, `GetLifeTime` of CEventVehicleToSteal (mov al), RunTimedAnim::GetTaskType (movzx word): compared at the width the exe returns.

## Mutation tests
11 source mutations, each caught (PC24 mismatches in brackets): GetTyreGrip *0.11 (3514), IsAudioTile wrong bit (1477), FindCamFOV +1 (3391), LerpFOV factor (3998), CanBeArrested missing case (51), CPedGroupMembership::IsMember null check dropped (769), IsCarInStuckCarArray `<=` (1474), CEventDanger `>=` -> `>` (3: boundary equality only), CPlaceable::IsWithinArea `>` (1399), GetRelationshipWithPlayer +1 (3252), FindSwitchDistanceFar 51 (1340); plus a TaskSimpleSay Type edit caught by the generated rows (16/16). Script: C:/gta-scratch/ow1/mut.py.

## Gates
* `run_all_tests.sh build/StandaloneRW` (worktree, GPU lock): **44 tests: 44 ok** (42 before + wave1_oracle_test + wave1_consts_oracle_test). wave1_oracle_test: 141 rows, PC24 0 / 0 (PC53 410 = the info mode, camera/time rows compiled for PC24); wave1_consts_oracle_test: 436 rows, PC24 0 / 0. The pre-existing oracle tests stay green (gameplay_oracle: PC24 57 / 0 hard = NaN-payload only, as before).
* StandaloneRelease built in the worktree (1121/1121, gta_reversed.exe 16.9 MB); soak 3 min (`soak.sh build/StandaloneRelease 3`, route 393 s, 21 markers, last `quit_ask`, exit via atexit): **RESULT: OK**, ~25 fps.
* Next-wave levers (details in ORACLE_COVERAGE.md): (1) `oracle::Patch` of the exe's operator new/delete + CRT lock helpers on both sides makes 773 of the 2785 class-D functions oracle-able (task/event Clone + constructors, vehicles); (2) 1445 class-D functions are allocator-only, 953 file IO (needs a CFileMgr stub); (3) class A leftovers: Tasks 282, root files 396, Events 182 (AffectsPed family beyond the 6 done), ControllerConfigManager 42; (4) fixture helper `Both` + `gen_bool_offsets.py` + `gen_const_rows.py` make new areas ~30 lines per function.

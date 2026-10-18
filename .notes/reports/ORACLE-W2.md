# ORACLE-W2 report

Agent ORACLE-W2, branch reverse/interior_c (worktree C:/gta-scratch/wto2, branch oracle-w2). Builds on ORACLE-W1 (`.notes/reports/ORACLE-W1.md`).

## Result

| | |
|---|---|
| new exe-oracle target | `wave2_oracle_test` (the 45th test of `run_all_tests.sh`) |
| rows | **1137** (268 constructor, 259 Clone, 610 member / static functions), 60-150 cases each, PC24 and PC53 |
| committed test | **0 mismatches, 0 faults** (PC24 0, PC53 0), exit code 0, about 10 min |
| exe-oracle coverage | 1715 -> **2872** of 9249 hook sites (`.notes/ORACLE_COVERAGE.md`, new class S); game_sa/Tasks/TaskTypes 347 -> 1269, game_sa/Events 116 -> 292 |
| port bugs fixed | **57 commits**, one per fix, asm address in the subject (list at the end) |
| mutation test | 13 of 13 mutants caught |
| rows that cannot run | 369 lines in `tests/standalone/tools/w2_skip.tsv`, each with its reason (section 5) |

## 1. Import patching (`tests/standalone/oracle_alloc.h`)

The "D" functions of W1 that reach only allocator / lock imports are oracle-able now (class S in the coverage map).

* **exe side**: the CRT entry points (`malloc`, `realloc`, `calloc`, `free`, `operator new`, `operator delete`) are overwritten in the mapped exe image with 5-byte `JMP`s to arena functions; the 16 kernel32
  imports the CRT still reaches (heap, critical sections, TLS) point to stdcall stubs of the harness.
* **port side**: the host `operator new/delete` of the test binary is routed to the **same arena** while a case runs, so a function that allocates (a task, a vector, a pool slot) gets the same address on both sides.
* **deterministic allocator, not pointer masking**: a bump arena that `Begin(seed)` refills with a seed pattern before every case. Uninitialised bytes, addresses and stored pointers are bit-identical on both
  sides. Masking was rejected: it would hide a wrong allocation size or order, and "the port left a member uninitialised" is exactly the bug class the playtests found.
* self-tests (`TestW2Infra`): arena sequence, rand streams (`SeedBoth`), IAT stub.
* **fake pools**: the task pool (stride 0x80) and the event pool (stride 0x44) sit at the exe globals 0xB744A8 / 0xB744AC; both sides run `CPool::New` on the same struct.
* **three-way compare** (init / port / exe): a bit the exe changed that the port left alone is a FAIL ("port left it uninitialised"); a bit only the port wrote is INFO. Compared: return value, the object, both
  pools, the arena, the entity reference world (`aRefs`, the registered-reference lists) and the whole exe data image 0x8A4000..0xCB0000 (every global the function touched).
* **fault policy**: both sides fault = agreed, case skipped; exe-only fault = skipped (garbage vptr, uninitialised exe global); port fault at EIP 0x400000 (unresolved symbol) = skipped; any other port-only fault =
  mismatch; a row whose cases are all skipped is "NOT ORACLE-ABLE" and counts as a failure, so the committed test contains none.
* exe crashes that kill the process (fast-fail, stack overflow) cannot be caught: `W2_SKIPFILE` + `W2_START` let a driver script resume after the crashed row; such rows went into `w2_skip.tsv`.
* options: `wave2_oracle_test.exe <substring>... [-v] [-n cases] [-trace]` (row filter, verbose mismatches).

## 2. Row generators (`tests/standalone/tools/gen_w2_rows.py`, `gen_w2_funcs.py`)

They parse the class headers and the `RH_Scoped*Install` sites, build a garbage `this` (entity pointers into the reference world, other pointers null, plain bools 0/1, enums in range, special floats every
few cases) and call the port's definition and the exe function on identical memory. Private / abstract classes are reached through an explicit-instantiation access trick. Priority order: police, combat, vehicle,
interior, group, attractor and flee tasks; their constructors, `Clone`, `MakeAbortable`, `ProcessPed`, `Create*SubTask`, `ControlSubTask`, `AffectsPed`, anim callbacks.

Fixture artefacts handled in the generators (never in the port): garbage bools / flag bytes, out-of-range enums (including `eAbortPriority`), negative ints for `CTaskTimer::Start`, uninitialised exe globals
(the harness does not run the CRT initialisers), embedded vptrs, x87-only extended exponent range.

## 3. Port bugs

Common patterns: a member the exe sets in the constructor / Clone / first sub task that the port forgot (the playtest bug class); a reference registered for the by-value parameter instead of the member (a dangling
stack slot in the entity's reference list: BeHit, Partner, FollowLeaderAnyMeans); a virtual call or event dereference before the exe's early-out (null event with IMMEDIATE priority: SwatRope, GiveCPR); structurally
wrong state machines (FollowPointRoute, StealCar, SeekCover, ScriptedBrain, PolicePursuit, KillCriminal).

Gameplay-relevant:

* `CTaskComplexPolicePursuit` never stored its cop (`m_Pursuer`), so the destructor never removed the cop from the wanted level's pursuit list (0x6908E9, 0x68D880).
* `CTaskComplexKillCriminal::FindNextCriminalToKill` (0x68C3C0) measured from the wrong ped, seeded the result with the current criminal and dereferenced it when null.
* `CTaskComplexFollowPointRoute::CalcGoToTaskType` (0x66E150) cleared / tested the wrong flags (route followers did not slow down for corners); `CreateNextSubTask` (0x671630) advanced past the end of the route after
  stand-still / none.
* `CTaskComplexStealCar::CreateFirstSubTask` (0x644210) ended the task at once when the vehicle was gone (the exe pauses).
* `CTaskComplexUseAttractor::ControlSubTask` (0x6327C0) aborted its sub task for every non-object entity (`||` instead of `&&`).
* `CTaskComplexUseScriptedBrain` (0x6359C0 / 0x635A90): the finished-brain test and the 2 s pause fallback were missing.
* `CTaskComplexTurnToFaceEntityOrCoord::ComputeTargetHeading` (0x66B9D0) and `CTaskComplexEvasiveDiveAndGetUp::CreateSubTask` (0x653820) used `CVector::Heading()` where the exe calls
  `GetRadianAngleBetweenPoints` (different range and axis handling). **Other `Heading()` callers in `Tasks/` (ReactToGunAimedAt, WaitForBackup, SimpleBeHit, AchieveHeading, WalkRoundBuildingAttempt) are not
  yet checked against the asm** - no row reaches them.
* `CEventHandler::ComputeObjectCollisionResponse` (0x4B92B0) copied the passive variant (extra `bInVehicle` test, `nullptr` stored on every failing path).
* `IKChainManager_c::CanAccept` (0x618800) read the frame of a null clump (LookAt on a ped that is not streamed in).
* `CTaskSimpleGiveCPR::MakeAbortable`, `CTaskComplexUseSwatRope::MakeAbortable`: crash on the common `MakeAbortable(ped, IMMEDIATE)` call (null event).
* constructor / Clone gaps: Wander (flag nibble), CarDrive (`m_bAsDriver`), InteriorSitAtDesk, TrackEntity (`m_fMoveRatio = -1`), InvestigateDeadPed, BikeJacked, Choking, GunControl, FightingControl,
  GoPickUpEntity (sentinel -999), RunNamedAnim, ...

Exe quirk deliberately NOT replicated (`refdiff:` rows in the skip table): `CTaskComplexDestroyCarArmed` registers the by-value parameter (0x621FD5, a stack slot in the vehicle's reference list).

## 4. Mutation test

13 mutations applied to the port (not to the test), one rebuild, the named rows run: **13 / 13 caught** - Wander flag nibble (49 mismatching cases), CarDrive `m_bAsDriver` (105), SitAtDesk `m_Anim` (133),
FightingControl default (144), Choking time (47), SeekEntityAiming copy ctor (120), GunControl defaults (120), Duck timer start (45), GoToPointFine flags (39), Pause timer (27), an extra allocation in a Clone
(120, address shift), a skipped pool slot (120), a swapped registration order (150). Restored with `git checkout` afterwards.

## 5. What is not covered, and why (`tests/standalone/tools/w2_skip.tsv`)

| rows | reason |
|---:|---|
| 165 | every case faults identically in port and exe (the fixture lacks the object the function dereferences): no information. Includes `CEventDamage::AffectsPed` - the fix there is asm-verified but not oracle-verified |
| 161 | the exe kills the process on the garbage fixture (fast-fail / stack overflow); includes many destructors |
| 26 | the port asserts (MSVC debug check: `std::array` index / `ranges::take` with a garbage negative count) where the exe silently reads / loops; a state the game never produces |
| 3 | the port copies a garbage unterminated name with `strcpy_s` (the exe copies a bounded number of bytes) |
| 2 | **OPEN**: `CTaskSimpleUseGun::ControlGunMove`, `CTaskSimpleDuck::ControlDuckMove`: 1 of 60 special-input cases differs (port 17.9 / exe -113.0). The step logic matches the asm for every ordinary input |
| 1 | **OPEN**: `CTaskComplexFacial::CreateNextSubTask`: 1 of 60 special-input cases leaves `m_RequestB` 0 (port) vs -1 (exe); asm and source agree on every path I read |
| 1 | `CTaskComplexTrackEntity::ControlSubTask`: the hook is Locked (the exe version is used in the game) |
| 1 | `CTaskComplexTrackEntity::CalcMoveRatio`: the exe's lazily-initialised function statics live in the exe data image, the port's in its own storage (compare differs by construction; values checked by hand) |
| 2 | `CTaskComplexSeekEntityAiming::Create{First,Next}SubTask`: embedded vptr artefact |
| 2 | FollowNodeRoute / FollowPointRoute `CalcBlendRatio`: port faults inside `std::tuple` / vector access on a garbage route where the exe reads mapped garbage |
| 1 | `CTaskSimpleSay::Constructor`: wrapper-typed parameter, no generator yet |

Other notes: a row with garbage pointers can corrupt the arena and make later rows fault (seen once after `CTask::operator delete`, now skipped), and garbage values derive from addresses, so a new row can change the
luck of a neighbour - re-run the whole test after adding rows.
## 6. Gates

| gate | result |
|---|---|
| `run_all_tests.sh build/StandaloneRW` (45 tests) | **45 / 45 ok**: the 26 oracle tests (incl. `wave2_oracle_test`) and the 21 `rw_*` tests, run in two batches (see note) |
| Release build (`build/StandaloneRelease`, main checkout, HEAD with all landed commits) | OK |
| soak 3 min (`soak.sh build/StandaloneRelease 3`) | first run **FAIL** (the START GAME click missed, the game sat in OPTIONS, 0 errors, no crash); re-run **RESULT: OK** (22 markers, 0 errors, 2 gate timeouts, peak committed 511 MB) |

Notes: (1) an ownerless `gpu.lock` directory (created 13:18, no owner file, no game process) blocked every agent for ~40 minutes; the 26 CPU-only oracle tests were therefore run without the lock, the 21 `rw_*` tests and both soaks under it. (2) The first soak's menu click miss is not a game-code failure (it never reached the world); GAMEPLAY-REGRESS was working on `input_mouse` in the same period. (3) The user's `gta_sa.set` was restored from `C:/gta-scratch/playtest1/gta_sa.set` after the soaks.

## 7. Commits (57 port fixes, in landing order; subject = function + asm address + fix)

- CTaskComplexWander ctor 0x66F450: clear the whole flag nibble
- CTaskComplexCarDrive ctor 0x63C940: m_bAsDriver = true
- CTaskInteriorSitAtDesk ctor 0x676010: zero m_Anim and the three state flags
- CTaskComplexPartner ctor 0x681E70: register m_partner, not the parameter
- CTaskSimpleBikeJacked ctor 0x648B90: store the jacker
- CTaskSimpleBeHit ctor 0x620780: register m_Attacker, not the parameter
- CTaskSimpleCarOpenDoorFromOutside ctor 0x645E50: register m_veh; drop a debug file write
- CTaskComplexInvestigateDeadPed ctor 0x696DF0: store the dead ped
- CTaskComplexWaitForBackup ctor 0x697290: register the aggressor first
- CTaskComplexTrackEntity ctor 0x65F3B0: m_fMoveRatio = -1
- CTaskSimpleFightingControl ctor 0x61DC10: m_unk3 = 1.0f
- CTaskSimplePickUpEntity ctor 0x6917B0: zero m_vecPickuposn
- CTaskSimpleChoking ctor 0x6202C0: choke time is 1000 + [0,1000)
- CTaskSimpleRunTimedAnim ctor 0x61AB70: m_taskId = TASK_SIMPLE_TIMED_ANIM
- CTaskSimpleUseAtm: task id is TASK_SIMPLE_USE_ATM (420), not 723
- CTaskComplexFacial ctor 0x690D20: chewing flag formula
- CTaskComplexGoToPointAiming ctor 0x668790: set m_newTargetSet
- CTaskComplexStareAtPed ctor 0x660270: validate the ped pointer
- CTaskComplexSmartFleeEntity::Clone 0x65CF40: copy the move state
- CTaskComplexOpenPassengerDoor::Clone 0x6438E0: copy the move state
- CTaskComplexGoPickUpEntity ctor 0x6919C0: the 'not set' sentinel is -999.0f
- CTaskSimpleWaitUntilLeaderAreaCodesMatch::Clone 0x5F67F0: do not clone the runtime state
- CTaskSimpleGunControl ctor 0x61F3F0: m_isFirstTime = true, m_isLOSBlocked = false
- CTaskComplexSeekEntityAiming copy ctor (Clone 0x695820): copy the seek radius
- CTaskSimpleDuck::SetDuckTimer 0x692530: restart the duck timer
- CTaskSimpleGoToPointFine::SetTargetPos 0x65F330: set the 0x20 flag
- CTaskSimpleStealthKill::FinishAnimStealthKillCB 0x622790: set, never clear, the abort flag
- CTaskSimplePause::ProcessPed 0x48E830: start the timer without an interval test
- CTaskSimpleRunNamedAnim ctor 0x61A990: clear m_bOffsetAvailable
- CTaskComplexBeCop::Clone 0x499440: clone the go-to task only when present
- CEventGroup::HasEventOfType 0x4AB5E0: a null event answers true
- CTaskManager::HasTaskSecondary 0x681820: plain pointer compare over the six slots
- CTaskInteriorShopKeeper::ControlSubTask 0x675740: consume m_bStartServeAnim
- CTaskComplexFallAndGetUp::MakeAbortable 0x6787F0: LEISURE always answers false
- CTaskComplexSitDownThenIdleThenStandUp::MakeAbortable 0x631510: ask the sub task with LEISURE
- CTaskSimpleCarCloseDoorFromInside::MakeAbortable 0x64AF50: return false on every path
- CTaskSimpleCarDrive::UpdateBopping 0x63C900: single rounding for the progress
- CTaskSimpleSitIdle::MakeAbortable 0x6311E0: also set m_durationMs to -1
- CTaskSimpleUseGun 0x61E040 / 0x61E0C0: ControlGun aborts the IK chains, ControlGunMove steps instead of clamping
- CTaskSimpleCarShuffle::FinishAnimCarShuffleCB 0x646970: null-check the anim
- CTaskComplexUseSwatRope::MakeAbortable 0x659530: inspect the event only for non-IMMEDIATE
- CTaskComplexSeekCoverUntilTargetDead::CreateNextSubTask 0x622C90: swapped branches
- CTaskComplexUseScriptedBrain::CreateNextSubTask 0x6359C0 / CreateFirstSubTask 0x635A90
- CTaskComplexTrackEntity 0x663640 / 0x65F460: expiry is start + interval; dtor skips a null target
- CTaskComplexUseAttractor::ControlSubTask 0x6327C0: abort only for a broken object
- CTaskComplexKillCriminal::FindNextCriminalToKill 0x68C3C0: reference distance is the current criminal, result starts null
- CTaskComplexTurnToFaceEntityOrCoord::ComputeTargetHeading 0x66B9D0: normalise, then GetRadianAngleBetweenPoints
- CTaskComplexOnFire::CreateSubTask 0x633470 / CTaskGangHassleVehicle::CreateNextSubTask 0x65FC80: read fEntityPosChangeThreshold at runtime
- CTaskComplexFollowLeaderAnyMeans ctor 0x694AA0: register the member, drop the DebugBreak
- CEventHandler::ComputeObjectCollisionResponse 0x4B92B0: no bInVehicle test, store only on success
- IKChainManager_c::CanAccept 0x618800: null clump test
- CTaskComplexStealCar::CreateFirstSubTask 0x644210 / CreateSubTask 0x640F30: pause without a vehicle, driver only when present
- CTaskComplexPolicePursuit::CreateFirstSubTask 0x6908E0: store the pursuing cop (m_Pursuer)
- CTaskComplexFollowPointRoute::CalcGoToTaskType 0x66E150 / CreateNextSubTask 0x671630: follow the asm
- CEventDamage::AffectsPed 0x4B35A0: no up-front GetSourceEntity()
- CTaskSimpleGiveCPR::MakeAbortable 0x6589B0: do not read the event for IMMEDIATE
- CTaskComplexEvasiveDiveAndGetUp::CreateSubTask 0x653820: heading through GetRadianAngleBetweenPoints

# ORACLE-W3-TASKS report

Agent ORACLE-W3-TASKS (rules v2), worktree C:/gta-wt/oracle-w3-tasks, branch oracle-w3-tasks. New target `wave3_tasks_oracle_test` (49th test): **40 rows, PC24 0 / PC53 0 mismatches**.

| | before | after |
|---|---:|---:|
| game_sa/Tasks/TaskTypes covered | 1272 | **1312** |
| uncovered A / B in Tasks/TaskTypes | 49 / 118 | **36 / 91** |
| exe-oracle coverage, whole game | 3203 | **3243** |
| port bugs fixed (one commit each, asm address in the subject) | | **2** |

## Port bugs (each row failed on the pre-fix code, log C:/gta-scratch/w3tasks/loop_prefix.log)
| function | asm | bug |
|---|---|---|
| CTaskSimpleWaitForBus::ProcessPed 0x632AA0 | 0x632B12 `xor al,al / ret 4` (no match), 0x632B1C `mov [ecx+8],edx / mov al,1` (match) | returned true always and reset m_bus to nullptr when no bus was found (51/60 cases); strict `range^2 > speed^2`, x87 sum |
| CTaskComplexGoToCarDoorAndStandStill::IsVehicleInRange 0x6458A0 | 0x6458EE fcompp / 0x6458F8 test ah,0x41 / 0x6458FD jne -> 0 | `sum <= range^2` is true for an exactly equal distance, the exe returns false (7/60 cases); x87 residency (double) |

## Infrastructure
- `tests/standalone/tools/gen_w3_tasks.py` (fork of gen_w3_ped.py): member / static / constructor / Clone / destructor rows for the uncovered A+B TaskTypes sites; struct returns (CVector, CColSphere, RwV3d) through the hidden pointer, buffer arguments (CVector&, CColSphere&, CNodeAddress&, CPointRoute&, PedsToAvoidArray&, uint8&), ped arguments from the wired ped / wired reference world, per-row exe-static presets (`PRE`), the task object built with the port constructor (`w3t::Build<T>`) when its parameters are producible.
- `tests/standalone/wave3_oracle_tasks.inc` (W3TaskCase on the ORACLE-W3-EVENTS fixture: `wave3_oracle_ev.inc` is included fixture-only through `W3_EV_FIXTURE_ONLY`), `wave3_oracle_tasks_hand.inc` (CanSunbathe 0x632140), target in `source/CMakeLists.txt`, switch `W3_TASKS_ONLY` in `wave2_oracle_all.inc`.
- Exe statics the harness presets (CRT start-up / lazy first call): 0xC18CE8 = 60.0 (flee safe distance read by CTaskComplexLeaveCarAndFlee::CreateSubTask; the port passes the literal 60), 0xC18CF4..0xC18D08 (CTaskComplexTrackEntity::CalcMoveRatio function-local statics 1.25, 0.25, 25.0, 1.0, 0.04 + guard 0x1F).

## Not covered: 117 functions skipped with a reason (tests/standalone/tools/w3_tasks_skip.tsv) + 10 the generator cannot parse (templates, private overloads, CEntity** arguments)
- 31 "exe process killed" (Clone / CreateFirstSubTask / CreateSubTask / destructors: the exe function has an SEH frame and faults on a pointer of the garbage fixture, e.g. CTaskComplexAttractorPartnerWait::m_CurrentFxPair) and 66 "every case faults identically" (same cause without the SEH frame): need a per-class fixture (valid pointed-to objects), see the state file.
- 16 layout / assert artefacts, analysed: padding bytes only the exe writes or leaves (IKChain / IKLookAt / IKPointArm / Say ctors, IKChain / UseGun Clone), embedded vptr of CTaskComplexSeekEntity<> (+0x40), port std::array asserts on garbage indices (GetStrikeDamage, IKManager, ComputeNearestLivingGroupPed), strcpy_s of a garbage name (UseScriptedBrain).
- 2 open possible port differences: CTaskSimpleCarGoToPointNearDoorUntilDoorNotInUse::Clone 0x646EC0 (port copy constructor faults in 36/60 cases where the exe does not) and CTaskSimpleCarShuffle::Clone 0x649C40 (CEntity::GetType fault in 1/60): need analysis with a valid object.
- By hand: template classes (CTaskComplexSeekEntity<...> ctor / Clone, 5), CTaskSimpleHoldEntity / PickUpEntity private ctor overloads, CEntity** array arguments (CTaskComplexAvoidEntity::ComputeBoundingSphere, CTaskComplexDriveToPoint::IsTargetBlocked), static `ShouldLoadSunbatheAnims` (internal linkage).

Trailer used: `Co-Authored-By: Claude Sonnet 5.5` (the session attribution reminder), not the Opus 5.5 trailer named in the brief.

## continuation 2 (ORACLE-W3-TASKS-2, rules v2, worktree oracle-w3-tasks2)
`wave3_tasks_oracle_test`: 40 -> **83 rows**, PC24 0 / PC53 8 (PC53 is informational: CTaskComplexWalkRoundBuildingAttempt::ComputeMoveDir's float vector math vs the exe's 53-bit x87). Coverage 3343 (after W3-COLL) -> **3386**; Tasks/TaskTypes 1312 -> 1355 covered, uncovered A 36 -> 23, B 91 -> 61. run_all_tests 51/51 ok in the worktree (after the rebase on W3-COLL).

### Port bugs (9, one commit each, asm address in the subject; each row failed on the pre-fix code, logs C:/gta-scratch/w3tasks2/loop_prefix_*.log)
| function | asm | bug (pre-fix failing cases) |
|---|---|---|
| CTaskSimpleCarGoToPointNearDoorUntilDoorNotInUse::Clone 0x646EC0 | 0x646EF3..0x646F0D | copy constructor read its own uninitialised sub task pointers instead of the source's (36/60) |
| CTaskSimpleCarShuffle::Clone 0x649C40 | 0x649C73..0x649C81 | same pattern: vehicle / door / line-up utility of `this` instead of the source (59/60) |
| CTaskComplexWalkRoundBuildingAttempt::ComputeMoveDir 0x654B80 | 0x654B80..0x654C2C | measured from m_pos instead of the ped position, no Normalise, no right-vector fallback (44/60) |
| CTaskComplexFollowPointRoute::GetNextWaypoint 0x669420 | 0x669459..0x669466 | LOOP mode returned the zero vector (dead LOOP case) (7/60) |
| CTaskComplexFleeEntity::CreateSubTask 0x65BA30 | 0x65BA72 | also restarted the position timer and updated m_pos (20/60) |
| CTaskComplexStealCar::CreateSubTask 0x640F30 | 0x641066 | no null test of the ped for TASK_FINISHED (9/60) |
| CTaskComplexCarSlowBeDraggedOut::CreateSubTask 0x64C6A0 | 0x64C74C sete cl | CTaskSimpleCarSetPedOut got m_IsDraggedBySelf instead of its negation (5/60) |
| CTaskComplexDrivePointRoute::CreateNextSubTask 0x63CF90 | 0x63CFAF..0x63CFB5 | post-increment compared the old index: built a drive task for point N (3/60) |
| CTaskComplexPolicePursuit::CreateSubTask 0x68D910 | 0x68DA69 | TASK_COMPLEX_ARREST_PED created without the exe's null test of m_Pursued |

### Infrastructure
- Fixture: fake point / node route pools (0xB744B0 / 0xB744B8; `PoolFx` got a size parameter), `CPointRoute*` / `CNodeRoute*` members point at valid pool routes (5..8 entries, kinds R / Q), route index members range-limited (kind x), constructor rows start from an all-zero object (`g_tzero`: the exe's bit-field / padding initialisation leaves bytes), byte masks for exe-uninitialised bytes (`g_tmask` in the object, `g_pmask` in every task pool slot: half-initialised CNodeAddress, padding after a bool, embedded SeekEntity vptr), CreateSubTask rows pass only the task types the port's switch handles (the default is NOTSA_UNREACHABLE), all TaskTypes TUs + PedGeometryAnalyser / PathFind / NodeRoute / PatrolRoute / scanners / FireManager / CarEnterExit linked (unresolved callees fault at 0x400000 = skipped cases, that was the main reason for "every case faults").
- Diagnostics: `W2_FAULTLOG=1` prints per faulting case where each side faulted (port eip / access / stack top, exe eip / access); `C:/gta-scratch/w3tasks2/fl.py` maps the port return address to the caller through the .map and groups by row.
- 71 functions skipped with a reason (w3_tasks_skip.tsv): ~38 "every case faults identically" now carry the exe fault eip and the accessed offset (mostly a null `ped->m_pVehicle`-like / entity member at +0x14, +0x47C, +0x590 that needs a per-class fixture), 31 "process killed" (exe SEH frame), CTaskComplexMoveBackAndJump::CreateSubTask (the exe takes one stack argument, `ret 4`, port is static: the generated exe call shape is wrong; also the hook is installed with a cdecl convention, harmless today), padding / assert artefacts as before.
- Possible follow-up (not a fix): the port's `default: NOTSA_UNREACHABLE()` in the CreateSubTask switches where the exe returns null.

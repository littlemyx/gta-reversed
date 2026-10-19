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

# ORACLE-C-TASKS report

Agent ORACLE-C-TASKS (rules v4), worktree C:/gta-wt/oracle-c-tasks, 2026-10-23 20:05 - 21:25 WEST. Class C (virtual calls) for game_sa/Tasks/TaskTypes, first wave.

| | before | after |
|---|---:|---:|
| TaskTypes covered | 1355 | **1430** |
| TaskTypes uncovered A / B / C | 24 / 61 / 143 | **23 / 39 / 91** |
| exe-oracle coverage, whole game | 4838 | **4913** |
| `wave3_tasks_oracle_test` | 83 rows | **159 rows**, PC24 **0**, PC53 8 (informational), exit 0, ~3 min |
| port bugs fixed | | 5 functions (7 defects) |

## Infrastructure
* `/SAFESEH:NO` on wave3_tasks_oracle_test (as ORACLE-C-VEH-2): all 31 "exe process killed" skips were this; 30 of them run now (1 still kills: CTaskComplexFollowPointRoute::Constructor).
* Class C: `W3TaskCase(..., vt)` writes the exe's vftable (RH_ScopedVirtualClass VA) into word 0 of the task `this` and of the ped under test (CPlayerPed 0x86D168) before the exe call, the port's (`??_7` through /alternatename) before the port call, clears both after; gen_w3_tasks.py `--classes A,B,C --prev <old gen file>` (the coverage tool counts earlier generated rows as covered; `--prev` regenerates them) emits `kTVt_<Class>` entries.
* 70 rows skipped with a reason in tests/standalone/tools/w3_tasks_skip.tsv (69 "every case faults identically": needs per-class fixtures with valid pointed-to objects).

## Port bugs fixed (one commit each, asm in the message; pre-fix = PC24 mismatching cases of 60)
| function | exe | bug | pre-fix |
|---|---|---|---|
| CTaskComplexRoadRage::CreateSubTask | 0x629080 (0x62909B, 0x6291A9, 0x6292C6) | no null test of m_rageWith; SEEK_ENTITY sought ped->m_pVehicle instead of the rage target's vehicle; DESTROY_CAR / SEEK_ENTITY without a target vehicle must fall back to CTaskComplexKillPedOnFoot | 11/60 |
| CTaskSimpleIKChain ctor / Clone (m_OffsetBone) | 0x633E38 `mov [esi+2Ch],edi`, 0x633F5F | offset bone argument never stored (always bone 0), member is 32 bit | 18/60 (IKLookAt::Clone, pool bytes +0x2C) |
| CTaskSimpleIKManager::Clone | 0x639350 (0x6393C4) | no null test of the empty IK slots: null dereference | 60/60 |
| CTaskComplexInvestigateDeadPed::CreateSubTask | 0x696F90 (0x69714C, 0x6970BD) | GO_TO_POINT_AND_STAND_STILL used the stale m_targetPos (exe recomputes it first); STAND_STILL lacked the IKChainManager::LookAt | 9/60 |
| CTaskComplexInvestigateDeadPed::ComputeTargetPos | 0x696EE0 (0x696F19) | exe zeroes z of the direction before Normalise (port: 3D normalise) | 4/60 (after the two above) |

Consumer audit: no layout / meaning change except IKChain m_OffsetBone 16 -> 32 bit (size 0x58 unchanged; readers: CTaskSimpleIKChain::CreateIKChain / Clone, writer IKLookAt::UpdateLookAtInfo, all pass it to IKChain_c as eBoneTag32). m_targetPos of InvestigateDeadPed is read by ControlSubTask / GoToPoint only.
Soak (Release build of the worktree, NOVSYNC, 3 min, new game): RESULT OK, 0 errors, 45700 frames (C:/gta-scratch/ctasks/soak_out). gameplay_regress not run (no script / streaming / population / path / police code touched).

## Open (skip lines carry the numbers; candidates for follow-ups)
* CReferences list consumption differs in 9/60 "special input" cases of CTaskSimpleIKLookAt::Clone, CTaskSimpleEvasiveDive::Clone and 52/60 of SeekCoverUntilTargetDead::CreateFirstSubTask (exe registers a reference, port does not): unanalysed, could be a RegisterReference / EntityRef difference.
* Port-only faults where the exe survives: CTaskComplexDie::SayDeathSample (null matrix in the Say / AddAudioEvent chain, 47/60), PassObject::CreateFirstSubTask (CEntity::GetType on a garbage pointer, 15/60), DuckToggle::ProcessPed (CTask::classof, 10/60); port assert/exception (0xE0000001) in BeInGroup x3, Sunbathe::Destructor, AffectSecondaryBehaviour::ProcessPed, GoPickUpEntity::Destructor.
* Created-task byte differences: FollowPedFootsteps::CreateFirstSubTask (+0x141), TrackEntity::CreateFirstSubTask / CreateNextSubTask (slot+8 = 0x32), WanderCop::CreateFirstSubTask (16 vs 32 bit store 0xFFFF).
* Lesson (again): a row that diverges in a "skip-looking" function pointed at real bugs - read the whole exe function, not only the diverging byte (InvestigateDeadPed had three independent defects).

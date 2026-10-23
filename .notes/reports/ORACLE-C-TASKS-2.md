# ORACLE-C-TASKS-2 report

Agent ORACLE-C-TASKS-2 (rules v4), worktree C:/gta-wt/oc2 (branch oc2), started 2026-10-23 21:32 WEST (end time in the hand-back). Follow-up of ORACLE-C-TASKS: the
`w3_tasks_skip.tsv` rows that pointed at real port bugs.

| | before | after |
|---|---:|---:|
| `wave3_tasks_oracle_test` rows | 159 | **175**, PC24 **0**, PC53 8 (informational, ComputeMoveDir as before) |
| skip rows un-skipped | | 16 (priorities 1-3 of the brief); FollowPointRoute::Constructor still skipped |
| port functions fixed, oracle-proven | | 7 |
| port functions fixed from the entity-reference audit (no oracle row possible) | | 6 |

## Finding: the "CReferences difference" of IKLookAt::Clone / EvasiveDive::Clone was a fixture artefact
`gen_w2_rows.py` did not recognise `notsa::EntityRef<>` / `CVehicle::Ref` members (the regex needed a type argument), so `m_TargetEntity` kept the garbage of the fixture and the
exe and the port registered / faulted differently depending on where the garbage pointed. With real reference-world entities both rows pass on the UNCHANGED port: not port bugs
(rows un-skipped). The same generator fix gives every other row with an EntityRef member real entities (UseGun::Reset then showed a real difference, below).

## Port fixes proven by an oracle row (one commit each, asm in the message; pre-fix = mismatching cases of 60, PC24)
| function | exe | bug | pre-fix |
|---|---|---|---:|
| CTaskComplexSeekCoverUntilTargetDead::CreateFirstSubTask | 0x62E6C0 (0x62E6DD..0x62E736) | exe: clean target, clean cover, store both raw, register target then cover ONLY when both exist; the port registered cover first, and a single present one | 52 |
| CTaskComplexPassObject::CreateFirstSubTask | 0x6613A0 (0x661403, 0x66147B, 0x66150D) | assert + unconditional RegisterReference without an entity; m_timer never started (exe: 1000 ms); created the position TurnToFace task instead of the entity one (0x66B890); x87 distance | 15 |
| CTaskComplexTrackEntity::CreateNextSubTask (+ CreateFirstSubTask via the vtable) | 0x65F590 (0x65F6C7), 0x65F700 | StandStill created with time 0, the exe passes 50 ms; range^2 in extended precision | 50 / 42 |
| CTaskSimpleDuckToggle::ProcessPed | 0x62F6E0 (0x62F759) | dyn_cast on a null simplest task (the exe tests it), ped read before the mode is tested | 10 |
| CAEPedSpeechAudioEntity::AddSayEvent (+ pedless I_AddSayEvent) | 0x4E6550 / 0x4E60D0 (0x4E6750) | entity position read on entry; the exe reads it at the camera distance test only (a ped without a matrix faulted; SayDeathSample row) | 47 |
| CTaskSimpleUseGun::Reset | 0x624DC0 (0x624E0E) | same target entity: the exe does nothing, the EntityRef assignment cleaned up + re-registered (CReferences order) | 1 |
| CPedGroupIntelligence::ReportFinishedTask | 0x5F76C0 (0x5F76DA) | task->GetTaskType() on entry; the exe asks lazily, only for a matching pair (BeInGroup::CreateNextSubTask row) | 60 |

Rows that only failed for fixture reasons and now pass on the unchanged port: IKLookAt::Clone, EvasiveDive::Clone (EntityRef garbage), BeInGroup::CreateFirstSubTask /
MonitorSecondaryGroupTask (garbage group id: the port's std::array range check; indices now drawn from 0..4, callee TUs PedGroupIntelligence / PedGroups / PedGroupMembership linked),
AffectSecondaryBehaviour::ProcessPed (garbage slot), Sunbathe::Destructor (the port's GetAnimBlocks() is `take(ms_numAnimBlocks)`, count preset), GoPickUpEntity::Destructor (anim group
index), FollowPedFootsteps::CreateFirstSubTask (embedded calculator vptr at +0x40, middle bytes differ: pool mask), WanderCop::CreateFirstSubTask (CNodeAddress node id:
CPathFind::FindNextNodeWandering copies an exe stack local whose node id is uninitialised, 0x451FD6 / 0x451FED, the port copies -1; only read when the area is valid; masked).

## Entity-reference consumer audit (read-only helper + own reading; table C:/gta-scratch/oc2/refaudit_report.md)
46 exe functions of Tasks/TaskTypes that register / clean up references and are not oracle-covered were compared with the port. Six real deviations fixed (no oracle row possible):
| function | exe | deviation |
|---|---|---|
| CTaskSimpleCarSetTempAction::ProcessPed | 0x645370 | cached `&m_pVehicle->m_autoPilot` before the null test and before CarDrive::ProcessPed can swap the vehicle |
| CTaskComplexFleeAnyMeans::ControlSubTask | 0x65D780 (0x65D8E4) | missing CCarEnterExit::IsVehicleStealable test (registered / tried to steal planes, helicopters) |
| CTaskComplexKillPedGroupOnFoot::CreateNextSubTask / CreateFirstSubTask | 0x629570 / 0x629630 | target cleaned up + nulled at entry; registered only when found; first sub task without a target is TASK_NONE (port: LOOK_ABOUT); unknown type returns null |
| CTaskComplexCarDrive::CreateNextSubTask | 0x644E20 | type 0x396 (GO_TO_POINT_ANY_MEANS) finishes; the port hit NOTSA_UNREACHABLE |
| CTaskComplexEnterCar m_DraggedPed (3 sites) | 0x63EE97, 0x63FB5B, 0x63FE69 | stored raw (also null), registered when set, old reference NOT cleaned up |

Identical to the exe (not port bugs): EnterCar / UseEffect destructors do not clean m_DraggedPed / m_Ped; CarDrive registers m_Veh without cleanup at two sites (the port cleans first = safer, left).

## Still skipped
CTaskComplexFollowPointRoute::Constructor 0x671510: the exe-side access violation kills the process on the garbage CPointRoute argument (needs a pool route as the argument; not done).
The "class-C fixture" rows of ORACLE-C-TASKS still need per-class fixtures.

## Consumer audit of the changes
* CTaskComplexKillPedGroupOnFoot::ComputeNewTargetAndTaskType gained a parameter; its only callers are the two fixed functions.
* CAEPedSpeechAudioEntity::I_AddSayEvent takes the position entity instead of a CVector; callers: AddSayEvent (ped) and CAEPedlessSpeechAudioEntity::AddSayEvent only.
* No other change alters a layout or the meaning of shared state.

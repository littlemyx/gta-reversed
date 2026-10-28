# ASM-S04b (leftovers of S04: FollowPointRoute / FollowNodeRoute / FollowPatrolRoute / PedFootsteps / Gang* / GetOnBoatSeat) - 2026-10-28
Result: 8 of 20 compared (6 MATCH, 2 FIXED in 2 commits), 12 NOT_REACHED (budget; the Gang* group minus the destructors/JoinRespond). Table: S04b.result.tsv.
Fixes (exe address, one line each):
- db1a8ff7 CTaskComplexFollowPointRoute::CreateTask (0x669090): GOTO_POINT_FINE while slowing down inherits the low 4 (target-circled) flag bits of the current SUB task when that is a simple go-to-point (port zeroed them). All other cases (LEAVE_CAR, STAND_STILL, NONE, GOTO_POINT, GOTO_POINT_AND_STAND_STILL) match; the S04 UNSURE is resolved.
- 5133e37e CTaskComplexFollowNodeRoute::ComputePathNodes (0x66EFA0): node route is cleared on entry (port kept stale nodes when the target was reachable straight); both first-node cut tests (+3.0 / +50.0) compare |target-ped| with |target-firstNode| (port: |firstNode-ped|); m_StartNode reset to {0xFFFF,0} (global 0x8A5F44); height test is NaN-taking; current-node search tests only the area id.
MATCH: FollowPatrolRoute::CreateFirstSubTask (0x670E20), PedFootsteps::ControlSubTask (0x695090, whole function), FollowPointRoute ctor (0x671510), GangFollower dtor, GangJoinRespond dtor + ControlSubTask, GetOnBoatSeat::CreateFirstSubTask (0x649310).
Open (NOT_REACHED): GangFollower CreateNext/CreateFirst/Control, GangLeader (8: dtor, GetRandomGangAmbientAnim [started], Abuse/Attack speech, CreateNext/First, Control, ScanForStuff).
Consumer audit: no signature or layout change.
Gate: see handback (Release + RW builds, wave2_oracle_test, gameplay_regress combat).

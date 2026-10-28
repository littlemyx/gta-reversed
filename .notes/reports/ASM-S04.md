# ASM-S04 (Tasks/TaskTypes: FleeShooting .. GetOnBoatSeat) - 2026-10-28
Result: 21 of 40 compared (14 MATCH, 6 FIXED in 6 commits, 1 UNSURE), 19 NOT_REACHED (budget). Table in S04.result.tsv. Task 0 (EnterCar) in the handback.
Fixes (exe address, one line each):
- c13b0177 CTaskComplexFleeShooting::ControlSubTask (0x65CA90): m_moveState=RUN stored when a secondary attack task exists; timers use the unguarded inline start.
- b9af7c17 CTaskComplexFollowLeaderInFormation::ControlSubTask (0x696940): leader-on-foot test uses (dz2+dy2)+dx2 and strict D2 > distSq (NaN returns); m_Dist test NaN-returning.
- 46c83e53 CTaskComplexFollowNodeRoute::StopTimer (0x6694E0): Pause (stopped flag + remaining time), the port cleared m_bStarted.
- dda88e46 CTaskComplexFollowNodeRoute::SetTarget (0x671750): with blending the speeding-up flag is SET (port cleared it).
- 54917d3c CTaskComplexFollowNodeRoute::CanGoStraightThere (0x66EF20): sqrt((dz2+dx2)+dy2) > maxDist (port: squared >=).
- 2d831779 CTaskComplexFollowPointRoute::ControlSubTask (0x66E860): m_bNewRoute -> CreateFirstSubTask was missing; speeding-up path CalcBlendRatio(ped,false) + apply ratio>=0; will-slow-down block sets slowing-down / clears speeding-up.
MATCH: FleeShooting (MakeAbortable, CreateNext/First), FollowLeaderAnyMeans (all 4), FollowLeaderInFormation (CreateSubTask/CreateNext/CreateFirst), FollowPatrolRoute (ctor, Clone, CreateSubTask, CreateNextSubTask).
UNSURE/open: FollowPointRoute::CreateTask (cases 0xC8/0xCB/0x2C0/0x384 match, 0x39E/0x387 not finished). NOT_REACHED: FollowNodeRoute::ComputePathNodes, FollowPatrolRoute::CreateFirstSubTask, PedFootsteps::ControlSubTask (first 40% read, matches), FollowPointRoute ctor, the Gang* group (11), GetOnBoatSeat.
Consumer audit: no layout/signature change; the timer/flag fixes are internal to the tasks.
Gate: Release + RW builds OK; wave2_oracle_test 1142 checks, 0 failures, 0 mismatches; gameplay_regress combat TOTAL PASS=14.

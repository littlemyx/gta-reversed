# ASM-P4S2: CEventHandler/CTheScripts oracle-only sweep (26 rows)

Checked 8 of 26 (budget): MATCH 3 (Flush, FlushImmediately [both asmsig symbol mismatches], ComputeSpecialResponse), FIXED 5, NOT_REACHED 18 (ComputePlayerCollisionWithPedResponse had its asm read but was not compared).

Fixes (all in source/game_sa/Events/EventHandler.cpp, one commit each):
- ComputePedThreatResponse f6ac304b1: FLEE_ENTITY -> CTaskComplexFleeEntity; SMART_FLEE_ENTITY_WALKING moveState WALK; STAND_STILL case; LEAVE_CAR guard; duck not handed out; Magnitude compare.
- ComputeGunAimedAtResponse be7608f23: existing duck leaves the response untouched; TASK_NONE.
- ComputeShotFiredResponse aeed9bd85: duck arms inverted; KILL_CRIMINAL flee walks.
- ComputeVehicleDamageResponse 3a47a3319: LEAVE_CAR_AND_FLEE flee point is the vehicle position.
- ComputeDeadPedResponse 1d4a2cfc3: exe proximity constant 0x85B9F4.

Note: ComputeEventResponseTask nulls m_EventResponseTask before dispatch, so the "leave untouched" exits of the exe are equivalent to nullptr; they only matter where the port returned a live/non-null task.

Verification: StandaloneRelease builds (ninja). StandaloneRW / run_all_tests not run (no StandaloneRW build in the fresh worktree, needs conan Debug): left to the gate. gameplay_regress combat missions: PASS=25, no FAIL.

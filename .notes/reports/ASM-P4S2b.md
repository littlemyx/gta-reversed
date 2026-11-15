# ASM-P4S2b: CEventHandler/CTheScripts oracle-only sweep, continuation (18 rows)

Checked 8 of 18 (budget): MATCH 6 (ComputePotentialPedCollideResponse, ComputePotentialWalkIntoFireResponse, ComputeSeenCopResponse, ComputeLow/HighAngerAtPlayerResponse, ComputeObjectCollisionResponse), FIXED 2, NOT_REACHED 10 (ComputeEventResponseTask-0, ComputePedFriendResponse, ComputePedToChaseResponse, ComputeSeenPanickedPedResponse, ComputeGotKnockedOverByCarResponse, 5 CTheScripts rows).

Fixes (source/game_sa/Events/EventHandler.cpp):
- ComputePlayerCollisionWithPedResponse b9cfc364f: the player always looks at the victim (the port made the victim look at the player in one arm); walk/walk arm: in front = look+say, behind-only = look+say+annoy, else nothing; STILL arm also for NONE, and it annoys; front-not-behind arm does not annoy, neither-front-nor-behind does nothing; victim WALK reaches the bump arm; sprint-into-still look time 1000; z,y,x dot and >=0 / <=0 tests.
- SetEventResponseTask db268f753: facial SetRequest durationB = 0.

Notes: the safe-distance globals 0xC18CE8/0xC18CEC are 60.0 (static init at 0x852690/0x8526A0), so the port 60.f is right.
Verification: StandaloneRelease builds. StandaloneRW / wave2_oracle_test not run (no StandaloneRW build in the fresh worktree), gameplay_regress left to the gate (GPU held).

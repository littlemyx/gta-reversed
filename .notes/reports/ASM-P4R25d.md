# ASM-P4R25d

Review of the three weapon fixes against the 1.0 exe: 52e51c6c8 (FireM16_1stPerson + FindNearestTargetEntityWithScreenCoors hunk), fc4dc6ef6 (CShotInfo::Initialise), 5d952a7b7 (FireProjectile HS crosshair) - all CONFIRMED, no review fix needed.

Step 2: 8 of the 14 NOT_REACHED rows compared, all MATCH (TryToMerge_WeaponType, FindNodePairClosestToCoors, ComputeRoute, AddInteriorLink, ReturnInteriorNodeIndex (unused, phantom param), SwitchRoadsOffInArea, SwitchRoadsOffInAreaForOneRegion, AddDynamicLinkBetween2Nodes_For1Node). No source changes, so no build/test run.

Still NOT_REACHED (budget): SteerAICarWithPhysicsFollowPath_Racing, CompleteNewInterior, CPathFind::Init, SteerAICarBlockingPlayerForwardAndBack, TestCollisionBetween2MovingRects (~200 asm lines), CWeapon::Update (jump-table function, 4 states).
Details: .notes/asm_sweep/P4R25d.result.tsv

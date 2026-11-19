# ASM-P4R25b: traffic + weapon rows with exe-address comments but no verdict

Compared 6 functions against the 1.0 exe: 5 MATCH, 1 FIXED (CWeapon::FireProjectile: the heat seeking lock-on crosshair writes only r,g,b, the port also wrote alpha 255).
MATCH: AddGunshell, FireFromCar, Fire (all three jump tables decoded), SteerAIBoatWithPhysicsHeadingForTarget, SteerAIBoatWithPhysicsCirclingPlayer.
NOT_REACHED (budget): FireInstantHit, SteerAICarWithPhysicsFollowPath_Racing, FindAngleToWeaveThroughTraffic, TakePhotograph, CompleteNewInterior, SteerAICarTowardsPointInEscort, SetUpPelletCol, FindNodePairClosestToCoors, SteerAIHeliToKeepEntityInView, CarEnterExit::*, other cand.tsv rows in the zone.
Build: StandaloneRelease ninja -j4 (see result in the final report).

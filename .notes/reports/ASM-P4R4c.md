# ASM-P4R4c
- All ~33 S22 NOT_REACHED CarCtrl rows already carry MATCH/FIXED in later TSVs (S22b-h, P4R4, P4R4b; ReconsiderRoute full MATCH in S22b, GenerateCarCreationCoors2 in P4R2g). Nothing left there.
- Walked in full vs exe: FindMaximumSpeedForThisCarInTraffic, WeaveForObject, FindNodesThisCarIsNearestTo, ClitargetOrientationToLink: all MATCH, 0 fixes (x87 rounding points, branch polarity, lane/dir fields all agree).
- Open (P4T1 REVIEW, zone, not walked): FindAngleToWeaveThroughTraffic, SteerAIHeliToKeepEntityInView, ChooseModel, SteerAICarPark{Perpendicular,Parallel}, SteerAICarWithPhysicsFollowPath(+_Racing), WeaveThroughCarsSectorList, ProcessPlayerWeapon, LookForCarAlarms, ~60 small Task* CreateSubTask/Constructor rows.
- No source changes, no build. Commits: result + report only.

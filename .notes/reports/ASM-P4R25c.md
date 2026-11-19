# ASM-P4R25c: traffic + weapon bodies with exe-address comments but no verdict

Source list: cand.tsv (zone filter: CarCtrl/PathFind/Weapon/...), minus every addr already in a result TSV, plus the AUDITED-IN-SOURCE rows of P4R7g (CCarCtrl) and CWeapon::Update.
Compared instruction by instruction against gta_sa_compact.exe: 7 functions, all MATCH (no source change, no build needed).

- CCarCtrl::WeaveThroughCarsSectorList, TestCollisionBetween2MovingRects_OnlyFrontBumper, SteerAIHeliToKeepEntityInView
- CPathFind::CalcRoadDensity, FindNodeOrientationForCarPlacement, FindParkingNodeInArea, SetLinksBridgeLights

Not reached (budget): SteerAICarWithPhysicsFollowPath_Racing (0xD10, largest), FindNodePairClosestToCoors, CompleteNewInterior, SwitchRoadsOffInArea(+OneRegion), AddDynamicLinkBetween2Nodes_For1Node, CPickups::TryToMerge_WeaponType, ComputeRoute, Init, AddInteriorLink, ReturnInteriorNodeIndex, SteerAICarBlockingPlayerForwardAndBack, TestCollisionBetween2MovingRects, CWeapon::Update. See P4R25c.result.tsv.

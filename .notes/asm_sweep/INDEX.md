# ASM sweep slices (uncovered B/C/S/D functions, behaviour subsystems first)

| slice | subsystem | functions | status | files |
|---|---|---|---|---|
| S01 | game_sa/Tasks/TaskTypes | 40 | partial 22/40 (11 M, 11 fixed) -> S01b | TaskInteriorBeInHouse.cpp, TaskInteriorBeInOffice.cpp, TaskInteriorBeInShop.cpp, TaskInteriorLieInBed.cpp, TaskInteriorShopKeeper.cpp, TaskInteriorSitAtDesk.cpp ... |
| S02 | game_sa/Tasks/TaskTypes | 40 | pending | TaskComplexAvoidEntity.cpp, TaskComplexAvoidOtherPedWhileWandering.cpp, TaskComplexBeInCouple.cpp, TaskComplexBeInGroup.cpp, TaskComplexBeStill.cpp, TaskComplexCarDrive.cpp ... |
| S03 | game_sa/Tasks/TaskTypes | 43 | pending | TaskComplexDragPedFromCar.cpp, TaskComplexDriveFireTruck.cpp, TaskComplexDriveToPoint.cpp, TaskComplexDriveWander.cpp, TaskComplexEnterBoatAsDriver.cpp, TaskComplexEnterCar.cpp ... |
| S04 | game_sa/Tasks/TaskTypes | 40 | pending | TaskComplexFleeShooting.cpp, TaskComplexFollowLeaderAnyMeans.cpp, TaskComplexFollowLeaderInFormation.cpp, TaskComplexFollowNodeRoute.cpp, TaskComplexFollowPatrolRoute.cpp, TaskComplexFollowPedFootsteps.cpp ... |
| S05 | game_sa/Tasks/TaskTypes | 42 | pending | TaskComplexGoPickUpEntity.cpp, TaskComplexGoToAttractor.cpp, TaskComplexGoToBoatSteeringWheel.cpp, TaskComplexGoToCarDoorAndStandStill.cpp, TaskComplexGoToPointAiming.cpp, TaskComplexGoToPointAndStandStillTimed.cpp ... |
| S06 | game_sa/Tasks/TaskTypes | 43 | pending | TaskComplexKillPedOnFootMelee.cpp, TaskComplexKillPedOnFootStealth.cpp, TaskComplexLeaveBoat.cpp, TaskComplexLeaveCar.cpp, TaskComplexLeaveCarAndDie.cpp, TaskComplexLeaveCarAndFlee.cpp ... |
| S07 | game_sa/Tasks/TaskTypes | 41 | pending | TaskComplexPassObject.cpp, TaskComplexPlayHandSignalAnim.cpp, TaskComplexPolicePursuit.cpp, TaskComplexPresentIdToCop.cpp, TaskComplexProstituteSolicit.cpp, TaskComplexReactToGunAimedAt.cpp ... |
| S08 | game_sa/Tasks/TaskTypes | 41 | pending | TaskComplexStareAtPed.cpp, TaskComplexStealCar.cpp, TaskComplexStuckInAir.cpp, TaskComplexSunbathe.cpp, TaskComplexTrackEntity.cpp, TaskComplexTreatAccident.cpp ... |
| S09 | game_sa/Tasks/TaskTypes | 40 | pending | TaskComplexWaitForBackup.cpp, TaskComplexWaitForPed.cpp, TaskComplexWalkAlongsidePed.cpp, TaskComplexWalkRoundBuildingAttempt.cpp, TaskComplexWalkRoundCar.cpp, TaskComplexWalkRoundObject.cpp ... |
| S10 | game_sa/Tasks/TaskTypes | 41 | pending | TaskLeanOnVehicle.cpp, TaskSimpleAnim.cpp, TaskSimpleArrestPed.cpp, TaskSimpleBeHit.cpp, TaskSimpleBeHitWhileMoving.cpp, TaskSimpleBeKickedOnGround.cpp ... |
| S11 | game_sa/Tasks/TaskTypes | 48 | partial 39/48 (30 M, 9 fixed) -> S11b | TaskSimpleCarGetOut.cpp, TaskSimpleCarGoToPointNearDoorUntilDoorNotInUse.cpp, TaskSimpleCarJumpOut.cpp, TaskSimpleCarOpenDoorFromOutside.cpp, TaskSimpleCarOpenLockedDoorFromOutside.cpp, TaskSimpleCarSetPedInAsDriver.cpp ... |
| S12 | game_sa/Tasks/TaskTypes | 46 | partial 36/46 (29 M, 8 fixed) -> S12b | TaskSimpleCreateCarAndGetIn.cpp, TaskSimpleDead.cpp, TaskSimpleDie.cpp, TaskSimpleDieInCar.cpp, TaskSimpleDoHandSignal.cpp, TaskSimpleDrown.cpp ... |
| S13 | game_sa/Tasks/TaskTypes | 41 | partial 29/41 (25 M, 3 fixed, 1 U) -> S13b | TaskSimpleGetUp.cpp, TaskSimpleGiveCPR.cpp, TaskSimpleGoTo.cpp, TaskSimpleGoToPoint.cpp, TaskSimpleGoToPointFine.cpp, TaskSimpleGunControl.cpp ... |
| S14 | game_sa/Tasks/TaskTypes | 41 | partial 18/41 (14 M, 4 fixed) -> S14b | TaskSimpleJump.cpp, TaskSimpleKillPedWithCar.cpp, TaskSimpleLand.cpp, TaskSimpleOnEscalator.cpp, TaskSimplePickUpBike.cpp, TaskSimplePickUpEntity.cpp ... |
| S15 | game_sa/Tasks/TaskTypes | 33 | pending | TaskSimpleSlideToCoord.cpp, TaskSimpleStandStill.cpp, TaskSimpleStandUp.cpp, TaskSimpleStealthKill.cpp, TaskSimpleSwim.cpp, TaskSimpleThrowProjectile.cpp ... |
| S16 | game_sa/Entity/Vehicle | 48 | partial 23/48 (18 M, 5 fixed) -> S16b | Automobile.cpp |
| S17 | game_sa/Entity/Vehicle | 43 | partial 21/43 (17 M, 4 fixed) -> S17b | Bike.cpp, Bmx.cpp, Boat.cpp |
| S18 | game_sa/Entity/Vehicle | 41 | partial 17/41 (13 M, 4 fixed) -> S18b | Heli.cpp, MonsterTruck.cpp, Plane.cpp, QuadBike.cpp |
| S19 | game_sa/Entity/Vehicle | 108 | pending | Trailer.cpp, Train.cpp, Vehicle.cpp |
| S20 | game_sa/Entity/Ped | 75 | partial 54/75 (42 M, 12 fixed) -> S20b | CivilianPed.cpp, CopPed.cpp, EmergencyPed.cpp, Ped.cpp |
| S21 | game_sa/Entity/Ped | 32 | partial 23/32 (20 M, 3 fixed) -> S21b | PedSaveStructure.cpp, PlayerPed.cpp, PlayerSkin.cpp |
| S22 | game_sa/CarCtrl.cpp | 55 | partial 21/55 (17 M, 4 fixed) -> S22b | CarCtrl.cpp |
| S23 | game_sa/Population.cpp | 33 | partial 20/33 (11 M, 9 fixed) -> S23b | Population.cpp |
| S24 | game_sa/PedIntelligence.cpp | 31 | done (28 M, 3 fixed) | PedIntelligence.cpp |
| S25 | game_sa/Events/GroupEventHandler.cpp | 30 | done (18 M, 12 fixed) | GroupEventHandler.cpp |
| S26 | game_sa/Entity/Physical.cpp | 28 | partial 19/28 (15 M, 4 fixed) -> S26b | Physical.cpp |
| S27 | game_sa/World.cpp | 44 | partial 29/44 (19 M, 10 fixed) -> S27b | World.cpp |
| S28 | game_sa/Streaming.cpp | 73 | pending | Streaming.cpp |
| S29 | game_sa/Entity/Object | 39 | partial 34/39 (24 M, 10 fixed) -> S29b | CutsceneObject.cpp, HandObject.cpp, Object.cpp |
| S30 | game_sa/Camera.cpp | 35 | pending | Camera.cpp |
| S31 | game_sa/Cam.cpp | 34 | pending | Cam.cpp |
| S32 | game_sa/Radar.cpp | 28 | pending | Radar.cpp |
| S33 | game_sa/WaterLevel.cpp | 26 | pending | WaterLevel.cpp |
| S34 | game_sa/Interior/Interior_c.cpp | 41 | pending | Interior_c.cpp |
| S22b | game_sa/CarCtrl.cpp (rest of S22) | 34 | partial 16/34 (16 M) -> S22c | CarCtrl.cpp |
| S23b | rest of S23 | 13 | done 12/13 (9 M, 3 fixed; AddPedAtAttractor left) | Population.cpp |
| S22c | rest of S22b | 18 | partial 3/18 (3 M) -> S22d | CarCtrl.cpp |
| S26b | rest of S26 | 9 | partial 7/9 (7 fixed) -> S26c | Physical.cpp |
| S26c | rest of S26b | 2 | done (2 fixed) | Physical.cpp |
| S22d | rest of S22c | 15 | partial 6/15 (5 M, 1 fixed) -> S22e | CarCtrl.cpp |
| S20b | rest of S20 | 21 | partial 8/21 (6 M, 2 fixed) -> S20c | Ped.cpp |
| S16b | rest of S16 | 25 | partial 8/25 (2 M, 5 fixed, 1 U) -> S16c | Automobile.cpp |
| S17b | rest of S17 | 22 | partial 5/22 (2 M, 3 fixed) -> S17c | Bike.cpp, Bmx.cpp, Boat.cpp |
| S21b | rest of S21 | 9 | partial 5/9 (4 M, 1 fixed) -> S21c | PlayerPed.cpp |
| S27b | rest of S27 | 15 | partial 10/15 (6 M, 4 fixed +1 FindGroundZ) -> S27c | World.cpp |
| S29b | rest of S29 | 5 | done (1 M, 4 fixed) | HandObject.cpp, Object.cpp |
| S13b | rest of S13 | 12 | partial 8/12 (5 M, 3 fixed) -> S13c | TaskSimpleGetUp.cpp, TaskSimpleGoToPoint.cpp, TaskSimpleGoToPointFine.cpp, TaskSimpleGunControl.cpp, TaskSimpleHoldEntity.cpp, TaskSimpleInAir.cpp, TaskSimpleJetPack.cpp |
| S11b | rest of S11 | 9 | pending | TaskSimpleCarSetPedInAsDriver.cpp, TaskSimpleCarSetPedInAsPassenger.cpp, TaskSimpleCarSetPedSlowDraggedOut.cpp, TaskSimpleClimb.cpp |
| S12b | rest of S12 | 10 | pending | TaskSimpleDead.cpp, TaskSimpleFight.cpp, TaskSimpleGangDriveBy.cpp |
| S20c | rest of S20b | 13 | partial 3/13 (2 M, 1 fixed) -> S20d | Ped.cpp |
| S16c | rest of S16b | 17 | partial 2/17 (2 fixed) -> S16d | Automobile.cpp |
| S17c | rest of S17b | 16 | partial 6/16 (2 M, 4 fixed) -> S17d | Bike.cpp, Bmx.cpp, Boat.cpp |
| S21c | rest of S21b | 4 | pending | PlayerPed.cpp |
| S13c | rest of S13b | 4 | done (1 M, 3 fixed) | TaskSimpleGunControl.cpp, TaskSimpleInAir.cpp, TaskSimpleJetPack.cpp |
| S22e | rest of S22d | 9 | pending | CarCtrl.cpp |
| S27c | rest of S27b | 5 | pending | World.cpp |
| S14b | rest of S14 | 23 | partial 17/23 (9 M, 8 fixed) -> S14c | TaskSimpleJump.cpp, TaskSimplePickUpBike.cpp, TaskSimplePlayerOnFire.cpp, TaskSimplePlayerOnFoot.cpp, TaskSimpleSay.cpp, TaskSimpleSetPedAsAutoDriver.cpp, TaskSimpleShakeFist.cpp, TaskSimpleSitDown.cpp, TaskSimpleSitIdle.cpp |
| S18b | rest of S18 | 24 | partial 13/24 (12 M, 1 fixed) -> S18c | Heli.cpp, Plane.cpp |
| S18c | rest of S18b | 11 | pending | Heli.cpp, Plane.cpp |
| S20d | rest of S20c | 10 | partial 6/10 (3 M, 3 fixed) -> S20e | Ped.cpp |
| S20e | rest of S20d | 4 | partial 2/4 (1 M, 1 fixed) -> S20f | Ped.cpp |
| S20f | rest of S20e | 2 | done (1 M, 1 fixed, ctor audio subobjects unread) | Ped.cpp |
| S16d | rest of S16c | 15 | partial 3/15 (3 fixed) -> S16e | Automobile.cpp |
| S16e | rest of S16d | 12 | partial 2/12 (2 fixed) -> S16f | Automobile.cpp |
| S16f | rest of S16e | 10 | partial 4/10 (4 fixed) -> S16g | Automobile.cpp |
| S16g | rest of S16f | 6 | partial 3/6 (1 M, 2 fixed) -> S16h | Automobile.cpp |
| S16h | rest of S16g | 3 | partial 2/3 + PreRender to 0x6AB154 (2 fixed) -> S16i | Automobile.cpp |
| S16i | PreRender from 0x6AB154 to ret (rest of S16h) | 1 | done 1/1 (MATCH; S16 complete) | Automobile.cpp |
| S17d | rest of S17c | 10 | partial 5/10 (5 M) -> S17e | Bike.cpp, Bmx.cpp, Boat.cpp |
| S17e | rest of S17d | 5 | partial 4/5 (3 M, 1 fixed; Bike ProcessControl to 0x6B97C9) -> S17f | Bike.cpp, Boat.cpp |
| S17f | rest of S17e | 1 | done (MATCH, Bike ProcessControl complete) | Bike.cpp |
| S01b | rest of S01 | 18 | done (10 M, 8 fixed) | TaskComplexArrestPed.cpp, TaskComplexSeekEntityRadiusAngleOffset.cpp, TaskComplexSeekEntityStandard.cpp |
| S14c | rest of S14b | 6 | done (5 M, 1 fixed) | TaskSimplePlayerOnFoot.cpp |

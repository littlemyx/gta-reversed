# ASM sweep slices (uncovered B/C/S/D functions, behaviour subsystems first)

| slice | subsystem | functions | status | files |
|---|---|---|---|---|
| S01 | game_sa/Tasks/TaskTypes | 40 | pending | TaskInteriorBeInHouse.cpp, TaskInteriorBeInOffice.cpp, TaskInteriorBeInShop.cpp, TaskInteriorLieInBed.cpp, TaskInteriorShopKeeper.cpp, TaskInteriorSitAtDesk.cpp ... |
| S02 | game_sa/Tasks/TaskTypes | 40 | pending | TaskComplexAvoidEntity.cpp, TaskComplexAvoidOtherPedWhileWandering.cpp, TaskComplexBeInCouple.cpp, TaskComplexBeInGroup.cpp, TaskComplexBeStill.cpp, TaskComplexCarDrive.cpp ... |
| S03 | game_sa/Tasks/TaskTypes | 43 | pending | TaskComplexDragPedFromCar.cpp, TaskComplexDriveFireTruck.cpp, TaskComplexDriveToPoint.cpp, TaskComplexDriveWander.cpp, TaskComplexEnterBoatAsDriver.cpp, TaskComplexEnterCar.cpp ... |
| S04 | game_sa/Tasks/TaskTypes | 40 | pending | TaskComplexFleeShooting.cpp, TaskComplexFollowLeaderAnyMeans.cpp, TaskComplexFollowLeaderInFormation.cpp, TaskComplexFollowNodeRoute.cpp, TaskComplexFollowPatrolRoute.cpp, TaskComplexFollowPedFootsteps.cpp ... |
| S05 | game_sa/Tasks/TaskTypes | 42 | pending | TaskComplexGoPickUpEntity.cpp, TaskComplexGoToAttractor.cpp, TaskComplexGoToBoatSteeringWheel.cpp, TaskComplexGoToCarDoorAndStandStill.cpp, TaskComplexGoToPointAiming.cpp, TaskComplexGoToPointAndStandStillTimed.cpp ... |
| S06 | game_sa/Tasks/TaskTypes | 43 | pending | TaskComplexKillPedOnFootMelee.cpp, TaskComplexKillPedOnFootStealth.cpp, TaskComplexLeaveBoat.cpp, TaskComplexLeaveCar.cpp, TaskComplexLeaveCarAndDie.cpp, TaskComplexLeaveCarAndFlee.cpp ... |
| S07 | game_sa/Tasks/TaskTypes | 41 | pending | TaskComplexPassObject.cpp, TaskComplexPlayHandSignalAnim.cpp, TaskComplexPolicePursuit.cpp, TaskComplexPresentIdToCop.cpp, TaskComplexProstituteSolicit.cpp, TaskComplexReactToGunAimedAt.cpp ... |
| S08 | game_sa/Tasks/TaskTypes | 41 | pending | TaskComplexStareAtPed.cpp, TaskComplexStealCar.cpp, TaskComplexStuckInAir.cpp, TaskComplexSunbathe.cpp, TaskComplexTrackEntity.cpp, TaskComplexTreatAccident.cpp ... |
| S09 | game_sa/Tasks/TaskTypes | 40 | pending | TaskComplexWaitForBackup.cpp, TaskComplexWaitForPed.cpp, TaskComplexWalkAlongsidePed.cpp, TaskComplexWalkRoundBuildingAttempt.cpp, TaskComplexWalkRoundCar.cpp, TaskComplexWalkRoundObject.cpp ... |
| S10 | game_sa/Tasks/TaskTypes | 41 | pending | TaskLeanOnVehicle.cpp, TaskSimpleAnim.cpp, TaskSimpleArrestPed.cpp, TaskSimpleBeHit.cpp, TaskSimpleBeHitWhileMoving.cpp, TaskSimpleBeKickedOnGround.cpp ... |
| S11 | game_sa/Tasks/TaskTypes | 48 | pending | TaskSimpleCarGetOut.cpp, TaskSimpleCarGoToPointNearDoorUntilDoorNotInUse.cpp, TaskSimpleCarJumpOut.cpp, TaskSimpleCarOpenDoorFromOutside.cpp, TaskSimpleCarOpenLockedDoorFromOutside.cpp, TaskSimpleCarSetPedInAsDriver.cpp ... |
| S12 | game_sa/Tasks/TaskTypes | 46 | pending | TaskSimpleCreateCarAndGetIn.cpp, TaskSimpleDead.cpp, TaskSimpleDie.cpp, TaskSimpleDieInCar.cpp, TaskSimpleDoHandSignal.cpp, TaskSimpleDrown.cpp ... |
| S13 | game_sa/Tasks/TaskTypes | 41 | pending | TaskSimpleGetUp.cpp, TaskSimpleGiveCPR.cpp, TaskSimpleGoTo.cpp, TaskSimpleGoToPoint.cpp, TaskSimpleGoToPointFine.cpp, TaskSimpleGunControl.cpp ... |
| S14 | game_sa/Tasks/TaskTypes | 41 | pending | TaskSimpleJump.cpp, TaskSimpleKillPedWithCar.cpp, TaskSimpleLand.cpp, TaskSimpleOnEscalator.cpp, TaskSimplePickUpBike.cpp, TaskSimplePickUpEntity.cpp ... |
| S15 | game_sa/Tasks/TaskTypes | 33 | pending | TaskSimpleSlideToCoord.cpp, TaskSimpleStandStill.cpp, TaskSimpleStandUp.cpp, TaskSimpleStealthKill.cpp, TaskSimpleSwim.cpp, TaskSimpleThrowProjectile.cpp ... |
| S16 | game_sa/Entity/Vehicle | 48 | pending | Automobile.cpp |
| S17 | game_sa/Entity/Vehicle | 43 | pending | Bike.cpp, Bmx.cpp, Boat.cpp |
| S18 | game_sa/Entity/Vehicle | 41 | pending | Heli.cpp, MonsterTruck.cpp, Plane.cpp, QuadBike.cpp |
| S19 | game_sa/Entity/Vehicle | 108 | pending | Trailer.cpp, Train.cpp, Vehicle.cpp |
| S20 | game_sa/Entity/Ped | 75 | pending | CivilianPed.cpp, CopPed.cpp, EmergencyPed.cpp, Ped.cpp |
| S21 | game_sa/Entity/Ped | 32 | pending | PedSaveStructure.cpp, PlayerPed.cpp, PlayerSkin.cpp |
| S22 | game_sa/CarCtrl.cpp | 55 | partial 21/55 (17 M, 4 fixed) -> S22b | CarCtrl.cpp |
| S23 | game_sa/Population.cpp | 33 | partial 20/33 (11 M, 9 fixed) -> S23b | Population.cpp |
| S24 | game_sa/PedIntelligence.cpp | 31 | pending | PedIntelligence.cpp |
| S25 | game_sa/Events/GroupEventHandler.cpp | 30 | pending | GroupEventHandler.cpp |
| S26 | game_sa/Entity/Physical.cpp | 28 | pending | Physical.cpp |
| S27 | game_sa/World.cpp | 44 | pending | World.cpp |
| S28 | game_sa/Streaming.cpp | 73 | pending | Streaming.cpp |
| S29 | game_sa/Entity/Object | 39 | pending | CutsceneObject.cpp, HandObject.cpp, Object.cpp |
| S30 | game_sa/Camera.cpp | 35 | pending | Camera.cpp |
| S31 | game_sa/Cam.cpp | 34 | pending | Cam.cpp |
| S32 | game_sa/Radar.cpp | 28 | pending | Radar.cpp |
| S33 | game_sa/WaterLevel.cpp | 26 | pending | WaterLevel.cpp |
| S34 | game_sa/Interior/Interior_c.cpp | 41 | pending | Interior_c.cpp |
| S22b | game_sa/CarCtrl.cpp (rest of S22) | 34 | partial 16/34 (16 M) -> S22c | CarCtrl.cpp |
| S23b | rest of S23 | 13 | pending | Population.cpp |
| S22c | rest of S22b | 18 | pending | CarCtrl.cpp |

# ASM sweep slices (uncovered B/C/S/D functions, behaviour subsystems first)

| slice | subsystem | functions | status | files |
|---|---|---|---|---|
| S01 | game_sa/Tasks/TaskTypes | 40 | partial 22/40 (11 M, 11 fixed) -> S01b | TaskInteriorBeInHouse.cpp, TaskInteriorBeInOffice.cpp, TaskInteriorBeInShop.cpp, TaskInteriorLieInBed.cpp, TaskInteriorShopKeeper.cpp, TaskInteriorSitAtDesk.cpp ... |
| S02 | game_sa/Tasks/TaskTypes | 40 | partial 30/40 (23 M, 7 fixed) -> S02b | TaskComplexAvoidEntity.cpp, TaskComplexAvoidOtherPedWhileWandering.cpp, TaskComplexBeInCouple.cpp, TaskComplexBeInGroup.cpp, TaskComplexBeStill.cpp, TaskComplexCarDrive.cpp ... |
| S03 | game_sa/Tasks/TaskTypes | 43 | partial 30/43 (21 M, 9 fixed) -> S03b | TaskComplexDragPedFromCar.cpp, TaskComplexDriveFireTruck.cpp, TaskComplexDriveToPoint.cpp, TaskComplexDriveWander.cpp, TaskComplexEnterBoatAsDriver.cpp, TaskComplexEnterCar.cpp ... |
| S04 | game_sa/Tasks/TaskTypes | 40 | partial 21/40 (14 M, 6 fixed, 1 U) -> S04b | TaskComplexFleeShooting.cpp, TaskComplexFollowLeaderAnyMeans.cpp, TaskComplexFollowLeaderInFormation.cpp, TaskComplexFollowNodeRoute.cpp, TaskComplexFollowPatrolRoute.cpp, TaskComplexFollowPedFootsteps.cpp ... |
| S05 | game_sa/Tasks/TaskTypes | 42 | partial 8/42 (8 M, 2 of them tail-open) -> S05b | TaskComplexGoPickUpEntity.cpp, TaskComplexGoToAttractor.cpp, TaskComplexGoToBoatSteeringWheel.cpp, TaskComplexGoToCarDoorAndStandStill.cpp, TaskComplexGoToPointAiming.cpp, TaskComplexGoToPointAndStandStillTimed.cpp ... |
| S06 | game_sa/Tasks/TaskTypes | 43 | partial 9/43 (4 M, 5 fixed) -> S06b | TaskComplexKillPedOnFootMelee.cpp, TaskComplexKillPedOnFootStealth.cpp, TaskComplexLeaveBoat.cpp, TaskComplexLeaveCar.cpp, TaskComplexLeaveCarAndDie.cpp, TaskComplexLeaveCarAndFlee.cpp ... |
| S07 | game_sa/Tasks/TaskTypes | 41 | partial 11/41 (10 M, 1 fixed) -> S07b | TaskComplexPassObject.cpp, TaskComplexPlayHandSignalAnim.cpp, TaskComplexPolicePursuit.cpp, TaskComplexPresentIdToCop.cpp, TaskComplexProstituteSolicit.cpp, TaskComplexReactToGunAimedAt.cpp ... |
| S08 | game_sa/Tasks/TaskTypes | 41 | partial 33/41 + 2 extras (21 M, 14 fixed) -> S08b | TaskComplexStareAtPed.cpp, TaskComplexStealCar.cpp, TaskComplexStuckInAir.cpp, TaskComplexSunbathe.cpp, TaskComplexTrackEntity.cpp, TaskComplexTreatAccident.cpp ... |
| S09 | game_sa/Tasks/TaskTypes | 40 | done 40/40 (31 M, 9 fixed) | TaskComplexWaitForBackup.cpp, TaskComplexWaitForPed.cpp, TaskComplexWalkAlongsidePed.cpp, TaskComplexWalkRoundBuildingAttempt.cpp, TaskComplexWalkRoundCar.cpp, TaskComplexWalkRoundObject.cpp ... |
| S10 | game_sa/Tasks/TaskTypes | 41 | partial 16/41 (14 M, 2 fixed) -> S10b | TaskLeanOnVehicle.cpp, TaskSimpleAnim.cpp, TaskSimpleArrestPed.cpp, TaskSimpleBeHit.cpp, TaskSimpleBeHitWhileMoving.cpp, TaskSimpleBeKickedOnGround.cpp ... |
| S11 | game_sa/Tasks/TaskTypes | 48 | partial 39/48 (30 M, 9 fixed) -> S11b | TaskSimpleCarGetOut.cpp, TaskSimpleCarGoToPointNearDoorUntilDoorNotInUse.cpp, TaskSimpleCarJumpOut.cpp, TaskSimpleCarOpenDoorFromOutside.cpp, TaskSimpleCarOpenLockedDoorFromOutside.cpp, TaskSimpleCarSetPedInAsDriver.cpp ... |
| S12 | game_sa/Tasks/TaskTypes | 46 | partial 36/46 (29 M, 8 fixed) -> S12b | TaskSimpleCreateCarAndGetIn.cpp, TaskSimpleDead.cpp, TaskSimpleDie.cpp, TaskSimpleDieInCar.cpp, TaskSimpleDoHandSignal.cpp, TaskSimpleDrown.cpp ... |
| S13 | game_sa/Tasks/TaskTypes | 41 | partial 29/41 (25 M, 3 fixed, 1 U) -> S13b | TaskSimpleGetUp.cpp, TaskSimpleGiveCPR.cpp, TaskSimpleGoTo.cpp, TaskSimpleGoToPoint.cpp, TaskSimpleGoToPointFine.cpp, TaskSimpleGunControl.cpp ... |
| S14 | game_sa/Tasks/TaskTypes | 41 | partial 18/41 (14 M, 4 fixed) -> S14b | TaskSimpleJump.cpp, TaskSimpleKillPedWithCar.cpp, TaskSimpleLand.cpp, TaskSimpleOnEscalator.cpp, TaskSimplePickUpBike.cpp, TaskSimplePickUpEntity.cpp ... |
| S15 | game_sa/Tasks/TaskTypes | 33 | partial 13/33 (8 M, 5 fixed) -> S15b | TaskSimpleSlideToCoord.cpp, TaskSimpleStandStill.cpp, TaskSimpleStandUp.cpp, TaskSimpleStealthKill.cpp, TaskSimpleSwim.cpp, TaskSimpleThrowProjectile.cpp ... |
| S16 | game_sa/Entity/Vehicle | 48 | partial 23/48 (18 M, 5 fixed) -> S16b | Automobile.cpp |
| S17 | game_sa/Entity/Vehicle | 43 | partial 21/43 (17 M, 4 fixed) -> S17b | Bike.cpp, Bmx.cpp, Boat.cpp |
| S18 | game_sa/Entity/Vehicle | 41 | partial 17/41 (13 M, 4 fixed) -> S18b | Heli.cpp, MonsterTruck.cpp, Plane.cpp, QuadBike.cpp |
| S19 | game_sa/Entity/Vehicle | 108 | partial 3/108 (3 M) -> S19b | Trailer.cpp, Train.cpp, Vehicle.cpp |
| S20 | game_sa/Entity/Ped | 75 | partial 54/75 (42 M, 12 fixed) -> S20b | CivilianPed.cpp, CopPed.cpp, EmergencyPed.cpp, Ped.cpp |
| S21 | game_sa/Entity/Ped | 32 | partial 23/32 (20 M, 3 fixed) -> S21b | PedSaveStructure.cpp, PlayerPed.cpp, PlayerSkin.cpp |
| S22 | game_sa/CarCtrl.cpp | 55 | partial 21/55 (17 M, 4 fixed) -> S22b | CarCtrl.cpp |
| S23 | game_sa/Population.cpp | 33 | partial 20/33 (11 M, 9 fixed) -> S23b | Population.cpp |
| S24 | game_sa/PedIntelligence.cpp | 31 | done (28 M, 3 fixed) | PedIntelligence.cpp |
| S25 | game_sa/Events/GroupEventHandler.cpp | 30 | done (18 M, 12 fixed) | GroupEventHandler.cpp |
| S26 | game_sa/Entity/Physical.cpp | 28 | partial 19/28 (15 M, 4 fixed) -> S26b | Physical.cpp |
| S27 | game_sa/World.cpp | 44 | partial 29/44 (19 M, 10 fixed) -> S27b | World.cpp |
| S28 | game_sa/Streaming.cpp | 73 | partial 19/73 (15 M, 4 fixed) -> S28b | Streaming.cpp |
| S29 | game_sa/Entity/Object | 39 | partial 34/39 (24 M, 10 fixed) -> S29b | CutsceneObject.cpp, HandObject.cpp, Object.cpp |
| S30 | game_sa/Camera.cpp | 35 | partial 14/35 (12 M, 2 fixed, 2 partial) -> S30b | Camera.cpp |
| S31 | game_sa/Cam.cpp | 34 | partial 3/34 (1 M, 2 fixed) -> S31b | Cam.cpp |
| S32 | game_sa/Radar.cpp | 28 | partial 1/28 (1 fixed) -> S32b | Radar.cpp |
| S33 | game_sa/WaterLevel.cpp | 26 | partial 8/26 (3 M, 5 fixed) -> S33b | WaterLevel.cpp |
| S34 | game_sa/Interior/Interior_c.cpp | 41 | partial 24/41 (24 M, 3 via new oracle rows) -> S34b | Interior_c.cpp |
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
| S11b | rest of S11 | 9 | 0/9 (not reached) -> S11c | TaskSimpleCarSetPedInAsDriver.cpp, TaskSimpleCarSetPedInAsPassenger.cpp, TaskSimpleCarSetPedSlowDraggedOut.cpp, TaskSimpleClimb.cpp |
| S12b | rest of S12 | 10 | partial 6/10 (5 M, 1 fixed) -> S12c | TaskSimpleDead.cpp, TaskSimpleFight.cpp, TaskSimpleGangDriveBy.cpp |
| S20c | rest of S20b | 13 | partial 3/13 (2 M, 1 fixed) -> S20d | Ped.cpp |
| S16c | rest of S16b | 17 | partial 2/17 (2 fixed) -> S16d | Automobile.cpp |
| S17c | rest of S17b | 16 | partial 6/16 (2 M, 4 fixed) -> S17d | Bike.cpp, Bmx.cpp, Boat.cpp |
| S21c | rest of S21b | 4 | done 4/4 (2 M, 2 fixed; S21 complete) | PlayerPed.cpp |
| S13c | rest of S13b | 4 | done (1 M, 3 fixed) | TaskSimpleGunControl.cpp, TaskSimpleInAir.cpp, TaskSimpleJetPack.cpp |
| S22e | rest of S22d | 9 | partial 4/9 (4 M) -> S22f | CarCtrl.cpp |
| S27c | rest of S27b | 5 | partial 3/5 (3 fixed) -> S27d | World.cpp |
| S14b | rest of S14 | 23 | partial 17/23 (9 M, 8 fixed) -> S14c | TaskSimpleJump.cpp, TaskSimplePickUpBike.cpp, TaskSimplePlayerOnFire.cpp, TaskSimplePlayerOnFoot.cpp, TaskSimpleSay.cpp, TaskSimpleSetPedAsAutoDriver.cpp, TaskSimpleShakeFist.cpp, TaskSimpleSitDown.cpp, TaskSimpleSitIdle.cpp |
| S18b | rest of S18 | 24 | partial 13/24 (12 M, 1 fixed) -> S18c | Heli.cpp, Plane.cpp |
| S18c | rest of S18b | 11 | partial 8/11 (7 M, 1 fixed) -> S18d | Heli.cpp, Plane.cpp |
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
| S18d | rest of S18c | 3 | partial 2/3 + PreRender to 0x6C9FD9 (1 M, 1 fixed) -> S18e | Heli.cpp, Plane.cpp |
| S18e | rest of S18d | 1 | done 1/1 (MATCH, 1 U: _CIpow; S18 complete) | Plane.cpp |
| S22f | rest of S22e + plane obstacle helper | 6 | partial 3/6 (3 M) -> S22g | CarCtrl.cpp |
| S22g | rest of S22f | 3 | partial 2/3 (1 M, 1 fixed) -> S22h | CarCtrl.cpp |
| S22h | rest of S22g | 1 | done 1/1 (MATCH; S22 complete) | CarCtrl.cpp |
| S27d | rest of S27c | 2 | done 2/2 (2 fixed; S27 complete) | World.cpp |
| S02b | rest of S02 | 11 | done 11/11 (7 M, 4 fixed; S02 complete) | TaskComplexCopInCar.cpp, TaskComplexDestroyCar.cpp, TaskComplexDestroyCarMelee.cpp, TaskComplexDie.cpp, TaskComplexDieInCar.cpp |
| S03b | rest of S03 | 13 | done 13/13 (5 M, 8 fixed; S03 complete) | TaskComplexEnterCar.cpp, TaskComplexFleeAnyMeans.cpp, TaskComplexFleeEntity.cpp, TaskComplexFleePoint.cpp |
| S04b | rest of S04 (+ UNSURE CreateTask 0x669090) | 20 | partial 8/20 (6 M, 2 fixed) -> S04c | TaskComplexFollowNodeRoute.cpp, TaskComplexFollowPatrolRoute.cpp, TaskComplexFollowPedFootsteps.cpp, TaskComplexFollowPointRoute.cpp, TaskComplexGangFollower.cpp, TaskComplexGangJoinRespond.cpp, TaskComplexGangLeader.cpp, TaskComplexGetOnBoatSeat.cpp |
| S04c | rest of S04b | 11 | done 11/11 (10 M, 1 fixed; S04 complete) | TaskComplexGangFollower.cpp, TaskComplexGangLeader.cpp |
| S05b | rest of S05 (+ tails: GoPickUpEntity CFST from 0x693881, InAirAndLand CNST from 0x67CFD8) | 36 | partial 12/36 (7 M, 5 fixed) -> S05c | TaskComplexGoPickUpEntity.cpp, TaskComplexGoToCarDoorAndStandStill.cpp, TaskComplexGoToPointAiming.cpp, TaskComplexGoToPointAndStandStillTimed.cpp, TaskComplexGotoDoorAndOpen.cpp, TaskComplexHitPedWithCar.cpp, TaskComplexInvestigateDeadPed.cpp, TaskComplexInvestigateDisturbance.cpp, TaskComplexKillAllThreats.cpp, TaskComplexKillCriminal.cpp, TaskComplexKillPedFromBoat.cpp, TaskComplexKillPedGroupOnFoot.cpp, TaskComplexKillPedOnFoot.cpp, TaskComplexKillPedOnFootArmed.cpp |
| S05c | rest of S05b | 24 | partial 16/24 (12 M, 4 fixed) -> S05d | TaskComplexGoPickUpEntity.cpp, TaskComplexGoToCarDoorAndStandStill.cpp, TaskComplexGoToPointAndStandStillTimed.cpp, TaskComplexHitPedWithCar.cpp, TaskComplexInAirAndLand.cpp, TaskComplexKillAllThreats.cpp, TaskComplexKillCriminal.cpp, TaskComplexKillPedFromBoat.cpp, TaskComplexKillPedGroupOnFoot.cpp, TaskComplexKillPedOnFoot.cpp, TaskComplexKillPedOnFootArmed.cpp |
| S05d | rest of S05c | 8 | partial 7/8 (4 M, 3 fixed) -> S05e | TaskComplexKillPedOnFoot.cpp, TaskComplexKillPedOnFootArmed.cpp |
| S05e | rest of S05d | 1 | done 1/1 (1 fixed; S05 complete) | TaskComplexKillPedOnFootArmed.cpp |
| S06b | rest of S06 + LeaveCarAndDie ctor | 35 | partial 33/35 (27 M, 6 fixed) -> S06c | TaskComplexLeaveBoat.cpp, TaskComplexLeaveCar.cpp, TaskComplexLeaveCarAsPassengerWait.cpp, TaskComplexMedicTreatInjuredPed.cpp, TaskComplexMoveBackAndJump.cpp, TaskComplexObserveTrafficLights.cpp, TaskComplexOnFire.cpp, TaskComplexPartner.cpp, TaskComplexPartnerDeal.cpp, TaskComplexPartnerGreet.cpp |
| S06c | rest of S06b | 2 | done 2/2 (MATCH; S06 complete) | TaskComplexPartner.cpp |
| S07b | rest of S07 | 30 | done 30/30 (22 M, 8 fixed; S07 complete) | TaskComplexReactToGunAimedAt.cpp, TaskComplexRoadRage.cpp, TaskComplexScreamInCarThenLeave.cpp, TaskComplexSeekCoverUntilTargetDead.cpp, TaskComplexSeekEntityAiming.cpp, TaskComplexShuffleSeats.cpp, TaskComplexSignalAtPed.cpp, TaskComplexSitDownThenIdleThenStandUp.cpp, TaskComplexSmartFleeEntity.cpp, TaskComplexSmartFleePoint.cpp |
| S08b | rest of S08 | 8 | done 8/8 (6 M, 2 fixed; S08 complete) | TaskComplexUsePairedAttractor.cpp, TaskComplexUseSwatRope.cpp, TaskComplexUseWaterCannon.cpp |
| S10b | rest of S10 (+ BeHitWhileMoving StartAnim tail from 0x6200F4) | 26 | done 26/26 (22 M, 4 fixed; S10 complete) | TaskSimpleCarAlign.cpp, TaskSimpleCarCloseDoorFromInside.cpp, TaskSimpleCarCloseDoorFromOutside.cpp, TaskSimpleCarDrive.cpp, TaskSimpleCarFallOut.cpp, TaskSimpleCarGetIn.cpp |
| S15b | rest of S15 | 20 | partial 14/20 (8 M, 6 fixed) -> S15c | TaskSimpleSwim.cpp, TaskSimpleThrowProjectile.cpp, TaskSimpleTired.cpp, TaskSimpleTurn180.cpp, TaskSimpleUseGun.cpp, TaskSimpleWaitUntilAreaCodesMatch.cpp, TaskSimpleWaitUntilPedIsOutCar.cpp, TaskUtilityLineUpPedWithCar.cpp |
| S15c | rest of S15b | 6 | partial 2/6 (1 M, 1 fixed) -> S15d | TaskSimpleSwim.cpp, TaskSimpleUseGun.cpp, TaskUtilityLineUpPedWithCar.cpp |
| S15d | rest of S15c | 4 | partial 3/4 (2 M, 1 fixed) -> S15e | TaskSimpleSwim.cpp, TaskUtilityLineUpPedWithCar.cpp |
| S15e | rest of S15d | 1 | done 1/1 (fixed NaN branches; slerp/quat arithmetic tails spot-checked; S15 complete) | TaskUtilityLineUpPedWithCar.cpp |
| S19b | rest of S19 | 105 | partial 1/105 (Train ProcessControl fixed; portable double cleanup) -> S19c | Trailer.cpp, Train.cpp, Vehicle.cpp |
| S19c | rest of S19b | 104 | partial 3/104 (2 M, 1 fixed + Train PC NaN fix) -> S19d | Trailer.cpp, Train.cpp, Vehicle.cpp |
| S19d | rest of S19c | 101 | partial 10/101 (8 M, 2 fixed; CTrain done) -> S19e | Trailer.cpp, Train.cpp, Vehicle.cpp |
| S19e | rest of S19d (+ Train MarkSurrounding sub-ulp sector clamp) | 92 | partial 8/92 (4 M, 3 fixed, 1 partial) -> S19f | Trailer.cpp, Vehicle.cpp |
| S19f | rest of S19e | 85 | partial 8/85 (5 M, 3 fixed) -> S19g | Trailer.cpp, Train.cpp, Vehicle.cpp |
| S19g | rest of S19f | 77 | partial 15/77 (8 M, 6 fixed, 1 U) -> S19h | Trailer.cpp, Vehicle.cpp |
| S19h | rest of S19g (+ AddExhaustParticles 2nd half) | 63 | partial 10/63 (6 M, 4 fixed) -> S19i | Trailer.cpp, Vehicle.cpp |
| S19i | rest of S19h | 53 | partial 1/53 (ProcessBoatControl fixed, preamble compare-level) + NaN cleanup -> S19j | Vehicle.cpp |
| S19j | rest of S19i (+ ProcessBoatControl preamble 0x6DBCE0-0x6DBF0A full arithmetic) | 53 | partial 6/53 (1 M, 4 fixed, 1 U) -> S19k | Vehicle.cpp |
| S19k | rest of S19j (+ AddSingleWheelParticles 0x6DEC3C-0x6DF3B9) | 48 | partial 19/48 (16 M, 3 fixed) -> S19l | Vehicle.cpp |
| S19l | rest of S19k | 29 | partial 15/29 (6 M, 8 fixed, 1 U) -> S19m | Vehicle.cpp |
| S19m | rest of S19l (+ DoHeadLightBeam x87 sub-ulp) | 15 | done 15/15 (12 M, 3 fixed; S19 complete) | Vehicle.cpp |
| S33b | rest of S33 (GetWaterLevel family is oracle-covered: world_oracle_env) | 18 | partial 4/18 (3 fixed, 1 partial) -> S33c | WaterLevel.cpp |
| S28b | rest of S28 | 54 | partial 20/54 (15 M, 5 fixed) -> S28c | Streaming.cpp |
| S28c | rest of S28b | 34 | partial 11/34 (7 M, 4 fixed + task0) -> S28d | Streaming.cpp |
| S28d | rest of S28c | 23 | done 23/23 (17 M, 6 fixed; S28 complete) | Streaming.cpp |
| S34b | rest of S34 | 17 | partial 13/17 (13 M) -> S34c | Interior_c.cpp |
| S34c | rest of S34b | 4 | done 4/4 (MATCH; S34 complete) | Interior_c.cpp |
| S32b | rest of S32 | 27 | partial 5/27 (5 fixed) -> S32c | Radar.cpp |
| S32c | rest of S32b (+ DrawLegend LOW/HIGH polygons + sprite path) | 23 | partial 13/23 (5 M, 8 fixed) -> S32d | Radar.cpp |
| S32d | rest of S32c | 8 | done 8/8 (6 M, 2 fixed + CRect recheck 4 fixed; S32 complete) | Radar.cpp |
| S30b | rest of S30 | 21 | partial 9/21 (7 M, 1 fixed, 1 partial) -> S30c | Camera.cpp |
| S30c | rest of S30b | 14 | 2/14 partial (CamControl decision logic + Process head MATCH) -> S30d | Camera.cpp |
| S30d | rest of S30c | 14 | partial 8/14 (3 M, 4 fixed, 1 partial) -> S30e | Camera.cpp |
| S30e | rest of S30d | 7 | partial 1/7 (Process tail MATCH, CamControl partial) -> S30f | Camera.cpp |
| S31b | rest of S31 (+ Process playerPos.z vs blended.z, Rocket heat-seek tail 0x511EC2-0x5120E7) | 33 | partial 4/33 (2 fixed, 2 partial, 1 U) -> S31c | Cam.cpp |
| S31c | rest of S31b (+ CCam::Process target.z stack-slot trace) | 31 | partial 3/31 (1 M, 2 fixed) -> S31d | Cam.cpp |
| S31d | rest of S31c (+ CCam::Process target.z trace) | 30 | partial 3/29 (1 M, 2 fixed) -> S31e | Cam.cpp |
| S31e | rest of S31d | 27 | partial 3/25 (3 fixed) -> S31f | Cam.cpp |
| S31f | rest of S31e | 27 | partial 8/27 (6 fixed, 2 partial) -> S31g | Cam.cpp |
| S31g | rest of S31f | 21 | partial 6/21 (3 M, 3 fixed) + Pickup field fix -> S31h | Cam.cpp |
| S31h | rest of S31g | 15 | partial 5/11 (5 M) -> S31i | Cam.cpp |
| S31i | rest of S31h (CCam::Process done: S31 full + S31e trace) | 6 | partial 5/6 (5 fixed) -> S31j | Cam.cpp |
| S31j | rest of S31i | 1 | done 1/1 (fixed; S31 complete) | Cam.cpp |
| S30f | rest of S30e | 6 | CamControl tail MATCH; pow bases + aimHeading fixed -> S30g | Camera.cpp |
| S30g | rest of S30f | 5 | 4/5 (3 M, 1 fixed) -> S30h | Camera.cpp |
| S30h | rest of S30g | 1 | done 1/1 (MATCH; S30 complete) | Camera.cpp |
| S33c | rest of S33b | 15 | partial 3/15 (1 M, 2 fixed; unicorn emulator models) -> S33d | WaterLevel.cpp |
| S33d | rest of S33c | 12 | partial 5/12 (1 M, 4 fixed) -> S33e | WaterLevel.cpp |
| S33e | rest of S33d | 7 | 6/7 (2 M, 4 fixed) -> S33f | WaterLevel.cpp |
| S33f | rest of S33e | 1 | 0/1 (not reached) -> S33g | WaterLevel.cpp |
| S12c | rest of S12b | 4 | done 4/4 (3 M, 1 fixed; S12 complete) | TaskSimpleGangDriveBy.cpp |
| S11c | rest of S11b | 9 | partial 4/9 (2 M, 2 fixed) -> S11d | TaskSimpleCarSetPedInAsDriver.cpp, TaskSimpleCarSetPedInAsPassenger.cpp, TaskSimpleCarSetPedSlowDraggedOut.cpp, TaskSimpleClimb.cpp |
| S33g | rest of S33f | 1 | 0/1 (not reached) -> S33h | WaterLevel.cpp |
| S11d | rest of S11c | 5 | done 5/5 (2 M, 3 fixed; S11 complete) | TaskSimpleClimb.cpp |
| S33h | rest of S33g | 1 | done 1/1 (fixed; S33 complete) | WaterLevel.cpp |
| S41 | phase2: unlisted non-oracle hooks | 40 | partial 31/40 (23 M, 8 fixed) -> S41b | AttractorScanner.cpp, CarAI.cpp, PedAtmAttractor.cpp, PedAttractor.cpp, PedAttractorPedPlacer.cpp, PedLookAtAttractor.cpp ... |
| S42 | phase2: unlisted non-oracle hooks | 40 | partial 21/40 (9 M, 12 fixed) -> S42b | CarAI.cpp, CarEnterExit.cpp, CarGenerator.cpp |
| S43 | phase2: unlisted non-oracle hooks | 40 | partial 3/40 (2 M, 1 fixed) -> S43b | CarGenerator.cpp, Cheat.cpp, Clothes.cpp, CollisionEventScanner.cpp, ConversationForPed.cpp, Cover.cpp ... |
| S44 | phase2: unlisted non-oracle hooks | 40 | done 40/40 (33 M, 7 fixed) | AnimatedBuilding.cpp, Building.cpp, CustomCarPlateMgr.cpp, Darkel.cpp, DummyObject.cpp, DummyPed.cpp ... |
| S45 | phase2: unlisted non-oracle hooks | 40 | partial 30/41 (19 M, 11 fixed) -> S45b | Bike.cpp, Bmx.cpp, Boat.cpp, EmergencyPed.cpp, Entity.cpp, EntityScanner.cpp ... |
| S46 | phase2: unlisted non-oracle hooks | 40 | 37/40 (30 M, 7 fixed) -> S46b | EntryExitManager.cpp, Event.cpp, EventAcquaintancePedDislike.cpp, EventAcquaintancePedLike.cpp, EventAcquaintancePedRespect.cpp, EventAttractor.cpp ... |
| S47 | phase2: unlisted non-oracle hooks | 40 | partial 21/40 (17 M, 4 fixed) -> S47b | EventHandler.cpp, EventHealthLow.cpp, EventHealthReallyLow.cpp, EventHighAngerAtPlayer.cpp, EventInAir.cpp, EventInWater.cpp ... |
| S48 | phase2: unlisted non-oracle hooks | 40 | partial 20/40 (18 M, 2 fixed) -> S48b | EventRevived.cpp, EventScanner.cpp, EventSexyPed.cpp, EventSexyVehicle.cpp, EventSpecial.cpp, EventVehicleThreat.cpp ... |
| S49 | phase2: unlisted non-oracle hooks | 40 | partial 30/40 (11 M, 19 fixed) -> S49b | Game.cpp, GameLogic.cpp, GangWars.cpp |
| S50 | phase2: unlisted non-oracle hooks | 40 | partial 25/40 (22 M, 3 fixed) -> S50b | GangWars.cpp, Gangs.cpp, Garage.cpp, Garages.cpp, InformFriendsEventQueue.cpp, InformGroupEvent.cpp ... |
| S51 | phase2: unlisted non-oracle hooks | 40 | partial 20/40 (20 M) -> S51b | FurnitureManager_c.cpp, FurnitureSubGroup_c.cpp, InterestingEvents.cpp, InteriorGroup_c.cpp, InteriorManager_c.cpp, Interior_c.cpp ... |
| S52 | phase2: unlisted non-oracle hooks | 40 | done (31 M, 6 fixed, 3 no callers) | PathFind.cpp, PedAttractorManager.cpp, PedClothesDesc.cpp |
| S53 | phase2: unlisted non-oracle hooks | 40 | partial 15/40 (14 M, 1 fixed) -> S53b | PedClothesDesc.cpp, PedDamageResponseCalculator.cpp, PedGeometryAnalyser.cpp, PedGroup.cpp, PedGroupIntelligence.cpp |
| S54 | phase2: unlisted non-oracle hooks | 40 | partial 27/40 (20 M, 7 fixed, 1 U) -> S54c | PedGroupIntelligence.cpp, PedGroupMembership.cpp, PedGroupPlacer.cpp, PedGroups.cpp, PedIK.cpp, PedList.cpp ... |
| S55 | phase2: unlisted non-oracle hooks | 40 | 38/40 (26 M, 12 fixed) -> S55b | PedPlacement.cpp, PedStats.cpp, PedStuckChecker.cpp, PedTaskPair.cpp, PedType.cpp, Pickup.cpp ... |
| S56 | phase2: unlisted non-oracle hooks | 40 | partial 27/40 (17 M, 10 fixed) -> S56b | Pickups.cpp, PlaneTrail.cpp, PlaneTrails.cpp, PlayerInfo.cpp, PlayerPedData.cpp, PlayerRelationshipRecorder.cpp |
| S57 | phase2: unlisted non-oracle hooks | 40 | partial 18/40 (15 M, 3 fixed) -> S57b | EventPool.h, PedAttractorPool.h, PedIntelligencePool.h, PedPool.h, PlayerRelationshipRecorder.cpp, Remote.cpp ... |
| S58 | phase2: unlisted non-oracle hooks | 40 | partial 23/40 (20 M, 3 fixed, 1 U) -> S58b | ScriptsForBrains.cpp, Shopping.cpp, TheScripts.cpp |
| S59 | phase2: unlisted non-oracle hooks | 40 | partial 25/40 (20 M, 5 fixed) -> S59b | Shopping.cpp, Stats.cpp, StreamedScripts.cpp, StuckCarCheck.cpp |
| S60 | phase2: unlisted non-oracle hooks | 40 | partial 23/40 (18 M, 5 fixed + 2 helpers) -> S60b | PedGroupDefaultTaskAllocatorChat.h, PedGroupDefaultTaskAllocatorFollowAnyMeans.h, PedGroupDefaultTaskAllocatorFollowLimited.h, PedGroupDefaultTaskAllocatorRandom.h, PedGroupDefaultTaskAllocatorSitInLeaderCar.h, PedGroupDefaultTaskAllocatorStandStill.h ... |
| S61 | phase2: unlisted non-oracle hooks | 40 | done (35 M, 5 fixed) | TaskComplexAvoidEntity.cpp, TaskComplexAvoidOtherPedWhileWandering.cpp, TaskComplexBeCop.cpp, TaskComplexBeStill.cpp, TaskComplexCarDriveMission.cpp, TaskComplexClimb.cpp ... |
| S62 | phase2: unlisted non-oracle hooks | 40 | done (39 M, 1 fixed) | TaskComplexFollowPatrolRoute.cpp, TaskComplexGangLeader.cpp, TaskComplexKillAllThreats.cpp, TaskComplexKillPedGroupOnFoot.cpp, TaskComplexLeaveCar.cpp, TaskComplexObserveTrafficLights.cpp ... |
| S63 | phase2: unlisted non-oracle hooks | 40 | done (33 M, 8 fixed) | TaskSimpleNone.cpp, TaskSimpleOnEscalator.cpp, TaskSimpleRunAnim.cpp, TaskSimpleRunAnimLoopedMiddle.cpp, TaskSimpleRunNamedAnim.cpp, TaskSimpleSay.cpp ... |
| S64 | phase2: unlisted non-oracle hooks | 40 | partial 9/40 (4 M, 5 fixed) -> S64b | VehicleAnimGroupData.cpp, VehicleRecording.cpp, VehicleSaveStructure.cpp, Weapon.cpp |
| S65 | phase2: unlisted non-oracle hooks | 7 | partial (4 M, 1 fixed) -> S65b | WeaponEffects.cpp, WeaponInfo.cpp, World.cpp |
| S41b | rest of S41 | 9 | done (2 M, 6 fixed, RunningScript row -> S66) | CarAI.cpp, PedAttractorPedPlacer.cpp, RunningScript.cpp matches |
| S66 | phase2: RunningScript.cpp hooks (missed by the generator: grep without -a) | 15 | done (4 M, 11 fixed) | RunningScript.cpp |
| S42b | rest of S42 | 19 | UpdateCarAI fixed to 0x41FE13 (tail partial); CarEnterExit 18 NR -> S42c | CarAI.cpp, CarEnterExit.cpp |
| S42c | rest of S42b (+ UpdateCarAI tail 0x41FE13-0x420678) | 19 | partial 10/19 (5 M, 5 fixed) -> S42d | CarEnterExit.cpp |
| S42d | rest of S42c | 9 | 8/9 (4 M, 4 fixed) -> S42e | CarEnterExit.cpp |
| S42e | rest of S42d | 1 | done 1/1 (fixed; S42 complete) | CarEnterExit.cpp |
| S43b | rest of S43 | 37 | partial 5/37 (3 M, 1 fixed, 1 U) -> S43c | CarGenerator.cpp, Cheat.cpp, Clothes.cpp, CollisionEventScanner.cpp, ConversationForPed.cpp, Cover.cpp, CoverPoint.cpp, CurrentVehicle.cpp, CustomCarPlateMgr.cpp, PedToPlayerConversations.cpp |
| S43c | rest of S43b | 32 | partial 6/32 (5 M, 1 U) + clock audit (2 fixed) -> S43d | Clothes.cpp, ConversationForPed.cpp, Cover.cpp, CoverPoint.cpp, CurrentVehicle.cpp, CustomCarPlateMgr.cpp, PedToPlayerConversations.cpp |
| S43d | rest of S43c (+ ConversationForPed::Update rest) | 27 | done (16 M, 7 fixed, 4 no exe callers; S43 complete) | Clothes.cpp, Cover.cpp, CurrentVehicle.cpp, CustomCarPlateMgr.cpp |
| S47b | rest of S47 | 19 | partial 14/19 (9 M, 5 fixed) -> S47c | EventHandler.cpp, EventInWater.cpp, EventInteriorUseInfo.cpp, EventKnockOffBike.cpp, EventLeaderEntryExit.cpp, EventNewGangMember.cpp, EventPedEnteredMyVehicle.cpp, EventPedToChase.cpp, EventPotentialGetRunOver.cpp |
| S55b | rest of S55 | 2 | done 2/2 (1 M, 1 fixed; S55 complete) |  |
| S49b | rest of S49 | 10 | done (8 M, 2 fixed + SetUpSkip; GameLogic::Update only first 640 lines) | Game.cpp, GameLogic.cpp, GangWars.cpp |
| S48b | rest of S48 | 20 | fire part done by S51; CGame 16 open -> S48c | FireManager.cpp, Game.cpp |
| S47c | rest of S47b | 5 | done 5/5 (1 M, 4 fixed; S47 complete) | EventHandler.cpp |
| S56b | rest of S56 | 13 | 13/14 (9 M, 4 fixed, 1 U) -> S56c | PlayerInfo.cpp, PlayerPedData.cpp |
| S50b | rest of S50 | 15 | done (12 M, 3 fixed; CGarage::Update handlers -> S48c) | GangWars.cpp, Garage.cpp, Garages.cpp |
| S56c | rest of S56b | 1 | done (MATCH; S56 complete) | PlayerInfo.cpp |
| S57b | rest of S57 | 25 | partial 18/25 (14 M, 4 fixed) -> S57c | PedAttractorPool.h, PedIntelligencePool.h, PedPool.h, Replay.cpp, TheScripts.cpp, VehiclePool.h |
| S51b | rest of S51 | 20 | done (14 M, 5 fixed, 1 n/a; S51 complete) | FurnitureManager_c.cpp, InterestingEvents.cpp, InteriorGroup_c.cpp, InteriorManager_c.cpp, Interior_c.cpp, PathFind.cpp |
| S53b | rest of S53 | 25 | 24/25 (15 M, 9 fixed) -> S53c | PedClothesDesc.cpp, PedGeometryAnalyser.cpp, PedGroup.cpp |
| S48c | CGame rows + CGarage::Update handlers + CGameLogic::Update tail | 18 | done (14 M, 4 fixed; S48, S49, S50 complete) | Game.cpp |
| S57c | rest of S57b | 10 | 3/10 (3 M) -> S57d | PedIntelligencePool.h, PedPool.h, Replay.cpp, VehiclePool.h |
| S53c | rest of S53b | 1 | moved to S54c | PedGeometryAnalyser.cpp |
| S58b | rest of S58 | 17 | done (11 M, 6 fixed; S58 complete; CleanUpThisPed/Vehicle re-verified MATCH) | Shopping.cpp, TheScripts.cpp |
| S64b | rest of S64 | 31 | partial 11/31 (3 M, 8 fixed) -> S64c | VehicleAnimGroupData.cpp, VehicleRecording.cpp, VehicleSaveStructure.cpp, Weapon.cpp |
| S57d | rest of S57c | 7 | partial 1/7 (1 fixed) -> S57e | Replay.cpp |
| S54c | rest of S54 (+ S53c ComputeClearTarget) | 14 | done (10 M, 3 fixed; S53, S54 complete) | PedGroupIntelligence.cpp, PedGroupMembership.cpp, PedGroupPlacer.cpp |
| S64c | rest of S64b | 20 | done (11 M, 8 fixed, LaserScopeDot no callers; S64 complete) | VehicleAnimGroupData.cpp, VehicleRecording.cpp, VehicleSaveStructure.cpp, Weapon.cpp |
| S46b | rest of S46 | 3 | done (2 M, 1 fixed) | EventHandler.cpp |
| S59b | rest of S59 | 15 | done (13 M, 2 fixed) | Stats.cpp |
| S45b | rest of S45 | 11 | 10/11 (4 M, 6 fixed) -> S45c | Entity.cpp, EntryExitManager.cpp |
| S60b | rest of S60 | 17 | done (14 M, 3 fixed) | Task.cpp, TaskAllocator.cpp, TaskAllocatorAttack.cpp, TaskAllocatorKillOnFoot.cpp, TaskAllocatorKillThreatsBasic.cpp, TaskAllocatorKillThreatsBasicRandomGroup.cpp, TaskAllocatorKillThreatsDriveby.cpp, TaskAllocatorPlayerCommandAttack.cpp, TaskComplex.cpp, TaskManager.cpp |
| S65b | rest of S65 | 2 | done (1 M, 4 fixed incl. 3 Zones extras) | WeaponEffects.cpp, WeaponInfo.cpp |
| S57e | rest of S57d | 6 | done (2 M, 4 fixed + IsVehicleStopped) | Replay.cpp |
| S45c | rest of S45b (ProcessLightsForEntity) | 1 | done (1 fixed, 5 deviations) | Entity.cpp |
| S67 | phase2b: coverage audit (COVERAGE.md) | 13 | done (2 M, 3 fixed, 2 covered, 7 dead in exe) | Automobile.cpp, Cam.cpp, CollisionEventScanner.cpp, Cover.cpp, CustomCarPlateMgr.cpp, Entity.cpp |
| S68 | phase2b: coverage audit (COVERAGE.md) | 36 | done (33 M, 3 fixed) | Cheat.cpp |
| S69 | phase2b: coverage audit (COVERAGE.md) | 38 | partial 28/38 (23 M, 5 fixed) -> S69b | ColAccel.cpp, ColModel.cpp, Collision.cpp, CollisionData.cpp, TempColModels.cpp |
| S70 | phase2b: coverage audit (COVERAGE.md) | 42 | partial 11/42 (7 M, 4 fixed) -> S70b | Birds.cpp, BouncingPanel.cpp, BreakManager_c.cpp, BreakObject_c.cpp, Door.cpp, Escalator.cpp |
| S71 | phase2b: coverage audit (COVERAGE.md) | 34 | partial 21/34 (13 M, 8 fixed) -> S71b | 3dMarker.cpp, MovingThings.cpp, Rope.cpp, Ropes.cpp, TrafficLights.cpp, WaterCannon.cpp |
| S72 | phase2b: coverage audit (COVERAGE.md) | 35 | done (28 M, 7 fixed) | 3dMarkers.cpp, Checkpoint.cpp, Checkpoints.cpp, ConversationNode.cpp, Conversations.cpp, Credits.cpp |
| S73 | phase2b: coverage audit (COVERAGE.md) | 44 | done (28 M, 14 fixed, 2 dead in exe) | CutsceneMgr.cpp, Formation.cpp, IdleCam.cpp, MentalHealth.cpp, Messages.cpp |
| S74 | phase2b: coverage audit (COVERAGE.md) | 41 | partial 22/41 (15 M, 7 fixed) -> S74b | MissionCleanup.cpp, OnscreenCounterEntry.cpp, OnscreenTimer.cpp, PatrolRoute.cpp, PlaceName.cpp, RoadBlocks.cpp |
| S75 | phase2b: coverage audit (COVERAGE.md) | 37 | partial 20/37 (17 M, 3 fixed) -> S75b | AccidentManager.cpp, AutoPilot.cpp, BulletInfo.cpp, BulletTraces.cpp, C_PcSave.cpp, DamageManager.cpp |
| S76 | phase2b: coverage audit (COVERAGE.md) | 46 | partial 26/46 (23 M, 3 fixed) -> S76b | ClothesBuilder.cpp, CustomRoadsignMgr.cpp, GenericGameStorage.cpp |
| S77 | phase2b: coverage audit (COVERAGE.md) | 46 | done (41 M, 5 fixed) | BuildingPool.h, Clock.cpp, ColModelPool.h, ControllerConfigManager.cpp, ControllerState.cpp, Curves.cpp |
| S78 | phase2b: coverage audit (COVERAGE.md) | 23 | done (13 M, 9 fixed, 1 NR: DirectInput ProcessPad replaced by SDL) | KeyboardState.cpp, ModelIndices.cpp, MouseControllerState.cpp, Pad.cpp, PopCycle.cpp, TimeCycle.cpp |
| S79 | phase2b: coverage audit (COVERAGE.md) | 50 | done (43 M, 7 fixed) | 3dMarker.cpp, AccidentManager.cpp, BouncingPanel.cpp, BreakObject_c.cpp, C_PcSave.cpp, ClothesBuilder.cpp |
| S80 | phase2b: coverage audit (COVERAGE.md) | 15 | done (12 M, 3 fixed) | ControllerConfigManager.cpp, DecisionMaker.cpp, Gamma.cpp, GridRef.cpp, MouseControllerState.cpp, Pad.cpp |
| S74b | rest of S74 | 19 | done (16 M, 3 fixed) | MissionCleanup.cpp, OnscreenCounterEntry.cpp, OnscreenTimer.cpp, PatrolRoute.cpp, PlaceName.cpp, TagManager.cpp, UserDisplay.cpp |
| S69b | rest of S69 | 10 | partial 4/10 (3 M, 1 fixed + Invert(CMatrix)) -> S69c | Collision.cpp |
| S70b | rest of S70 | 31 | partial 15/31 (7 M, 8 fixed) -> S70c | Birds.cpp, BreakManager_c.cpp, BreakObject_c.cpp, Escalator.cpp, FallingGlassPane.cpp, Glass.cpp |
| S75b | rest of S75 | 17 | done (9 M, 8 fixed; report in metrics, Write was refused) | AccidentManager.cpp, BulletTraces.cpp, C_PcSave.cpp, NodeRoute.cpp, Skidmarks.cpp, cTransmission.cpp |
| S69c | rest of S69b | 6 | done (5 M, 1 fixed) | Collision.cpp |
| S70c | rest of S70b | 16 | done (8 M, 8 fixed) | Birds.cpp, BreakManager_c.cpp, BreakObject_c.cpp, Escalator.cpp, Glass.cpp |
| S71b | rest of S71 | 15 | 14/15 (8 M, 6 fixed + WithUp) -> S71c | 3dMarker.cpp, Rope.cpp, Ropes.cpp, WaterCannon.cpp, WaterCannons.cpp |
| S71c | rest of S71b | 1 | done (1 M) | Rope.cpp |
| S76b | rest of S76 | 20 | pending | ClothesBuilder.cpp, CustomRoadsignMgr.cpp |
| S81 | phase3 tier A (PHASE3.md) | 41 | pending | AECollisionAudioEntity.cpp, AEDoorAudioEntity.cpp, AEFireAudioEntity.cpp, AEFrontendAudioEntity.cpp, AEGlobalWeaponAudioEntity.cpp, AEPedAudioEntity.cpp |
| S82 | phase3 tier A (PHASE3.md) | 44 | pending | AEPedSpeechAudioEntity.cpp, AEPedlessSpeechAudioEntity.cpp, AEPoliceScannerAudioEntity.cpp |
| S83 | phase3 tier A (PHASE3.md) | 47 | pending | AEScriptAudioEntity.cpp, AETwinLoopSoundEntity.cpp, AEVehicleAudioEntity.cpp |
| S84 | phase3 tier A (PHASE3.md) | 36 | pending | AEVehicleAudioEntity.cpp, AEWaterCannonAudioEntity.cpp |
| S85 | phase3 tier A (PHASE3.md) | 33 | pending | AEAudioEnvironment.cpp, AEAudioUtility.cpp, AESound.cpp, AEWeaponAudioEntity.cpp, AEWeatherAudioEntity.cpp |
| S86 | phase3 tier A (PHASE3.md) | 38 | pending | AudioEngine.cpp |
| S87 | phase3 tier A (PHASE3.md) | 24 | pending | AEAmbienceTrackManager.cpp, AEAudioHardware.cpp, AECutsceneTrackManager.cpp |
| S88 | phase3 tier A (PHASE3.md) | 43 | pending | AERadioTrackManager.cpp, AESoundManager.cpp |
| S89 | phase3 tier A (PHASE3.md) | 46 | pending | AEUserRadioTrackManager.cpp, AnimBlendAssocGroup.cpp, AnimManager.cpp |
| S90 | phase3 tier A (PHASE3.md) | 40 | pending | AEBankLoader.cpp, AEMP3BankLoader.cpp, AEMP3TrackLoader.cpp, AETrackLoader.cpp, ColStore.cpp, Directory.cpp |
| S91 | phase3 tier A (PHASE3.md) | 24 | pending | FileLoader.cpp |
| S92 | phase3 tier A (PHASE3.md) | 42 | pending | FileLoader.cpp, FileMgr.cpp, IniFile.cpp |
| S93 | phase3 tier A (PHASE3.md) | 43 | pending | Fx.cpp, FxEmitter.cpp, IplStore.cpp |
| S94 | phase3 tier A (PHASE3.md) | 45 | pending | FxEmitterBP.cpp, FxInfoManager.cpp, FxManager.cpp, FxPrimBP.cpp, FxSphere.cpp, FxSystem.cpp |
| S95 | phase3 tier B (PHASE3.md) | 30 | pending | Hud.cpp, HudColours.cpp |
| S96 | phase3 tier B (PHASE3.md) | 45 | pending | LoadingScreen.cpp, MenuManager.cpp |
| S97 | phase3 tier B (PHASE3.md) | 34 | pending | MenuManager.cpp, MenuSystem.cpp |
| S98 | phase3 tier B (PHASE3.md) | 37 | pending | AtomicModelInfo.cpp, BaseModelInfo.cpp, ClumpModelInfo.cpp, DamageAtomicModelInfo.cpp, ModelInfo.cpp, PedModelInfo.cpp |
| S99 | phase3 tier B (PHASE3.md) | 39 | pending | VehicleModelInfo.cpp, WeaponModelInfo.cpp |
| S100 | phase3 tier B (PHASE3.md) | 35 | pending | RealTimeShadow.cpp, RealTimeShadowManager.cpp, ShadowCamera.cpp |
| S101 | phase3 tier B (PHASE3.md) | 35 | pending | Shadows.cpp, StencilShadowObject.cpp, StencilShadows.cpp |
| S102 | phase3 tier B (PHASE3.md) | 40 | pending | BrightLights.cpp, Clouds.cpp, Coronas.cpp, Mirrors.cpp, MotionBlurStreaks.cpp, Occlusion.cpp |
| S103 | phase3 tier B (PHASE3.md) | 36 | pending | PostEffects.cpp, RegisteredMotionBlurStreak.cpp, ShinyTexts.cpp, SpecialFX.cpp |
| S104 | phase3 tier B (PHASE3.md) | 43 | pending | GrassRenderer.cpp, PPTriPlantBuffer.cpp, PlantColEntEntry.cpp, PlantLocTri.cpp, PlantMgr.cpp, PlantSurfPropMgr.cpp |
| S105 | phase3 tier B (PHASE3.md) | 39 | pending | Renderer.cpp, StreamingInfo.cpp, TxdStore.cpp |
| S106 | phase3 tier B (PHASE3.md) | 41 | pending | VisibilityPlugins.cpp |
| S107 | phase3 tier B (PHASE3.md) | 46 | pending | FxInfoAnimTexture.cpp, FxInfoAttractLine.cpp, FxInfoAttractPt.cpp, FxInfoColour.cpp, FxInfoColourBright.cpp, FxInfoColourRange.cpp |
| S108 | phase3 tier B (PHASE3.md) | 45 | pending | AnimBlendAssociation.cpp, AnimBlendClumpData.cpp, AnimBlendHierarchy.cpp, FxInfoJitter.cpp, FxInfoNoise.cpp, FxInfoRotSpeed.cpp |
| S109 | phase3 tier B (PHASE3.md) | 42 | pending | AnimBlendSequence.cpp, AnimBlendStaticAssociation.cpp, Matrix.cpp, MatrixLinkList.cpp, OctTree.cpp, OctTreeBase.cpp |
| S110 | phase3 tier B (PHASE3.md) | 12 | pending | QuadTreeNode.h |
| S111 | phase3 tier C (PHASE3.md) | 44 | pending | AEAudioChannel.cpp, AEAudioHardware.cpp, AESmoothFadeThread.cpp, AEStaticChannel.cpp, AEStreamThread.cpp |
| S112 | phase3 tier C (PHASE3.md) | 37 | pending | AEDataStream.cpp, AEStreamingChannel.cpp, AEStreamingDecoder.cpp |
| S113 | phase3 tier C (PHASE3.md) | 37 | pending | AEVorbisDecoder.cpp, AEWaveDecoder.cpp, Debug.cpp, Font.cpp, Lines.cpp, RenderBuffer.cpp |
| S114 | phase3 tier C (PHASE3.md) | 36 | pending | RwHelper.cpp, Sprite.cpp |
| S115 | phase3 tier C (PHASE3.md) | 42 | pending | CarFXRenderer.cpp, CustomBuildingDNPipeline.cpp, Sprite2d.cpp |
| S116 | phase3 tier C (PHASE3.md) | 45 | pending | BreakablePlugin.cpp, CollisionPlugin.cpp, CustomBuildingPipeline.cpp, CustomBuildingRenderer.cpp, CustomCarEnvMapPipeline.cpp, JPegCompress.cpp |
| S117 | phase3 tier C (PHASE3.md) | 41 | pending | 2dEffect.cpp, LoadMonitor.cpp, MemoryMgr.cpp, Securom.cpp, common.cpp, platform.cpp |

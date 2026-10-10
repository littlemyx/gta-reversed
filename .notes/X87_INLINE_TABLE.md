# Inline x87 transcendental instructions in gta_sa_compact.exe game code (generated 2026-10-10, task fid-x87)

Function = nearest preceding symbol / hooked address (symbols.txt + RH_ hooks); instruction counts per function; port = source file of the hooked C++ port ("-" = not ported/hooked).
Totals (game code, CRT excluded): 327 functions; fsin 359, fcos 302, fpatan 232, fyl2x 51, fptan 22, fsincos 12, fyl2xp1 1

CRT entry points called by game code: floor 0x8219F0, ceil 0x823820, pow 0x8220F0 / _CIpow 0x822130, asin _CI 0x821E70, acos _CI 0x822380, modf 0x8232F0, exp 0x825E10 / _CIexp 0x825E4C. 167 functions call them: floor 398 sites, pow 120 sites, asin 58 sites, pow(0x8220F0) 30 sites, ceil 27 sites, acos 19 sites, modf 2 sites, exp(0x825E4C) 1 sites.
No game function calls a CRT sin/cos/atan2/tan/sqrt/fmod: those are always the inline instructions.

| address | function | port file | inline instructions | CRT calls |
|---|---|---|---|---|
| 00406B70 | None::CdStreamInit | CdStreamInfo.cpp | - | floor x2 |
| 004084F0 | CStreaming::InstanceLoadedModels | Streaming.cpp | - | floor x10 |
| 004090A0 | CStreaming::DeleteAllRwObjects | Streaming.cpp | - | floor x2 |
| 00409210 | CStreaming::DeleteRwObjectsAfterDeath | Streaming.cpp | - | floor x2 |
| 0040D3F0 | CStreaming::AddModelsToRequestList | Streaming.cpp | - | floor x10 |
| 0040D7C0 | CStreaming::DeleteRwObjectsBehindCamera | Streaming.cpp | - | floor x2 |
| 0041AC40 | CCollision::BuildCacheOfCameraCollision | Collision/Collision.cpp | - | floor x8 |
| 0041BCA0 | CBridge::FindBridgeEntities | Bridge.cpp | fcos x2, fsin x2 | - |
| 0041CB70 | CCarAI::MellowOutChaseSpeedBoat | CarAI.cpp | fsin x4, fcos x4 | - |
| 0041CD00 | CCarAI::EntitiesGoHeadOn | CarAI.cpp | fsin x2, fcos x2 | - |
| 0041DA30 | CCarAI::UpdateCarAI | CarAI.cpp | fpatan x4 | asin x1 |
| 00420B30 | None::GetRoll | Entity/Placeable.cpp | fpatan x1 | - |
| 00421A40 | CCarCtrl::ChooseGangCarModel | CarCtrl.cpp | fcos x1, fsin x1 | - |
| 00422B20 | CCarCtrl::SteerAICarBlockingPlayerForwardAndBack | CarCtrl.cpp | fcos x2, fsin x2 | - |
| 00423000 | CCarCtrl::FlyAIPlaneInCertainDirection | CarCtrl.cpp | fpatan x1 | asin x1 |
| 00423940 | CCarCtrl::FlyAIHeliToTarget_FixedOrientation | CarCtrl.cpp | fcos x1, fsin x1 | - |
| 00429A70 | CCarCtrl::FlyAIHeliInCertainDirection | CarCtrl.cpp | fcos x1, fsin x1 | - |
| 0042CE40 | CCarCtrl::ScanForPedDanger | CarCtrl.cpp | - | floor x8 |
| 0042EC90 | CCarCtrl::DragCarToPoint | CarCtrl.cpp | fpatan x1, fcos x1, fsin x1 | - |
| 004325C0 | CCarCtrl::FindAngleToWeaveThroughTraffic | CarCtrl.cpp | - | floor x8 |
| 00434400 | CCarCtrl::FindMaximumSpeedForThisCarInTraffic | CarCtrl.cpp | - | floor x8 |
| 0043C880 | CCurves::CalcCorrectedDist | Curves.cpp | fcos x1, fsin x1 | - |
| 0043EAF0 | CEntryExit::FindValidTeleportPoint | EntryExit.cpp | fcos x2, fsin x2 | - |
| 00441DB0 | None::GetHeading | Entity/Placeable.cpp | fpatan x1 | - |
| 00442980 | CGameLogic::ResetStuffUponResurrection | GameLogic.cpp | fpatan x1 | - |
| 00442AD0 | CGameLogic::Update | GameLogic.cpp | fpatan x3 | - |
| 004453D0 | CGangWars::CreateDefendingGroup | GangWars.cpp | fsin x1, fcos x1 | - |
| 004479F0 | CGarage::BuildRotatedDoorMatrix | Garage.cpp | fsin x1, fcos x1 | - |
| 0044FEE0 | CPathFind::FindNodePairClosestToCoors | PathFind.cpp | fpatan x1 | - |
| 00450320 | CPathFind::FindNodeOrientationForCarPlacement | PathFind.cpp | fpatan x1 | - |
| 00450780 | CPathFind::FindNodeCoorsForScript | PathFind.cpp | fpatan x1 | - |
| 00451B70 | CPathFind::FindNextNodeWandering | PathFind.cpp | fsin x1, fcos x1 | - |
| 00452F40 | CPathFind::LoadPathFindData | PathFind.cpp | - | floor x8 |
| 00454E80 | CPickups::DoMoneyEffects | Pickups.cpp | fcos x1, fsin x1 | - |
| 00455720 | CPickups::DoPickUpEffects | Pickups.cpp | fsin x2, fcos x1 | - |
| 00455E20 | CPickups::DoCollectableEffects | Pickups.cpp | fsin x2, fcos x1 | - |
| 004560E0 | CPickups::DoMineEffects | Pickups.cpp | fsin x2, fcos x1 | - |
| 00458970 | CPickups::CreateSomeMoney | Pickups.cpp | fsin x1, fcos x1 | - |
| 00458A80 | CPickups::CreatePickupCoorsCloseToCoors | Pickups.cpp | fsin x1, fcos x1 | - |
| 0045D760 | CReplay::ProcessLookAroundCam | Replay.cpp | fpatan x1, fcos x2, fsin x2 | - |
| 00464980 | CTheScripts::DrawScriptSpritesAndRectangles | Scripts/TheScripts.cpp | fcos x1, fsin x1 | - |
| 0046FF20 | CScriptsForBrains::CheckIfNewEntityNeedsScript | ScriptsForBrains.cpp | - | pow x2 |
| 00470A20 | CTheScripts::ScriptConnectLodsFunction | Scripts/TheScripts.cpp | - | pow x1 |
| 004748F0 | None::Destructor | Tasks/TaskTypes/TaskSimpleSetCharIgnoreWeaponRangeFlag.cpp | fsin x1, fcos x1 | - |
| 00479D60 | CTheCarGenerators::Get | TheCarGenerators.cpp | fcos x2, fsin x2 | - |
| 00485C20 | CTheScripts::ScriptDebugCircle2D | Scripts/TheScripts.cpp | fsin x2, fcos x2 | - |
| 00487F60 | CRunningScript::CharInAngledAreaCheckCommand | Scripts/RunningScript.cpp | fsin x1, fcos x1 | - |
| 004883F0 | CRunningScript::ObjectInAngledAreaCheckCommand | Scripts/RunningScript.cpp | fsin x1, fcos x1 | - |
| 00488780 | CRunningScript::FlameInAngledAreaCheckCommand | Scripts/RunningScript.cpp | fsin x1, fcos x1 | - |
| 00489490 | CRunningScript::ThisIsAValidRandomPed | Scripts/RunningScript.cpp | - | acos x2 |
| 0048EA60 | None::Destructor | Tasks/TaskTypes/TaskSimpleHandsUp.cpp | fsin x2, fcos x2 | acos x1 |
| 004939F0 | CTheScripts::ProcessAllSearchLights | Scripts/TheScripts.cpp | fpatan x2 | - |
| 004A55E0 | None::GetValue | Fx/Blueprints/FxInfoFriction.cpp | - | pow x1 |
| 004A9140 | FxManager_c::CalcFrustumInfo | Fx/FxManager.cpp | fpatan x1, fsin x2 | - |
| 004ABAC0 | CEventSource::ComputeEventSourceType | Events/EventSource.cpp | fyl2x x2 | - |
| 004AC050 | None::CalcSoundLevelIncrement | Events/Event.cpp | fyl2x x1 | pow x2 |
| 004B2850 | None::GetSoundLevel | Events/Event.cpp | fyl2x x1 | - |
| 004B4AC0 | None::SetPedSafePosition | Events/EventKnockOffBike.cpp | fpatan x2 | - |
| 004C9800 | CLinkedUpgradeList::SetEnvMapCoeff | Models/VehicleModelInfo.cpp | - | floor x1 |
| 004D00E0 | CAnimBlendNode::CalcThetaFromQuats | Animation/AnimBlendNode.cpp | fsin x1 | acos x1 |
| 004D6E60 | CAEAmbienceTrackManager::UpdateAmbienceTrackAndVolume | Audio/Managers/AEAmbienceTrackManager.cpp | fyl2x x1 | - |
| 004D7F20 | CAEAudioEnvironment::GetDistanceAttenuation | Audio/AEAudioEnvironment.cpp | - | floor x1 |
| 004D8990 | CAEAudioHardware::RescaleChannelVolumes | Audio/Hardware/AEAudioHardware.cpp | - | pow x1 |
| 004D9CC0 | CAEAudioUtility::GetBankAndSoundFromScriptSlotAudioEvent | Audio/AEAudioUtility.cpp | - | floor x1 |
| 004D9E50 | CAEAudioUtility::AudioLog10 | Audio/AEAudioUtility.cpp | fyl2x x1 | - |
| 004D9EF0 | CAEAudioUtility::ConvertFromBytesToMS | Audio/AEAudioUtility.cpp | - | floor x1 |
| 004D9F40 | CAEAudioUtility::ConvertFromMSToBytes | Audio/AEAudioUtility.cpp | - | floor x1 |
| 004DA320 | None::Reset | Audio/Entities/AECollisionAudioEntity.cpp | fyl2x x1 | - |
| 004DA540 | None::UpdateLoopingCollisionSound | Audio/Entities/AECollisionAudioEntity.cpp | fyl2x x1 | - |
| 004DAE40 | None::ReportWaterSplash | Audio/Entities/AECollisionAudioEntity.cpp | fyl2x x2 | - |
| 004DB150 | None::PlayOneShotCollisionSound | Audio/Entities/AECollisionAudioEntity.cpp | fyl2x x1 | floor x2 |
| 004DB450 | None::PlayLoopingCollisionSound | Audio/Entities/AECollisionAudioEntity.cpp | fyl2x x1 | - |
| 004DF210 | None::ServiceAmbientGunFire | Audio/Entities/AEGlobalWeaponAudioEntity.cpp | fcos x1, fsin x1 | - |
| 004E0EE0 | CAEPedAudioEntity::UpdateJetPack | Audio/Entities/AEPedAudioEntity.cpp | fsin x1 | - |
| 004EA010 | CAERadioTrackManager::UpdateRadioVolumes | Audio/Managers/AERadioTrackManager.cpp | fyl2x x2 | - |
| 004EED10 | CAESmoothFadeThread::Service | Audio/AESmoothFadeThread.cpp | fyl2x x1 | pow x1 |
| 004F8070 | None::GetFreqForPlayerEngineSound | Audio/Entities/AEVehicleAudioEntity.cpp | fsin x1 | - |
| 004FFDC0 | None::ProcessDummyBicycle | Audio/Entities/AEVehicleAudioEntity.cpp | fsin x1 | - |
| 00500040 | None::ProcessPlayerBicycle | Audio/Entities/AEVehicleAudioEntity.cpp | fsin x1 | - |
| 00503CE0 | None::PlayGunSounds | Audio/Entities/AEWeaponAudioEntity.cpp | fyl2x x2 | - |
| 005052F0 | None::Service | Audio/Entities/AEWeatherAudioEntity.cpp | fyl2x x1 | - |
| 00505A00 | None::UpdateParameters | Audio/Entities/AEWeatherAudioEntity.cpp | fyl2x x5 | pow x1 |
| 00506800 | None::AddAudioEvent | Audio/Entities/AEWeatherAudioEntity.cpp | fyl2x x1 | - |
| 005083C0 | CAudioZones::Update | Audio/AudioZones.cpp | fptan x2 | modf x1, asin x1 |
| 0050A0A0 | CCam::LimitPrecision | Cam.cpp | - | pow x2, modf x1 |
| 0050A4F0 | CCam::RotCamIfInFrontCar | Cam.cpp | fpatan x1, fcos x1, fsin x1 | - |
| 0050AD40 | None::Find3rdPersonQuickAimPitch | Camera.cpp | fptan x1, fpatan x1 | - |
| 0050D350 | None::ProcessVectorTrackLinear | Camera.cpp | fsin x1 | - |
| 0050D430 | None::ProcessVectorMoveLinear | Camera.cpp | fsin x1 | - |
| 0050D510 | None::ProcessFOVLerp | Camera.cpp | fsin x1 | - |
| 0050DD70 | CCam::Finalise_DW_CineyCams | Cam.cpp | fsin x3, fcos x2 | - |
| 0050E760 | CIdleCam::FinaliseIdleCamera | IdleCam.cpp | fcos x3, fsin x3 | - |
| 0050EB70 | CCam::Process_1rstPersonPedOnPC | Cam.cpp | fcos x2, fsin x2, fpatan x1 | - |
| 0050F970 | CCam::Process_FollowPedWithMouse | Cam.cpp | fpatan x2, fcos x4, fsin x2, fptan x2 | pow x1 |
| 005105C0 | CCam::Process_M16_1stPerson | Cam.cpp | fcos x7, fpatan x2, fsin x6 | pow x3, asin x4 |
| 00511B50 | CCam::Process_Rocket | Cam.cpp | fcos x2, fsin x2 | - |
| 00512110 | CCam::Process_WheelCam | Cam.cpp | fcos x2, fsin x1 | - |
| 00512B10 | CCam::Process_AttachedCam | Cam.cpp | fsin x1, fcos x1 | - |
| 00512EF0 | CCam::ArrestCamLookAtCopHead | Cam.cpp | fcos x2, fsin x2 | - |
| 00513510 | CCam::Process_Cam_TwoPlayer_Separate_Cars | Cam.cpp | fsin x1, fcos x1 | - |
| 00513BE0 | CCam::Process_Cam_TwoPlayer_Separate_Cars_TopDown | Cam.cpp | fsin x1, fcos x1 | - |
| 00513E40 | CCam::Get_TwoPlayer_AimVector | Cam.cpp | fptan x1 | - |
| 00514030 | CCamera::AvoidTheGeometry | Cam.cpp | fcos x2, fsin x2, fptan x1 | pow x1 |
| 00514970 | None::Find3rdPersonCamTargetVector | Camera.cpp | fptan x1 | - |
| 00514D60 | None::CalculateFrustumPlanes | Camera.cpp | fcos x1, fsin x1 | - |
| 005150E0 | None::CalculateDerivedValues | Camera.cpp | fpatan x1 | - |
| 00515200 | None::StartTransition | Camera.cpp | fpatan x1 | - |
| 00516560 | None::ProcessJiggle | Camera.cpp | fsin x1, fcos x1 | - |
| 00516B20 | None::ImproveNearClip | Camera.cpp | fsin x1 | - |
| 00517130 | CCam::GetCoreDataForDWCineyCamMode | Cam.cpp | fcos x2, fsin x2 | - |
| 00517500 | CCam::Process_SpecialFixedForSyphon | Cam.cpp | fpatan x1 | - |
| 005179E0 | CIdleCam::ProcessSlerp | IdleCam.cpp | fsin x1 | - |
| 00517BF0 | CIdleCam::ProcessFOVZoom | IdleCam.cpp | fsin x1 | - |
| 00519250 | CCam::ProcessPedsDeadBaby | Cam.cpp | fsin x1, fcos x1 | - |
| 00519810 | CCam::Process_Cam_TwoPlayer_InCarAndShooting | Cam.cpp | fpatan x4, fsin x2, fcos x1, fptan x2 | pow x4 |
| 0051A740 | CCam::Process_DW_HeliChaseCam | Cam.cpp | fsin x3 | - |
| 0051B120 | CCam::Process_DW_CamManCam | Cam.cpp | fsin x2 | - |
| 0051B850 | CCam::Process_DW_BirdyCam | Cam.cpp | fsin x1 | - |
| 0051C250 | CCam::Process_DW_PlaneSpotterCam | Cam.cpp | fsin x2 | - |
| 0051C760 | CCam::Process_DW_PlaneCam1 | Cam.cpp | fsin x1 | - |
| 0051CC30 | CCam::Process_DW_PlaneCam2 | Cam.cpp | fsin x1 | - |
| 0051E560 | None::TryToStartNewCamMode | Camera.cpp | fcos x4, fsin x4 | - |
| 00520690 | CCam::LookBehind | Cam.cpp | fcos x1, fsin x1 | - |
| 00520E40 | CCam::LookRight | Cam.cpp | fcos x1, fsin x1 | - |
| 00521500 | CCam::Process_AimWeapon | Cam.cpp | fptan x1, fpatan x6, fcos x5, fsin x2 | pow x3, asin x3, acos x1 |
| 00522D40 | CCam::Process_FollowPed_SA | Cam.cpp | fpatan x3, fcos x6, fsin x6 | asin x5, pow x3 |
| 005245B0 | CCam::Process_FollowCar_SA | Cam.cpp | fpatan x7, fcos x10, fsin x7 | pow x3, asin x2 |
| 00525E50 | CCam::Process_Cam_TwoPlayer | Cam.cpp | fpatan x1 | pow x1 |
| 00526FC0 | CCam::Process | Cam.cpp | - | pow x3 |
| 00527FA0 | None::CamControl | Camera.cpp | fcos x1, fsin x1, fpatan x1 | - |
| 0052B730 | None::Process | Camera.cpp | fcos x8, fsin x4 | - |
| 00533050 | None::LivesInThisNonOverlapSector | Entity/Entity.cpp | - | floor x6 |
| 00533790 | None::CreateEffects | Entity/Entity.cpp | fpatan x2 | - |
| 005347D0 | None::Add | Entity/Entity.cpp | - | floor x4 |
| 00534AE0 | None::Remove | Entity/Entity.cpp | - | floor x4 |
| 00534E90 | None::ModifyMatrixForTreeInWind | Entity/Entity.cpp | fsin x2 | - |
| 00538090 | CFileLoader::LoadObjectInstance | FileLoader.cpp | - | acos x2 |
| 00538A50 | CFileMgr::GetErrorReadWrite | FileMgr.cpp | - | ceil x2 |
| 0053AF00 | CFireManager::Update | FireManager.cpp | fsin x2, fcos x2 | ceil x1 |
| 0053CBE0 | None::GetRadianAngleBetweenPoints | General.cpp | fpatan x1 | - |
| 0053CC70 | None::GetATanOfXY | General.cpp | fpatan x8 | - |
| 0053CDC0 | None::GetNodeHeadingFromVector | General.cpp | - | floor x1 |
| 0053D690 | None::DoRWStuffStartOfFrame | source/app/app.cpp | fptan x1 | - |
| 0053D7A0 | None::DoRWStuffStartOfFrame_Horizon | source/app/app.cpp | fptan x1 | - |
| 0053E770 | None::FrontendIdle | source/app/app_game.cpp | fptan x1 | - |
| 0053E920 | None::Idle | source/app/app_game.cpp | fptan x1 | - |
| 0053EC10 | None::AppEventHandler | source/app/app.cpp | fptan x1 | - |
| 00542560 | None::RemoveAndAdd | Entity/Physical.cpp | - | floor x4 |
| 00544A30 | None::Add | Entity/Physical.cpp | - | floor x4 |
| 00544C40 | None::ApplyAirResistance | Entity/Physical.cpp | - | pow x2 |
| 00546FF0 | None::PositionAttachedEntity | Entity/Physical.cpp | fpatan x2 | - |
| 00547B80 | None::ApplySpeed | Entity/Physical.cpp | fpatan x1 | - |
| 005483D0 | None::ApplyFriction | Entity/Physical.cpp | - | pow x2 |
| 0054D920 | None::CheckCollision | Entity/Physical.cpp | - | floor x4 |
| 0054DB10 | None::ProcessShift | Entity/Physical.cpp | - | pow x2, floor x8 |
| 0054EB50 | CMatrixLinkList::GetNumUsed2 | Core/MatrixLinkList.cpp | fcos x8, fsin x8 | - |
| 0054F010 | None::FreeStaticMatrix | Entity/Placeable.cpp | fpatan x1 | - |
| 0054F3B0 | None::RemoveMatrix | Entity/Placeable.cpp | fpatan x1 | - |
| 0054F610 | None::SetMatrix | Entity/Placeable.cpp | fpatan x1 | - |
| 00554840 | CRenderer::ScanSectorList | Renderer.cpp | fpatan x1 | - |
| 00554B10 | CRenderer::ScanBigBuildingList | Renderer.cpp | fpatan x1 | - |
| 005556E0 | CRenderer::ConstructRenderList | Renderer.cpp | fpatan x1 | - |
| 00557530 | CRope::Update | Rope.cpp | - | pow x1 |
| 0055C780 | CStats::UpdateStatsWhenCycling | Stats.cpp | - | ceil x1 |
| 005603D0 | CTimeCycle::CalcColoursForPoint | TimeCycle.cpp | fsin x1, fcos x1 | - |
| 00564500 | CWorld::ProcessVerticalLineSector | World.cpp | - | floor x8 |
| 00564A20 | CWorld::FindObjectsInRange | World.cpp | - | floor x4 |
| 00564C70 | CWorld::FindObjectsOfTypeInRange | World.cpp | - | floor x4 |
| 00566A60 | CWorld::CallOffChaseForArea | World.cpp | - | floor x4 |
| 005674E0 | CWorld::ProcessVerticalLine | World.cpp | - | floor x2 |
| 00567620 | CWorld::ProcessVerticalLine_FillGlobeColPoints | World.cpp | - | floor x2 |
| 005684A0 | CWorld::Process | World.cpp | - | pow x1 |
| 00568B80 | CWorld::FindObjectsKindaColliding | World.cpp | - | floor x4 |
| 00568DD0 | CWorld::FindObjectsIntersectingCube | World.cpp | - | floor x4 |
| 00568FF0 | CWorld::FindObjectsIntersectingAngledCollisionBox | World.cpp | - | floor x4 |
| 00569240 | CWorld::FindMissionEntitiesIntersectingCube | World.cpp | - | floor x4 |
| 005693F0 | CWorld::FindNearestObjectOfType | World.cpp | - | floor x4 |
| 00569E20 | CWorld::TestSphereAgainstWorld | World.cpp | - | floor x4 |
| 0056A490 | CWorld::GetIsLineOfSightClear | World.cpp | - | floor x16 |
| 0056B790 | CWorld::TriggerExplosion | World.cpp | - | floor x4 |
| 0056BA00 | CWorld::ProcessLineOfSight | World.cpp | - | floor x16 |
| 0056DBD0 | CPlayerInfo::FindObjectToSteal | PlayerInfo.cpp | - | floor x8 |
| 0056E450 | CWorld::FindPlayerHeading | World.cpp | fpatan x3 | - |
| 0056EC80 | CPlayerInfo::ProcessCarGunCrosshair_Hook | PlayerInfo.cpp | fptan x2 | - |
| 0056EF90 | CPlayerInfo::DrawCrosshair_Hook | PlayerInfo.cpp | fsin x2, fcos x1 | - |
| 00583670 | CRadar::CalculateCachedSinCos | Radar.cpp | fpatan x2, fsin x2, fcos x2 | - |
| 00584850 | CRadar::DrawRotatingRadarSprite | Radar.cpp | fsin x1, fcos x1 | - |
| 00584960 | CRadar::DrawYouAreHereSprite | Radar.cpp | fsin x1, fcos x1 | - |
| 00585700 | CRadar::DrawRadarMask | Radar.cpp | fcos x1, fsin x1 | - |
| 005858D0 | CRadar::StreamRadarSections | Radar.cpp | - | floor x1, ceil x1 |
| 00586650 | CRadar::DrawRadarGangOverlay | Radar.cpp | fsin x2 | - |
| 00586880 | CRadar::DrawRadarMap | Radar.cpp | fsin x1, fcos x1, fpatan x1 | floor x1, ceil x1 |
| 00587000 | CRadar::DrawEntityBlip | Radar.cpp | fcos x1, fsin x1 | - |
| 0058A330 | CHud::DrawRadar | Hud.cpp | fpatan x1, fsin x1, fcos x1 | - |
| 0059A840 | CMatrix::ConvertToEulerAngles | Core/Matrix.cpp | fpatan x9 | - |
| 0059AA40 | CMatrix::ConvertFromEulerAngles | Core/Matrix.cpp | fcos x3, fsin x3 | - |
| 0059AC90 | CMaths::InitMathsTables | Maths.cpp | fsin x1 | - |
| 0059AFA0 | CMatrix::SetRotateXOnly | Core/Matrix.cpp | fcos x1, fsin x1 | - |
| 0059AFE0 | CMatrix::SetRotateYOnly | Core/Matrix.cpp | fcos x1, fsin x1 | - |
| 0059B020 | CMatrix::SetRotateZOnly | Core/Matrix.cpp | fcos x1, fsin x1 | - |
| 0059B060 | CMatrix::SetRotateX | Core/Matrix.cpp | fcos x1, fsin x1 | - |
| 0059B0A0 | CMatrix::SetRotateY | Core/Matrix.cpp | fcos x1, fsin x1 | - |
| 0059B0E0 | CMatrix::SetRotateZ | Core/Matrix.cpp | fcos x1, fsin x1 | - |
| 0059B120 | CMatrix::SetRotate | Core/Matrix.cpp | fcos x3, fsin x3 | - |
| 0059B1E0 | CMatrix::RotateX | Core/Matrix.cpp | fcos x1, fsin x1 | - |
| 0059B2C0 | CMatrix::RotateY | Core/Matrix.cpp | fcos x1, fsin x1 | - |
| 0059B390 | CMatrix::RotateZ | Core/Matrix.cpp | fcos x1, fsin x1 | - |
| 0059B460 | CMatrix::Rotate | Core/Matrix.cpp | fcos x3, fsin x3 | - |
| 0059C160 | CQuaternion::Get | Core/Quaternion.cpp | fpatan x3, fsin x1, fcos x1 | - |
| 0059C230 | CQuaternion::Get | Core/Quaternion.cpp | fsin x1 | acos x1 |
| 0059C300 | CQuaternion::Slerp | Core/Quaternion.cpp | fsin x4 | - |
| 0059C530 | CQuaternion::Set | Core/Quaternion.cpp | fcos x3, fsin x3 | - |
| 0059C600 | CQuaternion::Set | Core/Quaternion.cpp | fsin x1, fcos x1 | - |
| 0059F200 | None::SetMatrixForTrainCrossing | Entity/Object/Object.cpp | fcos x1, fsin x1 | - |
| 005A02E0 | None::SpecialEntityCalcCollisionSteps | Entity/Object/Object.cpp | - | ceil x2 |
| 005A0760 | None::SetIsStatic | Entity/Object/Object.cpp | fpatan x1 | - |
| 005A0B50 | None::ProcessTrainCrossingBehaviour | Entity/Object/Object.cpp | - | acos x1 |
| 005A2130 | None::ProcessControl | Entity/Object/Object.cpp | fpatan x1 | pow x2 |
| 005A32D0 | ProcSurfaceInfo_c::AddObject | Plant/ProcSurfaceInfo.cpp | fcos x1, fsin x1 | - |
| 005B25F0 | CCam::Process_FlyBy | Cam.cpp | fcos x2, fsin x2 | - |
| 005B97F0 | CAEAudioUtility::StaticInitialise | Audio/AEAudioUtility.cpp | fyl2x x1 | pow x1 |
| 005BBAC0 | CTimeCycle::Initialise | TimeCycle.cpp | fcos x2, fsin x2 | - |
| 005BE670 | CWeaponInfo::LoadWeaponData | WeaponInfo.cpp | - | floor x2 |
| 005DAD00 | CGrassRenderer::DrawTriPlants | GrassRenderer.cpp | - | floor x1 |
| 005DB3D0 | CPlantMgr::CalculateWindBending | PlantMgr.cpp | fsin x2 | - |
| 005DC510 | CPlantMgr::_ColEntityCache_Update | PlantMgr.cpp | - | floor x8 |
| 005DEF60 | None::GetLocalDirection | Entity/Ped/Ped.cpp | fpatan x1 | - |
| 005DF910 | None::SetPedPositionInCar | Entity/Ped/Ped.cpp | fpatan x3 | - |
| 005DFDF0 | None::PositionAttachedPed | Entity/Ped/Ped.cpp | fpatan x1 | - |
| 005E0820 | None::PositionPedOutOfCollision | Entity/Ped/Ped.cpp | fpatan x1 | - |
| 005E1FA0 | None::ProcessBuoyancy | Entity/Ped/Ped.cpp | - | pow x1 |
| 005E2530 | None::ProcessEntityCollision | Entity/Ped/Ped.cpp | fpatan x1 | - |
| 005E3E90 | None::SpecialEntityCalcCollisionSteps | Entity/Ped/Ped.cpp | - | ceil x4 |
| 005E4C50 | None::CalculateNewVelocity | Entity/Ped/Ped.cpp | - | pow x2, asin x2 |
| 005E57F0 | None::PlayFootSteps | Entity/Ped/Ped.cpp | fyl2x x2 | - |
| 005E65A0 | None::PreRenderAfterTest | Entity/Ped/Ped.cpp | fsin x1 | - |
| 005E8CD0 | None::ProcessControl | Entity/Ped/Ped.cpp | - | pow x1 |
| 005EF420 | None::GetDisplacement | Attractors/PedShelterAttractor.cpp | fcos x1, fsin x1 | - |
| 005F13F0 | CPedGeometryAnalyser::ComputePedShotSide | PedGeometryAnalyser.cpp | fpatan x1 | - |
| 005F1500 | CPedGeometryAnalyser::ComputeEntityDirs | PedGeometryAnalyser.cpp | fsin x2, fcos x2 | - |
| 005F2F70 | CPedGeometryAnalyser::IsWanderPathClear | PedGeometryAnalyser.cpp | - | floor x1 |
| 005FCE80 | CPedGroupPlacer::PlaceChatGroup | PedGroupPlacer.cpp | fcos x2, fsin x1 | - |
| 005FD330 | CPedGroupPlacer::PlaceRandomGroup | PedGroupPlacer.cpp | fcos x2, fsin x1 | - |
| 005FD8F0 | CPedIK::GetWorldMatrix | PedIK.cpp | - | acos x2 |
| 005FDC00 | CPedIK::PointGunInDirection | PedIK.cpp | fpatan x1, fcos x2, fsin x2 | - |
| 005FDF90 | CPedIK::RotateTorsoForArm | PedIK.cpp | fpatan x1 | - |
| 005FE0E0 | CPedIK::PitchForSlope | PedIK.cpp | fsin x3, fcos x3, fpatan x2 | asin x2 |
| 005FFA20 | None::ScanForEntitiesInRange | EntityScanner.cpp | - | floor x8 |
| 00604500 | CCollisionEventScanner::ScanForCollisionEvents | Collision/CollisionEventScanner.cpp | fpatan x2 | - |
| 00605A30 | CInterestingEvents::ScanForNearbyEntities | InterestingEvents.cpp | - | floor x8 |
| 006060A0 | CAttractorScanner::ScanForAttractors | AttractorScanner.cpp | - | floor x8 |
| 0060BA80 | None::DrawTriangleForMouseRecruitPed | Entity/Ped/PlayerPed.cpp | fcos x1, fsin x1 | - |
| 0060DC50 | None::FindWeaponLockOnTarget | Entity/Ped/PlayerPed.cpp | fpatan x2 | - |
| 0060EA90 | None::ProcessControl | Entity/Ped/PlayerPed.cpp | fpatan x1 | - |
| 006123A0 | CPopulation::TestSafeForRealObject | Population.cpp | - | floor x8 |
| 006133F0 | CPopulation::CreateWaitingCoppers | Population.cpp | fsin x1, fcos x1, fpatan x1 | - |
| 00617080 | BoneNode_c::QuatToEuler | Ragdoll/BoneNode.cpp | fpatan x5 | - |
| 006171F0 | BoneNode_c::EulerToQuat | Ragdoll/BoneNode.cpp | fcos x3, fsin x3 | - |
| 006178B0 | IKChain_c::MoveBonesToTarget | Ragdoll/IKChain.cpp | - | acos x1 |
| 0061D6F0 | None::IsTargetInRange | Tasks/TaskTypes/TaskSimpleFight.cpp | fpatan x1 | - |
| 0061F9F0 | None::ProcessPed | Tasks/TaskTypes/TaskSimpleThrowControl.cpp | fpatan x1 | - |
| 00620490 | None::ProcessPed | Tasks/TaskTypes/TaskSimpleChoking.cpp | fpatan x1 | - |
| 00620A20 | None::ProcessPed | Tasks/TaskTypes/TaskSimpleBeHit.cpp | fpatan x1 | - |
| 00621960 | None::ProcessPlayerPed | Tasks/TaskTypes/TaskSimpleGangDriveBy.cpp | fpatan x2 | - |
| 00624710 | None::ChooseAttackPlayer | Tasks/TaskTypes/TaskSimpleFight.cpp | fpatan x1 | - |
| 00624B50 | None::CalcMoveCommand | Tasks/TaskTypes/TaskSimpleFightingControl.cpp | fpatan x1 | - |
| 00625270 | None::ProcessPed | Tasks/TaskTypes/TaskSimpleGunControl.cpp | fpatan x1 | - |
| 00627600 | None::ProcessAIPed | Tasks/TaskTypes/TaskSimpleGangDriveBy.cpp | fpatan x2 | - |
| 00628350 | None::ProcessAimIK | Tasks/TaskTypes/TaskSimpleGangDriveBy.cpp | fpatan x2, fsin x1, fcos x1 | asin x1 |
| 006289F0 | None::CalculateSearchPositionAndRanges | Tasks/TaskTypes/TaskComplexDestroyCarMelee.cpp | fpatan x1 | - |
| 00629920 | None::ProcessPed | Tasks/TaskTypes/TaskSimpleFight.cpp | fpatan x2 | - |
| 0062A380 | None::ProcessPed | Tasks/TaskTypes/TaskSimpleUseGun.cpp | fpatan x2 | pow x1 |
| 0062E540 | None::ProcessPed | Tasks/TaskTypes/TaskSimpleStealthKill.cpp | fpatan x3 | - |
| 00630600 | None::ProcessPed | Tasks/TaskTypes/TaskSimpleDead.cpp | - | asin x2 |
| 0063A380 | None::GetCameraStickModifier | Tasks/TaskTypes/TaskComplexEnterCar.cpp | fpatan x2 | - |
| 006428C0 | None::ProcessHeadBopping | Tasks/TaskTypes/TaskSimpleCarDrive.cpp | fsin x1 | - |
| 006479B0 | None::PositionPedOutOfCollision | Tasks/TaskTypes/TaskSimpleCarSetPedOut.cpp | fpatan x1 | - |
| 0064C220 | None::ProcessPed | Tasks/TaskTypes/TaskSimpleCarSetPedSlowDraggedOut.cpp | fpatan x1 | - |
| 0064DD60 | None::ProcessPed | Tasks/TaskTypes/TaskSimpleCarJumpOut.cpp | - | pow x1 |
| 0064F240 | CCarEnterExit::IsPlayerToQuitCarEnter | CarEnterExit.cpp | fpatan x2 | - |
| 006513A0 | CTaskUtilityLineUpPedWithCar::ProcessPed | Tasks/TaskTypes/TaskUtilityLineUpPedWithCar.cpp | fpatan x4, fcos x1, fsin x1 | - |
| 006528F0 | CCarEnterExit::GetNearestCarDoor | CarEnterExit.cpp | fpatan x2 | - |
| 0065B820 | None::ComputeTargetPoint | Tasks/TaskTypes/TaskComplexFleePoint.cpp | fcos x1, fsin x1 | - |
| 00664770 | None::CalcTargetPos | Tasks/TaskTypes/TaskGoToVehicleAndLean.cpp | fcos x1, fsin x1 | - |
| 00667F20 | None::SetUpIK | Tasks/TaskTypes/TaskSimpleAchieveHeading.cpp | fsin x1, fcos x1 | - |
| 0066E710 | None::CalcBlendRatio | Tasks/TaskTypes/TaskComplexFollowPointRoute.cpp | fcos x1 | - |
| 0066EDC0 | None::CalcBlendRatio | Tasks/TaskTypes/TaskComplexFollowNodeRoute.cpp | fcos x1 | - |
| 00679B80 | None::Launch | Tasks/TaskTypes/TaskSimpleJump.cpp | fsin x1, fcos x2 | - |
| 0067A390 | None::GetCameraTargetPos | Tasks/TaskTypes/TaskSimpleClimb.cpp | fpatan x1, fsin x1, fcos x1 | - |
| 0067A5D0 | None::GetCameraStickModifier | Tasks/TaskTypes/TaskSimpleClimb.cpp | fpatan x2, fsin x1, fcos x1 | - |
| 0067DBE0 | None::StartAnim | Tasks/TaskTypes/TaskSimpleClimb.cpp | fpatan x2 | - |
| 0067E7B0 | None::ProcessControlInput | Tasks/TaskTypes/TaskSimpleJetPack.cpp | fpatan x1 | pow x2 |
| 0067EF20 | None::ProcessThrust | Tasks/TaskTypes/TaskSimpleJetPack.cpp | fcos x1, fsin x3 | pow x2 |
| 0067F6A0 | None::RenderJetPack | Tasks/TaskTypes/TaskSimpleJetPack.cpp | fsin x2, fcos x2 | - |
| 0067FD30 | None::ScanToGrab | Tasks/TaskTypes/TaskSimpleClimb.cpp | fpatan x1 | floor x4 |
| 006801F0 | None::ProcessPed | Tasks/TaskTypes/TaskSimpleJetPack.cpp | fsin x1 | - |
| 006803A0 | None::TestForClimb | Tasks/TaskTypes/TaskSimpleClimb.cpp | fpatan x1, fsin x1, fcos x1 | - |
| 006804D0 | None::TestForVault | Tasks/TaskTypes/TaskSimpleClimb.cpp | fsin x1, fcos x1 | - |
| 00680570 | None::TestForStandUp | Tasks/TaskTypes/TaskSimpleClimb.cpp | fsin x1, fcos x1 | - |
| 00680DC0 | None::ProcessPed | Tasks/TaskTypes/TaskSimpleClimb.cpp | fpatan x1, fsin x1, fcos x1 | - |
| 006859A0 | None::ProcessPlayerWeapon | Tasks/TaskTypes/TaskSimplePlayerOnFoot.cpp | fsin x1, fpatan x1 | - |
| 00687530 | None::PlayerControlFighter | Tasks/TaskTypes/TaskSimplePlayerOnFoot.cpp | fsin x2, fcos x2 | - |
| 00687C20 | None::PlayerControlZeldaWeapon | Tasks/TaskTypes/TaskSimplePlayerOnFoot.cpp | fsin x1, fcos x1, fpatan x1 | - |
| 00687F30 | None::PlayerControlDucked | Tasks/TaskTypes/TaskSimplePlayerOnFoot.cpp | fsin x2, fcos x2, fpatan x1 | - |
| 006883D0 | None::PlayerControlZelda | Tasks/TaskTypes/TaskSimplePlayerOnFoot.cpp | fsin x1, fcos x1 | - |
| 00688A90 | None::ProcessControlInput | Tasks/TaskTypes/TaskSimpleSwim.cpp | fpatan x3, fsin x1, fcos x1 | asin x1, pow x3 |
| 00689640 | None::ProcessControlAI | Tasks/TaskTypes/TaskSimpleSwim.cpp | fpatan x1 | - |
| 0068A1D0 | None::ProcessSwimmingResistance | Tasks/TaskTypes/TaskSimpleSwim.cpp | fcos x1, fsin x2 | pow x1 |
| 0068AA70 | None::ProcessEffects | Tasks/TaskTypes/TaskSimpleSwim.cpp | fsin x1, fcos x1 | - |
| 00691AE0 | None::CreateNextSubTask | Tasks/TaskTypes/TaskComplexGoPickUpEntity.cpp | fpatan x1 | - |
| 00691D50 | None::ControlSubTask | Tasks/TaskTypes/TaskComplexGoPickUpEntity.cpp | fpatan x1 | - |
| 00693610 | None::CreateFirstSubTask | Tasks/TaskTypes/TaskComplexGoPickUpEntity.cpp | fpatan x3 | - |
| 00693C40 | None::ProcessPed | Tasks/TaskTypes/TaskSimpleHoldEntity.cpp | fpatan x2 | - |
| 006946F0 | None::ComputeEntitySeekPos | Tasks/TaskTypes/SeekEntity/PosCalculators/EntitySeekPosCalculatorRadiusAngleOffset.cpp | fsin x2, fcos x2 | - |
| 00695820 | None::Clone | Tasks/TaskTypes/TaskComplexSeekEntityAiming.cpp | fsin x2, fcos x2 | - |
| 00698D40 | CCover::FindDirFromVector | Cover.cpp | fpatan x1 | - |
| 00698D60 | CCover::FindVectorFromDir | Cover.cpp | fsin x1, fcos x1 | - |
| 00699120 | CCover::FindCoverPointsForThisBuilding | Cover.cpp | fpatan x1 | - |
| 006992B0 | CCover::FindAndReserveCoverPoint | Cover.cpp | fsin x1, fcos x1 | - |
| 006997E0 | CCover::Update | Cover.cpp | - | floor x8 |
| 0069A620 | CFormation::GenerateGatherDestinations | Formation.cpp | fsin x1, fcos x1 | - |
| 0069B700 | CFormation::DistributeDestinations_PedsToAttack | Formation.cpp | - | ceil x1 |
| 006A07A0 | None::HydraulicControl | Entity/Vehicle/Automobile.cpp | fpatan x1, fcos x1, fsin x1 | - |
| 006A47F0 | None::DoBurstAndSoftGroundRatios | Entity/Vehicle/Automobile.cpp | - | floor x2 |
| 006A6010 | None::GetCarRoll | Entity/Vehicle/Automobile.cpp | fpatan x1 | - |
| 006A6050 | None::GetCarPitch | Entity/Vehicle/Automobile.cpp | fpatan x1 | - |
| 006A8C00 | None::ProcessBuoyancy | Entity/Vehicle/Automobile.cpp | - | pow x1 |
| 006A9D70 | None::ProcessSwingingDoor | Entity/Vehicle/Automobile.cpp | fsin x1 | - |
| 006AA290 | None::UpdateWheelMatrix | Entity/Vehicle/Automobile.cpp | fsin x1, fpatan x1 | asin x6 |
| 006AAB50 | None::PreRender | Entity/Vehicle/Automobile.cpp | fsin x3, fcos x3, fpatan x2 | asin x3, pow x1 |
| 006AD690 | None::ProcessControlInputs | Entity/Vehicle/Automobile.cpp | - | pow x1 |
| 006AE850 | None::TankControl | Entity/Vehicle/Automobile.cpp | fpatan x2, fcos x3, fsin x3 | - |
| 006B1880 | None::ProcessControl | Entity/Vehicle/Automobile.cpp | - | asin x1, pow x3 |
| 006B4200 | None::DeadPedMakesTyresBloody | Entity/Ped/Ped.cpp | - | floor x8 |
| 006B4410 | None::SetTowLink | Entity/Vehicle/Automobile.cpp | fpatan x1 | - |
| 006B45E0 | None::RcbanditCheckHitWheels | Entity/Vehicle/Automobile.cpp | - | floor x8 |
| 006B5FB0 | None::ProcessBuoyancy | Entity/Vehicle/Bike.cpp | - | pow x1 |
| 006B6950 | None::DoBurstAndSoftGroundRatios | Entity/Vehicle/Bike.cpp | - | floor x2 |
| 006B7150 | None::CalculateLeanMatrix | Entity/Vehicle/Bike.cpp | fcos x1 | - |
| 006B7280 | None::ProcessRiderAnims | Entity/Vehicle/Bike.cpp | - | pow x2 |
| 006B9250 | None::ProcessControl | Entity/Vehicle/Bike.cpp | fsin x2, fcos x2 | pow x7, asin x4 |
| 006BD090 | None::PreRender | Entity/Vehicle/Bike.cpp | fsin x5, fcos x3 | asin x1 |
| 006BE310 | None::ProcessControlInputs | Entity/Vehicle/Bike.cpp | - | pow x2 |
| 006BEEB0 | None::PlaceOnRoadProperly | Entity/Vehicle/Bike.cpp | fpatan x1, fcos x1, fsin x1 | - |
| 006BF430 | None::Constructor | Entity/Vehicle/Bike.cpp | fptan x1 | - |
| 006BFA30 | None::ProcessControl | Entity/Vehicle/Bmx.cpp | fsin x2 | - |
| 006BFB50 | None::ProcessDrivingAnims | Entity/Vehicle/Bmx.cpp | - | pow x2 |
| 006C0390 | None::LaunchBunnyHopCB | Entity/Vehicle/Bmx.cpp | fpatan x1 | - |
| 006C0810 | None::PreRender | Entity/Vehicle/Bmx.cpp | fsin x5, fcos x3, fpatan x1 | asin x1 |
| 006C34E0 | cBuoyancy::AddSplashParticles | Buoyancy.cpp | fsin x1, fcos x1 | - |
| 006C3EF0 | cBuoyancy::ProcessBuoyancy | Buoyancy.cpp | fsin x1, fcos x1 | - |
| 006C4830 | None::ProcessControlInputs | Entity/Vehicle/Heli.cpp | - | pow x2 |
| 006C58E0 | None::SearchLightCone | Entity/Vehicle/Heli.cpp | fsin x1, fcos x1 | - |
| 006C6520 | None::GenerateHeli | Entity/Vehicle/Heli.cpp | fcos x3, fsin x3 | - |
| 006C94A0 | None::PreRender | Entity/Vehicle/Plane.cpp | - | asin x3, pow x1 |
| 006CADD0 | None::ProcessControlInputs | Entity/Vehicle/Plane.cpp | - | pow x2 |
| 006CB7C0 | None::ProcessFlyingCarStuff | Entity/Vehicle/Plane.cpp | fsin x1 | - |
| 006CD090 | None::FindPlaneCreationCoors | Entity/Vehicle/Plane.cpp | fcos x1, fsin x1 | - |
| 006CD2F0 | None::DoPlaneGenerationAndRemoval | Entity/Vehicle/Plane.cpp | fcos x2, fsin x2 | - |
| 006CDCC0 | None::ProcessControl | Entity/Vehicle/QuadBike.cpp | - | pow x2 |
| 006CE020 | None::ProcessControlInputs | Entity/Vehicle/QuadBike.cpp | - | pow x1 |
| 006CE460 | None::ProcessAI | Entity/Vehicle/QuadBike.cpp | - | pow x1 |
| 006CEAD0 | None::PreRender | Entity/Vehicle/QuadBike.cpp | fpatan x2 | - |
| 006CF590 | None::ProcessAI | Entity/Vehicle/Trailer.cpp | fpatan x2 | - |
| 006CFDF0 | None::SetTowLink | Entity/Vehicle/Trailer.cpp | fpatan x1 | - |
| 006D0E90 | None::SpecialEntityCalcCollisionSteps | Entity/Vehicle/Vehicle.cpp | - | ceil x1 |
| 006D2740 | None::ApplyBoatWaterResistance | Entity/Vehicle/Vehicle.cpp | - | pow x3 |
| 006D85F0 | None::FlyingControl | Entity/Vehicle/Vehicle.cpp | fpatan x1, fcos x4, fsin x5 | pow x12, asin x4, acos x1 |
| 006DBBB0 | None::SetTransmissionRotation | Entity/Vehicle/Vehicle.cpp | fpatan x2 | - |
| 006DBCE0 | None::ProcessBoatControl | Entity/Vehicle/Vehicle.cpp | fsin x1, fcos x1 | pow x3 |
| 006E0A50 | None::DoHeadLightEffect | Entity/Vehicle/Vehicle.cpp | fsin x1, fcos x1 | - |
| 006E1A60 | None::DoVehicleLights | Entity/Vehicle/Vehicle.cpp | fsin x2, fcos x2 | - |
| 006E2E50 | None::DoBladeCollision | Entity/Vehicle/Vehicle.cpp | - | floor x8 |
| 006E3B00 | CVehicleAnimGroup::GetGroup | VehicleAnimGroupData.cpp | - | pow x1 |
| 006E4160 | WaterCreature_c::Init | WaterCreature_c.cpp | fpatan x1 | - |
| 006E4670 | WaterCreature_c::Update | WaterCreature_c.cpp | fsin x2, fcos x2, fpatan x1 | - |
| 006E7680 | None::RenderAndEmptyRenderBuffer | RenderBuffer.cpp | - | floor x2 |
| 006E81E0 | CWaterLevel::AddWaveToResult | WaterLevel.cpp | - | floor x4 |
| 006E8580 | CWaterLevel::GetWaterLevelNoWaves | WaterLevel.cpp | - | floor x2 |
| 006E8780 | CWaterLevel::RenderHighDetailWaterTriangle_OneLayer | WaterLevel.cpp | - | floor x2 |
| 006E8ED0 | CWaterLevel::RenderFlatWaterTriangle_OneLayer | WaterLevel.cpp | - | floor x4 |
| 006E91D0 | CWaterLevel::RenderHighDetailWaterRectangle_OneLayer | WaterLevel.cpp | - | floor x2 |
| 006E9940 | CWaterLevel::RenderFlatWaterRectangle_OneLayer | WaterLevel.cpp | - | floor x4 |
| 006E9C80 | CWaterLevel::SetCameraRange | WaterLevel.cpp | - | floor x2, ceil x2 |
| 006EA260 | CWaterLevel::RenderWakeSegment | WaterLevel.cpp | fsin x4 | floor x10 |
| 006EF650 | CWaterLevel::RenderWater | WaterLevel.cpp | fsin x2, fcos x2 | - |
| 006F0A10 | None::ProcessControlInputs | Entity/Vehicle/Boat.cpp | - | pow x1 |
| 006F1770 | None::ProcessControl | Entity/Vehicle/Boat.cpp | fpatan x2 | - |
| 006F4040 | CDoor::Process | Door.cpp | fsin x1, fcos x1 | pow x1 |
| 006F49A0 | CBouncingPanel::ProcessPanel | BouncingPanel.cpp | - | pow x1 |
| 006F5240 | cHandlingDataMgr::ConvertBikeDataToWorldUnits | cHandlingDataMgr.cpp | - | asin x3 |
| 006F5290 | cHandlingDataMgr::ConvertBikeDataToGameUnits | cHandlingDataMgr.cpp | fsin x3 | - |
| 006F6640 | None::MarkSurroundingEntitiesForCollisionWithTrain | Entity/Vehicle/Train.cpp | - | floor x8 |
| 006F86A0 | None::ProcessControl | Entity/Vehicle/Train.cpp | fpatan x2 | pow x3 |
| 006FBAA0 | CCoronas::RenderSunReflection | Coronas.cpp | fsin x1 | - |
| 007039C0 | CPostEffects::UnderWaterRipple | PostEffects.cpp | fsin x2 | - |
| 00703CC0 | CPostEffects::ImmediateModeFilterStuffInitialize | PostEffects.cpp | fyl2x x3 | pow(0x8220F0) x2 |
| 00704150 | CPostEffects::Fog | PostEffects.cpp | fsin x2, fcos x2 | - |
| 007043D0 | CPostEffects::SetupBackBufferVertex | PostEffects.cpp | fyl2x x3 | pow(0x8220F0) x2 |
| 00707CA0 | CShadows::StoreRealTimeShadow | Shadows.cpp | fpatan x2 | - |
| 0070A960 | CShadows::RenderStoredShadows | Shadows.cpp | - | floor x10, ceil x2 |
| 0070B730 | CShadows::GeneratePolysForStaticShadow | Shadows.cpp | - | floor x8 |
| 0070D490 | CSprite::RenderOneXLUSprite_Rotate_Aspect | Sprite.cpp | fsin x3, fcos x3 | - |
| 0070E780 | CSprite::RenderBufferedOneXLUSprite_Rotate_Aspect | Sprite.cpp | fsin x1, fcos x1 | - |
| 0070EAB0 | CSprite::RenderBufferedOneXLUSprite_Rotate_Dimension | Sprite.cpp | fsin x1, fcos x1 | - |
| 0070EDE0 | CSprite::RenderBufferedOneXLUSprite_Rotate_2Colours | Sprite.cpp | fsin x1, fcos x1 | - |
| 0070F540 | CSprite::RenderOneXLUSprite2D | Sprite.cpp | fsin x2, fcos x2 | - |
| 00711760 | CStencilShadows::RegisterStencilShadows | StencilShadows.cpp | - | floor x8 |
| 00711EF0 | CBirds::CreateNumberOfBirds | Birds.cpp | fpatan x1 | - |
| 00712330 | CBirds::Update | Birds.cpp | fpatan x2, fsin x2, fcos x2 | - |
| 00712810 | CBirds::Render | Birds.cpp | fsin x6, fcos x3 | - |
| 00712FF0 | CClouds::Update | Clouds.cpp | fsin x1 | - |
| 00713950 | CClouds::Render | Clouds.cpp | fpatan x1 | - |
| 00714650 | CClouds::RenderSkyPolys | Clouds.cpp | fsin x1, fcos x1 | - |
| 007174F0 | CPlaneTrails::Update | PlaneTrails.cpp | fsin x3, fcos x3 | - |
| 0071CB70 | CGlass::HasGlassBeenShatteredAtCoors | Glass.cpp | - | floor x8 |
| 00722970 | CCheckpoints::SetHeading | Checkpoints.cpp | fcos x1, fsin x1 | - |
| 00723240 | C3dMarkers::User3dMarkersDraw | 3dMarkers.cpp | fsin x1 | - |
| 00723750 | CBulletTraces::AddTrace | BulletTraces.cpp | fyl2x x2 | - |
| 007241C0 | CBrightLights::Render | BrightLights.cpp | fsin x1, fcos x1 | - |
| 00725120 | C3dMarkers::PlaceMarker | 3dMarkers.cpp | fsin x3 | - |
| 00729B60 | None::FireTruckControl | Entity/Vehicle/Automobile.cpp | fpatan x4, fsin x3, fcos x2 | - |
| 0072A9A0 | CWeather::AddRain | Weather.cpp | fsin x1, fcos x1 | - |
| 0072B630 | CWeather::UpdateInTunnelness | Weather.cpp | fsin x1, fcos x1 | - |
| 0072B850 | CWeather::Update | Weather.cpp | fcos x2 | - |
| 0072CAE0 | CWorldScan::ScanWorld | Renderer.cpp | - | floor x30, ceil x4 |
| 0072D5E0 | CWorldScan::SetExtraRectangleToScan | Renderer.cpp | - | floor x2, ceil x4 |
| 00733160 | CVisibilityPlugins::SetupVehicleVariables | VisibilityPlugins.cpp | fpatan x1 | - |
| 007360D0 | CBulletInfo::Update | BulletInfo.cpp | - | asin x1 |
| 00737C80 | CProjectileInfo::AddProjectile | ProjectileInfo.cpp | fpatan x3, fsin x3, fcos x3 | - |
| 00738B20 | CProjectileInfo::Update | ProjectileInfo.cpp | - | pow x1 |
| 00739E60 | CShotInfo::Update | ShotInfo.cpp | - | pow x1 |
| 0073AF00 | CWeapon::FireOneInstantHitRound | Weapon.cpp | - | asin x1 |
| 0073B550 | CWeapon::DoBulletImpact | Weapon.cpp | - | asin x2 |
| 0073C710 | CWeapon::SetUpPelletCol | Weapon.cpp | fcos x1, fsin x1 | - |
| 0073D720 | CWeapon::DoDriveByAutoAiming | Weapon.cpp | fptan x1 | - |
| 0073E800 | CWeapon::FireAreaEffect | Weapon.cpp | fpatan x1, fsin x1, fcos x1, fptan x1 | - |
| 0073FB10 | CWeapon::FireInstantHit | Weapon.cpp | fsin x2, fcos x2 | - |
| 00742300 | CWeapon::Fire | Weapon.cpp | fpatan x1, fsin x1, fcos x1 | - |
| 00742CF0 | CWeaponEffects::Render | WeaponEffects.cpp | fsin x6, fcos x6, fpatan x2 | - |
| 00746A10 | CPad::ProcessPad | Pad.cpp | fsin x1, fcos x1 | - |
| 00747200 | CGamma::SetGamma | Gamma.cpp | - | pow(0x8220F0) x1 |
| 00747CD0 | None::GTATranslateShiftKey | source/app/platform/win/WinPlatform.cpp | - | pow(0x8220F0) x1 |
| 00751D20 | RpLightSetConeAngle | - | fcos x1 | - |
| 0075B780 | RpTriStripMeshTunnel | - | - | pow(0x8220F0) x1 |
| 00761170 | _rxD3D9VertexShaderDefaultLightingCallBack | - | fcos x1 | - |
| 00764F60 | _rwD3D9VSGetRadiusInLocalSpace | - | fsincos x12, fpatan x9, fsin x7, fyl2xp1 x1, fyl2x x16, fcos x3, fptan x1 | acos x6, floor x4, asin x3, pow x6, ceil x1, exp(0x825E4C) x1 |
| 007CD5A0 | None::RtAnimInterpolatorSetCurrentAnim | RenderWare/rw/rtanim.cpp | fsin x3, fcos x3, fpatan x3 | pow(0x8220F0) x21 |
| 007EE370 | None::RwCameraShowRaster | RenderWare/rw/rwcore.cpp | fsin x1, fcos x1, fpatan x1 | - |
| 007FD100 | _rwD3D9RenderStateReset | - | - | pow(0x8220F0) x2 |

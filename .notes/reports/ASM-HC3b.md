# ASM-HC3b: executed-but-unverified functions, gameplay files in source/game_sa (continuation of HC3)

Zone rows after dropping already-covered ones and render/audio/entity/task/script/etc. names: about 60 gameplay rows with count > 0 (see C:/gta-scratch/hc3b_rows.txt style filter). Compared: 50 (49 MATCH, 1 INTENTIONAL: CRestart::AddPoliceRestartPoint bound check). No source changed, no build needed.
CCurves::CalcCurvePoint (top row, 77k calls) was spot-checked only (source is already written from the asm with per-address comments); not a full line-by-line pass.
NOT_REACHED (budget): CPedGeometryAnalyser (ComputeEntityBoundingBoxCornersUncached, PlanesUncached, BoundingSphere, RouteRoundSphere, MoveDirToAvoidEntity), CWeapon::Reload/Initialise, CStreaming::AreTexturesUsedByRequestedModels/AddToLoadedVehiclesList/ClearFlagForAll/HasSpecialCharLoaded, CIplStore::AddIplSlot/GetIplEntityIndexArray/RequestIplAndIgnore, CPathFind rest (CountNeighboursToBeSwitchedOff, SwitchOff*, TestCrossesRoad ...), CGarages/CGarage rest, CCullZones::AddMirrorAttributeZone, CTagManager, CFire/CFireManager, CCam::Process_FlyBy/Init, camera script setters.
SKIP-RENDER: CStreaming::RenderEntity, CCamera::RenderMotionBlur, Font/Sprite/Radar/Hud etc. as in HC3.
Details: .notes/asm_sweep/HC3b.result.tsv.

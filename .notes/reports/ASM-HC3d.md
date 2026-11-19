# ASM-HC3d: executed-but-unverified functions, gameplay files in source/game_sa (continuation of HC3c)

Zone rows (hitcount ryder1.tsv, NONE, not covered by any result tsv, non-render): ~110 (PathFind 7, Garages 6, Streaming 6, Camera 10, Pad 5, TimeCycle 15, Acquaintance 4, CarCtrl 2, rest 1-3 each); WaterLevel/Streaming::RenderEntity/RenderMotionBlur are render-ish (only BlockHit compared).
Compared 11 entries (HC3c list: CalcCurvePoint, ComputeRouteRoundSphere, ComputeMoveDirToAvoidEntity, InitDoorsAtStart, Process_FlyBy; plus FindAngleToWeaveThroughTraffic tail, FindTimeCycleBox, 9 TimeCycle ambient getters, 4 CAcquaintance fns, CPad::SetTouched, CWaterLevel::BlockHit): 10 MATCH, 1 FIXED.
FIXED: CPedGeometryAnalyser::ComputeMoveDirToAvoidEntity (exe 0x5F37B2): dist3 starts with planes[3].z * ped.y (port: planes[3].y * ped.y).
NOT_REACHED (budget): CPedGeometryAnalyser::ComputeRouteRoundEntityBoundingBox (1900 instrs), CCarCtrl::SteerAICarWithPhysicsFollowPath, FindAngleToWeaveThroughTraffic prelude/sector loop, and the small CPathFind/CGarages/CIplStore/CStreaming/CCamera/CPad rows.
Details: .notes/asm_sweep/HC3d.result.tsv.

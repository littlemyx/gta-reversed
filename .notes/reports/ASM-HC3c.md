# ASM-HC3c: executed-but-unverified functions, gameplay files in source/game_sa (continuation of HC3/HC3b)

Zone rows taken from the HC3b NOT_REACHED list: 16. Compared 10 (9 MATCH-ish, 1 FIXED), 6 NOT_REACHED (budget): CCurves::CalcCurvePoint (full pass), CPedGeometryAnalyser::ComputeMoveDirToAvoidEntity / ComputeRouteRoundSphere / ComputeRouteRoundEntityBoundingBox, CCam::Process_FlyBy, CGarage::InitDoorsAtStart, plus CPathFind/CGarages/CIplStore rest.
FIXED: CPedGeometryAnalyser::ComputeEntityBoundingSphere: squared distance is summed (z^2+y^2)+x^2 in the exe (0x5F3CAB-0x5F3CB9), the port summed x, y, z.
CornersUncached: RIGHT branch checked line by line; FORWARD/UP branches only structurally (same code with other axes).
SKIP-RENDER: none in this slice. Details: .notes/asm_sweep/HC3c.result.tsv.

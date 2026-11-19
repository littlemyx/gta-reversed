# ASM-HC3g: gameplay rows in source/game_sa (continuation of HC3f)

Zone rows left: ~521 NONE rows in ryder1.tsv, nearly all render/HUD/audio/FX (skipped as SKIP-RENDER). Compared 7: 6 MATCH, 1 FIXED (2155214a9).
STEP 1: HC3f 9b73b85b2 CONFIRMED instruction-level (per-plane sum orders, clip numerator y,x,z, classify NaN, ped-forward dot z,y,x, matrix alloc).
STEP 2: rest of ComputeRouteRoundEntityBoundingBox (0x5F55D1-0x5F5A2B and the whole mode-0 CheckPath/choice, modes 1/2, route build) compared: FIXED enter/exit NaN tests and path-length sum order (z,y,x); everything else MATCH.
STEP 3: CCurves::CalcSpeedScaleFactor, FindIntersection (inlined), TestForPedTrafficLight, TestCrossesRoad, CRopes::FindRope, CEntity::Add, CStuckCarCheck::Process: MATCH.
NOT_REACHED: CCover::Update, CPedIK::PitchForSlope, CPedGroupIntelligence::Process, CEntryExit::TransitionFinished, CTheCarGenerators::CreateCarGenerator.
Build: StandaloneRelease links; tests/regress not run.

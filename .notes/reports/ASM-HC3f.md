# ASM-HC3f: gameplay rows in source/game_sa (continuation of HC3e)

Zone rows left in ryder1.tsv after HC3e: ~11 non-render + the known open rows. Compared 18: 17 MATCH (PathFind TakeWidth/SetPathsNeeded/GetPathNode/CountNeighbours/SwitchOffNodeAndNeighbours/Tidy/UnMark/SwitchPed, Streaming AreTexturesUsed/PossiblyStreamCarOut, CRect::IsPointInside x2, CGarage::IsPointInsideGarage, ComputeRouteRoundSphere, CPad::LookAround LR/UD (ped), CWanted::RemovePursuitCop x2), 1 FIXED (2a6a36713).
FIXED: ComputeRouteRoundEntityBoundingBox - the exe unrolls the entry/exit loop with different float sum orders per plane (dist: plane0 y,z,x else x,y,z; crossing num/den: plane0 z,y,x else x,z,y); clip numerator y,x,z; ped-forward dot z,y,x; NaN class/ignore sense; hit ped with no matrix gets one (AllocateMatrix + UpdateMatrix).
NOT_REACHED: ComputeRouteRoundEntityBoundingBox from 0x5F55D1 (mode-0 path choice, mode 1/2, final route build); CCurves, FindIntersection/TestForPedTrafficLight/TestCrossesRoad (<=9 calls), render rows (Coronas, FxInterpInfoFloat) skipped.
Build: StandaloneRelease links; tests/regress not run.

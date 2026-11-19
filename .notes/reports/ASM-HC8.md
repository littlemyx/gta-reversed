# ASM-HC8: second pass over hot MATCH functions (root source/game_sa)
Taken 8 (budget-limited, of ~40 planned): 7 RECONFIRMED, 1 FIXED. Wrong earlier verdict: S24 (CPedIntelligence::ProcessStaticCounter, fix 76fd22dc0).
Not reached: the rest of the top-40 (CGarage::Update S48c, CRope::Update S71c, CAttractorScanner::ScanForAttractors S41, CInformGroupEvent::Process S50, CDarkel::Update S44, CCarCtrl::SteerAICarWithPhysics S22c, CPedStuckChecker::TestPedStuck S55 - the last already has x87d code).
Pattern worth a grep: CVector::SquaredMagnitude() sums (x,y,z); the exe often sums (z,y,x). Other callers of (a-b).SquaredMagnitude()/DistanceBetweenPointsSquared in hot functions should be checked against the exe.
Build/link of PedIntelligence.cpp: see the report line (build slots were both busy).

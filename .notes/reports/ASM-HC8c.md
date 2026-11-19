# ASM-HC8c: re-review of fca1fc910 + HC8 not-reached rows
Reviewed 6 (+1 not reached): all RECONFIRMED, 0 fixes, no source changed (no build). No earlier MATCH verdict found wrong.
STEP 1: fca1fc910 (CPedGeometryAnalyser::ComputeRouteRoundEntityBoundingBox) CONFIRMED at instruction level (0x5F4545-0x5F462C enter/exit shape incl. both-zero skip to 0x5F46FF; 0x5F5634/0x5F5680 length sum z,y,x).
STEP 2: SteerAICarWithPhysics, CGarage::Update (head + type dispatch), CDarkel::Update, CAttractorScanner::ScanForAttractors, CInformGroupEvent::Process reconfirmed. CRope::Update not reached (S71c 1658-instr trace stands).
No SquaredMagnitude/Magnitude call-site sum-order bugs found in these functions. HC8's remaining top-40 not reached (budget).

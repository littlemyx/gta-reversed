# ASM-RVZ10: review of HC8e/HC8g fixes
1. ManageObject d771f35e4: CONFIRMED (all compares, constants 64.5/35/120/54.5/25/80/750 verified).
2. ce7485d38: CONFIRMED for its exe site, but it landed in CPhysical::ProcessShift (exe 0x54DE2B, same x,y,z pattern); the 0x54E629 site is in ProcessCollision.
3. The two DistanceBetweenPoints sites (Physical.cpp ~442, ~522) both map to the single exe tail 0x54E5DA..0x54E641 (x,y,z, unrounded): FIXED via PhysicalMovedDistanceOriginal.
StandaloneRelease links; no tests/game runs (left to the gate).

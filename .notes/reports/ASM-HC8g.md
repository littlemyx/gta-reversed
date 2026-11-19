# ASM-HC8g: sum-order class, continuation
STEP 1: HC8f fixes CONFIRMED against the exe: CBike::ProcessAI (0x6BC440/0x6BC77B), CBmx::ProcessAI (0x6C156F), CAutomobile::ProcessControlInputs (0x6ADE25).
STEP 2: 26 HC8f new sites checked: 24 MATCH (already ported with z,y,x order and strict/NaN compares), 1 FIXED CPopulation::ManageObject (05e1b0d6a: NaN strictness of 3 compares).
Extra: CPhysical::ProcessCollision 0x54E629 m_fMovingSpeed from unrounded x87 diffs (9d23ded33); 2 sibling DistanceBetweenPoints sites (Physical.cpp ~442, ~522) UNSURE (exe site not mapped).
Not reached: scan below call count 12501, 2-term+sqrt extension. Build StandaloneRelease OK; no tests/game runs (left to gate).

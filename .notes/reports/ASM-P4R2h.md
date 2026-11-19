# ASM-P4R2h: FixHeading review + CEntity::CreateEffects chain (20.11)

- FixHeading (0x645CB0): P4R2g fix CONFIRMED against the exe. CCamera offsets 0xA4C / 0xA94 / 0x970 CONFIRMED (m_mMatInverse, m_mMatMirrorInverse, m_bMirrorActive).
- CEntity::CreateEffects: all 2dfx cases match except the ENEX heading math (FIXED): the exe keeps the FPATAN heading unrounded, multiplies by 57.2957763671875, adds the angle, then stores a float; the port rounded the heading first. srand(this + days), AddOne argument order, bEnableAccess clear: CONFIRMED.
- DestroyEffects, CEntryExitManager::AddOne (incl. corners/quad tree/link), FinishAnimCarAlignCB: MATCH. Rest of the car-entry chain was already MATCH/FIXED in earlier result TSVs.

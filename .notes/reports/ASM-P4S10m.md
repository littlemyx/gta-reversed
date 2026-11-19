# ASM-P4S10m

Part A (independent review vs exe 1.0): the three P4S10l fixes are CONFIRMED (CTaskInteriorGoToInfo::CreateFirstSubTask, CTaskComplexEnterAnyCarAsDriver::CreateFirstSubTask, CTaskComplexHitPedWithCar::CreateSubTask).

Part B: 19 further queue rows compared, 17 MATCH, 2 FIXED (see result TSV).

Changes:
- CAutomobile::FixPanel (exe 0x6A3670-0x6A3737) and CAutomobile::FixDoor (0x6A35A0-0x6A366D): CMatrix over RwFrameGetLTM(frame) re-translated to GetPosition() -> CMatrix over the frame's modelling matrix (frame+0x10, RwFrameGetMatrix) re-translated to its own position (exe never touches the LTM nor the vehicle position). Commit d57dcacc2 (one commit for both, identical code).

Not reached: remaining queue rows are Clone/Destructor/Constructor stubs.

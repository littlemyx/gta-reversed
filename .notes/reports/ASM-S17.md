# ASM-S17 (Bike.cpp, Bmx.cpp, Boat.cpp, 43 functions) - 2026-10-26 20:42 .. 21:0x (see hand-back)
Result: 21 compared (4 FIXED in 4 commits, 17 MATCH), 0 UNSURE, 22 NOT REACHED (token budget; table in S17.result.tsv). Slice is PARTIAL.
Fixes (exe address, one line each):
- CBike::ProcessDrivingAnims (0x6BF400): returns when offscreen AND status != PLAYER (`test [+0x36],0xF8; jne ret`); the port returned for status == PLAYER (inverted).
- CBmx::CBmx (0x6BF987): m_fMidWheelFracY = front wheel X / (frontY - rearY) (`fld [front.x]; fdiv st(1)`); the port divided front Y. Read by CBmx::GetFrameOffset.
- CBoat::CBoat (0x6F2B8B, 0x6F2B54): door axis byte (+0x5EA) is 2 (Z) for both models (port Y); the "not set" sentinel is -9999.99 (0xC61C3FF6), port used -10000. Same
  sentinel is stored/compared/passed to FlyingControl in CBoat::ProcessControl (0x6F1899/0x6F1921/0x6F1975/0x6F1FA3) - changed there too, since FlyingControl tests for exactly -9999.99
  (Vehicle.cpp FLYING_CONTROL_NOT_SET): with -10000 the pad values were never used. Only those sentinel sites of ProcessControl were checked, the rest is NOT_REACHED.
- CBoat::ProcessControlInputs (0x6F0B0D..0x6F0C27): steering source selection - mouse moved (x != 0) always takes the mouse branch (port: only when last input was already mouse, so the
  mouse could never take over from the keyboard); idle pad with last input = mouse leaves m_fRawSteerAngle untouched (port: keyboard smoothing).
MATCH: GetComponentWorldPosition (Bike, Boat), SetupModelNodes (Boat; Bike's 0x6B5960 too), SetModelIndex (Bike, Boat), SetRemoveAnimFlags, DisplayHandlingData (unused debug), both destructors,
Bike::Render, Bike::PlayCarHorn, Teleport (Bike, Boat), CBmx::ProcessControl, Bike::ProcessControlCollisionCheck, Bike::BlowUpCar, Bike::SetUpWheelColModel.
Consumer audit: no layout/signature change. The Boat sentinel constant is file-local (BOAT_HEADING_NOT_LOCKED) and covers every reader/writer of m_LockedHeading (only Boat.cpp). m_BoatDoor has no readers beyond Init.
Not reached (22): Bike: Constructor, DamageKnockOffRider, ProcessAI, ProcessBuoyancy, ProcessRiderAnims, PlaceOnRoadProperly, BurstTyre, ProcessControlInputs, ProcessEntityCollision, PreRender,
ProcessControl, VehicleDamage, SetupSuspensionLines, DoBurstAndSoftGroundRatios; Bmx: ProcessDrivingAnims, PreRender, ProcessAI, ProcessBunnyHop; Boat: ProcessControl (rest), PreRender, Render, BlowUpCar.
Gate: Release build OK (1186/1186); no oracle target covers the touched functions (only trivial Bmx/Boat ones); soak: see hand-back.

# ASM-S16 (Automobile.cpp, 48 functions) - 2026-10-26 20:17 .. see hand-back
Result: 23 compared (5 FIXED in 4 commits, 18 MATCH), 0 UNSURE, 25 NOT REACHED (token budget; table in S16.result.tsv). Slice is PARTIAL.
Fixes (exe address, one line each):
- Fix (0x6A3440): NoDoors is the MODEL flag (handling+0xCC bit 8, m_bNoDoors); the port tested the handling flag m_bWheelFNarrow2 (+0xD0). The exe also loops 4 times over the panel
  resets (stride 0x20 from +0x6AC); the 4th `word = 0xFFFF` lands on the low half of m_swingingChassis.m_openAngle (+0x70C) - replicated.
- SetTowLink (0x6B4410): status SIMPLE switches to real physics and continues; every status other than PHYSICS/ABANDONED/IS_TOWED/SIMPLE returns false (port had SIMPLE and default swapped).
- DoHoverSuspensionRatios (0x6A45C0): compression is 0.99999 (0x3F7FFF58) when the water is at/below the line end (port 1.0); the wheel col point gets surfaceTypeB = 0x27 (never set).
- RcbanditCheck1CarWheels (0x6B3F70): skips the RC Bandit (0x1B9) and vehicles already scanned, marking the others scanned (port inverted, never marked); |dx|,|dy| < 10 (port signed);
  the wheel test reads the OTHER vehicle's prev compression/status (port read this).
- RcbanditCheckHitWheels (0x6B45E0): returns true at the first hit, false at the end (port: inner break only, always true).
MATCH: Constructor (every door/panel/chassis/flag/rand init, 7 rand calls), Destructor, SetupModelNodes, SetModelIndex, Save, Load, CustomCarPlate_Before/After, StopNitroEffect,
GetComponentWorldPosition, CloseBoot, RemoveBonnetInPedCollision, PlayCarHorn, ProcessControlCollisionCheck, SetupDamageAfterLoad, Teleport, SetTotalDamage, OpenDoor.
Notes: the exe constructor does not reset wheelSkidmarkMuddy (port fills false, harmless). Consumer audit: no layout/signature change; RcbanditCheck* have no other callers than
RcbanditCheckHitWheels (<- ProcessControl); ProcessScan() already existed.
Not reached (25): HydraulicControl, ProcessBuoyancy, ProcessHarvester, TankControl, ProcessCarWheelPair, PlaceOnRoadProperly, FireTruckControl, dmgDrawCarCollidingParticles, SpawnFlyingComponent,
UpdateWheelMatrix, ProcessSwingingDoor, TowTruckControl, ProcessCarOnFireAndExplode, PreRender, Render, DoBurstAndSoftGroundRatios, VehicleDamage, ProcessControl, ProcessAI, ProcessSuspension,
SetupSuspensionLines, ProcessEntityCollision, ProcessControlInputs, BlowUpCar, BlowUpCarCutSceneNoExtras.
Gate: Release build OK (1185/1185); no oracle target covers Automobile (D/S classes); soak (NOVSYNC, 3 min) RESULT: OK, 0 errors.

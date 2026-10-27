# ASM-S16d (Automobile.cpp, the 15 functions left from S16c) - 2026-10-27
Result: 3 compared over their whole exe range, all FIXED (3 commits); 0 MATCH, 0 UNSURE, 12 NOT REACHED (token budget; table in S16d.result.tsv). Slice is PARTIAL.
Fixes:
- ProcessSuspension (0x6AFB10-0x6B0686), 6d1fc939: damping gate is springLength < 0.99999 (0x871258; port 1.0); the Vortex force scale is the else of the (hydraulic-installed && speed < 0.15) branch; x87 register order of forward speed and of the roll-onto-wheels turn/move force scales.
- ProcessCarWheelPair (0x6A4EC0-0x6A600D), 252de6b1: burnout turn force = min(1,3000/tm)*(-0.002*tm)*steer*right; adhesion factor ((1-compr)*4*level)*bias for both wheels (port bias*level*4*(1-compr)). Everything else matched, including the left-only Hydra check.
- ProcessEntityCollision (0x6ACE70-0x6AD68C), 8b653c12: the wheel contact points added at buildings are counted (numColPts<31) and the loop continues over all 4 wheels; the port counted them only under FIX_BUGS (off in the run build) and stopped after the first, so they were never returned. Also the depth factor order and the dot-sum orders.
Consumer audit: no signature/layout change; ProcessEntityCollision now returns up to 4 more colpoints against buildings (callers take up to 32 as in the exe).
Not reached (12): ProcessControl (2212 asm lines), HydraulicControl, ProcessBuoyancy, ProcessHarvester, TankControl, PlaceOnRoadProperly, FireTruckControl, SpawnFlyingComponent, UpdateWheelMatrix, PreRender, VehicleDamage, SetupSuspensionLines.
Gate: Release build OK (0 errors); no oracle target covers Automobile; soak (NOVSYNC, 3 min) RESULT: OK (a first run was cut short by the orchestrator pause and rerun).

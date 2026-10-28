# ASM-S16f (Automobile.cpp, the 10 functions left from S16e) - 2026-10-28
Result: 4 compared over their whole exe range, all FIXED (4 commits); 0 MATCH, 0 UNSURE, 6 NOT REACHED (token budget; table in S16f.result.tsv). Slice is PARTIAL.
Fixes:
- PlaceOnRoadProperly (0x6AF420-0x6AF90B), 5262aa7f: lighting and z are assigned also when the first (upward) line hit is the closer one; new position = sum * (1/length) (port divided).
- ProcessHarvester (0x6A9680-0x6A9C76), 8ace0485: grass objects tested by the OBJECT's model, visible, not render-damaged (port tested the car's model); y,z,x dot sums; limb gets the car matrix (port called SetMatrix on the car); particle velocity rand = (t*0.1+v)-0.05 (port mirrored); limb turn speed range +-0.04 (port -0.04..0.12), args z,y,x.
- SpawnFlyingComponent (0x6A8580-0x6A8BDA), 867476a6: detached part never got car colours / remap texture / bChangesVehColor; move-speed z branch had up.z polarity flipped and tested collisionType 18 where the exe tests the node (windscreen), in two places; removal time float adds (PC24); buoyancy literal 0x3DDA740F; frame matrix copied before SetFrame; camera dot y,z,x.
- FireTruckControl (0x729B60-0x72A26E), 956da853: doom rotations snap to the target within one step (AI vertical, camera vertical and horizontal; port left them unchanged); wobble scale 6.28f/4096 (not 2pi/4096); 10/20 degree clamp literals are 1 ulp above DegreesToRadians.
Consumer audit: no signature/layout change. SpawnFlyingComponent callers (doors, bonnet, bumpers, wheels, panels on damage) now get coloured parts with real removal times; ProcessHarvester/FireTruckControl are per-vehicle only.
Not reached (6): HydraulicControl, ProcessBuoyancy, TankControl, UpdateWheelMatrix, PreRender (2273 asm lines), VehicleDamage.
Gate: Release build OK (0 errors); no oracle target covers Automobile; soak (NOVSYNC, 3 min, 44200 frames) RESULT: OK.

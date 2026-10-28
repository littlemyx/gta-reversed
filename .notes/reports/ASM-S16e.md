# ASM-S16e (Automobile.cpp, the 12 functions left from S16d) - 2026-10-28
Result: 2 compared over their whole exe range, both FIXED (2 commits); 0 MATCH, 0 UNSURE-only, 10 NOT REACHED (token budget; table in S16e.result.tsv). Slice is PARTIAL.
Fixes:
- ProcessControl (0x6B1880-0x6B377F, 2212 asm lines, all sections), a7a10944: the hit-wall flag for ProcessBoatControl is read before CPhysical::ProcessControl (which clears it; the Skimmer always got false);
  traction = K*mult (PHYSICS: (carTraction*mult)*K, K = 0xC1BFEC = 0x3B83126E) then *0.25 then /velFreq (port: 250/carTraction*mult for PHYSICS = 62500x too big, and a different op order);
  forward/right/turn-forward dot sums z,y,x; controller shake frequency = speed/mass*shake*400000+c and the shake time divides by the truncated byte (port: 200*shake*speed*2000/mass, divided by the float);
  stopped-car speed limit 0.005 (port 0.0045); earthquake shake is ADDED to z (port replaced z with the random value).
- SetupSuspensionLines (0x6A65D0-0x6A6ADB), fd079c1f: line end.z op order; front/rear height above road op order and the rear one uses wheel 3 (port wheel 1); the ground-clearance sphere clamp used centre.z-radius (exe: centre.z-minBottom, centre = minBottom+radius).
UNSURE (not fixed, in ProcessControl): hydraulic PHYSICS branch guards specialColIndex==-1 (exe indexes out of bounds); the exe constants 0.01/0.04 are 1 ulp above 0.01f/0.04f (0x86CD7C, 0x863244); std::pow vs _CIpow rounding in the wheelspin audio and hydraulic force.
Consumer audit: no signature/layout change. Traction change alters AI/physics car handling (STATUS_PHYSICS cars), SetupSuspensionLines runs per spawned car model (rear height and RCBandit/ground-clearance spheres).
Not reached (10): HydraulicControl, ProcessBuoyancy, ProcessHarvester, TankControl, PlaceOnRoadProperly, FireTruckControl, SpawnFlyingComponent, UpdateWheelMatrix, PreRender, VehicleDamage.
Gate: Release build OK (0 errors); no oracle target covers Automobile; soak (NOVSYNC, 3 min, 44500 frames) RESULT: OK.

# ASM-S16b (Automobile.cpp, the 25 functions left from S16) - 2026-10-27
Result: 8 compared (5 FIXED in 4 commits, 2 MATCH, 1 UNSURE), 17 NOT REACHED (token budget; table in S16b.result.tsv). Slice is PARTIAL.
Fixes (exe address, one line each):
- BlowUpCar (0x6B3780) / BlowUpCarCutSceneNoExtras (0x6B3BB0), 6729564a: property-damage stat = float(4000 + rand() % 6000); explosion z offset is the 3rd rand draw (rand/32767 + 0.5); cut-scene FuckCarCompletely(bNoExplosion); StartFire before Darkel in BlowUpCar.
- dmgDrawCarCollidingParticles (0x6A6DC0), e7ca468b: spark type 1, spark count from the full ftol(force) (& 0x3F), smoke prt-mult life 0.1, debris only when |v|^2 > 0.0625 (was inverted), debris colour scales RGB only.
- ProcessSwingingDoor (0x6A9D70), dd96a3e1: bonnet swing = (sin(angle) + 0.1) * 0.05 (port: sin(angle + 0.1)); dot-product order z, y, x.
- ProcessCarOnFireAndExplode (0x6A7090), cfc5f7f4: non-plane path never created the fire fx and reset the burn timer every frame (never exploded); plane bExplodeImmediately skipped BlowUpCar; fire-counter == 1 rate; RC smoke alpha/size/life sat in the wrong slots;
  smoke rand count/order (no pos.z draw); explosion roll int rand(0,250) < 3; fire fx position is object space.
MATCH: TowTruckControl, Render. UNSURE: DoBurstAndSoftGroundRatios (ulp only: dot-product order z,y,x and (rem*adhesion)*offroad in the sand term can change the stored compression in the last bit; not fixed).
Consumer audit: no signature/layout change; BlowUpCar_Impl callers unchanged (BlowUpCar, BlowUpCarCutSceneNoExtras); FIX_BUGS builds keep the deliberate cut-scene deviations (RC tiger, KillPedsGettingInVehicle).
Not reached (17): HydraulicControl, ProcessBuoyancy, ProcessHarvester, TankControl, ProcessCarWheelPair, PlaceOnRoadProperly, FireTruckControl, SpawnFlyingComponent, UpdateWheelMatrix, PreRender, VehicleDamage, ProcessControl, ProcessAI, ProcessSuspension, SetupSuspensionLines, ProcessEntityCollision, ProcessControlInputs.
Gate: Release build OK; no oracle target covers Automobile (D/S classes); soak (NOVSYNC, 3 min requested, 406 s route) RESULT: OK, 0 errors.
Process note: one edit script was run once with cwd in the MAIN checkout by mistake (it applied the ProcessCarOnFireAndExplode rewrite to its working tree); undoing was blocked by the permission classifier, so it needs `git checkout -- source/game_sa/Entity/Vehicle/Automobile.cpp` there (the diff is saved).

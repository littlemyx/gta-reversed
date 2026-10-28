# ASM-S17c (Bike.cpp, Bmx.cpp, Boat.cpp, 16 functions left from S17b) - 2026-10-28
Result: 6 compared (4 FIXED in 4 commits, 2 MATCH), 0 UNSURE, 10 NOT REACHED (token budget; table in S17c.result.tsv). Slice is PARTIAL.
Fixes (exe address, commit):
- CBoat::Render (0x6F0210-0x6F0930, 2c3f6019): Speeder rear vertex y is +0.06 (port -0.06); Reefer front/rear vertex pairs are swapped in the exe; the invisible cheat only skips CVehicle::Render and the two Im3D draws (port returned early, skipping the render-state set-up).
- CBoat::BlowUpCar (0x6F21B0-0x6F2543, fede4f79): debris buoyancy constant is 0x3DDA740F (8/75 gives ...0E); the atomic lookup result is cleared before the callback walk (port left it uninitialised).
- CBike::DamageKnockOffRider (0x6B5A10-0x6B5F3A, c2832116): the three impact dot products are summed (y+z)+x (port x+y+z); every other branch, threshold and event argument equal.
- CBike::ProcessBuoyancy (0x6B5FB0-0x6B6612, 486c374a): (ts*mass)*0.008f, (U/buoy)*f*1.05f, 1-f*0.05f (port divided by 125 and 20), same fix as CAutomobile in S16h.
MATCH: CBike::DoBurstAndSoftGroundRatios (0x6B6950), CBike::PlaceOnRoadProperly (0x6BEEB0; ProcessVerticalLine args, front-then-rear order, pitch/right/forward/up and the mid point equal).
Not reached (10): Bike ProcessAI, ProcessRiderAnims, ProcessControlInputs, PreRender, ProcessControl; Bmx ProcessDrivingAnims, PreRender, ProcessAI; Boat ProcessControl, PreRender. The big functions (ProcessControlInputs, ProcessRiderAnims, both ProcessControl) were not started: each is 450-3700 asm lines.
Consumer audit: no layout or signature change; the Boat.cpp edits are local to Render and BlowUpCar.
Gate: Release build OK (1186/1186 then 9/9 after the rebase, 0 errors); no oracle target covers Bike/Bmx/Boat; soak (NOVSYNC, 3 min) RESULT: OK. Scratch: C:/gta-scratch/asm-s17c (asm dumps, build.log, soak/, soak.out, jt.py).

# ASM-S17b (Bike.cpp, Bmx.cpp, Boat.cpp, 22 functions left from S17) - 2026-10-27
Result: 5 compared (3 FIXED in 3 commits, 2 MATCH), 0 UNSURE, 17 NOT REACHED (token budget; table in S17b.result.tsv). Slice is PARTIAL.
Fixes (exe address, one line each):
- CBmx::ProcessBunnyHop (0x6C067A, commit ceaeba2a): when the hop is not running (control jump <= 0, or controls disabled) an existing bunny-hop anim gets playing|auto-remove and blend delta -8; the port skipped this fade-out block entirely.
- CBike::ProcessEntityCollision (0x6BE1DC, commit d3642279): wheel ground-physical pointer is stored and registered without cleaning up the old reference (port used ChangeEntityReference; now SetEntityReference).
- CBike::CBike (0x6BF79A, commit 09b8ebd3): `and [this+0x429], 0xF0` clears Van, Bus, Big, LowVehicle; the port left bIsBus alone (no observable effect expected).
MATCH: CBike::BurstTyre (0x6BEB20; dead 0xD knock-off branch and the FixBugs bounds check are as documented), CBike::VehicleDamage (0x6B8EC0; the damage arg slot is reused as the working copy, the local at [esp+8] is prevHealth by 0x6B91E1),
CBike::SetupSuspensionLines (0x6B89B0; jump table, node walk, bound-sphere, ground clearance all equal). Note: the CBike ctor does not initialise m_aWheelSuspensionHeights/OrigHeights/SuspensionLength/LineLength (the port zeroes them), harmless.
Not reached (17): Bike: DamageKnockOffRider, ProcessAI, ProcessBuoyancy, ProcessRiderAnims, PlaceOnRoadProperly, ProcessControlInputs, PreRender, ProcessControl, DoBurstAndSoftGroundRatios; Bmx: ProcessDrivingAnims, PreRender, ProcessAI;
Boat: ProcessControl (rest), PreRender, Render, BlowUpCar. Budget reached after the smaller functions (Bmx ProcessBunnyHop, Bike ctor, BurstTyre, VehicleDamage, ProcessEntityCollision, SetupSuspensionLines).
Consumer audit: no layout/signature change. m_aGroundPhysicalPtrs readers only deref non-null pointers (Bike.cpp 1452/1624/1847/1975/3371). bIsBus on a bike: no reader.
Gate: Release build OK (3/3 incremental after the base 1186/1186); no oracle target covers Bike/Bmx; soak RESULT: OK (0 errors, 15100 frames).

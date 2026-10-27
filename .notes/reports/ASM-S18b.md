# ASM-S18b (Heli.cpp + Plane.cpp leftovers, 24 functions) - 2026-10-27
Result: 13 compared (1 FIXED, 12 MATCH), 0 UNSURE, 11 NOT REACHED (token budget; table in S18b.result.tsv). Slice is PARTIAL.
Fix (exe address, commit):
- CHeli::RenderAllHeliSearchLights (0x6C7C50-0x6C7D05, 465db0dc): the exe loops over NumberOfSearchLights entries only (uint8 counter); the port iterated all 4 HeliSearchLights slots and drew stale ones.
MATCH: Heli Pre/Post_SearchLightCone, SendDownSwat, TestSniperCollision, Render, ProcessControlInputs, PreRender, BlowUpCar; Plane Constructor, Render, OpenDoor, FindPlaneCreationCoors.
Notes: TestSniperCollision returns bool in the exe (any heli hit); the only caller (CBulletInfo::Update) ignores it. CHeli::GetRopeId casts the pointer to uint32 with reinterpret_cast (not NOTSA_PTR_ID32); left as is.
Not reached (11): Heli SearchLightCone, GenerateHeli, UpdateHelis, ProcessControl; Plane PreRender, ProcessControl, ProcessControlInputs, ProcessFlyingCarStuff, VehicleDamage, BlowUpCar, DoPlaneGenerationAndRemoval.
Consumer audit: no layout/signature change; RenderAllHeliSearchLights is called only from app_game.cpp (guarded by NumberOfSearchLights).
Gate: Release build OK (1186/1186 base, 2/2 incremental); no oracle target covers CHeli/CPlane; soak (NOVSYNC, 3 min route) RESULT: OK.

# ASM-S18 (Heli.cpp, MonsterTruck.cpp, Plane.cpp, QuadBike.cpp, 41 functions) - 2026-10-27
Result: 17 compared (4 FIXED in 4 commits, 13 MATCH), 0 UNSURE, 24 NOT REACHED (token budget; table in S18.result.tsv). Slice is PARTIAL: QuadBike and MonsterTruck done, Heli (13) and Plane (11) not started.
Fixes (exe address, commit):
- CQuadBike::ProcessAI (0x6CE7D9-0x6CEA32, c436b119): in-air assist needs ALL four wheels airborne (port: any); limit constant 0x871AD0 is 0.03 (port 0.029); handbrake yaw force applied at com+forward (port com+right = zero torque);
  without handbrake the roll assist runs only when GetAccelerate()==0 (port inverted) at com+up (port com+right). Rest of ProcessAI (lean fwd/back, LeanAngle) matches.
- CQuadBike::ProcessControlInputs (0x6CE145-0x6CE160, d171e316): mouse lean multiplier is -0.0035 (0x871AD8); port -0.035.
- CQuadBike::Fix (0x6CE2B0, b4ce7f18): exe starts with CDamageManager::ResetDamageStatus; port did not.
- CMonsterTruck::ProcessEntityCollision (0x6C8C24, 75edcb54): wheel entity is stored + registered without cleaning up the old reference (SetEntityReference, as for Bike/Automobile).
MATCH: QuadBike ctor, PreRender, ProcessControl (all x87 branches and constants), ProcessDrivingAnims, ProcessSuspension, SetupDamageAfterLoad, SetupSuspensionLines;
MonsterTruck ctor, ProcessSuspension, ProcessControlCollisionCheck, ProcessControl, SetupSuspensionLines, PreRender (also ExtendSuspension, not in the slice list).
Not reached (24): Heli: all 13 (Pre/Post_SearchLightCone, SearchLightCone, GenerateHeli, UpdateHelis, SendDownSwat, RenderAllHeliSearchLights, TestSniperCollision, Render, ProcessControlInputs, PreRender, BlowUpCar, ProcessControl);
Plane: all 11 (ctor, PreRender, Render, OpenDoor, ProcessControl, ProcessControlInputs, ProcessFlyingCarStuff, VehicleDamage, BlowUpCar, FindPlaneCreationCoors, DoPlaneGenerationAndRemoval).
Consumer audit: no layout/signature change; the only shared-state change is Fix now resetting m_damageManager (callers: cheats/script fix).
Gate: Release build OK (1185/1185 base, 49/49 incremental); oracle: no target covers the changed functions (gameplay_oracle_vehicle only has Monster/Quad ResetSuspension, untouched); soak (NOVSYNC, 3 min, 39500 frames) RESULT: OK, 0 errors.

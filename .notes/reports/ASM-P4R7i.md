# ASM-P4R7i: large never-audited Entity/root functions (continued)
Zone: 94 rows (col7=0, >=250 B, minus P4R7*) after dropping non-gameplay dirs; 21 compared (Entity/Vehicle/Ped, World, Weapon, Population, CarCtrl, CarEnterExit, ProjectileInfo, GameLogic), 73 NOT_REACHED (mostly ControllerConfigManager, Messages, CdStream, WaterLevel, CullZones, Cam; non-gameplay).
Fixes (2):
- 246433589 CWorld::FindUnsuspectingTargetCar: initial nearest distance 9999.9f (exe 0x864F90), not FLT_MAX.
- a058efce2 CWeapon::PickTargetForHeatSeekingMissile: best rating starts at 99999.9f and candidate must be strictly lower (port FLT_MAX and <=).
19 MATCH. Details in P4R7i.result.tsv. Build: World.cpp + Weapon.cpp + StandaloneRelease link.

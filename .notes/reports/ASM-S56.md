# ASM-S56 (Pickups rest, PlaneTrail/PlaneTrails, PlayerInfo (gameplay logic first), PlayerPedData, PlayerRelationshipRecorder) - 2026-10-29
Result: 40 rows: 10 FIXED (10 commits), 17 MATCH, 0 UNSURE, 13 NOT_REACHED (budget). Table: S56.result.tsv. S55b (2 rows) is in ASM-S55b.md.
Fixes (exe address, one line each):
- 0e5b8ae7f CPickups::Update (0x458DE0): the player-2 arm (0x459069) is unreachable in the exe (its guard reads player 1's tasks); port updated player 2 while player 1 was busy.
- bca886f4a CreateSomeMoney (0x458970): wad angle (rand()&0xFF)*(2pi/256) per axis (port: rand/32767*2pi).
- 92b68f485 CreatePickupCoorsCloseToCoors (0x458A80): LOS end uses the inlined float sqrt((dz2+dy2)+dx2).
- eb20eff10 CPlaneTrail::Render (0x717180): vertices packed into the global buffer, alpha factor *1e-4f, draw only for nvert>1 (port: indexed by point, /10000, nvert>=1).
- 51663c07e CPlaneTrails::Render (0x7173A0): sky brightness scale 1/256 (port 1/255).
- 8224ccc81 CPlaneTrails::Update path (0x7174F0): sin/cos times the trail centre unrounded (S43c fixes verified: centres, colours, hour ranges, modifiers).
- e5047b117 WorkOutEnergyFromHunger (0x56E610): last-hungry-state is an unsigned byte (port int8, muscle/health test flips above 127 hours).
- fcc1d6f04 MakePlayerSafe (0x56E870): step order as the exe.
- b6947de5b StreamParachuteWeapon (0x56EB30): outside co-op a false flag releases the parachute at once (port ignored the flag).
- b2f1161b5 CPlayerPedData::SetInitialState (0x56F210): player flags word masked (bits 0-3,6,7,11 cleared) - port only set bits 4 and 12.
MATCH: KillPlayer, ArrestPlayer, PlayerFailedCriticalMission, BlowUpRCBuggy, GivePlayerParachute, Clear, PlayerPedData ctor/Deallocate/Allocate, RecordRelationshipWithPlayer, pickups Save/Load/DetonateMines, PlaneTrail Init/RegisterPoint, PlaneTrails Init/RegisterPoint.
Open (NOT_REACHED, budget): PlayerInfo::Process, FindObjectToSteal, EvaluateCarPosition (these already carry exe-address annotations in the port), FindClosestCarSectorList, ProcessCarGunCrosshair/DrawCrosshair hooks, Constructor, Save/Load, skin load/set/delete, PlayerPedData::Destructor. CPlayerInfo::Clear stores m_nCrosshairActivated as a byte (port uint32, value 0/1: not changed).
Tests: Release build clean; PortableWin64 compile of the 5 touched files clean (0 errors); wave2_oracle_test / combat regress: see the run line below.

# ASM-S57 (also finishing S56b) - 2026-10-29
S56b: 14 rows (13 + the S56 Clear leftover): 4 FIXED, 9 MATCH, 1 UNSURE (DrawCrosshair trail loop not compared). Table: S56b.result.tsv.
S57: 40 rows: 3 FIXED, 15 MATCH, 1 UNSURE (CPedAttractorPool stride), 21 NOT_REACHED (20 CReplay functions + DrawScriptSpritesAndRectangles, budget). Table: S57.result.tsv.

Fixes (exe address, one line each; hashes in the result tables):
- CPlayerInfo::FindClosestCarSectorList (0x56F4E0): bike test (exe 0x56F58A) and boat test (0x56F6C9) read m_nVehicleType ([veh+0x590]); port used m_nVehicleSubType (BMX counts as bike in the exe, hovercraft/vortex differ). NaN up.z skips.
- CPlayerInfo::Process (0x56F8D0): "moved at least 10 units" is not(dist < 10) (unordered sets it). Everything else in Process verified: taxi fare, car stunts, display money steps, max health, parachute, road density clamp (rounding order), exit/enter vehicle (boat, water-aircraft door, co-op driver/passenger logic), remote explosion fade, upside down, distance stats (the two jump tables decode as appearance-2 and wanted-1), chase values.
- CPlayerInfo::SetPlayerSkin (0x5717F0): strncpy(32) zero-pads the name buffer (port strcpy_s kept the old tail).
- CPlayerInfo::Clear leftover: m_nCrosshairActivated is now uint8 + 3 pad bytes (the exe stores a byte at 0x56F49F; operator= copies the dword, 0x45E191).
- CTheScripts::Process (0x46A000): TIMERA/TIMERB step uses ms_fTimeStepNonClipped (exe [0xB7CB58]); port used the clipped ms_fTimeStep (differs below ~17 fps). The per-frame text-line refill stores WrapX 182.0 / CentreSize 640.0 (immediates), tScriptText defaults changed to these.
- CTheScripts::Init (0x4690B7): intro text lines get WrapX = CentreSize = RsGlobal.maximumWidth (port: height / width).
- CTheScripts::RemoveScriptTextureDictionary (0x465A40): the "script" txd is removed only if the slot holds a loaded RwTexDictionary.

Pools (no change): New() search order of CEventPool/CPedAttractorPool/CPedIntelligencePool/CPedPool/CVehiclePool matches 0x5E45E0 and siblings exactly (pre-increment cursor, one wrap to 0, free flag 0x80, 7-bit ref wrap, cursor stays on the allocated slot); strides 0x44/0x294/0x7C4/0xA18 match the sizeof of the pool's widest type. CPedAttractorPool: exe stride is 0xC4, port CPool<CPedAttractor> has 0x8C (internal only). CPedPool::GetAtRef: port adds a bounds check the exe lacks (benign).
MATCH: Remote (3), Restart Save/Load, ScriptResourceManager (3), ReturnScripted2DEffect, GetPlayerRelationshipRecorder, EvaluateCarPosition, FindObjectToSteal, crosshair process hook, skin load/delete, PlayerInfo ctor/Save/Load, PlayerPedData dtor.
Open: DrawCrosshair trail loop (0x56EF90), CReplay x20, DrawScriptSpritesAndRectangles (0x464980); the tScriptText WrapX default change is worth an eyeball with a mission that draws text without SET_TEXT_WRAPX.
Tests: Release + StandaloneRW build clean; wave2_oracle_test (rebuilt after the last edit): 1142 functions, PC24 0/0, PC53 0/0; combat regress PASS=14; v6 grep clean; PortableWin64 compile of PlayerInfo/TheScripts/Pickups/Remote/UtilityCommands clean.

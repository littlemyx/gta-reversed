# S6-H table (g23, ids 2300..2399, 58 commands) - file `source/game_sa/Scripts/Commands/Ported/Group23.cpp`, registration `g23::RegisterHandlers` (Commands.hpp + RunningScript.cpp)

Group processor g23 0x4762D0 (switch base 2300: `lea eax,[edi-0x8FC]; cmp eax,0x63`). All cases return `xor al, al` (OR_CONTINUE). Ids not listed here already had handlers (not in `TRAP_READERS_script_missing.txt`).
Oracle: `tests/standalone/script_oracle_g23_test.cpp` (target `script_oracle_g23_test`, run with RW_EXE_ORACLE): 56 commands, 3000 random cases each at PC=24, ScriptSpace + IP + compare flag + watched entity memory + callee log compared bit-exact against the exe processor; all callees are recorders on both sides (or forwards to the exe's own code for pure-memory ones).
Not covered by the oracle: 2323 (the new script reads its parameters through the usercall-style 0x464500), 2332 (needs 2dfx model infos + FindObjectsInRange; the search is the g22 function that script_oracle_g22_test covers).
Exe no-ops in this group: none. Unported callees: none (0x6002F0 = local copy of the g22 attractor search, 0x43E090 = 8 instructions inlined into 2380, 0x4700E0/0x40C1C0 are `+0x6676` thunks to CStreaming::SetMissionDoesntRequireModel/RemoveModel).
Dead stores into ScriptParams that the exe leaves behind (2319, 2320, 2323, 2332, 2342, ...) are dropped, except in the raw handlers where they are observable (2380).

| id | name | case | in -> out / notes | oracle |
|---|---|---|---|---|
| 2301 | SET_HEATHAZE_EFFECT | 0x4762F8 | flag -> CPostEffects::ScriptHeatHazeFXSwitch(flag!=0) | yes |
| 2302 | IS_HELP_MESSAGE_BEING_DISPLAYED | 0x476323 | () -> cmp CHud::HelpMessageDisplayed() | yes |
| 2303 | HAS_OBJECT_BEEN_DAMAGED_BY_WEAPON | 0x476346 | obj(ptr, null=>false), weapon -> cmp; type 0x38/0x39 => CDarkel::CheckDamagedWeaponType((int8)obj+0x148, type), else (int8)last == type | yes |
| 2304 | CLEAR_OBJECT_LAST_WEAPON_DAMAGE | 0x4763BB | obj(ptr) -> byte +0x148 = 0xFF (null: nothing) | yes |
| 2308 | GET_HUD_COLOUR | 0x47644C | idx(byte) -> CHudColours::GetRGB -> r,g,b,a as 4 ints | yes |
| 2309 | LOCK_DOOR | 0x4764AF | obj, flag -> !=0: CObject::LockDoor; else physical flags &= ~0xC, turn speed (+0x50) = 0, SetIsStatic(true) (vtbl slot 4) | yes |
| 2310 | SET_OBJECT_MASS | 0x47652F | obj, float -> +0x8C | yes |
| 2311 | GET_OBJECT_MASS | 0x476563 | obj -> raw dword +0x8C | yes |
| 2312 | SET_OBJECT_TURN_MASS | 0x4765A0 | obj, float -> +0x90 | yes |
| 2313 | GET_OBJECT_TURN_MASS | 0x4765D5 | obj -> raw dword +0x90 | yes |
| 2318 | SET_ACTIVE_MENU_ITEM | 0x476659 | menu(byte), item(byte) -> CMenuSystem::SetActiveMenuItem | yes |
| 2319 | MARK_STREAMED_SCRIPT_AS_NO_LONGER_NEEDED | 0x47668A | idx(word) -> GetProperIndex..., CStreaming::SetMissionDoesntRequireModel(proper + 0x6676) | yes |
| 2320 | REMOVE_STREAMED_SCRIPT | 0x4766C5 | idx(word) -> same decode, CStreaming::RemoveModel(proper + 0x6676) | yes |
| 2322 | SET_MESSAGE_FORMATTING | 0x476701 | flag, centre(word), width(word) -> bUseMessageFormatting 0xA44B66, MessageCentre 0xA44B64, MessageWidth 0xA44B60 | yes |
| 2323 | START_NEW_STREAMED_SCRIPT | 0x476742 | idx(word) -> StartNewStreamedScript(GetProperIndex(idx)), then ReadParametersForNewlyStartedScript(newScript) (null script crashes like the exe) | no (usercall-style 0x464500, reviewed by asm only) |
| 2325 | SET_WEATHER_TO_APPROPRIATE_TYPE_NOW | 0x47679F | () -> CWeather::SetWeatherToAppropriateTypeNow | yes |
| 2326 | WINCH_CAN_PICK_OBJECT_UP | 0x4767B2 | obj, flag -> objectFlags (+0x140) bit 0x40000 (bCanBeAttachedToMagnet) | yes |
| 2327 | SWITCH_AUDIO_ZONE | 0x476810 | label(8) BEFORE the param, flag -> CAudioZones::SwitchAudioZone(label, flag!=0) (raw handler) | yes |
| 2328 | SET_CAR_ENGINE_ON | 0x47684E | car, flag -> inlined CVehicle::SetEngineOn(bool) 0x41BDD0 (broken engine => off) | yes |
| 2329 | SET_CAR_LIGHTS_ON | 0x47688A | car, flag -> bLightsOn (byte 0x428 bit 6) | yes |
| 2332 | GET_USER_OF_CLOSEST_MAP_ATTRACTOR | 0x4768EC | x,y,z,radius,model (0=>-1, <0=>UsedObjectArray) then label(8) -> FindClosestScriptedAttractor(type 5, no free-slot check, no exclude) 0x6002F0 -> GetPedUsingEffect -> ped handle or -1 | no (the 0x6002F0 search is the g22 port, tested by script_oracle_g22_test; here only reviewed) |
| 2335 | GET_PLANE_UNDERCARRIAGE_POSITION | 0x476BA1 | plane -> raw dword +0x9CC (m_fLandingGearStatus) | yes |
| 2339 | SWITCH_AMBIENT_PLANES | 0x476C9B | flag -> CPlane::SwitchAmbientPlanes(flag!=0) | yes |
| 2340 | SET_DARKNESS_EFFECT | 0x476CC6 | flag, alpha -> CPostEffects::ScriptDarknessFilterSwitch(flag!=0, alpha) | yes |
| 2342 | GET_NUMBER_OF_INSTANCES_OF_STREAMED_SCRIPT | 0x476D10 | idx(word) -> m_aScripts[proper].m_NumberOfUsers (byte, no bounds check) | yes |
| 2344 | ALLOCATE_STREAMED_SCRIPT_TO_RANDOM_PED | 0x476D3F | idx(word), model(word), priority(word) -> AddNewScriptBrain(proper, model, prio, 0, -1, -1.0f) | yes |
| 2345 | ALLOCATE_STREAMED_SCRIPT_TO_OBJECT | 0x476D99 | idx(word), model (signed: <0 => UsedObjectArray), priority(word), radius(float), grouping(byte) -> AddNewScriptBrain(proper, model, prio, 1, grouping, radius) | yes |
| 2347 | GET_GROUP_MEMBER | 0x476E09 | group, member id -> GetActualScriptThingIndex(group, 8); ms_groups[idx].membership.GetMember(id) -> handle / -1 | yes |
| 2350 | GET_WATER_HEIGHT_AT_COORDS | 0x476E6E | x, y, flag -> GetWaterLevel(x,y,0,&out,true,null) / GetWaterLevelNoWaves(...); not found => -1000.0f | yes |
| 2361 | ATTACH_CAR_TO_OBJECT | 0x477175 | car, obj, offset(3), rot deg(3) -> rot*0.017453292f (float), car->AttachEntityToEntity(obj, offset, rot) | yes |
| 2362 | SET_GARAGE_RESPRAY_FREE | 0x477239 | label(8) BEFORE the param, flag -> GetGarageNumberByName; idx>=0 => garage flags (+0x4E) bit 7 = flag!=0 (raw handler) | yes |
| 2363 | SET_CHAR_BULLETPROOF_VEST | 0x47729D | ped, flag -> bit 0 of dword +0x478 (bHasBulletProofVest) | yes |
| 2368 | SET_GROUP_FOLLOW_STATUS | 0x477323 | group, flag -> idx via GetActualScriptThingIndex(8); 0<=idx<8 => ms_groups[idx] byte +4 (m_bMembersEnterLeadersVehicle) | yes |
| 2369 | SET_SEARCHLIGHT_CLIP_IF_COLLIDING | 0x477372 | searchlight, flag -> idx via GetActualScriptThingIndex(2); idx>=0 (no upper bound) => m_bClipIfColliding | yes |
| 2374 | SET_CHAR_USES_UPPERBODY_DAMAGE_ANIMS_ONLY | 0x47744C | ped, flag -> bit 2 of dword +0x478 | yes |
| 2375 | SET_CHAR_SAY_CONTEXT | 0x47749A | ped, ctx(word) -> CPed::Say(ctx,0,1.0f,0,0,0) -> sign-extended int16 result | yes |
| 2376 | ADD_EXPLOSION_VARIABLE_SHAKE | 0x4774DA | x,y,z,type,shake -> AddExplosion(null,null,type,pos,0,1,shake,0) | yes |
| 2377 | ATTACH_MISSION_AUDIO_TO_CHAR | 0x477545 | slot(byte, -1), ped(ptr) -> AudioEngine.AttachMissionAudioToPed | yes |
| 2378 | UPDATE_PICKUP_MONEY_PER_DAY | 0x477584 | pickup, money(word) -> CPickups::UpdateMoneyPerDay | yes |
| 2379 | GET_NAME_OF_ENTRY_EXIT_CHAR_USED | 0x4775B3 | ped -> string var: unbounded byte copy of enex name (+0), "" when no enex | yes |
| 2380 | GET_POSITION_OF_ENTRY_EXIT_CHAR_USED | 0x47760C | ped -> x,y,z,angle: linked (+0x38) => exit pos + exit angle*deg2rad; else rect centre + angle 0; NO enex => StoreParameters(4) stores STALE ScriptParams (raw handler keeps this) | yes |
| 2381 | IS_CHAR_TALKING | 0x4776C3 | ped -> cmp CPed::GetPedTalking | yes |
| 2384 | SET_UP_SKIP | 0x47776C | x,y,z,angle -> CGameLogic::SetUpSkip(pos, angle, false, null, false) | yes |
| 2385 | CLEAR_SKIP | 0x4777CE | () -> CGameLogic::ClearSkip(false) | yes |
| 2386 | PRELOAD_BEAT_TRACK | 0x4777E6 | track(word->int16) -> AudioEngine.PreloadBeatTrack | yes |
| 2387 | GET_BEAT_TRACK_STATUS | 0x477811 | () -> sign-extended byte of GetBeatTrackStatus | yes |
| 2388 | PLAY_BEAT_TRACK | 0x477823 | () -> PlayPreloadedBeatTrack(false) | yes |
| 2389 | STOP_BEAT_TRACK | 0x47783D | () -> StopBeatTrack | yes |
| 2390 | FIND_MAX_NUMBER_OF_GROUP_MEMBERS | 0x477855 | () -> CStats::FindMaxNumberOfGroupMembers | yes |
| 2391 | VEHICLE_DOES_PROVIDE_COVER | 0x47785F | car, flag -> bDoesProvideCover (byte 0x42E bit 2) | yes |
| 2392 | CREATE_SNAPSHOT_PICKUP | 0x4778AA (cmd 0x958) | x,y,z -> IncrementStat(0xE8), z<=-100 => ground z + 0.5, GenerateNewOne(model MI_PICKUP_CAMERA, type 0x14) -> handle | yes |
| 2393 | CREATE_HORSESHOE_PICKUP | 0x4778AA (cmd 0x959) | same, stat 0xF2, MI_HORSESHOE, type 3 | yes |
| 2394 | CREATE_OYSTER_PICKUP | 0x4778AA (cmd 0x95A) | same, stat 0xF4, MI_OYSTER, type 3 | yes |
| 2395 | HAS_OBJECT_BEEN_UPROOTED | 0x4779A7 | obj -> cmp !GetIsStatic() | yes |
| 2396 | ADD_SMOKE_PARTICLE | 0x4779EC | pos(3), vel(3), r,g,b,a, size, life -> FxPrtMult_c(r,g,b,a,size,1.0f,life); g_fx.m_SmokeHuge->AddParticle(pos,vel,0,mults,-1,1.2f,0.6f,false) | yes |
| 2397 | IS_CHAR_STUCK_UNDER_CAR | 0x477AA6 | ped -> cmp bit 3 of dword +0x478 (bStuckUnderCar) | yes |
| 2398 | CONTROL_CAR_DOOR | 0x477AE5 | car, door, status, ratio (read as INT: fild) -> door<6: ratio>=0 => doors[door].Open(ratio), SetDoorStatus, SetDoorDamage(door,false); door>=6 (swinging chassis +0x70C): Open + state byte +0x717 (raw handler: raw dword whatever the literal type) | yes |
| 2399 | GET_DOOR_ANGLE_RATIO | 0x477B8C | car, door -> door<6: doors[door].m_angle (through x87: sNaN quieted); else chassis angle (+0x718) raw | yes |

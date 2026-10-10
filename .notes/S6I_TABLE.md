# S6-I table (g24, ids 2400..2499, 79 commands) - files `source/game_sa/Scripts/Commands/Ported/Group24a.cpp` (2400..2446), `Group24b.cpp` (2449..2499), `Group24.cpp/.hpp` (registration)

Group processor g24 `CRunningScript::ProcessCommands2400To2499` @0x478000 (table 0x8A6168 + 4*24), switch base 2400 (`lea eax,[edi-0x960]`), 100 entries, jump table 0x479AFC. All cases return `xor al, al` (OR_CONTINUE).
Columns: id | name | exe case | in -> out / notes | oracle (differentially tested against the exe code by `tests/standalone/script_oracle_g24_test.cpp`, 3000 random cases each, bit-exact).

| id | name | case | in -> out / notes | oracle |
|---|---|---|---|---|
| 2400 | SET_PLAYER_DISPLAY_VITAL_STATS_BUTTON | 0x47803E | pad, enable  -- CPad::GetPad(pad)->bDisablePlayerDisplayVitalStats (+0x11F) = (enable == 0) | yes |
| 2401 | SET_CHAR_KEEP_TASK | 0x478089 | ped, flag  -- bKeepTasksAfterCleanUp (+0x478 bit 4) = (flag != 0) | yes |
| 2404 | CREATE_MENU_GRID | 0x4780D1 | title label (8; "DUMMY" => ""), x, y, width, columns, interactive, background, alignment => menu id (zero-extended byte) Same as CREATE_MENU (2260) but with the grid menu type (1). x = x * (maximumWidth * 1/640) [0x859520], width = width * (maximumWidth * 1/640), y = y * (maximumHeight * 1/448) [0x859524] (all x87, stored as ... |  |
| 2405 | IS_CHAR_SWIMMING | 0x47819F | ped => compare flag: CPedIntelligence::GetTaskSwim() != null [0x601070] | yes |
| 2406 | GET_CHAR_SWIM_STATE | 0x4781E1 | ped => the swim task's state (+0xA, movsx word). The task is not null checked. |  |
| 2407 | START_CHAR_FACIAL_TALK | 0x478221 | ped, duration  -- the secondary facial task (slot 3) ->SetRequest(TALKING (7), duration, NONE (-1), 0) [0x691230]. Not null checked. |  |
| 2408 | STOP_CHAR_FACIAL_TALK | 0x478266 | ped  -- the secondary facial task (slot 3) ->StopAll() [0x691250]. Not null checked. |  |
| 2409 | IS_BIG_VEHICLE | 0x47829F | car => compare flag: bIsBig (+0x429 bit 2) | yes |
| 2410 | SWITCH_POLICE_HELIS | 0x4782D9 | enable  -- SwitchPoliceHelis(enable != 0) [0x6C4800] | yes |
| 2411 | STORE_CAR_MOD_STATE | 0x4782FD | CShopping::StoreVehicleMods() [0x49B280] | yes |
| 2412 | RESTORE_CAR_MOD_STATE | 0x478309 | CShopping::RestoreVehicleMods() [0x49B3C0] | yes |
| 2413 | GET_CURRENT_CAR_MOD | 0x478315 | car, slot => upgrade model (-1 if none). Jump table @0x479C8C (17 entries, the slot is compared unsigned): 0: GetUpgrade(0)   1: GetUpgrade(1) or (if -1) GetUpgrade(2)   2: (6)   3: (8) or (if -1) (9)   4: (10)   5: (11)   6: (12)   7: (14)   8: (15)   9: (16)   10: (17) 11: -1   12: GetReplacementUpgrade(2)   13: (0x13)   14... | yes |
| 2414 | IS_CAR_LOW_RIDER | 0x4784E6 | car => compare flag: handling flags (+0xD0) bit 25 (m_bLowRider) | yes |
| 2415 | IS_CAR_STREET_RACER | 0x478534 | car => compare flag: handling flags (+0xD0) bit 26 (m_bStreetRacer) | yes |
| 2416 | FORCE_DEATH_RESTART | 0x478581 | CGameLogic::ForceDeathRestart() [0x441240] | yes |
| 2417 | SYNC_WATER | 0x47858D | CWaterLevel::m_nWaterTimeOffset (0xC228A4) = CTimer::m_snTimeInMilliseconds (0xB7CB84) -- inlined in the exe | yes |
| 2418 | SET_CHAR_COORDINATES_NO_OFFSET | 0x478599 | ped, x, y, z  -- SetCharCoordinates(ped, pos, warpGang = true, offset = false) [0x464DC0] | yes |
| 2419 | DOES_SCRIPT_FIRE_EXIST | 0x4785C6 | handle => compare flag: GetActualScriptThingIndex(handle, SCRIPT_THING_FIRE) in [0, 60) | yes |
| 2420 | RESET_STUFF_UPON_RESURRECTION | 0x478605 | CGameLogic::ResetStuffUponResurrection() [0x442980] | yes |
| 2421 | IS_EMERGENCY_SERVICES_VEHICLE | 0x478611 | car => compare flag: IsLawEnforcementVehicle() [0x6D2370] // model 416 (ambulan) / 407 (firetruk) / 544 (firela) | yes |
| 2422 | KILL_FX_SYSTEM_NOW | 0x47866F | handle  -- idx = GetActualScriptThingIndex(handle, SCRIPT_THING_EFFECT_SYSTEM); if idx >= 0 and the slot's system (+4) is set: g_fxMan.DestroyFxSystem(system) [0x4A9810]; CTheScripts::RemoveScriptEffectSystem(handle) [0x492FD0]; if the script uses mission cleanup: RemoveEntityFromList(handle, 4 (particle)) |  |
| 2423 | IS_OBJECT_WITHIN_BRAIN_ACTIVATION_RANGE | 0x4786D6 | object => compare flag: the player in focus has a ped, the object exists and CScriptsForBrains::IsObjectWithinBrainActivationRange(object, FindPlayerCentreOfWorld(PlayerInFocus)) [0x46B3D0] |  |
| 2424 | COPY_SHARED_CHAR_DECISION_MAKER | 0x47875E | dm handle => dm handle (the output variable is peeked: if it already holds a valid decision maker handle (GetActualScriptThingIndex(peek, 7) != -1) it is kept, no copy is made). Otherwise the (script) decision maker is copied (type 0) like COPY_CHAR_DECISION_MAKER (2021). In both cases the handle is registered with the script... |  |
| 2426 | REPORT_MISSION_AUDIO_EVENT_AT_POSITION | 0x478805 | x, y, z, event (low word)  -- AudioEngine.ReportMissionAudioEvent(event, pos) [0x507340] | yes |
| 2427 | REPORT_MISSION_AUDIO_EVENT_AT_OBJECT | 0x47884B | object, event (low word)  -- AudioEngine.ReportMissionAudioEvent(event, object) [0x507350] (object may be null) | yes |
| 2428 | ATTACH_MISSION_AUDIO_TO_OBJECT | 0x478881 | slot, object  -- AudioEngine.AttachMissionAudioToObject((uint8)(slot - 1), object) [0x507320] (object may be null) | yes |
| 2429 | GET_NUM_CAR_COLOURS | 0x4788B8 | car => the model info's colour variation count (+0x2D0, zero-extended byte) | yes |
| 2432 | EXTINGUISH_FIRE_AT_POINT | 0x4788FB | x, y, z, radius  -- gFireManager.ExtinguishPoint(pos, radius) [0x539450] | yes |
| 2433 | HAS_TRAIN_DERAILED | 0x478952 | car => compare flag: +0x5B9 bit 0 (CTrain::trainFlags.bNotOnARailRoad, the vehicle is NOT checked to be a train) | yes |
| 2434 | SET_CHAR_FORCE_DIE_IN_CAR | 0x478988 | ped, flag  -- bForceDieInCar (+0x478 bit 7) = bit 0 of flag (shl 7; xor; and 0x80) | yes |
| 2435 | SET_ONLY_CREATE_GANG_MEMBERS | 0x4789BF | flag  -- CPopulation::m_bOnlyCreateRandomGangMembers (0xC0FCB3) = (flag == 0) (sic) | yes |
| 2436 | GET_OBJECT_MODEL | 0x4789DE | object => model index (+0x22, movsx word) | yes |
| 2437 | SET_CHAR_USES_COLLISION_CLOSEST_OBJECT_OF_TYPE | 0x478A13 | x, y, z, radius, model, flag, ped z <= -100 (ordered) => ground z. Objects of the model within `radius` are collected (FindObjectsOfTypeInRange(model, pos, radius, 2D, &count, 16, list, buildings, !vehicles, !peds, objects, dummies) [0x564C70]); the closest one (strictly nearer than 2 * radius, extended precision, the best di... | yes |
| 2438 | CLEAR_ALL_SCRIPT_FIRE_FLAGS | 0x478BB7 | gFireManager.ClearAllScriptFireFlags() [0x5397A0] | yes |
| 2439 | GET_CAR_BLOCKING_CAR | 0x478BC8 | car => vehicle handle of the car's obstructing entity (autopilot +0x90 = +0x420) if it is a vehicle ((flags & 7) == 2), else -1 | yes |
| 2440 | GET_CURRENT_VEHICLE_PAINTJOB | 0x478C1C | car => CVehicle::GetRemapIndex() [0x6D0B70] | yes |
| 2441 | SET_HELP_MESSAGE_BOX_SIZE | 0x478C53 | size  -- CHud::m_fHelpMessageBoxWidth (0x8D0934) = (float)size | yes |
| 2442 | SET_GUNSHOT_SENSE_RANGE_FOR_RIOT2 | 0x478C6F | range  -- CEventGunShot::ms_fGunShotSenseRangeForRiot2 (0x8A625C) = the raw dword | yes |
| 2443 | STRING_CAT16 | 0x478C89 | src1, src2, dst = three global string variables (GetPointerToScriptVariable(2) each). Only if strlen(src1) + strlen(src2) < 16 the third variable is read (IP not advanced otherwise!) and dst = src1 + src2 (strcpy, then append src2, its length measured after the first copy) | yes |
| 2444 | STRING_CAT8 | 0x478C89 (shared with 2443) | same as 2443 with the limit 8 (strlen(src1) + strlen(src2) < 8) | yes |
| 2445 | GET_CAR_MOVING_COMPONENT_OFFSET | 0x478D2F | car => CAutomobile::GetMovingCollisionOffset() [0x6A2150] (the vehicle is not checked to be an automobile) | yes |
| 2446 | SET_NAMED_ENTRY_EXIT_FLAG | 0x478D67 | name label (8, read FIRST), flag (word), enable  -- 0x43EF20(name, flag, enable != 0): sets / clears the flag of the entry exits with that name |  |
| 2449 | PAUSE_CURRENT_BEAT_TRACK | 0x478DA5 | pause (low byte)  -- AudioEngine.PauseBeatTrack(pause) [0x507200] | yes |
| 2450 | SET_PLAYER_CYCLE_WEAPON_BUTTON | 0x478DC7 | pad, enable  -- CPad::GetPad(pad)->bDisablePlayerCycleWeapon (+0x11D) = (enable == 0) | yes |
| 2452 | MARK_ROAD_NODE_AS_DONT_WANDER | 0x478E13 | x, y, z  -- ThePaths.MarkRoadNodeAsDontWander(x, y, z) [0x450560] | yes |
| 2453 | UNMARK_ALL_ROAD_NODES_AS_DONT_WANDER | 0x478E41 | ThePaths.UnMarkAllRoadNodesAsDontWander() [0x44D400] | yes |
| 2454 | SET_CHECKPOINT_HEADING | 0x478E52 | checkpoint, heading  -- idx = GetActualScriptThingIndex(handle, SCRIPT_THING_CHECKPOINT); if valid and the slot's checkpoint (+4) is set: CCheckpoints::SetHeading(checkpoint->m_ID, heading) [0x722970] |  |
| 2455 | SET_MISSION_RESPECT_TOTAL | 0x478EA4 | value  -- CStats::SetStatValue(STAT_RESPECT_MISSION_TOTAL (0xE4), (float)value) [0x55A070] | yes |
| 2456 | AWARD_PLAYER_MISSION_RESPECT | 0x478ECB | value  -- CStats::IncrementStat(STAT_RESPECT_MISSION (0xE0), (float)value) [0x55C180] | yes |
| 2458 | SET_CAR_COLLISION | 0x478EF2 | car, flag  -- m_bUsesCollision (+0x1C bit 0) and physicalFlags.bApplyGravity (+0x40 bit 1) = (flag != 0) | yes |
| 2459 | CHANGE_PLAYBACK_TO_USE_AI | 0x478F43 | car  -- CVehicleRecording::ChangeCarPlaybackToUseAI(car) [0x45A360] | yes |
| 2461 | IS_NIGHT_VISION_ACTIVE | 0x478F9C | => compare flag: CPostEffects::m_bNightVision (0xC402B8) | yes |
| 2462 | SET_CREATE_RANDOM_COPS | 0x478FB2 | flag  -- CPopulation::m_bDontCreateRandomCops (0xC0FCB4) = (flag == 0) | yes |
| 2463 | TASK_SET_IGNORE_WEAPON_RANGE_FLAG | 0x478FD2 | ped, flag (byte)  -- handle != -1: the ped's bIgnoreWeaponRange (+0x478 bit 9) = bit 0 of the flag byte, immediately; handle == -1: GivePedScriptedTask(-1, new CTaskSimpleSetCharIgnoreWeaponRangeFlag(flag byte) [0x474620], command) [0x465C20] (sequence task building) | yes |
| 2464 | TASK_PICK_UP_SECOND_OBJECT | 0x47905C | CRunningScript::ScriptTaskPickUpObject(command) [0x46AF50] (no CollectParameters before) |  |
| 2465 | DROP_SECOND_OBJECT | 0x47906B | ped, flag  -- if the ped's secondary task in slot 0 (TASK_SECONDARY_ATTACK) is a TASK_SIMPLE_HOLD_ENTITY (0x133, virtual +0x10): task->DropEntity(ped, flag != 0) [0x6930F0] then task->MakeAbortable(ped, ABORT_PRIORITY_URGENT, nullptr) (virtual +0x18) |  |
| 2466 | REMOVE_OBJECT_ELEGANTLY | 0x479107 | object  -- CTheScripts::CleanUpThisObject(obj) [0x4866C0] (null allowed); if the object exists: all atomics of its clump get CClumpModelInfo::SetAtomicRendererCB(atomic, CVisibilityPlugins::RenderFadingClumpCB) [RpClumpForAllAtomics 0x749B70] and objectFlags /= 0x400000 (bFadingIn); then (object or not) if the script uses mis... |  |
| 2467 | DRAW_CROSSHAIR | 0x47917C | flag  -- CTheScripts::bDrawCrossHair (0xA44490, dword) = (flag != 0) | yes |
| 2470 | SHOW_BLIPS_ON_ALL_LEVELS | 0x479203 | flag  -- CTheScripts::RadarShowBlipOnAllLevels (0xA444A2) = (flag != 0) | yes |
| 2471 | SET_CHAR_DRUGGED_UP | 0x479222 | ped, flag  -- bDruggedUp (+0x478 bit 10) = (flag != 0) | yes |
| 2472 | IS_CHAR_HEAD_MISSING | 0x47926D | ped => compare flag: the ped exists && bRemoveHead (+0x46D bit 7) && m_nBodypartToRemove (+0x754) == 2 | yes |
| 2473 | GET_HASH_KEY | 0x4792AF | text label (15 chars, no CollectParameters) => CKeyGen::GetUppercaseKey(label) [0x53CF30] | yes |
| 2475 | RANDOM_PASSENGER_SAY | 0x47931F | car, context (low word)  -- passenger = PickRandomPassenger() [0x6D2A10]; if there is one: Say(context, 0, 1.0f, false, false, false) [0x5EFFE0] | yes |
| 2476 | HIDE_ALL_FRONTEND_BLIPS | 0x47936D | flag  -- CTheScripts::HideAllFrontEndMapBlips (0xA444A1) = (flag != 0) | yes |
| 2478 | IS_CHAR_IN_ANY_TRAIN | 0x4793B0 | ped => compare flag: bInVehicle (+0x46D bit 0) && ped->m_pVehicle->m_nVehicleType (+0x590) == 6 (train) | yes |
| 2479 | SET_UP_SKIP_AFTER_MISSION | 0x479401 | x, y, z, heading  -- CGameLogic::SetUpSkip(pos, heading, afterMission = true, vehicle = null, finishedByScript = false) [0x4423C0] | yes |
| 2480 | SET_VEHICLE_IS_CONSIDERED_BY_PLAYER | 0x47945B | car, flag  -- bConsideredByPlayer (+0x42E bit 5) = (flag != 0) | yes |
| 2482 | GET_RANDOM_CAR_MODEL_IN_MEMORY | 0x47949F | flag => model (-1 if none), vehicle class (model info +0x4D, movsx byte; -1 if none) model = m_AppropriateLoadedCars.PickRandomCar(false, flag != 0) [0x611C50] | yes |
| 2483 | GET_CAR_DOOR_LOCK_STATUS | 0x479500 | car => m_nDoorLock (+0x4F8) | yes |
| 2484 | SET_CLOSEST_ENTRY_EXIT_FLAG | 0x479536 | x, y, radius, flag, enable  -- CEntryExitManager::SetEntryExitFlagWithIndex(FindNearestEntryExit((x, y), radius, -1) [0x43F4B0], flag, enable != 0) [0x43EF90] | yes |
| 2488 | ADD_BLOOD | 0x47966F | x, y, z, dirx, diry, dirz, count, ped  -- g_fx.AddBlood(pos, dir, count, ped->m_fContactSurfaceBrightness (+0x12C)) [0x49EB00]. The ped is not null checked. | yes |
| 2489 | DISPLAY_CAR_NAMES | 0x4796EC | flag  -- CHud::bScriptDontDisplayVehicleName (0xBAA3F9) = (flag == 0) | yes |
| 2491 | IS_CAR_DOOR_DAMAGED | 0x47972B | car, door => compare flag: CDamageManager::GetDoorStatus(door) != 0 [0x6C2230] | yes |
| 2493 | SET_MINIGAME_IN_PROGRESS | 0x4797B6 | flag  -- flag != 0: script->+0xC8 = true, bMiniGameInProgress (0xA444A8) = true, bDisplayNonMiniGameHelpMessages (0xA444A7) = false; flag == 0: the opposite | yes |
| 2494 | IS_MINIGAME_IN_PROGRESS | 0x479800 | => compare flag: CTheScripts::bMiniGameInProgress (0xA444A8) | yes |
| 2495 | SET_FORCE_RANDOM_CAR_MODEL | 0x479817 | model  -- CTheScripts::ForceRandomCarModel (0xA4448C) = model | yes |
| 2496 | GET_RANDOM_CAR_OF_TYPE_IN_ANGLED_AREA_NO_SAVE | 0x479831 | x1, y1, x2, y2, width, model => vehicle handle (-1 if none) The vehicle pool is scanned from the LAST slot to the first, until a match is found: an automobile / bike (GetVehicleAppearance 1 or 2) whose model is `model` (or `model` < 0 = any), that CanBeDeleted(), and whose position (matrix or placement) is inside the angled a... | yes |
| 2498 | FAIL_KILL_FRENZY | 0x47994B | CDarkel::FailKillFrenzy() [0x43DC60] | yes |
| 2499 | IS_COP_VEHICLE_IN_AREA_3D_NO_SAVE | 0x479957 | x1, y1, z1, x2, y2, z2 => compare flag: any vehicle (all pool slots are scanned, no early out) that IsLawEnforcementVehicle() [0x6D2370] and isn't model 430 (predator), whose position (matrix or placement) lies in the box (the pairs are sorted first: swapped only if a > b; NaN => outside) | yes |

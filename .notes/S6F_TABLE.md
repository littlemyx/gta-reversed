# S6-F table (g20..g21, ids 2000..2199, 99 commands) - files `source/game_sa/Scripts/Commands/Ported/Group20a.cpp` (2000..2043), `Group20b.cpp` (2045..2086), `Group21a.cpp` (2087..2148), `Group21b.cpp` (2150..2199), `Group20_21.cpp/.hpp` (registration)

Group processors: g20 0x472310 (switch base 2000, 100 entries), g21 0x470A90 (base 2100). All cases return `xor al, al` (OR_CONTINUE). Ids 2087..2099 are in g20, 2101.. in g21.
Columns: id | name | exe case | params in -> results out / notes | oracle (differentially tested against the exe code by `script_oracle_g20_21_test`, 3000 random cases each).

| id | name | case | in -> out / notes | oracle |
|---|---|---|---|---|
| 2000 | GET_CURRENT_DAY_OF_WEEK | 0x47234E | () -> CClock::CurrentDay (byte, zero-extended) | yes |
| 2003 | REGISTER_SCRIPT_BRAIN_FOR_CODE_USE | 0x47236A | int idx (CollectParameters 1), then 8-char label -> GetProperIndexFromIndexUsedByScript, AddNewStreamedScriptBrainForCodeUse(idx16, label, 3); label buffer given a NOTSA terminator |  |
| 2005 | APPLY_FORCE_TO_CAR | 0x4723C6 | car, f(3), off(3) -> off+=M*COM, dir=norm(f), eff.mass scale (extended), ApplyForce(f*scale, off, true); local x87-order Normalise/Cross/SqMag |  |
| 2006 | IS_INT_LVAR_EQUAL_TO_INT_VAR | 0x472510 | 2 vars by ref (local, global) -> cmp (int ==) | yes |
| 2007 | IS_FLOAT_LVAR_EQUAL_TO_FLOAT_VAR | 0x47253A | 2 vars by ref (local, global) -> cmp (float ==, NaN false) | yes |
| 2010 | ADD_TO_CAR_ROTATION_VELOCITY | 0x472576 | car, x,y,z -> turn += M*(v*0.02); static => SetIsStatic(false)+AddToMovingList | yes |
| 2011 | SET_CAR_ROTATION_VELOCITY | 0x472677 | car, x,y,z -> turn = M*(timeStep*v)*0.02; static => SetIsStatic(false)+AddToMovingList first | yes |
| 2013 | SET_CHAR_SHOOT_RATE | 0x472759 | ped, rate -> ped+0x719 = low byte | yes |
| 2014 | IS_MODEL_IN_CDIMAGE | 0x472786 | model (script::Model) -> cmp (streaming info cd size != 0) |  |
| 2015 | REMOVE_OIL_PUDDLES_IN_AREA | 0x4727B4 | x1,y1,x2,y2 -> sort pairs (swap if >), RemoveOilInArea(x1,x2,y1,y2) |  |
| 2016 | SET_BLIP_AS_FRIENDLY | 0x472845 | blip, flag -> SetBlipFriendly(blip, bit0 of low byte) |  |
| 2017 | TASK_SWIM_TO_COORD | 0x47286C | ped, pos -> if ped already in CTaskSimpleSwim only replaces its target (+0x14), else GivePedScriptedTask(new CTaskSimpleSwim(&pos,0)) |  |
| 2020 | GET_MODEL_DIMENSIONS | 0x472932 | model (script::Model) -> 6 raw floats (colmodel bbox min, max) |  |
| 2021 | COPY_CHAR_DECISION_MAKER | 0x4729A4 | dm handle -> new dm handle (copy via local 0x6070F0 port, type 0) + mission cleanup add (type 9) if script uses cleanup |  |
| 2022 | COPY_GROUP_DECISION_MAKER | 0x472A32 | dm handle -> new dm handle (type 1), same as 2021 |  |
| 2023 | TASK_DRIVE_POINT_ROUTE_ADVANCED | 0x472ABF | ped, veh (>=0 check), speed, mode, carModel(0=>-1,1=>0x19F), style -> GivePedScriptedTask(new CTaskComplexDrivePointRoute(veh, route 0xC18D50, speed, mode, model, -1.0f, style)) |  |
| 2024 | IS_RELATIONSHIP_SET | 0x472B88 | id, pedType, pedType2 -> cmp ((acquaintances & flag) != 0); flag inline per asm (negative => shl by low 5 bits) |  |
| 2031 | GET_CITY_FROM_COORDS | 0x472C1B | x,y,z -> GetLevelFromPosition (zero-extended byte) |  |
| 2032 | HAS_OBJECT_OF_TYPE_BEEN_SMASHED | 0x472C63 | x,y,z(auto ground),radius,model -> cmp (object pool last->first, bIsBroken, model, dist<=radius in extended) |  |
| 2035 | SET_CHECKPOINT_COORDS | 0x472E4A | handle, pos -> UpdatePos(checkpoint id, pos) if valid |  |
| 2037 | CONTROL_CAR_HYDRAULICS | 0x472EBC | car, 4 floats -> special hydraulic slot (acquire via GetSpecialColModel if <0) wheelSuspension[0..3] |  |
| 2038 | GET_GROUP_SIZE | 0x472F57 | group handle -> hasLeader(0/1), member count excl. leader (0,0 if invalid) |  |
| 2039 | SET_OBJECT_COLLISION_DAMAGE_EFFECT | 0x472FD4 | object, flag -> swap +0x144/+0x145 bytes | yes |
| 2040 | SET_CAR_FOLLOW_CAR | 0x47303D | car, target, radius -> CCarAI::TellCarToFollowOtherCar |  |
| 2043 | SWITCH_ENTRY_EXIT | 0x4730A8 | label(8) read first, then flag -> set/clear 0x4000 on all entry/exits with that name (local port of 0x43EF20) |  |
| 2045 | DOES_GROUP_EXIST | 0x4731CE | group => compare flag (GetActualScriptThingIndex(h, 8) in [0,8)) |  |
| 2046 | GIVE_MELEE_ATTACK_TO_CHAR | 0x47320D | ped, style, moves  -- ped+0x72D/0x72E; jump table 0x474520 (moves 1..6) | yes |
| 2047 | SET_CAR_HYDRAULICS | 0x4732B1 | car, on  -- Add/RemoveVehicleUpgrade(MI_HYDRAULICS word @0x8CD76C) |  |
| 2048 | IS_2PLAYER_GAME_GOING_ON | 0x4732FE | => compare flag (CGameLogic::IsCoopGameGoingOn) |  |
| 2051 | DOES_CAR_HAVE_HYDRAULICS | 0x473350 | car => compare flag  -- subtype(+0x594)==0 && handlingFlags(+0x38C)&0x20000 | yes |
| 2052 | TASK_CHAR_SLIDE_TO_COORD_AND_PLAY_ANIM | 0x4733A6 | CollectParameters(6): ped,x,y,z,heading,speed; 2 labels (24,16); CollectParameters(6): blend,b,c,d,e,time -> sequence{GoToPointAndStandStill, SlideToCoord} given to ped |  |
| 2054 | GET_TOTAL_NUMBER_OF_PEDS_KILLED_BY_PLAYER | 0x4735FD | player => count |  |
| 2058 | GET_LEVEL_DESIGN_COORDS_FOR_OBJECT | 0x473617 | object, index => x,y,z (FindTriggerPointCoors) |  |
| 2062 | GET_CHAR_HIGHEST_PRIORITY_EVENT | 0x473679 | ped => byte @CPedIntelligence+0xD1 (zero extended) | yes |
| 2064 | GET_PARKING_NODE_IN_AREA | 0x4736A6 | x1,y1,z1,x2,y2,z2 (axis pairs sorted) => x,y,z |  |
| 2066 | TASK_PLAY_ANIM_NON_INTERRUPTABLE | 0x473831 | no CollectParameters; PlayAnimScriptCommand(0x812) |  |
| 2068 | ADD_STUNT_JUMP | 0x473840 | 16 params: 2 boxes (centre,size), camera, reward; CStuntJumpManager::AddOne |  |
| 2069 | SET_OBJECT_COORDINATES_AND_VELOCITY | 0x473A01 | object,x,y,z  -- ground Z if z<=-100, speed=(new-cur)/timestep clamped [-1,1], Teleport, ClearSpaceForMissionEntity |  |
| 2070 | SET_CHAR_KINDA_STAY_IN_SAME_PLACE | 0x473BF5 | ped, on  -- bKindaStayInSamePlace (0x470070) | yes |
| 2071 | TASK_FOLLOW_PATROL_ROUTE | 0x473C38 | ped, moveState, mode  -- CTaskComplexFollowPatrolRoute with route 0xC18DB8 |  |
| 2072 | IS_CHAR_IN_AIR | 0x473CB3 | ped => compare flag  -- flags + simplest active task == TASK_SIMPLE_FALL (207) |  |
| 2073 | GET_CHAR_HEIGHT_ABOVE_GROUND | 0x473D38 | ped => pos.z - FindGroundZFor3DCoord(pos) |  |
| 2074 | SET_CHAR_WEAPON_SKILL | 0x473DA3 | ped, skill  -- byte @+0x72C | yes |
| 2077 | SET_CAR_ENGINE_BROKEN | 0x473E51 | car, broken  -- +0x42D bit1 set / +0x428 bit4 cleared (or only +0x42D bit1 cleared) | yes |
| 2078 | IS_THIS_MODEL_A_BOAT | 0x473EA8 | raw model => compare flag |  |
| 2079 | IS_THIS_MODEL_A_PLANE | 0x473EE1 | raw model => compare flag |  |
| 2080 | IS_THIS_MODEL_A_HELI | 0x473F19 | raw model => compare flag |  |
| 2083 | TASK_GREET_PARTNER | 0x473F8A | pedA, pedB, distMul(float), handshake(int) -> two CTaskComplexPartnerGreet |  |
| 2085 | SET_HELI_BLADES_FULL_SPEED | 0x4740DC | vehicle  -- subtype 3: +0x84C=0.22f; subtype 4: +0x9C4=0.18f | yes |
| 2086 | DISPLAY_HUD | 0x474128 | on -> CTheScripts::bDisplayHud | yes |
| 2087 | CONNECT_LODS | 0x474148 | objA, objB (handles); both looked up first, ScriptConnectLodsFunction(a,b), then AddToListOfConnectedLodObjects(objA,objB) |  |
| 2088 | SET_MAX_FIRE_GENERATIONS | 0x47418F | n -> dword 0xB728E0 (gFireManager.m_nMaxFireGenerationsAllowed) | yes |
| 2089 | TASK_DIE_NAMED_ANIM | 0x4741AB | ped; label(24); label(16); blend(float), flag -> CTaskSimpleDie(.., flag==0?0x10:0xD0, blend, 1.0) via GivePedScriptedTask |  |
| 2096 | SET_POOL_TABLE_COORDS | 0x4742AC | min xyz -> 0x8CDF00, max xyz -> 0x8CDEF4 (raw dwords) | yes |
| 2099 | HAS_OBJECT_BEEN_PHOTOGRAPHED | 0x474322 | object => compare flag; clears bIsPhotographed (+0x140 bit 0x1000) |  |
| 2101 | GET_CURRENT_DATE | 0x470B03 | no params => day, month (zero-extended bytes 0xB70154/55) | yes |
| 2111 | GET_CAR_UPRIGHT_VALUE | 0x470E15 | car => matrix up.z | yes |
| 2112 | SET_VEHICLE_AREA_VISIBLE | 0x470E5B | car, area -> byte +0x2F | yes |
| 2113 | SELECT_WEAPONS_FOR_VEHICLE | 0x470E86 | car, weapon -> byte +0x513 | yes |
| 2114 | GET_CITY_PLAYER_IS_IN | 0x470EB4 | player (ignored) => 0xBA6718 | yes |
| 2115 | GET_NAME_OF_ZONE | 0x470ED9 | x,y,z => strncpy(out var, zone+8, 8); no null check on zone |  |
| 2125 | ACTIVATE_INTERIOR_PEDS | 0x470F77 | on -> InteriorManager_c::ActivatePeds |  |
| 2126 | SET_VEHICLE_CAN_BE_TARGETTED | 0x470FAA | car, flag -> +0x42D bit 4 | yes |
| 2128 | TASK_FOLLOW_FOOTSTEPS | 0x470FF5 | ped, target -> CTaskComplexFollowPedFootsteps |  |
| 2129 | DAMAGE_CHAR | 0x47105C | ped, amount, armourFirst (x87 extended math; health<=0 -> CEventScriptCommand(CTaskComplexDie))  |  |
| 2130 | SET_CAR_CAN_BE_VISIBLY_DAMAGED | 0x471161 | car, flag -> +0x868 bit 0x10 (no type check) | yes |
| 2131 | SET_HELI_REACHED_TARGET_DISTANCE | 0x4711AB | car, dist -> byte +0x3DF (autoPilot.m_ucHeliTargetDist2) | yes |
| 2133 | GET_SOUND_LEVEL_AT_COORDS | 0x4711D9 | ped(-1=none), x,y,z => float GetEventGlobalGroup()->GetSoundLevel |  |
| 2136 | SET_HEADING_FOR_ATTACHED_PLAYER | 0x4712AB | player(ignored), heading, step -> 0xA4449C / 0xA44498 (deg->rad) | yes |
| 2137 | TASK_WALK_ALONGSIDE_CHAR | 0x4712DF | ped, target -> CTaskComplexWalkAlongsidePed(target,10.0) |  |
| 2138 | CREATE_EMERGENCY_SERVICES_CAR | 0x47134B | model (not translated), x,y,z => compare flag |  |
| 2139 | TASK_KINDA_STAY_IN_SAME_PLACE | 0x4713B5 | ped(-1=sequence), flag |  |
| 2142 | START_PLAYBACK_RECORDED_CAR_LOOPED | 0x471470 | car, path -> StartPlaybackRecordedCar(car,path,false,true) |  |
| 2145 | IS_ATTACHED_PLAYER_HEADING_ACHIEVED | 0x471504 | player(ignored) => step == 0.0f | yes |
| 2148 | ENABLE_ENTRY_EXIT_PLAYER_GROUP_WARPING | 0x47152E | x,y,radius,enable -> flag 0x100 on nearest EntryExit and its link; null EE crashes in exe |  |
| 2150 | GET_CLOSEST_STEALABLE_OBJECT | 0x4715B8 | x, y, z, radius => object handle or -1; FindObjectsInRange(objects, max 16), type 4 + objectFlags 0x2000, strictly closer than 2*radius; no compare flag |  |
| 2151 | IS_PROCEDURAL_INTERIOR_ACTIVE | 0x47173A | group => compare flag (g_interiorMan.IsGroupActive) |  |
| 2161 | SWITCH_START | 0x47176C | value, 2*numEntries(u16), hasDefault, defaultLabel; then 14 params = 7 pairs into the switch table; at the end UseSwitchJumpTable + UpdatePC |  |
| 2162 | SWITCH_CONTINUED | 0x47176C | 18 params = 9 pairs (shared case with 2161) |  |
| 2163 | REMOVE_CAR_RECORDING | 0x47188A | file number |  |
| 2165 | SET_OBJECT_ONLY_DAMAGED_BY_PLAYER | 0x471916 | object, flag -- physicalFlags bit 0x400000 (+0x40); handle unchecked | yes |
| 2166 | CREATE_BIRDS | 0x47195E | 8 ints (start xyz, target xyz as fild floats, count, biome) -> CBirds::CreateNumberOfBirds(..., false) |  |
| 2168 | SET_VEHICLE_DIRT_LEVEL | 0x471A00 | car, level -- raw dword to +0x4B0; handle unchecked | yes |
| 2169 | SET_GANG_WARS_ACTIVE | 0x471A2D | active (!= 0) |  |
| 2170 | IS_GANG_WAR_GOING_ON | 0x471A61 | => compare flag |  |
| 2171 | GIVE_PLAYER_CLOTHES_OUTSIDE_SHOP | 0x471A70 | player (unchecked index), texture(16), model(16), part; SetTextureAndModel then SetPlayerHasBought(key(texture)) |  |
| 2172 | CLEAR_LOADED_SHOP | 0x471AF2 | no params |  |
| 2173 | SET_GROUP_SEQUENCE | 0x471AFE | group, sequence -- ms_groups[g]+0x2CC (m_TaskSeqId) = -1 or GetActualScriptThingIndex(seq, 4); g via GetActualScriptThingIndex(.,8) in [0,8) |  |
| 2180 | REGISTER_ATTRACTOR_SCRIPT_BRAIN_FOR_CODE_USE | 0x471C8F | streamed index, name(8) -> AddNewStreamedScriptBrainForCodeUse(proper idx, name, 5); name buffer NUL-terminated (NOTSA) |  |
| 2183 | SET_HEADING_LIMIT_FOR_ATTACHED_CHAR | 0x471CE9 | ped, dir(u16 at +0x778), limit*0.017453292f (+0x77C); handle unchecked | yes |
| 2184 | ADD_BLIP_FOR_DEAD_CHAR | 0x471D2B | char, <blip var> => 1 blip (identical to ADD_BLIP_FOR_CHAR) |  |
| 2186 | TASK_PLAY_ANIM_WITH_FLAGS | 0x471E0C | PlayAnimScriptCommand(cmd) |  |
| 2187 | SET_VEHICLE_AIR_RESISTANCE_MULTIPLIER | 0x471E1B | car, mult -- +0x98 from handling drag (+0x10): >0.01 ? drag/1000*0.5 : drag; then *= mult; no handling -> nothing | yes |
| 2188 | SET_CAR_COORDINATES_NO_OFFSET | 0x471EA5 | car, x, y, z -> SetCoordsOfScriptCar(car, x, y, z, 0, 0) |  |
| 2189 | SET_USES_COLLISION_OF_CLOSEST_OBJECT_OF_TYPE | 0x471EE8 | x, y, z(auto ground), radius, model, flag; closest of any found entity (< 2*radius) gets m_bUsesCollision = flag |  |
| 2190 | SET_TIME_ONE_DAY_FORWARD | 0x47207D | no params -> CClock::OffsetClockByADay(1) |  |
| 2192 | SET_TIMER_BEEP_COUNTDOWN_TIME | 0x47208E | <global var>, secs |  |
| 2195 | ATTACH_TRAILER_TO_CAB | 0x4720BB | trailer, cab -> trailer->SetTowLink(cab, true) if both exist |  |
| 2199 | IS_VEHICLE_TOUCHING_OBJECT | 0x472100 | car, object => compare flag (car null -> false; GetHasCollidedWith) |  |

# S6-B table (g9..g12, ids 900..1299, 118 commands) - files `source/game_sa/Scripts/Commands/Ported/Group09.cpp`, `Group10.cpp`, `Group11.cpp`, `Group12.cpp`, `Group09_12.cpp` (registration), `Group09_12.hpp` (helpers)

Group processors: g9 0x483BD0 (switch base 900), g10 0x489500 (base 1000), g11 0x48A320 (**base 1101**, 99 entries: 1100 is not in the switch), g12 0x48B590 (base 1200). All cases return `xor al, al` (OR_CONTINUE).
Columns: id | name | exe case | params in -> results out / notes (compare flag = `UpdateCompareFlag`, applied through the parser incl. m_NotFlag) | oracle (differentially tested against the exe code by `script_oracle_test`, 3000 random cases each).

| id | name | case | in -> out / notes | oracle |
|---|---|---|---|---|
| 906 | IS_POINT_OBSCURED_BY_A_MISSION_ENTITY | 0x483C8C | x, y, z, rx, ry, rz => compare flag |  |
| 908 | ADD_TO_OBJECT_VELOCITY | 0x483DF6 | object, x, y, z | yes |
| 914 | SET_OBJECT_DYNAMIC | 0x4841E3 | object, dynamic | yes |
| 916 | PLAY_MISSION_PASSED_TUNE | 0x4842AF | tune (only 1 and 2 play something) | yes |
| 917 | CLEAR_AREA | 0x4842EC | x, y, z, radius, flag | yes |
| 918 | FREEZE_ONSCREEN_TIMER | 0x484370 | freeze | yes |
| 919 | SWITCH_CAR_SIREN | 0x48438F | car, on  -- +0x42D bit 0x80 | yes |
| 924 | SET_CAR_WATERTIGHT | 0x4843DA | car, on  -- automobiles (+0x868 bit 4) and bikes (+0x614 bit 4) only | yes |
| 927 | TURN_CAR_TO_FACE_COORD | 0x4844B8 | car, x, y | yes |
| 929 | DRAW_SPHERE | 0x484562 | x, y, z, radius | yes |
| 930 | SET_CAR_STATUS | 0x484607 | car, status | yes |
| 939 | SET_CAR_STRONG | 0x48470F | car, strong  -- +0x429 bit 0x80 | yes |
| 943 | SWITCH_STREAMING | 0x48478E | on | yes |
| 944 | IS_GARAGE_OPEN | 0x4847AD | garage name => compare flag | yes |
| 945 | IS_GARAGE_CLOSED | 0x4847E4 | garage name => compare flag | yes |
| 950 | SWAP_NEAREST_BUILDING_MODEL | 0x48481B | x, y, z, radius, from model, to model |  |
| 951 | SWITCH_WORLD_PROCESSING | 0x4849CC | on  -- sets `CWorld::bProcessCutsceneOnly = !on` | yes |
| 954 | CLEAR_AREA_OF_CARS | 0x4849EB | x1, y1, z1, x2, y2, z2 (corners are sorted) | yes |
| 956 | ADD_SPHERE | 0x484AB4 | x, y, z, radius => 1 handle The exe also peeks the output variable (`CollectNextParameterWithoutIncreasingPC`) and calls `GetActualScriptThingIndex(v, 0)`, but throws the result away (both are pure), so they are not reproduced. | yes |
| 957 | REMOVE_SPHERE | 0x484B77 | handle | yes |
| 963 | DISPLAY_ONSCREEN_TIMER_WITH_STRING | 0x484C57 | var, direction, label The variable is read first (as a global variable *index*), then the direction, then the 8 character GXT key. |  |
| 964 | DISPLAY_ONSCREEN_COUNTER_WITH_STRING | 0x484CB3 | var, type, label |  |
| 965 | CREATE_RANDOM_CAR_FOR_CAR_PARK | 0x484D08 | x, y, z, heading NOTE: unlike `CCarCtrl::GetNewVehicleDependingOnCarModel` boats and trains (types 5..8) fall into the default (CAutomobile) here. |  |
| 967 | SET_WANTED_MULTIPLIER | 0x485003 | multiplier |  |
| 969 | IS_CAR_VISIBLY_DAMAGED | 0x485037 | car => compare flag  -- +0x42A bit 1 | yes |
| 976 | HAS_MISSION_AUDIO_LOADED | 0x4851C7 | slot (1-based) => compare flag | yes |
| 978 | HAS_MISSION_AUDIO_FINISHED | 0x485228 | slot (1-based) => compare flag | yes |
| 979 | GET_CLOSEST_CAR_NODE_WITH_HEADING | 0x485261 | x, y, z => x, y, z, heading (4 values) + compare flag |  |
| 983 | SET_MISSION_AUDIO_POSITION | 0x4853D9 | slot (1-based), x, y, z | yes |
| 984 | ACTIVATE_SAVE_MENU | 0x48542C | no params |  |
| 985 | HAS_SAVE_GAME_FINISHED | 0x48548A | => compare flag |  |
| 988 | ADD_BLIP_FOR_PICKUP | 0x4854B9 | pickup => 1 blip handle The exe also calls `CPickups::GetActualPickupIndex(pickup)` and `CRadar::GetActualBlipArrayIndex(<output var>)`; both are pure and their results unused. |  |
| 1005 | SET_UPSIDEDOWN_CAR_NOT_DAMAGED | 0x48954A | car, flag -- the exe writes the CAutomobile flag byte (+0x868 bit 8) for ANY vehicle type | yes |
| 1011 | GET_CAR_COLOURS | 0x48963E | car => primary, secondary (2 values, zero-extended bytes at +0x434 / +0x435) | yes |
| 1012 | SET_ALL_CARS_CAN_BE_DAMAGED | 0x489683 | flag |  |
| 1013 | SET_CAR_CAN_BE_DAMAGED | 0x4896E8 | car, flag  -- +0x42A bit 0x20 | yes |
| 1021 | SET_DRUNK_INPUT_DELAY | 0x489731 | pad, delay  -- `CPad::GetPad(a)->SetDrunkInputDelay(b)` (0x53FB70, 0x53F910) | yes |
| 1024 | GET_OFFSET_FROM_OBJECT_IN_WORLD_COORDS | 0x48979F | object, x, y, z => x, y, z  (no null matrix check in the exe) |  |
| 1031 | GET_OFFSET_FROM_CAR_IN_WORLD_COORDS | 0x489831 | car, x, y, z => x, y, z |  |
| 1036 | IS_GERMAN_GAME | 0x4898B4 | => compare flag |  |
| 1037 | CLEAR_MISSION_AUDIO | 0x4898CB | slot (1-based) | yes |
| 1044 | SET_FREE_HEALTH_CARE | 0x4898F0 | player, flag  -- `CWorld::Players[player].m_bFreeHealthCare` (+0x152) | yes |
| 1048 | SET_OBJECT_DRAW_LAST | 0x489AAE | object, flag  -- entity flags bit 0x4000 (m_bDrawLast) | yes |
| 1053 | SET_NEAR_CLIP | 0x489B53 | nearClip | yes |
| 1054 | SET_RADIO_CHANNEL | 0x489B74 | channel (-1 = nothing; 11 is bumped to 12; then `inc al` -> id + 1 in the low byte only) | yes |
| 1059 | SET_CAR_TRACTION | 0x489BAE | car, traction -- automobiles: +0x8A0 (m_fCarTraction), ANY other type: +0x794 (bike traction) | yes |
| 1060 | ARE_MEASUREMENTS_IN_METRES | 0x489BF3 | => compare flag |  |
| 1061 | CONVERT_METRES_TO_FEET | 0x489C1A | metres => feet (float); 0x859F60 = 3.3333333f | yes |
| 1064 | SET_CAR_AVOID_LEVEL_TRANSITIONS | 0x489C48 | car, flag  -- autopilot flags byte +0x3DB bit 4 | yes |
| 1067 | CLEAR_AREA_OF_CHARS | 0x489C92 | x1, y1, z1, x2, y2, z2 (corners are sorted) | yes |
| 1068 | SET_TOTAL_NUMBER_OF_MISSIONS | 0x489D5B | count  -- `CStats::SetStatValue(STAT_TOTAL_NUMBER_OF_MISSIONS_IN_GAME (0x94), (float)count)` | yes |
| 1069 | CONVERT_METRES_TO_FEET_INT | 0x489D82 | metres => feet (int, truncated by `_ftol`) | yes |
| 1070 | REGISTER_FASTEST_TIME | 0x489DB4 | stat, time | yes |
| 1073 | IS_CAR_PASSENGER_SEAT_FREE | 0x489E73 | car, seat => compare flag  (signed seat compare and unchecked array index, as in the exe) | yes |
| 1076 | START_CREDITS | 0x489F56 |  |  |
| 1077 | STOP_CREDITS | 0x489F62 |  |  |
| 1078 | ARE_CREDITS_FINISHED | 0x489F6E | => compare flag | yes |
| 1084 | SET_MUSIC_DOES_FADE | 0x489F94 | flag  -- `TheCamera.m_bIgnoreFadingStuffForMusic` (+0x25) = !flag | yes |
| 1089 | GET_CAR_MODEL | 0x489FB3 | car => model index (sign-extended word at +0x22) | yes |
| 1107 | SET_OBJECT_ROTATION | 0x48A350 | object, x, y, z (degrees) | yes |
| 1120 | SET_INTERPOLATION_PARAMETERS | 0x48A723 | stopMoving (float), time (int) | yes |
| 1126 | SET_CAR_STAY_IN_FAST_LANE | 0x48A8A8 | car, flag  -- autopilot flags byte +0x3DB bit 8 | yes |
| 1128 | CLEAR_CAR_LAST_WEAPON_DAMAGE | 0x48A93B | car (the handle may be invalid) | yes |
| 1132 | GET_DRIVER_OF_CAR | 0x48A974 | car => ped handle (-1 if none) | yes |
| 1133 | GET_NUMBER_OF_FOLLOWERS | 0x48A9E3 | ped (may be invalid) => number of group members excluding the leader (0 if the ped isn't a leader) |  |
| 1139 | LOCATE_CHAR_IN_CAR_OBJECT_2D | 0x48AB19 | `LocateCharObjectCommand(command)` does everything |  |
| 1142 | LOCATE_CHAR_IN_CAR_OBJECT_3D | 0x48AB19 | `LocateCharObjectCommand(command)` does everything |  |
| 1143 | SET_CAR_TEMP_ACTION | 0x48AB30 | car, action (byte), time  -- the action lasts until now + time | yes |
| 1156 | GET_REMOTE_CONTROLLED_CAR | 0x48AC3E | player => car handle (-1 if none). The player index is used as is (no -1 handling) | yes |
| 1157 | IS_PC_VERSION | 0x48AC7B | => true | yes |
| 1162 | SET_ENABLE_RC_DETONATE | 0x48AD30 | enable  -- `CVehicle::bDisableRemoteDetonation = !enable` | yes |
| 1163 | SET_CAR_RANDOM_ROUTE_SEED | 0x48AD58 | car, seed (word) | yes |
| 1164 | IS_ANY_PICKUP_AT_COORDS | 0x48AD8F | x, y, z => compare flag (an active pickup closer than 0.5 units) | yes |
| 1172 | GET_POSITION_OF_ANALOGUE_STICKS | 0x48AEDC | pad => left X, left Y, right X, right Y (sign-extended words) | yes |
| 1173 | IS_CAR_ON_FIRE | 0x48AF33 | car => compare flag | yes |
| 1174 | IS_CAR_TYRE_BURST | 0x48AFBB | car, tyre => compare flag Bikes: tyre 4 = any of the 2 wheels, 2 = wheel 0, 3 = wheel 1 (other values index the bytes at +0x65C directly). Others: tyre 4 = any of the 4 wheels. | yes |
| 1186 | HELI_GOTO_COORDS | 0x48B070 | heli, x, y, z, minAltitude, maxAltitude (all floats) | yes |
| 1187 | IS_INT_VAR_EQUAL_TO_CONSTANT | 0x48B0DC | var, constant => compare flag  (`GetPointerToScriptVariable` + `CollectParameters(1)`) | yes |
| 1188 | IS_INT_LVAR_EQUAL_TO_CONSTANT | 0x48B100 | lvar, constant => compare flag |  |
| 1190 | CREATE_PROTECTION_PICKUP | 0x48B19D | x, y, z, ammo, moneyPerDay => pickup handle (the exe also peeks the output variable and calls the pure `CPickups::GetActualPickupIndex` on it, the result is unused) |  |
| 1209 | GET_CLOSEST_STRAIGHT_ROAD | 0x48B7B5 | x, y, z, minDist, maxDist => 7 values (node A pos, node B pos, distance) + compare flag No ground-Z lookup here. If the pair isn't found (or node B can't be resolved) all 7 values are 0. |  |
| 1210 | SET_CAR_FORWARD_SPEED | 0x48B900 | car, speed  -- move speed = forward vector * (speed / 60) (0x859044); heli handling on an automobile also gets its rotor spinning | yes |
| 1211 | SET_AREA_VISIBLE | 0x48B98D | area | yes |
| 1213 | MARK_CAR_AS_CONVOY_CAR | 0x48B9B0 | car, flag  -- +0x42B bit 8 | yes |
| 1216 | CREATE_SCRIPT_ROADBLOCK | 0x48B9FB | x1, y1, z1, x2, y2, z2, gangRoadblock |  |
| 1217 | CLEAR_ALL_SCRIPT_ROADBLOCKS | 0x48BA8B |  |  |
| 1230 | ADD_SHORT_RANGE_SPRITE_BLIP_FOR_COORD | 0x48BC0A | x, y, z, sprite => blip handle (the exe also peeks the output variable and calls the pure `CRadar::GetActualBlipArrayIndex` on it, the result is unused) |  |
| 1232 | SET_HELI_ORIENTATION | 0x48BCC6 | heli, angle (degrees; `(angle + 90) * pi/180`, wrapped into [0, 2pi]) | yes |
| 1234 | PLANE_GOTO_COORDS | 0x48BDA3 | plane, x, y, z, altitudeMin, altitudeMax | yes |
| 1235 | GET_NTH_CLOSEST_CAR_NODE | 0x48BDED | x, y, z, n => x, y, z + compare flag (0, 0, 0 if not found) |  |
| 1237 | DRAW_WEAPONSHOP_CORONA | 0x48BEF2 | x, y, z, size, type, flare, r, g, b |  |
| 1238 | SET_ENABLE_RC_DETONATE_ON_CONTACT | 0x48BFC3 | enable  -- `bDisableRemoteDetonationOnContact = !enable` | yes |
| 1241 | SET_OBJECT_RECORDS_COLLISIONS | 0x48C07B | object, flag  -- physical flags bit 0x10000000 | yes |
| 1242 | HAS_OBJECT_COLLIDED_WITH_ANYTHING | 0x48C0C3 | object => compare flag  (`CPhysical::m_nNumEntitiesCollided` +0xB9 > 0) | yes |
| 1247 | SET_HELI_STABILISER | 0x48C165 | heli, flag  -- +0x42B bit 0x10 | yes |
| 1248 | SET_CAR_STRAIGHT_LINE_DISTANCE | 0x48C1B0 | car, distance (byte)  -- autopilot +0x3DD | yes |
| 1249 | POP_CAR_BOOT | 0x48C1DD | car (any vehicle type, the exe calls CAutomobile::PopBoot directly) | yes |
| 1252 | REQUEST_COLLISION | 0x48C244 | x, y  -- `CColStore::RequestCollision({x, y, 0}, CGame::currArea)` | yes |
| 1253 | LOCATE_OBJECT_2D | 0x48C28F |  |  |
| 1254 | LOCATE_OBJECT_3D | 0x48C28F |  |  |
| 1255 | IS_OBJECT_IN_WATER | 0x48C29E | object (may be invalid) => compare flag  -- physical flags bit 0x100 (submerged in water) | yes |
| 1257 | IS_OBJECT_IN_AREA_2D | 0x48C2E9 |  |  |
| 1258 | IS_OBJECT_IN_AREA_3D | 0x48C2E9 |  |  |
| 1259 | TASK_TOGGLE_DUCK | 0x48C2F8 | ped, mode  -- `GivePedScriptedTask(ped, new CTaskSimpleDuckToggle(mode), command)` |  |
| 1261 | REQUEST_ANIMATION | 0x48C351 | name (16 chars) |  |
| 1262 | HAS_ANIMATION_LOADED | 0x48C391 | name (16 chars) => compare flag  (the exe doesn't check for an unknown block) |  |
| 1263 | REMOVE_ANIMATION | 0x48C3D5 | name (16 chars) |  |
| 1265 | IS_CAR_WAITING_FOR_WORLD_COLLISION | 0x48C45C | car => compare flag  -- entity flags bit 0x40000 | yes |
| 1271 | DISPLAY_NTH_ONSCREEN_COUNTER_WITH_STRING | 0x48C52F | var, type, line (1-based), label |  |
| 1273 | SET_EXTRA_COLOURS | 0x48C65A | colour (1-based), flag | yes |
| 1274 | CLEAR_EXTRA_COLOURS | 0x48C69D | flag | yes |
| 1276 | GET_WHEELIE_STATS | 0x48C6D1 | player => 6 values (car 2 wheels time/dist, bike wheelie time/dist, bike stoppie time/dist); they are reset to 0 afterwards | yes |
| 1278 | BURST_CAR_TYRE | 0x48C768 | car, tyre  -- virtual `CVehicle::BurstTyre(tyre, true)`; for bikes 2 -> 0 and 3 -> 1 | yes |
| 1283 | CREATE_SWAT_ROPE | 0x48C863 | pedType, model, x, y, z => ped handle Creates a ped hanging from a swat rope (task `CTaskComplexUseSwatRope`), standing still as the default task; no ground-Z lookup. |  |
| 1286 | SET_CAR_MODEL_COMPONENTS | 0x48C9F4 | model (unused), comp1, comp2  -- `CVehicleModelInfo::ms_compsToUse[0..1]` (bytes at 0x8A6458/9) | yes |
| 1288 | CLOSE_ALL_CAR_DOORS | 0x48CA1C | car (any type, the exe calls CAutomobile::CloseAllDoors directly) | yes |
| 1294 | SORT_OUT_OBJECT_COLLISION_WITH_CAR | 0x48CB16 | object, car  -- `object->m_pEntityIgnoredCollision (+0x128) = car` (car may be an invalid handle -> null) | yes |
| 1295 | GET_MAX_WANTED_LEVEL | 0x48CB51 | => max wanted level (0x8CDEE4) | yes |

Oracle-tested: 80 commands.

## Notes / deviations
* No exe no-ops or traps in g9..g12: every one of the 118 cases is a real implementation (no REGISTER_COMMAND_NOP/UNIMPLEMENTED needed).
* Pure dead calls of the exe not reproduced (results unused): `GetActualScriptThingIndex` + `CollectNextParameterWithoutIncreasingPC` (956, 1230 via GetActualBlipArrayIndex), `GetActualPickupIndex` (988, 1190).
* 1283 CREATE_SWAT_ROPE: the exe passes a stale (never written) stack vector to ClearSpaceForMissionEntity; the port passes the real position (intentional).
* 965 / 1283 / 1209...: null-pool-full crashes of the exe are guarded behind `notsa::IsFixBugs()`.
* 1232, 908, 927, 1190: x87 extended-precision intermediates are float-rounded per step in the port (<= 1 ulp, PC=24 project rule).
* Found by the oracle: 1089 GET_CAR_MODEL is a `movsx` word; 1276 returns raw dwords (float copies quiet sNaNs).
* Found by review: 1012 radius is 4000.0f (0x457A0000); 965 height uses virtual `GetHeightAboveRoad` (slot 0xD4).
* Callee fixed: `CWorld::FindMissionEntitiesIntersectingCubeSectorList` (0x565300) was ported wrong (point-in-box, `*outCount++` bug, no count with null buffer): now the bounding-sphere test + count of the asm (used by 906 and StuckCarCheck).
* Oracle: `tests/standalone/script_oracle_test.cpp` (ninja target `script_oracle_test`, EXCLUDE_FROM_ALL), 80 commands x 3000 random cases bit-exact vs the exe group processors (memory, ScriptSpace, IP, compare flag, callee log).

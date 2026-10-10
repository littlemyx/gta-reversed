# S6-G table (g22, 78 commands, ids 2200..2299) - files `source/game_sa/Scripts/Commands/Ported/Group22a.cpp` (2200..2249), `Group22b.cpp` (2251..2299), shared helpers `Group22.hpp`

Group processor: ProcessCommands2200To2299 @0x474900 (switch base 2200 = `lea eax,[ecx-0x898]`, 100-entry dword jump table @0x476140, no index table). Every case ends `xor al, al` (OR_CONTINUE) except
2208 (OR_INTERRUPT in the external-script branch). Oracle: `tests/standalone/script_oracle_g22_test.cpp` (exe oracle, see the end).
Columns: id | name | exe case address | params in | out | notes. `h` = pool handle (GetAtRef, NO null check unless said), `f` float, `i` int.

| id | name | case | in | out | notes |
|---|---|---|---|---|---|
| 2200 | ENABLE_CRANE_CONTROLS | 0x47493E | i raise, i lower, i release | - | bytes 0xA44496 / 0xA44495 / 0xA44494 = `!= 0` |
| 2206 | GET_RANDOM_CHAR_IN_SPHERE_ONLY_DRUGS_BUYERS | 0x474A04 | f x,y,z, f radius | 1 h | peds last->first slot: createdBy==1, !bRemoveFromWorld, !bFadeOut, !IsPedDead, model info (+0x32 word, `CPedModelInfo::m_nPedFlags`) bit 0, no ped group; dist = ((x2+y2)+z2) sqrt in extended precision, `dist < radius` unrounded, best is an INT32 (9999 initially; `fild best; fcomp (float)dist`; new best = `_ftol2(dist)`); winner: SetCharCreatedBy(2), ms_nTotalMissionPeds++, mission cleanup AddEntityToList(h, 2) when the script uses cleanup |
| 2207 | GET_PED_TYPE | 0x474BD4 | h | 1 i | ped+0x598 |
| 2208 | TASK_USE_CLOSEST_MAP_ATTRACTOR | 0x474C0A | h ped (-1 = sequence), f radius, i model (0 => -1, <0 => UsedObjectArray), f x,y,z (only for ped -1), label(8) | cmp | search = port of the exe's unnamed 0x6002F0 (`FindClosestScriptedAttractor`); not found: cmp false; found + script `m_ExternalType != -1` (+0xC9): RemoveScriptFromList(active), AddScriptToList(idle), ShutdownThisScript, `CEventAttractor(effect, entity, true)` with task id 0xE9 (TASK_COMPLEX_USE_EFFECT) added to the ped's event group, `SetEffectInUse`, returns OR_INTERRUPT (no cmp update; no null check on the ped); else `GivePedScriptedTask(h, new CTaskComplexUseEffect(effect, entity), cmd)` + cmp true |
| 2211 | CAN_TRIGGER_GANG_WAR_WHEN_ON_A_MISSION | 0x474E88 | i | - | byte 0x96AB93 |
| 2212 | CONTROL_MOVABLE_VEHICLE_PART | 0x474EA8 | h car, f angle | - | `CAutomobile::UpdateMovingCollision(angle)` (0x6A1460), no type check (the exe leaves the 2nd param on the stack for the callee) |
| 2213 | WINCH_CAN_PICK_VEHICLE_UP | 0x474ED7 | h car, i | - | +0x42D bit 0x10 (`bWinchCanPickMeUp`) |
| 2214 | OPEN_CAR_DOOR_A_BIT | 0x474F22 | h car, i door, f ratio | - | as 1623 with the ratio: node = GetCarNodeIndexFromDoor; IsDoorMissing(door) (slot 38) / `m_aCarNodes[node]` null => nothing; OpenDoor(null,node,door,ratio,true) (slot 27) |
| 2215 | IS_CAR_DOOR_FULLY_OPEN | 0x474F8C | h car, i door | cmp | virtual slot 33 = `IsDoorFullyOpenU32(NODE)` |
| 2216 | SET_ALWAYS_DRAW_3D_MARKERS | 0x474FD9 | i | - | C3dMarkers::ForceRender(i != 0) |
| 2217 | STREAM_SCRIPT | 0x47500D | i (low 16 bits) | - | idx = StreamedScripts.GetProperIndexFromIndexUsedByScript (movsx, written to ScriptParams[0]); RequestModel(0x6676 + idx, 4) (no check for -1) |
| 2219 | HAS_STREAMED_SCRIPT_LOADED | 0x475049 | i (low 16 bits) | cmp | streaming info of 0x6676 + idx has load state 1 (`cmp byte [info+0x10], 1`) |
| 2220 | SET_GANG_WARS_TRAINING_MISSION | 0x475085 | i | - | byte 0x96AB91 |
| 2221 | SET_CHAR_HAS_USED_ENTRY_EXIT | 0x4750A5 | h ped, f x,y, f radius | - | ee = GetInSlot(FindNearestEntryExit((x,y), radius, -1)); link = ee->m_pLink ? that : ee (and `link->m_pLink = ee`!); ped areaCode (+0x2F) = link.area; ped.m_pEnex (+0x78C) = link.area != 0 ? ee : null; if ped->IsPlayer(): stack pos = 0, AddEntryExitToStack(ee), sky colour of link > 0 ? StartExtraColour(c-1, 0) : StopExtraColour(0) |
| 2223 | SET_CHAR_MAX_HEALTH | 0x475170 | h ped, i health | - | (float) store at +0x544 |
| 2225 | SET_NIGHT_VISION | 0x4751A5 | i | - | CPostEffects::ScriptNightVisionSwitch(i != 0) |
| 2226 | SET_INFRARED_VISION | 0x4751D9 | i | - | CPostEffects::ScriptInfraredVisionSwitch(i != 0) |
| 2228..2233 | IS_{GLOBAL,LOCAL}_VAR_BIT_SET_{CONST,VAR,LVAR} | 0x475244 (shared) | i value, i bit | cmp | `(1 << (bit & 31)) & value` (shl cl); all 6 collect 2 params |
| 2234..2236, 2237..2239 | SET_{GLOBAL,LOCAL}_VAR_BIT_* | 0x475274 / 0x4752A2 | var (by ref: GetPointerToScriptVariable FIRST), i bit | - | `*var \|= 1 << (bit & 31)` |
| 2240..2242, 2243..2245 | CLEAR_{GLOBAL,LOCAL}_VAR_BIT_* | 0x4752CC / 0x4752FC | var, i bit | - | `*var &= ~(1 << (bit & 31))` |
| 2246 | SET_CHAR_CAN_BE_KNOCKED_OFF_BIKE | 0x47532C | h ped, i | - | 2 bit field `CantBeKnockedOffBike` (dword +0x474 bits 27..28) = i & 3 |
| 2247 | SET_CHAR_COORDINATES_DONT_WARP_GANG | 0x47536F | h ped, f x,y,z | - | `CRunningScript::SetCharCoordinates(ped, pos, warpGang = false, offset = true)` |
| 2248 | ADD_PRICE_MODIFIER | 0x4753B0 | i key, i price | - | CShopping::AddPriceModifier(key, price) 0x49BDD0 (ported/fixed, see below) |
| 2249 | REMOVE_PRICE_MODIFIER | 0x4753D6 | i key | - | CShopping::RemovePriceModifier(key) 0x49ACD0 (ported/fixed) |
| 2251 | EXPLODE_CAR_IN_CUTSCENE_SHAKE_AND_BITS | 0x475400 | h car, i a, i b, i c | - | `bCanBeDamaged` (+0x42A bit 0x20) = 1; slot 42 BlowUpCarCutSceneNoExtras(a==0, b==0, false, c!=0) |
| 2256 | IS_SKIP_CUTSCENE_BUTTON_PRESSED | 0x475459 | - | cmp | IsCutsceneSkipButtonBeingPressed() |
| 2257 | GET_CUTSCENE_OFFSET | 0x47547A | - | 3 f | 0xBC4034 (raw dwords) |
| 2260 | CREATE_MENU | 0x475500 | label(8; "DUMMY" => ""), f x, f y, f w, i columns, i interactive, i background, i align | 1 i (movzx id) | x = x*(W*(1/640)), w = w*(W*(1/640)), y = y*(H*(1/448)) (W/H = 0xC17044/0xC17048 ints; extended precision, float stores); CreateNewMenu(0, title, x, y, w, ...) |
| 2262 | SET_MENU_COLUMN_ORIENTATION | 0x4755D0 | i,i,i (bytes) | - | CMenuSystem::SetColumnOrientation |
| 2263 | GET_MENU_ITEM_SELECTED | 0x475602 | i | 1 i (movsx byte) | CheckForSelected |
| 2264 | GET_MENU_ITEM_ACCEPTED | 0x475632 | i | 1 i (movsx byte) | CheckForAccept |
| 2265 | ACTIVATE_MENU_ITEM | 0x475661 | i,i,i (low bytes) | - | ActivateOneItem(menu, row, (uint8)state != 0) |
| 2266 | DELETE_MENU | 0x475693 | i (low byte) | - | SwitchOffMenu |
| 2267 | SET_MENU_COLUMN | 0x4756B4 | i menu, i column, 13 labels | - | header + 12 rows ("DUMMY" => ""); InsertMenu |
| 2268 | SET_BLIP_ENTRY_EXIT | 0x475838 | i blip, f x,y, f radius | - | SetBlipEntryExit(blip, GetInSlot(FindNearestEntryExit((x,y), radius, **0**))) |
| 2269 | SWITCH_DEATH_PENALTIES | 0x475891 | i | - | byte 0x8A5E48 |
| 2270 | SWITCH_ARREST_PENALTIES | 0x4758B1 | i | - | byte 0x8A5E49 |
| 2271 | SET_EXTRA_HOSPITAL_RESTART_POINT | 0x4758D0 | f x,y,z, radius, heading | - | 0xA43414 (x,y,z), 0xA43258 radius, 0xA43254 heading (raw dwords) |
| 2272 | SET_EXTRA_POLICE_STATION_RESTART_POINT | 0x475926 | f x,y,z, radius, heading | - | 0xA43420, 0xA43250 radius, 0xA4324C heading |
| 2273 | FIND_NUMBER_TAGS_TAGGED | 0x47597A | - | 1 i | CTagManager::ms_numTagged (0xA9AD74) |
| 2274 | GET_TERRITORY_UNDER_CONTROL_PERCENTAGE | 0x475996 | - | 1 i | `_ftol2(0x96AB9C * 100.0f)` |
| 2275 / 2276 | IS_OBJECT_IN_ANGLED_AREA_2D / _3D | 0x4759BC (shared) | - | (cmp by callee) | `CRunningScript::ObjectInAngledAreaCheckCommand(cmd)` (0x4883F0) |
| 2277 | GET_RANDOM_CHAR_IN_SPHERE_NO_BRAIN | 0x4759CB | f x,y,z, f radius | 1 h | as 2206 but: !IsPedDead, no ped group, !bHasAScriptBrain (dword +0x474 bit 0x800000); no model flag test |
| 2278 | SET_PLANE_UNDERCARRIAGE_UP | 0x475B96 | h plane, i | - | i != 0 ? SetGearUp() : SetGearDown() (no type check) |
| 2279 | DISABLE_ALL_ENTRY_EXITS | 0x475BD4 | i | - | byte 0x96A7C8 |
| 2280 | ATTACH_ANIMS_TO_MODEL | 0x475BF3 | i model (<0 => UsedObjectArray, no "0 => -1"), label(8) | - | ScriptAttachAnimGroupToCharModel(model, name) (result ignored) then AddToListOfSpecialAnimGroupsAttachedToCharModels(model, name) |
| 2281 | SET_OBJECT_AS_STEALABLE | 0x475C40 | h obj, i | - | objectFlags (+0x140) bit 0x2000 (`bIsLiftable`) |
| 2282 | SET_CREATE_RANDOM_GANG_MEMBERS | 0x475C91 | i | - | byte 0xC0FCB2 = (i == 0) |
| 2283 | ADD_SPARKS | 0x475CB0 | f x,y,z, f dx,dy,dz, i count | - | dir normalised by NormaliseAndMag (its length = force); g_fx.AddSparks(pos, dir, force, count, (0,0,0), 1, 0.4f, 1.0f) |
| 2284 | GET_VEHICLE_CLASS | 0x475D73 | h car | 1 i | `(int8)` byte +0x4D of the vehicle model info of the int16 model index (+0x22) |
| 2286 / 2287 | SET_MENU_ITEM_WITH_NUMBER / _WITH_2_NUMBERS | 0x475DE4 (shared) | i menu, i col, i row, label(8), i n1 [, i n2 (2287; 2286: -1)] | - | InsertOneMenuItemWithNumber; the command id selects 1 or 2 numbers |
| 2288 | APPEND_TO_NEXT_CUTSCENE | 0x475E76 | label(8), label(8) | - | AppendToNextCutscene(a, b) |
| 2289 | GET_NAME_OF_INFO_ZONE | 0x475EB7 | f x,y,z | 8 char text (var by ref) | zone = FindSmallestZoneForPosition(pos, true); strncpy(var, zone->m_InfoLabel (+0), 8) (GET_NAME_OF_ZONE uses +8) |
| 2290 | VEHICLE_CAN_BE_TARGETTED_BY_HS_MISSILE | 0x475F0A | h car, i | - | +0x42D bit 0x40 |
| 2291 | SET_FREEBIES_IN_VEHICLE | 0x475F55 | h car, i | - | +0x428 bit 0x80 (`bFreebies`) |
| 2292 | SET_SCRIPT_LIMIT_TO_GANG_SIZE | 0x475FA0 | i | - | player data +0x43 (byte) = i; group (player data +0x38) loses `CountMembersExcludingLeader - limit` followers when > 0 |
| 2293 / 2294 | MAKE_PLAYER_GANG_DISAPPEAR / _REAPPEAR | 0x47600F / 0x476027 | - | - | FindPlayerPed(-1)->MakePlayerGroupDisappear() / Reappear() |
| 2295 | GET_CLOTHES_ITEM | 0x47603F | i player, i part | 2 i | desc = Players[player].m_PlayerData.m_pPedClothesDesc; texture key = `desc[0x28/4 + part]`, model key = `desc[GetTextureDependency(part)]` (unchecked) |
| 2296 | SHOW_UPDATE_STATS | 0x47608E | i | - | byte 0x8CDE56 |
| 2299 | SET_COORD_BLIP_APPEARANCE | 0x4760FC | i blip, i appearance (byte) | - | CRadar::SetCoordBlipAppearance |

Exe no-ops: none in this group. The 0x6002F0 attractor search (unnamed, ~0x1E0 bytes) is ported as `FindClosestScriptedAttractor` (file local, no hook: never called from anywhere else that was found).

## Callee ports fixed / added
* `CShopping::AddPriceModifier(uint32 key, int32 price)` @0x49BDD0 and `CShopping::RemovePriceModifier(uint32 key)` @0x49ACD0 (Shopping.cpp): the previous (unverified, "may not be same") versions lacked the update of the already loaded price entry
  (`ms_prices` 0xA986F0, stride 0x18, +4 price: the first entry with the key gets the new price) and the remove looped on after the first hit. Now asm-exact and hooked (`RH_ScopedOverloadedInstall`, key overloads).

## Doubts / notes
* The text label buffers: the exe reads 8 bytes into stack buffers that are not terminated/initialised; the ports zero-initialise 16 byte buffers (an 8 char label is therefore terminated, where the exe reads garbage).
* 2215 calls `IsDoorFullyOpenU32(node)` (a NODE index, like the exe); the callee port asserts (debug only) for nodes other than 8..11 / 18 (the exe returns false).
* 2217 / 2219 use `GetProperIndexFromIndexUsedByScript`, which loops `m_nCountOfScripts` entries; the exe loops all 82 (matters only for script id -1 matching an unused slot).

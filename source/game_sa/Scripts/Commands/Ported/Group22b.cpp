#include <StdInc.h>

#include "../Commands.hpp"
#include <CommandParser/Parser.hpp>
#include "Group22.hpp"

#include <cmath>
#include <cstddef>
#include <cstring>

#include "World.h"
#include "Population.h"
#include "PedGroups.h"
#include "GangWars.h"
#include "GameLogic.h"
#include "Restart.h"
#include "TagManager.h"
#include "Stats.h"
#include "Radar.h"
#include "MenuSystem.h"
#include "EntryExit.h"
#include "EntryExitManager.h"
#include "CutsceneMgr.h"
#include "TheZones.h"
#include "Zone.h"
#include "Clothes.h"
#include "PedClothesDesc.h"
#include "PlayerInfo.h"
#include "PlayerPedData.h"
#include "Fx/Fx.h"
#include "Entity/Vehicle/Automobile.h"
#include "Entity/Vehicle/Plane.h"
#include "Entity/Object/Object.h"
#include "Entity/Ped/PlayerPed.h"
#include "Models/VehicleModelInfo.h"

using namespace notsa::script;
using namespace notsa::script::commands::ported::g22;

/*!
* Script commands ported from the exe's group processor g22 (`0x474900`, switch base 2200, jump table @0x476140), ids 2251..2299
* for the vanilla commands that had no handler of their own (S6-G, part 2). Written from the asm of each `case` (see `.notes/S6G_TABLE.md`).
* Idioms: see Group22a.cpp. Text labels are read with ReadTextLabelFromScript(buf, 8) like the exe; the exe's stack buffers are not
* initialised (an 8 character label has no terminator there), here they are zero initialised.
*/

namespace {
//! The menu text idiom: a text label (8) is read into a temporary; unless it is "DUMMY" (_stricmp [0x8229B6] against 0x859EE0) it is copied
//! (up to and including the NUL) into `dst` -- `dst` keeps its previous content (the callers zero its first byte) for "DUMMY".
void ReadMenuText(CRunningScript& S, char* dst) {
    char tmp[16]{};
    S.ReadTextLabelFromScript(tmp, 8); // 0x463D50
    if (_stricmp(tmp, "DUMMY") != 0) {
        strcpy(dst, tmp);
    }
}

// ============================================================================ g22 (ProcessCommands2200To2299 @0x474900), part 2

//! 2251 EXPLODE_CAR_IN_CUTSCENE_SHAKE_AND_BITS (case @0x475400): car, a, b, c
//! +0x42A bit 0x20 (`bCanBeDamaged`) is set, then virtual slot 42 `BlowUpCarCutSceneNoExtras(bNoCamShake = a == 0, bNoSpawnFlyingComps = b == 0,
//! bDetachWheels = false, bExplosionSound = c != 0)`
void ExplodeCarInCutsceneShakeAndBits(CVehicle& veh, int32 a, int32 b, int32 c) {
    static_assert(offsetof(CVehicle, vehicleFlags) + 2 == 0x42A);
    veh.vehicleFlags.bCanBeDamaged = true;
    veh.BlowUpCarCutSceneNoExtras(a == 0, b == 0, false, c != 0);
}

//! 2256 IS_SKIP_CUTSCENE_BUTTON_PRESSED (case @0x475459): => compare flag: IsCutsceneSkipButtonBeingPressed() [0x4D5D10]
bool IsSkipCutsceneButtonPressed() {
    return CCutsceneMgr::IsCutsceneSkipButtonBeingPressed();
}

//! 2257 GET_CUTSCENE_OFFSET (case @0x47547A): => x, y, z (raw dwords of 0xBC4034)
CVector GetCutsceneOffset() {
    return CCutsceneMgr::ms_cutsceneOffset;
}

//! 2260 CREATE_MENU (case @0x475500): title label (8; "DUMMY" => ""), x, y, width, columns, interactive, background, alignment => menu id (zero-extended byte)
//! x = x * (maximumWidth * 1/640) [0x859520], width = width * (maximumWidth * 1/640), y = y * (maximumHeight * 1/448) [0x859524] (all x87, stored as float);
//! CMenuSystem::CreateNewMenu(0, title, x, y, width, columns, interactive, background, alignment) [0x582300]
OpcodeResult ScriptCreateMenu(CRunningScript& S) {
    static_assert(offsetof(RsGlobalType, maximumWidth) == 4 && offsetof(RsGlobalType, maximumHeight) == 8); // 0xC17044 / 0xC17048
    char title[16]{};
    ReadMenuText(S, title);
    S.CollectParameters(7);
    const double widthScale  = (double)RsGlobal.maximumWidth * (double)std::bit_cast<float>(0x3ACCCCCDu);  // 0x859520
    const double heightScale = (double)RsGlobal.maximumHeight * (double)std::bit_cast<float>(0x3B124925u); // 0x859524
    const float  x           = (float)((double)ScriptParams[0].fParam * widthScale);
    const float  w           = (float)(widthScale * (double)ScriptParams[2].fParam);
    const float  y           = (float)(heightScale * (double)ScriptParams[1].fParam);
    const auto   id          = CMenuSystem::CreateNewMenu(
        (CMenuSystem::eMenuType)0,
        title,
        x,
        y,
        w,
        (uint8)ScriptParams[3].iParam,
        (uint8)ScriptParams[4].iParam != 0, // the callee tests / stores the low byte only
        (uint8)ScriptParams[5].iParam != 0,
        (eFontAlignment)ScriptParams[6].iParam
    );
    ScriptParams[0].iParam = (int32)(uint8)id; // movzx
    S.StoreParameters(1);
    return OR_CONTINUE;
}

//! 2262 SET_MENU_COLUMN_ORIENTATION (case @0x4755D0): menu, column, orientation (bytes) => CMenuSystem::SetColumnOrientation [0x582080]
void SetMenuColumnOrientation(int32 menu, int32 column, int32 orientation) {
    CMenuSystem::SetColumnOrientation((MenuId)(uint8)menu, (uint8)column, (uint8)orientation);
}

//! 2263 GET_MENU_ITEM_SELECTED (case @0x475602): menu => CheckForSelected [0x5807E0] (sign-extended byte)
int32 GetMenuItemSelected(int32 menu) {
    return (int32)CMenuSystem::CheckForSelected((MenuId)menu);
}

//! 2264 GET_MENU_ITEM_ACCEPTED (case @0x475632): menu => CheckForAccept [0x5807C0] (sign-extended byte)
int32 GetMenuItemAccepted(int32 menu) {
    return (int32)CMenuSystem::CheckForAccept((MenuId)menu);
}

//! 2265 ACTIVATE_MENU_ITEM (case @0x475661): menu, row, state (low bytes) => CMenuSystem::ActivateOneItem [0x581B30]
void ActivateMenuItem(int32 menu, int32 row, int32 state) {
    CMenuSystem::ActivateOneItem((MenuId)(uint8)menu, (uint8)row, (uint8)state != 0);
}

//! 2266 DELETE_MENU (case @0x475693): menu (low byte) => CMenuSystem::SwitchOffMenu [0x580750]
void ScriptDeleteMenu(int32 menu) {
    CMenuSystem::SwitchOffMenu((MenuId)(uint8)menu);
}

//! 2267 SET_MENU_COLUMN (case @0x4756B4): menu, column, then 13 text labels (header + 12 rows; "DUMMY" => "") => CMenuSystem::InsertMenu [0x581E00]
//! The exe reads: the header label, the first row label, then the 11 remaining row labels (one by one, in that order).
OpcodeResult SetMenuColumn(CRunningScript& S) {
    S.CollectParameters(2);
    const auto menu   = (MenuId)ScriptParams[0].iParam;
    const auto column = (uint8)ScriptParams[1].iParam;

    char header[16]{};
    char row0[16]{};
    char rows[11][16]{};
    ReadMenuText(S, header);
    ReadMenuText(S, row0);
    for (auto& r : rows) {
        ReadMenuText(S, r);
    }
    CMenuSystem::InsertMenu(
        menu,
        column,
        header,
        row0,
        rows[0], rows[1], rows[2], rows[3], rows[4], rows[5], rows[6], rows[7], rows[8], rows[9], rows[10]
    );
    return OR_CONTINUE;
}

//! 2268 SET_BLIP_ENTRY_EXIT (case @0x475838): blip, x, y, radius
//! CRadar::SetBlipEntryExit(blip, GetInSlot(FindNearestEntryExit((x, y), radius, 0))) [0x583F00]
void SetBlipEntryExit(uint32 blip, float x, float y, float radius) {
    const auto idx = CEntryExitManager::FindNearestEntryExit(CVector2D{ x, y }, radius, 0); // 0x43F4B0
    CRadar::SetBlipEntryExit(blip, CEntryExitManager::GetInSlot(idx));                      // 0x43EF00, 0x583F00
}

//! 2269 SWITCH_DEATH_PENALTIES (case @0x475891): flag => byte 0x8A5E48
void SwitchDeathPenalties(int32 flag) {
    CGameLogic::bPenaltyForDeathApplies = flag != 0;
}

//! 2270 SWITCH_ARREST_PENALTIES (case @0x4758B1): flag => byte 0x8A5E49
void SwitchArrestPenalties(int32 flag) {
    CGameLogic::bPenaltyForArrestApplies = flag != 0;
}

//! 2271 SET_EXTRA_HOSPITAL_RESTART_POINT (case @0x4758D0): x, y, z, radius, heading => raw dword copies into 0xA43414.. / 0xA43258 / 0xA43254
void SetExtraHospitalRestartPoint(float x, float y, float z, float radius, float heading) {
    CRestart::ExtraHospitalRestartCoors   = CVector{ x, y, z }; // 0xA43414
    CRestart::ExtraHospitalRestartRadius  = radius;             // 0xA43258
    CRestart::ExtraHospitalRestartHeading = heading;            // 0xA43254
}

//! 2272 SET_EXTRA_POLICE_STATION_RESTART_POINT (case @0x475926): x, y, z, radius, heading => raw dword copies into 0xA43420.. / 0xA43250 / 0xA4324C
void SetExtraPoliceStationRestartPoint(float x, float y, float z, float radius, float heading) {
    CRestart::ExtraPoliceStationRestartCoors   = CVector{ x, y, z }; // 0xA43420
    CRestart::ExtraPoliceStationRestartRadius  = radius;             // 0xA43250
    CRestart::ExtraPoliceStationRestartHeading = heading;            // 0xA4324C
}

//! 2273 FIND_NUMBER_TAGS_TAGGED (case @0x47597A): => CTagManager::ms_numTagged (0xA9AD74)
int32 FindNumberTagsTagged() {
    return CTagManager::ms_numTagged;
}

//! 2274 GET_TERRITORY_UNDER_CONTROL_PERCENTAGE (case @0x475996): => _ftol2(0x96AB9C * 100.0f [0x858628])
int32 GetTerritoryUnderControlPercentage() {
    return Ftol((double)CGangWars::TerritoryUnderControlPercentage * (double)100.0f);
}

//! 2275 IS_OBJECT_IN_ANGLED_AREA_2D / 2276 IS_OBJECT_IN_ANGLED_AREA_3D (shared case @0x4759BC): CRunningScript::ObjectInAngledAreaCheckCommand(command) [0x4883F0]
OpcodeResult IsObjectInAngledArea(CRunningScript& S, eScriptCommands command) {
    S.ObjectInAngledAreaCheckCommand((int32)command);
    return OR_CONTINUE;
}

//! 2277 GET_RANDOM_CHAR_IN_SPHERE_NO_BRAIN (case @0x4759CB): x, y, z, radius => handle (-1 = none)
//! Same loop as GET_RANDOM_CHAR_IN_SPHERE_ONLY_DRUGS_BUYERS (FindRandomCharInSphere, Group22.hpp), with these per-ped conditions (after createdBy == game,
//! !bRemoveFromWorld, !bFadeOut): !IsPedDead, no ped group, no script brain (`bHasAScriptBrain`: dword +0x474 bit 0x800000)
int32 GetRandomCharInSphereNoBrain(CRunningScript& S, float x, float y, float z, float radius) {
    return FindRandomCharInSphere(S, { x, y, z }, radius, [](CRunningScript& s, CPed* ped) {
        if (s.IsPedDead(ped)) { // 0x464D70
            return false;
        }
        if (CPedGroups::GetPedsGroup(ped)) { // 0x5F7E80
            return false;
        }
        return !ped->bHasAScriptBrain; // test dword [+0x474], 0x800000
    });
}

//! 2278 SET_PLANE_UNDERCARRIAGE_UP (case @0x475B96): plane, flag => CPlane::SetGearUp() [0x6CAC20] if flag != 0 else SetGearDown() [0x6CAC70] (no type check)
void SetPlaneUndercarriageUp(CVehicle& veh, int32 up) {
    if (up != 0) {
        static_cast<CPlane&>(veh).SetGearUp();
    } else {
        static_cast<CPlane&>(veh).SetGearDown();
    }
}

//! 2279 DISABLE_ALL_ENTRY_EXITS (case @0x475BD4): flag => byte 0x96A7C8 (`CEntryExitManager::ms_bDisabled`) = flag != 0
void DisableAllEntryExits(int32 flag) {
    CEntryExitManager::ms_bDisabled = (int8)(flag != 0);
}

//! 2280 ATTACH_ANIMS_TO_MODEL (case @0x475BF3): model (negative => UsedObjectArray[-model].nModelIndex; NO "0 => -1" rule), anim group label (8)
//! => ScriptAttachAnimGroupToCharModel(model, name) [0x474800] (the result is ignored), then AddToListOfSpecialAnimGroupsAttachedToCharModels(model, name) [0x474750]
OpcodeResult AttachAnimsToModel(CRunningScript& S) {
    S.CollectParameters(1);
    int32 model = ScriptParams[0].iParam;
    char  name[16]{};
    S.ReadTextLabelFromScript(name, 8); // 0x463D50
    if (model < 0) {
        model = CTheScripts::UsedObjectArray[-model].nModelIndex; // 0xA44B88 + 0x1C * -model
    }
    CTheScripts::ScriptAttachAnimGroupToCharModel(model, name);
    CTheScripts::AddToListOfSpecialAnimGroupsAttachedToCharModels(model, name);
    return OR_CONTINUE;
}

//! 2281 SET_OBJECT_AS_STEALABLE (case @0x475C40): object, flag => objectFlags (+0x140) bit 0x2000 (`bIsLiftable`), no null check
void SetObjectAsStealable(CObject& obj, int32 flag) {
    static_assert(offsetof(CObject, objectFlags) == 0x140);
    obj.objectFlags.bIsLiftable = flag != 0;
}

//! 2282 SET_CREATE_RANDOM_GANG_MEMBERS (case @0x475C91): flag => byte 0xC0FCB2 (`m_bDontCreateRandomGangMembers`) = flag == 0
void SetCreateRandomGangMembers(int32 flag) {
    CPopulation::m_bDontCreateRandomGangMembers = flag == 0;
}

//! 2283 ADD_SPARKS (case @0x475CB0): x, y, z, dirX, dirY, dirZ, count
//! The direction copy is normalised by CVector::NormaliseAndMag [0x59C970] (its length becomes the "force"), then
//! g_fx.AddSparks(pos, dir, force, count, across = (0, 0, 0), sparksType = 1, spread = 0.4f (0x3ECCCCCD), life = 1.0f) [0x49F040]
void AddSparks(CVector pos, CVector dir, int32 count) {
    const float force = dir.NormaliseAndMag();
    g_fx.AddSparks(pos, dir, force, count, CVector{ 0.0f, 0.0f, 0.0f }, SPARK_PARTICLE_SPARK, 0.4f, 1.0f);
}

//! 2284 GET_VEHICLE_CLASS (case @0x475D73): car => the model info's vehicle class (signed byte @+0x4D); model info = ms_modelInfoPtrs[(int16)+0x22], no checks
int32 GetVehicleClass(CVehicle& veh) {
    static_assert(offsetof(CVehicleModelInfo, m_nVehicleClass) == 0x4D && sizeof(eVehicleClass) == 1);
    const auto* const mi = static_cast<CVehicleModelInfo*>(CModelInfo::ms_modelInfoPtrs[(int32)(int16)veh.m_nModelIndex]);
    return (int32)(int8)mi->m_nVehicleClass;
}

//! 2286 SET_MENU_ITEM_WITH_NUMBER / 2287 SET_MENU_ITEM_WITH_2_NUMBERS (shared case @0x475DE4): menu, column, row, text label (8), then
//! 2286: number (second number = -1); 2287: number, number2  => CMenuSystem::InsertOneMenuItemWithNumber [0x581D70]
OpcodeResult SetMenuItemWithNumber(CRunningScript& S, eScriptCommands command) {
    S.CollectParameters(3);
    const auto menu   = ScriptParams[0].iParam;
    const auto column = ScriptParams[1].iParam;
    const auto row    = ScriptParams[2].iParam;
    char       text[16]{};
    S.ReadTextLabelFromScript(text, 8); // 0x463D50
    int32 num1, num2;
    if (command == COMMAND_SET_MENU_ITEM_WITH_NUMBER) {
        S.CollectParameters(1);
        num1 = ScriptParams[0].iParam;
        num2 = -1;
    } else if (command == COMMAND_SET_MENU_ITEM_WITH_2_NUMBERS) {
        S.CollectParameters(2);
        num1 = ScriptParams[0].iParam;
        num2 = ScriptParams[1].iParam;
    } else {
        return OR_CONTINUE; // (unreachable: the exe returns without doing anything)
    }
    CMenuSystem::InsertOneMenuItemWithNumber((MenuId)menu, (uint8)column, (uint8)row, text, num1, num2);
    return OR_CONTINUE;
}

//! 2288 APPEND_TO_NEXT_CUTSCENE (case @0x475E76): label (8), label (8) => AppendToNextCutscene(a, b) [0x4D5DB0]
OpcodeResult AppendToNextCutsceneCmd(CRunningScript& S) {
    char a[16]{};
    char b[16]{};
    S.ReadTextLabelFromScript(a, 8); // 0x463D50
    S.ReadTextLabelFromScript(b, 8);
    CCutsceneMgr::AppendToNextCutscene(a, b);
    return OR_CONTINUE;
}

//! 2289 GET_NAME_OF_INFO_ZONE (case @0x475EB7): x, y, z => 8 char label of the smallest zone: `strncpy(dst, zone, 8)`, i.e. the INFO label at +0 (GET_NAME_OF_ZONE copies +8)
//! `FindSmallestZoneForPosition(&pos, true)` [0x572360]; then the output variable is read (GetPointerToScriptVariable(2)). No null check on the zone.
OpcodeResult GetNameOfInfoZone(CRunningScript& S, CVector pos) {
    static_assert(offsetof(CZone, m_InfoLabel) == 0);
    const auto* const zone = CTheZones::FindSmallestZoneForPosition(pos, true);
    auto* const       dst  = reinterpret_cast<char*>(S.GetPointerToScriptVariable(VAR_GLOBAL));
    // strncpy(dst, src, 8) [0x821F40]: copies up to the first NUL, pads the rest with zeros
    const size_t len = strnlen(zone->m_InfoLabel, 8);
    memcpy(dst, zone->m_InfoLabel, len);
    memset(dst + len, 0, 8 - len);
    return OR_CONTINUE;
}

//! 2290 VEHICLE_CAN_BE_TARGETTED_BY_HS_MISSILE (case @0x475F0A): car, flag => +0x42D bit 0x40 (`bVehicleCanBeTargettedByHS`)
void VehicleCanBeTargettedByHSMissile(CVehicle& veh, int32 flag) {
    veh.vehicleFlags.bVehicleCanBeTargettedByHS = flag != 0;
}

//! 2291 SET_FREEBIES_IN_VEHICLE (case @0x475F55): car, flag => +0x428 bit 0x80 (`bFreebies`)
void SetFreebiesInVehicle(CVehicle& veh, int32 flag) {
    veh.vehicleFlags.bFreebies = flag != 0;
}

//! 2292 SET_SCRIPT_LIMIT_TO_GANG_SIZE (case @0x475FA0): limit (low byte)
//! player data (+0x480 of the player ped).m_nScriptLimitToGangSize (+0x43) = limit; then the player's group (index @+0x38) loses
//! `CountMembersExcludingLeader() - limit` followers when that is positive (RemoveNFollowers [0x5FB1D0]).
void SetScriptLimitToGangSize(int32 limit) {
    static_assert(offsetof(CPlayerPedData, m_nPlayerGroup) == 0x38 && offsetof(CPlayerPedData, m_nScriptLimitToGangSize) == 0x43);
    FindPlayerPed(-1)->GetPlayerData()->m_nScriptLimitToGangSize = (uint8)limit;
    const auto groupIdx = FindPlayerPed(-1)->GetPlayerData()->m_nPlayerGroup;
    auto&      members  = CPedGroups::ms_groups[groupIdx].GetMembership(); // 0xC09920 + 8 + 0x2D4 * idx
    const auto n        = members.CountMembersExcludingLeader() - (int32)FindPlayerPed(-1)->GetPlayerData()->m_nScriptLimitToGangSize; // 0x5F6AA0
    if (n > 0) {
        members.RemoveNFollowers(n); // 0x5FB1D0
    }
}

//! 2293 MAKE_PLAYER_GANG_DISAPPEAR (case @0x47600F): => FindPlayerPed(-1)->MakePlayerGroupDisappear() [0x60A440]
void MakePlayerGangDisappear() {
    FindPlayerPed(-1)->MakePlayerGroupDisappear();
}

//! 2294 MAKE_PLAYER_GANG_REAPPEAR (case @0x476027): => FindPlayerPed(-1)->MakePlayerGroupReappear() [0x60A4B0]
void MakePlayerGangReappear() {
    FindPlayerPed(-1)->MakePlayerGroupReappear();
}

//! 2295 GET_CLOTHES_ITEM (case @0x47603F): player, texture part => texture key, model key
//! desc = CWorld::Players[player] (stride 0x190).m_PlayerData.m_pPedClothesDesc (info +8); texture key = desc->m_anTextureKeys[part] (desc +0x28 + 4*part),
//! model key = ((uint32*)desc)[CClothes::GetTextureDependency(part) [0x5A7EA0]] (the model keys start at +0; "unavailable" indexes past them; no checks)
MultiRet<uint32, uint32> GetClothesItem(int32 player, int32 part) {
    static_assert(sizeof(CPlayerInfo) == 0x190);
    static_assert(offsetof(CPlayerInfo, m_PlayerData) + offsetof(CPlayerPedData, m_pPedClothesDesc) == 8);
    static_assert(offsetof(CPedClothesDesc, m_anModelKeys) == 0 && offsetof(CPedClothesDesc, m_anTextureKeys) == 0x28);
    const auto* const desc = CWorld::Players[player].m_PlayerData.m_pPedClothesDesc;
    const uint32      tex  = reinterpret_cast<const uint32*>(desc)[0x28 / 4 + part];
    const auto        dep  = (int32)CClothes::GetTextureDependency((eClothesTexturePart)part);
    const uint32      mdl  = reinterpret_cast<const uint32*>(desc)[dep];
    return { tex, mdl };
}

//! 2296 SHOW_UPDATE_STATS (case @0x47608E): flag => byte 0x8CDE56 (`CStats::bShowUpdateStats`)
void ShowUpdateStats(int32 flag) {
    CStats::bShowUpdateStats = flag != 0;
}

//! 2299 SET_COORD_BLIP_APPEARANCE (case @0x4760FC): blip, appearance (low byte) => CRadar::SetCoordBlipAppearance [0x583E50]
void SetCoordBlipAppearance(uint32 blip, int32 appearance) {
    CRadar::SetCoordBlipAppearance(blip, (eBlipAppearance)(uint8)appearance);
}
}; // namespace

void notsa::script::commands::ported::g22::RegisterG22b() {
    REGISTER_COMMAND_HANDLER_BEGIN("Ported g22b");

    REGISTER_COMMAND_HANDLER(COMMAND_EXPLODE_CAR_IN_CUTSCENE_SHAKE_AND_BITS, ExplodeCarInCutsceneShakeAndBits);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_SKIP_CUTSCENE_BUTTON_PRESSED, IsSkipCutsceneButtonPressed);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_CUTSCENE_OFFSET, GetCutsceneOffset);
    REGISTER_COMMAND_HANDLER(COMMAND_CREATE_MENU, ScriptCreateMenu);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_MENU_COLUMN_ORIENTATION, SetMenuColumnOrientation);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_MENU_ITEM_SELECTED, GetMenuItemSelected);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_MENU_ITEM_ACCEPTED, GetMenuItemAccepted);
    REGISTER_COMMAND_HANDLER(COMMAND_ACTIVATE_MENU_ITEM, ActivateMenuItem);
    REGISTER_COMMAND_HANDLER(COMMAND_DELETE_MENU, ScriptDeleteMenu);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_MENU_COLUMN, SetMenuColumn);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_BLIP_ENTRY_EXIT, SetBlipEntryExit);
    REGISTER_COMMAND_HANDLER(COMMAND_SWITCH_DEATH_PENALTIES, SwitchDeathPenalties);
    REGISTER_COMMAND_HANDLER(COMMAND_SWITCH_ARREST_PENALTIES, SwitchArrestPenalties);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_EXTRA_HOSPITAL_RESTART_POINT, SetExtraHospitalRestartPoint);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_EXTRA_POLICE_STATION_RESTART_POINT, SetExtraPoliceStationRestartPoint);
    REGISTER_COMMAND_HANDLER(COMMAND_FIND_NUMBER_TAGS_TAGGED, FindNumberTagsTagged);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_TERRITORY_UNDER_CONTROL_PERCENTAGE, GetTerritoryUnderControlPercentage);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_OBJECT_IN_ANGLED_AREA_2D, IsObjectInAngledArea);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_OBJECT_IN_ANGLED_AREA_3D, IsObjectInAngledArea);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_RANDOM_CHAR_IN_SPHERE_NO_BRAIN, GetRandomCharInSphereNoBrain);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_PLANE_UNDERCARRIAGE_UP, SetPlaneUndercarriageUp);
    REGISTER_COMMAND_HANDLER(COMMAND_DISABLE_ALL_ENTRY_EXITS, DisableAllEntryExits);
    REGISTER_COMMAND_HANDLER(COMMAND_ATTACH_ANIMS_TO_MODEL, AttachAnimsToModel);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_OBJECT_AS_STEALABLE, SetObjectAsStealable);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_CREATE_RANDOM_GANG_MEMBERS, SetCreateRandomGangMembers);
    REGISTER_COMMAND_HANDLER(COMMAND_ADD_SPARKS, AddSparks);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_VEHICLE_CLASS, GetVehicleClass);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_MENU_ITEM_WITH_NUMBER, SetMenuItemWithNumber);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_MENU_ITEM_WITH_2_NUMBERS, SetMenuItemWithNumber);
    REGISTER_COMMAND_HANDLER(COMMAND_APPEND_TO_NEXT_CUTSCENE, AppendToNextCutsceneCmd);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_NAME_OF_INFO_ZONE, GetNameOfInfoZone);
    REGISTER_COMMAND_HANDLER(COMMAND_VEHICLE_CAN_BE_TARGETTED_BY_HS_MISSILE, VehicleCanBeTargettedByHSMissile);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_FREEBIES_IN_VEHICLE, SetFreebiesInVehicle);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_SCRIPT_LIMIT_TO_GANG_SIZE, SetScriptLimitToGangSize);
    REGISTER_COMMAND_HANDLER(COMMAND_MAKE_PLAYER_GANG_DISAPPEAR, MakePlayerGangDisappear);
    REGISTER_COMMAND_HANDLER(COMMAND_MAKE_PLAYER_GANG_REAPPEAR, MakePlayerGangReappear);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_CLOTHES_ITEM, GetClothesItem);
    REGISTER_COMMAND_HANDLER(COMMAND_SHOW_UPDATE_STATS, ShowUpdateStats);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_COORD_BLIP_APPEARANCE, SetCoordBlipAppearance);
}

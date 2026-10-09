#include <StdInc.h>

#include "../Commands.hpp"
#include <CommandParser/Parser.hpp>
#include "Group25_26.hpp"

#include <bit>
#include <cstddef>
#include <cstring>

#include "World.h"
#include "Pad.h"
#include "Pickups.h"
#include "Pickup.h"
#include "LoadMonitor.h"
#include "Wanted.h"
#include "PedGroups.h"
#include "PedGroup.h"
#include "PedGroupMembership.h"
#include "GameLogic.h"
#include "TheCarGenerators.h"
#include "SpecialPlateHandler.h"
#include "PointLights.h"
#include "EntryExitManager.h"
#include "MenuSystem.h"
#include "Font.h"
#include "Messages.h"
#include "Text/Text.h"
#include "Hud.h"
#include "PlayerPedData.h"
#include "TaskManager.h"
#include "TaskComplexUseGoggles.h"
#include "Models/ModelInfo.h"
#include "Models/VehicleModelInfo.h"
#include "cHandlingDataMgr.h"
#include "Entity/Object/Object.h"
#include "Entity/Vehicle/Automobile.h"
#include "Entity/Vehicle/Bike.h"
#include "Entity/Vehicle/Train.h"
#include "Entity/Ped/PlayerPed.h"

using namespace notsa::script;
using namespace notsa::script::commands::ported::g25_26;

/*!
* Script commands of the exe's group processor for ids 2500..2599 (`CRunningScript::ProcessCommands2500To2599` @0x47A760, switch base 2500,
* the jump table @0x47BF4C indexed directly by `id - 2500`), ids 2500..2546 (S6-J, part a): the vanilla commands of that range that had no
* handler of their own. Written from the asm of each `case` (the case address is in the comment above each handler, see `.notes/S6J_TABLE.md`).
* Convention: `CollectParameters(n)` of the exe => the handler arguments; handles that the exe resolves with a pool `GetAt` are `Foo&`
* arguments (the exe doesn't null check them either); a `bool`/`void` return = UpdateCompareFlag / nothing.
*/

namespace {
// Raw offsets the asm relies on
static_assert(offsetof(CVehicle, vehicleFlags) == 0x428);
static_assert(offsetof(CVehicle, m_nVehicleType) == 0x590);
static_assert(offsetof(CVehicle, m_nTertiaryColor) == 0x436 && offsetof(CVehicle, m_nQuaternaryColor) == 0x437);
static_assert(offsetof(CVehicle, m_fHealth) == 0x4C0);
static_assert(offsetof(CPhysical, m_nPhysicalFlags) == 0x40);
static_assert(offsetof(CTrain, m_nTrainFlags) == 0x5B8);
static_assert(offsetof(CBike, m_nNoOfContactWheels) == 0x804);
static_assert(offsetof(CAutomobile, m_nNumContactWheels) == 0x960);
static_assert(offsetof(CAutomobile, m_wMiscComponentAngle) == 0x86C);
static_assert(offsetof(CPed, m_nAnimGroup) == 0x4D4);
static_assert(offsetof(CPed, m_pGogglesObject) == 0x4FC);
static_assert(offsetof(CPlayerPedData, m_nPlayerGroup) == 0x38);
static_assert(offsetof(CPlayerPedData, m_nScriptLimitToGangSize) == 0x43);
static_assert(offsetof(CPlayerPedData, m_bDontAllowWeaponChange) == 0x85);
static_assert(offsetof(CPlayerPedData, m_bForceInteriorLighting) == 0x86);
static_assert(offsetof(CPad, DisablePlayerControls) == 0x10E);
static_assert(offsetof(CPickup, m_nPickupType) == 0x1C && sizeof(CPickup) == 0x20);
static_assert(MAX_NUM_PICKUPS == 620); // 0x4D80 / 0x20
static_assert(offsetof(CVehicleModelInfo, m_nHandlingId) == 0x4A);
static_assert(0xC2B9C8 + offsetof(cHandlingDataMgr, m_aVehicleHandling) + offsetof(tHandlingData, m_nMonetaryValue) == 0xC2BAB4);
static_assert(sizeof(tHandlingData) == 0xE0);
static_assert(sizeof(CPlayerInfo) == 0x190);

//! 0x859A3C: 1/255
constexpr float ONE_OVER_255 = std::bit_cast<float>(0x3B808081u);
//! 0x859520: the screen width scale (see Group22b)
constexpr float SCREEN_WIDTH_SCALE = std::bit_cast<float>(0x3ACCCCCDu);

//! 2500 SET_PETROL_TANK_WEAKPOINT (case @0x47A7A2): car, flag  -- bPetrolTankIsWeakPoint (+0x42E bit 6) = bit 0 of flag (`shl cl, 6; xor; and 0x40`)
void SetPetrolTankWeakpoint(CVehicle& veh, int32 flag) {
    veh.vehicleFlags.bPetrolTankIsWeakPoint = (flag & 1) != 0;
}

//! 2503 SET_PLAYER_MODEL (case @0x47A820): player, model  (the ped isn't null checked)
//! ped = CWorld::Players[player].m_pPed; the anim group (+0x4D4) survives: save it, DeleteRwObject() (vtable +0x20), m_nModelIndex (+0x22) = -1,
//! SetModelIndex(model) (vtable +0x14), restore the anim group.
void SetPlayerModel(int32 playerId, int32 model) {
    CPed* const ped       = CWorld::Players[playerId].m_pPed;
    const auto  animGroup = ped->m_nAnimGroup;
    ped->DeleteRwObject();
    ped->m_nModelIndex = (uint16)0xFFFF;
    ped->SetModelIndex((uint32)model);
    ped->m_nAnimGroup = animGroup;
}

//! 2504 ARE_SUBTITLES_SWITCHED_ON (case @0x47A869): no params => compare flag = FrontEndMenuManager.m_bShowSubtitles (+0x44, zero extended byte)
bool AreSubtitlesSwitchedOn() {
    return FrontEndMenuManager.m_bShowSubtitles;
}

//! 2506 SET_OBJECT_PROOFS (case @0x47A921): object, bullet, fire, explosion, collision, melee  -- each flag = bit 0 of its param:
//! bBulletProof (+0x40 bit 18), bFireProof (19), bExplosionProof (23), bCollisionProof (20), bMeleeProof (21)
void SetObjectProofs(CObject& obj, int32 bullet, int32 fire, int32 explosion, int32 collision, int32 melee) {
    obj.physicalFlags.bBulletProof    = (bullet & 1) != 0;
    obj.physicalFlags.bFireProof      = (fire & 1) != 0;
    obj.physicalFlags.bExplosionProof = (explosion & 1) != 0;
    obj.physicalFlags.bCollisionProof = (collision & 1) != 0;
    obj.physicalFlags.bMeleeProof     = (melee & 1) != 0;
}

//! 2507 IS_CAR_TOUCHING_CAR (case @0x47A9B6): car A, car B => compare flag = A->GetHasCollidedWith(B) [0x543540]
bool IsCarTouchingCar(CVehicle& a, CVehicle& b) {
    return a.GetHasCollidedWith(&b);
}

//! 2511 SET_TRAIN_FORCED_TO_SLOW_DOWN (case @0x47AA49): train (vehicle pool handle, not type checked), flag  -- bForceSlowDown (+0x5B9 bit 1) = (flag != 0)
void SetTrainForcedToSlowDown(CVehicle& veh, int32 flag) {
    static_cast<CTrain&>(veh).trainFlags.bForceSlowDown = flag != 0;
}

//! 2512 IS_VEHICLE_ON_ALL_WHEELS (case @0x47AA8D): vehicle => compare flag
//! (+0x590) == 9 [bike] && CBike::m_nNoOfContactWheels (+0x804) == 4, or (+0x590) == 0 [automobile] && CAutomobile::m_nNumContactWheels (+0x960) == 4
bool IsVehicleOnAllWheels(CVehicle& veh) {
    bool result = false;
    if (veh.m_nVehicleType == VEHICLE_TYPE_BIKE && static_cast<CBike&>(veh).m_nNoOfContactWheels == 4) {
        result = true;
    }
    if (veh.m_nVehicleType == VEHICLE_TYPE_AUTOMOBILE && static_cast<CAutomobile&>(veh).m_nNumContactWheels == 4) {
        result = true;
    }
    return result;
}

//! 2513 DOES_PICKUP_EXIST (case @0x47AAEB): pickup handle => compare flag = CPickups::GetActualPickupIndex(handle) [0x4552A0] != -1
bool DoesPickupExist(int32 handle) {
    return CPickups::GetActualPickupIndex(handle) != -1;
}

//! 2514 ENABLE_AMBIENT_CRIME (case @0x47AB20): on  -- CLoadMonitor::m_bEnableAmbientCrime (g_LoadMonitor [0xB72994]) = (on != 0)
void EnableAmbientCrime(int32 on) {
    if (on != 0) {
        g_LoadMonitor.EnableAmbientCrime();
    } else {
        g_LoadMonitor.DisableAmbientCrime();
    }
}

//! 2516 CLEAR_WANTED_LEVEL_IN_GARAGE (case @0x47AB3F): FindPlayerWanted(-1)->ClearWantedLevelAndGoOnParole() [0x5625A0] (no null check)
void ClearWantedLevelInGarage() {
    FindPlayerWanted(-1)->ClearWantedLevelAndGoOnParole();
}

//! 2519 FORCE_INTERIOR_LIGHTING_FOR_PLAYER (case @0x47AC06): player, flag  -- Players[player].m_pPed->m_pPlayerData->m_bForceInteriorLighting (+0x86) = (flag != 0)
void ForceInteriorLightingForPlayer(int32 playerId, int32 flag) {
    CWorld::Players[playerId].m_pPed->GetPlayerData()->m_bForceInteriorLighting = flag != 0;
}

//! 2521 USE_DETONATOR (case @0x47AC4E): CWorld::UseDetonator(FindPlayerPed(-1)) [0x5660B0]
void UseDetonator() {
    CWorld::UseDetonator(FindPlayerPed(-1));
}

//! 2522 IS_MONEY_PICKUP_AT_COORDS (case @0x47AC65): x, y, z => compare flag
//! The first pickup (of the 620 slots, in order) with type 8 [PICKUP_MONEY] whose position is closer than 0.5 [0x858B8C] to (x, y, z):
//! `pos - coords` (0x40FE60, each component rounded to float), Magnitude (0x4082C0, kept unrounded) compared with `test ah, 5; jp skip` => dist < 0.5
bool IsMoneyPickupAtCoords(CVector coords) {
    for (auto& pickup : CPickups::aPickUps) {
        if (pickup.m_nPickupType != PICKUP_MONEY) {
            continue;
        }
        const CVector diff = pickup.GetPosn() - coords;
        if (Magnitude87(diff) < (double)0.5f) {
            return true;
        }
    }
    return false;
}

//! 2523 SET_MENU_COLUMN_WIDTH (case @0x47AD10): menu (byte), column (byte), width (word)
//! width = _ftol2( (maximumWidth [0xC17044] * 0x859520) * (int)(uint16)width ) -- x87: every step rounded by the PC=24 mode
//! CMenuSystem::SetColumnWidth(menu, column, (uint16)result) [0x582050]
void SetMenuColumnWidth(int32 menu, int32 column, int32 width) {
    const double scaled = ((double)RsGlobal.maximumWidth * (double)SCREEN_WIDTH_SCALE) * (double)(int32)(uint16)width;
    CMenuSystem::SetColumnWidth((MenuId)(uint8)menu, (uint8)column, (uint16)Ftol(scaled));
}

//! 2525 MAKE_ROOM_IN_PLAYER_GANG_FOR_MISSION_PEDS (case @0x47AD5A): n
//! group = CPedGroups::ms_groups[player data +0x38 (m_nPlayerGroup)]; extra = group.CountMembersExcludingLeader() [0x5F6AA0] - (byte)playerData+0x43
//! (m_nScriptLimitToGangSize) - n; if extra > 0 (signed): membership.RemoveNFollowers(extra) [0x5FB1D0]
void MakeRoomInPlayerGangForMissionPeds(int32 n) {
    CPlayerPedData* const data1 = FindPlayerPed(-1)->GetPlayerData();
    CPedGroupMembership&  ms    = CPedGroups::ms_groups[data1->m_nPlayerGroup].GetMembership();
    CPlayerPedData* const data2 = FindPlayerPed(-1)->GetPlayerData(); // (the exe asks for the player ped twice, before counting)
    const int32           extra = ms.CountMembersExcludingLeader() - (int32)data2->m_nScriptLimitToGangSize - n;
    if (extra > 0) {
        ms.RemoveNFollowers(extra);
    }
}

//! 2528 SET_UP_SKIP_FOR_SPECIFIC_VEHICLE (case @0x47AE2C): x, y, z, heading (float), vehicle
//! CGameLogic::SetUpSkip(pos, heading, false, vehicle, false) [0x4423C0]
void SetUpSkipForSpecificVehicle(CVector pos, float heading, CVehicle& veh) {
    CGameLogic::SetUpSkip(pos, heading, false, &veh, false);
}

//! 2529 GET_CAR_MODEL_VALUE (case @0x47AE95): model => value  (the model info isn't null checked)
//! gHandlingDataMgr.m_aVehicleHandling[(byte)modelInfo->+0x4A].m_nMonetaryValue (entry stride 0xE0, field +0xD8 [0xC2BAB4])
int32 GetCarModelValue(int32 model) {
    const auto* const mi = static_cast<CVehicleModelInfo*>(CModelInfo::ms_modelInfoPtrs[model]);
    return (int32)gHandlingDataMgr.m_aVehicleHandling[(uint8)mi->m_nHandlingId].m_nMonetaryValue;
}

//! 2530 CREATE_CAR_GENERATOR_WITH_PLATE (case @0x47AECF): CollectParameters(12): x, y, z, heading, model, colour1 (word), colour2 (word), forceSpawn (byte),
//! alarm (byte), doorLock (byte), minDelay (word), maxDelay (word); then the plate text label (ReadTextLabelFromScript(buf, 9)) => generator id
//! z > -100.0 [0x859014] (ordered; NaN / <= -100 keep z) => z += 0.015 [0x859F00], stored as float (fadd; fstp).
//! CTheCarGenerators::CreateCarGenerator(pos, heading, model, c1, c2, force, alarm, lock, min, max, iplId = 0, ignorePopLimit = 1) [0x6F31A0].
//! The plate: chars 0..7 that are '_' or NUL become ' '; buf[8] = 0; CSpecialPlateHandler::Add(id, buf) [0x6F2D90] on 0xC279D8; StoreParameters(1) = the id.
OpcodeResult CreateCarGeneratorWithPlate(CRunningScript& S) {
    S.CollectParameters(12);
    const float x = ScriptParams[0].fParam;
    const float y = ScriptParams[1].fParam;
    float       z = ScriptParams[2].fParam;
    if (z > -100.0f) {
        z = (float)((double)z + (double)std::bit_cast<float>(0x3C75C28Fu));
    }
    const int32 id = CTheCarGenerators::CreateCarGenerator(
        CVector{ x, y, z },
        ScriptParams[3].fParam,
        ScriptParams[4].iParam,
        (int16)(uint16)ScriptParams[5].iParam,
        (int16)(uint16)ScriptParams[6].iParam,
        (uint8)ScriptParams[7].iParam,
        (uint8)ScriptParams[8].iParam,
        (uint8)ScriptParams[9].iParam,
        (uint16)ScriptParams[10].iParam,
        (uint16)ScriptParams[11].iParam,
        0,
        true
    );
    char plate[16]{}; // NOTSA: zero-initialised (the exe's 9 byte buffer is only terminated by the explicit `buf[8] = 0` below)
    S.ReadTextLabelFromScript(plate, 9);
    for (int32 i = 0; i < 8; i++) {
        if (plate[i] == '_' || plate[i] == '\0') {
            plate[i] = ' ';
        }
    }
    plate[8] = '\0';
    CTheCarGenerators::m_SpecialPlateHandler.Add(id, plate);
    ScriptParams[0].iParam = id;
    S.StoreParameters(1);
    return OR_CONTINUE;
}

//! 2531 FIND_TRAIN_DIRECTION (case @0x47AFD2): train => compare flag = bClockwiseDirection (trainFlags bit 6, +0x5B8)
bool FindTrainDirection(CVehicle& veh) {
    return static_cast<CTrain&>(veh).trainFlags.bClockwiseDirection;
}

//! 2532 SET_AIRCRAFT_CARRIER_SAM_SITE (case @0x47B00C): flag  -- CObject::bAircraftCarrierSamSiteDisabled (0x8D0A24) = (flag == 0)
void SetAircraftCarrierSamSite(int32 flag) {
    CObject::bAircraftCarrierSamSiteDisabled = flag == 0;
}

//! 2533 DRAW_LIGHT_WITH_RANGE (case @0x47B02B): x, y, z, red, green, blue (ints), range (float)
//! CPointLights::AddLight(0, pos, (0, 0, 0), range, r/255, g/255, b/255, fogType 0, extra shadows 1, no entity) [0x7000E0];
//! each colour = (float)((double)int * (float)0x859A3C [1/255]) (fild; fmul; fstp)
void DrawLightWithRange(CVector pos, int32 red, int32 green, int32 blue, float range) {
    const float b = (float)((double)blue * (double)ONE_OVER_255);
    const float g = (float)((double)green * (double)ONE_OVER_255);
    const float r = (float)((double)red * (double)ONE_OVER_255);
    CPointLights::AddLight(0, pos, CVector{ 0.0f, 0.0f, 0.0f }, range, r, g, b, 0, true, nullptr);
}

//! 2534 ENABLE_BURGLARY_HOUSES (case @0x47B0E7): on => CEntryExitManager::EnableBurglaryHouses(on != 0) [0x43F180]
void EnableBurglaryHouses(int32 on) {
    CEntryExitManager::EnableBurglaryHouses(on != 0);
}

//! 2535 IS_PLAYER_CONTROL_ON (case @0x47B109): pad => compare flag = !(CPad::GetPad(pad) [0x53FB70] +0x10E bit 5 [bPlayerSafe])
bool IsPlayerControlOn(int32 padId) {
    return !CPad::GetPad(padId)->bPlayerSafe;
}

//! 2537 GIVE_NON_PLAYER_CAR_NITRO (case @0x47B179): car  -- CAutomobile::NitrousControl(1) [0x6A3EA0] (the vehicle isn't type checked)
void GiveNonPlayerCarNitro(CVehicle& veh) {
    static_cast<CAutomobile&>(veh).NitrousControl(1);
}

//! 2539 PLAYER_TAKE_OFF_GOGGLES (case @0x47B1A4): player, instantly  (the ped isn't null checked)
//! instantly != 0: needs the goggles on (+0x4FC != 0) and a free PRIMARY slot (CTaskManager primary task 3 [intelligence +0x10] == null): then
//! the primary task 3 = new CTaskComplexUseGoggles [0x634EF0] (SetTask(task, 3) [0x681AF0]) and playerData (+0x480, may be null) +0x85 = 1
//! instantly == 0: CPed::TakeOffGoggles() [0x5E6010]
void PlayerTakeOffGoggles(int32 playerId, int32 instantly) {
    CPed* const ped = CWorld::Players[playerId].m_pPed;
    if (instantly == 0) {
        ped->TakeOffGoggles();
        return;
    }
    if (ped->m_pGogglesObject == nullptr) {
        return;
    }
    auto& tm = ped->GetIntelligence()->GetTaskManager();
    if (tm.m_aPrimaryTasks[TASK_PRIMARY_PRIMARY] != nullptr) {
        return;
    }
    tm.SetTask(new CTaskComplexUseGoggles{}, TASK_PRIMARY_PRIMARY, false);
    if (CPlayerPedData* const data = ped->GetPlayerData()) {
        data->m_bDontAllowWeaponChange = true; // `mov byte [esi + 0x85], 1`
    }
}

//! 2542 FORCE_BIG_MESSAGE_AND_COUNTER (case @0x47B2BC): flag  -- CHud::bScriptForceDisplayWithCounters (0xBAA3FA) = low byte of flag (raw byte store)
void ForceBigMessageAndCounter(int32 flag) {
    *reinterpret_cast<uint8*>(&CHud::bScriptForceDisplayWithCounters) = (uint8)flag;
}

//! 2546 DOES_DECISION_MAKER_EXIST (case @0x47B37A): handle => compare flag
//! idx = GetActualScriptThingIndex(handle, 7 [DECISION_MAKER]); idx in [0, 20) (signed) and CDecisionMakerTypes::m_IsActive[idx] [0xC0B01C]
//! (CDecisionMakerTypes::GetInstance() [0x4684F0] is called first, its result is unused)
bool DoesDecisionMakerExist(int32 handle) {
    const auto idx = CTheScripts::GetActualScriptThingIndex(handle, SCRIPT_THING_DECISION_MAKER);
    if (idx < 0 || idx >= 20) {
        return false;
    }
    (void)CDecisionMakerTypes::GetInstance();
    return CDecisionMakerTypes::m_IsActive[idx];
}
}; // namespace

void notsa::script::commands::ported::g25_26::RegisterG25a() {
    REGISTER_COMMAND_HANDLER_BEGIN("Ported g25a");

    REGISTER_COMMAND_HANDLER(COMMAND_SET_PETROL_TANK_WEAKPOINT, SetPetrolTankWeakpoint);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_PLAYER_MODEL, SetPlayerModel);
    REGISTER_COMMAND_HANDLER(COMMAND_ARE_SUBTITLES_SWITCHED_ON, AreSubtitlesSwitchedOn);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_OBJECT_PROOFS, SetObjectProofs);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_CAR_TOUCHING_CAR, IsCarTouchingCar);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_TRAIN_FORCED_TO_SLOW_DOWN, SetTrainForcedToSlowDown);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_VEHICLE_ON_ALL_WHEELS, IsVehicleOnAllWheels);
    REGISTER_COMMAND_HANDLER(COMMAND_DOES_PICKUP_EXIST, DoesPickupExist);
    REGISTER_COMMAND_HANDLER(COMMAND_ENABLE_AMBIENT_CRIME, EnableAmbientCrime);
    REGISTER_COMMAND_HANDLER(COMMAND_CLEAR_WANTED_LEVEL_IN_GARAGE, ClearWantedLevelInGarage);
    REGISTER_COMMAND_HANDLER(COMMAND_FORCE_INTERIOR_LIGHTING_FOR_PLAYER, ForceInteriorLightingForPlayer);
    REGISTER_COMMAND_HANDLER(COMMAND_USE_DETONATOR, UseDetonator);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_MONEY_PICKUP_AT_COORDS, IsMoneyPickupAtCoords);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_MENU_COLUMN_WIDTH, SetMenuColumnWidth);
    REGISTER_COMMAND_HANDLER(COMMAND_MAKE_ROOM_IN_PLAYER_GANG_FOR_MISSION_PEDS, MakeRoomInPlayerGangForMissionPeds);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_UP_SKIP_FOR_SPECIFIC_VEHICLE, SetUpSkipForSpecificVehicle);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_CAR_MODEL_VALUE, GetCarModelValue);
    REGISTER_COMMAND_HANDLER(COMMAND_CREATE_CAR_GENERATOR_WITH_PLATE, CreateCarGeneratorWithPlate);
    REGISTER_COMMAND_HANDLER(COMMAND_FIND_TRAIN_DIRECTION, FindTrainDirection);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_AIRCRAFT_CARRIER_SAM_SITE, SetAircraftCarrierSamSite);
    REGISTER_COMMAND_HANDLER(COMMAND_DRAW_LIGHT_WITH_RANGE, DrawLightWithRange);
    REGISTER_COMMAND_HANDLER(COMMAND_ENABLE_BURGLARY_HOUSES, EnableBurglaryHouses);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_PLAYER_CONTROL_ON, IsPlayerControlOn);
    REGISTER_COMMAND_HANDLER(COMMAND_GIVE_NON_PLAYER_CAR_NITRO, GiveNonPlayerCarNitro);
    REGISTER_COMMAND_HANDLER(COMMAND_PLAYER_TAKE_OFF_GOGGLES, PlayerTakeOffGoggles);
    REGISTER_COMMAND_HANDLER(COMMAND_FORCE_BIG_MESSAGE_AND_COUNTER, ForceBigMessageAndCounter);
    REGISTER_COMMAND_HANDLER(COMMAND_DOES_DECISION_MAKER_EXIST, DoesDecisionMakerExist);
}

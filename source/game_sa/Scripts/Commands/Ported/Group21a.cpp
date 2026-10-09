#include <StdInc.h>

#include "../Commands.hpp"
#include <CommandParser/Parser.hpp>
#include "Group20_21.hpp"

#include <cstddef>
#include <cstring>

#include "World.h"
#include "CarCtrl.h"
#include "TheZones.h"
#include "Zone.h"
#include "FireManager.h"
#include "EntryExit.h"
#include "EntryExitManager.h"
#include "VehicleRecording.h"
#include "Clock.h"
#include "Interior/InteriorManager_c.h"
#include "Events/EventScriptCommand.h"
#include "Events/EventGlobalGroup.h"
#include "Entity/Object/Object.h"
#include "Entity/Vehicle/Automobile.h"
#include "TaskSequences.h"
#include "TaskSimpleDie.h"
#include "TaskComplexDie.h"
#include "TaskComplexFollowPedFootsteps.h"
#include "TaskComplexWalkAlongsidePed.h"
#include "TaskSimpleSetKindaStayInSamePlace.h"

using namespace notsa::script;
using namespace notsa::script::commands::ported::g09_12;
using namespace notsa::script::commands::ported::g20_21;

/*!
* Script commands ported from the exe's group processors g20 (`0x472310`, ids 2087..2099) and g21 (`0x470A90`, ids 2101..2148)
* for the vanilla commands that had no handler of their own (S6-F, slice C, 25 commands).
* Written from the asm of each `case`. Idioms: `CPed*` / `CVehicle*` / `CVehicle&` parser arguments are `GetAtRef(handle)`;
* `GivePedScriptedTask(handle, task, command)` is given the executing command id.
*/

namespace {
//! 0xB7449C object pool
//! 2087 CONNECT_LODS (case @0x474148): object A, object B  -- both objects are looked up first (0x465040), then
//! ScriptConnectLodsFunction(a, b) [0x470A20] and AddToListOfConnectedLodObjects(objA, objB) [0x470980] with the pointers looked up BEFORE the connect
void ConnectLods(int32 handleA, int32 handleB) {
    auto* const objA = GetObjectPool()->GetAtRef(handleA);
    auto* const objB = GetObjectPool()->GetAtRef(handleB);
    CTheScripts::ScriptConnectLodsFunction(handleA, handleB); // 0x470A20
    CTheScripts::AddToListOfConnectedLodObjects(objA, objB);  // 0x470980
}

//! 2088 SET_MAX_FIRE_GENERATIONS (case @0x47418F): n  -- raw dword store to 0xB728E0 (= `gFireManager.m_nMaxFireGenerationsAllowed`, right behind the 60 fires)
void SetMaxFireGenerations(int32 n) {
    static_assert(offsetof(CFireManager, m_nMaxFireGenerationsAllowed) == 0x960); // 0xB71F80 + 0x960 == 0xB728E0
    gFireManager.m_nMaxFireGenerationsAllowed = (uint32)n;
}

//! 2089 TASK_DIE_NAMED_ANIM (case @0x4741AB): ped, anim name (24 buf), ifp name (16 buf), blendDelta (float), flag
//! CollectParameters(1); ReadTextLabelFromScript(animName, 0x18); ReadTextLabelFromScript(animBlock, 0x10); CollectParameters(2);
//! CTaskSimpleDie(animName, animBlock, flag == 0 ? 0x10 : 0xD0, blendDelta, 1.0f) [0x62FA60]
OpcodeResult TaskDieNamedAnim(CRunningScript& S, eScriptCommands command) {
    S.CollectParameters(1);
    const auto pedHandle = ScriptParams[0].iParam;

    char animName[24]{};
    char animBlock[16]{};
    S.ReadTextLabelFromScript(animName, sizeof(animName));   // 0x463D50
    S.ReadTextLabelFromScript(animBlock, sizeof(animBlock)); // 0x463D50

    S.CollectParameters(2);
    const auto blendDelta = ScriptParams[0].fParam;
    const auto flags      = ScriptParams[1].iParam == 0 ? 0x10 : 0xD0;

    S.GivePedScriptedTask(
        pedHandle,
        new CTaskSimpleDie{ animName, animBlock, (eAnimationFlags)flags, blendDelta, 1.0f },
        command
    );
    return OR_CONTINUE;
}

//! 2096 SET_POOL_TABLE_COORDS (case @0x4742AC): minX, minY, minZ, maxX, maxY, maxZ  -- raw dword copies into 0x8CDF00 (min) / 0x8CDEF4 (max)
void SetPoolTableCoords(float minX, float minY, float minZ, float maxX, float maxY, float maxZ) {
    CWorld::SnookerTableMax = CVector{ maxX, maxY, maxZ }; // 0x8CDEF4
    CWorld::SnookerTableMin = CVector{ minX, minY, minZ }; // 0x8CDF00
}

//! 2099 HAS_OBJECT_BEEN_PHOTOGRAPHED (case @0x474322): object => compare flag  -- true (and the flag is CLEARED) if the object exists and has `bIsPhotographed` (+0x140 bit 0x1000)
bool HasObjectBeenPhotographed(int32 objectHandle) {
    static_assert(offsetof(CObject, objectFlags) == 0x140);
    auto* const obj = GetObjectPool()->GetAtRef(objectHandle); // 0x465040
    if (obj && obj->objectFlags.bIsPhotographed) {
        obj->objectFlags.bIsPhotographed = false;
        return true;
    }
    return false;
}

//! 2101 GET_CURRENT_DATE (case @0x470B03): => day, month  (no CollectParameters; zero-extended bytes 0xB70154 / 0xB70155)
OpcodeResult GetCurrentDate(CRunningScript& S) {
    StoreArg(&S, MultiRet<int32, int32>{ (int32)CClock::ms_nGameClockDays, (int32)CClock::ms_nGameClockMonth });
    return OR_CONTINUE;
}

//! 2111 GET_CAR_UPRIGHT_VALUE (case @0x470E15): car => z of the matrix' "up" vector (+0x14 -> +0x20 .. +0x28) (no null check)
float GetCarUprightValue(CVehicle& veh) {
    return veh.m_matrix->GetUp().z;
}

//! 2112 SET_VEHICLE_AREA_VISIBLE (case @0x470E5B): car, area  -- byte store to +0x2F (area code)
void SetVehicleAreaVisible(CVehicle& veh, int32 area) {
    veh.SetAreaCode((eAreaCodes)(uint8)area);
}

//! 2113 SELECT_WEAPONS_FOR_VEHICLE (case @0x470E86): car, weapon  -- byte store to +0x513 (`m_nVehicleWeaponInUse`)
void SelectWeaponsForVehicle(CVehicle& veh, int32 weapon) {
    static_assert(offsetof(CVehicle, m_nVehicleWeaponInUse) == 0x513 && sizeof(eCarWeapon) == 1);
    veh.m_nVehicleWeaponInUse = (eCarWeapon)(uint8)weapon;
}

//! 2114 GET_CITY_PLAYER_IS_IN (case @0x470EB4): player (collected, unused) => 0xBA6718 (`CTheZones::m_CurrLevel`)
int32 GetCityPlayerIsIn(int32 player) {
    return (int32)CTheZones::m_CurrLevel;
}

//! 2115 GET_NAME_OF_ZONE (case @0x470ED9): x, y, z => 8 char text label of the smallest zone [strncpy(dst, zone + 8, 8) into the output variable]
//! `FindSmallestZoneForPosition(&pos, true)` [0x572360]; then the output variable is read (GetPointerToScriptVariable(2)). No null check on the zone.
OpcodeResult GetNameOfZone(CRunningScript& S, CVector pos) {
    static_assert(offsetof(CZone, m_TextLabel) == 8);
    const auto* const zone = CTheZones::FindSmallestZoneForPosition(pos, true); // 0x572360
    auto* const       dst  = reinterpret_cast<char*>(S.GetPointerToScriptVariable(VAR_GLOBAL));
    // strncpy(dst, zone->m_TextLabel, 8) [0x821F40]: copies up to the first NUL, pads the rest with zeros
    const size_t len = strnlen(zone->m_TextLabel, 8);
    memcpy(dst, zone->m_TextLabel, len);
    memset(dst + len, 0, 8 - len);
    return OR_CONTINUE;
}

//! 2125 ACTIVATE_INTERIOR_PEDS (case @0x470F77): on  -- InteriorManager_c::ActivatePeds(on != 0) [0x598080] on 0xBAF670
void ActivateInteriorPeds(int32 on) {
    g_interiorMan.ActivatePeds(on != 0);
}

//! 2126 SET_VEHICLE_CAN_BE_TARGETTED (case @0x470FAA): car, flag  -- +0x42D bit 0x04 (`bVehicleCanBeTargetted`)
void SetVehicleCanBeTargetted(CVehicle& veh, int32 flag) {
    veh.vehicleFlags.bVehicleCanBeTargetted = flag != 0;
}

//! 2128 TASK_FOLLOW_FOOTSTEPS (case @0x470FF5): ped, target ped  -- CTaskComplexFollowPedFootsteps(target) [0x694E20]
void TaskFollowFootsteps(eScriptCommands command, CRunningScript& S, int32 pedHandle, CPed* target) {
    S.GivePedScriptedTask(pedHandle, new CTaskComplexFollowPedFootsteps{ target }, command);
}

//! 2129 DAMAGE_CHAR (case @0x47105C): ped, amount, armourFirst
//! The x87 stack keeps the (exact) amount: with `armourFirst` it first eats the armour (armour >= amount => armour -= amount, rest 0;
//! otherwise (or NaN armour) rest = amount - armour, armour = 0). health = health - rest (the extended `rest` is subtracted, the result is
//! stored as float). If the (unrounded) result is <= 0: health = 0 and a CEventScriptCommand(3, CTaskComplexDie(...)) is added to the ped's event group.
void DamageChar(CPed& ped, int32 amount, int32 armourFirst) {
    double rest = (double)amount; // fild
    if (armourFirst != 0) {
        const double armour = ped.m_fArmour;
        if (rest <= armour) {
            ped.m_fArmour = (float)(armour - rest);
            rest          = 0.0;
        } else { // (also: NaN armour)
            rest          = rest - armour;
            ped.m_fArmour = 0.0f;
        }
    }
    const double health = (double)ped.m_fHealth - rest;
    ped.m_fHealth       = (float)health;
    if (!(health <= 0.0)) { // fcomp + `test ah, 0x41; jp` => returns if health > 0 or unordered
        return;
    }
    ped.m_fHealth = 0.0f;

    auto* const task = new CTaskComplexDie{ WEAPON_UNARMED, (AssocGroupId)0, (AnimationId)0xF, 4.0f, 0.0f, false, false, eDirection::FORWARD, false }; // 0x630040
    CEventScriptCommand event{ 3, task, false }; // 0x4B0A00
    ped.GetIntelligence()->m_eventGroup.Add(&event, false); // 0x4AB420 (the event is cloned; ~CEventScriptCommand [0x4B0A50] deletes the task)
}

//! 2130 SET_CAR_CAN_BE_VISIBLY_DAMAGED (case @0x471161): car, flag  -- byte +0x868 bit 0x10 (CAutomobile `autoFlags.bCanBeVisiblyDamaged`), no vehicle type check
void SetCarCanBeVisiblyDamaged(CVehicle& veh, int32 flag) {
    static_assert(offsetof(CAutomobile, autoFlags) == 0x868);
    static_cast<CAutomobile&>(veh).autoFlags.bCanBeVisiblyDamaged = flag != 0;
}

//! 2131 SET_HELI_REACHED_TARGET_DISTANCE (case @0x4711AB): heli, distance  -- byte store to +0x3DF
void SetHeliReachedTargetDistance(CVehicle& veh, int32 dist) {
    static_assert(offsetof(CVehicle, m_autoPilot) + offsetof(CAutoPilot, m_ucHeliTargetDist2) == 0x3DF);
    veh.m_autoPilot.m_ucHeliTargetDist2 = (uint8)dist;
}

//! 2133 GET_SOUND_LEVEL_AT_COORDS (case @0x4711D9): ped (-1 = none), x, y, z => float  -- GetEventGlobalGroup()->GetSoundLevel(ped, &pos) [0x4AB900]
float GetSoundLevelAtCoords(CPed* ped, CVector pos) {
    return GetEventGlobalGroup()->GetSoundLevel(ped, pos);
}

//! 2136 SET_HEADING_FOR_ATTACHED_PLAYER (case @0x4712AB): player (unused), heading, step  -- both multiplied by deg->rad (0x8595EC)
//! 0xA4449C = heading * rad, 0xA44498 = step * rad
void SetHeadingForAttachedPlayer(int32 player, float heading, float step) {
    constexpr float DEG_TO_RAD = 0.017453292f; // 0x8595EC
    CTheScripts::fCameraHeadingWhenPlayerIsAttached     = heading * DEG_TO_RAD;
    CTheScripts::fCameraHeadingStepWhenPlayerIsAttached = step * DEG_TO_RAD;
}

//! 2137 TASK_WALK_ALONGSIDE_CHAR (case @0x4712DF): ped, target ped  -- CTaskComplexWalkAlongsidePed(target, 10.0f) [0x683240]
void TaskWalkAlongsideChar(eScriptCommands command, CRunningScript& S, int32 pedHandle, CPed* target) {
    S.GivePedScriptedTask(pedHandle, new CTaskComplexWalkAlongsidePed{ target, 10.0f }, command);
}

//! 2138 CREATE_EMERGENCY_SERVICES_CAR (case @0x47134B): model (NOT translated), x, y, z => compare flag
//! CCarCtrl::ScriptGenerateOneEmergencyServicesCar(model, pos) [0x42FBC0]
bool CreateEmergencyServicesCar(uint32 model, CVector pos) {
    return CCarCtrl::ScriptGenerateOneEmergencyServicesCar(model, pos);
}

//! 2139 TASK_KINDA_STAY_IN_SAME_PLACE (case @0x4713B5): ped, flag
//! ped != -1: a stack CTaskSimpleSetKindaStayInSamePlace(flag != 0) [0x62F610] runs ProcessPed(ped) [0x62F660] right away;
//! ped == -1: heap task appended to the active sequence [CTaskComplexSequence::AddTask 0x632D10]
void TaskKindaStayInSamePlace(int32 pedHandle, int32 flag) {
    if (pedHandle != -1) {
        auto* const ped = GetPedPool()->GetAtRef(pedHandle);
        CTaskSimpleSetKindaStayInSamePlace task{ flag != 0 };
        task.ProcessPed(ped);
        return;
    }
    CTaskSequences::AddTaskToActiveSequence(new CTaskSimpleSetKindaStayInSamePlace{ flag != 0 });
}

//! 2142 START_PLAYBACK_RECORDED_CAR_LOOPED (case @0x471470): car, path  -- CVehicleRecording::StartPlaybackRecordedCar(car, path, false (useCarAI), true (looped)) [0x45A980]
void StartPlaybackRecordedCarLooped(CVehicle* veh, int32 path) {
    CVehicleRecording::StartPlaybackRecordedCar(veh, path, false, true);
}

//! 2145 IS_ATTACHED_PLAYER_HEADING_ACHIEVED (case @0x471504): player (unused) => compare flag: the heading step (0xA44498) == 0.0f (ordered: NaN => false)
bool IsAttachedPlayerHeadingAchieved(int32 player) {
    return CTheScripts::fCameraHeadingStepWhenPlayerIsAttached == 0.0f;
}

//! 2148 ENABLE_ENTRY_EXIT_PLAYER_GROUP_WARPING (case @0x47152E): x, y, radius, enable
//! idx = CEntryExitManager::FindNearestEntryExit((x, y), radius, -1) [0x43F4B0]; the entry/exit (no null check) and its link get the word flag 0x100 (`bAcceptNpcGroup`)
//! set / cleared. (A null entry exit crashes the exe.)
void EnableEntryExitPlayerGroupWarping(float x, float y, float radius, int32 enable) {
    static_assert(offsetof(CEntryExit, m_nFlags) == 0x30 && offsetof(CEntryExit, m_pLink) == 0x38);
    const auto idx = CEntryExitManager::FindNearestEntryExit(CVector2D{ x, y }, radius, -1);
    auto* const ee = CEntryExitManager::GetInSlot(idx); // 0x43EF00
    if (enable != 0) {
        ee->m_nFlags |= 0x100;
        if (ee->m_pLink) {
            ee->m_pLink->m_nFlags |= 0x100;
        }
    } else {
        ee->m_nFlags &= 0xFEFF;
        if (ee->m_pLink) {
            ee->m_pLink->m_nFlags &= 0xFEFF;
        }
    }
}
}; // namespace

void notsa::script::commands::ported::g20_21::RegisterG21a() {
    REGISTER_COMMAND_HANDLER_BEGIN("Ported g21a");

    REGISTER_COMMAND_HANDLER(COMMAND_CONNECT_LODS, ConnectLods);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_MAX_FIRE_GENERATIONS, SetMaxFireGenerations);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_DIE_NAMED_ANIM, TaskDieNamedAnim);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_POOL_TABLE_COORDS, SetPoolTableCoords);
    REGISTER_COMMAND_HANDLER(COMMAND_HAS_OBJECT_BEEN_PHOTOGRAPHED, HasObjectBeenPhotographed);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_CURRENT_DATE, GetCurrentDate);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_CAR_UPRIGHT_VALUE, GetCarUprightValue);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_VEHICLE_AREA_VISIBLE, SetVehicleAreaVisible);
    REGISTER_COMMAND_HANDLER(COMMAND_SELECT_WEAPONS_FOR_VEHICLE, SelectWeaponsForVehicle);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_CITY_PLAYER_IS_IN, GetCityPlayerIsIn);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_NAME_OF_ZONE, GetNameOfZone);
    REGISTER_COMMAND_HANDLER(COMMAND_ACTIVATE_INTERIOR_PEDS, ActivateInteriorPeds);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_VEHICLE_CAN_BE_TARGETTED, SetVehicleCanBeTargetted);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_FOLLOW_FOOTSTEPS, TaskFollowFootsteps);
    REGISTER_COMMAND_HANDLER(COMMAND_DAMAGE_CHAR, DamageChar);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_CAR_CAN_BE_VISIBLY_DAMAGED, SetCarCanBeVisiblyDamaged);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_HELI_REACHED_TARGET_DISTANCE, SetHeliReachedTargetDistance);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_SOUND_LEVEL_AT_COORDS, GetSoundLevelAtCoords);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_HEADING_FOR_ATTACHED_PLAYER, SetHeadingForAttachedPlayer);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_WALK_ALONGSIDE_CHAR, TaskWalkAlongsideChar);
    REGISTER_COMMAND_HANDLER(COMMAND_CREATE_EMERGENCY_SERVICES_CAR, CreateEmergencyServicesCar);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_KINDA_STAY_IN_SAME_PLACE, TaskKindaStayInSamePlace);
    REGISTER_COMMAND_HANDLER(COMMAND_START_PLAYBACK_RECORDED_CAR_LOOPED, StartPlaybackRecordedCarLooped);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_ATTACHED_PLAYER_HEADING_ACHIEVED, IsAttachedPlayerHeadingAchieved);
    REGISTER_COMMAND_HANDLER(COMMAND_ENABLE_ENTRY_EXIT_PLAYER_GROUP_WARPING, EnableEntryExitPlayerGroupWarping);
}

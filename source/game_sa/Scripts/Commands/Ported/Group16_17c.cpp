#include <StdInc.h>

#include "../Commands.hpp"
#include <CommandParser/Parser.hpp>

#include <bit>
#include <cstddef>
#include <cstdint>

#include "World.h"
#include "CarCtrl.h"
#include "Streaming.h"
#include "PedGroups.h"
#include "PathFind.h"
#include "FireManager.h"
#include "TagManager.h"
#include "VehicleRecording.h"
#include "StuckCarCheck.h"
#include "Rect.h"
#include "Models/VehicleModelInfo.h"
#include "Entity/Vehicle/Vehicle.h"

// Registered by the central registration (Commands.hpp / RunningScript.cpp, orchestrator)
namespace notsa::script::commands::ported::g17c {
void RegisterHandlers();
}

using namespace notsa::script;

/*!
* Script commands ported from the exe's group processor g17 (CRunningScript::ProcessCommands1700To1799 @0x496E00) for the
* vanilla commands that had no handler of their own, part c: vehicle mods / paintjobs, groups, fires, car nodes, stuck car
* checks, playback, tags, cutscene skip (25 commands, ids 1733 and 1765..1799). The rest of g17 is in Group16_17b.cpp.
*
* Every handler below was written from the asm of the `case` in the group processor, NOT from the Ghidra decompilation.
* The case address is given in the comment above each handler.
*/

namespace {
static_assert(offsetof(CVehicleModelInfo, m_anUpgrades) == 0x2D6);
static_assert(offsetof(CRunningScript, m_SceneSkipIP) == 0xD8);
static_assert(offsetof(CVehicle, m_autoPilot) + offsetof(CAutoPilot, m_nCarMission) == 0x3BA);
static_assert(offsetof(CVehicle, m_autoPilot) + offsetof(CAutoPilot, m_nCruiseSpeed) == 0x3D0);
static_assert(offsetof(CVehicle, m_autoPilot) + offsetof(CAutoPilot, m_nTimeToStartMission) == 0x3AC);
static_assert(offsetof(CEntity, m_nModelIndex) == 0x22);

//! x87-compare idiom `fcom -100.0f; test ah, 0x41; jp skip`: the ground Z is looked up only when `z <= -100.0f` (ordered).
float GroundZIfAuto(float x, float y, float z) {
    if (z <= -100.0f) { // 0x859014
        return CWorld::FindGroundZForCoord(x, y);
    }
    return z;
}

//! Inlined `CVehicle::SetEngineOn(true)` (0x41BDD0): the engine can't be turned on if it's broken
void VehSetEngineOn(CVehicle& veh) {
    veh.vehicleFlags.bEngineOn = !veh.vehicleFlags.bEngineBroken;
}

//! The script handle -> model index decoding of the exe (`test edx, edx; jge`; else `UsedObjectArray[-handle].nModelIndex`, 0xA44B88 - handle * 0x1C)
int32 ScriptModelToIndex(int32 value) {
    if (value >= 0) {
        return value;
    }
    return CTheScripts::UsedObjectArray[-value].nModelIndex;
}

//! The `CRect` the exe builds from 2 corners: both axes are put in ascending order (swapped only if the first is `>` the second, ordered compare),
//! laid out as {left = minX, y(+4) = maxY, right = maxX, y(+C) = minY} == CRect(minX, minY, maxX, maxY)
CRect SortedRect(float x1, float y1, float x2, float y2) {
    if (x1 > x2) { // fcomp + `test ah, 0x41; jne skip`: not swapped for <=, nor for unordered
        std::swap(x1, x2);
    }
    if (y1 > y2) {
        std::swap(y1, y2);
    }
    return CRect{ x1, y1, x2, y2 };
}

//! 1733 SKIP_TO_END_AND_STOP_PLAYBACK_RECORDED_CAR (case @0x497C26): vehicle
//! CollectParameters(1); CVehicleRecording::SkipToEndAndStopPlaybackRecordedCar(veh) [0x45A4A0]
void SkipToEndAndStopPlaybackRecordedCar(CVehicle* veh) {
    CVehicleRecording::SkipToEndAndStopPlaybackRecordedCar(veh);
}

//! 1765 GET_AVAILABLE_VEHICLE_MOD (case @0x498340): vehicle, slot => 1 int
//! CollectParameters(2); the vehicle is NOT null checked, the slot is NOT range checked.
//! result = (int16)((CVehicleModelInfo*)modelInfo[veh->modelIndex])->m_anUpgrades[slot] (+0x2D6 + slot * 2), sign extended
int32 GetAvailableVehicleMod(CVehicle& veh, int32 slot) {
    auto* const mi = CModelInfo::GetModelInfo(veh.m_nModelIndex)->AsVehicleModelInfoPtr();
    return mi->m_anUpgrades.data()[slot];
}

//! 1766 GET_VEHICLE_MOD_TYPE (case @0x498388): model => 1 int
//! CollectParameters(1); the model is decoded as in the other mod commands (negative => UsedObjectArray). The result is
//! derived from the (CBaseModelInfo + 0x12) flags: `uses veh dummy` (bit 8) and `CarMod` (bits 10..14); every combination
//! the jump tables of the exe don't map keeps ScriptParams[0] untouched, i.e. the (undecoded) input is stored back.
int32 GetVehicleModType(int32 modelHandle) {
    const auto* const mi = CModelInfo::GetModelInfo(ScriptModelToIndex(modelHandle));
    const int32 carMod   = mi->CarMod;
    if (mi->bUsesVehDummy) { // byte table 0x4990BC / dwords 0x4990A0 (index = carMod - 1)
        switch (carMod) {
        case 1:  return 11;
        case 2:  return 12;
        case 12: return 14;
        case 13: return 15;
        case 19: return 13;
        case 20:
        case 21:
        case 22: return 16;
        }
    } else { // dwords 0x4990D4 (index = carMod, 0..17)
        switch (carMod) {
        case 0:  return 0;
        case 1:
        case 2:  return 1;
        case 6:  return 2;
        case 8:
        case 9:  return 3;
        case 10: return 4;
        case 11: return 5;
        case 12: return 6;
        case 14: return 7;
        case 15: return 8;
        case 16: return 9;
        case 17: return 10;
        }
    }
    return modelHandle; // ScriptParams[0] unchanged
}

//! 1767 ADD_VEHICLE_MOD (case @0x4985A6): vehicle, model => 1 int
//! CollectParameters(2); the vehicle is NOT null checked; CVehicle::AddVehicleUpgrade(model) [0x6E3290] returns the replaced upgrade (EAX), which is stored
int32 AddVehicleMod(CVehicle& veh, Model model) {
    return veh.AddVehicleUpgrade((int32)model.value);
}

//! 1768 REMOVE_VEHICLE_MOD (case @0x4985E4): vehicle, model
//! CollectParameters(2); the vehicle is NOT null checked; CVehicle::RemoveVehicleUpgrade(model) [0x6DF930]
void RemoveVehicleMod(CVehicle& veh, Model model) {
    veh.RemoveVehicleUpgrade((int32)model.value);
}

//! 1769 REQUEST_VEHICLE_MOD (case @0x498624): model
//! CollectParameters(1); CStreaming::RequestVehicleUpgrade(model, 0xC) [0x408C70]
void RequestVehicleMod(Model model) {
    CStreaming::RequestVehicleUpgrade((int32)model.value, 0xC);
}

//! 1770 HAS_VEHICLE_MOD_LOADED (case @0x498654): model => cmp
//! CollectParameters(1); CStreaming::HasVehicleUpgradeLoaded(model) [0x407820]
bool HasVehicleModLoaded(Model model) {
    return CStreaming::HasVehicleUpgradeLoaded((int32)model.value);
}

//! 1771 MARK_VEHICLE_MOD_AS_NO_LONGER_NEEDED (case @0x49868D): model
//! CollectParameters(1); SetMissionDoesntRequireModel(model) [0x409C90]; the linked upgrade (CLinkedUpgradeList::FindOtherUpgrade
//! [0x4C74D0] on 0xB4E6D8, int16 result sign extended) gets the same treatment unless it is -1
void MarkVehicleModAsNoLongerNeeded(Model model) {
    const auto modelId = (int32)model.value;
    CStreaming::SetMissionDoesntRequireModel(modelId);
    const int32 other = CVehicleModelInfo::ms_linkedUpgrades.FindOtherUpgrade((int16)modelId);
    if (other != -1) {
        CStreaming::SetMissionDoesntRequireModel(other);
    }
}

//! 1772 GET_NUM_AVAILABLE_PAINTJOBS (case @0x4986DC): vehicle => 1 int
//! CollectParameters(1); the vehicle is NOT null checked; CVehicleModelInfo::GetNumRemaps() [0x4C86B0] of its model info
int32 GetNumAvailablePaintjobs(CVehicle& veh) {
    return CModelInfo::GetModelInfo(veh.m_nModelIndex)->AsVehicleModelInfoPtr()->GetNumRemaps();
}

//! 1773 GIVE_VEHICLE_PAINTJOB (case @0x49870C): vehicle, paintjob
//! CollectParameters(2); the vehicle is NOT null checked; CVehicle::SetRemap(paintjob) [0x6D0C00]
void GiveVehiclePaintjob(CVehicle& veh, int32 paintjob) {
    veh.SetRemap(paintjob);
}

//! 1774 IS_GROUP_MEMBER (case @0x49873B): ped, group => cmp
//! CollectParameters(2); group = GetActualScriptThingIndex(P1, 8) in [0, 8) and ms_activeGroups[group] != 0 (0xC098E0),
//! then CPedGroupMembership::IsMember(ped) [0x5F6A10] of ms_groups[group] (0xC09920 + 8); the ped may be null
bool IsGroupMember(CPed* ped, int32 groupHandle) {
    const auto group = CTheScripts::GetActualScriptThingIndex(groupHandle, SCRIPT_THING_PED_GROUP);
    if (group < 0 || group >= 8) {
        return false;
    }
    if (!CPedGroups::ms_activeGroups[group]) {
        return false;
    }
    return CPedGroups::ms_groups[group].GetMembership().IsMember(ped);
}

//! 1775 IS_GROUP_LEADER (case @0x498796): ped, group => cmp
//! CollectParameters(2); like 1774 but WITHOUT the ms_activeGroups check: CPedGroupMembership::IsLeader(ped) [0x5F69C0]
bool IsGroupLeader(CPed* ped, int32 groupHandle) {
    const auto group = CTheScripts::GetActualScriptThingIndex(groupHandle, SCRIPT_THING_PED_GROUP);
    if (group < 0 || group >= 8) {
        return false;
    }
    return CPedGroups::ms_groups[group].GetMembership().IsLeader(ped);
}

//! 1776 SET_GROUP_SEPARATION_RANGE (case @0x498801): group, range
//! CollectParameters(2); group = GetActualScriptThingIndex(P0, 8) in [0, 8) => ms_groups[group] + 0x2C (membership.m_separationRange) = range
void SetGroupSeparationRange(int32 groupHandle, float range) {
    const auto group = CTheScripts::GetActualScriptThingIndex(groupHandle, SCRIPT_THING_PED_GROUP);
    if (group < 0 || group >= 8) {
        return;
    }
    CPedGroups::ms_groups[group].GetMembership().SetSeparationRange(range);
}

//! 1781 GET_SCRIPT_FIRE_COORDS (case @0x49888D): fire => 3 floats
//! CollectParameters(1); idx = GetActualScriptThingIndex(P0, 5) in [0, 60) => CFireManager::GetScriptFireCoords(idx) [0x5397E0], else (0, 0, 0)
CVector GetScriptFireCoords(int32 fireHandle) {
    const auto idx = CTheScripts::GetActualScriptThingIndex(fireHandle, SCRIPT_THING_FIRE);
    if (idx >= 0 && idx < 60) {
        return gFireManager.GetScriptFireCoords((int16)idx);
    }
    return CVector{ 0.0f, 0.0f, 0.0f };
}

//! 1784 GET_NTH_CLOSEST_CAR_NODE_WITH_HEADING (case @0x498900): x, y, z, n => x, y, z, heading + cmp
//! CollectParameters(4); z <= -100 => ground Z. CPathFind::FindNthNodeClosestToCoors(pos, type 0, maxDist 999999.875f (0x497423FE),
//! lowTraffic 0, unk 1, n - 1, boats 0, ignoreInterior 0, null) [0x44F8C0] => node; CPathFind::FindNodeCoorsForScript(node, &found) [0x4505E0].
//! Found: 4 results = coords + CPathFind::FindNodeOrientationForCarPlacement(node) [0x450320], flag true. Else 4 zeros and flag false.
//! (StoreParameters(4) comes first, then UpdateCompareFlag)
OpcodeResult GetNthClosestCarNodeWithHeading(CRunningScript& S, float x, float y, float z, int32 n) {
    z = GroundZIfAuto(x, y, z);
    const auto node = ThePaths.FindNthNodeClosestToCoors(
        CVector{ x, y, z }, 0, std::bit_cast<float>(0x497423FEu), false, true, n - 1, false, false, nullptr
    );
    bool         found = false;
    const CVector pos  = ThePaths.FindNodeCoorsForScript(node, &found);
    if (found) {
        StoreArg(&S, pos.x);
        StoreArg(&S, pos.y);
        StoreArg(&S, pos.z);
        StoreArg(&S, ThePaths.FindNodeOrientationForCarPlacement(node));
    } else {
        StoreArg(&S, 0.0f);
        StoreArg(&S, 0.0f);
        StoreArg(&S, 0.0f);
        StoreArg(&S, 0.0f);
    }
    S.UpdateCompareFlag(found);
    return OR_CONTINUE;
}

//! 1788 DOES_CAR_HAVE_STUCK_CAR_CHECK (case @0x498A4B): car handle => cmp
//! CollectParameters(1); CStuckCarCheck::IsCarInStuckCarArray(handle) [0x463C70] on CTheScripts::StuckCars (0xA90AB0)
bool DoesCarHaveStuckCarCheck(int32 carHandle) {
    return CTheScripts::StuckCars.IsCarInStuckCarArray(carHandle);
}

//! 1789 SET_PLAYBACK_SPEED (case @0x498A86): vehicle, speed
//! CollectParameters(2); CVehicleRecording::SetPlaybackSpeed(veh, speed) [0x459660]
void SetPlaybackSpeed(CVehicle* veh, float speed) {
    CVehicleRecording::SetPlaybackSpeed(veh, speed);
}

//! 1791 ARE_ANY_CHARS_NEAR_CHAR (case @0x498AB7): ped, radius => cmp
//! CollectParameters(2); the ped is NOT null checked. radiusSq = (float)(r * r) (float spill); every other ped of the ped pool
//! (from the last slot down to 0, stops at the first hit) whose offset to this ped (float differences) has an extended precision
//! squared 3D magnitude (0x406DA0: ((x*x + y*y) + z*z)) strictly < radiusSq (`test ah, 5; jp`: ordered "below") sets the flag
bool AreAnyCharsNearChar(CPed& ped, float radius) {
    const CVector pos    = ped.GetPosition();
    const float   radSq  = (float)((double)radius * (double)radius);
    auto* const   pool   = GetPedPool();
    bool          result = false;
    for (auto i = pool->GetSize(); i != 0;) {
        --i;
        if (result) {
            break;
        }
        auto* const other = pool->GetAt(i);
        if (!other || other == &ped) {
            continue;
        }
        const CVector otherPos = other->GetPosition();
        const CVector d{ otherPos.x - pos.x, otherPos.y - pos.y, otherPos.z - pos.z };
        const double  sq = ((double)d.x * d.x + (double)d.y * d.y) + (double)d.z * d.z;
        if (sq < (double)radSq) {
            result = true;
        }
    }
    return result;
}

//! 1793 SKIP_CUTSCENE_END (case @0x498BCF): no parameters; script->m_SceneSkipIP (+0xD8) = 0
void SkipCutsceneEnd(CRunningScript& S) {
    S.m_SceneSkipIP = 0;
}

//! 1794 GET_PERCENTAGE_TAGGED_IN_AREA (case @0x498BE0): x1, y1, x2, y2 => 1 int
//! CollectParameters(4); the 2 corners are put in ascending order, CTagManager::GetPercentageTaggedInArea(&rect) [0x49D0B0]
int32 GetPercentageTaggedInArea(float x1, float y1, float x2, float y2) {
    return CTagManager::GetPercentageTaggedInArea(SortedRect(x1, y1, x2, y2));
}

//! 1795 SET_TAG_STATUS_IN_AREA (case @0x498CA4): x1, y1, x2, y2, status
//! CollectParameters(5); same rect; CTagManager::SetAlphaInArea(&rect, status != 0 ? 0xFF : 0) [0x49CFE0]
void SetTagStatusInArea(float x1, float y1, float x2, float y2, int32 status) {
    CTagManager::SetAlphaInArea(SortedRect(x1, y1, x2, y2), status != 0 ? 0xFF : 0);
}

//! 1796 CAR_GOTO_COORDINATES_RACING (case @0x498D79): car, x, y, z
//! CollectParameters(4); the car is NOT null checked. z <= -100 => ground Z; z += GetDistanceFromCentreOfMassToBaseOfModel [0x536BE0];
//! JoinCarWithRoadSystemGotoCoors(car, pos, 0, 0) [0x42F870]; mission (unless 57 / 58) = joined ? 9 : 33 (0x21);
//! status = PHYSICS; engine on; cruise = max(cruise, 1); mission start time = now
void CarGotoCoordinatesRacing(CVehicle& veh, CVector pos) {
    pos.z = GroundZIfAuto(pos.x, pos.y, pos.z);
    pos.z = veh.GetDistanceFromCentreOfMassToBaseOfModel() + pos.z;
    const bool joined = CCarCtrl::JoinCarWithRoadSystemGotoCoors(&veh, pos, false, false);
    auto& ap = veh.m_autoPilot;
    if (ap.m_nCarMission != MISSION_PLANE_CRASH_AND_BURN && ap.m_nCarMission != MISSION_HELI_CRASH_AND_BURN) { // 0x39 / 0x3A
        ap.m_nCarMission = joined ? MISSION_GOTOCOORDINATES_STRAIGHTLINE : MISSION_GOTOCOORDINATES_RACING;
    }
    veh.SetStatus(STATUS_PHYSICS);
    VehSetEngineOn(veh);
    if (ap.m_nCruiseSpeed <= 1) { // `max(cruise, 1)` (unsigned compare)
        ap.m_nCruiseSpeed = 1;
    }
    ap.m_nTimeToStartMission = CTimer::GetTimeInMS();
}

//! 1797 START_PLAYBACK_RECORDED_CAR_USING_AI (case @0x498E78): vehicle, path
//! CollectParameters(2); CVehicleRecording::StartPlaybackRecordedCar(veh, path, true (useCarAI), false) [0x45A980]
void StartPlaybackRecordedCarUsingAi(CVehicle* veh, int32 path) {
    CVehicleRecording::StartPlaybackRecordedCar(veh, path, true, false);
}

//! 1798 SKIP_IN_PLAYBACK_RECORDED_CAR (case @0x498EAA): vehicle, distance
//! CollectParameters(2); CVehicleRecording::SkipForwardInRecording(veh, distance) [0x459D10]
void SkipInPlaybackRecordedCar(CVehicle* veh, float distance) {
    CVehicleRecording::SkipForwardInRecording(veh, distance);
}

//! 1799 SKIP_CUTSCENE_START_INTERNAL (case @0x498ED8): ip
//! CollectParameters(1); script->m_SceneSkipIP (+0xD8) = P0
void SkipCutsceneStartInternal(CRunningScript& S, int32 ip) {
    S.m_SceneSkipIP = ip;
}
}; // namespace

namespace notsa::script::commands::ported::g17c {
void RegisterHandlers() {
    REGISTER_COMMAND_HANDLER_BEGIN("Ported g17c");

    REGISTER_COMMAND_HANDLER(COMMAND_SKIP_TO_END_AND_STOP_PLAYBACK_RECORDED_CAR, SkipToEndAndStopPlaybackRecordedCar);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_AVAILABLE_VEHICLE_MOD, GetAvailableVehicleMod);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_VEHICLE_MOD_TYPE, GetVehicleModType);
    REGISTER_COMMAND_HANDLER(COMMAND_ADD_VEHICLE_MOD, AddVehicleMod);
    REGISTER_COMMAND_HANDLER(COMMAND_REMOVE_VEHICLE_MOD, RemoveVehicleMod);
    REGISTER_COMMAND_HANDLER(COMMAND_REQUEST_VEHICLE_MOD, RequestVehicleMod);
    REGISTER_COMMAND_HANDLER(COMMAND_HAS_VEHICLE_MOD_LOADED, HasVehicleModLoaded);
    REGISTER_COMMAND_HANDLER(COMMAND_MARK_VEHICLE_MOD_AS_NO_LONGER_NEEDED, MarkVehicleModAsNoLongerNeeded);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_NUM_AVAILABLE_PAINTJOBS, GetNumAvailablePaintjobs);
    REGISTER_COMMAND_HANDLER(COMMAND_GIVE_VEHICLE_PAINTJOB, GiveVehiclePaintjob);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_GROUP_MEMBER, IsGroupMember);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_GROUP_LEADER, IsGroupLeader);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_GROUP_SEPARATION_RANGE, SetGroupSeparationRange);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_SCRIPT_FIRE_COORDS, GetScriptFireCoords);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_NTH_CLOSEST_CAR_NODE_WITH_HEADING, GetNthClosestCarNodeWithHeading);
    REGISTER_COMMAND_HANDLER(COMMAND_DOES_CAR_HAVE_STUCK_CAR_CHECK, DoesCarHaveStuckCarCheck);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_PLAYBACK_SPEED, SetPlaybackSpeed);
    // 1790: the exe's switch maps it to the common tail (`xor al, al`) without reading a parameter: a no-op (it is unused by the game scripts)
    REGISTER_COMMAND_NOP(COMMAND_GET_CAR_VALUE);
    REGISTER_COMMAND_HANDLER(COMMAND_ARE_ANY_CHARS_NEAR_CHAR, AreAnyCharsNearChar);
    REGISTER_COMMAND_HANDLER(COMMAND_SKIP_CUTSCENE_END, SkipCutsceneEnd);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_PERCENTAGE_TAGGED_IN_AREA, GetPercentageTaggedInArea);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_TAG_STATUS_IN_AREA, SetTagStatusInArea);
    REGISTER_COMMAND_HANDLER(COMMAND_CAR_GOTO_COORDINATES_RACING, CarGotoCoordinatesRacing);
    REGISTER_COMMAND_HANDLER(COMMAND_START_PLAYBACK_RECORDED_CAR_USING_AI, StartPlaybackRecordedCarUsingAi);
    REGISTER_COMMAND_HANDLER(COMMAND_SKIP_IN_PLAYBACK_RECORDED_CAR, SkipInPlaybackRecordedCar);
    REGISTER_COMMAND_HANDLER(COMMAND_SKIP_CUTSCENE_START_INTERNAL, SkipCutsceneStartInternal);
}
}; // namespace notsa::script::commands::ported::g17c

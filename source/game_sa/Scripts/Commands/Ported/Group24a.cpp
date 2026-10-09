#include <StdInc.h>

#include "../Commands.hpp"
#include <CommandParser/Parser.hpp>
#include "Group24.hpp"

#include <bit>
#include <cmath>
#include <cstddef>
#include <cstring>

#include "Pad.h"
#include "World.h"
#include "Shopping.h"
#include "GameLogic.h"
#include "MenuSystem.h"
#include "PedType.h"
#include "EntryExitManager.h"
#include "MissionCleanup.h"
#include "Fx/FxManager.h"
#include "FireManager.h"
#include "Audio/AudioEngine.h"
#include "Core/KeyGen.h"
#include "WaterLevel.h"
#include "ScriptsForBrains.h"
#include "ScriptResourceManager.h"
#include "cHandlingDataMgr.h"
#include "Models/ModelInfo.h"
#include "Models/VehicleModelInfo.h"
#include "Entity/Vehicle/Vehicle.h"
#include "Entity/Vehicle/Train.h"
#include "Entity/Vehicle/Automobile.h"
#include "Entity/Object/Object.h"
#include "DecisionMakers/DecisionMakerTypes.h"
#include "Tasks/TaskTypes/TaskSimpleSwim.h"
#include "Tasks/TaskTypes/TaskComplexFacial.h"
#include "Hud.h"
#include "Population.h"
#include "Events/EventGunShot.h"
#include "Entity/Vehicle/Heli.h"
#include "Scripts/RunningScript.h"
#include "Pools/Pools.h"

using namespace notsa::script;
using namespace notsa::script::commands::ported::g24;

/*!
* Script commands ported from the exe's group processor for ids 2400..2499 (`CRunningScript::ProcessCommands2400To2499` @0x478000, switch base 2400
* (`lea eax, [edi - 0x960]`), 100 entries, jump table @0x479AFC) for the vanilla commands that had no handler of their own (S6-I, g24a: 2400..2446).
* Every handler was written from the asm of the `case` (see `.notes/S6I_TABLE.md`), not from the Ghidra decompilation.
* The pool getters used by the cases (GetAtRef of the ped / vehicle / object pools) are not null checked in the exe; neither are the handlers here
* (`CPed&`, `CVehicle&`, `CObject&` parameters: the parser asserts instead of crashing).
*/

// Raw offsets the exe uses directly in these cases
static_assert(offsetof(CPad, bDisablePlayerCycleWeapon) == 0x11D && offsetof(CPad, bDisablePlayerDisplayVitalStats) == 0x11F);
static_assert(sizeof(CPad) == 0x134); // CPad::GetPad (0x53FB70): 0xB73458 + id * 0x134
static_assert(offsetof(CTaskSimpleSwim, m_nSwimState) == 0xA);
static_assert(offsetof(CPhysical, m_pEntityIgnoredCollision) == 0x128);
static_assert(offsetof(CVehicle, vehicleFlags) == 0x428);
static_assert(offsetof(CVehicle, m_autoPilot) + offsetof(CAutoPilot, m_ObstructingEntity) == 0x420);
static_assert(offsetof(CVehicle, m_nDoorLock) == 0x4F8);
static_assert(offsetof(CTrain, m_nTrainFlags) + 1 == 0x5B9);
static_assert(offsetof(CVehicleModelInfo, m_nNumColorVariations) == 0x2D0);
static_assert(offsetof(CVehicleModelInfo, m_nHandlingId) == 0x4A);
static_assert(sizeof(tHandlingData) == 0xE0 && offsetof(cHandlingDataMgr, m_aVehicleHandling) == 0x14);
static_assert(offsetof(tHandlingData, m_nHandlingFlags) == 0xD0);
static_assert(offsetof(CPlayerInfo, m_pPed) == 0 && sizeof(CPlayerInfo) == 0x190); // Players[PlayerInFocus] @0xB7CD98 + id * 0x190
static_assert(sizeof(tScriptEffectSystem) == 8 && offsetof(tScriptEffectSystem, m_pFxSystem) == 4);
static_assert(SCRIPT_THING_EFFECT_SYSTEM == 1 && SCRIPT_THING_FIRE == 5 && SCRIPT_THING_DECISION_MAKER == 7);
static_assert(MISSION_CLEANUP_ENTITY_TYPE_PARTICLE == 4 && MISSION_CLEANUP_ENTITY_TYPE_DECISION_MAKER == 9 && RESOURCE_TYPE_DECISION_MAKER == 3);
static_assert(TASK_SECONDARY_FACIAL_COMPLEX == 3 && (int32)eFacialExpression::NONE == -1 && (int32)eFacialExpression::TALKING == 7);
static_assert(CMenuSystem::MENU_TYPE_GRID == 1);
static_assert(offsetof(CRunningScript, m_UsesMissionCleanup) == 0xC6);
static_assert(sizeof(CDecisionMaker) == 0x99C);
static_assert(offsetof(CDecisionMakerTypes, m_DecisionMakers) == 4);
static_assert(offsetof(CDecisionMakerTypes, m_DefaultMissionPedDecisionMaker) == 0xCB50);
static_assert(offsetof(CDecisionMakerTypes, m_DefaultMissionPedGroupDecisionMaker) == 0xE824);

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

//! 0x6070F0 (unreversed): adds a copy of a decision maker (by index; -1 => the default mission ped (type 0) / group (type != 0) one)
//! to the first free slot: tail calls `CDecisionMakerTypes::AddDecisionMaker` (0x607050)
int32 CopyDecisionMakerOriginal(CDecisionMakerTypes& types, int32 srcIndex, int32 type, bool forMission) {
    CDecisionMaker* src;
    if (srcIndex == -1) {
        src = type == 0 ? &types.m_DefaultMissionPedDecisionMaker : &types.m_DefaultMissionPedGroupDecisionMaker; // +0xCB50 / +0xE824
    } else {
        src = types.m_DecisionMakers.data() + srcIndex; // +4 + srcIndex * 0x99C (not bounds checked)
    }
    return types.AddDecisionMaker(src, (eDecisionTypes)type, forMission);
}

//! 0x43EF20 (unreversed): for every entry/exit whose name (first 8 chars) matches: sets / clears `flag` in its flags (+0x30)
void SetEntryExitFlagByName(const char* name, uint16 flag, bool enable) {
    for (auto& enex : CEntryExitManager::GetPool()->GetAllValid()) {
        if (strncmp(enex.m_szName, name, 8) != 0) {
            continue;
        }
        if (enable) {
            enex.m_nFlags |= flag;
        } else {
            enex.m_nFlags &= ~flag;
        }
    }
}

//! 0x4082C0 (`CVector::Magnitude`): the sum of squares and the square root stay in extended precision
double MagnitudeOriginal(const CVector& v) {
    return std::sqrt(((double)v.x * v.x + (double)v.y * v.y) + (double)v.z * v.z);
}

//! 2400 SET_PLAYER_DISPLAY_VITAL_STATS_BUTTON (case @0x47803E): pad, enable  -- CPad::GetPad(pad)->bDisablePlayerDisplayVitalStats (+0x11F) = (enable == 0)
void SetPlayerDisplayVitalStatsButton(int32 padId, int32 enable) {
    CPad::GetPad(padId)->bDisablePlayerDisplayVitalStats = (enable == 0); // 0x53FB70
}

//! 2401 SET_CHAR_KEEP_TASK (case @0x478089): ped, flag  -- bKeepTasksAfterCleanUp (+0x478 bit 4) = (flag != 0)
void SetCharKeepTask(CPed& ped, int32 flag) {
    ped.bKeepTasksAfterCleanUp = flag != 0;
}

//! 2404 CREATE_MENU_GRID (case @0x4780D1): title label (8; "DUMMY" => ""), x, y, width, columns, interactive, background, alignment => menu id (zero-extended byte)
//! Same as CREATE_MENU (2260) but with the grid menu type (1). x = x * (maximumWidth * 1/640) [0x859520], width = width * (maximumWidth * 1/640),
//! y = y * (maximumHeight * 1/448) [0x859524] (all x87, stored as float)
OpcodeResult CreateMenuGrid(CRunningScript& S) {
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
        CMenuSystem::MENU_TYPE_GRID,
        title,
        x,
        y,
        w,
        (uint8)ScriptParams[3].iParam,
        ScriptParams[4].iParam != 0,
        ScriptParams[5].iParam != 0,
        (eFontAlignment)ScriptParams[6].iParam
    );
    ScriptParams[0].iParam = (int32)(uint8)id; // movzx
    S.StoreParameters(1);
    return OR_CONTINUE;
}

//! 2405 IS_CHAR_SWIMMING (case @0x47819F): ped => compare flag: CPedIntelligence::GetTaskSwim() != null [0x601070]
bool IsCharSwimming(CPed& ped) {
    return ped.GetIntelligence()->GetTaskSwim() != nullptr;
}

//! 2406 GET_CHAR_SWIM_STATE (case @0x4781E1): ped => the swim task's state (+0xA, movsx word). The task is not null checked.
int32 GetCharSwimState(CPed& ped) {
    return (int32)(int16)ped.GetIntelligence()->GetTaskSwim()->m_nSwimState;
}

//! 2407 START_CHAR_FACIAL_TALK (case @0x478221): ped, duration  -- the secondary facial task (slot 3) ->SetRequest(TALKING (7), duration, NONE (-1), 0) [0x691230]. Not null checked.
void StartCharFacialTalk(CPed& ped, int32 duration) {
    auto* const task = static_cast<CTaskComplexFacial*>(ped.GetIntelligence()->m_TaskMgr.GetTaskSecondary(TASK_SECONDARY_FACIAL_COMPLEX)); // 0x681810
    task->SetRequest(eFacialExpression::TALKING, duration, eFacialExpression::NONE, 0);
}

//! 2408 STOP_CHAR_FACIAL_TALK (case @0x478266): ped  -- the secondary facial task (slot 3) ->StopAll() [0x691250]. Not null checked.
void StopCharFacialTalk(CPed& ped) {
    static_cast<CTaskComplexFacial*>(ped.GetIntelligence()->m_TaskMgr.GetTaskSecondary(TASK_SECONDARY_FACIAL_COMPLEX))->StopAll();
}

//! 2409 IS_BIG_VEHICLE (case @0x47829F): car => compare flag: bIsBig (+0x429 bit 2)
bool IsBigVehicle(CVehicle& veh) {
    return veh.vehicleFlags.bIsBig;
}

//! 2410 SWITCH_POLICE_HELIS (case @0x4782D9): enable  -- SwitchPoliceHelis(enable != 0) [0x6C4800]
void SwitchPoliceHelisCmd(int32 enable) {
    CHeli::SwitchPoliceHelis(enable != 0);
}

//! 2411 STORE_CAR_MOD_STATE (case @0x4782FD): CShopping::StoreVehicleMods() [0x49B280]
void StoreCarModState() {
    CShopping::StoreVehicleMods();
}

//! 2412 RESTORE_CAR_MOD_STATE (case @0x478309): CShopping::RestoreVehicleMods() [0x49B3C0]
void RestoreCarModState() {
    CShopping::RestoreVehicleMods();
}

//! 2413 GET_CURRENT_CAR_MOD (case @0x478315): car, slot => upgrade model (-1 if none). Jump table @0x479C8C (17 entries, the slot is compared unsigned):
//! 0: GetUpgrade(0)   1: GetUpgrade(1) or (if -1) GetUpgrade(2)   2: (6)   3: (8) or (if -1) (9)   4: (10)   5: (11)   6: (12)   7: (14)   8: (15)   9: (16)   10: (17)
//! 11: -1   12: GetReplacementUpgrade(2)   13: (0x13)   14: (12)   15: (13)   16: (0x14)   (default: -1)   [0x6D3650 / 0x6D3A50]
int32 GetCurrentCarMod(CVehicle& veh, int32 slotRaw) {
    switch ((uint32)slotRaw) {
    case 0:  return veh.GetUpgrade(0);
    case 1: {
        const auto r = veh.GetUpgrade(1);
        return r != -1 ? r : veh.GetUpgrade(2);
    }
    case 2:  return veh.GetUpgrade(6);
    case 3: {
        const auto r = veh.GetUpgrade(8);
        return r != -1 ? r : veh.GetUpgrade(9);
    }
    case 4:  return veh.GetUpgrade(10);
    case 5:  return veh.GetUpgrade(11);
    case 6:  return veh.GetUpgrade(12);
    case 7:  return veh.GetUpgrade(14);
    case 8:  return veh.GetUpgrade(15);
    case 9:  return veh.GetUpgrade(16);
    case 10: return veh.GetUpgrade(17);
    case 12: return veh.GetReplacementUpgrade(2);
    case 13: return veh.GetReplacementUpgrade(0x13);
    case 14: return veh.GetReplacementUpgrade(12);
    case 15: return veh.GetReplacementUpgrade(13);
    case 16: return veh.GetReplacementUpgrade(0x14);
    default: return -1; // 11 and > 16
    }
}

//! The handling entry the exe reaches through the model info (byte +0x4A of the vehicle model info; table @0xC2B9DC, stride 0xE0), NOT through the vehicle's own pointer
const tHandlingData& HandlingOfModel(CVehicle& veh) {
    const auto* const mi = static_cast<CVehicleModelInfo*>(CModelInfo::ms_modelInfoPtrs[(int16)veh.m_nModelIndex]); // 0xA9B0C8, movsx word +0x22
    return gHandlingDataMgr.m_aVehicleHandling[(uint8)mi->m_nHandlingId];
}

//! 2414 IS_CAR_LOW_RIDER (case @0x4784E6): car => compare flag: handling flags (+0xD0) bit 25 (m_bLowRider)
bool IsCarLowRider(CVehicle& veh) {
    return HandlingOfModel(veh).m_bLowRider;
}

//! 2415 IS_CAR_STREET_RACER (case @0x478534): car => compare flag: handling flags (+0xD0) bit 26 (m_bStreetRacer)
bool IsCarStreetRacer(CVehicle& veh) {
    return HandlingOfModel(veh).m_bStreetRacer;
}

//! 2416 FORCE_DEATH_RESTART (case @0x478581): CGameLogic::ForceDeathRestart() [0x441240]
void ForceDeathRestart() {
    CGameLogic::ForceDeathRestart();
}

//! 2417 SYNC_WATER (case @0x47858D): CWaterLevel::m_nWaterTimeOffset (0xC228A4) = CTimer::m_snTimeInMilliseconds (0xB7CB84) -- inlined in the exe
void SyncWater() {
    CWaterLevel::SyncWater();
}

//! 2418 SET_CHAR_COORDINATES_NO_OFFSET (case @0x478599): ped, x, y, z  -- SetCharCoordinates(ped, pos, warpGang = true, offset = false) [0x464DC0]
void SetCharCoordinatesNoOffset(CRunningScript& S, CPed& ped, CVector pos) {
    S.SetCharCoordinates(ped, pos, true, false);
}

//! 2419 DOES_SCRIPT_FIRE_EXIST (case @0x4785C6): handle => compare flag: GetActualScriptThingIndex(handle, SCRIPT_THING_FIRE) in [0, 60)
bool DoesScriptFireExist(int32 handle) {
    const auto idx = CTheScripts::GetActualScriptThingIndex(handle, SCRIPT_THING_FIRE); // 0x4839A0
    return idx >= 0 && idx < 60;
}

//! 2420 RESET_STUFF_UPON_RESURRECTION (case @0x478605): CGameLogic::ResetStuffUponResurrection() [0x442980]
void ResetStuffUponResurrection() {
    CGameLogic::ResetStuffUponResurrection();
}

//! 2421 IS_EMERGENCY_SERVICES_VEHICLE (case @0x478611): car => compare flag: IsLawEnforcementVehicle() [0x6D2370] || model 416 (ambulan) / 407 (firetruk) / 544 (firela)
bool IsEmergencyServicesVehicle(CVehicle& veh) {
    const int32 model = (int16)veh.m_nModelIndex; // movsx word +0x22
    return veh.IsLawEnforcementVehicle() || model == 0x1A0 || model == 0x197 || model == 0x220;
}

//! 2422 KILL_FX_SYSTEM_NOW (case @0x47866F): handle  -- idx = GetActualScriptThingIndex(handle, SCRIPT_THING_EFFECT_SYSTEM); if idx >= 0 and the slot's system (+4) is set:
//! g_fxMan.DestroyFxSystem(system) [0x4A9810]; CTheScripts::RemoveScriptEffectSystem(handle) [0x492FD0]; if the script uses mission cleanup: RemoveEntityFromList(handle, 4 (particle))
void KillFxSystemNow(CRunningScript& S, int32 handle) {
    const auto idx = CTheScripts::GetActualScriptThingIndex(handle, SCRIPT_THING_EFFECT_SYSTEM);
    if (idx < 0) {
        return;
    }
    auto* const fx = CTheScripts::ScriptEffectSystemArray[idx].m_pFxSystem; // 0xA44114 + idx * 8
    if (!fx) {
        return;
    }
    g_fxMan.DestroyFxSystem(fx);
    CTheScripts::RemoveScriptEffectSystem(handle);
    if (S.m_UsesMissionCleanup) {
        CTheScripts::MissionCleanUp.RemoveEntityFromList(handle, MISSION_CLEANUP_ENTITY_TYPE_PARTICLE); // 0x4654B0
    }
}

//! 2423 IS_OBJECT_WITHIN_BRAIN_ACTIVATION_RANGE (case @0x4786D6): object => compare flag: the player in focus has a ped, the object exists and
//! CScriptsForBrains::IsObjectWithinBrainActivationRange(object, FindPlayerCentreOfWorld(PlayerInFocus)) [0x46B3D0]
bool IsObjectWithinBrainActivationRange(CObject* obj) {
    const auto playerId = CWorld::PlayerInFocus;
    if (!CWorld::Players[playerId].m_pPed || !obj) {
        return false;
    }
    const CVector centre = FindPlayerCentreOfWorld(playerId); // 0x56E250 (copied)
    return CTheScripts::ScriptsForBrains.IsObjectWithinBrainActivationRange(obj, centre);
}

//! 2424 COPY_SHARED_CHAR_DECISION_MAKER (case @0x47875E): dm handle => dm handle (the output variable is peeked: if it already holds a valid decision maker handle
//! (GetActualScriptThingIndex(peek, 7) != -1) it is kept, no copy is made). Otherwise the (script) decision maker is copied (type 0) like COPY_CHAR_DECISION_MAKER (2021).
//! In both cases the handle is registered with the script resource manager (type 3) [0x4704B0] before it is stored.
OpcodeResult CopySharedCharDecisionMaker(CRunningScript& S, int32 dmHandle) {
    int32 srcIndex = -1;
    if (dmHandle != -1) {
        srcIndex = CTheScripts::GetActualScriptThingIndex(dmHandle, SCRIPT_THING_DECISION_MAKER);
    }
    const int32 peek = S.CollectNextParameterWithoutIncreasingPC(); // 0x464250 (the output variable's current value)
    int32 handle;
    if (CTheScripts::GetActualScriptThingIndex(peek, SCRIPT_THING_DECISION_MAKER) != -1) {
        handle = peek;
    } else {
        const auto newIndex = CopyDecisionMakerOriginal(*CDecisionMakerTypes::GetInstance(), srcIndex, 0, S.m_UsesMissionCleanup); // 0x4684F0, 0x6070F0
        handle = (int32)CTheScripts::GetNewUniqueScriptThingIndex(newIndex, SCRIPT_THING_DECISION_MAKER);
        if (S.m_UsesMissionCleanup) {
            CTheScripts::MissionCleanUp.AddEntityToList(handle, MISSION_CLEANUP_ENTITY_TYPE_DECISION_MAKER); // 0x4637E0
        }
    }
    CTheScripts::ScriptResourceManager.AddToResourceManager(handle, RESOURCE_TYPE_DECISION_MAKER, &S); // 0x4704B0
    StoreArg(&S, handle);
    return OR_CONTINUE;
}

//! 2426 REPORT_MISSION_AUDIO_EVENT_AT_POSITION (case @0x478805): x, y, z, event (low word)  -- AudioEngine.ReportMissionAudioEvent(event, pos) [0x507340]
void ReportMissionAudioEventAtPosition(CVector pos, int32 event) {
    AudioEngine.ReportMissionAudioEvent((uint16)event, pos);
}

//! 2427 REPORT_MISSION_AUDIO_EVENT_AT_OBJECT (case @0x47884B): object, event (low word)  -- AudioEngine.ReportMissionAudioEvent(event, object) [0x507350] (object may be null)
void ReportMissionAudioEventAtObject(CObject* obj, int32 event) {
    AudioEngine.ReportMissionAudioEvent((uint16)event, obj);
}

//! 2428 ATTACH_MISSION_AUDIO_TO_OBJECT (case @0x478881): slot, object  -- AudioEngine.AttachMissionAudioToObject((uint8)(slot - 1), object) [0x507320] (object may be null)
void AttachMissionAudioToObject(int32 slot, CObject* obj) {
    AudioEngine.AttachMissionAudioToObject((uint8)(slot - 1), obj);
}

//! 2429 GET_NUM_CAR_COLOURS (case @0x4788B8): car => the model info's colour variation count (+0x2D0, zero-extended byte)
int32 GetNumCarColours(CVehicle& veh) {
    return static_cast<CVehicleModelInfo*>(CModelInfo::ms_modelInfoPtrs[(int16)veh.m_nModelIndex])->m_nNumColorVariations; // movsx word +0x22
}

//! 2432 EXTINGUISH_FIRE_AT_POINT (case @0x4788FB): x, y, z, radius  -- gFireManager.ExtinguishPoint(pos, radius) [0x539450]
void ExtinguishFireAtPoint(CVector pos, float radius) {
    gFireManager.ExtinguishPoint(pos, radius);
}

//! 2433 HAS_TRAIN_DERAILED (case @0x478952): car => compare flag: +0x5B9 bit 0 (CTrain::trainFlags.bNotOnARailRoad, the vehicle is NOT checked to be a train)
bool HasTrainDerailed(CVehicle& veh) {
    return reinterpret_cast<CTrain&>(veh).trainFlags.bNotOnARailRoad;
}

//! 2434 SET_CHAR_FORCE_DIE_IN_CAR (case @0x478988): ped, flag  -- bForceDieInCar (+0x478 bit 7) = bit 0 of flag (shl 7; xor; and 0x80)
void SetCharForceDieInCar(CPed& ped, int32 flag) {
    ped.bForceDieInCar = (flag & 1) != 0;
}

//! 2435 SET_ONLY_CREATE_GANG_MEMBERS (case @0x4789BF): flag  -- CPopulation::m_bOnlyCreateRandomGangMembers (0xC0FCB3) = (flag == 0) (sic)
void SetOnlyCreateGangMembers(int32 flag) {
    CPopulation::m_bOnlyCreateRandomGangMembers = flag == 0;
}

//! 2436 GET_OBJECT_MODEL (case @0x4789DE): object => model index (+0x22, movsx word)
int32 GetObjectModel(CObject& obj) {
    return (int32)(int16)obj.m_nModelIndex; // movsx
}

//! 2437 SET_CHAR_USES_COLLISION_CLOSEST_OBJECT_OF_TYPE (case @0x478A13): x, y, z, radius, model, flag, ped
//! z <= -100 (ordered) => ground z. Objects of the model within `radius` are collected (FindObjectsOfTypeInRange(model, pos, radius, 2D, &count, 16, list, buildings,
//! !vehicles, !peds, objects, dummies) [0x564C70]); the closest one (strictly nearer than 2 * radius, extended precision, the best distance is stored as float) is
//! kept. If there is one: ped->m_pEntityIgnoredCollision (+0x128) = (flag != 0 ? null : object). The ped is looked up (not null checked) after the search.
void SetCharUsesCollisionClosestObjectOfType(CVector pos, float radius, Model model, int32 flag, CPed* ped) {
    if (pos.z <= -100.0f) { // 0x859014
        pos.z = CWorld::FindGroundZForCoord(pos.x, pos.y);
    }
    float bestDist = radius + radius; // fadd st(0), st(0) then stored as float
    CEntity* found[16];
    int16    count = 0;
    CWorld::FindObjectsOfTypeInRange((uint32)model.value, pos, radius, true, &count, 16, found, true, false, false, true, true);

    CEntity* best = nullptr;
    for (int32 i = 0; i < count; ++i) {
        const auto& p = found[i]->GetPosition();
        const CVector diff{ p.x - pos.x, p.y - pos.y, p.z - pos.z };
        const double  dist = MagnitudeOriginal(diff); // 0x4082C0
        if (dist < (double)bestDist) {
            bestDist = (float)dist;
            best     = found[i];
        }
    }
    if (!best) {
        return;
    }
    ped->m_pEntityIgnoredCollision = flag != 0 ? nullptr : best;
}

//! 2438 CLEAR_ALL_SCRIPT_FIRE_FLAGS (case @0x478BB7): gFireManager.ClearAllScriptFireFlags() [0x5397A0]
void ClearAllScriptFireFlags() {
    gFireManager.ClearAllScriptFireFlags();
}

//! 2439 GET_CAR_BLOCKING_CAR (case @0x478BC8): car => vehicle handle of the car's obstructing entity (autopilot +0x90 = +0x420) if it is a vehicle ((flags & 7) == 2), else -1
int32 GetCarBlockingCar(CVehicle& veh) {
    const auto* const obstructing = veh.m_autoPilot.m_ObstructingEntity;
    if (obstructing && obstructing->GetIsTypeVehicle()) {
        return GetVehiclePool()->GetRef(const_cast<CVehicle*>(static_cast<const CVehicle*>(obstructing))); // 0x424160
    }
    return -1;
}

//! 2440 GET_CURRENT_VEHICLE_PAINTJOB (case @0x478C1C): car => CVehicle::GetRemapIndex() [0x6D0B70]
int32 GetCurrentVehiclePaintjob(CVehicle& veh) {
    return veh.GetRemapIndex();
}

//! 2441 SET_HELP_MESSAGE_BOX_SIZE (case @0x478C53): size  -- CHud::m_fHelpMessageBoxWidth (0x8D0934) = (float)size
void SetHelpMessageBoxSize(int32 size) {
    CHud::m_fHelpMessageBoxWidth = (float)size;
}

//! 2442 SET_GUNSHOT_SENSE_RANGE_FOR_RIOT2 (case @0x478C6F): range  -- CEventGunShot::ms_fGunShotSenseRangeForRiot2 (0x8A625C) = the raw dword
void SetGunshotSenseRangeForRiot2(float range) {
    CEventGunShot::ms_fGunShotSenseRangeForRiot2 = range;
}

//! 2443 STRING_CAT16 / 2444 STRING_CAT8 (shared case @0x478C89, `edi` = the command id): src1, src2, dst  -- three variables (GetPointerToScriptVariable(2) each).
//! Only if strlen(src1) + strlen(src2) < 16 (CAT16) / 8 (CAT8) the third variable is read (the IP is not advanced otherwise!) and dst = src1 + src2:
//! dst is strcpy'd from src1, then src2 (its length measured AFTER that copy) is appended at the end of dst.
OpcodeResult StringCat(CRunningScript& S, eScriptCommands cmd) {
    const auto* const src1 = reinterpret_cast<const char*>(S.GetPointerToScriptVariable(VAR_GLOBAL));
    const auto* const src2 = reinterpret_cast<const char*>(S.GetPointerToScriptVariable(VAR_GLOBAL));
    const auto total = strlen(src1) + strlen(src2);
    if (cmd == COMMAND_STRING_CAT16) {
        if (total >= 16) {
            return OR_CONTINUE;
        }
    } else if (total >= 8) {
        return OR_CONTINUE;
    }
    auto* const dst = reinterpret_cast<char*>(S.GetPointerToScriptVariable(VAR_GLOBAL));
    strcpy(dst, src1);
    const auto n   = strlen(src2) + 1; // measured after the first copy (dst may alias src2)
    auto*      end = dst + strlen(dst);
    for (size_t i = 0; i < n; ++i) { // rep movs (forward)
        end[i] = src2[i];
    }
    return OR_CONTINUE;
}

//! 2445 GET_CAR_MOVING_COMPONENT_OFFSET (case @0x478D2F): car => CAutomobile::GetMovingCollisionOffset() [0x6A2150] (the vehicle is not checked to be an automobile)
float GetCarMovingComponentOffset(CVehicle& veh) {
    return static_cast<CAutomobile&>(veh).GetMovingCollisionOffset();
}

//! 2446 SET_NAMED_ENTRY_EXIT_FLAG (case @0x478D67): name label (8, read FIRST), flag (word), enable  -- 0x43EF20(name, flag, enable != 0): sets / clears the flag of the entry exits with that name
void SetNamedEntryExitFlag(CRunningScript& S) {
    char label[8 + 1]{}; // NOTSA: + terminator (the exe's buffer is 8 bytes and strncmp's at most 8 chars)
    S.ReadTextLabelFromScript(label, 8); // 0x463D50
    const auto flag   = notsa::script::Read<int32>(&S); // CollectParameters(2)
    const auto enable = notsa::script::Read<int32>(&S);
    SetEntryExitFlagByName(label, (uint16)flag, enable != 0);
}
}; // namespace

void notsa::script::commands::ported::g24::RegisterG24a() {
    REGISTER_COMMAND_HANDLER_BEGIN("Ported g24a");

    REGISTER_COMMAND_HANDLER(COMMAND_SET_PLAYER_DISPLAY_VITAL_STATS_BUTTON, SetPlayerDisplayVitalStatsButton);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_CHAR_KEEP_TASK, SetCharKeepTask);
    REGISTER_COMMAND_HANDLER(COMMAND_CREATE_MENU_GRID, CreateMenuGrid);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_CHAR_SWIMMING, IsCharSwimming);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_CHAR_SWIM_STATE, GetCharSwimState);
    REGISTER_COMMAND_HANDLER(COMMAND_START_CHAR_FACIAL_TALK, StartCharFacialTalk);
    REGISTER_COMMAND_HANDLER(COMMAND_STOP_CHAR_FACIAL_TALK, StopCharFacialTalk);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_BIG_VEHICLE, IsBigVehicle);
    REGISTER_COMMAND_HANDLER(COMMAND_SWITCH_POLICE_HELIS, SwitchPoliceHelisCmd);
    REGISTER_COMMAND_HANDLER(COMMAND_STORE_CAR_MOD_STATE, StoreCarModState);
    REGISTER_COMMAND_HANDLER(COMMAND_RESTORE_CAR_MOD_STATE, RestoreCarModState);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_CURRENT_CAR_MOD, GetCurrentCarMod);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_CAR_LOW_RIDER, IsCarLowRider);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_CAR_STREET_RACER, IsCarStreetRacer);
    REGISTER_COMMAND_HANDLER(COMMAND_FORCE_DEATH_RESTART, ForceDeathRestart);
    REGISTER_COMMAND_HANDLER(COMMAND_SYNC_WATER, SyncWater);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_CHAR_COORDINATES_NO_OFFSET, SetCharCoordinatesNoOffset);
    REGISTER_COMMAND_HANDLER(COMMAND_DOES_SCRIPT_FIRE_EXIST, DoesScriptFireExist);
    REGISTER_COMMAND_HANDLER(COMMAND_RESET_STUFF_UPON_RESURRECTION, ResetStuffUponResurrection);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_EMERGENCY_SERVICES_VEHICLE, IsEmergencyServicesVehicle);
    REGISTER_COMMAND_HANDLER(COMMAND_KILL_FX_SYSTEM_NOW, KillFxSystemNow);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_OBJECT_WITHIN_BRAIN_ACTIVATION_RANGE, IsObjectWithinBrainActivationRange);
    REGISTER_COMMAND_HANDLER(COMMAND_COPY_SHARED_CHAR_DECISION_MAKER, CopySharedCharDecisionMaker);
    REGISTER_COMMAND_HANDLER(COMMAND_REPORT_MISSION_AUDIO_EVENT_AT_POSITION, ReportMissionAudioEventAtPosition);
    REGISTER_COMMAND_HANDLER(COMMAND_REPORT_MISSION_AUDIO_EVENT_AT_OBJECT, ReportMissionAudioEventAtObject);
    REGISTER_COMMAND_HANDLER(COMMAND_ATTACH_MISSION_AUDIO_TO_OBJECT, AttachMissionAudioToObject);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_NUM_CAR_COLOURS, GetNumCarColours);
    REGISTER_COMMAND_HANDLER(COMMAND_EXTINGUISH_FIRE_AT_POINT, ExtinguishFireAtPoint);
    REGISTER_COMMAND_HANDLER(COMMAND_HAS_TRAIN_DERAILED, HasTrainDerailed);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_CHAR_FORCE_DIE_IN_CAR, SetCharForceDieInCar);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_ONLY_CREATE_GANG_MEMBERS, SetOnlyCreateGangMembers);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_OBJECT_MODEL, GetObjectModel);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_CHAR_USES_COLLISION_CLOSEST_OBJECT_OF_TYPE, SetCharUsesCollisionClosestObjectOfType);
    REGISTER_COMMAND_HANDLER(COMMAND_CLEAR_ALL_SCRIPT_FIRE_FLAGS, ClearAllScriptFireFlags);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_CAR_BLOCKING_CAR, GetCarBlockingCar);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_CURRENT_VEHICLE_PAINTJOB, GetCurrentVehiclePaintjob);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_HELP_MESSAGE_BOX_SIZE, SetHelpMessageBoxSize);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_GUNSHOT_SENSE_RANGE_FOR_RIOT2, SetGunshotSenseRangeForRiot2);
    REGISTER_COMMAND_HANDLER(COMMAND_STRING_CAT16, StringCat);
    REGISTER_COMMAND_HANDLER(COMMAND_STRING_CAT8, StringCat);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_CAR_MOVING_COMPONENT_OFFSET, GetCarMovingComponentOffset);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_NAMED_ENTRY_EXIT_FLAG, SetNamedEntryExitFlag);
}

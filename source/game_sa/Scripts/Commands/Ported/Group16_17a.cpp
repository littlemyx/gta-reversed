#include <StdInc.h>

#include "../Commands.hpp"
#include <CommandParser/Parser.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>

#include "World.h"
#include "Pickups.h"
#include "ScriptResourceManager.h"
#include "MissionCleanup.h"
#include "DamageManager.h"
#include "Automobile.h"
#include "Fx/Fx.h"
#include "Fx/FxManager.h"
#include "Fx/FxSystem.h"
#include "Ragdoll/IKChainManager.h"
#include "DecisionMakers/DecisionMakerTypesFileLoader.h"
#include "Models/VehicleModelInfo.h"
#include "Tasks/PedScriptedTaskRecord.h"
#include "Tasks/TaskSequences.h"
#include "Tasks/TaskComplexUseSequence.h"
#include "TaskTypes/TaskSimpleClearLookAt.h"
#include "TaskTypes/TaskSimpleTriggerLookAt.h"
#include "TaskTypes/TaskSimpleGunControl.h"
#include "TaskTypes/TaskSimpleThrowControl.h"
#include "TaskTypes/TaskComplexDestroyCar.h"
#include "TaskTypes/TaskComplexEvasiveDiveAndGetUp.h"
#include "TaskTypes/TaskComplexPartnerChat.h"
#include "TaskTypes/TaskComplexShuffleSeats.h"
#include "TaskTypes/TaskComplexLeaveAnyCar.h"
#include "TaskTypes/TaskSimpleTogglePedThreatScanner.h"

namespace notsa::script::commands::ported::g16 { void RegisterHandlers(); }

using namespace notsa::script;

/*!
* Script commands ported from the exe's per-100 group processor CRunningScript::ProcessCommands1600To1699
* (@0x493FE0, switch base 1602, jump table 0x495E00) for the vanilla commands that had no handler of their own:
* ids 1600..1699 (S6-D part 1, group g16).
*
* Every handler below was written from the asm of the `case` in the group processor, NOT from the Ghidra decompilation.
* The case address is given in the comment above each handler. `ebp`/`[esp+0xF4]` hold the command id in the processor.
*
* Idioms: see Group05_08.cpp (`fcom -100.0f` ground Z lookup, `_ftol` truncation, ...). 0x859014 = -100.0f.
*  - TASK_* commands: `GivePedScriptedTask(handle, task, cmd)`, handle -1 = the task is added to the open task sequence.
*/

namespace {
static_assert(offsetof(CPed, m_nActiveWeaponSlot) == 0x718);
static_assert(offsetof(CPed, m_aWeapons) == 0x5A0 && sizeof(CWeapon) == 0x1C);
static_assert(offsetof(CPedIntelligence, m_FollowNodeThresholdDistance) == 0xCC);
static_assert(offsetof(CPedIntelligence, m_TaskMgr) + sizeof(void*) * TASK_PRIMARY_PRIMARY == 0x10);
static_assert(offsetof(CTaskComplexUseSequence, m_nCurrentTaskIndex) == 0x10);
static_assert(offsetof(CPhysical, m_pAttachedTo) == 0xFC);
static_assert(offsetof(CPhysical, m_vecMoveSpeed) == 0x44);
static_assert(offsetof(CPhysical, m_fMass) == 0x8C);
static_assert(offsetof(CPhysical, physicalFlags) == 0x40);
static_assert(offsetof(CVehicle, m_pDriver) == 0x460);
static_assert(offsetof(CVehicle, m_apPassengers) == 0x464);
static_assert(offsetof(CVehicle, m_nNumPassengers) == 0x484 && offsetof(CVehicle, m_nMaxPassengers) == 0x488);
static_assert(offsetof(CVehicle, m_nExtendedRemovalRange) == 0x4A6); // the next (bitfield) byte at 0x4A8 holds m_nBombOnBoard:3, m_nOverrideLights:2 (mask 0x18), m_ropeType:2
static_assert(offsetof(CAutomobile, m_aCarNodes) == 0x648);
static_assert(sizeof(CPickup) == 0x20);
static_assert((int32)eScriptedTaskStatus::EVENT_ASSOCIATED == 0);

//! x87-compare idiom `fcom -100.0f; test ah, 0x41; jp skip`: the ground Z is looked up only when `z <= -100.0f` (ordered).
float GroundZIfAuto(float x, float y, float z) {
    if (z <= -100.0f) { // 0x859014
        return CWorld::FindGroundZForCoord(x, y);
    }
    return z;
}

//! 0x59C910 - `CVector::Normalise` as the original evaluates it (sum of squares and reciprocal root stay in extended precision)
void NormaliseOriginal(CVector& v) {
    const double sumSq = ((double)v.x * v.x + (double)v.y * v.y) + (double)v.z * v.z;
    if (sumSq <= 0.0) {
        v.x = 1.0f; // (NaN takes the sqrt path); y, z untouched
    } else {
        const double recip = 1.0 / std::sqrt(sumSq);
        v.x = (float)(v.x * recip);
        v.y = (float)(v.y * recip);
        v.z = (float)(v.z * recip);
    }
}

//! Inlined `tUserList::AddPedTypeToList` (0x492CB0): the first free (-1) slot becomes "by ped type" (-2) with the ped type stored
void AddPedTypeToUserList(tUserList& list, int32 pedType) {
    list.m_bUseList = true;
    bool done = false;
    for (int32 i = 0; i < 4; i++) {
        if (done) {
            break;
        }
        if (list.m_UserTypes[i] == -1) {
            list.m_UserTypes[i]          = -2;
            list.m_UserTypesByPedType[i] = pedType;
            done                         = true;
        }
    }
}

//! Shared tail of the FX creation commands: register the effect, mission cleanup, return the id (or -1)
int32 RegisterScriptFx(CRunningScript& S, FxSystem_c* fx) {
    if (!fx) {
        return -1;
    }
    const int32 id = (int32)CTheScripts::AddScriptEffectSystem(fx);
    if (S.m_UsesMissionCleanup) {
        CTheScripts::MissionCleanUp.AddEntityToList(id, MISSION_CLEANUP_ENTITY_TYPE_PARTICLE);
    }
    return id;
}

//! `GetActualScriptThingIndex(id, SCRIPT_THING_EFFECT_SYSTEM)` then `ScriptEffectSystemArray[idx].m_pFxSystem` ([idx*8 + 0xA44114]); null if idx < 0
FxSystem_c* ScriptFxOf(int32 id) {
    const int32 idx = CTheScripts::GetActualScriptThingIndex(id, SCRIPT_THING_EFFECT_SYSTEM);
    if (idx < 0) {
        return nullptr;
    }
    return CTheScripts::ScriptEffectSystemArray[idx].m_pFxSystem;
}

// ============================================================================ g16 (ProcessCommands1600To1699 @0x493FE0)

//! 1606 GET_SEQUENCE_PROGRESS (case @0x4940F3): char => 1 int
//! -1 when the status of the char's scripted task record with opcode 0x618 (PERFORM_SEQUENCE_TASK) is 0 (EVENT_ASSOCIATED); any other
//! value (also NONE = -1 when there is no record!) continues: the current task index of the
//! primary task (TASK_PRIMARY_PRIMARY, +0x10 of the intelligence) read as a CTaskComplexUseSequence WITHOUT a type check.
int32 GetSequenceProgress(int32 pedHandle) {
    CPed* const ped = GetPedPool()->GetAtRef(pedHandle);
    if (CPedScriptedTaskRecord::GetStatus(ped, 0x618) == eScriptedTaskStatus::EVENT_ASSOCIATED) { // `test eax, eax; jne`: ONLY status 0 => -1 (NONE is -1, nonzero!)
        return -1;
    }
    const auto* const task = static_cast<const CTaskComplexUseSequence*>(ped->GetIntelligence()->GetTaskManager().GetPrimaryTasks()[TASK_PRIMARY_PRIMARY]);
    return task->m_nCurrentTaskIndex;
}

//! 1607 CLEAR_LOOK_AT (case @0x49415B): char (handle, -1 = add a CTaskSimpleClearLookAt to the open sequence)
//! Otherwise: if the ped is looking, the look-at is aborted with a 500 ms blend-out.
void ClearLookAt(int32 pedHandle) {
    if (pedHandle == -1) {
        CTaskSequences::AddTaskToActiveSequence(new CTaskSimpleClearLookAt{});
        return;
    }
    CPed* const ped = GetPedPool()->GetAtRef(pedHandle);
    if (g_ikChainMan.IsLooking(ped)) { // 0x6181A0
        g_ikChainMan.AbortLookAt(ped, 500); // 0x618280
    }
}

//! 1608 SET_FOLLOW_NODE_THRESHOLD_DISTANCE (case @0x4941F5): char, distance (float stored raw to intelligence+0xCC)
void SetFollowNodeThresholdDistance(CPed& ped, float distance) {
    ped.GetIntelligence()->m_FollowNodeThresholdDistance = distance;
}

//! 1611 CREATE_FX_SYSTEM (case @0x494228): name(32), x, y, z, flag => 1 int (fx id or -1)
//! The label is read BEFORE the 4 params. The destination variable is peeked at (`GetActualScriptThingIndex` on it; result unused).
//! z <= -100 => ground Z. The system is created at the position (no matrix); `flag != 0` => ignoreBoundingChecks.
int32 CreateFxSystem(CRunningScript& S, const char* name, float x, float y, float z, int32 flag) {
    CTheScripts::GetActualScriptThingIndex(S.CollectNextParameterWithoutIncreasingPC(), SCRIPT_THING_EFFECT_SYSTEM); // dead call kept
    const CVector pos{ x, y, GroundZIfAuto(x, y, z) };
    return RegisterScriptFx(S, g_fxMan.CreateFxSystem(name, pos, nullptr, flag != 0)); // 0x4A9BE0
}

//! 1612 PLAY_FX_SYSTEM (case @0x494323): fx
void PlayFxSystem(int32 fxId) {
    if (auto* const fx = ScriptFxOf(fxId)) {
        fx->Play();
    }
}

//! 1614 STOP_FX_SYSTEM (case @0x494361): fx
void StopFxSystem(int32 fxId) {
    if (auto* const fx = ScriptFxOf(fxId)) {
        fx->Stop();
    }
}

//! Shared tail of 1615/1616 (0x4943D7): the script effect slot is released (given the RAW script id), and the cleanup entry removed
void ReleaseScriptFx(CRunningScript& S, int32 fxId) {
    CTheScripts::RemoveScriptEffectSystem(fxId);
    if (S.m_UsesMissionCleanup) {
        CTheScripts::MissionCleanUp.RemoveEntityFromList(fxId, MISSION_CLEANUP_ENTITY_TYPE_PARTICLE);
    }
}

//! 1615 PLAY_AND_KILL_FX_SYSTEM (case @0x49439F): fx
void PlayAndKillFxSystem(CRunningScript& S, int32 fxId) {
    auto* const fx = ScriptFxOf(fxId);
    if (!fx) {
        return;
    }
    fx->PlayAndKill();
    ReleaseScriptFx(S, fxId);
}

//! 1616 KILL_FX_SYSTEM (case @0x494402): fx
void KillFxSystem(CRunningScript& S, int32 fxId) {
    auto* const fx = ScriptFxOf(fxId);
    if (!fx) {
        return;
    }
    fx->Kill();
    ReleaseScriptFx(S, fxId);
}

//! 1620 SET_OBJECT_RENDER_SCORCHED (case @0x4944A1): object, on. physicalFlags (+0x40) bit 0x20000000 (bRenderScorched); no null check
void SetObjectRenderScorched(CObject& obj, int32 on) {
    obj.physicalFlags.bRenderScorched = (on != 0);
}

//! 1621 TASK_LOOK_AT_OBJECT (case @0x4944E9): char, object, time
//! time -1 => 20000, -2 => 0x7FFFFFFF. char -1 => CTaskSimpleTriggerLookAt added to the open sequence; otherwise
//! `g_ikChainMan.LookAt("COMMAND_TASK_LOOK_AT_OBJECT" (0x85A3D4), ped, obj, time, -1, nullptr, false, 0.25f, 500, 6, true)`.
void TaskLookAtObject(int32 pedHandle, CObject* obj, int32 time) {
    if (time == -1) {
        time = 20000;
    } else if (time == -2) {
        time = 0x7FFFFFFF;
    }
    if (pedHandle != -1) {
        g_ikChainMan.LookAt("COMMAND_TASK_LOOK_AT_OBJECT", GetPedPool()->GetAtRef(pedHandle), obj, time, BONE_UNKNOWN, nullptr, false, 0.25f, 500, 6, true);
        return;
    }
    CTaskSequences::AddTaskToActiveSequence(new CTaskSimpleTriggerLookAt{ obj, time, -1, RwV3d{ 0.0f, 0.0f, 0.0f }, true, 0.25f, 1000, 3 });
}

//! 1622 LIMIT_ANGLE (case @0x4945D5): float => float. Wraps into [0, 360] by repeated +-360 (x87 stack values => double)
float LimitAngle(float angle) {
    double a = angle;
    if (a < 0.0) {
        do {
            a += 360.0; // 0x859E2C
        } while (a < 0.0);
    }
    if (a > 360.0) {
        do {
            a -= 360.0;
        } while (a > 360.0);
    }
    return (float)a;
}

//! 1623 OPEN_CAR_DOOR (case @0x49467B): car, door
//! Nothing happens when the door is missing (virtual slot 38 = IsDoorMissing) or the car node frame is null
//! (`CAutomobile::m_aCarNodes[node]`, +0x648, read without a type check); else `OpenDoor(nullptr, node, door, 1.0f, true)` (slot 27).
void OpenCarDoor(CVehicle& veh, eDoors door) {
    const auto node = CDamageManager::GetCarNodeIndexFromDoor(door);
    if (veh.IsDoorMissing(door)) {
        return;
    }
    if (!static_cast<CAutomobile&>(veh).m_aCarNodes[node]) {
        return;
    }
    veh.OpenDoor(nullptr, node, door, 1.0f, true);
}

//! 1627 GET_PICKUP_COORDINATES (case @0x4946E4): pickup => 3 floats (default (0, 0, -100) when the pickup index is -1)
CVector GetPickupCoordinates(int32 pickup) {
    const int32 idx = CPickups::GetActualPickupIndex(tPickupReference{ pickup });
    if (idx == -1) {
        return { 0.0f, 0.0f, -100.0f };
    }
    return CPickups::aPickUps[idx].GetPosn();
}

//! 1628 REMOVE_DECISION_MAKER (case @0x494767): decision maker (raw script id)
//! idx = GetActualScriptThingIndex(id, DECISION_MAKER); when 0 <= idx < 20 and the script resource (type 3) is removed
//! from the resource manager => the decision maker is unloaded. The mission cleanup entry (type 9) is removed afterwards.
void RemoveDecisionMaker(CRunningScript& S, int32 id) {
    const int32 idx = CTheScripts::GetActualScriptThingIndex(id, SCRIPT_THING_DECISION_MAKER);
    if (idx >= 0 && idx < 20) {
        if (CTheScripts::ScriptResourceManager.RemoveFromResourceManager(id, RESOURCE_TYPE_DECISION_MAKER, &S)) {
            CDecisionMakerTypesFileLoader::UnloadDecisionMaker((eDecisionTypes)idx);
        }
    }
    if (S.m_UsesMissionCleanup) {
        CTheScripts::MissionCleanUp.RemoveEntityFromList(id, MISSION_CLEANUP_ENTITY_TYPE_DECISION_MAKER);
    }
}

//! 1639 TASK_AIM_GUN_AT_COORD (case @0x494868): char, x, y, z, time
//! CTaskSimpleGunControl(nullptr, &pos, nullptr, 0, 500, time)
void TaskAimGunAtCoord(CRunningScript& S, eScriptCommands cmd, int32 pedHandle, CVector pos, int32 time) {
    S.GivePedScriptedTask(pedHandle, new CTaskSimpleGunControl{ nullptr, pos, CVector{}, (eGunCommand)0, 500, time }, (int32)cmd);
}

//! 1640 TASK_SHOOT_AT_COORD (case @0x4948EF): char, x, y, z, time
//! When the char exists and its active weapon (`CWeaponInfo::GetWeaponInfo(type, 1)`) has the bThrow flag (bit 8 of +0x18) =>
//! CTaskSimpleThrowControl(nullptr, &pos); otherwise CTaskSimpleGunControl(nullptr, &pos, nullptr, 3, 5, time).
void TaskShootAtCoord(CRunningScript& S, eScriptCommands cmd, int32 pedHandle, CVector pos, int32 time) {
    if (pedHandle != -1) {
        CPed* const ped = GetPedPool()->GetAtRef(pedHandle);
        const auto  slot = (int32)(int8)ped->m_nActiveWeaponSlot;
        if (((CWeaponInfo::GetWeaponInfo(ped->m_aWeapons[slot].m_Type, (eWeaponSkill)1)->m_nFlags >> 8) & 1) != 0) {
            S.GivePedScriptedTask(pedHandle, new CTaskSimpleThrowControl{ nullptr, &pos }, (int32)cmd);
            return;
        }
    }
    S.GivePedScriptedTask(pedHandle, new CTaskSimpleGunControl{ nullptr, pos, CVector{}, (eGunCommand)3, 5, time }, (int32)cmd);
}

//! 1641 CREATE_FX_SYSTEM_ON_CHAR (case @0x4949F9): name(32), char, x, y, z, flag => 1 int
//! Peeks the destination variable (dead GetActualScriptThingIndex). Offset z <= -100 => ground Z (looked up at the OFFSET x/y).
//! The ped's RW object is created if missing; the system is attached to its modelling matrix. Failure (no matrix / no fx) => -1.
int32 CreateFxSystemOnEntity(CRunningScript& S, const char* name, CEntity& entity, CVector offsetIn, bool flag) {
    const CVector offset{ offsetIn.x, offsetIn.y, GroundZIfAuto(offsetIn.x, offsetIn.y, offsetIn.z) };
    if (!entity.GetRwObject()) {
        entity.CreateRwObject();
    }
    RwMatrix* const mat = entity.GetModellingMatrix();
    if (!mat) {
        return -1;
    }
    return RegisterScriptFx(S, g_fxMan.CreateFxSystem(name, offset, mat, flag)); // 0x4A9BE0
}

int32 CreateFxSystemOnChar(CRunningScript& S, const char* name, CPed& ped, float x, float y, float z, int32 flag) {
    CTheScripts::GetActualScriptThingIndex(S.CollectNextParameterWithoutIncreasingPC(), SCRIPT_THING_EFFECT_SYSTEM);
    return CreateFxSystemOnEntity(S, name, ped, { x, y, z }, flag != 0);
}

//! 1642 CREATE_FX_SYSTEM_ON_CHAR_WITH_DIRECTION (case @0x494A6E): name(32), char, x, y, z, dx, dy, dz, flag => 1 int
//! The transform is built with `g_fx.CreateMatFromVec(&mat, &offset, &direction)` (0x49E950), 0x4A9BB0 overload.
int32 CreateFxSystemOnEntityWithDirection(CRunningScript& S, const char* name, CEntity& entity, CVector offsetIn, CVector direction, bool flag) {
    const CVector offset{ offsetIn.x, offsetIn.y, GroundZIfAuto(offsetIn.x, offsetIn.y, offsetIn.z) };
    RwMatrix      transform{};
    g_fx.CreateMatFromVec(&transform, &offset, &direction);
    if (!entity.GetRwObject()) {
        entity.CreateRwObject();
    }
    RwMatrix* const mat = entity.GetModellingMatrix();
    if (!mat) {
        return -1;
    }
    return RegisterScriptFx(S, g_fxMan.CreateFxSystem(name, transform, mat, flag)); // 0x4A9BB0
}

int32 CreateFxSystemOnCharWithDirection(CRunningScript& S, const char* name, CPed& ped, float x, float y, float z, float dx, float dy, float dz, int32 flag) {
    CTheScripts::GetActualScriptThingIndex(S.CollectNextParameterWithoutIncreasingPC(), SCRIPT_THING_EFFECT_SYSTEM);
    return CreateFxSystemOnEntityWithDirection(S, name, ped, { x, y, z }, { dx, dy, dz }, flag != 0);
}

//! 1643 CREATE_FX_SYSTEM_ON_CAR (case @0x494B70): as 1641 with a vehicle
int32 CreateFxSystemOnCar(CRunningScript& S, const char* name, CVehicle& veh, float x, float y, float z, int32 flag) {
    CTheScripts::GetActualScriptThingIndex(S.CollectNextParameterWithoutIncreasingPC(), SCRIPT_THING_EFFECT_SYSTEM);
    return CreateFxSystemOnEntity(S, name, veh, { x, y, z }, flag != 0);
}

//! 1644 CREATE_FX_SYSTEM_ON_CAR_WITH_DIRECTION (case @0x494BAC): as 1642 with a vehicle
int32 CreateFxSystemOnCarWithDirection(CRunningScript& S, const char* name, CVehicle& veh, float x, float y, float z, float dx, float dy, float dz, int32 flag) {
    CTheScripts::GetActualScriptThingIndex(S.CollectNextParameterWithoutIncreasingPC(), SCRIPT_THING_EFFECT_SYSTEM);
    return CreateFxSystemOnEntityWithDirection(S, name, veh, { x, y, z }, { dx, dy, dz }, flag != 0);
}

//! 1645 CREATE_FX_SYSTEM_ON_OBJECT (case @0x494BEC): as 1641 with an object
int32 CreateFxSystemOnObject(CRunningScript& S, const char* name, CObject& obj, float x, float y, float z, int32 flag) {
    CTheScripts::GetActualScriptThingIndex(S.CollectNextParameterWithoutIncreasingPC(), SCRIPT_THING_EFFECT_SYSTEM);
    return CreateFxSystemOnEntity(S, name, obj, { x, y, z }, flag != 0);
}

//! 1646 CREATE_FX_SYSTEM_ON_OBJECT_WITH_DIRECTION (case @0x494CB8): as 1642 with an object
int32 CreateFxSystemOnObjectWithDirection(CRunningScript& S, const char* name, CObject& obj, float x, float y, float z, float dx, float dy, float dz, int32 flag) {
    CTheScripts::GetActualScriptThingIndex(S.CollectNextParameterWithoutIncreasingPC(), SCRIPT_THING_EFFECT_SYSTEM);
    return CreateFxSystemOnEntityWithDirection(S, name, obj, { x, y, z }, { dx, dy, dz }, flag != 0);
}

//! 1650 TASK_DESTROY_CAR (case @0x494DC6): char, car (null car passed through) => CTaskComplexDestroyCar(car, 0, 0, 0)
void TaskDestroyCar(CRunningScript& S, eScriptCommands cmd, int32 pedHandle, CVehicle* veh) {
    S.GivePedScriptedTask(pedHandle, new CTaskComplexDestroyCar{ veh, 0, 0, 0 }, (int32)cmd);
}

//! 1651 TASK_DIVE_AND_GET_UP (case @0x494E34): char, x, y, time
//! direction = normalise((x, y, 0)) with the original 0x59C910; CTaskComplexEvasiveDiveAndGetUp(nullptr, time, &dir, true)
void TaskDiveAndGetUp(CRunningScript& S, eScriptCommands cmd, int32 pedHandle, float x, float y, int32 time) {
    CVector dir{ x, y, 0.0f };
    NormaliseOriginal(dir);
    S.GivePedScriptedTask(pedHandle, new CTaskComplexEvasiveDiveAndGetUp{ nullptr, time, dir, true }, (int32)cmd);
}

//! 1652 CUSTOM_PLATE_FOR_NEXT_CAR (case @0x494EA4): model, plate(9)
//! The label is read AFTER the model. Every '_' or NUL of the first 8 chars becomes ' '. Then, if the model info exists, is a
//! vehicle model (GetModelType() == MODEL_INFO_VEHICLE = 6) and has an RW object (+0x24), the plate text is set.
void CustomPlateForNextCar(CRunningScript& S, int32 modelId) {
    char plate[9];
    S.ReadTextLabelFromScript(plate, 9);
    for (int32 i = 0; i < 8; i++) {
        if (plate[i] == '_' || plate[i] == '\0') {
            plate[i] = ' ';
        }
    }
    plate[8] = '\0';
    auto* const mi = CModelInfo::ms_modelInfoPtrs[modelId]; // 0xA9B0C8, no range check
    if (mi && mi->GetModelType() == MODEL_INFO_VEHICLE && mi->GetRwObject()) {
        static_cast<CVehicleModelInfo*>(mi)->SetCustomCarPlateText(plate);
    }
}

//! 1654 TASK_SHUFFLE_TO_NEXT_CAR_SEAT (case @0x494F1D): char, car => CTaskComplexShuffleSeats(car)
void TaskShuffleToNextCarSeat(CRunningScript& S, eScriptCommands cmd, int32 pedHandle, CVehicle* veh) {
    S.GivePedScriptedTask(pedHandle, new CTaskComplexShuffleSeats{ veh }, (int32)cmd);
}

//! 1655 TASK_CHAT_WITH_CHAR (case @0x494F85): char, partner, leadSpeaker, b4
//! CTaskComplexPartnerChat("COMMAND_TASK_CHAT_WITH_CHAR" (0x85A3B8), partner, leadSpeaker != 0, 0.5f, b4 != 0 ? -1 : 4, true, true, (0,0,0))
void TaskChatWithChar(CRunningScript& S, eScriptCommands cmd, int32 pedHandle, CPed* partner, int32 leadSpeaker, int32 b4) {
    S.GivePedScriptedTask(
        pedHandle,
        new CTaskComplexPartnerChat{ "COMMAND_TASK_CHAT_WITH_CHAR", partner, leadSpeaker != 0, 0.5f, (int8)(b4 != 0 ? -1 : 4), true, true, CVector{} },
        (int32)cmd
    );
}

//! 1663 FORCE_CAR_LIGHTS (case @0x49539D): car, mode. m_nOverrideLights (2 bits, mask 0x18 of the byte at +0x4A8) = mode << 3
void ForceCarLights(CVehicle& veh, int32 mode) {
    veh.m_nOverrideLights = (uint8)mode; // (b ^ (((mode << 3) ^ b) & 0x18))
}

//! 1664 ADD_PEDTYPE_AS_ATTRACTOR_USER (case @0x4953DD): 2d effect (script thing 6), pedType
//! idx in [0, 64): the user list of the effect (`ms_userLists[idx]`, 0xC3A200, stride 0x24) gets the ped type (inlined 0x492CB0).
void AddPedTypeAsAttractorUser(int32 effectId, int32 pedType) {
    const int32 idx = CTheScripts::GetActualScriptThingIndex(effectId, SCRIPT_THING_2D_EFFECT);
    if (idx < 0 || idx >= 64) {
        return;
    }
    AddPedTypeToUserList(CScripted2dEffects::ms_userLists[idx], pedType);
}

//! The 3 + 3 float pack of the attach commands: offset (p2..p4) as is, rotation (p5..p7) * deg2rad (0x8595EC = 0.017453292f) rounded to float
CVector RotationToRadians(float rx, float ry, float rz) {
    const double k = 0.01745329238474369; // (double)0.017453292f
    return { (float)(rx * k), (float)(ry * k), (float)(rz * k) };
}

//! 1665 ATTACH_OBJECT_TO_CAR (case @0x495425): object, car, ox, oy, oz, rx, ry, rz (degrees)
void AttachObjectToCar(CObject* obj, CVehicle* veh, float ox, float oy, float oz, float rx, float ry, float rz) {
    obj->AttachEntityToEntity(veh, CVector{ ox, oy, oz }, RotationToRadians(rx, ry, rz)); // 0x54D570 (null objects crash like in the exe)
}

//! Shared by 1666/1668: `DettachEntityFromEntity(x * k, (y * -1.0f) * k, z, flag != 0)`; skipped if null or not attached (+0xFC)
void DetachPhysical(CPhysical* phys, float x, float y, float z, int32 flag) {
    if (!phys || !phys->m_pAttachedTo) {
        return;
    }
    const float  y1 = (float)((double)y * (double)-1.0f); // 0x858C1C
    const double k  = 0.01745329238474369;
    phys->DettachEntityFromEntity((float)(x * k), (float)(y1 * k), z, flag != 0);
}

//! 1666 DETACH_OBJECT (case @0x4954E2): object, x, y, z, flag
void DetachObject(CObject* obj, float x, float y, float z, int32 flag) {
    DetachPhysical(obj, x, y, z, flag);
}

//! 1667 ATTACH_CAR_TO_CAR (case @0x49557B): car, target car, ox, oy, oz, rx, ry, rz
//! When `ox` (p2) is NOT greater than -999.9f (0x85A3B4; `test ah, 0x41; jp` => taken when > or unordered) the car is attached without
//! offset (quat overload with null, null); otherwise with the offset and rotation (vec overload).
void AttachCarToCar(CVehicle* veh, CVehicle* target, float ox, float oy, float oz, float rx, float ry, float rz) {
    const CVector rot = RotationToRadians(rx, ry, rz);
    if (!std::isunordered(ox, -999.9f) && !(ox > -999.9f)) {
        veh->AttachEntityToEntity(target, (CVector*)nullptr, (CQuaternion*)nullptr);
    } else {
        veh->AttachEntityToEntity(target, CVector{ ox, oy, oz }, rot);
    }
}

//! 1668 DETACH_CAR (case @0x495657): car, x, y, z, flag
void DetachCar(CVehicle* veh, float x, float y, float z, int32 flag) {
    DetachPhysical(veh, x, y, z, flag);
}

//! 1669 IS_OBJECT_ATTACHED (case @0x4956D3): object => compare flag (non-null and +0xFC != 0)
bool IsObjectAttached(CObject* obj) {
    return obj && obj->m_pAttachedTo;
}

//! 1670 IS_VEHICLE_ATTACHED (case @0x495719): car => compare flag
bool IsVehicleAttached(CVehicle* veh) {
    return veh && veh->m_pAttachedTo;
}

//! 1671 CLEAR_CHAR_TASKS (case @0x49575F): char => `ped->GetIntelligence()->ClearTasks(true, true)`
void ClearCharTasks(CPed& ped) {
    ped.GetIntelligence()->ClearTasks(true, true);
}

//! 1672 TASK_TOGGLE_PED_THREAT_SCANNER (case @0x49578F): char, a, b, c
//! char -1 => a heap CTaskSimpleTogglePedThreatScanner(a != 0, b != 0, c != 0) is added to the open sequence; otherwise a
//! stack task is run directly on the ped (`ProcessPed`, not via GivePedScriptedTask).
void TaskTogglePedThreatScanner(int32 pedHandle, int32 a, int32 b, int32 c) {
    if (pedHandle != -1) {
        CPed* const ped = GetPedPool()->GetAtRef(pedHandle);
        CTaskSimpleTogglePedThreatScanner task{ a != 0, b != 0, c != 0 };
        task.CTaskSimpleTogglePedThreatScanner::ProcessPed(ped); // 0x6337A0 (static call)
        return;
    }
    CTaskSequences::AddTaskToActiveSequence(new CTaskSimpleTogglePedThreatScanner{ a != 0, b != 0, c != 0 });
}

//! 1673 POP_CAR_DOOR (case @0x495882): car, door, flag => PopDoor(GetCarNodeIndexFromDoor(door), door, flag != 0)
void PopCarDoor(CVehicle& veh, eDoors door, int32 flag) {
    const auto node = CDamageManager::GetCarNodeIndexFromDoor(door);
    static_cast<CAutomobile&>(veh).PopDoor(node, door, flag != 0);
}

//! 1674 FIX_CAR_DOOR (case @0x4958D2): car, door => FixDoor(GetCarNodeIndexFromDoor(door), door)
void FixCarDoor(CVehicle& veh, eDoors door) {
    const auto node = CDamageManager::GetCarNodeIndexFromDoor(door);
    static_cast<CAutomobile&>(veh).FixDoor(node, door);
}

//! 1675 TASK_EVERYONE_LEAVE_CAR (case @0x49590E): car
//! The driver (if any) gets CTaskComplexLeaveAnyCar(0, true, false); then every passenger slot [0, m_nNumPassengers) with a ped
//! gets CTaskComplexLeaveAnyCar(rand(-250, 250) + 500 * (i + 1), true, false) (the random number is drawn before the task is built).
//! NOTE: the loop bound is the byte at +0x488 = m_nMaxPassengers (NOT m_nNumPassengers at +0x484).
void TaskEveryoneLeaveCar(CRunningScript& S, eScriptCommands cmd, CVehicle& veh) {
    if (veh.m_pDriver) {
        auto* const task = new CTaskComplexLeaveAnyCar{ 0, true, false };
        S.GivePedScriptedTask(GetPedPool()->GetRef(veh.m_pDriver), task, (int32)cmd);
    }
    for (int32 i = 0; i < (int32)veh.m_nMaxPassengers; i++) {
        CPed* const passenger = veh.m_apPassengers[i];
        if (!passenger) {
            continue;
        }
        const int32 delay = CGeneral::GetRandomNumberInRange(-250, 250) + i * 500 + 500;
        auto* const task  = new CTaskComplexLeaveAnyCar{ delay, true, false };
        S.GivePedScriptedTask(GetPedPool()->GetRef(passenger), task, (int32)cmd);
    }
}

//! 1687 POP_CAR_PANEL (case @0x495AF4): car, panel, flag => PopPanel(GetCarNodeIndexFromPanel(panel), panel, flag != 0)
void PopCarPanel(CVehicle& veh, ePanels panel, int32 flag) {
    const auto node = CDamageManager::GetCarNodeIndexFromPanel(panel);
    static_cast<CAutomobile&>(veh).PopPanel(node, panel, flag != 0);
}

//! 1688 FIX_CAR_PANEL (case @0x495B44): car, panel => FixPanel(GetCarNodeIndexFromPanel(panel), panel)
void FixCarPanel(CVehicle& veh, ePanels panel) {
    const auto node = CDamageManager::GetCarNodeIndexFromPanel(panel);
    static_cast<CAutomobile&>(veh).FixPanel(node, panel);
}

//! 1689 FIX_CAR_TYRE (case @0x495B80): car, tyre => FixTyre(tyre)
void FixCarTyre(CVehicle& veh, eWheels wheel) {
    static_cast<CAutomobile&>(veh).FixTyre(wheel);
}

//! 1690 ATTACH_OBJECT_TO_OBJECT (case @0x495BAF): object, target object, ox, oy, oz, rx, ry, rz
void AttachObjectToObject(CObject* obj, CObject* target, float ox, float oy, float oz, float rx, float ry, float rz) {
    obj->AttachEntityToEntity(target, CVector{ ox, oy, oz }, RotationToRadians(rx, ry, rz));
}

//! 1691 ATTACH_OBJECT_TO_CHAR (case @0x495C6A): object, char, ox, oy, oz, rx, ry, rz
void AttachObjectToChar(CObject* obj, CPed* ped, float ox, float oy, float oz, float rx, float ry, float rz) {
    obj->AttachEntityToEntity(ped, CVector{ ox, oy, oz }, RotationToRadians(rx, ry, rz));
}

//! 1698 GET_CAR_SPEED_VECTOR (case @0x495D27): car => 3 floats: move speed (+0x44) * 50.0f (0x858B40), each rounded to float
CVector GetCarSpeedVector(CVehicle& veh) {
    const CVector& v = veh.m_vecMoveSpeed;
    return { v.x * 50.0f, v.y * 50.0f, v.z * 50.0f };
}

//! 1699 GET_CAR_MASS (case @0x495DAD): car => 1 float (+0x8C)
float GetCarMass(CVehicle& veh) {
    return veh.m_fMass;
}
} // namespace

void notsa::script::commands::ported::g16::RegisterHandlers() {
    REGISTER_COMMAND_HANDLER_BEGIN("Ported g16");

    REGISTER_COMMAND_HANDLER(COMMAND_GET_SEQUENCE_PROGRESS, GetSequenceProgress);
    REGISTER_COMMAND_HANDLER(COMMAND_CLEAR_LOOK_AT, ClearLookAt);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_FOLLOW_NODE_THRESHOLD_DISTANCE, SetFollowNodeThresholdDistance);
    REGISTER_COMMAND_HANDLER(COMMAND_CREATE_FX_SYSTEM, CreateFxSystem);
    REGISTER_COMMAND_HANDLER(COMMAND_PLAY_FX_SYSTEM, PlayFxSystem);
    REGISTER_COMMAND_HANDLER(COMMAND_STOP_FX_SYSTEM, StopFxSystem);
    REGISTER_COMMAND_HANDLER(COMMAND_PLAY_AND_KILL_FX_SYSTEM, PlayAndKillFxSystem);
    REGISTER_COMMAND_HANDLER(COMMAND_KILL_FX_SYSTEM, KillFxSystem);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_OBJECT_RENDER_SCORCHED, SetObjectRenderScorched);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_LOOK_AT_OBJECT, TaskLookAtObject);
    REGISTER_COMMAND_HANDLER(COMMAND_LIMIT_ANGLE, LimitAngle);
    REGISTER_COMMAND_HANDLER(COMMAND_OPEN_CAR_DOOR, OpenCarDoor);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_PICKUP_COORDINATES, GetPickupCoordinates);
    REGISTER_COMMAND_HANDLER(COMMAND_REMOVE_DECISION_MAKER, RemoveDecisionMaker);
    REGISTER_COMMAND_NOP(COMMAND_BREAKPOINT, const char*); // 1633 (case @0x4947EC): reads a text label (ReadTextLabelFromScript, 40 bytes), does nothing
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_AIM_GUN_AT_COORD, TaskAimGunAtCoord);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_SHOOT_AT_COORD, TaskShootAtCoord);
    REGISTER_COMMAND_HANDLER(COMMAND_CREATE_FX_SYSTEM_ON_CHAR, CreateFxSystemOnChar);
    REGISTER_COMMAND_HANDLER(COMMAND_CREATE_FX_SYSTEM_ON_CHAR_WITH_DIRECTION, CreateFxSystemOnCharWithDirection);
    REGISTER_COMMAND_HANDLER(COMMAND_CREATE_FX_SYSTEM_ON_CAR, CreateFxSystemOnCar);
    REGISTER_COMMAND_HANDLER(COMMAND_CREATE_FX_SYSTEM_ON_CAR_WITH_DIRECTION, CreateFxSystemOnCarWithDirection);
    REGISTER_COMMAND_HANDLER(COMMAND_CREATE_FX_SYSTEM_ON_OBJECT, CreateFxSystemOnObject);
    REGISTER_COMMAND_HANDLER(COMMAND_CREATE_FX_SYSTEM_ON_OBJECT_WITH_DIRECTION, CreateFxSystemOnObjectWithDirection);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_DESTROY_CAR, TaskDestroyCar);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_DIVE_AND_GET_UP, TaskDiveAndGetUp);
    REGISTER_COMMAND_HANDLER(COMMAND_CUSTOM_PLATE_FOR_NEXT_CAR, CustomPlateForNextCar);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_SHUFFLE_TO_NEXT_CAR_SEAT, TaskShuffleToNextCarSeat);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_CHAT_WITH_CHAR, TaskChatWithChar);
    REGISTER_COMMAND_HANDLER(COMMAND_FORCE_CAR_LIGHTS, ForceCarLights);
    REGISTER_COMMAND_HANDLER(COMMAND_ADD_PEDTYPE_AS_ATTRACTOR_USER, AddPedTypeAsAttractorUser);
    REGISTER_COMMAND_HANDLER(COMMAND_ATTACH_OBJECT_TO_CAR, AttachObjectToCar);
    REGISTER_COMMAND_HANDLER(COMMAND_DETACH_OBJECT, DetachObject);
    REGISTER_COMMAND_HANDLER(COMMAND_ATTACH_CAR_TO_CAR, AttachCarToCar);
    REGISTER_COMMAND_HANDLER(COMMAND_DETACH_CAR, DetachCar);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_OBJECT_ATTACHED, IsObjectAttached);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_VEHICLE_ATTACHED, IsVehicleAttached);
    REGISTER_COMMAND_HANDLER(COMMAND_CLEAR_CHAR_TASKS, ClearCharTasks);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_TOGGLE_PED_THREAT_SCANNER, TaskTogglePedThreatScanner);
    REGISTER_COMMAND_HANDLER(COMMAND_POP_CAR_DOOR, PopCarDoor);
    REGISTER_COMMAND_HANDLER(COMMAND_FIX_CAR_DOOR, FixCarDoor);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_EVERYONE_LEAVE_CAR, TaskEveryoneLeaveCar);
    REGISTER_COMMAND_HANDLER(COMMAND_POP_CAR_PANEL, PopCarPanel);
    REGISTER_COMMAND_HANDLER(COMMAND_FIX_CAR_PANEL, FixCarPanel);
    REGISTER_COMMAND_HANDLER(COMMAND_FIX_CAR_TYRE, FixCarTyre);
    REGISTER_COMMAND_HANDLER(COMMAND_ATTACH_OBJECT_TO_OBJECT, AttachObjectToObject);
    REGISTER_COMMAND_HANDLER(COMMAND_ATTACH_OBJECT_TO_CHAR, AttachObjectToChar);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_CAR_SPEED_VECTOR, GetCarSpeedVector);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_CAR_MASS, GetCarMass);
}

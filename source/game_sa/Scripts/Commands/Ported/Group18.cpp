#include <StdInc.h>

#include "../Commands.hpp"
#include <CommandParser/Parser.hpp>

#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>

#include "World.h"
#include "Glass.h"
#include "BreakManager_c.h"
#include "Remote.h"
#include "Radar.h"
#include "Shopping.h"
#include "PedType.h"
#include "PedGroups.h"
#include "Streaming.h"
#include "Population.h"
#include "CarEnterExit.h"
#include "Events/Event.h"
#include "MissionCleanup.h"
#include "DamageManager.h"
#include "PatrolRoute.h"
#include "Automobile.h"
#include "Heli.h"
#include "Plane.h"
#include "Object.h"
#include "ObjectData.h"
#include "Animation/AnimManager.h"
#include "Plugins/RpAnimBlendPlugin/RpAnimBlend.h"
#include "DecisionMakers/DecisionMakerTypes.h"
#include "Tasks/PedScriptedTaskRecord.h"
#include "Tasks/TaskSequences.h"
#include "TaskTypes/TaskSimpleHoldEntity.h"
#include "TaskTypes/TaskSimpleGangDriveBy.h"
#include "TaskTypes/TaskComplexUseMobilePhone.h"
#include "TaskTypes/TaskSimpleCarSetPedInAsDriver.h"
#include "TaskTypes/TaskSimpleCarSetPedInAsPassenger.h"
#include "TaskTypes/TaskComplexUseEffect.h"
#include "TaskTypes/TaskSimpleGunControl.h"
#include "TaskTypes/TaskComplexFleeAnyMeans.h"
#include "TaskTypes/TaskComplexDie.h"
#include "Scripted2dEffects.h"
#include "game_sa/DetachedShared.h"
#line 48

namespace notsa::script::commands::ported::g18 { void RegisterHandlers(); }

using namespace notsa::script;

/*!
* Script commands ported from the exe's per-100 group processor CRunningScript::ProcessCommands1800To1899
* (@0x46D050, switch base 1800, jump table 0x46EDB4) for the vanilla commands that had no handler of their own:
* ids 1800..1899 (S6-E part 1, group g18).
*
* Every handler below was written from the asm of the `case` in the group processor, NOT from the Ghidra decompilation.
* The case address is given in the comment above each handler. `edi` holds the command id in the processor.
*
* Idioms: see Group05_08.cpp (`fcom -100.0f` ground Z lookup, `_ftol2`, ...).
*  - TASK_* commands: `GivePedScriptedTask(handle, task, cmd)`, handle -1 = the task is added to the open task sequence.
*  - 0x8595EC = 0.017453292f (degrees to radians, 0x3C8EFA35)
*/

namespace {
//! Offsets the raw-offset accesses of the asm resolve to (checked against the headers)
static_assert(offsetof(CVehicle, m_autoPilot) + offsetof(CAutoPilot, m_nCarMission) == 0x3BA);
static_assert(offsetof(CVehicle, m_autoPilot) + offsetof(CAutoPilot, m_nCruiseSpeed) == 0x3D0);
static_assert(offsetof(CVehicle, m_autoPilot) + offsetof(CAutoPilot, m_nCarCtrlFlags) == 0x3DB);
static_assert(offsetof(CVehicle, m_autoPilot) + offsetof(CAutoPilot, m_TargetEntity) == 0x41C);
static_assert(offsetof(CVehicle, m_nCreatedBy) == 0x4A4);
static_assert(offsetof(CAutomobile, m_damageManager) == 0x5A0);
static_assert(offsetof(CPlane, m_maxAltitude) == 0x9A8);   // 1807
static_assert(offsetof(CPlane, m_minAltitude) == 0x9B0);   // 1806, 1807, 1808
static_assert(offsetof(CPlane, m_planeHeading) == 0x9B4);  // 1807
static_assert(offsetof(CPlane, m_planeHeadingPrev) == 0x9B8); // 1807
static_assert(offsetof(CPlane, m_fAccelerationBreakStatusPrev) == 0x998); // 1858
static_assert(offsetof(CHeli, m_fMinAltitude) == 0x9B0);   // 1828, 1830, 1831
static_assert(offsetof(CAutomobile, m_wMiscComponentAngle) == 0x86C); // 1861
static_assert(offsetof(CPed, m_nPedState) == 0x530);
static_assert(offsetof(CPed, m_nPedType) == 0x598);
static_assert(offsetof(CPedIntelligence, m_nDmNumPedsToScan) == 0xC4);
static_assert(offsetof(CPedIntelligence, m_fDmRadius) == 0xC8);
static_assert(PEDSTATE_ARRESTED == 0x3F);
static_assert(offsetof(CObject, m_fHealth) == 0x154);
static_assert(offsetof(CObject, m_pObjectInfo) == 0x160);
static_assert(offsetof(CObject, m_nObjectFlags) == 0x140);
static_assert(offsetof(CObjectData, m_vecBreakVelocity) == 0x38 && offsetof(CObjectData, m_fBreakVelocityRand) == 0x44);
static_assert(offsetof(CTaskSimpleGangDriveBy, m_bFromScriptCommand) == 0xE);
static_assert(offsetof(CTaskSimpleGangDriveBy, m_pTargetEntity) == 0x34);
static_assert(offsetof(CTaskSimpleHoldEntity, m_pEntityToHold) == 0x8);
static_assert(offsetof(CPedIntelligence, m_TaskMgr) + sizeof(void*) * TASK_PRIMARY_PRIMARY == 0x10);

//! `_ftol2` (0x821B40): truncates to a 64 bit integer (NaN / out of range => the "integer indefinite") and the callers only
//! use the LOW dword (EAX). NOT the saturating 0x80000000 of a 32 bit conversion.
int32 Ftol(double v) {
    if (!(v > -9223372036854775808.0 && v < 9223372036854775808.0)) { // also catches NaN
        return 0;
    }
    return (int32)(int64)v;
}

//! x87-compare idiom `fcom -100.0f; test ah, 0x41; jp skip`: the ground Z is looked up only when `z <= -100.0f` (ordered).
float GroundZIfAuto(float x, float y, float z) {
    if (z <= -100.0f) { // 0x859014
        return CWorld::FindGroundZForCoord(x, y);
    }
    return z;
}

//! 0x8595EC
constexpr float DEG_TO_RAD = std::bit_cast<float>(0x3C8EFA35u);

//! Inlined `CVehicle::SetEngineOn(true)` (0x41BDD0): the engine can't be turned on if it's broken
void VehSetEngineOn(CVehicle& veh) {
    veh.vehicleFlags.bEngineOn = !veh.vehicleFlags.bEngineBroken;
}

//! `mov al, [veh + 0x3BA]; cmp al, 0x39; je skip; cmp al, 0x3A; je skip; mov [veh + 0x3BA], mission`
void SetMissionUnlessCrashing(CVehicle& veh, int32 mission) {
    const auto cur = veh.m_autoPilot.m_nCarMission;
    if (cur == MISSION_PLANE_CRASH_AND_BURN || cur == MISSION_HELI_CRASH_AND_BURN) {
        return;
    }
    veh.m_autoPilot.m_nCarMission = (eCarMission)mission;
}

//! The "follow target" of the plane/heli follow commands: `ped = (pedH >= 0) ? pedPool[pedH] : null; if (vehH >= 0) ped = vehPool[vehH]`
CEntity* GetFollowTarget(int32 pedH, int32 vehH) {
    CEntity* target = nullptr;
    if (pedH >= 0) {
        target = GetPedPool()->GetAtRef(pedH);
    }
    if (vehH >= 0) {
        target = GetVehiclePool()->GetAtRef(vehH);
    }
    return target;
}

//! `if (old) old->CleanUpOldReference(&slot); slot = target; target->RegisterReference(&slot)` on the autopilot target (+0x41C).
//! NOTE: `RegisterReference` is called on `target` WITHOUT a null check (both handles invalid => the original crashes as well)
void SetAutoPilotTarget(CVehicle& veh, CEntity* target) {
    auto* const slot = reinterpret_cast<CEntity**>(&veh.m_autoPilot.m_TargetEntity);
    if (*slot) {
        (*slot)->CleanUpOldReference(slot);
    }
    *slot = target;
    target->RegisterReference(slot);
}

//! 0x46B1A0 (SUPPRESS_CAR_MODEL helper): adds the model to the suppressed list unless it's already in it.
//! BUG: when the list (40 entries) is full the original writes the model at index 40, i.e. behind the array.
void SuppressVehicleModel(int32 model) {
    auto& list = CTheScripts::SuppressedVehicleModels;
    for (const auto m : list) {
        if ((int32)m == model) {
            return;
        }
    }
    size_t i = 0;
    while (i < list.size() && (int32)list[i] != -1) {
        i++;
    }
    if (i >= list.size() && notsa::IsFixBugs()) {
        return;
    }
    list.data()[i] = (eModelID)model; // i == 40 writes out of bounds, exactly like the original
}

//! 0x46A7E0 (DONT_SUPPRESS_CAR_MODEL helper): every entry equal to the model becomes -1
void DontSuppressVehicleModel(int32 model) {
    for (auto& m : CTheScripts::SuppressedVehicleModels) {
        if ((int32)m == model) {
            m = (eModelID)-1;
        }
    }
}

//! The script patrol route (CPatrolRoute @0xC18DB8, the one EXTEND/FLUSH_PATROL_ROUTE edit)
CPatrolRoute& ScriptPatrolRoute() {
    return NOTSA_GLOBAL_EXPR(0xC18DB8, (CPatrolRoute), notsa::shared::ScriptPatrolRoute);
}

//! 0x46A340 + 0x46AE80 (`CPatrolRoute::AddNode`): appends a point + anim (strings copied up to and including their NUL, the
//! rest of the 24/16 byte buffers is left untouched). Returns false (and does nothing) when the route already has 8 nodes.
bool PatrolRouteAddNode(CPatrolRoute& route, const CVector& pos, const char* animName, const char* groupName) {
    if (route.m_NumNodes >= (int32)CPatrolRoute::MAX_NODES) {
        return false;
    }
    route.m_Pos[route.m_NumNodes] = pos;
    auto& anim = route.m_Anims[route.m_NumNodes];
    char* d = anim.m_AnimName;
    for (const char* s = animName; (*d++ = *s++) != '\0';) {}
    d = anim.m_AnimGroupName;
    for (const char* s = groupName; (*d++ = *s++) != '\0';) {}
    route.m_NumNodes++;
    return true;
}

// ============================================================================ g18 (ProcessCommands1800To1899 @0x46D050)

//! 1800 CLEAR_CHAR_DECISION_MAKER_EVENT_RESPONSE / 1865 CLEAR_GROUP_DECISION_MAKER_EVENT_RESPONSE (cases @0x46D08E / @0x46E4B4,
//! the 2nd one jumps into the 1st): decision maker (script thing 7), event. Only for 0 <= idx < 20.
void ClearDecisionMakerEventResponse(int32 dm, int32 event) {
    const int32 idx = CTheScripts::GetActualScriptThingIndex(dm, SCRIPT_THING_DECISION_MAKER);
    if (idx < 0 || idx >= 20) {
        return;
    }
    CDecisionMakerTypes::GetInstance()->FlushDecisionMakerEventResponse(idx, (eEventType)event);
}

//! 1801 ADD_CHAR_DECISION_MAKER_EVENT_RESPONSE / 1866 ADD_GROUP_DECISION_MAKER_EVENT_RESPONSE (cases @0x46D0D3 / @0x46E4CB):
//! dm, event, task, p3..p6 (floats), p7, p8 (ints). Only for 0 <= idx < 20.
//! The processor copies the params into stack arrays in a peculiar order: chances = { p6, p5, p3, p4 }, flags = { p8, p7 }
//! (see .notes/S6E_TABLE.md for the stack offsets; the oracle test compares the resulting decision tables).
void AddDecisionMakerEventResponse(int32 dm, int32 event, int32 task, float p3, float p4, float p5, float p6, int32 p7, int32 p8) {
    const int32 idx = CTheScripts::GetActualScriptThingIndex(dm, SCRIPT_THING_DECISION_MAKER);
    if (idx < 0 || idx >= 20) {
        return;
    }
    float chances[4] = { p6, p5, p3, p4 };
    int32 flags[2]   = { p8, p7 };
    CDecisionMakerTypes::GetInstance()->AddEventResponse(idx, (eEventType)event, (eTaskType)task, chances, flags);
}

//! 1802 TASK_PICK_UP_OBJECT (case @0x46D166): the member reads its own params
void TaskPickUpObject(CRunningScript& S, eScriptCommands cmd) {
    S.ScriptTaskPickUpObject((int32)cmd);
}

//! 1803 DROP_OBJECT (case @0x46D175): char (no null check), flag. Only if the char has a CTaskSimpleHoldEntity (GetTaskHold(false)).
void DropObject(CPed& ped, int32 flag) {
    if (!ped.GetIntelligence()->GetTaskHold(false)) {
        return;
    }
    ped.GetIntelligence()->GetTaskHold(false)->DropEntity(&ped, flag != 0);
}

//! 1804 EXPLODE_CAR_IN_CUTSCENE (case @0x46D1DB): car (no null check). byte @+0x42A |= 0x20 (bCanBeDamaged), then
//! the virtual @+0xA4 = BlowUpCar(nullptr, true).
void ExplodeCarInCutscene(CVehicle& veh) {
    veh.vehicleFlags.bCanBeDamaged = true;
    veh.BlowUpCar(nullptr, true);
}

//! 1806 PLANE_ATTACK_PLAYER (case @0x46D24B): plane, (unused), float. Mission 0x23 unless 57/58; float @+0x9B0.
void PlaneAttackPlayer(CVehicle& veh, int32 /*unused*/, float value) {
    SetMissionUnlessCrashing(veh, MISSION_PLANE_ATTACK_PLAYER);
    static_cast<CPlane&>(veh).m_minAltitude = value;
}

//! 1807 PLANE_FLY_IN_DIRECTION (case @0x46D290): plane, f1, f2, f3. Mission 0x24 unless 57/58.
//! +0x9B4 = +0x9B8 = f1 * 0.017453292f (one rounding of the exact product), +0x9B0 = f2, +0x9A8 = f3 (raw bits of the param).
void PlaneFlyInDirection(CVehicle& veh, float f1, float f2, float f3) {
    SetMissionUnlessCrashing(veh, MISSION_PLANE_FLYINDIRECTION);
    auto&       plane   = static_cast<CPlane&>(veh);
    const float heading = f1 * DEG_TO_RAD;
    plane.m_planeHeading     = heading;
    plane.m_maxAltitude      = f3;
    plane.m_planeHeadingPrev = heading;
    plane.m_minAltitude      = f2;
}

//! 1808 PLANE_FOLLOW_ENTITY (case @0x46D304): plane, ped (>= 0), car (>= 0, wins), float.
//! Mission 0x25 unless 57/58; autopilot target (+0x41C) = the ped/car; engine on (unless broken); float @+0x9B0.
void PlaneFollowEntity(CVehicle& veh, int32 pedH, int32 vehH, float value) {
    CEntity* const target = GetFollowTarget(pedH, vehH);
    SetMissionUnlessCrashing(veh, MISSION_PLANE_FOLLOW_ENTITY);
    SetAutoPilotTarget(veh, target);
    VehSetEngineOn(veh);
    static_cast<CPlane&>(veh).m_minAltitude = value;
}

//! 1811 TASK_DRIVE_BY (case @0x46D3A7): char, target ped (>= 0), target car (>= 0, wins), x, y, z, abortRange (float), style (byte),
//! seatRHS (bool), frequency.
//! If the char (not -1) is already doing a gang drive-by (primary task type 0x3FE) AT THE SAME TARGET PED (the ped of the 2nd param,
//! not the car) nothing happens. Otherwise a CTaskSimpleGangDriveBy (+0xE = from script) is given.
void TaskDriveBy(CRunningScript& S, eScriptCommands cmd, int32 pedH, int32 targetPedH, int32 targetVehH, CVector pos, float abortRange, int32 style, int32 seatRHS, int32 frequency) {
    CPed*    targetPed = targetPedH >= 0 ? GetPedPool()->GetAtRef(targetPedH) : nullptr;
    CEntity* target    = targetPed;
    if (targetVehH >= 0) {
        target = GetVehiclePool()->GetAtRef(targetVehH);
    }
    if (pedH != -1) {
        CPed* const ped = GetPedPool()->GetAtRef(pedH);
        if (const auto* const cur = ped->GetIntelligence()->GetTaskManager().GetPrimaryTasks()[TASK_PRIMARY_PRIMARY]) {
            if (cur->GetTaskType() == TASK_SIMPLE_GANG_DRIVEBY && static_cast<const CTaskSimpleGangDriveBy*>(cur)->m_pTargetEntity == targetPed) {
                return;
            }
        }
    }
    auto* const task = new CTaskSimpleGangDriveBy{ target, &pos, abortRange, (int8)frequency, (eDrivebyStyle)(int8)style, seatRHS != 0 };
    task->m_bFromScriptCommand = true;
    S.GivePedScriptedTask(pedH, task, (int32)cmd);
}

//! 1812 SET_CAR_STAY_IN_SLOW_LANE (case @0x46D4FA): car, flag => bit 0x10 of the byte @+0x3DB
void SetCarStayInSlowLane(CVehicle& veh, int32 flag) {
    veh.m_autoPilot.carCtrlFlags.bStayInSlowLane = flag != 0;
}

//! 1813 TAKE_REMOTE_CONTROL_OF_CAR (case @0x46D545): (unused player), car => 0x45AD40
void TakeRemoteControlOfCar(int32 /*player*/, CVehicle& veh) {
    CRemote::TakeRemoteControlOfCar(&veh);
}

//! 1814 IS_CLOSEST_OBJECT_OF_TYPE_SMASHED_OR_DAMAGED (case @0x46D56F): x, y, z, radius, model (<0 = UsedObjectArray), smashed, damaged => cmp
//! z <= -100 => ground Z. FindNearestObjectOfType(model, pos, radius, 2d=0, buildings=0, vehicles=0, peds=0, objects=1, dummies=1).
//! For an OBJECT (type 4): smashed && object flag 0x400 (bIsBroken) => true; damaged && (renderDamaged || !visible) (0x46A2F0) => true.
bool IsClosestObjectOfTypeSmashedOrDamaged(float x, float y, float z, float radius, notsa::script::Model model, int32 smashed, int32 damaged) {
    const CVector pos{ x, y, GroundZIfAuto(x, y, z) };
    bool          result = false;
    CEntity* const e = CWorld::FindNearestObjectOfType((int32)model.value, pos, radius, false, false, false, false, true, true);
    if (e && e->GetType() == ENTITY_TYPE_OBJECT) {
        if (static_cast<CObject*>(e)->objectFlags.bIsBroken && smashed) {
            result = true;
        }
        if ((e->m_bRenderDamaged || !e->m_bIsVisible) && damaged) { // 0x46A2F0
            result = true;
        }
    }
    return result;
}

//! 1822 GET_OBJECT_HEALTH (case @0x46D6F4): object (no null check) => 1 int (float health, _ftol2)
int32 GetObjectHealth(CObject& obj) {
    return Ftol((double)obj.m_fHealth);
}

//! 1823 SET_OBJECT_HEALTH (case @0x46D71E): object (no null check), int => float @+0x154
void SetObjectHealth(CObject& obj, int32 health) {
    obj.m_fHealth = (float)health;
}

//! 1827 BREAK_OBJECT (case @0x46D754): object (no null check), smash flag
//! Glass: WindowRespondsToCollision(obj, 99999.9f (0x47C34FF3), zero speed, position, false), objectFlags |= 0x400.
//! Anything else: g_breakMan.Add(obj, &objInfo->breakVelocity, objInfo->breakVelocityRand, smash != 0); entity flags &= ~0x81;
//! removed from the moving list unless static; physical flags |= 0x800000; objectFlags |= 0x400; bIsStatic; speeds zeroed; DeleteRwObject.
void BreakObject(CObject& obj, int32 smash) {
    if (CGlass::IsObjectGlass(&obj)) {
        const CVector pos = obj.GetPosition(); // matrix ? matrix + 0x30 : placement + 4
        CGlass::WindowRespondsToCollision(&obj, std::bit_cast<float>(0x47C34FF3u), CVector{}, pos, false);
        obj.objectFlags.bIsBroken = true;
        return;
    }
    CVector brk = obj.m_pObjectInfo->m_vecBreakVelocity;
    g_breakMan.Add(&obj, &brk, obj.m_pObjectInfo->m_fBreakVelocityRand, smash != 0);
    obj.m_nFlags &= 0xFFFFFF7Eu; // bUsesCollision, bIsVisible
    if (!obj.GetIsStatic()) {
        obj.RemoveFromMovingList();
    }
    obj.m_nPhysicalFlags |= 0x800000;
    obj.objectFlags.bIsBroken = true;
    obj.m_bIsStatic           = true;
    obj.m_vecMoveSpeed        = CVector{};
    obj.m_vecTurnSpeed        = CVector{};
    obj.DeleteRwObject();
}

//! 1828 HELI_ATTACK_PLAYER (case @0x46D8A7): heli, (unused), float. Mission 0x17 unless 57/58; float @+0x9B0; cruise speed 100
void HeliAttackPlayer(CVehicle& veh, int32 /*unused*/, float value) {
    SetMissionUnlessCrashing(veh, MISSION_HELI_ATTACK_PLAYER);
    static_cast<CHeli&>(veh).m_fMinAltitude = value;
    veh.m_autoPilot.m_nCruiseSpeed          = 100;
}

//! 1830 HELI_FOLLOW_ENTITY / 1831 POLICE_HELI_CHASE_ENTITY (cases @0x46D8F3 / @0x46D994): heli, ped (>= 0), car (>= 0, wins), float.
//! Mission 0x27 / 0x28 unless 57/58; autopilot target (+0x41C); float @+0x9B0; cruise speed 100 (no SetEngineOn).
template<eCarMission Mission>
void HeliFollowEntity(CVehicle& veh, int32 pedH, int32 vehH, float value) {
    CEntity* const target = GetFollowTarget(pedH, vehH);
    SetMissionUnlessCrashing(veh, Mission);
    SetAutoPilotTarget(veh, target);
    static_cast<CHeli&>(veh).m_fMinAltitude = value;
    veh.m_autoPilot.m_nCruiseSpeed          = 100;
}

//! 1833 TASK_USE_MOBILE_PHONE (case @0x46DA35): char, flag
//! flag > 0 (signed): CTaskComplexUseMobilePhone(-1) is given. Otherwise the char's primary-set (3) task of type 0x640 is quit (0x634A40):
//! if `!m_bQuit` => m_bQuit = true and MakeAbortable(ped, LEISURE).
void TaskUseMobilePhone(CRunningScript& S, eScriptCommands cmd, int32 pedH, int32 flag) {
    if (flag > 0) {
        auto* const task = new CTaskComplexUseMobilePhone{ -1 }; // 0x6348A0
        S.GivePedScriptedTask(pedH, task, (int32)cmd);
        return;
    }
    CPed* const ped  = GetPedPool()->GetAtRef(pedH);
    auto* const task = ped->GetIntelligence()->GetTaskManager().FindTaskByType(TASK_PRIMARY_PRIMARY, TASK_COMPLEX_USE_MOBILE_PHONE);
    if (!task || task->GetTaskType() != TASK_COMPLEX_USE_MOBILE_PHONE) {
        return;
    }
    auto* const phone = static_cast<CTaskComplexUseMobilePhone*>(task);
    if (!phone->m_bQuit) { // 0x634A40
        phone->m_bQuit = true;
        phone->MakeAbortable(ped, ABORT_PRIORITY_LEISURE, nullptr);
    }
}

//! 1834 TASK_WARP_CHAR_INTO_CAR_AS_DRIVER (case @0x46DAE8): char, car (pool, null passed through). CTaskSimpleCarSetPedInAsDriver(car, no utility), warping = true
void TaskWarpCharIntoCarAsDriver(CRunningScript& S, eScriptCommands cmd, int32 pedH, CVehicle* veh) {
    S.GivePedScriptedTask(pedH, new CTaskSimpleCarSetPedInAsDriver{ veh, true }, (int32)cmd);
}

//! 1835 TASK_WARP_CHAR_INTO_CAR_AS_PASSENGER (case @0x46DB5D): char, car, seat (< 0 => door 0, otherwise ComputeTargetDoorToEnterAsPassenger(car, seat))
void TaskWarpCharIntoCarAsPassenger(CRunningScript& S, eScriptCommands cmd, int32 pedH, CVehicle* veh, int32 seat) {
    int32 door = 0;
    if (seat >= 0) {
        door = (int32)CCarEnterExit::ComputeTargetDoorToEnterAsPassenger(veh, seat);
    }
    S.GivePedScriptedTask(pedH, new CTaskSimpleCarSetPedInAsPassenger{ veh, (eTargetDoor)door, true }, (int32)cmd);
}

//! 1836 SWITCH_COPS_ON_BIKES (case @0x46DBE9): flag => CStreaming::DisableCopBikes(flag == 0)
void SwitchCopsOnBikes(int32 flag) {
    CStreaming::DisableCopBikes(flag == 0);
}

//! 1837 IS_FLAME_IN_ANGLED_AREA_2D / 1838 IS_FLAME_IN_ANGLED_AREA_3D (case @0x46DC1D, shared): the member reads its own params (id 0x72E = 3D)
void FlameInAngledArea(CRunningScript& S, eScriptCommands cmd) {
    S.FlameInAngledAreaCheckCommand((int32)cmd);
}

//! 1840 DAMAGE_CAR_PANEL (case @0x46DCAD) / 1852 DAMAGE_CAR_DOOR (case @0x46DFC5): car (no null check), index
//! ApplyDamage(car, component = index + 0xB / + 5, intensity 150.0f (0x871640), 1.0f)
template<int32 ComponentBase>
void DamageCarComponent(CVehicle& veh, int32 index) {
    auto& automobile = static_cast<CAutomobile&>(veh);
    automobile.m_damageManager.ApplyDamage(&automobile, (tComponent)(uint8)(index + ComponentBase), 150.0f, 1.0f);
}

//! 1841 SET_CAR_ROLL (case @0x46DCF0): car (no null check, needs a matrix), float roll (degrees)
//! pos = position; matrix.SetRotateZ(heading); matrix.RotateY(roll * 0.017453292f); matrix.pos += pos (0x46AE00)
void SetCarRoll(CVehicle& veh, float roll) {
    const CVector pos     = veh.GetPosition();
    const float   heading = veh.GetHeading();
    veh.m_matrix->SetRotateZ(heading);
    veh.m_matrix->RotateY(roll * DEG_TO_RAD);
    veh.m_matrix->GetPosition() += pos;
}

//! 1842 SUPPRESS_CAR_MODEL (case @0x46DD76) / 1843 DONT_SUPPRESS_CAR_MODEL (case @0x46DD95) / 1844 DONT_SUPPRESS_ANY_CAR_MODELS (case @0x46DDB4)
void SuppressCarModel(int32 model) {
    SuppressVehicleModel(model);
}
void DontSuppressCarModel(int32 model) {
    DontSuppressVehicleModel(model);
}
void DontSuppressAnyCarModels() {
    CTheScripts::SuppressedVehicleModels.fill((eModelID)-1);
}

//! 1847 IS_CHAR_HOLDING_OBJECT (case @0x46DE3E): char (>= 0, else none), object (>= 0, else none) => cmp
//! The held entity of a ped = its CTaskSimpleHoldEntity (GetTaskHold(false)) when SetPedPosition(ped) (vtable +0x20) says yes.
//! With a char: result = obj ? (obj == held) : (held != null). Without one: every ped of the pool (last to first, stops after the first
//! match) is checked with the same test and the object (if none: any held entity matches).
bool IsCharHoldingObject(int32 pedH, int32 objH) {
    CPed*    const ped = pedH >= 0 ? GetPedPool()->GetAtRef(pedH) : nullptr;
    CObject* const obj = objH >= 0 ? GetObjectPool()->GetAtRef(objH) : nullptr;
    const auto heldBy = [](CPed* p) -> CEntity* {
        CEntity* held = nullptr;
        if (const auto* const t = p->GetIntelligence()->GetTaskHold(false)) {
            if (p->GetIntelligence()->GetTaskHold(false)->SetPedPosition(p)) {
                held = p->GetIntelligence()->GetTaskHold(false)->m_pEntityToHold;
            }
        }
        return held;
    };
    if (ped) {
        CEntity* const held = heldBy(ped);
        return obj ? (obj == held) : (held != nullptr);
    }
    bool result = false;
    for (auto i = GetPedPool()->GetSize(); i-- > 0;) {
        if (result) {
            break;
        }
        CPed* const p = GetPedPool()->GetAt(i);
        if (!p) {
            continue;
        }
        CEntity* const held = heldBy(p);
        if (obj ? (obj == held) : (held != nullptr)) {
            result = true;
        }
    }
    return result;
}

//! 1851 SET_CAR_CAN_GO_AGAINST_TRAFFIC (case @0x46DF7A): car, flag => bit 0x40 of the byte @+0x3DB is CLEARED for flag != 0 (bCantGoAgainstTraffic)
void SetCarCanGoAgainstTraffic(CVehicle& veh, int32 flag) {
    veh.m_autoPilot.carCtrlFlags.bCantGoAgainstTraffic = flag == 0;
}

//! 1854 GET_RANDOM_CAR_IN_SPHERE_NO_SAVE (case @0x46E009): x, y, z, radius (float), model (-ve = any) => 1 handle (-1 = none)
//! Vehicles from the last pool slot to the first: appearance 1 or 2, !(vehicleFlags & 1), model matches (or model < 0), CanBeDeleted(),
//! distance < radius and < best (best starts at 9999.9f = 0x461C3F9A). Ties go to the first found (= highest slot).
int32 GetRandomCarInSphereNoSave(float x, float y, float z, float radius, int32 model) {
    int32 result = -1;
    float best   = std::bit_cast<float>(0x461C3F9Au);
    for (auto i = GetVehiclePool()->GetSize(); i-- > 0;) {
        CVehicle* const v = GetVehiclePool()->GetAt(i);
        if (!v) {
            continue;
        }
        if (v->GetVehicleAppearance() != 1 && v->GetVehicleAppearance() != 2) {
            continue;
        }
        if (v->vehicleFlags.bIsLawEnforcer) { // `test [+0x428], 1`
            continue;
        }
        if ((int32)(int16)v->m_nModelIndex != model && model >= 0) {
            continue;
        }
        if (!v->CanBeDeleted()) {
            continue;
        }
        const CVector p = v->GetPosition();
        const CVector d{ p.x - x, p.y - y, p.z - z };
        const float   dist = d.Magnitude();
        if (!(dist < radius)) {
            continue;
        }
        if (!(dist < best)) {
            continue;
        }
        result = GetVehiclePool()->GetRef(v);
        best   = dist;
    }
    return result;
}

//! 1855 GET_RANDOM_CHAR_IN_SPHERE (case @0x46E172): x, y, z, radius (float), civilian, gang, criminal => 1 handle (-1 = none)
//! Peds from the last pool slot to the first: createdBy == 1 (game), !bRemoveFromWorld (0x800), !bFadeOut (+0x470 & 8), !IsPedDead,
//! ThisIsAValidRandomPed(type, civilian, gang, criminal), no ped group, distance < radius and < best (9999.9f).
//! The chosen ped becomes a MISSION ped (SetCharCreatedBy(2)), ms_nTotalMissionPeds++ and (with mission cleanup) it is added to the list.
int32 GetRandomCharInSphere(CRunningScript& S, float x, float y, float z, float radius, int32 civilian, int32 gang, int32 criminal) {
    int32 result = -1;
    float best   = std::bit_cast<float>(0x461C3F9Au);
    for (auto i = GetPedPool()->GetSize(); i-- > 0;) {
        CPed* const p = GetPedPool()->GetAt(i);
        if (!p || p->GetCreatedBy() != PED_GAME || p->m_bRemoveFromWorld || p->bFadeOut) {
            continue;
        }
        if (S.IsPedDead(p)) {
            continue;
        }
        if (!S.ThisIsAValidRandomPed(p->m_nPedType, civilian != 0, gang != 0, criminal != 0)) {
            continue;
        }
        if (CPedGroups::GetPedsGroup(p)) {
            continue;
        }
        const CVector pos = p->GetPosition();
        const CVector d{ pos.x - x, pos.y - y, pos.z - z };
        const float   dist = d.Magnitude();
        if (!(dist < radius)) {
            continue;
        }
        if (!(dist < best)) {
            continue;
        }
        result = GetPedPool()->GetRef(p);
        best   = dist;
    }
    if (result >= 0) {
        GetPedPool()->GetAtRef(result)->SetCharCreatedBy(PED_MISSION);
        CPopulation::ms_nTotalMissionPeds++;
        if (S.m_UsesMissionCleanup) {
            CTheScripts::MissionCleanUp.AddEntityToList(result, MISSION_CLEANUP_ENTITY_TYPE_PED);
        }
    }
    return result;
}

//! 1857 HAS_CHAR_BEEN_ARRESTED (case @0x46E348): char (no null check) => cmp: m_nPedState == 0x3F
bool HasCharBeenArrested(CPed& ped) {
    return ped.m_nPedState == PEDSTATE_ARRESTED;
}

//! 1858 SET_PLANE_THROTTLE (case @0x46E37A): plane, float => float @+0x998
void SetPlaneThrottle(CVehicle& veh, float value) {
    static_cast<CPlane&>(veh).m_fAccelerationBreakStatusPrev = value;
}

//! 1859 HELI_LAND_AT_COORDS (case @0x46E3A8): heli, x, y, z, f4, f5 => TellHeliToGoToCoors; mission 0x2F unless 57/58
void HeliLandAtCoords(CVehicle& veh, float x, float y, float z, float f4, float f5) {
    static_cast<CAutomobile&>(veh).TellHeliToGoToCoors(x, y, z, f4, f5);
    SetMissionUnlessCrashing(veh, MISSION_HELI_LAND);
}

//! 1861 PLANE_STARTS_IN_AIR (case @0x46E411): plane. IsAlreadyFlying() (startedFlyingTime = now - 20000), then
//! for the model 0x208 (int16 @+0x22) the word @+0x86C is cleared.
void PlaneStartsInAir(CVehicle& veh) {
    static_cast<CPlane&>(veh).IsAlreadyFlying();
    if ((int16)veh.m_nModelIndex == 0x208) {
        static_cast<CAutomobile&>(veh).m_wMiscComponentAngle = 0;
    }
}

//! 1862 SET_RELATIONSHIP (case @0x46E450) / 1863 CLEAR_RELATIONSHIP (case @0x46E482): relationship, pedType1, pedType2
//! => (Set|Clear)PedTypeAsAcquaintance(rel, pedType1, GetPedFlag(pedType2))
void SetRelationship(int32 rel, int32 type1, int32 type2) {
    CPedType::SetPedTypeAsAcquaintance((AcquaintanceId)rel, (ePedType)type1, CPedType::GetPedFlag((ePedType)type2));
}
void ClearRelationship(int32 rel, int32 type1, int32 type2) {
    CPedType::ClearPedTypeAsAcquaintance((AcquaintanceId)rel, (ePedType)type1, CPedType::GetPedFlag((ePedType)type2));
}

//! 1868 TASK_USE_ATTRACTOR (case @0x46E675): char, attractor (script thing 6 = 2D effect). Only for 0 <= idx < 0x40:
//! CTaskComplexUseEffect(&ScriptedEffects[idx], nullptr)
void TaskUseAttractor(CRunningScript& S, eScriptCommands cmd, int32 pedH, int32 attractor) {
    const int32 idx = CTheScripts::GetActualScriptThingIndex(attractor, SCRIPT_THING_2D_EFFECT);
    if (idx < 0 || idx >= 0x40) {
        return;
    }
    auto* const fx = reinterpret_cast<C2dEffectPedAttractor*>(&CScripted2dEffects::ms_effects[idx]); // 0xC3AB00 + idx * 0x40
    S.GivePedScriptedTask(pedH, new CTaskComplexUseEffect{ fx, nullptr }, (int32)cmd);
}

//! 1869 TASK_SHOOT_AT_CHAR (case @0x46E6FF): char, target char (pool, null passed through), duration
//! CTaskSimpleGunControl(target, no pos, no move target, firingTask 3, burst 1, duration)
void TaskShootAtChar(CRunningScript& S, eScriptCommands cmd, int32 pedH, CPed* target, int32 duration) {
    S.GivePedScriptedTask(pedH, new CTaskSimpleGunControl{ target, CVector{}, CVector{}, (eGunCommand)3, 1, duration }, (int32)cmd);
}

//! 1870 SET_INFORM_RESPECTED_FRIENDS (case @0x46E77C): char (no null check), radius (float @intelligence+0xC8), number of peds (@+0xC4)
void SetInformRespectedFriends(CPed& ped, float radius, int32 num) {
    ped.GetIntelligence()->m_fDmRadius        = radius;
    ped.GetIntelligence()->m_nDmNumPedsToScan = (uint32)num;
}

//! 1871 IS_CHAR_RESPONDING_TO_EVENT (case @0x46E7BC): char (no null check), event => cmp
bool IsCharRespondingToEvent(CPed& ped, int32 event) {
    return ped.GetIntelligence()->IsRespondingToEvent((eEventType)event);
}

//! 1872 SET_OBJECT_VISIBLE (case @0x46E802): object (no null check), flag => entity flag 0x80 (bIsVisible)
void SetObjectVisible(CObject& obj, int32 flag) {
    obj.m_bIsVisible = flag != 0;
}

//! 1873 TASK_FLEE_CHAR_ANY_MEANS (case @0x46E84A): char, target char (pool, null passed through), safeDist (float), fleeTime, attackWhileFleeing,
//! shootTime, shootRecoverTime, stealCarDist (float).
//! posChangePeriod = 1000 (0x86F678), posChangeTolerance = the global float @0xC18CF0 (read at runtime).
void TaskFleeCharAnyMeans(CRunningScript& S, eScriptCommands cmd, int32 pedH, int32 targetH, float safeDist, int32 fleeTime, int32 attack, int32 shootTime, int32 shootRecover, float stealCarDist) {
    CPed* const target = GetPedPool()->GetAtRef(targetH);
    S.GivePedScriptedTask(pedH, new CTaskComplexFleeAnyMeans{
        target, attack != 0, safeDist, fleeTime, shootTime, shootRecover, stealCarDist,
        StaticRef<int32>(0x86F678), NOTSA_GLOBAL_EXPR(0xC18CF0, (float), notsa::shared::EntityPosChangeThreshold)
    }, (int32)cmd);
}

//! 1876 FLUSH_PATROL_ROUTE (case @0x46E92A): the script patrol route (@0xC18DB8) node count = 0
void FlushPatrolRoute() {
    ScriptPatrolRoute().m_NumNodes = 0;
}

//! 1877 EXTEND_PATROL_ROUTE (case @0x46E93B): x, y, z, then 2 text labels (24 and 16 bytes) read AFTER the params.
//! "NONE" (5 bytes incl. NUL, case sensitive) as the 1st label => the node has no anim. Silently ignored when the route is full (8 nodes).
void ExtendPatrolRoute(CRunningScript& S, CVector pos) {
    char anim[24]{};
    char group[16]{};
    S.ReadTextLabelFromScript(anim, sizeof(anim));
    S.ReadTextLabelFromScript(group, sizeof(group));
    if (std::memcmp(anim, "NONE", 5) == 0) { // 0x859E38
        anim[0]  = '\0';
        group[0] = '\0';
    }
    PatrolRouteAddNode(ScriptPatrolRoute(), pos, anim, group);
}

//! 1882 PLAY_OBJECT_ANIM (case @0x46E9FC): object (no null check), anim (24), block (16), then 3 params: blend (float), flag1, flag2 => cmp
//! hierarchy = GetAnimation(anim, GetAnimationBlock(block)); flags = (flag1 ? 2 : 0) | (flag2 == 0 ? 8 : 0).
//! Without a clump (+0x18): false. Otherwise the clump is initialised if needed and BlendAnimation(clump, hier, flags, blend) => true.
bool PlayObjectAnim(CRunningScript& S, CObject& obj) {
    char anim[24]{};
    char block[16]{};
    S.ReadTextLabelFromScript(anim, sizeof(anim));
    S.ReadTextLabelFromScript(block, sizeof(block));
    const float blend = Read<float>(&S);
    const int32 flag1 = Read<int32>(&S);
    const int32 flag2 = Read<int32>(&S);
    auto* const hier  = CAnimManager::GetAnimation(anim, CAnimManager::GetAnimationBlock(block));
    int32       flags = 0;
    if (flag1) {
        flags = 2;
    }
    if (flag2 == 0) {
        flags |= 8;
    }
    auto* const clump = reinterpret_cast<RpClump*>(obj.GetRwObject()); // +0x18, no type check
    if (!clump) {
        return false;
    }
    if (!RpAnimBlendClumpIsInitialized(clump)) {
        RpAnimBlendClumpInit(clump);
    }
    CAnimManager::BlendAnimation(clump, hier, flags, blend);
    return true;
}

//! 1883 SET_RADAR_ZOOM (case @0x46EAE7): byte @0xA444A3
void SetRadarZoom(int32 zoom) {
    CTheScripts::RadarZoomValue = (uint8)zoom;
}

//! 1884 DOES_BLIP_EXIST (case @0x46EB03): blip handle => cmp: GetActualBlipArrayIndex(h) != -1
bool DoesBlipExist(int32 blip) {
    return CRadar::GetActualBlipArrayIndex(blip) != -1;
}

//! 1885 LOAD_PRICES (case @0x46EB37) / 1886 LOAD_SHOP (case @0x46EB59): 16 byte text label
void LoadPrices(CRunningScript& S) {
    char name[16]{};
    S.ReadTextLabelFromScript(name, sizeof(name));
    CShopping::LoadPrices(name);
}
void LoadShop(CRunningScript& S) {
    char name[16]{};
    S.ReadTextLabelFromScript(name, sizeof(name));
    CShopping::LoadShop(name);
}

//! 1887 GET_NUMBER_OF_ITEMS_IN_SHOP (case @0x46EB7B) => 1 int (CShopping::ms_numItemsInShop @0xA9A7F0)
int32 GetNumberOfItemsInShop() {
    return StaticRef<int32>(0xA9A7F0); // CShopping::ms_numItemsInShop (private)
}

//! 1888 GET_ITEM_IN_SHOP (case @0x46EB97): index (no bounds check) => 1 int (ms_shopContents[index] @0xA9A318)
int32 GetItemInShop(int32 index) {
    return (int32)(&StaticRef<uint32>(0xA9A318))[index]; // CShopping::ms_shopContents (private), no bounds check
}

//! 1889 GET_PRICE_OF_ITEM (case @0x46EBAF): item => 1 int (CShopping::GetPrice)
int32 GetPriceOfItem(int32 item) {
    return CShopping::GetPrice((uint32)item);
}

//! 1890 TASK_DEAD (case @0x46EBDB): char => CTaskComplexDie(weapon 0, animGroup 0, anim 0xF, blend 4.0f (0x40800000), speed 0, ...0)
void TaskDead(CRunningScript& S, eScriptCommands cmd, int32 pedH) {
    S.GivePedScriptedTask(pedH, new CTaskComplexDie{ (eWeaponType)0, (AssocGroupId)0, (AnimationId)0xF, 4.0f, 0.0f, false, false, (eDirection)0, false }, (int32)cmd);
}

//! 1891 SET_CAR_AS_MISSION_CAR (case @0x46EC43): car (no null check)
//! With mission cleanup, a car that was created as RANDOM (1) or PARKED (3) becomes a MISSION vehicle (SetVehicleCreatedBy(2)) and is added
//! to the cleanup list.
void SetCarAsMissionCar(CRunningScript& S, CVehicle& veh) {
    const int32 handle = GetVehiclePool()->GetRef(&veh);
    if (!S.m_UsesMissionCleanup) {
        return;
    }
    const auto createdBy = veh.m_nCreatedBy;
    if (createdBy == RANDOM_VEHICLE || createdBy == PARKED_VEHICLE) {
        veh.SetVehicleCreatedBy(MISSION_VEHICLE);
        CTheScripts::MissionCleanUp.AddEntityToList(handle, MISSION_CLEANUP_ENTITY_TYPE_VEHICLE);
    }
}
} // namespace

void notsa::script::commands::ported::g18::RegisterHandlers() {
    REGISTER_COMMAND_HANDLER_BEGIN("Ported g18");

    REGISTER_COMMAND_HANDLER(COMMAND_CLEAR_CHAR_DECISION_MAKER_EVENT_RESPONSE, ClearDecisionMakerEventResponse);
    REGISTER_COMMAND_HANDLER(COMMAND_ADD_CHAR_DECISION_MAKER_EVENT_RESPONSE, AddDecisionMakerEventResponse);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_PICK_UP_OBJECT, TaskPickUpObject);
    REGISTER_COMMAND_HANDLER(COMMAND_DROP_OBJECT, DropObject);
    REGISTER_COMMAND_HANDLER(COMMAND_EXPLODE_CAR_IN_CUTSCENE, ExplodeCarInCutscene);
    REGISTER_COMMAND_HANDLER(COMMAND_PLANE_ATTACK_PLAYER, PlaneAttackPlayer);
    REGISTER_COMMAND_HANDLER(COMMAND_PLANE_FLY_IN_DIRECTION, PlaneFlyInDirection);
    REGISTER_COMMAND_HANDLER(COMMAND_PLANE_FOLLOW_ENTITY, PlaneFollowEntity);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_DRIVE_BY, TaskDriveBy);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_CAR_STAY_IN_SLOW_LANE, SetCarStayInSlowLane);
    REGISTER_COMMAND_HANDLER(COMMAND_TAKE_REMOTE_CONTROL_OF_CAR, TakeRemoteControlOfCar);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_CLOSEST_OBJECT_OF_TYPE_SMASHED_OR_DAMAGED, IsClosestObjectOfTypeSmashedOrDamaged);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_OBJECT_HEALTH, GetObjectHealth);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_OBJECT_HEALTH, SetObjectHealth);
    REGISTER_COMMAND_HANDLER(COMMAND_BREAK_OBJECT, BreakObject);
    REGISTER_COMMAND_HANDLER(COMMAND_HELI_ATTACK_PLAYER, HeliAttackPlayer);
    REGISTER_COMMAND_HANDLER(COMMAND_HELI_FOLLOW_ENTITY, HeliFollowEntity<MISSION_HELI_FOLLOW_ENTITY>);
    REGISTER_COMMAND_HANDLER(COMMAND_POLICE_HELI_CHASE_ENTITY, HeliFollowEntity<MISSION_HELI_POLICE_BEHAVIOUR>);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_USE_MOBILE_PHONE, TaskUseMobilePhone);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_WARP_CHAR_INTO_CAR_AS_DRIVER, TaskWarpCharIntoCarAsDriver);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_WARP_CHAR_INTO_CAR_AS_PASSENGER, TaskWarpCharIntoCarAsPassenger);
    REGISTER_COMMAND_HANDLER(COMMAND_SWITCH_COPS_ON_BIKES, SwitchCopsOnBikes);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_FLAME_IN_ANGLED_AREA_2D, FlameInAngledArea);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_FLAME_IN_ANGLED_AREA_3D, FlameInAngledArea);
    REGISTER_COMMAND_HANDLER(COMMAND_DAMAGE_CAR_PANEL, DamageCarComponent<0xB>);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_CAR_ROLL, SetCarRoll);
    REGISTER_COMMAND_HANDLER(COMMAND_SUPPRESS_CAR_MODEL, SuppressCarModel);
    REGISTER_COMMAND_HANDLER(COMMAND_DONT_SUPPRESS_CAR_MODEL, DontSuppressCarModel);
    REGISTER_COMMAND_HANDLER(COMMAND_DONT_SUPPRESS_ANY_CAR_MODELS, DontSuppressAnyCarModels);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_CHAR_HOLDING_OBJECT, IsCharHoldingObject);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_CAR_CAN_GO_AGAINST_TRAFFIC, SetCarCanGoAgainstTraffic);
    REGISTER_COMMAND_HANDLER(COMMAND_DAMAGE_CAR_DOOR, DamageCarComponent<5>);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_RANDOM_CAR_IN_SPHERE_NO_SAVE, GetRandomCarInSphereNoSave);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_RANDOM_CHAR_IN_SPHERE, GetRandomCharInSphere);
    REGISTER_COMMAND_HANDLER(COMMAND_HAS_CHAR_BEEN_ARRESTED, HasCharBeenArrested);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_PLANE_THROTTLE, SetPlaneThrottle);
    REGISTER_COMMAND_HANDLER(COMMAND_HELI_LAND_AT_COORDS, HeliLandAtCoords);
    REGISTER_COMMAND_HANDLER(COMMAND_PLANE_STARTS_IN_AIR, PlaneStartsInAir);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_RELATIONSHIP, SetRelationship);
    REGISTER_COMMAND_HANDLER(COMMAND_CLEAR_RELATIONSHIP, ClearRelationship);
    REGISTER_COMMAND_HANDLER(COMMAND_CLEAR_GROUP_DECISION_MAKER_EVENT_RESPONSE, ClearDecisionMakerEventResponse);
    REGISTER_COMMAND_HANDLER(COMMAND_ADD_GROUP_DECISION_MAKER_EVENT_RESPONSE, AddDecisionMakerEventResponse);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_USE_ATTRACTOR, TaskUseAttractor);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_SHOOT_AT_CHAR, TaskShootAtChar);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_INFORM_RESPECTED_FRIENDS, SetInformRespectedFriends);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_CHAR_RESPONDING_TO_EVENT, IsCharRespondingToEvent);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_OBJECT_VISIBLE, SetObjectVisible);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_FLEE_CHAR_ANY_MEANS, TaskFleeCharAnyMeans);
    REGISTER_COMMAND_HANDLER(COMMAND_FLUSH_PATROL_ROUTE, FlushPatrolRoute);
    REGISTER_COMMAND_HANDLER(COMMAND_EXTEND_PATROL_ROUTE, ExtendPatrolRoute);
    REGISTER_COMMAND_HANDLER(COMMAND_PLAY_OBJECT_ANIM, PlayObjectAnim);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_RADAR_ZOOM, SetRadarZoom);
    REGISTER_COMMAND_HANDLER(COMMAND_DOES_BLIP_EXIST, DoesBlipExist);
    REGISTER_COMMAND_HANDLER(COMMAND_LOAD_PRICES, LoadPrices);
    REGISTER_COMMAND_HANDLER(COMMAND_LOAD_SHOP, LoadShop);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_NUMBER_OF_ITEMS_IN_SHOP, GetNumberOfItemsInShop);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_ITEM_IN_SHOP, GetItemInShop);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_PRICE_OF_ITEM, GetPriceOfItem);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_DEAD, TaskDead);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_CAR_AS_MISSION_CAR, SetCarAsMissionCar);
}

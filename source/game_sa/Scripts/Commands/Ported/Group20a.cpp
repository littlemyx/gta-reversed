#include <StdInc.h>

#include "../Commands.hpp"
#include <CommandParser/Parser.hpp>
#include "Group20_21.hpp"

#include <cmath>
#include <cstddef>

#include "Clock.h"
#include "Timer.h"
#include "Radar.h"
#include "Shadows.h"
#include "Streaming.h"
#include "StreamedScripts.h"
#include "ScriptsForBrains.h"
#include "MissionCleanup.h"
#include "PedType.h"
#include "PedGroups.h"
#include "PedGroup.h"
#include "CarAI.h"
#include "Checkpoint.h"
#include "Checkpoints.h"
#include "TheZones.h"
#include "EntryExit.h"
#include "EntryExitManager.h"
#include "PedGeometryAnalyser.h" // CPointRoute
#include "DecisionMakers/DecisionMakerTypes.h"
#include "Models/ModelInfo.h"
#include "Entity/Vehicle/Vehicle.h"
#include "Entity/Object/Object.h"
#include "TaskSimpleSwim.h"
#include "TaskComplexDrivePointRoute.h"

using namespace notsa::script;
using namespace notsa::script::commands::ported::g09_12;
using namespace notsa::script::commands::ported::g20_21;

/*!
* Script commands ported from the exe's group processor for ids 2000..2043 (`CRunningScript::ProcessCommands2000To2099` @0x472xxx)
* for the vanilla commands that had no handler of their own (S6-F, g20a). Written from the asm of each `case`.
*/

// Raw offsets the exe uses directly in these cases
static_assert(offsetof(CPhysical, m_vecTurnSpeed) == 0x50);
static_assert(offsetof(CPhysical, m_fMass) == 0x8C);
static_assert(offsetof(CPhysical, m_fTurnMass) == 0x90);
static_assert(offsetof(CPhysical, m_vecCentreOfMass) == 0xA4);
static_assert(offsetof(CVehicle, m_vehicleSpecialColIndex) == 0x48B);
static_assert(offsetof(CObject, objectFlags) == 0x140);
static_assert(offsetof(CObject, m_nColDamageEffect) == 0x144);
static_assert(offsetof(CObject, m_nSpecialColResponseCase) == 0x145);
static_assert(offsetof(CPed, m_nWeaponShootingRate) == 0x719);
static_assert(offsetof(CTaskSimpleSwim, m_vecPos) == 0x14);
static_assert(offsetof(CCheckpoint, m_ID) == 4);
static_assert(offsetof(CRunningScript, m_UsesMissionCleanup) == 0xC6);
static_assert(sizeof(CDecisionMaker) == 0x99C);
static_assert(offsetof(CDecisionMakerTypes, m_DecisionMakers) == 4);
static_assert(offsetof(CDecisionMakerTypes, m_DefaultMissionPedDecisionMaker) == 0xCB50);
static_assert(offsetof(CDecisionMakerTypes, m_DefaultMissionPedGroupDecisionMaker) == 0xE824);
static_assert(sizeof(CPedGroup) == 0x2D4);
static_assert(offsetof(CPedGroup, m_groupMembership) == 8);
static_assert(sizeof(tHydraulicData) == 0x28 && offsetof(tHydraulicData, m_aWheelSuspension) == 0x18);
static_assert(MISSION_CLEANUP_ENTITY_TYPE_DECISION_MAKER == 9 && SCRIPT_THING_DECISION_MAKER == 7 && SCRIPT_THING_PED_GROUP == 8 && SCRIPT_THING_CHECKPOINT == 3);

namespace {
//! 0xC18D50: the route that FLUSH_ROUTE / EXTEND_ROUTE build and the point-route tasks consume
CPointRoute& ScriptRoute() { return StaticRef<CPointRoute>(0xC18D50); }

//! 0x59C910 (`CVector::Normalise`): the squared length and the inverse length stay in extended precision.
//! A non-positive squared length only sets `x = 1.0f` (y and z are left as they are); NaN goes the regular (sqrt) way.
void NormaliseOriginal(CVector& v) {
    const double magSq = ((double)v.x * v.x + (double)v.y * v.y) + (double)v.z * v.z;
    if (magSq <= 0.0) { // 0x858B50 (0.0f); fcom + test ah, 0x41 + jp
        v.x = 1.0f;
        return;
    }
    const double inv = 1.0 / std::sqrt(magSq); // fsqrt; fdivr 1.0f (0x858624)
    v.x = (float)(inv * v.x);
    v.y = (float)(inv * v.y);
    v.z = (float)(inv * v.z);
}

//! 0x59C730 (`CrossProduct(out, a, b)`): each component is a difference of two (exact) products, rounded to float once
CVector CrossProductOriginal(const CVector& a, const CVector& b) {
    return CVector{
        (float)((double)b.z * a.y - (double)a.z * b.y),
        (float)((double)a.z * b.x - (double)b.z * a.x),
        (float)((double)a.x * b.y - (double)b.x * a.y)
    };
}

//! 0x406DA0 (squared length): `(x*x + y*y) + z*z` kept in extended precision (not rounded to float)
double SquaredMagnitudeOriginal(const CVector& v) {
    return ((double)v.x * v.x + (double)v.y * v.y) + (double)v.z * v.z;
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

//! The common part of 2021 / 2022: `type` is 0 for the ped (char) variant and 1 for the group variant
//! CollectParameters(1) => decision maker script handle; the output variable is then peeked (CollectNextParameterWithoutIncreasingPC +
//! GetActualScriptThingIndex, both pure, the result is dropped by the exe) and finally written by StoreParameters(1)
OpcodeResult CopyDecisionMakerImpl(CRunningScript& S, int32 dmHandle, int32 type) {
    int32 srcIndex = -1;
    if (dmHandle != -1) {
        srcIndex = CTheScripts::GetActualScriptThingIndex(dmHandle, SCRIPT_THING_DECISION_MAKER);
    }
    const auto newIndex = CopyDecisionMakerOriginal(*CDecisionMakerTypes::GetInstance(), srcIndex, type, S.m_UsesMissionCleanup); // 0x4684F0, 0x6070F0
    const auto handle   = CTheScripts::GetNewUniqueScriptThingIndex(newIndex, SCRIPT_THING_DECISION_MAKER);
    StoreArg(&S, (int32)handle);
    if (S.m_UsesMissionCleanup) {
        CTheScripts::MissionCleanUp.AddEntityToList(handle, MISSION_CLEANUP_ENTITY_TYPE_DECISION_MAKER); // 0x4637E0
    }
    return OR_CONTINUE;
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

//! 2000 GET_CURRENT_DAY_OF_WEEK (case @0x47234E): => day (CClock::CurrentDay 0xB7014E, zero-extended byte)
int32 GetCurrentDayOfWeek() {
    return CClock::CurrentDay;
}

//! 2003 REGISTER_SCRIPT_BRAIN_FOR_CODE_USE (case @0x47236A): scmIndex, label  -- CollectParameters(1), then the label (ReadTextLabelFromScript, 8 chars)
//! The script index is translated by CStreamedScripts::GetProperIndexFromIndexUsedByScript (16 bit in, 16 bit out), brain type is 3
void RegisterScriptBrainForCodeUse(CRunningScript& S, int32 scmIndex) {
    const int16 index = CTheScripts::StreamedScripts.GetProperIndexFromIndexUsedByScript((int16)scmIndex); // 0x470810
    // BUG: the label is not 0-terminated by ReadTextLabelFromScript and the callee strcpy's it (the exe's buffer is 8 bytes, uninitialised)
    char label[8 + 1]{}; // NOTSA: + terminator
    S.ReadTextLabelFromScript(label, 8); // 0x463D50
    CTheScripts::ScriptsForBrains.AddNewStreamedScriptBrainForCodeUse(index, label, 3); // 0x46A9C0
}

//! 2005 APPLY_FORCE_TO_CAR (case @0x4723C6): car, fx, fy, fz, ox, oy, oz
//! CollectParameters(7); the vehicle is not null checked. offset += matrix * centreOfMass (0x59C790, 0x411A00); the force is
//! scaled by the effective mass in the direction of the force: 1 / (|(offset - matrix * COM) x normalise(force)|^2 / turnMass + 1 / mass)
//! (extended precision, rounded to float once); then CPhysical::ApplyForce(force, offset, true) [0x542B50]
void ApplyForceToCar(CVehicle& veh, CVector force, CVector offset) {
    const auto& mat = *veh.m_matrix;

    const CVector comWorld = TransformVectorOriginal(mat, veh.m_vecCentreOfMass); // 0x59C790
    offset.x = comWorld.x + offset.x; // 0x411A00 (`offset += comWorld`)
    offset.y = comWorld.y + offset.y;
    offset.z = comWorld.z + offset.z;

    CVector dir = force;
    NormaliseOriginal(dir); // 0x59C910

    const CVector comWorld2 = TransformVectorOriginal(mat, veh.m_vecCentreOfMass); // (the exe transforms it a second time)
    const CVector rel{ offset.x - comWorld2.x, offset.y - comWorld2.y, offset.z - comWorld2.z }; // 0x40FE60
    const CVector torque = CrossProductOriginal(rel, dir); // 0x59C730 (rel x dir)
    const float   scale  = (float)(1.0 / (SquaredMagnitudeOriginal(torque) / (double)veh.m_fTurnMass + 1.0 / (double)veh.m_fMass)); // 0x406DA0

    force.x = force.x * scale; // 0x40FEF0 (`force *= scale`)
    force.y = force.y * scale;
    force.z = force.z * scale;
    veh.ApplyForce(force, offset, true);
}

//! 2006 IS_INT_LVAR_EQUAL_TO_INT_VAR (case @0x472510): lvar, var  -- both read by reference (GetPointerToScriptVariable(1 = local), (2 = global)) => cmp
bool IsIntLvarEqualToIntVar(CRunningScript& S) {
    const auto lvar = S.GetPointerToScriptVariable(VAR_LOCAL);
    const auto var  = S.GetPointerToScriptVariable(VAR_GLOBAL);
    return var->iParam == lvar->iParam;
}

//! 2007 IS_FLOAT_LVAR_EQUAL_TO_FLOAT_VAR (case @0x47253A): lvar, var  -- by reference; float compare (fcomp; NaN => false) => cmp
bool IsFloatLvarEqualToFloatVar(CRunningScript& S) {
    const auto lvar = S.GetPointerToScriptVariable(VAR_LOCAL);
    const auto var  = S.GetPointerToScriptVariable(VAR_GLOBAL);
    return var->fParam == lvar->fParam;
}

//! 2010 ADD_TO_CAR_ROTATION_VELOCITY (case @0x472576): car, x, y, z  -- turn speed += matrix * (x, y, z) * 0.02 (0x858B38);
//! a static vehicle is made non-static and put into the moving list first
void AddToCarRotationVelocity(CVehicle& veh, float x, float y, float z) {
    const float k = 0.02f; // 0x858B38
    CVector turn = veh.m_vecTurnSpeed; // (copied before anything is touched, written back at the very end)
    const CVector r = TransformVectorOriginal(*veh.m_matrix, CVector{ x * k, y * k, z * k }); // 0x59C790
    turn.x = r.x + turn.x;
    turn.y = r.y + turn.y;
    turn.z = r.z + turn.z;
    if (veh.m_bIsStatic) { // +0x1C bit 2
        veh.SetIsStatic(false); // vtable +0x10
        veh.AddToMovingList(); // 0x542800
    }
    veh.m_vecTurnSpeed = turn;
}

//! 2011 SET_CAR_ROTATION_VELOCITY (case @0x472677): car, x, y, z  -- turn speed = matrix * (timeStep * (x, y, z)) * 0.02 (0x858B38)
//! (0xB7CB5C = CTimer::ms_fTimeStep); a static vehicle is made non-static and put into the moving list first
void SetCarRotationVelocity(CVehicle& veh, float x, float y, float z) {
    const float ts = CTimer::ms_fTimeStep;
    const CVector r = TransformVectorOriginal(*veh.m_matrix, CVector{ ts * x, ts * y, ts * z }); // 0x59C790
    if (veh.m_bIsStatic) {
        veh.SetIsStatic(false);
        veh.AddToMovingList();
    }
    const float k = 0.02f; // 0x858B38
    veh.m_vecTurnSpeed = CVector{ r.x * k, r.y * k, r.z * k }; // 0x45AFB0 (CPhysical::SetTurnSpeed)
}

//! 2013 SET_CHAR_SHOOT_RATE (case @0x472759): ped, rate  -- ped +0x719 (m_nWeaponShootingRate) = low byte; the ped is not null checked
void SetCharShootRate(CPed& ped, int32 rate) {
    ped.m_nWeaponShootingRate = (uint8)rate;
}

//! 2014 IS_MODEL_IN_CDIMAGE (case @0x472786): model (negative => UsedObjectArray) => cmp  -- CStreaming::IsObjectInCdImage [0x407800]
bool IsModelInCdimage(notsa::script::Model model) {
    return CStreaming::IsObjectInCdImage(model);
}

//! 2015 REMOVE_OIL_PUDDLES_IN_AREA (case @0x4727B4): x1, y1, x2, y2  -- both pairs sorted (swap only if a > b), CShadows::RemoveOilInArea(x1, x2, y1, y2) [0x7074F0]
void RemoveOilPuddlesInArea(float x1, float y1, float x2, float y2) {
    SortPair(x1, x2);
    SortPair(y1, y2);
    CShadows::RemoveOilInArea(x1, x2, y1, y2);
}

//! 2016 SET_BLIP_AS_FRIENDLY (case @0x472845): blip, flag  -- CRadar::SetBlipFriendly(blip, (uint8)flag) [0x583EB0]
//! (the callee only takes bit 0 of the byte: `shl al, 3; xor al, dl; and al, 8`)
void SetBlipAsFriendly(int32 blip, int32 flag) {
    CRadar::SetBlipFriendly(blip, ((uint8)flag & 1) != 0);
}

//! 2017 TASK_SWIM_TO_COORD (case @0x47286C): ped, x, y, z  -- CollectParameters(4)
//! If the ped (handle != -1, valid) is already swimming (simplest active task is CTaskSimpleSwim) only the target (task +0x14) is replaced and
//! nothing else happens. Otherwise CTaskSimpleSwim(&pos, null) [0x688930] is given as a scripted task (GivePedScriptedTask [0x465C20]).
void TaskSwimToCoord(eScriptCommands command, CRunningScript& S, int32 pedHandle, CVector pos) {
    if (pedHandle != -1) {
        if (const auto ped = GetPedPool()->GetAtRef(pedHandle)) {
            if (const auto swim = ped->GetIntelligence()->GetTaskSwim()) { // 0x601070 (the exe calls it twice)
                swim->m_vecPos = pos;
                return;
            }
        }
    }
    S.GivePedScriptedTask(pedHandle, new CTaskSimpleSwim{ &pos, nullptr }, command);
}

//! 2020 GET_MODEL_DIMENSIONS (case @0x472932): model (negative => UsedObjectArray) => min x, y, z, max x, y, z (raw floats of the model's collision bounding box)
//! No null checks: ms_modelInfoPtrs[model] (0xA9B0C8) -> +0x14 (m_pColModel) -> bounding box at +0 / +0xC
MultiRet<float, float, float, float, float, float> GetModelDimensions(notsa::script::Model model) {
    const auto& bb = CModelInfo::GetModelInfo(model)->GetColModel()->m_boundBox;
    return { bb.m_vecMin.x, bb.m_vecMin.y, bb.m_vecMin.z, bb.m_vecMax.x, bb.m_vecMax.y, bb.m_vecMax.z };
}

//! 2021 COPY_CHAR_DECISION_MAKER (case @0x4729A4): dm => new dm   (handle -1 => the default mission ped decision maker)
OpcodeResult CopyCharDecisionMaker(CRunningScript& S, int32 dm) {
    return CopyDecisionMakerImpl(S, dm, 0);
}

//! 2022 COPY_GROUP_DECISION_MAKER (case @0x472A32): dm => new dm   (handle -1 => the default mission group decision maker)
OpcodeResult CopyGroupDecisionMaker(CRunningScript& S, int32 dm) {
    return CopyDecisionMakerImpl(S, dm, 1);
}

//! 2023 TASK_DRIVE_POINT_ROUTE_ADVANCED (case @0x472ABF): ped, vehicle, speed, mode, carModel, drivingStyle  -- CollectParameters(6)
//! veh = (handle >= 0 (signed)) ? GetAtRef : null; carModel 0 => -1, 1 => 0x19F; CTaskComplexDrivePointRoute(veh, route 0xC18D50, speed, mode,
//! carModel, targetRadius -1.0f, drivingStyle) [0x6433E0]
void TaskDrivePointRouteAdvanced(eScriptCommands command, CRunningScript& S, int32 pedHandle, int32 vehHandle, float speed, int32 mode, int32 carModel, int32 drivingStyle) {
    CVehicle* veh = nullptr;
    if (vehHandle >= 0) {
        veh = GetVehiclePool()->GetAtRef(vehHandle);
    }
    if (carModel == 0) {
        carModel = -1;
    } else if (carModel == 1) {
        carModel = 0x19F;
    }
    S.GivePedScriptedTask(
        pedHandle,
        new CTaskComplexDrivePointRoute{ veh, ScriptRoute(), speed, (uint32)mode, (eModelID)carModel, -1.0f, (eCarDrivingStyle)drivingStyle },
        command
    );
}

//! 2024 IS_RELATIONSHIP_SET (case @0x472B88): acquaintanceId, pedType, pedType2 => cmp
//! (CPedType::GetPedTypeAcquaintances(id, pedType) [0x6089D0] & GetPedFlag(pedType2) [0x608830]) != 0
//! GetPedFlag: `pedType2 >= 32 (signed) ? 0 : 1 << (pedType2 & 31)` (a negative value shifts by its low 5 bits: `shl eax, cl`)
bool IsRelationshipSet(int32 acquaintanceId, int32 pedType, int32 pedType2) {
    const uint32 acquaintances = CPedType::GetPedTypeAcquaintances((AcquaintanceId)acquaintanceId, (ePedType)pedType);
    const uint32 flag          = pedType2 >= 32 ? 0u : (1u << (pedType2 & 31));
    return (acquaintances & flag) != 0;
}

//! 2031 GET_CITY_FROM_COORDS (case @0x472C1B): x, y, z => level (CTheZones::GetLevelFromPosition [0x572300], zero-extended byte)
int32 GetCityFromCoords(CVector pos) {
    return (int32)CTheZones::GetLevelFromPosition(pos);
}

//! 2032 HAS_OBJECT_OF_TYPE_BEEN_SMASHED (case @0x472C63): x, y, z, radius, model (negative => UsedObjectArray) => cmp
//! z <= -100.0f => ground Z. Walks the object pool from the last slot to the first until one is found: object is "broken" (+0x140 bit 10),
//! has that model (sign-extended 16 bit), and |pos - objectPos| <= radius (extended precision sqrt, ordered compare)
bool HasObjectOfTypeBeenSmashed(float x, float y, float z, float radius, notsa::script::Model model) {
    z = GroundZIfAuto(x, y, z);
    const CVector pos{ x, y, z };

    auto&      pool  = *GetObjectPool();
    bool       found = false;
    for (auto i = (int32)pool.GetSize(); i > 0 && !found;) {
        const auto obj = pool.GetAt(--i);
        if (!obj || !obj->objectFlags.bIsBroken || (int32)(int16)obj->m_nModelIndex != (int32)model) {
            continue;
        }
        const CVector objPos = obj->GetPosition(); // matrix ? matrix->pos : placement
        const CVector diff{ pos.x - objPos.x, pos.y - objPos.y, pos.z - objPos.z }; // 0x40FE60
        const double  dist = std::sqrt(SquaredMagnitudeOriginal(diff)); // 0x4082C0 (kept in extended precision)
        if (dist <= (double)radius) { // fcomp + test ah, 0x41 + jp: ordered `<=`
            found = true;
        }
    }
    return found;
}

//! 2035 SET_CHECKPOINT_COORDS (case @0x472E4A): checkpoint, x, y, z  -- checkpoint script thing (type 3) not valid / no CCheckpoint => nothing
//! CCheckpoints::UpdatePos(checkpoint->m_ID (+4), pos) [0x722900]
void SetCheckpointCoords(int32 handle, CVector pos) {
    const auto idx = CTheScripts::GetActualScriptThingIndex(handle, SCRIPT_THING_CHECKPOINT); // 0x4839A0
    if (idx < 0) {
        return;
    }
    if (const auto cp = CTheScripts::ScriptCheckpointArray[idx].m_Checkpoint) {
        CCheckpoints::UpdatePos(cp->m_ID, pos);
    }
}

//! 2037 CONTROL_CAR_HYDRAULICS (case @0x472EBC): car, w0, w1, w2, w3  -- sets the 4 wheel suspension values of the vehicle's special hydraulic data slot
//! (0xC1CB60 + slot * 0x28 + 0x18 = tHydraulicData::m_aWheelSuspension); the slot (int8 +0x48B) is acquired (GetSpecialColModel 0x6DF3D0) if it is negative
//! and nothing happens if it still is. The vehicle is not null checked.
void ControlCarHydraulics(CVehicle& veh, float w0, float w1, float w2, float w3) {
    if (veh.m_vehicleSpecialColIndex < 0) {
        veh.GetSpecialColModel();
    }
    const int8 slot = veh.m_vehicleSpecialColIndex;
    if (slot < 0) {
        return;
    }
    auto& susp = CVehicle::m_aSpecialHydraulicData[slot].m_aWheelSuspension;
    susp[0] = w0;
    susp[1] = w1;
    susp[2] = w2;
    susp[3] = w3;
}

//! 2038 GET_GROUP_SIZE (case @0x472F57): group => hasLeader (0 / 1), memberCount (excluding the leader)  -- both 0 if the group (script thing type 8)
//! isn't valid or its index is outside [0, 8). No compare flag.
MultiRet<int32, int32> GetGroupSize(int32 groupHandle) {
    const auto idx = CTheScripts::GetActualScriptThingIndex(groupHandle, SCRIPT_THING_PED_GROUP); // 0x4839A0
    if (idx >= 0 && idx < 8) {
        auto& membership = CPedGroups::ms_groups[idx].GetMembership(); // 0xC09928 + idx * 0x2D4
        const int32 hasLeader = membership.GetLeader() != nullptr; // 0x5F69A0
        const int32 count     = membership.CountMembersExcludingLeader(); // 0x5F6AA0
        return { hasLeader, count };
    }
    return { 0, 0 };
}

//! 2039 SET_OBJECT_COLLISION_DAMAGE_EFFECT (case @0x472FD4): object, flag  -- swaps the (byte) damage effect +0x144 and its saved copy +0x145:
//! flag != 0: if saved != 0 { effect = saved; saved = 0 }    flag == 0: if effect != 0 { saved = effect; effect = 0 }  -- the object is not null checked
void SetObjectCollisionDamageEffect(CObject& obj, int32 flag) {
    if (flag != 0) {
        if (obj.m_nSpecialColResponseCase != 0) {
            obj.m_nColDamageEffect        = obj.m_nSpecialColResponseCase;
            obj.m_nSpecialColResponseCase = 0;
        }
    } else {
        if (obj.m_nColDamageEffect != 0) {
            obj.m_nSpecialColResponseCase = obj.m_nColDamageEffect;
            obj.m_nColDamageEffect        = 0;
        }
    }
}

//! 2040 SET_CAR_FOLLOW_CAR (case @0x47303D): car, target, radius  -- CCarAI::TellCarToFollowOtherCar(car, target, radius) [0x41C960]
void SetCarFollowCar(CVehicle* car, CVehicle* target, float radius) {
    CCarAI::TellCarToFollowOtherCar(car, target, radius);
}

//! 2043 SWITCH_ENTRY_EXIT (case @0x4730A8): label (8 chars, read FIRST), enable  -- CollectParameters(1) after the label
//! 0x43F9B0 => 0x43EF20(label, 0x4000 (bEnableAccess), enable != 0)
void SwitchEntryExit(CRunningScript& S) {
    char label[8 + 1]{}; // NOTSA: + terminator (the exe's buffer is 8 bytes and strncmp's at most 8 chars)
    S.ReadTextLabelFromScript(label, 8); // 0x463D50
    const auto enable = notsa::script::Read<int32>(&S); // CollectParameters(1)
    SetEntryExitFlagByName(label, 0x4000, enable != 0);
}
}; // namespace

void notsa::script::commands::ported::g20_21::RegisterG20a() {
    REGISTER_COMMAND_HANDLER_BEGIN("Ported g20a");

    REGISTER_COMMAND_HANDLER(COMMAND_GET_CURRENT_DAY_OF_WEEK, GetCurrentDayOfWeek);
    REGISTER_COMMAND_HANDLER(COMMAND_REGISTER_SCRIPT_BRAIN_FOR_CODE_USE, RegisterScriptBrainForCodeUse);
    REGISTER_COMMAND_HANDLER(COMMAND_APPLY_FORCE_TO_CAR, ApplyForceToCar);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_INT_LVAR_EQUAL_TO_INT_VAR, IsIntLvarEqualToIntVar);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_FLOAT_LVAR_EQUAL_TO_FLOAT_VAR, IsFloatLvarEqualToFloatVar);
    REGISTER_COMMAND_HANDLER(COMMAND_ADD_TO_CAR_ROTATION_VELOCITY, AddToCarRotationVelocity);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_CAR_ROTATION_VELOCITY, SetCarRotationVelocity);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_CHAR_SHOOT_RATE, SetCharShootRate);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_MODEL_IN_CDIMAGE, IsModelInCdimage);
    REGISTER_COMMAND_HANDLER(COMMAND_REMOVE_OIL_PUDDLES_IN_AREA, RemoveOilPuddlesInArea);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_BLIP_AS_FRIENDLY, SetBlipAsFriendly);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_SWIM_TO_COORD, TaskSwimToCoord);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_MODEL_DIMENSIONS, GetModelDimensions);
    REGISTER_COMMAND_HANDLER(COMMAND_COPY_CHAR_DECISION_MAKER, CopyCharDecisionMaker);
    REGISTER_COMMAND_HANDLER(COMMAND_COPY_GROUP_DECISION_MAKER, CopyGroupDecisionMaker);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_DRIVE_POINT_ROUTE_ADVANCED, TaskDrivePointRouteAdvanced);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_RELATIONSHIP_SET, IsRelationshipSet);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_CITY_FROM_COORDS, GetCityFromCoords);
    REGISTER_COMMAND_HANDLER(COMMAND_HAS_OBJECT_OF_TYPE_BEEN_SMASHED, HasObjectOfTypeBeenSmashed);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_CHECKPOINT_COORDS, SetCheckpointCoords);
    REGISTER_COMMAND_HANDLER(COMMAND_CONTROL_CAR_HYDRAULICS, ControlCarHydraulics);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_GROUP_SIZE, GetGroupSize);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_OBJECT_COLLISION_DAMAGE_EFFECT, SetObjectCollisionDamageEffect);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_CAR_FOLLOW_CAR, SetCarFollowCar);
    REGISTER_COMMAND_HANDLER(COMMAND_SWITCH_ENTRY_EXIT, SwitchEntryExit);
}

#include <StdInc.h>

#include "../Commands.hpp"
#include <CommandParser/Parser.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>

#include "World.h"
#include "Game.h"
#include "Messages.h"
#include "IplStore.h"
#include "Shopping.h"
#include "TagManager.h"
#include "SearchLight.h"
#include "FireManager.h"
#include "VehicleRecording.h"
#include "PedGroups.h"
#include "PedType.h"
#include "Acquaintance.h"
#include "Radar.h"
#include "WeaponInfo.h"
#include "Quaternion.h"
#include "Matrix.h"
#include "common.h"
#include "Audio/AudioEngine.h"
#include "Entity/Vehicle/Heli.h"
#include "Entity/Vehicle/Train.h"
#include "Entity/Vehicle/Automobile.h"
#include "Entity/Object/Object.h"
#include "Models/VehicleModelInfo.h"
#include "Tasks/PedScriptedTaskRecord.h"
#include "Tasks/TaskSequences.h"
#include "Tasks/TaskComplexSequence.h"
#include "Tasks/TaskComplexUseSequence.h"
#include "TaskTypes/TaskSimpleSetCharDecisionMaker.h"
#include "TaskTypes/TaskComplexClimb.h"
#include "TaskTypes/TaskComplexKillPedOnFoot.h"
#include "TaskTypes/TaskSimpleJetPack.h"
#include "TaskTypes/TaskComplexGoPickUpEntity.h"
#include "TaskTypes/TaskComplexSeekEntityAiming.h"
#include "TaskTypes/TaskComplexGoToPointAndStandStill.h"
#include "TaskTypes/TaskSimpleSlideToCoord.h"
#include "TaskTypes/SeekEntity/TaskComplexSeekEntityStandard.h"
#include "game_sa/DetachedShared.h"
#line 48

namespace notsa::script::commands::ported::g19 { void RegisterHandlers(); }

using namespace notsa::script;

/*!
* Script commands ported from the exe's per-100 group processor CRunningScript::ProcessCommands1900To1999
* (@0x46B460, jump table 0x46CBEC) for the vanilla commands that had no handler of their own:
* ids 1900..1999 (S6-E, group g19): 59 commands.
*
* Every handler below was written from the asm of the `case` in the group processor, NOT from the Ghidra decompilation.
* The case address is given in the comment above each handler (`ebp` / `[esp+0x15C]` hold the command id in the processor).
*
* Idioms: see Group05_08.cpp / Group16_17a.cpp (`_ftol2` = low dword of the int64 truncation, x87 compares keep their NaN behaviour,
* `GivePedScriptedTask(handle, task, cmd)` with handle -1 = add to the open task sequence).
*  - `CPed*` / `CVehicle*` / `CObject*` parser arguments are exactly `GetAtRef(handle)` (-1 -> null); where the exe tests
*    `handle >= 0` (signed) before the lookup and overwrites an earlier result even if the lookup yields null, the handle is read as int32.
*  - The exe's `test eax, eax` after `operator new` is only the allocation-failure check and is not kept.
*/

namespace {
static_assert(offsetof(CVehicle, m_autoPilot) + offsetof(CAutoPilot, m_nCarMission)   == 0x3BA);
static_assert(offsetof(CVehicle, m_autoPilot) + offsetof(CAutoPilot, m_TargetEntity)  == 0x41C);
static_assert(offsetof(CVehicle, m_autoPilot) + offsetof(CAutoPilot, m_nCruiseSpeed)  == 0x3D0);
static_assert(offsetof(CVehicle, m_autoPilot) + offsetof(CAutoPilot, m_ucHeliTargetDist) == 0x3D8);
static_assert(offsetof(CVehicle, m_autoPilot) + offsetof(CAutoPilot, m_ucHeliSpeedMult)  == 0x3D9);
static_assert(offsetof(CHeli, m_fMinAltitude) == 0x9B0);
static_assert(offsetof(CVehicle, m_pTowingVehicle) == 0x4C4 && offsetof(CVehicle, m_pVehicleBeingTowed) == 0x4C8);
static_assert(offsetof(CPed, m_acquaintance) == 0x4E0);
static_assert(offsetof(CWeaponInfo, m_nModelId1) == 0xC && offsetof(CWeaponInfo, m_nSlot) == 0x14);
static_assert(offsetof(CVehicleModelInfo, m_nPlateType) == 0x31 && offsetof(CVehicleModelInfo, m_pPlateMaterial) == 0x24);
static_assert(offsetof(tBeatInfo, IsBeatInfoPresent) == 0xA0 && offsetof(tBeatInfo, BeatNumber) == 0xA8 && sizeof(tTrackInfo::tBeat) == 8);
static_assert(offsetof(CTaskComplexUseSequence, m_nCurrentTaskIndex) == 0x10 && offsetof(CTaskComplexUseSequence, m_nEndTaskIndex) == 0x14);
static_assert(offsetof(CTaskComplexJump, m_UsePlayerLaunchForce) == 0x10);
static_assert(sizeof(CPedGroup) == 0x2D4);
static_assert(sizeof(tScriptSearchlight) == 0x7C);
static_assert(PEDMOVE_WALK == 4);
static_assert(MISSION_HELI_KEEP_ENTITY_IN_VIEW == 0x33 && MISSION_PLANE_CRASH_AND_BURN == 0x39 && MISSION_HELI_CRASH_AND_BURN == 0x3A);
static_assert(TASK_COMPLEX_USE_SEQUENCE == 0x113);
static_assert(ENTITY_TYPE_VEHICLE == 2 && ENTITY_TYPE_PED == 3 && ENTITY_TYPE_OBJECT == 4);

//! `_ftol2` (0x821B40): truncates to a 64 bit integer (NaN / out of range => the "integer indefinite" 0x8000000000000000);
//! the callers only use the LOW dword (EAX).
int32 Ftol(double v) {
    if (!(v > -9223372036854775808.0 && v < 9223372036854775808.0)) { // also catches NaN
        return 0;
    }
    return (int32)(int64)v;
}

//! 0xA43C78.. -> not used; 0x8D237C: set by SET_NEXT_DESIRED_MOVE_STATE, consumed (and reset to PEDMOVE_RUN) by TASK_ENTER_CAR_AS_*
eMoveState& NextDesiredMoveState() { return NOTSA_GLOBAL_EXPR(0x8D237C, (eMoveState), notsa::shared::NextDesiredMoveState); }

//! `abs` as the exe computes it (`cdq; xor; sub`): wraps for INT_MIN
int32 AbsWrap(int32 v) {
    const uint32 s = (uint32)(v >> 31);
    return (int32)(((uint32)v ^ s) - s);
}

//! `strncpy(dst, src, 8)` (0x821F40): copies at most 8 chars, pads with NULs, no terminator
void Strncpy8(char* dst, const char* src) {
    const size_t len = strnlen(src, 8);
    memcpy(dst, src, len);
    memset(dst + len, 0, 8 - len);
}

//! Inlined `CSearchLight` "is `entity` in any (active) script search light" (0x493960): fills the packed script id (id << 16 | slot) of the first light
//! that lights the entity's position. Out: -1 when there is none.
bool IsEntityInAnySearchlight(const CEntity& entity, int32& outId) {
    outId = -1;
    const CVector pos = entity.GetPosition(); // matrix ? matrix.pos (+0x30) : placement.pos (+4)
    for (int32 i = 0; i < (int32)CTheScripts::ScriptSearchLightArray.size(); i++) {
        auto& light = CTheScripts::ScriptSearchLightArray[i];
        if (light.m_bUsed && CSearchLight::IsPointInsideLitEllipse(pos, i)) { // 0x493280
            outId = (int32)(((uint32)(uint16)light.m_nId << 16) | (uint32)i);
            return true;
        }
    }
    return false;
}

//! The shared "entity type of the thing hanging on a winch / rope" => handles of (vehicle, ped, object); the other two stay -1.
MultiRet<int32, int32, int32> HandlesOfCarriedEntity(CEntity* e) {
    int32 veh = -1, ped = -1, obj = -1;
    if (e) {
        switch (e->GetType()) {
        case ENTITY_TYPE_VEHICLE: veh = GetVehiclePool()->GetRef(static_cast<CVehicle*>(e)); break; // 0x424160
        case ENTITY_TYPE_PED:     ped = GetPedPool()->GetRef(static_cast<CPed*>(e));         break; // 0x4442D0
        case ENTITY_TYPE_OBJECT:  obj = GetObjectPool()->GetRef(static_cast<CObject*>(e));   break; // 0x465070
        default: break;
        }
    }
    return { veh, ped, obj };
}

//! Shared tail of GET_OBJECT_QUATERNION / GET_VEHICLE_QUATERNION: entity matrix => 4 floats (x, y, z, w)
MultiRet<float, float, float, float> GetEntityQuaternion(CEntity& entity) {
    CQuaternion q;
    q.Set(*entity.GetModellingMatrix()); // 0x46A2D0 + 0x59C3E0 (no null check on the matrix)
    return { q.x, q.y, q.z, q.w };
}

//! Shared tail of SET_OBJECT_QUATERNION / SET_VEHICLE_QUATERNION: the rotation of the quaternion replaces the entity matrix,
//! the position is put back afterwards (`SetMatrix` 0x54F610, `SetPosn` 0x4241C0).
void SetEntityQuaternion(CEntity& entity, float x, float y, float z, float w) {
    const CQuaternion q{ x, y, z, w };
    const CVector     pos = entity.GetPosition();
    RwMatrix          rw{}; // the exe leaves the position / flags (stack garbage) as they are; the position is overwritten below
    q.Get(&rw);
    {
        CMatrix mat{ &rw, false };
        entity.SetMatrix(mat);
    }
    entity.SetPosn(pos);
}

// ============================================================================ g19 (ProcessCommands1900To1999 @0x46B460)

//! 1903 IS_MESSAGE_BEING_DISPLAYED (case @0x46B571): => compare flag: the first brief message slot (0xC1A7F0) has text
bool IsMessageBeingDisplayed() {
    return CMessages::BriefMessages[0].IsValid();
}

//! 1904 SET_CHAR_IS_TARGET_PRIORITY (case @0x46B597): char, flag. Third ped flag dword (+0x470) bit 29 (bThisPedIsATargetPriority) = flag != 0. No null check.
void SetCharIsTargetPriority(CPed& ped, int32 flag) {
    ped.bThisPedIsATargetPriority = (flag != 0);
}

//! 1905 CUSTOM_PLATE_DESIGN_FOR_NEXT_CAR (case @0x46B5E8): model, design
//! `ms_modelInfoPtrs[model]` (no range check): if it exists, is a vehicle model (type 6) and its plate material (+0x24, `m_pPlateMaterial`, the first member
//! of CVehicleModelInfo - NOT the RW object of CBaseModelInfo at +0x1C) is set => plate type byte (+0x31) = design.
void CustomPlateDesignForNextCar(int32 modelId, int32 design) {
    auto* const mi = CModelInfo::ms_modelInfoPtrs[modelId]; // 0xA9B0C8
    if (mi && mi->GetModelType() == MODEL_INFO_VEHICLE) {
        auto* const vmi = static_cast<CVehicleModelInfo*>(mi);
        if (vmi->m_pPlateMaterial) {
            vmi->m_nPlateType = (uint8)design;
        }
    }
}

//! 1906 TASK_GOTO_CAR (case @0x46B630): char, car, time, radius
//! time < 0 => 50000. CTaskComplexSeekEntity<Standard>(car, time, 1000, radius, 2.0f (0x859E30), 2.0f (0x859E34), true, true) [0x46AC10]
void TaskGotoCar(CRunningScript& S, eScriptCommands cmd, int32 pedHandle, CVehicle* veh, int32 time, float radius) {
    if (time < 0) {
        time = 50000; // 0xC350
    }
    S.GivePedScriptedTask(pedHandle, new CTaskComplexSeekEntityStandard{ veh, time, 1000, radius, 2.0f, 2.0f, true, true }, (int32)cmd);
}

//! 1910 REQUEST_IPL (case @0x46B6D4): name(18 read, 8 used) => `CIplStore::RequestIplAndIgnore(FindIplSlot(name))`
//! (the exe's 18 byte buffer is not initialised; a name without NUL terminator would read garbage - here the rest is zero)
void RequestIpl(CRunningScript& S) {
    char name[0x12]{};
    S.ReadTextLabelFromScript(name, sizeof(name));
    CIplStore::RequestIplAndIgnore(CIplStore::FindIplSlot(name)); // 0x404AC0, 0x405850
}

//! 1911 REMOVE_IPL (case @0x46B6FC): name => `RemoveIplAndIgnore(FindIplSlot(name))`
void RemoveIpl(CRunningScript& S) {
    char name[0x12]{};
    S.ReadTextLabelFromScript(name, sizeof(name));
    CIplStore::RemoveIplAndIgnore(CIplStore::FindIplSlot(name)); // 0x405890
}

//! 1912 REMOVE_IPL_DISCREETLY (case @0x46B724): name => `RemoveIplWhenFarAway(FindIplSlot(name))`
void RemoveIplDiscreetly(CRunningScript& S) {
    char name[0x12]{};
    S.ReadTextLabelFromScript(name, sizeof(name));
    CIplStore::RemoveIplWhenFarAway(CIplStore::FindIplSlot(name)); // 0x4058D0
}

//! 1914 SET_CHAR_RELATIONSHIP (case @0x46B74C): char, acquaintance id, ped type => `ped.m_acquaintance.SetAsAcquaintance(id, CPedType::GetPedFlag(type))` (0x608DA0). No null check.
void SetCharRelationship(CPed& ped, int32 acquaintanceId, int32 pedType) {
    ped.GetAcquaintance().SetAsAcquaintance(acquaintanceId, CPedType::GetPedFlag((ePedType)pedType));
}

//! 1915 CLEAR_CHAR_RELATIONSHIP (case @0x46B791): char, acquaintance id, ped type => `ClearAsAcquaintance(id, GetPedFlag(type))` (0x608980)
void ClearCharRelationship(CPed& ped, int32 acquaintanceId, int32 pedType) {
    ped.GetAcquaintance().ClearAsAcquaintance(acquaintanceId, CPedType::GetPedFlag((ePedType)pedType));
}

//! 1916 CLEAR_ALL_CHAR_RELATIONSHIPS (case @0x46B7D7): char, acquaintance id => `ClearAcquaintances(id)` (0x6089A0)
void ClearAllCharRelationships(CPed& ped, int32 acquaintanceId) {
    ped.GetAcquaintance().ClearAcquaintances(acquaintanceId);
}

//! 1917 GET_CAR_PITCH (case @0x46B80C): car => float (degrees)
//! `CAutomobile::GetCarPitch` (0x6A6050, called on any vehicle type) * 57.29578f (0x859878): the product is stored as a float (`fst`) but the first compare
//! (`fcomp 0.0f`, ordered `< 0`) is made on the x87 register (NOT rounded to float: a tiny negative product that rounds to -0.0f still adds); then
//! + 360.0f (0x859E2C) onto the stored float; then the float > 360 (ordered) => - 360.
float GetCarPitchScript(CVehicle& veh) {
    const double product = (double)static_cast<CAutomobile&>(veh).GetCarPitch() * (double)57.2957763671875f;
    float        pitch   = (float)product;
    if (product < 0.0) {
        pitch = (float)((double)pitch + 360.0);
    }
    if (pitch > 360.0f) {
        pitch = (float)((double)pitch - 360.0);
    }
    return pitch;
}

//! 1918 GET_AREA_VISIBLE (case @0x46B88C): => int (CGame::currArea, 0xB72914)
int32 GetAreaVisible() {
    return (int32)CGame::currArea;
}

//! 1920 HELI_KEEP_ENTITY_IN_VIEW (case @0x46B8A6): heli, char, car, a, b
//! target = char (when its handle >= 0) overwritten by the car (when its handle >= 0, even if it resolves to null). Unless the mission is
//! PLANE/HELI_CRASH_AND_BURN (0x39 / 0x3A) it becomes 0x33; the target entity reference (+0x41C) is replaced WITHOUT a null check on the new
//! target (a null target crashes in RegisterReference like the exe); +0x9B0 (CHeli::m_fMinAltitude) = b raw; cruise speed (+0x3D0) = 100;
//! +0x3D8 (heli target dist) = (uint8)_ftol(a).
void HeliKeepEntityInView(CVehicle& heli, int32 pedHandle, int32 vehHandle, float a, float b) {
    CEntity* target = nullptr;
    if (pedHandle >= 0) {
        target = GetPedPool()->GetAtRef(pedHandle);
    }
    if (vehHandle >= 0) {
        target = GetVehiclePool()->GetAtRef(vehHandle);
    }
    auto& ap = heli.m_autoPilot;
    if (ap.m_nCarMission != MISSION_PLANE_CRASH_AND_BURN && ap.m_nCarMission != MISSION_HELI_CRASH_AND_BURN) {
        ap.m_nCarMission = MISSION_HELI_KEEP_ENTITY_IN_VIEW;
    }
    if (ap.m_TargetEntity) {
        ap.m_TargetEntity->CleanUpOldReference((CEntity**)&ap.m_TargetEntity); // 0x571A00
    }
    ap.m_TargetEntity = static_cast<CVehicle*>(target);
    target->RegisterReference((CEntity**)&ap.m_TargetEntity); // 0x571B70
    static_cast<CHeli&>(heli).m_fMinAltitude = b;
    ap.m_nCruiseSpeed     = 100; // 0x64
    ap.m_ucHeliTargetDist = (uint8)Ftol((double)a);
}

//! 1921 GET_WEAPONTYPE_MODEL (case @0x46B960): weapon type => `CWeaponInfo::GetWeaponInfo(type, 1)->m_nModelId1` (+0xC)
int32 GetWeapontypeModel(int32 weaponType) {
    return CWeaponInfo::GetWeaponInfo((eWeaponType)weaponType, (eWeaponSkill)1)->m_nModelId1;
}

//! 1922 GET_WEAPONTYPE_SLOT (case @0x46B992): weapon type => `GetWeaponInfo(type, 1)->m_nSlot` (+0x14)
int32 GetWeapontypeSlot(int32 weaponType) {
    return CWeaponInfo::GetWeaponInfo((eWeaponType)weaponType, (eWeaponSkill)1)->m_nSlot;
}

//! 1923 GET_SHOPPING_EXTRA_INFO (case @0x46B9C5): item key, index => `CShopping::GetExtraInfo(key, index)` (0x49ADE0)
int32 GetShoppingExtraInfo(int32 itemKey, int32 index) {
    return CShopping::GetExtraInfo((uint32)itemKey, index);
}

//! 1926 GET_NUMBER_OF_FIRES_IN_AREA (case @0x46BA33): x1, y1, z1, x2, y2, z2 => int
//! Each coordinate pair is ordered (swapped when `a > b`, ordered compare), then `gFireManager.GetNumFiresInArea(x1, y1, z1, x2, y2, z2)` (0x539860).
int32 GetNumberOfFiresInArea(float x1, float y1, float z1, float x2, float y2, float z2) {
    if (x1 > x2) {
        std::swap(x1, x2);
    }
    if (y1 > y2) {
        std::swap(y1, y2);
    }
    if (z1 > z2) {
        std::swap(z1, z2);
    }
    return (int32)gFireManager.GetNumFiresInArea(x1, y1, z1, x2, y2, z2);
}

//! 1928 ATTACH_WINCH_TO_HELI (case @0x46BB10): heli, arg => `heli.InitWinch(arg)` (0x6D3B60). No null check.
void AttachWinchToHeli(CVehicle& heli, int32 arg) {
    heli.InitWinch(arg);
}

//! 1929 RELEASE_ENTITY_FROM_WINCH (case @0x46BB3F): heli => `ReleasePickedUpEntityWithWinch()` (0x6D3CB0). No null check.
void ReleaseEntityFromWinch(CVehicle& heli) {
    heli.ReleasePickedUpEntityWithWinch();
}

//! 1930 GET_TRAIN_CARRIAGE (case @0x46BB68): train, index (byte) => car handle (-1 when there is none)
//! `CTrain::FindCarriage(train, (uint8)index)` (0x6F5EB0), the result goes through the vehicle pool `GetRef`.
CVehicle* GetTrainCarriage(CVehicle* train, int32 index) {
    return CTrain::FindCarriage(static_cast<CTrain*>(train), (uint8)index);
}

//! 1931 GRAB_ENTITY_ON_WINCH (case @0x46BBD3): heli => 3 handles (car, char, object), -1 for those that don't apply
//! `QueryPickedUpEntityWithWinch()` (0x6D3CF0): entity type 2 => car, 3 => ped, 4 => object (others / none => all -1).
MultiRet<int32, int32, int32> GrabEntityOnWinch(CVehicle& heli) {
    return HandlesOfCarriedEntity(heli.QueryPickedUpEntityWithWinch());
}

//! 1932 GET_NAME_OF_ITEM (case @0x46BC30): item key, => string variable (8 bytes, `strncpy`)
//! `CShopping::GetNameTag(key)` (0x49ADA0); the destination (any kind of script variable) is fetched with `GetPointerToScriptVariable(VAR_GLOBAL)` AFTER the key.
void GetNameOfItem(CRunningScript& S, int32 itemKey) {
    const char* const tag = CShopping::GetNameTag((uint32)itemKey);
    char* const       dst = (char*)S.GetPointerToScriptVariable(VAR_GLOBAL);
    Strncpy8(dst, tag);
}

//! 1935 TASK_CLIMB (case @0x46BC65): char, flag => CTaskComplexClimb (0x46A630) with the byte at +0x10 (m_UsePlayerLaunchForce) = flag != 0
void TaskClimb(CRunningScript& S, eScriptCommands cmd, int32 pedHandle, int32 flag) {
    auto* const task = new CTaskComplexClimb{};
    task->m_UsePlayerLaunchForce = (flag != 0);
    S.GivePedScriptedTask(pedHandle, task, (int32)cmd);
}

//! 1936 BUY_ITEM (case @0x46BCC6): item key => `CShopping::Buy(key, 0)` (0x49BF70)
void BuyItem(int32 itemKey) {
    CShopping::Buy((uint32)itemKey, 0);
}

//! 1939 STORE_CLOTHES_STATE (case @0x46BD15): `CShopping::StoreClothesState()` (0x49B200)
void StoreClothesState() {
    CShopping::StoreClothesState();
}

//! 1940 RESTORE_CLOTHES_STATE (case @0x46BD21): `CShopping::RestoreClothesState()` (0x49B240)
void RestoreClothesState() {
    CShopping::RestoreClothesState();
}

//! 1942 GET_ROPE_HEIGHT_FOR_OBJECT (case @0x46BD2D): object => float (`CObject::GetRopeHeight`, 0x59F380). No null check.
float GetRopeHeightForObject(CObject& obj) {
    return obj.GetRopeHeight();
}

//! 1943 SET_ROPE_HEIGHT_FOR_OBJECT (case @0x46BD65): object, height => `SetRopeHeight` (0x59F3A0). No null check.
void SetRopeHeightForObject(CObject& obj, float height) {
    obj.SetRopeHeight(height);
}

//! 1944 GRAB_ENTITY_ON_ROPE_FOR_OBJECT (case @0x46BD94): object => 3 handles (car, char, object) as 1931 (`GetObjectCarriedWithRope`, 0x59F3C0)
MultiRet<int32, int32, int32> GrabEntityOnRopeForObject(CObject& obj) {
    return HandlesOfCarriedEntity(obj.GetObjectCarriedWithRope());
}

//! 1945 RELEASE_ENTITY_FROM_ROPE_FOR_OBJECT (case @0x46BE30): object => `ReleaseObjectCarriedWithRope()` (0x59F3E0). No null check.
void ReleaseEntityFromRopeForObject(CObject& obj) {
    obj.ReleaseObjectCarriedWithRope();
}

//! 1952 PERFORM_SEQUENCE_TASK_FROM_PROGRESS (case @0x46BEA0): char, sequence, progress, endIndex
//! idx = GetActualScriptThingIndex(sequence, SEQUENCE_TASK); nothing happens unless 0 <= idx < 64. CTaskComplexUseSequence(idx) (0x635450) with
//! m_nCurrentTaskIndex (+0x10) = progress and m_nEndTaskIndex (+0x14) = endIndex.
void PerformSequenceTaskFromProgress(CRunningScript& S, eScriptCommands cmd, int32 pedHandle, int32 sequenceId, int32 progress, int32 endIndex) {
    const int32 idx = CTheScripts::GetActualScriptThingIndex(sequenceId, SCRIPT_THING_SEQUENCE_TASK);
    if (idx < 0 || idx >= 64) {
        return;
    }
    auto* const task = new CTaskComplexUseSequence{ idx };
    task->m_nEndTaskIndex     = endIndex;
    task->m_nCurrentTaskIndex = progress;
    S.GivePedScriptedTask(pedHandle, task, (int32)cmd);
}

//! 1953 SET_NEXT_DESIRED_MOVE_STATE (case @0x46BF39): move state => stored to 0x8D237C (consumed by TASK_ENTER_CAR_AS_DRIVER/PASSENGER)
void SetNextDesiredMoveState(int32 moveState) {
    NextDesiredMoveState() = (eMoveState)moveState;
}

//! 1955 TASK_GOTO_CHAR_AIMING (case @0x46BF53): char, target char, seekRadius, aimRadius => CTaskComplexSeekEntityAiming(target, seekRadius, aimRadius) (0x694B90)
void TaskGotoCharAiming(CRunningScript& S, eScriptCommands cmd, int32 pedHandle, CPed* target, float seekRadius, float aimRadius) {
    S.GivePedScriptedTask(pedHandle, new CTaskComplexSeekEntityAiming{ target, seekRadius, aimRadius }, (int32)cmd);
}

//! 1956 GET_SEQUENCE_PROGRESS_RECURSIVE (case @0x46BFD8): char => 2 ints (progress of the sequence, progress of the nested sequence), -1 by default
//! When the scripted task record with opcode 0x618 (PERFORM_SEQUENCE_TASK) has a status other than 0: the primary task (TASK_PRIMARY_PRIMARY, +0x10 of
//! the intelligence) is read as a CTaskComplexUseSequence WITHOUT a type check => its current task index; the sub task, if it is a
//! TASK_COMPLEX_USE_SEQUENCE (0x113), gives the second value.
MultiRet<int32, int32> GetSequenceProgressRecursive(CPed& ped) {
    int32 progress = -1, subProgress = -1;
    if (CPedScriptedTaskRecord::GetStatus(&ped, 0x618) != eScriptedTaskStatus::EVENT_ASSOCIATED) {
        CTask* const task = ped.GetIntelligence()->GetTaskManager().GetPrimaryTasks()[TASK_PRIMARY_PRIMARY];
        progress = static_cast<CTaskComplexUseSequence*>(task)->m_nCurrentTaskIndex; // raw read of +0x10, no type check
        // virtual call through `CTask` (vtable slot 2): `CTaskComplex::GetSubTask` is `final` and would be bound statically through a CTaskComplexUseSequence*
        if (CTask* const sub = task->GetSubTask(); sub && sub->GetTaskType() == TASK_COMPLEX_USE_SEQUENCE) {
            subProgress = static_cast<CTaskComplexUseSequence*>(sub)->m_nCurrentTaskIndex;
        }
    }
    return { progress, subProgress };
}

//! 1957 TASK_KILL_CHAR_ON_FOOT_TIMED (case @0x46C04F): char, target, time => CTaskComplexKillPedOnFoot(target, time, 0, 0, 0, 1) (0x620E30)
void TaskKillCharOnFootTimed(CRunningScript& S, eScriptCommands cmd, int32 pedHandle, CPed* target, int32 time) {
    S.GivePedScriptedTask(pedHandle, new CTaskComplexKillPedOnFoot{ target, time, 0, 0, 0, 1 }, (int32)cmd);
}

//! 1958 GET_NEAREST_TAG_POSITION (case @0x46C0AF): x, y, z => 3 floats (position of the nearest tag entity, (-4000, -4000, -4000) (0xC57A0000) when none)
CVector GetNearestTagPosition(CVector pos) {
    if (const CEntity* const tag = CTagManager::GetNearestTag(pos)) { // 0x49D160
        return tag->GetPosition();
    }
    return { -4000.0f, -4000.0f, -4000.0f };
}

//! 1959 TASK_JETPACK (case @0x46C149): char => CTaskSimpleJetPack(nullptr, 10.0f, 0, nullptr) (0x67B4E0)
void TaskJetpack(CRunningScript& S, eScriptCommands cmd, int32 pedHandle) {
    S.GivePedScriptedTask(pedHandle, new CTaskSimpleJetPack{ nullptr, 10.0f, 0, nullptr }, (int32)cmd);
}

//! 1960 SET_AREA51_SAM_SITE (case @0x46C1A7): enabled => `CObject::bArea51SamSiteDisabled` (0xBB4A72) = (enabled == 0)
void SetArea51SamSite(int32 enabled) {
    CObject::bArea51SamSiteDisabled = (enabled == 0);
}

//! 1961 IS_CHAR_IN_ANY_SEARCHLIGHT (case @0x46C1C7): char => searchlight id (-1 when none), compare flag (found)
//! The id is stored BEFORE the compare flag is updated.
MultiRet<int32, bool> IsCharInAnySearchlight(CPed& ped) {
    int32      id    = -1;
    const bool found = IsEntityInAnySearchlight(ped, id); // 0x493960
    return { id, found };
}

//! 1963 IS_TRAILER_ATTACHED_TO_CAB (case @0x46C22E): trailer, cab => compare flag
//! trailer && cab: trailer.m_pTowingVehicle (+0x4C4) == cab; trailer only: it has a tractor; cab only: cab.m_pVehicleBeingTowed (+0x4C8) is set.
//! (BUG: both null dereferences the null cab, like the exe.)
bool IsTrailerAttachedToCab(CVehicle* trailer, CVehicle* cab) {
    if (trailer) {
        if (cab) {
            return trailer->m_pTowingVehicle == cab;
        }
        return trailer->m_pTowingVehicle != nullptr;
    }
    return cab->m_pVehicleBeingTowed != nullptr;
}

//! 1964 DETACH_TRAILER_FROM_CAB (case @0x46C2AF): trailer, cab
//! A trailer given => its virtual slot 62 (0xF8, `BreakTowLink`) is called (also when a cab is given); only a cab => the trailer it tows (+0x4C8), if any, breaks the link.
//! (BUG: both null dereferences the null cab, like the exe.)
void DetachTrailerFromCab(CVehicle* trailer, CVehicle* cab) {
    if (trailer) {
        trailer->BreakTowLink();
        return;
    }
    if (CVehicle* const towed = cab->m_pVehicleBeingTowed) {
        towed->BreakTowLink();
    }
}

//! 1968 GET_LOADED_SHOP (case @0x46C364): => string variable (8 bytes)
//! `strncpy(tmp, CShopping::ms_shopLoaded (0xA9A7D8), 8)`, MakeUpperCase(tmp) (0x718710), then `strncpy(var, tmp, 8)`; the variable is fetched with
//! `GetPointerToScriptVariable(VAR_GLOBAL)` AFTER the upper-casing (no parameters are collected).
void GetLoadedShop(CRunningScript& S) {
    char tmp[9]{}; // the exe's buffer has no terminator slot (an 8 character shop name would run past it)
    Strncpy8(tmp, StaticRef<char[24]>(0xA9A7D8)); // CShopping::ms_shopLoaded (private)
    MakeUpperCase(tmp);
    char* const dst = (char*)S.GetPointerToScriptVariable(VAR_GLOBAL);
    Strncpy8(dst, tmp);
}

//! 1969 GET_BEAT_PROXIMITY (case @0x46C3A2): offset => 3 ints
//! `AudioEngine.GetBeatInfo()` (0x5071B0). No beat info (IsBeatInfoPresent == 0) => (-1, -1, -1). Otherwise, for offset < 0 / > 0: the beat window entry
//! [offset + 10] / [offset + 9] (NO range check; 8 bytes each: time, key) and BeatNumber. For offset 0: the entry [10] is used unless it is
//! non-zero, BeatNumber is non-zero and |entry[10].time| > |entry[9].time|, then entry [9] and BeatNumber - 1 are used.
MultiRet<int32, int32, int32> GetBeatProximity(int32 offset) {
    tBeatInfo* const info = AudioEngine.GetBeatInfo();
    if (offset != 0) {
        const int32 idx = offset < 0 ? offset + 10 : offset + 9;
        if (!info->IsBeatInfoPresent) {
            return { -1, -1, -1 };
        }
        const int32* const entry = reinterpret_cast<const int32*>(info) + idx * 2;
        return { entry[0], entry[1], info->BeatNumber };
    }
    if (!info->IsBeatInfoPresent) {
        return { -1, -1, -1 };
    }
    const int32 time9  = (int32)info->BeatWindow[9].m_nTime;
    const int32 key9   = (int32)info->BeatWindow[9].m_nKey;
    const int32 time10 = (int32)info->BeatWindow[10].m_nTime;
    const int32 key10  = (int32)info->BeatWindow[10].m_nKey;
    if (time10 != 0 && info->BeatNumber != 0 && AbsWrap(time10) > AbsWrap(time9)) {
        return { time9, key9, info->BeatNumber - 1 };
    }
    return { time10, key10, info->BeatNumber };
}

//! 1971 SET_GROUP_DEFAULT_TASK_ALLOCATOR (case @0x46C4B7): group, allocator type
//! idx = GetActualScriptThingIndex(group, PED_GROUP); only 0 <= idx < 8 does anything: `ms_groups[idx].GetIntelligence().SetDefaultTaskAllocatorType(type)` (0x5FBB70).
void SetGroupDefaultTaskAllocator(int32 groupId, int32 allocatorType) {
    const int32 idx = CTheScripts::GetActualScriptThingIndex(groupId, SCRIPT_THING_PED_GROUP);
    if (idx < 0 || idx >= 8) {
        return;
    }
    CPedGroups::ms_groups[idx].GetIntelligence().SetDefaultTaskAllocatorType((ePedGroupDefaultTaskAllocatorType)allocatorType);
}

//! 1979 ACTIVATE_HELI_SPEED_CHEAT (case @0x46C552): heli, value => byte at +0x3D9 (m_ucHeliSpeedMult) = value (low byte). No null check.
void ActivateHeliSpeedCheat(CVehicle& heli, int32 value) {
    heli.m_autoPilot.m_ucHeliSpeedMult = (uint8)value;
}

//! 1980 TASK_SET_CHAR_DECISION_MAKER (case @0x46C580): char, decision maker
//! dm = (id == -1) ? -1 : GetActualScriptThingIndex(id, DECISION_MAKER). Char handle != -1 => `ped.GetIntelligence()->SetPedDecisionMakerType(dm)` (0x600B50),
//! no null check; -1 => CTaskSimpleSetCharDecisionMaker(dm) (0x46A470) is added to the open sequence.
void TaskSetCharDecisionMaker(CRunningScript& S, eScriptCommands cmd, int32 pedHandle, int32 decisionMakerId) {
    int32 dm = -1;
    if (decisionMakerId != -1) {
        dm = CTheScripts::GetActualScriptThingIndex(decisionMakerId, SCRIPT_THING_DECISION_MAKER);
    }
    if (pedHandle != -1) {
        GetPedPool()->GetAtRef(pedHandle)->GetIntelligence()->SetPedDecisionMakerType(dm);
        return;
    }
    S.GivePedScriptedTask(-1, new CTaskSimpleSetCharDecisionMaker{ (uint32)dm }, (int32)cmd);
}

//! 1981 DELETE_MISSION_TRAIN (case @0x46C615): train => `CTrain::RemoveOneMissionTrain` (0x6F5DC0); a null train does nothing
void DeleteMissionTrain(CVehicle* train) {
    if (train) {
        CTrain::RemoveOneMissionTrain(static_cast<CTrain*>(train));
    }
}

//! 1982 MARK_MISSION_TRAIN_AS_NO_LONGER_NEEDED (case @0x46C647): train => `CTrain::ReleaseOneMissionTrain` (0x6F5DF0); a null train does nothing
void MarkMissionTrainAsNoLongerNeeded(CVehicle* train) {
    if (train) {
        CTrain::ReleaseOneMissionTrain(static_cast<CTrain*>(train));
    }
}

//! 1983 SET_BLIP_ALWAYS_DISPLAY_ON_ZOOMED_RADAR (case @0x46C67A): blip, flag (low byte)
//! `CRadar::SetBlipAlwaysDisplayInZoom(blip, byte)` (0x583DB0) - the callee only uses bit 0 of the byte.
void SetBlipAlwaysDisplayOnZoomedRadar(int32 blip, int32 flag) {
    CRadar::SetBlipAlwaysDisplayInZoom(blip, (flag & 1) != 0);
}

//! 1984 REQUEST_CAR_RECORDING (case @0x46C6A1): file number => `CVehicleRecording::RequestRecordingFile` (0x45A020)
void RequestCarRecording(int32 fileNumber) {
    CVehicleRecording::RequestRecordingFile(fileNumber);
}

//! 1985 HAS_CAR_RECORDING_BEEN_LOADED (case @0x46C6C0): file number => compare flag (0x45A060)
bool HasCarRecordingBeenLoaded(int32 fileNumber) {
    return CVehicleRecording::HasRecordingFileBeenLoaded(fileNumber);
}

//! 1987 GET_OBJECT_QUATERNION (case @0x46C6E4): object => 4 floats (x, y, z, w) of `CQuaternion::Set(*obj.GetModellingMatrix())` (0x59C3E0)
MultiRet<float, float, float, float> GetObjectQuaternion(CObject& obj) {
    return GetEntityQuaternion(obj);
}

//! 1988 SET_OBJECT_QUATERNION (case @0x46C747): object, x, y, z, w => the matrix gets the quaternion's rotation (0x59C080), the position is kept
void SetObjectQuaternion(CObject& obj, float x, float y, float z, float w) {
    SetEntityQuaternion(obj, x, y, z, w);
}

//! 1989 GET_VEHICLE_QUATERNION (case @0x46C7F7): car => 4 floats (as 1987)
MultiRet<float, float, float, float> GetVehicleQuaternion(CVehicle& veh) {
    return GetEntityQuaternion(veh);
}

//! 1990 SET_VEHICLE_QUATERNION (case @0x46C85A): car, x, y, z, w (as 1988)
void SetVehicleQuaternion(CVehicle& veh, float x, float y, float z, float w) {
    SetEntityQuaternion(veh, x, y, z, w);
}

//! 1991 SET_MISSION_TRAIN_COORDINATES (case @0x46C928): train, x, y, z => `CTrain::SetNewTrainPosition(train, pos)` (0x6F7140); the train is passed on even if null
void SetMissionTrainCoordinates(CVehicle* train, CVector pos) {
    CTrain::SetNewTrainPosition(static_cast<CTrain*>(train), pos);
}

//! 1993 TASK_COMPLEX_PICKUP_OBJECT (case @0x46C988): char, object => CTaskComplexGoPickUpEntity(object, 0x51) (0x6919C0)
void TaskComplexPickupObject(CRunningScript& S, eScriptCommands cmd, int32 pedHandle, CObject* obj) {
    S.GivePedScriptedTask(pedHandle, new CTaskComplexGoPickUpEntity{ obj, (AssocGroupId)0x51 }, (int32)cmd);
}

//! 1995 LISTEN_TO_PLAYER_GROUP_COMMANDS (case @0x46C9F2): char, listen
//! Third ped flag dword (+0x474) bit 5 (bDoesntListenToPlayerGroupCommands) = (listen == 0). No null check.
void ListenToPlayerGroupCommands(CPed& ped, int32 listen) {
    ped.bDoesntListenToPlayerGroupCommands = (listen == 0);
}

//! 1997 TASK_CHAR_SLIDE_TO_COORD (case @0x46CA88): char, x, y, z, heading, speed
//! angle = (float)(heading * 0.017453292f (0x8595EC)); speed < 0 (ordered) => 0.1f. A CTaskComplexSequence of
//! CTaskComplexGoToPointAndStandStill(PEDMOVE_WALK (4), pos, 0.5f (0x86FC84), 2.0f (0x86FC88), false, false) (0x668120) and
//! CTaskSimpleSlideToCoord(pos, angle, speed) (0x66C3E0).
void TaskCharSlideToCoord(CRunningScript& S, eScriptCommands cmd, int32 pedHandle, float x, float y, float z, float heading, float speed) {
    const CVector pos{ x, y, z };
    const float   angle = (float)((double)heading * (double)0.017453292f);
    if (speed < 0.0f) {
        speed = 0.1f; // 0x3DCCCCCD
    }
    auto* const seq = new CTaskComplexSequence{};
    seq->AddTask(new CTaskComplexGoToPointAndStandStill{ PEDMOVE_WALK, pos, 0.5f, 2.0f, false, false });
    seq->AddTask(new CTaskSimpleSlideToCoord{ pos, angle, speed });
    S.GivePedScriptedTask(pedHandle, seq, (int32)cmd);
}
} // namespace

void notsa::script::commands::ported::g19::RegisterHandlers() {
    REGISTER_COMMAND_HANDLER_BEGIN("Ported g19");

    REGISTER_COMMAND_HANDLER(COMMAND_IS_MESSAGE_BEING_DISPLAYED, IsMessageBeingDisplayed);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_CHAR_IS_TARGET_PRIORITY, SetCharIsTargetPriority);
    REGISTER_COMMAND_HANDLER(COMMAND_CUSTOM_PLATE_DESIGN_FOR_NEXT_CAR, CustomPlateDesignForNextCar);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_GOTO_CAR, TaskGotoCar);
    REGISTER_COMMAND_HANDLER(COMMAND_REQUEST_IPL, RequestIpl);
    REGISTER_COMMAND_HANDLER(COMMAND_REMOVE_IPL, RemoveIpl);
    REGISTER_COMMAND_HANDLER(COMMAND_REMOVE_IPL_DISCREETLY, RemoveIplDiscreetly);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_CHAR_RELATIONSHIP, SetCharRelationship);
    REGISTER_COMMAND_HANDLER(COMMAND_CLEAR_CHAR_RELATIONSHIP, ClearCharRelationship);
    REGISTER_COMMAND_HANDLER(COMMAND_CLEAR_ALL_CHAR_RELATIONSHIPS, ClearAllCharRelationships);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_CAR_PITCH, GetCarPitchScript);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_AREA_VISIBLE, GetAreaVisible);
    REGISTER_COMMAND_HANDLER(COMMAND_HELI_KEEP_ENTITY_IN_VIEW, HeliKeepEntityInView);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_WEAPONTYPE_MODEL, GetWeapontypeModel);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_WEAPONTYPE_SLOT, GetWeapontypeSlot);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_SHOPPING_EXTRA_INFO, GetShoppingExtraInfo);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_NUMBER_OF_FIRES_IN_AREA, GetNumberOfFiresInArea);
    REGISTER_COMMAND_HANDLER(COMMAND_ATTACH_WINCH_TO_HELI, AttachWinchToHeli);
    REGISTER_COMMAND_HANDLER(COMMAND_RELEASE_ENTITY_FROM_WINCH, ReleaseEntityFromWinch);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_TRAIN_CARRIAGE, GetTrainCarriage);
    REGISTER_COMMAND_HANDLER(COMMAND_GRAB_ENTITY_ON_WINCH, GrabEntityOnWinch);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_NAME_OF_ITEM, GetNameOfItem);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_CLIMB, TaskClimb);
    REGISTER_COMMAND_HANDLER(COMMAND_BUY_ITEM, BuyItem);
    REGISTER_COMMAND_HANDLER(COMMAND_STORE_CLOTHES_STATE, StoreClothesState);
    REGISTER_COMMAND_HANDLER(COMMAND_RESTORE_CLOTHES_STATE, RestoreClothesState);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_ROPE_HEIGHT_FOR_OBJECT, GetRopeHeightForObject);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_ROPE_HEIGHT_FOR_OBJECT, SetRopeHeightForObject);
    REGISTER_COMMAND_HANDLER(COMMAND_GRAB_ENTITY_ON_ROPE_FOR_OBJECT, GrabEntityOnRopeForObject);
    REGISTER_COMMAND_HANDLER(COMMAND_RELEASE_ENTITY_FROM_ROPE_FOR_OBJECT, ReleaseEntityFromRopeForObject);
    REGISTER_COMMAND_HANDLER(COMMAND_PERFORM_SEQUENCE_TASK_FROM_PROGRESS, PerformSequenceTaskFromProgress);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_NEXT_DESIRED_MOVE_STATE, SetNextDesiredMoveState);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_GOTO_CHAR_AIMING, TaskGotoCharAiming);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_SEQUENCE_PROGRESS_RECURSIVE, GetSequenceProgressRecursive);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_KILL_CHAR_ON_FOOT_TIMED, TaskKillCharOnFootTimed);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_NEAREST_TAG_POSITION, GetNearestTagPosition);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_JETPACK, TaskJetpack);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_AREA51_SAM_SITE, SetArea51SamSite);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_CHAR_IN_ANY_SEARCHLIGHT, IsCharInAnySearchlight);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_TRAILER_ATTACHED_TO_CAB, IsTrailerAttachedToCab);
    REGISTER_COMMAND_HANDLER(COMMAND_DETACH_TRAILER_FROM_CAB, DetachTrailerFromCab);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_LOADED_SHOP, GetLoadedShop);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_BEAT_PROXIMITY, GetBeatProximity);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_GROUP_DEFAULT_TASK_ALLOCATOR, SetGroupDefaultTaskAllocator);
    REGISTER_COMMAND_HANDLER(COMMAND_ACTIVATE_HELI_SPEED_CHEAT, ActivateHeliSpeedCheat);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_SET_CHAR_DECISION_MAKER, TaskSetCharDecisionMaker);
    REGISTER_COMMAND_HANDLER(COMMAND_DELETE_MISSION_TRAIN, DeleteMissionTrain);
    REGISTER_COMMAND_HANDLER(COMMAND_MARK_MISSION_TRAIN_AS_NO_LONGER_NEEDED, MarkMissionTrainAsNoLongerNeeded);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_BLIP_ALWAYS_DISPLAY_ON_ZOOMED_RADAR, SetBlipAlwaysDisplayOnZoomedRadar);
    REGISTER_COMMAND_HANDLER(COMMAND_REQUEST_CAR_RECORDING, RequestCarRecording);
    REGISTER_COMMAND_HANDLER(COMMAND_HAS_CAR_RECORDING_BEEN_LOADED, HasCarRecordingBeenLoaded);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_OBJECT_QUATERNION, GetObjectQuaternion);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_OBJECT_QUATERNION, SetObjectQuaternion);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_VEHICLE_QUATERNION, GetVehicleQuaternion);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_VEHICLE_QUATERNION, SetVehicleQuaternion);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_MISSION_TRAIN_COORDINATES, SetMissionTrainCoordinates);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_COMPLEX_PICKUP_OBJECT, TaskComplexPickupObject);
    REGISTER_COMMAND_HANDLER(COMMAND_LISTEN_TO_PLAYER_GROUP_COMMANDS, ListenToPlayerGroupCommands);
    REGISTER_COMMAND_HANDLER(COMMAND_TASK_CHAR_SLIDE_TO_COORD, TaskCharSlideToCoord);
}

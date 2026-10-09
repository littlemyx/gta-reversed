#include <StdInc.h>

#include "../Commands.hpp"
#include <CommandParser/Parser.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <algorithm>

#include "World.h"
#include "Radar.h"
#include "Pickups.h"
#include "Explosion.h"
#include "Weapon.h"
#include "Audio/AudioEngine.h"
#include "CutsceneMgr.h"
#include "Timer.h"
#include "Camera.h"
#include "Garages.h"
#include "TaskSimpleStandStill.h"
#include "Stats.h"
#include "UserDisplay.h"
#include "OnscreenTimer.h"
#include "VisibilityPlugins.h"
#include "VehicleRecording.h"
#include "PathFind.h"
#include "General.h"
#include "PedGroups.h"
#include "PedGroup.h"
#include "Tasks/TaskSequences.h"
#include "Tasks/TaskComplexUseSequence.h"
#include "Tasks/TaskTypes/TaskComplexBeInGroup.h"
#include "Events/EventScriptCommand.h"
#include "Scripts/Scripted2dEffects.h"
#include "MissionCleanup.h"
#include "Tasks/PedScriptedTaskRecord.h"
#include "ModelIndices.h"
#include "CopPed.h"
#include "EmergencyPed.h"
#include "CivilianPed.h"
#include "Population.h"
#include "Attractors/PedAttractorPedPlacer.h"
#include "Tasks/TaskTypes/TaskComplexWanderStandard.h"
#include "Tasks/TaskTypes/TaskComplexWanderCriminal.h"
#include "Tasks/TaskTypes/TaskComplexUseEffect.h"
#include "Tasks/TaskTypes/TaskComplexEnterCarAsPassenger.h"
#include "Tasks/TaskTypes/TaskSimpleCarDrive.h"
#include "Tasks/TaskComplexSequence.h"
#include "Events/EventGroupEvent.h"
#include "Events/EventLeaderEnteredCarAsDriver.h"
#include "PedGroupIntelligence.h"

using namespace notsa::script;

/*!
* Script commands ported from the exe's group processors for the vanilla commands that had no handler of their own:
* ids 1300..1599 (S6-C, groups g13..g15, everything except the TASK_* block that lives in Group13_15_task.cpp).
*
* NOTE: the exe's processors are NOT aligned to ids/100: g13 (0x48CDD0, table 0x8A6168 + 4 * 13) covers ids 1301..1397,
* g14 (0x48EAA0) covers 1403..1499 and g15 (0x490DB0) covers 1500..1593.
*
* Every handler below was written from the asm of the `case` in the group processor, NOT from the Ghidra decompilation.
* The case address is given in the comment above each handler.
*
* Idioms: `fcom/fcomp x; fnstsw; test ah, 0x41; jp skip` => the condition is taken for an ordered `<=`/`==` (unordered => skip).
* `GetAtRef` results of the vehicle / ped / object pools are used without a null check in most cases (the exe would crash);
* the parser's `CVehicle&` / `CPed&` / `CObject&` arguments are exactly that (`*GetAtRef(h)`).
*/

namespace {
static_assert(offsetof(CPhysical, m_nPhysicalFlags) == 0x40);
static_assert(offsetof(CVehicle, vehicleFlags) == 0x428);
static_assert(offsetof(CVehicle, m_pLastDamageEntity) == 0x50C);
static_assert(offsetof(CVehicle, m_nGunFiringTime) == 0x4D4);
static_assert(offsetof(CVehicle, m_autoPilot) + offsetof(CAutoPilot, m_nCarMission) == 0x3BA);
static_assert(offsetof(CPhysical, m_nFakePhysics) == 0xB8);
static_assert(offsetof(CEntity, m_nFlags) == 0x1C);
static_assert(offsetof(CPed, m_pVehicle) == 0x58C);
static_assert(offsetof(CRunningScript, m_szName) == 0x8);
static_assert(BLIP_CONTACT_POINT == 5);

//! x87-compare idiom `fcom -100.0f; test ah, 0x41; jp skip`: the ground Z is looked up only when `z <= -100.0f` (ordered).
float GroundZIfAuto(float x, float y, float z) {
    if (z <= -100.0f) { // 0x859014
        return CWorld::FindGroundZForCoord(x, y);
    }
    return z;
}

//! Same as above, but followed by `fadd 0.5f` (0x858B8C) on the looked up Z (the `fstp` after it stores a float)
float GroundZIfAutoPlusHalf(float x, float y, float z) {
    if (z <= -100.0f) {
        return (float)((double)CWorld::FindGroundZForCoord(x, y) + 0.5);
    }
    return z;
}

//! 0x59C890 - the original evaluation order; the sum stays in the FPU registers (extended precision), stored as float
CVector TransformPointOriginal(const CMatrix& m, const CVector& v) {
    const auto &r = m.GetRight(), &f = m.GetForward(), &u = m.GetUp(), &p = m.GetPosition();
    return CVector{
        (float)((((double)u.x * v.z + (double)f.x * v.y) + (double)r.x * v.x) + p.x),
        (float)((((double)u.y * v.z + (double)r.y * v.x) + (double)f.y * v.y) + p.y),
        (float)((((double)u.z * v.z + (double)r.z * v.x) + (double)f.z * v.y) + p.z)
    };
}

// ============================================================================ g13 (ProcessCommands1300To1399 @0x48CDD0)

//! 1303 CREATE_LOCKED_PROPERTY_PICKUP (case @0x48CDF9): x, y, z, label(8) => 1 handle
//! z <= -100 => ground Z + 0.5f. Model = MI_PICKUP_PROPERTY (word @0x8CD5E8), type 0x11 (PICKUP_PROPERTY_LOCKED), ammo 0,
//! text = TheText.Get(label). The dead GetActualPickupIndex(CollectNextParameterWithoutIncreasingPC()) call is kept.
int32 CreateLockedPropertyPickup(CRunningScript& S, CVector pos, const char* label) {
    pos.z = GroundZIfAutoPlusHalf(pos.x, pos.y, pos.z);
    (void)TheText.Get(label); // the result is unused by the exe: GenerateNewOne gets the label buffer itself (CPickup::FindTextIndexForString compares the key)
    CPickups::GetActualPickupIndex(tPickupReference{ S.CollectNextParameterWithoutIncreasingPC() });
    return CPickups::GenerateNewOne(pos, (uint32)(uint16)ModelIndices::MI_PICKUP_PROPERTY, PICKUP_PROPERTY_LOCKED, 0, 0, false, const_cast<char*>(label)).num;
}

//! 1304 CREATE_FORSALE_PROPERTY_PICKUP (case @0x48CEDB): x, y, z, price, label(8) => 1 handle
//! Same as above, model MI_PICKUP_PROPERTY_FORSALE (word @0x8CD5EC), type 0x12 (PICKUP_PROPERTY_FORSALE), ammo = price.
int32 CreateForsalePropertyPickup(CRunningScript& S, CVector pos, int32 price, const char* label) {
    pos.z = GroundZIfAutoPlusHalf(pos.x, pos.y, pos.z);
    (void)TheText.Get(label); // unused result, see above
    CPickups::GetActualPickupIndex(tPickupReference{ S.CollectNextParameterWithoutIncreasingPC() });
    return CPickups::GenerateNewOne(pos, (uint32)(uint16)ModelIndices::MI_PICKUP_PROPERTY_FORSALE, PICKUP_PROPERTY_FORSALE, (uint32)price, 0, false, const_cast<char*>(label)).num;
}

//! 1305 FREEZE_CAR_POSITION (case @0x48CFA5): car, freeze. No null check on the car.
//! freeze: physicalFlags |= 0x200C (bit2 | bit3 | bit13), SkipPhysics, move speed = 0, turn speed = 0 (0x441130 / 0x45AFB0 with a zero vector).
//! else: physicalFlags &= ~0x200C.
void FreezeCarPosition(CVehicle& veh, int32 freeze) {
    if (freeze) {
        veh.m_nPhysicalFlags |= 0x200C;
        veh.SkipPhysics();
        veh.m_vecMoveSpeed = CVector{ 0.0f, 0.0f, 0.0f };
        veh.m_vecTurnSpeed = CVector{ 0.0f, 0.0f, 0.0f };
    } else {
        veh.m_nPhysicalFlags &= 0xFFFFDFF3u;
    }
}

//! 1308 HAS_CAR_BEEN_DAMAGED_BY_CHAR (case @0x48D0D2): car, char => compare flag
//! The ped is looked up only if the handle isn't -1. false if the car doesn't exist / has no last damage entity.
//! handle == -1: the last damage entity is any ped (type 3). Otherwise: it is that ped, or (the ped is bInVehicle [+0x46C bit 8] and it is
//! the ped's vehicle @+0x58C). The ped pointer is dereferenced unchecked (exe crash on a stale handle).
bool HasCarBeenDamagedByChar(CVehicle* veh, int32 pedHandle) {
    CPed* ped = nullptr;
    if (pedHandle != -1) {
        ped = GetPedPool()->GetAtRef(pedHandle);
    }
    if (!veh) {
        return false;
    }
    CEntity* const last = veh->m_pLastDamageEntity;
    if (!last) {
        return false;
    }
    if (pedHandle == -1) {
        return last->GetType() == ENTITY_TYPE_PED;
    }
    bool result = false;
    if (last == ped) {
        result = true;
    }
    if (ped->bInVehicle && last == (CEntity*)ped->m_pVehicle) { // dword @+0x46C (first ped flags), `test dh, 1` = bit 8 = bInVehicle
        result = true;
    }
    return result;
}

//! 1309 HAS_CAR_BEEN_DAMAGED_BY_CAR (case @0x48D15B): car, car2 => compare flag
//! handle == -1: the last damage entity is any vehicle (type 2); otherwise it's that vehicle (the pool result, maybe null)
bool HasCarBeenDamagedByCar(CVehicle* veh, int32 otherHandle) {
    CVehicle* other = nullptr;
    if (otherHandle != -1) {
        other = GetVehiclePool()->GetAtRef(otherHandle);
    }
    if (!veh) {
        return false;
    }
    CEntity* const last = veh->m_pLastDamageEntity;
    if (!last) {
        return false;
    }
    if (otherHandle == -1) {
        return last->GetType() == ENTITY_TYPE_VEHICLE;
    }
    return last == other;
}

//! 1310 GET_RADIO_CHANNEL (case @0x48D1C6): => 1 int. `(int8)GetCurrentRadioStationID() - 1` (movsx; dec)
int32 GetRadioChannel() {
    return (int32)(int8)AudioEngine.GetCurrentRadioStationID() - 1;
}

//! 1342 GET_RANDOM_CAR_OF_TYPE_IN_AREA_NO_SAVE (case @0x48D278): x1, y1, x2, y2, model => 1 handle (-1 if none)
//! Walks the vehicle pool from the last slot down and takes the first vehicle that is an automobile / bike (appearance 1 or 2),
//! whose model index (int16) is `model` or `model < 0`, that CanBeDeleted(), and IsWithinArea(x1,y1,x2,y2). The vehicle gets
//! vehicleFlags (+0x42A mask 0x02 = bHasBeenOwnedByPlayer) set - but is NOT registered as a mission entity ("no save").
int32 GetRandomCarOfTypeInAreaNoSave(float x1, float y1, float x2, float y2, int32 model) {
    auto&  pool   = *GetVehiclePool();
    int32  result = -1;
    for (auto i = (int32)pool.GetSize(); i != 0;) {
        if (result != -1) {
            break;
        }
        --i;
        CVehicle* const veh = pool.GetAt(i);
        if (!veh) {
            continue;
        }
        const auto app = veh->GetVehicleAppearance();
        if (app != VEHICLE_APPEARANCE_AUTOMOBILE && app != VEHICLE_APPEARANCE_BIKE) {
            continue;
        }
        if ((int16)veh->m_nModelIndex != model && model >= 0) {
            continue;
        }
        if (!veh->CanBeDeleted()) {
            continue;
        }
        if (!veh->IsWithinArea(x1, y1, x2, y2)) {
            continue;
        }
        result = pool.GetRef(veh);
        veh->vehicleFlags.bHasBeenOwnedByPlayer = true; // byte @+0x42A |= 2
    }
    return result;
}

//! 1343 SET_CAN_BURST_CAR_TYRES (case @0x48D356): car, canBurst. byte @+0x42B bit 0x80 (bTyresDontBurst) = !canBurst. No null check.
void SetCanBurstCarTyres(CVehicle& veh, int32 canBurst) {
    veh.vehicleFlags.bTyresDontBurst = (canBurst == 0);
}

//! 1345 FIRE_HUNTER_GUN (case @0x48D3AA): heli (no null check)
//! At most every 0x96 ms (unsigned compare `time > m_nGunFiringTime(+0x4D4) + 0x96`): CWeapon(WEAPON_MINIGUN, 5000);
//! origin = TransformPoint(matrix, const vec @0x8D3394) + moveSpeed * timestep; FireInstantHit(heli, &origin, &origin, 0,0,0, false, true);
//! AddGunshell(heli, origin, (0, 0.1f), 0.025f); ReportWeaponEvent(AE_WEAPON_FIRE_PLANE=0x95, WEAPON_MINIGUN, heli); m_nGunFiringTime = time.
void FireHunterGun(CVehicle& veh) {
    if (!(CTimer::m_snTimeInMilliseconds > veh.m_nGunFiringTime + 0x96u)) {
        return;
    }
    CWeapon       weapon{ WEAPON_MINIGUN, 5000 };
    const CVector muzzleLocal = StaticRef<CVector>(0x8D3394);
    const CVector step        = veh.m_vecMoveSpeed * CTimer::ms_fTimeStep; // 0x40FEC0
    const CVector t           = TransformPointOriginal(veh.GetMatrix(), muzzleLocal); // 0x59C890
    CVector       origin      = t + step;                                   // 0x40FE30
    weapon.FireInstantHit(&veh, &origin, &origin, nullptr, nullptr, nullptr, false, true);
    const CVector2D dir{ 0.0f, 0.1f }; // 0x3DCCCCCD
    weapon.AddGunshell(&veh, origin, dir, 0.025f); // 0x3CCCCCCD
    AudioEngine.ReportWeaponEvent(AE_WEAPON_FIRE_PLANE, WEAPON_MINIGUN, &veh);
    veh.m_nGunFiringTime = CTimer::m_snTimeInMilliseconds;
}

//! 1359 CLEAR_CAR_LAST_DAMAGE_ENTITY (case @0x48D5E8): car. null car skipped; m_pLastDamageEntity (+0x50C) = null
void ClearCarLastDamageEntity(CVehicle* veh) {
    if (veh) {
        veh->m_pLastDamageEntity = nullptr;
    }
}

//! 1360 FREEZE_OBJECT_POSITION (case @0x48D621): object, freeze. No null check. physicalFlags |= / &= ~0x2004
void FreezeObjectPosition(CObject& obj, int32 freeze) {
    if (freeze) {
        obj.m_nPhysicalFlags |= 0x2004;
    } else {
        obj.m_nPhysicalFlags &= 0xFFFFDFFBu;
    }
}

//! 1365 REMOVE_WEAPON_FROM_CHAR (case @0x48D673): char, weaponType. No null check: `ped->ClearWeapon(type)` (0x5E62B0)
void RemoveWeaponFromChar(CPed& ped, int32 weaponType) {
    ped.ClearWeapon((eWeaponType)weaponType);
}

//! 1380 MAKE_HELI_COME_CRASHING_DOWN (case @0x48D903): heli (no null check)
//! If the car mission (+0x3BA) is neither 0x39 (PLANE_CRASH_AND_BURN) nor 0x3A (HELI_CRASH_AND_BURN) it is set to 0x3A.
void MakeHeliComeCrashingDown(CVehicle& veh) {
    const auto mission = veh.m_autoPilot.m_nCarMission;
    if (mission == MISSION_PLANE_CRASH_AND_BURN || mission == MISSION_HELI_CRASH_AND_BURN) {
        return;
    }
    veh.m_autoPilot.m_nCarMission = MISSION_HELI_CRASH_AND_BURN;
}

//! 1381 ADD_EXPLOSION_NO_SOUND (case @0x48D94A): x, y, z, type
//! `AddExplosion(null, null, type, pos, 0, usesSound = 0, camShake = -1.0f, isVisible = 0)`
void AddExplosionNoSound(CVector pos, int32 type) {
    CExplosion::AddExplosion(nullptr, nullptr, (eExplosionType)type, pos, 0, 0, -1.0f, 0);
}

//! 1382 SET_OBJECT_AREA_VISIBLE (case @0x48D9B2): object, area. No null check; byte @+0x2F (m_AreaCode) = (uint8)area
void SetObjectAreaVisible(CObject& obj, int32 area) {
    obj.SetAreaCode((eAreaCodes)(int8)(uint8)area);
}

//! 1386 WAS_CUTSCENE_SKIPPED (case @0x48DA3C): => compare flag = CCutsceneMgr::ms_wasCutsceneSkipped (byte @0xB5F854 != 0)
bool WasCutsceneSkipped() {
    return CCutsceneMgr::ms_wasCutsceneSkipped;
}

//! 1390 DOES_VEHICLE_EXIST (case @0x48DAF9): car => compare flag = (the pool object exists)
bool DoesVehicleExist(CVehicle* veh) {
    return veh != nullptr;
}

//! 1392 ADD_SHORT_RANGE_SPRITE_BLIP_FOR_CONTACT_POINT (case @0x48DB3D): x, y, z, sprite => 1 blip
//! z <= -100 => ground Z. `SetShortRangeCoordBlip(BLIP_CONTACT_POINT, pos, colour 2, display 3, scriptName)` then SetBlipSprite.
int32 AddShortRangeSpriteBlipForContactPoint(CRunningScript& S, CVector pos, int32 sprite) {
    pos.z = GroundZIfAuto(pos.x, pos.y, pos.z);
    CRadar::GetActualBlipArrayIndex(S.CollectNextParameterWithoutIncreasingPC()); // result unused
    const auto blip = CRadar::SetShortRangeCoordBlip(BLIP_CONTACT_POINT, pos, (eBlipColour)2, (eBlipDisplay)3, S.m_szName);
    CRadar::SetBlipSprite(blip, (eRadarSprite)sprite);
    return (int32)blip;
}

//! 1396 FREEZE_CAR_POSITION_AND_DONT_LOAD_COLLISION (case @0x48DC3F): car, freeze. No null check.
//! freeze: physicalFlags |= 0x2004; if the script uses mission cleanup (+0xC6): CWorld::Remove(car); entity flag @+0x1C |= 0x40000
//! (m_bIsStaticWaitingForCollision); CWorld::Add(car).
//! else: physicalFlags &= ~0x2004; byte @+0xB8 = 0.
void FreezeCarPositionAndDontLoadCollision(CRunningScript& S, CVehicle& veh, int32 freeze) {
    if (freeze) {
        veh.m_nPhysicalFlags |= 0x2004;
        if (S.m_UsesMissionCleanup) {
            CWorld::Remove(&veh);
            veh.m_nFlags |= 0x40000;
            CWorld::Add(&veh);
        }
    } else {
        veh.m_nPhysicalFlags &= 0xFFFFDFFBu;
        veh.m_nFakePhysics = 0; // byte @+0xB8
    }
}


// ============================================================================ g14 (ProcessCommands1400To1499 @0x48EAA0, covers 1403..1499)

//! 0x59C910 - `CVector::Normalise` as the original evaluates it (sum of squares and reciprocal root stay in extended precision)
void NormaliseOriginal(CVector& v) {
    const double sumSq = ((double)v.x * v.x + (double)v.y * v.y) + (double)v.z * v.z;
    if (sumSq <= 0.0) {
        v.x = 1.0f; // (NaN takes the sqrt path)
    } else {
        const double recip = 1.0 / std::sqrt(sumSq);
        v.x = (float)(v.x * recip);
        v.y = (float)(v.y * recip);
        v.z = (float)(v.z * recip);
    }
}

//! 0x59C730 - `CrossProduct(a, b)`; the products stay in the FPU registers
CVector CrossProductOriginal(const CVector& a, const CVector& b) {
    return {
        (float)((double)b.z * a.y - (double)a.z * b.y),
        (float)((double)b.x * a.z - (double)a.x * b.z),
        (float)((double)b.y * a.x - (double)a.y * b.x),
    };
}

//! 0x59C790 - rotation of `v` by the matrix (no translation), original add order
CVector TransformVectorOriginal(const CMatrix& m, const CVector& v) {
    const auto &r = m.GetRight(), &f = m.GetForward(), &u = m.GetUp();
    return CVector{
        (float)(((double)u.x * v.z + (double)f.x * v.y) + (double)r.x * v.x),
        (float)(((double)u.y * v.z + (double)r.y * v.x) + (double)f.y * v.y),
        (float)(((double)u.z * v.z + (double)r.z * v.x) + (double)f.z * v.y)
    };
}

//! 0x59C810 - `v` transformed by the transposed rotation of `m`
CVector Multiply3x3VMOriginal(const CVector& v, const CMatrix& m) {
    const auto &r = m.GetRight(), &f = m.GetForward(), &u = m.GetUp();
    return CVector{
        (float)(((double)r.y * v.y + (double)r.z * v.z) + (double)v.x * r.x),
        (float)(((double)f.y * v.y + (double)f.x * v.x) + (double)f.z * v.z),
        (float)(((double)u.y * v.y + (double)u.x * v.x) + (double)u.z * v.z)
    };
}

//! 0x4082C0 - `CVector::Magnitude`, the result stays in the FPU register
double MagnitudeOriginal(const CVector& v) {
    return std::sqrt(((double)v.x * v.x + (double)v.y * v.y) + (double)v.z * v.z);
}

//! 1415 SET_LOAD_COLLISION_FOR_CAR_FLAG (case @0x48EBF4): car, load. No null check.
//! load: physicalFlags &= ~0x4000 (bDontLoadCollision); if the script uses mission cleanup: Remove, entity flag 0x40000 (m_bIsStaticWaitingForCollision), Add.
//! else: physicalFlags |= 0x4000; if the entity has the 0x40000 flag: clear it and, if it isn't static anymore, AddToMovingList().
void SetLoadCollisionForCarFlag(CRunningScript& S, CVehicle& veh, int32 load) {
    if (load) {
        veh.m_nPhysicalFlags &= 0xFFFFBFFFu;
        if (S.m_UsesMissionCleanup) {
            CWorld::Remove(&veh);
            veh.m_nFlags |= 0x40000;
            CWorld::Add(&veh);
        }
    } else {
        veh.m_nPhysicalFlags |= 0x4000;
        if (veh.m_nFlags & 0x40000) {
            veh.m_nFlags &= 0xFFFBFFFFu;
            if (!veh.GetIsStatic()) {
                veh.AddToMovingList();
            }
        }
    }
}

//! 1420 GET_PROGRESS_PERCENTAGE (case @0x48EDA0): => 1 float
float GetProgressPercentage() {
    return CStats::GetPercentageProgress();
}

//! 1428 SET_VEHICLE_TO_FADE_IN (case @0x48EDBB): car, alpha. No null check. `CVisibilityPlugins::SetClumpAlpha(clump, alpha)`
void SetVehicleToFadeIn(CVehicle& veh, int32 alpha) {
    CVisibilityPlugins::SetClumpAlpha(reinterpret_cast<RpClump*>(veh.GetRwObject()), alpha);
}

//! 1429 REGISTER_ODDJOB_MISSION_PASSED (case @0x48EDEF): no params
//! IncrementStat(0x93, 1.0); IncrementStat(0xB1, GetStatValue(0xB0)); SetStatValue(0xB0, 0.0); LastMissionPassedTime = time.
void RegisterOddjobMissionPassed() {
    CStats::IncrementStat((eStats)0x93, 1.0f);
    CStats::IncrementStat((eStats)0xB1, CStats::GetStatValue((eStats)0xB0));
    CStats::SetStatValue((eStats)0xB0, 0.0f);
    CTheScripts::LastMissionPassedTime = CTimer::m_snTimeInMilliseconds;
}

//! 1434 IS_AUSTRALIAN_GAME (case @0x48EE42): compare flag = false
bool IsAustralianGame() {
    return false;
}

//! 1436 SET_ONSCREEN_COUNTER_FLASH_WHEN_FIRST_DISPLAYED (case @0x48EE90): <global var>, flash
//! The variable is read first (GetIndexOfGlobalVariable), then CollectParameters(1).
void SetOnscreenCounterFlashWhenFirstDisplayed(CRunningScript& S) {
    const uint16 varId = S.GetIndexOfGlobalVariable();
    const auto   flash = Read<int32>(&S);
    CUserDisplay::OnscnTimer.SetCounterFlashWhenFirstDisplayed(varId, flash != 0);
}

//! 1437 SHUFFLE_CARD_DECKS (case @0x48EED9): numDecks
//! CardStack[] (312 int16) is cleared; every one of the 52 values is assigned, in each round, to a random remaining card instance of every deck
//! (a local 312-entry list of free instances, rand() & 0xFFFF * 2^-15 * remaining, truncated to int16). CardStackPosition = 0.
void ShuffleCardDecks(int32 numDecks) {
    CTheScripts::CardStack.fill(0);
    std::array<int16, 0x138> avail;
    for (int32 i = 0; i < 0x138; i++) {
        avail[i] = (int16)i;
    }
    int32 remaining = numDecks * 0x34;
    for (int32 value = 1; value < 0x35; value++) {
        for (int32 deck = 0; deck < numDecks; deck++) {
            const int32 count = (int16)remaining; // movsx esi, di
            const int32 r     = rand() & 0xFFFF;
            const int32 idx   = (int16)(int32)(double)((double)r * (double)(1.0f / 32768.0f) * (double)count); // 0x858B14, _ftol, movsx
            const int32 card  = avail[idx];
            CTheScripts::CardStack[card] = (int16)value;
            if (idx < count) {
                for (int32 k = idx; k < count; k++) {
                    avail[k] = (k > 0x136) ? (int16)0 : avail[k + 1];
                }
            }
            remaining--;
        }
    }
    CTheScripts::CardStackPosition = 0;
}

//! 1438 FETCH_NEXT_CARD (case @0x48EFAD): => 1 int
//! If the slot at the current position is 0 the position restarts at 0; returns that slot, position++ (wraps to 0 at 0x138).
int32 FetchNextCard() {
    int16 pos = CTheScripts::CardStackPosition;
    if (CTheScripts::CardStack[pos] == 0) {
        pos = 0;
    }
    const int32 card = CTheScripts::CardStack[pos];
    pos++;
    CTheScripts::CardStackPosition = (pos == 0x138) ? (int16)0 : pos;
    return card;
}

//! 1441 ADD_TO_OBJECT_ROTATION_VELOCITY (case @0x48F07D): object, x, y, z. No null check.
//! turnSpeed += rotate(matrix, (x, y, z) * 0.02f) [each product / sum rounded to float]; if the object is static (flag @+0x1C & 4):
//! SetIsStatic(false) (vtable +0x10) + AddToMovingList().
void AddToObjectRotationVelocity(CObject& obj, CVector v) {
    constexpr float K = 0.02f; // 0x858B38
    const CVector   scaled{ (float)((double)v.x * K), (float)((double)v.y * K), (float)((double)v.z * K) };
    CVector         turn = obj.m_vecTurnSpeed;
    const CVector   rot  = TransformVectorOriginal(obj.GetMatrix(), scaled); // 0x59C790
    turn.x = (float)((double)turn.x + (double)rot.x);
    turn.y = (float)((double)turn.y + (double)rot.y);
    turn.z = (float)((double)turn.z + (double)rot.z);
    if (obj.m_nFlags & 4) {
        obj.SetIsStatic(false);
        obj.AddToMovingList();
    }
    obj.m_vecTurnSpeed = turn;
}

//! 1442 SET_OBJECT_ROTATION_VELOCITY (case @0x48F17E): object, x, y, z. No null check.
//! turnSpeed = rotate(matrix, (x, y, z) * timeStep) * 0.02f (static handling as above, before the store)
void SetObjectRotationVelocity(CObject& obj, CVector v) {
    constexpr float K    = 0.02f; // 0x858B38
    const double    step = CTimer::ms_fTimeStep;
    const CVector   scaled{ (float)(step * v.x), (float)(step * v.y), (float)(step * v.z) };
    const CVector   rot = TransformVectorOriginal(obj.GetMatrix(), scaled);
    if (obj.m_nFlags & 4) {
        obj.SetIsStatic(false);
        obj.AddToMovingList();
    }
    obj.m_vecTurnSpeed = CVector{ (float)((double)rot.x * K), (float)((double)rot.y * K), (float)((double)rot.z * K) }; // 0x45AFB0
}

//! 1443 IS_OBJECT_STATIC (case @0x48F260): object (no null check) => compare flag = GetIsStatic()
bool IsObjectStatic(CObject& obj) {
    return obj.GetIsStatic();
}

//! The CRT acos (0x822380 -> 0x82239D) on the x87 stack at the CURRENT precision control: |x| < 1 => atan2(sqrt((1 + x) * (1 - x)), x) (fpatan); x == 1 => 0;
//! x == -1 => pi; other (|x| > 1, inf) => the negative QNaN constant (0x8E3130); NaN => returned as is. (Same sequence as CQuaternion::CalcThetaFromQuats.)
double AcosCrt(double x) {
    const double ad   = std::fabs(x);
    const int    mode = ad < 1.0 ? 0 : x == 1.0 ? 1 : x == -1.0 ? 2 : x != x ? 4 : 3;
    static const unsigned char qnan[10] = { 0, 0, 0, 0, 0, 0, 0, 0xC0, 0xFF, 0xFF };
    double                     out;
    __asm {
        fld   qword ptr [x]
        mov   eax, mode
        test  eax, eax
        jne   L_special
        fld1
        fadd  st(0), st(1)
        fld1
        fsub  st(0), st(2)
        fmulp st(1), st(0)
        fsqrt
        fxch  st(1)
        fpatan
        jmp   L_have
    L_special:
        cmp   eax, 4
        je    L_have
        fstp  st(0)
        cmp   eax, 1
        jne   L_not1
        fldz
        jmp   L_have
    L_not1:
        cmp   eax, 2
        jne   L_nan
        fldpi
        jmp   L_have
    L_nan:
        fld   tbyte ptr [qnan]
    L_have:
        fstp  qword ptr [out]
    }
    return out;
}

//! x87 `fsqrt` at the current precision control (std::sqrt may be computed by the SSE2 CRT at full double precision)
double Fsqrt(double v) {
    __asm {
        fld   qword ptr [v]
        fsqrt
        fstp  qword ptr [v]
    }
    return v;
}

//! 1444 GET_ANGLE_BETWEEN_2D_VECTORS (case @0x48F2A2): ax, ay, bx, by => 1 float (degrees)
//! acos(dot / (|a| * |b|)) * 57.29578 (0x859878); everything stays on the x87 stack (acos = the CRT's x87 sequence, see AcosCrt).
float GetAngleBetween2DVectors(float ax, float ay, float bx, float by) {
    const double dot = (double)by * ay + (double)bx * ax;
    const double lb  = Fsqrt((double)by * by + (double)bx * bx);
    const double la  = Fsqrt((double)ay * ay + (double)ax * ax);
    const double c   = dot / (lb * la);
    return (float)(AcosCrt(c) * (double)57.2957763671875f);
}

//! 1445 DO_2D_RECTANGLES_COLLIDE (case @0x48F339): cx1, cy1, w1, h1, cx2, cy2, w2, h2 => compare flag
//! Rectangles given by centre and size. false if rect2.minY (unrounded, > rect1.maxY unrounded), minY1 > maxY2, maxX1 < minX2 or minX1 > maxX2 (all ordered; NaN keeps true).
bool Do2DRectanglesCollide(float p0, float p1, float p2, float p3, float p4, float p5, float p6, float p7) {
    const double h1 = (double)p3 * 0.5, h2 = (double)p7 * 0.5, w1 = (double)p2 * 0.5, w2 = (double)p6 * 0.5;
    const float  A = (float)((double)p1 - h1); // rect1.minY
    const float  B = (float)((double)p5 + h2); // rect2.maxY
    const float  C = (float)((double)p0 - w1); // rect1.minX
    const float  D = (float)((double)p4 - w2); // rect2.minX
    const float  E = (float)((double)p0 + w1); // rect1.maxX
    const float  F = (float)((double)p4 + w2); // rect2.maxX
    const double top1 = (double)p1 + h1;
    const double bot2 = (double)p5 - h2;
    bool         result = true;
    if (bot2 > top1) {
        result = false;
    }
    if (A > B) {
        result = false;
    }
    if (E < D) {
        result = false;
    }
    if (C > F) {
        result = false;
    }
    return result;
}

//! 1446 GET_OBJECT_ROTATION_VELOCITY (case @0x48F461): object => 3 floats = (turnSpeed * transposed rotation) * 50.0f
MultiRet<float, float, float> GetObjectRotationVelocity(CObject& obj) {
    const CVector r = Multiply3x3VMOriginal(obj.m_vecTurnSpeed, obj.GetMatrix()); // 0x59C810
    return { (float)((double)r.x * 50.0f), (float)((double)r.y * 50.0f), (float)((double)r.z * 50.0f) }; // 0x858B40
}

//! 1447 ADD_VELOCITY_RELATIVE_TO_OBJECT_VELOCITY (case @0x48F4E9): object, x, y, z. No null check.
//! v = (x, y, z) * timeStep / 50; nothing happens for static objects. Otherwise, with m = normalised move speed (nothing if m.z == 1.0),
//! t1 = normalise(m x (0,0,1)), t2 = normalise(t1 x m): moveSpeed += t1 * v.x + m * v.y + t2 * v.z.
void AddVelocityRelativeToObjectVelocity(CObject& obj, CVector v) {
    const float step = CTimer::ms_fTimeStep;
    v.x = (float)((double)step * v.x); // 0x40FEF0 (operator*=)
    v.y = (float)((double)step * v.y);
    v.z = (float)((double)step * v.z);
    const double recip = 1.0 / (double)50.0f; // 0x411A30 (operator/=): 1.0f / 50.0f stays in the FPU
    v.x = (float)(v.x * recip);
    v.y = (float)(v.y * recip);
    v.z = (float)(v.z * recip);
    if (obj.m_nFlags & 4) {
        return;
    }
    CVector m = obj.m_vecMoveSpeed;
    NormaliseOriginal(m);
    if ((double)m.z == 1.0) { // 0x85A310
        return;
    }
    const CVector M2 = m;
    CVector       t1 = CrossProductOriginal(m, CVector{ 0.0f, 0.0f, 1.0f });
    NormaliseOriginal(t1);
    CVector t2 = CrossProductOriginal(t1, M2);
    NormaliseOriginal(t2);
    CVector speed = obj.m_vecMoveSpeed;
    const auto Mad = [&](const CVector& d, float k) { // operator*(CVector, float) (0x40FEC0) then operator+= (0x411A00)
        const CVector scaled{ (float)((double)d.x * k), (float)((double)d.y * k), (float)((double)d.z * k) };
        speed.x = (float)((double)scaled.x + (double)speed.x);
        speed.y = (float)((double)scaled.y + (double)speed.y);
        speed.z = (float)((double)scaled.z + (double)speed.z);
    };
    Mad(t1, v.x);
    Mad(M2, v.y);
    Mad(t2, v.z);
    if (obj.m_nFlags & 4) { // (can't be true anymore, kept as in the exe)
        obj.SetIsStatic(false);
        obj.AddToMovingList();
    }
    obj.m_vecMoveSpeed = speed;
}

//! 1448 GET_OBJECT_SPEED (case @0x48F73F): object => 1 float = |moveSpeed| * 50.0f
float GetObjectSpeed(CObject& obj) {
    return (float)(MagnitudeOriginal(obj.m_vecMoveSpeed) * 50.0f);
}

//! 1456 GET_2D_LINES_INTERSECT_POINT (case @0x48F830): x1,y1,x2,y2 (line 1), x3,y3,x4,y4 (line 2) => compare flag, 2 floats
//! Slope / intercept form with 1e-6f replacing a zero dx. The intersection must lie within both segments' (swapped to min/max) boxes
//! extended by 0.01; otherwise the result is (-1000000.0f, -1000000.0f) and the flag false. The flag is set BEFORE the 2 results are stored.
OpcodeResult Get2DLinesIntersectPoint(CRunningScript& S, float p0, float p1, float p2, float p3, float p4, float p5, float p6, float p7) {
    bool         ok  = true;
    const double d1  = (double)p0 - (double)p2;
    const float  dx1 = (d1 == 0.0) ? 1.0e-6f : (float)d1; // 0x358637BD
    double       d2  = (double)p4 - (double)p6;
    if (d2 == 0.0) {
        d2 = (double)1.0e-6f; // 0x858C18
    }
    const double e1  = (double)p1 - (double)p3;
    const float  e1f = (float)e1;
    const double m1  = e1 / (double)dx1;
    const double e2  = (double)p5 - (double)p7;
    const float  m2f = (float)(e2 / d2);
    const float  c1f = (float)((double)p1 - m1 * (double)p0);
    const float  c2f = (float)((double)p5 - (double)m2f * (double)p4);
    const double nm1 = -m1, nm2 = -(double)m2f;
    const float  den = (float)(nm1 - nm2);
    const float  num = (float)((nm1 * (double)c2f) - (nm2 * (double)c1f));
    float        x = 0.0f, y = 0.0f;
    if (den == 0.0f) {
        ok = false;
    } else {
        x = (d1 == 0.0) ? p0 : (float)(((double)c1f - (double)c2f) / (double)den);
        y = (e1f == 0.0f) ? p1 : (float)((double)num / (double)den);
        // the 8 range checks (each fails for NaN)
        const float xlo1 = (p0 > p2) ? p2 : p0, xhi1 = (p0 > p2) ? p0 : p2;
        const float ylo1 = (p1 > p3) ? p3 : p1, yhi1 = (p1 > p3) ? p1 : p3;
        const float xlo2 = (p4 > p6) ? p6 : p4, xhi2 = (p4 > p6) ? p4 : p6;
        const float ylo2 = (p5 > p7) ? p7 : p5, yhi2 = (p5 > p7) ? p5 : p7;
        constexpr double T = 0.01; // 0x85A308
        const bool inside =
               ((double)xlo1 - T <= (double)x) && ((double)xhi1 + T >= (double)x)
            && ((double)xlo2 - T <= (double)x) && ((double)xhi2 + T >= (double)x)
            && ((double)ylo1 - T <= (double)y) && ((double)yhi1 + T >= (double)y)
            && ((double)ylo2 - T <= (double)y) && ((double)yhi2 + T >= (double)y);
        if (!inside) {
            ok = false;
        }
    }
    if (!ok) {
        x = y = -1000000.0f; // 0xC9742400
    }
    S.UpdateCompareFlag(ok);
    StoreArg(&S, x);
    StoreArg(&S, y);
    return OR_CONTINUE;
}

// ============================================================================ g15 (ProcessCommands1500To1599 @0x490DB0, covers 1500..1593)

//! 1515 START_PLAYBACK_RECORDED_CAR (case @0x490FB0): car, fileNumber. The (maybe null) pool object is passed on.
void StartPlaybackRecordedCar(CVehicle* veh, int32 fileNumber) {
    CVehicleRecording::StartPlaybackRecordedCar(veh, fileNumber, false, false);
}

//! 1516 STOP_PLAYBACK_RECORDED_CAR (case @0x490FE6): car. null car skipped
void StopPlaybackRecordedCar(CVehicle* veh) {
    if (veh) {
        CVehicleRecording::StopPlaybackRecordedCar(veh);
    }
}

//! 1517 PAUSE_PLAYBACK_RECORDED_CAR (case @0x491018): car (maybe null, passed on)
void PausePlaybackRecordedCar(CVehicle* veh) {
    CVehicleRecording::PausePlaybackRecordedCar(veh);
}

//! 1518 UNPAUSE_PLAYBACK_RECORDED_CAR (case @0x491043): car (maybe null, passed on)
void UnpausePlaybackRecordedCar(CVehicle* veh) {
    CVehicleRecording::UnpausePlaybackRecordedCar(veh);
}

//! 1521..1524 SET_CAR_ESCORT_CAR_LEFT/RIGHT/REAR/FRONT (cases @0x49106E / 0x4910CE / 0x49112E / 0x49118F): car, targetCar. No null checks.
//! car.m_autoPilot.m_TargetEntity (+0x41C) = target, target->RegisterReference(&that); unless the mission is 0x39 / 0x3A it becomes 0x1D / 0x1E / 0x1F / 0x20.
template<eCarMission Mission>
void SetCarEscortCar(CVehicle& veh, CVehicle& target) {
    veh.m_autoPilot.m_TargetEntity = &target;
    target.RegisterReference((CEntity**)&veh.m_autoPilot.m_TargetEntity);
    const auto mission = veh.m_autoPilot.m_nCarMission;
    if (mission == MISSION_PLANE_CRASH_AND_BURN || mission == MISSION_HELI_CRASH_AND_BURN) {
        return;
    }
    veh.m_autoPilot.m_nCarMission = Mission;
}

//! 1531 IS_CHAR_STOPPED_IN_ANGLED_AREA_IN_CAR_2D / 1537 ..._3D (case @0x491289, shared): `CharInAngledAreaCheckCommand(commandId)`; no CollectParameters before.
void CharStoppedInAngledAreaInCar(CRunningScript& S, eScriptCommands command) {
    S.CharInAngledAreaCheckCommand((int32)command);
}

//! 1540 GET_HEADING_FROM_VECTOR_2D (case @0x49137C): x, y => 1 float
//! atan2-like GetATanOfXY(x, y) * 57.29578 (0x859878) - 90.0 (0x85991C); while negative (ordered) add 360.0 (0x859E2C).
float GetHeadingFromVector2D(float x, float y) {
    double h = CGeneral::GetATanOfXYExt(x, y) * (double)57.2957763671875f - 90.0;
    while (h < 0.0) {
        h += 360.0;
    }
    return (float)h;
}

//! 1542 LOAD_PATH_NODES_IN_AREA (case @0x491404): x1, y1, x2, y2 (each pair ordered with `a > b` swaps)
//! ThePaths.MakeRequestForNodesToBeLoaded(minX, maxX, minY, maxY)
void LoadPathNodesInArea(float x1, float y1, float x2, float y2) {
    if (x1 > x2) {
        std::swap(x1, x2);
    }
    if (y1 > y2) {
        std::swap(y1, y2);
    }
    ThePaths.MakeRequestForNodesToBeLoaded(x1, x2, y1, y2);
}

//! 1543 RELEASE_PATH_NODES (case @0x491494)
void ReleasePathNodes() {
    ThePaths.ReleaseRequestedNodes();
}

//! 1550 IS_PLAYBACK_GOING_ON_FOR_CAR (case @0x4915F5): car (maybe null, passed on) => compare flag
bool IsPlaybackGoingOnForCar(CVehicle* veh) {
    return CVehicleRecording::IsPlaybackGoingOnForCar(veh);
}

//! 1551 SET_SENSE_RANGE (case @0x49162A): ped, range
//! Handle != -1: the ped's intelligence gets seeing + hearing range (no null check). Handle == -1: all mission peds (PED_MISSION) of the moving entity list.
void SetSenseRange(int32 pedHandle, float range) {
    if (pedHandle != -1) {
        auto* const intel = GetPedPool()->GetAtRef(pedHandle)->GetIntelligence();
        intel->SetSeeingRange(range);
        intel->SetHearingRange(range);
        return;
    }
    for (auto* const entity : CWorld::ms_listMovingEntityPtrs) {
        if (entity->GetType() != ENTITY_TYPE_PED) {
            continue;
        }
        auto* const ped = static_cast<CPed*>(entity);
        if (ped->GetCreatedBy() != PED_MISSION) {
            continue;
        }
        ped->GetIntelligence()->SetSeeingRange(range);
        ped->GetIntelligence()->SetHearingRange(range);
    }
}

//! 1557 OPEN_SEQUENCE_TASK (case @0x49186F): => 1 handle (-1 if no slot)
//! Slot = GetAvailableSlot(script uses mission cleanup); it's opened, flushed and made the active sequence. A (dead) GetActualScriptThingIndex on the
//! peeked output variable is kept. With mission cleanup the handle is added as type 8 (task sequence).
int32 OpenSequenceTask(CRunningScript& S) {
    CTheScripts::GetActualScriptThingIndex(S.CollectNextParameterWithoutIncreasingPC(), SCRIPT_THING_SEQUENCE_TASK);
    const int32 slot = CTaskSequences::GetAvailableSlot(S.m_UsesMissionCleanup);
    if (slot < 0 || slot >= 0x40) {
        return -1;
    }
    CTaskSequences::ms_bIsOpened[slot] = true;
    CTaskSequences::ms_taskSequence[slot].Flush();
    CTaskSequences::ms_iActiveSequence = slot;
    const int32 handle = CTheScripts::GetNewUniqueScriptThingIndex(slot, SCRIPT_THING_SEQUENCE_TASK);
    if (S.m_UsesMissionCleanup) {
        CTheScripts::MissionCleanUp.AddEntityToList(handle, (MissionCleanUpEntityType)8);
    }
    return handle;
}

//! 1558 CLOSE_SEQUENCE_TASK (case @0x491908): sequence. If valid: not opened anymore, no active sequence (-1)
void CloseSequenceTask(int32 handle) {
    const int32 idx = CTheScripts::GetActualScriptThingIndex(handle, SCRIPT_THING_SEQUENCE_TASK);
    if (idx < 0 || idx >= 0x40) {
        return;
    }
    CTaskSequences::ms_bIsOpened[idx] = false;
    CTaskSequences::ms_iActiveSequence = -1;
}

//! 1560 PERFORM_SEQUENCE_TASK (case @0x49194B): ped (handle, -1 = active sequence), sequence
//! If the sequence is valid: GivePedScriptedTask(pedHandle, new CTaskComplexUseSequence(idx), 1560).
void PerformSequenceTask(CRunningScript& S, eScriptCommands command, int32 pedHandle, int32 seqHandle) {
    const int32 idx = CTheScripts::GetActualScriptThingIndex(seqHandle, SCRIPT_THING_SEQUENCE_TASK);
    if (idx < 0 || idx >= 0x40) {
        return;
    }
    S.GivePedScriptedTask(pedHandle, new CTaskComplexUseSequence{ idx }, (int32)command);
}

//! 1563 CLEAR_SEQUENCE_TASK (case @0x491A4A): sequence
//! If valid: not opened; if the sequence is still referenced (+0x3C != 0) it's flagged to flush (+0x38 = 1), else flag = 0 and Flush(); the script thing is freed.
//! With mission cleanup: RemoveEntityFromList(handle, 8).
void ClearSequenceTask(CRunningScript& S, int32 handle) {
    const int32 idx = CTheScripts::GetActualScriptThingIndex(handle, SCRIPT_THING_SEQUENCE_TASK);
    if (idx >= 0 && idx < 0x40) {
        auto& seq = CTaskSequences::ms_taskSequence[idx];
        CTaskSequences::ms_bIsOpened[idx] = false;
        if (seq.m_RefCnt == 0) {
            seq.m_bFlushTasks = false;
            seq.Flush();
        } else {
            seq.m_bFlushTasks = true;
        }
        CTheScripts::ScriptSequenceTaskArray[idx].m_bUsed = false;
    }
    if (S.m_UsesMissionCleanup) {
        CTheScripts::MissionCleanUp.RemoveEntityFromList(handle, (MissionCleanUpEntityType)8);
    }
}

//! 1565 ADD_ATTRACTOR (case @0x491ABC): x, y, z, heading1, heading2, sequence => 1 handle (-1 on failure)
//! Allocates a scripted 2d effect (radius -1.0f). queueDir/useDir = normalise(-sin(a1), cos(a1), 0), forwardDir = same for a2 (degrees * 0.01745329f),
//! type 3 (attractor), attractor type 7 (scripted), the effect's sequence task id is the sequence index. The dead GetActualScriptThingIndex(peeked var) is kept.
int32 AddAttractor(CRunningScript& S, CVector pos, float a1, float a2, int32 seqHandle) {
    CTheScripts::GetActualScriptThingIndex(S.CollectNextParameterWithoutIncreasingPC(), SCRIPT_THING_2D_EFFECT);
    const int32 fx  = CScripted2dEffects::AddScripted2DEffect(-1.0f);
    const int32 seq = CTheScripts::GetActualScriptThingIndex(seqHandle, SCRIPT_THING_SEQUENCE_TASK);
    if (fx < 0 || fx >= 0x40 || seq < 0 || seq >= 0x40) {
        return -1;
    }
    constexpr float DEG2RAD = 0.01745329238474369f; // 0x8595EC
    const double    r1      = (double)a1 * DEG2RAD;
    CVector         d1{ (float)(-std::sin(r1)), (float)std::cos(r1), 0.0f };
    NormaliseOriginal(d1);
    const double r2 = (double)a2 * DEG2RAD;
    CVector      d2{ (float)(-std::sin(r2)), (float)std::cos(r2), 0.0f };
    NormaliseOriginal(d2);
    auto& e = *CScripted2dEffects::GetEffect(fx);
    e.m_Pos              = pos;
    e.m_vecQueueDir      = RwV3d{ d1.x, d1.y, d1.z };
    CScripted2dEffects::ms_effectSequenceTaskIDs[fx] = seq;
    e.m_vecUseDir        = RwV3d{ d1.x, d1.y, d1.z };
    e.m_vecForwardDir    = RwV3d{ d2.x, d2.y, d2.z };
    e.m_Type             = EFFECT_ATTRACTOR;
    e.m_nAttractorType   = PED_ATTRACTOR_SCRIPTED;
    const int32 handle = CTheScripts::GetNewUniqueScriptThingIndex(fx, SCRIPT_THING_2D_EFFECT);
    if (S.m_UsesMissionCleanup) {
        CTheScripts::MissionCleanUp.AddEntityToList(handle, (MissionCleanUpEntityType)7);
    }
    return handle;
}

//! 1566 CLEAR_ATTRACTOR (case @0x491C33): attractor. If valid: deactivated. With mission cleanup: RemoveEntityFromList(handle, 7).
void ClearAttractor(CRunningScript& S, int32 handle) {
    const int32 idx = CTheScripts::GetActualScriptThingIndex(handle, SCRIPT_THING_2D_EFFECT);
    if (idx >= 0 && idx < 0x40) {
        CScripted2dEffects::ms_activated[idx] = false;
    }
    if (S.m_UsesMissionCleanup) {
        CTheScripts::MissionCleanUp.RemoveEntityFromList(handle, (MissionCleanUpEntityType)7);
    }
}

//! 1582 GET_SCRIPT_TASK_STATUS (case @0x49220A): ped (no null check), taskCommand => 1 int (-1 => 7)
int32 GetScriptTaskStatus(CPed& ped, int32 taskCommand) {
    const auto status = (int32)CPedScriptedTaskRecord::GetStatus(&ped, taskCommand);
    return status == -1 ? 7 : status;
}

//! 1583 CREATE_GROUP (case @0x492258): defaultTaskAllocator => 1 handle
//! AddGroup, group intelligence default task allocator type, group flag @+0x2D0 (m_bIsMissionGroup) = 1; mission cleanup type 5. Dead peek kept.
int32 CreateGroup(CRunningScript& S, int32 allocatorType) {
    CTheScripts::GetActualScriptThingIndex(S.CollectNextParameterWithoutIncreasingPC(), SCRIPT_THING_PED_GROUP);
    const int32 grp = CPedGroups::AddGroup();
    auto&       g   = CPedGroups::ms_groups[grp];
    g.GetIntelligence().SetDefaultTaskAllocatorType((ePedGroupDefaultTaskAllocatorType)allocatorType);
    g.m_bIsMissionGroup = true;
    const int32 handle = CTheScripts::GetNewUniqueScriptThingIndex(grp, SCRIPT_THING_PED_GROUP);
    if (S.m_UsesMissionCleanup) {
        CTheScripts::MissionCleanUp.AddEntityToList(handle, (MissionCleanUpEntityType)5);
    }
    return handle;
}

//! 1584 SET_GROUP_LEADER (case @0x4922D8): group, ped (pool object, not null-checked)
//! A non-player ped gets a CEventScriptCommand(primary task, new CTaskComplexBeInGroup(group, true)); then membership.SetLeader(ped), group.Process().
void SetGroupLeader(int32 groupHandle, int32 pedHandle) {
    const int32 idx = CTheScripts::GetActualScriptThingIndex(groupHandle, SCRIPT_THING_PED_GROUP);
    CPed* const ped = GetPedPool()->GetAtRef(pedHandle);
    if (idx < 0 || idx >= 8) {
        return;
    }
    if (!ped->IsPlayer()) {
        CEventScriptCommand ev{ TASK_PRIMARY_PRIMARY, new CTaskComplexBeInGroup{ idx, true }, false };
        ped->GetIntelligence()->m_eventGroup.Add(&ev, false);
    }
    auto& g = CPedGroups::ms_groups[idx];
    g.GetMembership().SetLeader(ped);
    g.Process();
}

//! 1586 REMOVE_GROUP (case @0x4926E9): group
//! If the group is valid: an active group led by the player only loses its followers (and the handler returns WITHOUT the mission cleanup
//! removal); in every other case (inactive group, no leader, non-player leader) RemoveGroup. Then, with mission cleanup: RemoveEntityFromList(handle, 5).
void RemoveGroup(CRunningScript& S, int32 handle) {
    const int32 idx = CTheScripts::GetActualScriptThingIndex(handle, SCRIPT_THING_PED_GROUP);
    if (idx >= 0 && idx < 8) {
        if (CPedGroups::ms_activeGroups[idx]) {
            auto* const leader = CPedGroups::ms_groups[idx].GetMembership().GetLeader();
            if (leader && leader->IsPlayer()) {
                CPedGroups::RemoveAllFollowersFromGroup(idx);
                return;
            }
        }
        CPedGroups::RemoveGroup(idx); // 0x492753: also reached for an inactive group / a group without (player) leader
    }
    if (S.m_UsesMissionCleanup) {
        CTheScripts::MissionCleanUp.RemoveEntityFromList(handle, (MissionCleanUpEntityType)5);
    }
}
//! 1569 CREATE_CHAR_AT_ATTRACTOR (case @0x491C7F): pedType, model, attractor, taskCommand => 1 handle (-1 if the attractor is invalid)
//! The ped is created like CREATE_CHAR (cop / emergency / civilian by type, PED_MISSION, bAllowMedicsToReviveMe = false), placed at the
//! attractor (forward offset 0.01f), ClearSpaceForMissionEntity, static-waiting flag if the script uses mission cleanup, added to the world.
//! Its default task depends on `taskCommand`: 1466 (TASK_STAND_STILL): StandStill(0, true, false, 8.0f); 1503: no task; 1504: WanderCriminal(WALK, rand(0,8), true);
//! 1502 (TASK_WANDER_STANDARD) and everything else: WanderStandard(WALK, rand(0,8), true). Then the ped gets a CTaskComplexUseEffect through a CEventScriptCommand,
//! recorded in the scripted task records (opcode = this command), ms_nTotalMissionPeds++ and (cleanup) AddEntityToList(handle, ped).
int32 CreateCharAtAttractor(CRunningScript& S, eScriptCommands command, int32 pedTypeId, int32 modelId, int32 attractorHandle, int32 taskCommand) {
    const int32 fxIdx = CTheScripts::GetActualScriptThingIndex(attractorHandle, SCRIPT_THING_2D_EFFECT);
    if (fxIdx < 0 || fxIdx >= 0x40) {
        return -1;
    }
    const auto pedType             = (ePedType)pedTypeId;
    uint32     typeSpecificModelId = (uint32)modelId;
    S.GetCorrectPedModelIndexForEmergencyServiceType(pedType, &typeSpecificModelId);
    CPed* const ped = [&]() -> CPed* {
        switch (pedType) {
        case PED_TYPE_COP:     return new CCopPed{ typeSpecificModelId };
        case PED_TYPE_MEDIC:
        case PED_TYPE_FIREMAN: return new CEmergencyPed{ pedType, typeSpecificModelId };
        default:               return new CCivilianPed{ pedType, typeSpecificModelId };
        }
    }();
    ped->SetCharCreatedBy(PED_MISSION);
    ped->bAllowMedicsToReviveMe = false;
    auto* const fx = reinterpret_cast<C2dEffectPedAttractor*>(&CScripted2dEffects::ms_effects[fxIdx]);
    CPedAttractorPedPlacer::PlacePedAtEffect(*fx, nullptr, ped, 0.01f);
    CTheScripts::ClearSpaceForMissionEntity(ped->GetPosition(), ped);
    if (S.m_UsesMissionCleanup) {
        ped->m_bIsStaticWaitingForCollision = true;
    }
    CWorld::Add(ped);

    if (taskCommand != 1503) {
        CTask* task;
        switch (taskCommand) {
        case 1466:
            task = new CTaskSimpleStandStill{ 0, true, false, 8.0f };
            break;
        case 1504:
            task = new CTaskComplexWanderCriminal{ PEDMOVE_WALK, (uint8)CGeneral::GetRandomNumberInRange(0, 8), true };
            break;
        default: // 1502 and all others
            task = new CTaskComplexWanderStandard{ PEDMOVE_WALK, (uint8)CGeneral::GetRandomNumberInRange(0, 8), true };
            break;
        }
        ped->GetTaskManager().SetTask(task, TASK_PRIMARY_DEFAULT);
    }

    CEventScriptCommand ev{ TASK_PRIMARY_PRIMARY, new CTaskComplexUseEffect{ fx, nullptr }, false };
    auto* const added = static_cast<CEventScriptCommand*>(ped->GetIntelligence()->m_eventGroup.Add(&ev, false));
    const int32 slot  = CPedScriptedTaskRecord::GetVacantSlot();
    CPedScriptedTaskRecord::ms_scriptedTasks[slot].Set(ped, (int32)command, added);
    CPopulation::ms_nTotalMissionPeds++;
    const int32 handle = GetPedPool()->GetRef(ped);
    if (S.m_UsesMissionCleanup) {
        CTheScripts::MissionCleanUp.AddEntityToList(handle, MISSION_CLEANUP_ENTITY_TYPE_PED);
    }
    return handle;
}

//! 1585 SET_GROUP_MEMBER (case @0x4923BA): group, ped (pool object, not null-checked)
//! The ped gets a CEventScriptCommand(primary task, CTaskComplexBeInGroup(group, false)); a full group (7 followers) led by the player drops one follower;
//! AddFollower, group.Process(). If the leader is in a vehicle (bInVehicle -> m_pVehicle, else its TASK_COMPLEX_ENTER_CAR_AS_DRIVER's car) that has a free
//! passenger seat, a CEventLeaderEnteredCarAsDriver is evaluated for the group: TASK_GROUP_ENTER_CAR => the ped gets an enter-car-as-passenger group script task and
//! (once, bHasGroupDriveTask) a default task sequence {CarDrive, previous default}; TASK_GROUP_ENTER_CAR_AND_PERFORM_SEQUENCE => the group intelligence gets
//! a CEventGroupEvent(leader, new CEventLeaderEnteredCarAsDriver).
void SetGroupMember(int32 groupHandle, int32 pedHandle) {
    const int32 idx = CTheScripts::GetActualScriptThingIndex(groupHandle, SCRIPT_THING_PED_GROUP);
    CPed* const ped = GetPedPool()->GetAtRef(pedHandle);
    if (idx < 0 || idx >= 8) {
        return;
    }
    CEventScriptCommand ev{ TASK_PRIMARY_PRIMARY, new CTaskComplexBeInGroup{ idx, false }, false };
    ped->GetIntelligence()->m_eventGroup.Add(&ev, false);
    auto& group      = CPedGroups::ms_groups[idx];
    auto& membership = group.GetMembership();
    if (membership.CountMembersExcludingLeader() >= 7 && membership.GetLeader() && membership.GetLeader()->IsPlayer()) {
        membership.RemoveNFollowers(1);
    }
    membership.AddFollower(ped);
    group.Process();
    CPed* const leader = membership.GetLeader();
    if (!leader) {
        return;
    }
    CVehicle* veh = nullptr;
    if (leader->bInVehicle) {
        veh = leader->m_pVehicle;
    }
    if (!veh) {
        const auto task = leader->GetIntelligence()->FindTaskByType(TASK_COMPLEX_ENTER_CAR_AS_DRIVER); // 0x2BD
        if (!task) {
            return;
        }
        veh = static_cast<CTaskComplexEnterCar*>(task)->GetTargetCar(); // +0xC
        if (!veh) {
            return;
        }
    }
    bool hasFreeSeat = false;
    for (int32 i = 0; i < (int32)veh->m_nMaxPassengers; i++) {
        if (!veh->m_apPassengers[i]) {
            hasFreeSeat = true;
            break;
        }
    }
    if (!hasFreeSeat) {
        return;
    }
    CEventLeaderEnteredCarAsDriver leaderEvent{ veh };
    leaderEvent.ComputeResponseTaskType(&group);
    auto& intel = group.GetIntelligence();
    switch ((int16)leaderEvent.m_TaskId) {
    case TASK_GROUP_ENTER_CAR_AND_PERFORM_SEQUENCE: { // 0x5E9
        CEventGroupEvent groupEvent{ leader, new CEventLeaderEnteredCarAsDriver{ veh } };
        intel.AddEvent(&groupEvent);
        break;
    }
    case TASK_GROUP_ENTER_CAR: { // 0x5E8
        CTaskComplexEnterCarAsPassenger enterTask{ veh, 0, true };
        intel.SetScriptCommandTask(ped, enterTask);
        if (!ped->bHasGroupDriveTask) {
            ped->bHasGroupDriveTask = true;
            CTaskComplexSequence seq;
            seq.AddTask(new CTaskSimpleCarDrive{ veh, nullptr, true });
            if (const auto def = intel.GetTaskDefault(ped)) {
                seq.AddTask(def->Clone());
            }
            intel.SetDefaultTask(ped, seq);
        }
        break;
    }
    default:
        break;
    }
}

} // namespace

namespace notsa::script::commands::ported::g13_15 {
void RegisterHandlers();
void RegisterTaskHandlers(); // Group13_15_task.cpp
}

void notsa::script::commands::ported::g13_15::RegisterHandlers() {
    REGISTER_COMMAND_HANDLER_BEGIN("Ported g13-15");

    REGISTER_COMMAND_HANDLER(COMMAND_CREATE_LOCKED_PROPERTY_PICKUP, CreateLockedPropertyPickup);
    REGISTER_COMMAND_HANDLER(COMMAND_CREATE_FORSALE_PROPERTY_PICKUP, CreateForsalePropertyPickup);
    REGISTER_COMMAND_HANDLER(COMMAND_FREEZE_CAR_POSITION, FreezeCarPosition);
    REGISTER_COMMAND_HANDLER(COMMAND_HAS_CAR_BEEN_DAMAGED_BY_CHAR, HasCarBeenDamagedByChar);
    REGISTER_COMMAND_HANDLER(COMMAND_HAS_CAR_BEEN_DAMAGED_BY_CAR, HasCarBeenDamagedByCar);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_RADIO_CHANNEL, GetRadioChannel);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_RANDOM_CAR_OF_TYPE_IN_AREA_NO_SAVE, GetRandomCarOfTypeInAreaNoSave);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_CAN_BURST_CAR_TYRES, SetCanBurstCarTyres);
    REGISTER_COMMAND_HANDLER(COMMAND_FIRE_HUNTER_GUN, FireHunterGun);
    REGISTER_COMMAND_HANDLER(COMMAND_CLEAR_CAR_LAST_DAMAGE_ENTITY, ClearCarLastDamageEntity);
    REGISTER_COMMAND_HANDLER(COMMAND_FREEZE_OBJECT_POSITION, FreezeObjectPosition);
    REGISTER_COMMAND_HANDLER(COMMAND_REMOVE_WEAPON_FROM_CHAR, RemoveWeaponFromChar);
    REGISTER_COMMAND_HANDLER(COMMAND_MAKE_HELI_COME_CRASHING_DOWN, MakeHeliComeCrashingDown);
    REGISTER_COMMAND_HANDLER(COMMAND_ADD_EXPLOSION_NO_SOUND, AddExplosionNoSound);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_OBJECT_AREA_VISIBLE, SetObjectAreaVisible);
    REGISTER_COMMAND_HANDLER(COMMAND_WAS_CUTSCENE_SKIPPED, WasCutsceneSkipped);
    REGISTER_COMMAND_HANDLER(COMMAND_DOES_VEHICLE_EXIST, DoesVehicleExist);
    REGISTER_COMMAND_HANDLER(COMMAND_ADD_SHORT_RANGE_SPRITE_BLIP_FOR_CONTACT_POINT, AddShortRangeSpriteBlipForContactPoint);
    REGISTER_COMMAND_HANDLER(COMMAND_FREEZE_CAR_POSITION_AND_DONT_LOAD_COLLISION, FreezeCarPositionAndDontLoadCollision);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_LOAD_COLLISION_FOR_CAR_FLAG, SetLoadCollisionForCarFlag);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_PROGRESS_PERCENTAGE, GetProgressPercentage);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_VEHICLE_TO_FADE_IN, SetVehicleToFadeIn);
    REGISTER_COMMAND_HANDLER(COMMAND_REGISTER_ODDJOB_MISSION_PASSED, RegisterOddjobMissionPassed);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_AUSTRALIAN_GAME, IsAustralianGame);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_ONSCREEN_COUNTER_FLASH_WHEN_FIRST_DISPLAYED, SetOnscreenCounterFlashWhenFirstDisplayed);
    REGISTER_COMMAND_HANDLER(COMMAND_SHUFFLE_CARD_DECKS, ShuffleCardDecks);
    REGISTER_COMMAND_HANDLER(COMMAND_FETCH_NEXT_CARD, FetchNextCard);
    REGISTER_COMMAND_HANDLER(COMMAND_ADD_TO_OBJECT_ROTATION_VELOCITY, AddToObjectRotationVelocity);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_OBJECT_ROTATION_VELOCITY, SetObjectRotationVelocity);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_OBJECT_STATIC, IsObjectStatic);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_ANGLE_BETWEEN_2D_VECTORS, GetAngleBetween2DVectors);
    REGISTER_COMMAND_HANDLER(COMMAND_DO_2D_RECTANGLES_COLLIDE, Do2DRectanglesCollide);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_OBJECT_ROTATION_VELOCITY, GetObjectRotationVelocity);
    REGISTER_COMMAND_HANDLER(COMMAND_ADD_VELOCITY_RELATIVE_TO_OBJECT_VELOCITY, AddVelocityRelativeToObjectVelocity);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_OBJECT_SPEED, GetObjectSpeed);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_2D_LINES_INTERSECT_POINT, Get2DLinesIntersectPoint);
    REGISTER_COMMAND_HANDLER(COMMAND_START_PLAYBACK_RECORDED_CAR, StartPlaybackRecordedCar);
    REGISTER_COMMAND_HANDLER(COMMAND_STOP_PLAYBACK_RECORDED_CAR, StopPlaybackRecordedCar);
    REGISTER_COMMAND_HANDLER(COMMAND_PAUSE_PLAYBACK_RECORDED_CAR, PausePlaybackRecordedCar);
    REGISTER_COMMAND_HANDLER(COMMAND_UNPAUSE_PLAYBACK_RECORDED_CAR, UnpausePlaybackRecordedCar);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_CAR_ESCORT_CAR_LEFT, SetCarEscortCar<MISSION_ESCORT_LEFT>);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_CAR_ESCORT_CAR_RIGHT, SetCarEscortCar<MISSION_ESCORT_RIGHT>);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_CAR_ESCORT_CAR_REAR, SetCarEscortCar<MISSION_ESCORT_REAR>);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_CAR_ESCORT_CAR_FRONT, SetCarEscortCar<MISSION_ESCORT_FRONT>);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_CHAR_STOPPED_IN_ANGLED_AREA_IN_CAR_2D, CharStoppedInAngledAreaInCar);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_CHAR_STOPPED_IN_ANGLED_AREA_IN_CAR_3D, CharStoppedInAngledAreaInCar);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_HEADING_FROM_VECTOR_2D, GetHeadingFromVector2D);
    REGISTER_COMMAND_HANDLER(COMMAND_LOAD_PATH_NODES_IN_AREA, LoadPathNodesInArea);
    REGISTER_COMMAND_HANDLER(COMMAND_RELEASE_PATH_NODES, ReleasePathNodes);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_PLAYBACK_GOING_ON_FOR_CAR, IsPlaybackGoingOnForCar);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_SENSE_RANGE, SetSenseRange);
    REGISTER_COMMAND_HANDLER(COMMAND_OPEN_SEQUENCE_TASK, OpenSequenceTask);
    REGISTER_COMMAND_HANDLER(COMMAND_CLOSE_SEQUENCE_TASK, CloseSequenceTask);
    REGISTER_COMMAND_HANDLER(COMMAND_PERFORM_SEQUENCE_TASK, PerformSequenceTask);
    REGISTER_COMMAND_HANDLER(COMMAND_CLEAR_SEQUENCE_TASK, ClearSequenceTask);
    REGISTER_COMMAND_HANDLER(COMMAND_ADD_ATTRACTOR, AddAttractor);
    REGISTER_COMMAND_HANDLER(COMMAND_CLEAR_ATTRACTOR, ClearAttractor);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_SCRIPT_TASK_STATUS, GetScriptTaskStatus);
    REGISTER_COMMAND_HANDLER(COMMAND_CREATE_GROUP, CreateGroup);
    REGISTER_COMMAND_HANDLER(COMMAND_CREATE_CHAR_AT_ATTRACTOR, CreateCharAtAttractor);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_GROUP_LEADER, SetGroupLeader);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_GROUP_MEMBER, SetGroupMember);
    REGISTER_COMMAND_HANDLER(COMMAND_REMOVE_GROUP, RemoveGroup);

    RegisterTaskHandlers();
}

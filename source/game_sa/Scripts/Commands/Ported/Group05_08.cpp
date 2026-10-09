#include <StdInc.h>

#include "../Commands.hpp"
#include <CommandParser/Parser.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <bit>
#include <span>
#include <utility>

#include "World.h"
#include "CarCtrl.h"
#include "Radar.h"
#include "Restart.h"
#include "Darkel.h"
#include "Explosion.h"
#include "Pickups.h"
#include "Garages.h"
#include "Garage.h"
#include "Gangs.h"
#include "Streaming.h"
#include "ScriptResourceManager.h"
#include "Coronas.h"
#include "CutsceneMgr.h"
#include "FireManager.h"
#include "ProjectileInfo.h"
#include "PathFind.h"
#include "WaterLevel.h"
#include "SurfaceInfos_c.h"
#include "Population.h"
#include "CivilianPed.h"
#include "TaskSimpleStandStill.h"
#include "PedGroups.h"
#include "MissionCleanup.h"
#include "Stats.h"
#include "AnimManager.h"
#include "Camera.h"
#include "Automobile.h"
#include "Boat.h"
#include "ModelIndices.h"
#include "PedModelInfo.h"

using namespace notsa::script;

/*!
* Script commands ported from the exe's per-100 group processors (CRunningScript::ProcessCommands500To599 .. 800To899)
* for the vanilla commands that had no handler of their own: ids 500..899 (S6-A part 2, groups g5..g8).
*
* Every handler below was written from the asm of the `case` in the group processor (table 0x8A6168 + 4 * group),
* NOT from the Ghidra decompilation. The case address is given in the comment above each handler.
* Group processors: g5 0x47E090, g6 0x47F370, g7 0x47FA30, g8 0x481300.
*
* Idioms used throughout:
*  - `fcom/fcomp x; fnstsw; test ah, N; jp/jnp/jne` => see the comments, the NaN behaviour is kept (an unordered compare
*    sets C0|C2|C3, so `test ah, 0x41` / `test ah, 5` / `test ah, 0x44` treat it differently from a plain C++ compare)
*  - `fcom -100.0f ... FindGroundZForCoord` (0x859014): the Z is looked up only if `z <= -100.0f` (ordered)
*  - `_ftol2` (0x821B40) = truncation to int64, only the low dword is used (NaN / out of range => 0, NOT 0x80000000)
*/

namespace {
//! Offsets the raw-offset accesses of the asm resolve to (checked against the headers)
static_assert(offsetof(CVehicle, m_nDoorLock) == 0x4F8);
static_assert(offsetof(CVehicle, m_fHealth) == 0x4C0);
static_assert(offsetof(CVehicle, m_nPrimaryColor) == 0x434);
static_assert(offsetof(CVehicle, m_nSecondaryColor) == 0x435);
static_assert(offsetof(CVehicle, m_nLastWeaponDamageType) == 0x508);
static_assert(offsetof(CVehicle, m_autoPilot) + offsetof(CAutoPilot, m_nCarMission) == 0x3BA);
static_assert(offsetof(CVehicle, m_autoPilot) + offsetof(CAutoPilot, m_nCruiseSpeed) == 0x3D0);
static_assert(offsetof(CVehicle, m_autoPilot) + offsetof(CAutoPilot, m_nTimeToStartMission) == 0x3AC);
static_assert(offsetof(CVehicle, m_autoPilot) + offsetof(CAutoPilot, m_vecDestinationCoors) == 0x3EC);
static_assert(offsetof(CAutomobile, m_fWheelsSuspensionCompression) == 0x7D4);
static_assert(offsetof(CAutomobile, autoFlags) == 0x868);
static_assert(offsetof(CBoat, m_nBoatFlags) == 0x5AC);
static_assert(offsetof(CPed, m_fArmour) == 0x548);
static_assert(offsetof(CPed, m_nAnimGroup) == 0x4D4);
static_assert(offsetof(CPed, m_pVehicle) == 0x58C);
static_assert(offsetof(CPed, m_pTargetedObject) == 0x71C);
static_assert(offsetof(CPlayerInfo, m_nTotalNumCollectables) == 0xC4);
static_assert(offsetof(CPlayerInfo, m_nMaxArmour) == 0x150);
static_assert(offsetof(CRunningScript, m_IP) == 0x14);
static_assert(BLIP_COORD == 4 && BLIP_CONTACT_POINT == 5);

//! `_ftol2` (0x821B40): truncates to a 64 bit integer (`fistp qword` + truncation fix-up; NaN / out of range => the "integer
//! indefinite" 0x8000000000000000) and the callers only use the LOW dword (EAX). NOT the saturating 0x80000000 of a 32 bit conversion.
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

//! Same as above, but followed by `fadd 0.5f` (0x858B8C) on the looked up Z (the `fstp` after it stores a float)
float GroundZIfAutoPlusHalf(float x, float y, float z) {
    if (z <= -100.0f) {
        return (float)((double)CWorld::FindGroundZForCoord(x, y) + 0.5);
    }
    return z;
}

//! Inlined `CVehicle::SetEngineOn(true)` (0x41BDD0): the engine can't be turned on if it's broken
void VehSetEngineOn(CVehicle& veh) {
    veh.vehicleFlags.bEngineOn = !veh.vehicleFlags.bEngineBroken;
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

// ============================================================================ g5 (ProcessCommands500To599 @0x47E090)

//! 500 IS_CAR_UPSIDEDOWN (case @0x47E0D1): car => compare flag. `up.z <= 0.3f` (0x858C24; `test ah, 0x41; jnp` => C0|C3, NaN => false)
bool IsCarUpsideDown(CVehicle& veh) {
    return veh.m_matrix->GetUp().z <= 0.3f;
}

//! 502 CANCEL_OVERRIDE_RESTART (case @0x47E19B): no params
void CancelOverrideRestart() {
    CRestart::CancelOverrideRestart();
}

//! 505 START_KILL_FRENZY (case @0x47E263): label(8), weapon, timeLimit, killsNeeded, model1, model2, model3, model4, soundAndMessages
//! The label is read BEFORE the 8 params; "DUMMY" (case insensitive) means no text, anything else is looked up in TheText.
//! `killsNeeded` is passed as a uint16 (movzx), `soundAndMessages` as `!= 0`, the last `needHeadShot` argument is 0.
void StartKillFrenzy(const char* label, eWeaponType weapon, int32 timeLimit, int32 killsNeeded, int32 model1, int32 model2, int32 model3, int32 model4, int32 soundAndMessages) {
    const GxtChar* const text = (_stricmp(label, "DUMMY") == 0) ? nullptr : TheText.Get(label); // 0x859EE0 = "DUMMY"
    CDarkel::StartFrenzy(weapon, timeLimit, (uint16)killsNeeded, model1, text, model2, model3, model4, soundAndMessages != 0, false);
}

//! 506 READ_KILL_FRENZY_STATUS (case @0x47E304): no params => 1 value (`movzx edx, ax` of CDarkel::ReadStatus)
int32 ReadKillFrenzyStatus() {
    return (int32)(uint16)CDarkel::ReadStatus();
}

//! 507 SQRT (case @0x47E331): float => float (`fsqrt`)
float Sqrt(float value) {
    return (float)std::sqrt((double)value);
}

//! 522 LOCK_CAR_DOORS (case @0x47E40A): car, lock. A null car is skipped (the 2 params are still read)
void LockCarDoors(CVehicle* veh, int32 lock) {
    if (veh) {
        veh->m_nDoorLock = (eCarLock)lock; // dword store to +0x4F8
    }
}

//! 523 EXPLODE_CAR (case @0x47E44F): car. `veh->BlowUpCar(nullptr, false)` = vtable slot 0xA4 (CAutomobile: 0x6B3780)
void ExplodeCar(CVehicle& veh) {
    veh.BlowUpCar(nullptr, false);
}

//! 524 ADD_EXPLOSION (case @0x47E48D): x, y, z, type
//! `CExplosion::AddExplosion(nullptr, nullptr, type, pos, 0, 1, -1.0f, 0)`
void AddExplosion(CVector pos, eExplosionType type) {
    CExplosion::AddExplosion(nullptr, nullptr, type, pos, 0, 1, -1.0f, 0);
}

//! 525 IS_CAR_UPRIGHT (case @0x47E500): car => compare flag. `!(up.z < 0.0f)` with `test ah, 1` (C0 is also set for NaN) => `up.z >= 0.0f`
bool IsCarUpright(CVehicle& veh) {
    return veh.m_matrix->GetUp().z >= 0.0f;
}

//! 531 CREATE_PICKUP (case @0x47E57A): model, type, x, y, z => 1 handle
//! z <= -100 => ground Z + 0.5f. The destination variable is peeked at with `CollectNextParameterWithoutIncreasingPC` and fed
//! to GetActualPickupIndex, but the result is unused (dead call kept).
int32 CreatePickup(CRunningScript& S, notsa::script::Model model, int32 type, CVector pos) {
    pos.z = GroundZIfAutoPlusHalf(pos.x, pos.y, pos.z);
    CPickups::GetActualPickupIndex(tPickupReference{ S.CollectNextParameterWithoutIncreasingPC() });
    return CPickups::GenerateNewOne(pos, (uint32)model.value, (ePickupType)(uint8)type, 0, 0, false, nullptr).num;
}

//! 532 HAS_PICKUP_BEEN_COLLECTED (case @0x47E662): pickup => compare flag
bool HasPickupBeenCollected(int32 pickup) {
    return CPickups::IsPickUpPickedUp(tPickupReference{ pickup });
}

//! 533 REMOVE_PICKUP (case @0x47E6AB): pickup
void RemovePickup(int32 pickup) {
    CPickups::RemovePickUp(tPickupReference{ pickup });
}

//! 534 SET_TAXI_LIGHTS (case @0x47E6D9): car, on. `CAutomobile::SetTaxiLight` is called without a type check
void SetTaxiLights(CVehicle& veh, int32 on) {
    static_cast<CAutomobile&>(veh).SetTaxiLight(on != 0);
}

//! 539 SET_TARGET_CAR_FOR_MISSION_GARAGE (case @0x47E78F): label(8), car
//! The garage name is read first, then 1 param. Nothing happens if the garage doesn't exist. A negative handle => no car
//! (the pool is not asked), otherwise the (possibly null) pool object.
void SetTargetCarForMissionGarage(const char* garageName, int32 carHandle) {
    const int16 garage = CGarages::GetGarageNumberByName(garageName);
    if (garage < 0) {
        return;
    }
    CGarages::SetTargetCarForMissionGarage(garage, carHandle < 0 ? nullptr : GetVehiclePool()->GetAtRef(carHandle));
}

//! 548 SET_CAR_HEALTH (case @0x47E9E0): car, health (`fild`, stored as float)
void SetCarHealth(CVehicle& veh, int32 health) {
    veh.m_fHealth = (float)health;
}

//! 551 GET_CAR_HEALTH (case @0x47EA74): car => 1 int (`_ftol` of the float health)
int32 GetCarHealth(CVehicle& veh) {
    return Ftol((double)veh.m_fHealth);
}

//! 553 CHANGE_CAR_COLOUR (case @0x47EA97): car, primary, secondary (bytes)
void ChangeCarColour(CVehicle& veh, int32 primary, int32 secondary) {
    veh.m_nPrimaryColor   = (uint8)primary;
    veh.m_nSecondaryColor = (uint8)secondary;
}

//! 567 SET_GANG_WEAPONS (case @0x47ED00): gang (uint16), weapon1, weapon2, weapon3
void SetGangWeapons(int32 gang, eWeaponType w1, eWeaponType w2, eWeaponType w3) {
    CGangs::SetGangWeapons((int16)(uint16)gang, w1, w2, w3);
}

//! 572 LOAD_SPECIAL_CHARACTER (case @0x47EDA9): slot, name(8)
//! The slot is decremented, then the name is read, lowercased (A-Z only, all 8 bytes) and requested (flags 0xC), and the
//! slot is registered in the script resource manager (id + 0x122, type 2).
void LoadSpecialCharacter(CRunningScript& S, int32 slot, const char* name) {
    const int32 id = slot - 1;
    char        lowered[9]{};
    strncpy_s(lowered, name, 8);
    for (char& c : std::span{ lowered, 8 }) {
        if (c >= 'A' && c <= 'Z') {
            c += 0x20;
        }
    }
    CStreaming::RequestSpecialChar(id, lowered, 0xC);
    CTheScripts::ScriptResourceManager.AddToResourceManager(id + 0x122, RESOURCE_TYPE_MODEL_OR_SPECIAL_CHAR, &S);
}

//! 573 HAS_SPECIAL_CHARACTER_LOADED (case @0x47EE2D): slot => compare flag (the slot is decremented first)
bool HasSpecialCharacterLoaded(int32 slot) {
    return CStreaming::HasSpecialCharLoaded(slot - 1);
}

//! 580 SET_CUTSCENE_OFFSET (case @0x47EEB8): x, y, z (0x47E070 copies the 3 dwords to CCutsceneMgr::ms_cutsceneOffset)
void SetCutsceneOffset(CVector offset) {
    CCutsceneMgr::ms_cutsceneOffset = offset;
}

//! 581 SET_ANIM_GROUP_FOR_CHAR (case @0x47EF01): char, name(16)
//! Linear search of the anim group names (case insensitive); when nothing matches the index ends up == the number of groups.
void SetAnimGroupForChar(CPed& ped, const char* name) {
    uint16 i     = 0;
    bool   found = false;
    const int32 numGroups = (int32)CAnimManager::GetAssocGroupDefs().size(); // ms_numAnimAssocDefinitions
    if (numGroups > 0) {
        do {
            if (found) {
                break;
            }
            if (_stricmp(name, CAnimManager::GetAnimGroupName((AssocGroupId)i)) == 0) {
                found = true;
            } else {
                i++;
            }
        } while ((int32)i < numGroups);
    }
    ped.m_nAnimGroup = (AssocGroupId)i; // dword store (movzx ecx, si)
}

//! 591 DRAW_CORONA (case @0x47F08E): x, y, z, size, type, flare, r, g, b
//! The corona id is `ftol((double)(uint32)ftol((y + 3000) * 12001) + (x + 3000)) + (uintptr)m_IP + (uintptr)script`
//! (m_IP is the IP AFTER the 9 params). z <= -100 => ground Z (no offset).
void DrawCorona(CRunningScript& S, float x, float y, float z, float size, int32 type, int32 flare, int32 r, int32 g, int32 b) {
    const CVector pos{ x, y, GroundZIfAuto(x, y, z) };

    const int32  t1 = Ftol(((double)y - (double)-3000.0f) * (double)12001.0f); // 0x859A90, 0x859F1C
    double       u  = (double)t1;
    if (t1 < 0) {
        u += (double)4294967296.0f; // 0x858C54
    }
    const int32  t2 = Ftol(u + ((double)x - (double)-3000.0f));
    const uint32 id = (uint32)t2 + (uint32)(uintptr_t)S.m_IP + (uint32)(uintptr_t)&S;

    CCoronas::RegisterCorona(
        id,
        nullptr,
        (uint8)r, (uint8)g, (uint8)b,
        0xFF,
        pos,
        size,
        450.0f,
        (eCoronaType)(uint8)type,
        (eCoronaFlareType)(uint8)flare,
        (eCoronaReflType)1,
        (eCoronaLOSCheck)0,
        (eCoronaTrail)0,
        0.0f,
        false,
        1.5f,
        false,
        15.0f,
        false,
        false
    );
}

// ============================================================================ g6 (ProcessCommands659To697 @0x47F370)

//! 659 GET_CONTROLLER_MODE (case @0x47F399): no params => 1 value, always 0
int32 GetControllerMode() {
    return 0;
}

//! 660 SET_CAN_RESPRAY_CAR (case @0x47F3B8): car, can. Bit 1 of the byte at +0x868 (CAutomobile::autoFlags.bShouldNotChangeColour)
//! is `!can`; the write is done without a type check (the byte is accessed on whatever vehicle it is).
void SetCanRespraycar(CVehicle& veh, int32 can) {
    static_cast<CAutomobile&>(veh).autoFlags.bShouldNotChangeColour = (can == 0);
}

//! 662 UNLOAD_SPECIAL_CHARACTER (case @0x47F40D): slot. The slot is decremented, if it was registered (id + 0x122, type 2)
//! for this script it's released (0x40B490 = CStreaming::SetMissionDoesntRequireSpecialChar).
void UnloadSpecialCharacter(CRunningScript& S, int32 slot) {
    const int32 id = slot - 1;
    if (CTheScripts::ScriptResourceManager.RemoveFromResourceManager(id + 0x122, RESOURCE_TYPE_MODEL_OR_SPECIAL_CHAR, &S)) {
        CStreaming::SetMissionDoesntRequireSpecialChar(id);
    }
}

//! 665 ACTIVATE_GARAGE (case @0x47F4B6): name(8). Nothing happens if the garage doesn't exist
void ActivateGarage(const char* garageName) {
    const int16 garage = CGarages::GetGarageNumberByName(garageName);
    if (garage >= 0) {
        CGarages::ActivateGarage(garage);
    }
}

//! 667 CREATE_OBJECT_NO_OFFSET (case @0x47F4F1): model, x, y, z => 1 handle
//! (Like CREATE_OBJECT, but z <= -100 => the ground Z with NO base offset, and no PutToGroundIfTooLow)
CObject* CreateObjectNoOffset(CRunningScript& S, notsa::script::Model model, CVector pos) {
    const auto mi = CModelInfo::GetModelInfo(model);
    mi->m_nAlpha  = 255u;

    auto* const object = CObject::Create(model, false);
    object->m_nObjectType = (S.m_IsExternal || S.m_ExternalType != -1) ? OBJECT_MISSION2 : OBJECT_MISSION;

    pos.z = GroundZIfAuto(pos.x, pos.y, pos.z);
    object->SetPosn(pos.x, pos.y, pos.z);
    object->SetOrientation(0.0f, 0.0f, 0.0f);
    object->UpdateRwMatrix();
    object->UpdateRwFrame();
    if (mi->AsLodAtomicModelInfoPtr()) { // vtable slot 3
        object->SetupBigBuilding();
    }

    CTheScripts::ClearSpaceForMissionEntity(pos, object);
    CWorld::Add(object);

    if (S.m_UsesMissionCleanup) {
        CTheScripts::MissionCleanUp.AddEntityToList(*object);
    }
    return object;
}

//! 675 SWITCH_WIDESCREEN (case @0x47F684): on
void SwitchWidescreen(int32 on) {
    if (on) {
        TheCamera.SetWideScreenOn();
    } else {
        TheCamera.SetWideScreenOff();
    }
}

//! 679 ADD_SPRITE_BLIP_FOR_CONTACT_POINT (case @0x47F6BD): x, y, z, sprite => 1 blip
//! `SetCoordBlip(BLIP_CONTACT_POINT, pos, colour 2, BLIP_DISPLAY_BOTH, scriptName)` then `SetBlipSprite`
int32 AddSpriteBlipForContactPoint(CRunningScript& S, CVector pos, int32 sprite) {
    pos.z = GroundZIfAuto(pos.x, pos.y, pos.z);
    CRadar::GetActualBlipArrayIndex(S.CollectNextParameterWithoutIncreasingPC()); // result unused
    const auto blip = CRadar::SetCoordBlip(BLIP_CONTACT_POINT, pos, (eBlipColour)2, BLIP_DISPLAY_BOTH, S.m_szName);
    CRadar::SetBlipSprite(blip, (eRadarSprite)sprite);
    return (int32)blip;
}

//! 680 ADD_SPRITE_BLIP_FOR_COORD (case @0x47F75D): x, y, z, sprite => 1 blip
//! `SetCoordBlip(BLIP_COORD, pos, colour 5, BLIP_DISPLAY_BOTH, scriptName)` then `SetBlipSprite`
int32 AddSpriteBlipForCoord(CRunningScript& S, CVector pos, int32 sprite) {
    pos.z = GroundZIfAuto(pos.x, pos.y, pos.z);
    CRadar::GetActualBlipArrayIndex(S.CollectNextParameterWithoutIncreasingPC()); // result unused
    const auto blip = CRadar::SetCoordBlip(BLIP_COORD, pos, (eBlipColour)5, BLIP_DISPLAY_BOTH, S.m_szName);
    CRadar::SetBlipSprite(blip, (eRadarSprite)sprite);
    return (int32)blip;
}

//! 682 SET_CAR_ONLY_DAMAGED_BY_PLAYER (case @0x47F85E): car, on. Physical flag 0x400000 (+0x40) = bInvulnerable
void SetCarOnlyDamagedByPlayer(CVehicle& veh, int32 on) {
    veh.physicalFlags.bInvulnerable = (on != 0);
}

//! 697 DEACTIVATE_GARAGE (case @0x47F976): name(8). Nothing happens if the garage doesn't exist
void DeactivateGarage(const char* garageName) {
    const int16 garage = CGarages::GetGarageNumberByName(garageName);
    if (garage >= 0) {
        CGarages::DeActivateGarage(garage);
    }
}

// ============================================================================ g7 (ProcessCommands703To800 @0x47FA30)

//! 703 IS_CAR_IN_WATER (case @0x47FA67): car => compare flag. A null car is "false".
//! Submerged flag (+0x40 bit 8), or a Vortex (model 0x21B) whose first suspension compression is `< 1.0f` and whose first
//! wheel contact point is on shallow water.
bool IsCarInWater(CVehicle* veh) {
    if (!veh) {
        return false;
    }
    if (veh->physicalFlags.bSubmergedInWater) {
        return true;
    }
    if ((int16)veh->m_nModelIndex != 0x21B) {
        return false;
    }
    auto& automobile = static_cast<CAutomobile&>(*veh);
    if (!(automobile.m_fWheelsSuspensionCompression[0] < 1.0f)) { // `test ah, 5; jp` => !(x < 1.0f), NaN => false
        return false;
    }
    return g_surfaceInfos.IsShallowWater(automobile.m_wheelColPoint[0].m_nSurfaceTypeB); // +0x747
}

//! 705 GET_CLOSEST_CAR_NODE (case @0x47FBDE): x, y, z => x, y, z (3 values) + compare flag
//! z <= -100 => ground Z. Not found => (0, 0, 0) and false.
MultiRet<CVector, bool> GetClosestCarNode(CVector pos) {
    pos.z = GroundZIfAuto(pos.x, pos.y, pos.z);
    const auto node = ThePaths.FindNodeClosestToCoors(pos, PATH_TYPE_VEH, std::bit_cast<float>(0x497423FEu), 0, 1, 0, 0, 0);
    bool       found{};
    const auto nodePos = ThePaths.FindNodeCoorsForScript(node, &found);
    if (found) {
        return { nodePos, true };
    }
    return { CVector{ 0.0f, 0.0f, 0.0f }, false };
}

//! 706 CAR_GOTO_COORDINATES_ACCURATE (case @0x47FCA7): car, x, y, z
//! z <= -100 => ground Z; then `z += veh->GetDistanceFromCentreOfMassToBaseOfModel()`.
void CarGotoCoordinatesAccurate(CVehicle& veh, CVector pos) {
    pos.z = GroundZIfAuto(pos.x, pos.y, pos.z);
    pos.z = (float)((double)veh.GetDistanceFromCentreOfMassToBaseOfModel() + (double)pos.z);

    const bool joined = CCarCtrl::JoinCarWithRoadSystemGotoCoors(&veh, pos, false, false);
    auto&      ap     = veh.m_autoPilot;
    if (ap.m_nCarMission != MISSION_PLANE_CRASH_AND_BURN && ap.m_nCarMission != MISSION_HELI_CRASH_AND_BURN) { // 0x39, 0x3A
        ap.m_nCarMission = joined ? MISSION_GOTOCOORDINATES_STRAIGHTLINE_ACCURATE : MISSION_GOTOCOORDINATES_ACCURATE; // 13 / 12
    }
    veh.SetStatus(STATUS_PHYSICS);
    VehSetEngineOn(veh);
    if (ap.m_nCruiseSpeed <= 1) { // `max(cruise, 1)` (unsigned)
        ap.m_nCruiseSpeed = 1;
    }
    ap.m_nTimeToStartMission = CTimer::GetTimeInMS();
}

//! Shared tail of 714 / 716: `TheCamera.IsSphereVisible(entity->GetBoundCentre(), colModel->radius)`
bool IsEntitySphereVisible(CEntity& entity) {
    const float radius = CModelInfo::GetModelInfo((int16)entity.m_nModelIndex)->GetColModel()->GetBoundRadius(); // +0x24
    const auto  centre = entity.GetBoundCentre();
    return TheCamera.IsSphereVisible(centre, radius);
}

//! 714 IS_CAR_ON_SCREEN (case @0x47FDA1): car => compare flag
bool IsCarOnScreen(CVehicle& veh) {
    return IsEntitySphereVisible(veh);
}

//! 716 IS_OBJECT_ON_SCREEN (case @0x47FE34): object => compare flag
bool IsObjectOnScreen(CObject& obj) {
    return IsEntitySphereVisible(obj);
}

//! 720 IS_SCRIPT_FIRE_EXTINGUISHED (case @0x47FF83): fire => compare flag. Invalid / out of range (0..59) => false
bool IsScriptFireExtinguished(int32 fire) {
    const auto idx = CTheScripts::GetActualScriptThingIndex(fire, SCRIPT_THING_FIRE);
    if (idx < 0 || idx >= 60) {
        return false;
    }
    return gFireManager.IsScriptFireExtinguished((int16)idx);
}

//! 721 REMOVE_SCRIPT_FIRE (case @0x47FFC3): fire. Invalid / out of range (0..59) => nothing
void RemoveScriptFire(int32 fire) {
    const auto idx = CTheScripts::GetActualScriptThingIndex(fire, SCRIPT_THING_FIRE);
    if (idx >= 0 && idx < 60) {
        gFireManager.RemoveScriptFire((int16)idx);
    }
}

//! 723 BOAT_GOTO_COORDS (case @0x480008): boat, x, y, z
//! `if (z <= -100) GetWaterLevel(x, y, z, &level, true, nullptr)`; the destination is (x, y, level).
//! BUG (original): when `z > -100` the water level is never written and the (uninitialised) stack slot is stored as the destination Z.
void BoatGotoCoords(CVehicle& veh, CVector pos) {
    float level = notsa::IsFixBugs() ? pos.z : 0.0f; // BUG: uninitialised in the original
    if (pos.z <= -100.0f) {
        CWaterLevel::GetWaterLevel(pos.x, pos.y, pos.z, level, true, nullptr);
    }
    auto& ap = veh.m_autoPilot;
    if (ap.m_nCarMission != MISSION_PLANE_CRASH_AND_BURN && ap.m_nCarMission != MISSION_HELI_CRASH_AND_BURN) {
        ap.m_nCarMission = MISSION_GOTOCOORDINATES_ASTHECROWSWIMS; // 14
    }
    ap.m_vecDestinationCoors = CVector{ pos.x, pos.y, level };
    veh.SetStatus(STATUS_PHYSICS);
    VehSetEngineOn(veh);
    if (ap.m_nCruiseSpeed <= 1) {
        ap.m_nCruiseSpeed = 1;
    }
    ap.m_nTimeToStartMission = CTimer::GetTimeInMS();
}

//! 724 BOAT_STOP (case @0x4800E5): boat
void BoatStop(CVehicle& veh) {
    auto& ap = veh.m_autoPilot;
    if (ap.m_nCarMission != MISSION_PLANE_CRASH_AND_BURN && ap.m_nCarMission != MISSION_HELI_CRASH_AND_BURN) {
        ap.m_nCarMission = MISSION_NONE;
    }
    veh.SetStatus(STATUS_PHYSICS);
    veh.vehicleFlags.bEngineOn = false; // byte +0x428, bit 0x10
    ap.m_nCruiseSpeed          = 0;
}

//! 731 SET_BOAT_CRUISE_SPEED (case @0x480288): boat, speed (float; `_ftol`, low byte stored)
void SetBoatCruiseSpeed(CVehicle& veh, float speed) {
    veh.m_autoPilot.m_nCruiseSpeed = (uint8)Ftol((double)speed);
}

//! 737 CREATE_MONEY_PICKUP (case @0x48056A): x, y, z, amount, permanent => 1 handle
//! z <= -100 => ground Z + 0.5f. permanent => pickup type 0x13, else 8 (PICKUP_MONEY). Model = MI_MONEY (word @0x8CD59C).
//! The dead GetActualPickupIndex(CollectNextParameterWithoutIncreasingPC()) call is kept.
int32 CreateMoneyPickup(CRunningScript& S, CVector pos, int32 amount, int32 permanent) {
    pos.z = GroundZIfAutoPlusHalf(pos.x, pos.y, pos.z);
    CPickups::GetActualPickupIndex(tPickupReference{ S.CollectNextParameterWithoutIncreasingPC() });
    const auto type = permanent != 0 ? (ePickupType)0x13 : PICKUP_MONEY;
    return CPickups::GenerateNewOne(pos, (uint32)(uint16)ModelIndices::MI_MONEY, type, (uint32)amount, 0, false, nullptr).num;
}

//! 740 LOAD_CUTSCENE (case @0x4806FB): name(8)
void LoadCutscene(const char* name) {
    CCutsceneMgr::LoadCutsceneData(name);
}

//! 743 START_CUTSCENE (case @0x48072B): no params
void StartCutscene() {
    CCutsceneMgr::StartCutscene();
}

//! 744 GET_CUTSCENE_TIME (case @0x48073F): no params => 1 value. The case stores EAX of CCutsceneMgr::GetCutsceneTimeInMilleseconds
//! (0x5B0550 = `fld [ms_cutsceneTimerS]; fmul 1000.0f (0x858C4C); jmp _ftol2`), i.e. the low dword of `trunc(timer * 1000.0f)`.
//! NOTE: computed here and not through CCutsceneMgr::GetCutsceneTimeInMilleseconds - that function does `(uint64)timerS * 1000`
//! (truncates the seconds BEFORE scaling) and is wrong against the exe.
int32 GetCutsceneTime() {
    return Ftol((double)CCutsceneMgr::ms_cutsceneTimerS * (double)1000.0f);
}

//! 745 HAS_CUTSCENE_FINISHED (case @0x480761): no params => compare flag
bool HasCutsceneFinished() {
    return CCutsceneMgr::HasCutsceneFinished();
}

//! 746 CLEAR_CUTSCENE (case @0x48078A): no params
void ClearCutscene() {
    CCutsceneMgr::DeleteCutsceneData();
}

//! 749 SET_COLLECTABLE1_TOTAL (case @0x4807CF): total (`CWorld::Players[PlayerInFocus].m_nTotalNumCollectables`)
void SetCollectable1Total(int32 total) {
    CWorld::Players[CWorld::PlayerInFocus].m_nTotalNumCollectables = (uint32)total;
}

//! 750 IS_PROJECTILE_IN_AREA (case @0x4807FF): x1, y1, z1, x2, y2, z2 => compare flag
//! Each axis is ordered (swapped iff `a > b`, ordered), then IsProjectileInRange(x1, x2, y1, y2, z1, z2, false).
//! (A debug-draw call guarded by the constant byte 0 at 0x859CF8 is never executed in the exe and is not ported.)
bool IsProjectileInArea(float x1, float y1, float z1, float x2, float y2, float z2) {
    if (x1 > x2) {
        std::swap(x1, x2);
    }
    if (y1 > y2) {
        std::swap(y1, y2);
    }
    if (z1 > z2) {
        std::swap(z1, z2);
    }
    return CProjectileInfo::IsProjectileInRange(x1, x2, y1, y2, z1, z2, false);
}

//! 760 GET_CAR_FORWARD_X (case @0x4809B2): car => 1 float. `fx / sqrt(fy*fy + fx*fx)` (x87 intermediates)
float GetCarForwardX(CVehicle& veh) {
    const auto& fwd = veh.m_matrix->GetForward();
    return (float)((double)fwd.x / std::sqrt((double)fwd.y * (double)fwd.y + (double)fwd.x * (double)fwd.x));
}

//! 761 GET_CAR_FORWARD_Y (case @0x480A04): car => 1 float. `fy / sqrt(fy*fy + fx*fx)` (x87 intermediates)
float GetCarForwardY(CVehicle& veh) {
    const auto& fwd = veh.m_matrix->GetForward();
    return (float)((double)fwd.y / std::sqrt((double)fwd.y * (double)fwd.y + (double)fwd.x * (double)fwd.x));
}

//! 762 CHANGE_GARAGE_TYPE (case @0x480A57): name(8), type (byte). Nothing happens if the garage doesn't exist.
void ChangeGarageType(const char* garageName, int32 type) {
    const int16 garage = CGarages::GetGarageNumberByName(garageName);
    if (garage >= 0) {
        CGarages::ChangeGarageType(garage, (eGarageType)(uint8)type, 0);
    }
}

//! 781 SET_PROGRESS_TOTAL (case @0x480D28): total => `CStats::SetStatValue(STAT_TOTAL_PROGRESS, (float)total)`
void SetProgressTotal(int32 total) {
    CStats::SetStatValue(STAT_TOTAL_PROGRESS, (float)total);
}

//! 792 REGISTER_MISSION_PASSED (case @0x480D75): name(8)
//! (The label is also looked up in TheText, but the result is unused.)
void RegisterMissionPassed(const char* name) {
    (void)TheText.Get(name);
    { // `strncpy(CStats::LastMissionPassedName, label, 8)` (0x821F40): copies at most 8 chars, pads with NULs, no terminator
        auto&        dst = CStats::LastMissionPassedName;
        const size_t len = strnlen(name, sizeof(dst));
        memcpy(dst, name, len);
        memset(dst + len, 0, sizeof(dst) - len);
    }
    CStats::IncrementStat(STAT_MISSIONS_PASSED, 1.0f); // 0x93
    CStats::IncrementStat(STAT_TOTAL_LEGITIMATE_KILLS, CStats::GetStatValue(STAT_KILLS_SINCE_LAST_CHECKPOINT)); // 0xB1 += value(0xB0)
    CStats::SetStatValue(STAT_KILLS_SINCE_LAST_CHECKPOINT, 0.0f);
    CTheScripts::LastMissionPassedTime = CTimer::GetTimeInMS();
}

//! 798 HAS_CAR_BEEN_DAMAGED_BY_WEAPON (case @0x480E8A): car, weapon => compare flag. A null car => false.
//! ANYMELEE (0x38) / ANYWEAPON (0x39) go through CDarkel::CheckDamagedWeaponType, others compare the (signed byte) last damage type.
bool HasCarBeenDamagedByWeapon(CVehicle* veh, int32 weapon) {
    if (!veh) {
        return false;
    }
    const int32 lastDamage = (int8)veh->m_nLastWeaponDamageType;
    if (weapon == WEAPON_ANYMELEE || weapon == WEAPON_ANYWEAPON) {
        return CDarkel::CheckDamagedWeaponType((eWeaponType)lastDamage, (eWeaponType)weapon);
    }
    return lastDamage == weapon;
}

// ============================================================================ g8 (ProcessCommands801To899 @0x481300)

//! 803 ANCHOR_BOAT (case @0x481435): boat, anchored. CBoat::m_nBoatFlags.bLockedToXY (byte +0x5AC, mask 4)
void AnchorBoat(CVehicle& veh, int32 anchored) {
    static_cast<CBoat&>(veh).m_nBoatFlags.bLockedToXY = (anchored != 0);
}

//! 805 START_CAR_FIRE (case @0x481479): car => 1 fire handle. `StartScriptFire(carPos, car, 0.8f, 1, 0, 1)`
int32 StartCarFire(CVehicle& veh) {
    const CVector pos = veh.GetPosition();
    return gFireManager.StartScriptFire(pos, &veh, 0.8f, 1, 0, 1);
}

//! 807 GET_RANDOM_CAR_OF_TYPE_IN_AREA (case @0x48150A): x1, y1, x2, y2, model => 1 handle (-1 if none)
//! Walks the vehicle pool from the LAST slot down to 0 and takes the first car that is an automobile / bike (appearance 1 or 2),
//! not a law enforcer (bit 0 of the flags at +0x428), has the model (or model < 0 for any), `CanBeDeleted`, and is within the 2D area.
//! It is flagged as a mission vehicle (created by 2) and added to the mission cleanup list if the script uses it.
int32 GetRandomCarOfTypeInArea(CRunningScript& S, float x1, float y1, float x2, float y2, int32 modelId) {
    auto&   pool   = *GetVehiclePool();
    int32   result = -1;
    int32   idx    = (int32)pool.GetSize();
    while (idx > 0) {
        idx--;
        if (result != -1) {
            break;
        }
        CVehicle* const veh = pool.GetAt(idx);
        if (!veh) {
            continue;
        }
        const auto appearance = veh->GetVehicleAppearance();
        if (appearance != VEHICLE_APPEARANCE_AUTOMOBILE && appearance != VEHICLE_APPEARANCE_BIKE) {
            continue;
        }
        if (veh->vehicleFlags.bIsLawEnforcer) {
            continue;
        }
        if ((int16)veh->m_nModelIndex != modelId && modelId >= 0) {
            continue;
        }
        if (!veh->CanBeDeleted()) {
            continue;
        }
        if (!veh->IsWithinArea(x1, y1, x2, y2)) {
            continue;
        }
        result = pool.GetRef(veh);
        veh->SetVehicleCreatedBy(MISSION_VEHICLE); // 2
        if (S.m_UsesMissionCleanup) {
            CTheScripts::MissionCleanUp.AddEntityToList(result, MISSION_CLEANUP_ENTITY_TYPE_VEHICLE);
        }
    }
    return result;
}

//! 811 CREATE_PICKUP_WITH_AMMO (case @0x481678): model, type, ammo, x, y, z => 1 handle
//! z <= -100 => ground Z + 0.5f. The dead GetActualPickupIndex(CollectNextParameterWithoutIncreasingPC()) call is kept.
int32 CreatePickupWithAmmo(CRunningScript& S, notsa::script::Model model, int32 type, int32 ammo, CVector pos) {
    pos.z = GroundZIfAutoPlusHalf(pos.x, pos.y, pos.z);
    CPickups::GetActualPickupIndex(tPickupReference{ S.CollectNextParameterWithoutIncreasingPC() });
    return CPickups::GenerateNewOne(pos, (uint32)model.value, (ePickupType)(uint8)type, (uint32)ammo, 0, false, nullptr).num;
}

//! 821 SET_FREE_RESPRAYS (case @0x481808): on => CGarages::RespraysAreFree (int8)
void SetFreeResprays(int32 on) {
    CGarages::RespraysAreFree = (on != 0);
}

//! 824 SET_CAR_VISIBLE (case @0x481880): car, visible. Entity flag 0x80 (+0x1C) = bIsVisible
void SetCarVisible(CVehicle& veh, int32 visible) {
    veh.m_bIsVisible = (visible != 0);
}

//! 825 IS_AREA_OCCUPIED (case @0x4818BB): x1, y1, z1, x2, y2, z2, buildings, vehicles, peds, objects, dummies => compare flag
//! Each axis is ordered (swapped iff `a > b`, ordered), then `FindObjectsIntersectingCube(min, max, &count, 2, nullptr, ...)`; true if count > 0.
bool IsAreaOccupied(float x1, float y1, float z1, float x2, float y2, float z2, int32 buildings, int32 vehicles, int32 peds, int32 objects, int32 dummies) {
    if (x1 > x2) {
        std::swap(x1, x2);
    }
    if (y1 > y2) {
        std::swap(y1, y2);
    }
    if (z1 > z2) {
        std::swap(z1, z2);
    }
    const CVector cornerA{ x1, y1, z1 };
    const CVector cornerB{ x2, y2, z2 };
    int16         count = 0;
    CWorld::FindObjectsIntersectingCube(cornerA, cornerB, &count, 2, nullptr, buildings != 0, vehicles != 0, peds != 0, objects != 0, dummies != 0);
    return count > 0;
}

//! Shared code of 845 / 846 (inlined in the exe): builds the XY extent of 4 corners of the object's collision box transformed by `M`
//! (corners (min,min,min), (max,min,max), (max,max,min), (min,max,max); the min / max chains compare in the exe's order, so the
//! NaN behaviour is kept) and returns whether `FindObjectsIntersectingAngledCollisionBox` finds something (vehicles + peds).
bool IsObjectPlaceBlocked(CObject& obj, const CMatrix& M, const CVector& point) {
    const CColModel* const col = CModelInfo::GetModelInfo((int16)obj.m_nModelIndex)->GetColModel();
    const CVector&         mn  = col->m_boundBox.m_vecMin;
    const CVector&         mx  = col->m_boundBox.m_vecMax;

    const CVector r1 = TransformPointOriginal(M, { mn.x, mn.y, mn.z });
    const CVector r2 = TransformPointOriginal(M, { mx.x, mn.y, mx.z });
    const CVector r3 = TransformPointOriginal(M, { mx.x, mx.y, mn.z });
    const CVector r4 = TransformPointOriginal(M, { mn.x, mx.y, mx.z });

    //! min(a, b) = `a < b ? a : b`, max(a, b) = `a > b ? a : b` (a tie or NaN picks `b`). The exe chains them as
    //! t = op(r2, r1); t = op(r3, t); t = op(r4, t) (the new candidate is only taken if it is strictly smaller / greater).
    const auto Min = [](float a, float b) { return a < b ? a : b; };
    const auto Max = [](float a, float b) { return a > b ? a : b; };
    const float minX = Min(r4.x, Min(r3.x, Min(r2.x, r1.x)));
    const float maxX = Max(r4.x, Max(r3.x, Max(r2.x, r1.x)));
    const float minY = Min(r4.y, Min(r3.y, Min(r2.y, r1.y)));
    const float maxY = Max(r4.y, Max(r3.y, Max(r2.y, r1.y)));

    int16 count = 0;
    CWorld::FindObjectsIntersectingAngledCollisionBox(
        col->m_boundBox, M, point,
        minX, minY, maxX, maxY,
        &count, 2, nullptr,
        false, true, true, false, false
    );
    return count > 0;
}

//! 845 ROTATE_OBJECT (case @0x481CA5): object, targetAngle(deg), rate(deg), collisionCheck => compare flag (reached the angle)
//! The current heading in degrees (heading * 57.29578f, +360 if negative) moves towards the target by at most `rate` along the
//! shorter way round. With the collision check the rotated collision box is tested first: if something blocks it the object
//! isn't rotated and the command reports "done" (true). Not blocked => SetHeading, UpdateRwMatrix, UpdateRwFrame.
bool RotateObject(CObject& obj, float target, float rate, int32 collisionCheck) {
    constexpr float RAD_TO_DEG = 57.2957763671875f; // 0x859878
    constexpr float DEG_TO_RAD = 0.01745329238474369f; // 0x8595EC

    double cur = (double)obj.GetHeading() * (double)RAD_TO_DEG;
    if (cur < 0.0) {
        cur += 360.0;
    }
    if (cur == (double)target) { // fcom + `test ah, 0x44; jp`: only an exact (ordered) equality
        return true;
    }

    float d1 = (float)((double)target - cur); // stored to memory
    double e = cur - (double)target;          // stays on the FPU stack
    if (d1 < 0.0f) {
        d1 = (float)((double)d1 + 360.0);
    }
    if (e < 0.0) {
        e += 360.0;
    }

    float newAngle;
    if (d1 < e) {
        newAngle = (d1 < rate) ? target : (float)((double)rate + cur);
    } else {
        newAngle = (e < (double)rate) ? target : (float)(cur - (double)rate);
    }

    if (collisionCheck != 0) {
        const CVector pos = obj.GetPosition();
        CMatrix       M;
        M.SetRotateZ((float)((double)newAngle * (double)DEG_TO_RAD));
        M.GetPosition().x = M.GetPosition().x + pos.x;
        M.GetPosition().y = M.GetPosition().y + pos.y;
        M.GetPosition().z = M.GetPosition().z + pos.z;
        if (IsObjectPlaceBlocked(obj, M, pos)) {
            return true;
        }
    }

    obj.SetHeading((float)((double)newAngle * (double)DEG_TO_RAD));
    obj.UpdateRwMatrix();
    obj.UpdateRwFrame();
    return newAngle == target; // `test ah, 0x44; jp` => only an exact (ordered) equality
}

//! 846 SLIDE_OBJECT (case @0x482339): object, targetX, targetY, targetZ, speedX, speedY, speedZ, collisionCheck => compare flag
//! If the object is exactly at the target => true. Else every axis moves towards the target by at most its speed (snapping to the
//! target when closer than the speed). With the collision check the (moved) collision box is tested first: if something blocks
//! it the object stays and the command reports "done" (true). Else the object is teleported; the result is "new position == target".
bool SlideObject(CObject& obj, float tx, float ty, float tz, float sx, float sy, float sz, int32 collisionCheck) {
    const CVector curPos = obj.GetPosition();
    float cx = curPos.x, cy = curPos.y, cz = curPos.z;

    if (cx == tx && cy == ty && cz == tz) { // each `fcomp` + `test ah, 0x44; jp`: exact ordered equality
        return true;
    }

    //! One axis: d = c - t; d >= 0: `d > s ? c - s : t`; d < 0 or NaN: `-d > s ? s + c : t`
    //! X: `d` stays on the FPU stack (extended precision, NOT rounded to float); Y and Z: `d` is spilled to a float temp first.
    const auto Step = [](float c, float t, float s, bool spill) -> float {
        double d = (double)c - (double)t;
        if (spill) {
            d = (double)(float)d;
        }
        if (!(d < 0.0) && !std::isnan(d)) { // `fcom 0; test ah, 1; jne neg`: C0 (less) is also set for NaN
            if (d > (double)s) {
                return (float)((double)c - (double)s);
            }
            return t;
        }
        if (-d > (double)s) {
            return (float)((double)s + (double)c);
        }
        return t;
    };
    cx = Step(cx, tx, sx, false);
    cy = Step(cy, ty, sy, true);
    cz = Step(cz, tz, sz, true);

    const CVector newPos{ cx, cy, cz };
    if (collisionCheck != 0) {
        CMatrix M;
        M = *obj.m_matrix;
        M.GetPosition() = newPos;
        if (IsObjectPlaceBlocked(obj, M, newPos)) {
            return true;
        }
    }

    obj.Teleport(newPos, false); // vtable slot 0xE (0x5A17B0)
    return cx == tx && cy == ty && cz == tz;
}

//! 847 REMOVE_CHAR_ELEGANTLY (case @0x482A96): char
//! A mission ped (created by 2): in a vehicle => CTheScripts::RemoveThisPed, else it becomes a random ped (created by 1), leaves
//! its group, the mission ped counter is decremented, it fades out (flag +0x470 bit 3) and references to it are removed.
//! The mission cleanup entry (type 2) is removed if the script uses it - even if the handle was invalid.
void RemoveCharElegantly(CRunningScript& S, int32 handle) {
    CPed* const ped = GetPedPool()->GetAtRef(handle);
    if (ped && ped->GetCreatedBy() == PED_MISSION) {
        if (ped->bInVehicle && ped->m_pVehicle) {
            CTheScripts::RemoveThisPed(ped);
        } else {
            ped->SetCharCreatedBy(PED_GAME);
            if (auto* const group = CPedGroups::GetPedsGroup(ped)) {
                auto& membership = group->GetMembership();
                if (membership.IsFollower(ped)) {
                    membership.RemoveMember(ped);
                }
            }
            CPopulation::ms_nTotalMissionPeds--;
            ped->bFadeOut = true;
            CWorld::RemoveReferencesToDeletedObject(ped);
        }
    }
    if (S.m_UsesMissionCleanup) {
        CTheScripts::MissionCleanUp.RemoveEntityFromList(handle, MISSION_CLEANUP_ENTITY_TYPE_PED);
    }
}

//! 860 PLACE_OBJECT_RELATIVE_TO_CAR (case @0x482C67): object, car, x, y, z
void PlaceObjectRelativeToCar(CObject& obj, CVehicle& veh, CVector offset) {
    CPhysical::PlacePhysicalRelativeToOtherPhysical(&veh, &obj, offset);
}

//! 861 MAKE_OBJECT_TARGETTABLE (case @0x482CDB): object, targettable
//! Not targettable: if the focused player ped targets this object the target is released.
void MakeObjectTargettable(CObject& obj, int32 targettable) {
    auto* const player = CWorld::Players[CWorld::PlayerInFocus].m_pPed;
    if (targettable != 0) {
        obj.SetObjectTargettable(true);
        return;
    }
    obj.SetObjectTargettable(false);
    CEntity*& target = player->m_pTargetedObject;
    if (target == &obj) {
        if (target) {
            obj.CleanUpOldReference(&target);
        }
        target = nullptr;
    }
}

//! 863 ADD_ARMOUR_TO_CHAR (case @0x482D52): char, amount
//! armour += amount; players are limited to their max armour (`!(max > armour) => max`), others to 100 (`!(armour < 100) => 100`);
//! finally `!(armour > 0) => 0`.
void AddArmourToChar(CPed& ped, int32 amount) {
    ped.m_fArmour = (float)((double)amount + (double)ped.m_fArmour);
    if (ped.IsPlayer()) {
        const auto  slot = CWorld::FindPlayerSlotWithPedPointer(&ped);
        const float max  = (float)CWorld::Players[slot].m_nMaxArmour;
        if (!(max > ped.m_fArmour)) {
            ped.m_fArmour = max;
        }
    } else if (!(ped.m_fArmour < 100.0f)) { // 0x858628
        ped.m_fArmour = 100.0f;
    }
    if (!(ped.m_fArmour > 0.0f)) {
        ped.m_fArmour = 0.0f;
    }
}

//! 864 OPEN_GARAGE (case @0x482E1D): name(8). Nothing happens if the garage doesn't exist
void OpenGarage(const char* garageName) {
    const int16 garage = CGarages::GetGarageNumberByName(garageName);
    if (garage >= 0) {
        CGarages::GetGarage(garage).OpenThisGarage();
    }
}

//! 865 CLOSE_GARAGE (case @0x482E60): name(8). Nothing happens if the garage doesn't exist
void CloseGarage(const char* garageName) {
    const int16 garage = CGarages::GetGarageNumberByName(garageName);
    if (garage >= 0) {
        CGarages::GetGarage(garage).CloseThisGarage();
    }
}

//! 867 SET_VISIBILITY_OF_CLOSEST_OBJECT_OF_TYPE (case @0x482F84): x, y, z, radius, model, visible
//! z <= -100 => ground Z. Searches objects (+ buildings + dummies; LODs if none) of the model within `radius` (2D), picks the one
//! closest to the point (distance < 2 * radius). Visible: collision + visible are switched on, and (if it's not from an IPL)
//! it's removed from the invisibility swap array. Invisible: collision + visible are switched off and it's added to the array.
void SetVisibilityOfClosestObjectOfType(CVector pos, float radius, notsa::script::Model model, int32 visible) {
    pos.z = GroundZIfAuto(pos.x, pos.y, pos.z);

    float best = (float)((double)radius + (double)radius); // `fadd st(0), st(0)`, stored as float
    CEntity* closest = nullptr;

    CEntity* entities[16];
    int16    count = 0;
    CWorld::FindObjectsOfTypeInRange((uint32)model.value, pos, radius, true, &count, 16, entities, true, false, false, true, true);
    if (count == 0) {
        CWorld::FindLodOfTypeInRange((uint32)model.value, pos, radius, true, &count, 16, entities);
    }
    for (uint16 i = 0; (int32)i < (int32)count; i++) {
        CEntity* const entity = entities[i];
        const CVector  ep     = entity->GetPosition();
        const double   dz     = (double)ep.z - (double)pos.z;
        const double   dy     = (double)ep.y - (double)pos.y;
        const double   dx     = (double)ep.x - (double)pos.x;
        const double   dist   = std::sqrt((dx * dx + dy * dy) + dz * dz);
        if (dist < (double)best) { // `fcom; test ah, 5; jp skip`
            best    = (float)dist;
            closest = entity;
        }
    }
    if (!closest) {
        return;
    }

    if (visible != 0) {
        closest->m_nFlags |= 0x81; // bUsesCollision | bIsVisible
        if (closest->GetIplIndex() != 0) {
            return;
        }
        auto& swapArray = CTheScripts::InvisibilitySettingArray;
        for (auto& e : swapArray) {
            if (e == closest) {
                e = nullptr;
                break;
            }
        }
    } else {
        closest->m_nFlags &= 0xFFFFFF7E;
        CTheScripts::AddToInvisibilitySwapArray(closest, false);
    }
}

//! 870 HAS_OBJECT_BEEN_DAMAGED (case @0x4831F8): object => compare flag (0x46A2F0: `bRenderDamaged || !bIsVisible`)
bool HasObjectBeenDamaged(CObject& obj) {
    return obj.m_bRenderDamaged || !obj.m_bIsVisible;
}

//! 886 CREATE_RANDOM_CHAR (case @0x48332A): x, y, z => 1 handle
//! Picks up to 5 random civilian models with `ChooseCivilianOccupation`, stopping at the first loaded CIVMALE (or CIVFEMALE);
//! falls back to model 7 if the last pick isn't loaded. Creates a CCivilianPed standing still (mission ped) at z + 1.
CPed* CreateRandomChar(CRunningScript& S, CVector pos) {
    ePedType pedType = (ePedType)6;
    uint16   tries   = 0;
    auto     modelId = (eModelID)0;
    while (pedType != PED_TYPE_CIVFEMALE && tries < 5) {
        modelId = CPopulation::ChooseCivilianOccupation(false, false, (AssocGroupId)-1, (eModelID)-1, (ePedStats)-1, false, false, false, nullptr);
        const auto* const mi = CModelInfo::GetModelInfo(modelId);
        if (mi->GetRwObject()) {
            pedType = static_cast<const CPedModelInfo*>(mi)->m_nPedType;
        }
        tries++;
        if (pedType == PED_TYPE_CIVMALE) {
            break;
        }
    }
    if (!CModelInfo::GetModelInfo(modelId)->GetRwObject()) {
        pedType = static_cast<const CPedModelInfo*>(CModelInfo::GetModelInfo(7))->m_nPedType;
        modelId = (eModelID)7;
    }

    auto* const ped = new CCivilianPed{ pedType, (uint32)modelId };
    ped->GetTaskManager().SetTask(new CTaskSimpleStandStill{ 999999, true, false, 8.0f }, TASK_PRIMARY_DEFAULT, false);
    ped->SetCharCreatedBy(PED_MISSION);
    ped->bAllowMedicsToReviveMe = false; // `&= 0x7FFFFFFF` of the flags at +0x46C

    pos.z = (float)((double)GroundZIfAuto(pos.x, pos.y, pos.z) + 1.0);
    ped->SetPosn(pos);
    ped->SetOrientation(0.0f, 0.0f, 0.0f);
    CTheScripts::ClearSpaceForMissionEntity(pos, ped);
    if (S.m_UsesMissionCleanup) {
        ped->m_bIsStaticWaitingForCollision = true; // `or [ped + 0x1C], 0x40000`
    }
    CWorld::Add(ped);
    CPopulation::ms_nTotalMissionPeds++;

    if (S.m_UsesMissionCleanup) {
        CTheScripts::MissionCleanUp.AddEntityToList(CPools::GetPedRef(ped), MISSION_CLEANUP_ENTITY_TYPE_PED);
    }
    return ped; // (stored as the pool ref by the parser)
}
}; // namespace

void notsa::script::commands::ported::g05_08::RegisterHandlers() {
    REGISTER_COMMAND_HANDLER_BEGIN("Ported g5-8");

    // g5
    REGISTER_COMMAND_HANDLER(COMMAND_IS_CAR_UPSIDEDOWN, IsCarUpsideDown);
    REGISTER_COMMAND_HANDLER(COMMAND_CANCEL_OVERRIDE_RESTART, CancelOverrideRestart);
    REGISTER_COMMAND_HANDLER(COMMAND_START_KILL_FRENZY, StartKillFrenzy);
    REGISTER_COMMAND_HANDLER(COMMAND_READ_KILL_FRENZY_STATUS, ReadKillFrenzyStatus);
    REGISTER_COMMAND_HANDLER(COMMAND_SQRT, Sqrt);
    REGISTER_COMMAND_HANDLER(COMMAND_LOCK_CAR_DOORS, LockCarDoors);
    REGISTER_COMMAND_HANDLER(COMMAND_EXPLODE_CAR, ExplodeCar);
    REGISTER_COMMAND_HANDLER(COMMAND_ADD_EXPLOSION, AddExplosion);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_CAR_UPRIGHT, IsCarUpright);
    REGISTER_COMMAND_HANDLER(COMMAND_CREATE_PICKUP, CreatePickup);
    REGISTER_COMMAND_HANDLER(COMMAND_HAS_PICKUP_BEEN_COLLECTED, HasPickupBeenCollected);
    REGISTER_COMMAND_HANDLER(COMMAND_REMOVE_PICKUP, RemovePickup);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_TAXI_LIGHTS, SetTaxiLights);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_TARGET_CAR_FOR_MISSION_GARAGE, SetTargetCarForMissionGarage);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_CAR_HEALTH, SetCarHealth);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_CAR_HEALTH, GetCarHealth);
    REGISTER_COMMAND_HANDLER(COMMAND_CHANGE_CAR_COLOUR, ChangeCarColour);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_GANG_WEAPONS, SetGangWeapons);
    REGISTER_COMMAND_HANDLER(COMMAND_LOAD_SPECIAL_CHARACTER, LoadSpecialCharacter);
    REGISTER_COMMAND_HANDLER(COMMAND_HAS_SPECIAL_CHARACTER_LOADED, HasSpecialCharacterLoaded);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_CUTSCENE_OFFSET, SetCutsceneOffset);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_ANIM_GROUP_FOR_CHAR, SetAnimGroupForChar);
    REGISTER_COMMAND_HANDLER(COMMAND_DRAW_CORONA, DrawCorona);

    // g6
    REGISTER_COMMAND_HANDLER(COMMAND_GET_CONTROLLER_MODE, GetControllerMode);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_CAN_RESPRAY_CAR, SetCanRespraycar);
    REGISTER_COMMAND_HANDLER(COMMAND_UNLOAD_SPECIAL_CHARACTER, UnloadSpecialCharacter);
    REGISTER_COMMAND_HANDLER(COMMAND_ACTIVATE_GARAGE, ActivateGarage);
    REGISTER_COMMAND_HANDLER(COMMAND_CREATE_OBJECT_NO_OFFSET, CreateObjectNoOffset);
    REGISTER_COMMAND_HANDLER(COMMAND_SWITCH_WIDESCREEN, SwitchWidescreen);
    REGISTER_COMMAND_HANDLER(COMMAND_ADD_SPRITE_BLIP_FOR_CONTACT_POINT, AddSpriteBlipForContactPoint);
    REGISTER_COMMAND_HANDLER(COMMAND_ADD_SPRITE_BLIP_FOR_COORD, AddSpriteBlipForCoord);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_CAR_ONLY_DAMAGED_BY_PLAYER, SetCarOnlyDamagedByPlayer);
    REGISTER_COMMAND_HANDLER(COMMAND_DEACTIVATE_GARAGE, DeactivateGarage);

    // g7
    REGISTER_COMMAND_HANDLER(COMMAND_IS_CAR_IN_WATER, IsCarInWater);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_CLOSEST_CAR_NODE, GetClosestCarNode);
    REGISTER_COMMAND_HANDLER(COMMAND_CAR_GOTO_COORDINATES_ACCURATE, CarGotoCoordinatesAccurate);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_CAR_ON_SCREEN, IsCarOnScreen);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_OBJECT_ON_SCREEN, IsObjectOnScreen);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_SCRIPT_FIRE_EXTINGUISHED, IsScriptFireExtinguished);
    REGISTER_COMMAND_HANDLER(COMMAND_REMOVE_SCRIPT_FIRE, RemoveScriptFire);
    REGISTER_COMMAND_HANDLER(COMMAND_BOAT_GOTO_COORDS, BoatGotoCoords);
    REGISTER_COMMAND_HANDLER(COMMAND_BOAT_STOP, BoatStop);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_BOAT_CRUISE_SPEED, SetBoatCruiseSpeed);
    REGISTER_COMMAND_HANDLER(COMMAND_CREATE_MONEY_PICKUP, CreateMoneyPickup);
    REGISTER_COMMAND_HANDLER(COMMAND_LOAD_CUTSCENE, LoadCutscene);
    REGISTER_COMMAND_HANDLER(COMMAND_START_CUTSCENE, StartCutscene);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_CUTSCENE_TIME, GetCutsceneTime);
    REGISTER_COMMAND_HANDLER(COMMAND_HAS_CUTSCENE_FINISHED, HasCutsceneFinished);
    REGISTER_COMMAND_HANDLER(COMMAND_CLEAR_CUTSCENE, ClearCutscene);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_COLLECTABLE1_TOTAL, SetCollectable1Total);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_PROJECTILE_IN_AREA, IsProjectileInArea);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_CAR_FORWARD_X, GetCarForwardX);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_CAR_FORWARD_Y, GetCarForwardY);
    REGISTER_COMMAND_HANDLER(COMMAND_CHANGE_GARAGE_TYPE, ChangeGarageType);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_PROGRESS_TOTAL, SetProgressTotal);
    REGISTER_COMMAND_HANDLER(COMMAND_REGISTER_MISSION_PASSED, RegisterMissionPassed);
    REGISTER_COMMAND_HANDLER(COMMAND_HAS_CAR_BEEN_DAMAGED_BY_WEAPON, HasCarBeenDamagedByWeapon);

    // g8
    REGISTER_COMMAND_HANDLER(COMMAND_ANCHOR_BOAT, AnchorBoat);
    REGISTER_COMMAND_HANDLER(COMMAND_START_CAR_FIRE, StartCarFire);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_RANDOM_CAR_OF_TYPE_IN_AREA, GetRandomCarOfTypeInArea);
    REGISTER_COMMAND_HANDLER(COMMAND_CREATE_PICKUP_WITH_AMMO, CreatePickupWithAmmo);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_FREE_RESPRAYS, SetFreeResprays);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_CAR_VISIBLE, SetCarVisible);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_AREA_OCCUPIED, IsAreaOccupied);
    REGISTER_COMMAND_HANDLER(COMMAND_ROTATE_OBJECT, RotateObject);
    REGISTER_COMMAND_HANDLER(COMMAND_SLIDE_OBJECT, SlideObject);
    REGISTER_COMMAND_HANDLER(COMMAND_REMOVE_CHAR_ELEGANTLY, RemoveCharElegantly);
    REGISTER_COMMAND_HANDLER(COMMAND_PLACE_OBJECT_RELATIVE_TO_CAR, PlaceObjectRelativeToCar);
    REGISTER_COMMAND_HANDLER(COMMAND_MAKE_OBJECT_TARGETTABLE, MakeObjectTargettable);
    REGISTER_COMMAND_HANDLER(COMMAND_ADD_ARMOUR_TO_CHAR, AddArmourToChar);
    REGISTER_COMMAND_HANDLER(COMMAND_OPEN_GARAGE, OpenGarage);
    REGISTER_COMMAND_HANDLER(COMMAND_CLOSE_GARAGE, CloseGarage);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_VISIBILITY_OF_CLOSEST_OBJECT_OF_TYPE, SetVisibilityOfClosestObjectOfType);
    REGISTER_COMMAND_HANDLER(COMMAND_HAS_OBJECT_BEEN_DAMAGED, HasObjectBeenDamaged);
    REGISTER_COMMAND_HANDLER(COMMAND_CREATE_RANDOM_CHAR, CreateRandomChar);
}

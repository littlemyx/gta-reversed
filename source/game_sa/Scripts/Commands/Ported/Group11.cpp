#include <StdInc.h>

#include "../Commands.hpp"
#include <CommandParser/Parser.hpp>
#include "Group09_12.hpp"

#include "World.h"
#include "Camera.h"
#include "Pad.h"
#include "Pickups.h"
#include "PedGroups.h"
#include "ModelIndices.h"
#include "Entity/Vehicle/Automobile.h"
#include "Entity/Vehicle/Bike.h"

using namespace notsa::script;
using namespace notsa::script::commands::ported::g09_12;

/*!
* Script commands ported from the exe's group processor for ids 1101..1199 (`CRunningScript::ProcessCommands1100To1199` @0x48A320,
* the switch base is 1101) for the vanilla commands that had no handler of their own (S6-B, g11). Written from the asm of each `case`.
*/

namespace {
//! 1107 SET_OBJECT_ROTATION (case @0x48A350): object, x, y, z (degrees)
void SetObjectRotation(CObject& obj, float x, float y, float z) {
    CWorld::Remove(&obj);
    obj.SetOrientation(x * 0.017453292f, y * 0.017453292f, z * 0.017453292f); // 0x8595EC, args are spilled to float
    obj.UpdateRwMatrix();
    obj.UpdateRwFrame();
    CWorld::Add(&obj);
}

//! 1120 SET_INTERPOLATION_PARAMETERS (case @0x48A723): stopMoving (float), time (int)
void SetInterpolationParameters(float interpolationToStopMoving, int32 time) {
    TheCamera.SetParametersForScriptInterpolation(interpolationToStopMoving, 100.0f - interpolationToStopMoving, (uint32)time); // 0x858628 = 100.0f
}

//! 1126 SET_CAR_STAY_IN_FAST_LANE (case @0x48A8A8): car, flag  -- autopilot flags byte +0x3DB bit 8
void SetCarStayInFastLane(CVehicle& veh, int32 flag) {
    veh.m_autoPilot.carCtrlFlags.bStayInFastLane = flag != 0;
}

//! 1128 CLEAR_CAR_LAST_WEAPON_DAMAGE (case @0x48A93B): car (the handle may be invalid)
void ClearCarLastWeaponDamage(CVehicle* veh) {
    if (veh) {
        veh->m_nLastWeaponDamageType = 0xFF;
    }
}

//! 1132 GET_DRIVER_OF_CAR (case @0x48A974): car => ped handle (-1 if none)
CPed* GetDriverOfCar(CVehicle& veh) {
    return veh.m_pDriver;
}

//! 1133 GET_NUMBER_OF_FOLLOWERS (case @0x48A9E3): ped (may be invalid) => number of group members excluding the leader (0 if the ped isn't a leader)
int32 GetNumberOfFollowers(CPed* ped) {
    uint8 count = 0;
    if (const auto group = CPedGroups::GetPedsGroup(ped)) {
        auto& membership = group->GetMembership();
        if (membership.IsLeader(ped)) {
            count = (uint8)membership.CountMembersExcludingLeader();
        }
    }
    return count;
}

//! 1139 LOCATE_CHAR_IN_CAR_OBJECT_2D / 1142 LOCATE_CHAR_IN_CAR_OBJECT_3D (case @0x48AB19, shared): `LocateCharObjectCommand(command)` does everything
OpcodeResult LocateCharInCarObject(CRunningScript& S, eScriptCommands command) {
    S.LocateCharObjectCommand((int32)command);
    return OR_CONTINUE;
}

//! 1143 SET_CAR_TEMP_ACTION (case @0x48AB30): car, action (byte), time  -- the action lasts until now + time
void SetCarTempAction(CVehicle& veh, int32 action, int32 time) {
    veh.m_autoPilot.m_nTempAction = (eAutoPilotTempAction)(uint8)action;
    veh.m_autoPilot.m_nTempActionTime = CTimer::GetTimeInMS() + (uint32)time;
}

//! 1156 GET_REMOTE_CONTROLLED_CAR (case @0x48AC3E): player => car handle (-1 if none). The player index is used as is (no -1 handling)
CVehicle* GetRemoteControlledCar(int32 player) {
    return CWorld::Players[player].m_pRemoteVehicle; // +0xB0
}

//! 1157 IS_PC_VERSION (case @0x48AC7B) => true
bool IsPCVersion() {
    return true;
}

//! 1162 SET_ENABLE_RC_DETONATE (case @0x48AD30): enable  -- `CVehicle::bDisableRemoteDetonation = !enable`
void SetEnableRCDetonate(int32 enable) {
    CVehicle::bDisableRemoteDetonation = enable == 0;
}

//! 1163 SET_CAR_RANDOM_ROUTE_SEED (case @0x48AD58): car, seed (word)
void SetCarRandomRouteSeed(CVehicle& veh, int32 seed) {
    veh.m_nForcedRandomRouteSeed = (int16)seed;
}

//! 1164 IS_ANY_PICKUP_AT_COORDS (case @0x48AD8F): x, y, z => compare flag (an active pickup closer than 0.5 units)
bool IsAnyPickupAtCoords(float x, float y, float z) {
    for (auto& pickup : CPickups::aPickUps) { // 620 entries of 0x20 bytes
        if (pickup.m_nPickupType == PICKUP_NONE) {
            continue;
        }
        const CVector pos = pickup.GetPosn();
        const CVector delta{ pos.x - x, pos.y - y, pos.z - z };
        if (delta.Magnitude() < 0.5f) { // `fcomp` 0x858B8C + `test ah, 5` + `jp`: strictly less (ordered)
            return true;
        }
    }
    return false;
}

//! 1172 GET_POSITION_OF_ANALOGUE_STICKS (case @0x48AEDC): pad => left X, left Y, right X, right Y (sign-extended words)
MultiRet<int32, int32, int32, int32> GetPositionOfAnalogueSticks(int32 pad) {
    const auto& state = CPad::GetPad(pad)->NewState;
    return { (int32)state.LeftStickX, (int32)state.LeftStickY, (int32)state.RightStickX, (int32)state.RightStickY };
}

//! 1173 IS_CAR_ON_FIRE (case @0x48AF33): car => compare flag
bool IsCarOnFire(CVehicle& veh) {
    bool onFire = veh.m_pFire != nullptr;
    if (veh.m_nVehicleType == VEHICLE_TYPE_AUTOMOBILE) {
        if (static_cast<CAutomobile&>(veh).m_damageManager.GetEngineStatus() >= 225u) { // 0xE1
            onFire = true;
        }
    }
    if (veh.m_fHealth < 250.0f) { // 0x859F80
        onFire = true;
    }
    return onFire;
}

//! 1174 IS_CAR_TYRE_BURST (case @0x48AFBB): car, tyre => compare flag
//! Bikes: tyre 4 = any of the 2 wheels, 2 = wheel 0, 3 = wheel 1 (other values index the bytes at +0x65C directly). Others: tyre 4 = any of the 4 wheels.
bool IsCarTyreBurst(CVehicle& veh, int32 tyre) {
    if (veh.m_nVehicleType == VEHICLE_TYPE_BIKE) {
        auto& bike = static_cast<CBike&>(veh);
        if (tyre == 4) {
            return bike.m_nWheelStatus[0] == 1 || bike.m_nWheelStatus[1] == 1;
        }
        if (tyre == 2) {
            tyre = 0;
        } else if (tyre == 3) {
            tyre = 1;
        }
        return bike.m_nWheelStatus.data()[tyre] == 1; // unchecked index, as in the exe
    }
    auto& dmg = static_cast<CAutomobile&>(veh).m_damageManager;
    if (tyre == 4) {
        bool burst = false;
        for (uint32 i = 0; i < 4; i++) {
            if ((uint32)dmg.GetWheelStatus((eCarWheel)i) == 1) {
                burst = true;
            }
        }
        return burst;
    }
    return (uint32)dmg.GetWheelStatus((eCarWheel)tyre) == 1;
}

//! 1186 HELI_GOTO_COORDS (case @0x48B070): heli, x, y, z, minAltitude, maxAltitude (all floats)
void HeliGotoCoords(CVehicle& heli, float x, float y, float z, float altitudeMin, float altitudeMax) {
    static_cast<CAutomobile&>(heli).TellHeliToGoToCoors(x, y, z, altitudeMin, altitudeMax);
}

//! 1187 IS_INT_VAR_EQUAL_TO_CONSTANT (case @0x48B0DC): var, constant => compare flag  (`GetPointerToScriptVariable` + `CollectParameters(1)`)
bool IsIntVarEqualToConstant(int32 var, int32 constant) {
    return var == constant;
}

//! 1188 IS_INT_LVAR_EQUAL_TO_CONSTANT (case @0x48B100): lvar, constant => compare flag
bool IsIntLVarEqualToConstant(int32 var, int32 constant) {
    return var == constant;
}

//! 1190 CREATE_PROTECTION_PICKUP (case @0x48B19D): x, y, z, ammo, moneyPerDay => pickup handle
//! (the exe also peeks the output variable and calls the pure `CPickups::GetActualPickupIndex` on it, the result is unused)
int32 CreateProtectionPickup(float x, float y, float z, int32 ammo, int32 moneyPerDay) {
    if (z <= -100.0f) {
        z = CWorld::FindGroundZForCoord(x, y) + 0.5f; // 0x858B8C
    }
    return CPickups::GenerateNewOne({ x, y, z }, (uint16)ModelIndices::MI_PICKUP_REVENUE, PICKUP_ASSET_REVENUE, (uint32)ammo, (uint32)moneyPerDay, false, nullptr).num;
}
}; // namespace

static_assert(offsetof(CPlayerInfo, m_pRemoteVehicle) == 0xB0);
static_assert(offsetof(CVehicle, m_pFire) == 0x490 && offsetof(CVehicle, m_fHealth) == 0x4C0);
static_assert(offsetof(CVehicle, m_nLastWeaponDamageType) == 0x508 && offsetof(CVehicle, m_nForcedRandomRouteSeed) == 0x45E);
static_assert(offsetof(CVehicle, m_pDriver) == 0x460);
static_assert(offsetof(CVehicle, m_autoPilot) + offsetof(CAutoPilot, m_nTempAction) == 0x3BB && offsetof(CVehicle, m_autoPilot) + offsetof(CAutoPilot, m_nTempActionTime) == 0x3BC);
static_assert(offsetof(CBike, m_nWheelStatus) == 0x65C);
static_assert(offsetof(CAutomobile, m_damageManager) == 0x5A0);

void notsa::script::commands::ported::g09_12::RegisterG11() {
    REGISTER_COMMAND_HANDLER_BEGIN("Ported g11");

    REGISTER_COMMAND_HANDLER(COMMAND_SET_OBJECT_ROTATION, SetObjectRotation);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_INTERPOLATION_PARAMETERS, SetInterpolationParameters);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_CAR_STAY_IN_FAST_LANE, SetCarStayInFastLane);
    REGISTER_COMMAND_HANDLER(COMMAND_CLEAR_CAR_LAST_WEAPON_DAMAGE, ClearCarLastWeaponDamage);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_DRIVER_OF_CAR, GetDriverOfCar);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_NUMBER_OF_FOLLOWERS, GetNumberOfFollowers);
    REGISTER_COMMAND_HANDLER(COMMAND_LOCATE_CHAR_IN_CAR_OBJECT_2D, LocateCharInCarObject);
    REGISTER_COMMAND_HANDLER(COMMAND_LOCATE_CHAR_IN_CAR_OBJECT_3D, LocateCharInCarObject);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_CAR_TEMP_ACTION, SetCarTempAction);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_REMOTE_CONTROLLED_CAR, GetRemoteControlledCar);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_PC_VERSION, IsPCVersion);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_ENABLE_RC_DETONATE, SetEnableRCDetonate);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_CAR_RANDOM_ROUTE_SEED, SetCarRandomRouteSeed);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_ANY_PICKUP_AT_COORDS, IsAnyPickupAtCoords);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_POSITION_OF_ANALOGUE_STICKS, GetPositionOfAnalogueSticks);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_CAR_ON_FIRE, IsCarOnFire);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_CAR_TYRE_BURST, IsCarTyreBurst);
    REGISTER_COMMAND_HANDLER(COMMAND_HELI_GOTO_COORDS, HeliGotoCoords);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_INT_VAR_EQUAL_TO_CONSTANT, IsIntVarEqualToConstant);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_INT_LVAR_EQUAL_TO_CONSTANT, IsIntLVarEqualToConstant);
    REGISTER_COMMAND_HANDLER(COMMAND_CREATE_PROTECTION_PICKUP, CreateProtectionPickup);
}

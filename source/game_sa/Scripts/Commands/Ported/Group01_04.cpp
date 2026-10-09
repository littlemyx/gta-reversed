#include <StdInc.h>

#include "../Commands.hpp"
#include <CommandParser/Parser.hpp>

#include <cstdlib>

#include "World.h"
#include "CarCtrl.h"
#include "CarGenerator.h"
#include "TheCarGenerators.h"
#include "Radar.h"
#include "Restart.h"
#include "UserDisplay.h"
#include "UpsideDownCarCheck.h"
#include "Audio/AudioEngine.h"
#include "MissionCleanup.h"
#include "Population.h"
#include "CivilianPed.h"
#include "CopPed.h"
#include "EmergencyPed.h"
#include "TaskSimpleStandStill.h"
#include "TaskSimpleCarSetPedInAsPassenger.h"
#include "CarEnterExit.h"
#include "WeaponInfo.h"
#include "Weather.h"

using namespace notsa::script;

/*!
* Script commands ported from the exe's per-100 group processors (CRunningScript::ProcessCommands100To199 .. 400To499)
* for the vanilla commands that had no handler of their own: ids 152..492 (S6-A, groups g1..g4).
*
* Every handler below was written from the asm of the `case` in the group processor (table 0x8A6168 + 4 * group),
* NOT from the Ghidra decompilation. The case address is given in the comment above each handler.
*/

namespace {
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

//! 0x463490 (`CAutoPilot::SetCarMission`, 1 arg): doesn't touch the mission of a crashing plane / heli
void AutoPilotSetCarMissionUnlessCrashing(CAutoPilot& ap, eCarMission mission) {
    if (ap.m_nCarMission != MISSION_PLANE_CRASH_AND_BURN && ap.m_nCarMission != MISSION_HELI_CRASH_AND_BURN) {
        ap.m_nCarMission = mission;
    }
}

//! The common tail of 167 / 168: engine on, `cruise = max(cruise, 1)`, mission start time = now
void VehFinishScriptDrive(CVehicle& veh) {
    VehSetEngineOn(veh);
    if (veh.m_autoPilot.m_nCruiseSpeed <= 1) { // `max(cruise, 1)` (unsigned compare)
        veh.m_autoPilot.m_nCruiseSpeed = 1;
    }
    veh.m_autoPilot.m_nTimeToStartMission = CTimer::GetTimeInMS();
}

// ============================================================================ g1 (ProcessCommands100To199 @0x466DE0)

//! 152 GENERATE_RANDOM_FLOAT (case @0x4674E4): `rand()` is called 4 times, only the last result is used.
//! Stores 1 float: `(float)(int)rand() * 2^-16` (0x859D5C) - the product is exact.
float GenerateRandomFloat() {
    rand();
    rand();
    rand();
    const int32 r = rand();
    return (float)r * 1.52587890625e-05f;
}

//! 154 CREATE_CHAR (case @0x46754F): pedType, model, x, y, z => 1 handle
CPed* CreateChar(CRunningScript& S, ePedType pedType, int32 modelId, CVector pos) {
    uint32 typeSpecificModelId = (uint32)modelId;
    S.GetCorrectPedModelIndexForEmergencyServiceType(pedType, &typeSpecificModelId);
    CPed* const ped = [&]() -> CPed* {
        switch (pedType) {
        case PED_TYPE_COP:      return new CCopPed{ typeSpecificModelId };
        case PED_TYPE_MEDIC:
        case PED_TYPE_FIREMAN:  return new CEmergencyPed{ pedType, typeSpecificModelId };
        default:                return new CCivilianPed{ pedType, typeSpecificModelId };
        }
    }();
    ped->GetTaskManager().SetTask(new CTaskSimpleStandStill{ 999999, true, false, 8.0f }, TASK_PRIMARY_DEFAULT, false);
    ped->SetCharCreatedBy(PED_MISSION);
    ped->bAllowMedicsToReviveMe = false;

    pos.z = GroundZIfAuto(pos.x, pos.y, pos.z) + 1.0f;
    ped->SetPosn(pos);
    ped->SetOrientation(0.0f, 0.0f, 0.0f);
    CTheScripts::ClearSpaceForMissionEntity(pos, ped);
    if (S.m_UsesMissionCleanup) {
        ped->m_bIsStaticWaitingForCollision = true; // or [ped + 0x1C], 0x40000
    }
    CWorld::Add(ped);
    CPopulation::ms_nTotalMissionPeds++;

    const auto handle = CPools::GetPedRef(ped);
    if (S.m_UsesMissionCleanup) {
        CTheScripts::MissionCleanUp.AddEntityToList(handle, MISSION_CLEANUP_ENTITY_TYPE_PED);
    }
    return ped; // (stored as the pool ref by the parser)
}

//! 155 DELETE_CHAR (case @0x467789): handle
void DeleteChar(CRunningScript& S, int32 handle) {
    CTheScripts::RemoveThisPed(CPools::GetPed(handle));
    if (S.m_UsesMissionCleanup) {
        CTheScripts::MissionCleanUp.RemoveEntityFromList(handle, MISSION_CLEANUP_ENTITY_TYPE_PED);
    }
}

//! 165 CREATE_CAR (case @0x467AB3): model, x, y, z => 1 handle
CVehicle* CreateCar(CRunningScript& S, int32 modelId, CVector pos) {
    return CCarCtrl::CreateCarForScript(modelId, pos, S.m_UsesMissionCleanup);
}

//! 166 DELETE_CAR (case @0x467B1A): handle
void DeleteCar(CRunningScript& S, int32 handle) {
    if (const auto veh = CPools::GetVehicle(handle)) {
        CWorld::Remove(veh);
        CWorld::RemoveReferencesToDeletedObject(veh);
        delete veh;
    }
    if (S.m_UsesMissionCleanup) {
        CTheScripts::MissionCleanUp.RemoveEntityFromList(handle, MISSION_CLEANUP_ENTITY_TYPE_VEHICLE);
    }
}

//! 167 CAR_GOTO_COORDINATES (case @0x467B83): car, x, y, z
void CarGotoCoordinates(CVehicle& veh, CVector pos) {
    pos.z = GroundZIfAuto(pos.x, pos.y, pos.z);
    pos.z = veh.GetDistanceFromCentreOfMassToBaseOfModel() + pos.z; // spilled to a float temp
    const bool joined = CCarCtrl::JoinCarWithRoadSystemGotoCoors(&veh, pos, false, false);
    AutoPilotSetCarMissionUnlessCrashing(veh.m_autoPilot, joined ? MISSION_GOTOCOORDINATES_STRAIGHTLINE : MISSION_GOTOCOORDINATES);
    veh.SetStatus(STATUS_PHYSICS);
    VehFinishScriptDrive(veh);
}

//! 168 CAR_WANDER_RANDOMLY (case @0x467C83): car
void CarWanderRandomly(CVehicle& veh) {
    CCarCtrl::JoinCarWithRoadSystem(&veh);
    AutoPilotSetCarMissionUnlessCrashing(veh.m_autoPilot, MISSION_CRUISE);
    VehFinishScriptDrive(veh);
}

//! 169 CAR_SET_IDLE (case @0x467CFF): car
void CarSetIdle(CVehicle& veh) {
    AutoPilotSetCarMissionUnlessCrashing(veh.m_autoPilot, MISSION_NONE);
}

//! 170 GET_CAR_COORDINATES (case @0x467D48): car => x, y, z
CVector GetCarCoordinates(CVehicle& veh) {
    return veh.GetPosition();
}

//! 171 SET_CAR_COORDINATES (case @0x467DB5): car, x, y, z
void SetCarCoordinates(CVehicle& veh, CVector pos) {
    CCarCtrl::SetCoordsOfScriptCar(&veh, pos.x, pos.y, pos.z, 0, 1);
}

//! 173 SET_CAR_CRUISE_SPEED (case @0x467E05): car, speed(float)
void SetCarCruiseSpeed(CVehicle& veh, float speed) {
    // `_ftol` truncation, then only the low byte is kept (movzx edx, al)
    const uint8 speedByte = (uint8)(int32)speed;
    veh.m_autoPilot.m_nCruiseSpeed = speedByte; // dead store in the original (overwritten below), kept for fidelity
    // 0x858B34 = 60.0f; handling+0x88 = m_transmissionData.m_MaxFlatVelocity
    const float maxCruise = veh.m_pHandlingData->m_transmissionData.m_MaxFlatVelocity * 60.0f;
    const double cur = (double)speedByte;
    veh.m_autoPilot.m_nCruiseSpeed = (uint8)(int32)(cur < (double)maxCruise ? cur : (double)maxCruise); // `fcom` + jnp: take `cur` only if cur < max
}

//! 175 SET_CAR_MISSION (case @0x467EC1): car, mission
void SetCarMission(CVehicle& veh, int32 mission) {
    AutoPilotSetCarMissionUnlessCrashing(veh.m_autoPilot, (eCarMission)(uint8)mission);
    veh.m_autoPilot.m_nTimeToStartMission = CTimer::GetTimeInMS();
    VehSetEngineOn(veh);
}

//! 176 IS_CAR_IN_AREA_2D (case @0x467F1B): car, x1, y1, x2, y2, drawSphere
bool IsCarInArea2D(CRunningScript& S, CVehicle& veh, float x1, float y1, float x2, float y2, int32 drawSphere) {
    const bool inArea = veh.IsWithinArea(x1, y1, x2, y2);
    if (drawSphere) {
        S.HighlightImportantArea({ x1, y1 }, { x2, y2 }, -100.0f);
    }
    return inArea;
}

//! 177 IS_CAR_IN_AREA_3D (case @0x467FBD): car, x1, y1, z1, x2, y2, z2, drawSphere
bool IsCarInArea3D(CRunningScript& S, CVehicle& veh, CVector p1, CVector p2, int32 drawSphere) {
    const bool inArea = veh.IsWithinArea(p1.x, p1.y, p1.z, p2.x, p2.y, p2.z);
    if (drawSphere) {
        S.HighlightImportantArea({ p1.x, p1.y }, { p2.x, p2.y }, (float)(((double)p2.z + (double)p1.z) * 0.5)); // x87 stack: (z2 + z1) * 0.5f (0x858B8C), one float spill
    }
    return inArea;
}

// ============================================================================ g2 (ProcessCommands200To299 @0x469390)

//! 216 MISSION_HAS_FINISHED (case @0x46945E): no params
void MissionHasFinished(CRunningScript& S) {
    if (S.m_UsesMissionCleanup) {
        CTheScripts::MissionCleanUp.Process();
    }
}

//! 226 GET_PAD_STATE (case @0x469715): pad, button => 1 int (sign-extended int16)
int32 GetPadState(CRunningScript& S, int32 pad, int32 button) {
    return S.GetPadState((uint16)pad, (eButtonId)(uint16)button);
}

//! 273 SET_DEATHARREST_STATE (case @0x469A7B): state
void SetDeathArrestState(CRunningScript& S, int32 state) {
    S.m_IsDeathArrestCheckEnabled = (state == 1);
}

//! 274 HAS_DEATHARREST_BEEN_EXECUTED (case @0x469A9B): no params
bool HasDeathArrestBeenExecuted(CRunningScript& S) {
    return S.m_DoneDeathArrest != 0;
}

//! 276 ADD_AMMO_TO_CHAR (case @0x469AA8): char, weaponType, ammo
void AddAmmoToChar(CPed& ped, eWeaponType weaponType, int32 ammo) {
    ped.GrantAmmo(weaponType, (uint32)ammo);
}

//! 281 IS_CAR_DEAD (case @0x469B6D): car
bool IsCarDead(CVehicle* veh) {
    if (!veh || veh->GetStatus() == STATUS_WRECKED) {
        return true;
    }
    return veh->vehicleFlags.bIsDrowning; // byte 0x42B bit 6
}

// ============================================================================ g3 (ProcessCommands300To399 @0x47C100)

//! 311 IS_CAR_MODEL (case @0x47C127): car, model
bool IsCarModel(CVehicle& veh, int32 model) {
    return (int32)(int16)veh.m_nModelIndex == model; // movsx word [veh + 0x22]
}

//! 331 CREATE_CAR_GENERATOR (case @0x47C169): x, y, z, angle, model, color1, color2, forceSpawn, alarm, doorLock, minDelay, maxDelay => 1 int
int32 CreateCarGenerator(CVector pos, float angle, int32 model, int32 color1, int32 color2, int32 forceSpawn, int32 alarm, int32 doorLock, int32 minDelay, int32 maxDelay) {
    if (pos.z > -100.0f) { // 0x859014; NaN => untouched
        pos.z += 0.015f;   // 0x859F00
    }
    return CTheCarGenerators::CreateCarGenerator(
        pos,
        angle,
        model,
        (int16)color1,
        (int16)color2,
        (uint8)forceSpawn,
        (uint8)alarm,
        (uint8)doorLock,
        (uint16)minDelay,
        (uint16)maxDelay,
        0,    // iplId
        true  // ignorePopulationLimit
    );
}

//! 334 DISPLAY_ONSCREEN_TIMER (case @0x47C284): <global var>, direction. (The variable is read first, then CollectParameters(1))
void DisplayOnscreenTimer(CRunningScript& S) {
    const auto varId = S.GetIndexOfGlobalVariable();
    const auto dir   = Read<int32>(&S);
    CUserDisplay::OnscnTimer.AddClock(varId, nullptr, dir != 0 ? eTimerDirection::DECREASE : eTimerDirection::INCREASE);
}

//! 335 CLEAR_ONSCREEN_TIMER (case @0x47C2D2): <global var>
void ClearOnscreenTimer(CRunningScript& S) {
    CUserDisplay::OnscnTimer.ClearClock(S.GetIndexOfGlobalVariable());
}

//! 337 CLEAR_ONSCREEN_COUNTER (case @0x47C2F1): <global var>
void ClearOnscreenCounter(CRunningScript& S) {
    CUserDisplay::OnscnTimer.ClearCounter(S.GetIndexOfGlobalVariable());
}

//! 349 SET_TIME_SCALE (case @0x47C518): scale
void SetTimeScale(float scale) {
    CTimer::ms_fTimeScale = scale;
}

//! 353 ADD_BLIP_FOR_CAR_OLD (case @0x47C610): car, arg2, display, <blip var> => 1 blip
//! (the destination variable is peeked at with CollectNextParameterWithoutIncreasingPC and fed to GetActualBlipArrayIndex, result unused)
int32 AddBlipForCarOld(CRunningScript& S, int32 carHandle, int32 arg2, int32 display) {
    CRadar::GetActualBlipArrayIndex(S.CollectNextParameterWithoutIncreasingPC());
    return CRadar::SetEntityBlip(BLIP_CAR, carHandle, (uint32)arg2, (eBlipDisplay)display);
}

//! 356 REMOVE_BLIP (case @0x47C660): blip
void RemoveBlip(int32 blip) {
    CRadar::ClearBlip(blip);
}

//! 357 CHANGE_BLIP_COLOUR (case @0x47C682): blip, colour
void ChangeBlipColour(int32 blip, int32 colour) {
    CRadar::ChangeBlipColour(blip, (eBlipColour)colour);
}

//! 359 ADD_BLIP_FOR_COORD_OLD (case @0x47C6AA): x, y, z, colour, display, <blip var> => 1 blip
int32 AddBlipForCoordOld(CRunningScript& S, CVector pos, int32 colour, int32 display) {
    pos.z = GroundZIfAuto(pos.x, pos.y, pos.z);
    CRadar::GetActualBlipArrayIndex(S.CollectNextParameterWithoutIncreasingPC());
    return CRadar::SetCoordBlip(BLIP_COORD, pos, (eBlipColour)colour, (eBlipDisplay)display, S.m_szName);
}

//! 360 CHANGE_BLIP_SCALE (case @0x47C764): blip, scale
void ChangeBlipScale(int32 blip, int32 scale) {
    CRadar::ChangeBlipScale(blip, scale);
}

//! 361 SET_FADING_COLOUR (case @0x47C78C): r, g, b
void SetFadingColour(int32 r, int32 g, int32 b) {
    TheCamera.SetFadeColour((uint8)r, (uint8)g, (uint8)b);
}

//! 363 GET_FADING_STATUS (case @0x47C831): no params
bool GetFadingStatus() {
    return TheCamera.GetFading();
}

//! 364 ADD_HOSPITAL_RESTART (case @0x47C85A): x, y, z, heading, town
void AddHospitalRestart(CVector pos, float heading, int32 town) {
    pos.z = GroundZIfAuto(pos.x, pos.y, pos.z);
    CRestart::AddHospitalRestartPoint(pos, heading, town);
}

//! 365 ADD_POLICE_RESTART (case @0x47C8DC): x, y, z, heading, town
void AddPoliceRestart(CVector pos, float heading, int32 town) {
    pos.z = GroundZIfAuto(pos.x, pos.y, pos.z);
    CRestart::AddPoliceRestartPoint(pos, heading, town);
}

//! 366 OVERRIDE_NEXT_RESTART (case @0x47C95E): x, y, z, heading
void OverrideNextRestart(CVector pos, float heading) {
    pos.z = GroundZIfAuto(pos.x, pos.y, pos.z);
    CRestart::OverrideNextRestart(pos, heading);
}

//! 372 GET_CAR_HEADING (case @0x47CBE1): car => 1 float (degrees)
float GetCarHeading(CVehicle& veh) {
    constexpr float RAD_TO_DEG = 57.2957763671875f; // 0x859878
    float heading = (float)((double)veh.GetHeading() * (double)RAD_TO_DEG); // fmul + fst (float spill)
    if (heading < 0.0f) {                                                   // fcomp 0.0 (0x858B50), `test ah,5; jp`
        heading = (float)((double)heading + 360.0);                         // 0x859E2C
    }
    if (heading > 360.0f) {                                                 // fcomp 360.0, `test ah,0x41; jne`
        heading = (float)((double)heading - 360.0);
    }
    return heading;
}

//! 373 SET_CAR_HEADING (case @0x47CC64): car, heading (degrees)
void SetCarHeading(CVehicle& veh, float headingDeg) {
    constexpr float DEG_TO_RAD = 0.01745329238474369f; // 0x8595EC (float)
    double heading = headingDeg; // stays on the x87 stack until the single float spill into the call
    if (heading < 0.0) {
        heading += 360.0;
    }
    if (heading > 360.0) {
        heading -= 360.0;
    }
    veh.SetHeading((float)(heading * (double)DEG_TO_RAD));
    veh.UpdateRwMatrix();
}

//! 384 DECLARE_MISSION_FLAG (case @0x47CE00): <global var>. Reads the (untyped) variable operand straight from the IP.
void DeclareMissionFlag(CRunningScript& S) {
    S.GetAtIPAs<int8>();                                  // skip the parameter type byte (not checked)
    CTheScripts::OnAMissionFlag = S.GetAtIPAs<uint16>();  // offset of the variable
}

//! 389 IS_CAR_HEALTH_GREATER (case @0x47CE71): car, health(int)
bool IsCarHealthGreater(CVehicle& veh, int32 health) {
    return (double)health < (double)veh.m_fHealth; // fild + fcomp, `test ah,5; jnp` => true iff exactly "below"
}

//! Common tail of 390/391/392: create the entity blip, set the scale, store the blip. (script name is passed as the 5th arg of SetEntityBlip, ignored by it)
int32 AddEntityBlipScale3(CRunningScript& S, eBlipType type, int32 handle, uint32 colour) {
    CRadar::GetActualBlipArrayIndex(S.CollectNextParameterWithoutIncreasingPC());
    const auto blip = CRadar::SetEntityBlip(type, handle, colour, BLIP_DISPLAY_BOTH);
    CRadar::ChangeBlipScale(blip, 3);
    return blip;
}

//! 390 ADD_BLIP_FOR_CAR (case @0x47CEBF): car, <blip var> => 1 blip
int32 AddBlipForCar(CRunningScript& S, int32 carHandle) {
    return AddEntityBlipScale3(S, BLIP_CAR, carHandle, 0);
}

//! 391 ADD_BLIP_FOR_CHAR (case @0x47CEFD): char, <blip var> => 1 blip
int32 AddBlipForChar(CRunningScript& S, int32 charHandle) {
    return AddEntityBlipScale3(S, BLIP_CHAR, charHandle, 1);
}

//! 392 ADD_BLIP_FOR_OBJECT (case @0x47CF25): object, <blip var> => 1 blip
int32 AddBlipForObject(CRunningScript& S, int32 objectHandle) {
    return AddEntityBlipScale3(S, BLIP_OBJECT, objectHandle, 6);
}

//! 394 ADD_BLIP_FOR_COORD (case @0x47CF4E): x, y, z, <blip var> => 1 blip
int32 AddBlipForCoord(CRunningScript& S, CVector pos) {
    pos.z = GroundZIfAuto(pos.x, pos.y, pos.z);
    CRadar::GetActualBlipArrayIndex(S.CollectNextParameterWithoutIncreasingPC());
    const auto blip = CRadar::SetCoordBlip(BLIP_COORD, pos, (eBlipColour)5, BLIP_DISPLAY_BOTH, S.m_szName);
    CRadar::ChangeBlipScale(blip, 3);
    return blip;
}

//! 395 CHANGE_BLIP_DISPLAY (case @0x47D00B): blip, display
void ChangeBlipDisplay(int32 blip, int32 display) {
    CRadar::ChangeBlipDisplay(blip, (eBlipDisplay)display);
}

//! 396 ADD_ONE_OFF_SOUND (case @0x47D034): x, y, z, soundId
void AddOneOffSound(CVector pos, int32 soundId) {
    AudioEngine.ReportMissionAudioEvent((uint16)soundId, pos);
}

//! 399 IS_CAR_STUCK_ON_ROOF (case @0x47D0AC): car (handle)
bool IsCarStuckOnRoof(int32 carHandle) {
    return CTheScripts::UpsideDownCars.HasCarBeenUpsideDownForAWhile(carHandle);
}

// ============================================================================ g4 (ProcessCommands400To499 @0x47D210)

//! 400 ADD_UPSIDEDOWN_CAR_CHECK (case @0x47D24E): car (handle)
void AddUpsidedownCarCheck(int32 carHandle) {
    CTheScripts::UpsideDownCars.AddCarToCheck(carHandle);
}

//! 401 REMOVE_UPSIDEDOWN_CAR_CHECK (case @0x47D27E): car (handle)
void RemoveUpsidedownCarCheck(int32 carHandle) {
    CTheScripts::UpsideDownCars.RemoveCarFromCheck(carHandle);
}

//! 427 / 428 IS_CAR_STOPPED_IN_AREA_2D / _3D (cases @0x47D2CB): the exe forwards the command id to CRunningScript::CarInAreaCheckCommand
void CarInAreaCheck(CRunningScript& S, eScriptCommands cmd) {
    S.CarInAreaCheckCommand((int32)cmd);
}

//! 429..432 LOCATE_CAR_2D / LOCATE_STOPPED_CAR_2D / LOCATE_CAR_3D / LOCATE_STOPPED_CAR_3D (cases @0x47D2E9): forwards the command id to CRunningScript::LocateCarCommand
void LocateCar(CRunningScript& S, eScriptCommands cmd) {
    S.LocateCarCommand((int32)cmd);
}

//! 434 GIVE_WEAPON_TO_CHAR (case @0x47D307): char, weaponType, ammo
void GiveWeaponToChar(CPed& ped, eWeaponType weaponType, int32 ammo) {
    const auto slot = ped.GiveWeapon(weaponType, (uint32)ammo, true);
    if (ped.IsPlayer()) {
        ped.GetPlayerData()->m_nChosenWeapon = (uint8)slot;
        return;
    }
    ped.SetCurrentWeapon((int32)slot);
    if (ped.bInVehicle && ped.m_pVehicle) { // [ped + 0x46C] bit 8, [ped + 0x58C]
        const auto activeWeaponType = ped.m_aWeapons[(int8)ped.m_nActiveWeaponSlot].m_Type; // movsx byte [ped + 0x718]
        ped.RemoveWeaponModel(CWeaponInfo::GetWeaponInfo(activeWeaponType, eWeaponSkill::STD)->m_nModelId1);
    }
}

//! 437 FORCE_WEATHER (case @0x47D42B): weather (u16)
void ForceWeather(int32 weather) {
    CWeather::ForceWeather((eWeatherType)(uint16)weather);
}

//! 438 FORCE_WEATHER_NOW (case @0x47D45C): weather (u16)
void ForceWeatherNow(int32 weather) {
    CWeather::ForceWeatherNow((eWeatherType)(uint16)weather);
}

//! 439 RELEASE_WEATHER (case @0x47D48D): no params
void ReleaseWeather() {
    CWeather::ReleaseWeather();
}

//! 445 GET_GAME_TIMER (case @0x47D69E): no params => 1 int
uint32 GetGameTimer() {
    return CTimer::GetTimeInMS();
}

//! 448 STORE_WANTED_LEVEL (case @0x47D6A8): playerIdx => 1 int. (CWorld::Players[idx].m_pPed->GetWantedLevel(), no index check)
int32 StoreWantedLevel(int32 playerIdx) {
    return (int32)CWorld::Players[playerIdx].m_pPed->GetWantedLevel();
}

//! 449 IS_CAR_STOPPED (case @0x47D6CD): car
bool IsCarStopped(CVehicle* veh) {
    return CTheScripts::IsVehicleStopped(veh);
}

//! 451 MARK_CAR_AS_NO_LONGER_NEEDED (case @0x47D75A): car (handle)
void MarkCarAsNoLongerNeeded(CRunningScript& S, int32 carHandle) {
    CTheScripts::CleanUpThisVehicle(CPools::GetVehicle(carHandle));
    if (S.m_UsesMissionCleanup) {
        CTheScripts::MissionCleanUp.RemoveEntityFromList(carHandle, MISSION_CLEANUP_ENTITY_TYPE_VEHICLE);
    }
}

//! 453 DONT_REMOVE_CHAR (case @0x47D7E1): char (handle). NOTE: not guarded by `m_UsesMissionCleanup` (unlike 451)
void DontRemoveChar(int32 charHandle) {
    CTheScripts::MissionCleanUp.RemoveEntityFromList(charHandle, MISSION_CLEANUP_ENTITY_TYPE_PED);
}

//! 456 CREATE_CHAR_AS_PASSENGER (case @0x47D845): car, pedType, model, seat => 1 handle
CPed* CreateCharAsPassenger(CRunningScript& S, CVehicle& veh, ePedType pedType, int32 modelId, int32 seat) {
    uint32 typeSpecificModelId = (uint32)modelId;
    S.GetCorrectPedModelIndexForEmergencyServiceType(pedType, &typeSpecificModelId);
    CPed* const ped = [&]() -> CPed* {
        switch (pedType) {
        case PED_TYPE_COP:      return new CCopPed{ typeSpecificModelId };
        case PED_TYPE_MEDIC:
        case PED_TYPE_FIREMAN:  return new CEmergencyPed{ pedType, typeSpecificModelId };
        default:                return new CCivilianPed{ pedType, typeSpecificModelId };
        }
    }();
    ped->SetCharCreatedBy(PED_MISSION);
    ped->bAllowMedicsToReviveMe = false;
    if (veh.vehicleFlags.bIsBus) { // byte [veh + 0x429] bit 1
        ped->bRenderPedInCar = false; // [ped + 0x46C] &= ~0x2000
    }
    const int32 door = seat >= 0 ? CCarEnterExit::ComputeTargetDoorToEnterAsPassenger(&veh, seat) : 0;
    {
        CTaskSimpleCarSetPedInAsPassenger task{ &veh, (eTargetDoor)door };
        task.ProcessPed(ped);
    }
    CPopulation::ms_nTotalMissionPeds++;
    CWorld::Add(ped);
    if (S.m_UsesMissionCleanup) {
        CTheScripts::MissionCleanUp.AddEntityToList(CPools::GetPedRef(ped), MISSION_CLEANUP_ENTITY_TYPE_PED);
    }
    return ped;
}

//! 489 GET_NUMBER_OF_PASSENGERS (case @0x47DD2F): car => 1 int
int32 GetNumberOfPassengers(CVehicle& veh) {
    return veh.m_nNumPassengers; // byte [veh + 0x484]
}

//! 490 GET_MAXIMUM_NUMBER_OF_PASSENGERS (case @0x47DD75): car => 1 int
int32 GetMaximumNumberOfPassengers(CVehicle& veh) {
    return veh.m_nMaxPassengers; // byte [veh + 0x488]
}

//! 491 SET_CAR_DENSITY_MULTIPLIER (case @0x47DDBB): multiplier
void SetCarDensityMultiplier(float multiplier) {
    CCarCtrl::CarDensityMultiplier = multiplier; // 0x8A5B20
}

//! 492 SET_CAR_HEAVY (case @0x47DDE6): car, heavy
void SetCarHeavy(CVehicle* veh, int32 heavy) {
    if (!veh) {
        return;
    }
    const auto hd = veh->m_pHandlingData;
    if (heavy) {
        veh->physicalFlags.bMakeMassTwiceAsBig = true; // [veh + 0x40] |= 1
        veh->m_fMass             = hd->m_fMass * 3.0f;           // 0x858B3C
        veh->m_fTurnMass         = hd->m_fTurnMass * 5.0f;       // 0x858C80
        veh->m_fBuoyancyConstant = hd->m_fBuoyancyConstant + hd->m_fBuoyancyConstant; // fadd st(0), st(0)
    } else {
        veh->physicalFlags.bMakeMassTwiceAsBig = false;
        veh->m_fMass             = hd->m_fMass;
        veh->m_fTurnMass         = hd->m_fTurnMass;
        veh->m_fBuoyancyConstant = hd->m_fBuoyancyConstant;
    }
}
}; // namespace

void notsa::script::commands::ported::g01_04::RegisterHandlers() {
    REGISTER_COMMAND_HANDLER_BEGIN("Ported g1-4");

    // g1
    REGISTER_COMMAND_HANDLER(COMMAND_GENERATE_RANDOM_FLOAT, GenerateRandomFloat);
    REGISTER_COMMAND_HANDLER(COMMAND_CREATE_CHAR, CreateChar);
    REGISTER_COMMAND_HANDLER(COMMAND_DELETE_CHAR, DeleteChar);
    REGISTER_COMMAND_HANDLER(COMMAND_CREATE_CAR, CreateCar);
    REGISTER_COMMAND_HANDLER(COMMAND_DELETE_CAR, DeleteCar);
    REGISTER_COMMAND_HANDLER(COMMAND_CAR_GOTO_COORDINATES, CarGotoCoordinates);
    REGISTER_COMMAND_HANDLER(COMMAND_CAR_WANDER_RANDOMLY, CarWanderRandomly);
    REGISTER_COMMAND_HANDLER(COMMAND_CAR_SET_IDLE, CarSetIdle);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_CAR_COORDINATES, GetCarCoordinates);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_CAR_COORDINATES, SetCarCoordinates);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_CAR_CRUISE_SPEED, SetCarCruiseSpeed);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_CAR_MISSION, SetCarMission);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_CAR_IN_AREA_2D, IsCarInArea2D);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_CAR_IN_AREA_3D, IsCarInArea3D);

    // g2
    REGISTER_COMMAND_HANDLER(COMMAND_MISSION_HAS_FINISHED, MissionHasFinished);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_PAD_STATE, GetPadState);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_DEATHARREST_STATE, SetDeathArrestState);
    REGISTER_COMMAND_HANDLER(COMMAND_HAS_DEATHARREST_BEEN_EXECUTED, HasDeathArrestBeenExecuted);
    REGISTER_COMMAND_HANDLER(COMMAND_ADD_AMMO_TO_CHAR, AddAmmoToChar);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_CAR_DEAD, IsCarDead);

    // g3
    REGISTER_COMMAND_HANDLER(COMMAND_IS_CAR_MODEL, IsCarModel);
    REGISTER_COMMAND_HANDLER(COMMAND_CREATE_CAR_GENERATOR, CreateCarGenerator);
    REGISTER_COMMAND_HANDLER(COMMAND_DISPLAY_ONSCREEN_TIMER, DisplayOnscreenTimer);
    REGISTER_COMMAND_HANDLER(COMMAND_CLEAR_ONSCREEN_TIMER, ClearOnscreenTimer);
    REGISTER_COMMAND_HANDLER(COMMAND_CLEAR_ONSCREEN_COUNTER, ClearOnscreenCounter);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_TIME_SCALE, SetTimeScale);
    REGISTER_COMMAND_HANDLER(COMMAND_ADD_BLIP_FOR_CAR_OLD, AddBlipForCarOld);
    REGISTER_COMMAND_HANDLER(COMMAND_REMOVE_BLIP, RemoveBlip);
    REGISTER_COMMAND_HANDLER(COMMAND_CHANGE_BLIP_COLOUR, ChangeBlipColour);
    REGISTER_COMMAND_HANDLER(COMMAND_ADD_BLIP_FOR_COORD_OLD, AddBlipForCoordOld);
    REGISTER_COMMAND_HANDLER(COMMAND_CHANGE_BLIP_SCALE, ChangeBlipScale);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_FADING_COLOUR, SetFadingColour);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_FADING_STATUS, GetFadingStatus);
    REGISTER_COMMAND_HANDLER(COMMAND_ADD_HOSPITAL_RESTART, AddHospitalRestart);
    REGISTER_COMMAND_HANDLER(COMMAND_ADD_POLICE_RESTART, AddPoliceRestart);
    REGISTER_COMMAND_HANDLER(COMMAND_OVERRIDE_NEXT_RESTART, OverrideNextRestart);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_CAR_HEADING, GetCarHeading);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_CAR_HEADING, SetCarHeading);
    REGISTER_COMMAND_HANDLER(COMMAND_DECLARE_MISSION_FLAG, DeclareMissionFlag);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_CAR_HEALTH_GREATER, IsCarHealthGreater);
    REGISTER_COMMAND_HANDLER(COMMAND_ADD_BLIP_FOR_CAR, AddBlipForCar);
    REGISTER_COMMAND_HANDLER(COMMAND_ADD_BLIP_FOR_CHAR, AddBlipForChar);
    REGISTER_COMMAND_HANDLER(COMMAND_ADD_BLIP_FOR_OBJECT, AddBlipForObject);
    REGISTER_COMMAND_HANDLER(COMMAND_ADD_BLIP_FOR_COORD, AddBlipForCoord);
    REGISTER_COMMAND_HANDLER(COMMAND_CHANGE_BLIP_DISPLAY, ChangeBlipDisplay);
    REGISTER_COMMAND_HANDLER(COMMAND_ADD_ONE_OFF_SOUND, AddOneOffSound);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_CAR_STUCK_ON_ROOF, IsCarStuckOnRoof);

    // g4
    REGISTER_COMMAND_HANDLER(COMMAND_ADD_UPSIDEDOWN_CAR_CHECK, AddUpsidedownCarCheck);
    REGISTER_COMMAND_HANDLER(COMMAND_REMOVE_UPSIDEDOWN_CAR_CHECK, RemoveUpsidedownCarCheck);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_CAR_STOPPED_IN_AREA_2D, CarInAreaCheck);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_CAR_STOPPED_IN_AREA_3D, CarInAreaCheck);
    REGISTER_COMMAND_HANDLER(COMMAND_LOCATE_CAR_2D, LocateCar);
    REGISTER_COMMAND_HANDLER(COMMAND_LOCATE_STOPPED_CAR_2D, LocateCar);
    REGISTER_COMMAND_HANDLER(COMMAND_LOCATE_CAR_3D, LocateCar);
    REGISTER_COMMAND_HANDLER(COMMAND_LOCATE_STOPPED_CAR_3D, LocateCar);
    REGISTER_COMMAND_HANDLER(COMMAND_GIVE_WEAPON_TO_CHAR, GiveWeaponToChar);
    REGISTER_COMMAND_HANDLER(COMMAND_FORCE_WEATHER, ForceWeather);
    REGISTER_COMMAND_HANDLER(COMMAND_FORCE_WEATHER_NOW, ForceWeatherNow);
    REGISTER_COMMAND_HANDLER(COMMAND_RELEASE_WEATHER, ReleaseWeather);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_GAME_TIMER, GetGameTimer);
    REGISTER_COMMAND_HANDLER(COMMAND_STORE_WANTED_LEVEL, StoreWantedLevel);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_CAR_STOPPED, IsCarStopped);
    REGISTER_COMMAND_HANDLER(COMMAND_MARK_CAR_AS_NO_LONGER_NEEDED, MarkCarAsNoLongerNeeded);
    REGISTER_COMMAND_HANDLER(COMMAND_DONT_REMOVE_CHAR, DontRemoveChar);
    REGISTER_COMMAND_HANDLER(COMMAND_CREATE_CHAR_AS_PASSENGER, CreateCharAsPassenger);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_NUMBER_OF_PASSENGERS, GetNumberOfPassengers);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_MAXIMUM_NUMBER_OF_PASSENGERS, GetMaximumNumberOfPassengers);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_CAR_DENSITY_MULTIPLIER, SetCarDensityMultiplier);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_CAR_HEAVY, SetCarHeavy);
}

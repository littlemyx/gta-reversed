#include <StdInc.h>

#include "../Commands.hpp"
#include <CommandParser/Parser.hpp>
#include "Group09_12.hpp"

#include "World.h"
#include "CarCtrl.h"
#include "3dMarkers.h"
#include "Garages.h"
#include "OnscreenTimer.h"
#include "Radar.h"
#include "Population.h"
#include "UserDisplay.h"
#include "MenuManager.h"
#include "Audio/AudioEngine.h"
#include "Entity/Building.h"
#include "Entity/Vehicle/Automobile.h"
#include "Entity/Vehicle/Bike.h"
#include "Entity/Vehicle/Bmx.h"
#include "Entity/Vehicle/MonsterTruck.h"
#include "Entity/Vehicle/QuadBike.h"
#include "Entity/Vehicle/Heli.h"
#include "Entity/Vehicle/Plane.h"
#include "Entity/Vehicle/Trailer.h"

using namespace notsa::script;
using namespace notsa::script::commands::ported::g09_12;

/*!
* Script commands ported from the exe's group processor for ids 900..999 (`CRunningScript::ProcessCommands900To999` @0x483BD0)
* for the vanilla commands that had no handler of their own (S6-B, g9). Written from the asm of each `case`, see `.notes/S6B_TABLE.md`.
*/

namespace {
//! 0xBA18D9 (set by FREEZE_ONSCREEN_TIMER, cleared by `CMissionCleanup::Init`)
auto& OnscreenTimerFrozen = StaticRef<bool>(0xBA18D9);

//! Inlined `CAutoPilot::SetCarMission` (0x463490, 1 arg): doesn't touch the mission of a crashing plane / heli
void AutoPilotSetCarMissionUnlessCrashing(CAutoPilot& ap, eCarMission mission) {
    if (ap.m_nCarMission != MISSION_PLANE_CRASH_AND_BURN && ap.m_nCarMission != MISSION_HELI_CRASH_AND_BURN) {
        ap.m_nCarMission = mission;
    }
}

//! 906 IS_POINT_OBSCURED_BY_A_MISSION_ENTITY (case @0x483C8C): x, y, z, rx, ry, rz => compare flag
bool IsPointObscuredByAMissionEntity(float x, float y, float z, float rx, float ry, float rz) {
    float minX = x - rx, maxX = x + rx;
    float minY = y - ry, maxY = y + ry;
    float minZ = z - rz, maxZ = z + rz;
    SortPair(minX, maxX);
    SortPair(minY, maxY);
    SortPair(minZ, maxZ);

    int16 count{}; // the exe passes a null entity array and a max count of 2, only the count matters
    CWorld::FindMissionEntitiesIntersectingCube({ minX, minY, minZ }, { maxX, maxY, maxZ }, &count, 2, nullptr, true, true, true);
    return count > 0;
}

//! 908 ADD_TO_OBJECT_VELOCITY (case @0x483DF6): object, x, y, z
void AddToObjectVelocity(CObject& obj, float x, float y, float z) {
    // 0x858B38 = 0.02f; each component is added to the float in memory (one rounded `fadd` per component)
    obj.m_vecMoveSpeed.x = x * 0.02f + obj.m_vecMoveSpeed.x;
    obj.m_vecMoveSpeed.y = y * 0.02f + obj.m_vecMoveSpeed.y;
    obj.m_vecMoveSpeed.z = z * 0.02f + obj.m_vecMoveSpeed.z;
}

//! 914 SET_OBJECT_DYNAMIC (case @0x4841E3): object, dynamic
void SetObjectDynamic(CObject& obj, int32 dynamic) {
    if (dynamic) {
        if (obj.m_bIsStatic) {
            obj.SetIsStatic(false);
            obj.AddToMovingList();
        }
    } else {
        if (!obj.m_bIsStatic) {
            obj.SetIsStatic(true);
            obj.RemoveFromMovingList();
        }
    }
}

//! 916 PLAY_MISSION_PASSED_TUNE (case @0x4842AF): tune (only 1 and 2 play something)
void PlayMissionPassedTune(int32 tune) {
    if (tune == 1 || tune == 2) {
        AudioEngine.PreloadBeatTrack((int16)(tune + 10));
        AudioEngine.PlayPreloadedBeatTrack(true);
    }
}

//! 917 CLEAR_AREA (case @0x4842EC): x, y, z, radius, flag
void ClearArea(float x, float y, float z, float radius, int32 removeProjectilesAndShadows) {
    z = GroundZIfAuto(x, y, z);
    const CVector pos{ x, y, z };
    CWorld::ClearExcitingStuffFromArea(pos, radius, (uint8)removeProjectilesAndShadows); // `mov al, byte [ScriptParams[4]]`: only the low byte is passed
}

//! 918 FREEZE_ONSCREEN_TIMER (case @0x484370): freeze
void FreezeOnscreenTimer(int32 freeze) {
    OnscreenTimerFrozen = freeze != 0;
}

//! 919 SWITCH_CAR_SIREN (case @0x48438F): car, on  -- +0x42D bit 0x80
void SwitchCarSiren(CVehicle& veh, int32 on) {
    veh.vehicleFlags.bSirenOrAlarm = on != 0;
}

//! 924 SET_CAR_WATERTIGHT (case @0x4843DA): car, on  -- automobiles (+0x868 bit 4) and bikes (+0x614 bit 4) only
void SetCarWatertight(CVehicle& veh, int32 on) {
    if (veh.m_nVehicleType == VEHICLE_TYPE_AUTOMOBILE) {
        static_cast<CAutomobile&>(veh).autoFlags.bWaterTight = on != 0;
    } else if (veh.m_nVehicleType == VEHICLE_TYPE_BIKE) {
        static_cast<CBike&>(veh).bikeFlags.bWaterTight = on != 0;
    }
}

//! 927 TURN_CAR_TO_FACE_COORD (case @0x4844B8): car, x, y
void TurnCarToFaceCoord(CVehicle& veh, float x, float y) {
    const CVector& pos = veh.GetPosition();
    const float dx = pos.x - x; // both differences are spilled to float temps
    const float dy = pos.y - y;
    // The arctangent stays on the x87 stack, `+ pi/2` (0x858FE4) is rounded together with it, the result is spilled to a float
    float heading = (float)(CGeneral::GetATanOfXYExt(dx, dy) + (double)1.5707963705062866f);
    if (heading > 6.2831854820251465f) { // 2 pi, 0x858CBC
        heading -= 6.2831854820251465f;
    }
    veh.SetHeading(heading);
}

//! 929 DRAW_SPHERE (case @0x484562): x, y, z, radius
void DrawSphere(CRunningScript& S, float x, float y, float z, float radius) {
    z = GroundZIfAuto(x, y, z);
    CVector pos{ x, y, z };
    C3dMarkers::PlaceMarkerSet(ScriptThingIdFromIP(S), (e3dMarkerType)1, pos, radius, 255, 0, 0, 228, 0x800, 0.1f, 0);
}

//! 930 SET_CAR_STATUS (case @0x484607): car, status
void SetCarStatus(CVehicle& veh, int32 status) {
    if (veh.GetStatus() == STATUS_SIMPLE && status != STATUS_SIMPLE) {
        CCarCtrl::SwitchVehicleToRealPhysics(&veh);
    }
    veh.SetStatus((eEntityStatus)(status & 0x1F)); // `shl al, 3` on a byte, merged with the 3 type bits
}

//! 939 SET_CAR_STRONG (case @0x48470F): car, strong  -- +0x429 bit 0x80
void SetCarStrong(CVehicle& veh, int32 strong) {
    veh.vehicleFlags.bTakeLessDamage = strong != 0;
}

//! 943 SWITCH_STREAMING (case @0x48478E): on
void SwitchStreaming(int32 on) {
    CStreaming::ms_disableStreaming = on == 0;
}

//! 944 IS_GARAGE_OPEN (case @0x4847AD): garage name => compare flag
bool IsGarageOpen(const char* name) {
    const auto garage = CGarages::GetGarageNumberByName(name);
    return garage >= 0 && CGarages::IsGarageOpen(garage);
}

//! 945 IS_GARAGE_CLOSED (case @0x4847E4): garage name => compare flag
bool IsGarageClosed(const char* name) {
    const auto garage = CGarages::GetGarageNumberByName(name);
    return garage >= 0 && CGarages::IsGarageClosed(garage);
}

//! 950 SWAP_NEAREST_BUILDING_MODEL (case @0x48481B): x, y, z, radius, from model, to model
void SwapNearestBuildingModel(float x, float y, float z, float radius, Model fromModel, Model toModel) {
    z = GroundZIfAuto(x, y, z);
    float bestDist = radius + radius;

    CEntity* found[16];
    int16    numFound{};
    const CVector pos{ x, y, z };
    CWorld::FindObjectsOfTypeInRange((uint32)fromModel.value, pos, radius, true, &numFound, 16, found, true, false, false, false, false);

    CEntity* closest{};
    for (uint16 i = 0; (int32)i < numFound; i++) {
        CEntity* const e = found[i];
        const auto&    ep = e->GetPosition();
        const CVector  delta{ ep.x - pos.x, ep.y - pos.y, ep.z - pos.z };
        const float    dist = delta.Magnitude();
        if (dist < bestDist) { // `fcom` + `test ah, 5` + `jp`: take only if strictly less (ordered)
            bestDist = dist;
            closest  = e;
        }
    }
    if (!closest) {
        return;
    }
    auto* const bld = static_cast<CBuilding*>(closest);
    bld->ReplaceWithNewModel(toModel.value);
    CTheScripts::AddToBuildingSwapArray(bld, fromModel.value, toModel.value);
}

//! 951 SWITCH_WORLD_PROCESSING (case @0x4849CC): on  -- sets `CWorld::bProcessCutsceneOnly = !on`
void SwitchWorldProcessing(int32 on) {
    CWorld::bProcessCutsceneOnly = on == 0;
}

//! 954 CLEAR_AREA_OF_CARS (case @0x4849EB): x1, y1, z1, x2, y2, z2 (corners are sorted)
void ClearAreaOfCars(float x1, float y1, float z1, float x2, float y2, float z2) {
    SortPair(x1, x2);
    SortPair(y1, y2);
    SortPair(z1, z2);
    CWorld::ClearCarsFromArea(x1, y1, z1, x2, y2, z2);
}

//! 956 ADD_SPHERE (case @0x484AB4): x, y, z, radius => 1 handle
//! The exe also peeks the output variable (`CollectNextParameterWithoutIncreasingPC`) and calls `GetActualScriptThingIndex(v, 0)`,
//! but throws the result away (both are pure), so they are not reproduced.
uint32 AddSphere(CRunningScript& S, float x, float y, float z, float radius) {
    z = GroundZIfAuto(x, y, z);
    return CTheScripts::AddScriptSphere(ScriptThingIdFromIP(S), { x, y, z }, radius);
}

//! 957 REMOVE_SPHERE (case @0x484B77): handle
void RemoveSphere(int32 handle) {
    CTheScripts::RemoveScriptSphere(handle);
}

//! 963 DISPLAY_ONSCREEN_TIMER_WITH_STRING (case @0x484C57): var, direction, label
//! The variable is read first (as a global variable *index*), then the direction, then the 8 character GXT key.
void DisplayOnscreenTimerWithString(CRunningScript& S) {
    const uint16 varId = S.GetIndexOfGlobalVariable();
    const int32  direction = Read<int32>(&S);
    char key[24]{};
    S.ReadTextLabelFromScript(key, 8);
    (void)TheText.Get(key); // result unused in the exe (the key itself is stored, the text is looked up when drawing)
    CUserDisplay::OnscnTimer.AddClock(varId, key, (eTimerDirection)(direction != 0));
}

//! 964 DISPLAY_ONSCREEN_COUNTER_WITH_STRING (case @0x484CB3): var, type, label
void DisplayOnscreenCounterWithString(CRunningScript& S) {
    const uint16 varId = S.GetIndexOfGlobalVariable();
    const int32  type = Read<int32>(&S);
    char key[24]{};
    S.ReadTextLabelFromScript(key, 8);
    (void)TheText.Get(key); // result unused in the exe
    CUserDisplay::OnscnTimer.AddCounter(varId, (eOnscreenCounter)(uint16)type, key, 0); // `mov cx, word [ScriptParams[0]]`
}

//! Constructs a vehicle of the given class in the vehicle pool (null if the pool is full)
template<typename T, typename... Args>
T* NewVehicle(int32 modelId, Args... args) {
    void* const mem = CVehicle::operator new(sizeof(T));
    if (!mem) {
        return nullptr;
    }
    return ::new (mem) T(modelId, static_cast<eVehicleCreatedBy>(1), args...); // created-by 1 = RANDOM_VEHICLE
}

//! 965 CREATE_RANDOM_CAR_FOR_CAR_PARK (case @0x484D08): x, y, z, heading
//! NOTE: unlike `CCarCtrl::GetNewVehicleDependingOnCarModel` boats and trains (types 5..8) fall into the default (CAutomobile) here.
void CreateRandomCarForCarPark(float x, float y, float z, float heading) {
    if (CCarCtrl::NumRandomCars >= 45) {
        return;
    }
    const auto modelId = CPopulation::m_AppropriateLoadedCars.PickRandomCar(false, true);
    if ((int32)modelId == -1) {
        return;
    }
    CVehicle* veh{};
    switch (CModelInfo::GetModelInfo(modelId)->AsVehicleModelInfoPtr()->m_nVehicleType) {
    case VEHICLE_TYPE_MTRUCK:  veh = NewVehicle<CMonsterTruck>(modelId); break;
    case VEHICLE_TYPE_QUAD:    veh = NewVehicle<CQuadBike>(modelId); break;
    case VEHICLE_TYPE_HELI:    veh = NewVehicle<CHeli>(modelId); break;
    case VEHICLE_TYPE_PLANE:   veh = NewVehicle<CPlane>(modelId); break;
    case VEHICLE_TYPE_BIKE: {
        const auto bike = NewVehicle<CBike>(modelId);
        if (notsa::IsFixBugs() && !bike) { // BUG: the original writes the bike flags through a null pointer if the pool is full
            return;
        }
        bike->bikeFlags.bOnSideStand = true; // `or byte [bike + 0x614], 0x10`
        veh = bike;
        break;
    }
    case VEHICLE_TYPE_BMX: {
        const auto bmx = NewVehicle<CBmx>(modelId);
        if (notsa::IsFixBugs() && !bmx) { // BUG: same
            return;
        }
        bmx->bikeFlags.bOnSideStand = true;
        veh = bmx;
        break;
    }
    case VEHICLE_TYPE_TRAILER: veh = NewVehicle<CTrailer>(modelId); break;
    default:                   veh = NewVehicle<CAutomobile>(modelId, true); break;
    }
    if (notsa::IsFixBugs() && !veh) { // BUG: the original goes on with a null vehicle (crash) if the pool is full
        return;
    }

    // z = height above road + z (the virtual call at vtable slot 0xD4 = CVehicle::GetHeightAboveRoad)
    const float baseZ = veh->GetHeightAboveRoad() + z;
    const CVector pos{ x, y, baseZ };
    veh->SetPosn(pos);
    veh->SetHeading(heading * 0.017453292f); // 0x8595EC
    CTheScripts::ClearSpaceForMissionEntity(pos, veh);

    veh->SetStatus(STATUS_ABANDONED);
    veh->vehicleFlags.bIsLocked = false;
    veh->vehicleFlags.bIsCarParkVehicle = true;
    CCarCtrl::JoinCarWithRoadSystem(veh);
    AutoPilotSetCarMissionUnlessCrashing(veh->m_autoPilot, MISSION_NONE);
    veh->m_autoPilot.m_nTempAction = TEMPACT_NONE;
    veh->m_autoPilot.m_nCarDrivingStyle = DRIVING_STYLE_STOP_FOR_CARS;
    veh->m_autoPilot.m_speed = 9.0f;
    veh->m_autoPilot.m_nCruiseSpeed = 9;
    veh->m_autoPilot.m_nCurrentLane = 0;
    veh->m_autoPilot.m_nNextLane = 0;
    veh->vehicleFlags.bEngineOn = false;
    CWorld::Add(veh);
}

//! 967 SET_WANTED_MULTIPLIER (case @0x485003): multiplier
void SetWantedMultiplier(float multiplier) {
    FindPlayerWanted(-1)->m_Multiplier = multiplier;
}

//! 969 IS_CAR_VISIBLY_DAMAGED (case @0x485037): car => compare flag  -- +0x42A bit 1
bool IsCarVisiblyDamaged(CVehicle& veh) {
    return veh.vehicleFlags.bIsDamaged;
}

//! 976 HAS_MISSION_AUDIO_LOADED (case @0x4851C7): slot (1-based) => compare flag
bool HasMissionAudioLoaded(int32 slot) {
    return AudioEngine.GetMissionAudioLoadingStatus((uint8)(slot - 1)) == 1;
}

//! 978 HAS_MISSION_AUDIO_FINISHED (case @0x485228): slot (1-based) => compare flag
bool HasMissionAudioFinished(int32 slot) {
    return AudioEngine.IsMissionAudioSampleFinished((uint8)(slot - 1));
}

//! 979 GET_CLOSEST_CAR_NODE_WITH_HEADING (case @0x485261): x, y, z => x, y, z, heading (4 values) + compare flag
OpcodeResult GetClosestCarNodeWithHeading(CRunningScript& S, float x, float y, float z) {
    z = GroundZIfAuto(x, y, z);
    const auto node = ThePaths.FindNodeClosestToCoors({ x, y, z }, (ePathType)0, 999999.88f, 0, 1, 0, 0, 0);
    bool found{};
    const CVector nodePos = ThePaths.FindNodeCoorsForScript(node, &found);
    if (found) {
        const float heading = ThePaths.FindNodeOrientationForCarPlacement(node);
        StoreArg(&S, MultiRet<float, float, float, float>{ nodePos.x, nodePos.y, nodePos.z, heading });
    } else {
        StoreArg(&S, MultiRet<float, float, float, float>{ 0.0f, 0.0f, 0.0f, 0.0f });
    }
    S.UpdateCompareFlag(found);
    return OR_CONTINUE;
}

//! 983 SET_MISSION_AUDIO_POSITION (case @0x4853D9): slot (1-based), x, y, z
void SetMissionAudioPosition(int32 slot, CVector pos) {
    AudioEngine.SetMissionAudioPosition((uint8)(slot - 1), pos);
}

//! 984 ACTIVATE_SAVE_MENU (case @0x48542C): no params
void ActivateSaveMenu() {
    if (!FindPlayerPed(-1)) {
        return;
    }
    if (FindPlayerPed(-1)->bInVehicle) { // `test ch, 1` on the ped flags dword at +0x46C
        return;
    }
    FrontEndMenuManager.m_bIsSaveDone = true; // 0xBA67A7
    FindPlayerPed(-1)->m_vecMoveSpeed = CVector{};  // 0x441130 (CPhysical::SetVelocity)
    FindPlayerPed(-1)->m_vecTurnSpeed = CVector{};  // 0x45AFB0 (CPhysical::SetTurnSpeed)
}

//! 985 HAS_SAVE_GAME_FINISHED (case @0x48548A): => compare flag
bool HasSaveGameFinished() {
    return !FrontEndMenuManager.m_bMenuActive && !FrontEndMenuManager.m_bIsSaveDone; // 0xBA67A4, 0xBA67A7
}

//! 988 ADD_BLIP_FOR_PICKUP (case @0x4854B9): pickup => 1 blip handle
//! The exe also calls `CPickups::GetActualPickupIndex(pickup)` and `CRadar::GetActualBlipArrayIndex(<output var>)`; both are pure and their results unused.
tBlipHandle AddBlipForPickup(int32 pickup) {
    const auto blip = CRadar::SetEntityBlip(BLIP_PICKUP, pickup, 6, BLIP_DISPLAY_BOTH); // (the exe also passes the script name, unused)
    CRadar::ChangeBlipScale(blip, 3);
    return blip;
}
}; // namespace

static_assert(offsetof(CMenuManager, m_bMenuActive) == 0x5C && offsetof(CMenuManager, m_bIsSaveDone) == 0x5F);

void notsa::script::commands::ported::g09_12::RegisterG9() {
    REGISTER_COMMAND_HANDLER_BEGIN("Ported g9");

    REGISTER_COMMAND_HANDLER(COMMAND_IS_POINT_OBSCURED_BY_A_MISSION_ENTITY, IsPointObscuredByAMissionEntity);
    REGISTER_COMMAND_HANDLER(COMMAND_ADD_TO_OBJECT_VELOCITY, AddToObjectVelocity);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_OBJECT_DYNAMIC, SetObjectDynamic);
    REGISTER_COMMAND_HANDLER(COMMAND_PLAY_MISSION_PASSED_TUNE, PlayMissionPassedTune);
    REGISTER_COMMAND_HANDLER(COMMAND_CLEAR_AREA, ClearArea);
    REGISTER_COMMAND_HANDLER(COMMAND_FREEZE_ONSCREEN_TIMER, FreezeOnscreenTimer);
    REGISTER_COMMAND_HANDLER(COMMAND_SWITCH_CAR_SIREN, SwitchCarSiren);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_CAR_WATERTIGHT, SetCarWatertight);
    REGISTER_COMMAND_HANDLER(COMMAND_TURN_CAR_TO_FACE_COORD, TurnCarToFaceCoord);
    REGISTER_COMMAND_HANDLER(COMMAND_DRAW_SPHERE, DrawSphere);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_CAR_STATUS, SetCarStatus);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_CAR_STRONG, SetCarStrong);
    REGISTER_COMMAND_HANDLER(COMMAND_SWITCH_STREAMING, SwitchStreaming);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_GARAGE_OPEN, IsGarageOpen);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_GARAGE_CLOSED, IsGarageClosed);
    REGISTER_COMMAND_HANDLER(COMMAND_SWAP_NEAREST_BUILDING_MODEL, SwapNearestBuildingModel);
    REGISTER_COMMAND_HANDLER(COMMAND_SWITCH_WORLD_PROCESSING, SwitchWorldProcessing);
    REGISTER_COMMAND_HANDLER(COMMAND_CLEAR_AREA_OF_CARS, ClearAreaOfCars);
    REGISTER_COMMAND_HANDLER(COMMAND_ADD_SPHERE, AddSphere);
    REGISTER_COMMAND_HANDLER(COMMAND_REMOVE_SPHERE, RemoveSphere);
    REGISTER_COMMAND_HANDLER(COMMAND_DISPLAY_ONSCREEN_TIMER_WITH_STRING, DisplayOnscreenTimerWithString);
    REGISTER_COMMAND_HANDLER(COMMAND_DISPLAY_ONSCREEN_COUNTER_WITH_STRING, DisplayOnscreenCounterWithString);
    REGISTER_COMMAND_HANDLER(COMMAND_CREATE_RANDOM_CAR_FOR_CAR_PARK, CreateRandomCarForCarPark);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_WANTED_MULTIPLIER, SetWantedMultiplier);
    REGISTER_COMMAND_HANDLER(COMMAND_IS_CAR_VISIBLY_DAMAGED, IsCarVisiblyDamaged);
    REGISTER_COMMAND_HANDLER(COMMAND_HAS_MISSION_AUDIO_LOADED, HasMissionAudioLoaded);
    REGISTER_COMMAND_HANDLER(COMMAND_HAS_MISSION_AUDIO_FINISHED, HasMissionAudioFinished);
    REGISTER_COMMAND_HANDLER(COMMAND_GET_CLOSEST_CAR_NODE_WITH_HEADING, GetClosestCarNodeWithHeading);
    REGISTER_COMMAND_HANDLER(COMMAND_SET_MISSION_AUDIO_POSITION, SetMissionAudioPosition);
    REGISTER_COMMAND_HANDLER(COMMAND_ACTIVATE_SAVE_MENU, ActivateSaveMenu);
    REGISTER_COMMAND_HANDLER(COMMAND_HAS_SAVE_GAME_FINISHED, HasSaveGameFinished);
    REGISTER_COMMAND_HANDLER(COMMAND_ADD_BLIP_FOR_PICKUP, AddBlipForPickup);
}

/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/

#include "StdInc.h"

#include "CarCtrl.h"
#include "TrafficLights.h"
#include "TheScripts.h"
#include "GangWars.h"
#include "Garages.h"
#include "Game.h"
#include "General.h"
#include "GameLogic.h"
#include "CutsceneMgr.h"
#include "TheCarGenerators.h"
#include "eAreaCodes.h"

#include <reversiblebugfixes/Bugs.hpp>
#include <numbers>
#include <bit>

auto& apCarsToKeep = StaticRef<CVehicle*[2]>(0x969084);
auto& aCarsToKeepTime = StaticRef<std::array<uint32, 2>>(0x96907C);

// Tunables of the AI plane flight code (all in .data, names are NOTSA)
//! 0x8A5B2C - Pitch angles tested (from `FindPlaneObstacleAltitude`) when looking for obstacles ahead of an AI plane
auto& s_PlaneAIObstacleProbePitches = StaticRef<std::array<float, 6>>(0x8A5B2C);
//! 0x8A5B44 - Factor of the plane's roll that is added to the elevator
auto& s_PlaneAIRollToElevatorFactor = StaticRef<float>(0x8A5B44);
//! 0x8A5B48 - Factor applied to the roll-rate corrected aileron (rudder) input
auto& s_PlaneAIAileronFactor = StaticRef<float>(0x8A5B48);
//! 0x8A5B4C - Numerator of the roll-rate prediction (30 / timestep)
auto& s_PlaneAIRollRatePredictionFactor = StaticRef<float>(0x8A5B4C);
//! 0x8A5B50 - Elevator gain
auto& s_PlaneAIElevatorGain = StaticRef<float>(0x8A5B50);
//! 0x8A5B54 - Cruise speed of remote controlled planes
auto& s_PlaneAIRCCruiseSpeed = StaticRef<float>(0x8A5B54);

//! 0x422E10 - Casts a ray ahead of the plane in the given direction (pitch/heading), returns true (and the z of the hit point) if something was hit
static bool ProbePlaneObstacle(CPlane* plane, float pitch, float heading, float* outHitZ) {
    const auto& moveSpeed = plane->m_vecMoveSpeed;
    const auto  stepX = (float)((double)moveSpeed.x * 50.0f);
    const auto  stepY = (float)((double)moveSpeed.y * 50.0f);
    const auto  stepZ = (double)moveSpeed.z * 50.0f; // x87: kept in extended precision
    const auto& pos   = plane->GetPosition();

    const CVector origin{
        (float)((double)stepX + pos.x),
        (float)((double)stepY + pos.y),
        (float)(stepZ + pos.z)
    };

    CVector dir{ (float)std::cos((double)heading), (float)std::sin((double)heading), 0.0f };
    dir.Normalise();

    // x87: some of the intermediate values are rounded to float, others aren't
    const auto sinPitch  = std::sin((double)pitch);
    const auto zero      = (float)(0.0f * sinPitch);
    const auto sinPitchF = (float)sinPitch;
    const auto cosPitch  = std::cos((double)pitch);
    const auto xc        = (double)dir.x * cosPitch;
    const auto yc        = (float)((double)dir.y * cosPitch);
    const auto zc        = (float)(cosPitch * dir.z);
    const auto ux        = (float)(xc + zero);
    const auto uy        = (double)yc + zero;
    const auto uz        = (double)zc + sinPitchF;
    const auto tx        = (float)(ux * 200.0f);
    const auto ty        = (float)(uy * 200.0f);
    const auto tz        = uz * 200.0f;

    const CVector target{
        (float)((double)tx + origin.x),
        (float)((double)ty + origin.y),
        (float)(tz + origin.z)
    };

    CColPoint colPoint;
    CEntity*  hitEntity{};
    if (!CWorld::ProcessLineOfSight(origin, target, colPoint, hitEntity, true, false, false, false, false, false, false, true)) {
        return false;
    }
    *outHitZ = colPoint.m_vecPoint.z;
    return true;
}

//! 0x422F80 - Finds the altitude of the highest obstacle ahead of the plane (when heading in `heading`)
static float FindPlaneObstacleAltitude(CPlane* plane, float heading) {
    float highest = 0.0f;
    for (auto i = 0u; i < s_PlaneAIObstacleProbePitches.size(); i++) {
        float hitZ;
        if (!ProbePlaneObstacle(plane, s_PlaneAIObstacleProbePitches[i], heading, &hitZ)) {
            continue;
        }
        if (i == 0) {
            return 100000.0f; // Something is right in front of the plane
        }
        if (!(highest > hitZ)) {
            highest = hitZ;
        }
    }
    return highest;
}

void CCarCtrl::InjectHooks()
{
    RH_ScopedClass(CCarCtrl);
    RH_ScopedCategoryGlobal();

    using namespace ReversibleHooks;
    RH_ScopedInstall(Init, 0x4212E0);
    RH_ScopedInstall(ReInit, 0x4213B0);
    RH_ScopedInstall(InitSequence, 0x421740);
    RH_ScopedInstall(ChooseGangCarModel, 0x421A40);
    RH_ScopedInstall(ChoosePoliceCarModel, 0x421980);
    RH_ScopedInstall(CreateCarForScript, 0x431F80);
    RH_ScopedInstall(ChooseBoatModel, 0x421970);
    RH_ScopedInstall(ChooseCarModelToLoad, 0x421900);
    RH_ScopedInstall(GetNewVehicleDependingOnCarModel, 0x421440);
    RH_ScopedInstall(IsAnyoneParking, 0x42C250);
    RH_ScopedInstall(IsThisVehicleInteresting, 0x423EA0);
    RH_ScopedInstall(JoinCarWithRoadAccordingToMission, 0x432CB0);
    RH_ScopedInstall(PossiblyFireHSMissile, 0x429600);
    RH_ScopedInstall(PruneVehiclesOfInterest, 0x423F10);
    RH_ScopedInstall(RemoveCarsIfThePoolGetsFull, 0x4322B0);
    RH_ScopedInstall(RemoveDistantCars, 0x42CD10);
    RH_ScopedInstall(RemoveFromInterestingVehicleList, 0x423ED0);
    RH_ScopedInstall(ScriptGenerateOneEmergencyServicesCar, 0x42FBC0);
    RH_ScopedInstall(SlowCarDownForObject, 0x426220);
    RH_ScopedInstall(SlowCarOnRailsDownForTrafficAndLights, 0x434790);
    RH_ScopedInstall(FindMaxSteerAngle, 0x427FE0);
    RH_ScopedInstall(GenerateRandomCars, 0x4341C0);
    RH_ScopedInstall(SetUpDriverAndPassengersForVehicle, 0x4217C0);
    RH_ScopedInstall(SwitchBetweenPhysicsAndGhost, 0x4222A0);
    RH_ScopedInstall(FindIntersection2Lines, 0x4226F0);
    RH_ScopedInstall(ClitargetOrientationToLink, 0x422760);
    RH_ScopedInstall(SteerAICarBlockingPlayerForwardAndBack, 0x422B20);
    RH_ScopedInstall(FlyAIPlaneInCertainDirection, 0x423000);
    RH_ScopedInstall(SteerAIPlaneTowardsTargetCoors, 0x423790);
    RH_ScopedInstall(SteerAIPlaneToFollowEntity, 0x4237F0);
    RH_ScopedInstall(SteerAIPlaneToCrashAndBurn, 0x423880);
    RH_ScopedInstall(SteerAIHeliToCrashAndBurn, 0x4238E0);
    RH_ScopedInstall(FlyAIHeliToTarget_FixedOrientation, 0x423940);
    RH_ScopedInstall(RegisterVehicleOfInterest, 0x423DE0);
    RH_ScopedInstall(ClearInterestingVehicleList, 0x423F00);
    RH_ScopedInstall(SwitchVehicleToRealPhysics, 0x423FC0);
    RH_ScopedInstall(UpdateCarCount, 0x424000);
    RH_ScopedInstall(PossiblyRemoveVehicle, 0x424F80);
}

// 0x4212E0
void CCarCtrl::Init() {
    ZoneScoped;

    CarDensityMultiplier = 1.0f;
    NumRandomCars = 0;
    NumLawEnforcerCars = 0;
    NumMissionCars = 0;
    NumParkedCars = 0;
    NumPermanentVehicles = 0;
    NumAmbulancesOnDuty = 0;
    NumFireTrucksOnDuty = 0;

    LastTimeAmbulanceCreated = 0;
    LastTimeFireTruckCreated = 0;
    bAllowEmergencyServicesToBeCreated = true;
    bCarsGeneratedAroundCamera = false;
    CountDownToCarsAtStart = 2;

    TimeNextMadDriverChaseCreated = CGeneral::GetRandomNumberInRange(600.0f, 1200.0f);

    std::ranges::fill(apCarsToKeep, nullptr);
    for (auto& group : CPopulation::m_LoadedGangCars) {
        group.Clear();
    }
    CPopulation::m_AppropriateLoadedCars.Clear();
    CPopulation::m_InAppropriateLoadedCars.Clear();
    CPopulation::m_LoadedBoats.Clear();
}

// 0x4213B0
void CCarCtrl::ReInit() {
    CarDensityMultiplier = 1.0f;
    NumRandomCars = 0;
    NumLawEnforcerCars = 0;
    NumMissionCars = 0;
    NumParkedCars = 0;
    NumPermanentVehicles = 0;
    NumAmbulancesOnDuty = 0;
    NumFireTrucksOnDuty = 0;

    LastTimeLawEnforcerCreated = 0;

    bAllowEmergencyServicesToBeCreated = true;
    CountDownToCarsAtStart = 2;

    std::ranges::fill(apCarsToKeep, nullptr);
    for (auto& group : CPopulation::m_LoadedGangCars) {
        group.Clear();
    }
    CPopulation::m_AppropriateLoadedCars.Clear();
    CPopulation::m_InAppropriateLoadedCars.Clear();
    CPopulation::m_LoadedBoats.Clear();
}

// 0x421970
int32 CCarCtrl::ChooseBoatModel() {
    return CPopulation::m_LoadedBoats.PickLeastUsedModel(1);
}

// 0x421900
int32 CCarCtrl::ChooseCarModelToLoad(int32 groupID) {
    const auto numCarsInGroup = CPopulation::m_nNumCarsInGroup[groupID];
    if (numCarsInGroup > 0) {
        for (auto i = 0; i < 16; i++) { // 16 tries
            const auto model = CPopulation::m_CarGroups[groupID][CGeneral::GetRandomNumberInRange(numCarsInGroup)];
            if (!CStreaming::IsModelLoaded(model)) {
                return model;
            }
        }
    }
    return -1;
}

eModelID CCarCtrl::ChooseGangCarModel(eGangID loadedCarGroupId) {
    return CPopulation::PickGangCar(loadedCarGroupId);
}

// 0x424CE0
int32 CCarCtrl::ChooseModel(int32* arg1) {
    return plugin::CallAndReturn<int32, 0x424CE0, int32*>(arg1);
}

int32 CCarCtrl::ChoosePoliceCarModel(uint32 ignoreLvpd1Model) {
    CWanted* playerWanted = FindPlayerWanted();
    if (playerWanted->AreSwatRequired()
        && CStreaming::IsModelLoaded(MODEL_ENFORCER)
        && CStreaming::IsModelLoaded(MODEL_SWAT)
    ) {
        if (CGeneral::GetRandomNumberInRange(0, 3) == 2)
            return MODEL_ENFORCER;
    }
    else
    {
        if (playerWanted->AreFbiRequired()
            && CStreaming::IsModelLoaded(MODEL_FBIRANCH)
            && CStreaming::IsModelLoaded(MODEL_FBI))
            return MODEL_FBIRANCH;

        if (playerWanted->AreArmyRequired()
            && CStreaming::IsModelLoaded(MODEL_RHINO)
            && CStreaming::IsModelLoaded(MODEL_BARRACKS)
            && CStreaming::IsModelLoaded(MODEL_ARMY))
            return (CGeneral::GetRandomNumber() < 0x3FFF) + MODEL_RHINO;
    }
    return CStreaming::GetDefaultCopCarModel(ignoreLvpd1Model);
}

// 0x423F00
void CCarCtrl::ClearInterestingVehicleList() {
    std::ranges::fill(apCarsToKeep, nullptr);
}

// 0x422760
void CCarCtrl::ClitargetOrientationToLink(CVehicle* vehicle, CCarPathLinkAddress linkAddr, int8 dirSign, float* pOrientation, float targetX, float targetY) {
    if (!ThePaths.m_pPathNodes[linkAddr.m_wAreaId]) {
        return;
    }
    const auto& link = ThePaths.GetCarPathLink(linkAddr);

    // `CCarPathLink::m_dir`/`m_nPathNodeWidth` hide their raw values, but the original works on them
    const auto raw = std::bit_cast<std::array<uint8, sizeof(CCarPathLink)>>(link);
    const auto dirX = (int32)(int8)raw[8], dirY = (int32)(int8)raw[9];
    const auto width = (uint32)raw[10];

    // Direction vector scaled by `dirSign` (the intermediates are kept in extended precision)
    const auto dirXScaled = (float)((double)dirX * 0.01f * (double)dirSign);
    const auto dirYScaled = (float)((double)dirY * 0.01f * (double)dirSign);

    // 0x422760 - result is unused:
    // CGeneral::GetATanOfXY(targetX - vehicle->GetPosition().x, targetY - vehicle->GetPosition().y);

    const auto numOppositeLanes = (uint32)link.m_numOppositeDirLanes;
    const auto numSameLanes     = (uint32)link.m_numSameDirLanes;

    float  laneA; // stored as float
    double laneB; // kept on the x87 stack
    if (numOppositeLanes == 0) {
        laneA = (float)((double)numSameLanes * 0.5f);
        laneB = laneA;
    } else if (numSameLanes == 0) {
        laneA = (float)((double)numOppositeLanes * 0.5f);
        laneB = laneA;
    } else {
        const auto [a, b] = dirSign == 0
            ? std::pair{ numOppositeLanes, numSameLanes }
            : std::pair{ numSameLanes, numOppositeLanes };
        laneA = (float)((double)width * 0.011574074f + (double)a);
        laneB = (double)b;
    }

    const auto laneAOffset = (double)laneA - 0.3f;
    const auto laneBOffset = (float)(laneB - 0.3f);

    // Points to the left/right of the target
    const auto x1 = (float)((double)dirYScaled * laneAOffset * 5.4f + targetX);
    const auto y1 = (double)targetY - (double)dirXScaled * laneAOffset * 5.4f; // not rounded to float
    const auto x2 = (float)((double)targetX - (double)laneBOffset * dirYScaled * 5.4f);
    const auto y2 = (float)((double)targetY + (double)laneBOffset * dirXScaled * 5.4f);

    const auto& vehPos = vehicle->GetPosition();
    const auto angle1 = CGeneral::GetATanOfXY(x1 - vehPos.x, (float)(y1 - vehPos.y));
    const auto angle2 = CGeneral::GetATanOfXY(x2 - vehPos.x, y2 - vehPos.y);

    constexpr auto PI = std::numbers::pi_v<float>;

    const auto origOrientation = *pOrientation;
    auto  diff1  = (double)(float)(angle1 - origOrientation);
    auto  diff1f = (float)diff1; // stored copy (only updated if wrapped)
    auto  diff2  = (double)angle2 - origOrientation;

    if (diff1 > PI) {
        do { diff1 -= 2.0f * PI; } while (diff1 > PI);
        diff1f = (float)diff1;
    }
    if (diff1 < -PI) {
        do { diff1 += 2.0f * PI; } while (diff1 < -PI);
        diff1f = (float)diff1;
    }
    while (diff2 > PI) {
        diff2 -= 2.0f * PI;
    }
    while (diff2 < -PI) {
        diff2 += 2.0f * PI;
    }

    if (diff1 < 0.0 && diff2 < 0.0) {
        *pOrientation = diff1 > diff2
            ? (float)((double)diff1f + origOrientation)
            : (float)(diff2 + origOrientation);
    } else if (diff1 > 0.0 && diff2 > 0.0) {
        *pOrientation = diff1 < diff2
            ? (float)((double)diff1f + origOrientation)
            : (float)(diff2 + origOrientation);
    }

    // Wrap into [0, 2PI]
    if (*pOrientation < 0.0f) {
        double v = *pOrientation;
        do { v += 2.0f * PI; } while (v < 0.0);
        *pOrientation = (float)v;
    }
    if (*pOrientation > 2.0f * PI) {
        double v = *pOrientation;
        do { v -= 2.0f * PI; } while (v > 2.0f * PI);
        *pOrientation = (float)v;
    }
}

// 0x431F80
CVehicle* CCarCtrl::CreateCarForScript(int32 modelid, CVector posn, bool doMissionCleanup) {
    if (CModelInfo::IsBoatModel(modelid))
    {
        auto* boat = new CBoat(modelid, eVehicleCreatedBy::MISSION_VEHICLE);
        if (posn.z <= MAP_Z_LOW_LIMIT)
            posn.z = CWorld::FindGroundZForCoord(posn.x, posn.y);

        posn.z += boat->GetDistanceFromCentreOfMassToBaseOfModel();
        boat->SetPosn(posn);

        CTheScripts::ClearSpaceForMissionEntity(posn, boat);
        boat->vehicleFlags.bEngineOn = false;
        boat->vehicleFlags.bIsLocked = true;
        boat->SetStatus(STATUS_ABANDONED);
        JoinCarWithRoadSystem(boat);

        boat->m_autoPilot.SetCarMission(eCarMission::MISSION_NONE);
        boat->m_autoPilot.m_nTempAction = TEMPACT_NONE;
        boat->m_autoPilot.m_speed = 20.0F;
        boat->m_autoPilot.SetCruiseSpeed(20);

        if (doMissionCleanup)
            boat->m_bIsStaticWaitingForCollision = true;

        boat->m_autoPilot.movementFlags.bIsStopped = true;
        CWorld::Add(boat);

        if (doMissionCleanup)
            CTheScripts::MissionCleanUp.AddEntityToList(GetVehiclePool()->GetRef(boat), MISSION_CLEANUP_ENTITY_TYPE_VEHICLE);

        return boat;
    }

    auto* vehicle = GetNewVehicleDependingOnCarModel(modelid, eVehicleCreatedBy::MISSION_VEHICLE);
    if (posn.z <= MAP_Z_LOW_LIMIT)
        posn.z = CWorld::FindGroundZForCoord(posn.x, posn.y);

    posn.z += vehicle->GetDistanceFromCentreOfMassToBaseOfModel();
    vehicle->SetPosn(posn);

    if (!doMissionCleanup)
    {
        if (vehicle->IsAutomobile())
            vehicle->AsAutomobile()->PlaceOnRoadProperly();
        else if (vehicle->IsBike())
            vehicle->AsBike()->PlaceOnRoadProperly();
    }

    if (vehicle->IsTrain())
        vehicle->AsTrain()->trainFlags.bNotOnARailRoad = true;

    CTheScripts::ClearSpaceForMissionEntity(posn, vehicle);
    vehicle->vehicleFlags.bIsLocked = true;
    vehicle->SetStatus(STATUS_ABANDONED);
    JoinCarWithRoadSystem(vehicle);
    vehicle->vehicleFlags.bEngineOn = false;
    vehicle->vehicleFlags.bHasBeenOwnedByPlayer = true;

    vehicle->m_autoPilot.SetCarMission(eCarMission::MISSION_NONE);
    vehicle->m_autoPilot.m_nTempAction = TEMPACT_NONE;
    vehicle->m_autoPilot.m_nCarDrivingStyle = DRIVING_STYLE_STOP_FOR_CARS;
    vehicle->m_autoPilot.m_speed = 13.0F;
    vehicle->m_autoPilot.SetCruiseSpeed(13);
    vehicle->m_autoPilot.m_nCurrentLane = 0;
    vehicle->m_autoPilot.m_nNextLane = 0;

    if (doMissionCleanup)
        vehicle->m_bIsStaticWaitingForCollision = true;

    CWorld::Add(vehicle);
    if (doMissionCleanup)
        CTheScripts::MissionCleanUp.AddEntityToList(GetVehiclePool()->GetRef(vehicle), MISSION_CLEANUP_ENTITY_TYPE_VEHICLE);

    if (vehicle->IsSubRoadVehicle())
        vehicle->m_autoPilot.movementFlags.bIsStopped = true;

    return vehicle;
}

// 0x42C740
bool CCarCtrl::CreateConvoy(CVehicle* vehicle, int32 arg2) {
    return plugin::CallAndReturn<bool, 0x42C740, CVehicle*, int32>(vehicle, arg2);
}

// 0x42C2B0
bool CCarCtrl::CreatePoliceChase(CVehicle* vehicle, int32 arg2, CNodeAddress NodeAddress) {
    return plugin::CallAndReturn<bool, 0x42C2B0, CVehicle*, int32, CNodeAddress>(vehicle, arg2, NodeAddress);
}

// 0x428040
bool CCarCtrl::DealWithBend_Racing(CVehicle* vehicle, CCarPathLinkAddress LinkAddress1, CCarPathLinkAddress LinkAddress2, CCarPathLinkAddress LinkAddress3, CCarPathLinkAddress LinkAddress4, char arg6, char arg7, char arg8, char arg9, float arg10, float* arg11, float* arg12, float* arg13, float* arg14, CVector* pos) {
    return plugin::CallAndReturn<bool, 0x428040, CVehicle*, CCarPathLinkAddress, CCarPathLinkAddress, CCarPathLinkAddress, CCarPathLinkAddress, int8, int8, int8, int8, float, float*, float*, float*, float*, CVector*>(vehicle, LinkAddress1, LinkAddress2, LinkAddress3, LinkAddress4, arg6, arg7, arg8, arg9, arg10, arg11, arg12, arg13, arg14, pos);
}

// 0x42EC90
void CCarCtrl::DragCarToPoint(CVehicle* vehicle, CVector* pos) {
    plugin::Call<0x42EC90, CVehicle*, CVector*>(vehicle, pos);
}

// 0x4325C0
float CCarCtrl::FindAngleToWeaveThroughTraffic(CVehicle* vehicle, CPhysical* physical, float arg3, float arg4, float arg5) {
    return plugin::CallAndReturn<float, 0x4325C0, CVehicle*, CPhysical*, float, float, float>(vehicle, physical, arg3, arg4, arg5);
}

// 0x4226F0
void CCarCtrl::FindIntersection2Lines(float x1, float y1, float dx1, float dy1, float x2, float y2, float dx2, float dy2, float* outX, float* outY) {
    // x87: everything is kept in extended precision until the final stores
    const auto denom = (double)dx1 * dy2 - (double)dy1 * dx2;
    const auto t = denom != 0.0
        ? (((double)x2 - x1) * dy2 - ((double)y2 - y1) * dx2) / denom
        : 0.0;
    *outX = (float)(dx1 * t + x1);
    *outY = (float)(t * dy1 + y1);
}

// 0x42B470
void CCarCtrl::FindLinksToGoWithTheseNodes(CVehicle* vehicle) {
    plugin::Call<0x42B470, CVehicle*>(vehicle);
}

// 0x434400
float CCarCtrl::FindMaximumSpeedForThisCarInTraffic(CVehicle* vehicle) {
    return plugin::CallAndReturn<float, 0x434400, CVehicle*>(vehicle);
}

// 0x42BD20
void CCarCtrl::FindNodesThisCarIsNearestTo(CVehicle* vehicle, CNodeAddress& nodeAddress1, CNodeAddress& nodeAddress2) {
    plugin::Call<0x42BD20, CVehicle*, CNodeAddress&, CNodeAddress&>(vehicle, nodeAddress1, nodeAddress2);
}

// 0x422090
int8 CCarCtrl::FindPathDirection(CNodeAddress nodeAddress1, CNodeAddress nodeAddress2, CNodeAddress nodeAddress3, bool* arg4) {
    return plugin::CallAndReturn<int8, 0x422090, CNodeAddress, CNodeAddress, CNodeAddress, bool*>(nodeAddress1, nodeAddress2, nodeAddress3, arg4);
}

// 0x422620
float CCarCtrl::FindPercDependingOnDistToLink(CVehicle* vehicle, CCarPathLinkAddress linkAddress) {
    return plugin::CallAndReturn<float, 0x422620, CVehicle*, CCarPathLinkAddress>(vehicle, linkAddress);
}

// 0x421770
int32 CCarCtrl::FindSequenceElement(int32 arg1) {
    return plugin::CallAndReturn<int32, 0x421770, int32>(arg1);
}

// 0x4224E0
float CCarCtrl::FindSpeedMultiplier(float arg1, float arg2, float arg3, float arg4) {
    return plugin::CallAndReturn<float, 0x4224E0, float, float, float, float>(arg1, arg2, arg3, arg4);
}

// 0x424130
float CCarCtrl::FindSpeedMultiplierWithSpeedFromNodes(int8 arg1) {
    return plugin::CallAndReturn<float, 0x424130, int8>(arg1);
}

float CCarCtrl::FindGhostRoadHeight(CVehicle* vehicle) {
    return plugin::CallAndReturn<float, 0x422370, CVehicle*>(vehicle);
}

// 0x42B270
void CCarCtrl::FireHeliRocketsAtTarget(CAutomobile* entityLauncher, CEntity* entity) {
    plugin::Call<0x42B270, CAutomobile*, CEntity*>(entityLauncher, entity);
}

// 0x429A70
void CCarCtrl::FlyAIHeliInCertainDirection(CHeli* heli, float arg2, float arg3, bool arg4) {
    plugin::Call<0x429A70, CHeli*, float, float, bool>(heli, arg2, arg3, arg4);
}

// 0x423940
void CCarCtrl::FlyAIHeliToTarget_FixedOrientation(CHeli* heli, float orientation, CVector posn) {
    constexpr auto PI = std::numbers::pi_v<float>;

    const auto& moveSpeed = heli->m_vecMoveSpeed;
    const auto& mat       = *heli->m_matrix; // Not null checked in the original

    // Twice a second (at a different time for each heli) check for obstacles below the heli
    const uint32 seed = heli->m_nRandomSeed;
    if ((seed + CTimer::m_snTimeInMilliseconds) % 500 < (CTimer::m_snPreviousTimeInMilliseconds + seed) % 500) {
        heli->field_9AC = heli->m_fMaxAltitude;

        const auto& pos = heli->GetPosition();
        const auto stepX = (float)((double)moveSpeed.x * 50.0f);
        const auto stepY = (float)((double)moveSpeed.y * 50.0f);
        const auto stepZ = (double)moveSpeed.z * 50.0f; // x87: kept in extended precision
        const CVector origin{
            (float)((double)stepX + pos.x),
            (float)((double)stepY + pos.y),
            (float)(stepZ + pos.z)
        };

        CVector dir{ (float)std::cos((double)orientation), (float)std::sin((double)orientation), -1.0f };
        dir.Normalise();

        const auto offsetZ = (float)((double)dir.z * 60.0f);
        const CVector target{
            (float)((double)dir.x * 60.0f + origin.x),
            (float)((double)dir.y * 60.0f + origin.y),
            (float)((double)offsetZ + origin.z)
        };

        CColPoint colPoint;
        CEntity*  hitEntity{};
        if (CWorld::ProcessLineOfSight(origin, target, colPoint, hitEntity, true, false, false, false, false, false, false, true)) {
            auto altitude = (double)colPoint.m_vecPoint.z + heli->m_fMinAltitude;
            if (altitude < heli->field_9AC) {
                altitude = heli->field_9AC;
            }
            heli->field_9AC = (float)altitude;
        }
    }

    const auto heading = CGeneral::GetATanOfXY(mat.GetForward().x, mat.GetForward().y);

    // Throttle: Try to get to the wanted altitude
    {
        heli->m_fAccelerationBreakStatus = 0.0f;
        const auto altDiff = (double)heli->field_9AC - ((double)moveSpeed.z * 100.0f + heli->GetPosition().z);
        heli->m_fAccelerationBreakStatus = (float)(altDiff * (altDiff > 0.0 ? 0.1f : 0.2f));

        // Add some noise
        auto throttle = ((double)(rand() & 0xF) - 7.0f) * 0.00200000009f + heli->m_fAccelerationBreakStatus;
        heli->m_fAccelerationBreakStatus = (float)throttle;
        if (throttle < 1.0f) {
            if (-0.3f > throttle) {
                throttle = -0.3f;
            }
        } else {
            throttle = 1.0f;
        }
        heli->m_fAccelerationBreakStatus = (float)throttle;
    }

    // Steer: Rotate towards the wanted orientation
    {
        auto headingDiff = (double)orientation - heading;
        if (headingDiff > PI) {
            do { headingDiff -= 2.0f * PI; } while (headingDiff > PI);
        }
        if (headingDiff < -PI) {
            do { headingDiff += 2.0f * PI; } while (headingDiff < -PI);
        }
        auto steer = headingDiff * -0.5f;
        heli->m_fLeftRightSkid = (float)steer;
        if (steer < 1.0f) {
            if (-1.0f > steer) {
                steer = -1.0f;
            }
        } else {
            steer = 1.0f;
        }
        heli->m_fLeftRightSkid = (float)steer;
    }

    // Move towards the target position
    const auto& pos = heli->GetPosition();
    const auto dx = (double)posn.x - pos.x;
    const auto dy = (double)posn.y - pos.y;

    const auto speedRight = (float)(((double)moveSpeed.z * mat.GetRight().z + (double)moveSpeed.y * mat.GetRight().y) + (double)mat.GetRight().x * moveSpeed.x);
    const auto speedFwd   = (float)(((double)moveSpeed.z * mat.GetForward().z + (double)moveSpeed.y * mat.GetForward().y) + (double)moveSpeed.x * mat.GetForward().x);

    const auto rightErr = (float)((((double)dy * mat.GetRight().y + (double)dx * mat.GetRight().x) + (double)mat.GetRight().z * 0.0f) + (double)speedRight * 80.0f);
    const auto fwdErr   = (((double)dy * mat.GetForward().y + (double)dx * mat.GetForward().x) + (double)mat.GetForward().z * 0.0f) + (double)speedFwd * 80.0f; // not rounded (x87)

    // Left/right
    heli->m_fSteeringLeftRight = (std::abs(rightErr) < 5.0f)
        ? speedRight
        : (float)((double)rightErr * -0.02);
    if (heli->m_fSteeringLeftRight < 0.75f) {
        if (-0.75f > heli->m_fSteeringLeftRight) {
            heli->m_fSteeringLeftRight = -0.75f;
        }
    } else {
        heli->m_fSteeringLeftRight = 0.75f;
    }

    // Forward/backward
    heli->m_fSteeringUpDown = ((fwdErr < 0.0 ? -fwdErr : fwdErr) < 5.0f)
        ? speedFwd
        : (float)(fwdErr * -0.015);
    if (heli->m_fSteeringUpDown < 0.5f && -0.5f > heli->m_fSteeringUpDown) {
        heli->m_fSteeringUpDown = -0.5f;
    } else if (!(heli->m_fSteeringUpDown < 0.5f)) {
        heli->m_fSteeringUpDown = 0.5f;
    }
}

// 0x423000
void CCarCtrl::FlyAIPlaneInCertainDirection(CPlane* plane) {
    constexpr auto PI = std::numbers::pi_v<float>;

    const auto& moveSpeed = plane->m_vecMoveSpeed;
    const auto& mat       = *plane->m_matrix; // Not null checked in the original
    const auto& fwd       = mat.GetForward();

    // Once a second (at a different time for each plane) pick a new altitude/heading to avoid obstacles
    const uint32 seed = plane->m_nRandomSeed;
    if ((seed + CTimer::m_snTimeInMilliseconds) % 1000 < (CTimer::m_snPreviousTimeInMilliseconds + seed) % 1000) {
        const auto curHeading = CGeneral::GetATanOfXY(fwd.x, fwd.y);

        auto targetHeading = plane->m_planeHeading;
        switch (plane->m_autoPilot.m_nTempAction) {
        case TEMPACT_PLANE_FLY_STRAIGHT:
            targetHeading = curHeading;
            break;
        case TEMPACT_PLANE_SHARP_LEFT:
            targetHeading = (float)((double)curHeading - 2.0f);
            break;
        case TEMPACT_PLANE_SHARP_RIGHT:
            targetHeading = (float)(2.0f + (double)curHeading);
            break;
        }

        plane->m_altitude         = 500.0f;
        plane->m_planeHeadingPrev = plane->m_planeHeading + PI;

        // x87: the heading difference is kept in extended precision
        auto headingDiff = (double)curHeading - targetHeading;
        if (headingDiff > PI) {
            do { headingDiff -= 2.0f * PI; } while (headingDiff > PI);
        }
        if (headingDiff < -PI) {
            do { headingDiff += 2.0f * PI; } while (headingDiff < -PI);
        }

        const auto absHeadingDiff = headingDiff < 0.0 ? -headingDiff : headingDiff;
        const auto turnAngle      = absHeadingDiff < 0.52359879f // 30 deg
            ? 0.0f
            : (float)(headingDiff * 1.5f);

        for (auto pass = 0; pass < 2; pass++) {
            for (auto i = 1; i < 20; i++) {
                const auto angle = (float)((double)(i / 2) * ((i & 1) ? 0.261799395f : -0.261799395f)); // 15 deg steps, alternating sides
                const auto probeHeading = (float)((double)angle + targetHeading);

                // Is this angle between the straight ahead and the turn angle?
                const auto isInTurnDirection = (angle < 0.0f && angle > turnAngle) || (angle > 0.0f && angle < turnAngle);
                if (pass == 0 ? !isInTurnDirection : isInTurnDirection) {
                    continue;
                }

                const auto obstacleAlt = FindPlaneObstacleAltitude(plane, probeHeading);
                if (!(obstacleAlt < 150.0f)) {
                    continue;
                }

                plane->m_planeHeadingPrev = (float)((double)angle * 1.10000002f + plane->m_planeHeading);

                auto altitude = (double)obstacleAlt + plane->m_minAltitude;
                if (!(altitude > plane->m_maxAltitude)) {
                    altitude = plane->m_maxAltitude;
                }
                plane->m_altitude = (float)altitude;
                break;
            }
        }
    }

    const float desiredSpeed = plane->vehicleFlags.bIsRCVehicle
        ? s_PlaneAIRCCruiseSpeed
        : 32.0f;

    const float heading  = CGeneral::GetATanOfXY(fwd.x, fwd.y);
    const auto  curSpeed = (float)(((double)fwd.y * moveSpeed.y + (double)fwd.x * moveSpeed.x) * 60.0f);

    if (std::ranges::any_of(plane->m_fWheelsSuspensionCompression, [](float v) { return v < 1.0f; })) {
        plane->m_nStartedFlyingTime = CTimer::m_snTimeInMilliseconds;
    }

    bool isTakingOff;
    if (CTimer::m_snTimeInMilliseconds - plane->m_nStartedFlyingTime <= 4000) {
        isTakingOff = true;
        plane->m_fSteeringUpDown = curSpeed < desiredSpeed ? 0.0f : 0.4f;
    } else {
        isTakingOff = false;

        if (plane->m_fLandingGearStatus != 1.0f) {
            plane->SetGearUp();
        }

        const auto predictedZ = (float)((double)moveSpeed.z * 100.0f + plane->GetPosition().z);

        // 0x821E70 is the CRT's asin
        const auto pitch     = std::asin((double)fwd.z);
        const auto prevPitch = plane->m_forwardZ;
        const auto timeStep  = (double)CTimer::ms_fTimeStep;
        plane->m_forwardZ    = (float)pitch;
        const auto predictedPitch = (float)((100.0f / timeStep) * (pitch - prevPitch) + pitch);

        auto climb = ((double)plane->m_altitude - predictedZ) * 0.0333333351f;
        if (!(climb < 0.4f)) {
            climb = 0.4f;
        } else if (!(climb > -0.4f)) {
            climb = -0.4f;
        }
        if (curSpeed < desiredSpeed && !(climb < 0.25f)) {
            climb = 0.25f;
        }
        plane->m_fSteeringUpDown = (float)((climb - predictedPitch) * s_PlaneAIElevatorGain);
    }

    auto headingError = (double)plane->m_planeHeadingPrev - heading;
    if (headingError < -PI) {
        do { headingError += 2.0f * PI; } while (headingError < -PI);
    }
    if (headingError > PI) {
        do { headingError -= 2.0f * PI; } while (headingError > PI);
    }

    if (isTakingOff) {
        auto steer = -((double)plane->m_planeCreationHeading - heading) * 10.0f;
        if (1.0f < steer) {
            steer = 1.0f;
        } else if (-1.0f > steer) {
            steer = -1.0f;
        }
        plane->m_fLeftRightSkid     = (float)steer;
        plane->m_fSteeringLeftRight = 0.0f;
    } else {
        auto turn = -headingError * 1.5f;
        if (0.9f < turn) {
            turn = 0.9f;
        } else if (-0.9f > turn) {
            turn = -0.9f;
        }

        double bank, absBank;
        const auto SetBankFromTurn = [&] {
            bank    = turn;
            absBank = turn < 0.0 ? -turn : turn;
        };
        if (plane->vehicleFlags.bIsRCVehicle) {
            if (0.7f < turn) {
                bank = absBank = 0.7f;
            } else if (-0.7f > turn) {
                bank    = -0.7f;
                absBank = 0.7f;
            } else {
                SetBankFromTurn();
            }
        } else {
            SetBankFromTurn();
        }

        double yaw;
        if (!(absBank < 0.1f)) {
            plane->m_fLeftRightSkid = (float)bank;
            yaw = -bank;
        } else {
            plane->m_fLeftRightSkid = (float)(bank * 4.0f);
            yaw = 0.0;
        }

        const auto slowSpeedLimit = (double)desiredSpeed * 1.20000005f;
        if (curSpeed < slowSpeedLimit) {
            auto factor = 1.0f - (slowSpeedLimit - curSpeed) / ((double)desiredSpeed * 0.5f);
            if (0.0 > factor) {
                factor = 0.0;
            }
            yaw *= factor;
        }

        // NOTE: The original checks if the matrix is null here, but it's dereferenced above already
        auto len = std::sqrt((double)mat.GetRight().x * mat.GetRight().x + (double)mat.GetRight().y * mat.GetRight().y);
        if (mat.GetUp().z < 0.0f) {
            len *= -1.0f;
        }
        const auto roll = (float)std::atan2((double)mat.GetRight().z, len);

        const auto timeStep = CTimer::ms_fTimeStep < 1.0f ? 1.0 : (double)CTimer::ms_fTimeStep;
        auto rollError = yaw - (((double)roll - plane->m_fSteeringFactor) * (s_PlaneAIRollRatePredictionFactor / timeStep) + roll);
        if (rollError > PI) {
            do { rollError -= 2.0f * PI; } while (rollError > PI);
        }
        if (rollError < -PI) {
            do { rollError += 2.0f * PI; } while (rollError < -PI);
        }
        rollError *= s_PlaneAIAileronFactor;

        plane->m_fSteeringFactor = roll;
        if (!(rollError < 1.0f)) {
            rollError = 1.0f;
        }
        if (!(rollError > -1.0f)) {
            rollError = -1.0f;
        }
        plane->m_fSteeringLeftRight = (float)rollError;

        const double elevator = plane->m_fSteeringUpDown;
        auto rollCompensation = s_PlaneAIRollToElevatorFactor * (double)roll;
        if (rollCompensation < 0.0) {
            rollCompensation = -rollCompensation;
        }
        const auto newElevator = (float)(rollCompensation + elevator);
        plane->m_fSteeringUpDown = newElevator;
        if (elevator < 0.0) {
            const auto halved = elevator * 0.5f;
            plane->m_fSteeringUpDown = halved > newElevator ? newElevator : (float)halved;
        }
    }

    auto elevator = plane->m_fSteeringUpDown < 1.0f ? (double)plane->m_fSteeringUpDown : 1.0;
    if (!(elevator > -1.0f)) {
        elevator = -1.0f;
    }
    plane->m_fSteeringUpDown = (float)elevator;

    plane->m_fAccelerationBreakStatus = plane->m_fAccelerationBreakStatusPrev;

    if (plane->m_autoPilot.m_nTempAction == TEMPACT_PLANE_FLY_UP) {
        plane->m_fSteeringUpDown = 1.0f;
        if (curSpeed < 20.0) {
            plane->m_autoPilot.m_nTempAction = TEMPACT_NONE;
        }
    }
}

// 0x424210
bool CCarCtrl::GenerateCarCreationCoors2(CVector posn, float radius, float arg3, float arg4, bool arg5, float arg6, float arg7, CVector* pOrigin, CNodeAddress* pNodeAddress1, CNodeAddress* pNodeAddress12, float* arg11, bool arg12, bool arg13) {
    return plugin::CallAndReturn<bool, 0x424210, CVector, float, float, float, bool, float, float, CVector*, CNodeAddress*, CNodeAddress*, float*, bool, bool>(posn, radius, arg3, arg4, arg5, arg6, arg7, pOrigin, pNodeAddress1, pNodeAddress12, arg11, arg12, arg13);
}

// 0x42F9C0
void CCarCtrl::GenerateEmergencyServicesCar() {
    plugin::Call<0x42F9C0>();
}

// 0x42B7D0
CAutomobile* CCarCtrl::GenerateOneEmergencyServicesCar(uint32 modelId, CVector posn) {
    return plugin::CallAndReturn<CAutomobile*, 0x42B7D0, uint32, CVector>(modelId, posn);
}

// 0x430050
void CCarCtrl::GenerateOneRandomCar() {
    plugin::Call<0x430050>();
}

// 0x4341C0
void CCarCtrl::GenerateRandomCars() {
    if (CCutsceneMgr::ms_running) {
        CountDownToCarsAtStart = 2;
        return;
    }
    if (CGangWars::DontCreateCivilians() || !CGame::CanSeeOutSideFromCurrArea()) {
        return;
    }

    if (CGameLogic::LaRiotsActiveHere() && TimeNextMadDriverChaseCreated > 480.0f) {
        TimeNextMadDriverChaseCreated = CGeneral::GetRandomNumberInRange(240.0f, 480.0f);
    }
    TimeNextMadDriverChaseCreated -= (CTimer::GetTimeStep() * 0.02f);

    if (NumRandomCars < 45) {
        if (CountDownToCarsAtStart) {
            CountDownToCarsAtStart--;
            for (auto i = 100; i --> 0;) {
                GenerateOneRandomCar();
            }
            CTheCarGenerators::GenerateEvenIfPlayerIsCloseCounter = 20;
        } else {
            GenerateOneRandomCar();
            GenerateOneRandomCar();
        }
    }

}

// 0x42F3C0
void CCarCtrl::GetAIHeliToAttackPlayer(CAutomobile* automobile) {
    plugin::Call<0x42F3C0, CAutomobile*>(automobile);
}

// 0x42A730
void CCarCtrl::GetAIHeliToFlyInDirection(CAutomobile* automobile) {
    plugin::Call<0x42A730, CAutomobile*>(automobile);
}

// 0x429780
void CCarCtrl::GetAIPlaneToAttackPlayer(CAutomobile* automobile) {
    plugin::Call<0x429780, CAutomobile*>(automobile);
}

// 0x429890
void CCarCtrl::GetAIPlaneToDoDogFight(CAutomobile* automobile) {
    plugin::Call<0x429890, CAutomobile*>(automobile);
}

// 0x42F370
void CCarCtrl::GetAIPlaneToDoDogFightAgainstPlayer(CAutomobile* automobile) {
    plugin::Call<0x42F370, CAutomobile*>(automobile);
}

// Allocates from the vehicle pool, and only calls the constructor if that succeeded (as the original code does)
template<typename T, typename... Args>
static T* CreateVehicle(int32 modelId, uint8 createdBy, Args... args) {
    void* mem = CVehicle::operator new(sizeof(T));
    if (!mem) {
        return nullptr;
    }
    return ::new (mem) T(modelId, static_cast<eVehicleCreatedBy>(createdBy), args...);
}

// 0x421440
CVehicle* CCarCtrl::GetNewVehicleDependingOnCarModel(int32 modelId, uint8 createdBy) {
    switch (CModelInfo::GetModelInfo(modelId)->AsVehicleModelInfoPtr()->m_nVehicleType) {
    case VEHICLE_TYPE_MTRUCK:  return CreateVehicle<CMonsterTruck>(modelId, createdBy);
    case VEHICLE_TYPE_QUAD:    return CreateVehicle<CQuadBike>(modelId, createdBy);
    case VEHICLE_TYPE_HELI:    return CreateVehicle<CHeli>(modelId, createdBy);
    case VEHICLE_TYPE_PLANE:   return CreateVehicle<CPlane>(modelId, createdBy);
    case VEHICLE_TYPE_BOAT:    return CreateVehicle<CBoat>(modelId, createdBy);
    case VEHICLE_TYPE_TRAIN:   return CreateVehicle<CTrain>(modelId, createdBy);
    case VEHICLE_TYPE_BIKE: {
        auto* const bike = CreateVehicle<CBike>(modelId, createdBy);
        if constexpr (notsa::IsFixBugs()) {
            if (!bike) { // BUG: Original code writes the bike flags through a null pointer if the pool is full (crash)
                return nullptr;
            }
        }
        bike->bikeFlags.bOnSideStand = true;
        return bike;
    }
    case VEHICLE_TYPE_BMX: {
        auto* const bmx = CreateVehicle<CBmx>(modelId, createdBy);
        if constexpr (notsa::IsFixBugs()) {
            if (!bmx) { // BUG: Same as above
                return nullptr;
            }
        }
        bmx->bikeFlags.bOnSideStand = true;
        return bmx;
    }
    case VEHICLE_TYPE_TRAILER: return CreateVehicle<CTrailer>(modelId, createdBy);
    default:                   return CreateVehicle<CAutomobile>(modelId, createdBy, true);
    }
}

// 0x42C250
bool CCarCtrl::IsAnyoneParking() {
    for (auto& veh : GetVehiclePool()->GetAllValid()) {
        switch (veh.m_autoPilot.m_nCarMission) {
        case eCarMission::MISSION_PARK_PARALLEL:
        case eCarMission::MISSION_PARK_PARALLEL_2:
        case eCarMission::MISSION_PARK_PERPENDICULAR:
        case eCarMission::MISSION_PARK_PERPENDICULAR_2:
            return true;
        }
    }
    return false;
}

// 0x42DAB0
bool CCarCtrl::IsThisAnAppropriateNode(CVehicle* vehicle, CNodeAddress nodeAddress1, CNodeAddress nodeAddress2, CNodeAddress nodeAddress3, bool arg5) {
    return plugin::CallAndReturn<bool, 0x42DAB0, CVehicle*, CNodeAddress, CNodeAddress, CNodeAddress, bool>(vehicle, nodeAddress1, nodeAddress2, nodeAddress3, arg5);
}

// 0x423EA0
bool CCarCtrl::IsThisVehicleInteresting(CVehicle* vehicle) {
    for (auto& car : apCarsToKeep) {
        if (car == vehicle) {
            return true;
        }
    }
    return false;
}

// 0x432CB0
void CCarCtrl::JoinCarWithRoadAccordingToMission(CVehicle* vehicle) {
    switch (vehicle->m_autoPilot.m_nCarMission) {
    case MISSION_NONE:
    case MISSION_CRUISE:
    case MISSION_WAITFORDELETION:
    case MISSION_EMERGENCYVEHICLE_STOP:
    case MISSION_STOP_FOREVER:
    case MISSION_FOLLOW_RECORDED_PATH:
    case MISSION_PARK_PERPENDICULAR:
    case MISSION_PARK_PARALLEL:
    case MISSION_PARK_PERPENDICULAR_2:
    case MISSION_PARK_PARALLEL_2:
        return JoinCarWithRoadSystem(vehicle);
    case MISSION_RAMPLAYER_FARAWAY:
    case MISSION_RAMPLAYER_CLOSE:
    case MISSION_BLOCKPLAYER_FARAWAY:
    case MISSION_BLOCKPLAYER_CLOSE:
    case MISSION_BLOCKPLAYER_HANDBRAKESTOP:
    case MISSION_BOAT_ATTACKPLAYER:
    case MISSION_SLOWLY_DRIVE_TOWARDS_PLAYER_1:
    case MISSION_SLOWLY_DRIVE_TOWARDS_PLAYER_2:
    case MISSION_BLOCKPLAYER_FORWARDANDBACK:
    case MISSION_APPROACHPLAYER_FARAWAY:
    case MISSION_APPROACHPLAYER_CLOSE:
    case MISSION_BOAT_CIRCLEPLAYER: {
        JoinCarWithRoadSystemGotoCoors(vehicle, FindPlayerCoors(-1), true, vehicle->IsSubBoat());
        break;
    }
    case MISSION_GOTOCOORDINATES:
    case MISSION_GOTOCOORDINATES_STRAIGHTLINE:
    case MISSION_GOTOCOORDINATES_ACCURATE:
    case MISSION_GOTOCOORDINATES_STRAIGHTLINE_ACCURATE:
    case MISSION_GOTOCOORDINATES_ASTHECROWSWIMS:
    case MISSION_GOTOCOORDINATES_RACING: {
        JoinCarWithRoadSystemGotoCoors(vehicle, vehicle->m_autoPilot.m_vecDestinationCoors, true, vehicle->IsSubBoat());
        break;
    }
    case MISSION_RAMCAR_FARAWAY:
    case MISSION_RAMCAR_CLOSE:
    case MISSION_BLOCKCAR_FARAWAY:
    case MISSION_BLOCKCAR_CLOSE:
    case MISSION_BLOCKCAR_HANDBRAKESTOP:
    case MISSION_PROTECTION_REAR:
    case MISSION_PROTECTION_FRONT:
    case MISSION_ESCORT_LEFT:
    case MISSION_ESCORT_RIGHT:
    case MISSION_ESCORT_REAR:
    case MISSION_ESCORT_FRONT:
    case MISSION_FOLLOWCAR_FARAWAY:
    case MISSION_FOLLOWCAR_CLOSE:
    case MISSION_KILLPED_FARAWAY:
    case MISSION_KILLPED_CLOSE:
    case MISSION_DO_DRIVEBY_CLOSE:
    case MISSION_DO_DRIVEBY_FARAWAY:
    case MISSION_ESCORT_LEFT_FARAWAY:
    case MISSION_ESCORT_RIGHT_FARAWAY:
    case MISSION_ESCORT_REAR_FARAWAY:
    case MISSION_ESCORT_FRONT_FARAWAY: {
        JoinCarWithRoadSystemGotoCoors(vehicle, vehicle->m_autoPilot.m_TargetEntity->GetPosition(), true, vehicle->IsSubBoat());
        break;
    }
    }
}

// 0x42F5A0
void CCarCtrl::JoinCarWithRoadSystem(CVehicle* vehicle) {
    plugin::Call<0x42F5A0, CVehicle*>(vehicle);
}

// 0x42F870
bool CCarCtrl::JoinCarWithRoadSystemGotoCoors(CVehicle* vehicle, const CVector& posn, bool unused, bool bIsBoat) {
    return plugin::CallAndReturn<bool, 0x42F870, CVehicle*, const CVector&, bool, bool>(vehicle, posn, unused, bIsBoat);
}

// 0x432B10
bool CCarCtrl::PickNextNodeAccordingStrategy(CVehicle* vehicle) {
    return plugin::CallAndReturn<bool, 0x432B10, CVehicle*>(vehicle);
}

// 0x421740
void CCarCtrl::InitSequence(int32 numSequenceElements) {
    SequenceElements = numSequenceElements;
    SequenceRandomOffset = CGeneral::GetRandomNumber() % numSequenceElements;
    bSequenceOtherWay = (CGeneral::GetRandomNumber() / 4) % 2;
}

// 0x42DE80
void CCarCtrl::PickNextNodeRandomly(CVehicle* vehicle) {
    plugin::Call<0x42DE80, CVehicle*>(vehicle);
}

// 0x426EF0
bool CCarCtrl::PickNextNodeToChaseCar(CVehicle* vehicle, float destX, float destY, float destZ) {
    return plugin::CallAndReturn<bool, 0x426EF0, CVehicle*, float, float, float>(vehicle, destX, destY, destZ);
}

// 0x427740
bool CCarCtrl::PickNextNodeToFollowPath(CVehicle* vehicle) {
    return plugin::CallAndReturn<bool, 0x427740, CVehicle*>(vehicle);
}

// 0x429600
void CCarCtrl::PossiblyFireHSMissile(CVehicle* entityLauncher, CEntity* targetEntity) {
    if (!targetEntity)
        return;

    if (CTimer::GetTimeInMS() / 2000u == CTimer::GetPreviousTimeInMS() / 2000u)
        return;

    const CVector launcherPos = entityLauncher->GetPosition();
    const CVector targetPos = targetEntity->GetPosition();
    CVector dir = targetPos - launcherPos;
    const float dist = dir.Magnitude();
    if (dist < 160.0f && dist > 30.0f) {
        CMatrix launcherMat = entityLauncher->GetMatrix();
        CVector dirNormalized = dir;
        dir.Normalise();
        if (DotProduct(launcherMat.GetForward(), dirNormalized) > 0.8f) {
            CProjectileInfo::AddProjectile(
                entityLauncher,
                eWeaponType::WEAPON_ROCKET_HS,
                launcherPos + launcherMat.GetForward() * 4.0f - launcherMat.GetUp() * 3.0f,
                1.0f,
                &entityLauncher->GetMatrix().GetForward(),
                targetEntity
            );
        }
    }
}

// 0x424F80
void CCarCtrl::PossiblyRemoveVehicle(CVehicle* vehicle) {
    const auto Remove = [&] { // 0x4251E5 / 0x425220 / 0x42541D
        CWorld::Remove(vehicle);
        delete vehicle;
    };

    if (vehicle->m_nNumGettingIn) {
        return;
    }
    if (vehicle->vehicleFlags.bPartOfConvoy && vehicle->m_autoPilot.m_TargetEntity) {
        return;
    }

    // BUG: The original only initializes this in the first block below, but reads it in the later ones too
    CVector playerCentre;
    if (notsa::IsFixBugs()) {
        playerCentre = FindPlayerCentreOfWorld(CWorld::PlayerInFocus);
    }

    if (!vehicle->vehicleFlags.bIsLocked && vehicle->CanBeDeleted() && !CCranes::IsThisCarBeingTargettedByAnyCrane(vehicle)) {
        if (vehicle->vehicleFlags.bFadeOut && CVisibilityPlugins::GetClumpAlpha(vehicle->GetRpClump()) == 0) {
            Remove();
            return;
        }

        if (!notsa::IsFixBugs()) {
            playerCentre = FindPlayerCentreOfWorld(CWorld::PlayerInFocus);
        }

        // x87: the difference is kept in extended precision
        const auto dx = (double)vehicle->GetPosition().x - playerCentre.x;
        const auto dy = (double)vehicle->GetPosition().y - playerCentre.y;
        const auto distToPlayer2D = (float)std::sqrt(dx * dx + dy * dy);

        const auto& activeCam = TheCamera.m_aCams[TheCamera.m_nActiveCam];
        float removalRange;
        if (!vehicle->GetIsOnScreen()
            && !activeCam.m_bLookingLeft
            && !activeCam.m_bLookingRight
            && !activeCam.m_bLookingBehind
            && TheCamera.GetLookDirection()
            && vehicle->m_nCreatedBy != PARKED_VEHICLE
            && vehicle->m_nModelIndex != MODEL_AMBULAN
            && vehicle->m_nModelIndex != MODEL_FIRETRUK
            && !vehicle->vehicleFlags.bIsLawEnforcer
            && !vehicle->vehicleFlags.bIsCarParkVehicle
            && vehicle->m_nTimeTillWeNeedThisCar <= CTimer::GetTimeInMS()
            && !vehicle->vehicleFlags.bNeverUseSmallerRemovalRange
        ) {
            removalRange = 45.0f;
        } else {
            removalRange = TheCamera.m_fGenerationDistMultiplier * 170.0f;
        }
        if (TheCamera.m_mCameraMatrix.GetForward().z < -0.9f) { // Looking (almost) straight down
            removalRange = 75.0f;
        }

        // 0x420800 - `a > b ? a : b`
        const auto extRange = (float)vehicle->m_nExtendedRemovalRange;
        const auto maxRange = extRange > 170.0f ? extRange : 170.0f;

        // x87: kept in extended precision until the comparison
        if ((double)maxRange * removalRange * (double)(1.0f / 170.0f) < (double)distToPlayer2D && vehicle->m_autoPilot.m_nCarMission != MISSION_PLANE_ATTACK_PLAYER_POLICE) {
            if (!CGarages::IsPointWithinHideOutGarage(vehicle->GetPosition())) {
                if (IsThisVehicleInteresting(vehicle)) {
                    vehicle->m_nFakePhysics = 10;
                    return;
                }
                if (vehicle->GetIsOnScreen()) {
                    vehicle->vehicleFlags.bFadeOut = true;
                    return;
                }
                Remove();
                return;
            }
        }

        if (vehicle->GetStatus() == STATUS_SIMPLE) {
            // Note: Uses the matrix directly (not null checked)
            const auto upZ = vehicle->m_matrix->GetUp().z;
            if ((upZ < 0.0f ? -upZ : upZ) < 0.74f) { // Vehicle is on its side (or upside down)
                Remove();
                return;
            }
        }

        if (   (vehicle->GetStatus() == STATUS_PHYSICS || vehicle->GetStatus() == STATUS_WRECKED)
            && (vehicle->m_nVehicleSubType == VEHICLE_TYPE_PLANE || vehicle->m_nVehicleSubType == VEHICLE_TYPE_HELI)
            && vehicle->m_bIsStuck
        ) {
            Remove();
            return;
        }
    }

    if (   vehicle->m_nVehicleSubType != VEHICLE_TYPE_HELI
        && vehicle->m_nVehicleSubType != VEHICLE_TYPE_PLANE
        && vehicle->m_autoPilot.m_nTempAction != TEMPACT_STUCKINTRAFFIC
        && (   vehicle->GetStatus() == STATUS_SIMPLE
            || (vehicle->GetStatus() == STATUS_PHYSICS && (vehicle->m_autoPilot.m_nCarDrivingStyle == DRIVING_STYLE_STOP_FOR_CARS || vehicle->m_autoPilot.m_nCarDrivingStyle == DRIVING_STYLE_STOP_FOR_CARS_IGNORE_LIGHTS))
        )
        && CTimer::GetTimeInMS() - vehicle->m_autoPilot.m_nTimeSwitchedToRealPhysics > 5000
        && vehicle->m_nTimeTillWeNeedThisCar == 0
        && CTimer::GetTimeInMS() != 0
        && !vehicle->GetIsOnScreen()
    ) {
        const CVector offset = vehicle->GetPosition() - playerCentre; // BUG: `playerCentre` is uninitialized if the first block wasn't entered (locked/can't be deleted/targeted by a crane), see above
        // x87: kept in extended precision
        if (std::sqrt((double)offset.y * offset.y + (double)offset.x * offset.x) > 22.0f
            && !IsThisVehicleInteresting(vehicle)
            && !vehicle->vehicleFlags.bIsLocked
            && vehicle->CanBeDeleted()
            && !CTrafficLights::ShouldCarStopForLight(vehicle, true)
            && !CTrafficLights::ShouldCarStopForBridge(vehicle)
            && !CGarages::IsPointWithinHideOutGarage(vehicle->GetPosition())
        ) {
            Remove();
            return;
        }
    }

    // Wrecks
    if (vehicle->GetStatus() != STATUS_WRECKED
        || !vehicle->m_nTimeWhenBlowedUp
        || !(CTimer::GetTimeInMS() > vehicle->m_nTimeWhenBlowedUp + 60'000)
        || !(CTimer::GetTimeInMS() > vehicle->m_nTimeTillWeNeedThisCar)
        || vehicle->GetIsOnScreen()
    ) {
        return;
    }
    const CVector offset = vehicle->GetPosition() - playerCentre; // BUG: Same as above
    if (!((double)offset.x * offset.x + (double)offset.y * offset.y + (double)offset.z * offset.z > 42.25f)) { // x87: kept in extended precision
        return;
    }
    if (CGarages::IsPointWithinHideOutGarage(vehicle->GetPosition())) {
        return;
    }
    Remove();
}

// 0x423F10
void CCarCtrl::PruneVehiclesOfInterest() {
    ZoneScoped;

    if ((CTimer::GetFrameCounter() % 64) == 19 && FindPlayerCoors(-1).z < 950.0f) {
        for (size_t i = 0; i < std::size(apCarsToKeep); i++) {
            if (apCarsToKeep[i]) {
                if (CTimer::GetTimeInMS() > aCarsToKeepTime[i] + 180000) {
                    apCarsToKeep[i] = nullptr;
                }
            }
        }
    }
}

// 0x42FC40
void CCarCtrl::ReconsiderRoute(CVehicle* vehicle) {
    plugin::Call<0x42FC40, CVehicle*>(vehicle);
}

// 0x423DE0
void CCarCtrl::RegisterVehicleOfInterest(CVehicle* vehicle) {
    // Already registered => just refresh the time
    for (auto i = 0; i < (int32)std::size(apCarsToKeep); i++) {
        if (apCarsToKeep[i] == vehicle) {
            aCarsToKeepTime[i] = CTimer::GetTimeInMS();
            return;
        }
    }

    // Use a free slot
    for (auto i = 0; i < (int32)std::size(apCarsToKeep); i++) {
        if (!apCarsToKeep[i]) {
            apCarsToKeep[i]    = vehicle;
            aCarsToKeepTime[i] = CTimer::GetTimeInMS();
            return;
        }
    }

    // Replace the oldest one
    auto oldestTime = UINT32_MAX;
    auto oldestIdx  = 0;
    for (auto i = 0; i < (int32)std::size(apCarsToKeep); i++) {
        if (apCarsToKeep[i] && aCarsToKeepTime[i] < oldestTime) {
            oldestTime = aCarsToKeepTime[i];
            oldestIdx  = i;
        }
    }
    apCarsToKeep[oldestIdx]    = vehicle;
    aCarsToKeepTime[oldestIdx] = CTimer::GetTimeInMS();
}

// 0x4322B0
void CCarCtrl::RemoveCarsIfThePoolGetsFull() {
    ZoneScoped;

    if (CTimer::GetFrameCounter() % 8 != 3)
        return;

    if (GetVehiclePool()->GetNoOfFreeSpaces() >= 8)
        return;

    // Find closest deletable vehicle
    const CVector camPos = TheCamera.GetPosition();
    float fClosestDist = std::numeric_limits<float>::max();
    CVehicle* closestVeh = nullptr;
    for (auto& veh : GetVehiclePool()->GetAllValid()) {
        if (IsThisVehicleInteresting(&veh)) {
            continue;
        }
        if (veh.vehicleFlags.bIsLocked) {
            continue;
        }
        if (!veh.CanBeDeleted()) {
            continue;
        }
        if (CCranes::IsThisCarBeingTargettedByAnyCrane(&veh)) {
            continue;
        }

        const float fCamVehDist = (camPos - veh.GetPosition()).Magnitude();
        if (fClosestDist > fCamVehDist) {
            fClosestDist = fCamVehDist;
            closestVeh   = &veh;
        }
    }
    if (closestVeh) {
        CWorld::Remove(closestVeh);
        delete closestVeh;
    }
}

// 0x42CD10
void CCarCtrl::RemoveDistantCars() {
    ZoneScoped;

    // FIXBUGS: First remove vehicles that can be removed
    if (notsa::bugfixes::CCarCtrl_RemoveDistantCars_UseAfterFree) {
        for (auto& veh : GetVehiclePool()->GetAllValid()) {
            PossiblyRemoveVehicle(&veh);
        }
    }

    //... only then process them, this way we don't do use-after-free
    // only other solution would be `PossiblyRemoveVehicle` returning a `bool`
    // to indicate whenever the vehicle was deleted or not.
    for (auto& veh : GetVehiclePool()->GetAllValid()) {
        if (!notsa::bugfixes::CCarCtrl_RemoveDistantCars_UseAfterFree) {
            PossiblyRemoveVehicle(&veh); // This may or may not invalidate `veh`
        }
        if (!veh.vehicleFlags.bCreateRoadBlockPeds) {
            continue;
        }
        if (DistanceBetweenPoints(FindPlayerCentreOfWorld(), veh.GetPosition()) >= 54.5f) {
            continue;
        }
        CRoadBlocks::GenerateRoadBlockPedsForCar(
            &veh,
            veh.m_nPedsPositionForRoadBlock,
            veh.IsLawEnforcementVehicle() ? PED_TYPE_COP : PED_TYPE_GANG1
        );
        veh.vehicleFlags.bCreateRoadBlockPeds = false;
    }
}

// 0x423ED0
void CCarCtrl::RemoveFromInterestingVehicleList(CVehicle* vehicle) {
    for (auto& car : apCarsToKeep) {
        if (car == vehicle) {
            car = nullptr;
            break;
        }
    }
}

// 0x42CE40
void CCarCtrl::ScanForPedDanger(CVehicle* vehicle) {
    plugin::Call<0x42CE40, CVehicle*>(vehicle);
}

// 0x42FBC0
bool CCarCtrl::ScriptGenerateOneEmergencyServicesCar(uint32 modelId, CVector posn) {
    if (CStreaming::IsModelLoaded(modelId)) {
        if (auto pAuto = GenerateOneEmergencyServicesCar(modelId, posn)) {
            pAuto->m_autoPilot.m_vecDestinationCoors = posn;
            pAuto->m_autoPilot.SetCarMission(JoinCarWithRoadSystemGotoCoors(pAuto, posn, false, false) ? MISSION_GOTOCOORDINATES_STRAIGHTLINE : MISSION_GOTOCOORDINATES);
            return true;
        }
    }
    return false;
}

// 0x4342A0
void CCarCtrl::SetCoordsOfScriptCar(CVehicle* vehicle, float x, float y, float z, uint8 arg5, uint8 arg6) {
    plugin::Call<0x4342A0, CVehicle*, float, float, float, uint8, uint8>(vehicle, x, y, z, arg5, arg6);
}

// 0x4217C0
void CCarCtrl::SetUpDriverAndPassengersForVehicle(CVehicle* vehicle, int32 pedType, int32 minPassengers, bool arg4, bool arg5, int32 maxPassengers) {
    const auto IsGangLikePedType = [&] { // 14..23
        return pedType >= (int32)PED_TYPE_GANG8 && pedType <= (int32)PED_TYPE_SPECIAL;
    };

    vehicle->SetUpDriver(pedType, arg4, arg5);
    if (IsGangLikePedType()) {
        if (rand() < 0x3FFF) {
            vehicle->m_pDriver->GiveObjectToPedToHold(ModelIndices::MI_GANG_SMOKE, 1); // BUG: m_pDriver isn't checked for null (SetUpDriver can fail)
        }
    }

    maxPassengers = std::min<int32>(maxPassengers, vehicle->m_nMaxPassengers);

    auto numPassengers = minPassengers;
    if (numPassengers < maxPassengers) {
        for (auto i = maxPassengers - minPassengers; i > 0; i--) {
            // x87: the product is kept in extended precision (doubles are exact here)
            if ((double)rand() * (double)RAND_MAX_FLOAT_RECIPROCAL < 0.125) {
                numPassengers++;
            }
        }
    }
    if (numPassengers >= maxPassengers) {
        numPassengers = maxPassengers;
    }

    if (CModelInfo::IsCarModel(vehicle->m_nModelIndex)) {
        // Vans can only have a single passenger
        const auto vanAnimBlock = CAnimManager::GetAnimationBlockIndex("van");
        if (CModelInfo::GetModelInfo(vehicle->m_nModelIndex)->GetAnimFileIndex() == vanAnimBlock && numPassengers > 0) {
            numPassengers = 1;
        }
    }

    for (auto seat = 0; seat < numPassengers; seat++) {
        if (const auto passenger = vehicle->SetupPassenger(seat, pedType, arg4, arg5)) {
            passenger->UpdateStatEnteringVehicle();
            if (IsGangLikePedType()) {
                if (rand() < 0x3FFF) {
                    passenger->GiveObjectToPedToHold(ModelIndices::MI_GANG_SMOKE, 1);
                }
            }
        }
    }
}

// 0x432420
template<typename PtrListType>
void CCarCtrl::SlowCarDownForCarsSectorList(PtrListType& ptrList, CVehicle* vehicle, float arg3, float arg4, float arg5, float arg6, float* arg7, float arg8) {
    plugin::Call<0x432420, PtrListType&, CVehicle*, float, float, float, float, float*, float>(ptrList, vehicle, arg3, arg4, arg5, arg6, arg7, arg8);
}

// 0x426220
void CCarCtrl::SlowCarDownForObject(CEntity* entity, CVehicle* vehicle, float* arg3, float arg4) {
    const CVector entityDir = entity->GetPosition() - vehicle->GetPosition();
    const float entityHeading = DotProduct(entityDir, vehicle->GetMatrix().GetForward());
    if (entityHeading > 0.0f && entityHeading < 20.0f) {
        if (entity->GetColModel()->GetBoundRadius() + vehicle->GetColModel()->GetBoundingBox().m_vecMax.x > fabs(DotProduct(entityDir, vehicle->GetMatrix().GetRight()))) {
            if (entityHeading >= 7.0f) {
                *arg3 = std::min(*arg3, (1.0f - (entityHeading - 7.0f) / 13.0f)) * arg4; // Original code multiplies by 0.07692308, which is the recp. of 13
            } else {
                *arg3 = 0.0f;
            }
        }
    }
}

// 0x42D4F0
template<typename PtrListType>
void CCarCtrl::SlowCarDownForObjectsSectorList(PtrListType& ptrList, CVehicle* vehicle, float arg3, float arg4, float arg5, float arg6, float* arg7, float arg8) {
    plugin::Call<0x42D4F0, PtrListType&, CVehicle*, float, float, float, float, float*, float>(ptrList, vehicle, arg3, arg4, arg5, arg6, arg7, arg8);
}

// 0x42D0E0
void CCarCtrl::SlowCarDownForOtherCar(CEntity* car1, CVehicle* car2, float* arg3, float arg4) {
    plugin::Call<0x42D0E0, CEntity*, CVehicle*, float*, float>(car1, car2, arg3, arg4);
}

// 0x425440
template<typename PtrListType>
void CCarCtrl::SlowCarDownForPedsSectorList(PtrListType& PtrList, CVehicle* vehicle, float arg3, float arg4, float arg5, float arg6, float* arg7, float arg8) {
    plugin::Call<0x425440, PtrListType&, CVehicle*, float, float, float, float, float*, float>(PtrList, vehicle, arg3, arg4, arg5, arg6, arg7, arg8);
}

// 0x434790
void CCarCtrl::SlowCarOnRailsDownForTrafficAndLights(CVehicle* vehicle) {
    auto& autoPilot = vehicle->m_autoPilot;

    if ((((int8)CTimer::GetFrameCounter() + (int8)(vehicle->m_nRandomSeed)) & 3) == 0) {
        if (CTrafficLights::ShouldCarStopForLight(vehicle, false) || CTrafficLights::ShouldCarStopForBridge(vehicle)) {
            CCarAI::CarHasReasonToStop(vehicle);
            autoPilot.m_fMaxTrafficSpeed = 0.0f;
        } else {
            autoPilot.m_fMaxTrafficSpeed = FindMaximumSpeedForThisCarInTraffic(vehicle);
        }
    }

    if (autoPilot.m_fMaxTrafficSpeed >= autoPilot.m_speed) {
        autoPilot.ModifySpeed(std::min(autoPilot.m_fMaxTrafficSpeed, CTimer::GetTimeStep() * 0.05f + autoPilot.m_speed));
    } else if (autoPilot.m_speed >= 0.1f) {
        autoPilot.ModifySpeed(std::max(autoPilot.m_fMaxTrafficSpeed, autoPilot.m_speed - CTimer::GetTimeStep() * 0.7f));
    } else if (autoPilot.m_speed != 0.0f) {
        autoPilot.ModifySpeed(0.0f);
    }
}

// 0x428DE0
void CCarCtrl::SteerAIBoatWithPhysicsAttackingPlayer(CVehicle* vehicle, float* arg2, float* arg3, float* arg4, bool* arg5) {
    plugin::Call<0x428DE0, CVehicle*, float*, float*, float*, bool*>(vehicle, arg2, arg3, arg4, arg5);
}

// 0x429090
void CCarCtrl::SteerAIBoatWithPhysicsCirclingPlayer(CVehicle* vehicle, float* arg2, float* arg3, float* arg4, bool* arg5) {
    plugin::Call<0x429090, CVehicle*, float*, float*, float*, bool*>(vehicle, arg2, arg3, arg4, arg5);
}

// 0x428BE0
void CCarCtrl::SteerAIBoatWithPhysicsHeadingForTarget(CVehicle* vehicle, float arg2, float arg3, float* arg4, float* arg5, float* arg6) {
    plugin::Call<0x428BE0, CVehicle*, float, float, float*, float*, float*>(vehicle, arg2, arg3, arg4, arg5, arg6);
}

// 0x422B20
void CCarCtrl::SteerAICarBlockingPlayerForwardAndBack(CVehicle* vehicle, float* pSteer, float* pGas, float* pBrake, bool* pHandbrake) {
    *pSteer     = 0.0f;
    *pHandbrake = false;

    // Where the player will be in a moment (position delta = speed + 0.1 * forward)
    const auto plSpeed = FindPlayerSpeed();
    const auto player  = FindPlayerEntity();
    const auto& plFwd  = player->GetMatrix().GetForward();

    const auto speedX = (float)((double)plFwd.x * 0.1f + plSpeed.x);
    const auto speedY = (float)((double)plSpeed.y + (double)plFwd.y * 0.1f);

    // Note: Uses the matrix directly (not null checked)
    auto& vehMat = *vehicle->m_matrix;

    CVector right{ vehMat.GetRight().x, vehMat.GetRight().y, 0.0f };
    right.Normalise();

    CVector fwd{ vehMat.GetForward().x, vehMat.GetForward().y, 0.0f };
    fwd.Normalise();

    // Offset from this vehicle to the player
    const auto GetOffsetToPlayer = [&] {
        const auto plPos  = FindPlayerCoors();
        const auto vehPos = vehicle->GetPosition();
        return std::array<double, 3>{ (double)plPos.x - vehPos.x, (double)plPos.y - vehPos.y, (double)plPos.z - vehPos.z };
    };

    // x87: all intermediates are kept in extended precision
    const auto [dx, dy, dz] = GetOffsetToPlayer();
    const auto rightDotOffset = ((double)right.y * dy + dx * right.x) + dz * right.z;
    auto       rightDotSpeed  = ((double)right.y * speedY + (double)right.x * speedX) + (double)right.z * 0.0f;
    if (rightDotSpeed == 0.0) {
        rightDotSpeed = 0.01f;
    }
    const auto t  = -(rightDotOffset / rightDotSpeed); // Time until the player is level with the vehicle
    const auto tf = (float)t;
    if (t < 0.0) {
        *pGas   = 0.0f;
        *pBrake = 0.0f;
        return;
    }

    const auto [dx2, dy2, dz2] = GetOffsetToPlayer();
    const auto& moveSpeed = vehicle->m_vecMoveSpeed;
    const auto  vehSpeedAlongFwd = (float)(((double)fwd.y * moveSpeed.y + (double)fwd.z * moveSpeed.z) + (double)fwd.x * moveSpeed.x);

    const auto fwdDotSpeed  = ((double)fwd.y * speedY + (double)fwd.x * speedX) + (double)fwd.z * 0.0f;
    const auto fwdDotOffset = ((double)fwd.y * dy2 + dx2 * fwd.x) + dz2 * fwd.z;
    const auto dist         = (fwdDotSpeed * tf + fwdDotOffset) - (double)vehSpeedAlongFwd * tf; // Distance to the player (along forward) when he's level with the vehicle

    if (dist > 0.0) {
        *pGas   = (float)std::min<double>(dist * 0.1f, 1.0f);
        *pBrake = 0.0f;
    } else if (vehSpeedAlongFwd > 0.0f) {
        *pGas = 0.0f;
        const auto brake = std::min<double>(dist * -0.1f, 1.0f);
        *pBrake = (float)brake;
        if (brake > 0.95f) {
            *pHandbrake = true;
        }
    } else {
        *pGas   = (float)std::max<double>(dist * 0.1f, -1.0f);
        *pBrake = 0.0f;
    }
}

// 0x433BA0
void CCarCtrl::SteerAICarParkParallel(CVehicle* vehicle, float* arg2, float* arg3, float* arg4, bool* arg5) {
    plugin::Call<0x433BA0, CVehicle*, float*, float*, float*, bool*>(vehicle, arg2, arg3, arg4, arg5);
}

// 0x433EA0
void CCarCtrl::SteerAICarParkPerpendicular(CVehicle* vehicle, float* arg2, float* arg3, float* arg4, bool* arg5) {
    plugin::Call<0x433EA0, CVehicle*, float*, float*, float*, bool*>(vehicle, arg2, arg3, arg4, arg5);
}

// 0x4336D0
void CCarCtrl::SteerAICarTowardsPointInEscort(CVehicle* vehicle1, CVehicle* vehicle2, float arg3, float arg4, float* arg5, float* arg6, float* arg7, bool* arg8) {
    plugin::Call<0x4336D0, CVehicle*, CVehicle*, float, float, float*, float*, float*, bool*>(vehicle1, vehicle2, arg3, arg4, arg5, arg6, arg7, arg8);
}

// 0x437C20
void CCarCtrl::SteerAICarWithPhysics(CVehicle* vehicle) {
    plugin::Call<0x437C20, CVehicle*>(vehicle);
}

// 0x434900
void CCarCtrl::SteerAICarWithPhysicsFollowPath(CVehicle* vehicle, float* arg2, float* arg3, float* arg4, bool* arg5) {
    plugin::Call<0x434900, CVehicle*, float*, float*, float*, bool*>(vehicle, arg2, arg3, arg4, arg5);
}

// 0x435830
void CCarCtrl::SteerAICarWithPhysicsFollowPath_Racing(CVehicle* vehicle, float* arg2, float* arg3, float* arg4, bool* arg5) {
    plugin::Call<0x435830, CVehicle*, float*, float*, float*, bool*>(vehicle, arg2, arg3, arg4, arg5);
}

// 0x432DD0
void CCarCtrl::SteerAICarWithPhysicsFollowPreRecordedPath(CVehicle* vehicle, float* arg2, float* arg3, float* arg4, bool* arg5) {
    plugin::Call<0x432DD0, CVehicle*, float*, float*, float*, bool*>(vehicle, arg2, arg3, arg4, arg5);
}

// 0x433280
void CCarCtrl::SteerAICarWithPhysicsHeadingForTarget(CVehicle* vehicle, CPhysical* target, float arg3, float arg4, float* arg5, float* arg6, float* arg7, bool* arg8) {
    plugin::Call<0x433280, CVehicle*, CPhysical*, float, float, float*, float*, float*, bool*>(vehicle, target, arg3, arg4, arg5, arg6, arg7, arg8);
}

// 0x4335E0
void CCarCtrl::SteerAICarWithPhysicsTryingToBlockTarget(CVehicle* vehicle, CEntity* Unusued, float arg3, float arg4, float arg5, float arg6, float* arg7, float* arg8, float* arg9, bool* arg10) {
    plugin::Call<0x4335E0, CVehicle*, CEntity*, float, float, float, float, float*, float*, float*, bool*>(vehicle, Unusued, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10);
}

// 0x428990
void CCarCtrl::SteerAICarWithPhysicsTryingToBlockTarget_Stop(CVehicle* vehicle, float x, float y, float arg4, float arg5, float* arg6, float* arg7, float* arg8, bool* arg9) {
    plugin::Call<0x428990, CVehicle*, float, float, float, float, float*, float*, float*, bool*>(vehicle, x, y, arg4, arg5, arg6, arg7, arg8, arg9);
}

// 0x436A90
void CCarCtrl::SteerAICarWithPhysics_OnlyMission(CVehicle* vehicle, float* arg2, float* arg3, float* arg4, bool* arg5) {
    plugin::Call<0x436A90, CVehicle*, float*, float*, float*, bool*>(vehicle, arg2, arg3, arg4, arg5);
}

// 0x42AAD0
void CCarCtrl::SteerAIHeliAsPoliceHeli(CAutomobile* automobile) {
    plugin::Call<0x42AAD0, CAutomobile*>(automobile);
}

// 0x42ACB0
void CCarCtrl::SteerAIHeliFlyingAwayFromPlayer(CAutomobile* automobile) {
    plugin::Call<0x42ACB0, CAutomobile*>(automobile);
}

// 0x4238E0
void CCarCtrl::SteerAIHeliToCrashAndBurn(CAutomobile* automobile) {
    const auto heli = static_cast<CHeli*>(automobile);
    const auto left = (heli->m_nRandomSeed & 1) != 0;
    heli->m_fSteeringUpDown = -0.3f;
    heli->m_fLeftRightSkid  = left ? heli->field_A14 : -heli->field_A14;
    heli->m_fAccelerationBreakStatus = -0.5f;
    heli->m_fSteeringLeftRight       = left ? 1.0f : -1.0f;
}

// 0x42A750
void CCarCtrl::SteerAIHeliToFollowEntity(CAutomobile* automobile) {
    plugin::Call<0x42A750, CAutomobile*>(automobile);
}

// 0x42AEB0
void CCarCtrl::SteerAIHeliToKeepEntityInView(CAutomobile* automobile) {
    plugin::Call<0x42AEB0, CAutomobile*>(automobile);
}

// 0x42AD30
void CCarCtrl::SteerAIHeliToLand(CAutomobile* automobile) {
    plugin::Call<0x42AD30, CAutomobile*>(automobile);
}

// 0x42A630
void CCarCtrl::SteerAIHeliTowardsTargetCoors(CAutomobile* automobile) {
    plugin::Call<0x42A630, CAutomobile*>(automobile);
}

// 0x423880
void CCarCtrl::SteerAIPlaneToCrashAndBurn(CAutomobile* automobile) {
    const auto plane = static_cast<CPlane*>(automobile);
    const auto dir   = (plane->m_nRandomSeed & 1) ? 1.0f : -1.0f;
    plane->m_fSteeringUpDown            = -0.3f;
    plane->m_fLeftRightSkid             = dir;
    plane->m_fAccelerationBreakStatus   = 0.0f;
    plane->m_fSteeringLeftRight         = dir;
}

// 0x4237F0
void CCarCtrl::SteerAIPlaneToFollowEntity(CAutomobile* automobile) {
    const auto plane  = static_cast<CPlane*>(automobile);
    const auto target = plane->m_autoPilot.m_TargetEntity;
    plane->m_planeHeading = CGeneral::GetATanOfXY(
        target->GetPosition().x - plane->GetPosition().x,
        target->GetPosition().y - plane->GetPosition().y
    );
    plane->m_maxAltitude = plane->m_autoPilot.m_TargetEntity->GetPosition().z;
    FlyAIPlaneInCertainDirection(plane);
}

// 0x423790
void CCarCtrl::SteerAIPlaneTowardsTargetCoors(CAutomobile* automobile) {
    const auto plane = static_cast<CPlane*>(automobile);
    const auto& dest = plane->m_autoPilot.m_vecDestinationCoors;
    const auto& pos  = plane->GetPosition();
    plane->m_planeHeading = CGeneral::GetATanOfXY(dest.x - pos.x, dest.y - pos.y);
    FlyAIPlaneInCertainDirection(plane);
}

// 0x422590
bool CCarCtrl::StopCarIfNodesAreInvalid(CVehicle* vehicle) {
    return plugin::CallAndReturn<bool, 0x422590, CVehicle*>(vehicle);
}

// 0x4222A0
void CCarCtrl::SwitchBetweenPhysicsAndGhost(CVehicle* vehicle) {
    if (!vehicle->physicalFlags.bDontLoadCollision) {
        return;
    }
    if (!vehicle->IsMissionVehicle()) {
        return;
    }
    switch (vehicle->m_nVehicleSubType) {
    case VEHICLE_TYPE_BOAT:
    case VEHICLE_TYPE_PLANE:
    case VEHICLE_TYPE_HELI:
        return;
    }

    switch (vehicle->GetStatus()) {
    case STATUS_PHYSICS: {
        if (CColStore::HasCollisionLoaded(vehicle->GetPosition(), AREA_CODE_NORMAL_WORLD)) {
            return;
        }
        vehicle->SetStatus(STATUS_GHOST);
        if (vehicle->m_nVehicleSubType == VEHICLE_TYPE_AUTOMOBILE) {
            for (auto wheel = 0; wheel < 4; wheel++) {
                vehicle->AsAutomobile()->m_damageManager.SetWheelStatus((eCarWheel)wheel, WHEEL_STATUS_OK);
            }
        }
        break;
    }
    case STATUS_GHOST: {
        if (!CColStore::HasCollisionLoaded(vehicle->GetPosition(), AREA_CODE_NORMAL_WORLD)) {
            return;
        }
        vehicle->SetStatus(STATUS_PHYSICS);
        switch (vehicle->m_nVehicleType) {
        case VEHICLE_TYPE_AUTOMOBILE:
            vehicle->AsAutomobile()->PlaceOnRoadProperly();
            break;
        case VEHICLE_TYPE_BIKE:
            vehicle->AsBike()->PlaceOnRoadProperly();
            break;
        }
        break;
    }
    }
}

// 0x423FC0
void CCarCtrl::SwitchVehicleToRealPhysics(CVehicle* vehicle) {
    vehicle->SetStatus(STATUS_PHYSICS);
    vehicle->m_autoPilot.m_nTempAction                 = TEMPACT_NONE;
    vehicle->m_autoPilot.m_nTimeToStartMission         = CTimer::GetTimeInMS() + 2000;
    vehicle->m_autoPilot.m_nTimeSwitchedToRealPhysics  = CTimer::GetTimeInMS();
    vehicle->m_nFakePhysics                            = 0;
}

// 0x425B30
float CCarCtrl::TestCollisionBetween2MovingRects(CVehicle* vehicle1, CVehicle* vehicle2, float arg3, float arg4, CVector* pos1, CVector* pos2) {
    return plugin::CallAndReturn<float, 0x425B30, CVehicle*, CVehicle*, float, float, CVector*, CVector*>(vehicle1, vehicle2, arg3, arg4, pos1, pos2);
}

// 0x425F70
float CCarCtrl::TestCollisionBetween2MovingRects_OnlyFrontBumper(CVehicle* vehicle1, CVehicle* vehicle2, float arg3, float arg4, CVector* pos1, CVector* pos2) {
    return plugin::CallAndReturn<float, 0x425F70, CVehicle*, CVehicle*, float, float, CVector*, CVector*>(vehicle1, vehicle2, arg3, arg4, pos1, pos2);
}

// 0x429520
void CCarCtrl::TestWhetherToFirePlaneGuns(CVehicle* vehicle, CEntity* target) {
    plugin::Call<0x429520, CVehicle*, CEntity*>(vehicle, target);
}

// 0x421FE0
bool CCarCtrl::ThisVehicleShouldTryNotToTurn(CVehicle* vehicle) {
    return plugin::CallAndReturn<bool, 0x421FE0, CVehicle*>(vehicle);
}

// 0x429300
void CCarCtrl::TriggerDogFightMoves(CVehicle* vehicle1, CVehicle* vehicle2) {
    plugin::Call<0x429300, CVehicle*, CVehicle*>(vehicle1, vehicle2);
}

// 0x424000
void CCarCtrl::UpdateCarCount(CVehicle* vehicle, uint8 bDecrease) {
    // The counters are compared as signed integers, even the ones declared as unsigned
    const auto Decrement = [](auto& counter) {
        counter--;
        if ((int32)counter < 0) {
            counter = 0;
        }
    };

    if (!bDecrease) {
        switch (vehicle->m_nCreatedBy) {
        case RANDOM_VEHICLE:
            if (vehicle->vehicleFlags.bIsLawEnforcer) {
                NumLawEnforcerCars++;
            }
            NumRandomCars++;
            break;
        case MISSION_VEHICLE:
            if (vehicle->vehicleFlags.bIsLawEnforcer) {
                vehicle->vehicleFlags.bIsLawEnforcer = false;
                NumLawEnforcerCars--;
            }
            NumMissionCars++;
            break;
        case PARKED_VEHICLE:
            NumParkedCars++;
            break;
        case PERMANENT_VEHICLE:
            NumPermanentVehicles++;
            break;
        }
    } else {
        switch (vehicle->m_nCreatedBy) {
        case RANDOM_VEHICLE:
            if (vehicle->vehicleFlags.bIsLawEnforcer) {
                Decrement(NumLawEnforcerCars);
            }
            Decrement(NumRandomCars);
            break;
        case MISSION_VEHICLE:
            Decrement(NumMissionCars);
            break;
        case PARKED_VEHICLE:
            Decrement(NumParkedCars);
            break;
        case PERMANENT_VEHICLE:
            Decrement(NumPermanentVehicles);
            break;
        }
    }
}

// 0x436540
void CCarCtrl::UpdateCarOnRails(CVehicle* vehicle) {
    plugin::Call<0x436540, CVehicle*>(vehicle);
}

// 0x426BC0
void CCarCtrl::WeaveForObject(CEntity* entity, CVehicle* vehicle, float* arg3, float* arg4) {
    plugin::Call<0x426BC0, CEntity*, CVehicle*, float*, float*>(entity, vehicle, arg3, arg4);
}

// 0x426350
void CCarCtrl::WeaveForOtherCar(CEntity* entity, CVehicle* vehicle, float* arg3, float* arg4) {
    plugin::Call<0x426350, CEntity*, CVehicle*, float*, float*>(entity, vehicle, arg3, arg4);
}

// 0x42D680
template<typename PtrListType>
void CCarCtrl::WeaveThroughCarsSectorList(PtrListType& ptrList, CVehicle* vehicle, CPhysical* physical, float arg4, float arg5, float arg6, float arg7, float* arg8, float* arg9) {
    plugin::Call<0x42D680, PtrListType&, CVehicle*, CPhysical*, float, float, float, float, float*, float*>(ptrList, vehicle, physical, arg4, arg5, arg6, arg7, arg8, arg9);
}

// 0x42D950
template<typename PtrListType>
void CCarCtrl::WeaveThroughObjectsSectorList(PtrListType& ptrList, CVehicle* vehicle, float arg3, float arg4, float arg5, float arg6, float* arg7, float* arg8) {
    plugin::Call<0x42D950, PtrListType&, CVehicle*, float, float, float, float, float*, float*>(ptrList, vehicle, arg3, arg4, arg5, arg6, arg7, arg8);
}

// 0x42D7E0
template<typename PtrListType>
void CCarCtrl::WeaveThroughPedsSectorList(PtrListType& ptrList, CVehicle* vehicle, CPhysical* physical, float arg4, float arg5, float arg6, float arg7, float* arg8, float* arg9) {
    plugin::Call<0x42D7E0, PtrListType&, CVehicle*, CPhysical*, float, float, float, float, float*, float*>(ptrList, vehicle, physical, arg4, arg5, arg6, arg7, arg8, arg9);
}

// 0x427FE0
float CCarCtrl::FindMaxSteerAngle(CVehicle* veh) {
    return std::clamp(0.9f - veh->GetMoveSpeed().Magnitude(), 0.2f, 0.7f);
}

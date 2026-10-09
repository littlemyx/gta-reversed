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
#include "CarAI.h"
#include "Events/EventPotentialGetRunOver.h"
#include "Game.h"
#include "General.h"
#include "GameLogic.h"
#include "CutsceneMgr.h"
#include "TheCarGenerators.h"
#include "eAreaCodes.h"
#include "TaskTypes/TaskComplexWander.h"
#include "Curves.h"

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

//! 0x59C910 - `CVector::Normalise`. The sum of squares and the reciprocal stay in the FPU registers (extended precision), the shared `CVector::Normalise` rounds them to float
static void NormaliseOriginal(CVector& v) {
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

//! Normalized 2D (X, Y) forward vector of the vehicle (the one used by the AI boat code)
//! x87: The length is kept in extended precision. Note: Uses the matrix directly (not null checked)
static CVector2D GetNormalizedForward2D(CVehicle* vehicle) {
    const auto& fwd = vehicle->m_matrix->GetForward();
    const auto  len = std::sqrt((double)fwd.y * fwd.y + (double)fwd.x * fwd.x);
    if (len == 0.0) {
        return { 1.0f, fwd.y };
    }
    const auto invLen = 1.0 / len;
    return { (float)(invLen * fwd.x), (float)(invLen * fwd.y) };
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
    RH_ScopedInstall(SlowCarDownForPedsSectorList, 0x425440);
    RH_ScopedInstall(SteerAICarWithPhysicsTryingToBlockTarget_Stop, 0x428990);
    RH_ScopedInstall(SteerAIBoatWithPhysicsHeadingForTarget, 0x428BE0);
    RH_ScopedInstall(SteerAIBoatWithPhysicsAttackingPlayer, 0x428DE0);
    RH_ScopedInstall(SteerAIBoatWithPhysicsCirclingPlayer, 0x429090);
    RH_ScopedInstall(TriggerDogFightMoves, 0x429300);
    RH_ScopedInstall(TestWhetherToFirePlaneGuns, 0x429520);
    RH_ScopedInstall(GetAIPlaneToAttackPlayer, 0x429780);
    RH_ScopedInstall(GetAIPlaneToDoDogFight, 0x429890);
    RH_ScopedInstall(FlyAIHeliInCertainDirection, 0x429A70);
    RH_ScopedInstall(SteerAIHeliTowardsTargetCoors, 0x42A630);
    RH_ScopedInstall(GetAIHeliToFlyInDirection, 0x42A730);
    RH_ScopedInstall(SteerAIHeliToFollowEntity, 0x42A750);
    RH_ScopedInstall(SteerAIHeliAsPoliceHeli, 0x42AAD0);
    RH_ScopedInstall(SteerAIHeliFlyingAwayFromPlayer, 0x42ACB0);
    RH_ScopedInstall(SteerAIHeliToLand, 0x42AD30);
    RH_ScopedInstall(SteerAIHeliToKeepEntityInView, 0x42AEB0);
    RH_ScopedInstall(FireHeliRocketsAtTarget, 0x42B270);
    RH_ScopedInstall(FindLinksToGoWithTheseNodes, 0x42B470);
    RH_ScopedInstall(WeaveForObject, 0x426BC0);
    RH_ScopedInstall(WeaveForOtherCar, 0x426350);
    RH_ScopedInstall(FindNodesThisCarIsNearestTo, 0x42BD20);
    RH_ScopedInstall(ScanForPedDanger, 0x42CE40);
    RH_ScopedInstall(SlowCarDownForOtherCar, 0x42D0E0);
    RH_ScopedInstall(SlowCarDownForObjectsSectorList, 0x42D4F0);
    RH_ScopedInstall(WeaveThroughCarsSectorList, 0x42D680);
    RH_ScopedInstall(WeaveThroughPedsSectorList, 0x42D7E0);
    RH_ScopedInstall(WeaveThroughObjectsSectorList, 0x42D950);
    RH_ScopedInstall(PickNextNodeRandomly, 0x42DE80);
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
    if (vehicle->m_nForcedRandomRouteSeed) {
        srand((uint16)vehicle->m_nForcedRandomRouteSeed);
    }

    auto&      autoPilot = vehicle->m_autoPilot;
    const auto cur       = autoPilot.m_currentAddress;
    const auto start     = autoPilot.m_startingRouteNode;

    const auto& curNode  = ThePaths.m_pPathNodes[cur.m_wAreaId][cur.m_wNodeId];
    const auto  linkBase = (int32)curNode.m_wBaseLinkId;
    const auto  GetLink  = [&](int32 idx) { return ThePaths.m_pNodeLinks[cur.m_wAreaId][linkBase + idx]; };
    const auto  IsBefore = [](CNodeAddress a, CNodeAddress b) { // Compares the (area, node) pair
        return a.m_wAreaId < b.m_wAreaId || (a.m_wAreaId == b.m_wAreaId && a.m_wNodeId < b.m_wNodeId);
    };

    // Find the link that leads to the starting node (BUG: If there's none the index ends up being 12, which is out of range)
    int16 startLinkIdx = 0;
    do {
        if (GetLink(startLinkIdx) == start) {
            break;
        }
        startLinkIdx++;
    } while (startLinkIdx < 12);
    autoPilot.m_nNextPathNodeInfo = ThePaths.m_pNaviLinks[cur.m_wAreaId][linkBase + startLinkIdx];
    autoPilot._smthNext           = IsBefore(cur, start) ? -1 : 1;

    // Find the link to the node that is closest to the vehicle (the line leading there)
    int32 linkIdx;
    if (curNode.m_nNumLinks == 1) {
        linkIdx = 0;
    } else {
        linkIdx = -1;
        auto closestDist = std::bit_cast<float>(0x497423FEu); // ~999999.9
        for (int32 i = 0; i < (int32)curNode.m_nNumLinks; i++) {
            const auto link = GetLink(i);
            if (link == start || !ThePaths.m_pPathNodes[link.m_wAreaId]) {
                continue;
            }
            const auto& linkedNode = ThePaths.m_pPathNodes[link.m_wAreaId][link.m_wNodeId];
            const auto  dist       = CCollision::DistToLine(curNode.GetPosition(), linkedNode.GetPosition(), vehicle->GetPosition());
            if (dist < closestDist) {
                closestDist = dist;
                linkIdx     = i;
            }
        }
        if (linkIdx < 0) {
            linkIdx = 0;
        }
    }

    autoPilot.m_nCurrentPathNodeInfo = ThePaths.m_pNaviLinks[cur.m_wAreaId][linkBase + linkIdx];
    autoPilot._smthCurr              = IsBefore(GetLink(linkIdx), cur) ? -1 : 1;
}

// 0x434400
float CCarCtrl::FindMaximumSpeedForThisCarInTraffic(CVehicle* vehicle) {
    return plugin::CallAndReturn<float, 0x434400, CVehicle*>(vehicle);
}

// 0x42BD20
void CCarCtrl::FindNodesThisCarIsNearestTo(CVehicle* vehicle, CNodeAddress& nodeAddress1, CNodeAddress& nodeAddress2) {
    // BUG: In the original the high words (node ids) of the results are uninitialized stack memory if no node is found, the area ids are 0xFFFF
    CNodeAddress bestNode{};
    CNodeAddress bestLinkedNode{};
    float        bestScore = std::bit_cast<float>(0x47C34FF3u); // ~99999.9

    const auto& pos = vehicle->GetPosition();

    const auto xRegion = (int32)ThePaths.FindXRegionForCoors(pos.x);
    const auto yRegion = (int32)ThePaths.FindYRegionForCoors(pos.y);

    // Position inside of the region (x87: only the X one is rounded to float)
    const auto relX = (float)((double)pos.x - ThePaths.FindXCoorsForRegion(xRegion));
    const auto relY = (double)pos.y - ThePaths.FindYCoorsForRegion(yRegion);

    // Also check the neighbouring regions if the vehicle is close to the border
    auto minX = xRegion, minY = yRegion, maxX = xRegion, maxY = yRegion;
    if (!(relX >= 200.0f)) {
        minX--;
    }
    if (!(relY >= 200.0f)) {
        minY--;
    }
    if (!(relX <= 550.0f)) {
        maxX++;
    }
    if (!(relY <= 550.0f)) {
        maxY++;
    }
    minX = std::max(minX, 0);
    minY = std::max(minY, 0);
    maxX = std::min(maxX, 7);
    maxY = std::min(maxY, 7);

    // Note: The matrix is used directly (not null checked) when it's needed for the direction
    for (auto x = minX; x <= maxX; x++) {
        for (auto y = minY; y <= maxY; y++) {
            const auto areaId = x + y * 8;
            const auto nodes  = ThePaths.m_pPathNodes[areaId];
            if (!nodes) {
                continue;
            }
            for (auto i = 0u; i < ThePaths.m_anNumVehicleNodes[areaId]; i++) {
                const auto& node    = nodes[i];
                const auto  nodePos = node.GetPosition();

                // x87: the Y/Z differences are kept in extended precision (only X is rounded to float)
                const auto dx = (float)((double)nodePos.x - pos.x);
                const auto dy = (double)nodePos.y - pos.y;
                const auto dz = (double)nodePos.z - pos.z;
                if (std::sqrt((dz * dz + dy * dy) + (double)dx * dx) >= 150.0f) {
                    continue;
                }

                for (auto j = 0; j < (int32)node.m_nNumLinks; j++) {
                    const auto linkedAddr = ThePaths.m_pNodeLinks[areaId][node.m_wBaseLinkId + j];
                    const auto linkedArea = ThePaths.m_pPathNodes[linkedAddr.m_wAreaId];
                    if (!linkedArea) {
                        continue;
                    }
                    const auto& linkedNode    = linkedArea[linkedAddr.m_wNodeId];
                    const auto  linkedNodePos = linkedNode.GetPosition();

                    // The Z coordinates are scaled by 3
                    const CVector lineStart{ nodePos.x, nodePos.y, nodePos.z * 3.0f };
                    const CVector lineEnd{ linkedNodePos.x, linkedNodePos.y, linkedNodePos.z * 3.0f };
                    const CVector point{ pos.x, pos.y, pos.z * 3.0f };
                    const auto    distToLine = CCollision::DistToLine(lineStart, lineEnd, point); // 0x417610

                    CVector dir = linkedNodePos - nodePos;
                    NormaliseOriginal(dir); // 0x59C910

                    // x87: the dot product is kept in extended precision
                    const auto& fwd = vehicle->m_matrix->GetForward();
                    const auto  dot = ((double)dir.z * fwd.z + (double)dir.y * fwd.y) + (double)dir.x * fwd.x;
                    const auto  score = (1.0f - dot) * 5.0f + distToLine;
                    if (!(score >= bestScore)) {
                        bestScore      = (float)score;
                        bestNode       = { (uint16)areaId, (uint16)i };
                        bestLinkedNode = linkedAddr;
                    }
                }
            }
        }
    }

    nodeAddress1 = bestNode;
    nodeAddress2 = bestLinkedNode;
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
    if (entityLauncher->m_nVehicleWeaponInUse != CAR_WEAPON_DOUBLE_ROCKET && entityLauncher->m_nVehicleWeaponInUse != CAR_WEAPON_NOT_USED) {
        return;
    }

    // Four times a second
    if (CTimer::GetTimeInMS() / 250u == CTimer::GetPreviousTimeInMS() / 250u) {
        return;
    }

    // Only if the target is in range
    const auto& launcherPos = entityLauncher->GetPosition();
    const auto& targetPos   = entity->GetPosition();
    {
        // x87: The differences are kept in extended precision
        const auto dx = (double)launcherPos.x - targetPos.x;
        const auto dy = (double)launcherPos.y - targetPos.y;
        const auto dz = (double)launcherPos.z - targetPos.z;
        if (!(std::sqrt((dz * dz + dy * dy) + dx * dx) < 80.0f)) {
            return;
        }
    }

    // And if the launcher is (more or less) pointing at it
    const auto& mat = *entityLauncher->m_matrix; // Not null checked in the original
    const CVector ahead{
        (float)((double)mat.GetForward().x + launcherPos.x),
        (float)((double)mat.GetForward().y + launcherPos.y),
        (float)((double)mat.GetForward().z + launcherPos.z)
    };
    if (!(CCollision::DistToMathematicalLine(&launcherPos, &ahead, &targetPos) < 7.0f)) {
        return;
    }

    // Fire from the right or the left side of the heli (alternates)
    const auto bLeft = ((CTimer::GetTimeInMS() / 250u) & 1) != 0;

    // x87: Some values are rounded to float, others aren't
    const auto& fwd   = mat.GetForward();
    const auto  noseX = (double)fwd.x * 4.0f + launcherPos.x;
    const auto  noseY = (double)(float)((double)fwd.y * 4.0f) + launcherPos.y;
    const auto  noseZ = (float)((double)(float)((double)fwd.z * 4.0f) + launcherPos.z);

    const auto& right = mat.GetRight();
    auto        sideX = (float)((double)right.x * 1.5f);
    auto        sideZ = (float)((double)right.z * 1.5f);
    double      sideY = (double)right.y * 1.5f;
    if (bLeft) {
        sideX = -sideX;
        sideY = (float)-sideY;
        sideZ = -sideZ;
    }

    const CVector launchPos{
        (float)((double)sideX + noseX),
        (float)(sideY + noseY),
        (float)((double)sideZ + noseZ)
    };
    CProjectileInfo::AddProjectile(entityLauncher, WEAPON_ROCKET, launchPos, 1.0f, &fwd, nullptr); // 0x737C80
}

// 0x429A70
//! @param angle           The direction to fly in (radians)
//! @param targetDist      Distance to the target; the heli only closes in (via the pitch) if it's not further than 60 units
//! @param bUseDestination Pitch towards the destination (`m_vecDestinationCoors`) to get closer to it
void CCarCtrl::FlyAIHeliInCertainDirection(CHeli* heli, float angle, float targetDist, bool bUseDestination) {
    constexpr auto PI = std::numbers::pi_v<float>; // 0x858CB8
    const auto&    moveSpeed = heli->m_vecMoveSpeed;
    auto&          autoPilot = heli->m_autoPilot;

    // News heli: Look sideways when it's (almost) stopped
    bool bSlowNewsHeli = false;
    if (autoPilot.m_nCarMission == MISSION_HELI_NEWS_BEHAVIOUR
        && targetDist < (float)autoPilot.m_ucHeliTargetDist2
        && std::sqrt((double)moveSpeed.y * moveSpeed.y + (double)moveSpeed.x * moveSpeed.x) < 0.01f
    ) {
        bSlowNewsHeli = true;
        angle         = (float)((double)angle + PI / 2.0f); // 0x858FE4
    }

    // Twice a second (at a different time for each heli) check for obstacles ahead and to the sides
    const uint32 seed = heli->m_nRandomSeed;
    if ((CTimer::GetTimeInMS() + seed) % 500 < (seed + CTimer::GetPreviousTimeInMS()) % 500) {
        heli->field_9AC = heli->m_fMaxAltitude;

        const auto  LineOfSight = [](const CVector& from, const CVector& to, CColPoint& colPoint) {
            CEntity* hitEntity{};
            return CWorld::ProcessLineOfSight(from, to, colPoint, hitEntity, true, false, false, false, false, false, false, true); // 0x56BA00
        };
        CColPoint colPoint;

        const auto stepX = (float)((double)moveSpeed.x * 50.0f);
        const auto stepY = (float)((double)moveSpeed.y * 50.0f);
        const auto stepZ = (double)moveSpeed.z * 50.0f; // x87: kept in extended precision
        const auto& pos  = heli->GetPosition();
        const CVector origin{
            (float)((double)stepX + pos.x),
            (float)((double)stepY + pos.y),
            (float)(stepZ + pos.z)
        };

        // Probe ahead (and downwards)
        const auto cosF = (float)std::cos((double)angle);
        const auto sinF = (float)std::sin((double)angle);
        CVector    dir{ cosF, sinF, -1.0f };
        NormaliseOriginal(dir); // 0x59C910
        const CVector probeTarget{
            (float)((double)dir.x * 60.0f + origin.x),
            (float)((double)dir.y * 60.0f + origin.y),
            (float)((double)(float)((double)dir.z * 60.0f) + origin.z)
        };
        if (LineOfSight(origin, probeTarget, colPoint)) {
            const auto altitude = bSlowNewsHeli
                ? (double)heli->m_fMinAltitude * 0.5f + colPoint.m_vecPoint.z
                : (double)colPoint.m_vecPoint.z + heli->m_fMinAltitude;
            if (!((double)heli->field_9AC > altitude)) {
                heli->field_9AC = (float)altitude;
            }
        }

        // Probe ahead (level): How far ahead do we look depends on the speed
        const auto len = (float)(std::sqrt((double)moveSpeed.y * moveSpeed.y + (double)moveSpeed.x * moveSpeed.x) * 100.0f + (heli->vehicleFlags.bIsRCVehicle ? 5.0f : 30.0f));
        heli->field_9B8 = 1;

        const auto ProbeTarget = [&](const CVector& from) { // The point `len` away from `from` in the flying direction
            return CVector{
                (float)((double)cosF * len + from.x),
                (float)((double)sinF * len + from.y),
                (float)((double)(float)((double)0.0f * len) + from.z)
            };
        };
        if (!LineOfSight(origin, ProbeTarget(origin), colPoint)) {
            heli->field_9B8              = 0;
            heli->m_fSteeringLeftRight   = 0.0f;
        } else {
            // Something is ahead: Find out which side is free for longer
            const auto& mat = *heli->m_matrix; // Not null checked in the original
            CVector right{ mat.GetRight().x, mat.GetRight().y, 0.0f };
            NormaliseOriginal(right); // 0x59C910

            const auto DistToHit = [&](const CVector& from) { // x87: the differences and the sum are kept in extended precision
                const auto dx = (double)colPoint.m_vecPoint.x - from.x;
                const auto dy = (double)colPoint.m_vecPoint.y - from.y;
                const auto dz = (double)colPoint.m_vecPoint.z - from.z;
                return std::sqrt((dz * dz + dy * dy) + dx * dx);
            };

            // Right side
            const auto& pos1 = heli->GetPosition();
            const CVector sideR{
                (float)((double)right.x * 10.0f + pos1.x),
                (float)((double)right.y * 10.0f + pos1.y),
                (float)((double)(float)((double)right.z * 10.0f) + pos1.z)
            };
            float distR = 1000.0f;
            if (LineOfSight(pos1, sideR, colPoint)) {
                distR = 0.0f;
            } else if (LineOfSight(sideR, ProbeTarget(sideR), colPoint)) {
                distR = (float)DistToHit(sideR);
            }

            // Left side
            const auto& pos2 = heli->GetPosition();
            const CVector sideL{
                (float)((double)pos2.x - (double)right.x * 10.0f),
                (float)((double)pos2.y - (double)right.y * 10.0f),
                (float)((double)pos2.z - (float)((double)right.z * 10.0f))
            };
            double distL = 1000.0f;
            if (LineOfSight(pos2, sideL, colPoint)) {
                distL = 0.0f;
            } else if (LineOfSight(sideL, ProbeTarget(sideL), colPoint)) {
                distL = DistToHit(sideL); // not rounded (x87)
            }

            heli->m_fSteeringLeftRight = distL > distR ? 0.5f : -0.5f;
        }
    }

    if (autoPilot.m_nCarMission == MISSION_HELI_LAND_TOUCHING_DOWN) {
        heli->field_9B8            = 0;
        heli->m_fSteeringLeftRight = 0.0f;
    }

    const auto& mat     = *heli->m_matrix; // Not null checked in the original
    const auto  heading = CGeneral::GetATanOfXY(mat.GetForward().x, mat.GetForward().y);

    // Throttle: Try to get to the wanted altitude
    float climbFactor; // 0.5 less than the throttle (before it's clamped)
    {
        heli->m_fAccelerationBreakStatus = 0.0f;
        const auto altDiff = (double)heli->field_9AC - ((double)moveSpeed.z * 100.0f + heli->GetPosition().z);
        heli->m_fAccelerationBreakStatus = (float)(altDiff * (altDiff > 0.0 ? 0.1f : 0.2f));

        // Add some noise
        const auto throttle = ((double)(rand() & 0xF) - 7.0f) * 0.00200000009f + heli->m_fAccelerationBreakStatus;
        heli->m_fAccelerationBreakStatus = (float)throttle;
        climbFactor = (float)(throttle - 0.5f);

        if (!(throttle < 1.0f)) {
            heli->m_fAccelerationBreakStatus = 1.0f;
        } else {
            const auto throttleF = (float)throttle;
            heli->m_fAccelerationBreakStatus = -0.3f > throttleF ? -0.3f : throttleF;
        }
    }

    // Steer: Rotate towards the wanted direction
    {
        auto headingDiff = (double)angle - heading;
        while (headingDiff > PI) {
            headingDiff -= 2.0f * PI;
        }
        while (headingDiff < -PI) {
            headingDiff += 2.0f * PI;
        }
        const auto skid = headingDiff * -2.0f;
        heli->m_fLeftRightSkid = (float)skid;
        if (skid < 1.0f) {
            if (-1.0f > skid) {
                heli->m_fLeftRightSkid = -1.0f;
            }
        } else {
            heli->m_fLeftRightSkid = 1.0f;
        }
    }

    // Pitch: Fly forward / slow down to get to the destination
    if (targetDist > 60.0f || !bUseDestination) {
        heli->m_fSteeringUpDown = -0.8f;
    } else {
        const auto  stepY   = (float)((double)moveSpeed.y * 50.0f);
        const auto& pos     = heli->GetPosition();
        const auto& dest    = autoPilot.m_vecDestinationCoors;
        const auto  dx      = (float)(((double)moveSpeed.x * 50.0f + pos.x) - dest.x);
        const auto  dy      = ((double)stepY + pos.y) - dest.y; // x87: not rounded
        const auto  distF   = (float)autoPilot.m_ucHeliTargetDist2;
        const auto  excess  = std::sqrt(dy * dy + (double)dx * dx) - distF;
        if (!(excess < 0.0)) {
            heli->m_fSteeringUpDown = (float)((excess * -0.8f) / (30.0 - distF));
        } // else: unchanged (keeps the value of the last frame)
    }

    // Leveling out: Don't pitch forward (too much) if we're not facing the way we want to go
    if (heli->m_fSteeringUpDown < 0.0f) {
        auto diff = (double)heading - angle;
        while (diff < -PI) {
            diff += 2.0f * PI;
        }
        while (diff > PI) {
            diff -= 2.0f * PI;
        }
        if (diff < 0.0) {
            diff = -diff;
        }
        auto factor = 1.0 - std::bit_cast<float>(0x4007CFEDu) * diff; // 0x858FE0
        if (0.0f > factor) {
            factor = 0.0f;
        }
        heli->m_fSteeringUpDown = (float)(factor * heli->m_fSteeringUpDown);

        if (diff > PI / 2.0f && (double)moveSpeed.z * mat.GetForward().z + (double)moveSpeed.y * mat.GetForward().y + (double)mat.GetForward().x * moveSpeed.x > 0.0) {
            heli->m_fSteeringUpDown = 0.3f;
        }
    }

    // Don't pitch forward as much if we're climbing quickly
    if (climbFactor > 0.0f) {
        const auto factor = climbFactor < 1.0f ? climbFactor : 1.0f;
        heli->m_fSteeringUpDown = (float)((1.0f - factor) * heli->m_fSteeringUpDown);
    }

    // Still pitching forward: Limit the pitch by the speed (so the heli can brake)
    if (heli->m_fSteeringUpDown < 0.0f) {
        const auto fwdSpeed = (double)moveSpeed.z * mat.GetForward().z + (double)moveSpeed.y * mat.GetForward().y + (double)moveSpeed.x * mat.GetForward().x;
        auto       limit    = ((double)autoPilot.m_nCruiseSpeed - fwdSpeed * 60.0f) * 0.1f;
        if (limit > 1.0f) {
            limit = 1.0f;
        } else if (0.0f > limit) {
            limit = 0.0f;
        }
        const auto steer = -limit < heli->m_fSteeringUpDown ? (double)heli->m_fSteeringUpDown : -limit;
        heli->m_fSteeringUpDown = (float)steer;

        // Speed boost (while it's not that much pitched back)
        if (autoPilot.m_ucHeliSpeedMult && steer < -0.2f && !heli->field_9B8) {
            const auto k = (double)(int8)autoPilot.m_ucHeliSpeedMult * CTimer::GetTimeStep() * 0.001f;
            const auto& fwd = mat.GetForward();
            const auto newX = (float)((double)fwd.x * k + moveSpeed.x);
            const auto newY = (float)((double)(float)((double)fwd.y * k) + moveSpeed.y);
            const auto newZ = (float)((double)(float)(k * 0.0f) + moveSpeed.z);
            heli->m_vecMoveSpeed = CVector{ newX, newY, newZ };
        }
    }

    // About to hit something ahead: Brake
    if (heli->field_9B8) {
        CVector fwd{ mat.GetForward().x, mat.GetForward().y, 0.0f };
        NormaliseOriginal(fwd); // 0x59C910
        heli->m_fSteeringUpDown = (float)(((double)fwd.y * moveSpeed.y + (double)fwd.z * moveSpeed.z + (double)fwd.x * moveSpeed.x) * 2.0);
    }

    // Clamp
    if (heli->m_fSteeringUpDown < 1.0f) {
        if (-1.0f > heli->m_fSteeringUpDown) {
            heli->m_fSteeringUpDown = -1.0f;
        }
    } else {
        heli->m_fSteeringUpDown = 1.0f;
    }
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
    const auto heli = static_cast<CHeli*>(automobile);
    FlyAIHeliInCertainDirection(heli, heli->field_9B4, 1000.0f, false);
}

// 0x429780
void CCarCtrl::GetAIPlaneToAttackPlayer(CAutomobile* automobile) {
    const auto plane = static_cast<CPlane*>(automobile);

    // Aim at where the player will be in 50 frames
    const auto& playerSpeed = FindPlayerSpeed(-1);
    const auto  speedX      = (float)((double)playerSpeed.x * 50.0f);
    const auto  speedY      = (float)((double)playerSpeed.y * 50.0f);
    const auto  speedZ      = (float)((double)playerSpeed.z * 50.0f);
    const auto  playerPos   = FindPlayerCoors(-1);

    // x87: The X, Y values are kept in extended precision (the Z is rounded, as it's stored)
    const auto  targetZ = (float)((double)speedZ + playerPos.z);
    const auto& pos     = plane->GetPosition();
    plane->m_planeHeading = CGeneral::GetATanOfXY(
        (float)(((double)speedX + playerPos.x) - pos.x),
        (float)(((double)speedY + playerPos.y) - pos.y)
    );
    plane->m_maxAltitude = targetZ;
    FlyAIPlaneInCertainDirection(plane);

    if (FindPlayerVehicle(-1, false)) {
        if (FindPlayerVehicle(-1, false)->GetVehicleAppearance() == VEHICLE_APPEARANCE_PLANE) {
            TriggerDogFightMoves(plane, FindPlayerVehicle(-1, false));
        }
    }
    TestWhetherToFirePlaneGuns(plane, FindPlayerVehicle(-1, false));
    PossiblyFireHSMissile(plane, FindPlayerVehicle(-1, false));
}

// 0x429890
void CCarCtrl::GetAIPlaneToDoDogFight(CAutomobile* automobile) {
    const auto plane  = static_cast<CPlane*>(automobile);
    const auto target = plane->m_autoPilot.m_TargetEntity;

    // Where the target will be in 50 frames
    // x87: The X, Y values are kept in extended precision (the Z is rounded, as it's stored)
    const auto& targetPos   = target->GetPosition();
    const auto& targetSpeed = target->m_vecMoveSpeed;
    const auto  stepZ       = (float)((double)targetSpeed.z * 50.0f);
    const auto  predX       = (float)((double)targetSpeed.x * 50.0f + targetPos.x);
    const auto  predY       = (float)((double)targetSpeed.y * 50.0f + targetPos.y);
    const auto  predZ       = (float)((double)stepZ + targetPos.z);

    auto&       dest = plane->m_autoPilot.m_vecDestinationCoors;
    const auto& pos  = plane->GetPosition();
    if (plane->m_autoPilot.m_bPlaneDogfightSomething) {
        // Flying to the random point
        plane->m_planeHeading = CGeneral::GetATanOfXY(
            (float)((double)dest.x - pos.x),
            (float)((double)dest.y - pos.y)
        );
        plane->m_maxAltitude = dest.z;

        // Reached it?
        const auto& pos2 = plane->GetPosition();
        const auto  dx   = (double)dest.x - pos2.x; // x87: not rounded
        const auto  dy   = (double)dest.y - pos2.y;
        if (std::sqrt(dy * dy + dx * dx) < 50.0f) {
            plane->m_autoPilot.m_bPlaneDogfightSomething = false;
        }
    } else {
        // Chase the target
        plane->m_planeHeading = CGeneral::GetATanOfXY(
            (float)((double)predX - pos.x),
            (float)((double)predY - pos.y)
        );
        plane->m_maxAltitude = predZ;

        // Every now and then fly to a random point near the target instead
        if ((rand() & 0x3FF) == 500) {
            plane->m_autoPilot.m_bPlaneDogfightSomething = true;
            dest.x = (float)((((double)rand() * RAND_MAX_FLOAT_RECIPROCAL) * 600.0f + predX) - 300.0f);
            dest.y = (float)((((double)rand() * RAND_MAX_FLOAT_RECIPROCAL) * 600.0f + predY) - 300.0f);
            dest.z = (float)((double)predZ + 50.0f);
        }
    }

    FlyAIPlaneInCertainDirection(plane);
    TestWhetherToFirePlaneGuns(plane, target);
    PossiblyFireHSMissile(plane, target);
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
bool CCarCtrl::IsThisAnAppropriateNode(CVehicle* vehicle, CNodeAddress nodeAddress1, CNodeAddress nodeAddress2, CNodeAddress nodeAddress3, bool arg5, bool arg6) {
    return plugin::CallAndReturn<bool, 0x42DAB0, CVehicle*, CNodeAddress, CNodeAddress, CNodeAddress, bool, bool>(vehicle, nodeAddress1, nodeAddress2, nodeAddress3, arg5, arg6);
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
    auto& ap = vehicle->m_autoPilot;

    if (vehicle->m_nForcedRandomRouteSeed) {
        srand((uint16)vehicle->m_nForcedRandomRouteSeed);
    }

    // NOTE: The names of the autopilot's node addresses are misleading here:
    // `m_currentAddress` is the node the vehicle is coming from, `m_startingRouteNode` is the one it's heading to, `m_endingRouteNode` is the previous node
    const auto origCur  = ap.m_currentAddress;
    const auto origNext = ap.m_startingRouteNode;
    if (!ThePaths.m_pPathNodes[origCur.m_wAreaId]) {
        return;
    }
    if (!ThePaths.m_pPathNodes[origNext.m_wAreaId]) {
        return;
    }
    if (!ThePaths.m_pPathNodes[ap.m_nNextPathNodeInfo.m_wAreaId]) {
        return;
    }

    const auto& nextNode = ThePaths.m_pPathNodes[origNext.m_wAreaId][origNext.m_wNodeId];
    const auto  numLinks = (int32)nextNode.m_nNumLinks;

    // Number of lanes (in the direction of travel) of the link the vehicle is on, and whether there are none in the other direction
    int32 numLanes;
    bool  noOtherLanes;
    {
        const auto& link = ThePaths.GetCarPathLink(ap.m_nNextPathNodeInfo);
        if (link.m_attachedTo == origNext) {
            numLanes     = link.m_numOppositeDirLanes;
            noOtherLanes = link.m_numSameDirLanes == 0;
        } else {
            numLanes     = link.m_numSameDirLanes;
            noOtherLanes = link.m_numOppositeDirLanes == 0;
        }
    }

    // Directions (as returned by `FindPathDirection`) that are acceptable: 4 = lane 0, 2 = last lane, 1 = anything
    uint8 dirMask = 0;
    if (ap.m_nNextLane == 0) {
        dirMask = 4;
    }
    if ((int32)ap.m_nNextLane == numLanes - 1) {
        dirMask |= 2;
    }
    if (numLanes < 3 || dirMask == 0) {
        dirMask |= 1;
    }

    ap.m_endingRouteNode = ap.m_currentAddress;
    ap.m_currentAddress  = ap.m_startingRouteNode;

    if (ThisVehicleShouldTryNotToTurn(vehicle)) { // 0x421FE0
        dirMask = 1;
    }

    InitSequence(numLinks); // 0x421740

    // Index of the i-th link to try (random order)
    const auto GetLinkIdx = [](int32 i) {
        return bSequenceOtherWay
            ? (SequenceRandomOffset + i) % SequenceElements
            : (SequenceElements - i + SequenceRandomOffset) % SequenceElements;
    };

    CCarPathLinkAddress chosenLink{};
    bool                found = false;

    // 1st pass: Find a link that is appropriate in every aspect
    for (int32 i = 0; i < numLinks && !found; i++) {
        const auto idx  = GetLinkIdx(i);
        const auto cand = ThePaths.m_pNodeLinks[origNext.m_wAreaId][nextNode.m_wBaseLinkId + idx];
        ap.m_startingRouteNode = cand;
        if (!ThePaths.m_pPathNodes[cand.m_wAreaId]) {
            continue;
        }

        bool       outBool = false;
        const auto dir     = FindPathDirection(origCur, ap.m_currentAddress, cand, &outBool); // 0x422090

        const auto navi = ThePaths.m_pNaviLinks[origNext.m_wAreaId][nextNode.m_wBaseLinkId + idx];
        if (!ThePaths.m_pPathNodes[navi.m_wAreaId]) {
            continue;
        }
        if (vehicle->GetStatus() == STATUS_SIMPLE && outBool) {
            continue;
        }

        bool flagC, flagB;
        {
            const auto& link = ThePaths.GetCarPathLink(navi);
            if (link.m_attachedTo == origNext) {
                flagC = link.m_numSameDirLanes == 0;
                flagB = link.m_numOppositeDirLanes == 0;
            } else {
                flagC = link.m_numOppositeDirLanes == 0;
                flagB = link.m_numSameDirLanes == 0;
            }
        }
        if (IsThisAnAppropriateNode(vehicle, origCur, origNext, cand, flagC, flagB) && (dir & dirMask)) { // 0x42DAB0
            if (!noOtherLanes || !flagB) {
                chosenLink = navi;
                found      = true;
            }
        }
    }

    // Gets the link's lane info that tells if the link has no lanes going the way we're going from `origNext`
    const auto HasNoLanesFromNext = [&](CCarPathLinkAddress navi) {
        const auto& link = ThePaths.GetCarPathLink(navi);
        return link.m_attachedTo == origNext
            ? link.m_numSameDirLanes == 0
            : link.m_numOppositeDirLanes == 0;
    };

    // 2nd pass: Anything, other than going back (unless the node we're coming from is switched off, but the next isn't)
    for (int32 i = 0; i < numLinks && !found; i++) {
        const auto idx  = GetLinkIdx(i);
        const auto cand = ThePaths.m_pNodeLinks[origNext.m_wAreaId][nextNode.m_wBaseLinkId + idx];
        ap.m_startingRouteNode = cand;
        if (!ThePaths.m_pPathNodes[cand.m_wAreaId]) {
            continue;
        }

        const auto navi = ThePaths.m_pNaviLinks[origNext.m_wAreaId][nextNode.m_wBaseLinkId + idx];
        if (!ThePaths.m_pPathNodes[navi.m_wAreaId]) {
            continue;
        }

        if (HasNoLanesFromNext(navi) || origCur == ap.m_startingRouteNode) {
            continue;
        }
        if (!ThePaths.m_pPathNodes[cand.m_wAreaId][cand.m_wNodeId].m_isSwitchedOff || ThePaths.m_pPathNodes[origCur.m_wAreaId][origCur.m_wNodeId].m_isSwitchedOff) {
            chosenLink = navi;
            found      = true;
        }
    }

    // 3rd pass: Anything, other than going back
    for (int32 i = 0; i < numLinks && !found; i++) {
        const auto idx  = GetLinkIdx(i);
        const auto cand = ThePaths.m_pNodeLinks[origNext.m_wAreaId][nextNode.m_wBaseLinkId + idx];
        ap.m_startingRouteNode = cand;
        if (!ThePaths.m_pPathNodes[cand.m_wAreaId]) {
            continue;
        }

        const auto navi = ThePaths.m_pNaviLinks[origNext.m_wAreaId][nextNode.m_wBaseLinkId + idx];
        if (!ThePaths.m_pPathNodes[navi.m_wAreaId]) {
            continue;
        }

        if (origCur == ap.m_startingRouteNode) {
            continue;
        }
        if (!HasNoLanesFromNext(navi)) {
            chosenLink = navi;
            found      = true;
        }
    }

    // Dead end: Go back
    if (!found) {
        ap.m_startingRouteNode = origCur;
        chosenLink             = ap.m_nNextPathNodeInfo;
    }

    // The vehicle is turning around
    if (origCur == ap.m_startingRouteNode && vehicle->GetStatus() != STATUS_PHYSICS) {
        SwitchVehicleToRealPhysics(vehicle); // Inlined in the original
    }

    // Special nodes
    const auto behaviour = ThePaths.m_pPathNodes[ap.m_startingRouteNode.m_wAreaId][ap.m_startingRouteNode.m_wNodeId].m_nBehaviourType;
    switch (behaviour) {
    case 1:
    case 2:
        SwitchVehicleToRealPhysics(vehicle); // Inlined in the original
        ap.m_nCarMission      = behaviour == 1 ? MISSION_PARK_PARALLEL : MISSION_PARK_PERPENDICULAR;
        ap.m_nCarDrivingStyle = DRIVING_STYLE_STOP_FOR_CARS;
        break;
    case 10:
        ap.m_nTempAction     = TEMPACT_WAIT;
        ap.m_nTempActionTime = CTimer::GetTimeInMS() + 10000;
        if (vehicle->GetStatus() == STATUS_SIMPLE) {
            ap.ModifySpeed(0.0f); // 0x41B980
        }
        break;
    }

    if (ThePaths.m_pPathNodes[ap.m_currentAddress.m_wAreaId][ap.m_currentAddress.m_wNodeId].m_nBehaviourType == 9 && vehicle->m_nCreatedBy != MISSION_VEHICLE) {
        ap.m_nTempAction     = TEMPACT_WAIT;
        ap.m_nTempActionTime = CTimer::GetTimeInMS() + 4500;
        if (vehicle->GetStatus() == STATUS_SIMPLE) {
            ap.ModifySpeed(0.0f); // 0x41B980
        }
    }

    // Move on to the next link
    ap.m_nPreviousPathNodeInfo = ap.m_nCurrentPathNodeInfo;
    ap.m_nCurrentPathNodeInfo  = ap.m_nNextPathNodeInfo;
    ap.m_nNextPathNodeInfo     = chosenLink;
    ap.field_C                += (int32)ap.m_nSpeedScaleFactor;
    ap._smthPrev               = ap._smthCurr;
    ap._smthCurr               = ap._smthNext;
    ap.m_nCurrentLane          = ap.m_nNextLane;

    // The direction of travel on the new link (depends on the order of the nodes)
    const auto chosenNext = ap.m_startingRouteNode;
    ap._smthNext = (origNext.m_wAreaId < chosenNext.m_wAreaId || (origNext.m_wAreaId == chosenNext.m_wAreaId && origNext.m_wNodeId < chosenNext.m_wNodeId))
        ? -1
        : 1;

    int32 numLanesNext = 1;
    if (ThePaths.m_pPathNodes[ap.m_nNextPathNodeInfo.m_wAreaId]) {
        const auto& link = ThePaths.GetCarPathLink(ap.m_nNextPathNodeInfo);
        numLanesNext = ap._smthNext == -1 ? link.m_numSameDirLanes : link.m_numOppositeDirLanes;
    }

    // `CCarPathLink::m_dir`/`m_posn` hide their raw values, but the original works on them. x87: kept in extended precision until stored
    const auto& curLink   = ThePaths.GetCarPathLink(ap.m_nCurrentPathNodeInfo);
    const auto& nextLink  = ThePaths.GetCarPathLink(ap.m_nNextPathNodeInfo);
    const auto  curRaw8   = reinterpret_cast<const int8*>(&curLink);
    const auto  nextRaw8  = reinterpret_cast<const int8*>(&nextLink);
    const auto  curRaw16  = reinterpret_cast<const int16*>(&curLink);
    const auto  nextRaw16 = reinterpret_cast<const int16*>(&nextLink);

    const auto Dir = [](int8 d, int8 sign) { return (float)((double)d * (double)0.01f * (double)sign); }; // 0x858C58
    const float curDirX  = Dir(curRaw8[8], ap._smthCurr);
    const float curDirY  = Dir(curRaw8[9], ap._smthCurr);
    const float nextDirX = Dir(nextRaw8[8], ap._smthNext);
    const float nextDirY = Dir(nextRaw8[9], ap._smthNext);

    // If the next node is far enough from the current one the vehicle changes lanes from time to time
    {
        const auto curPos  = ThePaths.m_pPathNodes[ap.m_currentAddress.m_wAreaId][ap.m_currentAddress.m_wNodeId].GetPosition();
        const auto nextPos = ThePaths.m_pPathNodes[ap.m_startingRouteNode.m_wAreaId][ap.m_startingRouteNode.m_wNodeId].GetPosition();
        const auto dx      = (double)nextPos.x - curPos.x;
        const auto dy      = (double)nextPos.y - curPos.y;
        if (!(dx * dx + dy * dy <= 256.0f)) { // 0x858FB4
            if (--ap.field_50 == 0) {
                ap.field_50 = (char)((rand() & 3) + 4);
                ap.m_nNextLane += rand() >= 0x3FFF ? -1 : 1;
            }
        }
    }

    // Keep the lane in range
    {
        int32 lane = ap.m_nNextLane;
        if (lane >= numLanesNext - 1) {
            lane = numLanesNext - 1;
        }
        ap.m_nNextLane = (int8)lane;
        if (ap.m_nNextLane <= 0) {
            ap.m_nNextLane = 0;
        }
    }

    if (ap.carCtrlFlags.bStayInFastLane) {
        ap.m_nNextLane = 0;
    } else if (ap.carCtrlFlags.bStayInSlowLane) {
        ap.m_nNextLane = (int8)std::max(numLanesNext - 1, 0);
    }

    if (vehicle->GetStatus() != STATUS_SIMPLE) {
        return;
    }

    // Calculate the speed scale factor. x87: `k1` is rounded to float, `k2` is not
    const auto k1 = (float)((curLink.OneWayLaneOffsetExtended() + (double)ap.m_nCurrentLane) * (double)5.4f);   // 0x858C50
    const auto k2 = (nextLink.OneWayLaneOffsetExtended() + (double)ap.m_nNextLane) * (double)5.4f;

    const CVector end{
        (float)((double)nextRaw16[0] * (double)0.125f + k2 * (double)nextDirY),
        (float)((double)nextRaw16[1] * (double)0.125f - k2 * (double)nextDirX),
        0.0f
    };
    const CVector start{
        (float)((double)curRaw16[0] * (double)0.125f + (double)k1 * (double)curDirY),
        (float)((double)curRaw16[1] * (double)0.125f - (double)k1 * (double)curDirX),
        0.0f
    };

    const auto scale    = CCurves::CalcSpeedScaleFactor(start, end, curDirX, curDirY, nextDirX, nextDirY); // 0x43C710
    const auto newScale = (int32)((double)scale * (1000.0f / (double)ap.m_speed));                          // 0x858C4C
    ap.m_nSpeedScaleFactor = newScale > 10 ? newScale : 10;
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
    const auto radius = vehicle == FindPlayerVehicle(-1, false) ? 44.0f : 11.0f;

    const auto bHonkAtPedBefore = vehicle->m_autoPilot.carCtrlFlags.bHonkAtPed;

    const auto& pos = vehicle->GetPosition();
    const auto  minX = pos.x - radius;
    const auto  maxX = radius + pos.x;
    const auto  minY = pos.y - radius;
    const auto  maxY = radius + pos.y;

    // x87: The value is kept in extended precision for the first `floor`, then the rounded (to float) copy is used for the second one
    const auto GetSector = [](float v) {
        const double sector = (double)v * 0.02f + 60.0f;
        return std::pair{ sector, (float)sector };
    };
    const auto GetMinSector = [&](float v) -> int32 {
        const auto [ext, rounded] = GetSector(v);
        return (int32)std::floor(ext) > 0 ? (int32)std::floor((double)rounded) : 0;
    };
    const auto GetMaxSector = [&](float v) -> int32 {
        const auto [ext, rounded] = GetSector(v);
        return (int32)std::floor(ext) < 119 ? (int32)std::floor((double)rounded) : 119;
    };
    const auto sectorMinX = GetMinSector(minX);
    const auto sectorMinY = GetMinSector(minY);
    const auto sectorMaxX = GetMaxSector(maxX);
    const auto sectorMaxY = GetMaxSector(maxY);

    CWorld::AdvanceCurrentScanCode();

    float speedFactor = (float)vehicle->m_autoPilot.m_nCruiseSpeed;
    for (auto y = sectorMinY; y <= sectorMaxY; y++) {
        for (auto x = sectorMinX; x <= sectorMaxX; x++) {
            SlowCarDownForPedsSectorList( // 0x425440
                CWorld::GetRepeatSector(x, y).Peds,
                vehicle,
                minX, minY, maxX, maxY,
                &speedFactor,
                (float)vehicle->m_autoPilot.m_nCruiseSpeed
            );
        }
    }

    vehicle->vehicleFlags.bWarnedPeds = true;
    vehicle->m_autoPilot.carCtrlFlags.bHonkAtPed = bHonkAtPedBefore;
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
void CCarCtrl::SlowCarDownForObjectsSectorList(CPtrListDoubleLink<CObject*>& objList, CVehicle* vehicle, float minX, float minY, float maxX, float maxY, float* speedFactor, float speedMult) {
    // The original stores the next node before processing the current one
    for (auto it = objList.begin(); it != objList.end();) {
        CObject* const obj = *it;
        ++it;

        if (obj->IsScanCodeCurrent()) {
            continue;
        }
        obj->SetCurrentScanCode();

        // Only these objects block the road
        if (   obj->m_nModelIndex != ModelIndices::MI_ROADWORKBARRIER1
            && obj->m_nModelIndex != ModelIndices::MI_ROADBLOCKFUCKEDCAR1
            && obj->m_nModelIndex != ModelIndices::MI_ROADBLOCKFUCKEDCAR2
        ) {
            continue;
        }

        CVector centre;
        obj->GetBoundCentre(centre); // 0x534250
        if (centre.x <= minX || centre.x >= maxX || centre.y <= minY || centre.y >= maxY) {
            continue;
        }

        {
            const auto zDiff = (double)centre.z - vehicle->GetPosition().z;
            if ((zDiff < 0.0 ? -zDiff : zDiff) >= 10.0f) {
                continue;
            }
        }

        // Note: The matrix is used directly (not null checked)
        const auto& fwd = vehicle->m_matrix->GetForward();
        const auto distAlongLine = CCollision::DistAlongLine2D(
            vehicle->GetPosition().x, vehicle->GetPosition().y,
            fwd.x, fwd.y,
            centre.x, centre.y
        ); // 0x412A80
        const auto  vehPosZ = vehicle->GetPosition().z;
        const auto  vehFwdZ = vehicle->GetForwardVector().z;

        // x87: kept in extended precision
        const auto zErr = (double)centre.z - ((double)distAlongLine * vehFwdZ + vehPosZ);
        if ((zErr >= 0.0 ? zErr : -zErr) >= 3.0f) {
            continue;
        }

        SlowCarDownForObject(obj, vehicle, speedFactor, speedMult); // 0x426220
    }
}

// 0x42D0E0
void CCarCtrl::SlowCarDownForOtherCar(CEntity* entity, CVehicle* vehicle, float* speedFactor, float speedMult) {
    // Note: Both entities are assumed to be vehicles, and `vehicle`'s matrix is used directly (not null checked)
    const auto dir = GetNormalizedForward2D(vehicle);

    // Only the cars that are in front of `vehicle` are of interest. x87: kept in extended precision
    {
        const auto& entPos = entity->GetPosition();
        const auto& vehPos = vehicle->GetPosition();
        const auto  dot    = ((double)entPos.y - vehPos.y) * dir.y + ((double)entPos.x - vehPos.x) * dir.x;
        if (!(dot >= 0.0)) {
            return;
        }
    }

    // Velocity of the other car, and `vehicle`'s own (desired) velocity
    const auto& entSpeed = entity->AsPhysical()->m_vecMoveSpeed;
    const auto  entVelX  = entSpeed.x * 60.0f;
    const auto  entVelY  = entSpeed.y * 60.0f;
    const auto  vehVelX  = dir.x * speedMult;
    const auto  vehVelY  = dir.y * speedMult;

    entity->GetMatrix(); // Allocates the matrix if it hasn't got one (the original does this twice, the second is a no-op)

    CVector vehDir{ dir.x, dir.y, 0.0f };
    const auto entDir2D = GetNormalizedForward2D(entity->AsVehicle());
    CVector entDir{ entDir2D.x, entDir2D.y, 0.0f };

    // Relative velocity
    const auto relX = entVelX - vehVelX;
    const auto relY = entVelY - vehVelY;

    // Time (?) till the collision of the 2 cars
    float dist = TestCollisionBetween2MovingRects_OnlyFrontBumper(entity->AsVehicle(), vehicle, relX, relY, &vehDir, &entDir); // 0x425F70
    {
        const auto dist2 = TestCollisionBetween2MovingRects(vehicle, entity->AsVehicle(), -relX, -relY, &entDir, &vehDir); // 0x425B30
        if (dist2 <= dist) {
            dist = dist2;
        }
    }
    if (dist < 0.0f) {
        return;
    }

    if (!(dist >= 1.5f)) {
        vehicle->m_autoPilot.carCtrlFlags.bHonkAtCar = true;
        vehicle->m_autoPilot.m_ObstructingEntity = entity;
        entity->RegisterReference(&vehicle->m_autoPilot.m_ObstructingEntity); // 0x571B70

        const double recip = 1.0f / (double)speedMult; // x87: kept in extended precision
        if (!(dist >= recip)) {
            *speedFactor = 0.0f;
        } else if (!(dist >= 3.0f * recip)) {
            if (*speedFactor >= 1.0f) {
                *speedFactor = 1.0f;
            }
        } else {
            const double scaled = ((double)dist - 0.2f) * std::bit_cast<float>(0x3F44EC4Fu); // 0x858FFC (~0.7692308)
            dist = 0.0 <= scaled ? (float)scaled : 0.0f;

            const double newFactor = (double)dist * speedMult;
            if (newFactor <= *speedFactor) {
                *speedFactor = (float)newFactor;
            }
        }
    }

    // Cars that are moving towards each other and have been in the traffic for a while make the one with the lower address go on (and stop being 'simple')
    if (dist < 0.0f || dist >= 0.5f) {
        return;
    }
    if (!entity->GetIsTypeVehicle()) {
        return;
    }
    const auto timeMs = CTimer::GetTimeInMS();
    if (!(timeMs - vehicle->m_autoPilot.m_nTimeSwitchedToRealPhysics > 15000u)) {
        return;
    }
    if (!(timeMs - entity->AsVehicle()->m_autoPilot.m_nTimeSwitchedToRealPhysics > 15000u)) {
        return;
    }

    // x87: the dot product is kept in extended precision (rounded to float when stored)
    const auto& vehFwd = vehicle->m_matrix->GetForward();
    const auto& entFwd = entity->GetMatrix().GetForward();
    const auto  fwdDot = (float)((double)entFwd.y * vehFwd.y + (double)entFwd.x * vehFwd.x);
    if (entity == FindPlayerVehicle(-1, false)) {
        return;
    }
    if (fwdDot >= -0.5f) {
        return;
    }
    if (vehicle >= entity->AsVehicle()) { // Only one of the 2 cars (the one with the lower address) is affected
        return;
    }

    const double newFactor = (double)speedMult * 0.2f; // x87: the comparison is done with the not rounded value
    if (newFactor >= *speedFactor) {
        *speedFactor = (float)newFactor;
    }
    if (vehicle->GetStatus() == STATUS_SIMPLE) {
        SwitchVehicleToRealPhysics(vehicle);
    }
    vehicle->m_autoPilot.m_nCarDrivingStyle = DRIVING_STYLE_AVOID_CARS;
    vehicle->m_autoPilot.m_nTempActionTime  = CTimer::GetTimeInMS() + 1000;
}

// 0x425440
void CCarCtrl::SlowCarDownForPedsSectorList(CPtrListDoubleLink<CPed*>& pedList, CVehicle* vehicle, float minX, float minY, float maxX, float maxY, float* speedFactor, float speedMult) {
    // Note: The matrix is used directly (not null checked) throughout
    const auto& vehMat = *vehicle->m_matrix;
    const auto& fwd    = vehMat.GetForward();
    const auto& right  = vehMat.GetRight();

    auto halfWidth = CModelInfo::GetModelInfo(vehicle->m_nModelIndex)->GetColModel()->m_boundBox.m_vecMax.x;

    // x87: kept in extended precision (only `fwdSpeed` is rounded to float)
    const auto fwdSpeedExt = ((double)fwd.z * vehicle->m_vecMoveSpeed.z + (double)fwd.y * vehicle->m_vecMoveSpeed.y) + (double)fwd.x * vehicle->m_vecMoveSpeed.x;
    const auto fwdSpeed    = (float)fwdSpeedExt;
    const auto slowDownDist = (float)(fwdSpeedExt * 200.0f); // How far ahead peds are considered
    const auto closeDist    = (float)((double)std::abs(fwdSpeed) * 50.0f);

    const auto isVehicleOfConcern = [&] { // `bVar15` in the decomp - Peds in front of such vehicles are in danger
        if (vehicle == FindPlayerVehicle()) {
            return true;
        }
        switch (vehicle->m_nVehicleType) {
        case VEHICLE_TYPE_TRAIN:
        case VEHICLE_TYPE_PLANE:
        case VEHICLE_TYPE_HELI:
            return true;
        }
        const auto style = vehicle->m_autoPilot.m_nCarDrivingStyle;
        if (style != DRIVING_STYLE_STOP_FOR_CARS && style != DRIVING_STYLE_DRIVINGMODE_AVOIDCARS_STOPFORPEDS_OBEYLIGHTS) {
            return true;
        }
        return vehicle->GetStatus() == STATUS_PHYSICS;
    }();

    const auto halfLength = CModelInfo::GetModelInfo(vehicle->m_nModelIndex)->GetColModel()->m_boundBox.m_vecMax.y;

    // The original stores the next node before processing the current one
    for (auto it = pedList.begin(); it != pedList.end();) {
        CPed* const ped = *it;
        ++it;

        if (ped->IsScanCodeCurrent() || !ped->m_bUsesCollision) {
            continue;
        }
        ped->SetCurrentScanCode();

        const CVector pedPos = ped->GetPosition();
        if (!(pedPos.x > minX) || !(pedPos.x < maxX) || !(pedPos.y > minY) || !(pedPos.y < maxY)) {
            continue;
        }

        {
            const auto zDiff = (double)pedPos.z - vehicle->GetPosition().z;
            if (!((zDiff < 0.0 ? -zDiff : zDiff) < 6.0f)) {
                continue;
            }
        }

        // How far along the vehicle's forward vector (in 2D) is the ped, and how far is he from that line (in Z)
        const auto distAlongLine = CCollision::DistAlongLine2D(
            vehicle->GetPosition().x, vehicle->GetPosition().y,
            fwd.x, fwd.y,
            pedPos.x, pedPos.y
        ); // 0x412A80
        {
            const auto zErr = (double)pedPos.z - ((double)distAlongLine * vehicle->GetForwardVector().z + vehicle->GetPosition().z);
            if (!((zErr < 0.0 ? -zErr : zErr) < 3.0f)) {
                continue;
            }
        }

        // Ped's position relative to the vehicle (each component is rounded to float)
        const auto dz = (float)((double)pedPos.z - vehicle->GetPosition().z);
        const auto dy = (float)((double)pedPos.y - vehicle->GetPosition().y);
        const auto dx = (float)((double)pedPos.x - vehicle->GetPosition().x);

        // Distance of the ped along the vehicle's forward vector (x87: sum is kept in extended precision, rounded to float when stored)
        auto pedFwdDist = (float)(((double)dy * fwd.y + (double)dz * fwd.z) + (double)dx * fwd.x);

        const auto style = vehicle->m_autoPilot.m_nCarDrivingStyle;
        if (   (style == DRIVING_STYLE_STOP_FOR_CARS || style == DRIVING_STYLE_STOP_FOR_CARS_IGNORE_LIGHTS || style == DRIVING_STYLE_SLOW_DOWN_FOR_CARS || style == DRIVING_STYLE_DRIVINGMODE_AVOIDCARS_STOPFORPEDS_OBEYLIGHTS)
            && (ped != FindPlayerPed() || CWorld::Players[CWorld::PlayerInFocus].m_pLastTargetVehicle != vehicle)
            && halfLength < pedFwdDist
        ) {
            const auto distToFront = (double)pedFwdDist - halfLength; // x87: not rounded when compared
            if (distToFront < slowDownDist) {
                const auto distToFrontF = (float)distToFront;

                // x87: kept in extended precision
                const auto pedRightDist = std::abs(((double)dy * right.y + (double)dz * right.z) + (double)dx * right.x);
                if (vehicle->m_nVehicleType == VEHICLE_TYPE_BIKE) {
                    halfWidth = halfWidth * 1.6f; // BUG: This is persistent for the rest of the loop, so it's applied for every ped, not just once
                }
                if (pedRightDist <= (double)halfWidth + 0.5f && distToFrontF < 13.0f) {
                    const auto gap = distToFrontF - 1.0f;
                    const double clampedGap = 0.0f > gap ? 0.0f : gap;
                    const auto   scaledGap  = ((double)(1.0f / 13.0f) * clampedGap) * speedMult;
                    const auto   newFactor  = 1.0f > scaledGap ? 1.0 : scaledGap;
                    *speedFactor = newFactor > *speedFactor ? *speedFactor : (float)newFactor;

                    vehicle->m_autoPilot.carCtrlFlags.bHonkAtPed = true;
                    if (distToFrontF < 4.0f) {
                        vehicle->m_autoPilot.m_nTempAction     = TEMPACT_WAIT;
                        vehicle->m_autoPilot.m_nTempActionTime = CTimer::GetTimeInMS() + 4000;
                    }
                    if (distToFrontF < 2.5f) {
                        vehicle->m_autoPilot.m_nTempAction     = TEMPACT_BRAKE;
                        vehicle->m_autoPilot.m_nTempActionTime = CTimer::GetTimeInMS() + 4000;
                    }
                }
            }
        }

        if (ped->GetType() != ENTITY_TYPE_PED) {
            continue;
        }

        // The player is honking at the ped
        if (vehicle == FindPlayerVehicle() && vehicle->m_HornCounter) {
            const CVector offset = ped->GetPosition() - vehicle->GetPosition(); // 0x40FE60
            // x87: kept in extended precision
            if ((((double)offset.x * offset.x + (double)offset.y * offset.y) + (double)offset.z * offset.z) < 49.0f) {
                CEventPotentialGetRunOver event{ vehicle };
                ped->GetIntelligence()->GetEventGroup().Add(&event, false);
            }
        }

        // Ped is in the way of a fast moving vehicle
        if (isVehicleOfConcern && fwdSpeed != 0.0f) {
            const auto fwdSign   = pedFwdDist < 0.0f ? -1 : 1;
            const auto speedSign = fwdSpeed < 0.0f ? -1 : 1;
            if (fwdSign != speedSign) {
                continue;
            }
            pedFwdDist = std::abs(pedFwdDist);
            if (!(pedFwdDist > halfLength) || !(std::abs(fwdSpeed) > 0.05f) || !((double)pedFwdDist - halfLength < closeDist)) {
                continue;
            }
            const auto pedRightDist = std::abs(((double)dy * right.y + (double)dz * right.z) + (double)dx * right.x);
            if (!(pedRightDist <= (double)halfWidth + 0.35f)) {
                continue;
            }
            CEventPotentialGetRunOver event{ vehicle };
            ped->GetIntelligence()->GetEventGroup().Add(&event, false);
            if (vehicle->m_pDriver && vehicle->m_pDriver->IsPlayer()) {
                ped->GetIntelligence()->IncrementAngerAtPlayer(2);
            }
        }
    }
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

//! Wraps the angle (extended precision) into [-PI, PI]
static double WrapAngleToPi(double angle) {
    constexpr auto pi = std::numbers::pi_v<float>;
    while (angle < -pi) {
        angle += 2.0f * pi;
    }
    while (pi < angle) {
        angle -= 2.0f * pi;
    }
    return angle;
}

//! Calculates the gas of the AI controlled boats (same code in `SteerAIBoatWithPhysicsHeadingForTarget`, `...AttackingPlayer` and `...CirclingPlayer`)
//! @returns If the boat is going too fast (so it has to slow down)
static bool CalcBoatAIGas(CVehicle* vehicle, float* pGas) {
    // x87: kept in extended precision
    const auto cruiseSpeed = (float)vehicle->m_autoPilot.m_nCruiseSpeed;
    const auto speedDiff   = (double)cruiseSpeed - std::sqrt((double)vehicle->m_vecMoveSpeed.x * vehicle->m_vecMoveSpeed.x + (double)vehicle->m_vecMoveSpeed.y * vehicle->m_vecMoveSpeed.y) * 60.0f;
    if (!(speedDiff <= 0.0)) { // (Also true for NaN)
        const auto ratio = speedDiff / cruiseSpeed;
        *pGas = ratio > 0.25f
            ? 1.0f
            : (float)(1.0 - (0.25f - ratio) * 4.0f);
        return false;
    }
    *pGas = speedDiff < -5.0f ? -0.2f : -0.1f;
    return true;
}

// 0x428DE0
void CCarCtrl::SteerAIBoatWithPhysicsAttackingPlayer(CVehicle* vehicle, float* pSteer, float* pGas, float* pBrake, bool* pHandbrake) {
    const auto& vehPos = vehicle->GetPosition();

    // Distance to the player. x87: kept in extended precision
    const auto playerPos = FindPlayerCoors();
    const auto dx        = (double)playerPos.x - vehPos.x;
    const auto dy        = (double)playerPos.y - vehPos.y;
    const auto dz        = (double)playerPos.z - vehPos.z;
    const auto distToPlayerExt = std::sqrt((dz * dz + dy * dy) + dx * dx);
    const auto distToPlayer    = (float)distToPlayerExt;

    // How far ahead (in time) the player's position is predicted
    const auto  predictionTimeExt = distToPlayerExt * 0.05f;
    const float predictionTime    = predictionTimeExt < 2.0 ? (float)predictionTimeExt : 2.0f;

    const auto fwd = GetNormalizedForward2D(vehicle);

    // Where the player will be. x87: X is rounded to float, Y isn't
    const auto& playerSpeed  = FindPlayerSpeed();
    const auto  targetX      = (float)(((double)predictionTime * playerSpeed.x) * 60.0f + FindPlayerCoors().x);
    const auto  targetYDelta = (float)(((double)predictionTime * playerSpeed.y) * 60.0f);
    const auto  targetY      = (double)targetYDelta + FindPlayerCoors().y;

    const float targetHeading = CGeneral::GetATanOfXY((float)((double)targetX - vehPos.x), (float)(targetY - vehPos.y));
    const auto  steer         = WrapAngleToPi((double)targetHeading - CGeneral::GetATanOfXY(fwd.x, fwd.y));

    CalcBoatAIGas(vehicle, pGas);
    *pBrake     = 0.0f;
    *pSteer     = (float)steer;
    *pHandbrake = false;

    if (vehicle->m_nModelIndex == MODEL_PREDATOR && distToPlayer < 40.0f && steer < 0.15f) { // BUG: `steer` is signed, so this fires if the target is anywhere to the left too
        vehicle->FireFixedMachineGuns();
    }
}

// 0x429090
void CCarCtrl::SteerAIBoatWithPhysicsCirclingPlayer(CVehicle* vehicle, float* pSteer, float* pGas, float* pBrake, bool* pHandbrake) {
    const auto& vehPos = vehicle->GetPosition();

    // Direction from the vehicle to the player (2D)
    const auto playerPos = FindPlayerCoors();
    CVector toPlayer{ (float)((double)playerPos.x - vehPos.x), (float)((double)playerPos.y - vehPos.y), 0.0f };
    NormaliseOriginal(toPlayer); // 0x59C910

    // Offset (perpendicular to the direction) to the point to circle around - the direction of circling depends on the random seed
    const auto radius = (vehicle->m_nRandomSeed & 1) ? -12.0f : 26.0f;
    const auto offX   = (float)((double)toPlayer.y * radius);
    const auto offY   = (float)((double)-toPlayer.x * radius);

    // x87: Not rounded to float
    const auto targetX = (double)offX + FindPlayerCoors().x;
    const auto targetY = (double)offY + FindPlayerCoors().y;

    const auto fwd = GetNormalizedForward2D(vehicle);

    const float targetHeading = CGeneral::GetATanOfXY((float)(targetX - vehPos.x), (float)(targetY - vehPos.y));
    const auto  steer         = WrapAngleToPi((double)targetHeading - CGeneral::GetATanOfXY(fwd.x, fwd.y));

    CalcBoatAIGas(vehicle, pGas);
    *pBrake     = 0.0f;
    *pSteer     = (float)steer;
    *pHandbrake = false;
}

// 0x428BE0
void CCarCtrl::SteerAIBoatWithPhysicsHeadingForTarget(CVehicle* vehicle, float x, float y, float* pSteer, float* pGas, float* pBrake) {
    const auto fwd = GetNormalizedForward2D(vehicle);
    const auto& pos = vehicle->GetPosition();

    // Angle needed to turn to face the target (in [-PI, PI])
    const float targetHeading = CGeneral::GetATanOfXY(x - pos.x, y - pos.y);
    auto        steer         = WrapAngleToPi((double)targetHeading - CGeneral::GetATanOfXY(fwd.x, fwd.y));

    // Clamp to [-0.5, 0.5]
    if (steer < -0.5f) {
        steer = -0.5f;
    } else if (steer > 0.5f) {
        steer = 0.5f;
    }

    const auto goingTooFast = CalcBoatAIGas(vehicle, pGas);
    *pBrake = 0.0f;
    *pSteer = goingTooFast
        ? (float)(steer * -1.0f) // Reverse the steering when slowing down
        : (float)steer;
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
void CCarCtrl::SteerAICarWithPhysicsTryingToBlockTarget_Stop(CVehicle* vehicle, float x, float y, float arg4, float arg5, float* pSteer, float* pGas, float* pBrake, bool* pHandbrake) {
    *pSteer     = 0.0f;
    *pGas       = 0.0f;
    *pBrake     = 1.0f;
    *pHandbrake = true;

    // Squared distance to the target. x87: kept in extended precision for the first comparison
    const auto& pos       = vehicle->GetPosition();
    const auto  distSqExt = ((double)pos.y - y) * ((double)pos.y - y) + ((double)pos.x - x) * ((double)pos.x - x);
    const auto  distSq    = (float)distSqExt;

    auto& autoPilot = vehicle->m_autoPilot;

    // Not there yet
    if (distSqExt > 100.0f) {
        autoPilot.m_nCarMission = autoPilot.m_nCarMission != MISSION_BLOCKCAR_HANDBRAKESTOP
            ? MISSION_BLOCKPLAYER_CLOSE
            : MISSION_BLOCKCAR_CLOSE;
        return;
    }

    const auto LeaveCar = [&] {
        CCarAI::TellOccupantsToLeaveCar(vehicle);
        autoPilot.m_nCruiseSpeed = 0;
        autoPilot.m_nCarMission  = MISSION_NONE;
    };

    // x87: Kept in extended precision
    const auto GetPlayerVehicleSpeed = [] {
        const auto& speed = FindPlayerVehicle()->m_vecMoveSpeed;
        return std::sqrt(((double)speed.x * speed.x + (double)speed.y * speed.y) + (double)speed.z * speed.z);
    };

    if (autoPilot.m_nCarMission == MISSION_BLOCKCAR_HANDBRAKESTOP) {
        if (vehicle->m_pDriver) {
            if (const auto task = vehicle->m_pDriver->GetTaskManager().GetActiveTask()) {
                if (task->GetTaskType() == TASK_COMPLEX_KILL_CRIMINAL) {
                    return;
                }
            }
        }
        if (!((double)vehicle->m_vecMoveSpeed.x * vehicle->m_vecMoveSpeed.x + (double)vehicle->m_vecMoveSpeed.y * vehicle->m_vecMoveSpeed.y < 0.0001f)) {
            return;
        }
        if (!((double)arg4 * arg4 + (double)arg5 * arg5 < 0.0004f)) {
            return;
        }
        if (!vehicle->vehicleFlags.bIsLawEnforcer) {
            return;
        }
        LeaveCar();
        return;
    }

    // The timer counts the time the player is not moving
    if (FindPlayerVehicle() && GetPlayerVehicleSpeed() < 0.05f) {
        vehicle->m_nCopsInCarTimer = (int16)(int64)((double)CTimer::GetTimeStep() * 16.666666f + (double)(uint16)vehicle->m_nCopsInCarTimer); // 0x821B40 (ftol)
    } else {
        vehicle->m_nCopsInCarTimer = 0;
    }

    if (FindPlayerVehicle()) {
        if (!FindPlayerVehicle()->IsUpsideDown()) {
            if (!(GetPlayerVehicleSpeed() < 0.05f)) {
                return;
            }
            if ((uint16)vehicle->m_nCopsInCarTimer <= 2500u) {
                return;
            }
        }
    }
    if (!vehicle->vehicleFlags.bIsLawEnforcer) {
        return;
    }
    if (!(distSq < 100.0f)) {
        return;
    }
    LeaveCar();
}

// 0x436A90
void CCarCtrl::SteerAICarWithPhysics_OnlyMission(CVehicle* vehicle, float* arg2, float* arg3, float* arg4, bool* arg5) {
    plugin::Call<0x436A90, CVehicle*, float*, float*, float*, bool*>(vehicle, arg2, arg3, arg4, arg5);
}

// 0x42AAD0
void CCarCtrl::SteerAIHeliAsPoliceHeli(CAutomobile* automobile) {
    const auto heli   = static_cast<CHeli*>(automobile);
    const auto target = heli->m_autoPilot.m_TargetEntity;

    const auto heading = CGeneral::GetATanOfXY(
        (float)((double)target->GetPosition().x - heli->GetPosition().x),
        (float)((double)target->GetPosition().y - heli->GetPosition().y)
    );

    // x87: The differences are kept in extended precision
    const auto& targetPos = target->GetPosition();
    const auto& heliPos   = heli->GetPosition();
    const auto  dy        = (double)targetPos.y - heliPos.y;
    const auto  dx        = (double)targetPos.x - heliPos.x;
    const auto  dist      = (float)std::sqrt(dy * dy + dx * dx);

    // Stay above the target, further away fly higher
    const auto altitude = targetPos.z > 6.0f ? targetPos.z : 6.0f; // 0x858B44
    heli->m_fMaxAltitude = altitude;
    if (dist > 50.0f) {
        heli->m_fMaxAltitude = altitude > 25.0f ? altitude : 25.0f; // 0x858FE8
    }

    heli->m_autoPilot.m_vecDestinationCoors = target->GetPosition();
    FlyAIHeliInCertainDirection(heli, heading, dist, true);

    if (heli->m_fHealth < 230.0f) {
        heli->m_autoPilot.m_nCarMission = MISSION_HELI_FLY_AWAY_FROM_PLAYER;
    }
}

// 0x42ACB0
void CCarCtrl::SteerAIHeliFlyingAwayFromPlayer(CAutomobile* automobile) {
    const auto heli = static_cast<CHeli*>(automobile);
    const auto& pos = heli->GetPosition();

    // Face away from the player
    const auto heading = CGeneral::GetATanOfXY(
        (float)((double)FindPlayerCoors(-1).x - pos.x),
        (float)((double)FindPlayerCoors(-1).y - pos.y)
    );
    FlyAIHeliInCertainDirection(heli, (float)((double)heading + std::numbers::pi_v<float>), 1000.0f, false);
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
    const auto heli      = static_cast<CHeli*>(automobile);
    auto&      autoPilot = heli->m_autoPilot;
    CEntity* const target = autoPilot.m_TargetEntity; // Not necessarily a vehicle

    // Fly to where the target is (or will be)
    auto& dest = autoPilot.m_vecDestinationCoors;
    dest = target->GetPosition();
    if (autoPilot.field_4A) {
        if (!target->m_matrix) {
            target->AllocateMatrix();
            target->m_placement.UpdateMatrix(target->m_matrix);
        }
        const auto  k   = (double)(int8)autoPilot.field_4A;
        const auto& fwd = target->m_matrix->GetForward();
        dest.x = (float)((double)fwd.x * k + dest.x);
        dest.y = (float)((double)(float)((double)fwd.y * k) + dest.y);
        dest.z = (float)((double)(float)(k * 0.0f) + dest.z);
    }

    const auto  heading = CGeneral::GetATanOfXY(
        (float)((double)dest.x - heli->GetPosition().x),
        (float)((double)dest.y - heli->GetPosition().y)
    );

    // x87: The differences are kept in extended precision
    const auto& targetPos = target->GetPosition();
    const auto& heliPos   = heli->GetPosition();
    const auto  dy        = (double)targetPos.y - heliPos.y;
    const auto  dx        = (double)targetPos.x - heliPos.x;
    const auto  dist      = (float)std::sqrt(dy * dy + dx * dx);

    // Stay above the target, further away fly higher
    const auto altitude = targetPos.z > 6.0f ? targetPos.z : 6.0f; // 0x858B44
    heli->m_fMaxAltitude = altitude;
    if (dist > 50.0f) {
        heli->m_fMaxAltitude = altitude > 25.0f ? altitude : 25.0f; // 0x858FE8
    }

    if (heli->m_fForcedOrientation >= 0.0f) {
        FlyAIHeliToTarget_FixedOrientation(heli, heli->m_fForcedOrientation, target->GetPosition());
    } else {
        FlyAIHeliInCertainDirection(heli, heading, dist, true);
    }

    // Once we're close to the target make it move on
    if (autoPilot.carCtrlFlags.bDoTargetCatchupCheck) {
        const auto& targetPos2 = target->GetPosition();
        const auto& heliPos2   = heli->GetPosition();
        const auto  dx2        = (double)heliPos2.x - targetPos2.x;
        const auto  dy2        = (double)heliPos2.y - targetPos2.y;
        if (std::sqrt(dy2 * dy2 + dx2 * dx2) < 25.0f) {
            if (target->GetType() == ENTITY_TYPE_VEHICLE) {
                if (target->GetStatus() == STATUS_SIMPLE || target->GetStatus() == STATUS_PHYSICS) {
                    auto& targetAP = static_cast<CVehicle*>(target)->m_autoPilot;
                    targetAP.m_nCarMission      = MISSION_CRUISE;
                    targetAP.m_nCruiseSpeed     = 100;
                    targetAP.m_nCarDrivingStyle = DRIVING_STYLE_AVOID_CARS;
                    target->SetStatus(STATUS_PHYSICS);
                }
            } else if (target->GetType() == ENTITY_TYPE_PED) {
                if (const auto task = static_cast<CPed*>(target)->GetTaskManager().GetActiveTask()) {
                    if (task->GetTaskType() == TASK_COMPLEX_WANDER) {
                        static_cast<CTaskComplexWander*>(task)->m_nMoveState = PEDMOVE_SPRINT;
                    }
                }
            }
            autoPilot.carCtrlFlags.bDoTargetCatchupCheck = false;
        }
    }

    // Stop following after a while / when the heli got damaged
    if (autoPilot.carCtrlFlags.bHeliFollowTarget && CTimer::GetTimeInMS() > heli->m_nCreationTime + 50000u) {
        autoPilot.carCtrlFlags.bHeliFollowTarget = false;
        autoPilot.m_nCarMission                  = MISSION_HELI_FLY_AWAY_FROM_PLAYER;
    }
    if (autoPilot.m_nCarMission == MISSION_HELI_NEWS_BEHAVIOUR && heli->m_fHealth < 300.0f) {
        autoPilot.m_nCarMission = MISSION_HELI_FLY_AWAY_FROM_PLAYER;
    }
}

// 0x42AEB0
void CCarCtrl::SteerAIHeliToKeepEntityInView(CAutomobile* automobile) {
    constexpr auto PI = std::numbers::pi_v<float>;

    const auto heli      = static_cast<CHeli*>(automobile);
    auto&      autoPilot = heli->m_autoPilot;
    const auto target    = autoPilot.m_TargetEntity;
    const auto targetDistance = (double)autoPilot.m_ucHeliTargetDist;

    const auto& targetPos = target->GetPosition();
    const auto& pos       = heli->GetPosition();
    const auto  heading   = CGeneral::GetATanOfXY(
        (float)((double)targetPos.x - pos.x),
        (float)((double)targetPos.y - pos.y)
    );

    // x87: kept in extended precision (only the stored copy is rounded)
    const auto dy       = (double)targetPos.y - pos.y;
    const auto dx       = (double)targetPos.x - pos.x;
    const auto dist     = std::sqrt(dy * dy + dx * dx);
    const auto distF    = (float)dist;
    if (dist > targetDistance + targetDistance) {
        // Too far away: Just follow it
        SteerAIHeliToFollowEntity(heli);
        return;
    }

    // Rotate to look at the target
    {
        const auto wantedHeading = (float)((double)heading + PI / 2.0f);
        auto       diff          = (double)wantedHeading - CGeneral::GetATanOfXY(heli->m_matrix->GetForward().x, heli->m_matrix->GetForward().y);
        while (diff > PI) {
            diff -= 2.0f * PI;
        }
        while (diff < -PI) {
            diff += 2.0f * PI;
        }
        const auto skid = diff * -1.0f;
        heli->m_fLeftRightSkid = (float)skid;
        if (skid < 1.0f) {
            if (-1.0f > skid) {
                heli->m_fLeftRightSkid = -1.0f;
            }
        } else {
            heli->m_fLeftRightSkid = 1.0f;
        }
    }

    // Fly above the target, and don't change the altitude
    heli->m_fMaxAltitude = (float)((double)target->GetPosition().z + 15.0f);
    autoPilot.m_vecDestinationCoors = target->GetPosition();
    heli->field_9AC = heli->m_fMaxAltitude;

    // Throttle
    {
        const auto altDiff = (double)heli->m_fMaxAltitude - ((double)heli->m_vecMoveSpeed.z * 100.0f + heli->GetPosition().z);
        heli->m_fAccelerationBreakStatus = 0.3f; // (Overwritten right below)
        heli->m_fAccelerationBreakStatus = (float)(altDiff * (altDiff > 0.0 ? 0.1f : 0.2f) + 0.3f);

        // Add some noise
        const auto throttle = ((double)(rand() & 0xF) - 7.0f) * 0.00200000009f + heli->m_fAccelerationBreakStatus;
        heli->m_fAccelerationBreakStatus = (float)throttle;
        if (throttle < 1.0f) {
            heli->m_fAccelerationBreakStatus = 0.0f > throttle ? 0.0f : (float)throttle;
        } else {
            heli->m_fAccelerationBreakStatus = 1.0f;
        }
    }

    // Move sideways to keep the distance
    const auto& moveSpeed = heli->m_vecMoveSpeed;
    const auto& mat       = *heli->m_matrix; // Not null checked in the original
    if (0.5f * targetDistance > distF) {
        heli->m_fSteeringLeftRight = 0.5f;
    } else if (distF > targetDistance) {
        heli->m_fSteeringLeftRight = -0.5f;
    } else {
        heli->m_fSteeringLeftRight = (float)((double)moveSpeed.z * mat.GetRight().z + (double)moveSpeed.y * mat.GetRight().y + (double)mat.GetRight().x * moveSpeed.x);
    }

    // Pitch: Brake if the target is close
    heli->m_fSteeringUpDown = 0.0f;
    if (distF < 1.5f * targetDistance) {
        heli->m_fSteeringUpDown = (float)((double)moveSpeed.z * mat.GetForward().z + (double)moveSpeed.y * mat.GetForward().y + (double)moveSpeed.x * mat.GetForward().x);
    }
}

// 0x42AD30
void CCarCtrl::SteerAIHeliToLand(CAutomobile* automobile) {
    const auto  heli = static_cast<CHeli*>(automobile);
    const auto& dest = heli->m_autoPilot.m_vecDestinationCoors;
    const auto& pos  = heli->GetPosition();

    const auto heading = CGeneral::GetATanOfXY((float)((double)dest.x - pos.x), (float)((double)dest.y - pos.y));

    // x87: The differences are kept in extended precision
    const auto dy   = (double)dest.y - pos.y;
    const auto dx   = (double)dest.x - pos.x;
    const auto dist = (float)std::sqrt(dy * dy + dx * dx);
    FlyAIHeliInCertainDirection(heli, heading, dist, true);

    // Touched down? Stop the engines
    if (!(dist < 10.0f)) {
        return;
    }
    const auto& moveSpeed = heli->m_vecMoveSpeed;
    if (!(std::sqrt((double)moveSpeed.y * moveSpeed.y + (double)moveSpeed.x * moveSpeed.x) < 0.05f)) { // 0x858C28
        return;
    }

    heli->m_fMinAltitude = 0.0f;
    heli->m_fMaxAltitude = 0.0f;

    const auto& compression = heli->m_fWheelsSuspensionCompression;
    if (compression[0] < 1.0f || compression[1] < 1.0f || compression[2] < 1.0f || compression[3] < 1.0f) {
        heli->m_fAccelerationBreakStatus = 0.0f;
        heli->m_fLeftRightSkid           = 0.0f;
        heli->m_fSteeringUpDown          = 0.0f;
        heli->m_fSteeringLeftRight       = 0.0f;
    }
}

// 0x42A630
void CCarCtrl::SteerAIHeliTowardsTargetCoors(CAutomobile* automobile) {
    const auto heli = static_cast<CHeli*>(automobile);
    const auto& dest = heli->m_autoPilot.m_vecDestinationCoors;

    // Flies to the destination with a specific orientation, if one is set
    if (heli->m_fForcedOrientation >= 0.0f) {
        FlyAIHeliToTarget_FixedOrientation(heli, heli->m_fForcedOrientation, dest);
        return;
    }

    // Otherwise face towards it
    const auto& pos     = heli->GetPosition();
    const auto  heading = CGeneral::GetATanOfXY((float)((double)dest.x - pos.x), (float)((double)dest.y - pos.y));

    // x87: The differences are kept in extended precision
    const auto dy = (double)dest.y - pos.y;
    const auto dx = (double)dest.x - pos.x;
    FlyAIHeliInCertainDirection(heli, heading, (float)std::sqrt(dy * dy + dx * dx), true);
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
    vehicle->vehicleFlags.bFireGun = false;

    if (vehicle->m_nVehicleWeaponInUse != CAR_WEAPON_NOT_USED && vehicle->m_nVehicleWeaponInUse != CAR_WEAPON_HEAVY_GUN) {
        return;
    }
    if (!target) {
        return;
    }

    // x87: The difference is kept in extended precision (only X is rounded to float for the length)
    const auto& vehPos    = vehicle->GetPosition();
    const auto& targetPos = target->GetPosition();
    CVector     dir{
        (float)((double)targetPos.x - vehPos.x),
        (float)((double)targetPos.y - vehPos.y),
        (float)((double)targetPos.z - vehPos.z)
    };
    const auto dy = (double)targetPos.y - vehPos.y;
    const auto dz = (double)targetPos.z - vehPos.z;
    if (!(std::sqrt(((double)dir.x * dir.x + dy * dy) + dz * dz) < 150.0f)) {
        return;
    }

    NormaliseOriginal(dir); // 0x59C910
    const auto& fwd = vehicle->m_matrix->GetForward(); // Not null checked in the original
    if ((double)dir.z * fwd.z + (double)dir.y * fwd.y + (double)dir.x * fwd.x > 0.8f) {
        vehicle->vehicleFlags.bFireGun = true;
    }
}

// 0x421FE0
bool CCarCtrl::ThisVehicleShouldTryNotToTurn(CVehicle* vehicle) {
    return plugin::CallAndReturn<bool, 0x421FE0, CVehicle*>(vehicle);
}

// 0x429300
void CCarCtrl::TriggerDogFightMoves(CVehicle* vehicle1, CVehicle* vehicle2) {
    auto& autoPilot = vehicle1->m_autoPilot;
    if (autoPilot.m_nTempAction != TEMPACT_NONE) {
        return;
    }

    // x87: The Y, Z differences are kept in extended precision (only X is rounded to float for the length)
    const auto& pos1 = vehicle1->GetPosition();
    const auto& pos2 = vehicle2->GetPosition();
    CVector     dir{
        (float)((double)pos1.x - pos2.x),
        (float)((double)pos1.y - pos2.y),
        (float)((double)pos1.z - pos2.z)
    };
    const auto dy = (double)pos1.y - pos2.y;
    const auto dz = (double)pos1.z - pos2.z;
    if (!(std::sqrt(((double)dir.x * dir.x + dy * dy) + dz * dz) < 70.0f)) {
        return;
    }

    NormaliseOriginal(dir); // 0x59C910

    // How much vehicle1 is in front of vehicle2
    const auto& fwd2 = vehicle2->m_matrix->GetForward(); // Not null checked in the original
    const auto  dot  = (float)((double)dir.z * fwd2.z + (double)dir.y * fwd2.y + (double)dir.x * fwd2.x);

    // Vertical distance
    const auto heightDiff = (double)vehicle1->GetPosition().z - vehicle2->GetPosition().z;
    const auto heightDist = heightDiff < 0.0 ? -heightDiff : heightDiff;
    if (!(heightDist < 15.0f)) {
        return;
    }

    switch (rand() & 0xFF) {
    case 0xC:
        if (dot > 0.0f) {
            autoPilot.SetTempAction(TEMPACT_PLANE_FLY_STRAIGHT, (rand() & 0x3FF) + 0x5DC);
        }
        break;
    case 0xD:
        if (dot > 0.0f) {
            autoPilot.SetTempAction(TEMPACT_PLANE_SHARP_LEFT, (rand() & 0x1FF) + 0x2BC);
        }
        break;
    case 0xE:
        if (dot > 0.0f) {
            autoPilot.SetTempAction(TEMPACT_PLANE_SHARP_RIGHT, (rand() & 0x1FF) + 0x2BC);
        }
        break;
    case 0xF:
        if (dot > 0.7f) {
            autoPilot.SetTempAction(TEMPACT_PLANE_FLY_UP, (rand() & 0x7FF) + 0xBB8);
        }
        break;
    }
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

namespace {
// These replicate the operation order of the original (`CMatrix::Multiply3x3` and `CMatrix::MultiplyMatrixWithVector`), as the ones in `CMatrix` do the additions
// in a different order. (x87: the sum is kept in extended precision and rounded to float only once)

// 0x59C790
CVector TransformVectorOriginal(const CMatrix& m, const CVector& v) {
    const auto &r = m.GetRight(), &f = m.GetForward(), &u = m.GetUp();
    return CVector{
        (float)(((double)u.x * v.z + (double)f.x * v.y) + (double)r.x * v.x),
        (float)(((double)u.y * v.z + (double)r.y * v.x) + (double)f.y * v.y),
        (float)(((double)u.z * v.z + (double)r.z * v.x) + (double)f.z * v.y)
    };
}

// 0x59C890
CVector TransformPointOriginal(const CMatrix& m, const CVector& v) {
    const auto &r = m.GetRight(), &f = m.GetForward(), &u = m.GetUp(), &p = m.GetPosition();
    return CVector{
        (float)((((double)u.x * v.z + (double)f.x * v.y) + (double)r.x * v.x) + p.x),
        (float)((((double)u.y * v.z + (double)r.y * v.x) + (double)f.y * v.y) + p.y),
        (float)((((double)u.z * v.z + (double)r.z * v.x) + (double)f.z * v.y) + p.z)
    };
}

constexpr auto WEAVE_ANGLE_STEP = std::bit_cast<float>(0x3DD67750u); // 0x858FAC - 6 degrees (in radians)
} // namespace

//! 0x421A50 (unnamed in the original) - Used by `WeaveForOtherCar`
//! Tests whether the 2 edges (`a0`/`a1` and `a2`/`a3` are start + direction pairs) of the box A would hit the 2 edges of box B, when A drives in `heading`'s direction at `aSpeed`,
//! and B moves with the speed of (`bSpeedX`, `bSpeedY`). The edges of the box that moves (relative to the other one) are swept along that relative motion.
//! If `aIsSmaller` is set the roles of the boxes are swapped, so A is the one moving (in the opposite direction).
static bool WouldBoxesCollide(
    float heading,
    const CVector& a0, const CVector& a1, const CVector& a2, const CVector& a3,
    const CVector& b0, const CVector& b1, const CVector& b2, const CVector& b3,
    float bSpeedX, float bSpeedY,
    float aSpeed,
    bool aIsSmaller
) {
    // x87: cos/sin and the differences are kept in extended precision
    const auto relSpeedX = (float)(((double)bSpeedX - std::cos((double)heading) * aSpeed) * 100.0f);
    const auto relSpeedY = (float)(((double)bSpeedY - std::sin((double)heading) * aSpeed) * 100.0f);

    // The relative motion applied to the moving box (swapped if the roles are)
    const auto relX = aIsSmaller ? -relSpeedX : relSpeedX;
    const auto relY = aIsSmaller ? -relSpeedY : relSpeedY;

    // The box that moves, and the one that stands still (relative to the moving one)
    const CVector &m0 = aIsSmaller ? a0 : b0, &m1 = aIsSmaller ? a1 : b1, &m2 = aIsSmaller ? a2 : b2, &m3 = aIsSmaller ? a3 : b3;
    const CVector &s0 = aIsSmaller ? b0 : a0, &s1 = aIsSmaller ? b1 : a1, &s2 = aIsSmaller ? b2 : a2, &s3 = aIsSmaller ? b3 : a3;

    // Tests if the (moving) edge (start `p`, direction `d`) or any of the other 3 sides of the quad it sweeps hits the static edge (`ss`, `sd`)
    const auto TestSweptEdge = [&](const CVector& p, const CVector& d) {
        // All of these are rounded to float, as they're stored in the original
        const auto qX  = (float)((double)p.x + d.x),          qY  = (float)((double)p.y + d.y);    // End of the edge
        const auto pmX = (float)((double)relX + p.x),         pmY = (float)((double)relY + p.y);   // Start of the moved edge
        const auto qmX = (float)((double)qX + relX),          qmY = (float)((double)qY + relY);    // End of the moved edge

        const auto Test = [&](const CVector& ss, const CVector& sd, float startX, float startY, float dirX, float dirY) {
            return CCollision::Test2DLineAgainst2DLine(ss.x, ss.y, sd.x, sd.y, startX, startY, dirX, dirY); // 0x4138D0
        };
        const auto TestAgainst = [&](const CVector& ss, const CVector& sd) {
            return Test(ss, sd, p.x, p.y, (float)((double)qX - p.x), (float)((double)qY - p.y))                    // The edge
                || Test(ss, sd, qX, qY, (float)((double)pmX - qX), (float)((double)pmY - qY))                      // From the end to the start of the moved edge
                || Test(ss, sd, pmX, pmY, (float)((double)qmX - pmX), (float)((double)qmY - pmY))                  // The moved edge
                || Test(ss, sd, qmX, qmY, (float)((double)p.x - qmX), (float)((double)p.y - qmY));                 // From the end of the moved edge to the start of the edge
        };
        return TestAgainst(s0, s1) || TestAgainst(s2, s3);
    };

    return TestSweptEdge(m0, m1) || TestSweptEdge(m2, m3);
}

// 0x426BC0
void CCarCtrl::WeaveForObject(CEntity* entity, CVehicle* vehicle, float* pLowerAngle, float* pUpperAngle) {
    // Offset of the "pole" of the object (in its local space) that the vehicle should avoid
    float offX, offY;
    const auto modelId = entity->m_nModelIndex;
    if (modelId == ModelIndices::MI_TRAFFICLIGHTS) {
        offX = 2.957f;
        offY = 0.147f;
    } else if (modelId == ModelIndices::MI_SINGLESTREETLIGHTS1) {
        offX = 0.744f;
        offY = 0.0f;
    } else if (modelId == ModelIndices::MI_SINGLESTREETLIGHTS2) {
        offX = 0.043f;
        offY = 0.0f;
    } else if (modelId == ModelIndices::MI_SINGLESTREETLIGHTS3) {
        offX = 1.143f;
        offY = 0.145f;
    } else if (modelId == ModelIndices::MI_DOUBLESTREETLIGHTS) {
        offX = 0.0f;
        offY = -0.048f;
    } else {
        if (!CModelInfo::GetModelInfo(modelId)->SwaysInWind()) {
            return;
        }
        offX = 0.0f;
        offY = 0.0f;
    }

    const CVector entityPos = entity->GetPosition(); // Has to be before the matrix is (possibly) allocated
    const auto&   entityMat = entity->GetMatrix();
    const auto&   vehPos    = vehicle->GetPosition();

    // x87: the position is kept in extended precision (only X is rounded to float, Y isn't)
    const auto poleX = (float)(((double)offY * entityMat.GetForward().x + (double)offX * entityMat.GetRight().x) + entityPos.x);
    const auto poleY = ((double)offX * entityMat.GetRight().y + (double)offY * entityMat.GetForward().y) + entityPos.y;

    const auto toPoleX = (float)((double)poleX - vehPos.x);
    const auto toPoleY = (float)(poleY - vehPos.y);

    // x87: The result of this is kept in extended precision (the function leaves it in ST0)
    const double heading = CGeneral::GetATanOfXY(toPoleX, toPoleY); // 0x53CC70

    // Half of the angle (as seen from the vehicle) the pole (and the car) takes up
    const auto poleAngleWidth = (float)(((double)CModelInfo::GetModelInfo(vehicle->m_nModelIndex)->GetColModel()->m_boundBox.m_vecMax.x * 2.4f + 0.3f) / std::sqrt((double)toPoleY * toPoleY + (double)toPoleX * toPoleX));
    const auto halfWidth      = poleAngleWidth * 0.5f;

    const auto pi = std::numbers::pi_v<float>;

    // Makes the angle (in extended precision) to be in -PI to PI
    const auto Wrap = [&](double angle) {
        while (angle < -pi) {
            angle += 2.0f * pi;
        }
        while (pi < angle) {
            angle -= 2.0f * pi;
        }
        return angle;
    };

    // Lower end
    {
        const auto diff = Wrap(heading - *pLowerAngle);
        if ((diff < 0.0 ? -diff : diff) < halfWidth) {
            const double lower = heading - halfWidth;
            *pLowerAngle = (float)lower;
            if (lower < -pi) {
                double angle = *pLowerAngle;
                do {
                    angle += 2.0f * pi;
                } while (angle < -pi);
                *pLowerAngle = (float)angle;
            }
        }
    }

    // Upper end
    {
        const auto diff = Wrap(heading - *pUpperAngle);
        if ((diff < 0.0 ? -diff : diff) < halfWidth) {
            const double upper = heading + halfWidth;
            *pUpperAngle = (float)upper;
            if (upper > pi) {
                double angle = *pUpperAngle;
                do {
                    angle -= 2.0f * pi;
                } while (angle > pi);
                *pUpperAngle = (float)angle;
            }
        }
    }
}

// 0x426350
void CCarCtrl::WeaveForOtherCar(CEntity* entity, CVehicle* vehicle, float* pLowerAngle, float* pUpperAngle) {
    // Cars that are supposed to be rammed/followed/escorted by `vehicle` (or the other way around) don't need to be avoided
    const auto  mission = vehicle->m_autoPilot.m_nCarMission;
    const auto* target  = vehicle->m_autoPilot.m_TargetEntity;
    if (mission == MISSION_RAMPLAYER_CLOSE && entity == FindPlayerVehicle()) {
        return;
    }
    if (mission == MISSION_RAMCAR_CLOSE && entity == target) {
        return;
    }
    if (mission == MISSION_FOLLOWCAR_CLOSE) {
        if (entity == target) {
            return;
        }
        if (entity->GetIsTypeVehicle() && entity->AsVehicle()->vehicleFlags.bPartOfConvoy) {
            return;
        }
    }
    if (mission == MISSION_KILLPED_CLOSE) {
        if (entity->GetIsTypePed() && entity->AsPed()->bInVehicle && entity->AsPed()->m_pVehicle == target) {
            return;
        }
    }
    // Note: `entity` is assumed to be a vehicle from here on
    const auto otherMission = entity->AsVehicle()->m_autoPilot.m_nCarMission;
    if (otherMission == MISSION_PROTECTION_REAR || otherMission == MISSION_PROTECTION_FRONT
        || otherMission == MISSION_ESCORT_LEFT || otherMission == MISSION_ESCORT_RIGHT || otherMission == MISSION_ESCORT_REAR || otherMission == MISSION_ESCORT_FRONT
    ) {
        if (entity->AsVehicle()->m_autoPilot.m_TargetEntity == vehicle) {
            return;
        }
    }

    // Note: Both of the matrices are used directly (not null checked)
    const auto& vehMat = *vehicle->m_matrix;
    const auto& entMat = *entity->m_matrix;

    const auto& entPos = entity->GetPosition();
    const auto& vehPos = vehicle->GetPosition();

    // Vector from this vehicle to the other. x87: `toEntYExt` is kept in extended precision for the first use
    const auto toEntX    = (float)((double)entPos.x - vehPos.x);
    const auto toEntYExt = (double)entPos.y - vehPos.y;
    const auto toEntY    = (float)toEntYExt;

    // Is the other car in front of this one?
    const auto fwdDot = toEntYExt * vehMat.GetForward().y + (double)toEntX * vehMat.GetForward().x;
    if (fwdDot < 0.0) {
        return;
    }
    const auto rightDot = (float)((double)toEntY * vehMat.GetRight().y + (double)toEntX * vehMat.GetRight().x);

    // Edges of this vehicle's box (a bit bigger than the actual bounding box) that are on the side of the other car (start + direction of each)
    const auto& vehBB = CModelInfo::GetModelInfo(vehicle->m_nModelIndex)->GetColModel()->m_boundBox;
    const CVector vehEdge1Start = TransformPointOriginal(vehMat, {
        (float)((double)vehBB.m_vecMin.x - 0.2f),
        fwdDot > 0.0 ? (float)((double)vehBB.m_vecMax.y + 0.2f) : (float)((double)vehBB.m_vecMin.y - 0.2f),
        0.0f
    });
    const CVector vehEdge1Dir = TransformVectorOriginal(vehMat, { (float)((((double)vehBB.m_vecMax.x + 0.2f) - vehBB.m_vecMin.x) - 0.2f), 0.0f, 0.0f });
    const CVector vehEdge2Start = TransformPointOriginal(vehMat, {
        rightDot > 0.0f ? (float)((double)vehBB.m_vecMax.x + 0.2f) : (float)((double)vehBB.m_vecMin.x - 0.2f),
        (float)((double)vehBB.m_vecMin.y - 0.2f),
        0.0f
    });
    const CVector vehEdge2Dir = TransformVectorOriginal(vehMat, { 0.0f, (float)((((double)vehBB.m_vecMax.y + 0.2f) - vehBB.m_vecMin.y) - 0.2f), 0.0f });

    // Same for the other car
    const auto entFwdDot   = (double)toEntY * entMat.GetForward().y + (double)toEntX * entMat.GetForward().x;
    const auto entRightDot = (float)((double)toEntY * entMat.GetRight().y + (double)toEntX * entMat.GetRight().x);

    const auto& entBB = CModelInfo::GetModelInfo(entity->m_nModelIndex)->GetColModel()->m_boundBox;
    const CVector entEdge1Start = TransformPointOriginal(entMat, {
        (float)((double)entBB.m_vecMin.x - 0.2f),
        entFwdDot < 0.0 ? (float)((double)entBB.m_vecMax.y + 0.2f) : (float)((double)entBB.m_vecMin.y - 0.2f),
        0.0f
    });
    const CVector entEdge1Dir = TransformVectorOriginal(entMat, { (float)((((double)entBB.m_vecMax.x + 0.2f) - entBB.m_vecMin.x) - 0.2f), 0.0f, 0.0f });
    const CVector entEdge2Start = TransformPointOriginal(entMat, {
        entRightDot < 0.0f ? (float)((double)entBB.m_vecMax.x + 0.2f) : (float)((double)entBB.m_vecMin.x - 0.2f),
        (float)((double)entBB.m_vecMin.y - 0.2f),
        0.0f
    });
    const CVector entEdge2Dir = TransformVectorOriginal(entMat, { 0.0f, (float)((((double)entBB.m_vecMax.y + 0.2f) - entBB.m_vecMin.y) - 0.2f), 0.0f });

    // If this vehicle is smaller it's the one that "moves" (in the test)
    const bool vehIsSmaller = vehBB.m_vecMax.x < entBB.m_vecMax.x;

    // x87: kept in extended precision until it's rounded to float
    const auto vehSpeed2D = (float)std::sqrt((double)vehicle->m_vecMoveSpeed.x * vehicle->m_vecMoveSpeed.x + (double)vehicle->m_vecMoveSpeed.y * vehicle->m_vecMoveSpeed.y);
    const auto& entSpeed  = entity->AsVehicle()->m_vecMoveSpeed;

    const auto WouldCollide = [&](float heading) {
        return WouldBoxesCollide(
            heading,
            vehEdge1Start, vehEdge1Dir, vehEdge2Start, vehEdge2Dir,
            entEdge1Start, entEdge1Dir, entEdge2Start, entEdge2Dir,
            entSpeed.x, entSpeed.y,
            vehSpeed2D,
            vehIsSmaller
        );
    };

    const auto pi = std::numbers::pi_v<float>;

    // Move the lower angle down (until no collision, or at most 8 times)
    for (auto i = 0; i < 8; i++) {
        if (!WouldCollide(*pLowerAngle)) {
            break;
        }
        const double angle = (double)*pLowerAngle - WEAVE_ANGLE_STEP; // x87: not rounded for the comparison
        *pLowerAngle = (float)angle;
        if (angle < 0.0) {
            *pLowerAngle = (float)(angle + 2.0f * pi);
        }
    }

    // Move the upper angle up
    for (auto i = 0; i < 8; i++) {
        if (!WouldCollide(*pUpperAngle)) {
            break;
        }
        const double angle = (double)*pUpperAngle + WEAVE_ANGLE_STEP;
        *pUpperAngle = (float)angle;
        if (angle > 2.0f * pi) {
            *pUpperAngle = (float)(angle - 2.0f * pi);
        }
    }
}

// 0x42D680
void CCarCtrl::WeaveThroughCarsSectorList(CPtrListDoubleLink<CVehicle*>& ptrList, CVehicle* vehicle, CPhysical* physical, float minX, float minY, float maxX, float maxY, float* pLowerAngle, float* pUpperAngle) {
    // The original stores the next node before processing the current one
    for (auto it = ptrList.begin(); it != ptrList.end();) {
        CVehicle* const other = *it;
        ++it;

        if (other->IsScanCodeCurrent() || !other->m_bUsesCollision || other == physical) {
            continue;
        }
        other->SetCurrentScanCode();

        CVector centre;
        other->GetBoundCentre(centre); // 0x534250
        if (centre.x <= minX || centre.x >= maxX || centre.y <= minY || centre.y >= maxY) {
            continue;
        }

        {
            const auto zDiff = (double)other->GetPosition().z - vehicle->GetPosition().z; // x87: kept in extended precision
            if ((zDiff >= 0.0 ? zDiff : -zDiff) >= 8.0f) {
                continue;
            }
        }

        // Convoy vehicles don't avoid each other
        if (other == vehicle || (vehicle->vehicleFlags.bPartOfConvoy && other->vehicleFlags.bPartOfConvoy)) {
            continue;
        }

        WeaveForOtherCar(other, vehicle, pLowerAngle, pUpperAngle); // 0x426350
    }
}

// 0x42D7E0
void CCarCtrl::WeaveThroughPedsSectorList(CPtrListDoubleLink<CPed*>& ptrList, CVehicle* vehicle, CPhysical* physical, float minX, float minY, float maxX, float maxY, float* pLowerAngle, float* pUpperAngle) {
    // The original stores the next node before processing the current one
    for (auto it = ptrList.begin(); it != ptrList.end();) {
        CPed* const ped = *it;
        ++it;

        if (ped->IsScanCodeCurrent() || !ped->m_bUsesCollision || ped == physical) {
            continue;
        }
        ped->SetCurrentScanCode();

        const auto& pedPos = ped->GetPosition();
        if (pedPos.x <= minX || pedPos.x >= maxX || pedPos.y <= minY || pedPos.y >= maxY) {
            continue;
        }

        {
            const auto zDiff = (double)ped->GetPosition().z - vehicle->GetPosition().z; // x87: kept in extended precision
            if ((zDiff >= 0.0 ? zDiff : -zDiff) >= 4.0f) {
                continue;
            }
        }

        // Peds standing on/attached to the vehicle aren't avoided
        if (ped->m_pContactEntity == vehicle || ped->m_pAttachedTo == vehicle) {
            continue;
        }

        // 0x426970 - `CCarCtrl::WeaveForPed` (isn't in the headers/inventory, so it's called directly)
        plugin::Call<0x426970, CPed*, CVehicle*, float*, float*>(ped, vehicle, pLowerAngle, pUpperAngle);
    }
}

// 0x42D950
void CCarCtrl::WeaveThroughObjectsSectorList(CPtrListDoubleLink<CObject*>& ptrList, CVehicle* vehicle, float minX, float minY, float maxX, float maxY, float* pLowerAngle, float* pUpperAngle) {
    // The original stores the next node before processing the current one
    for (auto it = ptrList.begin(); it != ptrList.end();) {
        CObject* const obj = *it;
        ++it;

        if (obj->IsScanCodeCurrent() || !obj->m_bUsesCollision) {
            continue;
        }
        obj->SetCurrentScanCode();

        const auto& objPos = obj->GetPosition();
        if (objPos.x <= minX || objPos.x >= maxX || objPos.y <= minY || objPos.y >= maxY) {
            continue;
        }

        {
            const auto zDiff = (double)obj->GetPosition().z - vehicle->GetPosition().z; // x87: kept in extended precision
            if ((zDiff >= 0.0 ? zDiff : -zDiff) >= 8.0f) {
                continue;
            }
        }

        // Only the objects that are standing upright
        if (obj->GetMatrix().GetUp().z <= 0.9f) {
            continue;
        }

        WeaveForObject(obj, vehicle, pLowerAngle, pUpperAngle); // 0x426BC0
    }
}

// 0x427FE0
float CCarCtrl::FindMaxSteerAngle(CVehicle* veh) {
    return std::clamp(0.9f - veh->GetMoveSpeed().Magnitude(), 0.2f, 0.7f);
}

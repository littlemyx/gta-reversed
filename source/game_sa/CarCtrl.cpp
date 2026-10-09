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
#include "TaskTypes/TaskComplexLeaveAnyCar.h"
#include "VehicleRecording.h"
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

//! `CrossProduct` (0x59C730) - the products stay in the FPU registers (extended precision), the shared `CrossProduct` rounds them to float
static CVector CrossProductOriginal(const CVector& a, const CVector& b) {
    return {
        (float)((double)b.z * a.y - (double)a.z * b.y),
        (float)((double)a.z * b.x - (double)b.z * a.x),
        (float)((double)a.x * b.y - (double)b.x * a.y),
    };
}

//! 0x4082C0 - `CVector::Magnitude`. The sum of squares and the square root stay in the FPU registers (extended precision)
static double MagnitudeOriginal(const CVector& v) {
    return std::sqrt(((double)v.x * v.x + (double)v.y * v.y) + (double)v.z * v.z);
}

//! 0x40FDB0 - `DotProduct`. The sum is returned in the FPU register (extended precision)
static double DotProductOriginal(const CVector& a, const CVector& b) {
    return ((double)a.z * b.z + (double)a.y * b.y) + (double)a.x * b.x;
}

//! The bounding box of the collision model of the vehicle's model (the original reads it through the model info pointer table)
static const CBoundingBox& GetModelBoundBox(const CVehicle* vehicle) {
    return CModelInfo::GetModelInfo(vehicle->m_nModelIndex)->GetColModel()->m_boundBox;
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
} // namespace

void CCarCtrl::InjectHooks()
{
    RH_ScopedClass(CCarCtrl);
    RH_ScopedCategoryGlobal();

    using namespace ReversibleHooks;
    RH_ScopedInstall(Init, 0x4212E0);
    RH_ScopedInstall(ReInit, 0x4213B0);
    RH_ScopedInstall(InitSequence, 0x421740);
    RH_ScopedInstall(FindSequenceElement, 0x421770);
    RH_ScopedInstall(ThisVehicleShouldTryNotToTurn, 0x421FE0);
    RH_ScopedInstall(FindPathDirection, 0x422090);
    RH_ScopedInstall(FindGhostRoadHeight, 0x422370);
    RH_ScopedInstall(FindSpeedMultiplier, 0x4224E0);
    RH_ScopedInstall(StopCarIfNodesAreInvalid, 0x422590);
    RH_ScopedInstall(FindPercDependingOnDistToLink, 0x422620);
    RH_ScopedInstall(TestCollisionBetween2MovingRects, 0x425B30);
    RH_ScopedInstall(TestCollisionBetween2MovingRects_OnlyFrontBumper, 0x425F70);
    RH_ScopedInstall(WeaveForPed, 0x426970);
    RH_ScopedInstall(PickNextNodeToChaseCar, 0x426EF0);
    RH_ScopedInstall(IsThisAnAppropriateNode, 0x42DAB0);
    RH_ScopedInstall(FindAngleToWeaveThroughTraffic, 0x4325C0);
    RH_ScopedInstall(FindMaximumSpeedForThisCarInTraffic, 0x434400);
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
    RH_ScopedInstall(SlowCarDownForCarsSectorList, 0x432420);
    RH_ScopedInstall(SlowCarDownForObject, 0x426220);
    RH_ScopedInstall(SlowCarOnRailsDownForTrafficAndLights, 0x434790);
    RH_ScopedInstall(FindMaxSteerAngle, 0x427FE0);
    RH_ScopedInstall(GenerateRandomCars, 0x4341C0);
    RH_ScopedInstall(GenerateOneRandomCar, 0x430050);
    RH_ScopedInstall(FindSpeedMultiplierWithSpeedFromNodes, 0x424130);
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
    RH_ScopedInstall(DragCarToPoint, 0x42EC90);
    RH_ScopedInstall(GetAIPlaneToDoDogFightAgainstPlayer, 0x42F370);
    RH_ScopedInstall(GetAIHeliToAttackPlayer, 0x42F3C0);
    RH_ScopedInstall(JoinCarWithRoadSystem, 0x42F5A0);
    RH_ScopedInstall(GenerateEmergencyServicesCar, 0x42F9C0);
    RH_ScopedInstall(ReconsiderRoute, 0x42FC40);
    RH_ScopedInstall(SteerAICarWithPhysicsFollowPreRecordedPath, 0x432DD0);
    RH_ScopedInstall(SteerAICarWithPhysicsHeadingForTarget, 0x433280);
    RH_ScopedInstall(UpdateCarOnRails, 0x436540);
    RH_ScopedInstall(SteerAICarWithPhysicsFollowPath, 0x434900);
    RH_ScopedInstall(SteerAICarWithPhysicsFollowPath_Racing, 0x435830);
    RH_ScopedInstall(SteerAICarWithPhysics_OnlyMission, 0x436A90);
    RH_ScopedInstall(SteerAICarWithPhysics, 0x437C20);
    RH_ScopedInstall(SteerAICarWithPhysicsTryingToBlockTarget, 0x4335E0);
    RH_ScopedInstall(SteerAICarTowardsPointInEscort, 0x4336D0);
    RH_ScopedInstall(SteerAICarParkParallel, 0x433BA0);
    RH_ScopedInstall(SteerAICarParkPerpendicular, 0x433EA0);
    RH_ScopedInstall(SetCoordsOfScriptCar, 0x4342A0);
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
    // Note: The vehicle's matrix is used directly (not null checked) until the position is set at the end
    float rangeAbove = 3.0f; // How far above/below the wheels' position the ground is looked for
    float rangeBelow = 3.0f;

    // Length of the vehicle (wheelbase). x87: kept in extended precision for the first uses
    const auto& bb   = CModelInfo::GetModelInfo(vehicle->m_nModelIndex)->GetColModel()->m_boundBox;
    const auto  lenExt = ((double)bb.m_vecMax.y - bb.m_vecMin.y) * 0.95f; // 0x858EF0
    const auto  len    = (float)lenExt;
    if (vehicle->m_autoPilot.field_51 > 0x10) {
        rangeAbove = rangeBelow = 100.0f;
    }

    const CVector oldFwd = vehicle->m_matrix->GetForward();
    const auto&   fwd    = oldFwd;

    const float targetX = pos->x;
    const float targetY = pos->y;

    // Where the front wheels are (the wheelbase's half in front of the vehicle's position)
    const auto frontX = (float)((double)vehicle->GetPosition().x - ((double)fwd.x * lenExt) * 0.5f);
    const auto frontY = (float)((double)vehicle->GetPosition().y - ((double)fwd.y * lenExt) * 0.5f);

    // The point (on the line from the front wheels, to the target) where the rear wheels have to be
    float rearX, rearY;
    {
        const auto fwdX15 = (double)fwd.x * 1.5f;
        const auto fwdY15 = (float)((double)fwd.y * 1.5f);
        const CColLine   line{
            CVector{ frontX, frontY, 0.0f },
            CVector{ (float)(fwdX15 + frontX), (float)((double)fwdY15 + frontY), 0.0f }
        };
        const CColSphere sphere{ CSphere{ len, CVector{ targetX, targetY, 0.0f } } };
        CColPoint colPoint;
        float     depth = 1.0f;
        if (CCollision::ProcessLineSphere(line, sphere, colPoint, depth)) { // 0x412AA0
            rearX = colPoint.m_vecPoint.x;
            rearY = colPoint.m_vecPoint.y;
        } else {
            // x87: kept in extended precision
            const auto dy    = (double)targetY - frontY;
            const auto dx    = (double)targetX - frontX;
            const auto ratio = (double)len / std::sqrt(dx * dx + dy * dy);
            rearX = (float)(((double)frontX - targetX) * ratio + targetX);
            rearY = (float)(((double)frontY - targetY) * ratio + targetY);
        }
    }

    // Height of the wheels over the ground (the Z of the front is the base of the rear's as well)
    const auto halfLenZExt = ((double)fwd.z * len) * 0.5f;
    const auto halfLenZ    = (float)halfLenZExt;

    // Finds the Z of the ground at the given position. `centerZExt` is the Z the search is based on (kept in extended precision as in the original)
    const auto FindGroundZ = [&](float x, float y, double centerZExt, CStoredCollPoly* poly, float oldGroundZ) -> float {
        const auto centerZ = (float)centerZExt;
        const auto lower   = (float)(centerZExt - rangeBelow);
        const auto upper   = (float)((double)centerZ + rangeAbove);

        CVector    origin{ x, y, upper };
        CColPoint  colPoint;
        CEntity*   entity{};
        if (CCollision::IsStoredPolyStillValidVerticalLine(origin, lower, colPoint, poly)) { // 0x414D70
            return colPoint.m_vecPoint.z;
        }

        origin.z = (float)((double)centerZ + 1.5f);
        if (!CWorld::ProcessVerticalLine(origin, (float)((double)centerZ - 2.0f), colPoint, entity, true, false, false, false, false, false, poly)) { // 0x5674E0
            origin.z = upper;
            if (!CWorld::ProcessVerticalLine(origin, lower, colPoint, entity, true, false, false, false, false, false, poly)) {
                return oldGroundZ;
            }
        }

        vehicle->m_pEntityWeAreOn = entity;
        vehicle->m_bTunnel           = entity->m_bTunnel;
        vehicle->m_bTunnelTransition = entity->m_bTunnelTransition;
        vehicle->m_autoPilot.field_51 = 0;
        return colPoint.m_vecPoint.z;
    };

    // Front
    vehicle->m_autoPilot.field_51++;
    const auto frontGroundZ = FindGroundZ(targetX, targetY, halfLenZExt + vehicle->GetPosition().z, &vehicle->m_FrontCollPoly, vehicle->m_fVehicleFrontGroundZ);
    vehicle->m_fVehicleFrontGroundZ = frontGroundZ;

    // Rear
    const auto rearGroundZ = FindGroundZ(rearX, rearY, (double)vehicle->GetPosition().z - halfLenZ, &vehicle->m_RearCollPoly, vehicle->m_fVehicleRearGroundZ);
    vehicle->m_fVehicleRearGroundZ = rearGroundZ;

    // Orientation. x87: kept in extended precision
    const auto invLen   = 1.0f / (double)len;
    const auto pitch    = std::atan2(((double)frontGroundZ - rearGroundZ) * invLen, 1.0);
    const auto cosPitch = std::cos(pitch);
    auto&      mat      = *vehicle->m_matrix;

    mat.GetRight().x = (float)(((double)targetY - rearY) * invLen);
    mat.GetRight().y = (float)(((double)targetX - rearX) * ((double)-1.0f / len));
    mat.GetRight().z = 0.0f;

    mat.GetForward().x = (float)-(cosPitch * mat.GetRight().y);
    mat.GetForward().y = (float)(cosPitch * mat.GetRight().x);
    mat.GetForward().z = (float)std::sin(pitch);

    mat.GetUp() = CrossProductOriginal(mat.GetRight(), mat.GetForward()); // 0x59C730

    // New position: the middle of the 2 wheels pairs, a bit above the ground
    const auto groundZSum = (float)((double)rearGroundZ + frontGroundZ);
    const auto centerX    = (float)(((double)targetX + rearX) * 0.5f);
    const auto centerY    = (float)(((double)targetY + rearY) * 0.5f);
    const auto centerZ    = (float)((double)groundZSum * 0.5f);

    const auto posZ = (float)((double)vehicle->GetHeightAboveRoad() + centerZ);
    vehicle->UpdateLightingFromStoredPolys(); // 0x6D0CC0

    vehicle->GetPosition() = CVector{ centerX, centerY, posZ };

    // Steer into the direction we turned to
    const auto steerCross = CrossProductOriginal(mat.GetForward(), oldFwd); // 0x59C730
    const auto steer      = (double)steerCross.z * -10.0f; // 0x859004
    if (!(steer < 0.5f)) {
        vehicle->m_fSteerAngle = 0.5f;
    } else if (steer > -0.5f) {
        vehicle->m_fSteerAngle = (float)steer;
    } else {
        vehicle->m_fSteerAngle = -0.5f;
    }
}

namespace {
//! The range of the (repeat) sectors that are overlapped by the given area (as used by the traffic AI)
struct TrafficSectorRange {
    int32 minX, minY, maxX, maxY;
};

//! x87: The sector coordinate is kept in extended precision for the first `floor` (that is used for the check against the limits),
//! then the rounded (to float) copy is used for the 2nd one
TrafficSectorRange GetTrafficSectorRange(float minX, float minY, float maxX, float maxY) {
    const auto GetSector = [](float v) {
        const double sector = (double)v * 0.02f + 60.0f; // 0x858B38, 0x858B34
        return std::pair{ sector, (float)sector };
    };
    const auto GetMin = [&](float v) -> int32 {
        const auto [ext, rounded] = GetSector(v);
        return (int32)std::floor(ext) > 0 ? (int32)std::floor((double)rounded) : 0;
    };
    const auto GetMax = [&](float v) -> int32 {
        const auto [ext, rounded] = GetSector(v);
        return (int32)std::floor(ext) < 119 ? (int32)std::floor((double)rounded) : 119;
    };
    return { GetMin(minX), GetMin(minY), GetMax(maxX), GetMax(maxY) };
}
} // namespace

// 0x4325C0
float CCarCtrl::FindAngleToWeaveThroughTraffic(CVehicle* vehicle, CPhysical* physical, float targetAngle, float heading, float distMult) {
    // x87: The search distance is kept in extended precision
    const auto& moveSpeed = vehicle->m_vecMoveSpeed;
    const auto  speed     = std::sqrt((double)moveSpeed.x * moveSpeed.x + (double)moveSpeed.y * moveSpeed.y);
    double      lookAhead = speed * 2.5f + 1.0f; // 0x858FA0
    if (!(lookAhead < 2.0f)) {
        lookAhead = 2.0f;
    }
    const double searchDist = lookAhead * distMult * 12.0f; // 0x858CCC

    const auto& pos  = vehicle->GetPosition();
    const auto  minX = (float)((double)pos.x - searchDist);
    const auto  maxX = (float)(searchDist + pos.x);
    const auto  minY = (float)((double)pos.y - searchDist);
    const auto  maxY = (float)(searchDist + pos.y);

    const auto sectors = GetTrafficSectorRange(minX, minY, maxX, maxY);

    CWorld::AdvanceCurrentScanCode();

    // The range of angles that are free to go to (starts as just the target angle)
    float lowerAngle = targetAngle;
    float upperAngle = targetAngle;

    // Repeat until the angles don't change anymore
    float prevLower = std::bit_cast<float>(0xC61C3F9Au);
    float prevUpper = prevLower;
    while (!(prevLower == lowerAngle && prevUpper == upperAngle)) {
        prevLower = lowerAngle;
        prevUpper = upperAngle;

        for (auto y = sectors.minY; y <= sectors.maxY; y++) {
            for (auto x = sectors.minX; x <= sectors.maxX; x++) {
                auto& sector = CWorld::GetRepeatSector(x, y);
                WeaveThroughCarsSectorList(sector.Vehicles, vehicle, physical, minX, minY, maxX, maxY, &lowerAngle, &upperAngle); // 0x42D680
                if (vehicle->m_autoPilot.m_nCarDrivingStyle != DRIVING_STYLE_DRIVINGMODE_AVOIDCARS_STOPFORPEDS_OBEYLIGHTS) {
                    WeaveThroughPedsSectorList(sector.Peds, vehicle, physical, minX, minY, maxX, maxY, &lowerAngle, &upperAngle); // 0x42D7E0
                }
                WeaveThroughObjectsSectorList(sector.Objects, vehicle, minX, minY, maxX, maxY, &lowerAngle, &upperAngle); // 0x42D950
                WeaveThroughCarsSectorList(sector.Vehicles, vehicle, physical, minX, minY, maxX, maxY, &lowerAngle, &upperAngle); // 0x42D680 (Again, but the vehicles were already scanned)
            }
        }
    }

    const auto pi = std::numbers::pi_v<float>;

    // Makes the angle (in extended precision) to be in -PI to PI
    const auto Wrap = [&](double angle) {
        while (angle < -pi) {
            angle += 2.0f * pi;
        }
        while (angle > pi) {
            angle -= 2.0f * pi;
        }
        return angle;
    };

    // The angle between the target angle and the heading, then the middle of those two
    double middle = Wrap((double)heading - targetAngle) * 0.5f + targetAngle;
    auto   middleF = (float)middle; // Stored as float (and again, after each wrapping)
    if (middle < -pi) {
        do {
            middle += 2.0f * pi;
        } while (middle < -pi);
        middleF = (float)middle;
    }
    if (middle > pi) {
        do {
            middle -= 2.0f * pi;
        } while (middle > pi);
        middleF = (float)middle;
    }

    // How far (absolute) are the ends of the free angle range from the middle
    const auto lowerDiff = std::abs(Wrap((double)lowerAngle - middle));
    const auto upperDiff = std::abs(Wrap((double)upperAngle - middleF));

    if (lowerDiff > std::numbers::pi_v<float> / 2.0f && upperDiff > std::numbers::pi_v<float> / 2.0f) { // 0x858FE4
        return middleF;
    }
    if (std::abs(lowerDiff - upperDiff) < 0.08f) { // 0x859018
        return upperAngle;
    }
    return lowerDiff < upperDiff ? lowerAngle : upperAngle;
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

//! 0x434400 - The original returns the result in a FPU register (not rounded to float)
static double FindMaximumSpeedForThisCarInTrafficOriginal(CVehicle* vehicle) {
    auto& ap = vehicle->m_autoPilot;

    const auto cruiseSpeed = (double)ap.m_nCruiseSpeed * ap.m_SpeedMult; // x87: kept in extended precision (exact)

    switch (ap.m_nCarDrivingStyle) {
    case DRIVING_STYLE_AVOID_CARS:
    case DRIVING_STYLE_PLOUGH_THROUGH:
    case DRIVING_STYLE_DRIVINGMODE_AVOIDCARS_OBEYLIGHTS:
        return cruiseSpeed;
    default:
        break;
    }

    const auto& pos  = vehicle->GetPosition();
    const auto  minX = (float)((double)pos.x - 14.0f); // 0x859030
    const auto  maxX = (float)((double)pos.x + 14.0f);
    const auto  minY = (float)((double)pos.y - 14.0f);
    const auto  maxY = (float)((double)pos.y + 14.0f);

    const auto sectors = GetTrafficSectorRange(minX, minY, maxX, maxY);

    CWorld::AdvanceCurrentScanCode();

    float speedFactor = (float)cruiseSpeed;
    for (auto y = sectors.minY; y <= sectors.maxY; y++) {
        for (auto x = sectors.minX; x <= sectors.maxX; x++) {
            auto& sector = CWorld::GetRepeatSector(x, y);
            if (ap.m_nCarDrivingStyle != DRIVING_STYLE_DRIVINGMODE_AVOIDCARS_STOPFORPEDS_OBEYLIGHTS) {
                CCarCtrl::SlowCarDownForCarsSectorList(sector.Vehicles, vehicle, minX, minY, maxX, maxY, &speedFactor, (float)cruiseSpeed); // 0x432420
            }
            CCarCtrl::SlowCarDownForPedsSectorList(sector.Peds, vehicle, minX, minY, maxX, maxY, &speedFactor, (float)cruiseSpeed); // 0x425440
            CCarCtrl::SlowCarDownForObjectsSectorList(sector.Objects, vehicle, minX, minY, maxX, maxY, &speedFactor, (float)cruiseSpeed); // 0x42D4F0
        }
    }

    vehicle->vehicleFlags.bWarnedPeds = true;

    if (ap.m_nCarDrivingStyle == DRIVING_STYLE_STOP_FOR_CARS || ap.m_nCarDrivingStyle == DRIVING_STYLE_STOP_FOR_CARS_IGNORE_LIGHTS) {
        return speedFactor;
    }
    return (cruiseSpeed + speedFactor) * 0.5f;
}

// 0x434400
float CCarCtrl::FindMaximumSpeedForThisCarInTraffic(CVehicle* vehicle) {
    return (float)FindMaximumSpeedForThisCarInTrafficOriginal(vehicle);
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
    if (relX < 200.0f) {
        minX--;
    }
    if (relY < 200.0f) {
        minY--;
    }
    if (relX > 550.0f) {
        maxX++;
    }
    if (relY > 550.0f) {
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
                if (!(std::sqrt((dz * dz + dy * dy) + (double)dx * dx) < 150.0f)) {
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
                    if (score < bestScore) {
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
int8 CCarCtrl::FindPathDirection(CNodeAddress nodeAddress1, CNodeAddress nodeAddress2, CNodeAddress nodeAddress3, bool* outUTurn) {
    *outUTurn = false;

    if (!nodeAddress1.IsAreaValid() || !nodeAddress2.IsAreaValid() || !nodeAddress3.IsAreaValid()) {
        return 0;
    }
    if (!ThePaths.m_pPathNodes[nodeAddress1.m_wAreaId] || !ThePaths.m_pPathNodes[nodeAddress2.m_wAreaId] || !ThePaths.m_pPathNodes[nodeAddress3.m_wAreaId]) {
        return 0;
    }

    const auto& nodeA = ThePaths.m_pPathNodes[nodeAddress1.m_wAreaId][nodeAddress1.m_wNodeId];
    const auto& nodeB = ThePaths.m_pPathNodes[nodeAddress2.m_wAreaId][nodeAddress2.m_wNodeId];
    const auto& nodeC = ThePaths.m_pPathNodes[nodeAddress3.m_wAreaId][nodeAddress3.m_wNodeId];
    const auto  posA  = nodeA.GetPosition();
    const auto  posB  = nodeB.GetPosition();
    const auto  posC  = nodeC.GetPosition();

    // Direction A -> B and B -> C (stored as float)
    const auto abX = (float)((double)posB.x - posA.x);
    const auto abY = (float)((double)posB.y - posA.y);
    const auto bcX = (float)((double)posC.x - posB.x);
    const auto bcY = (float)((double)posC.y - posB.y);

    // x87: The normalized values are kept in extended precision, other than the X of the 1st one
    const auto lenAB = std::sqrt((double)abY * abY + (double)abX * abX);
    if (lenAB == 0.0) {
        return 0;
    }
    const auto invAB = 1.0 / lenAB;
    const auto nAbX  = (float)((double)abX * invAB);
    const auto nAbY  = invAB * abY;

    const auto lenBC = std::sqrt((double)bcY * bcY + (double)bcX * bcX);
    if (lenBC == 0.0) {
        return 0;
    }
    const auto invBC = 1.0 / lenBC;
    const auto nBcX  = (double)bcX * invBC;
    const auto nBcY  = invBC * bcY;

    const auto cross = (float)(nAbX * nBcY - nBcX * nAbY); // Stored as float
    const auto dot   = nAbY * nBcY + nBcX * nAbX;
    if (dot > 0.4f) { // 0x858EE8 - Going (almost) straight
        return 1;
    }
    if (dot < -0.3f) { // 0x858EE4 - Making a U-turn
        *outUTurn = true;
    }
    return cross > 0.0f ? 4 : 2; // 4 = left, 2 = right
}

//! 0x422620 - The original returns the result in a FPU register (not rounded to float)
static double FindPercDependingOnDistToLinkOriginal(CVehicle* vehicle, CCarPathLinkAddress linkAddress) {
    // `CCarPathLink::m_posn` hides its raw values, but the original works on them
    const auto raw = reinterpret_cast<const int16*>(&ThePaths.GetCarPathLink(linkAddress));
    const auto& pos = vehicle->GetPosition();

    const double linkY = (float)((double)raw[1] * 0.125f); // 0x858C48
    const double linkX = (float)((double)raw[0] * 0.125f); // (both are exact in float)

    // x87: The 1st compare uses the unrounded distance, the others the one stored as float
    const double dist  = std::sqrt((linkX - pos.x) * (linkX - pos.x) + (linkY - pos.y) * (linkY - pos.y));
    const auto   distF = (float)dist;
    if (dist < 5.0f) { // 0x858C80
        return 0.5f; // 0x858B8C
    }
    if (!(distF < 15.0f)) { // 0x858B48
        return 1.0f;
    }
    return ((double)distF - 5.0f) * 0.05f + 0.5f; // 0x858C28
}

// 0x422620
float CCarCtrl::FindPercDependingOnDistToLink(CVehicle* vehicle, CCarPathLinkAddress linkAddress) {
    return (float)FindPercDependingOnDistToLinkOriginal(vehicle, linkAddress);
}

// 0x421770
int32 CCarCtrl::FindSequenceElement(int32 index) {
    if (bSequenceOtherWay) {
        return (index + SequenceRandomOffset) % SequenceElements;
    }
    return ((SequenceElements - index) + SequenceRandomOffset) % SequenceElements;
}

// 0x4224E0
float CCarCtrl::FindSpeedMultiplier(float angle, float minAngle, float maxAngle, float minMult) {
    // x87: everything is kept in extended precision until the final float store
    double a = angle;
    while (a < -std::numbers::pi_v<float>) { // 0x858CC0
        a += 2.0f * std::numbers::pi_v<float>; // 0x858CBC
    }
    while (a > std::numbers::pi_v<float>) { // 0x858CB8
        a -= 2.0f * std::numbers::pi_v<float>;
    }
    if (a < 0.0) {
        a = -a;
    }

    double t = a - minAngle;
    if (0.0 > t) { // (NaN stays)
        t = 0.0;
    }
    const double range = (double)maxAngle - minAngle;
    const auto   mult  = (float)(1.0 - (t / range) * (1.0 - (double)minMult)); // Stored as float
    if (t > range) {
        return minMult;
    }
    return mult;
}

// 0x424130
float CCarCtrl::FindSpeedMultiplierWithSpeedFromNodes(int8 arg1) {
    // `arg1` is `(CPathNode byte 1 >> 4) & 3` (bit 0 = `m_bNotHighway`, bit 1 = `m_bHighway`), or -1
    switch ((uint8)arg1) {
    case 0xFF: return 0.5f;  // 0x858B8C
    case 0:    return 0.65f; // 0x858F50
    case 2:    return 2.3f;  // 0x858F54
    default:   return 1.0f;  // 0x858624
    }
}

//! 0x422370 - The original returns the result in a FPU register (not rounded to float)
static double FindGhostRoadHeightOriginal(CVehicle* vehicle) {
    const auto& ap = vehicle->m_autoPilot;
    if (ap.m_currentAddress.m_wAreaId == 0xFFFF || ap.m_startingRouteNode.m_wAreaId == 0xFFFF) {
        return 0.0f;
    }
    if (!ThePaths.m_pPathNodes[ap.m_currentAddress.m_wAreaId] || !ThePaths.m_pPathNodes[ap.m_startingRouteNode.m_wAreaId]) {
        return 0.0f;
    }

    const auto& node1 = ThePaths.m_pPathNodes[ap.m_currentAddress.m_wAreaId][ap.m_currentAddress.m_wNodeId];
    const auto& node2 = ThePaths.m_pPathNodes[ap.m_startingRouteNode.m_wAreaId][ap.m_startingRouteNode.m_wNodeId];
    const auto  z1    = node1.GetPosition().z;
    const auto  z2    = node2.GetPosition().z;

    const auto& vehPos = vehicle->GetPosition();

    // x87: The 1st distance is stored as float, the 2nd one is kept in the FPU register
    const auto pos1  = node1.GetPosition();
    const auto dist1 = (float)std::sqrt(((double)pos1.y - vehPos.y) * ((double)pos1.y - vehPos.y) + ((double)pos1.x - vehPos.x) * ((double)pos1.x - vehPos.x));
    const auto pos2  = node2.GetPosition();
    const auto dist2 = std::sqrt(((double)pos2.y - vehPos.y) * ((double)pos2.y - vehPos.y) + ((double)pos2.x - vehPos.x) * ((double)pos2.x - vehPos.x));

    return ((double)dist1 * z2 + dist2 * z1) / (dist2 + dist1);
}

// 0x422370
float CCarCtrl::FindGhostRoadHeight(CVehicle* vehicle) {
    return (float)FindGhostRoadHeightOriginal(vehicle);
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
    if (!bAllowEmergencyServicesToBeCreated) {
        return;
    }
    if (CGangWars::GangWarFightingGoingOn()) { // 0x443AC0
        return;
    }

    // Note: The original doesn't check for the player data to be there (it crashes if it's not)
    const auto* const playerData = FindPlayerPed(-1)->GetPlayerData();
    const auto* const wanted     = playerData ? playerData->m_pWanted : nullptr;
    if ((int32)wanted->m_WantedLevel > 3) {
        return;
    }

    if (CGame::currArea != 0 || CTheZones::m_CurrLevel == 0) {
        return;
    }

    if ((int32)NumAmbulancesOnDuty + (int32)NumFireTrucksOnDuty + (int32)NumParkedCars + (int32)NumMissionCars + (int32)NumLawEnforcerCars + (int32)NumRandomCars > (int32)MaxNumberOfCarsInUse) {
        return;
    }

    if (NumAmbulancesOnDuty == 0) {
        if (CAccidentManager::GetInstance()->GetNumberOfFreeAccidents() < 2) { // 0x56CEE0
            CStreaming::StreamAmbulanceAndMedic(false); // 0x40A2A0
        } else {
            auto playerPos = FindPlayerCoors(-1);
            if (const auto accident = CAccidentManager::GetInstance()->GetNearestFreeAccident(playerPos, false)) { // 0x56D050
                if (CStreaming::StreamAmbulanceAndMedic(true) && CTimer::GetTimeInMS() > (uint32)LastTimeAmbulanceCreated + 30000u) {
                    const auto model = CStreaming::ms_aDefaultAmbulanceModel[CTheZones::m_CurrLevel]; // 0x407D30
                    if (GenerateOneEmergencyServicesCar(model, accident->m_pPed->GetPosition())) { // 0x42B7D0
                        LastTimeAmbulanceCreated = CTimer::GetTimeInMS();
                    }
                }
            }
        }
    }

    if (NumFireTrucksOnDuty == 0) {
        if ((uint16)gFireManager.GetNumOfNonScriptFires() < 3) { // 0x538F10 (the original compares the low 16 bits)
            CStreaming::StreamFireEngineAndFireman(false); // 0x40A400
        } else if (const auto fire = gFireManager.FindNearestFire(FindPlayerCoors(-1), true, true)) { // 0x538F40
            if (CStreaming::StreamFireEngineAndFireman(true) && CTimer::GetTimeInMS() > (uint32)LastTimeFireTruckCreated + 35000u) {
                const auto model = CStreaming::ms_aDefaultFireEngineModel[CTheZones::m_CurrLevel]; // 0x407DC0
                if (GenerateOneEmergencyServicesCar(model, fire->GetPosition())) {
                    LastTimeFireTruckCreated = CTimer::GetTimeInMS();
                }
            }
        }
    }
}

// 0x42B7D0
CAutomobile* CCarCtrl::GenerateOneEmergencyServicesCar(uint32 modelId, CVector posn) {
    return plugin::CallAndReturn<CAutomobile*, 0x42B7D0, uint32, CVector>(modelId, posn);
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
    const auto heli = static_cast<CHeli*>(automobile);

    // Note: The original gets the player's coords over and over
    const auto  playerPos = FindPlayerCoors(-1);
    const auto& heliPos   = heli->GetPosition();

    auto heading = CGeneral::GetATanOfXY( // 0x53CC70
        (float)((double)playerPos.x - heliPos.x),
        (float)((double)playerPos.y - heliPos.y)
    );

    // Distance to the player. x87: The differences are kept in extended precision (only the Y is rounded to float at first)
    const auto dyExt = (double)playerPos.y - heliPos.y;
    const auto dxExt = (double)playerPos.x - heliPos.x;
    auto       dist  = (float)std::sqrt(dyExt * (float)dyExt + dxExt * dxExt);

    heli->m_fMaxAltitude = playerPos.z;
    heli->m_autoPilot.m_vecDestinationCoors = playerPos;

    if (heli->m_autoPilot.m_nCarMission == MISSION_HELI_ATTACK_PLAYER) {
        if (dist < 15.0f) { // 0x858B48
            heli->m_autoPilot.m_nCarMission = MISSION_HELI_ATTACK_PLAYER_FLY_AWAY;
        }
        dist += 50.0f; // 0x858B40
    } else if (heli->m_autoPilot.m_nCarMission == MISSION_HELI_ATTACK_PLAYER_FLY_AWAY) {
        if (dist > 18.0f) { // 0x859008
            heli->m_autoPilot.m_nCarMission = MISSION_HELI_ATTACK_PLAYER;
        }
        heading += std::numbers::pi_v<float>;
    }

    FlyAIHeliInCertainDirection(heli, heading, dist, false); // 0x429A70
    TestWhetherToFirePlaneGuns(automobile, FindPlayerEntity(-1)); // 0x429520
    FireHeliRocketsAtTarget(automobile, FindPlayerEntity(-1)); // 0x42B270
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
    // Note: The target is stored without registering a reference
    if (FindPlayerVehicle(-1, false)) {
        automobile->m_autoPilot.m_TargetEntity = FindPlayerVehicle(-1, false);
    } else {
        automobile->m_autoPilot.m_TargetEntity = reinterpret_cast<CVehicle*>(FindPlayerPed(-1)); // Not a vehicle, but the original does the same
    }
    GetAIPlaneToDoDogFight(automobile);
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

static_assert(offsetof(CVehicle, m_nCreatedBy) == 0x4A4);
static_assert(offsetof(CPed, m_nPedType) == 0x598);
static_assert(offsetof(CVehicleModelInfo, m_nVehicleClass) == 0x4D);

// 0x42DAB0
bool CCarCtrl::IsThisAnAppropriateNode(CVehicle* vehicle, CNodeAddress nodeAddress1, CNodeAddress nodeAddress2, CNodeAddress nodeAddress3, bool arg5, bool arg6) {
    // `arg6` is unused
    // NOTE: `nodeAddress2` is the node the vehicle is at, `nodeAddress3` is the one that is being checked, `nodeAddress1` is the one it came from
    if (!ThePaths.m_pPathNodes[nodeAddress3.m_wAreaId]) {
        return false;
    }
    if (nodeAddress1 == nodeAddress3) { // Going back
        return false;
    }

    // The node of `nodeAddress2` isn't checked in the original (the area isn't null checked)
    const auto& curNode  = ThePaths.m_pPathNodes[nodeAddress2.m_wAreaId][nodeAddress2.m_wNodeId];
    const auto& nextNode = ThePaths.m_pPathNodes[nodeAddress3.m_wAreaId][nodeAddress3.m_wNodeId];

    // Only a hovercraft can go from water to land (or the other way around)
    if (curNode.m_bWaterNode != nextNode.m_bWaterNode && vehicle->m_nModelIndex != MODEL_VORTEX) {
        return false;
    }

    const auto IsMissionVehicleOrDriver = [&] {
        return vehicle->m_nCreatedBy == MISSION_VEHICLE || (vehicle->m_pDriver && vehicle->m_pDriver->IsCreatedBy(PED_MISSION));
    };
    const auto HasBigVehicleClass = [&] {
        return CModelInfo::GetModelInfo(vehicle->m_nModelIndex)->AsVehicleModelInfoPtr()->m_nVehicleClass == VEHICLE_CLASS_BIG;
    };
    const auto IsAnyoneBlockingNode = [&] { // The 2nd half of the checks of the roadblock (1) and parking (2) node types
        if (vehicle->IsLawEnforcementVehicle()) { // 0x6D2370
            return true;
        }
        if (HasBigVehicleClass()) {
            return true;
        }
        if (vehicle->m_pDriver && vehicle->m_pDriver->m_nPedType == PED_TYPE_CRIMINAL) {
            return true;
        }
        if (IsAnyoneParking()) { // 0x42C250
            return true;
        }
        int16 numColliding;
        CWorld::FindObjectsKindaColliding(nextNode.GetPosition(), 5.0f, true, &numColliding, 2, nullptr, false, true, false, false, false); // 0x568B80
        return numColliding != 0;
    };

    switch (nextNode.m_nBehaviourType) {
    case 1: { // Road blocks
        if (IsMissionVehicleOrDriver() || HasBigVehicleClass()) {
            return false;
        }
        if (!arg5) {
            const auto toNode = nextNode.GetPosition() - vehicle->GetPosition();
            const auto cross  = CrossProductOriginal(nextNode.GetPosition() - curNode.GetPosition(), toNode); // 0x59C730
            if (cross.z < 0.0f) {
                return false;
            }
        }
        return !IsAnyoneBlockingNode();
    }
    case 2: { // Parking
        if (IsMissionVehicleOrDriver()) {
            return false;
        }
        if (!arg5) {
            const auto toNode = nextNode.GetPosition() - vehicle->GetPosition();
            if (DotProductOriginal(vehicle->GetRightVector(), toNode) < 0.0) { // 0x41CC70, 0x40FDB0
                return false;
            }
        }
        return !IsAnyoneBlockingNode();
    }
    case 5:
    case 10: {
        if (arg5) {
            return false;
        }
        if (GetModelBoundBox(vehicle).m_vecMax.z < 2.0f) { // 0x858CA0
            return false;
        }
        break;
    }
    case 8:
    case 9: {
        if (arg5) {
            return false;
        }
        const auto& bb = GetModelBoundBox(vehicle);
        if (bb.m_vecMax.z > 1.5f) { // 0x858CE8
            return false;
        }
        if (bb.m_vecMax.x > 2.0f) { // 0x858CA0
            return false;
        }
        if (bb.m_vecMax.y > 4.0f) { // 0x858B90
            return false;
        }
        break;
    }
    default:
        break;
    }

    // A mission vehicle that is cruising doesn't go to nodes that it's not allowed to wander to (unless the current node is the same)
    if (vehicle->m_nCreatedBy == MISSION_VEHICLE && vehicle->m_autoPilot.m_nCarMission == MISSION_CRUISE && nextNode.m_bDontWander) {
        if (!curNode.m_bDontWander) {
            return false;
        }
    }
    if (nextNode.m_onDeadEnd && !curNode.m_onDeadEnd) {
        return false;
    }
    if (nextNode.m_isSwitchedOff && !curNode.m_isSwitchedOff) {
        return false;
    }
    return !arg5;
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
    auto& ap = vehicle->m_autoPilot;

    // Note: Only the area ids of the node addresses are reset
    ap.m_endingRouteNode.ResetAreaId();
    ap.m_currentAddress.ResetAreaId();
    ap.m_startingRouteNode.ResetAreaId();
    ap.m_nCurrentPathNodeInfo  = {};
    ap.m_nPreviousPathNodeInfo = {};
    ap.m_nNextPathNodeInfo     = {};
    ap.m_nPathFindNodesCount   = 0;

    // Note: The matrix is used directly (not null checked)
    const auto& fwd = vehicle->m_matrix->GetForward();
    const auto  nearest = ThePaths.FindNodeClosestToCoorsFavourDirection(vehicle->GetPosition(), PATH_TYPE_VEH, CVector2D{ fwd.x, fwd.y }); // 0x44FCE0
    if (nearest.m_wAreaId == UINT16_MAX) {
        return;
    }
    if (!ThePaths.m_pPathNodes[nearest.m_wAreaId]) {
        return;
    }

    const auto& nearestNode = ThePaths.m_pPathNodes[nearest.m_wAreaId][nearest.m_wNodeId];
    const auto  nearestPos  = nearestNode.GetPosition();

    // Find the linked node that's the closest to this one
    float        closestDist = std::bit_cast<float>(0x497423FEu); // ~999999.9
    CNodeAddress closest{}; // Note: Only the area id is reset in the original (the node id is only used if a node was found)
    for (auto i = 0; i < (int32)nearestNode.m_nNumLinks; i++) {
        const auto link = ThePaths.m_pNodeLinks[nearest.m_wAreaId][nearestNode.m_wBaseLinkId + i];
        if (!ThePaths.m_pPathNodes[link.m_wAreaId]) {
            continue;
        }
        const auto  linkedPos = ThePaths.m_pPathNodes[link.m_wAreaId][link.m_wNodeId].GetPosition();
        const auto dist = std::sqrt(((double)linkedPos.y - nearestPos.y) * ((double)linkedPos.y - nearestPos.y) + ((double)linkedPos.x - nearestPos.x) * ((double)linkedPos.x - nearestPos.x)); // x87: kept in extended precision
        if (dist < closestDist) {
            closestDist = (float)dist;
            closest     = link;
        }
    }
    if (closest.m_wAreaId == UINT16_MAX) {
        return;
    }

    // The vehicle should be heading from the node that's behind it to the one that's in front of it
    float dirX = fwd.x;
    const float dirY = fwd.y;
    if (dirX == 0.0f && dirY == 0.0f) {
        dirX = 1.0f;
    }

    const auto closestPos = ThePaths.m_pPathNodes[closest.m_wAreaId][closest.m_wNodeId].GetPosition();
    const auto dot        = ((double)nearestPos.y - closestPos.y) * dirY + ((double)nearestPos.x - closestPos.x) * dirX;

    CNodeAddress from, to;
    if (!(dot < 0.0)) {
        from = closest;
        to   = nearest;
    } else {
        from = nearest;
        to   = closest;
    }
    ap.m_endingRouteNode.ResetAreaId();
    ap.m_currentAddress      = from;
    ap.m_startingRouteNode   = to;
    FindLinksToGoWithTheseNodes(vehicle); // 0x42B470
    ap.m_nCurrentLane = 0;
    ap.m_nNextLane    = 0;
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
        if (dx * dx + dy * dy > 256.0f) { // 0x858FB4
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
    auto& ap = vehicle->m_autoPilot;

    // NOTE: The names of the autopilot's node addresses are misleading here (see `PickNextNodeRandomly`):
    // `m_currentAddress` is the node the vehicle is coming from, `m_startingRouteNode` is the one it's heading to
    CNodeAddress results[2]{}; // (The original only sets the area of these to -1)
    CNodeAddress chosen{};

    if (vehicle->m_nForcedRandomRouteSeed) {
        srand((uint16)vehicle->m_nForcedRandomRouteSeed);
    }

    const auto cur  = ap.m_currentAddress;
    const auto next = ap.m_startingRouteNode;

    constexpr auto MAX_DIST = std::bit_cast<float>(0x497423FEu); // ~999999.9
    const auto     isDefaultRegion = CWeather::WeatherRegion == WEATHER_REGION_DEFAULT || CWeather::WeatherRegion == WEATHER_REGION_DESERT;
    const auto     maxSearchDist   = isDefaultRegion ? 50.0f : MAX_DIST; // 0x42480000

    // NOTE: The area of `next` isn't null checked in the original
    const auto& nextNode = ThePaths.m_pPathNodes[next.m_wAreaId][next.m_wNodeId];

    int16 numResults{};
    float pathDist{};
    ThePaths.DoPathSearch(
        PATH_TYPE_VEH,
        nextNode.GetPosition(),
        next,
        CVector{ destX, destY, destZ },
        results,
        numResults,
        2,
        &pathDist,
        maxSearchDist,
        nullptr,
        MAX_DIST,
        false,
        StaticRef<CNodeAddress>(0x8A5F44), // (area = -1, node = 0), effectively none
        vehicle->m_nModelIndex == MODEL_VORTEX,
        false
    ); // 0x4515D0

    if (isDefaultRegion) {
        if (numResults == 0) {
            return true;
        }

        // The path is too long compared to the distance to the destination
        const auto& pos = vehicle->GetPosition();
        const auto  dx  = (double)destX - pos.x;
        const auto  dy  = (double)destY - pos.y;
        if (std::sqrt(dy * dy + dx * dx) * 3.0f < pathDist) { // 0x858B3C
            return true;
        }
    }

    // Index of the `chosen` link in the links of `next`
    int16 linkIdx = 0;
    bool  decided = false;
    if (numResults == 1 || numResults == 2) {
        // Go to the first node of the route (unless it's the node we're at), otherwise to the 2nd one
        if (results[0] != next) {
            chosen  = results[0];
            decided = true;
        } else if (numResults == 2 && results[1] != next) {
            chosen  = results[1];
            decided = true;
        }
        if (decided) {
            // NOTE: Not bounded in the original
            while (ThePaths.m_pNodeLinks[next.m_wAreaId][nextNode.m_wBaseLinkId + linkIdx] != chosen) {
                linkIdx++;
            }
        }
    }

    if (!decided) {
        // Pick the link that leads the closest to the direction of the destination
        const auto& pos     = vehicle->GetPosition();
        const auto  heading = (float)CGeneral::GetATanOfXY((float)((double)destX - pos.x), (float)((double)destY - pos.y)); // 0x53CC70 (stored as float)

        const auto numLinks  = (int16)nextNode.m_nNumLinks;
        float      bestAngle = 10.0f; // 0x41200000
        chosen = {};
        for (int16 i = 0; i < numLinks; i++) {
            const auto cand = ThePaths.m_pNodeLinks[next.m_wAreaId][nextNode.m_wBaseLinkId + i];
            if (cand == cur && numLinks > 1) { // Don't go back (unless this is a dead end)
                continue;
            }
            if (!ThePaths.m_pPathNodes[cand.m_wAreaId]) {
                continue;
            }

            const auto candPos = ThePaths.m_pPathNodes[cand.m_wAreaId][cand.m_wNodeId].GetPosition();
            const auto nodePos = nextNode.GetPosition();

            // x87: The angle is kept in extended precision (only the final value is rounded to float)
            double angle = (double)CGeneral::GetATanOfXY(candPos.x - nodePos.x, candPos.y - nodePos.y) - heading; // 0x53CC70
            if (angle > std::numbers::pi_v<float>) {
                do {
                    angle -= 2.0f * std::numbers::pi_v<float>;
                } while (angle > std::numbers::pi_v<float>);
            }
            if (angle < -std::numbers::pi_v<float>) {
                do {
                    angle += 2.0f * std::numbers::pi_v<float>;
                } while (angle < -std::numbers::pi_v<float>);
            }
            if (angle < 0.0) {
                angle = -angle;
            }

            if (angle <= bestAngle) {
                bestAngle = (float)angle;
                linkIdx   = i;
                chosen    = cand;
            }
        }
    }

    // Move on to the next link
    ap.m_endingRouteNode       = ap.m_currentAddress;
    ap.m_currentAddress        = ap.m_startingRouteNode;
    ap.m_startingRouteNode     = chosen;
    ap.field_C                += (int32)ap.m_nSpeedScaleFactor;
    ap.m_nPreviousPathNodeInfo = ap.m_nCurrentPathNodeInfo;
    ap.m_nCurrentPathNodeInfo  = ap.m_nNextPathNodeInfo;
    ap._smthPrev               = ap._smthCurr;
    ap._smthCurr               = ap._smthNext;
    ap.m_nCurrentLane          = ap.m_nNextLane;
    ap.m_nNextPathNodeInfo     = ThePaths.m_pNaviLinks[next.m_wAreaId][nextNode.m_wBaseLinkId + linkIdx];

    if (StopCarIfNodesAreInvalid(vehicle)) { // 0x422590
        return true;
    }

    // The direction of travel on the new link (depends on the order of the nodes), and the number of lanes in that direction
    int32 numLanes;
    {
        const auto& link = ThePaths.GetCarPathLink(ap.m_nNextPathNodeInfo);
        const auto  goingToLowerNode = next.m_wAreaId < ap.m_startingRouteNode.m_wAreaId
            || (next.m_wAreaId == ap.m_startingRouteNode.m_wAreaId && next.m_wNodeId < ap.m_startingRouteNode.m_wNodeId);
        if (goingToLowerNode) {
            ap._smthNext = -1;
            numLanes     = link.m_numOppositeDirLanes;
        } else {
            ap._smthNext = 1;
            numLanes     = link.m_numSameDirLanes;
        }
    }

    // If the next link is far enough from the current one the vehicle changes lanes from time to time
    {
        // `CCarPathLink::m_posn` hides its raw values, but the original works on them
        const auto curRaw  = reinterpret_cast<const int16*>(&ThePaths.GetCarPathLink(ap.m_nCurrentPathNodeInfo));
        const auto nextRaw = reinterpret_cast<const int16*>(&ThePaths.GetCarPathLink(ap.m_nNextPathNodeInfo));
        const auto dx      = (double)nextRaw[0] * 0.125f - (double)curRaw[0] * 0.125f; // 0x858C48
        const auto dy      = (double)nextRaw[1] * 0.125f - (double)curRaw[1] * 0.125f;
        if (dx * dx + dy * dy > 256.0f) { // 0x858FB4
            switch (ap.m_nCarMission) {
            case MISSION_RAMPLAYER_FARAWAY:
            case MISSION_BLOCKPLAYER_FARAWAY:
            case MISSION_RAMCAR_FARAWAY:
            case MISSION_FOLLOWCAR_FARAWAY:
            case MISSION_BLOCKCAR_FARAWAY:
            case MISSION_KILLPED_FARAWAY:
            case MISSION_APPROACHPLAYER_FARAWAY:
            case MISSION_DO_DRIVEBY_FARAWAY:
                break;
            default:
                if (--ap.field_50 == 0) {
                    ap.field_50 = (char)((rand() & 3) + 4);
                    ap.m_nNextLane += rand() >= 0x3FFF ? -1 : 1;
                }
                break;
            }
        }
    }

    // Keep the lane in range
    {
        int32 lane = ap.m_nNextLane;
        if (lane >= numLanes - 1) {
            lane = numLanes - 1;
        }
        ap.m_nNextLane = (int8)lane;
        if (ap.m_nNextLane <= 0) {
            ap.m_nNextLane = 0;
        }
    }

    if (ap.carCtrlFlags.bStayInFastLane) {
        ap.m_nNextLane = 0;
    } else if (ap.carCtrlFlags.bStayInSlowLane || vehicle->m_nVehicleSubType == VEHICLE_TYPE_BMX) {
        ap.m_nNextLane = (int8)std::max(numLanes - 1, 0);
    }

    // 0x44DB00 (x2): `OneWayLaneOffset` of the current and the next link, the results are discarded
    return false;
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
    auto& ap = vehicle->m_autoPilot;

    // Only every 2 seconds (the seed makes it so that not every vehicle does this at the same time)
    const auto seed = (uint32)vehicle->m_nRandomSeed;
    if ((seed + CTimer::GetTimeInMS()) / 2000u == (seed + CTimer::GetPreviousTimeInMS()) / 2000u) {
        return;
    }

    switch (ap.m_nCarMission) {
    case MISSION_RAMPLAYER_FARAWAY:
    case MISSION_BLOCKPLAYER_FARAWAY:
    case MISSION_GOTOCOORDINATES:
    case MISSION_RAMCAR_FARAWAY:
    case MISSION_BLOCKCAR_FARAWAY:
    case MISSION_APPROACHPLAYER_FARAWAY:
    case MISSION_FOLLOWCAR_FARAWAY:
    case MISSION_KILLPED_FARAWAY:
    case MISSION_DO_DRIVEBY_FARAWAY:
        break;
    default:
        ap.m_ucCarMissionModeCounter = 0;
        return;
    }

    // BUG: In the original the node ids (only the area ids are reset) are uninitialized if `FindNodesThisCarIsNearestTo` doesn't find anything
    CNodeAddress nodeA{}, nodeB{};
    FindNodesThisCarIsNearestTo(vehicle, nodeA, nodeB); // 0x42BD20
    if (nodeA.m_wAreaId == UINT16_MAX) {
        return;
    }

    // Is the vehicle already going the way it should?
    const auto& cur  = ap.m_currentAddress;
    const auto& next = ap.m_startingRouteNode;
    const auto& prev = ap.m_endingRouteNode;
    if (   (cur == nodeA && next == nodeB)
        || (cur == nodeB && next == nodeA)
        || (prev == nodeA && cur == nodeB)
        || (prev == nodeB && cur == nodeA)
        || cur == nodeB
        || prev == nodeB
    ) {
        ap.m_ucCarMissionModeCounter = 0;
        return;
    }

    // It must have been like this for a few ticks
    if (++ap.m_ucCarMissionModeCounter <= 4) {
        return;
    }

    // Where the vehicle is heading
    CVector target;
    switch (ap.m_nCarMission) {
    case MISSION_RAMPLAYER_FARAWAY:
    case MISSION_BLOCKPLAYER_FARAWAY:
    case MISSION_APPROACHPLAYER_FARAWAY:
        target = FindPlayerCoors(-1);
        break;
    case MISSION_GOTOCOORDINATES:
        target = ap.m_vecDestinationCoors;
        break;
    default: // MISSION_RAMCAR_FARAWAY, MISSION_BLOCKCAR_FARAWAY, MISSION_FOLLOWCAR_FARAWAY, MISSION_KILLPED_FARAWAY, MISSION_DO_DRIVEBY_FARAWAY
        target = ap.m_TargetEntity->GetPosition();
        break;
    }

    const auto forVortex = vehicle->m_nModelIndex == MODEL_VORTEX;
    const auto oneSide   = (bool)ap.carCtrlFlags.bCantGoAgainstTraffic;
    constexpr auto MAX_DIST = std::bit_cast<float>(0x497423FEu); // ~999999.9

    int16 numNodes;
    float dist;
    ThePaths.DoPathSearch(PATH_TYPE_VEH, vehicle->GetPosition(), nodeB, target, nullptr, numNodes, 0, &dist, MAX_DIST, nullptr, MAX_DIST, oneSide, ap.m_currentAddress, forVortex, false); // 0x4515D0
    if (!(dist < 90000.0f) || numNodes < 2) { // 0x85900C
        ap.m_ucCarMissionModeCounter = 0;
        return;
    }

    // There is a route, so go along it
    ap.m_currentAddress    = nodeA;
    ap.m_startingRouteNode = nodeB;
    FindLinksToGoWithTheseNodes(vehicle); // 0x42B470

    ThePaths.DoPathSearch(PATH_TYPE_VEH, vehicle->GetPosition(), nodeB, target, ap.m_aPathFindNodesInfo.data(), reinterpret_cast<int16&>(ap.m_nPathFindNodesCount), 8, nullptr, MAX_DIST, nullptr, MAX_DIST, oneSide, ap.m_currentAddress, forVortex, false);
    ap.RemoveOnePathNode(); // 0x41B950

    ap.m_ucCarMissionModeCounter = 0;
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
void CCarCtrl::SetCoordsOfScriptCar(CVehicle* vehicle, float x, float y, float z, uint8 resetRotation, uint8 placeOnGround) {
    const auto vehicleRef = CPools::GetVehicleRef(vehicle);

    if (z <= -100.0f) { // 0x859014
        z = CWorld::FindGroundZForCoord(x, y); // 0x569660
    }
    if (placeOnGround) {
        z = (float)(vehicle->GetDistanceFromCentreOfMassToBaseOfModel() + (double)z); // x87: the sum is rounded only once
    }

    vehicle->SetIsStatic(false);
    CTheScripts::StuckCars.ClearStuckFlagForCar(vehicleRef); // 0x463C40

    const CVector pos{ x, y, z };
    vehicle->Teleport(pos, resetRotation != 0);

    if (vehicle->m_nVehicleType != VEHICLE_TYPE_BOAT) {
        switch (vehicle->m_nVehicleType) {
        case VEHICLE_TYPE_AUTOMOBILE:
        case VEHICLE_TYPE_TRAILER:
            vehicle->AsAutomobile()->PlaceOnRoadProperly(); // 0x6AF420
            break;
        case VEHICLE_TYPE_BIKE:
            vehicle->AsBike()->PlaceOnRoadProperly(); // 0x6BEEB0
            break;
        default:
            break;
        }
        CTheScripts::ClearSpaceForMissionEntity(pos, vehicle); // 0x486B00
        JoinCarWithRoadAccordingToMission(vehicle); // 0x432CB0
    } else {
        CTheScripts::ClearSpaceForMissionEntity(pos, vehicle); // 0x486B00
    }
    vehicle->m_autoPilot.m_nTempAction = TEMPACT_NONE;
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
void CCarCtrl::SlowCarDownForCarsSectorList(CPtrListDoubleLink<CVehicle*>& carList, CVehicle* vehicle, float minX, float minY, float maxX, float maxY, float* speedFactor, float speedMult) {
    // The original stores the next node before processing the current one
    for (auto it = carList.begin(); it != carList.end();) {
        CVehicle* const other = *it;
        ++it;

        if (other == vehicle || other->IsScanCodeCurrent() || !other->m_bUsesCollision) {
            continue;
        }
        other->SetCurrentScanCode();

        CVector centre;
        other->GetBoundCentre(centre); // 0x534250
        if (!(centre.x > minX) || !(centre.x < maxX) || !(centre.y > minY) || !(centre.y < maxY)) {
            continue;
        }

        {
            const auto zDiff = (double)centre.z - vehicle->GetPosition().z;
            if (!((zDiff < 0.0 ? -zDiff : zDiff) < 10.0f)) { // x87: FABS
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
        const auto vehPosZ = vehicle->GetPosition().z;
        const auto vehFwdZ = vehicle->GetForwardVector().z;

        // x87: kept in extended precision
        const auto zErr = (double)centre.z - ((double)distAlongLine * vehFwdZ + vehPosZ);
        if (!((zErr < 0.0 ? -zErr : zErr) < 3.0f)) {
            continue;
        }

        SlowCarDownForOtherCar(other, vehicle, speedFactor, speedMult); // 0x42D0E0
    }
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
        if (!(centre.x > minX) || !(centre.x < maxX) || !(centre.y > minY) || !(centre.y < maxY)) {
            continue;
        }

        {
            const auto zDiff = (double)centre.z - vehicle->GetPosition().z;
            if (!((zDiff < 0.0 ? -zDiff : zDiff) < 10.0f)) { // x87: FABS
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
        if (!((zErr < 0.0 ? -zErr : zErr) < 3.0f)) {
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
        if (dot < 0.0) {
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
        if (!(dist < dist2)) {
            dist = dist2;
        }
    }
    if (!(dist >= 0.0f)) {
        return;
    }

    if (dist < 1.5f) {
        vehicle->m_autoPilot.carCtrlFlags.bHonkAtCar = true;
        vehicle->m_autoPilot.m_ObstructingEntity = entity;
        entity->RegisterReference(&vehicle->m_autoPilot.m_ObstructingEntity); // 0x571B70

        const double recip = 1.0f / (double)speedMult; // x87: kept in extended precision
        if (dist < recip) {
            *speedFactor = 0.0f;
        } else if (dist < 3.0f * recip) {
            if (!(*speedFactor < 1.0f)) {
                *speedFactor = 1.0f;
            }
        } else {
            const double scaled = ((double)dist - 0.2f) * std::bit_cast<float>(0x3F44EC4Fu); // 0x858FFC (~0.7692308)
            dist = 0.0 > scaled ? 0.0f : (float)scaled;

            const double newFactor = (double)dist * speedMult;
            if (!(newFactor > *speedFactor)) {
                *speedFactor = (float)newFactor;
            }
        }
    }

    // Cars that are moving towards each other and have been in the traffic for a while make the one with the lower address go on (and stop being 'simple')
    if (!(dist >= 0.0f) || !(dist < 0.5f)) {
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
    if (!(fwdDot < -0.5f)) {
        return;
    }
    if (vehicle >= entity->AsVehicle()) { // Only one of the 2 cars (the one with the lower address) is affected
        return;
    }

    const double newFactor = (double)speedMult * 0.2f; // x87: the comparison is done with the not rounded value
    if (!(newFactor < *speedFactor)) {
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

// The raw offsets the AI steering code of the original works with (checked, so the named members can be used)
static_assert(offsetof(CVehicle, m_autoPilot) + offsetof(CAutoPilot, m_vehicleRecordingId) == 0x424);
static_assert(offsetof(CVehicle, m_autoPilot) + offsetof(CAutoPilot, m_nCarDrivingStyle) == 0x3B9);
static_assert(offsetof(CVehicle, m_autoPilot) + offsetof(CAutoPilot, m_nCarMission) == 0x3BA);
static_assert(offsetof(CVehicle, m_autoPilot) + offsetof(CAutoPilot, m_nTempAction) == 0x3BB);
static_assert(offsetof(CVehicle, m_autoPilot) + offsetof(CAutoPilot, m_nCruiseSpeed) == 0x3D0);
static_assert(offsetof(CVehicle, m_autoPilot) + offsetof(CAutoPilot, m_nMovementFlags) == 0x3DC);
static_assert(offsetof(CVehicle, m_autoPilot) + offsetof(CAutoPilot, m_startingRouteNode) == 0x394);
static_assert(offsetof(CVehicle, m_autoPilot) + offsetof(CAutoPilot, m_nTimeToStartMission) == 0x3AC);
static_assert(offsetof(CVehicle, m_autoPilot) + offsetof(CAutoPilot, m_nTimeSwitchedToRealPhysics) == 0x3B0);
static_assert(offsetof(CVehicle, m_autoPilot) + offsetof(CAutoPilot, m_nTempActionTime) == 0x3BC);
static_assert(offsetof(CVehicle, m_autoPilot) + offsetof(CAutoPilot, m_vecDestinationCoors) == 0x3EC);
static_assert(offsetof(CVehicle, m_autoPilot) + offsetof(CAutoPilot, m_ucCarFollowDist) == 0x3DE);
static_assert(offsetof(CVehicle, m_autoPilot) + offsetof(CAutoPilot, m_TargetEntity) == 0x41C);
static_assert(offsetof(CVehicle, m_autoPilot) + offsetof(CAutoPilot, m_ObstructingEntity) == 0x420);
static_assert(offsetof(CVehicle, m_fSteerAngle) == 0x494);
static_assert(offsetof(CVehicle, m_GasPedal) == 0x49C);
static_assert(offsetof(CVehicle, m_BrakePedal) == 0x4A0);
static_assert(offsetof(CVehicle, m_nRandomSeed) == 0x20);
static_assert(offsetof(CBmx, m_fControlPedaling) == 0x818);

//! Limits the steering angle to +-`FindMaxSteerAngle`
static float ClampSteerToMax(CVehicle* vehicle, float steer) {
    const auto maxSteer = CCarCtrl::FindMaxSteerAngle(vehicle);
    if (steer < -maxSteer) {
        steer = -maxSteer;
    }
    if (steer > maxSteer) {
        steer = maxSteer;
    }
    return steer;
}

//! Parked vehicles: the driver and all the passengers get out of the vehicle (that is parked now)
static void MakeOccupantsLeaveParkedVehicle(CVehicle* vehicle) {
    const auto MakeLeave = [](CPed* ped) {
        ped->GetTaskManager().SetTask(new CTaskComplexLeaveAnyCar{ 0, true, false }, TASK_PRIMARY_PRIMARY, false); // 0x681AF0
    };
    if (vehicle->m_pDriver) {
        MakeLeave(vehicle->m_pDriver);
    }
    for (auto* const passenger : vehicle->m_apPassengers) {
        if (passenger) {
            MakeLeave(passenger);
        }
    }
}

// 0x433BA0
void CCarCtrl::SteerAICarParkParallel(CVehicle* vehicle, float* pSteer, float* pGas, float* pBrake, bool* pHandbrake) {
    auto& autoPilot = vehicle->m_autoPilot;

    // Note: Only the nodes' areas are checked (not the validity of the node's address)
    const auto startNodes = ThePaths.m_pPathNodes[autoPilot.m_startingRouteNode.m_wAreaId];
    const auto curNodes   = ThePaths.m_pPathNodes[autoPilot.m_currentAddress.m_wAreaId];
    if (!startNodes || !curNodes) {
        autoPilot.m_nCarMission = MISSION_STOP_FOREVER;
        *pBrake = 0.0f;
        *pGas   = 0.0f;
        *pSteer = 0.0f;
        return;
    }
    const auto& startNode = startNodes[autoPilot.m_startingRouteNode.m_wNodeId];
    const auto& curNode   = curNodes[autoPilot.m_currentAddress.m_wNodeId];

    CVector target;
    if (autoPilot.m_nCarMission == MISSION_PARK_PARALLEL) {
        // Aim a bit past the start node (in the direction of the road)
        const auto curPos   = curNode.GetPosition(); // 0x420A10
        const auto startPos = startNode.GetPosition(); // 0x420A10
        CVector dir{ startPos.x - curPos.x, startPos.y - curPos.y, startPos.z - curPos.z };
        NormaliseOriginal(dir); // 0x59C910
        const auto startPos2 = startNode.GetPosition(); // 0x420A10
        target = CVector{
            (float)((double)dir.x + startPos2.x),
            (float)((double)dir.y + startPos2.y),
            (float)((double)dir.z + startPos2.z)
        };
    } else {
        target = startNode.GetPosition(); // 0x420A10
    }

    SteerAICarWithPhysicsHeadingForTarget(vehicle, nullptr, target.x, target.y, pSteer, pGas, pBrake, pHandbrake); // 0x433280

    autoPilot.m_nCruiseSpeed = std::min<uint8>(autoPilot.m_nCruiseSpeed, 8);

    // x87: extended precision
    const auto&  pos  = vehicle->GetPosition();
    const double toY  = (double)target.y - pos.y;
    const double toX  = (double)target.x - pos.x;
    const double dist = std::sqrt(toX * toX + toY * toY);

    if (autoPilot.m_nCarMission == MISSION_PARK_PARALLEL) {
        if (dist < 4.0f) {
            autoPilot.m_nCarMission = MISSION_PARK_PARALLEL_2;
        }
    } else if (dist < 2.0f) { // Parked
        autoPilot.m_nCarMission = MISSION_STOP_FOREVER;
        vehicle->vehicleFlags.bEngineOn = false;
        vehicle->vehicleFlags.bLightsOn = false;
        MakeOccupantsLeaveParkedVehicle(vehicle);
    }
}

// 0x433EA0
void CCarCtrl::SteerAICarParkPerpendicular(CVehicle* vehicle, float* pSteer, float* pGas, float* pBrake, bool* pHandbrake) {
    auto& autoPilot = vehicle->m_autoPilot;

    // Note: Only the nodes' areas are checked (not the validity of the node's address)
    const auto startNodes = ThePaths.m_pPathNodes[autoPilot.m_startingRouteNode.m_wAreaId];
    const auto curNodes   = ThePaths.m_pPathNodes[autoPilot.m_currentAddress.m_wAreaId];
    if (!startNodes || !curNodes) {
        autoPilot.m_nCarMission = MISSION_STOP_FOREVER;
        *pBrake = 0.0f;
        *pGas   = 0.0f;
        *pSteer = 0.0f;
        return;
    }
    const auto& startNode = startNodes[autoPilot.m_startingRouteNode.m_wNodeId];
    const auto& curNode   = curNodes[autoPilot.m_currentAddress.m_wNodeId];

    bool headForStartNode = true;
    if (autoPilot.m_nCarMission == MISSION_PARK_PERPENDICULAR) {
        // As long as we are too far from the line (cur -> start) aim for the current node, once we are close enough (to the line) switch to the next phase
        const auto startPos = startNode.GetPosition(); // 0x420A10
        const auto curPos   = curNode.GetPosition();   // 0x420A10
        const CVector lineStart{ startPos.x, startPos.y, 0.0f };
        const CVector lineEnd{ curPos.x, curPos.y, 0.0f };
        const auto&   pos = vehicle->GetPosition();
        const CVector point{ pos.x, pos.y, 0.0f };
        if (CCollision::DistToMathematicalLine(&lineStart, &lineEnd, &point) < 6.0f) { // 0x412970
            autoPilot.m_nCarMission = MISSION_PARK_PERPENDICULAR_2;
        }
        headForStartNode = autoPilot.m_nCarMission != MISSION_PARK_PERPENDICULAR;
    }
    const auto target = (headForStartNode ? startNode : curNode).GetPosition(); // 0x420A10

    SteerAICarWithPhysicsHeadingForTarget(vehicle, nullptr, target.x, target.y, pSteer, pGas, pBrake, pHandbrake); // 0x433280

    autoPilot.m_nCruiseSpeed = std::min<uint8>(autoPilot.m_nCruiseSpeed, 8);

    // Distance to the start node (not the target!). x87: extended precision
    const auto   startPos = startNode.GetPosition(); // 0x420A10
    const auto&  pos      = vehicle->GetPosition();
    const double toY      = (double)startPos.y - pos.y;
    const double toX      = (double)startPos.x - pos.x;
    if (std::sqrt(toX * toX + toY * toY) < 2.0f) { // Parked
        autoPilot.movementFlags.bIsParked = true;
        vehicle->vehicleFlags.bEngineOn   = false;
        vehicle->vehicleFlags.bLightsOn   = false;
        autoPilot.m_nCarMission           = MISSION_STOP_FOREVER;
        MakeOccupantsLeaveParkedVehicle(vehicle);
    }
}

// 0x4336D0
void CCarCtrl::SteerAICarTowardsPointInEscort(CVehicle* vehicle, CVehicle* escorted, float offsetX, float offsetY, float* pSteer, float* pGas, float* pBrake, bool* pHandbrake) {
    // The point (offset in the escorted vehicle's space) we have to reach. Note: The matrices are used directly (not null checked)
    const auto offset = TransformPointOriginal(*escorted->m_matrix, { offsetX, offsetY, 0.0f }); // 0x59C890
    const auto targetX = (float)((double)escorted->m_vecMoveSpeed.x + offset.x);
    const auto targetY = (float)((double)escorted->m_vecMoveSpeed.y + offset.y);

    *pHandbrake = false;

    const auto  dir = GetNormalizedForward2D(vehicle);
    const auto& escortedFwd = escorted->m_matrix->GetForward();
    const auto& pos = vehicle->GetPosition();

    // Steer towards a point a bit ahead of the target. x87: Y stays in extended precision
    const auto   lookAtX = (float)((double)escortedFwd.x * 3.0f + targetX);
    const double lookAtY = (double)escortedFwd.y * 3.0f + targetY;
    const auto   targetAngle = CGeneral::GetATanOfXY((float)((double)lookAtX - pos.x), (float)(lookAtY - pos.y)); // 0x53CC70
    const auto   heading     = CGeneral::GetATanOfXY(dir.x, dir.y); // 0x53CC70
    const auto   weaveAngle  = FindAngleToWeaveThroughTraffic(vehicle, nullptr, targetAngle, heading, 1.0f); // 0x4325C0
    float steer = (float)WrapAngleToPi((double)weaveAngle - heading);
    steer = ClampSteerToMax(vehicle, steer); // 0x427FE0

    // NOTSA: The original also calls `GetATanOfXY(targetX - pos.x, targetY - pos.y)` here, but ignores the result (it has no side effects)

    // How far is the target in front of us, and how far is it in general. x87: extended precision
    const double toTargetY = (double)targetY - pos.y;
    const double toTargetX = (double)targetX - pos.x;
    const double ahead     = toTargetY * dir.y + toTargetX * dir.x;
    const double dist      = std::sqrt(toTargetY * toTargetY + toTargetX * toTargetX);
    const auto   distF     = (float)dist;

    double targetSpeed;
    if (ahead > 0.5f) {
        const auto& escSpeed = escorted->m_vecMoveSpeed;
        const auto  escortedSpeed = (float)(std::sqrt(((double)escSpeed.x * escSpeed.x + (double)escSpeed.y * escSpeed.y) + (double)escSpeed.z * escSpeed.z) * 60.0f);
        if (dist < 15.0f) {
            const double aheadLimited = (ahead - 0.5f) - 0.1f;
            const double extra        = 4.0f < aheadLimited ? 4.0f : aheadLimited;
            const auto   distScaled   = (float)((double)distF * 3.5f); // 0x859028
            if ((double)escortedSpeed + extra > distScaled) {
                targetSpeed = extra + escortedSpeed;
            } else {
                targetSpeed = distScaled;
            }
        } else {
            targetSpeed = 300.0f; // 0x858FD8
        }
        const auto maxSpeed = (float)((double)escortedSpeed + 10.0f);
        if (!(targetSpeed < maxSpeed)) {
            targetSpeed = maxSpeed;
        }
    } else {
        if (dist < 15.0f) { // Target is behind us (or right next to us), just stop (or crawl)
            *pSteer = 0.0f;
            *pGas   = 0.0f;
            *pBrake = ahead < -3.0f ? 1.0f : 0.1f;
            return;
        }
        targetSpeed = 8.0f; // 0x859000
    }

    const auto& ms = vehicle->m_vecMoveSpeed;
    *pBrake = 0.0f;
    const auto   curSpeed  = (float)(std::sqrt(((double)ms.z * ms.z + (double)ms.y * ms.y) + (double)ms.x * ms.x) * 60.0f);
    const double speedDiff = targetSpeed - curSpeed;
    if (speedDiff > 0.0) {
        if (curSpeed < 25.0f) {
            double gas = speedDiff * 0.1f;
            if (1.0f < gas) {
                gas = 1.0f;
            }
            *pGas = (float)gas;
        } else {
            *pGas = 1.0f;
        }
    } else {
        double brake = speedDiff * -0.05f; // 0x85901C
        *pGas = 0.0f;
        if (0.5f < brake) {
            brake = 0.5f;
        }
        *pBrake = (float)brake;
    }
    *pSteer = steer;
}

// 0x437C20
void CCarCtrl::SteerAICarWithPhysics(CVehicle* vehicle) {
    auto& ap = vehicle->m_autoPilot;

    float steer     = 0.0f;
    float gas       = 0.0f;
    float brake     = 0.0f;
    bool  handbrake = false;

    if (ap.m_vehicleRecordingId >= 0 && !CVehicleRecording::bUseCarAI[ap.m_vehicleRecordingId]) {
        return;
    }

    SwitchBetweenPhysicsAndGhost(vehicle); // 0x4222A0
    ap.m_ObstructingEntity = nullptr;

    // Missions that need a target entity are dropped when it is gone
    switch (ap.m_nCarMission) {
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
    case MISSION_PLANE_FOLLOW_ENTITY:
    case MISSION_FOLLOWCAR_FARAWAY:
    case MISSION_FOLLOWCAR_CLOSE:
    case MISSION_KILLPED_FARAWAY:
    case MISSION_KILLPED_CLOSE:
        if (!ap.m_TargetEntity) {
            ap.m_nCarMission = MISSION_NONE;
        }
        break;
    case MISSION_HELI_FOLLOW_ENTITY:
        if (!ap.m_TargetEntity) {
            ap.m_nCarMission = MISSION_HELI_FLY_AWAY_FROM_PLAYER;
        }
        break;
    default:
        break;
    }

    // Resets the temp action once its time is over
    const auto ExpireTempAction = [&] {
        if (CTimer::GetTimeInMS() > ap.m_nTempActionTime) {
            ap.m_nTempAction = TEMPACT_NONE;
        }
    };

    if (ap.movementFlags.bIsStopped) {
        // Waiting for the road nodes around to be loaded
        steer = 0.0f;
        gas   = 0.0f;
        brake = 0.2f; // 0x858CC4

        const auto& pos = vehicle->GetPosition();
        if (ThePaths.AreNodesLoadedForArea(pos.x - 270.0f, pos.x + 270.0f, pos.y - 270.0f, pos.y + 270.0f)) { // 0x44DD10, 0x859070
            ap.movementFlags.bIsStopped = false;
            JoinCarWithRoadAccordingToMission(vehicle); // 0x432CB0
        }
    } else {
        if (ap.movementFlags.bIsParked && ap.m_nCarMission != MISSION_STOP_FOREVER && ap.m_nCarMission != MISSION_NONE) {
            // Leave the parking spot: reverse out of it
            ap.m_nTempAction = TEMPACT_REVERSE_STRAIGHT;
            ap.m_nTempActionTime = CTimer::GetTimeInMS() + 2000;
            ap.movementFlags.bIsParked = false;

            const auto cur   = ap.m_currentAddress;
            const auto start = ap.m_startingRouteNode;
            ap.m_startingRouteNode = cur;
            ap.m_currentAddress    = start;
            ap.m_endingRouteNode   = cur;
        }

        const auto tempAction = ap.m_nTempAction;
        switch (tempAction) {
        case TEMPACT_WAIT:
        case TEMPACT_BRAKE:
            steer     = 0.0f;
            gas       = 0.0f;
            handbrake = false;
            brake     = tempAction == TEMPACT_WAIT ? 0.2f : 1.0f; // 0x858CC4, 0x858624
            if (CTimer::GetTimeInMS() > ap.m_nTempActionTime) {
                ap.m_nTempAction                  = TEMPACT_NONE;
                ap.m_nTimeToStartMission          = CTimer::GetTimeInMS();
                ap.m_nTimeSwitchedToRealPhysics   = CTimer::GetTimeInMS();
            }
            break;
        case TEMPACT_REVERSE: {
            SteerAICarWithPhysics_OnlyMission(vehicle, &steer, &gas, &brake, &handbrake); // 0x436A90
            steer = -steer;
            handbrake = false;

            // x87: The dot product is kept in extended precision
            const auto& moveSpeed = vehicle->m_vecMoveSpeed;
            const auto& fwd       = vehicle->m_matrix->GetForward();
            const auto  dot       = ((double)moveSpeed.z * fwd.z + (double)moveSpeed.y * fwd.y) + (double)moveSpeed.x * fwd.x; // 0x40FDB0
            if (!(dot > 0.04f)) { // 0x858CEC
                gas   = -0.5f;
                brake = 0.0f; // 0x858B50
            } else {
                gas   = 0.0f;
                brake = 0.5f; // 0x858B8C
            }
            if (CTimer::GetTimeInMS() > ap.m_nTempActionTime) {
                ap.m_nTempAction                = TEMPACT_NONE;
                ap.m_nTimeToStartMission        = CTimer::GetTimeInMS();
            }
            break;
        }
        case TEMPACT_HANDBRAKETURNLEFT:
            handbrake = true;
            steer     = 1.0f;
            gas       = 0.0f;
            brake     = 0.0f;
            ExpireTempAction();
            break;
        case TEMPACT_HANDBRAKETURNRIGHT:
            handbrake = true;
            steer     = -1.0f;
            gas       = 0.0f;
            brake     = 0.0f;
            ExpireTempAction();
            break;
        case TEMPACT_HANDBRAKESTRAIGHT:
            handbrake = true;
            steer     = 0.0f;
            gas       = 0.0f;
            brake     = 0.0f;
            ExpireTempAction();
            break;
        case TEMPACT_TURNLEFT:
            handbrake = false;
            steer     = 1.0f;
            gas       = 1.0f;
            brake     = 0.0f;
            ExpireTempAction();
            break;
        case TEMPACT_TURNRIGHT:
            handbrake = false;
            steer     = -1.0f;
            gas       = 1.0f;
            brake     = 0.0f;
            ExpireTempAction();
            break;
        case TEMPACT_GOFORWARD:
            handbrake = false;
            steer     = 0.0f;
            gas       = 0.5f;
            brake     = 0.0f;
            ExpireTempAction();
            break;
        case TEMPACT_SWIRVELEFT:
        case TEMPACT_SWIRVERIGHT:
        case TEMPACT_SWIRVELEFT_STOP:
        case TEMPACT_SWIRVERIGHT_STOP: {
            handbrake = false;
            steer     = (tempAction == TEMPACT_SWIRVERIGHT || tempAction == TEMPACT_SWIRVERIGHT_STOP) ? 0.25f : -0.25f;
            gas       = 0.0f;
            brake     = 0.001f; // 0x858CDC
            if (CTimer::GetTimeInMS() > ap.m_nTempActionTime - 1250u) { // Unsigned wrap-around is intended
                steer = -steer;
            }
            if (CTimer::GetTimeInMS() > ap.m_nTempActionTime) {
                if (tempAction == TEMPACT_SWIRVELEFT_STOP || tempAction == TEMPACT_SWIRVERIGHT_STOP) {
                    ap.m_nTempAction     = TEMPACT_WAIT;
                    ap.m_nTempActionTime = CTimer::GetTimeInMS() + 4000;
                } else {
                    ap.m_nTempAction = TEMPACT_NONE;
                }
            }
            break;
        }
        case TEMPACT_REVERSE_LEFT:
        case TEMPACT_REVERSE_RIGHT: {
            handbrake = false;
            gas       = -0.75f;
            brake     = 0.0f;
            const auto maxSteer = FindMaxSteerAngle(vehicle); // 0x427FE0
            steer = tempAction == TEMPACT_REVERSE_LEFT ? maxSteer : -maxSteer;
            if (CTimer::GetTimeInMS() > ap.m_nTempActionTime) {
                ap.m_nTempAction         = TEMPACT_NONE;
                ap.m_nTimeToStartMission = CTimer::GetTimeInMS();
            }
            break;
        }
        case TEMPACT_PLANE_FLY_UP:
        case TEMPACT_PLANE_FLY_STRAIGHT:
        case TEMPACT_PLANE_SHARP_LEFT:
        case TEMPACT_PLANE_SHARP_RIGHT:
            if (CTimer::GetTimeInMS() > ap.m_nTempActionTime) {
                ap.m_nTempAction = TEMPACT_NONE;
            }
            SteerAICarWithPhysics_OnlyMission(vehicle, &steer, &gas, &brake, &handbrake); // 0x436A90
            break;
        case TEMPACT_HEADON_COLLISION: {
            handbrake = false;
            gas       = 0.0f;
            brake     = CTimer::GetTimeInMS() > ap.m_nTempActionTime ? 1.0f : 0.0f; // 0x858624, 0x858B50

            // x87: The new angle is kept in extended precision (0x859068, 0x859060, 0x859058)
            const auto current = vehicle->m_fSteerAngle;
            const auto delta   = (double)CTimer::GetTimeStep() * 0.05;
            if (current > 0.0f) {
                double angle = delta + current;
                if (0.5 < angle) {
                    angle = 0.5;
                }
                steer = (float)angle;
            } else {
                double angle = current - delta;
                if (-0.5 > angle) {
                    angle = -0.5;
                }
                steer = (float)angle;
            }
            ExpireTempAction();
            break;
        }
        case TEMPACT_REVERSE_STRAIGHT: {
            handbrake = false;
            steer     = 0.0f;

            // x87: The dot product is kept in extended precision
            const auto& moveSpeed = vehicle->m_vecMoveSpeed;
            const auto& fwd       = vehicle->m_matrix->GetForward();
            const auto  dot       = ((double)moveSpeed.z * fwd.z + (double)moveSpeed.y * fwd.y) + (double)moveSpeed.x * fwd.x; // 0x40FDB0
            if (!(dot > 0.1f)) { // 0x858B1C
                gas   = -0.5f;
                brake = 0.0f; // 0x858B50
            } else {
                gas   = 0.0f;
                brake = 0.5f; // 0x858B8C
            }
            ExpireTempAction();
            break;
        }
        case TEMPACT_BOOST_USE_STEERING_ANGLE: {
            handbrake = false;
            steer     = vehicle->m_fSteerAngle;
            gas       = 1.0f;
            brake     = 0.0f;

            // Boost forwards
            const auto scale = CTimer::GetTimeStep() * 0.012f; // 0x859054
            const auto fwd   = vehicle->GetForwardVector();    // 0x41CCB0
            vehicle->m_vecMoveSpeed = vehicle->m_vecMoveSpeed + fwd * scale;
            ExpireTempAction();
            break;
        }
        default: // Includes `TEMPACT_NONE`, `TEMPACT_EMPTYTOBEREUSED` and `TEMPACT_STUCKINTRAFFIC`
            SteerAICarWithPhysics_OnlyMission(vehicle, &steer, &gas, &brake, &handbrake); // 0x436A90
            break;
        }
    }

    vehicle->m_BrakePedal                   = brake;
    vehicle->m_fSteerAngle                  = steer;
    vehicle->vehicleFlags.bIsHandbrakeOn    = handbrake;
    vehicle->m_GasPedal                     = gas;

    if (vehicle->m_nModelIndex == MODEL_VORTEX) {
        // BUG: The original stores into the +0x988 / +0x994 floats, which are beyond the end of a `CAutomobile`
        // (they belong to the bigger vehicle classes, so most likely a wrong model check)
        if (!notsa::IsFixBugs()) {
            auto* const raw = reinterpret_cast<uint8*>(vehicle);
            *reinterpret_cast<float*>(raw + 0x994) = gas;
            *reinterpret_cast<float*>(raw + 0x988) = steer;
        }
    }
}

// 0x434900
void CCarCtrl::SteerAICarWithPhysicsFollowPath(CVehicle* vehicle, float* pSteer, float* pGas, float* pBrake, bool* pHandbrake) {
    if (StopCarIfNodesAreInvalid(vehicle)) { // 0x422590
        return;
    }
    auto& ap = vehicle->m_autoPilot;

    const auto NoInput = [&] {
        *pBrake     = 1.0f;
        *pGas       = 0.0f;
        *pSteer     = 0.0f;
        *pHandbrake = false;
    };

    const auto fwd2D = GetNormalizedForward2D(vehicle); // Note: Not null checked

    if (!ThePaths.IsAreaLoaded(ap.m_nCurrentPathNodeInfo.m_wAreaId) || !ThePaths.IsAreaLoaded(ap.m_nNextPathNodeInfo.m_wAreaId)) {
        NoInput();
        return;
    }

    const auto LinkOf = [](const CCarPathLinkAddress& addr) -> const CCarPathLink& {
        return ThePaths.m_pNaviNodes[addr.m_wAreaId][addr.m_wCarPathLinkId];
    };
    // `CCarPathLink::m_dir`/`m_posn` hide their raw values, but the original works on them
    const auto Raw8  = [](const CCarPathLink& l) { return reinterpret_cast<const int8*>(&l); };
    const auto Raw16 = [](const CCarPathLink& l) { return reinterpret_cast<const int16*>(&l); };

    // Directions of the current and the next link. x87: (dir * 0.01f) * sign, rounded to float (0x858C58)
    float curDirX, curDirY, nextDirX, nextDirY;
    const auto LoadDirs = [&] {
        const auto Dir = [](int8 d, int8 sign) { return (float)((double)d * (double)0.01f * (double)sign); };
        const auto& cur  = LinkOf(ap.m_nCurrentPathNodeInfo);
        const auto& next = LinkOf(ap.m_nNextPathNodeInfo);
        curDirX  = Dir(Raw8(cur)[8], ap._smthCurr);
        curDirY  = Dir(Raw8(cur)[9], ap._smthCurr);
        nextDirX = Dir(Raw8(next)[8], ap._smthNext);
        nextDirY = Dir(Raw8(next)[9], ap._smthNext);
    };
    LoadDirs();

    const auto& pos = vehicle->GetPosition();

    // Lane offset of the current link. x87: rounded to float
    float curK = (float)((LinkOf(ap.m_nCurrentPathNodeInfo).OneWayLaneOffsetExtended() + (double)ap.m_nCurrentLane) * (double)5.4f); // 0x44DB00, 0x858C50
    float distToStart;
    {
        // Lane offset of the next link. x87: NOT rounded to float
        double nextK = (LinkOf(ap.m_nNextPathNodeInfo).OneWayLaneOffsetExtended() + (double)ap.m_nNextLane) * (double)5.4f;
        if (vehicle->m_nVehicleSubType == VEHICLE_TYPE_BMX) {
            constexpr auto BMX_LANE_OFFSET = std::bit_cast<float>(0x3FBA9FBFu); // 0x859010
            curK  = (float)((double)curK + (double)BMX_LANE_OFFSET);
            nextK = nextK + (double)BMX_LANE_OFFSET;
        }

        const auto& cur  = LinkOf(ap.m_nCurrentPathNodeInfo);
        const auto& next = LinkOf(ap.m_nNextPathNodeInfo);
        const float startX = (float)((double)Raw16(cur)[0] * (double)0.125f + (double)curK * curDirY); // 0x858C48
        const float startY = (float)((double)Raw16(cur)[1] * (double)0.125f - (double)curK * curDirX);
        const float endX   = (float)((double)Raw16(next)[0] * (double)0.125f + nextK * nextDirY);
        const float endY   = (float)((double)Raw16(next)[1] * (double)0.125f - nextK * nextDirX);

        // x87: the intermediate values are kept in extended precision
        const double dx = (double)pos.x - startX, dy = (double)pos.y - startY;
        distToStart     = (float)std::sqrt(dy * dy + dx * dx);
        const double ex = (double)endX - startX, ey = (double)endY - startY;
        const float  segLen = (float)std::sqrt(ey * ey + ex * ex);
        const float  dot    = (float)(ey * dy + ex * dx);

        bool needNextNode;
        if (distToStart < 5.0f) { // 0x858C80
            needNextNode = true;
        } else if (dot > 0.0f && distToStart < 8.0f) { // 0x859000
            needNextNode = true;
        } else {
            const auto cosAngle = (double)dot / ((double)segLen * distToStart);
            needNextNode = cosAngle > 0.7f || std::bit_cast<uint16>(ap.m_nNextPathNodeInfo) == std::bit_cast<uint16>(ap.m_nCurrentPathNodeInfo); // 0x858CB0
        }

        if (needNextNode) {
            if (PickNextNodeAccordingStrategy(vehicle)) { // 0x432B10
                // The car got a new node. Some missions switch to "close" variants and use a different steering function
                const auto HeadForTarget = [&](const CVector& target) {
                    SteerAICarWithPhysicsHeadingForTarget(vehicle, nullptr, target.x, target.y, pSteer, pGas, pBrake, pHandbrake); // 0x433280
                };
                switch (ap.m_nCarMission) {
                case MISSION_RAMPLAYER_FARAWAY:
                    ap.m_nCarMission = MISSION_RAMPLAYER_CLOSE;
                    HeadForTarget(FindPlayerCoors()); // 0x56E010
                    return;
                case MISSION_BLOCKPLAYER_FARAWAY:
                    ap.m_nCarMission = MISSION_BLOCKPLAYER_CLOSE;
                    HeadForTarget(FindPlayerCoors()); // 0x56E010
                    return;
                case MISSION_GOTOCOORDINATES:
                    ap.m_nCarMission = MISSION_GOTOCOORDINATES_STRAIGHTLINE;
                    HeadForTarget(ap.m_vecDestinationCoors);
                    return;
                case MISSION_GOTOCOORDINATES_ACCURATE:
                    ap.m_nCarMission = MISSION_GOTOCOORDINATES_STRAIGHTLINE_ACCURATE;
                    HeadForTarget(ap.m_vecDestinationCoors);
                    return;
                case MISSION_RAMCAR_FARAWAY:
                    ap.m_nCarMission = MISSION_RAMCAR_CLOSE;
                    HeadForTarget(ap.m_TargetEntity->GetPosition()); // Note: Not null checked
                    return;
                case MISSION_BLOCKCAR_FARAWAY:
                    ap.m_nCarMission = MISSION_BLOCKCAR_CLOSE;
                    HeadForTarget(ap.m_TargetEntity->GetPosition()); // Note: Not null checked
                    return;
                case MISSION_APPROACHPLAYER_FARAWAY:
                    ap.m_nCarMission = MISSION_APPROACHPLAYER_CLOSE;
                    HeadForTarget(FindPlayerCoors()); // 0x56E010
                    return;
                case MISSION_ESCORT_LEFT_FARAWAY:
                case MISSION_ESCORT_RIGHT_FARAWAY:
                case MISSION_ESCORT_REAR_FARAWAY:
                case MISSION_ESCORT_FRONT_FARAWAY:
                    ap.m_nCarMission = (eCarMission)(ap.m_nCarMission - 0x24); // MISSION_ESCORT_LEFT.. MISSION_ESCORT_FRONT
                    HeadForTarget(ap.m_TargetEntity->GetPosition()); // Note: Not null checked
                    return;
                default:
                    break;
                }
            }

            // Either there's no new node, or it wasn't handled above => recalculate the stuff for the (maybe new) link
            if (!ThePaths.IsAreaLoaded(ap.m_nCurrentPathNodeInfo.m_wAreaId) || !ThePaths.IsAreaLoaded(ap.m_nNextPathNodeInfo.m_wAreaId)) {
                NoInput();
                return;
            }

            // Note: The BMX offset isn't applied here, and the (old) directions are used for the distance
            const auto& newCur = LinkOf(ap.m_nCurrentPathNodeInfo);
            curK = (float)((newCur.OneWayLaneOffsetExtended() + (double)ap.m_nCurrentLane) * (double)5.4f); // 0x44DB00, 0x858C50
            const double dx2 = ((double)Raw16(newCur)[0] * (double)0.125f + (double)curK * curDirY) - pos.x;
            const double dy2 = ((double)Raw16(newCur)[1] * (double)0.125f - (double)curK * curDirX) - pos.y;
            distToStart = (float)std::sqrt(dy2 * dy2 + dx2 * dx2);
            LoadDirs();
        }
    }

    // The point we steer to
    const auto& curLink = LinkOf(ap.m_nCurrentPathNodeInfo);
    const auto  curRaw16 = Raw16(curLink);

    const double kDirY = (double)curK * curDirY;
    const float  startX = (float)((double)curRaw16[0] * (double)0.125f + kDirY);
    const float  kDirXf = (float)((double)curK * curDirX); // x87: rounded to float here
    const float  startY = (float)((double)curRaw16[1] * (double)0.125f - kDirXf);

    float targetX = (float)((double)startX - ((double)distToStart * curDirX) * (double)0.35f); // 0x858F9C
    float targetY = (float)((double)startY - ((double)distToStart * curDirY) * (double)0.35f);
    if (distToStart > 40.0f) { // 0x858A10
        targetX = startX;
        targetY = startY;
    }

    const auto targetAngle = CGeneral::GetATanOfXY((float)((double)targetX - pos.x), (float)((double)targetY - pos.y)); // 0x53CC70
    const auto heading     = CGeneral::GetATanOfXY(fwd2D.x, fwd2D.y); // 0x53CC70

    float angle;
    switch (ap.m_nCarDrivingStyle) {
    case DRIVING_STYLE_AVOID_CARS:
    case DRIVING_STYLE_DRIVINGMODE_AVOIDCARS_OBEYLIGHTS:
    case DRIVING_STYLE_DRIVINGMODE_AVOIDCARS_STOPFORPEDS_OBEYLIGHTS:
        angle = FindAngleToWeaveThroughTraffic(vehicle, nullptr, targetAngle, heading, 1.0f); // 0x4325C0
        break;
    default:
        angle = targetAngle;
        break;
    }
    float steer = (float)WrapAngleToPi((double)angle - heading);
    steer = ClampSteerToMax(vehicle, steer); // 0x427FE0

    // The speed along the forward direction. x87: kept in extended precision until stored
    const auto& vehFwd  = vehicle->m_matrix->GetForward();
    const auto& moveSpd = vehicle->m_vecMoveSpeed;
    const auto  curSpeed = (float)((((double)moveSpd.z * vehFwd.z + (double)moveSpd.y * vehFwd.y) + (double)moveSpd.x * vehFwd.x) * (double)60.0f); // 0x858B34

    // The speed (factor of the cruise speed) allowed by the traffic
    float speedFactor;
    switch (ap.m_nCarDrivingStyle) {
    case DRIVING_STYLE_STOP_FOR_CARS:
    case DRIVING_STYLE_SLOW_DOWN_FOR_CARS:
    case DRIVING_STYLE_STOP_FOR_CARS_IGNORE_LIGHTS:
    case DRIVING_STYLE_DRIVINGMODE_AVOIDCARS_STOPFORPEDS_OBEYLIGHTS:
        speedFactor = (float)(FindMaximumSpeedForThisCarInTrafficOriginal(vehicle) / (int32)ap.m_nCruiseSpeed); // 0x434400
        break;
    default:
        speedFactor = 1.0f;
        break;
    }
    switch (ap.m_nCarDrivingStyle) {
    case DRIVING_STYLE_STOP_FOR_CARS:
    case DRIVING_STYLE_SLOW_DOWN_FOR_CARS:
    case DRIVING_STYLE_DRIVINGMODE_AVOIDCARS_OBEYLIGHTS:
    case DRIVING_STYLE_DRIVINGMODE_AVOIDCARS_STOPFORPEDS_OBEYLIGHTS:
        if (CTrafficLights::ShouldCarStopForLight(vehicle, false)) { // 0x49D610
            CCarAI::CarHasReasonToStop(vehicle); // 0x41C050
            speedFactor = 0.0f;
        }
        break;
    default:
        break;
    }
    if (CTrafficLights::ShouldCarStopForBridge(vehicle)) { // 0x49D420
        CCarAI::CarHasReasonToStop(vehicle); // 0x41C050
        speedFactor = 0.0f;
    }

    // Slow down for bends. x87: `curK2` is not rounded to float
    const auto curK2 = (curLink.OneWayLaneOffsetExtended() + (double)ap.m_nCurrentLane) * (double)5.4f; // 0x44DB00, 0x858C50
    const auto bendStartX = (float)(((double)curRaw16[0] * (double)0.125f + curK2 * curDirY) - pos.x);
    const auto bendStartY = (float)(((double)curRaw16[1] * (double)0.125f - curK2 * curDirX) - pos.y);
    const auto angleToBend = CGeneral::GetATanOfXY(bendStartX, bendStartY); // 0x53CC70
    const float turnFactor1 = FindSpeedMultiplier((float)((double)angleToBend - heading), 0.4f, 1.2f, 0.4f); // 0x4224E0
    const auto curDirAngle  = CGeneral::GetATanOfXY(curDirX, curDirY); // 0x53CC70
    const auto nextDirAngle = CGeneral::GetATanOfXY(nextDirX, nextDirY); // 0x53CC70
    const float turnFactor2 = FindSpeedMultiplier((float)((double)curDirAngle - nextDirAngle), 0.1f, 1.2f, 0.4f); // 0x4224E0

    double limit = 1.0;
    if (!(distToStart > 40.0f) && ap.m_nCruiseSpeed >= 12) { // 0x858A10
        limit = 1.0 - (1.0 - turnFactor2) * (1.0 - (double)distToStart * 0.025f); // 0x859038
    }

    float factor;
    const double lower = turnFactor1 < limit ? (double)turnFactor1 : limit;
    if (!(lower < speedFactor)) {
        factor = speedFactor;
    } else {
        factor = turnFactor1 < limit ? turnFactor1 : (float)limit;
    }

    *pBrake = 0.0f;
    const double targetSpeed = (double)ap.m_nCruiseSpeed * factor;
    const float  speedDiff   = (float)(targetSpeed - curSpeed);
    if (targetSpeed < 0.05f && speedDiff < 0.03f) { // 0x858C28, 0x858B10
        *pBrake = 1.0f;
        *pGas   = 0.0f;
    } else if (!(speedDiff > 0.0f)) {
        const double brake = (double)speedDiff * -(1.0f / 12.0f); // 0x859034
        *pGas   = 0.0f;
        *pBrake = 0.5f < brake ? 0.5f : (float)brake; // 0x858B8C
    } else {
        const double gas = (double)speedDiff * (!(curSpeed < 2.0f) ? 0.125f : 0.25f); // 0x858CA0, 0x858C48, 0x858C84
        *pGas = 1.0f < gas ? 1.0f : (float)gas;

        if (vehicle->m_nVehicleSubType == VEHICLE_TYPE_BMX && speedDiff > 3.0f) { // 0x858B3C
            auto* const bmx = static_cast<CBmx*>(vehicle);
            if (bmx->m_fControlPedaling <= 0.0f) { // Note: "always 0.0f" according to the header
                bmx->m_fControlPedaling = 10.0f;
            }
        }
    }
    *pSteer     = steer;
    *pHandbrake = false;

    // Go-to-coordinates missions: if we're (almost) there and the destination is behind us => reverse
    if ((ap.m_nCarMission == MISSION_GOTOCOORDINATES || ap.m_nCarMission == MISSION_GOTOCOORDINATES_ACCURATE) && ap.m_nTempAction == TEMPACT_NONE) {
        const CVector toDest = pos - ap.m_vecDestinationCoors; // 0x40FE60
        // x87: kept in extended precision (0x4082C0)
        const double distToDest = std::sqrt(((double)toDest.x * toDest.x + (double)toDest.y * toDest.y) + (double)toDest.z * toDest.z);
        if (distToDest < 8.0f) { // 0x859000
            // BUG: This uses the position of the vehicle, not the vector to the destination
            const auto& fwd3D = vehicle->m_matrix->GetForward();
            double dot = ((double)pos.z * fwd3D.z + (double)pos.y * fwd3D.y) + (double)pos.x * fwd3D.x; // 0x40FDB0
            if (dot < 0.0) {
                dot = -dot;
            }
            if (dot / (float)distToDest < 0.2f) { // 0x858CC4
                ap.m_nTempAction     = TEMPACT_REVERSE;
                ap.m_nTempActionTime = CTimer::GetTimeInMS() + 2000;
            }
        }
    }
}

// 0x435830
void CCarCtrl::SteerAICarWithPhysicsFollowPath_Racing(CVehicle* vehicle, float* pSteer, float* pGas, float* pBrake, bool* pHandbrake) {
    if (StopCarIfNodesAreInvalid(vehicle)) { // 0x422590
        return;
    }
    auto& ap = vehicle->m_autoPilot;

    // Speed along the forward direction. x87: kept in extended precision until stored
    const auto& vehFwd   = vehicle->m_matrix->GetForward(); // Note: Not null checked
    const auto& moveSpd  = vehicle->m_vecMoveSpeed;
    const auto  curSpeed = (float)((((double)moveSpd.z * vehFwd.z + (double)moveSpd.y * vehFwd.y) + (double)moveSpd.x * vehFwd.x) * (double)60.0f); // 0x858B34

    const auto fwd2D = GetNormalizedForward2D(vehicle);

    if (!ThePaths.IsAreaLoaded(ap.m_nCurrentPathNodeInfo.m_wAreaId) || !ThePaths.IsAreaLoaded(ap.m_nNextPathNodeInfo.m_wAreaId)) {
        *pBrake     = 1.0f;
        *pGas       = 0.0f;
        *pSteer     = 0.0f;
        *pHandbrake = false;
        return;
    }

    const auto LinkOf = [](const CCarPathLinkAddress& addr) -> const CCarPathLink& {
        return ThePaths.m_pNaviNodes[addr.m_wAreaId][addr.m_wCarPathLinkId];
    };
    // `CCarPathLink::m_posn` hides its raw values, but the original works on them
    const auto LinkPos = [&](const CCarPathLinkAddress& addr) -> CVector2D {
        const auto raw = reinterpret_cast<const int16*>(&LinkOf(addr));
        return { (float)raw[0] * 0.125f, (float)raw[1] * 0.125f }; // 0x858C48
    };
    const auto RawOf = [](const CCarPathLinkAddress& addr) { return std::bit_cast<uint16>(addr); };
    const auto IsBefore = [](const CNodeAddress& a, const CNodeAddress& b) { // 0x420980 - Compares the (area, node) pair
        return a.m_wAreaId < b.m_wAreaId || (a.m_wAreaId == b.m_wAreaId && a.m_wNodeId < b.m_wNodeId);
    };

    const auto& pos = vehicle->GetPosition();

    // Distance to the current link, and the angle between the direction towards it and the direction of the road
    float distToCur;
    bool  needNextNode;
    {
        const auto curPos  = LinkPos(ap.m_nCurrentPathNodeInfo);
        const auto nextPos = LinkPos(ap.m_nNextPathNodeInfo);

        // x87: the intermediate values are kept in extended precision
        const double dx = (double)pos.x - curPos.x, dy = (double)pos.y - curPos.y;
        distToCur       = (float)std::sqrt(dy * dy + dx * dx);
        const double ex = (double)nextPos.x - curPos.x, ey = (double)nextPos.y - curPos.y;
        const float  segLen = (float)std::sqrt(ey * ey + ex * ex);
        const float  dot    = (float)(ey * dy + ex * dx);

        if (distToCur < 8.0f) { // 0x859000
            needNextNode = true;
        } else if (dot > 0.0f && distToCur < 12.0f) { // 0x858CCC
            needNextNode = true;
        } else {
            const auto cosAngle = (double)dot / ((double)segLen * distToCur);
            needNextNode = cosAngle > 0.7f || RawOf(ap.m_nNextPathNodeInfo) == RawOf(ap.m_nCurrentPathNodeInfo); // 0x858CB0
        }
    }

    if (needNextNode) {
        if ((int16)ap.m_nPathFindNodesCount < 4) {
            // Find the next nodes of the route
            constexpr auto MAX_DIST = std::bit_cast<float>(0x497423FEu); // ~999999.9
            const auto     forbiddenNode = StaticRef<CNodeAddress>(0x8A5F44); // (area = -1, node = 0), effectively none
            const auto     oneSide       = (bool)ap.carCtrlFlags.bCantGoAgainstTraffic;
            const auto     forVortex     = vehicle->m_nModelIndex == MODEL_VORTEX;
            auto&          numNodes      = reinterpret_cast<int16&>(ap.m_nPathFindNodesCount);
            ThePaths.DoPathSearch(PATH_TYPE_VEH, pos, ap.m_startingRouteNode, ap.m_vecDestinationCoors, ap.m_aPathFindNodesInfo.data(), numNodes, 8, nullptr, MAX_DIST, nullptr, MAX_DIST, oneSide, forbiddenNode, forVortex, false); // 0x4515D0
            if (numNodes > 0) {
                ap.RemoveOnePathNode(); // 0x41B950
                if (numNodes > 0 && ap.m_startingRouteNode == ap.m_aPathFindNodesInfo[0]) {
                    ap.RemoveOnePathNode(); // 0x41B950
                }
            }
            if (numNodes < 1) { // No route
                ap.m_nCarMission = MISSION_GOTOCOORDINATES_STRAIGHTLINE;
                SteerAICarWithPhysicsHeadingForTarget(vehicle, nullptr, ap.m_vecDestinationCoors.x, ap.m_vecDestinationCoors.y, pSteer, pGas, pBrake, pHandbrake); // 0x433280
                return;
            }
        }

        // Move on to the next node
        ap.m_endingRouteNode   = ap.m_currentAddress;
        ap.m_currentAddress    = ap.m_startingRouteNode;
        ap.m_startingRouteNode = ap.m_aPathFindNodesInfo[0];
        ap.RemoveOnePathNode(); // 0x41B950

        const auto prevLink = ap.m_nCurrentPathNodeInfo;
        ap.m_nCurrentPathNodeInfo  = ap.m_nNextPathNodeInfo;
        ap._smthPrev               = ap._smthCurr;
        ap.m_nCurrentLane          = ap.m_nNextLane;
        ap.m_nPreviousPathNodeInfo = prevLink;
        ap._smthCurr               = ap._smthNext;

        // Find the link that leads to the (new) starting node (BUG: There's no bounds check)
        const auto  cur      = ap.m_currentAddress;
        const auto  linkBase = (int32)ThePaths.m_pPathNodes[cur.m_wAreaId][cur.m_wNodeId].m_wBaseLinkId;
        int32       linkIdx  = 0;
        while (!(ThePaths.m_pNodeLinks[cur.m_wAreaId][linkBase + linkIdx] == ap.m_startingRouteNode)) {
            linkIdx++;
        }
        ap.m_nNextPathNodeInfo = ThePaths.m_pNaviLinks[cur.m_wAreaId][linkBase + linkIdx];
        ap._smthNext           = IsBefore(cur, ap.m_startingRouteNode) ? -1 : 1; // 0x420980
    }

    // The links ahead of us (the 2 we're on, and the ones up to 4 nodes ahead) and in which direction (-1/1) we're going along them
    CCarPathLinkAddress links[8]{}; // Note: Set to invalid
    int8                dirs[8]{};  // NOTSA: Zero initialized. Some of these remain uninitialized in the original (but are only used with invalid links)
    CNodeAddress        nodes[8]{};
    links[0] = ap.m_nCurrentPathNodeInfo;
    links[1] = ap.m_nNextPathNodeInfo;
    dirs[0]  = ap._smthCurr;
    dirs[1]  = ap._smthNext;
    nodes[0] = ap.m_currentAddress;
    nodes[1] = ap.m_startingRouteNode;
    for (auto i = 0; i < 4; i++) {
        const auto node = ap.m_aPathFindNodesInfo[i];
        if (i < (int16)ap.m_nPathFindNodesCount && node.m_wAreaId != 0xFFFF) {
            nodes[2 + i] = node;
            links[2 + i] = ThePaths.FindLinkBetweenNodes(nodes[1 + i], node); // 0x451350
            dirs[2 + i]  = IsBefore(nodes[1 + i], node) ? -1 : 1;      // 0x420980
        } else {
            links[2 + i] = CCarPathLinkAddress{};
            nodes[2 + i].m_wAreaId = 0xFFFF;
        }
    }

    // Is there a bend ahead?
    float    outOrientation = distToCur; // Note: Initial value is irrelevant
    float    outSpeedLimit  = 1.0f;
    float    outUnk3        = LinkPos(ap.m_nCurrentPathNodeInfo).y; // (Unused after the call)
    float    outUnk4        = LinkPos(ap.m_nNextPathNodeInfo).x;    // (Unused after the call)
    CVector  outPos;
    uint16   bendLink = 0xFFFF;
    for (auto i = 0; i < 4; i++) {
        if (DealWithBend_Racing(vehicle, links[i], links[i + 1], links[i + 2], links[i + 3], dirs[i], dirs[i + 1], dirs[i + 2], dirs[i + 3], curSpeed, &outOrientation, &outSpeedLimit, &outUnk3, &outUnk4, &outPos)) { // 0x428040
            bendLink = RawOf(links[i]);
            break;
        }
    }

    const auto heading = CGeneral::GetATanOfXY(fwd2D.x, fwd2D.y); // 0x53CC70

    const auto ClipOrientation = [&](const CCarPathLinkAddress& link, int8 dirSign, float x, float y) {
        ClitargetOrientationToLink(vehicle, link, dirSign, &outOrientation, x, y); // 0x422760
    };
    const auto ClipOrientationToLinkPos = [&](const CCarPathLinkAddress& link, int8 dirSign) {
        const auto lp = LinkPos(link);
        ClipOrientation(link, dirSign, lp.x, lp.y);
    };

    if (bendLink == 0xFFFF) {
        outOrientation = heading;
        for (auto i = 5; i >= 2; i--) {
            if (RawOf(links[i]) != 0xFFFF) {
                ClipOrientationToLinkPos(links[i], dirs[i]);
            }
        }
        ClipOrientationToLinkPos(ap.m_nNextPathNodeInfo, ap._smthNext);

        // x87: kept in extended precision
        const double t  = FindPercDependingOnDistToLinkOriginal(vehicle, ap.m_nCurrentPathNodeInfo); // 0x422620
        const double tm = 1.0 - t;
        const auto   curP  = LinkPos(ap.m_nCurrentPathNodeInfo);
        const auto   nextP = LinkPos(ap.m_nNextPathNodeInfo);
        const float  y     = (float)((double)curP.y * t + (double)nextP.y * tm);
        const float  x     = (float)((double)curP.x * t + (double)nextP.x * tm);
        ClipOrientation(ap.m_nCurrentPathNodeInfo, ap._smthCurr, x, y);
    } else {
        if (RawOf(ap.m_nCurrentPathNodeInfo) != bendLink) {
            ClipOrientationToLinkPos(ap.m_nNextPathNodeInfo, ap._smthNext);
        }

        // x87: kept in extended precision
        const double t  = FindPercDependingOnDistToLinkOriginal(vehicle, ap.m_nCurrentPathNodeInfo); // 0x422620
        const double u  = (t + 2.0f) * 0.33333334f; // 0x859040
        const double um = 1.0 - u;
        const auto   curP  = LinkPos(ap.m_nCurrentPathNodeInfo);
        const auto   nextP = LinkPos(ap.m_nNextPathNodeInfo);
        const float  y     = (float)((double)nextP.y * um + (double)curP.y * u);
        const float  x     = (float)((double)nextP.x * um + (double)curP.x * u);
        ClipOrientation(ap.m_nCurrentPathNodeInfo, ap._smthCurr, x, y);
    }

    const auto weaveAngle = FindAngleToWeaveThroughTraffic(vehicle, nullptr, outOrientation, heading, 2.0f); // 0x4325C0
    const double wrapped  = WrapAngleToPi((double)weaveAngle - heading);
    float  steer          = (float)wrapped;
    bool   steerLimited   = false;

    // Limit the steering depending on the speed. x87: the length is kept in extended precision
    const auto speedLen = std::sqrt(((double)moveSpd.x * moveSpd.x + (double)moveSpd.y * moveSpd.y) + (double)moveSpd.z * moveSpd.z);
    float steerLimit;
    if (speedLen > 0.7f) { // 0x858CB0
        steerLimit = 0.2f; // 0x858CC4
    } else {
        const auto d = (double)0.9f - speedLen; // 0x858C20
        steerLimit   = 0.7f < d ? 0.7f : (float)d;
    }
    const float negSteerLimit = -steerLimit;
    double      steerExt      = wrapped;
    if (steerExt < negSteerLimit) {
        steerLimited = true;
        steerExt     = negSteerLimit;
        steer        = negSteerLimit;
    }
    if (steerExt > steerLimit) {
        steer        = steerLimit;
        steerLimited = true;
    }

    float trafficLimit = 1.0f;
    if (CTrafficLights::ShouldCarStopForBridge(vehicle)) { // 0x49D420
        CCarAI::CarHasReasonToStop(vehicle); // 0x41C050
        trafficLimit = 0.0f;
    }
    const float speedFactor = trafficLimit < outSpeedLimit ? trafficLimit : outSpeedLimit;

    *pBrake = 0.0f;
    const double targetSpeed = (double)speedFactor * (int32)ap.m_nCruiseSpeed;
    const float  speedDiff   = (float)(targetSpeed - curSpeed);
    if (targetSpeed < 0.05f && speedDiff < 0.03f) { // 0x858C28, 0x858B10
        *pBrake = 1.0f;
        *pGas   = 0.0f;
    } else if (!(speedDiff > 0.0f)) {
        *pGas = 0.0f;
        if (vehicle->m_nVehicleSubType == VEHICLE_TYPE_BMX) {
            const double brake = (double)speedDiff * -0.06666667f; // 0x85903C
            *pBrake = 1.0f < brake ? 1.0f : (float)brake;
        } else {
            const double brake = (double)speedDiff * -0.05f; // 0x85901C
            *pBrake = 0.5f < brake ? 0.5f : (float)brake; // 0x858B8C
        }
    } else if (steerLimited && curSpeed > 7.0f) { // 0x858F48 - No gas in the bends
        *pGas = 0.0f;
    } else {
        const double gas = (double)speedDiff * (!(curSpeed < 2.0f) ? 0.125f : 0.25f); // 0x858CA0, 0x858C48, 0x858C84
        *pGas = 1.0f < gas ? 1.0f : (float)gas;

        if (vehicle->m_nVehicleSubType == VEHICLE_TYPE_BMX && speedDiff > 3.0f) { // 0x858B3C
            auto* const bmx = static_cast<CBmx*>(vehicle);
            if (bmx->m_fControlPedaling <= 0.0f) { // Note: "always 0.0f" according to the header
                bmx->m_fControlPedaling = 15.0f;
            }
        }
    }
    *pSteer     = steer;
    *pHandbrake = false;
}

// 0x432DD0
void CCarCtrl::SteerAICarWithPhysicsFollowPreRecordedPath(CVehicle* vehicle, float* pSteer, float* pGas, float* pBrake, bool* pHandbrake) {
    const auto StopRecordedPath = [&] {
        *pBrake     = 0.0f;
        *pGas       = 0.0f;
        *pSteer     = 0.0f;
        *pHandbrake = false;
        vehicle->m_autoPilot.m_nCarMission = MISSION_STOP_FOREVER;
    };

    const int32 id = vehicle->m_autoPilot.m_vehicleRecordingId;
    if (id < 0) {
        StopRecordedPath();
        return;
    }

    const auto buffer = reinterpret_cast<const uint8*>(CVehicleRecording::pPlaybackBuffer[id]);
    if (!buffer) {
        StopRecordedPath();
        return;
    }

    if (CVehicleRecording::bPlaybackPaused[id]) {
        *pSteer     = 0.0f;
        *pGas       = 0.0f;
        *pBrake     = 0.5f;
        *pHandbrake = false;
        return;
    }

    auto& playbackOffset = CVehicleRecording::PlaybackIndex[id]; // Byte offset into the buffer (it's advanced by the size of a frame)
    const auto FrameAt = [&](int32 byteOffset) -> const CVehicleStateEachFrame& {
        return *reinterpret_cast<const CVehicleStateEachFrame*>(buffer + byteOffset);
    };
    // x87: the squared distance is kept in extended precision
    const auto DistTo = [&](const CVector& p) {
        const auto& pos = vehicle->GetPosition();
        const double dx = (double)pos.x - p.x, dy = (double)pos.y - p.y, dz = (double)pos.z - p.z;
        return std::sqrt((dz * dz + dy * dy) + dx * dx);
    };

    // BUG: The original keeps using the frame it started with (instead of the one it advanced to) for the rest of the function
    const auto& startFrame = FrameAt(playbackOffset);
    const CVehicleStateEachFrame* frame = &startFrame;
    while (true) {
        const auto& cur  = FrameAt(playbackOffset);
        const auto& next = FrameAt(playbackOffset + (int32)sizeof(CVehicleStateEachFrame));
        const auto  distToCur  = DistTo(cur.m_vecPosn);
        const auto  distToNext = DistTo(next.m_vecPosn);
        if (!(distToCur < 10.0f) && !(distToNext < distToCur)) {
            break;
        }

        playbackOffset += (int32)sizeof(CVehicleStateEachFrame);
        if ((uint32)playbackOffset >= (uint32)(CVehicleRecording::PlaybackBufferSize[id] - (int32)sizeof(CVehicleStateEachFrame))) { // Reached the end
            CVehicleRecording::StopPlaybackWithIndex(id); // 0x459440
            vehicle->m_autoPilot.m_vehicleRecordingId = -1;
            StopRecordedPath();
            return;
        }
        if (notsa::IsFixBugs()) {
            frame = &FrameAt(playbackOffset);
        }
    }

    const auto& vehFwd = vehicle->m_matrix->GetForward(); // Note: Not null checked
    const auto  heading = CGeneral::GetATanOfXY(vehFwd.x, vehFwd.y); // 0x53CC70
    const auto  targetAngle = CGeneral::GetATanOfXY( // 0x53CC70
        (float)((double)frame->m_vecPosn.x - vehicle->GetPosition().x),
        (float)((double)frame->m_vecPosn.y - vehicle->GetPosition().y)
    );
    const auto weaveAngle = FindAngleToWeaveThroughTraffic(vehicle, nullptr, targetAngle, heading, 2.0f); // 0x4325C0
    float steer = (float)WrapAngleToPi((double)weaveAngle - heading);
    steer = ClampSteerToMax(vehicle, steer); // 0x427FE0

    // Speed we should have. x87: kept in extended precision (the scale is 1/16383.5, 0x858EAC)
    constexpr auto VELOCITY_SCALE = std::bit_cast<float>(0x38800100u);
    const auto     vel            = std::bit_cast<std::array<int16, 3>>(frame->m_sVelocity);
    const double   velX           = (double)vel[0] * VELOCITY_SCALE;
    const double   velY           = (double)vel[1] * VELOCITY_SCALE;
    double targetSpeed = std::sqrt(velX * velX + velY * velY) * CVehicleRecording::PlaybackSpeed[id] * 60.0f;
    if ((uint32)playbackOffset <= 0x320u && !(targetSpeed > 5.0f)) {
        targetSpeed = 5.0f;
    }

    *pBrake = 0.0f;
    const auto curSpeed = (float)(std::sqrt((double)vehicle->m_vecMoveSpeed.x * vehicle->m_vecMoveSpeed.x + (double)vehicle->m_vecMoveSpeed.y * vehicle->m_vecMoveSpeed.y) * 60.0f);
    const double speedDiff = targetSpeed - curSpeed;
    if (speedDiff > 0.0) {
        double gas = speedDiff * (curSpeed < 2.0f ? 0.25f : 0.125f);
        if (1.0f < gas) {
            gas = 1.0f;
        }
        *pGas = (float)gas;
    } else {
        double brake = speedDiff * -0.05f; // 0x85901C
        *pGas = 0.0f;
        if (0.5f < brake) {
            brake = 0.5f;
        }
        *pBrake = (float)brake;
    }
    *pSteer     = steer;
    *pHandbrake = false;
}

// 0x433280
void CCarCtrl::SteerAICarWithPhysicsHeadingForTarget(CVehicle* vehicle, CPhysical* target, float x, float y, float* pSteer, float* pGas, float* pBrake, bool* pHandbrake) {
    *pHandbrake = false;

    const auto dir = GetNormalizedForward2D(vehicle);

    const auto& pos         = vehicle->GetPosition();
    const auto  targetAngle = CGeneral::GetATanOfXY((float)((double)x - pos.x), (float)((double)y - pos.y)); // 0x53CC70
    const auto  heading     = CGeneral::GetATanOfXY(dir.x, dir.y); // 0x53CC70

    float angle = targetAngle;
    const auto drivingStyle = vehicle->m_autoPilot.m_nCarDrivingStyle;
    if (   drivingStyle == DRIVING_STYLE_AVOID_CARS
        || drivingStyle == DRIVING_STYLE_DRIVINGMODE_AVOIDCARS_OBEYLIGHTS
        || drivingStyle == DRIVING_STYLE_DRIVINGMODE_AVOIDCARS_STOPFORPEDS_OBEYLIGHTS
    ) {
        angle = FindAngleToWeaveThroughTraffic(vehicle, target, targetAngle, heading, 1.0f); // 0x4325C0
    }

    // x87: the wrapped difference stays in extended precision (used for the handbrake below), the steering is rounded to float
    const double angleDiff = WrapAngleToPi((double)angle - heading);
    float        steer     = (float)angleDiff;

    // Handbrake when moving and the turn is too sharp
    {
        const auto& ms = vehicle->m_vecMoveSpeed;
        const double speed = std::sqrt(((double)ms.x * ms.x + (double)ms.y * ms.y) + (double)ms.z * ms.z);
        if (0.3f < speed) {
            if (0.7f < (angleDiff < 0.0 ? -angleDiff : angleDiff)) {
                *pHandbrake = true;
            }
        }
    }

    steer = ClampSteerToMax(vehicle, steer); // 0x427FE0

    const auto& pos2 = vehicle->GetPosition();
    const auto  targetAngle2 = CGeneral::GetATanOfXY((float)((double)x - pos2.x), (float)((double)y - pos2.y)); // 0x53CC70
    const auto  speedMult = FindSpeedMultiplier((float)((double)targetAngle2 - heading), 0.4f, 1.2f, 0.4f); // 0x4224E0

    const auto& ms = vehicle->m_vecMoveSpeed;
    *pBrake = 0.0f;
    const auto curSpeed = (float)(std::sqrt(((double)ms.z * ms.z + (double)ms.y * ms.y) + (double)ms.x * ms.x) * 60.0f);
    const double speedDiff = (double)speedMult * (int32)vehicle->m_autoPilot.m_nCruiseSpeed - curSpeed;

    if (!(speedDiff > 0.0)) {
        double brake = speedDiff * -0.05f; // 0x85901C
        *pGas = 0.0f;
        if (0.5f < brake) {
            brake = 0.5f;
        }
        *pBrake = (float)brake;
        *pSteer = steer;
        return;
    }

    if (curSpeed < 25.0f) {
        double gas = 0.1f * speedDiff;
        if (1.0f < gas) {
            gas = 1.0f;
        }
        *pGas = (float)gas;
    } else {
        *pGas = 1.0f;
    }

    // Bikes (BMX) have to be kept pedaling/jumping
    if (vehicle->m_nVehicleSubType == VEHICLE_TYPE_BMX && 3.0f < speedDiff) {
        auto* const bmx = static_cast<CBmx*>(vehicle);
        if (bmx->m_fControlPedaling <= 0.0f) { // Note: "always 0.0f" according to the header
            bmx->m_fControlPedaling = 10.0f;
        }
    }
    *pSteer = steer;
}

// 0x4335E0
void CCarCtrl::SteerAICarWithPhysicsTryingToBlockTarget(CVehicle* vehicle, CEntity* Unusued, float x, float y, float dirX, float dirY, float* pSteer, float* pGas, float* pBrake, bool* pHandbrake) {
    // x87: the (limited) direction stays in extended precision
    double dx = dirX, dy = dirY;
    const double len = std::sqrt((double)dirX * dirX + (double)dirY * dirY);
    if (len > 0.13f) { // 0x859020
        const double factor = 0.13f / len;
        dx *= factor;
        dy *= factor;
    }
    const auto targetX = (float)(dx * 60.0f + x);
    const auto targetY = (float)(dy * 60.0f + y);

    vehicle->m_autoPilot.m_nCarDrivingStyle = DRIVING_STYLE_AVOID_CARS;
    SteerAICarWithPhysicsHeadingForTarget(vehicle, nullptr, targetX, targetY, pSteer, pGas, pBrake, pHandbrake); // 0x433280

    // Close enough to the target? (Stop and block)
    const auto&  pos    = vehicle->GetPosition();
    const double distY  = (double)targetY - pos.y;
    const double distX  = (double)targetX - pos.x;
    if (distY * distY + distX * distX < 25.0f) {
        auto& mission = vehicle->m_autoPilot.m_nCarMission;
        mission = mission == MISSION_BLOCKCAR_CLOSE
            ? MISSION_BLOCKCAR_HANDBRAKESTOP
            : MISSION_BLOCKPLAYER_HANDBRAKESTOP;
    }
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
void CCarCtrl::SteerAICarWithPhysics_OnlyMission(CVehicle* vehicle, float* pSteer, float* pGas, float* pBrake, bool* pHandbrake) {
    auto& ap = vehicle->m_autoPilot;

    const auto HeadForTarget = [&](CPhysical* target, float x, float y) {
        SteerAICarWithPhysicsHeadingForTarget(vehicle, target, x, y, pSteer, pGas, pBrake, pHandbrake); // 0x433280
    };
    const auto Stop = [&] {
        *pSteer     = 0.0f;
        *pGas       = 0.0f;
        *pHandbrake = true;
        *pBrake     = 0.5f; // 0x858B8C
    };
    const auto TowardsTarget = [&](float offsetX, float offsetY) {
        SteerAICarTowardsPointInEscort(vehicle, ap.m_TargetEntity, offsetX, offsetY, pSteer, pGas, pBrake, pHandbrake); // 0x4336D0
    };

    switch (ap.m_nCarMission) {
    case MISSION_NONE:
    case MISSION_EMERGENCYVEHICLE_STOP:
    case MISSION_STOP_FOREVER:
        Stop();
        return;

    case MISSION_CRUISE:
    case MISSION_RAMPLAYER_FARAWAY:
    case MISSION_BLOCKPLAYER_FARAWAY:
    case MISSION_GOTOCOORDINATES:
    case MISSION_GOTOCOORDINATES_ACCURATE:
    case MISSION_RAMCAR_FARAWAY:
    case MISSION_BLOCKCAR_FARAWAY:
    case MISSION_APPROACHPLAYER_FARAWAY:
    case MISSION_FOLLOWCAR_FARAWAY:
    case MISSION_KILLPED_FARAWAY:
    case MISSION_DO_DRIVEBY_FARAWAY:
    case MISSION_ESCORT_LEFT_FARAWAY:
    case MISSION_ESCORT_RIGHT_FARAWAY:
    case MISSION_ESCORT_REAR_FARAWAY:
    case MISSION_ESCORT_FRONT_FARAWAY:
        SteerAICarWithPhysicsFollowPath(vehicle, pSteer, pGas, pBrake, pHandbrake); // 0x434900
        return;

    case MISSION_RAMPLAYER_CLOSE: {
        auto target = FindPlayerCoors(-1); // 0x56E010
        if (auto* const playerVeh = FindPlayerVehicle(-1, false)) { // 0x56E0D0
            const auto& playerMat = *playerVeh->m_matrix;
            // x87: The dot product is returned in extended precision, but stored as a float
            const auto dot = (float)DotProductOriginal(playerMat.GetForward(), vehicle->m_matrix->GetForward()); // 0x40FDB0

            const auto seed = (uint8)vehicle->m_nRandomSeed;
            if (!(seed & 1) || !(dot > 0.5f)) { // 0x858B8C
                // Spread the cars around the player. x87: Each coordinate is rounded to float once
                const auto spread = (double)((int32)seed - 0x80);
                target.x = (float)(spread * playerMat.GetRight().x * 0.00625f + target.x); // 0x859050
                target.y = (float)(spread * playerMat.GetRight().y * 0.00625f + target.y);
            } else {
                // Get next to the player
                // x87: The sum of the extents is rounded to float
                const auto sideOffset = (float)(((double)GetModelBoundBox(vehicle).m_vecMax.x + GetModelBoundBox(playerVeh).m_vecMax.x) - 0.2f); // 0x858CC4
                if (seed & 2) {
                    target.x = (float)((double)sideOffset * playerMat.GetRight().x + target.x);
                    target.y = (float)((double)sideOffset * playerMat.GetRight().y + target.y);
                } else {
                    target.x = (float)(target.x - (double)sideOffset * playerMat.GetRight().x);
                    target.y = (float)(target.y - (double)sideOffset * playerMat.GetRight().y);
                }

                // x87: The threshold is kept in extended precision
                const auto  relSpeed = playerVeh->m_vecMoveSpeed - vehicle->m_vecMoveSpeed; // 0x40FE60
                const auto  maxDist  = MagnitudeOriginal(relSpeed) * 12.0f + 2.0f;          // 0x4082C0, 0x858CCC, 0x858CA0
                const auto& pos      = vehicle->GetPosition();
                const auto  dy       = (double)pos.y - target.y;
                const auto  dx       = (double)pos.x - target.x;
                if (std::sqrt(dy * dy + dx * dx) < maxDist && ap.m_nTempAction == TEMPACT_NONE) {
                    ap.m_nTempAction     = (seed & 2) ? TEMPACT_TURNLEFT : TEMPACT_TURNRIGHT;
                    ap.m_nTempActionTime = CTimer::GetTimeInMS() + 250;
                }
            }

            if (dot < 0.0f) {
                // The player is going the other way, so lead the target. x87: The factor is only rounded to float for the Y coordinate
                const auto& playerSpeed = FindPlayerSpeed(-1); // 0x56E090
                const auto  factor      = (double)dot * -0.02f; // 0x85904C
                target.x = (float)(factor * playerSpeed.x + target.x);
                target.y = (float)((double)(float)factor * playerSpeed.y + target.y);
            }

            if (vehicle->vehicleFlags.bIsLawEnforcer) {
                const auto& playerMoveSpeed = playerVeh->m_vecMoveSpeed;
                if (std::sqrt((double)playerMoveSpeed.x * playerMoveSpeed.x + (double)playerMoveSpeed.y * playerMoveSpeed.y) > 0.4f) { // 0x858EE8
                    if (auto* const passenger = playerVeh->PickRandomPassenger()) { // 0x6D2A10
                        passenger->Say(CTX_GLOBAL_CAR_POLICE_PURSUIT, 0, 1.0f, false, false, false); // 0x5EFFE0
                    }
                }
            }
        }
        HeadForTarget(FindPlayerVehicle(-1, false), target.x, target.y);
        return;
    }

    case MISSION_BLOCKPLAYER_CLOSE: {
        const auto& speed  = FindPlayerSpeed(-1);
        const auto  coords = FindPlayerCoors(-1);
        SteerAICarWithPhysicsTryingToBlockTarget(vehicle, FindPlayerEntity(-1), coords.x, coords.y, speed.x, speed.y, pSteer, pGas, pBrake, pHandbrake); // 0x4335E0
        return;
    }

    case MISSION_BLOCKPLAYER_HANDBRAKESTOP: {
        const auto& speed  = FindPlayerSpeed(-1);
        const auto  coords = FindPlayerCoors(-1);
        SteerAICarWithPhysicsTryingToBlockTarget_Stop(vehicle, coords.x, coords.y, speed.x, speed.y, pSteer, pGas, pBrake, pHandbrake); // 0x428990
        return;
    }

    case MISSION_WAITFORDELETION:
    case MISSION_PROTECTION_REAR:
    case MISSION_PROTECTION_FRONT:
        return;

    case MISSION_GOTOCOORDINATES_STRAIGHTLINE:
    case MISSION_GOTOCOORDINATES_STRAIGHTLINE_ACCURATE:
    case MISSION_SLOWLY_DRIVE_TOWARDS_PLAYER_1:
        HeadForTarget(nullptr, ap.m_vecDestinationCoors.x, ap.m_vecDestinationCoors.y);
        return;

    case MISSION_GOTOCOORDINATES_ASTHECROWSWIMS:
        SteerAIBoatWithPhysicsHeadingForTarget(vehicle, ap.m_vecDestinationCoors.x, ap.m_vecDestinationCoors.y, pSteer, pGas, pBrake); // 0x428BE0
        *pHandbrake = false;
        return;

    case MISSION_RAMCAR_CLOSE:
    case MISSION_KILLPED_CLOSE: {
        const auto* const target = ap.m_TargetEntity;
        const auto&       pos    = target->GetPosition();
        HeadForTarget(ap.m_TargetEntity, pos.x, pos.y);
        return;
    }

    case MISSION_BLOCKCAR_CLOSE: {
        const auto* const target = ap.m_TargetEntity;
        const auto&       pos    = target->GetPosition();
        SteerAICarWithPhysicsTryingToBlockTarget(vehicle, ap.m_TargetEntity, pos.x, pos.y, target->m_vecMoveSpeed.x, target->m_vecMoveSpeed.y, pSteer, pGas, pBrake, pHandbrake); // 0x4335E0
        return;
    }

    case MISSION_BLOCKCAR_HANDBRAKESTOP: {
        const auto* const target = ap.m_TargetEntity;
        const auto&       pos    = target->GetPosition();
        SteerAICarWithPhysicsTryingToBlockTarget_Stop(vehicle, pos.x, pos.y, target->m_vecMoveSpeed.x, target->m_vecMoveSpeed.y, pSteer, pGas, pBrake, pHandbrake); // 0x428990
        return;
    }

    case MISSION_HELI_FLYTOCOORS:
        SteerAIHeliTowardsTargetCoors(static_cast<CAutomobile*>(vehicle)); // 0x42A630
        return;

    case MISSION_BOAT_ATTACKPLAYER:
        SteerAIBoatWithPhysicsAttackingPlayer(vehicle, pSteer, pGas, pBrake, pHandbrake); // 0x428DE0
        return;

    case MISSION_PLANE_FLYTOCOORS:
        SteerAIPlaneTowardsTargetCoors(static_cast<CAutomobile*>(vehicle)); // 0x423790
        return;

    case MISSION_HELI_ATTACK_PLAYER:
    case MISSION_HELI_ATTACK_PLAYER_FLY_AWAY:
        GetAIHeliToAttackPlayer(static_cast<CAutomobile*>(vehicle)); // 0x42F3C0
        return;

    case MISSION_SLOWLY_DRIVE_TOWARDS_PLAYER_2: {
        const auto coords = FindPlayerCoors(-1);
        HeadForTarget(nullptr, coords.x, coords.y);
        return;
    }

    case MISSION_BLOCKPLAYER_FORWARDANDBACK:
        SteerAICarBlockingPlayerForwardAndBack(vehicle, pSteer, pGas, pBrake, pHandbrake); // 0x422B20
        return;

    case MISSION_ESCORT_LEFT:
    case MISSION_ESCORT_RIGHT: {
        // x87: The offset is rounded to float once
        const auto offset = (float)(((double)GetModelBoundBox(ap.m_TargetEntity).m_vecMax.x + GetModelBoundBox(vehicle).m_vecMax.x) + 2.0f); // 0x858CA0
        TowardsTarget(ap.m_nCarMission == MISSION_ESCORT_LEFT ? -offset : offset, 0.0f);
        return;
    }

    case MISSION_ESCORT_REAR:
    case MISSION_ESCORT_FRONT: {
        const auto offset = (float)(((double)GetModelBoundBox(ap.m_TargetEntity).m_vecMax.y + GetModelBoundBox(vehicle).m_vecMax.y) + 7.0f); // 0x858F48
        TowardsTarget(0.0f, ap.m_nCarMission == MISSION_ESCORT_REAR ? -offset : offset);
        return;
    }

    case MISSION_GOTOCOORDINATES_RACING:
        SteerAICarWithPhysicsFollowPath_Racing(vehicle, pSteer, pGas, pBrake, pHandbrake); // 0x435830
        return;

    case MISSION_FOLLOW_RECORDED_PATH:
        SteerAICarWithPhysicsFollowPreRecordedPath(vehicle, pSteer, pGas, pBrake, pHandbrake); // 0x432DD0
        return;

    case MISSION_PLANE_ATTACK_PLAYER:
    case MISSION_PLANE_ATTACK_PLAYER_POLICE:
        GetAIPlaneToAttackPlayer(static_cast<CAutomobile*>(vehicle)); // 0x429780
        return;

    case MISSION_PLANE_FLYINDIRECTION:
        FlyAIPlaneInCertainDirection(static_cast<CPlane*>(vehicle)); // 0x423000
        return;

    case MISSION_PLANE_FOLLOW_ENTITY:
        SteerAIPlaneToFollowEntity(static_cast<CAutomobile*>(vehicle)); // 0x4237F0
        return;

    case MISSION_HELI_FLYINDIRECTION:
        GetAIHeliToFlyInDirection(static_cast<CAutomobile*>(vehicle)); // 0x42A730
        return;

    case MISSION_HELI_FOLLOW_ENTITY:
    case MISSION_HELI_NEWS_BEHAVIOUR:
        SteerAIHeliToFollowEntity(static_cast<CAutomobile*>(vehicle)); // 0x42A750
        return;

    case MISSION_HELI_POLICE_BEHAVIOUR:
        SteerAIHeliAsPoliceHeli(static_cast<CAutomobile*>(vehicle)); // 0x42AAD0
        return;

    case MISSION_HELI_FLY_AWAY_FROM_PLAYER:
        SteerAIHeliFlyingAwayFromPlayer(static_cast<CAutomobile*>(vehicle)); // 0x42ACB0
        return;

    case MISSION_APPROACHPLAYER_CLOSE: {
        const auto& pos = vehicle->GetPosition();

        // Close enough: stop
        if (MagnitudeOriginal(FindPlayerCoors(-1) - pos) < 10.0f) { // 0x56E010, 0x40FE60, 0x4082C0, 0x85862C
            *pSteer     = 0.0f;
            *pGas       = 0.0f;
            *pBrake     = 1.0f;
            *pHandbrake = false;
            return;
        }

        auto target = FindPlayerCoors(-1);
        if (auto* const playerVeh = FindPlayerVehicle(-1, false)) {
            const auto& playerMat = *playerVeh->m_matrix;
            const auto  diff      = pos - FindPlayerCoors(-1);
            // x87: The sum is kept in extended precision
            if (((double)playerMat.GetForward().y + playerMat.GetForward().x) * diff.x > 0.0f) { // 0x858B50
                // Go to the closer one of the points in front and behind the player's vehicle
                const auto front = TransformPointOriginal(playerMat, { 4.0f, 0.0f, 0.0f });  // 0x59C890
                const auto back  = TransformPointOriginal(playerMat, { -4.0f, 0.0f, 0.0f }); // 0x59C890
                // x87: The first distance is rounded to float, the second one is not
                const auto distFront = (float)MagnitudeOriginal(pos - front);
                const auto distBack  = MagnitudeOriginal(pos - back);
                target = distBack > distFront ? front : back;
            }
        }
        HeadForTarget(FindPlayerVehicle(-1, false), target.x, target.y);
        return;
    }

    case MISSION_PARK_PERPENDICULAR:
    case MISSION_PARK_PERPENDICULAR_2:
        SteerAICarParkPerpendicular(vehicle, pSteer, pGas, pBrake, pHandbrake); // 0x433EA0
        return;

    case MISSION_PARK_PARALLEL:
    case MISSION_PARK_PARALLEL_2:
        SteerAICarParkParallel(vehicle, pSteer, pGas, pBrake, pHandbrake); // 0x433BA0
        return;

    case MISSION_HELI_LAND:
    case MISSION_HELI_LAND_TOUCHING_DOWN:
        SteerAIHeliToLand(static_cast<CAutomobile*>(vehicle)); // 0x42AD30
        return;

    case MISSION_HELI_KEEP_ENTITY_IN_VIEW:
        SteerAIHeliToKeepEntityInView(static_cast<CAutomobile*>(vehicle)); // 0x42AEB0
        return;

    case MISSION_FOLLOWCAR_CLOSE: {
        auto* const target = ap.m_TargetEntity;
        const auto& targetPos = target->GetPosition();
        HeadForTarget(target, targetPos.x, targetPos.y);

        // x87: The distance is kept in extended precision
        const auto dist   = MagnitudeOriginal(vehicle->GetPosition() - target->GetPosition()); // 0x40FE60, 0x4082C0
        const auto follow = (float)ap.m_ucCarFollowDist;
        if (!(dist < (double)follow + 10.0f)) { // 0x85862C
            return;
        }

        // Match the speed of the target (x87: the target's speed is not rounded to float, ours is)
        const auto targetSpeed = std::sqrt((double)target->m_vecMoveSpeed.x * target->m_vecMoveSpeed.x + (double)target->m_vecMoveSpeed.y * target->m_vecMoveSpeed.y) * 60.0f; // 0x858B34
        const auto ownSpeed    = (float)(std::sqrt((double)vehicle->m_vecMoveSpeed.x * vehicle->m_vecMoveSpeed.x + (double)vehicle->m_vecMoveSpeed.y * vehicle->m_vecMoveSpeed.y) * 60.0f);

        double gap = (double)(float)dist - follow;
        if (gap < 0.0) {
            gap *= 5.0f; // 0x858C80
        } else {
            gap += gap;
        }

        double speedDiff = (targetSpeed + gap) - ownSpeed;
        if (speedDiff < 0.0) {
            speedDiff *= -0.1f; // 0x858EF4
            *pGas = 0.0f;
            if (1.0f < speedDiff) { // 0x858624
                speedDiff = 1.0f;
            }
            *pBrake = (float)speedDiff;
        } else {
            speedDiff *= 0.05f; // 0x858C28
            if (1.0f < speedDiff) {
                speedDiff = 1.0f;
            }
            *pGas   = (float)speedDiff;
            *pBrake = 0.0f;
        }
        return;
    }

    case MISSION_PLANE_CRASH_AND_BURN:
        SteerAIPlaneToCrashAndBurn(static_cast<CAutomobile*>(vehicle)); // 0x423880
        return;

    case MISSION_HELI_CRASH_AND_BURN:
        SteerAIHeliToCrashAndBurn(static_cast<CAutomobile*>(vehicle)); // 0x4238E0
        return;

    case MISSION_DO_DRIVEBY_CLOSE: {
        const auto* const target = ap.m_TargetEntity;
        if (!target) {
            return;
        }
        const auto  targetPos = target->GetPosition();
        const auto& pos       = vehicle->GetPosition();

        // Drive next to the target, on the side that's decided by the random seed
        CVector side{ targetPos.y - pos.y, pos.x - targetPos.x, 0.0f };
        NormaliseOriginal(side); // 0x59C910
        side.x *= 10.0f; // 0x85862C
        side.y *= 10.0f;
        side.z *= 10.0f;
        if ((uint8)vehicle->m_nRandomSeed & 1) {
            side = -side;
        }
        HeadForTarget(nullptr, side.x + targetPos.x, side.y + targetPos.y);
        return;
    }

    case MISSION_PLANE_DOG_FIGHT_ENTITY:
        GetAIPlaneToDoDogFight(static_cast<CAutomobile*>(vehicle)); // 0x429890
        return;

    case MISSION_PLANE_DOG_FIGHT_PLAYER:
        GetAIPlaneToDoDogFightAgainstPlayer(static_cast<CAutomobile*>(vehicle)); // 0x42F370
        return;

    case MISSION_BOAT_CIRCLEPLAYER:
        SteerAIBoatWithPhysicsCirclingPlayer(vehicle, pSteer, pGas, pBrake, pHandbrake); // 0x429090
        return;

    default:
        return;
    }
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
    auto& ap = vehicle->m_autoPilot;
    if (ap.m_nCurrentPathNodeInfo.IsValid() && ThePaths.m_pPathNodes[ap.m_nCurrentPathNodeInfo.m_wAreaId]
        && ap.m_nNextPathNodeInfo.IsValid() && ThePaths.m_pPathNodes[ap.m_nNextPathNodeInfo.m_wAreaId]
        && ap.m_currentAddress.IsAreaValid() && ThePaths.m_pPathNodes[ap.m_currentAddress.m_wAreaId]
        && ap.m_startingRouteNode.IsAreaValid() && ThePaths.m_pPathNodes[ap.m_startingRouteNode.m_wAreaId]
    ) {
        return false;
    }
    ap.movementFlags.bIsStopped = true;
    return true;
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
float CCarCtrl::TestCollisionBetween2MovingRects(CVehicle* vehicle1, CVehicle* vehicle2, float moveX, float moveY, CVector* dir1, CVector* dir2) {
    const auto& pos1 = vehicle1->GetPosition();
    const auto& pos2 = vehicle2->GetPosition();

    // Vector from vehicle 2 to vehicle 1
    const auto dx = (float)((double)pos1.x - pos2.x);
    const auto dy = (float)((double)pos1.y - pos2.y);

    // The extents of the collision models (the original uses `min.y` of vehicle 2 negated)
    const auto& bb2 = GetModelBoundBox(vehicle2);
    const auto& bb1 = GetModelBoundBox(vehicle1);
    const float hx2    = bb2.m_vecMax.x;
    const float hy2    = bb2.m_vecMax.y;
    const float negMin2 = -bb2.m_vecMin.y;
    const float hx1    = bb1.m_vecMax.x;
    const float hy1    = bb1.m_vecMax.y;

    // Velocity of vehicle 2 relative to vehicle 1 (moveX/moveY) in the frame of vehicle 1
    const auto f7 = (float)((double)moveX * dir1->y - (double)moveY * dir1->x);
    const auto f8 = (float)((double)moveX * dir1->x + (double)moveY * dir1->y);

    float best = 1.0f;
    for (auto i = 0; i < 2; i++) {
        // Corner of the rectangle of vehicle 1 (relative to vehicle 2)
        float cx, cy;
        if (i == 0) {
            cx = (float)(((double)hy1 * dir2->x + (double)hx1 * dir2->y) + dx);
            cy = (float)(((double)hy1 * dir2->y + dy) - (double)hx1 * dir2->x);
        } else {
            cx = (float)(((double)hy1 * dir2->x + dx) - (double)hx1 * dir2->y);
            cy = (float)(((double)hx1 * dir2->x + (double)hy1 * dir2->y) + dy);
        }

        // Time interval of overlap on the X axis of vehicle 2: [tMin1 (float), tMax1 (x87 register)]
        const auto pa = (float)((double)cx * dir1->y - (double)cy * dir1->x);
        float      tMin1 = 0.0f;
        double     tMax1 = 1.0;
        if (pa > hx2) {
            if (!(f7 < 0.0f)) {
                tMin1 = 1.0f;
            } else {
                const double inv = 1.0 / f7;
                const double t   = -(((double)pa - hx2) * inv);
                if (!(t < 1.0)) {
                    tMin1 = 1.0f;
                } else {
                    tMin1 = (float)t;
                    const double u = t - inv * (hx2 + hx2);
                    if (u < 1.0) {
                        tMax1 = (float)u;
                    }
                }
            }
        } else if (-hx2 > pa) {
            if (!(f7 > 0.0f)) {
                tMin1 = 1.0f;
            } else {
                const double inv = 1.0 / f7;
                const double t   = -(((double)pa + hx2) * inv);
                if (!(t < 1.0)) {
                    tMin1 = 1.0f;
                } else {
                    tMin1 = (float)t;
                    const double u = inv * (hx2 + hx2) + t;
                    if (u < 1.0) {
                        tMax1 = (float)u;
                    }
                }
            }
        } else {
            if (f7 > 0.0f) {
                tMax1 = ((double)hx2 - pa) / f7;
            } else if (f7 < 0.0f) {
                tMax1 = -(((double)pa + hx2) / f7);
            }
        }

        // Time interval of overlap on the Y axis of vehicle 2: [tMin2 (x87 register), tMax2 (float)]
        const auto pb = (float)((double)cy * dir1->y + (double)cx * dir1->x);
        double     tMin2 = 0.0;
        float      tMax2 = 1.0f;
        if (pb > hy2) {
            tMin2 = 1.0;
            if (f8 < 0.0f) {
                const auto   inv = (float)(1.0 / f8);
                const double q   = -(((double)pb - hy2) * inv);
                if (q < 1.0) {
                    const auto m = (float)q;
                    tMin2 = m;
                    const double r = m - ((double)negMin2 + hy2) * inv;
                    if (r < 1.0) {
                        tMax2 = (float)r;
                    }
                }
            }
        } else if (-negMin2 > pb) {
            tMin2 = 1.0;
            if (f8 > 0.0f) {
                const auto   inv = (float)(1.0 / f8);
                const double q   = -(((double)pb + negMin2) * inv);
                if (q < 1.0) {
                    const auto m = (float)q;
                    tMin2 = m;
                    const double r = ((double)negMin2 + hy2) * inv + m;
                    if (r < 1.0) {
                        tMax2 = (float)r;
                    }
                }
            }
        } else {
            if (f8 > 0.0f) {
                tMax2 = (float)(((double)hy2 - pb) / f8);
            } else if (f8 < 0.0f) {
                tMax2 = (float)(-(((double)pb + negMin2) / f8));
            }
        }

        if (tMin1 > tMin2) {
            tMin2 = tMin1;
        }
        const auto tMin2f = (float)tMin2;
        if (tMin2 < tMax1 && tMin2f < tMax2 && !(best < tMin2f)) {
            best = tMin2f;
        }
    }
    return best;
}

// 0x425F70
float CCarCtrl::TestCollisionBetween2MovingRects_OnlyFrontBumper(CVehicle* vehicle1, CVehicle* vehicle2, float moveX, float moveY, CVector* dir1, CVector* dir2) {
    const auto& pos1 = vehicle1->GetPosition();
    const auto& pos2 = vehicle2->GetPosition();

    const auto& bb2 = GetModelBoundBox(vehicle2);
    const auto& bb1 = GetModelBoundBox(vehicle1);
    const float x2  = bb2.m_vecMax.x;
    const float y2  = bb2.m_vecMax.y;
    const float x1  = bb1.m_vecMax.x;
    const float y1  = bb1.m_vecMax.y;
    const float ny1 = -bb1.m_vecMin.y;

    // The corners of the (front edge of the) rectangle of vehicle 2 (x87: some are rounded to float, others aren't)
    const double a = (double)x2 * dir1->y;
    const double b = (double)y2 * dir1->x;
    const auto   q1x = (float)((b + a) + pos2.x);
    const auto   y2d = (float)((double)y2 * dir1->y);
    const auto   x2d = (float)((double)x2 * dir1->x);
    const auto   q1y = (float)(((double)y2d + pos2.y) - x2d);
    const auto   q2x = (float)((b + pos2.x) - a);
    const double q2y = (double)pos2.y + ((double)x2d + y2d);

    float best = 1.0f;
    for (auto i = 0; i < 4; i++) {
        // The corners of the rectangle of vehicle 1
        double u, v;
        switch (i) {
        case 0:
            u = ((double)x1 * dir2->y + (double)y1 * dir2->x) + pos1.x;
            v = ((double)y1 * dir2->y + pos1.y) - (double)x1 * dir2->x;
            break;
        case 1:
            u = ((double)y1 * dir2->x + pos1.x) - (double)x1 * dir2->y;
            v = ((double)y1 * dir2->y + (double)x1 * dir2->x) + pos1.y;
            break;
        case 2:
            u = ((double)pos1.x - (double)ny1 * dir2->x) + (double)x1 * dir2->y;
            v = ((double)pos1.y - (double)ny1 * dir2->y) - (double)x1 * dir2->x;
            break;
        default:
            u = ((double)pos1.x - (double)ny1 * dir2->x) - (double)x1 * dir2->y;
            v = ((double)pos1.y - (double)ny1 * dir2->y) + (double)x1 * dir2->x;
            break;
        }

        // Where the corner is (side of the front edge), before and after moving
        const double ux = u + moveX;
        const double vy = v + moveY;
        const auto   s1 = (float)((v - q1y) * dir1->y + (u - q1x) * dir1->x);
        const auto   s2 = (float)((vy - q1y) * dir1->y + (ux - q1x) * dir1->x);
        if (!(s1 > 0.0f) || !(s2 < 0.0f)) {
            continue;
        }

        // Do the end points of the edge lie on different sides of the movement line?
        const double m1 = (q2x - u) * moveY - (q2y - v) * moveX;
        const double n1 = (q1x - u) * moveY - (q1y - v) * moveX;
        if (!(m1 * n1 < 0.0)) {
            continue;
        }

        const double t = (double)s1 / ((double)s1 - s2);
        if (!(best < t)) {
            best = (float)t;
        }
    }
    return best;
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
    switch (vehicle->m_nModelIndex) {
    case MODEL_LINERUN:
    case MODEL_DUMPER:
    case MODEL_BUS:
    case MODEL_COACH:
    case MODEL_PACKER:
    case MODEL_FLATBED:
    case MODEL_PETRO:
    case MODEL_RDTRAIN:
    case MODEL_CEMENT:
        return true;
    default:
        return false;
    }
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
    StopCarIfNodesAreInvalid(vehicle); // 0x422590 - Result unused

    auto& ap = vehicle->m_autoPilot;
    if (ap.movementFlags.bIsStopped) {
        return;
    }

    const auto StopOnTheSpot = [&] {
        vehicle->m_vecMoveSpeed = CVector{};
        ap.ModifySpeed(0.0f); // 0x41B980
    };

    switch (ap.m_nTempAction) {
    case TEMPACT_STUCKINTRAFFIC:
        StopOnTheSpot();
        return;
    case TEMPACT_WAIT:
    case TEMPACT_BRAKE:
        StopOnTheSpot();
        if (CTimer::GetTimeInMS() > ap.m_nTempActionTime) {
            ap.m_nTempAction              = TEMPACT_NONE;
            ap.m_nTimeToStartMission      = CTimer::GetTimeInMS();
            ap.m_nTimeSwitchedToRealPhysics = CTimer::GetTimeInMS();
        }
        return;
    default:
        break;
    }

    SlowCarOnRailsDownForTrafficAndLights(vehicle); // 0x434790

    // Time at which the car arrives on the next link
    const auto arrivalTime = (int32)((uint32)ap.field_C + ap.m_nSpeedScaleFactor);
    if (arrivalTime < 0 || !(CTimer::GetTimeInMS() < (uint32)arrivalTime)) {
        PickNextNodeAccordingStrategy(vehicle); // 0x432B10
    }

    if (vehicle->GetStatus() == STATUS_PHYSICS) {
        return;
    }

    // x87: The time (0..1) is kept in extended precision until it is stored to a float
    const auto elapsed = (int32)(CTimer::GetTimeInMS() - (uint32)ap.field_C);
    const auto time    = (float)((elapsed < 0 ? (double)(uint32)elapsed : (double)elapsed) / (double)(int32)ap.m_nSpeedScaleFactor);

    const auto  curAddr   = std::bit_cast<uint16>(ap.m_nCurrentPathNodeInfo);
    const auto  nextAddr  = std::bit_cast<uint16>(ap.m_nNextPathNodeInfo);
    const auto& curLink   = ThePaths.m_pNaviNodes[ap.m_nCurrentPathNodeInfo.m_wAreaId][ap.m_nCurrentPathNodeInfo.m_wCarPathLinkId];
    const auto& nextLink  = ThePaths.m_pNaviNodes[ap.m_nNextPathNodeInfo.m_wAreaId][ap.m_nNextPathNodeInfo.m_wCarPathLinkId];
    // `CCarPathLink::m_dir`/`m_posn` hide their raw values, but the original works on them
    const auto  curRaw8   = reinterpret_cast<const int8*>(&curLink);
    const auto  nextRaw8  = reinterpret_cast<const int8*>(&nextLink);
    const auto  curRaw16  = reinterpret_cast<const int16*>(&curLink);
    const auto  nextRaw16 = reinterpret_cast<const int16*>(&nextLink);

    const auto Dir = [](int8 d, int8 sign) { return (float)((double)d * (double)0.01f * (double)sign); }; // 0x858C58
    const float curDirX  = Dir(curRaw8[8], ap._smthCurr);
    const float curDirY  = Dir(curRaw8[9], ap._smthCurr);
    const float nextDirX = Dir(nextRaw8[8], ap._smthNext);
    const float nextDirY = Dir(nextRaw8[9], ap._smthNext);

    // Lane offsets. x87: rounded to float
    float curK  = (float)((curLink.OneWayLaneOffsetExtended() + (double)ap.m_nCurrentLane) * (double)5.4f); // 0x44DB00, 0x858C50
    float nextK = (float)((nextLink.OneWayLaneOffsetExtended() + (double)ap.m_nNextLane) * (double)5.4f);
    if (vehicle->m_nVehicleSubType == VEHICLE_TYPE_BMX) {
        constexpr auto BMX_LANE_OFFSET = std::bit_cast<float>(0x3FBA9FBFu); // 0x859010
        curK  = (float)((double)curK + (double)BMX_LANE_OFFSET);
        nextK = (float)((double)nextK + (double)BMX_LANE_OFFSET);
    }

    // Per-car randomization of the directions (0x859048 = 0.009f). x87: the start's offsets are not rounded to float, the end's are
    const int32 seed = vehicle->m_nRandomSeed;
    const auto  Jitter = [&](uint16 addr, bool second) {
        const int32 sum = seed + addr;
        return (double)((second ? ((sum >> 3) & 7) : (sum & 7)) - 3) * (double)0.009f;
    };
    CVector startDir{
        (float)(Jitter(curAddr, false) + curDirX),
        (float)(Jitter(curAddr, true) + curDirY),
        0.0f
    };
    CVector endDir{
        (float)((double)(float)Jitter(nextAddr, false) + nextDirX),
        (float)((double)(float)Jitter(nextAddr, true) + nextDirY),
        0.0f
    };
    NormaliseOriginal(startDir); // 0x59C910
    NormaliseOriginal(endDir);   // 0x59C910

    // x87: kept in extended precision until stored
    const CVector end{
        (float)((double)nextRaw16[0] * (double)0.125f + (double)nextK * nextDirY), // 0x858C48
        (float)((double)nextRaw16[1] * (double)0.125f - (double)nextK * nextDirX),
        0.0f
    };
    const CVector start{
        (float)((double)curRaw16[0] * (double)0.125f + (double)curK * curDirY),
        (float)((double)curRaw16[1] * (double)0.125f - (double)curK * curDirX),
        0.0f
    };

    CVector pos, speed;
    CCurves::CalcCurvePoint(start, end, startDir, endDir, time, (int32)ap.m_nSpeedScaleFactor, pos, speed); // 0x43C900
    pos.z = 15.0f;
    DragCarToPoint(vehicle, &pos); // 0x42EC90

    constexpr auto INV_60 = std::bit_cast<float>(0x3C888889u); // 0x859044
    speed.x = (float)((double)speed.x * INV_60);
    speed.y = (float)((double)speed.y * INV_60);
    speed.z = (float)((double)speed.z * INV_60);

    if (ap.m_nCurrentPathNodeInfo.m_wCarPathLinkId == ap.m_nNextPathNodeInfo.m_wCarPathLinkId &&
        ap.m_nCurrentPathNodeInfo.m_wAreaId == ap.m_nNextPathNodeInfo.m_wAreaId) {
        return;
    }
    vehicle->m_vecMoveSpeed = speed;
}

namespace {
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

// 0x426970
void CCarCtrl::WeaveForPed(CPed* ped, CVehicle* vehicle, float* pLowerAngle, float* pUpperAngle) {
    // Peds that are supposed to be rammed/killed by `vehicle` don't need to be avoided
    const auto mission = vehicle->m_autoPilot.m_nCarMission;
    if (mission == MISSION_RAMPLAYER_CLOSE && ped == FindPlayerPed(-1)) {
        return;
    }
    if (mission == MISSION_KILLPED_CLOSE && static_cast<CEntity*>(ped) == static_cast<CEntity*>(vehicle->m_autoPilot.m_TargetEntity)) {
        return;
    }

    const auto& pedPos = ped->GetPosition();
    const auto& vehPos = vehicle->GetPosition();
    const auto  toPedX = (float)((double)pedPos.x - vehPos.x);
    const auto  toPedY = (float)((double)pedPos.y - vehPos.y);

    // x87: The result is stored as float here
    const auto heading = (float)CGeneral::GetATanOfXY(toPedX, toPedY); // 0x53CC70

    // x87: the distance is kept in extended precision
    const auto dist = std::sqrt((double)toPedY * toPedY + (double)toPedX * toPedX);
    if (dist < 1.0f) {
        return;
    }

    // Half of the angle (as seen from the vehicle) the ped (and the car) takes up
    const auto pedAngleWidth = (float)(((double)GetModelBoundBox(vehicle).m_vecMax.x * 2.4f + 0.8f) / dist); // 0x858FB0, 0x858C98
    const auto halfWidth     = (float)((double)pedAngleWidth * 0.5f);

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
        const auto diff = Wrap((double)heading - *pLowerAngle);
        if ((diff < 0.0 ? -diff : diff) < halfWidth) {
            const double lower = (double)heading - halfWidth;
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
        const auto diff = Wrap((double)heading - *pUpperAngle);
        if ((diff < 0.0 ? -diff : diff) < halfWidth) {
            const double upper = (double)halfWidth + heading;
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
        if (!(centre.x > minX) || !(centre.x < maxX) || !(centre.y > minY) || !(centre.y < maxY)) {
            continue;
        }

        {
            const auto zDiff = (double)other->GetPosition().z - vehicle->GetPosition().z; // x87: kept in extended precision
            if (!((zDiff < 0.0 ? -zDiff : zDiff) < 8.0f)) {
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
        if (!(pedPos.x > minX) || !(pedPos.x < maxX) || !(pedPos.y > minY) || !(pedPos.y < maxY)) {
            continue;
        }

        {
            const auto zDiff = (double)ped->GetPosition().z - vehicle->GetPosition().z; // x87: kept in extended precision
            if (!((zDiff < 0.0 ? -zDiff : zDiff) < 4.0f)) {
                continue;
            }
        }

        // Peds standing on/attached to the vehicle aren't avoided
        if (ped->m_pContactEntity == vehicle || ped->m_pAttachedTo == vehicle) {
            continue;
        }

        WeaveForPed(ped, vehicle, pLowerAngle, pUpperAngle); // 0x426970
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
        if (!(objPos.x > minX) || !(objPos.x < maxX) || !(objPos.y > minY) || !(objPos.y < maxY)) {
            continue;
        }

        {
            const auto zDiff = (double)obj->GetPosition().z - vehicle->GetPosition().z; // x87: kept in extended precision
            if (!((zDiff < 0.0 ? -zDiff : zDiff) < 8.0f)) {
                continue;
            }
        }

        // Only the objects that are standing upright
        if (!(obj->GetMatrix().GetUp().z > 0.9f)) {
            continue;
        }

        WeaveForObject(obj, vehicle, pLowerAngle, pUpperAngle); // 0x426BC0
    }
}

// 0x427FE0
float CCarCtrl::FindMaxSteerAngle(CVehicle* veh) {
    return std::clamp(0.9f - veh->GetMoveSpeed().Magnitude(), 0.2f, 0.7f);
}

//
// CCarCtrl::GenerateOneRandomCar (0x430050) and its file-local helpers.
// The original is one 7915 bytes long function (3 jump tables: 0x431F3C, 0x431F4C, 0x431F5C), split here along its natural blocks.
// The order of the calls (esp. the RNG ones) is the same as in the original.
//
namespace {
// The raw offsets the original works with (checked, so the named members can be used)
static_assert(offsetof(CVehicle, m_autoPilot) == 0x390);
static_assert(offsetof(CVehicle, vehicleFlags) == 0x428);
static_assert(offsetof(CVehicle, m_pDriver) == 0x460);
static_assert(offsetof(CVehicle, m_nExtendedRemovalRange) == 0x4A6);
static_assert(offsetof(CVehicle, m_fHealth) == 0x4C0);
static_assert(offsetof(CVehicle, m_nPrimaryColor) == 0x434);
static_assert(offsetof(CVehicle, m_nVehicleType) == 0x590);
static_assert(offsetof(CVehicle, m_nVehicleSubType) == 0x594);
static_assert(offsetof(CVehicle, m_vecMoveSpeed) == 0x44);
static_assert(offsetof(CVehicleModelInfo, m_nVehicleType) == 0x3C);
static_assert(offsetof(CVehicleModelInfo, m_nTimesUsed) == 0x50);
static_assert(offsetof(CCamera, m_mCameraMatrix) + 0x18 == 0xB6F9B4 - 0xB6F028);   // `m_mCameraMatrix.GetForward().z`
static_assert(offsetof(CCamera, m_fCamFrontXNorm) == 0xB6F104 - 0xB6F028);
static_assert(offsetof(CCamera, m_fGenerationDistMultiplier) == 0xB6F11C - 0xB6F028);

//! Where (and in what direction) `GenerateCarCreationCoors2` looks for a place for the new car
struct RandomCarSearch {
    float dirX{}, dirY{};
    float arg4{};          //!< 0.707, 0.85 or -1
    bool  arg5{};
    bool  bLookingDown{};  //!< The camera is looking (almost) straight down
};

//! State shared between the blocks of `GenerateOneRandomCar`
struct RandomCarState {
    CVehicle*    veh{};
    CVector      playerPos{};
    CVector      origin{};           //!< Where the car is created (first the place found by `GenerateCarCreationCoors2`, later the point of the curve)
    CNodeAddress nodeA{}, nodeB{};   //!< The 2 path nodes the car is created in between
    CPathNode*   pnA{};
    CPathNode*   pnB{};
    float        fraction{};         //!< Position (0..1) along the link between the 2 nodes
    int32        modelId{};
    int32        carType{};          //!< 13 = cop car, 24 = cop boat, ...
    bool         bBoat{};
    bool         bLookingDown{};
    bool         bMadDriver{};
    bool         bZoneTypeMatches{};
    int32        numLanes{};
    CCarPathLinkAddress naviAddr{};
};

//! Pops the vehicle that was not added to the world yet (`delete veh`, 0x431AA5 / 0x431B2D / 0x431F28)
void AbortRandomCar(CVehicle* veh) {
    delete veh;
}

//! 0x420800 - `a > b ? a : b` (NaN => b)
float Max420800(float a, float b) {
    return a > b ? a : b;
}

//! x87: The length of the 2D vector is kept in extended precision
double Length2DExt(const CVector& v) {
    return std::sqrt((double)v.x * v.x + (double)v.y * v.y);
}

//! 0x420980 (CNodeAddress, unnamed) - `a < b`
bool NodeAddressLess(const CNodeAddress& a, const CNodeAddress& b) {
    return a.m_wAreaId < b.m_wAreaId || (a.m_wAreaId == b.m_wAreaId && a.m_wNodeId < b.m_wNodeId);
}

CPathNode& GetPathNodeAt(const CNodeAddress& addr) {
    return ThePaths.m_pPathNodes[addr.m_wAreaId][addr.m_wNodeId];
}

CCarPathLink& GetNaviLinkAt(const CCarPathLinkAddress& addr) {
    return ThePaths.m_pNaviNodes[addr.m_wAreaId][addr.m_wCarPathLinkId];
}

uint16 RawOf(const CCarPathLinkAddress& addr) {
    return std::bit_cast<uint16>(addr);
}

//! 0x420A60 (CCarPathLink, unnamed) - World position of the link (z = 0)
CVector GetNaviLinkCoors(const CCarPathLink& link) {
    const auto raw = reinterpret_cast<const int16*>(&link);
    return { (float)raw[0] * 0.125f, (float)raw[1] * 0.125f, 0.0f }; // 0x858C48
}

//! 0x4119D0 - `v / s`. x87: the reciprocal is kept in extended precision for the Z, and rounded to float for X and Y
CVector DivideOriginal(const CVector& v, float s) {
    const double recip  = 1.0 / (double)s;
    const double recipF = (double)(float)recip;
    return {
        (float)((double)v.x * recipF),
        (float)((double)v.y * recipF),
        (float)((double)v.z * recip),
    };
}

//! 0x4302AB - Direction (and the related parameters) the new car is searched in
RandomCarSearch ChooseRandomCarSearchDirection() {
    RandomCarSearch r{};

    // 0x858CAC (-0.9). x87: Falls through only if `<`, NaN => else
    if (TheCamera.m_mCameraMatrix.GetForward().z < -0.9f) {
        r.bLookingDown = true;
        r.dirX         = 0.707f;
        r.dirY         = 0.707f;
        r.arg4         = -1.0f;
        r.arg5         = true;
        return r;
    }

    constexpr float ARG4_A = 0.85f;  // 0x3F59999A
    constexpr float ARG4_B = 0.707f; // 0x3F34FDF4

    const auto frame = CTimer::m_FrameCounter;
    bool       bUseCamFront = true;
    if (const auto* const veh = FindPlayerVehicle(-1, false)) {
        const float vx = veh->m_vecMoveSpeed.x; // 0x44
        const float vy = veh->m_vecMoveSpeed.y; // 0x48
        const double speed = std::sqrt((double)vy * vy + (double)vx * vx);

        const auto SetDir = [&] {
            const double inv = 1.0 / speed;
            r.dirX = (float)(vx * inv);
            r.dirY = (float)(inv * vy);
        };

        if (speed > 0.4f) { // 0x858EE8
            SetDir();
            bUseCamFront = false;
            switch (frame & 3) { // jump table 0x431F3C
            case 0:
            case 1: r.arg4 = ARG4_A; r.arg5 = true;  break;
            case 2: r.arg4 = ARG4_B; r.arg5 = true;  break;
            case 3: r.arg4 = ARG4_B; r.arg5 = false; break;
            }
        } else if (speed > 0.1f) { // 0x858B1C
            SetDir();
            bUseCamFront = false;
            switch (frame & 3) { // jump table 0x431F4C
            case 0: r.arg4 = ARG4_A; r.arg5 = true;  break;
            case 1: r.arg4 = ARG4_B; r.arg5 = true;  break;
            case 2:
            case 3: r.arg4 = ARG4_B; r.arg5 = false; break;
            }
        }
    }
    if (bUseCamFront) { // 0x4303BA
        r.dirX = TheCamera.m_fCamFrontXNorm;
        r.dirY = TheCamera.m_fCamFrontYNorm;
        r.arg4 = ARG4_B;
        r.arg5 = (frame & 1) == 0;
    }
    return r;
}

//! 0x430050 (top) - Chooses the model (and the type) of the car to create
//! @returns false if no car should be created
bool ChooseRandomCarModel(int32& modelId, int32& carType) {
    const auto Wanted = [] { return FindPlayerWanted(-1); };

    // 0x43016C
    bool bPolice = false;
    if ((int32)Wanted()->GetWantedLevel() > 1 && (int32)CCarCtrl::NumLawEnforcerCars < (int32)Wanted()->m_MaxCopCarsInPursuit) {
        const auto* const w1 = Wanted();
        const auto* const w2 = Wanted();
        if (w2->m_NumCopsInPursuit < w1->m_MaxCopsInPursuit && CGame::currArea == 0 && !CGangWars::GangWarFightingGoingOn()) {
            const auto now = CTimer::GetTimeInMS();
            const auto last = (uint32)CCarCtrl::LastTimeLawEnforcerCreated;
            if ((int32)Wanted()->GetWantedLevel() > 3) {
                bPolice = true;
            } else {
                if ((int32)Wanted()->GetWantedLevel() > 2 && now > last + 5000) {
                    bPolice = true;
                } else if (now > last + 8000) {
                    bPolice = true;
                }
            }
        }
    }

    if (bPolice) {
        modelId = CCarCtrl::ChoosePoliceCarModel(0); // 0x43020E
        carType = 13;
    } else {
        modelId = CCarCtrl::ChooseModel(&carType); // 0x424CE0
        if (modelId == -1) {
            return false;
        }
        if ((carType == 13 || carType == 24) && (int32)Wanted()->GetWantedLevel() >= 1) {
            return false;
        }
    }

    // 0x430263
    if (CGameLogic::LaRiotsActiveHere() && !gbLARiots_NoPoliceCars && (rand() & 0x7F) < 0x37) {
        modelId = CCarCtrl::ChoosePoliceCarModel(0);
        carType = 13;
    }
    return true;
}

//! 0x4306A6 (part) - Mission, driving style and cruise speed of the new car
void InitRandomCarMission(CVehicle* veh, int32 carType, int32 modelId, bool bBoat) {
    auto& ap = veh->m_autoPilot;

    const auto SetCruiseSpeed = [&](float min, float max) { // 0x41BD90 + 0x821B40 (ftol)
        ap.m_nCruiseSpeed = (uint8)(int32)CGeneral::GetRandomNumberInRange(min, max);
    };

    if (carType == 13) { // 0x43080B
        ap.m_nTempAction = TEMPACT_NONE;
        if (FindPlayerWanted(-1)->GetWantedLevel() == eWantedLevel::WANTED_CLEAN) {
            SetCruiseSpeed(18.0f, 24.0f);
            ap.m_nCarDrivingStyle = DRIVING_STYLE_STOP_FOR_CARS;
            ap.m_nCarMission      = MISSION_CRUISE;
        } else {
            ap.m_nCruiseSpeed = (uint8)CCarAI::FindPoliceCarSpeedForWantedLevel(veh); // 0x430851
            ap.m_nCarMission  = veh->GetVehicleAppearance() == VEHICLE_APPEARANCE_BIKE
                ? CCarAI::FindPoliceBikeMissionForWantedLevel()
                : CCarAI::FindPoliceCarMissionForWantedLevel();
            ap.m_nCarDrivingStyle = DRIVING_STYLE_AVOID_CARS;
        }
        if (modelId == MODEL_FBIRANCH) { // 0x430884
            veh->m_nPrimaryColor   = 0;
            veh->m_nSecondaryColor = 0;
        }
        veh->vehicleFlags.bCreatedAsPoliceVehicle = true; // 0x43089A
    } else if (carType == 24) { // 0x4307D0
        ap.m_nTempAction = TEMPACT_NONE;
        SetCruiseSpeed(14.0f, 18.0f);
        ap.m_nCarDrivingStyle = DRIVING_STYLE_AVOID_CARS;
        ap.m_nCarMission      = CCarAI::FindPoliceBoatMissionForWantedLevel();
        veh->vehicleFlags.bCreatedAsPoliceVehicle = true;
    } else {
        SetCruiseSpeed(13.0f, 21.0f); // 0x4306E8
        if (carType == 3) {
            SetCruiseSpeed(18.0f, 27.0f);
        } else if (carType == 1) {
            SetCruiseSpeed(10.0f, 15.0f);
        }

        // x87: The difference is not rounded to float
        const auto& bb = CModelInfo::GetModelInfo(veh->m_nModelIndex)->GetColModel()->GetBoundingBox();
        if ((double)bb.m_vecMax.y - (double)bb.m_vecMin.y > 10.0f /* 0x85862C */ || carType == 5) {
            ap.m_nCruiseSpeed = (uint8)(((int32)ap.m_nCruiseSpeed * 3) / 4);
        }

        if (bBoat) {
            switch (veh->m_nModelIndex) {
            case MODEL_SQUALO:
            case MODEL_SPEEDER:
            case MODEL_JETMAX: SetCruiseSpeed(25.0f, 35.0f); break;
            default:           SetCruiseSpeed(15.0f, 24.0f); break;
            }
        }
        ap.m_nCarMission      = MISSION_CRUISE; // 0x4307B6
        ap.m_nTempAction      = TEMPACT_NONE;
        ap.m_nCarDrivingStyle = DRIVING_STYLE_STOP_FOR_CARS;
    }
}

//! 0x430A58..0x430C79 - Fraction along the link, lane directions, the link to go to and the orientation of the car
//! @returns false if the car has to be thrown away
bool OrientRandomCar(RandomCarState& s) {
    auto* const veh = s.veh;
    auto&       ap  = veh->m_autoPilot;

    const CVector posA = s.pnA->GetPosition();
    const CVector posB = s.pnB->GetPosition();

    // Fraction along the link. Note: The values of this are mixed with a few other uses of the same variable in the original
    {
        const auto& colBB = CModelInfo::GetModelInfo(veh->m_nModelIndex)->GetColModel()->GetBoundingBox();
        // x87: kept in extended precision until stored
        const float halfLen = (float)(((double)colBB.m_vecMax.y - (double)colBB.m_vecMin.y) * 0.5f /* 0x858B8C */ + 1.0f);

        const double dy   = (double)posA.y - posB.y;
        const float  dyF  = (float)((double)posA.y - posB.y);
        const double dist = std::sqrt(dy * dyF + ((double)posA.x - posB.x) * ((double)posA.x - posB.x));

        if (0.5f * dist < halfLen) { // 0x430A4D, x87: NaN => else
            s.fraction = 0.5f;
        } else {
            const double ratio = (double)halfLen / dist;
            if (!((double)s.fraction > ratio)) {
                s.fraction = (float)ratio;
            }
            const double oneMinusRatio = 1.0 - ratio;
            if (!((double)s.fraction < oneMinusRatio)) {
                s.fraction = (float)oneMinusRatio;
            }
        }
    }
    ap._smthNext = NodeAddressLess(s.nodeA, s.nodeB) ? -1 : 1; // 0x430A62

    // 0x430AB2
    if (s.pnA->m_nNumLinks == 1) {
        return false;
    }

    // Pick a link that isn't the one the car was set up with
    int16 linkIdx;
    CCarPathLinkAddress newCur;
    do {
        linkIdx = (int16)(rand() % (int32)s.pnA->m_nNumLinks);
        newCur  = ThePaths.m_pNaviLinks[s.nodeA.m_wAreaId][s.pnA->m_wBaseLinkId + linkIdx];
    } while (RawOf(newCur) == RawOf(ap.m_nNextPathNodeInfo));
    ap.m_nCurrentPathNodeInfo = newCur; // 0x430B10
    if (!ThePaths.m_pPathNodes[newCur.m_wAreaId]) {
        return false;
    }
    ap._smthCurr = NodeAddressLess(ThePaths.m_pNodeLinks[s.nodeA.m_wAreaId][s.pnA->m_wBaseLinkId + linkIdx], s.nodeA) ? -1 : 1; // 0x430B44

    // Orientation. The matrix is used without checking if there is one (as in the original)
    {
        const CVector dirAB = posB - posA; // 0x430B9A
        CVector       fwd   = dirAB;
        CVector2D     w{ dirAB.x, dirAB.y };
        {
            const double len = Length2DExt(CVector{ dirAB.x, dirAB.y, 0.0f }); // 0x430BE7
            if (len == 0.0) {
                w.x = 1.0f;
            } else {
                const double inv = 1.0 / len;
                w.x = (float)(w.x * inv);
                w.y = (float)(inv * w.y);
            }
        }
        NormaliseOriginal(fwd); // 0x430C21

        auto& mat = *veh->m_matrix;
        mat.GetForward() = fwd;
        mat.GetRight()   = CVector{ w.y, -w.x, 0.0f };
        mat.GetUp()      = CVector{ 0.0f, 0.0f, 1.0f };
    }
    return true;
}

//! 0x430C80..0x4315CB - Point (on the curve between the 2 links) the car is placed on
void PlaceRandomCarOnCurve(RandomCarState& s, CVector& outCurveSpeed) {
    auto* const veh = s.veh;
    auto&       ap  = veh->m_autoPilot;

    // Where the car is, in terms of the 'distance' to the next link (t)
    float t;
    {
        const auto& nextLink0 = GetNaviLinkAt(ap.m_nNextPathNodeInfo);
        const CVector pCur0   = GetPathNodeAt(ap.m_currentAddress).GetPosition();
        const CVector nextPos = GetNaviLinkCoors(nextLink0);
        const float   d1      = (float)Length2DExt(nextPos - pCur0); // 0x430D21
        const CVector pStart  = GetPathNodeAt(ap.m_startingRouteNode).GetPosition();
        const double  d2      = Length2DExt(nextPos - pStart);
        const double  ratio   = (double)d1 / (d2 + (double)d1);

        if (ratio > (double)s.fraction) { // 0x430D7D
            const auto&   curLink = GetNaviLinkAt(ap.m_nCurrentPathNodeInfo);
            const CVector pCur    = GetPathNodeAt(ap.m_currentAddress).GetPosition();
            const float   d3      = (float)Length2DExt(GetNaviLinkCoors(curLink) - pCur); // 0x430E1A
            const double  d4      = Length2DExt(s.origin - GetPathNodeAt(ap.m_currentAddress).GetPosition());
            t = (float)((d4 + (double)d3) / ((double)d3 + (double)d1));
        } else {
            CCarCtrl::PickNextNodeRandomly(veh); // 0x430E63
            const CVector pCur = GetPathNodeAt(ap.m_currentAddress).GetPosition();
            const CVector v1   = GetNaviLinkCoors(GetNaviLinkAt(ap.m_nNextPathNodeInfo)) - pCur;
            const CVector v2   = GetNaviLinkCoors(GetNaviLinkAt(ap.m_nCurrentPathNodeInfo)) - GetPathNodeAt(ap.m_currentAddress).GetPosition();
            const float   d5   = (float)Length2DExt(v2);
            const double  d6   = Length2DExt(s.origin - GetPathNodeAt(ap.m_currentAddress).GetPosition());
            const double  d7   = std::sqrt((double)v1.x * v1.x + (double)v1.y * v1.y);
            t = (float)(((double)d5 - d6) / (d7 + (double)d5));
        }
    }
    if (0.0f > t) { // 0x430FD9
        t = 0.0f;
    } else if (1.0f < t) {
        t = 1.0f;
    }

    // `CCarPathLink::m_dir`/`m_posn` hide their raw values, but the original works on them. x87: kept in extended precision until stored
    const auto& curLink   = GetNaviLinkAt(ap.m_nCurrentPathNodeInfo);
    const auto& nextLink  = GetNaviLinkAt(ap.m_nNextPathNodeInfo);
    const auto  curRaw8   = reinterpret_cast<const int8*>(&curLink);
    const auto  nextRaw8  = reinterpret_cast<const int8*>(&nextLink);
    const auto  curRaw16  = reinterpret_cast<const int16*>(&curLink);
    const auto  nextRaw16 = reinterpret_cast<const int16*>(&nextLink);

    const auto Dir = [](int8 d, int8 sign) { return (float)((double)d * (double)0.01f * (double)sign); }; // 0x858C58
    const float curDirX  = Dir(curRaw8[8], ap._smthCurr);
    const float curDirY  = Dir(curRaw8[9], ap._smthCurr);
    const float nextDirX = Dir(nextRaw8[8], ap._smthNext);
    const float nextDirY = Dir(nextRaw8[9], ap._smthNext);

    // Lane offsets (both are rounded to float, in contrast to `JoinCarWithRoadSystem`)
    float k1 = (float)((curLink.OneWayLaneOffsetExtended() + (double)ap.m_nCurrentLane) * (double)5.4f);  // 0x858C50
    float k2 = (float)((nextLink.OneWayLaneOffsetExtended() + (double)ap.m_nNextLane) * (double)5.4f);
    if (veh->m_nVehicleSubType == VEHICLE_TYPE_BMX) { // 0x43116F
        constexpr auto BMX_LANE_OFFSET = std::bit_cast<float>(0x3FBA9FBFu); // 0x859010
        k1 = (float)((double)k1 + (double)BMX_LANE_OFFSET);
        k2 = (float)((double)k2 + (double)BMX_LANE_OFFSET);
    }

    // 0x43118D
    {
        const auto& node = GetPathNodeAt(ap.m_startingRouteNode);
        const int8  hw   = (int8)(node.m_bNotHighway | (node.m_bHighway << 1)); // (byte 1 >> 4) & 3
        ap.field_41    = hw;
        ap.m_SpeedMult = CCarCtrl::FindSpeedMultiplierWithSpeedFromNodes(hw); // 0x4311BA
        ap.m_speed     = (float)((double)ap.m_nCruiseSpeed * ap.m_SpeedMult);
    }

    const float k2DirX = (float)((double)k2 * nextDirX);
    const float k2DirY = (float)((double)k2 * nextDirY);
    const float k1DirX = (float)((double)k1 * curDirX);
    const float k1DirY = (float)((double)k1 * curDirY);
    const CVector end{
        (float)((double)nextRaw16[0] * (double)0.125f + (double)k2DirY),
        (float)((double)nextRaw16[1] * (double)0.125f - (double)k2DirX),
        0.0f
    };
    const CVector start{
        (float)((double)curRaw16[0] * (double)0.125f + (double)k1DirY),
        (float)((double)curRaw16[1] * (double)0.125f - (double)k1DirX),
        0.0f
    };

    const auto scale      = CCurves::CalcSpeedScaleFactor(start, end, curDirX, curDirY, nextDirX, nextDirY); // 0x4313F1
    const auto speedScale = (int32)((double)scale * (1000.0f / (double)ap.m_speed));                          // 0x858C4C
    ap.m_nSpeedScaleFactor = (uint32)speedScale;

    const auto now       = CTimer::GetTimeInMS();
    const auto startTime = (int32)((double)now - (double)t * (double)speedScale); // 0x43143A, x87: extended precision
    ap.field_C           = startTime;

    const float time = (float)((double)(uint32)(now - (uint32)startTime) / (double)speedScale);
    CCurves::CalcCurvePoint(start, end, CVector{ curDirX, curDirY, 0.0f }, CVector{ nextDirX, nextDirY, 0.0f }, time, speedScale, s.origin, outCurveSpeed); // 0x4315CB
}

//! 0x4315D0..0x43185B - Final position (the height) of the car
//! @returns false if the car has to be thrown away
bool SnapRandomCarToGround(RandomCarState& s) {
    auto* const veh = s.veh;

    const CVector pA = s.pnA->GetPosition();
    const CVector pB = s.pnB->GetPosition();

    const CVector vAB  = pA - pB; // 0x43161F
    const auto    mag  = std::sqrt(((double)vAB.x * vAB.x + (double)vAB.y * vAB.y) + (double)vAB.z * vAB.z); // 0x4082C0, x87: not rounded to float
    const auto    step = vAB * (float)(2.0f / mag); // 0x40FEC0

    CVector posn = s.origin + step; // 0x40FE30

    // x87: The 1st term is rounded to float
    const float zA = (float)(((double)1.0f - s.fraction) * pA.z);
    posn.z         = (float)((double)s.fraction * pB.z + (double)zA);

    float ground = 1.0e9f; // 0x858FEC
    if (s.bBoat) {
        float waterLevel;
        if (!CWaterLevel::GetWaterLevel(posn.x, posn.y, posn.z, waterLevel, true, nullptr)) { // 0x4316FC
            return false;
        }
        ground = waterLevel;
    } else {
        CColPoint colPoint;
        CEntity*  hitEntity;
        if (CWorld::ProcessVerticalLine(posn, 1000.0f, colPoint, hitEntity, true, false, false, false, true, false, nullptr)) { // 0x431742
            ground = colPoint.m_vecPoint.z;
        }
        if (CWorld::ProcessVerticalLine(posn, -1000.0f, colPoint, hitEntity, true, false, false, false, true, false, nullptr)) { // 0x431784
            // x87: kept in extended precision
            double d1 = (double)colPoint.m_vecPoint.z - posn.z;
            if (d1 < 0.0) {
                d1 = -d1;
            }
            double d2 = (double)ground - posn.z;
            if (d2 < 0.0) {
                d2 = -d2;
            }
            if (d1 < d2) {
                ground = colPoint.m_vecPoint.z;
            }
        }
    }

    if (ground == 1.0e9f) { // 0x4317E3
        return false;
    }
    {
        double d = (double)ground - posn.z; // 0x4317F4
        if (d < 0.0) {
            d = -d;
        }
        if (d > 7.0f) { // 0x858F48
            return false;
        }
    }

    if (CModelInfo::IsBoatModel(veh->m_nModelIndex)) { // 0x431824
        posn.z                       = ground;
        veh->m_nExtendedRemovalRange = 0xFF;
    } else {
        posn.z = (float)((double)veh->GetHeightAboveRoad() + ground); // 0x431848 (vtable +0xD4)
    }
    veh->SetPosn(posn); // 0x431862
    veh->m_vecMoveSpeed = CVector{ 0.0f, 0.0f, 0.0f };

    return true;
}

//! 0x43194C - Entity status of the new car
void SetRandomCarStatus(RandomCarState& s) {
    auto* const veh = s.veh;
    if (s.carType == 13) { // 0x43194C
        if (veh->m_autoPilot.m_nCarMission == MISSION_CRUISE) {
            veh->SetStatus(STATUS_SIMPLE);
        } else {
            veh->SetStatus(STATUS_PHYSICS);
        }
    } else if (s.carType == 24) { // 0x43193E
        veh->SetStatus(STATUS_PHYSICS);
    } else if (s.bBoat) {
        veh->SetStatus(STATUS_PHYSICS);
    } else if (veh->GetStatus() != STATUS_PHYSICS) {
        veh->SetStatus(STATUS_SIMPLE);
    }
}

//! Decides if the car is too far/close to be created (as it would be visible/not needed)
//! @returns false if the car has to be thrown away
bool IsRandomCarPositionOK(RandomCarState& s) {
    auto* const veh = s.veh;

    const auto genMult = TheCamera.m_fGenerationDistMultiplier; // 0xB6F11C
    const auto range   = (float)veh->m_nExtendedRemovalRange;

    if (veh->GetIsOnScreen()) { // 0x43199F
        const CVector toPlayer = s.playerPos - veh->GetPosition();
        const float   dist     = (float)Length2DExt(toPlayer);
        if ((double)Max420800(170.0f, range) * genMult < dist) { // 0x431A04, x87: NaN => continue
            return false;
        }
        if ((double)genMult * 150.0f /* 0x858A28 */ > dist) {
            return false;
        }
        const CVector toCam = TheCamera.GetPosition() - veh->GetPosition();
        if ((double)genMult * 120.0f /* 0x858BB0 */ > Length2DExt(toCam)) { // 0x431A80
            return false;
        }
        if (s.bLookingDown) {
            return false;
        }
        if (veh->m_nModelIndex == MODEL_MARQUIS) {
            return false;
        }
    } else { // 0x431AB8
        const CVector toPlayer = s.playerPos - veh->GetPosition();
        const float   dist     = (float)Length2DExt(toPlayer);
        constexpr auto INV_170 = std::bit_cast<float>(0x3BC0C0C1u); // 0x858F94
        if ((double)Max420800(170.0f, range) * INV_170 * 45.0f /* 0x858CB4 */ < dist && !s.bLookingDown) { // x87: NaN => continue
            return false;
        }
    }
    return true;
}
} // namespace

// 0x430050
void CCarCtrl::GenerateOneRandomCar() {
    RandomCarState s{};
    s.nodeA = s.nodeB = CNodeAddress{ 0xFFFF, 0xFFFF };

    s.playerPos = FindPlayerCentreOfWorld(CWorld::PlayerInFocus); // 0x430080
    const CVector playerSpeed = FindPlayerSpeed(-1); // 0x4300A4 (copied to the stack, used for the 'moving towards the player' check at 0x4318B2)

    // 0x4300B1 - Is there room for more cars?
    const int32 numCars = (int32)(NumFireTrucksOnDuty + NumAmbulancesOnDuty + NumMissionCars + NumLawEnforcerCars + NumRandomCars);
    float density = CarDensityMultiplier;
    if (CCullZones::FewerCars()) {
        density *= 0.6f; // 0x858CC8
    }
    const float numCarsF = (float)numCars;
    if ((double)CPopulation::FindCarMultiplierMotorway() * (int32)MaxNumberOfCarsInUse * density <= numCarsF) { // 0x43012D
        return;
    }
    if (((double)CPopCycle::m_NumOther_Cars + CPopCycle::m_NumCops_Cars + CPopCycle::m_NumGangs_Cars + CPopCycle::m_NumDealers_Cars) * CPopulation::FindCarMultiplierMotorway() * density <= numCarsF) { // 0x43015F
        return;
    }

    // 0x43016C
    if (!ChooseRandomCarModel(s.modelId, s.carType)) {
        return;
    }

    // 0x430298 - Where to look for a place
    const auto search = ChooseRandomCarSearchDirection();
    s.bLookingDown = search.bLookingDown;

    {
        // 0x4303E6
        const bool arg12 = !(s.carType == 13 && (int32)FindPlayerWanted(-1)->GetWantedLevel() >= 1);
        s.fraction = search.dirY; // (the out parameter reuses the variable of the direction)
        if (!GenerateCarCreationCoors2(
            s.playerPos,
            search.dirX,
            search.dirY,
            search.arg4,
            search.arg5,
            TheCamera.m_fGenerationDistMultiplier * 160.0f, // 0x858970
            38.0f,
            &s.origin,
            &s.nodeA,
            &s.nodeB,
            &s.fraction,
            arg12,
            false
        )) {
            return;
        }
    }

    // 0x430465
    s.pnA = &GetPathNodeAt(s.nodeA);
    s.pnB = &GetPathNodeAt(s.nodeB);

    const auto minSpawnProb = std::min<uint8>(s.pnA->m_nSpawnProbability, s.pnB->m_nSpawnProbability);
    if ((rand() & 0xF) > minSpawnProb) { // 0x4304B9
        return;
    }

    float searchRadius;
    if (s.pnA->m_bWaterNode) { // 0x4304D4
        s.bBoat = true;
        if (s.carType == 13) {
            s.modelId = MODEL_PREDATOR;
            s.carType = 24;
            if (!CStreaming::GetInfo(MODEL_PREDATOR).IsLoaded()) {
                CStreaming::RequestModel(MODEL_PREDATOR, STREAMING_KEEP_IN_MEMORY); // 0x430506
                return;
            }
        } else {
            s.modelId = CPopulation::m_LoadedBoats.PickLeastUsedModel(1); // 0x430520
            if (s.modelId == -1) {
                return;
            }
            if (!CStreaming::GetInfo(s.modelId).IsLoaded()) {
                return;
            }
        }
        searchRadius = 40.0f;
    } else {
        searchRadius = 8.0f;
    }

    {
        int16 numColliding{};
        CWorld::FindObjectsKindaColliding(s.origin, searchRadius, true, &numColliding, 2, nullptr, false, true, true, false, false); // 0x430577
        if (numColliding != 0) {
            return;
        }
    }

    // 0x43058B - Find the link (+ the navi link) that connects the 2 nodes
    {
        int16 linkIdx = 0;
        const int32 numLinks = s.pnA->m_nNumLinks;
        if (numLinks > 0) {
            do {
                if (!(ThePaths.m_pNodeLinks[s.nodeA.m_wAreaId][s.pnA->m_wBaseLinkId + linkIdx] != s.nodeB)) { // 0x4305B8
                    break;
                }
                linkIdx++;
            } while (linkIdx < numLinks);
        }
        s.naviAddr = ThePaths.m_pNaviLinks[s.nodeA.m_wAreaId][s.pnA->m_wBaseLinkId + linkIdx];

        const auto& naviLink = GetNaviLinkAt(s.naviAddr);
        s.numLanes = naviLink.m_attachedTo == s.nodeB // 0x430603
            ? naviLink.m_numOppositeDirLanes
            : naviLink.m_numSameDirLanes;
    }

    // 0x430625
    if (s.numLanes > 1) {
        if (CModelInfo::GetVehicleModelInfo(s.modelId)->m_nVehicleType == VEHICLE_TYPE_BMX) {
            return;
        }
    } else if (s.modelId == MODEL_COACH || s.modelId == MODEL_BUS) {
        return;
    }
    if (s.numLanes == 0) {
        return;
    }

    // 0x430661 - The zone types where only certain cars are allowed
    if (CPopCycle::m_pCurrZone) {
        const auto popType = (int32)CTheZones::GetZoneInfo(s.origin, nullptr)->PopType;
        if (popType >= +eZonePopulationType::GOLF_CLUB && popType <= +eZonePopulationType::AIRPORT_RUNWAY) {
            if (popType != CPopCycle::m_nCurrentZoneType) {
                return;
            }
            s.bZoneTypeMatches = true;
        }
    }

    // 0x4306A1
    s.veh = GetNewVehicleDependingOnCarModel(s.modelId, RANDOM_VEHICLE);
    auto* const veh = s.veh;
    auto&       ap  = veh->m_autoPilot;
    ap.m_endingRouteNode.ResetAreaId();
    ap.m_currentAddress      = s.nodeA;
    ap.m_startingRouteNode   = s.nodeB;

    InitRandomCarMission(veh, s.carType, s.modelId, s.bBoat);

    // 0x4308A1
    if (veh->m_nModelIndex == MODEL_MRWHOOP) {
        veh->vehicleFlags.bSirenOrAlarm = true; // 0x42D |= 0x80
    }
    ap.m_nNextPathNodeInfo = s.naviAddr;
    {
        const auto lane       = (int8)(rand() % (int16)s.numLanes); // 0x4308BC
        ap.m_nCurrentLane     = lane;
        ap.m_nNextLane        = lane;
    }

    // 0x4308D5 - Chance for the car to become a 'mad driver' (1 / chance)
    int32 madDriverChance;
    if (CGameLogic::LaRiotsActiveHere()) {
        madDriverChance = 80;
    } else {
        switch (veh->GetVehicleAppearance()) {
        case VEHICLE_APPEARANCE_BIKE: madDriverChance = 50;  break;
        case VEHICLE_APPEARANCE_BOAT: madDriverChance = 10;  break;
        default:                      madDriverChance = 200; break;
        }
    }
    if (!s.bBoat && s.carType != 13 && !s.bZoneTypeMatches) { // 0x430909
        if (CGeneral::GetRandomNumberInRange(0, madDriverChance) == 0 || CCheat::IsActive(CHEAT_AGGRESSIVE_DRIVERS)) {
            s.bMadDriver = true;
            s.fraction   = 1.0f;
        }
    }

    // 0x430943 - Place and orient the car
    if (!OrientRandomCar(s)) {
        AbortRandomCar(veh);
        return;
    }
    CVector curveSpeed{};
    PlaceRandomCarOnCurve(s, curveSpeed);

    if (!SnapRandomCarToGround(s)) {
        AbortRandomCar(veh);
        return;
    }

    // 0x4318B2 - Is the car moving towards the player?
    const CVector relSpeed = DivideOriginal(curveSpeed, 60.0f) - playerSpeed; // 0x4119D0, 0x40FE60 (curve speed per frame minus the speed of the player)
    const CVector toOrigin = { s.origin.x - s.playerPos.x, s.origin.y - s.playerPos.y, 0.0f };

    SetRandomCarStatus(s); // 0x4318FE
    CVisibilityPlugins::SetClumpAlpha(veh->GetRpClump(), 0); // 0x431973

    if (CCheat::IsActive(CHEAT_FUNHOUSE_THEME) && veh->m_nVehicleType == VEHICLE_TYPE_AUTOMOBILE) { // 0x969176
        veh->AddVehicleUpgrade(ModelIndices::MI_HYDRAULICS); // 0x431998
    }

    if (!IsRandomCarPositionOK(s)) {
        AbortRandomCar(veh);
        return;
    }

    // 0x431B40
    {
        int16 numColliding{};
        const auto radius = CModelInfo::GetModelInfo(veh->m_nModelIndex)->GetColModel()->GetBoundRadius();
        CWorld::FindObjectsKindaColliding(veh->GetPosition(), radius, true, &numColliding, 2, nullptr, false, true, true, false, false); // 0x431B7A
        if (numColliding != 0) {
            AbortRandomCar(veh);
            return;
        }
    }
    // x87: kept in extended precision. Note: only when the car moves towards the player
    if (!((double)relSpeed.x * toOrigin.x + (double)relSpeed.y * toOrigin.y < 0.0)) { // 0x431BA6
        AbortRandomCar(veh);
        return;
    }

    // 0x431BB7
    CModelInfo::GetVehicleModelInfo(veh->m_nModelIndex)->ChooseVehicleColour(veh->m_nPrimaryColor, veh->m_nSecondaryColor, veh->m_nTertiaryColor, veh->m_nQuaternaryColor, 1);
    CWorld::Add(veh); // 0x431BE6

    if (veh->m_nModelIndex == MODEL_TRACTOR || veh->m_nModelIndex == MODEL_COMBINE || veh->m_nVehicleSubType == VEHICLE_TYPE_BMX) {
        ap.m_nCruiseSpeed = (uint8)((int32)ap.m_nCruiseSpeed / 3);
    }

    if (CGameLogic::LaRiotsActiveHere()) { // 0x431C22
        veh->m_fHealth = (float)(rand() % 1000);
    }

    if (s.carType == 13) { // 0x431C46
        LastTimeLawEnforcerCreated = (int32)CTimer::GetTimeInMS();
    }

    if (veh->m_nModelIndex == MODEL_CADDY) { // 0x431C57
        veh->SetStatus(STATUS_PHYSICS);
        ap.m_nCarDrivingStyle = DRIVING_STYLE_AVOID_CARS;
    }

    // 0x431C70 - Random damage (jump table 0x431F5C, indexed by the byte table at 0x431F68)
    if (veh->m_nVehicleType == VEHICLE_TYPE_AUTOMOBILE && (uint32)s.carType <= 0x17) {
        constexpr uint8 DAMAGE_TYPES[24]{ 0, 1, 2, 2, 0, 0, 0, 2, 2, 2, 2, 2, 2, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
        switch (DAMAGE_TYPES[s.carType]) {
        case 0:
            if (CGeneral::GetRandomNumberInRange(0, 20) == 0) {
                static_cast<CAutomobile*>(veh)->SetRandomDamage(false);
            }
            break;
        case 1:
            if (CGeneral::GetRandomNumberInRange(0, 8) == 0) {
                static_cast<CAutomobile*>(veh)->SetRandomDamage(true);
            }
            break;
        case 2:
            break;
        }
    }

    if (veh->m_nVehicleSubType == VEHICLE_TYPE_BIKE && ap.m_nCarDrivingStyle == DRIVING_STYLE_STOP_FOR_CARS) { // 0x431CB9
        veh->SetStatus(STATUS_PHYSICS);
        ap.m_nCarDrivingStyle = DRIVING_STYLE_DRIVINGMODE_AVOIDCARS_STOPFORPEDS_OBEYLIGHTS;
    }

    // 0x431CDF - Driver(s) of the car
    bool bDriversDone = false;
    if (!s.bBoat && s.carType != 13) {
        if (FindPlayerPed(-1)->GetWantedLevel() == eWantedLevel::WANTED_CLEAN) {
            // x87: `<= 0`, NaN => no
            if (CCheat::IsActive(CHEAT_AGGRESSIVE_DRIVERS) || TimeNextMadDriverChaseCreated <= 0.0f) {
                if (!s.bZoneTypeMatches && CreatePoliceChase(veh, s.carType, s.nodeA)) { // 0x431D34
                    if (CGameLogic::LaRiotsActiveHere()) {
                        TimeNextMadDriverChaseCreated = CGeneral::GetRandomNumberInRange(240.0f, 480.0f);
                    } else {
                        TimeNextMadDriverChaseCreated = CGeneral::GetRandomNumberInRange(600.0f, 1200.0f);
                    }
                    bDriversDone = true;
                }
            }
        }
    }

    if (!bDriversDone) {
        if (s.bMadDriver) { // 0x431D7D
            const auto model = veh->m_nModelIndex;
            const bool bBikerModel =
                model == MODEL_FREEWAY || model == MODEL_PCJ600 || model == MODEL_FCR900 ||
                model == MODEL_NRG500  || model == MODEL_BF400  || model == MODEL_WAYFARER;
            if (bBikerModel && !gbLARiots && CGeneral::GetRandomNumberInRange(0, 7) == 0 && CreateConvoy(veh, s.carType)) { // 0x431DCC
                SetUpDriverAndPassengersForVehicle(veh, s.carType, 1, true, false, 99); // 0x431DE2
            } else {
                SetUpDriverAndPassengersForVehicle(veh, s.carType, 1, true, false, 99); // 0x431DF9
                veh->SetStatus(STATUS_PHYSICS);
                ap.m_nCarDrivingStyle = DRIVING_STYLE_AVOID_CARS;
                ap.m_nCruiseSpeed     = (uint8)(int32)((double)ap.m_nCruiseSpeed + 10.0f); // 0x431E1C, 0x85862C
                veh->m_vecMoveSpeed   = (veh->GetForwardVector() * (float)ap.m_nCruiseSpeed) * 0.02f; // 0x431E4F, 0x431E5D, 0x431E6E (0x3CA3D70A)
                if (CGameLogic::LaRiotsActiveHere() || CCheat::IsActive(CHEAT_AGGRESSIVE_DRIVERS)) { // 0x431E87
                    if (veh->m_pDriver) {
                        veh->m_pDriver->bWantedByPolice = true; // 0x478 |= 0x800
                    }
                }
                veh->vehicleFlags.bMadDriver = true; // 0x42E |= 8
            }
        } else if (s.carType == 13 || s.carType == 24) { // 0x431EB6
            CCarAI::AddPoliceCarOccupants(veh, false); // 0x431EE5
        } else {
            bCarIsBeingCreated = true;
            SetUpDriverAndPassengersForVehicle(veh, s.carType, 0, false, false, 99); // 0x431ED1
            bCarIsBeingCreated = false;
        }
    }

    // 0x431EED
    if (s.carType == 13 || s.carType == 24) {
        veh->ChangeLawEnforcerState(true);
    }
    CStreaming::PossiblyStreamCarOutAfterCreation(veh->m_nModelIndex);

    // 0x431F18 - 0x421120 (CVehicleModelInfo, unnamed in the original): `++m_nTimesUsed`, capped at 120
    {
        auto* const mi = CModelInfo::GetVehicleModelInfo(veh->m_nModelIndex);
        mi->m_nTimesUsed = (uint8)std::min<int32>((int32)(int8)mi->m_nTimesUsed + 1, 0x78);
    }
}

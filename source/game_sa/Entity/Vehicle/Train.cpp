/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#include "StdInc.h"
#include "Core/X87Intrinsics.h"

#include "Train.h"

#include "Buoyancy.h"
#include "CarCtrl.h"
#include "CarEnterExit.h"
#include "CullZones.h"
#include "FileMgr.h"
#include "GameLogic.h"
#include "General.h"
#include "ModelIndices.h"
#include "PedGroups.h"
#include "Population.h"
#include "Replay.h"
#include "Stats.h"
#include "Streaming.h"
#include "TheZones.h"
#include "Weather.h"

#include "TaskComplexSequence.h"
#include "TaskComplexEnterCarAsDriver.h"
#include "TaskComplexEnterCarAsPassenger.h"
#include "TaskComplexLeaveCarAndWander.h"
#include "SeekEntity/TaskComplexSeekEntityXYOffset.h"

CVector CTrain::aStationCoors[6] = { // 0x8D48F8
    CVector{ 1741.0f, -1954.0f, 15.0f },
    CVector{ 1297.0f, -1898.0f, 3.0f  },
    CVector{ -1945.0f, 128.0f,  29.0f },
    CVector{ 1434.0f,  2632.0f, 13.0f },
    CVector{ 2783.0f,  1758.0f, 12.0f },
    CVector{ 2865.0f,  1281.0f, 12.0  }
};

auto& pTrackNodes = StaticRef<CTrainNode*[4]>(0xC38024);
auto& NumTrackNodes = StaticRef<std::array<int32, 4>>(0xC38014);
auto& arrTotalTrackLength = StaticRef<std::array<float, 4>>(0xC37FEC);
auto& StationDist = StaticRef<std::array<float, 6>>(0xC38034);
// Train configurations: 16 configs, each is a list of up to 15 model IDs terminated by 0
auto& TrainConfigs = StaticRef<int32[16][16]>(0x8D44F8);

void CTrain::InjectHooks() {
    RH_ScopedVirtualClass(CTrain, 0x872370, 66);
    RH_ScopedCategory("Vehicle");

    RH_ScopedInstall(Constructor, 0x6F6030);
    RH_ScopedInstall(InitTrains, 0x6F7440);
    RH_ScopedInstall(ReadAndInterpretTrackFile, 0x6F55D0);
    RH_ScopedInstall(Shutdown, 0x6F58D0);
    RH_ScopedInstall(UpdateTrains, 0x6F5900);
    RH_ScopedInstall(FindCoorsFromPositionOnTrack, 0x6F59E0);
    RH_ScopedInstall(FindMaximumSpeedToStopAtStations, 0x6F5BA0);
    RH_ScopedInstall(FindNumCarriagesPulled, 0x6F5CD0);
    RH_ScopedInstall(OpenTrainDoor, 0x6F5D80);
    RH_ScopedInstall(AddPassenger, 0x6F5D90);
    RH_ScopedInstall(RemovePassenger, 0x6F5DA0);
    RH_ScopedInstall(DisableRandomTrains, 0x6F5DB0);
    RH_ScopedInstall(RemoveOneMissionTrain, 0x6F5DC0);
    RH_ScopedInstall(ReleaseOneMissionTrain, 0x6F5DF0);
    RH_ScopedInstall(SetTrainSpeed, 0x6F5E20);
    RH_ScopedInstall(SetTrainCruiseSpeed, 0x6F5E50);
    RH_ScopedInstall(FindCaboose, 0x6F5E70);
    RH_ScopedInstall(FindEngine, 0x6F5E90);
    RH_ScopedInstall(FindCarriage, 0x6F5EB0);
    RH_ScopedInstall(FindSideStationIsOn, 0x6F5EF0);
    RH_ScopedInstall(FindNextStationPositionInDirection, 0x6F5F00);
    RH_ScopedInstall(IsInTunnel, 0x6F6320);
    RH_ScopedInstall(RemoveRandomPassenger, 0x6F6850);
    RH_ScopedInstall(RemoveMissionTrains, 0x6F6A20);
    RH_ScopedInstall(RemoveAllTrains, 0x6F6AA0);
    RH_ScopedInstall(ReleaseMissionTrains, 0x6F6B60);
    RH_ScopedInstall(FindClosestTrackNode, 0x6F6BD0);
    RH_ScopedInstall(FindPositionOnTrackFromCoors, 0x6F6CC0);
    RH_ScopedInstall(FindNearestTrain, 0x6F7090);
    RH_ScopedInstall(SetNewTrainPosition, 0x6F7140);
    RH_ScopedInstall(IsNextStationAllowed, 0x6F7260);
    RH_ScopedInstall(SkipToNextAllowedStation, 0x6F72F0);
    RH_ScopedInstall(CreateMissionTrain, 0x6F7550);
    RH_ScopedInstall(DoTrainGenerationAndRemoval, 0x6F7900);
    RH_ScopedInstall(AddNearbyPedAsRandomPassenger, 0x6F8170);
    RH_ScopedVMTInstall(ProcessControl, 0x6F86A0);

    RH_ScopedGlobalInstall(ProcessTrainAnnouncements, 0x6F5910);
    RH_ScopedGlobalInstall(PlayAnnouncement, 0x6F5920);
    RH_ScopedGlobalInstall(MarkSurroundingEntitiesForCollisionWithTrain, 0x6F6640);
    RH_ScopedGlobalInstall(TrainHitStuff<CPtrListSingleLink<CPhysical*>>, 0x6F5CF0);

    RH_ScopedVMTDestructorInstall(0x6F6300);
}

// 0x6F6030
CTrain::CTrain(int32 modelIndex, eVehicleCreatedBy createdBy) : CVehicle(createdBy) {
    std::memset(&m_aDoors, 0, sizeof(m_aDoors));

    m_nVehicleSubType = VEHICLE_TYPE_TRAIN;
    m_nVehicleType    = VEHICLE_TYPE_TRAIN;

    const auto mi = CModelInfo::GetModelInfo(modelIndex)->AsVehicleModelInfoPtr();
    m_pHandlingData = gHandlingDataMgr.GetVehiclePointer(mi->m_nHandlingId);
    m_nHandlingFlagsIntValue = m_pHandlingData->m_nHandlingFlags;

    CVehicle::SetModelIndex(modelIndex);
    SetupModelNodes();

    if (m_nModelIndex == MODEL_STREAKC) {
        m_aDoors[DOOR_LEFT_FRONT].Init(1.25f, 0.25f, DOOR_AXIS_NEG_Y, DOOR_AXIS_Z, DOOR_EXTRA_BASED);
        m_aDoors[DOOR_RIGHT_FRONT].Init(1.25f, 0.25f, DOOR_AXIS_NEG_Y, DOOR_AXIS_Z, DOOR_EXTRA_BASED);
    } else {
        // NOTE: `TWO_PI / 5.0f` isn't used, because the project's `TWO_PI` is less precise than the original's constant (0x3FA0D97C)
        m_aDoors[DOOR_LEFT_FRONT].Init(-1.2566371f, 0.0f, DOOR_AXIS_NEG_Y, DOOR_AXIS_Z, DOOR_EXTRA_BASED);
        m_aDoors[DOOR_RIGHT_FRONT].Init(+1.2566371f, 0.0f, DOOR_AXIS_NEG_Y, DOOR_AXIS_Z, DOOR_EXTRA_BASED);
    }

    // NOTSA: The original code only touches some of the flags here, the rest (`bIsFrontCarriage`, `bIsLastCarriage` and `bClockwiseDirection`) are left as they are
    // (they are always initialized later by `CreateMissionTrain`)
    trainFlags.b01                         = true;
    trainFlags.bStoppedAtStation           = false;
    trainFlags.bPassengersCanEnterAndLeave = false;
    trainFlags.bMissionTrain               = false;
    trainFlags.bStopsAtStations            = true;
    trainFlags.bNotOnARailRoad             = false;
    trainFlags.bForceSlowDown              = true;
    trainFlags.bIsStreakModel              = false;

    m_nPassengersGenerationState = TRAIN_PASSENGERS_QUERY_NUM_PASSENGERS_TO_LEAVE;
    m_nNumPassengersToLeave = CGeneral::GetRandomNumber() & 3;       // [0, 3]
    m_nNumPassengersToEnter = (CGeneral::GetRandomNumber() & 3) + 1; // [1, 4]
    m_pTemporaryPassenger = nullptr;
    m_nMaxPassengers = 5;
    physicalFlags.bDisableSimpleCollision = true;
    SetUsesCollision(true);
    m_nTimeWhenCreated = CTimer::GetTimeInMS();
    field_5C8 = 0;
    m_nTrackId = 0;
    m_fCurrentRailDistance = 0.0f;
    m_fTrainSpeed = 0.0f;
    m_nTimeWhenStoppedAtStation = 0;
    mi->ChooseVehicleColour(m_nPrimaryColor, m_nSecondaryColor, m_nTertiaryColor, m_nQuaternaryColor, 1);
    m_fMass = m_pHandlingData->m_fMass;
    m_fTurnMass = m_pHandlingData->m_fTurnMass;
    m_vecCentreOfMass = m_pHandlingData->m_vecCentreOfMass;
    m_fElasticity = 0.05f;
    m_fBuoyancyConstant = m_pHandlingData->m_fBuoyancyConstant;
    m_fAirResistance = GetDefaultAirResistance();

    physicalFlags.bCollidable = false;
    physicalFlags.bDisableCollisionForce = true;
    m_bTunnelTransition = true;
    SetStatus(STATUS_TRAIN_MOVING);
    m_pPrevCarriage = nullptr;
    m_pNextCarriage = nullptr;
    m_autoPilot.m_speed = 0.0f;
    m_autoPilot.SetCruiseSpeed(0);
    m_vehicleAudio.Initialise(this);
}

void CTrain::SetupModelNodes() {
    std::ranges::fill(m_aTrainNodes, nullptr);
    CClumpModelInfo::FillFrameArray(GetRpClump(), m_aTrainNodes.data());
}

// 0x6F7440
void CTrain::InitTrains() {
    ZoneScoped;

    bDisableRandomTrains = false;
    GenTrain_Status = 0;

    constexpr const char* filenames[] {
        "data\\paths\\tracks.dat",
        "data\\paths\\tracks3.dat",
        "data\\paths\\tracks2.dat",
        "data\\paths\\tracks4.dat",
    };
    for (auto i = 0u; i < std::size(filenames); ++i) {
        if (!pTrackNodes[i]) {
            ReadAndInterpretTrackFile(filenames[i], &pTrackNodes[i], &NumTrackNodes[i], &arrTotalTrackLength[i], (int32)i);
        }
    }

    // BUG: The track the closest node was found on (`trackId`) is ignored, and the node index is always used with the nodes of the first track
    const auto* const trackNodes = pTrackNodes[0];
    for (auto i = 0u; i < std::size(aStationCoors); ++i) {
        int32 trackId;
        const auto nodeIdx = FindClosestTrackNode(aStationCoors[i], &trackId);
        StationDist[i] = (float)trackNodes[nodeIdx].m_nDistanceFromStart * ExeRecip(3.0f);
    }
}

// 0x6F55D0
void CTrain::ReadAndInterpretTrackFile(const char* filename, CTrainNode** nodes, int32* lineCount, float* totalDist, int32 skipStations) {
    if (!*nodes) {
        auto* const fileData = new char[0xB530];
        CFileMgr::LoadFile(filename, reinterpret_cast<uint8*>(fileData), 0xB530, "rb");

        size_t pos = 0;
        const auto ReadLine = [&](bool terminate) {
            size_t n = 0;
            while (fileData[pos] != '\n') {
                gString[n++] = fileData[pos++];
            }
            if (terminate) {
                gString[n] = '\0';
            }
            pos++; // Skip the newline
        };

        ReadLine(true);
        if (std::strcmp(gString, "processed") == 0) {
            ReadLine(true);
        }
        sscanf(gString, "%d", lineCount);

        *nodes = new CTrainNode[*lineCount];

        auto numStationsFound = 0u;
        for (auto i = 0; i < *lineCount; i++) {
            // BUG: The line isn't null terminated, so whatever was left over in the buffer from the previous line is also parsed
            ReadLine(false);

            float x, y, z;
            int32 isStation;
            sscanf(gString, "%f %f %f %d", &x, &y, &z, &isStation);

            auto& node = (*nodes)[i];
            node.SetX(x);
            node.SetY(y);
            node.SetZ(z);

            if (skipStations == 0 && isStation) {
                aStationCoors[numStationsFound++] = CVector{ x, y, z }; // BUG: No bounds check
            }
        }

        delete[] fileData;
    }

    // Calculate the distance of each node from the start of the track
    if (*lineCount > 0) {
        double total = 0.0;
        for (auto i = 0; i < *lineCount; i++) {
            const auto& node = (*nodes)[i];
            const auto& nextNode = (*nodes)[(i + 1) % *lineCount];

            (*nodes)[i].m_nDistanceFromStart = (uint16)(int32)(total * 3.0);

            const double dy = (double)node.GetY() - (double)nextNode.GetY();
            const double dx = (double)node.GetX() - (double)nextNode.GetX();
            total += std::sqrt(dy * dy + dx * dx);
        }
        *totalDist = (float)total;
    } else {
        *totalDist = 0.0f;
    }
}

// 0x6F58D0
void CTrain::Shutdown() {
    for (auto node : pTrackNodes) {
        delete node;
        node = nullptr;
    }
}

// 0x6F5900
void CTrain::UpdateTrains() {
    ZoneScoped;

    // NOP
}

// 0x6F5910
void ProcessTrainAnnouncements() {
    // NOP
}

// 0x6F5920
void PlayAnnouncement(uint8 arg0, uint8 arg1) {
    // NOP
}

// 0x6F59E0
void CTrain::FindCoorsFromPositionOnTrack(float railDistance, int32 trackId, CVector* outCoors) {
    const auto numNodes = NumTrackNodes[trackId];
    if (numNodes <= 0) {
        return;
    }

    const auto* const nodes = pTrackNodes[trackId];
    for (auto i = 0; i < numNodes; i++) {
        const auto& node     = nodes[i];
        const auto& nextNode = nodes[(i + 1) % numNodes];

        const float distToNode     = railDistance - (float)node.m_nDistanceFromStart * ExeRecip(3.0f);
        const float distToNextNode = (float)nextNode.m_nDistanceFromStart * ExeRecip(3.0f) - railDistance;
        if (!(0.0f <= distToNode && 0.0f <= distToNextNode)) {
            continue;
        }

        // Interpolate between the 2 nodes
        const float invTotal = 1.0f / (distToNextNode + distToNode);
        *outCoors = (node.GetPosn() * distToNextNode + nextNode.GetPosn() * distToNode) * invTotal;
        return;
    }
}

// 0x6F5BA0
bool CTrain::FindMaximumSpeedToStopAtStations(float* speed) {
    *speed = 50.0f;
    if (m_nTrackId != 0) { // Stations are only on the first track
        return false;
    }

    const float totalTrackLength = arrTotalTrackLength[0];
    const bool  isClockwise      = trainFlags.bClockwiseDirection;

    float closestDist = 10'000.0f;
    for (const float stationDist : StationDist) {
        float dist = stationDist - m_fCurrentRailDistance;
        dist += isClockwise ? 40.0f : -40.0f;

        // Wrap around into the range [-total / 2, total / 2]
        while (dist > totalTrackLength * 0.5f) {
            dist -= totalTrackLength;
        }
        while (dist < totalTrackLength * -0.5f) {
            dist += totalTrackLength;
        }

        if (isClockwise) {
            if (!(dist > 0.0f)) {
                continue;
            }
        } else {
            if (!(dist < 0.0f)) {
                continue;
            }
            dist = -dist;
        }
        if (dist < closestDist) {
            closestDist = dist;
        }
    }

    if (closestDist >= 500.0f) {
        *speed = 100'000.0f;
    } else {
        *speed = (1.0f - (500.0f - closestDist) * 0.002f) * 50.0f;
    }
    return closestDist < 5.0f;
}

// 0x6F5CD0
uint32 CTrain::FindNumCarriagesPulled() {
    uint32 num;
    CTrain* carriage = m_pNextCarriage;
    for (num = 0; carriage; ++num) {
        carriage = carriage->m_pNextCarriage;
    }
    return num;
}

// 0x6F5D80
void CTrain::OpenTrainDoor(float state) {
    // NOP
}

// 0x6F5D90
void CTrain::AddPassenger(CPed* ped) {
    // NOP
}

// 0x6F5DA0
void CTrain::RemovePassenger(CPed* ped) {
    // NOP
}

// 0x6F5DB0
void CTrain::DisableRandomTrains(bool disable) {
    bDisableRandomTrains = disable;
}

// 0x6F5DC0
void CTrain::RemoveOneMissionTrain(CTrain* train) {
    if (!train)
        return;

    CTrain* next;
    CTrain* _train = train;
    do {
        next = _train->m_pNextCarriage;
        CWorld::Remove(_train);
        delete _train;
        _train = next;
    } while (next);
}

// 0x6F5DF0
void CTrain::ReleaseOneMissionTrain(CTrain* train) {
    for (auto* head = train; head; head = head->m_pNextCarriage) {
        head->trainFlags.bMissionTrain = false;
    }
}

// 0x6F5E20
void CTrain::SetTrainSpeed(CTrain* train, float speed) {
    train->m_fTrainSpeed = speed * ExeRecip(50.0f);
    if (!train->trainFlags.bClockwiseDirection) {
        train->m_fTrainSpeed = -train->m_fTrainSpeed;
    }
}

// 0x6F5E50
void CTrain::SetTrainCruiseSpeed(CTrain* train, float speed) {
    train->m_autoPilot.SetCruiseSpeed((uint8)speed);
}

// 0x6F5E70
CTrain* CTrain::FindCaboose(CTrain* train) {
    while (train->m_pNextCarriage) {
        train = train->m_pNextCarriage;
    }
    return train;
}

// 0x6F5E90
CTrain* CTrain::FindEngine(CTrain* train) {
    while (train->m_pPrevCarriage) {
        train = train->m_pPrevCarriage;
    }
    return train;
}

// 0x6F5EB0
CTrain* CTrain::FindCarriage(CTrain* train, uint8 carriage) {
    for (uint8 i = 0; i < carriage; i++) {
        train = train->m_pNextCarriage;
        if (!train) {
            return nullptr;
        }
    }
    return train;
}

// 0x6F5EF0
bool CTrain::FindSideStationIsOn() const {
    return trainFlags.bClockwiseDirection; // ?
}

// 0x6F5F00
void CTrain::FindNextStationPositionInDirection(bool clockwiseDirection, float distance, float* distanceToStation, int32* numStations) {
    // Find the first station that's after `distance`
    int32 stationIdx = 0;
    for (auto i = 0u; i < StationDist.size(); i++) {
        if (StationDist[i] > distance) {
            stationIdx = (int32)i;
            break;
        }
    }

    if (!clockwiseDirection) {
        if (--stationIdx < 0) {
            stationIdx += 6;
        }
    }

    // We're (almost) at this station already, so go to the next one
    if (std::fabs(distance - StationDist[stationIdx]) < 100.0f) {
        stationIdx += clockwiseDirection ? 1 : -1;
        if (stationIdx < 0) {
            stationIdx += 6;
        }
        if (stationIdx >= 6) {
            stationIdx = 0;
        }
    }

    *numStations = stationIdx;
    *distanceToStation = StationDist[stationIdx];
}

// 0x6F6320
bool CTrain::IsInTunnel() const {
    const auto& pos = GetPosition();
    CColPoint colPoint{};
    CEntity* colEntity{};
    return CWorld::ProcessVerticalLine(pos, pos.z + 100.0f, colPoint, colEntity, true);
}

// 0x6F6640
void MarkSurroundingEntitiesForCollisionWithTrain(CVector pos, float radius, CEntity* entity, bool bOnlyVehicles) {
    int32 startSectorX = std::max(CWorld::GetSectorX(pos.x - radius), 0);
    int32 startSectorY = std::max(CWorld::GetSectorY(pos.y - radius), 0);
    int32 endSectorX   = std::min(CWorld::GetSectorX(pos.x + radius), MAX_SECTORS_X - 1);
    int32 endSectorY   = std::min(CWorld::GetSectorY(pos.y + radius), MAX_SECTORS_Y - 1);

    CWorld::AdvanceCurrentScanCode();

    for (int32 sectorY = startSectorY; sectorY <= endSectorY; ++sectorY) {
        for (int32 sectorX = startSectorX; sectorX <= endSectorX; ++sectorX) {
            auto& repeatSector = CWorld::GetRepeatSector(sectorX, sectorY);
            TrainHitStuff(repeatSector.Vehicles, entity);
            if (!bOnlyVehicles) {
                TrainHitStuff(repeatSector.Peds, entity);
                TrainHitStuff(repeatSector.Objects, entity);
            }
        }
    }
}

// 0x6F5CF0
template<typename PtrListType>
void TrainHitStuff(PtrListType& ptrList, CEntity* entity) {
    for (CPhysical* const physical : ptrList) {
        if (physical == entity) {
            continue;
        }

        physical->physicalFlags.bProcessCollisionEvenIfStationary = true;

        // Wake up the magnocrane's objects
        if (physical->GetType() == ENTITY_TYPE_OBJECT
            && (physical->m_bIsStatic || physical->m_bIsStaticWaitingForCollision)
            && (   physical->m_nModelIndex == ModelIndices::MI_OBJECTFORMAGNOCRANE1
                || physical->m_nModelIndex == ModelIndices::MI_OBJECTFORMAGNOCRANE2
                || physical->m_nModelIndex == ModelIndices::MI_OBJECTFORMAGNOCRANE3)
        ) {
            physical->SetIsStatic(false);
            physical->AddToMovingList();
            physical->m_nFakePhysics = 0;
        }
    }
}

// 0x6F6850
void CTrain::RemoveRandomPassenger() {
    if (CReplay::Mode == MODE_PLAYBACK) {
        return;
    }

    auto* const player = FindPlayerPed();
    if (player->m_pVehicle == this) {
        return;
    }

    // Don't remove anyone if the player is about to enter this train
    if (auto* const task = player->GetTaskManager().GetTaskPrimary(TASK_PRIMARY_PRIMARY)) {
        const auto type = task->GetTaskType();
        if ((type == TASK_COMPLEX_ENTER_CAR_AS_DRIVER || type == TASK_COMPLEX_ENTER_CAR_AS_PASSENGER)
            && static_cast<CTaskComplexEnterCar*>(task)->GetTargetCar() == this
        ) {
            return;
        }
    }

    const bool isClockwise = trainFlags.bClockwiseDirection;

    // NOTSA: This is dead code, as the player's vehicle was already checked above
    if (player->bInVehicle && player->m_pVehicle == this) {
        if (isClockwise) {
            if (m_pDriver == player) {
                return;
            }
        } else if (m_pDriver != player) {
            return;
        }
    }

    if (m_nNumPassengersToLeave == 0) {
        return;
    }

    if (m_pTemporaryPassenger) {
        if (m_pTemporaryPassenger->bInVehicle) {
            return;
        }
        CEntity::ClearReference(m_pTemporaryPassenger);
    }

    int32 carRating = CGeneral::GetRandomNumberInRange(0, 12);
    if (carRating == 12) {
        carRating = 11;
    }
    if (CGeneral::GetRandomNumber() < 100) {
        carRating = 25;
    }

    auto* const ped = CPopulation::AddPedInCar(this, !isClockwise, carRating, 0, false, false);
    if (!ped) {
        return;
    }

    ped->bJustGotOffTrain = true; // Ped flag dword at +0x478, mask 0x40000: marks the ped as a temporary train passenger
    m_nNumPassengersToLeave = m_nNumPassengersToLeave - 1;
    m_pTemporaryPassenger = ped;
    CEntity::RegisterReference(m_pTemporaryPassenger);

    ped->GetTaskManager().SetTask(new CTaskComplexLeaveCarAndWander{ this, TARGET_DOOR_FRONT_LEFT, 0, true }, TASK_PRIMARY_PRIMARY);
}

// 0x6F6A20
void CTrain::RemoveMissionTrains() {
    for (auto& vehicle : GetVehiclePool()->GetAllValid()) {
        if (vehicle.IsTrain() &&
            &vehicle != FindPlayerVehicle() &&
            vehicle.AsTrain()->trainFlags.bMissionTrain
        ) {
            CWorld::Remove(&vehicle);
            delete &vehicle;
        }
    }
}

// 0x6F6AA0
void CTrain::RemoveAllTrains() {
    auto* const pool = GetVehiclePool();
    for (auto i = pool->GetSize(); i-- > 0;) {
        auto* const vehicle = pool->GetAt(i);
        if (!vehicle || !vehicle->IsTrain()) {
            continue;
        }
        auto* const train = vehicle->AsTrain();

        // Don't remove the train if the player is in it
        bool isPlayerInTrain = false;
        for (auto* c = train; c; c = c->m_pPrevCarriage) {
            if (c == FindPlayerVehicle()) {
                isPlayerInTrain = true;
            }
        }
        for (auto* c = train; c; c = c->m_pNextCarriage) {
            if (c == FindPlayerVehicle()) {
                isPlayerInTrain = true;
            }
        }

        if (!isPlayerInTrain) {
            CWorld::Remove(train);
            delete train;
        }
    }
}

// 0x6F6B60
void CTrain::ReleaseMissionTrains() {
    for (auto& vehicle : GetVehiclePool()->GetAllValid()) {
        if (vehicle.IsTrain() && &vehicle != FindPlayerVehicle()) {
            vehicle.AsTrain()->trainFlags.bMissionTrain = false;
        }
    }
}

// 0x6F6BD0
int32 CTrain::FindClosestTrackNode(CVector posn, int32* outTrackId) {
    float closestDist = 99999.9f;
    int32 closestNode = 0; // NOTSA: Uninitialized in the original (only matters if there are no nodes at all)
    for (auto trackId = 0u; trackId < std::size(pTrackNodes); trackId++) {
        for (auto nodeIdx = 0; nodeIdx < NumTrackNodes[trackId]; nodeIdx++) {
            const auto dist = (posn - pTrackNodes[trackId][nodeIdx].GetPosn()).Magnitude();
            if (dist < closestDist) {
                *outTrackId = (int32)trackId;
                closestNode = nodeIdx;
                closestDist = dist;
            }
        }
    }
    return closestNode;
}

// 0x6F6CC0
void CTrain::FindPositionOnTrackFromCoors() {
    const auto numNodes = NumTrackNodes[m_nTrackId];
    if (numNodes <= 0) {
        return;
    }

    auto* const nodes = pTrackNodes[m_nTrackId];
    for (auto i = 0; i < numNodes; i++) {
        auto& node     = nodes[i];
        auto& nextNode = nodes[(i + 1) % numNodes];

        const float nodeX = node.GetX(), nodeY = node.GetY();
        const float nextX = nextNode.GetX(), nextY = nextNode.GetY();
        const CVector pos = GetPosition();

        // Project our position onto the segment `node -> nextNode` (in 2D)
        const float segLen = std::sqrt((nodeX - nextX) * (nodeX - nextX) + (nodeY - nextY) * (nodeY - nextY));
        const float t = ((pos.x - nodeX) * (nextX - nodeX) + (pos.y - nodeY) * (nextY - nodeY)) / (segLen * segLen);
        if (!(0.001f < t && t < 1.001f)) {
            continue;
        }

        const float closestX = (nextX - nodeX) * t + nodeX;
        const float closestY = (nextY - nodeY) * t + nodeY;
        const float zTerm    = t * 0.0f; // They (probably) meant to use the Z coordinate here, but it's always 0
        const float dist     = std::sqrt((closestX - pos.x) * (closestX - pos.x) + (closestY - pos.y) * (closestY - pos.y) + zTerm * zTerm);

        if (dist < 3.0f) {
            // We're on the track
            const float nodeDist     = (float)node.m_nDistanceFromStart * ExeRecip(3.0f);
            const float nextNodeDist = (float)nextNode.m_nDistanceFromStart * ExeRecip(3.0f);
            m_fCurrentRailDistance = (nextNodeDist - nodeDist) * t + nodeDist;

            // The position is at the center of the carriage, but the rail distance is at the front
            const auto& bbox = CModelInfo::GetModelInfo(m_nModelIndex)->GetColModel()->GetBoundingBox();
            m_fCurrentRailDistance -= bbox.GetLength() * 0.5f;
            if (m_fCurrentRailDistance < 0.0f) {
                m_fCurrentRailDistance += arrTotalTrackLength[m_nTrackId];
            }

            // 0x6F6F8D (oracle-proven): the inlined magnitude sums z, y, x in that order; the dot product is (dy*vy + vz*0) + dx*vx
            const float speed = (float)x87::sqrt((double)((m_vecMoveSpeed.z * m_vecMoveSpeed.z + m_vecMoveSpeed.y * m_vecMoveSpeed.y) + m_vecMoveSpeed.x * m_vecMoveSpeed.x));
            m_fTrainSpeed = speed;

            const float dot = ((nextY - nodeY) * m_vecMoveSpeed.y + m_vecMoveSpeed.z * 0.0f) + (nextX - nodeX) * m_vecMoveSpeed.x;
            if (trainFlags.bClockwiseDirection == (dot > 0.0f)) {
                m_fTrainSpeed = -speed;
            }
            return;
        }

        // Not close enough to the track, so only update the lighting
        const float lighting     = node.GetLightingFromCollision().GetCurrentLighting();
        const float nextLighting = nextNode.GetLightingFromCollision().GetCurrentLighting();
        m_fContactSurfaceBrightness = (nextLighting - lighting) * t + lighting;
    }
}

// 0x6F7090
CTrain* CTrain::FindNearestTrain(CVector posn, bool mustBeMainTrain) {
    CTrain* nearest = nullptr;
    float nearestDist = 10'000'000.0f;

    auto* const pool = GetVehiclePool();
    for (auto i = pool->GetSize(); i-- > 0;) {
        auto* const vehicle = pool->GetAt(i);
        if (!vehicle || !vehicle->IsTrain()) {
            continue;
        }
        auto* const train = vehicle->AsTrain();

        const auto& trainPos = train->GetPosition();
        const float dist = std::sqrt((trainPos.x - posn.x) * (trainPos.x - posn.x) + (trainPos.y - posn.y) * (trainPos.y - posn.y));
        if (dist < nearestDist && (!mustBeMainTrain || train->trainFlags.bIsFrontCarriage)) {
            nearest = train;
            nearestDist = dist;
        }
    }
    return nearest;
}

// 0x6F7140
void CTrain::SetNewTrainPosition(CTrain* train, CVector posn) {
    train->SetPosn(posn);
    train->FindPositionOnTrackFromCoors();
}

// 0x6F7260
bool CTrain::IsNextStationAllowed(CTrain* train) {
    const auto* const engine = FindEngine(train);

    float distToStation;
    int32 stationIdx;
    FindNextStationPositionInDirection(engine->trainFlags.bClockwiseDirection, engine->m_fCurrentRailDistance, &distToStation, &stationIdx);

    const auto level = (float)CTheZones::GetLevelFromPosition(aStationCoors[stationIdx]);
    return !(CStats::GetStatValue(STAT_CITY_UNLOCKED) + 1.0f < level);
}

// 0x6F72F0
void CTrain::SkipToNextAllowedStation(CTrain* train) {
    auto* const engine = FindEngine(train);

    float distance = engine->m_fCurrentRailDistance;
    int32 stationIdx;
    float level;
    do {
        FindNextStationPositionInDirection(engine->trainFlags.bClockwiseDirection, distance, &distance, &stationIdx);
        level = (float)CTheZones::GetLevelFromPosition(aStationCoors[stationIdx]);
    } while (CStats::GetStatValue(STAT_CITY_UNLOCKED) + 1.0f < level);

    // Place the train a bit before the station
    if (engine->trainFlags.bClockwiseDirection) {
        engine->m_fCurrentRailDistance = distance - 20.0f;
        engine->m_fTrainSpeed = 0.1f;
    } else {
        engine->m_fCurrentRailDistance = distance + 20.0f;
        engine->m_fTrainSpeed = -0.1f;
    }

    const auto& stationPos = aStationCoors[stationIdx];
    CStreaming::LoadScene(stationPos);
    CStreaming::LoadAllRequestedModels(false);

    // Pass some time, depending on how far the station is
    const auto& enginePos = engine->GetPosition();
    const auto  dist2D = std::sqrt((stationPos.x - enginePos.x) * (stationPos.x - enginePos.x) + (stationPos.y - enginePos.y) * (stationPos.y - enginePos.y));
    CGameLogic::PassTime((uint32)(dist2D * 0.05f + 23.0f));
}

// 0x6F7550
void CTrain::CreateMissionTrain(CVector posn, bool clockwiseDirection, uint32 trainType, CTrain** outFirstCarriage, CTrain** outLastCarriage, int32 nodeIndex, int32 trackId, bool isMissionTrain) {
    CTrain* carriages[16]{};
    int32   numCarriages = 0;

    if (nodeIndex < 0) {
        nodeIndex = FindClosestTrackNode(posn, &trackId);
    }

    float railDistance = (float)pTrackNodes[trackId][nodeIndex].m_nDistanceFromStart * ExeRecip(3.0f);

    CTrain* prev = nullptr;
    for (const int32* modelId = TrainConfigs[trainType]; *modelId; modelId++) {
        auto* const train = new CTrain(*modelId, PERMANENT_VEHICLE);
        train->GetMatrix().SetTranslate(CVector{ 0.0f, 0.0f, 0.0f });

        train->m_nNodeIndex = (int16)nodeIndex;
        train->SetStatus(STATUS_ABANDONED);
        train->vehicleFlags.bIsLocked = true;
        train->m_fCurrentRailDistance = railDistance;
        train->trainFlags.bMissionTrain = isMissionTrain;
        train->trainFlags.bClockwiseDirection = clockwiseDirection;
        train->m_nTrackId = (uint8)trackId;

        bool setLength = true;
        if (isMissionTrain) {
            train->trainFlags.bStopsAtStations = false;
            if (!prev) {
                train->SetPosn(posn);
                train->FindPositionOnTrackFromCoors();
                train->m_fLength = 0.0f;
                setLength = false;
            }
        } else if (!prev) {
            train->m_fLength = 0.0f;
            setLength = false;
        }
        if (setLength) {
            if (clockwiseDirection) {
                const auto length = CModelInfo::GetModelInfo(train->m_nModelIndex)->GetColModel()->GetBoundingBox().GetLength();
                train->m_fLength = -length;
                railDistance -= length;
            } else {
                const auto length = CModelInfo::GetModelInfo(prev->m_nModelIndex)->GetColModel()->GetBoundingBox().GetLength();
                train->m_fLength = length;
                railDistance += length;
            }
        }

        carriages[numCarriages++] = train;

        if (!prev) {
            train->trainFlags.bIsFrontCarriage = true;
            if (outFirstCarriage) {
                *outFirstCarriage = train;
            }
        } else {
            train->trainFlags.bIsFrontCarriage = false;
            train->vehicleFlags.bHasBeenOwnedByPlayer = true; // NOTSA: bit 17 of the vehicle flags (byte 0x42A, mask 0x2)
            prev->trainFlags.bIsLastCarriage = false;
        }
        train->trainFlags.bIsLastCarriage = true;
        if (outLastCarriage) {
            *outLastCarriage = train;
        }

        train->m_pPrevCarriage = prev;
        CEntity::SafeRegisterRef(train->m_pPrevCarriage);
        train->m_pNextCarriage = nullptr;
        if (prev) {
            prev->m_pNextCarriage = train;
            CEntity::RegisterReference(prev->m_pNextCarriage);
        }

        train->ProcessControl();

        prev = train;
    }

    for (auto i = numCarriages - 1; i >= 0; i--) {
        CWorld::Remove(carriages[i]);
        CWorld::Add(carriages[i]);
    }

    // BUG: `outFirstCarriage` is dereferenced without checking if it's null
    if ((*outFirstCarriage)->m_nModelIndex != MODEL_TRAM) {
        CPopulation::AddPedInCar(*outFirstCarriage, true, -1, 0, false, false);
        if (*outFirstCarriage && (*outFirstCarriage)->m_pDriver) {
            (*outFirstCarriage)->m_pDriver->GetIntelligence()->SetPedDecisionMakerType(6);
        }
    }

    // Is there a streak (the long train) model in the train?
    bool hasStreak = false;
    for (auto* c = *outFirstCarriage; c; c = c->m_pNextCarriage) {
        if (c->m_nModelIndex == MODEL_STREAK) {
            hasStreak = true;
            break;
        }
    }
    for (auto* c = *outFirstCarriage; c; c = c->m_pNextCarriage) {
        c->trainFlags.bIsStreakModel = hasStreak;
    }
}

// 0x6F7900
void CTrain::DoTrainGenerationAndRemoval() {
    // Check if the player is near a station every 3 seconds
    if (CTimer::GetTimeInMS() / 3000 != CTimer::m_snPreviousTimeInMilliseconds / 3000) {
        GenTrain_IsNearStation = false;
        for (const auto& stationPos : aStationCoors) {
            const auto playerPos = FindPlayerCoors();
            const auto dist = std::sqrt((stationPos.x - playerPos.x) * (stationPos.x - playerPos.x) + (stationPos.y - playerPos.y) * (stationPos.y - playerPos.y));
            if (dist < 60.0f) {
                GenTrain_IsNearStation = true;
            }
        }
    }

    // Distance from the camera at which trains are generated (closer is considered too close)
    uint32 interval;
    float  generationRange;
    if (GenTrain_IsNearStation) {
        interval        = 1;
        generationRange = 100.0f;
    } else {
        interval        = 950;
        generationRange = 70.0f;
    }
    if (CTimer::GetTimeInMS() / interval == CTimer::m_snPreviousTimeInMilliseconds / interval) {
        return;
    }

    const auto Dist2D = [](const CVector& a, const CVector& b) {
        return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
    };

    // Remove trains that are far away from the camera
    bool anyMainTrainExists = false;
    auto* const vehiclePool = GetVehiclePool();
    for (auto i = vehiclePool->GetSize(); i-- > 0;) {
        auto* const vehicle = vehiclePool->GetAt(i);
        if (!vehicle || !vehicle->IsTrain()) {
            continue;
        }
        auto* const train = vehicle->AsTrain();
        if (!train->trainFlags.bIsFrontCarriage || train->trainFlags.bMissionTrain) {
            continue;
        }

        anyMainTrainExists = true;

        bool canRemove = true;
        for (auto* c = train; c; c = c->m_pNextCarriage) {
            if (c == FindPlayerVehicle() || Dist2D(TheCamera.GetPosition(), c->GetPosition()) < 220.0f) {
                canRemove = false;
            }
        }
        if (canRemove) {
            for (auto* c = train; c;) {
                auto* const next = c->m_pNextCarriage;
                CWorld::Remove(c);
                delete c;
                c = next;
            }
        }
    }

    if (bDisableRandomTrains) {
        return;
    }

    const auto GetTrackNodePos = [](uint32 trackId, int32 nodeIdx) {
        return pTrackNodes[trackId][nodeIdx].GetPosn();
    };

    if (GenTrain_Status == 0) { // Looking for a place to generate a train at
        if (anyMainTrainExists) {
            return;
        }

        const CVector camPos = TheCamera.GetPosition();

        // Moves `GenTrain_GenerationNode` one node along the track (in the generation direction), and returns the new distance to the camera
        const auto StepNode = [&]() {
            const auto numNodes = NumTrackNodes[GenTrain_Track];
            auto node = (int32)GenTrain_GenerationNode;
            if (GenTrain_Direction) {
                if (--node < 0) {
                    node += numNodes;
                }
            } else {
                node = (node + 1) % numNodes;
            }
            GenTrain_GenerationNode = (uint32)node;
            return Dist2D(GetTrackNodePos(GenTrain_Track, node), camPos);
        };

        GenTrain_Track = 0;
        GenTrain_GenerationNode = CGeneral::GetRandomNumber() % NumTrackNodes[0];
        const auto nodePos = GetTrackNodePos(GenTrain_Track, GenTrain_GenerationNode);
        float dist = Dist2D(nodePos, camPos);

        bool tryOtherTrack = true;
        if (dist < generationRange) {
            tryOtherTrack = false;

            // Don't generate the train if the node is in a tunnel, but the player is outside of one
            if (nodePos.z + 6.0f <= FindPlayerCoors().z) {
                const auto playerAttrs = (uint16)CCullZones::FindTunnelAttributesForCoors(FindPlayerCoors());
                if ((playerAttrs & (TUNNEL | TUNNEL_TRANSITION)) == 0) {
                    if ((uint16)CCullZones::FindTunnelAttributesForCoors(nodePos) & TUNNEL) {
                        tryOtherTrack = true;
                    }
                }
            }
        }

        if (!tryOtherTrack) {
            GenTrain_Direction = CGeneral::GetRandomNumber() & 1;

            // The node before the one we ended up at
            auto prevNode = (int32)GenTrain_GenerationNode;
            while (dist < 170.0f) {
                prevNode = (int32)GenTrain_GenerationNode;
                dist = StepNode();
            }

            GenTrain_LastConfig = (GenTrain_LastConfig + 1) & 7;
            GenTrain_TrainConfig = GenTrain_LastConfig;
            GenTrain_Status = 1;

            if (dist > 220.0f) {
                GenTrain_GenerationNode = prevNode;
            }
            return;
        }

        // Try the second track (the one in San Fierro)
        if (GenTrain_Status != 0 || CWeather::WeatherRegion != WEATHER_REGION_SF) {
            return;
        }

        GenTrain_Track = 1;
        GenTrain_GenerationNode = CGeneral::GetRandomNumber() % NumTrackNodes[1];
        dist = Dist2D(GetTrackNodePos(GenTrain_Track, GenTrain_GenerationNode), camPos);
        if (dist >= generationRange) {
            return;
        }

        GenTrain_Direction = CGeneral::GetRandomNumber() & 1;
        while (dist < 170.0f) {
            dist = StepNode();
        }
        if (dist < 220.0f) {
            GenTrain_Status = 1;
            GenTrain_TrainConfig = (CGeneral::GetRandomNumber() & 1) + 8;
        }
    } else if (GenTrain_Status == 1) { // Generate the train (once the models are loaded)
        const auto* const models = TrainConfigs[GenTrain_TrainConfig];

        bool allModelsLoaded = true;
        for (auto* modelId = models; *modelId; modelId++) {
            if (!CStreaming::GetInfo(*modelId).IsLoaded()) {
                CStreaming::RequestModel(*modelId, STREAMING_KEEP_IN_MEMORY);
                allModelsLoaded = false;
            }
        }
        if (!allModelsLoaded) {
            return;
        }

        const CVector camPos = TheCamera.GetPosition();
        if (Dist2D(GetTrackNodePos(GenTrain_Track, GenTrain_GenerationNode), camPos) > 60.0f) {
            CTrain* train{};
            CreateMissionTrain(
                CVector{ 0.0f, 0.0f, 0.0f },
                GenTrain_Direction != 0,
                GenTrain_TrainConfig,
                &train,
                nullptr,
                (int32)GenTrain_GenerationNode,
                (int32)GenTrain_Track,
                false
            );

            int32 speed;
            if (GenTrain_Track == 0) {
                speed = CGeneral::GetRandomNumber() % 30 + 15;
                // Slow down near the Unity station
                if (Dist2D(GetTrackNodePos(GenTrain_Track, GenTrain_GenerationNode), CVector{ 2222.0f, -1750.0f, 0.0f }) < 300.0f) {
                    speed /= 2;
                }
            } else {
                speed = CGeneral::GetRandomNumber() % 7 + 7;
            }

            float maxSpeed;
            train->FindMaximumSpeedToStopAtStations(&maxSpeed);
            if ((float)speed < maxSpeed) {
                maxSpeed = (float)speed;
            }
            SetTrainSpeed(train, maxSpeed);
            train->m_autoPilot.m_nCruiseSpeed = (uint8)speed;
        }

        for (auto* modelId = models; *modelId; modelId++) {
            CStreaming::SetModelIsDeletable(*modelId);
            CStreaming::SetModelTxdIsDeletable(*modelId);
        }
        GenTrain_Status = 0;
    }
}

// 0x6F8170
void CTrain::AddNearbyPedAsRandomPassenger() {
    auto* const player = FindPlayerPed();
    if (player->m_pVehicle == this) {
        return;
    }

    // Don't add anyone if the player is about to enter this train
    if (auto* const task = player->GetTaskManager().GetTaskPrimary(TASK_PRIMARY_PRIMARY)) {
        const auto type = task->GetTaskType();
        if ((type == TASK_COMPLEX_ENTER_CAR_AS_DRIVER || type == TASK_COMPLEX_ENTER_CAR_AS_PASSENGER)
            && static_cast<CTaskComplexEnterCar*>(task)->GetTargetCar() == this
        ) {
            return;
        }
    }

    const bool isClockwise = trainFlags.bClockwiseDirection;

    // Done adding passengers
    if (m_nNumPassengersToLeave == m_nNumPassengersToEnter) {
        return;
    }

    if (m_pTemporaryPassenger) {
        auto* const tmpPed = m_pTemporaryPassenger;
        const bool isTmpPassenger = tmpPed->bJustGotOffTrain; // Ped flag dword at +0x478, mask 0x40000
        if (!isTmpPassenger && !tmpPed->bInVehicle) {
            return;
        }
        if (isTmpPassenger && tmpPed->bInVehicle) {
            return;
        }
        if (tmpPed->GetIntelligence()->FindTaskByType(TASK_COMPLEX_LEAVE_CAR_AND_WANDER)) {
            return;
        }
        CEntity::SafeCleanUpRef(m_pTemporaryPassenger);
        if (!isTmpPassenger && tmpPed->bInVehicle && tmpPed->m_pVehicle == this) {
            CPopulation::RemovePed(tmpPed);
        }
        m_pTemporaryPassenger = nullptr;
    }

    // Find the closest ped that's on the side of the train where the doors are
    const auto& myPos = GetPosition();
    const auto& right = GetRight();
    const float planeD = -(myPos.z * right.z + myPos.y * right.y + myPos.x * right.x);

    CPed* nearestPed = nullptr;
    float nearestDist = 999999.0f;

    auto* const pedPool = GetPedPool();
    if (pedPool->GetSize() == 0) {
        return;
    }
    for (auto i = pedPool->GetSize(); i-- > 0;) {
        auto* const ped = pedPool->GetAt(i);
        if (!ped) {
            continue;
        }
        if (ped->GetCreatedBy() != PED_GAME
            || CPedGroups::GetPedsGroup(ped)
            || ped->GetPlayerData()
            || ped->m_nPedType == PED_TYPE_COP
            || ped->bInVehicle
            || ped->bJustGotOffTrain // Ped flag dword at +0x478, mask 0x40000
            || ped->m_nPedState == PEDSTATE_DIE
            || ped->m_nPedState == PEDSTATE_DEAD
            || ped->m_nPedState == PEDSTATE_DIE_BY_STEALTH
            || ped->GetIntelligence()->FindTaskByType(TASK_COMPLEX_ENTER_CAR_AS_PASSENGER)
        ) {
            continue;
        }

        const auto& pedPos = ped->GetPosition();
        CVector diff = pedPos - myPos;
        diff.z = 0.0f;
        const float dist = diff.Magnitude();
        if (!(dist <= 625.0f)) {
            continue;
        }

        const float side = DotProduct(pedPos, right) + planeD;
        const bool isOnCorrectSide = isClockwise
            ? side >= 0.0f
            : side <= 0.0f;
        if (isOnCorrectSide && dist < nearestDist) {
            nearestDist = dist;
            nearestPed = ped;
        }
    }
    if (!nearestPed) {
        return;
    }

    // Calculate the position the ped should go to (in the local space of the train)
    CVector doorPos;
    int32   doorId;
    CCarEnterExit::GetNearestCarDoor(nearestPed, this, doorPos, doorId);

    CMatrix invMat;
    Invert(GetMatrix(), invMat);
    CVector localPos = invMat.TransformPoint(doorPos);
    // BUG: A direction in world space is applied onto a position in the local space
    if (isClockwise) {
        localPos += GetRight();
    } else {
        localPos -= GetRight();
    }

    auto* const seekTask = new CTaskComplexSeekEntityXYOffset{
        this,
        50'000,
        1000,
        1.0f,
        2.0f,
        2.0f,
        true,
        true,
        CEntitySeekPosCalculatorXYOffset{ localPos }
    };
    seekTask->SetMoveState(PEDMOVE_SPRINT);

    auto* const sequence = new CTaskComplexSequence{};
    sequence->AddTask(seekTask);
    if (isClockwise) {
        sequence->AddTask(new CTaskComplexEnterCarAsPassenger{ this, 0, false });
    } else {
        sequence->AddTask(new CTaskComplexEnterCarAsDriver{ this });
    }
    nearestPed->GetTaskManager().SetTask(sequence, TASK_PRIMARY_PRIMARY);

    m_nNumPassengersToLeave = m_nNumPassengersToLeave + 1;
    m_pTemporaryPassenger = nearestPed;
    CEntity::RegisterReference(m_pTemporaryPassenger);
}

// 0x6F86A0
void CTrain::ProcessControl() {
    vehicleFlags.bWarnedPeds = 0;
    m_vehicleAudio.Service();
    if (gbModelViewer) {
        return;
    }

    CVector vecOldTrainPosition = GetPosition();
    float fOldTrainHeading = GetHeading();

    const float& fTotalTrackLength = arrTotalTrackLength[m_nTrackId];
    CTrainNode* trainNodes = pTrackNodes[m_nTrackId];
    auto numTrackNodes = NumTrackNodes[m_nTrackId];

    if (trainFlags.bNotOnARailRoad == 0) {
        if (!trainFlags.bIsFrontCarriage) {
            if (m_pPrevCarriage) {
                m_fTrainSpeed = m_pPrevCarriage->m_fTrainSpeed;
                m_fCurrentRailDistance = m_pPrevCarriage->m_fCurrentRailDistance + m_fLength;
            } else {
                m_fTrainSpeed *= std::pow(0.9900000095367432f, CTimer::GetTimeStep());
                m_fCurrentRailDistance += m_fTrainSpeed * CTimer::GetTimeStep();
            }

            if (trainFlags.b01 && trainFlags.bStoppedAtStation && m_nModelIndex == MODEL_STREAKC && !trainFlags.bMissionTrain) {
                CPlayerPed* localPlayer = FindPlayerPed();
                if (m_nPassengersGenerationState == TRAIN_PASSENGERS_QUERY_NUM_PASSENGERS_TO_LEAVE) {
                    if (localPlayer->m_pVehicle == this) {
                        m_nNumPassengersToLeave = 0;
                    } else {
                        m_nNumPassengersToLeave = (CGeneral::GetRandomNumber() & 3) + 1; // [1, 4]
                    }
                    m_nPassengersGenerationState = TRAIN_PASSENGERS_TELL_PASSENGERS_TO_LEAVE;
                }

                if (m_nPassengersGenerationState == TRAIN_PASSENGERS_TELL_PASSENGERS_TO_LEAVE) {
                    RemoveRandomPassenger();
                    if (m_nNumPassengersToLeave == 0) {
                        m_nPassengersGenerationState = TRAIN_PASSENGERS_QUERY_NUM_PASSENGERS_TO_ENTER;
                    }
                }

                if (m_nPassengersGenerationState == TRAIN_PASSENGERS_QUERY_NUM_PASSENGERS_TO_ENTER) {
                    if (localPlayer->m_pVehicle == this) {
                        m_nNumPassengersToEnter = 0;
                    } else {
                        m_nNumPassengersToEnter = CGeneral::GetRandomNumber() % 4 + 1; // rand(1, 4)
                    }
                    m_nPassengersGenerationState = TRAIN_PASSENGERS_TELL_PASSENGERS_TO_ENTER;
                }

                if (m_nPassengersGenerationState == TRAIN_PASSENGERS_TELL_PASSENGERS_TO_ENTER) {
                    if (trainFlags.bPassengersCanEnterAndLeave) {
                        AddNearbyPedAsRandomPassenger();
                        if (m_nNumPassengersToLeave == m_nNumPassengersToEnter) {
                            m_nPassengersGenerationState = TRAIN_PASSENGERS_GENERATION_FINISHED;
                        }
                    }
                }
            }
        } else {
            CPad* pad = CPad::GetPad();
            if (m_pDriver && m_pDriver->IsPlayer()) {
                pad = m_pDriver->AsPlayer()->GetPadFromPlayer();
            }

            uint32 numCarriagesPulled = FindNumCarriagesPulled();
            if (!trainFlags.bClockwiseDirection) {
                m_fTrainSpeed = -m_fTrainSpeed;
            }

            if (GetStatus()) {
                bool bIsStreakModel = trainFlags.bIsStreakModel;
                auto fStopAtStationSpeed = static_cast<float>(m_autoPilot.m_nCruiseSpeed);

                uint32 timeAtStation = CTimer::GetTimeInMS() - m_nTimeWhenStoppedAtStation;
                if (timeAtStation >= (bIsStreakModel ? 20'000u : 10'000u)) {
                    if (timeAtStation >= (bIsStreakModel ? 28'000u : 18'000u)) {
                        if (timeAtStation >= (bIsStreakModel ? 32'000u : 22'000u)) {
                            if (trainFlags.bStopsAtStations) {
                                float maxTrainSpeed = 0.0f;
                                if (FindMaximumSpeedToStopAtStations(&maxTrainSpeed)) {
                                    fStopAtStationSpeed = 0.0f;
                                    m_nTimeWhenStoppedAtStation = CTimer::GetTimeInMS();
                                } else {
                                    if (fStopAtStationSpeed >= maxTrainSpeed) {
                                        fStopAtStationSpeed = maxTrainSpeed;
                                    }
                                }
                            }
                        } else if (trainFlags.bStoppedAtStation) {
                            CTrain* trainCarriage = this;
                            do {
                                trainFlags.bStoppedAtStation = false;
                                trainCarriage->m_nPassengersGenerationState = TRAIN_PASSENGERS_GENERATION_FINISHED;
                                trainCarriage = trainCarriage->m_pNextCarriage;
                            } while (trainCarriage);
                        }
                    } else {
                        fStopAtStationSpeed = 0.0f;
                        if (trainFlags.bStoppedAtStation) {
                            CTrain* trainCarriage = this;
                            do {
                                trainFlags.bPassengersCanEnterAndLeave = false;
                                trainCarriage->m_nPassengersGenerationState = TRAIN_PASSENGERS_GENERATION_FINISHED;
                                trainCarriage = trainCarriage->m_pNextCarriage;
                            } while (trainCarriage);
                        }
                    }
                } else {
                    fStopAtStationSpeed = 0.0f;
                    if (!trainFlags.bStoppedAtStation) {
                        CTrain* trainCarriage = this;
                        do {
                            trainFlags.bStoppedAtStation = true;
                            trainFlags.bPassengersCanEnterAndLeave = true;
                            trainCarriage->m_nPassengersGenerationState = TRAIN_PASSENGERS_QUERY_NUM_PASSENGERS_TO_LEAVE;
                            trainCarriage = trainCarriage->m_pNextCarriage;
                        } while (trainCarriage);
                    }
                }

                fStopAtStationSpeed = fStopAtStationSpeed * ExeRecip(50.0f) - m_fTrainSpeed;
                if (fStopAtStationSpeed > 0.0f) {
                    m_fTrainGas = fStopAtStationSpeed * 30.0f;
                    if (m_fTrainGas >= 1.0f) {
                        m_fTrainGas = 1.0f;
                    }

                    m_fTrainGas *= 255.0f;
                    m_fTrainBrake = 0.0f;
                } else {
                    float fTrainSpeed = fStopAtStationSpeed * -30.0f;
                    m_fTrainGas = 0.0f;
                    if (fTrainSpeed >= 1.0f) {
                        fTrainSpeed = 1.0f;
                    }
                    m_fTrainBrake = fTrainSpeed * 255.0f;
                }
            } else {
                float fTrainSpeed = m_fTrainSpeed;
                if (fTrainSpeed < 0.0f) {
                    fTrainSpeed = -fTrainSpeed;
                }

                if (fTrainSpeed < 0.001f) {
                    m_fTrainBrake = 0.0f;
                    m_fTrainGas = static_cast<float>(pad->GetAccelerate() - pad->GetBrake());
                } else {
                    if (m_fTrainSpeed > 0.0f) {
                        m_fTrainBrake = static_cast<float>(pad->GetBrake());
                        m_fTrainGas = static_cast<float>(pad->GetAccelerate());
                    } else {
                        m_fTrainGas = static_cast<float>(-pad->GetBrake());
                        m_fTrainBrake = static_cast<float>(pad->GetAccelerate());
                    }
                }
            }

            if (trainFlags.bForceSlowDown) {
                const CVector& vecPoint = GetPosition();
                CVector vecDistance{};
                if (CGameLogic::CalcDistanceToForbiddenTrainCrossing(vecPoint, m_vecMoveSpeed, true, vecDistance) < 230.0f) {
                    if (DotProduct(GetForwardVector(), vecDistance) <= 0.0f) {
                        m_fTrainGas = std::max(0.0f, m_fTrainGas);
                    } else {
                        m_fTrainGas = std::min(0.0f, m_fTrainGas);
                    }

                    if (CGameLogic::CalcDistanceToForbiddenTrainCrossing(vecPoint, m_vecMoveSpeed, false, vecDistance) < 230.0f) {
                        m_fTrainBrake = 512.0f;
                    }
                }
            }

            numCarriagesPulled += 3;

            m_fTrainSpeed += m_fTrainGas / 256.0f * CTimer::GetTimeStep() * 0.002f / float(numCarriagesPulled);

            if (m_fTrainBrake != 0.0f) {
                float fTrainSpeed = m_fTrainSpeed;
                if (m_fTrainSpeed < 0.0f) {
                    fTrainSpeed = -fTrainSpeed;
                }
                float fBreak = m_fTrainBrake / 256.0f * CTimer::GetTimeStep() * 0.006f / float(numCarriagesPulled);
                if (fTrainSpeed >= fBreak) {
                    if (m_fTrainSpeed < 0.0f) {
                        m_fTrainSpeed += fBreak;
                    } else {
                        m_fTrainSpeed -= fBreak;
                    }
                } else {
                    m_fTrainSpeed = 0.0f;
                }
            }

            m_fTrainSpeed *= pow(0.999750018119812f, CTimer::GetTimeStep());
            if (!trainFlags.bClockwiseDirection) {
                m_fTrainSpeed = -m_fTrainSpeed;
            }

            m_fCurrentRailDistance += CTimer::GetTimeStep() * m_fTrainSpeed;

            if (GetStatus() == STATUS_PLAYER) {

                float fTheTrainSpeed = m_fTrainSpeed;
                if (fTheTrainSpeed < 0.0f) {
                    fTheTrainSpeed = -fTheTrainSpeed;
                }
                if (fTheTrainSpeed > 1.0f * 0.95f) {
                    CPad::GetPad()->StartShake(300, 70, 0);
                    TheCamera.CamShake(0.1f, GetPosition());
                }

                fTheTrainSpeed = m_fTrainSpeed;
                if (fTheTrainSpeed < 0.0f) {
                    fTheTrainSpeed = -fTheTrainSpeed;
                }

                if (fTheTrainSpeed > 1.0f) {
                    int32 nNodeIndex = m_nNodeIndex;
                    int32 previousNodeIndex = nNodeIndex - 1;
                    if (previousNodeIndex < 0) {
                        previousNodeIndex = numTrackNodes;
                    }

                    int32 previousNodeIndex2 = previousNodeIndex - 1;
                    if (previousNodeIndex2 < 0) {
                        previousNodeIndex2 = numTrackNodes;
                    }

                    CTrainNode* pCurrentTrainNode = &trainNodes[m_nNodeIndex];
                    CTrainNode* pPreviousTrainNode = &trainNodes[previousNodeIndex];
                    CTrainNode* pPreviousTrainNode2 = &trainNodes[previousNodeIndex2];

                    CVector vecDifference1 = pCurrentTrainNode->GetPosn() - pPreviousTrainNode->GetPosn();
                    CVector vecDifference2 = pPreviousTrainNode->GetPosn() - pPreviousTrainNode2->GetPosn();
                    vecDifference1.Normalise();
                    vecDifference2.Normalise();

                    if (DotProduct(vecDifference1, vecDifference2) < 0.996f) {
                        CTrain* carriage = this;
                        bool bIsInTunnel = false;
                        while (!bIsInTunnel) {
                            bIsInTunnel = carriage->IsInTunnel();
                            carriage = carriage->m_pNextCarriage;
                            if (!carriage) {
                                if (!bIsInTunnel) {
                                    CTrain* theTrainCarriage = this;
                                    do {
                                        trainFlags.bNotOnARailRoad = true;
                                        theTrainCarriage->physicalFlags.bDisableCollisionForce = false;
                                        theTrainCarriage->physicalFlags.bDisableSimpleCollision = false;
                                        theTrainCarriage->SetIsStatic(false);
                                        theTrainCarriage = theTrainCarriage->m_pNextCarriage;
                                    } while (theTrainCarriage);

                                    CPhysical::ProcessControl();
                                }
                                break;
                            }
                        }
                    }
                }
            }
        }

        if (m_fCurrentRailDistance < 0.0f) {
            do {
                m_fCurrentRailDistance += fTotalTrackLength;
            } while (m_fCurrentRailDistance < 0.0f);
        }

        if (m_fCurrentRailDistance >= fTotalTrackLength) {
            do {
                m_fCurrentRailDistance -= fTotalTrackLength;
            } while (m_fCurrentRailDistance >= fTotalTrackLength);
        }

        float fNextNodeTrackLength = 0.0f;
        int32 nextNodeIndex = m_nNodeIndex + 1;
        if (nextNodeIndex < numTrackNodes) {
            CTrainNode* nextTrainNode = &trainNodes[nextNodeIndex];
            fNextNodeTrackLength = nextTrainNode->GetDistanceFromStart();
        } else {
            fNextNodeTrackLength = fTotalTrackLength;
            nextNodeIndex = 0;
        }

        CTrainNode* theTrainNode = &trainNodes[m_nNodeIndex];
        float fCurrentNodeTrackLength = theTrainNode->GetDistanceFromStart();
        while (m_fCurrentRailDistance < fCurrentNodeTrackLength || fNextNodeTrackLength < m_fCurrentRailDistance) {
            int32 newNodeIndex = m_nNodeIndex - 1; // previous node
            if (fCurrentNodeTrackLength <= m_fCurrentRailDistance) {
                newNodeIndex = m_nNodeIndex + 1; // next node
            }
            m_nNodeIndex = newNodeIndex % numTrackNodes;
            m_vehicleAudio.AddAudioEvent(AE_TRAIN_CLACK, 0.0f);

            theTrainNode = &trainNodes[m_nNodeIndex];
            fCurrentNodeTrackLength = theTrainNode->GetDistanceFromStart();

            nextNodeIndex = m_nNodeIndex + 1;
            if (nextNodeIndex < numTrackNodes) {
                CTrainNode* nextTrainNode = &trainNodes[nextNodeIndex];
                fNextNodeTrackLength = nextTrainNode->GetDistanceFromStart();
            } else {
                fNextNodeTrackLength = fTotalTrackLength;
                nextNodeIndex = 0;
            }
        }

        CTrainNode* nextTrainNode = &trainNodes[nextNodeIndex];
        fNextNodeTrackLength = nextTrainNode->GetDistanceFromStart();

        float fTrackNodeDifference = fNextNodeTrackLength - fCurrentNodeTrackLength;
        if (fTrackNodeDifference < 0.0f) {
            fTrackNodeDifference += fTotalTrackLength;
        }

        float fTheDistance = (m_fCurrentRailDistance - fCurrentNodeTrackLength) / fTrackNodeDifference;
        CVector vecPosition1 = theTrainNode->GetPosn() * (1.0f - fTheDistance) + nextTrainNode->GetPosn() * fTheDistance;

        CColModel* vehicleColModel = CModelInfo::GetModelInfo(m_nModelIndex)->GetColModel();
        const CBoundingBox& bbox = vehicleColModel->GetBoundingBox();
        float fTotalCurrentRailDistance = bbox.GetLength() + m_fCurrentRailDistance;
        if (fTotalCurrentRailDistance > fTotalTrackLength) {
            fTotalCurrentRailDistance -= fTotalTrackLength;
        }

        nextNodeIndex = m_nNodeIndex + 1;
        if (nextNodeIndex < numTrackNodes) {
            fNextNodeTrackLength = trainNodes[nextNodeIndex].GetDistanceFromStart();
        } else {
            fNextNodeTrackLength = fTotalTrackLength;
            nextNodeIndex = 0;
        }

        int32 trainNodeIndex = m_nNodeIndex;
        while (fTotalCurrentRailDistance < fCurrentNodeTrackLength || fTotalCurrentRailDistance > fNextNodeTrackLength) {
            trainNodeIndex = (trainNodeIndex + 1) % numTrackNodes;

            theTrainNode = &trainNodes[trainNodeIndex];
            fCurrentNodeTrackLength = theTrainNode->GetDistanceFromStart();

            nextNodeIndex = trainNodeIndex + 1;
            if (nextNodeIndex < numTrackNodes) {
                fNextNodeTrackLength = trainNodes[nextNodeIndex].GetDistanceFromStart();
            } else {
                fNextNodeTrackLength = fTotalTrackLength;
                nextNodeIndex = 0;
            }
        }

        fNextNodeTrackLength = trainNodes[nextNodeIndex].GetDistanceFromStart();

        fTrackNodeDifference = fNextNodeTrackLength - fCurrentNodeTrackLength;
        if (fTrackNodeDifference < 0.0f) {
            fTrackNodeDifference += fTotalTrackLength;
        }

        fTheDistance = (fTotalCurrentRailDistance - fCurrentNodeTrackLength) / fTrackNodeDifference;
        CVector vecPosition2 = theTrainNode->GetPosn() * (1.0f - fTheDistance) + trainNodes[nextNodeIndex].GetPosn() * fTheDistance;

        {
            CVector& vecVehiclePosition = GetPosition();
            vecVehiclePosition = (vecPosition1 + vecPosition2) / 2.0f;
            vecVehiclePosition.z += m_pHandlingData->m_fSuspensionLowerLimit - bbox.m_vecMin.z;
        }

        GetForward() = vecPosition2 - vecPosition1;
        GetForward().Normalise();
        if (!trainFlags.bClockwiseDirection) {
            GetForward() *= -1.0f;
        }

        CVector vecTemp(0.0f, 0.0f, 1.0f);
        CrossProduct(&GetRight(), &GetForward(), &vecTemp);
        GetRight().Normalise();
        CrossProduct(&GetUp(), &GetRight(), &GetForward());

        auto fTrainNodeLighting     = theTrainNode->GetLightingFromCollision().GetCurrentLighting();
        auto fTrainNextNodeLighting = trainNodes[nextNodeIndex].GetLightingFromCollision().GetCurrentLighting();

        fTrainNodeLighting += (fTrainNextNodeLighting - fTrainNodeLighting) * fTheDistance;
        m_fContactSurfaceBrightness = fTrainNodeLighting;
        m_vecMoveSpeed = (1.0f / CTimer::GetTimeStep()) * (GetPosition() - vecOldTrainPosition);

        float fNewTrainHeading = GetHeading();
        float fHeading = fNewTrainHeading - fOldTrainHeading;
        if (fHeading <= PI) {
            if (fHeading < -PI) {
                fHeading += TWO_PI;
            }
        } else {
            fHeading -= TWO_PI;
        }

        m_vecTurnSpeed = CVector(0.0f, 0.0f, fHeading / CTimer::GetTimeStep());

        if (trainFlags.bNotOnARailRoad) {
            m_vecMoveSpeed *= -1.0f;
            m_vecTurnSpeed *= -1.0f;

            ApplyMoveSpeed();

            m_vecMoveSpeed *= -1.0f;
            m_vecTurnSpeed *= -1.0f;

            CPhysical::ProcessControl();
        } else {
            m_vecMoveSpeed.x = std::clamp(m_vecMoveSpeed.x, -2.0f, 2.0f);
            m_vecMoveSpeed.y = std::clamp(m_vecMoveSpeed.y, -2.0f, 2.0f);
            m_vecMoveSpeed.z = std::clamp(m_vecMoveSpeed.z, -2.0f, 2.0f);

            m_vecTurnSpeed.x = std::clamp(m_vecTurnSpeed.x, -0.1f, 0.1f);
            m_vecTurnSpeed.y = std::clamp(m_vecTurnSpeed.y, -0.1f, 0.1f);
            m_vecTurnSpeed.z = std::clamp(m_vecTurnSpeed.z, -0.1f, 0.1f);
        }

        UpdateRwMatrix();
        UpdateRwFrame();
        RemoveAndAdd();

        SetIsStuck(false);
        SetWasPostponed(false);
        SetIsInSafePosition(true);

        m_fMovingSpeed = DistanceBetweenPoints(GetPosition(), vecOldTrainPosition);

        if (trainFlags.bIsFrontCarriage || trainFlags.bIsLastCarriage) {
            CVector vecPoint = bbox.m_vecMax.y * GetForward();
            vecPoint += GetPosition();
            vecPoint += CTimer::GetTimeStep() * m_vecMoveSpeed;

            MarkSurroundingEntitiesForCollisionWithTrain(vecPoint, 3.0f, this, false);
        }

        if (!vehicleFlags.bWarnedPeds) {
            CCarCtrl::ScanForPedDanger(this);
        }
        return;
    } else {
        if (!GetIsStuck()) {
            float fMaxForce = 0.003f;
            float fMaxTorque = 0.0009f;
            float fMaxMovingSpeed = 0.005f;

            if (GetStatus() != STATUS_PLAYER) {
                fMaxForce = 0.006f;
                fMaxTorque = 0.0015f;
                fMaxMovingSpeed = 0.015f;
            }

            float fMaxForceTimeStep  = (fMaxForce  * CTimer::GetTimeStep()) * (fMaxForce * CTimer::GetTimeStep());
            float fMaxTorqueTimeStep = (fMaxTorque * CTimer::GetTimeStep()) * (fMaxTorque * CTimer::GetTimeStep());

            m_vecForce  = (m_vecForce  + m_vecMoveSpeed) / 2.0f;
            m_vecTorque = (m_vecTorque + m_vecTurnSpeed) / 2.0f;

            if (m_vecForce.SquaredMagnitude() > fMaxForceTimeStep ||
                m_vecTorque.SquaredMagnitude() > fMaxTorqueTimeStep ||
                m_fMovingSpeed >= fMaxMovingSpeed ||
                m_fDamageIntensity > 0.0f && m_pDamageEntity != nullptr && m_pDamageEntity->GetIsTypePed()
            ) {
                m_nFakePhysics = 0;
            } else {
                m_nFakePhysics += 1;
                if (m_nFakePhysics > 10 /*&& !plugin::Call<0x424100>()*/) {
                    // if (m_nFakePhysics > 10) { // OG redundant check
                        m_nFakePhysics = 10;
                    // }

                    ResetMoveSpeed();
                    ResetTurnSpeed();
                    SkipPhysics();
                    return;
                }
            }
        }

        CPhysical::ProcessControl();

        CVector vecMoveForce{}, vecTurnForce{};
        if (mod_Buoyancy.ProcessBuoyancy(this, m_fBuoyancyConstant, &vecMoveForce, &vecTurnForce)) {
            physicalFlags.bTouchingWater = true;

            float fTimeStep = 0.01f;
            if (CTimer::GetTimeStep() >= 0.01f) {
                fTimeStep = CTimer::GetTimeStep();
            }

            float fSpeedFactor = 1.0f - vecOldTrainPosition.z / (fTimeStep * m_fMass * 0.008f) * 0.05f;
            fSpeedFactor = std::pow(fSpeedFactor, CTimer::GetTimeStep());

            m_vecMoveSpeed *= fSpeedFactor;
            m_vecTurnSpeed *= fSpeedFactor;
            ApplyMoveForce(vecOldTrainPosition.x, vecOldTrainPosition.y, vecOldTrainPosition.z);
            ApplyTurnForce(vecTurnForce, vecMoveForce);
        }
    }
}


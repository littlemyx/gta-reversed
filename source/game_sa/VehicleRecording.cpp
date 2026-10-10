#include "StdInc.h"
#include "VehicleRecording.h"
#include "Entity/Vehicle/Bike.h"
#include "Fx/FxFtol.h"

#ifdef EXTRA_CARREC_LOGS
    #define CARREC_LOG(...) NOTSA_LOG_DEBUG(__VA_ARGS__)
#else
    #define CARREC_LOG(...)
#endif

// VehicleRecording naming convention:
//
// `StreamingArray` contains 'recordings' that are typed CPath.
// It's indices are named `recordId`
// 
// CPath contains a variable-sized frame array. (`CVehicleStateEachFrame`)
// It's 'number' is taken from the loaded file hence named `fileNumber`
// 
// `pPlaybackBuffer` contains 16 'recordings' that can be played
// simultaneously.
// It's incides are named `playbackId`

// 0x459390
void CVehicleRecording::Init() {
    ZoneScoped;

    rng::fill(bPlaybackGoingOn, false);
    rng::fill(bPlaybackPaused, false);
    rng::fill(pPlaybackBuffer, nullptr);
    rng::fill(pVehicleForPlayback, nullptr);

    for (auto& recording : StreamingArray) {
        recording.m_pData = nullptr;
        recording.m_nRefCount = 0;
    }
}

// 0x45A1B0
void CVehicleRecording::InitAtStartOfGame() {
    for (auto& recording : StreamingArray) {
        recording.m_pData = nullptr;
    }
    Init();
}

// 0x459400
void CVehicleRecording::ShutDown() {
    rng::for_each(StreamingArray, &CPath::Remove);
}

// 0x459F70 hook not needed
void CVehicleRecording::Render() {
}

// 0x45A360
void CVehicleRecording::ChangeCarPlaybackToUseAI(CVehicle* vehicle) {
    if (const auto i = FindVehicleRecordingIndex(vehicle); i != -1) {
        bUseCarAI[i] = true;

        vehicle->m_autoPilot.SetCarMission(MISSION_FOLLOW_RECORDED_PATH);
        SetRecordingToPointClosestToCoors(i, vehicle->GetPosition());
        vehicle->physicalFlags.bDisableCollisionForce = false;
        vehicle->ProcessControlCollisionCheck(false);
        vehicle->SetStatus(STATUS_PHYSICS);
    }
}

// inlined
// 0x459FF0
uint32 CVehicleRecording::FindIndexWithFileNameNumber(int32 fileNumber) {
    for (auto&& [i, recording] : rngv::enumerate(GetRecordings())) {
        if (recording.m_nNumber == fileNumber) {
            return i;
        }
    }
    return 0;
}

// 0x459B30
void CVehicleRecording::InterpolateInfoForCar(CVehicle* vehicle, const CVehicleStateEachFrame& frame, float interpValue) {
    CMatrix transition;
    RestoreInfoForMatrix(transition, frame);

    vehicle->GetMatrix() = Lerp(vehicle->GetMatrix(), transition, interpValue);
    vehicle->GetMoveSpeed() = Lerp(vehicle->GetMoveSpeed(), frame.m_sVelocity, interpValue);
}

// 0x45A060
bool CVehicleRecording::HasRecordingFileBeenLoaded(int32 fileNumber) {
    const auto recording = FindRecording(fileNumber);
    return recording && recording->m_pData;
}

// 0x45A8F0
void CVehicleRecording::Load(RwStream* stream, int32 recordId, int32 totalSize) {
    const auto allocated = CMemoryMgr::Malloc(totalSize);
    StreamingArray[recordId].m_pData = static_cast<CVehicleStateEachFrame*>(allocated);
    const auto size = RwStreamRead(stream, allocated, 9'999'999u);
    StreamingArray[recordId].m_nSize = size;
    RwStreamClose(stream, nullptr);

    CARREC_LOG("Load carrec to streaming slot idx:{} (size={})", recordId, totalSize);

    for (auto&& [i, frame] : rngv::enumerate(StreamingArray[recordId].GetFrames())) {
        if (i != 0 && frame.m_nTime == 0) {
            // no valid frame that is not zeroth can have zero as a time.
            // so we count them as invalid and prune the following including itself.
            CARREC_LOG("\tRecording pruned at index {}", i);

            StreamingArray[recordId].m_nSize = i * sizeof(CVehicleStateEachFrame);
            break;
        }
    }
    SmoothRecording(recordId);
}

// 0x45A0F0
void CVehicleRecording::SmoothRecording(int32 recordId) {
    // 0x45A0F0: smooths the time of frame j - 1 to the mean of the times of frames j - 2 and j, for j = 2 .. n - 1 (the port started at j = 4).
    // The sum is added as an int (wraps), converted via fild (+ 2^32 for a "negative" sum), halved on the x87 stack (24-bit mantissa) and truncated by _ftol2.
    auto& path = StreamingArray[recordId];
    const uint32 limit = (uint32)path.m_nSize - 0x20u;
    if (limit <= 0x20u) {
        return;
    }
    auto* const frames = path.m_pData;
    for (size_t j = 2; j * sizeof(CVehicleStateEachFrame) - 0x20u < limit; j++) {
        const uint32 sum = frames[j - 2].m_nTime + frames[j].m_nTime;
        double       v   = (double)(int32)sum;
        if ((int32)sum < 0) {
            v += 4294967296.0; // 0x858C54
        }
        frames[j - 1].m_nTime = (uint32)notsa::detail::Ftol(v * 0.5);
    }
}

// 0x459F80
int32 CVehicleRecording::RegisterRecordingFile(const char* name) {
    auto fileNumber = 850;
    if (sscanf_s(name, "carrec%d", &fileNumber) == 0) {
        VERIFY(sscanf_s(name, "CARREC%d", &fileNumber) == 1);
    }

    CARREC_LOG("Registering carrec file '{}', (streamIdx={})", name, NumPlayBackFiles);

    StreamingArray[NumPlayBackFiles].m_nNumber = fileNumber;
    StreamingArray[NumPlayBackFiles].m_pData = nullptr;
    return NumPlayBackFiles++;
}

// 0x45A0A0
void CVehicleRecording::RemoveRecordingFile(int32 fileNumber) {
    if (const auto recording = FindRecording(fileNumber)) {
        if (recording->m_pData && !recording->m_nRefCount) {
            recording->Remove();
        }
    }
}

// 0x45A020
void CVehicleRecording::RequestRecordingFile(int32 fileNumber) {
    if (const auto rec = FindRecording(fileNumber)) {
        CStreaming::RequestModel(RRRToModelId(rec->GetIndex()), STREAMING_KEEP_IN_MEMORY | STREAMING_MISSION_REQUIRED);
        rec->Remove();
    }
}

// 0x459440
void CVehicleRecording::StopPlaybackWithIndex(int32 playbackId) {
    if (auto vehicle = pVehicleForPlayback[playbackId]) {
        vehicle->m_autoPilot.m_vehicleRecordingId = -1;
        pVehicleForPlayback[playbackId]->physicalFlags.bDisableCollisionForce = false;
    }
    pVehicleForPlayback[playbackId] = nullptr;
    pPlaybackBuffer[playbackId] = nullptr;
    PlaybackBufferSize[playbackId] = 0;
    bPlaybackGoingOn[playbackId] = false;

    StreamingArray[PlayBackStreamingIndex[playbackId]].RemoveRef();
}

// 0x45A980
void CVehicleRecording::StartPlaybackRecordedCar(CVehicle* vehicle, int32 fileNumber, bool useCarAI, bool looped) {
    const auto GetInactivePlaybackIndices = [] {
        return rng::views::iota(0, TOTAL_VEHICLE_RECORDS) | std::views::filter([](auto&& i) { return !bPlaybackGoingOn[i]; });
    };
    const auto recordId = FindIndexWithFileNameNumber(fileNumber);
    const auto playbackId = *GetInactivePlaybackIndices().begin();

    pVehicleForPlayback[playbackId] = vehicle;
    CEntity::RegisterReference(pVehicleForPlayback[playbackId]);
    bPlaybackLooped[playbackId] = looped;
    PlayBackStreamingIndex[playbackId] = recordId;
    pPlaybackBuffer[playbackId] = StreamingArray[recordId].m_pData;
    PlaybackBufferSize[playbackId] = StreamingArray[recordId].m_nSize;
    bUseCarAI[playbackId] = useCarAI;
    PlaybackIndex[playbackId] = 0;
    PlaybackRunningTime[playbackId] = 0.0f;
    PlaybackSpeed[playbackId] = 1.0f;
    bPlaybackGoingOn[playbackId] = true;
    bPlaybackPaused[playbackId] = false;
    StreamingArray[recordId].AddRef();
    
    if (useCarAI) {
        vehicle->m_autoPilot.SetCarMission(MISSION_FOLLOW_RECORDED_PATH);
        SetRecordingToPointClosestToCoors(playbackId, vehicle->GetPosition());
    } else {
        vehicle->physicalFlags.bDisableCollisionForce = true;
        vehicle->physicalFlags.bCollidable = false;
    }
    vehicle->m_autoPilot.m_vehicleRecordingId = playbackId;
}

// 0x45A280
void CVehicleRecording::StopPlaybackRecordedCar(CVehicle* vehicle) {
    if (const auto i = FindVehicleRecordingIndex(vehicle); i != -1) {
        StopPlaybackWithIndex(i);
    }
}

// 0x459740
void CVehicleRecording::PausePlaybackRecordedCar(CVehicle* vehicle) {
    if (const auto i = FindVehicleRecordingIndex(vehicle); i != -1) {
        bPlaybackPaused[i] = true;
    }
}

// 0x459850
void CVehicleRecording::UnpausePlaybackRecordedCar(CVehicle* vehicle) {
    if (const auto i = FindVehicleRecordingIndex(vehicle); i != -1) {
        bPlaybackPaused[i] = false;
    }
}

// 0x459660
void CVehicleRecording::SetPlaybackSpeed(CVehicle* vehicle, float speed) {
    if (const auto i = FindVehicleRecordingIndex(vehicle); i != -1) {
        PlaybackSpeed[i] = speed;
    }
}

// [debug]
// 0x459F00
void CVehicleRecording::RenderLineSegment(int32& numVertices) {
    if (numVertices > 1) {
        for (int32 i = 0; i < numVertices - 1; i++) {
            aTempBufferIndices[2 * i] = i;
            aTempBufferIndices[2 * i + 1] = i + 1;
        }
        if (RwIm3DTransform(TempBufferVertices.m_3d, numVertices, nullptr, 0)) {
            RwIm3DRenderIndexedPrimitive(rwPRIMTYPELINELIST, aTempBufferIndices, (numVertices - 1) * 2);
            RwIm3DEnd();
        }
    }
    numVertices = 0;
}

// 0x45A160
void CVehicleRecording::RemoveAllRecordingsThatArentUsed() {
    for (auto&& [i, recording] : rngv::enumerate(GetRecordings())) {
        if (recording.m_nNumber == i && recording.m_pData && !recording.m_nRefCount) {
            recording.Remove();
        }
    }
}

// 0x459A30
void CVehicleRecording::RestoreInfoForCar(CVehicle* vehicle, const CVehicleStateEachFrame& frame, bool pause) {
    // 0x459A30: the recorded bytes are SIGNED (movsx) and converted with float constants: 1/16383.5 (0x858EAC), 0.05 (0x858C28 steering), 0.01 (0x858C58 pedals).
    // The recorded velocity is restored, too; the pause only clears gas / brake / speed / handbrake (the steering angle is kept).
    const auto* const raw = reinterpret_cast<const uint8*>(&frame);
    const auto Vel = [&](size_t off) { return (float)*reinterpret_cast<const int16*>(raw + off) * std::bit_cast<float>(0x38800100u); };

    RestoreInfoForMatrix(vehicle->GetMatrix(), frame);
    vehicle->m_vecMoveSpeed = CVector{ Vel(4), Vel(6), Vel(8) };
    vehicle->ResetTurnSpeed();
    vehicle->m_fSteerAngle = (float)(int8)raw[0x10] * 0.05f;
    vehicle->m_GasPedal    = (float)(int8)raw[0x11] * 0.01f;
    vehicle->m_BrakePedal  = (float)(int8)raw[0x12] * 0.01f;
    vehicle->vehicleFlags.bIsHandbrakeOn = frame.m_bHandbrakeUsed;

    if (pause) {
        vehicle->m_GasPedal   = 0.0f;
        vehicle->m_BrakePedal = 0.0f;
        vehicle->m_vecMoveSpeed = CVector{};
        vehicle->vehicleFlags.bIsHandbrakeOn = false;
    }

    if (vehicle->m_nVehicleType == VEHICLE_TYPE_BIKE) {
        static_assert(offsetof(CBike, nBikeFlags) == 0x614);
        static_cast<CBike*>(vehicle)->nBikeFlags &= 0xE7; // bGettingPickedUp, bOnSideStand
    }
}
void CVehicleRecording::RestoreInfoForMatrix(CMatrix& matrix, const CVehicleStateEachFrame& frame) {
    // 0x459960: signed bytes * the float constant 0x859BCC (= 0.0078740157f); `FixedFloat` divides by 127 (different last bit)
    const auto* const comp = reinterpret_cast<const int8*>(&frame) + 0xA; // m_bRight (3), m_bTop (3)
    const auto Dec = [](int8 v) { return (float)v * std::bit_cast<float>(0x3C010204u); };
    matrix.GetRight()    = CVector{ Dec(comp[0]), Dec(comp[1]), Dec(comp[2]) };
    matrix.GetForward()  = CVector{ Dec(comp[3]), Dec(comp[4]), Dec(comp[5]) };
    matrix.GetUp()       = CrossProduct(matrix.GetRight(), matrix.GetForward());
    matrix.GetPosition() = frame.m_vecPosn;
}

// 0x45A610
void CVehicleRecording::SaveOrRetrieveDataForThisFrame() {
    if (CReplay::Mode == MODE_PLAYBACK)
        return;

    for (const auto i : GetActivePlaybackIndices()) {
        auto vehicle = pVehicleForPlayback[i];

        if (!vehicle || vehicle->physicalFlags.bRenderScorched) {
            StopPlaybackWithIndex(i);
            continue;
        }
        if (bUseCarAI[i])
            continue;

        const auto delta = static_cast<float>(CTimer::GetTimeInMS() - CTimer::m_snPPPPreviousTimeInMilliseconds);
        const auto step = delta * PlaybackSpeed[i] / 4.0f;
        if (step > 500.0f) {
            NOTSA_LOG_DEBUG("That's a really big step (={:2f})\n", step);
        }
        PlaybackRunningTime[i] += step;

        const auto frames = GetFramesFromPlaybackBuffer(i);
        auto current = GetCurrentFrameIndex(i);

        // find the exact frame that matches the playback time.
        for (auto& next = frames[current + 1]; current + 1 < frames.size() && next.m_nTime < PlaybackRunningTime[i]; current++)
            ;
        // current can not be back from the current frame index.
        for (; current > GetCurrentFrameIndex(i) && frames[current].m_nTime > PlaybackRunningTime[i]; current--)
            ;
        PlaybackIndex[i] = current * sizeof(CVehicleStateEachFrame);

        if (current + 1 < frames.size()) {
            // current is not the last frame, so we interpolate with the next.
            const auto& frameCurrent = frames[current];
            const auto& frameNext = frames[current + 1];

            RestoreInfoForCar(vehicle, frameCurrent, false);

            const auto interp = (PlaybackRunningTime[i] - (float)frameCurrent.m_nTime) / (float)(frameNext.m_nTime - frameCurrent.m_nTime);
            InterpolateInfoForCar(vehicle, frameNext, interp);

            if (vehicle->IsSubTrain()) {
                vehicle->AsTrain()->FindPositionOnTrackFromCoors();
            }

            vehicle->ProcessControlCollisionCheck(false);
            vehicle->RemoveAndAdd();
            vehicle->UpdateRwMatrix();
            vehicle->UpdateRwFrame();

            MarkSurroundingEntitiesForCollisionWithTrain(vehicle->GetPosition(), 5.0f, vehicle, true);
        } else if (bPlaybackLooped[i]) {
            // current is the last frame, set next frame to be processed to first frame cuz we're looping.
            PlaybackRunningTime[i] = 0.0f;
            PlaybackIndex[i] = 0;
        } else {
            // current is the last frame, farewell.
            StopPlaybackRecordedCar(vehicle);
        }
    }
}

// 0x45A1E0
void CVehicleRecording::SetRecordingToPointClosestToCoors(int32 playbackId, CVector posn) {
    auto minDist = 1'000'000.0f; // FLT_MAX
    for (auto&& [i, frame] : rngv::enumerate(GetFramesFromPlaybackBuffer(playbackId))) {
        if (const auto d = DistanceBetweenPoints(frame.m_vecPosn, posn); d < minDist) {
            PlaybackIndex[playbackId] = i;
            minDist = d;
        }
    }
}

// 0x4594C0
bool CVehicleRecording::IsPlaybackGoingOnForCar(CVehicle* vehicle) {
    return FindVehicleRecordingIndex(vehicle) != -1;
}

// 0x4595A0
bool CVehicleRecording::IsPlaybackPausedForCar(CVehicle* vehicle) {
    // SA code loops through all playbacks but always returns false.
    return false;
}

// unused
// 0x459D10
void CVehicleRecording::SkipForwardInRecording(CVehicle* vehicle, float distance) {
    // Not tested as it's unused.

    // NOTSA: Original code does OOB-access if no index found.
    if (const auto i = FindVehicleRecordingIndex(vehicle); i != -1) {
        const auto frames = GetFramesFromPlaybackBuffer(i);
        auto index = GetCurrentFrameIndex(i);
        auto pace = 0.0f;

        for (; pace < distance && index + 1 < frames.size(); index++) {
            pace += DistanceBetweenPoints2D(frames[index].m_vecPosn, frames[index + 1].m_vecPosn);
        }

        // if we overshoot, we try to get the closest but smaller than or equal the `distance` pace.
        for (; pace > distance && index > 1; index--) {
            pace -= DistanceBetweenPoints2D(frames[index].m_vecPosn, frames[index - 1].m_vecPosn);
        }

        PlaybackRunningTime[i] = static_cast<float>(frames[index].m_nTime);
        if (const auto usesAI = bUseCarAI[i]) {
            RestoreInfoForCar(vehicle, frames[index], false);
            vehicle->ProcessControlCollisionCheck(false);
        }
    }
}

// unused
// 0x45A4A0
void CVehicleRecording::SkipToEndAndStopPlaybackRecordedCar(CVehicle* vehicle) {
    if (const auto i = FindVehicleRecordingIndex(vehicle); i != -1) {
        assert(!GetFramesFromPlaybackBuffer(i).empty());

        vehicle->physicalFlags.bCollidable = false;
        RestoreInfoForCar(vehicle, GetFramesFromPlaybackBuffer(i).back(), false);
        vehicle->ProcessControlCollisionCheck(false);
        pVehicleForPlayback[i] = nullptr;
        pPlaybackBuffer[i] = nullptr;
        PlaybackBufferSize[i] = 0;
        bPlaybackGoingOn[i] = false;
        vehicle->m_autoPilot.m_vehicleRecordingId = -1;

        StreamingArray[PlayBackStreamingIndex[i]].RemoveRef();
    }
}

void CVehicleRecording::InjectHooks() {
    RH_ScopedClass(CVehicleRecording);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Init, 0x459390);
    RH_ScopedInstall(InitAtStartOfGame, 0x45A1B0);
    RH_ScopedInstall(ShutDown, 0x459400);
    RH_ScopedInstall(Render, 0x459F70);
    RH_ScopedInstall(ChangeCarPlaybackToUseAI, 0x45A360);
    RH_ScopedInstall(FindIndexWithFileNameNumber, 0x459FF0);
    RH_ScopedInstall(InterpolateInfoForCar, 0x459B30);
    RH_ScopedInstall(HasRecordingFileBeenLoaded, 0x45A060);
    RH_ScopedInstall(Load, 0x45A8F0);
    RH_ScopedInstall(SmoothRecording, 0x45A0F0);
    RH_ScopedInstall(RegisterRecordingFile, 0x459F80);
    RH_ScopedInstall(RemoveRecordingFile, 0x45A0A0);
    RH_ScopedInstall(RequestRecordingFile, 0x45A020);
    RH_ScopedInstall(StopPlaybackWithIndex, 0x459440);
    RH_ScopedInstall(StartPlaybackRecordedCar, 0x45A980);
    RH_ScopedInstall(StopPlaybackRecordedCar, 0x45A280);
    RH_ScopedInstall(PausePlaybackRecordedCar, 0x459740);
    RH_ScopedInstall(UnpausePlaybackRecordedCar, 0x459850);
    RH_ScopedInstall(SetPlaybackSpeed, 0x459660);
    RH_ScopedInstall(RenderLineSegment, 0x459F00);
    RH_ScopedInstall(RemoveAllRecordingsThatArentUsed, 0x45A160);
    RH_ScopedInstall(RestoreInfoForCar, 0x459A30);
    RH_ScopedInstall(RestoreInfoForMatrix, 0x459960);
    RH_ScopedInstall(SaveOrRetrieveDataForThisFrame, 0x45A610);
    RH_ScopedInstall(SetRecordingToPointClosestToCoors, 0x45A1E0);
    RH_ScopedInstall(IsPlaybackGoingOnForCar, 0x4594C0);
    RH_ScopedInstall(IsPlaybackPausedForCar, 0x4595A0);
    RH_ScopedInstall(SkipForwardInRecording, 0x459D10);
    RH_ScopedInstall(SkipToEndAndStopPlaybackRecordedCar, 0x45A4A0);
}

uint32 CPath::GetIndex() const {
    const auto index = this - CVehicleRecording::StreamingArray.data();
    assert(index >= 0 && static_cast<size_t>(index) < CVehicleRecording::StreamingArray.size());

    return index;
}

void CPath::AddRef() {
    CARREC_LOG("Ref added for path {} (number= {}, size= {}, ptr= {})", GetIndex(), m_nNumber, m_nSize, LOG_PTR(m_pData));
    m_nRefCount++;
}

void CPath::RemoveRef() {
    CARREC_LOG("Ref removed for path {} (number= {}, size= {}, ptr= {})", GetIndex(), m_nNumber, m_nSize, LOG_PTR(m_pData));
    if (!--m_nRefCount) {
        Remove();
    }
}

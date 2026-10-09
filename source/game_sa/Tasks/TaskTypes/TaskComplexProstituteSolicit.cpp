#include "StdInc.h"

#include "TaskComplexProstituteSolicit.h"
#include "Ragdoll/IKChainManager.h"
#include "TaskComplexLeaveCar.h"
#include "TaskComplexEnterCarAsPassenger.h"
#include "TaskSimpleStandStill.h"
#include "TaskComplexCarDrive.h"
#include "TaskComplexTurnToFaceEntityOrCoord.h"
#include "SeekEntity/TaskComplexSeekEntity.h"
#include "SeekEntity/PosCalculators/EntitySeekPosCalculatorXYOffset.h"
#include "CarEnterExit.h"
#include "Messages.h"
#include "Cheat.h"

void CTaskComplexProstituteSolicit::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexProstituteSolicit, 0x86FB88, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(CreateSubTask, 0x666360);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x6666A0);
    RH_ScopedVMTInstall(CreateNextSubTask, 0x666780);
    RH_ScopedVMTInstall(ControlSubTask, 0x6669D0);
}

// 0x59C890 - the original evaluation order; the sum stays in the FPU registers (extended precision), stored as float
static CVector TransformPointOriginal(const CMatrix& m, const CVector& v) {
    const auto &r = m.GetRight(), &f = m.GetForward(), &u = m.GetUp(), &p = m.GetPosition();
    return CVector{
        (float)((((double)u.x * v.z + (double)f.x * v.y) + (double)r.x * v.x) + p.x),
        (float)((((double)u.y * v.z + (double)r.y * v.x) + (double)f.y * v.y) + p.y),
        (float)((((double)u.z * v.z + (double)r.z * v.x) + (double)f.z * v.y) + p.z)
    };
}

// 0x661A60
CTaskComplexProstituteSolicit::CTaskComplexProstituteSolicit(CPed* client) : CTaskComplex() {
    m_nLastSavedTime = 0;
    m_nNextTimeToCheckForSecludedPlace = 0;
    m_nLastPaymentTime = 0;
    m_nCurrentTimer = 0;
    m_pClient = client;
    m_nVehicleMovementTimer = 850;
    b07 = true;
    b08 = true;
    b10 = true;
    m_pClient->RegisterReference(m_pClient);
}

// 0x661AF0
CTaskComplexProstituteSolicit::~CTaskComplexProstituteSolicit() {
    auto player = FindPlayerPed();
    if (!player)
        return;

    CEntity::ClearReference(player->GetPlayerData()->m_pCurrentProstitutePed);
    if (bMoveCameraDown) {
        bMoveCameraDown = false;
    }
}

// 0x661B80
bool CTaskComplexProstituteSolicit::MakeAbortable(CPed* ped, eAbortPriority priority, const CEvent* event) {
    bool aborted = m_pSubTask->MakeAbortable(ped, priority, event);
    if (aborted) {
        bMoveCameraDown = false;
    }
    return aborted;
}

// 0x661D30
void CTaskComplexProstituteSolicit::GetRidOfPlayerProstitute() {
    auto* prostitute = FindPlayerPed()->GetPlayerData()->m_pCurrentProstitutePed;
    if (!prostitute)
        return;

    const auto intel = prostitute->GetIntelligence();
    auto* task = intel->FindTaskByType(TASK_COMPLEX_PROSTITUTE_SOLICIT);
    if (!task)
        return;

    auto* t = static_cast<CTaskComplexProstituteSolicit*>(task);
    t->bTaskCanBeFinished = true;
    t->m_nCurrentTimer = 0;
}

// 0x666360
CTask* CTaskComplexProstituteSolicit::CreateSubTask(eTaskType taskType, CPed* prostitute) {
    switch (taskType) {
    case TASK_COMPLEX_CAR_DRIVE:
        bSearchingForSecludedPlace = true;
        return new CTaskComplexCarDrive(m_pClient->m_pVehicle);

    case TASK_SIMPLE_STAND_STILL:
        return new CTaskSimpleStandStill(5000, false, false, 8.0f);

    case TASK_COMPLEX_ENTER_CAR_AS_PASSENGER:
        return new CTaskComplexEnterCarAsPassenger(m_pClient->m_pVehicle, 8, false);

    case TASK_COMPLEX_LEAVE_CAR:
        return new CTaskComplexLeaveCar(m_pClient->m_pVehicle, 0, 0, true, false);

    case TASK_COMPLEX_SEEK_ENTITY: {
        auto* const veh = m_pClient->m_pVehicle;

        CMatrix invVehMat;
        Invert(*veh->m_matrix, invVehMat); // 0x59B920

        const auto door10 = CCarEnterExit::GetPositionToOpenCarDoor(veh, 10);
        const auto door8  = CCarEnterExit::GetPositionToOpenCarDoor(veh, 8);

        // Pick the door closer to the prostitute. The x87 code keeps everything in extended precision except
        // the x delta of the first door, which is stored as float.
        const auto& pos = prostitute->GetPosition();
        const double d10x = (float)((double)pos.x - (double)door10.x);
        const double d10y = (double)pos.y - (double)door10.y;
        const double d10z = (double)pos.z - (double)door10.z;
        const double d8x  = (double)pos.x - (double)door8.x;
        const double d8y  = (double)pos.y - (double)door8.y;
        const double d8z  = (double)pos.z - (double)door8.z;
        const double distSq10 = (d10x * d10x + d10y * d10y) + d10z * d10z;
        const double distSq8  = (d8x * d8x + d8y * d8y) + d8z * d8z;

        const auto offset = TransformPointOriginal(invVehMat, !(distSq8 > distSq10) ? door8 : door10); // 0x59C890

        return new CTaskComplexSeekEntity<CEntitySeekPosCalculatorXYOffset>{
            veh,
            50'000,
            1'000,
            1.0f, // 0x86FC2C
            2.0f, // 0x86FC28
            2.0f, // 0x86FC30
            false,
            false,
            CEntitySeekPosCalculatorXYOffset{ offset }
        };
    }

    case TASK_COMPLEX_TURN_TO_FACE_ENTITY:
        return new CTaskComplexTurnToFaceEntityOrCoord(m_pClient, 0.5f, 0.2f);

    default:
        return nullptr;
    }
}

// 0x661BB0
bool CTaskComplexProstituteSolicit::IsTaskValid(CPed* prostitute, CPed* ped) {
    if (FindPlayerPed() != ped)
        return false;

    if (!ped)
        return false;

    if (!ped->IsInVehicle())
        return false;

    if (ped->bIsBeingArrested)
        return false;

    if (ped->GetPlayerData()->m_pCurrentProstitutePed && ped->GetPlayerData()->m_pCurrentProstitutePed != prostitute)
        return false;

    if (ped->m_pVehicle->GetVehicleAppearance() != VEHICLE_APPEARANCE_AUTOMOBILE)
        return false;

    if (ped->m_pVehicle->IsUpsideDown())
        return false;

    if (ped->m_pVehicle->IsOnItsSide())
        return false;

    auto task = ped->GetTaskManager().GetSimplestActiveTask();
    if (task->GetTaskType() != TASK_SIMPLE_CAR_DRIVE)
        return false;

    if (ped->m_pVehicle->m_pDriver != ped)
        return false;

    if (prostitute->m_pVehicle) {
        if (prostitute->m_pVehicle != ped->m_pVehicle || prostitute->m_pVehicle->m_nNumPassengers != 1)
            return false;
    } else if (ped->m_pVehicle->m_nNumPassengers) {
        return false;
    }

    if (!ped->m_pVehicle->m_nMaxPassengers || ped->m_pVehicle->m_pHandlingData->m_bTandemSeats)
        return false;

    CVector out = ped->GetPosition() - prostitute->GetPosition();
    if (out.SquaredMagnitude() > 100.0f || CTheScripts::IsPlayerOnAMission() || CGameLogic::IsCoopGameGoingOn()) {
        return false;
    }
    return true;
}

// 0x6666A0
CTask* CTaskComplexProstituteSolicit::CreateFirstSubTask(CPed* ped) {
    if (!IsTaskValid(ped, m_pClient)) {
        bTaskCanBeFinished = true;
        return nullptr;
    }

    m_vecVehiclePosn = m_pClient->m_pVehicle->GetPosition();

    auto* const playerData = m_pClient->GetPlayerData();
    playerData->m_pCurrentProstitutePed = ped;
    CEntity::RegisterReference(FindPlayerPed()->GetPlayerData()->m_pCurrentProstitutePed);

    if (playerData->m_pLastProstituteShagged != ped) {
        CEntity::SafeCleanUpRef(playerData->m_pLastProstituteShagged);
        playerData->m_pLastProstituteShagged = ped;
        CEntity::RegisterReference(playerData->m_pLastProstituteShagged);
    }

    return CreateSubTask(TASK_COMPLEX_SEEK_ENTITY, ped);
}

// 0x666780
CTask* CTaskComplexProstituteSolicit::CreateNextSubTask(CPed* ped) {
    if (!m_pClient) {
        return nullptr;
    }

    if (!IsTaskValid(ped, m_pClient)) {
        bTaskCanBeFinished = true;
    }

    switch (m_pSubTask->GetTaskType()) {
    case TASK_COMPLEX_CAR_DRIVE:
        return CreateSubTask(TASK_COMPLEX_LEAVE_CAR, ped);

    case TASK_SIMPLE_STAND_STILL: {
        if (!bPlayerHasAcceptedSexProposition) {
            return CreateSubTask(TASK_FINISHED, ped);
        }
        if (!CCheat::IsActive(CHEAT_PROSTITUTES_PAY_YOU)) {
            if (FindPlayerPed()->GetPlayerInfoForThisPlayerPed()->m_nMoney < 20) {
                CMessages::ClearMessages(false);
                CMessages::AddMessageQ(TheText.Get("PROS_06"), 2000, 1, true); // You've got money right?
                CMessages::AddMessageQ(TheText.Get("PROS_09"), 3000, 1, true); // Stop wasting my time!
                return CreateSubTask(TASK_FINISHED, ped);
            }
        }
        return CreateSubTask(TASK_COMPLEX_ENTER_CAR_AS_PASSENGER, ped);
    }

    case TASK_COMPLEX_ENTER_CAR_AS_PASSENGER:
        ped->Say(CTX_GLOBAL_SOLICIT_THANKS);
        m_vecVehiclePosn = m_pClient->m_pVehicle->GetPosition();
        return CreateSubTask(TASK_COMPLEX_CAR_DRIVE, ped);

    case TASK_COMPLEX_LEAVE_CAR:
        g_ikChainMan.LookAt("TaskProzzy", ped, m_pClient, 2500, BONE_UNKNOWN, nullptr, false, 0.25f, 500, 3, false);
        return CreateSubTask(TASK_FINISHED, ped);

    case TASK_COMPLEX_SEEK_ENTITY:
        g_ikChainMan.LookAt("TaskProzzy", ped, m_pClient, 5000, BONE_UNKNOWN, nullptr, false, 0.25f, 500, 3, false);
        return CreateSubTask(TASK_COMPLEX_TURN_TO_FACE_ENTITY, ped);

    case TASK_COMPLEX_TURN_TO_FACE_ENTITY:
        ped->Say(CTX_GLOBAL_SOLICIT);
        CMessages::AddMessageQ(TheText.Get("PROS_04"), 5000, 1, true); // You want a good time, honey?
        return CreateSubTask(TASK_SIMPLE_STAND_STILL, ped);

    default:
        return nullptr;
    }
}

// 0x6669D0
CTask* CTaskComplexProstituteSolicit::ControlSubTask(CPed* ped) {
    // 0x406DA0 - squared magnitude, summed in extended precision in this order
    const auto SquaredMagnitude = [](const CVector& v) {
        return ((double)v.x * (double)v.x + (double)v.y * (double)v.y) + (double)v.z * (double)v.z;
    };

    bMoveCameraDown = bSexProcessStarted;

    if (!IsTaskValid(ped, m_pClient)) {
        bMoveCameraDown    = false;
        bTaskCanBeFinished = true;
    }

    // x87: `timeStep * 0.02f * 1000.0f` stays in extended precision until `_ftol`
    const auto dt = (int32)(int64)((double)CTimer::GetTimeStep() * (double)0.02f * (double)1000.0f);

    if (bTaskCanBeFinished) {
        if (m_nCurrentTimer == 0) {
            if (m_pSubTask->GetTaskType() != TASK_COMPLEX_LEAVE_CAR && m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_URGENT, nullptr)) {
                return CreateSubTask(TASK_COMPLEX_LEAVE_CAR, ped);
            }
        } else {
            m_nCurrentTimer = (int16)(m_nCurrentTimer - dt);
            if (m_nCurrentTimer <= 0) {
                CAEPedSpeechAudioEntity::SetCJMood(MOOD_WR, 120'000, -1, -1, -1);
                m_pClient->Say(CTX_GLOBAL_AFTER_SEX);
                m_nCurrentTimer = 0;
            }
        }
        return m_pSubTask;
    }

    const auto subTaskType = m_pSubTask->GetTaskType();

    if (subTaskType == TASK_SIMPLE_STAND_STILL) {
        auto* const pad = CPad::GetPad(0);
        if (pad->ConversationYesJustDown()) {
            bPlayerHasAcceptedSexProposition = true;
            if (m_pClient) {
                m_pClient->Say(CTX_GLOBAL_SOLICIT_PRO_YES);
            }
            return CreateNextSubTask(ped);
        }
        if (pad->ConversationNoJustDown()) {
            if (m_pClient) {
                m_pClient->Say(CTX_GLOBAL_SOLICIT_PRO_NO);
            }
            bTaskCanBeFinished = true;
        }
        return m_pSubTask;
    }

    if (subTaskType == TASK_COMPLEX_SEEK_ENTITY || subTaskType == TASK_COMPLEX_ENTER_CAR_AS_PASSENGER) {
        const auto  vehPos = m_pClient->m_pVehicle->GetPosition();
        const double dx = (double)vehPos.x - (double)m_vecVehiclePosn.x;
        const double dy = (double)vehPos.y - (double)m_vecVehiclePosn.y;
        const double dz = (double)vehPos.z - (double)m_vecVehiclePosn.z;
        if ((dx * dx + dz * dz) + dy * dy > 16.0f) { // the asm sums x, z, y
            bTaskCanBeFinished = true;
        }
        return m_pSubTask;
    }

    if (subTaskType != TASK_COMPLEX_CAR_DRIVE) {
        return m_pSubTask;
    }

    const auto now = CTimer::GetTimeInMS();
    if (TheCamera.m_nWhoIsInControlOfTheCamera == 1) {
        m_nLastSavedTime = now;
        m_nCurrentTimer  = 8000;
        bMoveCameraDown  = false;
        b08              = true;
        bVehicleShifted  = true;
        return m_pSubTask;
    }

    // Is the vehicle (nearly) standing still?
    const auto vehicleSpeed = m_pClient->m_pVehicle->m_vecMoveSpeed * 50.0f;
    const bool isVehicleStill = SquaredMagnitude(vehicleSpeed) < 0.5625f;
    if (!isVehicleStill || b08) {
        b08              = false;
        m_nLastSavedTime = now;
    }

    auto* const wanted = FindPlayerWanted(-1);

    if (now > (uint32)m_nNextTimeToCheckForSecludedPlace) {
        bPedsCanPotentiallySeeThis = false;
        bPedsCanSeeThis            = false;
        bCopsCanSeeThis            = false;
        m_nNextTimeToCheckForSecludedPlace = now + 1000;

        const auto nearbyPeds = ped->GetIntelligence()->GetPedEntities();
        for (auto i = 0; i < 16; i++) {
            auto* const other = static_cast<CPed*>(nearbyPeds[i]);
            if (!other || other == m_pClient || other->m_nPedType == PED_TYPE_PROSTITUTE) {
                continue;
            }

            const auto sqDist = SquaredMagnitude(other->GetPosition() - ped->GetPosition());
            if (sqDist < 56.25f) {
                bPedsCanSeeThis = true;
            }
            if (sqDist < 400.0f) {
                bPedsCanPotentiallySeeThis = true;
            }

            if (other->m_nPedType == PED_TYPE_COP && bSexProcessStarted && wanted && (int32)wanted->m_WantedLevel < 1) {
                auto* const veh = m_pClient->m_pVehicle;
                const bool  usedCollision = veh->m_bUsesCollision;
                veh->m_bUsesCollision = false;
                const bool isClear = CWorld::GetIsLineOfSightClear(other->GetPosition(), m_pClient->GetPosition(), true, true, false, true, false, true, false);
                veh->m_bUsesCollision = usedCollision;
                if (isClear) {
                    bCopsCanSeeThis = true;
                }
            }
        }
    }

    if (bSearchingForSecludedPlace) {
        if (isVehicleStill && now - m_nLastSavedTime > 4000u) {
            if (bPedsCanPotentiallySeeThis) {
                if (!bSecludedPlaceMessageShown) {
                    CMessages::AddMessageQ(TheText.Get("PROS_01"), 3000, 1, true);
                    bSecludedPlaceMessageShown = true;
                }
            } else {
                bSearchingForSecludedPlace = false;
                bSexProcessStarted         = true;
                m_nLastPaymentTime         = now;
                CMessages::AddMessageQ(TheText.Get("PROS_02"), 2000, 1, true);
            }
        }
        return m_pSubTask;
    }

    if (!bSexProcessStarted) {
        return m_pSubTask;
    }

    if (b07) {
        b07 = false;
        m_nCurrentTimer = 15'000;
        CStats::IncrementStat(STAT_NUMBER_OF_PROSTITUTES_VISITED, 1.0f);
    }

    auto* const pad = CPad::GetPad(0);
    const bool  isPlayerDriving = pad->GetAccelerate() || pad->GetBrake();
    const auto  randVal = rand(); // 0x821B1E

    bool copsCanSee = false;
    if (bCopsCanSeeThis && wanted && (int32)wanted->m_WantedLevel < 1) {
        FindPlayerWanted(-1)->SetWantedLevel(eWantedLevel::WANTED_LEVEL_1);
        copsCanSee = true;
    }

    if (isPlayerDriving || bPedsCanSeeThis || copsCanSee) {
        // Interrupt the sex
        m_nLastSavedTime           = now;
        bSexProcessStarted         = false;
        bSearchingForSecludedPlace = true;
        bVehicleShifted            = true;
        if (isPlayerDriving) {
            if ((uint32)randVal >= 0x1FFFFFFFu && m_nCurrentTimer >= 3000) {
                m_nCurrentTimer = 8000;
                return m_pSubTask;
            }
            bTaskCanBeFinished = true;
            m_nCurrentTimer    = 0;
            CMessages::AddMessageQ(TheText.Get("PROS_09"), 3000, 1, true);
            return m_pSubTask;
        }
        if (m_nCurrentTimer < 3000) {
            bTaskCanBeFinished = true;
            m_nCurrentTimer    = 0;
            return m_pSubTask;
        }
        CMessages::AddMessageQ(TheText.Get("PROS_01"), 3000, 1, true);
        m_nCurrentTimer = 8000;
        return m_pSubTask;
    }

    // Shake the vehicle
    m_nVehicleMovementTimer = (int16)(m_nVehicleMovementTimer - dt);
    if (m_nVehicleMovementTimer <= 0) {
        float shakeScale = CGeneral::GetRandomNumberInRange(-0.5f, -0.9f);
        if (m_nCurrentTimer > 10'000) {
            m_nVehicleMovementTimer = 850;
        } else if (m_nCurrentTimer > 5000) {
            m_nVehicleMovementTimer = 450;
        } else if (m_nCurrentTimer > 1000) {
            m_nVehicleMovementTimer = 120;
        } else {
            shakeScale = (float)((double)shakeScale * (double)0.5f);
            m_nVehicleMovementTimer = 850;
            CPad::GetPad(0)->StartShake(1000, 120, 0);
        }

        auto* const veh       = m_pClient->m_pVehicle;
        const auto  vehPos    = veh->GetPosition();
        const auto  clientPos = m_pClient->GetPosition();
        // 0x404330 Min(150, vehMass / 15), 0x420800 Max(that, clientMass); both pick the 2nd operand on NaN
        const float scaledMass = veh->m_fMass * 0.06666667f; // 0x863E0C
        const float minMass    = 150.0f < scaledMass ? 150.0f : scaledMass;
        const float mass    = minMass > m_pClient->m_fMass ? minMass : m_pClient->m_fMass;

        const auto& pointSrc = (randVal & 1) ? clientPos : ped->GetPosition();
        const float pointY   = (float)((double)pointSrc.y - (double)vehPos.y);
        const float pointX   = (float)((double)pointSrc.x - (double)vehPos.x);
        veh->ApplyTurnForce(CVector{ 0.0f, 0.0f, (float)((double)mass * (double)shakeScale) }, CVector{ pointX, pointY, 0.0f });
        veh->m_vehicleAudio.AddAudioEvent(AE_SUSPENSION_BOUNCE, 0.0f);

        if (b10 && (uint32)randVal > 0xFFFFFFFu) { // NOTE: never true, `rand()` is at most 0x7FFF
            ped->Say((randVal & 0xFFFF) < 0xFF ? CTX_GLOBAL_GIVING_HEAD : CTX_GLOBAL_HAVING_SEX, 0, 0.5f);
        }
    }

    m_nCurrentTimer = (int16)(m_nCurrentTimer - dt);
    if (m_nCurrentTimer <= 0) {
        m_nCurrentTimer    = 3000;
        bSexProcessStarted = false;
        bTaskCanBeFinished = true;
    }

    if (now - m_nLastPaymentTime > 1000u) {
        m_nLastPaymentTime = now;
        auto* const playerInfo = static_cast<CPlayerPed*>(m_pClient)->GetPlayerInfoForThisPlayerPed();
        if (CCheat::IsActive(CHEAT_PROSTITUTES_PAY_YOU)) {
            playerInfo->m_nMoney += 2;
        } else if (playerInfo->m_nMoney >= 2) {
            playerInfo->m_nMoney -= 2;
            CStats::IncrementStat(STAT_PROSTITUTE_BUDGET, 2.0f);
            ped->m_nMoneyCount++;
        } else {
            playerInfo->m_nMoney = 0;
            m_nCurrentTimer      = 0;
            bSexProcessStarted   = false;
            bTaskCanBeFinished   = true;
            CMessages::ClearMessages(false);
            CMessages::AddMessageQ(TheText.Get("PROS_06"), 2000, 1, true); // You've got money right?
            CMessages::AddMessageQ(TheText.Get("PROS_09"), 3000, 1, true); // Stop wasting my time!
        }
        if (!bVehicleShifted) {
            playerInfo->AddHealth(2);
        }
    }
    return m_pSubTask;
}

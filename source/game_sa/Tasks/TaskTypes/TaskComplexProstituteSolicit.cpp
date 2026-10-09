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
    return plugin::CallMethodAndReturn<CTask*, 0x6669D0, CTaskComplexProstituteSolicit*, CPed*>(this, ped);
}

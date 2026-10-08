#include "StdInc.h"
#include "TaskComplexEnterCarAsPassengerWait.h"
#include "TaskComplexEnterCarAsPassengerTimed.h"
#include "TaskComplexSequence.h"
#include "TaskComplexTurnToFaceEntityOrCoord.h"
#include "TaskComplexGoToPointAndStandStill.h"
#include "TaskSimpleStandStill.h"
#include "TaskSimpleCarDriveTimed.h"
#include "TaskSimpleWaitUntilPedIsInCar.h"
#include "PedGeometryAnalyser.h"
#include "PedGroups.h"
#include "PedGroup.h"
#include "CarEnterExit.h"
#include <optional>

void CTaskComplexEnterCarAsPassengerWait::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexEnterCarAsPassengerWait, 0x86E778, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x63B320);
    RH_ScopedInstall(Destructor, 0x63B3C0);

    RH_ScopedInstall(CreateSubTask, 0x6408D0);
    RH_ScopedVMTInstall(Clone, 0x63D850);
    RH_ScopedVMTInstall(GetTaskType, 0x63B3B0);
    RH_ScopedVMTInstall(CreateNextSubTask, 0x643E10);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x643F00);
    RH_ScopedVMTInstall(ControlSubTask, 0x640730);
}

// 0x63B320
CTaskComplexEnterCarAsPassengerWait::CTaskComplexEnterCarAsPassengerWait(CVehicle* target, CPed* waitFor, bool forceFrontSeat, eMoveState ms) :
    m_Car{target},
    m_WaitForPed{waitFor},
    m_bForceFrontSeat{forceFrontSeat},
    m_MoveState{ms}
{
    CEntity::SafeRegisterRef(m_Car);
    CEntity::SafeRegisterRef(m_WaitForPed);
}
    
// NOTSA (for 0x63D850)
CTaskComplexEnterCarAsPassengerWait::CTaskComplexEnterCarAsPassengerWait(const CTaskComplexEnterCarAsPassengerWait& o) :
    CTaskComplexEnterCarAsPassengerWait{
        o.m_Car,
        o.m_WaitForPed,
        o.m_bForceFrontSeat,
        o.m_MoveState
    }
{
}

// 0x63B3C0
CTaskComplexEnterCarAsPassengerWait::~CTaskComplexEnterCarAsPassengerWait() {
    CEntity::SafeCleanUpRef(m_Car);
    CEntity::SafeCleanUpRef(m_WaitForPed);
}

// 0x6408D0
CTask* CTaskComplexEnterCarAsPassengerWait::CreateSubTask(int32 taskType, CPed* ped) {
    switch (taskType) {
    case TASK_SIMPLE_CAR_DRIVE_TIMED:
        return new CTaskSimpleCarDriveTimed{m_Car, 0};
    case TASK_COMPLEX_SEQUENCE: {
        const auto seq = new CTaskComplexSequence{};
        seq->AddTask(new CTaskComplexTurnToFaceEntityOrCoord{m_Car, 0.5f, 0.2f});
        seq->AddTask(new CTaskSimpleStandStill{1000, false, false, 8.f});
        return seq;
    }
    case TASK_COMPLEX_ENTER_CAR_AS_PASSENGER_TIMED: {
        const auto targetSeat = m_bForceFrontSeat
            ? (uint32)CCarEnterExit::ComputeTargetDoorToEnterAsPassenger(m_Car, 0)
            : 0u;
        const auto task = new CTaskComplexEnterCarAsPassengerTimed{m_Car, targetSeat, (uint32)-1, true};
        task->SetMoveState(m_MoveState);
        return task;
    }
    case TASK_SIMPLE_WAIT_UNTIL_PED_IN_CAR:
        return new CTaskSimpleWaitUntilPedIsInCar{m_WaitForPed};
    case TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL: {
        CVector closest;
        if (CPedGeometryAnalyser::ComputeClosestSurfacePoint(*ped, *m_Car, closest)) {
            return new CTaskComplexGoToPointAndStandStill{m_MoveState, closest, 1.5f, 2.f, false, false};
        }
        const float radius = m_Car->GetModelInfo()->GetColModel()->GetBoundRadius() + 1.5f;
        return new CTaskComplexGoToPointAndStandStill{m_MoveState, m_Car->GetPosition(), radius, 2.f, false, false};
    }
    default: // Includes TASK_FINISHED
        return nullptr;
    }
}

// 0x643E10
CTask* CTaskComplexEnterCarAsPassengerWait::CreateNextSubTask(CPed* ped) {
    if (!m_Car) {
        return CreateSubTask(TASK_FINISHED, ped);
    }

    switch (m_pSubTask->GetTaskType()) {
    case TASK_SIMPLE_CAR_DRIVE_TIMED:
        return CreateSubTask(TASK_FINISHED, ped);
    case TASK_COMPLEX_ENTER_CAR_AS_PASSENGER_TIMED:
        if (!ped->bInVehicle && ++m_EnterCarFails < 16 && m_Car) {
            return CreateSubTask(TASK_COMPLEX_SEQUENCE, ped);
        }
        return CreateSubTask(TASK_FINISHED, ped);
    case TASK_COMPLEX_SEQUENCE:
    case TASK_SIMPLE_WAIT_UNTIL_PED_IN_CAR:
        if (m_Car) {
            return CreateSubTask(TASK_COMPLEX_ENTER_CAR_AS_PASSENGER_TIMED, ped);
        }
        return CreateSubTask(TASK_FINISHED, ped);
    case TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL:
        if (m_WaitForPed) {
            return CreateSubTask(TASK_SIMPLE_WAIT_UNTIL_PED_IN_CAR, ped);
        }
        return CreateSubTask(TASK_FINISHED, ped);
    default:
        return nullptr;
    }
}

// 0x643F00
CTask* CTaskComplexEnterCarAsPassengerWait::CreateFirstSubTask(CPed* ped) {
    if (ped->bInVehicle && ped->m_pVehicle && ped->m_pVehicle == m_Car) {
        if (ped->m_pVehicle->IsPassenger(ped)) {
            return CreateSubTask(TASK_SIMPLE_CAR_DRIVE_TIMED, ped);
        }
    }

    if (!m_WaitForPed) {
        if (m_Car) {
            return CreateSubTask(TASK_COMPLEX_ENTER_CAR_AS_PASSENGER_TIMED, ped);
        }
    } else if (m_Car) {
        return CreateSubTask(TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL, ped);
    }
    return CreateSubTask(TASK_FINISHED, ped);
}

// 0x640730
CTask* CTaskComplexEnterCarAsPassengerWait::ControlSubTask(CPed* ped) {
    // Say something once
    if (!m_bPlayedSample) {
        std::optional<eGlobalSpeechContext> ctx{};
        if (m_WaitForPed) {
            if (m_WaitForPed->IsPlayer() && ped->GetTaskManager().FindActiveTaskByType(TASK_SIMPLE_CAR_GET_IN)) {
                ctx = CTX_GLOBAL_CAR_GET_IN;
            }
        } else if (const auto group = CPedGroups::GetPedsGroup(ped)) {
            if (const auto leader = group->GetMembership().GetLeader()) {
                if (leader->IsPlayer() && ped->GetTaskManager().FindActiveTaskByType(TASK_SIMPLE_CAR_GET_IN)) {
                    ctx = CTX_GLOBAL_CAR_WAIT_FOR_ME;
                }
            }
        }
        if (ctx) {
            m_bPlayedSample = true;
            ped->Say(*ctx, 0, 1.f, false, false, false);
        }
    }

    if (m_Car && m_pSubTask->GetTaskType() == TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL) {
        const auto subTask = static_cast<CTaskComplexGoToPointAndStandStill*>(m_pSubTask);

        // BUG: `closest` is not written to if there's no surface point, the original reads uninitialized stack memory then
        CVector closest{};
        const bool found = CPedGeometryAnalyser::ComputeClosestSurfacePoint(*ped, *m_Car, closest);
        const float dx = closest.x - subTask->m_vecTargetPoint.x;
        const float dy = closest.y - subTask->m_vecTargetPoint.y;
        const float dz = closest.z - subTask->m_vecTargetPoint.z;
        const float distSq = dx * dx + dy * dy + dz * dz;

        if (distSq > 0.025f) {
            if (found) {
                subTask->GoToPoint(closest, 1.5f, 2.f, false);
            } else {
                const float radius = m_Car->GetModelInfo()->GetColModel()->GetBoundRadius() + 1.5f;
                subTask->GoToPoint(m_Car->GetPosition(), radius, 2.f, false);
            }
        }
    }

    return m_pSubTask;
}

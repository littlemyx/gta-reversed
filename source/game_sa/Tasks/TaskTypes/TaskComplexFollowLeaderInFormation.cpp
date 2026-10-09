#include "StdInc.h"

#include "TaskComplexFollowLeaderInFormation.h"

#include "TaskSimpleCarDrive.h"
#include "TaskSimplePause.h"
#include "TaskSimpleStandStill.h"
#include "TaskSimpleAchieveHeading.h"
#include "TaskComplexLeaveCar.h"
#include "TaskComplexEnterCarAsPassenger.h"
#include "SeekEntity/TaskComplexSeekEntity.h"

void CTaskComplexFollowLeaderInFormation::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexFollowLeaderInFormation, 0x870c3c, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x6949A0);
    RH_ScopedInstall(Destructor, 0x694A40);

    RH_ScopedInstall(CreateSubTask, 0x6962A0);

    RH_ScopedVMTInstall(Clone, 0x695740);
    RH_ScopedVMTInstall(GetTaskType, 0x694A30);
    RH_ScopedVMTInstall(CreateNextSubTask, 0x696820);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x6968E0);
    RH_ScopedVMTInstall(ControlSubTask, 0x696940);
}

// 0x6949A0
CTaskComplexFollowLeaderInFormation::CTaskComplexFollowLeaderInFormation(CPedGroup* pedGroup, CPed* ped, const CVector& posn, float a5) :
    CTaskComplex(),
    m_Group{pedGroup},
    m_Leader{ped},
    m_Pos{posn},
    m_Int{4},
    m_Dist{a5}
{
    CEntity::SafeRegisterRef(m_Leader);
}

// For 0x695740
CTaskComplexFollowLeaderInFormation::CTaskComplexFollowLeaderInFormation(const CTaskComplexFollowLeaderInFormation& o) :
    CTaskComplexFollowLeaderInFormation{
        o.m_Group,
        o.m_Leader,
        o.m_Pos,
        o.m_Dist
    }
{
}

// 0x694A40
CTaskComplexFollowLeaderInFormation::~CTaskComplexFollowLeaderInFormation() {
    CEntity::SafeCleanUpRef(m_Leader);
}

// 0x6962A0
CTask* CTaskComplexFollowLeaderInFormation::CreateSubTask(eTaskType taskType, CPed* ped) {
    switch (taskType) {
    case TASK_SIMPLE_CAR_DRIVE:
        return new CTaskSimpleCarDrive{ped->m_pVehicle, nullptr, false};
    case TASK_COMPLEX_ENTER_CAR_AS_PASSENGER:
        return new CTaskComplexEnterCarAsPassenger{m_Leader->m_pVehicle, 0, false};
    case TASK_SIMPLE_PAUSE:
        return new CTaskSimplePause{2000};
    case TASK_SIMPLE_STAND_STILL:
        return new CTaskSimpleStandStill{500, false, false, 8.f};
    case TASK_COMPLEX_LEAVE_CAR:
        return new CTaskComplexLeaveCar{ped->m_pVehicle, 0, 0, true, false};
    case TASK_SIMPLE_ACHIEVE_HEADING: {
        auto& mem = m_Group->GetMembership();
        auto  other = mem.GetMember(CGeneral::GetRandomNumberInRange(0, (int32)mem.CountMembers()));
        if (other == ped) {
            other = mem.GetLeader();
        }
        if (other && CGeneral::GetRandomNumberInRange(0, 50) == 20) {
            auto dir = other->GetPosition() - ped->GetPosition();
            dir.Normalise();
            return new CTaskSimpleAchieveHeading{CGeneral::LimitRadianAngle(CGeneral::GetRadianAngleBetweenPoints(dir.x, dir.y, 0.f, 0.f)), 0.5f, 0.2f};
        }
        return new CTaskSimpleAchieveHeading{ped->m_fCurrentRotation, 0.5f, 0.2f};
    }
    case TASK_COMPLEX_SEEK_ENTITY: {
        const auto seek = new CTaskComplexSeekEntity<CEntitySeekPosCalculatorXYOffset>{
            m_Leader,
            50'000,
            1'000,
            1.f,
            2.f,
            2.f,
            false,
            true,
            CEntitySeekPosCalculatorXYOffset{m_Pos}
        };
        seek->SetMoveState(PEDMOVE_SPRINT);
        return seek;
    }
    default: // Includes `TASK_FINISHED`
        return nullptr;
    }
}

// 0x696820
CTask* CTaskComplexFollowLeaderInFormation::CreateNextSubTask(CPed* ped) {
    const auto leaderMoveState = m_Leader->m_nMoveState;
    switch (m_pSubTask->GetTaskType()) {
    case TASK_SIMPLE_PAUSE:
        return CreateSubTask(TASK_SIMPLE_PAUSE, ped);
    case TASK_SIMPLE_STAND_STILL:
        return CreateSubTask(TASK_COMPLEX_SEEK_ENTITY, ped);
    case TASK_COMPLEX_ENTER_CAR_AS_PASSENGER:
        return CreateFirstSubTask(ped);
    case TASK_COMPLEX_LEAVE_CAR:
    case TASK_SIMPLE_CAR_DRIVE:
        return CreateSubTask(TASK_COMPLEX_SEEK_ENTITY, ped);
    case TASK_COMPLEX_SEEK_ENTITY:
        if (leaderMoveState == PEDMOVE_STILL || leaderMoveState == PEDMOVE_NONE) {
            return CreateSubTask(TASK_SIMPLE_STAND_STILL, ped);
        }
        return CreateSubTask(TASK_COMPLEX_SEEK_ENTITY, ped);
    default:
        return nullptr;
    }
}

// 0x6968E0
CTask* CTaskComplexFollowLeaderInFormation::CreateFirstSubTask(CPed* ped) {
    if (ped->bInVehicle && ped->m_pVehicle) {
        if (m_Leader->bInVehicle && m_Leader->m_pVehicle == ped->m_pVehicle) {
            return CreateSubTask(TASK_SIMPLE_CAR_DRIVE, ped);
        }
        return CreateSubTask(TASK_COMPLEX_LEAVE_CAR, ped);
    }
    return CreateSubTask(TASK_COMPLEX_SEEK_ENTITY, ped);
}

// 0x696940
CTask* CTaskComplexFollowLeaderInFormation::ControlSubTask(CPed* ped) {
    const auto ret = m_pSubTask;
    if (!m_Leader) {
        return nullptr;
    }

    if (m_pSubTask->GetTaskType() == TASK_SIMPLE_CAR_DRIVE) {
        return ret;
    }

    if (m_Leader->bInVehicle && m_Leader->m_pVehicle) { // Leader is in a vehicle
        const auto leaderVeh = m_Leader->m_pVehicle;
        if (m_pSubTask->GetTaskType() == TASK_COMPLEX_SEEK_ENTITY) {
            // Set the seek task's max dist to the vehicle's bound radius (this also resets the seek task's scan timer)
            const auto radius = CModelInfo::GetModelInfo(leaderVeh->m_nModelIndex)->GetColModel()->GetBoundRadius();
            static_cast<CTaskComplexSeekEntity<>*>(m_pSubTask)->SetMaxEntityDist2D(radius); // 0x6955D0

            if (m_Dist <= 0.f) {
                return ret;
            }

            const auto distSq = (ped->GetPosition() - m_Leader->GetPosition()).SquaredMagnitude();
            if (!(sq(m_Dist) < distSq)) {
                return ret;
            }

            if (!m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_LEISURE, nullptr)) {
                return ret;
            }

            CTaskSimpleStandStill{0, false, false, 8.f}.ProcessPed(ped);
            return new CTaskSimplePause{2000};
        }

        if (m_pSubTask->GetTaskType() != TASK_SIMPLE_STAND_STILL) {
            return ret;
        }

        if (CPedGroups::GetPedsGroup(ped)) {
            return ret;
        }

        const auto radius = CModelInfo::GetModelInfo(leaderVeh->m_nModelIndex)->GetColModel()->GetBoundRadius();
        const auto radiusSq = radius * radius;
        const auto distSq = (ped->GetPosition() - m_Leader->GetPosition()).SquaredMagnitude();
        if (distSq < radiusSq) { // Close to the vehicle => try to enter it
            if (!(leaderVeh->m_nNumPassengers < leaderVeh->m_nMaxPassengers)) {
                return ret;
            }
            if (!m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_LEISURE, nullptr)) {
                return ret;
            }
            return new CTaskComplexEnterCarAsPassenger{leaderVeh, 0, false};
        }

        if (!(distSq < sq(m_Dist))) {
            return ret;
        }
        if (!m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_LEISURE, nullptr)) {
            return ret;
        }
        return CreateSubTask(TASK_COMPLEX_SEEK_ENTITY, ped);
    }

    // Leader is on foot
    if (m_pSubTask->GetTaskType() == TASK_COMPLEX_ENTER_CAR_AS_PASSENGER) {
        if (!m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_LEISURE, nullptr)) {
            return ret;
        }
        return CreateFirstSubTask(ped);
    }

    if (m_pSubTask->GetTaskType() != TASK_SIMPLE_PAUSE && m_pSubTask->GetTaskType() != TASK_SIMPLE_STAND_STILL) {
        return ret;
    }

    const auto distSq = (ped->GetPosition() - m_Leader->GetPosition()).SquaredMagnitude();
    if (sq(m_Dist) <= distSq) {
        return ret;
    }
    return CreateSubTask(TASK_COMPLEX_SEEK_ENTITY, ped);
}

#include "StdInc.h"
#include "Ragdoll/IKChainManager.h"
#include "TaskComplexInvestigateDeadPed.h"
#include "TaskComplexGoToPointAndStandStill.h"
#include "TaskSimpleStandStill.h"
#include "TaskSimpleRunAnim.h"
#include "TaskSimpleGoToPoint.h"
#include "TaskComplexSignalAtPed.h"
#include "TaskSimpleAchieveHeading.h"

void CTaskComplexInvestigateDeadPed::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexInvestigateDeadPed, 0x870d3c, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x696DF0);
    RH_ScopedInstall(Destructor, 0x696E80);

    RH_ScopedInstall(ComputeTargetPos, 0x696EE0);
    RH_ScopedInstall(CreateSubTask, 0x696F90);

    RH_ScopedVMTInstall(Clone, 0x697680);
    RH_ScopedVMTInstall(GetTaskType, 0x696E70);
    RH_ScopedVMTInstall(CreateNextSubTask, 0x697840);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x697990);
    RH_ScopedVMTInstall(ControlSubTask, 0x697A00);
}

// 0x696DF0
CTaskComplexInvestigateDeadPed::CTaskComplexInvestigateDeadPed(CPed* deadPed) :
    m_deadPed{deadPed} // 0x696E18: [esi+0Ch] = deadPed (the member was never assigned: null reference registered, dead ped pointer lost)
{
    CEntity::SafeRegisterRef(m_deadPed);
}

CTaskComplexInvestigateDeadPed::CTaskComplexInvestigateDeadPed(const CTaskComplexInvestigateDeadPed& o) :
    CTaskComplexInvestigateDeadPed{o.m_deadPed}
{
}

// 0x696E80
CTaskComplexInvestigateDeadPed::~CTaskComplexInvestigateDeadPed() {
    CEntity::SafeCleanUpRef(m_deadPed);
}

// 0x696EE0
void CTaskComplexInvestigateDeadPed::ComputeTargetPos(CPed* ped, CVector& targetPos) {
    const auto& deadPedPos = m_deadPed->GetPosition();
    auto dir = ped->GetPosition() - deadPedPos;
    dir.z = 0.f; // 0x696F19: mov [esp+0Ch], 0 - the exe normalises the 2D direction only (z stays the dead ped's z)
    targetPos = deadPedPos + Normalized(dir) * 2.f;
}

// 0x696F90
CTask* CTaskComplexInvestigateDeadPed::CreateSubTask(eTaskType taskType, CPed* ped) {
    switch (taskType) {
    case TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL:
        ComputeTargetPos(ped, m_targetPos); // 0x69714C: the exe recomputes the target position (into this+0x1C) before building the task; the port used the stale member
        return new CTaskComplexGoToPointAndStandStill{ PEDMOVE_RUN, m_targetPos, 0.5f, 2.f, false, false };
    case TASK_COMPLEX_SIGNAL_AT_PED:
        return new CTaskComplexSignalAtPed{ m_deadPed, -1, false };
    case TASK_FINISHED:
        return nullptr;
    case TASK_SIMPLE_ACHIEVE_HEADING:
        return new CTaskSimpleAchieveHeading{ m_targetHeading, 0.5f, 0.2f };
    case TASK_SIMPLE_STAND_STILL: {
        auto* const task = new CTaskSimpleStandStill{ 1000 };
        // 0x6970A3..0x6970BD: the exe also starts the head look-at at the dead ped right after building the stand-still task (name 0x870D68 "TaskInvDeadPed", 1000 ms, head bone, no offset, no torso, 0.25, 500, prio 3)
        g_ikChainMan.LookAt("TaskInvDeadPed", ped, m_deadPed, 1000, BONE_HEAD, nullptr, false, 0.25f, 500, 3, false);
        return task;
    }
    case TASK_SIMPLE_ANIM:
        return new CTaskSimpleRunAnim{ ped->m_nAnimGroup, ANIM_ID_ROADCROSS, 4.0f, false };
    case TASK_SIMPLE_GO_TO_POINT:
        return new CTaskSimpleGoToPoint{ PEDMOVE_WALK, m_pedInitalPos, 0.5f, false, false };
    default:
        NOTSA_UNREACHABLE();
        return nullptr; // 0x69718D: default => esi = 0
    }
}

// 0x697840
CTask* CTaskComplexInvestigateDeadPed::CreateNextSubTask(CPed* ped) {
    switch (m_pSubTask->GetTaskType()) {
    case TASK_SIMPLE_ACHIEVE_HEADING: {
        g_ikChainMan.LookAt("TaskInvDeadPed", ped, m_deadPed, m_lookDurMs, BONE_HEAD, nullptr, false, 0.25f, 500, 3, false);
        return CreateSubTask(TASK_COMPLEX_SIGNAL_AT_PED, ped);
    }
    case TASK_SIMPLE_STAND_STILL:
        return CreateSubTask(TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL, ped);
    case TASK_SIMPLE_ANIM:
        return CreateSubTask(TASK_SIMPLE_GO_TO_POINT, ped);
    case TASK_SIMPLE_GO_TO_POINT:
        return CreateSubTask(TASK_FINISHED, ped);
    case TASK_COMPLEX_SIGNAL_AT_PED:
        return CreateSubTask(TASK_SIMPLE_ANIM, ped);
    case TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL: {
        m_targetHeading = CGeneral::GetRadianAngleBetweenPoints({ m_deadPed->GetPosition() }, { ped->GetPosition() });
        return CreateSubTask(TASK_SIMPLE_ACHIEVE_HEADING, ped);
    }
    default:
        NOTSA_UNREACHABLE();
        return nullptr; // 0x697985: default => edi = 0
    }
}

// 0x697990
CTask* CTaskComplexInvestigateDeadPed::CreateFirstSubTask(CPed* ped) {
    m_pedInitalPos = ped->GetPosition();
    m_lookDurMs = CGeneral::GetRandomNumberInRange(5000, 7000);
    return CreateSubTask(TASK_SIMPLE_STAND_STILL, ped);
}

// 0x697A00
CTask* CTaskComplexInvestigateDeadPed::ControlSubTask(CPed* ped) {
    if (!m_deadPed) {
        return CreateSubTask(TASK_FINISHED, ped);
    }

    switch (m_pSubTask->GetTaskType()) {
    case TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL: {
        if (m_timer.IsOutOfTime()) { // NOTE: Timer is never started so this will never be true
            return nullptr;
        }
        
        // Set new target position if the distance between the last one is greater than 1
        CVector targetPos{};
        ComputeTargetPos(ped, targetPos);
        // 0x697A00..0x697B41: the exe sums the squared deltas as (z^2 + y^2) + x^2 (CVector::SquaredMagnitude is (x^2 + y^2) + z^2)
        const auto delta = targetPos - m_targetPos;
        if ((delta.z * delta.z + delta.y * delta.y) + delta.x * delta.x > 1.f) {
            m_targetPos = targetPos;
            static_cast<CTaskComplexGoToPointAndStandStill*>(m_pSubTask)->GoToPoint(m_targetPos, 0.5f, 2.f);
        }

        break;
    }
    case TASK_SIMPLE_ANIM: {
        ped->Say(CTX_GLOBAL_SHOCKED);
        break;
    }
    }
    return m_pSubTask;
}

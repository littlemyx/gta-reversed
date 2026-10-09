#include "StdInc.h"

#include "TaskComplexFallAndStayDown.h"
#include "TaskSimpleFall.h"

void CTaskComplexFallAndStayDown::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexFallAndStayDown, 0x870480, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor1, 0x6789D0);
    RH_ScopedInstall(Constructor2, 0x678A10);
    RH_ScopedInstall(CreateSubTask, 0x678BD0);
    RH_ScopedVMTDestructorInstall(0x67CBC0);
    RH_ScopedVMTInstall(Clone, 0x67C270);
    RH_ScopedVMTInstall(GetTaskType, 0x678A00);
    RH_ScopedVMTInstall(MakeAbortable, 0x678AA0);
    RH_ScopedVMTInstall(CreateNextSubTask, 0x67CBE0);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x678B20);
    RH_ScopedVMTInstall(ControlSubTask, 0x678BC0);
}

// 0x6789D0
CTaskComplexFallAndStayDown::CTaskComplexFallAndStayDown(AnimationId fallAnimId, AssocGroupId fallAnimGroup) :
    m_FallAnimId{ fallAnimId },
    m_FallAnimGroup{ fallAnimGroup }
{
}

// 0x678A10
CTaskComplexFallAndStayDown::CTaskComplexFallAndStayDown(int32 dir) :
    m_FallAnimGroup{ ANIM_GROUP_DEFAULT }
{
    switch (dir) { // Jump table @ 0x678A80
    case 0: m_FallAnimId = ANIM_ID_KO_SKID_FRONT; break;
    case 1: m_FallAnimId = ANIM_ID_KO_SPIN_R;     break;
    case 2: m_FallAnimId = ANIM_ID_KO_SKID_BACK;  break;
    case 3: m_FallAnimId = ANIM_ID_KO_SPIN_L;     break;
    default: break; // The original leaves the anim id uninitialized
    }
}

// 0x678AA0
bool CTaskComplexFallAndStayDown::MakeAbortable(CPed* ped, eAbortPriority priority, const CEvent* event) {
    switch (priority) {
    case ABORT_PRIORITY_IMMEDIATE:
        return m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_IMMEDIATE, event);
    case ABORT_PRIORITY_URGENT:
        // Can't abort a fall that way
        return m_pSubTask->GetTaskType() != TASK_SIMPLE_FALL && m_pSubTask->MakeAbortable(ped, priority, event);
    default:
        if (m_pSubTask->GetTaskType() != TASK_SIMPLE_FALL) {
            m_pSubTask->MakeAbortable(ped, priority, event); // Result is ignored
        }
        return false;
    }
}

// 0x67CBE0
CTask* CTaskComplexFallAndStayDown::CreateNextSubTask(CPed* ped) {
    if (m_pSubTask->GetTaskType() == TASK_SIMPLE_FALL) {
        return CreateSubTask(TASK_FINISHED);
    }
    return nullptr;
}

// 0x678B20
CTask* CTaskComplexFallAndStayDown::CreateFirstSubTask(CPed* ped) {
    return CreateSubTask(TASK_SIMPLE_FALL); // The original has this inlined
}

// 0x678BC0
CTask* CTaskComplexFallAndStayDown::ControlSubTask(CPed* ped) {
    return m_pSubTask;
}

// 0x678BD0
CTask* CTaskComplexFallAndStayDown::CreateSubTask(eTaskType taskType) {
    switch (taskType) {
    case TASK_SIMPLE_FALL:
        return new CTaskSimpleFall{ m_FallAnimId, m_FallAnimGroup, 99'999'999 }; // 0x5F5E0FF => stays down (practically) forever
    default: // Including `TASK_FINISHED`
        return nullptr;
    }
}

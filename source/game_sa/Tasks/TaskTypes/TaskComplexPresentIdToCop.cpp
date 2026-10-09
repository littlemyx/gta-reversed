#include "StdInc.h"

#include "TaskComplexPresentIdToCop.h"
#include "TaskSimpleAchieveHeading.h"
#include "TaskSimpleHandsUp.h"

void CTaskComplexPresentIdToCop::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexPresentIdToCop, 0x86F544, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x6591D0);
    RH_ScopedInstall(CreateSubTask, 0x65A110);
    RH_ScopedInstall(ComputeHeadingToCop, 0x6592A0);
    RH_ScopedVMTDestructorInstall(0x65A0F0);
    RH_ScopedVMTInstall(Clone, 0x659B60);
    RH_ScopedVMTInstall(GetTaskType, 0x659230);
    RH_ScopedVMTInstall(CreateNextSubTask, 0x65AFE0);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x65B050);
    RH_ScopedVMTInstall(ControlSubTask, 0x65B070);
}

// 0x6591D0
CTaskComplexPresentIdToCop::CTaskComplexPresentIdToCop(CPed* cop) :
    m_Cop{cop}
{
}

// 0x65AFE0
CTask* CTaskComplexPresentIdToCop::CreateNextSubTask(CPed* ped) {
    switch (m_pSubTask->GetTaskType()) {
    case TASK_SIMPLE_HANDS_UP:
        return CreateSubTask(TASK_FINISHED, ped);
    case TASK_SIMPLE_ACHIEVE_HEADING:
        ped->Say(CTX_GLOBAL_CRIMINAL_PLEAD, 0, 1.f, false, false, false); // 0x5EFFE0
        return CreateSubTask(TASK_SIMPLE_HANDS_UP, ped);
    default:
        return nullptr;
    }
}

// 0x65B050
CTask* CTaskComplexPresentIdToCop::CreateFirstSubTask(CPed* ped) {
    return CreateSubTask(TASK_SIMPLE_ACHIEVE_HEADING, ped);
}

// 0x65B070
CTask* CTaskComplexPresentIdToCop::ControlSubTask(CPed* ped) {
    if (!m_Cop) { // Cop is gone => finish
        return CreateSubTask(TASK_FINISHED, ped);
    }
    return m_pSubTask;
}

// 0x65A110
CTask* CTaskComplexPresentIdToCop::CreateSubTask(eTaskType taskType, CPed* ped) {
    switch (taskType) {
    case TASK_SIMPLE_HANDS_UP:
        return new CTaskSimpleHandsUp{ 3000 };
    case TASK_SIMPLE_ACHIEVE_HEADING:
        return new CTaskSimpleAchieveHeading{ ComputeHeadingToCop(ped), 0.5f, 0.2f }; // 0x86FC7C, 0x86FC80
    default: // Including `TASK_FINISHED`
        return nullptr;
    }
}

// 0x6592A0
float CTaskComplexPresentIdToCop::ComputeHeadingToCop(CPed* ped) const {
    const auto& pedPos = ped->GetPosition();
    const auto& copPos = m_Cop->GetPosition();
    return CGeneral::LimitRadianAngle(CGeneral::GetRadianAngleBetweenPoints(copPos.x, copPos.y, pedPos.x, pedPos.y)); // 0x53CBE0, 0x53CB50
}

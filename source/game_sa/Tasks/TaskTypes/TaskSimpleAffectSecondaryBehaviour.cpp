#include "StdInc.h"
#include "TaskSimpleAffectSecondaryBehaviour.h"

void CTaskSimpleAffectSecondaryBehaviour::InjectHooks() {
    RH_ScopedVirtualClass(CTaskSimpleAffectSecondaryBehaviour, 0x870b08, 9);
    RH_ScopedCategory("Tasks/TaskTypes");
    RH_ScopedVMTDestructorInstall(0x692FB0); // scalar deleting destructor; the destructor body is ported (VT-OVERRIDES)

    RH_ScopedInstall(Constructor, 0x691270);

    RH_ScopedVMTInstall(Clone, 0x692910);
    RH_ScopedVMTInstall(GetTaskType, 0x6912A0);
    RH_ScopedVMTInstall(MakeAbortable, 0x6912B0);
    RH_ScopedVMTInstall(ProcessPed, 0x691320);
}

// 0x691270
CTaskSimpleAffectSecondaryBehaviour::CTaskSimpleAffectSecondaryBehaviour(bool add, eSecondaryTask secondaryTaskType, CTask* task) :
    m_bAdd{add},
    m_secTaskType{secondaryTaskType},
    m_task{task}
{
}

// 0x6912C0. The task owns `m_task` (the port leaked it unless ProcessPed handed it over and nulled it)
CTaskSimpleAffectSecondaryBehaviour::~CTaskSimpleAffectSecondaryBehaviour() {
    delete m_task;
}

// NOTSA
CTaskSimpleAffectSecondaryBehaviour::CTaskSimpleAffectSecondaryBehaviour(const CTaskSimpleAffectSecondaryBehaviour& o) :
    CTaskSimpleAffectSecondaryBehaviour{o.m_bAdd, o.m_secTaskType, o.m_task ? o.m_task->Clone() : nullptr}
{
}

// 0x691320
bool CTaskSimpleAffectSecondaryBehaviour::ProcessPed(CPed* ped) {
    const auto currSecTask = ped->GetTaskManager().GetTaskSecondary(m_secTaskType);
    
    if (m_bAdd) {
        if (!currSecTask || currSecTask->MakeAbortable(ped)) {
            ped->GetTaskManager().SetTaskSecondary(m_task->Clone(), m_secTaskType);
            return true;
        }
        return false;
    }

    // 0x69137B: nothing left to take away => finished (true); otherwise ask it to go (LEISURE) and keep the task alive (false).
    // The port returned the opposite (true after the first request, false forever once there was nothing left to remove).
    if (!currSecTask) {
        return true;
    }
    currSecTask->MakeAbortable(ped, ABORT_PRIORITY_LEISURE, nullptr);
    return false;
}

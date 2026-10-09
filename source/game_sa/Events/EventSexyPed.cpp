#include "StdInc.h"
#include "EventSexyPed.h"

void CEventSexyPed::InjectHooks() {
    RH_ScopedVirtualClass(CEventSexyPed, 0x85B0B0, 17);
    RH_ScopedCategory("Events");

    RH_ScopedVMTDestructorInstall(0x4B6120);
    RH_ScopedVMTInstall(GetEventType, 0x4AEE60);
    RH_ScopedVMTInstall(GetEventPriority, 0x4AEE90);
    RH_ScopedVMTInstall(GetLifeTime, 0x4AEE70);
    RH_ScopedVMTInstall(AffectsPed, 0x4AEF00);
    RH_ScopedVMTInstall(GetSourceEntity, 0x4AEE80);
    RH_ScopedVMTInstall(CloneEditable, 0x4B7280);
}


CEventSexyPed::CEventSexyPed(CPed* ped) : CEventEditableResponse() {
    m_SexyPed = ped;
    CEntity::SafeRegisterRef(m_SexyPed);
}

CEventSexyPed::CEventSexyPed(CPed* sexyPed, eTaskType taskType) :
    CEventSexyPed{sexyPed}
{
    m_TaskId = taskType;
}

CEventSexyPed::~CEventSexyPed() {
    CEntity::SafeCleanUpRef(m_SexyPed);
}

bool CEventSexyPed::AffectsPed(CPed* ped) {
    if (!ped->IsAlive())
        return false;

    if (!m_SexyPed)
        return false;

    if (!m_SexyPed->IsAlive())
        return false;

    if (g_ikChainMan.IsLooking(ped) && g_ikChainMan.GetLookAtEntity(ped) == m_SexyPed) {
        return false;
    }

    if (ped->GetTaskManager().IsFirstFoundTaskMatching<TASK_COMPLEX_PARTNER_DEAL, TASK_COMPLEX_BE_IN_COUPLE, TASK_COMPLEX_PARTNER_GREET>(m_SexyPed->GetTaskManager())) {
        return false;
    }

    return true;
}


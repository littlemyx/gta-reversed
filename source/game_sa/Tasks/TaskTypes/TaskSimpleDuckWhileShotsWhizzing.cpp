#include "StdInc.h"

#include "TaskSimpleDuckWhileShotsWhizzing.h"
#include "Events/Event.h"

void CTaskSimpleDuckWhileShotsWhizzing::InjectHooks() {
    RH_ScopedVirtualClass(CTaskSimpleDuckWhileShotsWhizzing, 0x870BEC, 9);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x692680);
    RH_ScopedVMTDestructorInstall(0x693B80);
    RH_ScopedVMTInstall(Clone, 0x692D60);
    RH_ScopedVMTInstall(GetTaskType, 0x6926E0);
    RH_ScopedVMTInstall(MakeAbortable, 0x692700);
    RH_ScopedVMTInstall(ProcessPed, 0x694640);
}

// 0x692680
CTaskSimpleDuckWhileShotsWhizzing::CTaskSimpleDuckWhileShotsWhizzing(uint16 lengthOfDuck) :
    CTaskSimpleDuck{ DUCK_STANDALONE, lengthOfDuck, -1 }
{
}

// 0x692700
bool CTaskSimpleDuckWhileShotsWhizzing::MakeAbortable(CPed* ped, eAbortPriority priority, const CEvent* event) {
    if (priority != ABORT_PRIORITY_IMMEDIATE && event && event->GetEventType() == EVENT_SHOT_FIRED_WHIZZED_BY) {
        // Another shot whizzed by => keep ducking, just drop our anim references
        if (!m_DuckAnim) {
            return false;
        }
        m_DuckAnim->SetDefaultFinishCallback();
        m_DuckAnim = nullptr;
        if (m_MoveAnim) {
            m_MoveAnim->SetDefaultFinishCallback();
            m_MoveAnim = nullptr;
        }
        ped->bIsDucking = false;
        return true;
    }
    return CTaskSimpleDuck::MakeAbortable(ped, priority, event); // 0x692100
}

// 0x694640
bool CTaskSimpleDuckWhileShotsWhizzing::ProcessPed(CPed* ped) {
    if (!m_DuckAnim) {
        m_DuckAnim = CAnimManager::BlendAnimation(ped->GetRpClump(), ANIM_GROUP_DEFAULT, ANIM_ID_DUCK_COWER, 4.f);
        m_DuckAnim->SetDeleteCallback(DeleteDuckAnimCB, this); // 0x4CEBC0
    }
    return CTaskSimpleDuck::ProcessPed(ped); // 0x694390
}

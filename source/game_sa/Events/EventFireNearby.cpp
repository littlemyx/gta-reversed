#include "StdInc.h"
#include "EventFireNearby.h"

void CEventFireNearby::InjectHooks() {
    RH_ScopedVirtualClass(CEventFireNearby, 0x85B6E8, 17);
    RH_ScopedCategory("Events");

    RH_ScopedVMTDestructorInstall(0x4B6940);
    RH_ScopedVMTInstall(GetEventType, 0x4B1F50);
    RH_ScopedVMTInstall(GetEventPriority, 0x4B1F70);
    RH_ScopedVMTInstall(GetLifeTime, 0x4B1F60);
    RH_ScopedVMTInstall(AffectsPed, 0x4B1F90);
    RH_ScopedVMTInstall(TakesPriorityOver, 0x4B1FC0);
    RH_ScopedVMTInstall(CloneEditable, 0x4B7A70);
}


CEventFireNearby::CEventFireNearby(const CVector& position) : CEventEditableResponse() {
    m_position = position;
}

bool CEventFireNearby::AffectsPed(CPed* ped) {
    return !ped->GetTaskManager().Has<TASK_COMPLEX_EXTINGUISH_FIRES>() && ped->IsAlive();
}


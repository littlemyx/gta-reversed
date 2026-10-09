#include "StdInc.h"
#include "EventRevived.h"

void CEventRevived::InjectHooks() {
    RH_ScopedVirtualClass(CEventRevived, 0x85B030, 16);
    RH_ScopedCategory("Events");

    RH_ScopedVMTDestructorInstall(0x4B6080);
    RH_ScopedVMTInstall(GetEventType, 0x4AEC70);
    RH_ScopedVMTInstall(GetEventPriority, 0x4AEC90);
    RH_ScopedVMTInstall(GetLifeTime, 0x4AEC80);
    RH_ScopedVMTInstall(Clone, 0x4B71E0);
    RH_ScopedVMTInstall(AffectsPed, 0x4AECB0);
}


bool CEventRevived::AffectsPed(CPed* ped) {
    return !ped->IsCreatedByMission() && !ped->IsAlive();
}


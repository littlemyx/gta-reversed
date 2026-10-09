#include "StdInc.h"

#include "EventAcquaintancePedRespect.h"

void CEventAcquaintancePedRespect::InjectHooks() {
    RH_ScopedVirtualClass(CEventAcquaintancePedRespect, 0x86CA38, 17);
    RH_ScopedCategory("Events");

    RH_ScopedVMTInstall(GetEventType, 0x5FF040);
    RH_ScopedVMTInstall(GetEventPriority, 0x5FF8B0);
    RH_ScopedVMTInstall(CloneEditable, 0x5FF050);

    RH_ScopedVMTDestructorInstall(0x5FF0B0);
}

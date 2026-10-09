#include "StdInc.h"

#include "EventAcquaintancePedDislike.h"

void CEventAcquaintancePedDislike::InjectHooks() {
    RH_ScopedVirtualClass(CEventAcquaintancePedDislike, 0x86CAC8, 17);
    RH_ScopedCategory("Events");

    RH_ScopedVMTInstall(GetEventType, 0x5FF1C0);
    RH_ScopedVMTInstall(GetEventPriority, 0x5FF8D0);
    RH_ScopedVMTInstall(CloneEditable, 0x5FF1D0);

    RH_ScopedVMTDestructorInstall(0x5FF230);
}

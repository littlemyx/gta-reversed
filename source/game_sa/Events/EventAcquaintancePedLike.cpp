#include "StdInc.h"

#include "EventAcquaintancePedLike.h"

void CEventAcquaintancePedLike::InjectHooks() {
    RH_ScopedVirtualClass(CEventAcquaintancePedLike, 0x86CA80, 17);
    RH_ScopedCategory("Events");

    RH_ScopedVMTInstall(GetEventType, 0x5FF100);
    RH_ScopedVMTInstall(GetEventPriority, 0x5FF8C0);
    RH_ScopedVMTInstall(CloneEditable, 0x5FF110);

    RH_ScopedVMTDestructorInstall(0x5FF170);
}

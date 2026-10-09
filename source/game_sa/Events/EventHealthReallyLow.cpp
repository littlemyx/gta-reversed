#include "StdInc.h"

#include "EventHealthReallyLow.h"

void CEventHealthReallyLow::InjectHooks() {
    RH_ScopedVirtualClass(CEventHealthReallyLow, 0x86CBE8, 17);
    RH_ScopedCategory("Events");

    RH_ScopedVMTDestructorInstall(0x5FF5A0);
    RH_ScopedVMTInstall(GetEventType, 0x5FF530);
    RH_ScopedVMTInstall(GetEventPriority, 0x5FF8E0);
    RH_ScopedVMTInstall(GetLifeTime, 0x5FF540);
    RH_ScopedVMTInstall(AffectsPed, 0x4B0E80);
    RH_ScopedVMTInstall(CloneEditable, 0x5FF550);
}

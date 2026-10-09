#include "StdInc.h"

#include "TaskSimple.h"

void CTaskSimple::InjectHooks() {
    RH_ScopedVirtualClass(CTaskSimple, 0x86D4A8, 9);
    RH_ScopedCategory("Tasks");

    RH_ScopedVMTDestructorInstall(0x61A5E0);
    RH_ScopedVMTInstall(GetSubTask, 0x43E300);
    RH_ScopedVMTInstall(IsSimple, 0x43E310);
    RH_ScopedVMTInstall(SetPedPosition, 0x43E320);
}

#include "StdInc.h"

#include "TaskSimpleAbseil.h"

void CTaskSimpleAbseil::InjectHooks() {
    RH_ScopedVirtualClass(CTaskSimpleAbseil, 0x86F45C, 10);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedVMTDestructorInstall(0x659DE0);
    RH_ScopedVMTInstall(Clone, 0x6586D0);
    RH_ScopedVMTInstall(IsInterruptable, 0x658740);
}

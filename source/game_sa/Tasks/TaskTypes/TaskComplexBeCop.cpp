#include "StdInc.h"

#include "TaskComplexBeCop.h"

void CTaskComplexBeCop::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexBeCop, 0x85A428, 15);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedVMTDestructorInstall(0x4994C0);
    RH_ScopedVMTInstall(Clone, 0x499440);
    RH_ScopedVMTInstall(GetTaskType, 0x499430);
}

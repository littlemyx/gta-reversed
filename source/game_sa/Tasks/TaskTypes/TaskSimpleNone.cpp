#include "StdInc.h"

#include "TaskSimpleNone.h"

void CTaskSimpleNone::InjectHooks() {
    RH_ScopedVirtualClass(CTaskSimpleNone, 0x859CFC, 9);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedVMTDestructorInstall(0x4635F0);
    RH_ScopedVMTInstall(Clone, 0x463570);
    RH_ScopedVMTInstall(GetTaskType, 0x4635C0);
    RH_ScopedVMTInstall(MakeAbortable, 0x4635D0);
    RH_ScopedVMTInstall(ProcessPed, 0x4635E0);
}

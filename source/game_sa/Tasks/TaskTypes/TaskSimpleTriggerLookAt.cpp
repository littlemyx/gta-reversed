#include "StdInc.h"

#include "TaskSimpleTriggerLookAt.h"

void CTaskSimpleTriggerLookAt::InjectHooks() {
    RH_ScopedVirtualClass(CTaskSimpleTriggerLookAt, 0x86E3CC, 9);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedVMTDestructorInstall(0x6394D0);
}

#include "StdInc.h"

#include "TaskComplexOpenPassengerDoor.h"

void CTaskComplexOpenPassengerDoor::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexOpenPassengerDoor, 0x86EB3C, 12);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedVMTDestructorInstall(0x643CC0);
    RH_ScopedVMTInstall(Clone, 0x6438E0);
    RH_ScopedVMTInstall(GetTaskType, 0x640410);
}

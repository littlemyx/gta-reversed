#include "StdInc.h"

#include "TaskComplex.h"

void CTaskComplex::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplex, 0x86D4CC, 11);
    RH_ScopedCategory("Tasks");

    RH_ScopedVMTDestructorInstall(0x61A620);
    RH_ScopedVMTInstall(GetSubTask, 0x421190);
    RH_ScopedVMTInstall(IsSimple, 0x4211A0);
    RH_ScopedVMTInstall(MakeAbortable, 0x4211B0);
    RH_ScopedVMTInstall(SetSubTask, 0x61A430);
}

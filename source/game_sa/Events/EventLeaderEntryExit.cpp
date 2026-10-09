#include "StdInc.h"
#include "EventLeaderEntryExit.h"


void CEventLeaderEntryExit::InjectHooks()
{
    RH_ScopedVirtualClass(CEventLeaderEntryExit, 0x859540, 16);
    RH_ScopedCategory("Events");

    RH_ScopedInstall(Constructor, 0x43E1C0);

    RH_ScopedVMTDestructorInstall(0x43E290);
    RH_ScopedVMTInstall(GetEventType, 0x43E200);
    RH_ScopedVMTInstall(GetLifeTime, 0x43E210);
    RH_ScopedVMTInstall(Clone, 0x43E220);
    RH_ScopedVMTInstall(AffectsPedGroup, 0x4B23A0);
    RH_ScopedVMTInstall(TakesPriorityOver, 0x4B2390);
    RH_ScopedVMTInstall(CanBeInterruptedBySameEvent, 0x43E280);
}

// 0x43E1C0
CEventLeaderEntryExit* CEventLeaderEntryExit::Constructor(CPed* ped)
{
    this->CEventLeaderEntryExit::CEventLeaderEntryExit(ped);
    return this;
}


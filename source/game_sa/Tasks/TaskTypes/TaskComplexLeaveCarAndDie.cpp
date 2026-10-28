#include "StdInc.h"
#include "TaskComplexLeaveCarAndDie.h"

void CTaskComplexLeaveCarAndDie::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexLeaveCarAndDie, 0x86dd00, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedVMTInstall(Clone, 0x62F1D0);
    RH_ScopedVMTInstall(GetTaskType, 0x62F240);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x642280);
}

// 0x62F1A0: base ctor (veh, door, delay, sensible = false, forceGetOut = true), then the AndDie vtable and m_bDie = true
CTaskComplexLeaveCarAndDie::CTaskComplexLeaveCarAndDie(CVehicle* targetVehicle, int32 targetDoor, int32 delayTime) :
    CTaskComplexLeaveCar{targetVehicle, targetDoor, delayTime, false, true}
{
    m_bDie = true;
}

// NOTSA
CTaskComplexLeaveCarAndDie::CTaskComplexLeaveCarAndDie(const CTaskComplexLeaveCarAndDie& o) :
    CTaskComplexLeaveCar{o.m_pTargetVehicle, o.m_nTargetDoor, o.m_nDelayTime, false, true}
{
    m_bDie = true;
}

// 0x642280
CTask* CTaskComplexLeaveCarAndDie::CreateFirstSubTask(CPed* ped) {
    ped->m_nPedState = PEDSTATE_DIE; // Don't use `SetPedState` because it has side-effects
    return CTaskComplexLeaveCar::CreateFirstSubTask(ped);
}

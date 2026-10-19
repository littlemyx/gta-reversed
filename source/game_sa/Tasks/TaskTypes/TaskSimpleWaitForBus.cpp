#include "StdInc.h"
#include "TaskSimpleWaitForBus.h"

void CTaskSimpleWaitForBus::InjectHooks() {
    RH_ScopedVirtualClass(CTaskSimpleWaitForBus, 0x86e1b8, 9);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x632A50);
    RH_ScopedInstall(Destructor, 0x632A90);

    RH_ScopedVMTInstall(Clone, 0x636AD0);
    RH_ScopedVMTInstall(GetTaskType, 0x632A70);
    RH_ScopedVMTInstall(MakeAbortable, 0x632A80);
    RH_ScopedVMTInstall(ProcessPed, 0x632AA0);
}

CTaskSimpleWaitForBus::CTaskSimpleWaitForBus(const CTaskSimpleWaitForBus& o) :
    CTaskSimpleWaitForBus{}
{
}

// 0x632AA0
bool CTaskSimpleWaitForBus::ProcessPed(CPed* ped) {
    // 0x632AA0: the exe walks the 16 scanned entities and, on the first bus with a free seat that is (nearly) standing, stores it in m_bus and returns true;
    // without a match it returns false and leaves m_bus alone (the port cleared m_bus and always returned true). `range^2 > sum` is strict (fcompp / test ah,0x41 / jne);
    // the squared speed is summed in extended precision (`double` mirrors the x87 stack).
    for (auto& veh : ped->GetIntelligence()->GetVehicleScanner().GetEntities<CVehicle>()) {
        if (!veh.vehicleFlags.bIsBus) {
            continue;
        }
        const double vx = veh.m_vecMoveSpeed.x, vy = veh.m_vecMoveSpeed.y, vz = veh.m_vecMoveSpeed.z;
        const double range = 0.005f;
        if (!(range * range > vx * vx + vy * vy + vz * vz)) {
            continue;
        }
        if (!veh.HasSpaceForAPassenger()) {
            continue;
        }
        m_bus = &veh;
        return true;
    }
    return false;
}

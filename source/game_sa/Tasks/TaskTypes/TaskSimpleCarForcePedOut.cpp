#include "StdInc.h"

#include "TaskSimpleCarForcePedOut.h"
#include "CarEnterExit.h"

void CTaskSimpleCarForcePedOut::InjectHooks() {
    RH_ScopedVirtualClass(CTaskSimpleCarForcePedOut, 0x86EE94, 9);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x647710);
    RH_ScopedInstall(Destructor, 0x647790);

    RH_ScopedVMTInstall(GetTaskType, 0x647770);
    RH_ScopedVMTInstall(MakeAbortable, 0x647780);
    RH_ScopedVMTInstall(ProcessPed, 0x6477F0);
    RH_ScopedVMTInstall(Clone, 0x649EE0);
}

// 0x647710
CTaskSimpleCarForcePedOut::CTaskSimpleCarForcePedOut(CVehicle* vehicle, eTargetDoor door) :
    m_vehicle{ vehicle },
    m_door{ door }
{
    CEntity::SafeRegisterRef(m_vehicle);
}

// 0x647790
CTaskSimpleCarForcePedOut::~CTaskSimpleCarForcePedOut() {
    CEntity::SafeCleanUpRef(m_vehicle);
}

// 0x649EE0
CTask* CTaskSimpleCarForcePedOut::Clone() const {
    return new CTaskSimpleCarForcePedOut{ m_vehicle, m_door };
}

// 0x6477F0
bool CTaskSimpleCarForcePedOut::ProcessPed(CPed* ped) {
    if (!m_vehicle) {
        return true;
    }

    if (!m_vehicle->IsDriver(ped)) { // 0x6D1C40
        // If the door is in use, wait (return false) while someone is in the driver's seat
        // or in any seat in front of the one belonging to this door
        const auto oppositeDoorFlag = CCarEnterExit::ComputeOppositeDoorFlag(m_vehicle, m_door, false); // 0x64E610
        if ((uint8)oppositeDoorFlag & m_vehicle->m_nGettingOutFlags) {
            if (m_vehicle->m_pDriver) {
                return false;
            }

            const auto seatIdx = CCarEnterExit::ComputePassengerIndexFromCarDoor(m_vehicle, m_door); // 0x64F1E0
            for (int32 i = 0; i < seatIdx; i++) {
                if (m_vehicle->m_apPassengers[i]) {
                    return false;
                }
            }
        }
    }

    ped->PositionPedOutOfCollision(m_door, m_vehicle, true); // 0x5E0820
    return true;
}

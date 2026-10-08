#include "StdInc.h"

#include "TaskComplexDragPedFromCar.h"
#include "CarEnterExit.h"

void CTaskComplexDragPedFromCar__InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexDragPedFromCar, 0x86EB6C, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedVMTInstall(ControlSubTask, 0x640530);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x643D00);
}

// 0x640430
CTaskComplexDragPedFromCar::CTaskComplexDragPedFromCar(CPed* ped, int32 draggedPedDownTime) : CTaskComplexEnterCar(nullptr, false, false, true, false) {
    m_Ped = ped;
    CEntity::SafeRegisterRef(m_Ped);
    m_DraggedPedDownTime = draggedPedDownTime;
}

// 0x6404D0
CTaskComplexDragPedFromCar::~CTaskComplexDragPedFromCar() {
    CEntity::SafeCleanUpRef(m_Ped);
}

// 0x640530


CTask* CTaskComplexDragPedFromCar::ControlSubTask(CPed* ped) {
    if (m_NumGettingInSet)
        return CTaskComplexEnterCar::ControlSubTask(ped);

    if (!m_Ped || m_Ped->bInVehicle || !m_pSubTask->MakeAbortable(ped))
        return CTaskComplexEnterCar::ControlSubTask(ped);

    return CTaskComplexEnterCar::CreateSubTask(TASK_NONE, nullptr);
}

// 0x643D00
CTask* CTaskComplexDragPedFromCar::CreateFirstSubTask(CPed* ped) {
    if (m_Ped && m_Ped->m_pVehicle && m_Ped->bInVehicle) {
        if (m_Ped->m_pVehicle->IsPassenger(m_Ped) || m_Ped->m_pVehicle->IsDriver(m_Ped)) {
            // Target the vehicle the dragged ped is in
            CEntity::ChangeEntityReference(m_Car, m_Ped->m_pVehicle);

            m_bAsDriver                = m_Car->m_pDriver == m_Ped;
            m_bQuitAfterOpeningDoor    = false;
            m_bQuitAfterDraggingPedOut = true;

            if (m_Car) {
                if (   !(m_Car->m_pHandlingData->m_nModelFlags & VEHICLE_HANDLING_MODEL_TANDEM_SEATS)
                    && m_Ped->m_pVehicle->m_nVehicleType != VEHICLE_TYPE_BIKE
                    && m_Ped->m_pVehicle->m_nVehicleSubType != VEHICLE_TYPE_QUAD
                ) {
                    m_TargetDoor = CCarEnterExit::ComputeTargetDoorToExit(m_Car, m_Ped);
                } else {
                    m_TargetDoor = 0;
                }
            }
            return CTaskComplexEnterCar::CreateFirstSubTask(ped);
        }
    }
    return CreateSubTask(TASK_FINISHED, ped);
}

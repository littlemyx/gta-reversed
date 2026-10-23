#include "StdInc.h"
#include "TaskSimpleCarSetTempAction.h"

void CTaskSimpleCarSetTempAction::InjectHooks() {
    RH_ScopedVirtualClass(CTaskSimpleCarSetTempAction, 0x86ea6c, 9);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x63D6F0);
    RH_ScopedInstall(Destructor, 0x63D730);

    RH_ScopedVMTInstall(Clone, 0x63DF10);
    RH_ScopedVMTInstall(GetTaskType, 0x63D720);
    RH_ScopedVMTInstall(MakeAbortable, 0x63D740);
    RH_ScopedVMTInstall(ProcessPed, 0x645370);
}

// 0x63D6F0
CTaskSimpleCarSetTempAction::CTaskSimpleCarSetTempAction(CVehicle* veh, eAutoPilotTempAction action, uint32 timeMs) :
    CTaskSimpleCarDrive{veh},
    m_TempAct{action},
    m_DurMs{timeMs}
{
}

// NOTSA
CTaskSimpleCarSetTempAction::CTaskSimpleCarSetTempAction(const CTaskSimpleCarSetTempAction& o) :
    CTaskSimpleCarSetTempAction{ o.m_pVehicle, o.m_TempAct, o.m_DurMs }
{
}

// 0x63D740
bool CTaskSimpleCarSetTempAction::MakeAbortable(CPed* ped, eAbortPriority priority, CEvent const* event) {
    if (CTaskSimpleCarDrive::MakeAbortable(ped, priority, event)) {
        if (m_pVehicle) {
            m_pVehicle->m_autoPilot.ClearTempAct();
        }
        return true;
    }
    return false;
}

// 0x645370
bool CTaskSimpleCarSetTempAction::ProcessPed(CPed* ped) {
    // The exe re-reads m_pVehicle at every use (0x6453A1, 0x6453C4, 0x6453FF): CTaskSimpleCarDrive::ProcessPed can swap it (0x6444CC), so no pointer into the vehicle is cached
    const auto AP = [this]() -> CAutoPilot& { return m_pVehicle->m_autoPilot; };

    if (!m_pVehicle) {
        m_pVehicle = ped->m_pVehicle;
        CEntity::SafeRegisterRef(m_pVehicle);
    }

    if (CTaskSimpleCarDrive::ProcessPed(ped)) {
        if (m_pVehicle) {
            AP().SetTempAction(TEMPACT_NONE, 0);
            return true;
        }
    }

    if (m_pVehicle) {
        if (m_TempAct != TEMPACT_NONE) {
            AP().SetTempAction(m_TempAct, m_DurMs);
            m_TempAct = TEMPACT_NONE;
            return false;
        }
        if (AP().m_nTempAction == TEMPACT_NONE) {
            return true;
        }
    }

    return false;
}

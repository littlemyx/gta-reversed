#include "StdInc.h"

#include "TaskSimpleOnEscalator.h"
#include "ModelIndices.h"

void CTaskSimpleOnEscalator::InjectHooks() {
    RH_ScopedVirtualClass(CTaskSimpleOnEscalator, 0x85B8DC, 9);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedVMTDestructorInstall(0x4B8730);
    RH_ScopedVMTInstall(Clone, 0x4B86C0);
    RH_ScopedVMTInstall(GetTaskType, 0x4B8720);
    RH_ScopedVMTInstall(ProcessPed, 0x62F520);
}

// 0x62F520
bool CTaskSimpleOnEscalator::ProcessPed(CPed* ped) {
    CTaskSimpleStandStill::ProcessPed(ped); // 0x62F370 (result ignored)

    if (const auto contact = ped->m_pContactEntity) {
        if (contact->m_nModelIndex == ModelIndices::MI_ESCALATORSTEP || contact->m_nModelIndex == ModelIndices::MI_ESCALATORSTEP8) {
            const auto& vel = contact->AsPhysical()->GetMoveSpeed();
            ped->m_fCurrentRotation = CGeneral::LimitRadianAngle(CGeneral::GetRadianAngleBetweenPoints(vel.x, vel.y, 0.f, 0.f)); // 0x53CBE0, 0x53CB50
            return false;
        }
    }
    return true;
}

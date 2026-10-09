#include "StdInc.h"

#include "TaskComplexExtinguishFireOnFoot.h"
#include "TaskComplexGoToPointAndStandStill.h"
#include "TaskSimpleGunControl.h"
#include "Fire.h"
#include "FireManager.h"

void CTaskComplexExtinguishFireOnFoot::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexExtinguishFireOnFoot, 0x86F5FC, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x659870);
    RH_ScopedInstall(FindFireNearPed, 0x659990);
    RH_ScopedVMTDestructorInstall(0x65A700);
    RH_ScopedVMTInstall(Clone, 0x659D70);
    RH_ScopedVMTInstall(GetTaskType, 0x6598A0);
    RH_ScopedVMTInstall(CreateNextSubTask, 0x6598C0);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x65A720);
    RH_ScopedVMTInstall(ControlSubTask, 0x659980);
}

// 0x659870
CTaskComplexExtinguishFireOnFoot::CTaskComplexExtinguishFireOnFoot(const CVector& firePos) :
    m_FirePos{ firePos }
{
}

// 0x6598C0
CTask* CTaskComplexExtinguishFireOnFoot::CreateNextSubTask(CPed* ped) {
    switch (m_pSubTask->GetTaskType()) {
    case TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL:
        // Reached the fire => use the extinguisher
        return new CTaskSimpleGunControl{ nullptr, m_FirePos, CVector{}, eGunCommand::RELOAD, 1, -1 }; // NOTE: The exe passes `4` as the command
    case TASK_SIMPLE_GUN_CTRL:
        return CreateFirstSubTask(ped); // Virtual call (slot 9)
    default:
        return nullptr;
    }
}

// 0x65A720
CTask* CTaskComplexExtinguishFireOnFoot::CreateFirstSubTask(CPed* ped) {
    const auto fire = FindFireNearPed(ped);
    if (!fire) {
        return nullptr;
    }
    m_FirePos = fire->GetPosition();
    return new CTaskComplexGoToPointAndStandStill{ PEDMOVE_RUN, fire->GetPosition(), 2.f, 2.f, false, false };
}

// 0x659980
CTask* CTaskComplexExtinguishFireOnFoot::ControlSubTask(CPed* ped) {
    return m_pSubTask;
}

// 0x659990
CFire* CTaskComplexExtinguishFireOnFoot::FindFireNearPed(CPed* ped) {
    const auto& pedPos = ped->GetPosition();
    const auto  fire   = gFireManager.FindNearestFire(pedPos, false, true);
    if (!fire) {
        return nullptr;
    }

    // x87 evaluates this in extended precision, in this order
    const auto& firePos = fire->GetPosition();
    const double dx = (double)pedPos.x - (double)firePos.x;
    const double dy = (double)pedPos.y - (double)firePos.y;
    const double dz = (double)pedPos.z - (double)firePos.z;
    const double distSq = (dz * dz + dx * dx) + dy * dy;
    return distSq < 100.0 // 0x858628
        ? fire
        : nullptr;
}

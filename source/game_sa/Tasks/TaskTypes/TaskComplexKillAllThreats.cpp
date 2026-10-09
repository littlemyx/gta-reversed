#include "StdInc.h"

#include "TaskComplexKillAllThreats.h"
#include "TaskComplexKillPedOnFoot.h"
#include "TaskSimpleStandStill.h"
#include "Events/EventScanner.h"
#include "Acquaintance.h"

void CTaskComplexKillAllThreats::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexKillAllThreats, 0x86DA1C, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x622180);
    RH_ScopedInstall(FindThreat, 0x6221D0);
    RH_ScopedVMTDestructorInstall(0x6293A0);
    RH_ScopedVMTInstall(Clone, 0x623750);
    RH_ScopedVMTInstall(GetTaskType, 0x6221B0);
    RH_ScopedVMTInstall(CreateNextSubTask, 0x6293C0);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x629480);
    RH_ScopedVMTInstall(ControlSubTask, 0x622230);
}

// 0x622180
CTaskComplexKillAllThreats::CTaskComplexKillAllThreats(int32 pedFlags, int32 actionDelay, int32 actionChance) :
    m_PedFlags{ pedFlags },
    m_ActionDelay{ actionDelay },
    m_ActionChance{ actionChance }
{
}

// 0x6293C0
CTask* CTaskComplexKillAllThreats::CreateNextSubTask(CPed* ped) {
    switch (m_pSubTask->GetTaskType()) {
    case TASK_COMPLEX_KILL_PED_ON_FOOT: { // Killed one, find the next
        if (const auto threat = FindThreat(ped)) {
            return new CTaskComplexKillPedOnFoot{ threat, -1, m_PedFlags, m_ActionDelay, m_ActionChance, 1 };
        }
        return nullptr;
    }
    default: // Including `TASK_SIMPLE_STAND_STILL`
        return nullptr;
    }
}

// 0x629480
CTask* CTaskComplexKillAllThreats::CreateFirstSubTask(CPed* ped) {
    if (const auto threat = FindThreat(ped)) {
        return new CTaskComplexKillPedOnFoot{ threat, -1, m_PedFlags, m_ActionDelay, m_ActionChance, 1 };
    }
    return new CTaskSimpleStandStill{ 0, false, false, 8.f };
}

// 0x622230
CTask* CTaskComplexKillAllThreats::ControlSubTask(CPed* ped) {
    return m_pSubTask;
}

// 0x6221D0
CPed* CTaskComplexKillAllThreats::FindThreat(CPed* ped) {
    CPed*                   threat{};
    int32                   threatIdx{}; // Unused
    CPedAcquaintanceScanner scanner{};
    scanner.SetOnlyScriptPedAllowed(); // Just to match the state of the original's on-stack scanner (Doesn't affect `ScanForPedAcquaintances`)
    scanner.ScanForPedAcquaintances(*ped, ACQUAINTANCE_HATE, ped->GetIntelligence()->GetPedEntities(), 16, threat, threatIdx); // 0x607A90
    return threat;
}

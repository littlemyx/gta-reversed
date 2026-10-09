#include "StdInc.h"

#include "TaskSimpleWaitForPizza.h"

void CTaskSimpleWaitForPizza::InjectHooks() {
    RH_ScopedVirtualClass(CTaskSimpleWaitForPizza, 0x86E1DC, 9);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x632B30);
    RH_ScopedVMTDestructorInstall(0x6389D0);
    RH_ScopedVMTInstall(Clone, 0x636B40);
    RH_ScopedVMTInstall(GetTaskType, 0x632B50);
    RH_ScopedVMTInstall(MakeAbortable, 0x632B60);
    RH_ScopedVMTInstall(ProcessPed, 0x632B80);
}

// 0x632B60
bool CTaskSimpleWaitForPizza::MakeAbortable(CPed* ped, eAbortPriority priority, const CEvent* event) {
    return true;
}

// 0x632B80
bool CTaskSimpleWaitForPizza::ProcessPed(CPed* ped) {
    if (!m_Timer.m_bStarted) { // Start the timer (2 seconds)
        m_Timer.m_nStartTime = CTimer::GetTimeInMS();
        m_Timer.m_nInterval  = 2000;
        m_Timer.m_bStarted   = true;
    }
    if (m_Timer.m_bStopped) { // Resume
        m_Timer.m_nStartTime = CTimer::GetTimeInMS();
        m_Timer.m_bStopped   = false;
    }
    return CTimer::GetTimeInMS() >= m_Timer.m_nStartTime + m_Timer.m_nInterval; // Unsigned compare, see CTaskTimer::IsOutOfTime
}

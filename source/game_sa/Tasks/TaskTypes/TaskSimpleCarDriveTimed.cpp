#include "StdInc.h"

#include "TaskSimpleCarDriveTimed.h"

void CTaskSimpleCarDriveTimed::InjectHooks() {
    RH_ScopedVirtualClass(CTaskSimpleCarDriveTimed, 0x859E50, 9);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedVMTInstall(ProcessPed, 0x46F610);
}

CTaskSimpleCarDriveTimed* CTaskSimpleCarDriveTimed::Constructor(CVehicle* vehicle, int32 nTime) {
    this->CTaskSimpleCarDriveTimed::CTaskSimpleCarDriveTimed(vehicle, nTime);
    return this;
}

// 0x5FF940
CTaskSimpleCarDriveTimed::CTaskSimpleCarDriveTimed(CVehicle* vehicle, int32 nTime) : CTaskSimpleCarDrive(vehicle, nullptr, false), m_nTimer() {
    m_nTime = nTime;
}

// 0x46F610
bool CTaskSimpleCarDriveTimed::ProcessPed(CPed* ped) {
    // The original starts the timer unconditionally with `m_nTime` (even if it is negative), `CTaskTimer::Start` would refuse that
    m_nTimer.StartIfNotAlready(m_nTime);
    if (m_nTimer.IsOutOfTime()) {
        return true;
    }
    return CTaskSimpleCarDrive::ProcessPed(ped); // 0x644470
}

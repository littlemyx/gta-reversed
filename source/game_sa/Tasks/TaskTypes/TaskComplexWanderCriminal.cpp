#include "StdInc.h"

#include "TaskComplexWanderCriminal.h"

#include "CarEnterExit.h"
#include "EventVehicleToSteal.h"

void CTaskComplexWanderCriminal::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexWanderCriminal, 0x85A23C, 15);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(LookForCarsToSteal, 0x66B4F0);
    RH_ScopedVMTInstall(ScanForStuff, 0x670350);
}

// 0x48E610
CTaskComplexWanderCriminal::CTaskComplexWanderCriminal(eMoveState MoveState, uint8 Dir, bool bWanderSensibly) : CTaskComplexWander(MoveState, Dir, bWanderSensibly) { }

// 0x670350
void CTaskComplexWanderCriminal::ScanForStuff(CPed* ped) {
    if (!m_TaskTimer.IsStarted()) {
        m_TaskTimer.Start(50);
        // NOTE: The original multiplies by -30000.0f (0x870110) and *subtracts* the truncated result => `now + [0, 30000)`
        m_nMinNextScanTime = CTimer::GetTimeInMS() - (uint32)(int32)((double)(CGeneral::GetRandomNumber() & 0xFFFF) * (1.0 / 32768.0) * -30000.0);
    }

    if (m_TaskTimer.IsOutOfTime()) {
        m_TaskTimer.Start(50);
        if (CTimer::GetTimeInMS() >= m_nMinNextScanTime) {
            LookForCarsToSteal(ped);
        }
    }
}

// 0x66B4F0
void CTaskComplexWanderCriminal::LookForCarsToSteal(CPed* ped) {
    // Find the closest stealable vehicle
    CVehicle* closest = nullptr;
    float     bestDistSq = FLT_MAX;
    for (auto* const entity : ped->GetIntelligence()->m_vehicleScanner.m_apEntities) {
        auto* const veh = static_cast<CVehicle*>(entity);
        if (!veh || !CCarEnterExit::IsVehicleStealable(veh, ped)) {
            continue;
        }

        // Original keeps this in extended precision on the x87 stack: (dz^2 + dy^2) + dx^2
        const auto&  vehPos = veh->GetPosition();
        const auto&  pedPos = ped->GetPosition();
        const double dx     = (double)pedPos.x - (double)vehPos.x;
        const double dy     = (double)pedPos.y - (double)vehPos.y;
        const double dz     = (double)pedPos.z - (double)vehPos.z;
        const double distSq = (dz * dz + dy * dy) + dx * dx;
        if (distSq < (double)bestDistSq) {
            bestDistSq = (float)distSq;
            closest    = veh;
        }
    }

    if (!closest) {
        return;
    }

    m_nMinNextScanTime = CTimer::GetTimeInMS() + 30'000;

    if ((double)rand() * (double)RAND_MAX_FLOAT_RECIPROCAL < (double)0.2f) { // 0x858CC4
        CEventVehicleToSteal event{closest};
        ped->GetIntelligence()->m_eventGroup.Add(&event, false);
    }
}

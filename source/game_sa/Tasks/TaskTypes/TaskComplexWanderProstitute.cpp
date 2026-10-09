#include "StdInc.h"

#include "TaskComplexWanderProstitute.h"

#include "TaskComplexProstituteSolicit.h"
#include "EventAcquaintancePedRespect.h"

void CTaskComplexWanderProstitute::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexWanderProstitute, 0x870148, 15);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedVMTInstall(ScanForStuff, 0x672700);
}

// 0x672690
CTaskComplexWanderProstitute::CTaskComplexWanderProstitute(eMoveState MoveState, uint8 Dir, bool bWanderSensibly) :
    CTaskComplexWanderStandard(MoveState, Dir, bWanderSensibly),
    m_nStartTimeInMs{ 0 }
{
}

// 0x672700
void CTaskComplexWanderProstitute::ScanForStuff(CPed* ped) {
    CTaskComplexWanderStandard::ScanForStuff(ped); // 0x672600

    if (CTimer::GetTimeInMS() <= m_nStartTimeInMs) {
        return;
    }
    m_nStartTimeInMs = CTimer::GetTimeInMS() + 2000;

    CEntity** const entities = ped->GetIntelligence()->GetPedEntities();

    // Don't solicit if there's a cop around
    for (auto i = 0u; i < 16u; i++) {
        const auto entity = entities[i];
        if (entity && entity->AsPed()->m_nPedType == PED_TYPE_COP) {
            return;
        }
    }

    for (auto i = 0u; i < 16u; i++) {
        const auto other = static_cast<CPed*>(entities[i]);
        if (!other || !other->GetPlayerData() || !other->m_pVehicle) {
            continue;
        }
        if (other->GetPlayerData()->m_pCurrentProstitutePed == ped) {
            continue;
        }

        // Chance based on the vehicle's value
        const auto veh = other->m_pVehicle;
        if (!((uint32)(int32)((double)(CGeneral::GetRandomNumber() & 0xFFFF) * (1.0 / 32768.0) * 50000.0) < veh->m_pHandlingData->m_nMonetaryValue)) {
            continue;
        }

        // Vehicle must be (almost) standing still: |speed * 50|^2 < 4 (x87 extended in the original, hence `double`)
        const double sx = (double)veh->m_vecMoveSpeed.x * 50.0;
        const double sy = (double)veh->m_vecMoveSpeed.y * 50.0;
        const double sz = (double)veh->m_vecMoveSpeed.z * 50.0;
        if (!((sx * sx + sy * sy) + sz * sz < 4.0)) {
            continue;
        }

        if (!CTaskComplexProstituteSolicit::IsTaskValid(ped, other)) { // 0x661BB0
            continue;
        }

        CEventAcquaintancePedRespect event{other, TASK_COMPLEX_PROSTITUTE_SOLICIT};
        ped->GetIntelligence()->m_eventGroup.Add(&event, false);
    }
}

#include "StdInc.h"

#include "TaskSimpleCarSetPedInAsPassenger.h"
#include "TaskUtilityLineUpPedWithCar.h"
#include "TaskSimpleCarDrive.h"
#include "TaskSimpleCarSetPedOut.h"
#include "CarEnterExit.h"
#include "Crime.h"
#include "PlayerPed.h"

void CTaskSimpleCarSetPedInAsPassenger::InjectHooks() {
    RH_ScopedVirtualClass(CTaskSimpleCarSetPedInAsPassenger, 0x86EE04, 9);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedVMTInstall(ProcessPed, 0x64B5D0);
}

// OG constructor was at 0x646FE0
CTaskSimpleCarSetPedInAsPassenger::CTaskSimpleCarSetPedInAsPassenger(CVehicle* targetVehicle, eTargetDoor nTargetDoor, bool warpingInToCar, CTaskUtilityLineUpPedWithCar* utility) :
    m_nTargetDoor{ nTargetDoor },
    m_pTargetVehicle{ targetVehicle },
    m_pUtility{ utility },
    m_bWarpingInToCar{warpingInToCar}
{
    CEntity::SafeRegisterRef(m_pTargetVehicle);
}

// For 0x649D90
CTaskSimpleCarSetPedInAsPassenger::CTaskSimpleCarSetPedInAsPassenger(const CTaskSimpleCarSetPedInAsPassenger& o) :
    CTaskSimpleCarSetPedInAsPassenger{
        o.m_pTargetVehicle,
        o.m_nTargetDoor,
        o.m_bWarpingInToCar,
        o.m_pUtility
    }
{
    m_nNumGettingInToClear = o.m_nNumGettingInToClear;
}

// 0x647080
CTaskSimpleCarSetPedInAsPassenger::~CTaskSimpleCarSetPedInAsPassenger() {
    CEntity::SafeCleanUpRef(m_pTargetVehicle);
}

// 0x64B5D0
bool CTaskSimpleCarSetPedInAsPassenger::ProcessPed(CPed* ped) {
    auto* const veh = m_pTargetVehicle;

    if (ped->bInVehicle && m_Parent) {
        (void)m_Parent->GetTaskType(); // Result unused in the original (virtual call kept)
    }

    CEntity::SafeCleanUpRef(ped->m_pVehicle);
    ped->m_pVehicle = veh;
    CEntity::RegisterReference(ped->m_pVehicle); // Unconditional in the original

    ped->m_fAimingRotation = ped->m_fCurrentRotation;
    ped->bInVehicle = true;

    if (ped->IsPlayer()) {
        ped->GetPlayerData()->m_bPlayersGangActive = true; // +0x8D
        static_cast<CPlayerPed*>(ped)->ClearAdrenaline();
    }

    if (ped->IsPlayer()) {
        if (!veh->vehicleFlags.bHasBeenOwnedByPlayer) {
            veh->vehicleFlags.bHasBeenOwnedByPlayer = true;
            CCrime::ReportCrime(CRIME_CAR_STEAL, veh, FindPlayerPed());
        }
    }

    int32 seatIdx = -1;
    if (!veh->vehicleFlags.bIsBus) {
        seatIdx = CCarEnterExit::ComputePassengerIndexFromCarDoor(veh, m_nTargetDoor); // 0x64F1E0
        if (seatIdx != -1) {
            // Kick out the ped currently sitting in that seat (if any)
            if (auto* const oldPassenger = veh->m_apPassengers[seatIdx]; oldPassenger && oldPassenger != ped) {
                if (!oldPassenger->GetIntelligence()->GetTaskManager().Find<
                    TASK_COMPLEX_LEAVE_CAR,
                    TASK_COMPLEX_CAR_SLOW_BE_DRAGGED_OUT_AND_STAND_UP,
                    TASK_COMPLEX_LEAVE_CAR_AND_DIE,
                    TASK_SIMPLE_BIKE_JACKED
                >()) {
                    CTaskSimpleCarSetPedOut setPedOut{
                        veh,
                        (eTargetDoor)CCarEnterExit::ComputeTargetDoorToExit(veh, oldPassenger), // 0x64F110
                        true
                    };
                    setPedOut.m_bWarpingOutOfCar = true;
                    setPedOut.ProcessPed(oldPassenger); // 0x647D10
                }
            }
        }
    }

    if (m_bWarpingInToCar) {
        ped->GetPosition() = veh->GetPosition();
    }

    if (seatIdx != -1) {
        veh->AddPassenger(ped, (uint8)seatIdx); // 0x6D14D0
    } else {
        veh->AddPassenger(ped); // 0x6D13A0
    }

    ped->UpdateStatEnteringVehicle();
    ped->SetMoveState(PEDMOVE_NONE);
    ped->SetMoveAnim();
    ped->m_bUsesCollision = false;

    if (veh->m_nAlarmState == (uint16)-1) {
        veh->m_nAlarmState = 15'000;
    }

    ped->SetPedState(PEDSTATE_DRIVING); // Yes, driving (0x32)

    if (m_nDoorFlagsToClear) {
        veh->ClearGettingInFlags(m_nDoorFlagsToClear);
    }
    if (m_nNumGettingInToClear) {
        veh->m_nNumGettingIn -= m_nNumGettingInToClear;
    }

    ped->RemoveWeaponWhenEnteringVehicle(0);

    ped->bRenderPedInCar = !veh->vehicleFlags.bIsBus;

    CCarEnterExit::RemoveGetInAnims(ped);
    CCarEnterExit::AddInCarAnim(veh, ped, false);

    if (!m_bWarpingInToCar) {
        // BUG: `m_pUtility` is not null-checked in the original
        m_pUtility->ProcessPed(ped, veh, nullptr); // 0x6513A0
    }

    ped->GetIntelligence()->GetTaskManager().SetTask(new CTaskSimpleCarDrive{ veh, m_pUtility, false }, TASK_PRIMARY_DEFAULT);

    return true;
}

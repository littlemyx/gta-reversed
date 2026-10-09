#include "StdInc.h"

#include "TaskSimpleCarSetPedInAsDriver.h"

#include "TaskSimpleCarDrive.h"
#include "TaskSimpleCarSetPedOut.h"
#include "CarEnterExit.h"
#include "Crime.h"
#include "PedGroups.h"
#include "PlayerPed.h"
#include "EventGroupEvent.h"
#include "EventLeaderEnteredCarAsDriver.h"
#include "EventCopCarBeingStolen.h"

void CTaskSimpleCarSetPedInAsDriver::InjectHooks() {
    RH_ScopedVirtualClass(CTaskSimpleCarSetPedInAsDriver, 0x86EE28, 9);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedVMTInstall(ProcessPed, 0x64B950);
}

// 0x6470E0
CTaskSimpleCarSetPedInAsDriver::CTaskSimpleCarSetPedInAsDriver(CVehicle* targetVehicle, CTaskUtilityLineUpPedWithCar* utility) : CTaskSimple() {
    m_bIsFinished = 0;
    m_pAnim = 0;
    m_pTargetVehicle = targetVehicle;
    m_pUtility = utility;
    m_bWarpingInToCar = 0;
    m_nDoorFlagsToClear = 0;
    m_nNumGettingInToClear = 0;
    CEntity::SafeRegisterRef(m_pTargetVehicle);
}

CTaskSimpleCarSetPedInAsDriver::CTaskSimpleCarSetPedInAsDriver(CVehicle* targetVehicle, bool warpingInToCar, CTaskUtilityLineUpPedWithCar* utility) : // NOTSA
    CTaskSimpleCarSetPedInAsDriver{ targetVehicle, utility }
{
    m_bWarpingInToCar = warpingInToCar;
}

CTaskSimpleCarSetPedInAsDriver::~CTaskSimpleCarSetPedInAsDriver() {
    CEntity::SafeCleanUpRef(m_pTargetVehicle);
}

// 0x649E00
CTask* CTaskSimpleCarSetPedInAsDriver::Clone() const {
    auto task = new CTaskSimpleCarSetPedInAsDriver(m_pTargetVehicle, m_pUtility);
    task->m_bWarpingInToCar = m_bWarpingInToCar;
    task->m_nDoorFlagsToClear = m_nDoorFlagsToClear;
    task->m_nNumGettingInToClear = m_nNumGettingInToClear;
    return task;
}

// 0x64B950
bool CTaskSimpleCarSetPedInAsDriver::ProcessPed(CPed* ped) {
    auto* const veh = m_pTargetVehicle;

    CEntity::SafeCleanUpRef(ped->m_pVehicle);
    ped->m_pVehicle = veh;
    CEntity::RegisterReference(ped->m_pVehicle); // Unconditional in the original

    // Kick out the current driver (if any)
    if (auto* const oldDriver = veh->m_pDriver; oldDriver && oldDriver != ped) {
        if (!oldDriver->GetIntelligence()->GetTaskManager().Find<
            TASK_COMPLEX_LEAVE_CAR,
            TASK_COMPLEX_CAR_SLOW_BE_DRAGGED_OUT_AND_STAND_UP,
            TASK_COMPLEX_LEAVE_CAR_AND_DIE,
            TASK_SIMPLE_BIKE_JACKED
        >()) {
            CTaskSimpleCarSetPedOut setPedOut{
                veh,
                (eTargetDoor)CCarEnterExit::ComputeTargetDoorToExit(veh, oldDriver), // 0x64F110
                true
            };
            setPedOut.m_bWarpingOutOfCar = true;
            setPedOut.ProcessPed(oldDriver); // 0x647D10
        }
    }

    ped->m_fAimingRotation = ped->m_fCurrentRotation;

    if (m_bWarpingInToCar) {
        ped->GetPosition() = veh->GetPosition();
    }

    // Tell the group (if the ped is its leader) that the leader entered a car as a driver
    if (const auto group = CPedGroups::GetPedsGroup(ped)) {
        if (group->GetMembership().IsLeader(ped) && !group->GetIntelligence().GetCurrentEvent()) {
            CEventGroupEvent groupEvent{ ped, new CEventLeaderEnteredCarAsDriver{ veh } };
            group->GetIntelligence().AddEvent(&groupEvent);
        }
    }

    veh->SetDriver(ped); // 0x6D16A0
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

    ped->UpdateStatEnteringVehicle();
    ped->SetMoveState(PEDMOVE_NONE);
    ped->SetMoveAnim();
    ped->m_bUsesCollision = false;

    if (veh->m_nAlarmState == (uint16)-1) {
        veh->m_nAlarmState = 15'000;
    }

    veh->vehicleFlags.bEngineOn = !veh->vehicleFlags.bEngineBroken;

    ped->SetPedState(PEDSTATE_DRIVING);

    if (ped->IsPlayer()) {
        veh->SetStatus(STATUS_PLAYER);
    } else if (veh->GetStatus() != STATUS_SIMPLE || ped->IsCreatedBy(PED_MISSION)) {
        veh->SetStatus(veh->m_nVehicleSubType == VEHICLE_TYPE_TRAIN ? STATUS_TRAIN_NOT_MOVING : STATUS_PHYSICS);
    }

    if (m_nDoorFlagsToClear) {
        veh->ClearGettingInFlags(m_nDoorFlagsToClear);
    }
    if (m_nNumGettingInToClear) {
        veh->m_nNumGettingIn -= m_nNumGettingInToClear;
    }

    CCarEnterExit::RemoveGetInAnims(ped);
    CCarEnterExit::AddInCarAnim(veh, ped, true);

    ped->RemoveWeaponWhenEnteringVehicle(0);

    ped->bRenderPedInCar = !(veh->vehicleFlags.bIsBus && veh->m_nModelIndex != MODEL_BUS);

    ped->GetIntelligence()->GetTaskManager().SetTask(
        m_bWarpingInToCar
            ? new CTaskSimpleCarDrive{ veh, nullptr, false }
            : new CTaskSimpleCarDrive{ veh, m_pUtility, false },
        TASK_PRIMARY_DEFAULT
    );

    if (ped->IsPlayer() && veh && veh->IsLawEnforcementVehicle()) {
        CEventCopCarBeingStolen event{ ped, veh };
        GetEventGlobalGroup()->Add(&event, false);
    }

    return true;
}

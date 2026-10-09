#include "StdInc.h"

#include "TaskComplexLeaveCar.h"

#include "CarEnterExit.h"
#include "EventDamage.h"
#include "Events/Event.h"
#include "TaskSimpleCarCloseDoorFromOutside.h"
#include "TaskSimpleCarSetPedOut.h"
#include "TaskSimpleCarForcePedOut.h"
#include "TaskSimpleCarWaitForDoorNotToBeInUse.h"
#include "TaskSimpleCarWaitToSlowDown.h"
#include "TaskSimpleCarDriveTimed.h"
#include "TaskSimpleCarGetOut.h"
#include "TaskSimpleCarJumpOut.h"
#include "TaskSimpleDie.h"
#include "TaskSimplePause.h"
#include "TaskComplexLeaveBoat.h"
#include "TaskComplexGetUpAndStandStill.h"
#include "TaskComplexCarSlowBeDraggedOut.h"
#include "World.h"
#include "PedGroups.h"
#include "PedGroup.h"
#include "EventGroupEvent.h"
#include "EventLeaderExitedCarAsDriver.h"
#include "Animation/AnimManager.h"

namespace {
// 0x646CF0 - NOTSA name, can't be hooked (not a member)
// Checks whether the vehicle's door is currently being used to get in/out
void GetDoorInUseFlags(const CVehicle* veh, int32 door, bool& gettingIn, bool& gettingOut) {
    uint8 mask;
    switch (door) {
    case 8:  mask = 4; break;
    case 9:  mask = 8; break;
    case 10: mask = 1; break;
    case 11: mask = 2; break;
    default: return;
    }
    if (veh->m_nGettingInFlags & mask) {
        gettingIn = true;
    }
    if (veh->m_nGettingOutFlags & mask) {
        gettingOut = true;
    }
}
}

void CTaskComplexLeaveCar::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexLeaveCar, 0x86E828, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(SetupGettingOut, 0x63BA00);
    RH_ScopedInstall(ComputeTargetDoor, 0x63BAB0);
    RH_ScopedInstall(CreateLineUpTask, 0x63BAE0);
    RH_ScopedInstall(CreateSubTask, 0x641530);
    RH_ScopedVMTInstall(MakeAbortable, 0x641100);
    RH_ScopedVMTInstall(CreateNextSubTask, 0x6419F0);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x641FC0);
}

// 0x62F1A0
CTaskComplexLeaveCar::CTaskComplexLeaveCar(CVehicle* targetVehicle, int32 nTargetDoor, int32 nDelayTime) : CTaskComplexLeaveCar(targetVehicle, nTargetDoor, nDelayTime, false, true) {
    m_bDie = true;
}

// 0x63B8C0
CTaskComplexLeaveCar::CTaskComplexLeaveCar(CVehicle* targetVehicle, int32 nTargetDoor, int32 nDelayTime, bool bSensibleLeaveCar, bool bForceGetOut) : CTaskComplex() {
    m_nTargetDoor                  = nTargetDoor;
    m_nDelayTime                   = nDelayTime;
    m_bSensibleLeaveCar            = bSensibleLeaveCar;
    m_pTargetVehicle               = targetVehicle;
    m_bForceGetOut                 = bForceGetOut;
    m_bDie                         = false;
    m_pTaskUtilityLineUpPedWithCar = nullptr;
    m_nDoorFlagsSet                = 0;
    m_nNumGettingInSet             = 0;
    m_nDieAnimID                   = ANIM_ID_KO_SHOT_FRONT_0;
    m_fDieAnimBlendDelta           = 4.0f;
    m_fDieAnimSpeed                = 1.0f;
    m_bIsInAir                     = false;

    CEntity::SafeRegisterRef(m_pTargetVehicle);
}

// 0x63B970
CTaskComplexLeaveCar::~CTaskComplexLeaveCar() {
    if (m_pTargetVehicle) {
        m_pTargetVehicle->ClearGettingOutFlags(m_nDoorFlagsSet);
        m_pTargetVehicle->m_nNumGettingIn -= m_nNumGettingInSet;
        m_pTargetVehicle->CleanUpOldReference(reinterpret_cast<CEntity**>(&m_pTargetVehicle));
    }

    delete m_pTaskUtilityLineUpPedWithCar;
}

CTaskComplexLeaveCar::CTaskComplexLeaveCar(const CTaskComplexLeaveCar& o) :
    CTaskComplexLeaveCar{ o.m_pTargetVehicle, o.m_nTargetDoor, o.m_nDelayTime, o.m_bSensibleLeaveCar, o.m_bForceGetOut }
{
}

// 0x63BA00
void CTaskComplexLeaveCar::SetupGettingOut(CPed* ped) {
    const auto flags = (uint8)CCarEnterExit::ComputeDoorFlag(m_pTargetVehicle, m_nTargetDoor, true);
    m_nDoorFlagsSet = flags;
    m_pTargetVehicle->SetGettingOutFlags(flags);
    m_nNumGettingInSet = 1;
    m_pTargetVehicle->m_nNumGettingIn++;

    const auto veh = m_pTargetVehicle;
    if (veh->m_pDriver && !veh->m_pDriver->IsPlayer() && ped == veh->m_pDriver && m_bSensibleLeaveCar) {
        veh->m_autoPilot.m_nCruiseSpeed = 0;
        veh->m_autoPilot.m_nCarMission  = MISSION_NONE;
    }

    if (ped->IsPlayer() && ped == m_pTargetVehicle->m_pDriver) {
        m_pTargetVehicle->SetStatus(STATUS_FORCED_STOP);
    }
}

// 0x63BAB0
void CTaskComplexLeaveCar::ComputeTargetDoor(CPed* ped) {
    if (!m_nTargetDoor) {
        m_nTargetDoor = CCarEnterExit::ComputeTargetDoorToExit(m_pTargetVehicle, ped);
    }
}

// 0x63BAE0
void CTaskComplexLeaveCar::CreateLineUpTask(CPed*) {
    m_pTaskUtilityLineUpPedWithCar = new CTaskUtilityLineUpPedWithCar{ CVector{}, 0, 0, m_nTargetDoor };
}

// 0x641100
bool CTaskComplexLeaveCar::MakeAbortable(CPed* ped, eAbortPriority priority, const CEvent* event) {
    if (!m_pTargetVehicle) {
        return true;
    }

    // Undo what `SetupGettingOut` did to the vehicle
    const auto ReleaseVehicle = [this] {
        m_pTargetVehicle->ClearGettingOutFlags(m_nDoorFlagsSet);
        m_nDoorFlagsSet = 0;
        m_pTargetVehicle->m_nNumGettingIn -= m_nNumGettingInSet;
        m_nNumGettingInSet = 0;
    };

    switch (priority) {
    case ABORT_PRIORITY_IMMEDIATE: {
        m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_IMMEDIATE, event);

        if (ped->bInVehicle) {
            if (!m_nTargetDoor) {
                ComputeTargetDoor(ped);
            }

            // Close the door and set the ped out immediately
            CTaskSimpleCarCloseDoorFromOutside closeDoorTask{ m_pTargetVehicle, (uint32)m_nTargetDoor, nullptr };
            closeDoorTask.MakeAbortable(ped, ABORT_PRIORITY_IMMEDIATE, event);

            CTaskSimpleCarSetPedOut setPedOutTask{ m_pTargetVehicle, (eTargetDoor)m_nTargetDoor, m_bSensibleLeaveCar };
            setPedOutTask.ProcessPed(ped);
        }

        ReleaseVehicle();
        return true;
    }
    case ABORT_PRIORITY_URGENT: {
        const auto AbortSubTask = [&] { return m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_URGENT, event); };

        if (m_pSubTask->GetTaskType() == TASK_SIMPLE_DIE) {
            if (m_pSubTask->GetTaskType() == TASK_SIMPLE_CAR_DRIVE_TIMED) { // Dead code, but the original does it
                return false;
            }
            const auto aborted = AbortSubTask();
            if (aborted) {
                ReleaseVehicle();
            }
            return aborted;
        }

        const auto IsEventKillingDamage = [&] {
            return event
                && event->GetEventType() == EVENT_DAMAGE
                && static_cast<const CEventDamage*>(event)->HasKilledPed();
        };

        if (m_pSubTask->GetTaskType() == TASK_SIMPLE_CAR_WAIT_TO_SLOW_DOWN && IsEventKillingDamage()) {
            const auto aborted = AbortSubTask();
            if (aborted) {
                ReleaseVehicle();
            }
            return aborted;
        }

        if (m_pSubTask->GetTaskType() == TASK_COMPLEX_GET_UP_AND_STAND_STILL && IsEventKillingDamage()) {
            if (!AbortSubTask()) {
                return false;
            }
            ReleaseVehicle();
            return true;
        }

        if (m_pSubTask->GetTaskType() == TASK_SIMPLE_CAR_JUMP_OUT) {
            if (!event) {
                return false;
            }

            if (event->GetEventType() == EVENT_IN_AIR || event->GetEventType() == EVENT_IN_WATER) {
                if (event->GetEventType() == EVENT_IN_AIR) {
                    m_bIsInAir = true;
                }

                if (!AbortSubTask()) {
                    return false;
                }

                float blendDelta;
                if (ped->GetActiveWeapon().m_Type == WEAPON_PARACHUTE
                    && event->GetEventType() == EVENT_IN_AIR
                    && m_pTargetVehicle->GetNumContactWheels() == 0
                ) {
                    // x87: `Magnitude` isn't rounded to float
                    const auto& speed = m_pTargetVehicle->m_vecMoveSpeed;
                    const auto  mag   = std::sqrt(((double)speed.x * (double)speed.x + (double)speed.y * (double)speed.y) + (double)speed.z * (double)speed.z);
                    if (m_pTargetVehicle->m_nVehicleSubType != VEHICLE_TYPE_PLANE || !(mag > 0.2f)) {
                        if (!ped->IsPlayer()) {
                            return false;
                        }

                        CColPoint colPoint;
                        CEntity*  hitEntity;
                        if (CWorld::ProcessVerticalLine(ped->GetPosition(), -10.f, colPoint, hitEntity, true, false, false, false, true, false, nullptr)) {
                            return false;
                        }
                    }
                    blendDelta = 8.f;
                } else {
                    if (event->GetEventType() != EVENT_IN_WATER) {
                        return false;
                    }
                    if (!(ped->m_vecMoveSpeed.z < -0.3f)) {
                        return false;
                    }
                    blendDelta = 16.f;
                }

                CAnimManager::BlendAnimation(ped->GetRpClump(), ANIM_GROUP_DEFAULT, ANIM_ID_FALL_FRONT, blendDelta);
                return false;
            }
        }

        if (!event) {
            return false;
        }
        if (m_pSubTask->GetTaskType() != TASK_SIMPLE_CAR_DRIVE_TIMED) {
            return false;
        }
        return AbortSubTask();
    }
    default:
        return false;
    }
}

// 0x6419F0
CTask* CTaskComplexLeaveCar::CreateNextSubTask(CPed* ped) {
    switch (m_pSubTask->GetTaskType()) {
    case TASK_SIMPLE_PAUSE:
    case TASK_SIMPLE_DIE:
        return CreateSubTask(TASK_FINISHED, ped);
    case TASK_COMPLEX_LEAVE_BOAT:
        return CreateSubTask(m_bDie ? TASK_SIMPLE_DIE : TASK_FINISHED, ped);
    case TASK_COMPLEX_GET_UP_AND_STAND_STILL:
        return CreateSubTask(ped->bInVehicle ? TASK_SIMPLE_CAR_SET_PED_OUT : TASK_FINISHED, ped);
    case TASK_SIMPLE_CAR_CLOSE_DOOR_FROM_OUTSIDE:
        m_nDieAnimID = ANIM_ID_KO_SHOT_FRONT_0;
        return CreateSubTask(TASK_SIMPLE_CAR_SET_PED_OUT, ped);
    case TASK_SIMPLE_CAR_DRIVE_TIMED: {
        if (!ped->bInVehicle) {
            return CreateSubTask(TASK_FINISHED, ped);
        }

        if (!ped->m_pVehicle->IsPassenger(ped) && !ped->m_pVehicle->IsDriver(ped)) {
            ped->bInVehicle = false;
            return CreateSubTask(TASK_FINISHED, ped);
        }

        ComputeTargetDoor(ped);

        bool gettingIn = false, gettingOut = false;
        GetDoorInUseFlags(m_pTargetVehicle, m_nTargetDoor, gettingIn, gettingOut); // 0x646CF0

        if (!m_bForceGetOut) {
            if (gettingOut) {
                return CreateSubTask(TASK_SIMPLE_CAR_WAIT_FOR_DOOR_NOT_TO_BE_IN_USE, ped);
            }
            if (gettingIn) {
                return CreateSubTask(TASK_FINISHED, ped);
            }
        }
        return CreateSubTask(TASK_SIMPLE_CAR_WAIT_TO_SLOW_DOWN, ped);
    }
    case TASK_SIMPLE_CAR_WAIT_FOR_DOOR_NOT_TO_BE_IN_USE:
        return CreateSubTask(TASK_SIMPLE_CAR_WAIT_TO_SLOW_DOWN, ped);
    case TASK_SIMPLE_CAR_WAIT_TO_SLOW_DOWN: {
        const auto veh = m_pTargetVehicle;

        // Jump out if the ped can't just step out
        if (!veh->CanPedStepOutCar(false)) {
            SetupGettingOut(ped);
            ped->SetPedState(PEDSTATE_NONE);
            return CreateSubTask(TASK_SIMPLE_CAR_JUMP_OUT, ped);
        }

        // The ped is forced out if there's no way to exit the vehicle
        const auto ForcePedOut = [&] {
            SetupGettingOut(ped);
            ped->SetPedState(PEDSTATE_NONE);
            CreateLineUpTask(ped);
            return CreateSubTask(TASK_SIMPLE_CAR_FORCE_PED_OUT, ped);
        };

        if (!CCarEnterExit::IsRoomForPedToLeaveCar(veh, m_nTargetDoor, nullptr)) {
            // Try the door on the other side
            int32 altDoor = 0;
            switch (m_nTargetDoor) {
            case 8:
                if (veh->m_pDriver || (veh->m_nGettingInFlags & 1)) {
                    return ForcePedOut();
                }
                altDoor = 10;
                break;
            case 9:
                if (veh->m_apPassengers[1] || (veh->m_nGettingInFlags & 2)) {
                    return ForcePedOut();
                }
                altDoor = 11;
                break;
            case 10:
                if (veh->m_nVehicleType != VEHICLE_TYPE_BIKE && !veh->m_pHandlingData->m_bTandemSeats) {
                    if (veh->m_apPassengers[0] || (veh->m_nGettingInFlags & 4)) {
                        return ForcePedOut();
                    }
                }
                altDoor = 8;
                break;
            case 11:
                altDoor = 9;
                if (veh->m_nVehicleType != VEHICLE_TYPE_BIKE && !veh->m_pHandlingData->m_bTandemSeats) {
                    if (veh->m_apPassengers[2] || (veh->m_nGettingInFlags & 8)) {
                        return ForcePedOut();
                    }
                }
                break;
            }

            if (!CCarEnterExit::IsRoomForPedToLeaveCar(veh, altDoor, nullptr)) {
                return ForcePedOut();
            }

            if (veh->m_pHandlingData->m_bForceDoorCheck
                && veh->m_nVehicleType == VEHICLE_TYPE_AUTOMOBILE
                && !veh->AsAutomobile()->m_aCarNodes[altDoor]
            ) {
                return ForcePedOut();
            }

            m_nTargetDoor = altDoor;
        }

        SetupGettingOut(ped);
        ped->SetPedState(PEDSTATE_NONE);

        if (m_bDie) {
            return CreateSubTask(TASK_COMPLEX_CAR_SLOW_BE_DRAGGED_OUT, ped);
        }

        CreateLineUpTask(ped);
        return CreateSubTask(TASK_SIMPLE_CAR_GET_OUT, ped);
    }
    case TASK_SIMPLE_CAR_GET_OUT: {
        m_pTaskUtilityLineUpPedWithCar->m_nDoorOpenPosType = 2;

        if (!m_bForceGetOut || m_pTargetVehicle->m_nDoorLock != CARLOCK_UNLOCKED) {
            if (CCarEnterExit::CarHasDoorToClose(m_pTargetVehicle, m_nTargetDoor)) {
                return CreateSubTask(TASK_SIMPLE_CAR_CLOSE_DOOR_FROM_OUTSIDE, ped);
            }
        }

        // Bash the door if it was left open
        if (m_pTargetVehicle->m_nVehicleType == VEHICLE_TYPE_AUTOMOBILE && CCarEnterExit::CarHasDoorToClose(m_pTargetVehicle, m_nTargetDoor)) {
            auto& dmgMgr = m_pTargetVehicle->AsAutomobile()->m_damageManager;
            const auto door = (tComponent)m_nTargetDoor;
            if (dmgMgr.GetDoorStatus_Component(door) == DAMSTATE_OK || dmgMgr.GetDoorStatus_Component(door) == DAMSTATE_DAMAGED) {
                dmgMgr.SetDoorStatus_Component(door, (eDoorStatus)(dmgMgr.GetDoorStatus_Component(door) + 1));
            }
        }
        return CreateSubTask(TASK_SIMPLE_CAR_SET_PED_OUT, ped);
    }
    case TASK_SIMPLE_CAR_JUMP_OUT: {
        if (m_bDie) {
            m_nDieAnimID         = ANIM_ID_KO_SHOT_FRONT_0;
            m_fDieAnimBlendDelta = 1000.f;
            m_fDieAnimSpeed      = 0.5f;
            return CreateSubTask(TASK_SIMPLE_DIE, ped);
        }

        const auto veh = m_pTargetVehicle;
        if (!veh || veh->m_nVehicleType == VEHICLE_TYPE_BIKE || veh->m_nVehicleSubType == VEHICLE_TYPE_QUAD) {
            return CreateSubTask(TASK_FINISHED, ped);
        }

        if (m_bIsInAir) {
            return CreateSubTask(TASK_SIMPLE_CAR_SET_PED_OUT, ped);
        }

        if (ped->bIsDrowning) {
            return CreateSubTask(TASK_SIMPLE_CAR_SET_PED_OUT, ped);
        }

        veh->ClearGettingOutFlags(m_nDoorFlagsSet);
        m_nDoorFlagsSet = 0;
        veh->m_nNumGettingIn -= m_nNumGettingInSet;
        m_nNumGettingInSet = 0;
        return CreateSubTask(TASK_COMPLEX_GET_UP_AND_STAND_STILL, ped);
    }
    case TASK_SIMPLE_CAR_FORCE_PED_OUT:
        return CreateSubTask(TASK_SIMPLE_CAR_SET_PED_OUT, ped);
    case TASK_SIMPLE_CAR_SET_PED_OUT:
        return CreateSubTask(m_bDie ? TASK_SIMPLE_DIE : TASK_FINISHED, ped);
    case TASK_COMPLEX_CAR_SLOW_BE_DRAGGED_OUT: {
        m_nDieAnimID         = ANIM_ID_FLOOR_HIT;
        m_fDieAnimBlendDelta = 1000.f;
        m_fDieAnimSpeed      = 0.5f;
        if (m_pTargetVehicle && m_pTargetVehicle->m_nDoorLock == CARLOCK_COP_CAR) {
            m_pTargetVehicle->m_nDoorLock = CARLOCK_UNLOCKED;
        }
        return CreateSubTask(TASK_SIMPLE_DIE, ped);
    }
    default:
        return nullptr;
    }
}

// 0x641FC0
CTask* CTaskComplexLeaveCar::CreateFirstSubTask(CPed* ped) {
    if ((ped->m_nPedState == PEDSTATE_ARRESTED || ped->bIsBeingArrested) && ped->IsPlayer()) {
        return new CTaskSimplePause{ -1 };
    }

    if (!ped->bInVehicle) {
        NOTSA_LOG_DEBUG("CTaskComplexLeaveCar - ped not in car"); // vanilla (printf)
        return new CTaskSimplePause{ -1 };
    }

    if (m_pTargetVehicle->m_pDriver == ped) {
        // Tell the group that the leader is leaving the vehicle
        if (const auto grp = CPedGroups::GetPedsGroup(ped); grp && grp->GetMembership().IsLeader(ped)) {
            CEventGroupEvent event{ ped, new CEventLeaderExitedCarAsDriver{} };
            grp->GetIntelligence().AddEvent(&event);
        }

        if (!ped->IsPlayer()) {
            ped->SetRadioStation();
        } else if (ped->m_pVehicle) {
            ped->m_pVehicle->m_vehicleAudio.PlayerAboutToExitVehicleAsDriver();
        }
    }

    if (m_pTargetVehicle->m_nVehicleType == VEHICLE_TYPE_BOAT) {
        return new CTaskComplexLeaveBoat{ m_pTargetVehicle, 0 };
    }

    return new CTaskSimpleCarDriveTimed{ m_pTargetVehicle, m_nDelayTime };
}

// 0x6421B0
CTask* CTaskComplexLeaveCar::ControlSubTask(CPed* ped) {
    if (!m_pTargetVehicle) {
        return nullptr;
    }

    const auto subTaskType = m_pSubTask->GetTaskType();
    if (!ped->bInVehicle
        && subTaskType != TASK_SIMPLE_CAR_SET_PED_OUT
        && subTaskType != TASK_SIMPLE_CAR_JUMP_OUT
        && subTaskType != TASK_COMPLEX_GET_UP_AND_STAND_STILL
        && subTaskType != TASK_SIMPLE_DIE
    ) {
        return CreateSubTask(TASK_SIMPLE_CAR_SET_PED_OUT, ped);
    }

    if (!m_bSensibleLeaveCar && subTaskType == TASK_SIMPLE_CAR_WAIT_TO_SLOW_DOWN) {
        m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_LEISURE, nullptr);
    }

    return m_pSubTask;
}

// 0x641530
CTask* CTaskComplexLeaveCar::CreateSubTask(eTaskType taskType, CPed* ped) {
    switch (taskType) {
    case TASK_SIMPLE_CAR_WAIT_FOR_DOOR_NOT_TO_BE_IN_USE:
        return new CTaskSimpleCarWaitForDoorNotToBeInUse{ m_pTargetVehicle, (uint32)m_nTargetDoor, 0 };
    case TASK_COMPLEX_LEAVE_BOAT:
        return new CTaskComplexLeaveBoat{ m_pTargetVehicle, 0 };
    case TASK_SIMPLE_PAUSE:
        return new CTaskSimplePause{ -1 };
    case TASK_COMPLEX_GET_UP_AND_STAND_STILL:
        return new CTaskComplexGetUpAndStandStill{};
    case TASK_SIMPLE_DIE:
        return new CTaskSimpleDie{ ANIM_GROUP_DEFAULT, (AnimationId)m_nDieAnimID, m_fDieAnimBlendDelta, m_fDieAnimSpeed };
    case TASK_SIMPLE_CAR_DRIVE_TIMED:
        return new CTaskSimpleCarDriveTimed{ m_pTargetVehicle, m_nDelayTime };
    case TASK_SIMPLE_CAR_CLOSE_DOOR_FROM_OUTSIDE:
        return new CTaskSimpleCarCloseDoorFromOutside{ m_pTargetVehicle, (uint32)m_nTargetDoor, m_pTaskUtilityLineUpPedWithCar };
    case TASK_SIMPLE_CAR_WAIT_TO_SLOW_DOWN: {
        using SlowDownType = CTaskSimpleCarWaitToSlowDown::SlowDownType;
        if (m_bDie || !m_bSensibleLeaveCar) {
            return new CTaskSimpleCarWaitToSlowDown{ m_pTargetVehicle, SlowDownType::DONT_WAIT };
        }
        return new CTaskSimpleCarWaitToSlowDown{ m_pTargetVehicle, ped->IsPlayer() ? SlowDownType::SLOW_ENOUGH_TO_STEP_OR_JUMP : SlowDownType::SLOW_ENOUGH_TO_STEP };
    }
    case TASK_SIMPLE_CAR_SET_PED_OUT:
        return new CTaskSimpleCarSetPedOut{ m_pTargetVehicle, (eTargetDoor)m_nTargetDoor, m_bSensibleLeaveCar };
    case TASK_SIMPLE_CAR_GET_OUT:
        return new CTaskSimpleCarGetOut{ m_pTargetVehicle, (uint32)m_nTargetDoor, m_pTaskUtilityLineUpPedWithCar };
    case TASK_SIMPLE_CAR_JUMP_OUT:
        return new CTaskSimpleCarJumpOut{ m_pTargetVehicle, (uint32)m_nTargetDoor, m_pTaskUtilityLineUpPedWithCar };
    case TASK_SIMPLE_CAR_FORCE_PED_OUT:
        return new CTaskSimpleCarForcePedOut{ m_pTargetVehicle, (eTargetDoor)m_nTargetDoor }; // 0x647710
    case TASK_COMPLEX_CAR_SLOW_BE_DRAGGED_OUT:
        return new CTaskComplexCarSlowBeDraggedOut{ m_pTargetVehicle, (eTargetDoor)m_nTargetDoor, true };
    default:
        return nullptr;
    }
}

#include "StdInc.h"

#include "TaskComplexLeaveCar.h"

#include "CarEnterExit.h"
#include "EventDamage.h"
#include "Events/Event.h"
#include "TaskSimpleCarCloseDoorFromOutside.h"
#include "TaskSimpleCarSetPedOut.h"
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
#include "Animation/AnimManager.h"

void CTaskComplexLeaveCar::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexLeaveCar, 0x86E828, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(ComputeTargetDoor, 0x63BAB0);
    RH_ScopedInstall(CreateSubTask, 0x641530);
    RH_ScopedVMTInstall(MakeAbortable, 0x641100);
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

// 0x63BAB0
void CTaskComplexLeaveCar::ComputeTargetDoor(CPed* ped) {
    if (!m_nTargetDoor) {
        m_nTargetDoor = CCarEnterExit::ComputeTargetDoorToExit(m_pTargetVehicle, ped);
    }
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
    return plugin::CallMethodAndReturn<CTask*, 0x6419F0, CTask*, CPed*>(this, ped);
}

// 0x641FC0
CTask* CTaskComplexLeaveCar::CreateFirstSubTask(CPed* ped) {
    return plugin::CallMethodAndReturn<CTask*, 0x641FC0, CTask*, CPed*>(this, ped);
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
    case TASK_SIMPLE_CAR_FORCE_PED_OUT: {
        // TODO: `CTaskSimpleCarForcePedOut` isn't reversed yet, so allocate it manually (size 0x10) and call its constructor
        const auto mem = static_cast<CTask*>(CTask::operator new(0x10));
        if (!mem) {
            return nullptr;
        }
        return plugin::CallMethodAndReturn<CTask*, 0x647710, CTask*, CVehicle*, int32>(mem, m_pTargetVehicle, m_nTargetDoor);
    }
    case TASK_COMPLEX_CAR_SLOW_BE_DRAGGED_OUT:
        return new CTaskComplexCarSlowBeDraggedOut{ m_pTargetVehicle, (eTargetDoor)m_nTargetDoor, true };
    default:
        return nullptr;
    }
}

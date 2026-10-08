#include "StdInc.h"

#include "TaskComplexCarSlowBeDraggedOutAndStandUp.h"

#include "TaskComplexCarSlowBeDraggedOut.h"
#include "TaskComplexGetUpAndStandStill.h"
#include "TaskComplexLeaveCar.h"
#include "TaskSimpleCarSetPedOut.h"
#include "EventDeath.h"

void CTaskComplexCarSlowBeDraggedOutAndStandUp::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexCarSlowBeDraggedOutAndStandUp, 0x86EF80, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x648620);
    RH_ScopedInstall(Destructor, 0x648690);
    RH_ScopedInstall(CreateSubTask, 0x648710);
    RH_ScopedVMTInstall(Clone, 0x64A190);
    RH_ScopedVMTInstall(GetTaskType, 0x648680);
    RH_ScopedVMTInstall(MakeAbortable, 0x6486F0);
    RH_ScopedVMTInstall(CreateNextSubTask, 0x6488F0);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x648A10);
    RH_ScopedVMTInstall(ControlSubTask, 0x648B80);
}

// 0x648620
CTaskComplexCarSlowBeDraggedOutAndStandUp::CTaskComplexCarSlowBeDraggedOutAndStandUp(CVehicle* vehicle, int32 door) : CTaskComplex() {
    m_Vehicle = vehicle;
    m_Door = (eTargetDoor)door;
    CEntity::SafeRegisterRef(m_Vehicle);
}

// 0x648690
CTaskComplexCarSlowBeDraggedOutAndStandUp::~CTaskComplexCarSlowBeDraggedOutAndStandUp() {
    CEntity::SafeCleanUpRef(m_Vehicle);
}

// 0x648710
CTask* CTaskComplexCarSlowBeDraggedOutAndStandUp::CreateSubTask(eTaskType taskType, CPed* ped) {
    switch (taskType) {
    case TASK_SIMPLE_CAR_SET_PED_OUT:
        return new CTaskSimpleCarSetPedOut{ped->m_pVehicle, m_Door, true};
    case TASK_COMPLEX_GET_UP_AND_STAND_STILL:
        return new CTaskComplexGetUpAndStandStill{};
    case TASK_COMPLEX_LEAVE_CAR: {
        const auto animGroup = m_Vehicle->m_pHandlingData->GetAnimGroupId();
        if (animGroup == ANIM_GROUP_COACHCARANIMS) {
            return new CTaskComplexLeaveCar{ped->m_pVehicle, TARGET_DOOR_FRONT_RIGHT, 0, false, true};
        }
        if (animGroup == ANIM_GROUP_TANKCARANIMS) {
            return new CTaskComplexLeaveCar{ped->m_pVehicle, TARGET_DOOR_DRIVER, 0, false, true};
        }
        if (m_Door == TARGET_DOOR_FRONT_RIGHT && animGroup == ANIM_GROUP_BUSCARANIMS) {
            return new CTaskComplexLeaveCar{ped->m_pVehicle, TARGET_DOOR_FRONT_RIGHT, 0, false, true};
        }
        return nullptr;
    }
    case TASK_COMPLEX_CAR_SLOW_BE_DRAGGED_OUT:
        return new CTaskComplexCarSlowBeDraggedOut{m_Vehicle, m_Door, false};
    default: // Includes `TASK_FINISHED`
        return nullptr;
    }
}

// 0x6486F0
bool CTaskComplexCarSlowBeDraggedOutAndStandUp::MakeAbortable(CPed* ped, eAbortPriority priority, const CEvent* event) {
    if (priority != ABORT_PRIORITY_IMMEDIATE) {
        return false;
    }
    return m_pSubTask->MakeAbortable(ped, priority, event);
}

// 0x6488F0
CTask* CTaskComplexCarSlowBeDraggedOutAndStandUp::CreateNextSubTask(CPed* ped) {
    switch (m_pSubTask->GetTaskType()) {
    case TASK_SIMPLE_CAR_SET_PED_OUT:
    case TASK_COMPLEX_LEAVE_CAR:
        return nullptr;
    case TASK_COMPLEX_GET_UP_AND_STAND_STILL:
        // NOTE: The original calls `ped->IsPlayer()` here, but ignores the result
        return nullptr;
    case TASK_COMPLEX_CAR_SLOW_BE_DRAGGED_OUT: {
        if (ped->IsAlive() && ped->m_fHealth > 0.f) {
            return new CTaskComplexGetUpAndStandStill{};
        }
        ped->GetEventGroup().Add(CEventDeath{false});
        return nullptr;
    }
    default:
        return nullptr;
    }
}

// 0x648A10
CTask* CTaskComplexCarSlowBeDraggedOutAndStandUp::CreateFirstSubTask(CPed* ped) {
    if (ped == FindPlayerPed(0)) {
        if (ped->m_pVehicle) {
            ped->m_pVehicle->m_vehicleAudio.PlayerAboutToExitVehicleAsDriver();
        }
    } else if (!notsa::IsFixBugs() || ped->m_pVehicle) { // BUG: `ped->m_pVehicle` might be null here
        if (ped->m_pVehicle->m_pDriver == ped) {
            ped->SetRadioStation();
        }
    }

    const auto veh = ped->m_pVehicle;
    if (veh) {
        const auto animGroup = veh->m_pHandlingData->GetAnimGroupId();
        if (animGroup == ANIM_GROUP_COACHCARANIMS
            || animGroup == ANIM_GROUP_TANKCARANIMS
            || (m_Door == TARGET_DOOR_FRONT_RIGHT && animGroup == ANIM_GROUP_BUSCARANIMS)
        ) {
            return CreateSubTask(TASK_COMPLEX_LEAVE_CAR, ped);
        }
    }

    // NOTE: Switch on the `tHandlingData::m_nAnimGroup` (relative to `ANIM_GROUP_STDCARAMIMS`) [0x648AC1]
    // BUG: `veh` might be null here (the original crashes in that case too)
    switch (veh->m_pHandlingData->m_nAnimGroup) {
    case 0:
    case 13:
    case 14:
    case 18:
    case 19:
    case 22:
    case 23:
    case 28:
        if (m_Door == TARGET_DOOR_DRIVER) {
            ped->GetAE().AddAudioEvent(AE_PED_JACKED_CAR_PUNCH, 0.f, 1.f);
        } else if (m_Door == TARGET_DOOR_FRONT_RIGHT) {
            ped->GetAE().AddAudioEvent(AE_PED_JACKED_CAR_HEAD_BANG, 0.f, 1.f);
        }
        break;
    case 1:
        ped->GetAE().AddAudioEvent(AE_PED_JACKED_CAR_KICK, 0.f, 1.f);
        break;
    case 17:
        ped->GetAE().AddAudioEvent(AE_PED_JACKED_DOZER, 0.f, 1.f);
        break;
    default:
        break;
    }

    return CreateSubTask(TASK_COMPLEX_CAR_SLOW_BE_DRAGGED_OUT, ped);
}

// 0x648B80
CTask* CTaskComplexCarSlowBeDraggedOutAndStandUp::ControlSubTask(CPed* ped) {
    return m_pSubTask;
}

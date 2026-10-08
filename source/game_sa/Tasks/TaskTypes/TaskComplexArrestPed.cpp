#include "StdInc.h"

#include "extensions/utility.hpp"

#include "TaskComplexArrestPed.h"
#include "TaskComplexFallAndGetUp.h"
#include "TaskComplexEnterCar.h"
#include "TaskSimpleWaitUntilPedIsOutCar.h"
#include "TaskSimpleArrestPed.h"
#include "TaskComplexKillPedOnFoot.h"
#include "TaskComplexDestroyCar.h"
#include "SeekEntity/PosCalculators/EntitySeekPosCalculatorStandard.h"
#include "SeekEntity/TaskComplexSeekEntity.h"
#include "TaskComplexDragPedFromCar.h"
#include "TaskComplexOpenDriverDoor.h"
#include "TaskComplexOpenPassengerDoor.h"

#include "eTargetDoor.h"

void CTaskComplexArrestPed::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexArrestPed, 0x8709A8, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x68B990);
    RH_ScopedInstall(Destructor, 0x68BA00);
    RH_ScopedVMTInstall(MakeAbortable, 0x68BA60);
    RH_ScopedVMTInstall(CreateNextSubTask, 0x690220);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x6907A0);
    RH_ScopedVMTInstall(ControlSubTask, 0x68D350);
    RH_ScopedInstall(CreateSubTask, 0x68CF80);
}

// 0x68B990
CTaskComplexArrestPed::CTaskComplexArrestPed(CPed* ped) : CTaskComplex() {
    m_PedToArrest = ped;
    m_Vehicle = nullptr;
    CEntity::SafeRegisterRef(m_PedToArrest);
}

// 0x68BA00
CTaskComplexArrestPed::~CTaskComplexArrestPed() {
    CEntity::SafeCleanUpRef(m_PedToArrest);
}

// 0x68BA60


bool CTaskComplexArrestPed::MakeAbortable(CPed* ped, eAbortPriority priority, const CEvent* event) {
    return m_pSubTask->MakeAbortable(ped, priority, event);
}

namespace {
bool IsEnterCarQuitAfterOpeningDoor(const CTask* task) {
    return static_cast<const CTaskComplexEnterCar*>(task)->IsQuitAfterOpeningDoor();
}

bool IsEnterCarQuitAfterDraggingPedOut(const CTask* task) {
    return static_cast<const CTaskComplexEnterCar*>(task)->IsQuitAfterDraggingPedOut();
}

// NOTSA: Only `CTaskComplexSeekEntity<CEntitySeekPosCalculatorStandard>` is created by this task
bool HasSeekEntityAchievedEntity(const CTask* task) {
    return static_cast<const CTaskComplexSeekEntity<CEntitySeekPosCalculatorStandard>*>(task)->HasAchievedSeekEntity();
}

// Is `target` close enough (on the XY plane, and the Z axis) to `ped`, to be arrested
bool IsPedCloseEnoughToArrest(const CPed* target, const CPed* ped) {
    auto diff = target->GetPosition() - ped->GetPosition();
    if (std::abs(diff.z) > 2.f) {
        return false;
    }
    diff.z = 0.f;
    return diff.SquaredMagnitude() <= sq(3.f);
}
}

// 0x690220
CTask* CTaskComplexArrestPed::CreateNextSubTask(CPed* ped) {
    if (!m_PedToArrest) {
        return CreateSubTask(TASK_FINISHED, ped);
    }

    if (m_PedToArrest->bIsBeingArrested && m_pSubTask->GetTaskType() != TASK_SIMPLE_ARREST_PED) {
        if (m_pSubTask->GetTaskType() == TASK_COMPLEX_SEEK_ENTITY && HasSeekEntityAchievedEntity(m_pSubTask)) {
            return CreateSubTask(TASK_SIMPLE_ARREST_PED, ped);
        }
        return CreateSubTask(TASK_COMPLEX_SEEK_ENTITY, ped);
    }

    switch (m_pSubTask->GetTaskType()) {
    case TASK_COMPLEX_SEEK_ENTITY: {
        if (const auto task = static_cast<CTaskComplexFallAndGetUp*>(m_PedToArrest->GetTaskManager().FindActiveTaskByType(TASK_COMPLEX_FALL_AND_GET_UP))) {
            if (task->IsFalling() && HasSeekEntityAchievedEntity(m_pSubTask)) {
                if (IsPedCloseEnoughToArrest(m_PedToArrest, ped)) {
                    task->SetDownTime(100'000);
                    return CreateSubTask(TASK_SIMPLE_ARREST_PED, ped);
                }
            }
        }
        break;
    }
    case TASK_COMPLEX_DRAG_PED_FROM_CAR: {
        if (const auto task = static_cast<CTaskComplexFallAndGetUp*>(m_PedToArrest->GetTaskManager().FindActiveTaskByType(TASK_COMPLEX_FALL_AND_GET_UP))) {
            if (task->IsFalling() && !IsEnterCarQuitAfterDraggingPedOut(m_pSubTask)) {
                if (IsPedCloseEnoughToArrest(m_PedToArrest, ped)) {
                    task->SetDownTime(100'000);
                    return CreateSubTask(TASK_SIMPLE_ARREST_PED, ped);
                }
            }
        }
        break;
    }
    case TASK_COMPLEX_CAR_OPEN_DRIVER_DOOR:
    case TASK_COMPLEX_CAR_OPEN_PASSENGER_DOOR: { // Both are identical
        if (IsEnterCarQuitAfterOpeningDoor(m_pSubTask) && m_PedToArrest->m_pVehicle && !m_PedToArrest->m_pVehicle->CanPedOpenLocks(ped)) {
            m_Vehicle = m_PedToArrest->m_pVehicle;
        }
        if (!m_PedToArrest->IsAlive()) {
            return CreateSubTask(TASK_SIMPLE_ARREST_PED, ped);
        }
        if (m_PedToArrest->bInVehicle && !IsEnterCarQuitAfterOpeningDoor(m_pSubTask)) {
            if (m_PedToArrest->GetTaskManager().FindActiveTaskByType(TASK_COMPLEX_LEAVE_CAR)) {
                return CreateSubTask(TASK_COMPLEX_KILL_PED_ON_FOOT, ped);
            }
            return CreateSubTask(TASK_SIMPLE_ARREST_PED, ped);
        }
        break;
    }
    case TASK_COMPLEX_KILL_PED_ON_FOOT: {
        if (m_PedToArrest->m_fHealth <= 0.f) {
            return CreateSubTask(TASK_SIMPLE_ARREST_PED, ped);
        }
        if (const auto task = static_cast<CTaskComplexFallAndGetUp*>(m_PedToArrest->GetTaskManager().FindActiveTaskByType(TASK_COMPLEX_FALL_AND_GET_UP)); task && task->IsFalling()) {
            if (!IsPedCloseEnoughToArrest(m_PedToArrest, ped)) {
                return CreateSubTask(TASK_COMPLEX_SEEK_ENTITY, ped);
            }
            task->SetDownTime(100'000);
            return CreateSubTask(TASK_SIMPLE_ARREST_PED, ped);
        }
        if (ped->m_nPedType != PED_TYPE_COP && m_PedToArrest->IsPlayer()) {
            // BUG: `GetPlayerWanted()` is null if the player data is missing, the original dereferences it anyway
            if (const auto wanted = m_PedToArrest->GetPlayerWanted(); (!notsa::IsFixBugs() || wanted) && wanted->m_NumCopsInPursuit > 0) {
                return CreateSubTask(TASK_FINISHED, ped);
            }
        }
        break;
    }
    case TASK_COMPLEX_DESTROY_CAR:
        return CreateSubTask(TASK_FINISHED, ped);
    case TASK_SIMPLE_ARREST_PED:
        return CreateSubTask(TASK_FINISHED, ped);
    case TASK_SIMPLE_WAIT_UNTIL_PED_OUT_CAR:
        break; // Falls to the bottom
    default:
        return nullptr;
    }
    return CreateSubTask(TASK_COMPLEX_KILL_PED_ON_FOOT, ped);
}

// NOTSA
void MakeSurePedHasWeaponInHand(CPed* ped) {
    // Make sure ped has an actual weapon in their hand
    if (!ped->GetActiveWeapon().IsTypeMelee())
        return;

    if (ped->DoWeHaveWeaponAvailable(WEAPON_SHOTGUN)) { // Use shotgun (if available)
        ped->SetCurrentWeapon(WEAPON_SHOTGUN);
        return;
    }

    // Otherwise a pistol
    if (!ped->DoWeHaveWeaponAvailable(WEAPON_PISTOL)) { // Make sure they have one
        ped->GiveWeapon(WEAPON_PISTOL, 10, false);
    }
    ped->SetCurrentWeapon(WEAPON_PISTOL);
}

// 0x6907A0


CTask* CTaskComplexArrestPed::CreateFirstSubTask(CPed* ped) {
    if (!m_PedToArrest) {
        return nullptr;
    }

    m_bSubTaskNeedsToBeCreated = false;

    if (!m_PedToArrest->bInVehicle) {
        return CreateSubTask(TASK_COMPLEX_KILL_PED_ON_FOOT, ped);
    }

    if (m_PedToArrest->m_pVehicle->IsBike() || m_PedToArrest->m_pVehicle->IsSubQuad()) { // Just drag ped from a bike/quad
        return CreateSubTask(TASK_COMPLEX_DRAG_PED_FROM_CAR, ped);
    }

    if (m_PedToArrest->m_pVehicle->IsSubBoat()) { // If they're in a boat, just destroy it
        MakeSurePedHasWeaponInHand(ped);
        return CreateSubTask(TASK_COMPLEX_DESTROY_CAR, ped);
    } else {
        if (m_PedToArrest->m_pVehicle->IsUpsideDown() || m_PedToArrest->m_pVehicle->IsOnItsSide()) {
            return CreateSubTask(TASK_COMPLEX_DESTROY_CAR, ped);
        }
        return CreateSubTask(TASK_COMPLEX_CAR_OPEN_DRIVER_DOOR, ped);
    }
}

// 0x68D350
CTask* CTaskComplexArrestPed::ControlSubTask(CPed* ped) {
    CTask*    retTask     = m_pSubTask;
    eTaskType newTaskType = TASK_NONE; // Task to replace the current subtask with (if possible)

    const auto DoDestroyCarTask = [&] {
        // Make sure ped has an actual weapon in their hand
        if (ped->GetActiveWeapon().IsTypeMelee()) {
            ped->SetCurrentWeapon(ped->DoWeHaveWeaponAvailable(WEAPON_SHOTGUN) ? WEAPON_SHOTGUN : WEAPON_PISTOL);
        }
        newTaskType = TASK_COMPLEX_DESTROY_CAR;
    };

    [&] {
        if (!m_PedToArrest || m_PedToArrest->m_fHealth <= 0.f) {
            newTaskType = TASK_FINISHED;
            return;
        }

        if (m_bSubTaskNeedsToBeCreated) {
            if (m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_URGENT, nullptr)) {
                retTask = CreateFirstSubTask(ped);
            }
            return;
        }

        if (m_PedToArrest->bIsBeingArrested && m_pSubTask->GetTaskType() != TASK_SIMPLE_ARREST_PED && m_pSubTask->GetTaskType() != TASK_COMPLEX_SEEK_ENTITY) {
            newTaskType = TASK_COMPLEX_SEEK_ENTITY;
            return;
        }

        switch (m_pSubTask->GetTaskType()) {
        case TASK_COMPLEX_CAR_OPEN_PASSENGER_DOOR: { // 0x68D4FF
            const auto isLeavingCar = m_PedToArrest->GetTaskManager().FindActiveTaskByType(TASK_COMPLEX_LEAVE_CAR) != nullptr;
            const auto distSq       = (m_PedToArrest->GetPosition() - ped->GetPosition()).SquaredMagnitude();
            if (isLeavingCar && m_PedToArrest->bInVehicle && distSq < 25.f) {
                newTaskType = TASK_SIMPLE_WAIT_UNTIL_PED_OUT_CAR;
                return;
            }
            if (!m_PedToArrest->bInVehicle) {
                newTaskType = TASK_COMPLEX_KILL_PED_ON_FOOT;
                return;
            }
            if (CCarEnterExit::IsRoomForPedToLeaveCar(m_PedToArrest->m_pVehicle, TARGET_DOOR_FRONT_RIGHT, nullptr)) {
                return;
            }
            if (CCarEnterExit::IsRoomForPedToLeaveCar(m_PedToArrest->m_pVehicle, TARGET_DOOR_DRIVER, nullptr)) {
                newTaskType = TASK_COMPLEX_CAR_OPEN_DRIVER_DOOR;
                return;
            }
            DoDestroyCarTask();
            return;
        }
        case TASK_COMPLEX_CAR_OPEN_DRIVER_DOOR: { // 0x68D42A
            const auto isLeavingCar = m_PedToArrest->GetTaskManager().FindActiveTaskByType(TASK_COMPLEX_LEAVE_CAR) != nullptr;
            const auto distSq       = (m_PedToArrest->GetPosition() - ped->GetPosition()).SquaredMagnitude();
            if (isLeavingCar && m_PedToArrest->bInVehicle && distSq < 25.f) {
                newTaskType = TASK_SIMPLE_WAIT_UNTIL_PED_OUT_CAR;
                return;
            }
            if (!m_PedToArrest->bInVehicle) {
                newTaskType = TASK_COMPLEX_KILL_PED_ON_FOOT;
                return;
            }
            if (!CCarEnterExit::IsRoomForPedToLeaveCar(m_PedToArrest->m_pVehicle, TARGET_DOOR_DRIVER, nullptr)) {
                if (CCarEnterExit::IsRoomForPedToLeaveCar(m_PedToArrest->m_pVehicle, TARGET_DOOR_FRONT_RIGHT, nullptr)) {
                    newTaskType = TASK_COMPLEX_CAR_OPEN_PASSENGER_DOOR;
                }
            }
            return;
        }
        case TASK_COMPLEX_DRAG_PED_FROM_CAR: // 0x68D5F6
        case TASK_COMPLEX_DESTROY_CAR: {
            if (!m_PedToArrest->bInVehicle) { // If not in vehicle anymore, try to kill them
                newTaskType = TASK_COMPLEX_KILL_PED_ON_FOOT;
            }
            return;
        }
        case TASK_COMPLEX_KILL_PED_ON_FOOT: { // 0x68D615
            // See if ped is falling, and is close enough
            if (const auto task = static_cast<CTaskComplexFallAndGetUp*>(m_PedToArrest->GetTaskManager().FindActiveTaskByType(TASK_COMPLEX_FALL_AND_GET_UP)); task && task->IsFalling()) {
                if (IsPedCloseEnoughToArrest(m_PedToArrest, ped)) {
                    task->SetDownTime(100'000);
                    newTaskType = TASK_SIMPLE_ARREST_PED;
                } else {
                    newTaskType = TASK_COMPLEX_SEEK_ENTITY;
                }
                return;
            }

            if (!m_PedToArrest->bInVehicle || !m_PedToArrest->m_pVehicle) {
                return;
            }

            // Ped has gotten into a vehicle, we need a different task!
            const auto veh = m_PedToArrest->m_pVehicle;
            if (veh->IsBoat() || veh->IsSubPlane() || veh->IsSubHeli()) {
                DoDestroyCarTask();
                return;
            }

            if (!ped->GetActiveWeapon().IsTypeMelee()) {
                if (!FindPlayerWanted()->IsClosestCop(ped->AsCop(), 2)) {
                    newTaskType = TASK_COMPLEX_DESTROY_CAR; // NOTE: No weapon switching here
                    return;
                }
            }

            if (m_Vehicle == veh) {
                return;
            }

            if (m_PedToArrest->GetTaskManager().FindActiveTaskByType(TASK_COMPLEX_LEAVE_CAR)) {
                return;
            }

            if (veh->IsBike() || veh->IsSubQuad()) {
                newTaskType = TASK_COMPLEX_DRAG_PED_FROM_CAR;
                return;
            }

            if (veh->IsUpsideDown() || veh->IsOnItsSide()) {
                newTaskType = TASK_COMPLEX_DESTROY_CAR; // NOTE: No weapon switching here
                return;
            }

            newTaskType = TASK_COMPLEX_CAR_OPEN_DRIVER_DOOR;
            return;
        }
        default:
            return;
        }
    }();

    // Make the ped say something
    if (m_PedToArrest && m_PedToArrest->IsPlayer()) {
        if (FindPlayerWanted()->m_NumCopsInPursuit == 1) {
            ped->Say(CTX_GLOBAL_SOLO);
        }
    }

    if (newTaskType != TASK_NONE && m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_URGENT, nullptr)) {
        return CreateSubTask(newTaskType, ped);
    }
    return retTask;
}

// 0x68CF80
CTask* CTaskComplexArrestPed::CreateSubTask(eTaskType taskType, CPed* ped) {
    switch (taskType) {
    case TASK_COMPLEX_KILL_PED_ON_FOOT:
        return new CTaskComplexKillPedOnFoot(m_PedToArrest, -1, 0, 0, 0, 1);

    case TASK_COMPLEX_DRAG_PED_FROM_CAR:
        return new CTaskComplexDragPedFromCar(m_PedToArrest, 100'000);

    case TASK_COMPLEX_CAR_OPEN_DRIVER_DOOR:
        return new CTaskComplexOpenDriverDoor(m_PedToArrest->m_pVehicle);

    case TASK_COMPLEX_CAR_OPEN_PASSENGER_DOOR:
        return new CTaskComplexOpenPassengerDoor(m_PedToArrest->m_pVehicle, 8); // todo: magic number

    case TASK_SIMPLE_WAIT_UNTIL_PED_OUT_CAR:
        return new CTaskSimpleWaitUntilPedIsOutCar{m_PedToArrest, m_PedToArrest->GetPosition() - ped->GetPosition()};

    case TASK_COMPLEX_SEEK_ENTITY: {
        const float radius = m_PedToArrest->bIsBeingArrested ? 4.f : 3.f;
        return new CTaskComplexSeekEntity<CEntitySeekPosCalculatorStandard>{m_PedToArrest, 50'000, 1'000, radius, 2.f, 2.f, true, true};
    }

    case TASK_SIMPLE_ARREST_PED: {
        if (const auto veh = m_PedToArrest->m_pVehicle) {
            if (veh->IsDriver(m_PedToArrest)) { // Make the vehicle stop
                veh->vehicleFlags.bIsHandbrakeOn = true;
                veh->SetStatus(STATUS_FORCED_STOP);
            }
        }
        return new CTaskSimpleArrestPed(m_PedToArrest);
    }

    case TASK_COMPLEX_DESTROY_CAR:
        return new CTaskComplexDestroyCar(m_PedToArrest->m_pVehicle, 0, 0, 0);

    default: // Includes `TASK_FINISHED`
        return nullptr;
    }
}

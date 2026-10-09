#include "StdInc.h"

#include "TaskComplexKillPedOnFoot.h"

#include "TaskSimpleStandStill.h"
#include "TaskSimplePause.h"
#include "TaskSimpleLeaveGroup.h"
#include "TaskSimpleCarDriveTimed.h"
#include "TaskComplexLeaveCar.h"
#include "TaskComplexDragPedFromCar.h"
#include "TaskComplexDestroyCar.h"
#include "TaskComplexKillPedOnFootArmed.h"
#include "TaskComplexKillPedOnFootMelee.h"
#include "TaskComplexSignalAtPed.h"
#include "TaskComplexGangLeader.h"
#include "PedGroups.h"
#include "General.h"
#include "CopPed.h"
#include "PlayerPedData.h"
#include "CarEnterExit.h"
#include "EventAreaCodes.h"
#include "CullZones.h"

void CTaskComplexKillPedOnFoot::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexKillPedOnFoot, 0x86D894, 11);
    RH_ScopedCategory("Tasks/TaskTypes");
    RH_ScopedInstall(Constructor, 0x620E30);
    RH_ScopedInstall(CreateSubTask, 0x625E70);
    RH_ScopedVMTInstall(MakeAbortable, 0x625E40);
    RH_ScopedVMTInstall(CreateNextSubTask, 0x62B150);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x62B490);
    RH_ScopedVMTInstall(ControlSubTask, 0x626260);
}

CTaskComplexKillPedOnFoot::CTaskComplexKillPedOnFoot(
    CPed* target,
    int32 time,
    int32 pedFlags,
    int32 delay,
    int32 chance,
    uint8 nCompetence,
    bool bWaitForPlayerToBeSafe,
    bool bWaitingForPlayerToBeSafe
) :
    m_bWaitForPlayerToBeSafe{ bWaitForPlayerToBeSafe },
    m_bWaitingForPlayerToBeSafe{ bWaitingForPlayerToBeSafe },
    m_target{ target },
    m_pedFlags{ pedFlags },
    m_actionDelay{ delay },
    m_actionChance{ chance },
    m_nCompetence{ nCompetence },
    m_time{ time },
    m_startTime{ CTimer::GetTimeInMS() }
{
    CEntity::SafeRegisterRef(m_target);
}

CTaskComplexKillPedOnFoot::~CTaskComplexKillPedOnFoot() {
    CEntity::SafeCleanUpRef(m_target);
}

CTaskComplexKillPedOnFoot* CTaskComplexKillPedOnFoot::Constructor(CPed* target, int32 time, int32 pedFlags, int32 delay, int32 chance, int8 a7) {
    this->CTaskComplexKillPedOnFoot::CTaskComplexKillPedOnFoot(target, time, pedFlags, delay, chance, a7);
    return this;
}

// 0x625E40
bool CTaskComplexKillPedOnFoot::MakeAbortable(CPed* ped, eAbortPriority priority, const CEvent* event) {
    ped->bDontAcceptIKLookAts = false;
    return !m_pSubTask || m_pSubTask->MakeAbortable(ped, priority, event);
}

// 0x625E70
CTask* CTaskComplexKillPedOnFoot::CreateSubTask(int32 taskId, CPed* ped) {
    switch (taskId) {
    case TASK_SIMPLE_STAND_STILL:
        return new CTaskSimpleStandStill{ 0, false, false, 8.f };
    case TASK_SIMPLE_PAUSE: {
        CTaskSimpleStandStill{ 0, false, false, 8.f }.ProcessPed(ped); // Makes the ped stand still
        if (m_bWaitForPlayerToBeSafe && m_bWaitingForPlayerToBeSafe) {
            return new CTaskSimplePause{ 10'000 };
        }
        return new CTaskSimplePause{ 2'000 };
    }
    case TASK_NONE: // Yes, really
        return new CTaskSimpleLeaveGroup{};
    case TASK_COMPLEX_LEAVE_CAR:
        return new CTaskComplexLeaveCar{ ped->m_pVehicle, 0, 0, true, true };
    case TASK_COMPLEX_DRAG_PED_FROM_CAR: {
        const auto task = new CTaskComplexDragPedFromCar{ m_target, 0 };
        m_timer.Start(2000);
        return task;
    }
    case TASK_SIMPLE_CAR_DRIVE_TIMED:
        return new CTaskSimpleCarDriveTimed{ ped->m_pVehicle, 2000 };
    case TASK_COMPLEX_KILL_PED_ON_FOOT_ARMED: {
        const auto task = new CTaskComplexKillPedOnFootArmed{ m_target, (uint32)m_pedFlags, (uint32)m_actionDelay, (uint32)m_actionChance, (int8)m_nCompetence };
        task->m_aimImmediate = m_bAimImmediate;
        m_bAimImmediate      = false;
        m_timer.Start(2000);
        return task;
    }
    case TASK_COMPLEX_KILL_PED_ON_FOOT_MELEE: {
        const auto task = new CTaskComplexKillPedOnFootMelee{ m_target };
        m_timer.Start(2000);
        return task;
    }
    case TASK_COMPLEX_DESTROY_CAR:
        return new CTaskComplexDestroyCar{ m_target->m_pVehicle, 0, 0, 0 };
    case TASK_COMPLEX_SIGNAL_AT_PED: {
        if (m_target == FindPlayerPed(0)) {
            ped->Say(CTX_GLOBAL_PLAYER_WASTED);
        } else {
            const auto grp = CPedGroups::GetPedsGroup(m_target);
            if (!grp || grp->GetMembership().GetLeader() != FindPlayerPed(0)) {
                ped->Say(CTX_GLOBAL_ENEMY_GANG_WASTED);
            }
        }
        const auto delay = CGeneral::GetRandomNumberInRange<int32>(0, 1500);
        return new CTaskComplexSignalAtPed{ m_target, delay, true };
    }
    case TASK_FINISHED:
        ped->bDontAcceptIKLookAts = false;
        return nullptr;
    default:
        return nullptr;
    }
}

// 0x626260
CTask* CTaskComplexKillPedOnFoot::ControlSubTask(CPed* ped) {
    ped->bDontAcceptIKLookAts = true;

    auto nextTaskType = TASK_NONE; // TASK_NONE => no change

    if (!m_target) {
        nextTaskType = TASK_FINISHED;
        goto epilogue;
    }

    {
        // Updates `m_bRoomToDragPedOutOfCar`
        const auto UpdateRoomToDragPedOut = [&] {
            const auto veh = m_target->m_pVehicle;
            m_bRoomToDragPedOutOfCar = CCarEnterExit::IsRoomForPedToLeaveCar(veh, CCarEnterExit::ComputeTargetDoorToExit(veh, m_target), nullptr);
        };
        // x87: Everything is kept in extended precision
        const auto GetSpeed2D = [](const CVehicle* veh) {
            return std::sqrt((double)veh->m_vecMoveSpeed.x * (double)veh->m_vecMoveSpeed.x + (double)veh->m_vecMoveSpeed.y * (double)veh->m_vecMoveSpeed.y);
        };

        if (m_pSubTask->GetTaskType() == TASK_SIMPLE_PAUSE) {
            if (m_bWaitForPlayerToBeSafe && m_bWaitingForPlayerToBeSafe && !FindPlayerWanted(-1)->m_bEverybodyBackOff) {
                m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_URGENT, nullptr);
            }
            return m_pSubTask;
        }

        if (m_time > 0 && CTimer::GetTimeInMS() > m_startTime + m_time) { // Timed out
            if (m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_URGENT, nullptr)) {
                return new CTaskSimpleStandStill{ 0, false, false, 8.f };
            }
            goto epilogue;
        }

        if (m_bNewTarget) {
            if (m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_URGENT, nullptr)) {
                return CreateFirstSubTask(ped);
            }
            goto epilogue;
        }

        if (!m_bTargetKilled) {
            if (m_target->m_fHealth <= 0.f) { // Target is dead
                if (m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_URGENT, nullptr)) {
                    m_bTargetKilled = true;

                    if (m_pSubTask->GetTaskType() == TASK_COMPLEX_SIGNAL_AT_PED) {
                        return m_pSubTask;
                    }

                    if (ped->m_nPedType == PED_TYPE_COP && !m_target->IsPlayer()) {
                        ped->Say(CTX_GLOBAL_ARREST_CRIM);
                    }

                    if (ped->bSignalAfterKill) {
                        return CreateSubTask(TASK_COMPLEX_SIGNAL_AT_PED, ped);
                    }
                    return new CTaskSimpleLeaveGroup{};
                }
            } else if (m_target->IsPlayer()) {
                // Is the player's wanted system telling everyone to back off?
                bool waitForPlayer = m_target->GetPlayerData()->m_pWanted->m_bEverybodyBackOff;
                if (!waitForPlayer) {
                    waitForPlayer = ped->m_nPedType == PED_TYPE_COP
                        && ped->AsCop()->m_bDontPursuit
                        && FindPlayerWanted(-1)->m_ChanceOnRoadBlock == 0;
                }
                if (waitForPlayer && m_pSubTask->GetTaskType() != TASK_SIMPLE_PAUSE) {
                    nextTaskType = TASK_SIMPLE_PAUSE;
                    if (m_bWaitForPlayerToBeSafe && m_target->GetPlayerData()->m_pWanted->m_bEverybodyBackOff) {
                        m_bWaitingForPlayerToBeSafe = true;
                    }
                    goto resetShotFired;
                }
            }
        }

        switch (m_pSubTask->GetTaskType()) {
        case TASK_COMPLEX_KILL_PED_ON_FOOT_MELEE: {
            static_cast<CTaskComplexKillPedOnFootMelee*>(m_pSubTask)->m_bShotFiredByPlayerFlag = m_bShotFiredByPlayerFlag;

            if (!ped->GetActiveWeapon().IsTypeMelee()) {
                nextTaskType = TASK_COMPLEX_KILL_PED_ON_FOOT_ARMED;
                break;
            }

            if (m_target->bInVehicle) {
                const auto subType = m_target->m_pVehicle->m_nVehicleSubType;
                if (subType == VEHICLE_TYPE_PLANE || subType == VEHICLE_TYPE_HELI) {
                    nextTaskType = TASK_COMPLEX_DESTROY_CAR;
                    break;
                }
            }
            if (!m_target->bInVehicle) {
                break;
            }
            const auto veh = m_target->m_pVehicle;
            if (!veh || veh->m_nVehicleType == VEHICLE_TYPE_BOAT || ped->bStayInSamePlace || !veh->CanPedOpenLocks(ped)) {
                break;
            }
            UpdateRoomToDragPedOut();
            nextTaskType = m_bRoomToDragPedOutOfCar
                ? TASK_COMPLEX_DRAG_PED_FROM_CAR
                : TASK_COMPLEX_DESTROY_CAR;
            break;
        }
        case TASK_COMPLEX_DRAG_PED_FROM_CAR: {
            if (!m_target->bInVehicle) {
                break;
            }
            auto veh = m_target->m_pVehicle;
            if (!veh) {
                break;
            }
            if (!veh->IsDriver(m_target) && !veh->IsPassenger(m_target)) {
                break;
            }
            if (GetSpeed2D(m_target->m_pVehicle) > 0.1f) {
                nextTaskType = TASK_COMPLEX_DESTROY_CAR;
                break;
            }
            if (!m_timer.IsOutOfTime()) {
                break;
            }
            veh = m_target->m_pVehicle;
            if (!veh) {
                break;
            }
            if (!veh->IsPassenger(m_target) && !veh->IsDriver(m_target)) {
                break;
            }
            UpdateRoomToDragPedOut();
            if (!m_bRoomToDragPedOutOfCar) {
                nextTaskType = TASK_COMPLEX_DESTROY_CAR;
            }
            m_timer.Start(2000);
            break;
        }
        case TASK_COMPLEX_KILL_PED_ON_FOOT_ARMED: {
            static_cast<CTaskComplexKillPedOnFootArmed*>(m_pSubTask)->m_bShotFiredByPlayer = m_bShotFiredByPlayerFlag;

            auto& activeWep = ped->GetActiveWeapon();
            if (activeWep.IsTypeMelee() && !ped->IsPlayer()) {
                nextTaskType = TASK_COMPLEX_KILL_PED_ON_FOOT_MELEE;
            } else if (activeWep.m_TotalAmmo == 0 && !ped->IsPlayer()) {
                // Out of ammo => find a weapon with some
                int32 slot = 0;
                for (; slot < 13; slot++) {
                    if ((int32)ped->m_aWeapons[slot].m_TotalAmmo > 0) {
                        ped->SetCurrentWeapon(slot);
                        break;
                    }
                }
                if (slot == 13) {
                    ped->SetCurrentWeapon(WEAPON_UNARMED);
                    nextTaskType = TASK_COMPLEX_KILL_PED_ON_FOOT_MELEE;
                }
            }

            if (!m_target->bInVehicle) {
                break;
            }
            auto veh = m_target->m_pVehicle;
            if (!veh) {
                break;
            }
            if (!veh->IsDriver(m_target) && !veh->IsPassenger(m_target)) {
                break;
            }
            veh = m_target->m_pVehicle;
            if (veh->m_nVehicleSubType == VEHICLE_TYPE_PLANE || veh->m_nVehicleSubType == VEHICLE_TYPE_HELI) {
                nextTaskType = TASK_COMPLEX_DESTROY_CAR;
                break;
            }
            if (veh->m_nVehicleType == VEHICLE_TYPE_BIKE || veh->m_nVehicleType == VEHICLE_TYPE_BOAT) {
                break;
            }
            if (ped->bStayInSamePlace) {
                break;
            }
            if (!(GetSpeed2D(veh) <= 0.1f)) {
                break;
            }
            if (!veh->CanPedOpenLocks(ped)) {
                break;
            }
            UpdateRoomToDragPedOut();
            nextTaskType = m_bRoomToDragPedOutOfCar
                ? TASK_COMPLEX_DRAG_PED_FROM_CAR
                : TASK_COMPLEX_DESTROY_CAR;
            break;
        }
        case TASK_COMPLEX_DESTROY_CAR: {
            if (!m_target->bInVehicle) {
                nextTaskType = ped->GetActiveWeapon().IsTypeMelee()
                    ? TASK_COMPLEX_KILL_PED_ON_FOOT_MELEE
                    : TASK_COMPLEX_KILL_PED_ON_FOOT_ARMED;
                break;
            }
            if (m_target->m_pVehicle && !CCarEnterExit::IsVehicleHealthy(m_target->m_pVehicle)) {
                nextTaskType = TASK_COMPLEX_SIGNAL_AT_PED;
                break;
            }
            if (m_target->bDontDragMeOutCar) {
                break;
            }
            const auto veh = m_target->m_pVehicle; // NOTE: Not null-checked
            if (veh->m_nVehicleSubType == VEHICLE_TYPE_PLANE || veh->m_nVehicleSubType == VEHICLE_TYPE_HELI) {
                break;
            }
            if (veh->m_nVehicleType == VEHICLE_TYPE_BIKE || veh->m_nVehicleType == VEHICLE_TYPE_BOAT) {
                break;
            }
            if (ped->bStayInSamePlace) {
                break;
            }
            if (!(GetSpeed2D(veh) <= 0.1f)) {
                break;
            }
            if (!veh->CanPedOpenLocks(ped)) {
                break;
            }
            if (m_bRoomToDragPedOutOfCar) {
                nextTaskType = TASK_COMPLEX_DRAG_PED_FROM_CAR;
                break;
            }
            if (!m_timer.IsOutOfTime()) {
                break;
            }
            UpdateRoomToDragPedOut();
            if (m_bRoomToDragPedOutOfCar) {
                nextTaskType = TASK_COMPLEX_DRAG_PED_FROM_CAR;
            }
            m_timer.Start(2000);
            break;
        }
        default:
            break;
        }
    }

resetShotFired:
    m_bShotFiredByPlayerFlag = false;

epilogue:
    // Target is in a different area => tell the ped
    if (m_target && m_target->m_pContactEntity && ped->m_pContactEntity) {
        if (m_target->m_pContactEntity->GetAreaCode() != ped->m_pContactEntity->GetAreaCode()) {
            CEventAreaCodes event{ m_target };
            ped->GetIntelligence()->m_eventGroup.Add(&event, false);
        }
    }

    if (nextTaskType != TASK_NONE && m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_URGENT, nullptr)) {
        return CreateSubTask(nextTaskType, ped);
    }

    if (m_target) {
        CTaskComplexGangLeader::DoGangAttackSpeech(ped, m_target); // 0x65E9A0 // Makes gang peds say something when they see an enemy
    }

    return m_pSubTask;
}

namespace {
// NOTSA: Shared by CreateNextSubTask and CreateFirstSubTask (inlined in the original)
void GivePistolToPedIfPlayerIsStandingStill(CPed* ped) {
    const auto type = ped->m_nPedType;
    if ((IsPedTypeGang(type) || type == PED_TYPE_COP) && !ped->IsCreatedByMission()) {
        ped->GiveWeapon(WEAPON_PISTOL, 1000, true);
        ped->SetCurrentWeapon(WEAPON_PISTOL);
        ped->SetMoveState(PEDMOVE_STILL);
    }
}
}

// 0x62B150
CTask* CTaskComplexKillPedOnFoot::CreateNextSubTask(CPed* ped) {
    if (!m_target) {
        return CreateSubTask(TASK_FINISHED, ped);
    }

    switch (m_pSubTask->GetTaskType()) {
    case TASK_SIMPLE_STAND_STILL:
    case TASK_NONE:
    case TASK_COMPLEX_DESTROY_CAR:
    case TASK_COMPLEX_KILL_PED_ON_FOOT_MELEE:
    case TASK_COMPLEX_KILL_PED_ON_FOOT_ARMED:
        return CreateSubTask(TASK_FINISHED, ped);
    case TASK_SIMPLE_CAR_DRIVE_TIMED:
        return CreateSubTask(TASK_COMPLEX_LEAVE_CAR, ped);
    case TASK_SIMPLE_PAUSE: {
        if (ped->m_nPedType == PED_TYPE_COP && ped->AsCop()->m_bDontPursuit) {
            if (FindPlayerWanted(-1)->m_ChanceOnRoadBlock != 0) {
                return CreateSubTask(TASK_COMPLEX_KILL_PED_ON_FOOT_ARMED, ped);
            }
            return CreateSubTask(TASK_SIMPLE_PAUSE, ped);
        }
        if (m_bWaitForPlayerToBeSafe && m_bWaitingForPlayerToBeSafe && !FindPlayerWanted(-1)->m_bEverybodyBackOff) {
            m_bWaitingForPlayerToBeSafe = false;
            return CreateFirstSubTask(ped);
        }
        return CreateSubTask(TASK_FINISHED, ped);
    }
    case TASK_COMPLEX_DRAG_PED_FROM_CAR: {
        if (static_cast<CTaskComplexDragPedFromCar*>(m_pSubTask)->IsQuitAfterDraggingPedOut()) {
            return CreateSubTask(TASK_COMPLEX_DESTROY_CAR, ped);
        }
        break; // -> Melee / Armed
    }
    case TASK_COMPLEX_LEAVE_CAR: {
        CPed* const player = m_target->IsPlayer() ? m_target : nullptr;

        if (ped->bInVehicle) {
            return CreateSubTask(TASK_SIMPLE_CAR_DRIVE_TIMED, ped);
        }

        const auto& pedPos = ped->GetPosition();
        const auto& tgtPos = m_target->GetPosition();
        const float dx     = tgtPos.x - pedPos.x; // Stored as floats
        const float dy     = tgtPos.y - pedPos.y;
        const float dz     = tgtPos.z - pedPos.z;

        if (CCullZones::NoPolice()
            || (m_target->m_standingOnEntity
                && m_target->m_standingOnEntity != ped->m_standingOnEntity
                && std::sqrt((double)dz * (double)dz + (double)dy * (double)dy + (double)dx * (double)dx) < 5.f) // x87: Kept in extended precision
        ) {
            if (player && player->GetPlayerData()->m_bStoppedMoving) {
                GivePistolToPedIfPlayerIsStandingStill(ped);
            }
        }
        break; // -> Melee / Armed
    }
    default:
        return nullptr;
    }

    return CreateSubTask(
        ped->GetActiveWeapon().IsTypeMelee()
            ? TASK_COMPLEX_KILL_PED_ON_FOOT_MELEE
            : TASK_COMPLEX_KILL_PED_ON_FOOT_ARMED,
        ped
    );
}

// 0x62B490
CTask* CTaskComplexKillPedOnFoot::CreateFirstSubTask(CPed* ped) {
    m_bNewTarget             = false;
    m_bRoomToDragPedOutOfCar = true;

    if (!m_target) {
        ped->bDontAcceptIKLookAts = false;
        return nullptr;
    }

    if (m_target->m_fHealth <= 0.f) {
        m_bTargetKilled = true;
    }

    // x87: Kept in extended precision, but then stored as a float
    const auto& pedPos = ped->GetPosition();
    const auto& tgtPos = m_target->GetPosition();
    const double dx    = (double)tgtPos.x - (double)pedPos.x;
    const double dy    = (double)tgtPos.y - (double)pedPos.y;
    const double dz    = (double)tgtPos.z - (double)pedPos.z;
    const float dist   = (float)std::sqrt((dz * dz + dy * dy) + dx * dx);

    CPed* const player = m_target->IsPlayer() ? m_target : nullptr;

    eTaskType taskType;
    if (ped->m_pVehicle && ped->bInVehicle) {
        taskType = TASK_COMPLEX_LEAVE_CAR;
    } else {
        if (player
            && (CCullZones::NoPolice()
                || (m_target->m_standingOnEntity && m_target->m_standingOnEntity != ped->m_standingOnEntity && dist < 5.f))
            && player->GetPlayerData()->m_bStoppedMoving
        ) {
            GivePistolToPedIfPlayerIsStandingStill(ped);
        }

        const auto tgtVeh = m_target->m_pVehicle;
        if (!m_target->bInVehicle || !tgtVeh || (!tgtVeh->IsDriver(m_target) && !tgtVeh->IsPassenger(m_target))) {
            taskType = ped->GetActiveWeapon().IsTypeMelee()
                ? TASK_COMPLEX_KILL_PED_ON_FOOT_MELEE
                : TASK_COMPLEX_KILL_PED_ON_FOOT_ARMED;
        } else {
            m_bRoomToDragPedOutOfCar = true;

            taskType = TASK_COMPLEX_DESTROY_CAR;
            // x87: Kept in extended precision
            const double speed2D = std::sqrt((double)tgtVeh->m_vecMoveSpeed.y * (double)tgtVeh->m_vecMoveSpeed.y + (double)tgtVeh->m_vecMoveSpeed.x * (double)tgtVeh->m_vecMoveSpeed.x);
            if (tgtVeh->m_nVehicleSubType != VEHICLE_TYPE_HELI
                && tgtVeh->m_nVehicleSubType != VEHICLE_TYPE_PLANE
                && !ped->bStayInSamePlace
                && !(speed2D > 0.1f)
                && tgtVeh->CanPedOpenLocks(ped)
                && (tgtVeh->m_nVehicleType != VEHICLE_TYPE_BIKE || ped->GetActiveWeapon().IsTypeMelee())
                && tgtVeh->m_nVehicleType != VEHICLE_TYPE_BOAT
            ) {
                m_bRoomToDragPedOutOfCar = CCarEnterExit::IsRoomForPedToLeaveCar(tgtVeh, CCarEnterExit::ComputeTargetDoorToExit(tgtVeh, m_target), nullptr);
                if (m_bRoomToDragPedOutOfCar) {
                    taskType = m_target->bDontDragMeOutCar
                        ? TASK_COMPLEX_DESTROY_CAR
                        : TASK_COMPLEX_DRAG_PED_FROM_CAR;
                }
            }
        }
    }

    const auto task = CreateSubTask(taskType, ped);
    ped->DropEntityThatThisPedIsHolding(true);
    return task;
}

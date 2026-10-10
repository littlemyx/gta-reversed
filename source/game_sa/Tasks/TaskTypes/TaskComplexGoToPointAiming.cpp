#include "StdInc.h"

#include "TaskComplexGoToPointAiming.h"
#include "TaskComplexGoToPointAndStandStill.h"
#include "TaskSimpleGunControl.h"
#include "TaskSimpleUseGun.h"
#include "WeaponInfo.h"

void CTaskComplexGoToPointAiming::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexGoToPointAiming, 0x86fe00, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x668790);
    RH_ScopedInstall(Destructor, 0x668870);
    RH_ScopedInstall(CreateSubTask, 0x6688D0);

    RH_ScopedVMTInstall(Clone, 0x66CD80);
    RH_ScopedVMTInstall(GetTaskType, 0x668860);
    RH_ScopedVMTInstall(CreateNextSubTask, 0x66DD70);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x66DDB0);
    RH_ScopedVMTInstall(ControlSubTask, 0x6689E0);
}

// 0x668790
CTaskComplexGoToPointAiming::CTaskComplexGoToPointAiming(
    eMoveState moveState,
    const CVector& movePos,
    CEntity* aimAtEntity,
    CVector aimPos,
    const float targetRadius,
    const float slowDownDist
) :
    m_aimPos{aimPos},
    m_aimAtEntity{aimAtEntity},
    m_moveState{moveState},
    m_movePos{movePos},
    m_moveTargetRadius{targetRadius},
    m_slowDownDistance{slowDownDist}
{
    CEntity::SafeRegisterRef(m_aimAtEntity);
}

CTaskComplexGoToPointAiming::CTaskComplexGoToPointAiming(const CTaskComplexGoToPointAiming& o) :
    CTaskComplexGoToPointAiming{
        o.m_moveState,
        o.m_movePos,
        o.m_aimAtEntity,
        o.m_aimPos,
        o.m_moveTargetRadius,
        o.m_slowDownDistance
    }
{
}

// 0x668870
CTaskComplexGoToPointAiming::~CTaskComplexGoToPointAiming() {
    CEntity::SafeCleanUpRef(m_aimAtEntity);
}

// 0x6688D0
CTask* CTaskComplexGoToPointAiming::CreateSubTask(eTaskType taskType) {
    switch (taskType) {
    case TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL:
        return new CTaskComplexGoToPointAndStandStill{m_moveState, m_movePos, m_moveTargetRadius, m_slowDownDistance, false, false};
    case TASK_SIMPLE_GUN_CTRL: {
        // Derived `CTaskComplexGoToPointShooting` fires bursts
        const auto cmd = GetTaskType() == TASK_COMPLEX_GO_TO_POINT_SHOOTING ? eGunCommand::FIREBURST : eGunCommand::NONE;
        return new CTaskSimpleGunControl{m_aimAtEntity, m_aimPos, CVector{}, cmd, 1, 600'000};
    }
    default: // Includes TASK_FINISHED
        return nullptr;
    }
}

// 0x66DD70
CTask* CTaskComplexGoToPointAiming::CreateNextSubTask(CPed* ped) {
    switch (m_pSubTask->GetTaskType()) {
    case TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL:
    case TASK_SIMPLE_GUN_CTRL:
        return CreateSubTask(TASK_FINISHED);
    default:
        NOTSA_UNREACHABLE();
        return nullptr; // 0x66DD9B: default => edi = 0
    }
}

// 0x66DDB0
CTask* CTaskComplexGoToPointAiming::CreateFirstSubTask(CPed* ped) {
    m_newTargetSet = false;

    const auto wi = CWeaponInfo::GetWeaponInfo(ped->GetActiveWeapon().m_Type, ped->GetWeaponSkill());
    if (!wi->flags.bAimWithArm) {
        if (wi->flags.bCanAim && (wi->m_nWeaponFire == WEAPON_FIRE_INSTANT_HIT || wi->m_nWeaponFire == WEAPON_FIRE_AREA_EFFECT)) {
            return CreateSubTask(TASK_SIMPLE_GUN_CTRL);
        }
        return CreateSubTask(TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL);
    }

    auto& taskMgr = ped->GetTaskManager();
    if (const auto attackTask = taskMgr.GetTaskSecondary(TASK_SECONDARY_ATTACK)) {
        attackTask->MakeAbortable(ped, ABORT_PRIORITY_URGENT, nullptr);
    } else {
        // Derived `CTaskComplexGoToPointShooting` fires bursts
        const bool bShooting = GetTaskType() == TASK_COMPLEX_GO_TO_POINT_SHOOTING;
        taskMgr.SetTaskSecondary(
            new CTaskSimpleUseGun{m_aimAtEntity, m_aimPos, bShooting ? eGunCommand::FIREBURST : eGunCommand::AIM, (uint16)(bShooting ? 3 : 1), false},
            TASK_SECONDARY_ATTACK
        );
    }
    return CreateSubTask(TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL);
}

// 0x6689E0
CTask* CTaskComplexGoToPointAiming::ControlSubTask(CPed* ped) {
    const auto wi = CWeaponInfo::GetWeaponInfo(ped->GetActiveWeapon().m_Type, ped->GetWeaponSkill());

    CTask* subTask = m_pSubTask;
    if (m_newTargetSet) {
        return CreateFirstSubTask(ped);
    }

    auto& taskMgr = ped->GetTaskManager();
    switch (m_pSubTask->GetTaskType()) {
    case TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL: {
        static_cast<CTaskComplexGoToPointAndStandStill*>(m_pSubTask)->GoToPoint(m_movePos, 0.5f, 2.f, false);
        if (!wi->flags.bAimWithArm) {
            break;
        }

        const auto attackTask = static_cast<CTaskSimpleUseGun*>(taskMgr.GetTaskSecondary(TASK_SECONDARY_ATTACK));
        if (!attackTask) {
            taskMgr.SetTaskSecondary(
                new CTaskSimpleUseGun{m_aimAtEntity, m_aimPos, eGunCommand::AIM, 1, false},
                TASK_SECONDARY_ATTACK
            );
        } else if (!ped->GetIntelligence()->GetTaskUseGun()) {
            attackTask->MakeAbortable(ped, ABORT_PRIORITY_URGENT, nullptr);
        } else if (GetTaskType() == TASK_COMPLEX_GO_TO_POINT_SHOOTING && (rand() & 0x3F) == 0) {
            attackTask->ControlGun(ped, m_aimAtEntity, eGunCommand::FIRE);
        } else {
            attackTask->ControlGun(ped, m_aimAtEntity, eGunCommand::AIM);
        }
        break;
    }
    case TASK_SIMPLE_GUN_CTRL: {
        if (!ped->GetIntelligence()->GetTaskUseGun()) {
            break;
        }

        // Move towards `m_movePos` (In the ped's local space)
        const auto  d     = m_movePos - ped->GetPosition();
        const auto& right = ped->GetRight();
        const auto& fwd   = ped->GetForward();
        const float moveX = d.z * right.z + d.y * right.y + d.x * right.x;
        const float moveY = (d.z * fwd.z + d.y * fwd.y + d.x * fwd.x) * -1.f;

        CVector2D moveSpeed{0.f, 0.f};
        if (moveY * moveY + moveX * moveX > m_moveTargetRadius * m_moveTargetRadius) {
            const float inv = 1.f / std::sqrt(moveY * moveY + moveX * moveX);
            moveSpeed = CVector2D{moveX * inv, moveY * inv};
        }
        static_cast<CTaskSimpleUseGun*>(taskMgr.GetTaskSecondary(TASK_SECONDARY_ATTACK))->ControlGunMove(moveSpeed);
        break;
    }
    default:
        break;
    }
    return subTask;
}

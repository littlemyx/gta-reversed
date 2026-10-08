#include "StdInc.h"

#include "TaskComplexHitPedWithCar.h"

#include "TaskComplexEvasiveStep.h"
#include "TaskComplexFallAndGetUp.h"
#include "TaskSimpleLeaveGroup.h"
#include "TaskSimpleHitFromBehind.h"
#include "TaskSimpleKillPedWithCar.h"
#include "TaskSimpleHurtPedWithCar.h"
#include "PedGeometryAnalyser.h"

void CTaskComplexHitPedWithCar::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexHitPedWithCar, 0x86f294, 11);
    RH_ScopedCategory(); // TODO: Change this to the appropriate category!

    RH_ScopedInstall(Constructor, 0x6539A0);
    RH_ScopedInstall(Destructor, 0x653A30);

    RH_ScopedGlobalInstall(ComputeEvasiveStepMoveDir, 0x653B40);

    RH_ScopedInstall(HitHurtsPed, 0x653AE0);
    RH_ScopedInstall(CreateSubTask, 0x6560E0);

    RH_ScopedVMTInstall(Clone, 0x6559B0);
    RH_ScopedVMTInstall(GetTaskType, 0x653A20);
    RH_ScopedVMTInstall(CreateNextSubTask, 0x657AF0);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x656300);
    RH_ScopedVMTInstall(ControlSubTask, 0x653A90);

}

// 0x6539A0
CTaskComplexHitPedWithCar::CTaskComplexHitPedWithCar(CVehicle* veh, float impulseMagnitude) :
    m_Veh{veh},
    m_ImpulseMag{impulseMagnitude}
{
    CEntity::SafeRegisterRef(m_Veh);
}

CTaskComplexHitPedWithCar::CTaskComplexHitPedWithCar(const CTaskComplexHitPedWithCar& o) :
    CTaskComplexHitPedWithCar{o.m_Veh, o.m_ImpulseMag}
{
}

// 0x653A30
CTaskComplexHitPedWithCar::~CTaskComplexHitPedWithCar() {
    CEntity::SafeCleanUpRef(m_Veh);
}

// 0x653B40
CVector CTaskComplexHitPedWithCar::ComputeEvasiveStepMoveDir(const CPed* ped, CVehicle* veh) {
    return CPedGeometryAnalyser::ComputeEntityDir(*veh, (eDirection)CPedGeometryAnalyser::ComputeEntityHitSide(*ped, *veh)); // Same shit, difference packaging
}

// 0x653AE0
bool CTaskComplexHitPedWithCar::HitHurtsPed(CPed* ped) {
    if (m_ImpulseMag > (ped->IsPlayer() ? 10.f : 6.f)) {
        return true;
    }
    // 0xE8 => z of `m_vecLastCollisionImpactVelocity`
    return ped->m_vecLastCollisionImpactVelocity.z < -0.8f && m_ImpulseMag > 3.f;
}

// 0x6560E0
CTask* CTaskComplexHitPedWithCar::CreateSubTask(eTaskType tt) {
    switch (tt) {
    case TASK_COMPLEX_EVASIVE_STEP:
        return new CTaskComplexEvasiveStep{ m_Veh, m_MoveDir };
    case TASK_NONE: // 200 (Probably meant to be TASK_SIMPLE_LEAVE_GROUP, but that's just how it was)
        return new CTaskSimpleLeaveGroup{};
    case TASK_COMPLEX_FALL_AND_GET_UP: {
        AnimationId animId;
        switch (m_PedHitSide) {
        case 0:  animId = ANIM_ID_KO_SKID_BACK;  break;
        case 1:  animId = ANIM_ID_KO_SPIN_R;     break;
        case 2:  animId = ANIM_ID_KO_SKID_FRONT; break;
        case 3:  animId = ANIM_ID_KO_SPIN_L;     break;
        default: animId = (AnimationId)TASK_COMPLEX_FALL_AND_GET_UP; break; // BUG: 0xD0 used as an anim ID (unreachable, hit side is always [0, 3])
        }
        return new CTaskComplexFallAndGetUp{ animId, ANIM_GROUP_DEFAULT, m_DownTime };
    }
    case TASK_SIMPLE_HIT_BEHIND:
        return new CTaskSimpleHitFromBehind{};
    case TASK_SIMPLE_KILL_PED_WITH_CAR:
        return new CTaskSimpleKillPedWithCar{ m_Veh, m_ImpulseMag };
    case TASK_SIMPLE_HURT_PED_WITH_CAR:
        return new CTaskSimpleHurtPedWithCar{ m_Veh, m_ImpulseMag };
    default:
        return nullptr;
    }
}

// 0x657AF0
CTask* CTaskComplexHitPedWithCar::CreateNextSubTask(CPed* ped) {
    switch (m_pSubTask->GetTaskType()) {
    case TASK_COMPLEX_EVASIVE_STEP:
    case TASK_NONE:
    case TASK_COMPLEX_FALL_AND_GET_UP:
    case TASK_SIMPLE_HIT_BEHIND:
    case TASK_SIMPLE_KILL_PED_WITH_CAR:
        return CreateSubTask(TASK_FINISHED);
    case TASK_SIMPLE_HURT_PED_WITH_CAR: {
        // NOTSA: `CTaskSimpleHurtPedWithCar::m_bWillKillPed` is private, so read it by offset (0x10)
        if (*(const bool*)((const uint8*)m_pSubTask + 0x10)) {
            return nullptr;
        }
        return CreateSubTask(TASK_COMPLEX_FALL_AND_GET_UP);
    }
    default:
        return nullptr;
    }
}

// 0x656300
CTask* CTaskComplexHitPedWithCar::CreateFirstSubTask(CPed* ped) {
    m_PedHitSide = CPedGeometryAnalyser::ComputePedHitSide(*ped, *m_Veh);

    if (m_ImpulseMag > (ped->IsPlayer() ? 20.f : 12.f)) {
        return CreateSubTask(TASK_SIMPLE_KILL_PED_WITH_CAR);
    }
    if (HitHurtsPed(ped)) {
        return CreateSubTask(TASK_SIMPLE_HURT_PED_WITH_CAR);
    }
    m_MoveDir = ComputeEvasiveStepMoveDir(ped, m_Veh);
    return CreateSubTask(TASK_COMPLEX_EVASIVE_STEP);
}

// 0x653A90
CTask* CTaskComplexHitPedWithCar::ControlSubTask(CPed* ped) {
    return m_pSubTask;
}

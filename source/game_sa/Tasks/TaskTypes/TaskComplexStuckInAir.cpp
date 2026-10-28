#include "StdInc.h"

#include "TaskComplexFleePoint.h"
#include "TaskSimpleFall.h"
#include "TaskComplexJump.h"
#include "TaskSimpleInAir.h"
#include "TaskSimpleStandStill.h"
#include "TaskComplexFallAndGetUp.h"

#include "PedStuckChecker.h"
#include "TaskComplexStuckInAir.h"

// 0x67BE50
CTask* CTaskComplexStuckInAir::ControlSubTask(CPed* ped) {
    auto* const intel = ped->GetIntelligence();

    // 0x67C0A0: the exe keeps the result in ebp and, at the very end, aborts the (old) sub-task with URGENT if the result is a different task
    // (i.e. AFTER the new task was created and after the ped flags were touched, not before).
    CTask* result = m_pSubTask;

    switch (m_pSubTask->GetTaskType()) {
    case TASK_COMPLEX_FLEE_POINT: { // 0x67C07A
        if (intel->GetStuckChecker().GetState() != PED_STUCK_STATE_NONE) {
            result = CreateSubTask(TASK_SIMPLE_STAND_STILL, ped);
        }
        break;
    }
    case TASK_COMPLEX_FALL_AND_GET_UP: { // 0x67BE80
        if (intel->GetStuckChecker().GetState() != PED_STUCK_STATE_NONE) {
            if (auto* const tSubSubSimpleFall = notsa::dyn_cast_if_present<CTaskSimpleFall>(m_pSubTask->GetSubTask()))  {
                if (tSubSubSimpleFall->IsFinished()) {
                    ped->bIsStanding = true;
                }
            }
        }
        break;
    }
    case TASK_SIMPLE_STAND_STILL: { // 0x67BEE3
        if (intel->GetStuckChecker().GetState() == PED_STUCK_STATE_NONE) {
            result = nullptr; // 0x67BEF3
            break;
        }
        if (ped->IsPlayer()) { // 0x67BEFA [exe tests ped->m_pPlayerData]
            auto* const player = ped->AsPlayer();
            auto* const pad = player->GetPadFromPlayer();
            if (std::abs(pad->GetPedWalkLeftRight()) > 0.f || std::abs(pad->GetPedWalkUpDown()) > 0.f) { // 0x67BF3E
                // 0x67BF77..0x67BF90: LimitRadianAngle(GetRadianAngleBetweenPoints(0, 0, -x, y) - TheCamera.m_fOrientation [0xB6F178])
                ped->m_fAimingRotation = CGeneral::LimitRadianAngle(CGeneral::GetRadianAngleBetweenPoints(0.f, 0.f, (float)(-pad->GetPedWalkLeftRight()), (float)(pad->GetPedWalkUpDown())) - TheCamera.m_fOrientation);
            }
            if (pad->JumpJustDown()) { // 0x67BF9B: does NOT return here, the flag / anim code below still runs
                result = CreateSubTask(TASK_COMPLEX_JUMP, ped);
            }
        }
        // 0x67BFB3..0x67BFC2: `flags = (flags & ~bIsInTheAir) | bIsStanding`
        ped->bIsInTheAir = false;
        ped->bIsStanding = true;
        for (int32 i = ANIM_ID_JUMP_LAUNCH; i <= ANIM_ID_FALL_GLIDE; i++) {
            if (auto* const a = RpAnimBlendClumpGetAssociation(ped->GetRpClump(), static_cast<AnimationId>(i))) {
                if (a->GetBlendAmount() > 0.f && a->GetBlendDelta() >= 0.f) {
                    a->SetBlendDelta(-8.f);
                }
            }
        }
        break;
    }
    case TASK_COMPLEX_JUMP: { // 0x67C016
        if (intel->GetStuckChecker().GetState() != PED_STUCK_STATE_NONE) {
            if (auto* const subSubTask = m_pSubTask->GetSubTask()) {
                if (notsa::isa<CTaskSimpleInAir>(subSubTask) || notsa::isa_and_present<CTaskSimpleInAir>(subSubTask->GetSubTask())) { // check if 2nd or 3rd sub tasks (1st is `TASK_COMPLEX_JUMP`)
                    result = CreateSubTask(TASK_SIMPLE_STAND_STILL, ped);
                }
            }
        }
        break;
    }
    default: // 0x67C0A0: returns the unchanged sub-task
        break;
    }

    if (result != m_pSubTask) {
        m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_URGENT);
    }
    return result;
}

// 0x67BE20
CTask* CTaskComplexStuckInAir::CreateFirstSubTask(CPed* ped) {
    return CreateSubTask(
        ped->GetIntelligence()->GetStuckChecker().GetState() == PED_STUCK_STATE_WAS_STUCK
            ? TASK_COMPLEX_FALL_AND_GET_UP
            : TASK_SIMPLE_STAND_STILL,
        ped
    );        
}

// 0x67BD10
CTask* CTaskComplexStuckInAir::CreateNextSubTask(CPed* ped) {
    switch (m_pSubTask->GetTaskType()) {
    case TASK_COMPLEX_FALL_AND_GET_UP: // 0x67BD39
    case TASK_COMPLEX_FLEE_POINT: { // 0x67BDFA
        return CreateSubTask(
            ped->GetIntelligence()->GetStuckChecker().GetState() == PED_STUCK_STATE_NONE
                ? TASK_FINISHED
                : TASK_SIMPLE_STAND_STILL,
            ped
        );
    }
    case TASK_SIMPLE_STAND_STILL: { // 0x67BD7C
        return CreateSubTask(
            ped->GetIntelligence()->GetStuckChecker().GetState() == PED_STUCK_STATE_NONE
                ? TASK_FINISHED
                : ped->IsPlayer() // inverted
                    ? TASK_SIMPLE_STAND_STILL
                    : TASK_COMPLEX_JUMP,
            ped
        );
    }
    case TASK_COMPLEX_JUMP: { // 0x67BDC1
        return CreateSubTask(
            ped->GetIntelligence()->GetStuckChecker().GetState() != PED_STUCK_STATE_NONE
                ? TASK_SIMPLE_STAND_STILL
                : ped->IsPlayer()
                    ? TASK_FINISHED
                    : TASK_COMPLEX_FLEE_POINT,
            ped
        );
    }
    }
    NOTSA_UNREACHABLE("sub-task type was {}", m_pSubTask->GetTaskType());
    return nullptr; // 0x67BE0B: default => esi = 0
}

// 0x67BA80
CTask* CTaskComplexStuckInAir::CreateSubTask(eTaskType taskType, CPed* ped) {
    switch (taskType) {
    case TASK_COMPLEX_FLEE_POINT: { // 0x67BBDD
        ped->bIsStanding = true; // 0x67BC06-0x67BC0E: `flags |= 1` (the port did not set it for this type)

        const CVector point = ped->GetPosition()
            - ped->GetForward() * 0.5f
            + ped->GetRight() * (CGeneral::RandomBit() ? 0.5f : -0.5f) /* exe 0x67BC5D: rand() & 1 */;
        return new CTaskComplexFleePoint{ point, false, 5.f, 10'000 };
    }
    case TASK_FINISHED: { // 0x67BBE5
        return nullptr;
    }
    case TASK_COMPLEX_JUMP: { // 0x67BB74
        ped->bIsStanding = true;

        auto* const sc = &ped->GetIntelligence()->GetStuckChecker();
        sc->m_radius   = 0;
        sc->m_state    = PED_STUCK_STATE_NONE;

        return new CTaskComplexJump{ CTaskComplexJump::eForceClimb::DISABLE };
    }
    case TASK_SIMPLE_STAND_STILL: { // 0x67BB2C
        ped->bIsStanding = true;

        return new CTaskSimpleStandStill{ 5'000 };
    }
    case TASK_COMPLEX_FALL_AND_GET_UP: { // 0x67BAC9
        auto* const sc = &ped->GetIntelligence()->GetStuckChecker();
        sc->m_radius   = 0;
        sc->m_state    = PED_STUCK_STATE_NONE;

        return new CTaskComplexFallAndGetUp{ ANIM_ID_KO_SKID_BACK, ANIM_GROUP_DEFAULT, 1'000 };
    }
    }
    NOTSA_UNREACHABLE("task type was {}", taskType);
    return nullptr; // 0x67BBE9: default => eax = 0
}

void CTaskComplexStuckInAir::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexStuckInAir, 0x870608, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(CreateSubTask, 0x67BA80);
    RH_ScopedVMTInstall(Clone, 0x67C700);
    RH_ScopedVMTInstall(GetTaskType, 0x67BA60);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x67BE20);
    RH_ScopedVMTInstall(CreateNextSubTask, 0x67BD10);
    RH_ScopedVMTInstall(ControlSubTask, 0x67BE50);
}

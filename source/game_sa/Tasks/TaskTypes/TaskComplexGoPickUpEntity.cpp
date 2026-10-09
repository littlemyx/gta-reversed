#include "StdInc.h"

#include <numbers>

#include "TaskComplexGoPickUpEntity.h"
#include "TaskSimpleHoldEntity.h"
#include "TaskSimplePickUpEntity.h"
#include "TaskSimpleAchieveHeading.h"
#include "TaskComplexGoToPointAndStandStill.h"
#include "AnimManager.h"
#include "PlayerPed.h"
#include "Physical.h"
#include "General.h"

// 0x8D2FF8 / 0x8D2FFC - timeouts [ms] of the GoToPoint (5000) / PickUp (8000) sub-tasks
static inline auto& s_GoToPointTimeoutMs = StaticRef<uint32>(0x8D2FF8);
static inline auto& s_PickUpTimeoutMs    = StaticRef<uint32>(0x8D2FFC);

// 0x59C890 - the original evaluation order; the sum stays in the FPU registers (extended precision), stored as float
static CVector TransformPointExt(const CMatrix& m, const CVector& v) {
    const auto &r = m.GetRight(), &f = m.GetForward(), &u = m.GetUp(), &p = m.GetPosition();
    return CVector{
        (float)((((double)u.x * v.z + (double)f.x * v.y) + (double)r.x * v.x) + p.x),
        (float)((((double)u.y * v.z + (double)r.y * v.x) + (double)f.y * v.y) + p.y),
        (float)((((double)u.z * v.z + (double)r.z * v.x) + (double)f.z * v.y) + p.z)
    };
}

// FPATAN(st1 = y, st0 = x) of two floats, rounded to float on the store
static float AtanF(float y, float x) {
    return (float)std::atan2((double)y, (double)x);
}

void CTaskComplexGoPickUpEntity::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexGoPickUpEntity, 0x870B98, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x6919C0);
    RH_ScopedInstall(Destructor, 0x691A50);

    RH_ScopedVMTInstall(Clone, 0x692C80);
    RH_ScopedVMTInstall(GetTaskType, 0x691A40);
    RH_ScopedVMTInstall(CreateNextSubTask, 0x691AE0);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x693610);
    RH_ScopedVMTInstall(ControlSubTask, 0x691D50);
}

// 0x6919C0
CTaskComplexGoPickUpEntity::CTaskComplexGoPickUpEntity(CEntity* entity, AssocGroupId animGroup) :
    CTaskComplex{},
    m_pEntity{ entity },
    m_vecPosition{ -1000.f, 0.f, 0.f },
    m_vecPickupPosition{ -1000.f, 0.f, 0.f },
    m_nTimePassedSinceLastSubTaskCreatedInMs{ 0 },
    m_nAnimGroupId{ animGroup },
    m_bAnimBlockReferenced{ false }
{
    CEntity::SafeRegisterRef(m_pEntity);
}

// 0x691A50
CTaskComplexGoPickUpEntity::~CTaskComplexGoPickUpEntity() {
    CEntity::SafeCleanUpRef(m_pEntity);

    if (m_bAnimBlockReferenced) {
        // BUG: `m_nAnimGroupId` may have been changed by `CreateNextSubTask` since the reference was taken
        CAnimManager::RemoveAnimBlockRef(CAnimManager::GetAnimationBlockIndex(m_nAnimGroupId)); // 0x4D3FD0
        m_bAnimBlockReferenced = false;
    }
}

// 0x692C80
CTask* CTaskComplexGoPickUpEntity::Clone() const {
    return new CTaskComplexGoPickUpEntity{ m_pEntity, m_nAnimGroupId };
}

// 0x691AE0
CTask* CTaskComplexGoPickUpEntity::CreateNextSubTask(CPed* ped) {
    if (!m_pEntity) {
        return nullptr;
    }

    switch (m_pSubTask->GetTaskType()) {
    case TASK_SIMPLE_PICKUP_ENTITY: { // 0x134 - picked up, now carry it
        m_nTimePassedSinceLastSubTaskCreatedInMs = CTimer::GetTimeInMS();

        auto& taskMgr = ped->GetTaskManager();
        if (taskMgr.GetTaskSecondary(TASK_SECONDARY_PARTIAL_ANIM)) {
            return nullptr;
        }
        taskMgr.SetTaskSecondary(
            new CTaskSimpleHoldEntity{ m_pEntity, &m_vecPosition, 6, 1, ANIM_ID_CRRY_PRTIAL, ANIM_GROUP_CARRY, false },
            TASK_SECONDARY_PARTIAL_ANIM
        );
        return nullptr;
    }
    case TASK_SIMPLE_ACHIEVE_HEADING: { // 0x386 - turned towards the entity, now pick it up
        float animSpeed = 0.6f; // 0x870A80
        if (m_nAnimGroupId == ANIM_GROUP_CARRY) {
            const auto& entityPos = m_pEntity->GetPosition();
            const auto* const colModel = m_pEntity->GetColModel();
            const double entityBottomZ = (double)colModel->m_boundBox.m_vecMin.z + entityPos.z;
            const auto& pedPos = ped->GetPosition();
            if (entityBottomZ > pedPos.z) {
                m_nAnimGroupId = ANIM_GROUP_CARRY105;
                animSpeed      = 0.2f; // 0x870A88
            } else if (pedPos.z < (double)0.55f + entityBottomZ) { // 0x866CAC
                m_nAnimGroupId = ANIM_GROUP_CARRY05;
                animSpeed      = 0.26666668f; // 0x870A84
            }
        }
        auto* const task = new CTaskSimplePickUpEntity{ m_pEntity, &m_vecPosition, 6, 1, ANIM_ID_LIFTUP, m_nAnimGroupId, animSpeed };
        task->m_vecPickuposn = m_vecPickupPosition;
        return task;
    }
    case TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL: { // 0x387 - arrived, face the entity
        const auto& pedPos    = ped->GetPosition();
        const auto& entityPos = m_pEntity->GetPosition();
        const float dy        = entityPos.y - pedPos.y;
        const float dx        = entityPos.x - pedPos.x;
        return new CTaskSimpleAchieveHeading{ AtanF(-dx, dy), 1.f, 0.001f };
    }
    default:
        return nullptr;
    }
}

// 0x693610
CTask* CTaskComplexGoPickUpEntity::CreateFirstSubTask(CPed* ped) {
    const auto entity = m_pEntity;
    if (!entity) {
        return nullptr;
    }

    m_nTimePassedSinceLastSubTaskCreatedInMs = CTimer::GetTimeInMS();

    // Vector from the entity to the ped
    const auto& entityPos = entity->GetPosition();
    const auto& pedPos    = ped->GetPosition();
    const float dz = pedPos.z - entityPos.z;
    const float dy = pedPos.y - entityPos.y;
    const float dx = pedPos.x - entityPos.x;

    const auto& bb = CModelInfo::GetModelInfo(entity->m_nModelIndex)->m_pColModel->m_boundBox;

    m_vecPosition.x = -0.2f; // 0x870A74
    m_vecPosition.y = (float)(-(double)bb.m_vecMin.y - (double)0.2f); // 0x870A78
    m_vecPosition.z = -bb.m_vecMin.z;

    if (2.0 * bb.m_vecMax.y < bb.m_vecMax.x) {
        entity->SetHeading(AtanF(-dx, dy));
    } else if (2.0 * bb.m_vecMax.x < bb.m_vecMax.y) {
        entity->SetHeading((float)((double)AtanF(-dx, dy) - (double)(std::numbers::pi_v<float> / 2.f))); // 0x858FE4: pi/2 as float
    } else {
        if (entity->GetMatrix().GetUp().z < 0.9f) { // 0x858C20
            const auto& fwd = entity->GetMatrix().GetForward();
            entity->SetHeading(AtanF(-fwd.x, fwd.y));
        }
    }

    if (entity->GetType() == ENTITY_TYPE_OBJECT) {
        auto* const phys = static_cast<CPhysical*>(entity);
        phys->m_vecMoveSpeed = CVector{ 0.f, 0.f, 0.f };
        phys->m_vecTurnSpeed = CVector{ 0.f, 0.f, 0.f };
        entity->SetIsStatic(true);
    }

    entity->UpdateRwMatrix();

    const auto& mat = entity->GetMatrix();
    const auto& r   = mat.GetRight();
    const auto& f   = mat.GetForward();

    // Evaluation order of the original: z, y, x terms
    const float  dotR = (float)(((double)dz * r.z + (double)dy * r.y) + (double)dx * r.x);
    const double dotF = ((double)dz * f.z + (double)dy * f.y) + (double)dx * f.x; // stays in extended precision
    const double absF = std::abs(dotF);

    if ((double)dotR > absF) {
        m_vecPickupPosition.x = (float)((double)0.4f + bb.m_vecMax.x); // 0x870A7C
        m_vecPickupPosition.y = 0.f;
    } else if (-absF > (double)dotR) {
        m_vecPickupPosition.x = (float)((double)bb.m_vecMin.x - (double)0.4f);
        m_vecPickupPosition.y = 0.f;
    } else {
        m_vecPickupPosition.x = 0.f;
        m_vecPickupPosition.y = dotF > 0.0
            ? (float)((double)0.4f + bb.m_vecMax.y)
            : (float)((double)bb.m_vecMin.y - (double)0.4f);
    }
    m_vecPickupPosition.z = (float)((double)bb.m_vecMin.z + 1.0); // 0x858624

    const CVector target = TransformPointExt(entity->GetMatrix(), m_vecPickupPosition); // 0x59C890
    return new CTaskComplexGoToPointAndStandStill{ PEDMOVE_WALK, target, 0.2f, 0.f, false, true };
}

// 0x691D50
CTask* CTaskComplexGoPickUpEntity::ControlSubTask(CPed* ped) {
    // Make sure the anim block is loaded and referenced
    if (m_nAnimGroupId != ANIM_GROUP_DEFAULT && !m_bAnimBlockReferenced) {
        auto* blk = CAnimManager::GetAnimationBlock(m_nAnimGroupId);
        if (!blk) {
            blk = CAnimManager::GetAnimationBlock(CAnimManager::GetAnimBlockName(m_nAnimGroupId));
        }
        const auto blkIdx = CAnimManager::GetAnimationBlockIndex(blk);
        if (blk->IsLoaded) {
            CAnimManager::AddAnimBlockRef(blkIdx);
            m_bAnimBlockReferenced = true;
        } else {
            CStreaming::RequestModel(IFPToModelId(blkIdx), STREAMING_KEEP_IN_MEMORY); // 8
        }
    }

    if (!m_pSubTask) {
        return m_pSubTask;
    }

    switch (m_pSubTask->GetTaskType()) {
    case TASK_SIMPLE_PICKUP_ENTITY: { // 0x134
        const auto* const task = static_cast<CTaskSimpleHoldEntity*>(m_pSubTask);
        if (!task->m_pAnimBlendAssociation && !task->m_bEntityDropped
            && CTimer::GetTimeInMS() > m_nTimePassedSinceLastSubTaskCreatedInMs + s_PickUpTimeoutMs) {
            m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_URGENT, nullptr);
            return nullptr;
        }
        break;
    }
    case TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL: { // 0x387
        if (CTimer::GetTimeInMS() > m_nTimePassedSinceLastSubTaskCreatedInMs + s_GoToPointTimeoutMs) {
            m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_URGENT, nullptr);
            return nullptr;
        }

        if (!ped->GetPlayerData()) {
            break;
        }
        const auto* const goTo = static_cast<CTaskComplexGoToPointAndStandStill*>(m_pSubTask);
        auto* const       pad  = ped->AsPlayer()->GetPadFromPlayer();
        if (!pad) {
            break;
        }

        const float walkUpDown    = (float)pad->GetPedWalkUpDown();
        const int32 walkLeftRight = pad->GetPedWalkLeftRight();
        const float stickMag      = (float)(std::sqrt((double)walkUpDown * walkUpDown + (double)walkLeftRight * walkLeftRight) * 0.0078125); // 0x858B88

        // Heading from the ped to the go-to point
        const auto& pedPos = ped->GetPosition();
        const float heading = AtanF(-(goTo->m_vecTargetPoint.x - pedPos.x), goTo->m_vecTargetPoint.y - pedPos.y);

        // Heading of the stick relative to the camera
        float stickAngle = (float)((double)CGeneral::GetRadianAngleBetweenPoints(0.f, 0.f, -(float)walkLeftRight, walkUpDown) - (double)TheCamera.m_fOrientation);

        constexpr auto kPi    = std::numbers::pi_v<float>;     // 0x858CB8
        constexpr auto kTwoPi = 2.f * std::numbers::pi_v<float>; // 0x858CBC
        if ((double)heading + kPi < stickAngle) {
            stickAngle = (float)((double)stickAngle - kTwoPi);
        } else if ((double)heading - kPi > stickAngle) {
            stickAngle = (float)((double)stickAngle + kTwoPi);
        }

        bool abort = pad->JumpJustDown() || pad->SprintJustDown();
        if (!abort) {
            if (!(stickMag > 0.75f)) { // 0x858F34
                break;
            }
            // BUG: the original tests `(stickAngle - heading) > pi/4` (no fabs), so only one side of the arc aborts
            abort = (double)stickAngle - (double)heading > (double)(std::numbers::pi_v<float> / 4.f); // 0x859AB0
        }
        if (abort) {
            m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_URGENT, nullptr);
            return nullptr;
        }
        break;
    }
    default:
        break;
    }

    return m_pSubTask;
}

#include "StdInc.h"

#include "TaskGangHassleVehicle.h"

#include "Ragdoll/IKChainManager.h"
#include "TaskComplexGangLeader.h"
#include "TaskComplexKillPedOnFoot.h"
#include "TaskGangHasslePed.h"
#include "TaskComplexLeaveCar.h"
#include "TaskComplexPlayHandSignalAnim.h"
#include "TaskComplexSmartFleeEntity.h"
#include "TaskComplexTrackEntity.h"
#include "TaskSimpleFight.h"
#include "TaskSimpleRunAnim.h"
#include "TaskSimpleShakeFist.h"

void CTaskGangHassleVehicle::InjectHooks() {
    RH_ScopedVirtualClass(CTaskGangHassleVehicle, 0x86F9D4, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x65FAC0);
    RH_ScopedInstall(Destructor, 0x65FB60);
    RH_ScopedInstall(GetTargetHeading, 0x65FDD0);
    RH_ScopedInstall(CalcTargetOffset, 0x6641A0);
    RH_ScopedVMTInstall(Clone, 0x65FC00);
    RH_ScopedVMTInstall(CreateNextSubTask, 0x65FC80);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x664BA0);
    RH_ScopedVMTInstall(ControlSubTask, 0x6637C0);
}

// 0x65FAC0
CTaskGangHassleVehicle::CTaskGangHassleVehicle(CVehicle* vehicle, int32 a3, uint8 a4, float a5, float a6) : CTaskComplex() {
    m_bAggressive = a4;
    m_fTriggerDist = a5;
    m_Vehicle = vehicle;
    m_nHasslePosId = -1;
    m_fOffsetX = a6;
    m_bRemoveAnim = false;
    m_pEntity = nullptr;
    CEntity::SafeRegisterRef(m_Vehicle);
}

// 0x65FB60
CTaskGangHassleVehicle::~CTaskGangHassleVehicle() {
    if (m_Vehicle) {
        if (m_nHasslePosId > -1) {
            m_Vehicle->SetHasslePosId(m_nHasslePosId, false);
        }
        CEntity::SafeCleanUpRef(m_Vehicle);
    }

    CEntity::SafeCleanUpRef(m_pEntity);

    if (m_bRemoveAnim) {
        CAnimManager::RemoveAnimBlockRef(CAnimManager::GetAnimationBlockIndex("gangs"));
        m_bRemoveAnim = false;
    }
}

// 0x65FDD0
float CTaskGangHassleVehicle::GetTargetHeading(CPed* ped) {
    const auto& bb = CModelInfo::GetModelInfo(m_Vehicle->m_nModelIndex)->GetColModel()->GetBoundingBox();

    float x = bb.m_vecMin.x;
    float y = bb.m_vecMin.y;
    switch (m_nHasslePosId) {
    case 0:
    case 2:
        break;
    case 1:
    case 3:
        x = -x;
        y = -y;
        break;
    case 4:
        // NOTSA: The original reads a `CVector` at `CColModel + 0x10` here (that's `{ bb.max.y, bb.max.z, boundSphere.center.x }`)
        x = bb.m_vecMax.y;
        y = bb.m_vecMax.z;
        break;
    case 5:
        x = -bb.m_vecMax.y;
        y = -bb.m_vecMax.z;
        break;
    default:
        break;
    }
    return CGeneral::LimitRadianAngle(CGeneral::GetRadianAngleBetweenPoints(x, y, 0.f, 0.f));
}

// 0x6641A0
void CTaskGangHassleVehicle::CalcTargetOffset() {
    m_vecPosn = CVector{};

    const auto& bb = CModelInfo::GetModelInfo(m_Vehicle->m_nModelIndex)->GetColModel()->GetBoundingBox();
    const auto& min = bb.m_vecMin;
    const auto& max = bb.m_vecMax;

    switch (m_nHasslePosId) {
    case 0:
        m_vecPosn.x = min.x - m_fOffsetX;
        m_vecPosn.y = max.y * 0.5f;
        break;
    case 1:
        m_vecPosn.x = max.x + m_fOffsetX;
        m_vecPosn.y = max.y * 0.5f;
        break;
    case 2:
        m_vecPosn.x = min.x - m_fOffsetX;
        m_vecPosn.y = min.y * 0.5f;
        break;
    case 3:
        m_vecPosn.x = max.x + m_fOffsetX;
        m_vecPosn.y = min.y * 0.5f;
        break;
    case 4:
        m_vecPosn.y = min.y - m_fOffsetX;
        break;
    case 5:
        m_vecPosn.y = max.y + m_fOffsetX;
        break;
    default:
        break;
    }
}

// 0x65FC80
CTask* CTaskGangHassleVehicle::CreateNextSubTask(CPed* ped) {
    if (!m_Vehicle) {
        return nullptr;
    }

    if (m_pSubTask && m_pSubTask->GetTaskType() == TASK_COMPLEX_SMART_FLEE_ENTITY) {
        return nullptr;
    }

    if (m_Vehicle->m_fHealth >= 250.f) {
        if (m_pSubTask && m_pSubTask->GetTaskType() == TASK_COMPLEX_GANG_HASSLE_PED) {
            return nullptr;
        }
        if (m_pSubTask && m_pSubTask->GetTaskType() == TASK_COMPLEX_TRACK_ENTITY) {
            return nullptr;
        }
        return new CTaskComplexTrackEntity{m_Vehicle, m_vecPosn, 1, -1, 10.f, 40.f, 1};
    }

    // Vehicle is badly damaged => flee
    return new CTaskComplexSmartFleeEntity{
        m_Vehicle,
        false,
        30.f,
        1'000'000,
        1'000,
        1.f // 0xC18CF0 (`fEntityPosChangeThreshold`)
    };
}

// 0x664BA0
CTask* CTaskGangHassleVehicle::CreateFirstSubTask(CPed* ped) {
    if (!m_Vehicle) {
        return nullptr;
    }

    m_pEntity = m_Vehicle->m_pDriver;
    CEntity::SafeRegisterRef(m_pEntity);

    const auto& bb = CModelInfo::GetModelInfo(m_Vehicle->m_nModelIndex)->GetColModel()->GetBoundingBox();
    if (bb.m_vecMax.x - bb.m_vecMin.x > 4.f || bb.m_vecMax.y - bb.m_vecMin.y > 8.f) {
        return nullptr;
    }

    m_nHasslePosId = m_Vehicle->GetSpareHasslePosId();
    if (m_nHasslePosId == -1) {
        return nullptr;
    }

    m_Vehicle->SetHasslePosId(m_nHasslePosId, true);
    CalcTargetOffset();
    m_State = 0;
    ped->DropEntityThatThisPedIsHolding(true);

    m_Timer.m_nStartTime = CTimer::GetTimeInMS();
    m_Timer.m_nInterval  = CGeneral::GetRandomNumberInRange(150'000, 250'000);
    m_Timer.m_bStarted   = true;

    if (ped->bInVehicle && ped->m_pVehicle) {
        return new CTaskComplexLeaveCar{ped->m_pVehicle, 0, 0, true, false};
    }
    return CreateNextSubTask(ped);
}

// 0x6637C0
CTask* CTaskGangHassleVehicle::ControlSubTask(CPed* ped) {
    if (m_pSubTask && m_pSubTask->GetTaskType() != TASK_COMPLEX_KILL_PED_ON_FOOT && !m_Vehicle) {
        if (m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_URGENT, nullptr)) {
            return nullptr;
        }
        return m_pSubTask;
    }

    // Make sure anims are loaded (if they can/need to be)
    if (!m_bRemoveAnim) {
        if (CTaskComplexGangLeader::ShouldLoadGangAnims()) {
            const auto blk = CAnimManager::GetAnimationBlockIndex("gangs");
            if (CAnimManager::GetAnimBlocks()[blk].IsLoaded) {
                CAnimManager::AddAnimBlockRef(blk);
                m_bRemoveAnim = true;
            } else {
                CStreaming::RequestModel(IFPToModelId(blk), STREAMING_KEEP_IN_MEMORY);
            }
        }
    } else if (!CTaskComplexGangLeader::ShouldLoadGangAnims()) {
        CAnimManager::RemoveAnimBlockRef(CAnimManager::GetAnimationBlockIndex("gangs"));
        m_bRemoveAnim = false;
    }

    // Time is up => attack the driver
    if (m_Timer.IsOutOfTime() && m_bAggressive) {
        if (m_pSubTask->GetTaskType() == TASK_COMPLEX_KILL_PED_ON_FOOT) {
            return m_pSubTask;
        }
        return new CTaskComplexKillPedOnFoot{m_pEntity, -1, 0, 0, 0, 1};
    }

    // Driver left the vehicle => hassle them (on foot)
    if (m_Vehicle && !m_Vehicle->m_pDriver && m_pEntity) {
        if (m_pSubTask->GetTaskType() != TASK_COMPLEX_GANG_HASSLE_PED && m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_URGENT, nullptr)) {
            m_State = 3;
            return new CTaskGangHasslePed{m_pEntity, m_bAggressive ? 2 : 1, 12'000, 20'000};
        }
    }

    float distToTargetSq = 100.f;
    if (m_pSubTask->GetTaskType() == TASK_COMPLEX_TRACK_ENTITY) {
        distToTargetSq = static_cast<CTaskComplexTrackEntity*>(m_pSubTask)->m_distToTargetSq;
    }

    // Look at the driver
    if (ped->IsVisible() && distToTargetSq < 4.f && !g_ikChainMan.IsLooking(ped) && CGeneral::GetRandomNumberInRange(0, 100) > 60) {
        const auto lookTime = CGeneral::GetRandomNumberInRange(1000, 3000);
        if (const auto driver = m_Vehicle->m_pDriver) {
            g_ikChainMan.LookAt(
                "TaskHassleVehicle",
                ped,
                driver,
                lookTime,
                BONE_HEAD,
                nullptr,
                true,
                0.15f,
                500,
                3,
                false
            );
        }
    }

    if (!m_pSubTask || (m_pSubTask->GetTaskType() != TASK_COMPLEX_TRACK_ENTITY && m_pSubTask->GetTaskType() != TASK_COMPLEX_FOLLOW_NODE_ROUTE)) {
        return m_pSubTask;
    }

    // Vehicle is badly damaged => flee
    if (m_Vehicle->m_fHealth < 250.f && m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_URGENT, nullptr)) {
        return CreateNextSubTask(ped);
    }

    switch (m_State) {
    case 0: { // Approaching the vehicle
        if (!ped->GetTaskManager().GetTaskSecondary(TASK_SECONDARY_PARTIAL_ANIM) && CGeneral::GetRandomNumberInRange(0, 100) > 60) {
            ped->GetTaskManager().SetTaskSecondary(new CTaskSimpleShakeFist{}, TASK_SECONDARY_PARTIAL_ANIM);
        }
        if (distToTargetSq <= sq(m_fTriggerDist)) {
            m_State = 1;
        }
        ped->Say(CTX_GLOBAL_CHASE_CAR, 0, 1.f);
        return m_pSubTask;
    }
    case 1: { // Turning towards the vehicle
        const auto targetHeading = GetTargetHeading(ped);
        ped->m_fAimingRotation = targetHeading;
        const auto heading = ped->GetHeading();

        if (m_pSubTask->GetTaskType() != TASK_COMPLEX_TRACK_ENTITY) {
            return m_pSubTask;
        }

        if (distToTargetSq > sq(m_fTriggerDist)) {
            m_State = 0;
        } else if (std::abs(heading - targetHeading) < 0.05f) {
            m_State = 2;
        }
        return m_pSubTask;
    }
    case 2: // Hassling the vehicle
        break;
    default:
        return m_pSubTask;
    }

    if (m_pSubTask->GetTaskType() == TASK_COMPLEX_TRACK_ENTITY && sq(m_fTriggerDist) + 0.05f < distToTargetSq) {
        m_State = 0;
    }

    const auto targetHeading = GetTargetHeading(ped);
    const auto heading       = ped->GetHeading();
    if (!(std::abs(heading - targetHeading) < 0.1f)) {
        m_State = 1;
    }

    switch (CGeneral::GetRandomNumberInRange(0, 3)) {
    case 0: ped->Say(CTX_GLOBAL_DRIVE_THROUGH_TAUNT, 0, 1.f); break;
    case 1: ped->Say(CTX_GLOBAL_ATTACK_CAR, 0, 1.f); break;
    case 2: ped->Say(CTX_GLOBAL_TIP_CAR, 0, 1.f); break;
    default: break;
    }

    if (!m_bRemoveAnim || ped->m_nMoveState >= PEDMOVE_RUN) {
        return m_pSubTask;
    }

    if (!ped->GetTaskManager().GetTaskSecondary(TASK_SECONDARY_PARTIAL_ANIM)) {
        // Not playing any partial anim => start one randomly
        const auto rnd = (float)CGeneral::GetRandomNumberInRange(0, 200);

        CTask* task;
        if (rnd > 166.f) {
            task = new CTaskSimpleRunAnim{ANIM_GROUP_GANGS, ANIM_ID_SHAKE_CARA, 4.f, false};
        } else if (rnd > 133.f) {
            task = new CTaskSimpleRunAnim{ANIM_GROUP_GANGS, ANIM_ID_SHAKE_CARSH, 4.f, false};
        } else if (rnd > 100.f) {
            task = new CTaskSimpleRunAnim{ANIM_GROUP_GANGS, ANIM_ID_SHAKE_CARK, 4.f, false};
        } else if (rnd > 70.f) {
            // `rand() & 0xFFFF` * 2^-15 * 8 [truncated] => [0, 7]
            const auto animIdx = (int32)((float)(CGeneral::GetRandomNumber() & 0xFFFF) * (1.f / 32768.f) * 8.f);
            task = new CTaskSimpleRunAnim{ANIM_GROUP_GANGS, (AnimationId)(ANIM_ID_PRTIAL_GNGTLKA + animIdx), 4.f, false};
        } else if (rnd > 60.f) {
            if (ped->IsPlayingHandSignal()) {
                return m_pSubTask;
            }
            task = new CTaskComplexPlayHandSignalAnim{ANIM_ID_UNDEFINED, 4.f};
        } else {
            if (rnd > 40.f) {
                m_State = 0;
            }
            return m_pSubTask;
        }
        ped->GetTaskManager().SetTaskSecondary(task, TASK_SECONDARY_PARTIAL_ANIM);
        return m_pSubTask;
    }

    // A partial anim is already playing...
    if (sq(m_fTriggerDist) < distToTargetSq) {
        m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_URGENT, nullptr);
    }

    // Check if the "shake car" anim has reached the point where the car should be hit
    float                  hitTime = 0.5f;
    CAnimBlendAssociation* shakeAnim = RpAnimBlendClumpGetAssociation(ped->GetRpClump(), ANIM_ID_SHAKE_CARA);
    if (!shakeAnim) {
        hitTime   = 0.7f;
        shakeAnim = RpAnimBlendClumpGetAssociation(ped->GetRpClump(), ANIM_ID_SHAKE_CARSH);
        if (!shakeAnim) {
            hitTime   = 0.5f;
            shakeAnim = RpAnimBlendClumpGetAssociation(ped->GetRpClump(), ANIM_ID_SHAKE_CARK);
            if (!shakeAnim) {
                return m_pSubTask;
            }
        }
    }
    if (!(hitTime < shakeAnim->m_CurrentTime) || !(shakeAnim->m_CurrentTime - shakeAnim->m_TimeStep <= hitTime)) {
        return m_pSubTask;
    }

    // Rock the vehicle a bit
    const auto forceMag = m_Vehicle->m_fMass * 0.02f;
    const auto force    = CVector{0.f, 0.f, forceMag};
    const auto point    = [&]() -> CVector {
        switch (m_nHasslePosId) {
        case 0:
        case 2: return -m_Vehicle->GetRight();
        case 1:
        case 3: return m_Vehicle->GetRight();
        case 4: return -m_Vehicle->GetForward();
        case 5: return m_Vehicle->GetForward();
        default: return force;
        }
    }();
    m_Vehicle->ApplyTurnForce(force, point);

    // ...and "hit" it
    const auto savedHealth = m_Vehicle->m_fHealth;

    CTaskSimpleFight fight{m_Vehicle, 11, 20'000};
    fight.m_nComboSet     = 4;
    fight.m_nCurrentMove  = FIGHT_ATTACK_HIT_2;
    fight.m_nLastCommand  = 11;

    CMatrix hitMat{ped->GetMatrix()};
    hitMat.GetPosition() += ped->GetForward();

    plugin::CallMethod<0x61D5F0, CTaskSimpleFight*, float>(&fight, 0.5f); // `CTaskSimpleFight::FightSetUpCol`

    const auto numColPts = CCollision::ProcessColModels(
        hitMat,
        col1[0],
        m_Vehicle->GetMatrix(),
        *CModelInfo::GetModelInfo(m_Vehicle->m_nModelIndex)->GetColModel(),
        CWorld::m_aTempColPts,
        nullptr,
        nullptr,
        false
    );
    if (numColPts > 0) {
        const auto& cp = CWorld::m_aTempColPts[0];
        // `CTaskSimpleFight::FightHitCar`
        plugin::CallMethod<0x61D0B0, CTaskSimpleFight*, CPed*, CVehicle*, const CVector*, const CVector*, int16, int8>(
            &fight,
            ped,
            m_Vehicle,
            &CWorld::m_aTempColPts[0].m_vecPoint,
            &CWorld::m_aTempColPts[0].m_vecNormal,
            (int16)cp.m_nPieceTypeB,
            (int8)cp.m_nSurfaceTypeB
        );
    }

    m_Vehicle->m_fHealth = savedHealth; // Don't actually damage the vehicle
    m_Vehicle->m_vehicleAudio.AddAudioEvent(AE_SUSPENSION_BOUNCE, 0.f);

    return m_pSubTask;
}

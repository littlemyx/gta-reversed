#include "StdInc.h"

#include "TaskComplexPartner.h"
#include "TaskComplexPartnerChat.h"
#include "TaskComplexWanderStandard.h"
#include "TaskComplexTurnToFaceEntityOrCoord.h"
#include "TaskComplexGoToPointAndStandStill.h"
#include "TaskSimpleStandStill.h"
#include <Ragdoll/IKChainManager.h>

// 0x44E480 - `CVector2D::Normalise` as the exe has it: the squared length and `1 / sqrt` stay in extended precision,
// a NaN length is normalised (JP), a non-positive one yields (1, y)
static void NormaliseOriginal(CVector2D& v) {
    const double sq = (double)v.x * (double)v.x + (double)v.y * (double)v.y;
    if (!(sq <= 0.0)) {
        const double recip = 1.0 / std::sqrt(sq);
        v.x = (float)((double)v.x * recip);
        v.y = (float)((double)v.y * recip);
    } else {
        v.x = 1.0f;
    }
}

void CTaskComplexPartner::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexPartner, 0x870664, 14);
    RH_ScopedCategory("Tasks/TaskTypes");
    RH_ScopedInstall(Constructor, 0x681E70);
    RH_ScopedVMTInstall(CreateNextSubTask, 0x683AD0);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x681F20);
    RH_ScopedVMTInstall(ControlSubTask, 0x6840D0);
    RH_ScopedVMTInstall(StreamRequiredAnims, 0x682310);
    RH_ScopedVMTInstall(RemoveStreamedAnims, 0x682370);
}

// 0x681E70
CTaskComplexPartner::CTaskComplexPartner(const char* commandName, CPed* partner, bool leadSpeaker, float distanceMultiplier, bool makePedAlwaysFacePartner, int8 updateDirectionCount, CVector point)
    : CTaskComplex()
{
    m_leadSpeaker              = leadSpeaker;
    m_makePedAlwaysFacePartner = makePedAlwaysFacePartner;
    m_distanceMultiplier       = distanceMultiplier;
    m_updateDirectionCount     = updateDirectionCount;
    m_point                    = point;
    m_partner                  = partner;
    m_partnerState             = PARTNER_STATE_UNK_1;
    m_taskCompleted            = false;
    m_firstToTargetFlag        = -1;
    m_requiredAnimsStreamedIn  = false;
    m_animBlockName[0]         = '\0';
    CEntity::SafeRegisterRef(partner);
}

/*!
 * @addr 0x683A40
 */
CTaskComplexPartner::~CTaskComplexPartner() {
    CEntity::SafeCleanUpRef(m_partner);

    if (m_requiredAnimsStreamedIn) {
        if (strcmp(m_animBlockName, "") != 0) {
            CAnimManager::RemoveAnimBlockRef(CAnimManager::GetAnimationBlockIndex(m_animBlockName));
        }
        m_requiredAnimsStreamedIn = false;
    }
}

CTaskComplexPartner* CTaskComplexPartner::Constructor(const char* commandName, CPed* partner, bool leadSpeaker, float distanceMultiplier, bool makePedAlwaysFacePartner, int8 updateDirectionCount, CVector point) {
    this->CTaskComplexPartner::CTaskComplexPartner(commandName, partner, leadSpeaker, distanceMultiplier, makePedAlwaysFacePartner, updateDirectionCount, point);
    return this;
}

// 0x683AD0
CTask* CTaskComplexPartner::CreateNextSubTask(CPed* ped) {
    if (!m_partner) {
        return nullptr;
    }
    const auto partnerTask = static_cast<CTaskComplexPartner*>(m_partner->GetTaskManager().FindActiveTaskByType(static_cast<eTaskType>(m_taskId)));
    if (!partnerTask || partnerTask->m_partner != ped) {
        return nullptr;
    }

    const auto MakeStandStill = [] { return new CTaskSimpleStandStill(50, false, false, 8.0f); };

    switch (m_partnerState) {
    case PARTNER_STATE_UNK_6: {
        if (!m_requiredAnimsStreamedIn || !m_updateDirectionCount) {
            return nullptr;
        }
        m_updateDirectionCount--;
        return GetPartnerSequence();
    }
    case PARTNER_STATE_UNK_5: {
        if (!m_makePedAlwaysFacePartner) {
            if (partnerTask->m_partnerState >= PARTNER_STATE_UNK_5) { // 0x6822B0
                const auto defaultTask = ped->GetTaskManager().m_aPrimaryTasks[TASK_PRIMARY_DEFAULT];
                if (defaultTask && defaultTask->GetTaskType() == TASK_COMPLEX_WANDER && static_cast<CTaskComplexWander*>(defaultTask)->GetWanderType() == WANDER_TYPE_STANDARD) {
                    const auto fwd     = ped->GetForwardVector(); // 0x41CCB0
                    const auto heading = CGeneral::GetNodeHeadingFromVector(-fwd.x, -fwd.y);
                    const auto wander  = new CTaskComplexWanderStandard(PEDMOVE_WALK, (uint8)heading, true);
                    wander->m_nMinNextScanTime = CTimer::GetTimeInMS() + 100'000; // 0x66B150
                    ped->GetTaskManager().SetTask(wander, TASK_PRIMARY_DEFAULT, false);
                }
                m_partnerState = PARTNER_STATE_UNK_6;
            }
            return MakeStandStill();
        }

        if (m_firstToTargetFlag == 1) {
            if (partnerTask->m_partnerState == PARTNER_STATE_UNK_6) {
                m_partnerState = PARTNER_STATE_UNK_6;
            }
            return MakeStandStill();
        }

        // The mix of float stores and extended precision temporaries follows the x87 code
        const CVector2D pedPos     = ped->GetPosition();
        const CVector2D partnerPos = m_partner->GetPosition();
        const auto      dyExt      = (double)pedPos.y - (double)partnerPos.y;
        CVector2D       diff{ (float)((double)pedPos.x - (double)partnerPos.x), (float)dyExt };
        const auto      dist = std::sqrt((double)diff.x * (double)diff.x + dyExt * dyExt);
        if (dist > 0.99f && dist < 1.01f) { // `!(dist > c)` / `dist >= c` ⇒ normalise (NaN included)
            m_partnerState = PARTNER_STATE_UNK_6;
            return MakeStandStill();
        }

        NormaliseOriginal(diff); // 0x44E480
        const auto txExt = ((double)diff.x + (double)partnerPos.x) - (double)pedPos.x;
        const auto tyExt = ((double)diff.y + (double)partnerPos.y) - (double)pedPos.y;
        CVector2D  t{ (float)txExt, (float)tyExt };
        const auto len = std::sqrt(txExt * txExt + (double)t.y * (double)t.y);
        double     x, y;
        if (len > 0.02f) {
            NormaliseOriginal(t); // 0x44E480
            x = (double)t.x * (double)0.02f;
            y = (double)t.y * (double)0.02f;
        } else {
            x = t.x;
            y = t.y;
        }
        const auto& right = ped->GetRight();
        const auto& fwd   = ped->GetForward();
        ped->m_vecAnimMovingShiftLocal.x = (float)((double)right.x * x + (double)right.y * y);
        ped->m_vecAnimMovingShiftLocal.y = (float)((double)fwd.x * x + (double)fwd.y * y);
        return MakeStandStill();
    }
    case PARTNER_STATE_UNK_4: {
        const auto partnerState = partnerTask->m_partnerState;
        if (partnerState == PARTNER_STATE_UNK_4 || partnerState == PARTNER_STATE_UNK_5) {
            if (m_firstToTargetFlag == -1) {
                m_firstToTargetFlag = 0;
            }
            if (partnerTask->m_firstToTargetFlag == -1) {
                partnerTask->m_firstToTargetFlag = 1;
            }
            m_partnerState = PARTNER_STATE_UNK_5;
        } else {
            if (m_firstToTargetFlag == -1) {
                m_firstToTargetFlag = 1;
            }
            if (partnerTask->m_firstToTargetFlag == -1) {
                partnerTask->m_firstToTargetFlag = 0;
            }
            partnerTask->m_point = m_targetPoint;
        }
        return MakeStandStill();
    }
    case PARTNER_STATE_UNK_3: {
        if (m_pSubTask->GetTaskType() == TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL) {
            return new CTaskComplexTurnToFaceEntityOrCoord(m_partner, 0.5f, 0.2f);
        }
        if (m_pSubTask->GetTaskType() != TASK_COMPLEX_TURN_TO_FACE_ENTITY) {
            return nullptr;
        }
        m_partnerState = PARTNER_STATE_UNK_4;
        return MakeStandStill();
    }
    case PARTNER_STATE_UNK_2: {
        if (m_pSubTask->GetTaskType() != TASK_SIMPLE_STAND_STILL) {
            return nullptr;
        }
        if (m_point.x != 0.0f || m_point.y != 0.0f) { // NaN-preserving: `!=` is true for NaN, like the JP in the exe
            m_partnerState = PARTNER_STATE_UNK_3;
            GetGoToPointFrameCounter() = 0;
            return new CTaskComplexGoToPointAndStandStill(PEDMOVE_WALK, m_point, 0.1f, 0.0f, false, true);
        }
        return MakeStandStill();
    }
    case PARTNER_STATE_UNK_1: {
        if (m_pSubTask->GetTaskType() != TASK_SIMPLE_STAND_STILL) {
            return nullptr;
        }
        if (m_leadSpeaker) {
            // 0x681FE0 (unnamed, not part of this batch): computes `m_targetPoint` from `m_point`
            plugin::CallMethod<0x681FE0, CTaskComplexPartner*, CPed*, CVector*, CVector*>(this, ped, &m_point, &m_targetPoint);
            partnerTask->m_point = m_targetPoint;
        }
        m_partnerState = PARTNER_STATE_UNK_2;
        return MakeStandStill();
    }
    default:
        return nullptr;
    }
}

// 0x681F20 (also the target of the 5-byte thunk 0x6823B0 used by `CTaskComplexPartnerDeal`)
CTask* CTaskComplexPartner::CreateFirstSubTask(CPed* ped) {
    if (m_leadSpeaker && m_taskId == TASK_COMPLEX_PARTNER_CHAT) {
        const auto chat = static_cast<CTaskComplexPartnerChat*>(this);
        if (chat->m_conversationEnabled) {
            if (CAEPedSpeechAudioEntity::RequestPedConversation(ped, m_partner)) {
                chat->m_pedConversationLoaded = true;
            } else if (!chat->field_75) {
                return nullptr;
            } else {
                chat->m_conversationEnabled = false;
            }
        }
    }
    ped->StopPlayingHandSignal();
    return new CTaskSimpleStandStill(50, false, false, 8.0f);
}

// 0x6840D0
CTask* CTaskComplexPartner::ControlSubTask(CPed* ped) {
    const auto ShouldAbort = [&] {
        if (m_partnerState <= PARTNER_STATE_UNK_1 && m_partner && !m_taskCompleted) {
            return false;
        }
        if (m_partner) {
            const auto partnerTask = static_cast<CTaskComplexPartner*>(m_partner->GetTaskManager().FindActiveTaskByType(static_cast<eTaskType>(m_taskId)));
            if (partnerTask && partnerTask->m_partner == ped && !m_taskCompleted) {
                return false;
            }
        }
        return true;
    };
    if (ShouldAbort() && m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_URGENT, nullptr)) {
        return nullptr;
    }

    if (m_pSubTask->GetTaskType() == TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL) {
        if (++GetGoToPointFrameCounter() > 150 && m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_URGENT, nullptr)) {
            return nullptr;
        }
    }

    ped->DropEntityThatThisPedIsHolding(true);
    StreamRequiredAnims();
    if (g_ikChainMan.IsLooking(ped)) {
        g_ikChainMan.AbortLookAt(ped, 500);
    }
    return m_pSubTask;
}

// 0x682310
void CTaskComplexPartner::StreamRequiredAnims() {
    if (m_requiredAnimsStreamedIn) {
        return;
    }

    if (strcmp(m_animBlockName, "") != 0) {
        const auto blk = CAnimManager::GetAnimationBlockIndex(m_animBlockName);
        if (!CAnimManager::GetAnimBlocks()[blk].IsLoaded) {
            CStreaming::RequestModel(IFPToModelId(blk), STREAMING_KEEP_IN_MEMORY);
            return;
        }
        CAnimManager::AddAnimBlockRef(blk);
    }
    m_requiredAnimsStreamedIn = true;
}

// 0x682370
void CTaskComplexPartner::RemoveStreamedAnims() {
    if (!m_requiredAnimsStreamedIn) {
        return;
    }

    if (strcmp(m_animBlockName, "") != 0) {
        CAnimManager::RemoveAnimBlockRef(CAnimManager::GetAnimationBlockIndex(m_animBlockName));
    }
    m_requiredAnimsStreamedIn = false;
}

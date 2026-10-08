#include "StdInc.h"

#include "TaskSimpleRunNamedAnim.h"

#include "Plugins/RpAnimBlendPlugin/RpAnimBlend.h"
#include "Events/EventGroup.h"

void CTaskSimpleRunNamedAnim::InjectHooks() {
    RH_ScopedVirtualClass(CTaskSimpleRunNamedAnim, 0x86D54C, 9);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedOverloadedInstall(Constructor, "Default", 0x6674B0, CTaskSimpleRunNamedAnim*(CTaskSimpleRunNamedAnim::*)());
    RH_ScopedOverloadedInstall(Constructor, "Anim", 0x61A990, CTaskSimpleRunNamedAnim*(CTaskSimpleRunNamedAnim::*)(char const*, char const*, int32, float, int32, bool, bool, bool, bool));
    RH_ScopedInstall(Destructor, 0x61BF10);

    RH_ScopedInstall(FinishRunAnimMovePedCB, 0x61AAA0);
    RH_ScopedInstall(StartAnim, 0x61BB10);
    RH_ScopedVMTInstall(Clone, 0x61B770);
    RH_ScopedVMTInstall(GetTaskType, 0x61AA90);
    RH_ScopedVMTInstall(ProcessPed, 0x61BF20);
    RH_ScopedInstall(OffsetPedPosition, 0x61AB00);
}

// 0x6674B0
CTaskSimpleRunNamedAnim::CTaskSimpleRunNamedAnim() :
    CTaskSimpleAnim{ false }
{
    // Rest done in header
}

// todo: check m_vecOffsetAtEnd initialization
// 0x61A990
CTaskSimpleRunNamedAnim::CTaskSimpleRunNamedAnim(
    const char* animName,
    const char* animGroupName,
    uint32 animFlags,
    float blendDelta,
    uint32 endTime,
    bool bDontInterrupt,
    bool bRunInSequence,
    bool bOffsetPed,
    bool bHoldLastFrame
) :
    CTaskSimpleAnim(bHoldLastFrame),
    m_Time{ endTime },
    m_animFlags{ animFlags }
{
    m_bDontInterrupt = bDontInterrupt;
    m_bRunInSequence = bRunInSequence;
    m_bOffsetAtEnd = bOffsetPed;
    m_fBlendDelta = blendDelta;
    strcpy_s(m_animName, animName);
    strcpy_s(m_animGroupName, animGroupName);
    if (const auto block = CAnimManager::GetAnimationBlock(m_animGroupName)) {
        m_pAnimHierarchy = CAnimManager::GetAnimation(m_animName, block);
    }
}

// 0x61BF20
bool CTaskSimpleRunNamedAnim::ProcessPed(CPed* ped) {
    if (m_bOffsetAvailable) {
        OffsetPedPosition(ped);
    }

    if (m_bIsFinished) {
        if (!m_bOffsetAtEnd) {
            auto* const assoc = RpAnimBlendClumpGetAssociation(ped->GetRpClump(), true, m_pAnimHierarchy);
            CVector bonePos{};
            ped->GetBonePosition(&bonePos, (eBoneTag)3, false);
            if (m_fBlendDelta > 100.f && assoc) {
                if ((ped->GetPosition() - bonePos).Magnitude() > 1.f) {
                    assoc->m_Flags |= ANIMATION_IS_BLEND_AUTO_REMOVE;
                    assoc->m_BlendDelta = m_fBlendDelta * -1.f;
                }
            }
        }
        return true;
    }

    if (!m_pAnimHierarchy || !m_pAnimHierarchy->m_pSequences) {
        return true;
    }

    if (m_bDontInterrupt && (!(m_animFlags & ANIMATION_IS_FINISH_AUTO_REMOVE) || (m_animFlags & ANIMATION_IS_LOOPED))) {
        ped->GetEventGroup().RemoveInvalidEvents(true);
        ped->GetEventGroup().Reorganise();
    }

    if (m_Timer.m_bStarted && m_Timer.IsOutOfTime()) {
        if (!(m_animFlags & ANIMATION_IS_FINISH_AUTO_REMOVE)) {
            if (m_pAnim) {
                m_pAnim->SetDefaultFinishCallback();
            }
            m_bIsFinished = true;
            m_pAnim       = nullptr;
            return true;
        }
        MakeAbortable(ped, ABORT_PRIORITY_LEISURE, nullptr);
    }

    if (!m_pAnim) {
        // Some other task is already using this anim (it has callback data set)
        const auto assoc = RpAnimBlendClumpGetAssociation(ped->GetRpClump(), true, m_pAnimHierarchy);
        if (assoc && assoc->m_pCallbackData) {
            return true;
        }
        StartAnim(ped);
    }
    return m_bIsFinished;
}

// 0x61AB00
void CTaskSimpleRunNamedAnim::OffsetPedPosition(CPed* ped) {
    ped->UpdateRpHAnim();
    ped->m_bDontUpdateHierarchy = true;
    auto& pos = ped->GetPosition();
    pos += ped->m_matrix->TransformVector(m_vecOffsetAtEnd);
    m_bOffsetAvailable = false;
}

// 0x61BB10
void CTaskSimpleRunNamedAnim::StartAnim(CPed* ped) {
    if ((int32)m_Time >= 0) {
        m_Timer.Start((int32)m_Time);
    }

    if (!(m_animFlags & ANIMATION_IS_FINISH_AUTO_REMOVE)) {
        m_animFlags |= ANIMATION_IS_BLEND_AUTO_REMOVE;
    }

    m_pAnim = CAnimManager::BlendAnimation(ped->GetRpClump(), m_pAnimHierarchy, (int32)m_animFlags, m_fBlendDelta);

    if (m_bOffsetAtEnd) {
        m_pAnim->SetFinishCallback(FinishRunAnimMovePedCB, this);
    } else if ((m_animFlags & ANIMATION_IS_FINISH_AUTO_REMOVE) || m_bRunInSequence) {
        m_pAnim->SetFinishCallback(CTaskSimpleAnim::FinishRunAnimCB, this);
    } else {
        m_pAnim->SetDeleteCallback(CTaskSimpleAnim::FinishRunAnimCB, this);
    }

    m_nAnimId = (int16)m_pAnim->GetAnimId();
}

// 0x61AAA0
void CTaskSimpleRunNamedAnim::FinishRunAnimMovePedCB(CAnimBlendAssociation* assoc, void* data) {
    auto* const task = static_cast<CTaskSimpleRunNamedAnim*>(data);

    assoc->GetNode(0)->GetCurrentTranslation(task->m_vecOffsetAtEnd, 0.f);
    task->m_bOffsetAvailable = true;
    assoc->m_Flags |= ANIMATION_IGNORE_ROOT_TRANSLATION;

    if ((assoc->m_Flags & ANIMATION_IS_FINISH_AUTO_REMOVE) || assoc->m_nCallbackType == ANIM_BLEND_CALLBACK_DELETE) {
        task->m_bIsFinished = true;
    } else {
        assoc->SetDeleteCallback(CTaskSimpleAnim::FinishRunAnimCB, task);
    }
}

#include "StdInc.h"

#include "TaskSimpleRunAnimLoopedMiddle.h"
#include "AnimBlendAssociation.h"
#include "AnimBlendHierarchy.h"
#include "Plugins/RpAnimBlendPlugin/RpAnimBlend.h"

void CTaskSimpleRunAnimLoopedMiddle::InjectHooks() {
    RH_ScopedVirtualClass(CTaskSimpleRunAnimLoopedMiddle, 0x86D594, 9);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor1, 0x61AC90);
    RH_ScopedInstall(Constructor2, 0x61AD10);
    RH_ScopedInstall(StartAnim, 0x61ADF0);
    RH_ScopedVMTDestructorInstall(0x61BC80);
    RH_ScopedVMTInstall(Clone, 0x61B890);
    RH_ScopedVMTInstall(GetTaskType, 0x61AD00);
    RH_ScopedVMTInstall(ProcessPed, 0x61BCB0);
}

// 0x61AC90
CTaskSimpleRunAnimLoopedMiddle::CTaskSimpleRunAnimLoopedMiddle(AssocGroupId animGroup, AnimationId animId, float blendDelta, float loopStartFrac, float loopEndFrac, int32 durationMs, bool bHoldLastFrame) :
    CTaskSimpleAnim{ bHoldLastFrame },
    m_AnimGroup{ animGroup },
    m_AnimId{ animId },
    m_AnimFlags{ 0 },
    m_AnimHierarchy{ nullptr },
    m_BlendDelta{ blendDelta },
    m_LoopStartFrac{ loopStartFrac },
    m_LoopEndFrac{ loopEndFrac },
    m_DurationMs{ durationMs }
{
}

// 0x61AD10
CTaskSimpleRunAnimLoopedMiddle::CTaskSimpleRunAnimLoopedMiddle(const char* animName, const char* animBlockName, uint32 animFlags, float blendDelta, float loopStartFrac, float loopEndFrac, int32 durationMs, bool bHoldLastFrame) :
    CTaskSimpleAnim{ bHoldLastFrame },
    m_AnimFlags{ animFlags },
    m_BlendDelta{ blendDelta },
    m_LoopStartFrac{ loopStartFrac },
    m_LoopEndFrac{ loopEndFrac },
    m_DurationMs{ durationMs }
{
    strcpy_s(m_AnimName, animName);
    strcpy_s(m_AnimBlockName, animBlockName);
    m_AnimHierarchy = CAnimManager::GetAnimation(m_AnimName, CAnimManager::GetAnimationBlock(m_AnimBlockName)); // 0x4D42F0, 0x4D3940
    m_AnimGroup     = ANIM_GROUP_DEFAULT;
    m_AnimId        = ANIM_ID_NO_ANIMATION_SET;
}

// 0x61B890
CTask* CTaskSimpleRunAnimLoopedMiddle::Clone() const {
    if (m_AnimHierarchy) {
        return new CTaskSimpleRunAnimLoopedMiddle{ m_AnimName, m_AnimBlockName, m_AnimFlags, m_BlendDelta, m_LoopStartFrac, m_LoopEndFrac, m_DurationMs, m_bHoldLastFrame };
    }
    return new CTaskSimpleRunAnimLoopedMiddle{ m_AnimGroup, m_AnimId, m_BlendDelta, m_LoopStartFrac, m_LoopEndFrac, m_DurationMs, m_bHoldLastFrame };
}

// 0x61ADF0
void CTaskSimpleRunAnimLoopedMiddle::StartAnim(CPed* ped) {
    if (m_AnimHierarchy) {
        m_pAnim = CAnimManager::BlendAnimation(ped->GetRpClump(), m_AnimHierarchy, (int32)m_AnimFlags, m_BlendDelta); // 0x4D4410
    } else {
        m_pAnim = CAnimManager::BlendAnimation(ped->GetRpClump(), m_AnimGroup, m_AnimId, m_BlendDelta); // 0x4D4610
    }
    m_pAnim->SetFinishCallback(FinishRunAnimCB, this); // 0x61A8A0
}

// 0x61BCB0
bool CTaskSimpleRunAnimLoopedMiddle::ProcessPed(CPed* ped) {
    if (m_bIsFinished) {
        return true;
    }

    // Is the anim running? (Not started yet => start it)
    // BUG?: The original looks up the anim by id `(m_AnimId == 0)` (so either id 0 or id 1), which is hardly what was intended. Kept as is.
    if (!m_pAnim || RpAnimBlendClumpGetAssociation(ped->GetRpClump(), (uint32)(m_AnimId == 0))) {
        StartAnim(ped);

        const auto totalTime = m_pAnim->m_BlendHier->m_fTotalTime;
        m_LoopStartTime = totalTime * m_LoopStartFrac;
        m_LoopEndTime   = totalTime * m_LoopEndFrac;

        m_Timer.m_nStartTime = CTimer::GetTimeInMS();
        m_Timer.m_nInterval  = m_DurationMs;
        m_Timer.m_bStarted   = true;
        return false;
    }

    if (m_Timer.m_bStarted && m_Timer.IsOutOfTime()) { // 0x420E30
        return false;
    }

    if (m_pAnim) {
        const auto cur = m_pAnim->m_CurrentTime;
        if (cur > m_LoopEndTime) { // Just passed the end of the loop?
            if ((double)cur - (double)m_pAnim->m_TimeStep <= (double)m_LoopEndTime) { // ...and the previous step was before it
                m_pAnim->SetCurrentTime(m_LoopStartTime);
                m_pAnim->m_Flags |= ANIMATION_IS_PLAYING;
            }
        }
    }
    return false;
}

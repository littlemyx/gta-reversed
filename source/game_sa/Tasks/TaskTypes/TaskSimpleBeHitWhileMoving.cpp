#include "StdInc.h"

#include "TaskSimpleBeHitWhileMoving.h"
#include "AnimBlendAssociation.h"

void CTaskSimpleBeHitWhileMoving::InjectHooks() {
    RH_ScopedVirtualClass(CTaskSimpleBeHitWhileMoving, 0x86D7FC, 9);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x61FE30);
    RH_ScopedInstall(StartAnim, 0x61FF40);
    RH_ScopedInstall(FinishAnimCB, 0x61FF30);
    RH_ScopedVMTDestructorInstall(0x625BA0);
    RH_ScopedVMTInstall(Clone, 0x623190);
    RH_ScopedVMTInstall(GetTaskType, 0x61FE60);
    RH_ScopedVMTInstall(MakeAbortable, 0x61FEE0);
    RH_ScopedVMTInstall(ProcessPed, 0x620290);
}

// 0x61FE70
CTaskSimpleBeHitWhileMoving::~CTaskSimpleBeHitWhileMoving() {
    if (m_pAnim) {
        m_pAnim->m_BlendDelta = -4.f;
        m_pAnim->SetFinishCallback(CDefaultAnimCallback::DefaultAnimCB, nullptr);
    }
}

// 0x61FEE0
bool CTaskSimpleBeHitWhileMoving::MakeAbortable(CPed* ped, eAbortPriority priority, const CEvent* event) {
    if (priority == ABORT_PRIORITY_URGENT || priority == ABORT_PRIORITY_IMMEDIATE) {
        if (m_pAnim) {
            m_pAnim->SetFinishCallback(CDefaultAnimCallback::DefaultAnimCB, nullptr);
            m_pAnim = nullptr;
        }
        return true;
    }
    if (m_pAnim) {
        m_pAnim->m_Flags |= ANIMATION_IS_BLEND_AUTO_REMOVE;
        m_pAnim->m_BlendDelta = -4.f;
    }
    return false;
}

// 0x620290
bool CTaskSimpleBeHitWhileMoving::ProcessPed(CPed* ped) {
    if (m_bFinished) {
        return true;
    }
    if (!m_pAnim) {
        StartAnim(ped);
    }
    return false;
}

// 0x61FF40
void CTaskSimpleBeHitWhileMoving::StartAnim(CPed* ped) {
    const auto clump = ped->GetRpClump();

    // The "get or blend" sequence shared by the 0x61FFAA and 0x6201F6 paths. Reuses the existing anim unless it is fading out.
    const auto PlayHitAnim = [&](AnimationId animId) {
        m_pAnim = RpAnimBlendClumpGetAssociation(clump, (uint32)animId);
        if (!m_pAnim || m_pAnim->m_BlendDelta < 0.f) { // FCOMP + JP: reuse if `!(delta < 0)` (so NaN reuses too)
            m_pAnim = CAnimManager::BlendAnimation(clump, ANIM_GROUP_DEFAULT, animId, 8.f);
        }
        m_pAnim->SetCurrentTime(0.f);
        m_pAnim->m_Flags |= ANIMATION_IS_PLAYING;
        m_pAnim->m_Flags |= ANIMATION_IS_FINISH_AUTO_REMOVE;
    };

    bool useKnockDown = false;
    if (m_HitType == 4 || m_HitType == 2) {
        if (!ped->IsPlayer() && (rand() & 1)) { // 0x5DF8F0, 0x821B1E
            useKnockDown = true;
        } else {
            useKnockDown = (rand() & 7) == 0; // 0x821B1E
        }
    }

    if (useKnockDown) {
        AnimationId animId{};
        bool        partialAnim = false; // The `PlayHitAnim` path (0x61FFAA) vs the knock-down path (0x62001D)
        switch (m_HitDir) {
        case 1:
            animId = ANIM_ID_KO_SPIN_R;
            break;
        case 2:
            if (rand() & 1) { // 0x821B1E
                animId      = ANIM_ID_HIT_BACK;
                partialAnim = true;
            } else {
                animId = ANIM_ID_KO_SKID_BACK;
            }
            break;
        case 3:
            animId = ANIM_ID_KO_SPIN_L;
            break;
        default:
            if (m_HitType == 2) {
                animId = ANIM_ID_KO_SHOT_STOM;
            } else if (rand() & 1) { // 0x821B1E
                animId      = ANIM_ID_HIT_WALK;
                partialAnim = true;
            } else if (rand() & 1) { // 0x821B1E
                animId      = ANIM_ID_HIT_FRONT;
                partialAnim = true;
            } else {
                animId = ANIM_ID_KO_SHOT_FACE;
            }
            break;
        }

        if (partialAnim) {
            PlayHitAnim(animId);
        } else if (!ped->m_pAttachedTo) {
            ped->ClearLookFlag(); // 0x5E1950
            ped->ClearAimFlag();  // 0x5DEF20

            CAnimBlendAssociation* assoc{};
            if (animId != ANIM_ID_NO_ANIMATION_SET) { // Always true here, but kept as in the original
                assoc = RpAnimBlendClumpGetAssociation(clump, (uint32)animId);
                if (assoc) {
                    assoc->SetCurrentTime(0.f);
                    assoc->m_Flags |= ANIMATION_IS_PLAYING;
                    assoc->m_BlendAmount = 0.f;
                    assoc->m_BlendDelta  = 8.f;
                } else {
                    m_pAnim = CAnimManager::BlendAnimation(clump, ANIM_GROUP_DEFAULT, animId, 8.f);
                }
            } else if (ped->IsPlayer()) { // Dead code (see above)
                assoc = RpAnimBlendClumpGetAssociation(clump, (uint32)ANIM_ID_DEFAULT_CAR_ROLLOUT_LHS);
                if (!assoc) {
                    assoc = RpAnimBlendClumpGetAssociation(clump, (uint32)ANIM_ID_DEFAULT_CAR_ROLLOUT_RHS);
                }
            }

            // Set the time the ped stays down for
            const double ms    = (double)CTimer::GetTimeInMS();
            const auto   rem   = (uint32)((uint32)ped->m_nRandomSeed + CTimer::GetFrameCounter()) % 1000u;
            if (ped->IsPlayer()) {
                if (!assoc) {
                    ped->m_nUnconsciousTimer = rem + CTimer::GetTimeInMS() + 1000u;
                } else if (assoc->m_AnimId == ANIM_ID_DEFAULT_CAR_ROLLOUT_LHS || assoc->m_AnimId == ANIM_ID_DEFAULT_CAR_ROLLOUT_RHS) {
                    ped->m_nUnconsciousTimer = (uint32)(int32)(
                        ((double)assoc->m_BlendHier->m_fTotalTime * 1000.0 + ms) - (double)assoc->m_CurrentTime * 1000.0 + 100.0
                    );
                } else {
                    ped->m_nUnconsciousTimer = (uint32)(int32)(
                        ((double)assoc->m_BlendHier->m_fTotalTime * 1000.0 + ms) + 500.0
                    );
                }
            } else {
                if (!assoc) {
                    ped->m_nUnconsciousTimer = rem + CTimer::GetTimeInMS() + 1000u;
                } else {
                    ped->m_nUnconsciousTimer = (uint32)(int32)(
                        (((double)(int32)rem + (double)assoc->m_BlendHier->m_fTotalTime * 1000.0) + ms) + 500.0
                    );
                }
            }
        }
    } else {
        if (!ped->IsPlayer() && !(rand() & 1)) { // 0x5DF8F0, 0x821B1E
            (void)CGeneral::GetRandomNumberInRange(1000, 3000); // 0x407180 - NOTSA: result is unused in the original too
            // BUG: The original now registers the finish callback on `m_pAnim`, which is null here => crashes
        } else {
            switch (m_HitDir) {
            case 1:  PlayHitAnim(ANIM_ID_SHOT_LEFTP);     break;
            case 2:  PlayHitAnim(ANIM_ID_SHOT_PARTIAL_B); break;
            case 3:  PlayHitAnim(ANIM_ID_SHOT_RIGHTP);    break;
            default: PlayHitAnim(ANIM_ID_SHOT_PARTIAL);   break;
            }
        }
    }

    if (!m_pAnim && notsa::IsFixBugs()) { // BUG: See above (also reachable via the "ped is attached" path)
        return;
    }
    m_pAnim->SetFinishCallback(FinishAnimCB, this); // 0x4CEBE0
}

// 0x61FF30
void CTaskSimpleBeHitWhileMoving::FinishAnimCB(CAnimBlendAssociation* assoc, void* data) {
    const auto self = static_cast<CTaskSimpleBeHitWhileMoving*>(data);
    self->m_pAnim     = nullptr;
    self->m_bFinished = true;
}

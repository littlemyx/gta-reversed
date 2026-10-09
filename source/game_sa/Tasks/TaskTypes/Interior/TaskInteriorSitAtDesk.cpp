#include "StdInc.h"
#include "TaskInteriorSitAtDesk.h"
#include "Interior/InteriorManager_c.h"
#include "Interior/InteriorInfo_t.h"
#include "CarEnterExit.h"
#include <numbers>

void CTaskInteriorSitAtDesk::InjectHooks() {
    RH_ScopedVirtualClass(CTaskInteriorSitAtDesk, 0x87035c, 9);
    RH_ScopedCategory("Tasks/TaskTypes/Interior");

    RH_ScopedInstall(Constructor, 0x676010);
    RH_ScopedInstall(Destructor, 0x676080);

    RH_ScopedInstall(FinishAnimCB, 0x676190);
    RH_ScopedInstall(StartRandomLoopAnim, 0x677780);
    RH_ScopedInstall(StartRandomOneOffAnim, 0x677880);
    RH_ScopedVMTInstall(Clone, 0x6760E0);
    RH_ScopedVMTInstall(GetTaskType, 0x676070);
    RH_ScopedVMTInstall(MakeAbortable, 0x676150);
    RH_ScopedVMTInstall(ProcessPed, 0x677920);
}

// 0x676010
CTaskInteriorSitAtDesk::CTaskInteriorSitAtDesk(int32 duration, InteriorInfo_t* interiorInfo, bool bDoInstantly) :
    m_bDoInstantly{bDoInstantly},
    m_InteriorInfo{interiorInfo},
    m_Duration{duration}
{
}

// 0x6760E0
CTaskInteriorSitAtDesk::CTaskInteriorSitAtDesk(const CTaskInteriorSitAtDesk& o) :
    CTaskInteriorSitAtDesk{o.m_Duration, o.m_InteriorInfo, o.m_bDoInstantly}
{
}

// 0x676080
CTaskInteriorSitAtDesk::~CTaskInteriorSitAtDesk() {
    if (m_Anim) {
        m_Anim->SetDefaultDeleteCallback(); // 0x4CEBC0
    }
}

// 0x676190
void CTaskInteriorSitAtDesk::FinishAnimCB(CAnimBlendAssociation* anim, void* data) {
    const auto self = notsa::cast<CTaskInteriorSitAtDesk>(static_cast<CTask*>(data));

    assert(self && anim);
    assert(anim == self->m_Anim);

    self->m_PrevAnimId = anim->GetAnimId();
    if (anim->m_AnimId == ANIM_ID_OFF_SIT_2IDLE_180 || self->m_bTaskAborting && self->m_PrevAnimId == ANIM_ID_OFF_SIT_IN) {
        anim->SetBlendDelta(-1000.f);
        self->m_bTaskFinished = true;
    }

    self->m_Anim = nullptr;
}

namespace {
// x87-accurate version of `0x59C890` (MultiplyMatrixWithVector): each component is accumulated in extended
// precision in the order of the original and rounded to float once at the end.
CVector TransformPointX87(const CMatrix& m, const CVector& v) {
    return {
        (float)((((double)m.GetUp().x * v.z + (double)m.GetForward().x * v.y) + (double)m.GetRight().x * v.x) + m.GetPosition().x),
        (float)((((double)m.GetUp().y * v.z + (double)m.GetRight().y * v.x) + (double)m.GetForward().y * v.y) + m.GetPosition().y),
        (float)((((double)m.GetUp().z * v.z + (double)m.GetRight().z * v.x) + (double)m.GetForward().z * v.y) + m.GetPosition().z)
    };
}

// The exe's idiom: `(int)((rand() & 0xFFFF) * (1 / 32768.f) * range)` (0x858B14), evaluated exactly (x87).
// NOT the same as `CGeneral::GetRandomNumberInRange` (that one divides by RAND_MAX, and is exclusive of `max`)
int32 RandomScaled(int32 range) {
    return (int32)(((uint32)CGeneral::GetRandomNumber() & 0xFFFF) * (uint32)range) >> 15;
}
}

// 0x677780
void CTaskInteriorSitAtDesk::StartRandomLoopAnim(CPed* ped, float blendDelta) {
    if (m_Anim) {
        m_Anim->SetDefaultDeleteCallback(); // 0x4CEBC0
    }

    // Original draws the chance, then the duration, and only then blends the animation
    const auto chance = RandomScaled(100);
    AnimationId animId;
    int32       duration;
    if (chance > 40) {
        animId   = ANIM_ID_OFF_SIT_TYPE_LOOP;
        duration = 7000 + RandomScaled(5000);
    } else if (chance > 10) {
        animId   = ANIM_ID_OFF_SIT_BORED_LOOP;
        duration = 2000 + RandomScaled(3000);
    } else {
        animId   = ANIM_ID_OFF_SIT_IDLE_LOOP;
        duration = 2000 + RandomScaled(3000);
    }

    m_Anim = CAnimManager::BlendAnimation(ped->GetRpClump(), ANIM_GROUP_INT_OFFICE, animId, blendDelta);
    m_Anim->SetFinishCallback(FinishAnimCB, this);

    // Unconditional (no `>= 0` check), but duration is always positive
    m_AnimTimer.m_nInterval  = duration;
    m_AnimTimer.m_nStartTime = CTimer::GetTimeInMS();
    m_AnimTimer.m_bStarted   = true;
}

// 0x677880
void CTaskInteriorSitAtDesk::StartRandomOneOffAnim(CPed* ped) {
    const auto chance = RandomScaled(100);
    StartAnim(
        ped,
        chance > 60
            ? ANIM_ID_OFF_SIT_READ
            : chance > 35
                ? ANIM_ID_OFF_SIT_DRINK
                : chance > 10
                    ? ANIM_ID_OFF_SIT_WATCH
                    : ANIM_ID_OFF_SIT_CRASH,
        4.f
    );
}

// 0x676150
bool CTaskInteriorSitAtDesk::MakeAbortable(CPed* ped, eAbortPriority priority, CEvent const* event) {
    if (priority == ABORT_PRIORITY_IMMEDIATE) {
        if (m_Anim) {
            m_Anim->SetBlendDelta(-1000.f);
            m_Anim->SetDefaultDeleteCallback(); // 0x4CEBC0
            m_Anim = nullptr;
        }
        return true;
    }
    m_bTaskAborting = true;
    return false;
}

// 0x677920
bool CTaskInteriorSitAtDesk::ProcessPed(CPed* ped) {
    const int32 curAnimId = m_Anim ? (int32)m_Anim->m_AnimId : -1;

    ped->SetMoveState(PEDMOVE_STILL);

    // Finished => turn the ped around (as the ped stood up facing the other way)
    if (m_bTaskFinished && !RpAnimBlendClumpGetAssociation(ped->GetRpClump(), ANIM_ID_OFF_SIT_2IDLE_180)) {
        const auto heading = CGeneral::LimitRadianAngle(ped->m_fCurrentRotation + std::numbers::pi_v<float>);
        ped->m_fAimingRotation  = heading;
        ped->m_fCurrentRotation = heading;
        if (ped->m_matrix) {
            ped->m_matrix->SetRotateZOnly(heading);
        } else {
            ped->m_placement.m_fHeading = heading;
        }
        return true;
    }

    if (m_bTaskAborting) {
        if (!InteriorManager_c::AreAnimsLoaded(2)) {
            return true;
        }
        if (curAnimId == ANIM_ID_OFF_SIT_IN) {
            m_Anim->m_BlendDelta = -8.f;
        } else if (curAnimId >= ANIM_ID_OFF_SIT_IDLE_LOOP && curAnimId <= ANIM_ID_OFF_SIT_WATCH) {
            if (!m_bUpdatePedPos) {
                m_Anim->SetDeleteCallback(CDefaultAnimCallback::DefaultAnimCB, nullptr);
                m_Anim = CAnimManager::BlendAnimation(ped->GetRpClump(), ANIM_GROUP_INT_OFFICE, ANIM_ID_OFF_SIT_2IDLE_180, 1000.f);
                m_Anim->SetFinishCallback(FinishAnimCB, this);
                m_bUpdatePedPos = true;
                return false;
            }
        } else if (curAnimId == ANIM_ID_OFF_SIT_2IDLE_180) {
            m_Anim->m_Speed = 2.f;
        }
    }

    if (!m_Anim) {
        if (!InteriorManager_c::AreAnimsLoaded(2)) {
            return false;
        }
        if (m_PrevAnimId == ANIM_ID_UNDEFINED) {
            if (!m_bDoInstantly) {
                m_Anim = CAnimManager::BlendAnimation(ped->GetRpClump(), ANIM_GROUP_INT_OFFICE, ANIM_ID_OFF_SIT_IN, 4.f);
                m_Anim->SetFinishCallback(FinishAnimCB, this);
                return false;
            }
        } else if (m_PrevAnimId != ANIM_ID_OFF_SIT_IN) {
            if (m_PrevAnimId >= ANIM_ID_OFF_SIT_CRASH && m_PrevAnimId <= ANIM_ID_OFF_SIT_WATCH) {
                StartRandomLoopAnim(ped, 4.f);
            }
            return false;
        }
        m_TaskTimer.Start(m_Duration);
        StartRandomLoopAnim(ped, 1000.f);
        m_bUpdatePedPos = true;
        return false;
    }

    if (m_bUpdatePedPos) {
        CVector pos = ped->GetPosition();
        const auto z = pos.z;
        if (curAnimId >= ANIM_ID_OFF_SIT_IDLE_LOOP && curAnimId <= ANIM_ID_OFF_SIT_BORED_LOOP) {
            pos = TransformPointX87(*ped->m_matrix, CCarEnterExit::ms_vecPedDeskAnimOffset); // 0x59C890
        } else if (curAnimId == ANIM_ID_OFF_SIT_2IDLE_180) {
            pos = TransformPointX87(*ped->m_matrix, -CCarEnterExit::ms_vecPedDeskAnimOffset); // 0x59C890
        }
        pos.z = z;
        ped->SetPosn(pos);
        m_bUpdatePedPos = false;
    }

    if (m_TaskTimer.m_bStarted && m_TaskTimer.IsOutOfTime()) {
        if (m_Anim->m_AnimId != ANIM_ID_OFF_SIT_2IDLE_180) {
            m_Anim->SetDeleteCallback(CDefaultAnimCallback::DefaultAnimCB, nullptr);
            m_Anim = CAnimManager::BlendAnimation(ped->GetRpClump(), ANIM_GROUP_INT_OFFICE, ANIM_ID_OFF_SIT_2IDLE_180, 1000.f);
            m_Anim->SetFinishCallback(FinishAnimCB, this);
            m_bUpdatePedPos = true;
        }
    } else if (m_AnimTimer.m_bStarted && m_AnimTimer.IsOutOfTime()) {
        m_AnimTimer.Stop();
        if (rand() < 0x3FFF) {
            StartRandomOneOffAnim(ped);
        } else {
            StartRandomLoopAnim(ped, 4.f);
        }
    }

    // Keep the ped moving towards (and facing) the chair while sitting down
    if (m_Anim->m_AnimId == ANIM_ID_OFF_SIT_IN) {
        const auto& pedPos = ped->GetPosition();
        const auto& mat    = *ped->m_matrix; // BUG: Original code doesn't check for the matrix here, so neither do we

        // x87: Each of these is stored as a float
        const float dx = m_InteriorInfo->Pos.x - pedPos.x;
        const float dy = m_InteriorInfo->Pos.y - pedPos.y;
        const float dz = m_InteriorInfo->Pos.z - pedPos.z;

        // x87: The sum of squares + sqrt is kept in extended precision until the comparison
        const double lenD = std::sqrt((double)dz * (double)dz + (double)dy * (double)dy + (double)dx * (double)dx);
        const float  len  = (float)lenD;
        const float  clamped = lenD < (double)0.02f ? len : 0.02f;

        const double inv = 1.0 / (double)len;
        const double vx  = ((double)dx * inv) * (double)clamped;
        const double vy  = (double)(float)((double)dy * inv) * (double)clamped;
        const double vz  = (double)(float)((double)dz * inv) * (double)clamped;

        const auto& right = mat.GetRight();
        const auto& up    = mat.GetForward(); // Matrix' second vector (offset 0x10)
        ped->m_vecAnimMovingShiftLocal.x = (float)(((double)right.z * vz + (double)right.y * vy) + (double)right.x * vx);
        ped->m_vecAnimMovingShiftLocal.y = (float)(((double)up.y * vy + (double)up.z * vz) + (double)up.x * vx);

        ped->m_fAimingRotation = CGeneral::LimitRadianAngle(CGeneral::GetRadianAngleBetweenPoints(
            m_InteriorInfo->Dir.x,
            m_InteriorInfo->Dir.y,
            0.f,
            0.f
        ));
    }
    return false;
}

void CTaskInteriorSitAtDesk::StartAnim(CPed* ped, AnimationId animId, float blendDelta) {
    if (m_Anim) {
        m_Anim->SetDefaultDeleteCallback(); // 0x4CEBC0
    }
    m_Anim = CAnimManager::BlendAnimation(ped->GetRpClump(), ANIM_GROUP_INT_OFFICE, animId, blendDelta);
    m_Anim->SetFinishCallback(FinishAnimCB, this);
}

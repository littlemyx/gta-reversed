#include "StdInc.h"
#include "TaskInteriorLieInBed.h"

#include "Interior/InteriorInfo_t.h"
#include "Interior/Interior_c.h"
#include "Interior/InteriorManager_c.h"
#include "CarEnterExit.h"
#include <numbers>

void CTaskInteriorLieInBed::InjectHooks() {
    RH_ScopedVirtualClass(CTaskInteriorLieInBed, 0x870338, 9);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x675E20);
    RH_ScopedInstall(Destructor, 0x675E90);

    RH_ScopedInstall(FinishAnimCB, 0x675FC0);

    RH_ScopedVMTInstall(Clone, 0x675EF0);
    RH_ScopedVMTInstall(GetTaskType, 0x675E80);
    RH_ScopedVMTInstall(MakeAbortable, 0x675F60);
    RH_ScopedVMTInstall(ProcessPed, 0x6772E0);
}

// 0x675E20
CTaskInteriorLieInBed::CTaskInteriorLieInBed(
    int32 duration,
    InteriorInfo_t* intInfo,
    bool rightHandSide,
    bool doInstantly
) :
    m_IntInfo{intInfo},
    m_GetOutAfterInterval{duration},
    m_bDoInstantly{doInstantly},
    m_BaseAnimId{ rightHandSide ? ANIM_ID_BED_IN_R : ANIM_ID_BED_IN_L }
#ifdef FIX_BUGS
    , m_bRghtHandSide{rightHandSide}
#endif
{   
}

// For `0x675EF0`
CTaskInteriorLieInBed::CTaskInteriorLieInBed(const CTaskInteriorLieInBed& o) :
    CTaskInteriorLieInBed{
        o.m_GetOutAfterInterval,
        o.m_IntInfo,
        o.m_bRghtHandSide,
        o.m_bDoInstantly
    }
{
}

// 0x675E90
CTaskInteriorLieInBed::~CTaskInteriorLieInBed() {
    if (m_Anim) {
        m_Anim->SetDefaultDeleteCallback(); // 0x4CEBC0
    }
}

// 0x675FC0
void CTaskInteriorLieInBed::FinishAnimCB(CAnimBlendAssociation* anim, void* data) {
    const auto self = notsa::cast<CTaskInteriorLieInBed>(static_cast<CTask*>(data));

    self->m_PrevAnimId = anim->GetAnimId();

    if (self->m_PrevAnimId == self->GetAnimIdInSeq(AnimSeqIdx::GET_OUT) // Last animation in the sequence
     || self->m_TaskAborting && self->m_PrevAnimId == (AnimationId)self->m_BaseAnimId
    ) { 
        anim->SetBlendDelta(-1000.f);
        self->m_LastAnimFinished = true;
    }

    self->m_Anim = nullptr;
}

// 0x675F60
bool CTaskInteriorLieInBed::MakeAbortable(CPed* ped, eAbortPriority priority, CEvent const* event) {
    if (priority == ABORT_PRIORITY_IMMEDIATE) {
        if (m_Anim) {
            m_Anim->SetBlendDelta(-1000.f);
            m_Anim->SetDefaultDeleteCallback(); // 0x4CEBC0
            m_Anim = nullptr;
        }
        ped->GetIntelligence()->GetEventScanner().GetAcquaintanceScanner().SetOnlyScriptPedAllowed();
        return true;
    } else {
        m_TaskAborting = true;
        return false;
    }
}

namespace {
// x87-accurate version of `0x59C890` (MultiplyMatrixWithVector): each component is accumulated in extended
// precision in the order (at * z + [right|forward] * ...) and rounded to float once at the end.
CVector TransformPointX87(const CMatrix& m, const CVector& v) {
    return {
        (float)((((double)m.GetUp().x * v.z + (double)m.GetForward().x * v.y) + (double)m.GetRight().x * v.x) + m.GetPosition().x),
        (float)((((double)m.GetUp().y * v.z + (double)m.GetRight().y * v.x) + (double)m.GetForward().y * v.y) + m.GetPosition().y),
        (float)((((double)m.GetUp().z * v.z + (double)m.GetRight().z * v.x) + (double)m.GetForward().z * v.y) + m.GetPosition().z)
    };
}
}

// 0x6772E0
bool CTaskInteriorLieInBed::ProcessPed(CPed* ped) {
    const auto currAnimId = m_Anim
        ? (AnimationId)m_Anim->GetAnimId()
        : ANIM_ID_UNDEFINED;

    ped->SetMoveState(PEDMOVE_STILL);

    if (m_LastAnimFinished) {
        if (!RpAnimBlendClumpGetAssociation(ped->GetRpClump(), GetAnimIdInSeq(AnimSeqIdx::GET_OUT))) { // Check if last anim has really finished
            ped->GetIntelligence()->GetEventScanner().GetAcquaintanceScanner().SetOnlyScriptPedAllowed();
            return true;
        }
    }

    // Blends in the given animation and hooks up the finish callback
    const auto StartAnim = [&, this](AnimSeqIdx seqIdx, float blendDelta) {
        m_Anim = CAnimManager::BlendAnimation(ped->GetRpClump(), ANIM_GROUP_INT_HOUSE, GetAnimIdInSeq(seqIdx), blendDelta);
        m_Anim->SetFinishCallback(FinishAnimCB, this); // 0x4CEBE0
    };

    // Same as above, but first detaches the callback from the animation that's currently playing
    const auto SwitchToAnim = [&, this](AnimSeqIdx seqIdx, float blendDelta) {
        m_Anim->SetDefaultDeleteCallback(); // 0x4CEBC0
        StartAnim(seqIdx, blendDelta);
    };

    // Starts the get-out timer (inlined `CTaskTimer::Start`)
    const auto StartTimer = [this] {
        m_GetOutTimer.Start(m_GetOutAfterInterval);
    };

    if (m_TaskAborting) {
        if (!InteriorManager_c::AreAnimsLoaded(ANIM_GROUP_DEFAULT)) {
            ped->GetIntelligence()->GetEventScanner().GetAcquaintanceScanner().SetOnlyScriptPedAllowed();
            return true;
        }

        if (currAnimId == GetAnimIdInSeq(AnimSeqIdx::GET_IN)) {
            m_Anim->SetBlendDelta(-8.f);
        } else if (currAnimId == GetAnimIdInSeq(AnimSeqIdx::LOOP)) {
            if (!m_UpdatePedPos) {
                SwitchToAnim(AnimSeqIdx::GET_OUT, 1000.f);
                m_UpdatePedPos = true;
                return false;
            }
        } else if (currAnimId == GetAnimIdInSeq(AnimSeqIdx::GET_OUT)) {
            m_Anim->SetSpeed(3.f); // Speed (+0x24), NOT the blend delta
        }
    }

    if (m_Anim) {
        if (m_UpdatePedPos) {
            const auto animOffsetWS = [&, this]() -> CVector {
                switch (currAnimId) {
                case ANIM_ID_BED_LOOP_L:
                case ANIM_ID_BED_OUT_L:
                    return TransformPointX87(*ped->m_matrix, CCarEnterExit::ms_vecPedBedLAnimOffset);
                case ANIM_ID_BED_LOOP_R:
                case ANIM_ID_BED_OUT_R:
                    return TransformPointX87(*ped->m_matrix, CCarEnterExit::ms_vecPedBedRAnimOffset);
                default:
                    return ped->GetPosition(); // Original doesn't transform anything here
                }
            }();
            ped->SetPosn({ animOffsetWS.x, animOffsetWS.y, ped->GetPosition().z }); // 0x4241C0
            m_UpdatePedPos = false;
            if (currAnimId == GetAnimIdInSeq(AnimSeqIdx::LOOP)) {
                // x87: sum is rounded to float when passed to `LimitRadianAngle`
                const auto heading = CGeneral::LimitRadianAngle((float)((double)ped->m_fCurrentRotation + (double)std::numbers::pi_v<float>));
                ped->m_fCurrentRotation = heading;
                ped->m_fAimingRotation  = heading;
                ped->SetHeading(heading); // 0x59B020 / m_placement.m_fHeading
            }
        }

        if (m_GetOutTimer.IsOutOfTime()) {
            if (m_Anim->GetAnimId() != GetAnimIdInSeq(AnimSeqIdx::GET_OUT)) {
                SwitchToAnim(AnimSeqIdx::GET_OUT, 1000.f);
                m_UpdatePedPos = true;
            }
        }

        // Update ped's anim shift and rotation (only while getting into the bed, and
        // checking the animation that's playing *now*, as it might have just been switched)
        if (m_Anim->GetAnimId() == GetAnimIdInSeq(AnimSeqIdx::GET_IN)) {
            const auto& pedPos = ped->GetPosition();
            const auto& mat    = *ped->m_matrix; // Original doesn't check for the matrix here either

            // x87: Each of these is stored as a float
            const float dx = m_IntInfo->Pos.x - pedPos.x;
            const float dy = m_IntInfo->Pos.y - pedPos.y;
            const double dzX = (double)m_IntInfo->Pos.z - (double)pedPos.z; // `fst` stores a float copy, but st0 keeps the exact difference
            const float  dz  = (float)dzX;

            // x87: The sum of squares + sqrt is kept in extended precision until the comparison
            const double lenD    = std::sqrt(dzX * (double)dz + (double)dy * (double)dy + (double)dx * (double)dx);
            const float  len     = (float)lenD;
            const float  clamped = lenD < (double)0.02f ? len : 0.02f; // 0x858B38 is 0.02f

            const double inv = 1.0 / (double)len;
            const double vx  = ((double)dx * inv) * (double)clamped;
            const double vy  = (double)(float)((double)dy * inv) * (double)clamped;
            const double vz  = (double)(float)((double)dz * inv) * (double)clamped;

            const auto& right = mat.GetRight();
            const auto& fwd   = mat.GetForward(); // Matrix' second vector (offset 0x10)
            ped->m_vecAnimMovingShiftLocal.x = (float)(((double)right.z * vz + (double)right.y * vy) + (double)right.x * vx);
            ped->m_vecAnimMovingShiftLocal.y = (float)(((double)fwd.z * vz + (double)fwd.y * vy) + (double)fwd.x * vx);

            ped->m_fAimingRotation = CGeneral::LimitRadianAngle(CGeneral::GetRadianAngleBetweenPoints(
                m_IntInfo->Dir.x,
                m_IntInfo->Dir.y,
                0.f,
                0.f
            ));
        }
    } else if (InteriorManager_c::AreAnimsLoaded(ANIM_GROUP_DEFAULT)) { // Create animation
        if (m_PrevAnimId == ANIM_ID_UNDEFINED) {
            ped->GetIntelligence()->GetEventScanner().GetAcquaintanceScanner().TurnOffAllScanners();
            if (m_bDoInstantly) {
                StartTimer();
                StartAnim(AnimSeqIdx::LOOP, 1000.f);
                m_UpdatePedPos = true;
            } else {
                // Note: No timer, no `m_UpdatePedPos`
                StartAnim(AnimSeqIdx::GET_IN, 4.f);
            }
        } else if (m_PrevAnimId == GetAnimIdInSeq(AnimSeqIdx::GET_IN)) {
            StartTimer();
            StartAnim(AnimSeqIdx::LOOP, 1000.f);
            m_UpdatePedPos = true;
        }
    }

    return false;
}

// NOTSA
AnimationId CTaskInteriorLieInBed::GetAnimIdInSeq(AnimSeqIdx sequenceIdx) {
    using enum AnimSeqIdx;

    switch (m_BaseAnimId) {
    case ANIM_ID_BED_IN_R:
        switch (sequenceIdx) {
        case GET_IN:  return ANIM_ID_BED_IN_R;
        case LOOP:    return ANIM_ID_BED_LOOP_R;
        case GET_OUT: return ANIM_ID_BED_OUT_R;
        default: NOTSA_UNREACHABLE();
        }
    case ANIM_ID_BED_IN_L:
        switch (sequenceIdx) {
        case GET_IN:  return ANIM_ID_BED_IN_L;
        case LOOP:    return ANIM_ID_BED_LOOP_L;
        case GET_OUT: return ANIM_ID_BED_OUT_L;
        default: NOTSA_UNREACHABLE();
        }
    default:
        NOTSA_UNREACHABLE();
    }
}

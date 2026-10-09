#include "StdInc.h"

#include "TaskInteriorSitOnChair.h"
#include "Interior/InteriorManager_c.h"
#include "Interior/InteriorInfo_t.h"
#include "Interior/InteriorGroup_c.h"
#include "Ragdoll/IKChainManager.h"
#include "CarEnterExit.h"
#include <numbers>

void CTaskInteriorSitOnChair::InjectHooks() {
    RH_ScopedVirtualClass(CTaskInteriorSitOnChair, 0x870314, 9);
    RH_ScopedCategory("Tasks/TaskTypes/Interior");

    RH_ScopedInstall(Constructor, 0x675C30);
    RH_ScopedInstall(Destructor, 0x675C90);

    RH_ScopedInstall(FinishAnimCB, 0x675DD0);
    RH_ScopedVMTInstall(Clone, 0x675CF0);
    RH_ScopedVMTInstall(GetTaskType, 0x675C80);
    RH_ScopedVMTInstall(MakeAbortable, 0x675D60);
    RH_ScopedVMTInstall(ProcessPed, 0x676D30);
}

// 0x59C890 - as the original evaluates it: the sums stay in the FPU registers (extended precision), stored as float
static CVector TransformPointExt(const CMatrix& m, const CVector& v) {
    const auto &r = m.GetRight(), &f = m.GetForward(), &u = m.GetUp(), &p = m.GetPosition();
    return CVector{
        (float)((((double)u.x * v.z + (double)f.x * v.y) + (double)r.x * v.x) + p.x),
        (float)((((double)u.y * v.z + (double)r.y * v.x) + (double)f.y * v.y) + p.y),
        (float)((((double)u.z * v.z + (double)r.z * v.x) + (double)f.z * v.y) + p.z)
    };
}

// 0x675C30
CTaskInteriorSitOnChair::CTaskInteriorSitOnChair(int32 duration, InteriorInfo_t* interiorInfo, bool bDoInstantly) :
    m_Duration{ duration },
    m_InteriorInfo{ interiorInfo },
    m_bDoInstantly{ bDoInstantly }
{
}

// 0x675C90
CTaskInteriorSitOnChair::~CTaskInteriorSitOnChair() {
    if (m_Anim) {
        m_Anim->SetDefaultDeleteCallback(); // 0x4CEBC0
    }
}

// 0x675DD0
void CTaskInteriorSitOnChair::FinishAnimCB(CAnimBlendAssociation* anim, void* data) {
    const auto self = static_cast<CTaskInteriorSitOnChair*>(data);

    self->m_PrevAnimId = anim->GetAnimId();
    if (self->m_PrevAnimId == ANIM_ID_LOU_OUT) {
        anim->m_BlendDelta = -1000.f;
        self->m_bTaskFinished = true;
    }
    if (self->m_bTaskAborting && self->m_PrevAnimId == ANIM_ID_LOU_IN) {
        anim->m_BlendDelta = -1000.f;
        self->m_bTaskFinished = true;
    }
    self->m_Anim = nullptr;
}

// 0x675D60
bool CTaskInteriorSitOnChair::MakeAbortable(CPed* ped, eAbortPriority priority, const CEvent* event) {
    if (priority == ABORT_PRIORITY_IMMEDIATE) {
        if (g_ikChainMan.IsLooking(ped)) { // 0x6181A0
            g_ikChainMan.AbortLookAt(ped, 250); // 0x618280
        }
        if (m_Anim) {
            m_Anim->m_BlendDelta = -1000.f;
            m_Anim->SetDefaultDeleteCallback(); // 0x4CEBC0
            m_Anim = nullptr;
        }
        return true;
    }
    m_bTaskAborting = true;
    return false;
}

// 0x676D30
bool CTaskInteriorSitOnChair::ProcessPed(CPed* ped) {
    const int32 curAnimId = m_Anim ? (int32)m_Anim->GetAnimId() : -1;

    ped->SetMoveState(PEDMOVE_STILL); // 0x5DEC00

    // The ped stood up: we're done
    if (m_bTaskFinished && !RpAnimBlendClumpGetAssociation(ped->GetRpClump(), ANIM_ID_LOU_OUT)) {
        return true;
    }

    if (m_bTaskAborting) {
        if (!InteriorManager_c::AreAnimsLoaded(0)) { // 0x5980F0
            return true;
        }
        if (curAnimId == ANIM_ID_LOU_IN) {
            m_Anim->m_BlendDelta = -8.f;
        } else if (curAnimId == ANIM_ID_LOU_LOOP) {
            if (!m_bUpdatePedPos) {
                m_Anim->SetDefaultDeleteCallback(); // 0x4CEBC0
                m_Anim = CAnimManager::BlendAnimation(ped->GetRpClump(), ANIM_GROUP_INT_HOUSE, ANIM_ID_LOU_OUT, 1000.f); // 0x4D4610
                m_Anim->SetFinishCallback(FinishAnimCB, this); // 0x4CEBE0
                m_bUpdatePedPos = true;
                return false;
            }
        } else if (curAnimId == ANIM_ID_LOU_OUT) {
            m_Anim->m_Speed = 3.f;
        }
    }

    // Start the timer (if there's a duration), and go on with the sitting loop
    const auto StartSittingLoop = [&] {
        m_TaskTimer.Start(m_Duration);
        m_Anim = CAnimManager::BlendAnimation(ped->GetRpClump(), ANIM_GROUP_INT_HOUSE, ANIM_ID_LOU_LOOP, 1000.f); // 0x4D4610
        m_Anim->SetFinishCallback(FinishAnimCB, this); // 0x4CEBE0
        m_bUpdatePedPos = true;
    };

    if (!m_Anim) {
        if (InteriorManager_c::AreAnimsLoaded(0)) { // 0x5980F0
            if (m_PrevAnimId == ANIM_ID_UNDEFINED) {
                if (m_bDoInstantly) {
                    StartSittingLoop();
                } else {
                    m_Anim = CAnimManager::BlendAnimation(ped->GetRpClump(), ANIM_GROUP_INT_HOUSE, ANIM_ID_LOU_IN, 4.f); // 0x4D4610
                    m_Anim->SetFinishCallback(FinishAnimCB, this); // 0x4CEBE0
                }
            } else if (m_PrevAnimId == ANIM_ID_LOU_IN) {
                StartSittingLoop();
            }
        }
    } else {
        if (m_bUpdatePedPos) {
            CVector pos = ped->GetPosition();
            const auto z = pos.z;
            if (curAnimId == ANIM_ID_LOU_LOOP) {
                pos = TransformPointExt(*ped->m_matrix, CCarEnterExit::ms_vecPedChairAnimOffset); // 0x59C890; NOTE: The original doesn't check `m_matrix`

                // Turn the ped around (as it sat down facing the other way)
                const auto heading = CGeneral::LimitRadianAngle(ped->m_fCurrentRotation + std::numbers::pi_v<float>); // 0x53CB50
                ped->m_fAimingRotation  = heading;
                ped->m_fCurrentRotation = heading;
                if (ped->m_matrix) {
                    ped->m_matrix->SetRotateZOnly(heading); // 0x59B020
                } else {
                    ped->m_placement.m_fHeading = heading;
                }
            } else if (curAnimId == ANIM_ID_LOU_OUT) {
                pos = TransformPointExt(*ped->m_matrix, CCarEnterExit::ms_vecPedChairAnimOffset); // 0x59C890
            }
            pos.z = z;
            ped->SetPosn(pos); // 0x4241C0
            m_bUpdatePedPos = false;
        }

        if (m_TaskTimer.m_bStarted && m_TaskTimer.IsOutOfTime()) { // 0x420E30
            if (m_Anim->GetAnimId() != ANIM_ID_LOU_OUT) {
                m_Anim->SetDefaultDeleteCallback(); // 0x4CEBC0
                m_Anim = CAnimManager::BlendAnimation(ped->GetRpClump(), ANIM_GROUP_INT_HOUSE, ANIM_ID_LOU_OUT, 1000.f); // 0x4D4610
                m_Anim->SetFinishCallback(FinishAnimCB, this); // 0x4CEBE0
                m_bUpdatePedPos = true;
            }
        }

        // Keep the ped moving towards (and facing) the chair while sitting down
        if (m_Anim->GetAnimId() == ANIM_ID_LOU_IN) {
            const auto& pedPos = ped->GetPosition();
            const auto& mat    = *ped->m_matrix; // NOTE: The original doesn't check `m_matrix`

            // x87: dx and dy are stored as floats, dz is stored but also kept (unrounded) on the FPU stack
            const float  dx  = m_InteriorInfo->Pos.x - pedPos.x;
            const float  dy  = m_InteriorInfo->Pos.y - pedPos.y;
            const double dzE = (double)m_InteriorInfo->Pos.z - (double)pedPos.z;
            const float  dz  = (float)dzE;

            // x87: The sum of squares + sqrt is kept in extended precision until the comparison
            const double lenD    = std::sqrt(dzE * (double)dz + (double)dy * (double)dy + (double)dx * (double)dx);
            const float  len     = (float)lenD;
            const float  clamped = lenD < (double)0.02f ? len : 0.02f; // FCOMP + JP: NaN takes the constant

            const double inv = 1.0 / (double)len;
            const double vx  = ((double)dx * inv) * (double)clamped;
            const double vy  = (double)(float)((double)dy * inv) * (double)clamped;
            const double vz  = (double)(float)((double)dz * inv) * (double)clamped;

            const auto& right = mat.GetRight();
            const auto& fwd   = mat.GetForward();
            ped->m_vecAnimMovingShiftLocal.x = (float)(((double)right.z * vz + (double)right.y * vy) + (double)right.x * vx);
            ped->m_vecAnimMovingShiftLocal.y = (float)(((double)fwd.z * vz + (double)fwd.y * vy) + (double)fwd.x * vx);

            ped->m_fAimingRotation = CGeneral::LimitRadianAngle(CGeneral::GetRadianAngleBetweenPoints( // 0x53CB50, 0x53CBE0
                m_InteriorInfo->Dir.x,
                m_InteriorInfo->Dir.y,
                0.f,
                0.f
            ));
        }
    }

    // Look around from time to time while sitting
    if (m_Anim && m_Anim->GetAnimId() == ANIM_ID_LOU_LOOP) {
        if (g_ikChainMan.IsLooking(ped)) { // 0x6181A0
            return false;
        }
        if (CGeneral::GetRandomNumberInRange(0, 1000) > 980) { // 0x407180
            if (const auto grp = g_interiorMan.GetPedsInteriorGroup(ped)) { // 0x598240
                InteriorInfo_t* info{};
                Interior_c*     interior{};
                float           distSq{};
                grp->FindClosestInteriorInfo(0, ped->GetPosition(), 10.f, &info, &interior, &distSq); // 0x594A50
                if (info) {
                    g_ikChainMan.LookAt( // 0x618970
                        "TaskSitInChair",
                        ped,
                        nullptr,
                        CGeneral::GetRandomNumberInRange(10'000, 20'000), // 0x407180
                        BONE_UNKNOWN,
                        &info->Pos,
                        false,
                        0.25f,
                        500,
                        3,
                        false
                    );
                }
            }
        }
        return false;
    }

    if (g_ikChainMan.IsLooking(ped)) { // 0x6181A0
        g_ikChainMan.AbortLookAt(ped, 250); // 0x618280
    }
    return false;
}

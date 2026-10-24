#include "StdInc.h"

#include "TaskSimpleSlideToCoord.h"
#include "TaskSimpleStandStill.h"

void CTaskSimpleSlideToCoord::InjectHooks() {
    RH_ScopedVirtualClass(CTaskSimpleSlideToCoord, 0x86FFEC, 9);
    RH_ScopedCategory("Tasks/TaskTypes");

    // + RH_ScopedOverloadedInstall(Constructor, "NoAnim", 0x66C3E0, CTaskSimpleSlideToCoord*(CTaskSimpleSlideToCoord::*)(CVector const&, float, float));
    // + RH_ScopedOverloadedInstall(Constructor, "Anim", 0x66C450, CTaskSimpleSlideToCoord*(CTaskSimpleSlideToCoord::*)(CVector const&, float, float, char const*, char const*, int32, float, bool, int32));
    // + RH_ScopedVmtInstall(MakeAbortable, 0x66C4D0);
    RH_ScopedVMTInstall(ProcessPed, 0x66C4E0);
}

// 0x66C3E0
CTaskSimpleSlideToCoord::CTaskSimpleSlideToCoord(const CVector& slideToPos, float aimingRotation, float speed) :
    CTaskSimpleRunNamedAnim(),
    m_SlideToPos{ slideToPos },
    m_fAimingRotation{ aimingRotation },
    m_Speed{ speed },
    m_bFirstTime{ true },
    m_bRunningAnim{ false }
{
    // m_Timer not initialized
}

// 0x66C450
CTaskSimpleSlideToCoord::CTaskSimpleSlideToCoord(const CVector& slideToPos, float aimingRotation, float speed, const char* animBlockName, const char* animGroupName, uint32 animFlags, float animBlendDelta, bool bRunInSequence, uint32 endTime) :
    CTaskSimpleRunNamedAnim{ animBlockName, animGroupName, animFlags, animBlendDelta, endTime, false, bRunInSequence, false, false },
    m_SlideToPos{ slideToPos },
    m_fAimingRotation{ aimingRotation },
    m_Speed{ speed },
    m_bFirstTime{ true },
    m_bRunningAnim{ false },
    m_Timer{ -1 }
{
}

// 0x66D300
CTask* CTaskSimpleSlideToCoord::Clone() const {
    return m_bRunningAnim
        ? new CTaskSimpleSlideToCoord(m_SlideToPos, m_fAimingRotation, m_Speed, m_animName, m_animGroupName, m_animFlags, m_fBlendDelta, !!m_bRunInSequence, m_Time)
        : new CTaskSimpleSlideToCoord(m_SlideToPos, m_fAimingRotation, m_Speed);
}

// 0x66C4D0
bool CTaskSimpleSlideToCoord::MakeAbortable(CPed* ped, eAbortPriority priority, CEvent const* event) {
    return m_bRunningAnim ? CTaskSimpleAnim::MakeAbortable(ped, priority, event) : true;
}

// 0x66C4E0
bool CTaskSimpleSlideToCoord::ProcessPed(CPed* ped) {
    bool bAnimOK = true;
    if (m_bRunningAnim) {
        bAnimOK = CTaskSimpleRunNamedAnim::ProcessPed(ped); // 0x61BF20
    }

    // NOTE: `m_Timer` (+0x7C) is not the same as `CTaskSimpleRunNamedAnim::m_Timer`
    if (m_Timer == -1) {
        if (!m_bRunningAnim) {
            m_Timer = CTimer::GetTimeInMS() + 2000;
        } else if (bAnimOK) {
            m_Timer = CTimer::GetTimeInMS() + 500;
        }
    }

    if (m_bFirstTime) {
        CTaskSimpleStandStill standStill{ STAND_STILL_TIME, false, false, 8.f }; // 0x62F310
        standStill.ProcessPed(ped); // 0x62F370
        if (ped->IsPlayer()) {
            ped->GetTaskManager().GetTaskPrimary(TASK_PRIMARY_DEFAULT)->MakeAbortable(ped, ABORT_PRIORITY_IMMEDIATE, nullptr); // BUG: Not checked for null
        }
        ped->m_fAimingRotation = m_fAimingRotation; // +0x55C
        m_bFirstTime = false;
    }

    // x87: The differences/sum aren't rounded to float
    const auto&  pedPos = ped->GetPosition();
    const x87d dx     = (x87d)pedPos.x - (x87d)m_SlideToPos.x;
    const x87d dy     = (x87d)pedPos.y - (x87d)m_SlideToPos.y;
    const x87d distSq = dy * dy + dx * dx;
    const x87d minSq  = (x87d)0.05f * (x87d)0.05f; // 0x86FD10

    if (!(distSq >= minSq)) { // (FCOMPP, JNZ on C0: NaN takes this path)
        ped->m_vecAnimMovingShiftLocal = CVector2D{ 0.f, 0.f };
    } else {
        const auto& mat = *ped->m_matrix; // BUG: Not checked for null
        const x87d a  = (x87d)m_SlideToPos.x - (x87d)pedPos.x;
        const x87d b  = (x87d)m_SlideToPos.y - (x87d)pedPos.y;
        const float  cf = (float)((x87d)m_SlideToPos.z - (x87d)pedPos.z);
        const x87d spd = (x87d)m_Speed;

        const float  axf = (float)(a * spd);
        const x87d by  = b * spd;
        const x87d cz  = (x87d)cf * spd;
        const float  czf = (float)cz;

        const auto &r = mat.GetRight(), &f = mat.GetForward();
        const x87d fwd   = (cz * f.z + by * f.y) + (x87d)axf * f.x;
        const x87d right = ((x87d)czf * r.z + by * r.y) + (x87d)axf * r.x;
        ped->m_vecAnimMovingShiftLocal = CVector2D{ (float)right, (float)fwd };
    }

    if ((uint32)m_Timer < CTimer::GetTimeInMS()) {
        return true;
    }

    if (bAnimOK && distSq < minSq) {
        // 0x86FD14 = 0.1f
        if (std::abs(CGeneral::LimitRadianAngle(ped->m_fCurrentRotation - ped->m_fAimingRotation)) < 0.1f) {
            return true;
        }
    }
    return false;
}

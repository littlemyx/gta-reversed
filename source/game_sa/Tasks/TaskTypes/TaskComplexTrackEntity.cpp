#include "StdInc.h"
#include "TaskComplexTrackEntity.h"
#include "TaskSimpleGoToPointFine.h"
#include "TaskSimpleStandStill.h"
#include "TaskComplexFollowNodeRoute.h"

void CTaskComplexTrackEntity::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexTrackEntity, 0x86F998, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x65F3B0);
    RH_ScopedInstall(Destructor, 0x65F460);

    RH_ScopedInstall(SetOffsetPos, 0x65F760);
    RH_ScopedInstall(CalcTargetPos, 0x65F780);
    RH_ScopedInstall(CalcMoveRatio, 0x65F930);

    RH_ScopedVMTInstall(Clone, 0x65F4E0);
    RH_ScopedVMTInstall(GetTaskType, 0x65F450);
    RH_ScopedVMTInstall(MakeAbortable, 0x65F4C0);
    RH_ScopedVMTInstall(CreateNextSubTask, 0x65F590);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x65F700);
    RH_ScopedVMTInstall(ControlSubTask, 0x663640, { .Locked = true }); // Locked because it fucks up the stack and crashes 
}

// 0x65F3B0
CTaskComplexTrackEntity::CTaskComplexTrackEntity(CEntity* entity, CVector offsetPos, uint8 a6, int32 a7, float rangeMin, float rangeMax, uint8 a10) :
    m_rangeMax{rangeMax},
    m_rangeMin{rangeMin},
    m_toTrack{entity},
    m_offsetPosn{offsetPos},
    a{a6},
    b{a7},
    f{a10}
{
    CEntity::RegisterReference(m_toTrack);
}

// NOTSA
CTaskComplexTrackEntity::CTaskComplexTrackEntity(const CTaskComplexTrackEntity& o) :
    CTaskComplexTrackEntity{o.m_toTrack, o.m_offsetPosn, o.a, o.b, o.m_rangeMin, o.m_rangeMax, o.f}
{
    m_fMoveRatio = o.m_fMoveRatio;
}

// 0x65F460
CTaskComplexTrackEntity::~CTaskComplexTrackEntity() {
    CEntity::SafeCleanUpRef(m_toTrack); // 0x65F483: the exe skips the call when there is no tracked entity
}

// 0x65F760
void CTaskComplexTrackEntity::SetOffsetPos(CVector posn) {
    m_offsetPosn = posn;
}

// 0x65F780
void CTaskComplexTrackEntity::CalcTargetPos(CPed* ped) {
    // NOTE: Products/sums below mirror the x87 code: values kept on the FPU stack are `double`, spilled ones are rounded to `float`
    m_goToPos = m_toTrack->GetPosition();

    if (!f) { // NOTE: The asm tests the byte at 0x2C (`f`), not 0x1C (`a`) like `CalcMoveRatio` does
        m_goToPos.x = (float)((x87d)m_offsetPosn.x + (x87d)m_goToPos.x);
        m_goToPos.y = (float)((x87d)m_offsetPosn.y + (x87d)m_goToPos.y);
    } else {
        const CVector right = m_toTrack->GetMatrix().GetRight();
        const CVector fwd   = m_toTrack->GetMatrix().GetForward();

        const x87d offX = m_offsetPosn.x;
        m_goToPos.x = (float)((x87d)right.x * offX + (x87d)m_goToPos.x);
        const float ry = (float)((x87d)right.y * offX);
        const float rz = (float)((x87d)right.z * offX);
        m_goToPos.y = (float)((x87d)ry + (x87d)m_goToPos.y);
        m_goToPos.z = (float)((x87d)rz + (x87d)m_goToPos.z);

        const x87d offY = m_offsetPosn.y;
        m_goToPos.x = (float)((x87d)fwd.x * offY + (x87d)m_goToPos.x);
        const float fy = (float)((x87d)fwd.y * offY);
        const float fz = (float)((x87d)fwd.z * offY);
        m_goToPos.y = (float)((x87d)fy + (x87d)m_goToPos.y);
        m_goToPos.z = (float)((x87d)fz + (x87d)m_goToPos.z);
    }

    // Predict where the tracked entity will be
    if (m_toTrack->GetType() == ENTITY_TYPE_VEHICLE || m_toTrack->GetType() == ENTITY_TYPE_PED) {
        const auto&  speed = m_toTrack->AsPhysical()->m_vecMoveSpeed;
        const x87d ts    = CTimer::GetTimeStep();
        const x87d px    = ts * speed.x;
        const x87d py    = ts * speed.y;
        const float  pz    = (float)(ts * speed.z);
        m_goToPos.x = (float)(px + (x87d)m_goToPos.x);
        m_goToPos.y = (float)(py + (x87d)m_goToPos.y);
        m_goToPos.z = (float)((x87d)pz + (x87d)m_goToPos.z);
    }

    const auto&  pedPos = ped->GetPosition();
    const x87d dx     = (x87d)m_goToPos.x - (x87d)pedPos.x;
    const x87d dy     = (x87d)m_goToPos.y - (x87d)pedPos.y;
    m_distToTargetSq    = (float)(dy * dy + dx * dx);
}

// 0x65F930
void CTaskComplexTrackEntity::CalcMoveRatio(CPed* ped) {
    // Function-local statics in the original (lazily initialised, guarded by flags in 0xC18D08)
    constexpr float RANGE_MIN = 0.2f, RANGE_MID = 1.f, RANGE_MAX = 5.f;
    static const float sqMin    = (float)((x87d)RANGE_MIN * (x87d)RANGE_MIN); // 0xC18D04
    static const float sqMid    = (float)((x87d)RANGE_MID * (x87d)RANGE_MID); // 0xC18D00
    static const float sqMax    = (float)((x87d)RANGE_MAX * (x87d)RANGE_MAX); // 0xC18CFC
    static const float scaleHi  = (float)((x87d)1.0 / ((x87d)RANGE_MAX - (x87d)RANGE_MID)); // 0xC18CF8
    static const float scaleLow = (float)((x87d)1.0 / ((x87d)RANGE_MID - (x87d)RANGE_MIN)); // 0xC18CF4

    if (m_distToTargetSq < sqMin) {
        float40 = 0.f;
    } else if (m_distToTargetSq > sqMax) {
        float40 = 1.f;
    } else {
        const x87d dist = std::sqrt((x87d)m_distToTargetSq);
        if (m_distToTargetSq < sqMid) {
            float40 = (float)((dist - (x87d)RANGE_MIN) * (x87d)scaleLow * (x87d)0.5);
        } else {
            float40 = (float)((dist - (x87d)RANGE_MID) * (x87d)scaleHi * (x87d)0.5 + (x87d)0.5);
        }
    }

    const x87d ratio = std::sqrt((x87d)float40) * (x87d)3.0;
    float40 = (float)ratio;
    if (!a && ratio > 2.0) { // FCOMP on the unrounded value; NaN => not taken
        float40 = 2.0f;
    }

    if ((x87d)float40 - (x87d)m_fMoveRatio > (double)0.2f) { // NaN => copy
        m_fMoveRatio = (float)((x87d)0.2f + (x87d)m_fMoveRatio);
    } else {
        m_fMoveRatio = float40;
    }
}

// 0x65F4C0
bool CTaskComplexTrackEntity::MakeAbortable(CPed* ped, eAbortPriority priority, CEvent const* event) {
    return m_pSubTask->MakeAbortable(ped, priority, event);
}

// 0x65F590
CTask* CTaskComplexTrackEntity::CreateNextSubTask(CPed* ped) {
    if (!m_toTrack) {
        return nullptr;
    }

    if (!m_pSubTask || m_pSubTask->GetTaskType() != TASK_SIMPLE_STAND_STILL) {
        return new CTaskSimpleStandStill{ 50 }; // 0x65F6C7 `push 0x32`: the exe stands still for 50 ms (the port passed the default 0)
    }

    if ((double)m_distToTargetSq < (x87d)m_rangeMin * (x87d)m_rangeMin) { // 0x65F5D0..0x65F5E3: x87 product in extended precision, strict compare (NaN / equal => follow route)
        return new CTaskSimpleGoToPointFine{ m_fMoveRatio, m_goToPos, 0.25f, nullptr };
    }

    return new CTaskComplexFollowNodeRoute{
        PEDMOVE_RUN,
        m_toTrack->GetPosition(),
        0.5f,
        0.2f,
        2.0f,
        false,
        -1,
        true
    };
}

// 0x65F700
CTask* CTaskComplexTrackEntity::CreateFirstSubTask(CPed* ped) {
    if (m_fMoveRatio < 0.0) {
        m_fMoveRatio = [ped] {
            switch (ped->m_nMoveState) {
            case 1: return 0.0f;
            case 4: return 1.0f;
            case 6: return 2.0f;
            default: return 3.0f;
            }
        }();
    }
    return CreateNextSubTask(ped);
}

// 0x663640
CTask* CTaskComplexTrackEntity::ControlSubTask(CPed* ped) {
    const auto TryAbort = [this, ped] {
        return MakeAbortable(ped);
    };

    const auto TryAbortGetTask = [&, this] {
        return TryAbort() ? nullptr : m_pSubTask;
    };

    if (!m_toTrack) {
        return TryAbortGetTask();
    }

    assert(!gap2 && !gap3); // NOTE: Both seem to be always be false, let's see if that's the case.

    if (gap2) {
        if (gap3) {
            m_someStartTimeMs = CTimer::GetTimeInMS();
            gap3 = false;
        }
        // 0x66366D: start + interval (the port added the start time twice)
        if (CTimer::GetTimeInMS() >= m_someStartTimeMs + m_expirationInterval) {
            return TryAbortGetTask();
        }
    }

    CalcTargetPos(ped);

    switch (m_pSubTask->GetTaskType()) {
    case TASK_COMPLEX_FOLLOW_POINT_ROUTE: {
        // If we're totally out of range...
        if (m_distToTargetSq > sq(m_rangeMax)) {
            return nullptr;
        }

        // If we're now in range be a little more precise and create `TASK_SIMPLE_GO_TO_POINT_FINE`
        if (m_distToTargetSq < sq(m_rangeMin) && TryAbort()) {
            return CreateNextSubTask(ped);
        }

        break;
    }
    case TASK_SIMPLE_GO_TO_POINT_FINE: {
        // Check if we're still in range, if not, abort and create `TASK_COMPLEX_FOLLOW_POINT_ROUTE`
        if (m_distToTargetSq >= sq(m_rangeMin) && TryAbort()) {
            return CreateNextSubTask(ped);
        }

        CalcMoveRatio(ped);

        const auto gotoTask = static_cast<CTaskSimpleGoToPointFine*>(m_pSubTask);
        gotoTask->SetTargetPos(m_goToPos);
        gotoTask->SetMoveRatio(m_fMoveRatio);

        break;
    }
    }

    return m_pSubTask;
}

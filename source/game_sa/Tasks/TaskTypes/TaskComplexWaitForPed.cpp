#include "StdInc.h"

#include "TaskComplexWaitForPed.h"
#include "TaskComplexWalkAlongsidePed.h"
#include "TaskSimpleStandStill.h"
#include "EventDeadPed.h"
#include "General.h"

void CTaskComplexWaitForPed::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexWaitForPed, 0x8706F8, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x683340);
    RH_ScopedInstall(Destructor, 0x6833D0);

    RH_ScopedVMTInstall(Clone, 0x683950);
    RH_ScopedVMTInstall(GetTaskType, 0x6833C0);
    RH_ScopedVMTInstall(MakeAbortable, 0x683430);
    RH_ScopedVMTInstall(CreateNextSubTask, 0x683440);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x683450);
    RH_ScopedVMTInstall(ControlSubTask, 0x6834E0);
}

// 0x683340
CTaskComplexWaitForPed::CTaskComplexWaitForPed(CPed* ped, float radius, uint32 timeInMs, bool rotateOtherPedsToWaitingPed) :
    m_ped{ ped },
    m_radius{ radius },
    m_timeInMs{ timeInMs },
    m_bRotateOtherPedsToWaitingPed{ rotateOtherPedsToWaitingPed },
    m_timer{},
    m_framesToWaitForSettingRotation{} // NOTSA: the original leaves it uninitialised (CreateFirstSubTask always sets it before use)
{
    CEntity::SafeRegisterRef(m_ped);
}

// 0x6833D0
CTaskComplexWaitForPed::~CTaskComplexWaitForPed() {
    CEntity::SafeCleanUpRef(m_ped);
}

// 0x683950
CTask* CTaskComplexWaitForPed::Clone() const {
    return new CTaskComplexWaitForPed{ m_ped, m_radius, m_timeInMs, m_bRotateOtherPedsToWaitingPed };
}

// 0x683450
CTask* CTaskComplexWaitForPed::CreateFirstSubTask(CPed* ped) {
    if (!m_ped) {
        return nullptr;
    }

    // Not `CTaskTimer::Start`: the original stores unconditionally and leaves `m_bStopped` alone
    m_timer.m_nStartTime = CTimer::GetTimeInMS();
    m_timer.m_nInterval  = (int32)m_timeInMs;
    m_timer.m_bStarted   = true;

    m_framesToWaitForSettingRotation = 0;

    return new CTaskSimpleStandStill{ 999999, false, false, 8.f };
}

// 0x59C890 - as the original evaluates it: the sum stays in the FPU registers (extended precision), stored as float
static CVector TransformPointExt(const CMatrix& m, const CVector& v) {
    const auto &r = m.GetRight(), &f = m.GetForward(), &u = m.GetUp(), &p = m.GetPosition();
    return CVector{
        (float)((((double)u.x * v.z + (double)f.x * v.y) + (double)r.x * v.x) + p.x),
        (float)((((double)u.y * v.z + (double)r.y * v.x) + (double)f.y * v.y) + p.y),
        (float)((((double)u.z * v.z + (double)r.z * v.x) + (double)f.z * v.y) + p.z)
    };
}

// 0x6834E0
CTask* CTaskComplexWaitForPed::ControlSubTask(CPed* ped) {
    if (!m_ped) {
        return nullptr;
    }

    if (!m_ped->IsAlive()) { // 0x5E0170
        CEventDeadPed event{ m_ped, false, CTimer::GetTimeInMS() }; // 0x4ADEA0
        event.m_TaskId = TASK_COMPLEX_INVESTIGATE_DEAD_PED;
        ped->GetIntelligence()->m_eventGroup.Add(&event, false); // 0x4AB420
        return nullptr;
    }

    if (m_timer.m_bStarted && m_timer.IsOutOfTime()) { // 0x420E30
        return nullptr;
    }

    // Offset from the origin of this ped to the waited-for ped. If the latter is walking alongside someone,
    // the walk offset (transformed by *our* matrix, which is not null-checked) is used as our position
    const auto&  partnerPos = m_ped->GetPosition();
    double       dx, dy, dz;
    if (const auto walk = static_cast<CTaskComplexWalkAlongsidePed*>(m_ped->GetIntelligence()->FindTaskByType(TASK_COMPLEX_WALK_ALONGSIDE_PED))) { // 0x600EE0
        const auto pos = TransformPointExt(*ped->m_matrix, walk->GetOffset()); // 0x59C890
        dx = (double)pos.x - partnerPos.x;
        dy = (double)pos.y - partnerPos.y;
        dz = (double)pos.z - partnerPos.z;
    } else {
        const auto& pedPos = ped->GetPosition();
        dx = (double)pedPos.x - partnerPos.x;
        dy = (double)pedPos.y - partnerPos.y;
        dz = (double)pedPos.z - partnerPos.z;
    }

    // Extended precision sum, evaluated z, y, x
    const double distSq = (dz * dz + dy * dy) + dx * dx;
    const double radius = m_radius;
    if (distSq < radius * radius) { // FCOMPP + JP: NaN continues
        return nullptr;
    }

    if (m_bRotateOtherPedsToWaitingPed && m_framesToWaitForSettingRotation == 0) {
        const CVector toPartner = m_ped->GetPosition() - ped->GetPosition(); // 0x40FE60
        ped->m_fCurrentRotation = CGeneral::LimitRadianAngle(CGeneral::GetRadianAngleBetweenPoints(toPartner.x, toPartner.y, 0.f, 0.f)); // 0x53CBE0, 0x53CB50
    }

    m_framesToWaitForSettingRotation++;
    if (m_framesToWaitForSettingRotation > 10) {
        m_framesToWaitForSettingRotation = 0;
    }

    return m_pSubTask;
}

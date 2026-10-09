#include "StdInc.h"

#include "TaskComplexFollowPedFootsteps.h"
// #include "PointRoute.h"
#include "TaskSimpleGoToPoint.h"
#include "TaskSimpleStandStill.h"
#include "TaskSimpleHitHead.h"
#include "SeekEntity/TaskComplexSeekEntity.h"
#include "SeekEntity/PosCalculators/EntitySeekPosCalculatorStandard.h"

void CTaskComplexFollowPedFootsteps::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexFollowPedFootsteps, 0x870CC0, 12);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x694E20);

    RH_ScopedVMTInstall(MakeAbortable, 0x694ED0);
    RH_ScopedVMTInstall(CreateNextSubTask, 0x694EE0);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x695000);
    RH_ScopedVMTInstall(ControlSubTask, 0x695090);
    RH_ScopedVMTInstall(CreateSubTask, 0x695E40);
}

// 0x694E20
CTaskComplexFollowPedFootsteps::CTaskComplexFollowPedFootsteps(CPed* ped) : CTaskComplex() {
    m_targetPed               = ped;
    m_updateGoToPoint         = false;
    m_subTaskCreateCheckTimer = CTimer::GetTimeInMS();
    m_lineOfSightCheckTimer   = 0;
    m_pointRoute              = nullptr;
    m_moveState               = PEDMOVE_WALK;

    CEntity::SafeRegisterRef(m_targetPed);

    m_pointRoute = new CPointRoute();
}

CTaskComplexFollowPedFootsteps* CTaskComplexFollowPedFootsteps::Constructor(CPed* ped) {
    this->CTaskComplexFollowPedFootsteps::CTaskComplexFollowPedFootsteps(ped);
    return this;
}

CTaskComplexFollowPedFootsteps::~CTaskComplexFollowPedFootsteps() {
    CEntity::SafeCleanUpRef(m_targetPed);

    delete m_pointRoute;
    m_pointRoute = nullptr;
}

// 0x694ED0 (a jump to the sub-task's virtual `MakeAbortable`)
bool CTaskComplexFollowPedFootsteps::MakeAbortable(CPed* ped, eAbortPriority priority, const CEvent* event) {
    return m_pSubTask->MakeAbortable(ped, priority, event);
}

// 0x694EE0
CTask* CTaskComplexFollowPedFootsteps::CreateNextSubTask(CPed* ped) {
    if (!m_targetPed) {
        return CreateSubTask(TASK_FINISHED, ped);
    }
    switch (m_pSubTask->GetTaskType()) {
    case TASK_SIMPLE_HIT_HEAD:
        return CreateSubTask(TASK_SIMPLE_STAND_STILL, ped);
    case TASK_NONE:
        return CreateSubTask(TASK_FINISHED, ped);
    case TASK_SIMPLE_STAND_STILL:
    case TASK_SIMPLE_GO_TO_POINT:
        if (m_pointRoute && m_pointRoute->m_NumEntries) {
            return CreateSubTask(TASK_SIMPLE_GO_TO_POINT, ped);
        }
        return CreateSubTask(TASK_SIMPLE_STAND_STILL, ped);
    case TASK_COMPLEX_SEEK_ENTITY: {
        const auto& targetPos = m_targetPed->GetPosition();
        const auto& pedPos    = ped->GetPosition();
        const double dx = (double)targetPos.x - pedPos.x;
        const double dy = (double)targetPos.y - pedPos.y;
        const double dz = (double)targetPos.z - pedPos.z;
        const double distSq = (dz * dz + dx * dx) + dy * dy; // x87, no float rounding
        constexpr float SEEK_DIST = 1.4f; // 0x870C20
        if (distSq > (double)SEEK_DIST) { // NOTE: Compared against the distance itself, not its square (as in the original)
            return CreateSubTask(TASK_COMPLEX_SEEK_ENTITY, ped);
        }
        return CreateSubTask(TASK_SIMPLE_STAND_STILL, ped);
    }
    default:
        return CreateSubTask(TASK_FINISHED, ped);
    }
}

// 0x695000
CTask* CTaskComplexFollowPedFootsteps::CreateFirstSubTask(CPed* ped) {
    if (!m_targetPed) {
        return CreateSubTask(TASK_FINISHED, ped);
    }
    const auto& pedPos    = ped->GetPosition();
    const auto& targetPos = m_targetPed->GetPosition();
    const double dx = (double)targetPos.x - pedPos.x;
    const double dy = (double)targetPos.y - pedPos.y;
    const double dz = (double)targetPos.z - pedPos.z;
    const double distSq = (dx * dx + dz * dz) + dy * dy; // x87, no float rounding
    constexpr float SEEK_DIST = 1.4f; // 0x870C20
    if ((double)SEEK_DIST < distSq) { // NOTE: Compared against the distance itself, not its square (as in the original)
        return CreateSubTask(TASK_COMPLEX_SEEK_ENTITY, ped);
    }
    return CreateSubTask(TASK_SIMPLE_STAND_STILL, ped);
}

//! 0x59C910 - `CVector::Normalise` as the original evaluates it (sum of squares and reciprocal root in extended precision).
static void NormaliseOriginal(CVector& v) {
    const double sumSq = ((double)v.x * v.x + (double)v.y * v.y) + (double)v.z * v.z;
    if (sumSq <= 0.0) { // NaN takes the sqrt path
        v.x = 1.0f;
        return;
    }
    const double recip = 1.0 / std::sqrt(sumSq);
    v.x = (float)(v.x * recip);
    v.y = (float)(v.y * recip);
    v.z = (float)(v.z * recip);
}

//! 0x406DA0 - Squared magnitude, left in ST0 (extended precision) by the original: (x^2 + y^2) + z^2
static double SqMagnitudeOriginal(const CVector& v) {
    return ((double)v.x * v.x + (double)v.y * v.y) + (double)v.z * v.z;
}

// 0x695090
CTask* CTaskComplexFollowPedFootsteps::ControlSubTask(CPed* ped) {
    CTask* ret = m_pSubTask;

    const auto Finish = [&](eTaskType newTaskType) -> CTask* { // 0x6955A2
        if (m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_URGENT, nullptr)) {
            ret = CreateSubTask(newTaskType, ped);
        }
        return ret;
    };

    if (!m_targetPed || !m_pointRoute || !m_targetPed->IsAlive()) {
        return Finish(TASK_FINISHED);
    }

    if (m_pSubTask->GetTaskType() == TASK_COMPLEX_SEEK_ENTITY) {
        return m_pSubTask;
    }

    if (m_targetPed->bInVehicle) { // 0x46C & 0x100
        return Finish(TASK_COMPLEX_SEEK_ENTITY);
    }

    constexpr int32 MAX_STATIC_COUNTER = 30; // 0x86C938
    if (ped->GetIntelligence()->m_AnotherStaticCounter > MAX_STATIC_COUNTER) {
        return Finish(TASK_SIMPLE_HIT_HEAD);
    }

    const auto  now     = CTimer::GetTimeInMS();
    const auto& pedPos  = ped->GetPosition();
    if (now - m_lineOfSightCheckTimer > 500u) {
        m_lineOfSightCheckTimer = now;
        m_updateGoToPoint = CWorld::GetIsLineOfSightClear(pedPos, m_targetPed->GetPosition(), true, false, false, true, false, false, false);
    }

    const CVector targetPos = m_targetPed->GetPosition();
    const CVector delta     = targetPos - pedPos; // 0x40FE60
    const float   sqDist    = (float)SqMagnitudeOriginal(delta);
    const float   dist2D    = (float)std::sqrt((double)delta.y * delta.y + (double)delta.x * delta.x);

    constexpr float NEAR_DIST = 1.4f;  // 0x870C20
    constexpr float FAR_DIST  = 8.f;   // 0x870C38
    if ((double)NEAR_DIST * NEAR_DIST > sqDist && m_pSubTask->GetTaskType() != TASK_SIMPLE_STAND_STILL) {
        if (!m_updateGoToPoint) {
            return ret;
        }
        m_pointRoute->Clear();
        return Finish(TASK_SIMPLE_STAND_STILL);
    }

    if ((double)FAR_DIST * FAR_DIST < sqDist) {
        return Finish(TASK_COMPLEX_SEEK_ENTITY);
    }
    if (dist2D < 1.f && std::abs(delta.z) > 2.f) { // 0x858624, 0x858CA0
        return Finish(TASK_COMPLEX_SEEK_ENTITY);
    }

    if (m_updateGoToPoint) {
        m_pointRoute->Clear();
        m_pointRoute->AddUnlessFull(m_targetPed->GetPosition()); // 0x48E180
        m_subTaskCreateCheckTimer = CTimer::GetTimeInMS() - 332;
        if (CTask::IsGoToTask(ped->GetTaskManager().GetSimplestActiveTask())) { // 0x61A360
            static_cast<CTaskSimpleGoToPoint*>(ped->GetTaskManager().GetSimplestActiveTask())->UpdatePoint(m_targetPed->GetPosition(), 0.5f, false); // 0x645700
        }
    }

    if (CTimer::GetTimeInMS() - m_subTaskCreateCheckTimer < 166u || m_pointRoute->m_NumEntries >= 8u) {
        if (m_pSubTask->GetTaskType() != TASK_SIMPLE_STAND_STILL) {
            return ret;
        }
        if (!((int32)m_pointRoute->m_NumEntries > 1)) {
            return ret;
        }
        return Finish(TASK_SIMPLE_GO_TO_POINT);
    }

    m_subTaskCreateCheckTimer = CTimer::GetTimeInMS();

    const auto numPoints = m_pointRoute->m_NumEntries;
    bool       addPoint  = true;
    if (numPoints != 0) {
        const CVector last = m_pointRoute->m_Entries[numPoints - 1];
        CVector       d    = targetPos - last;
        if ((double)0.35f * 0.35f < SqMagnitudeOriginal(d)) { // 0x870C24
            if (m_pointRoute->m_NumEntries >= 2) { // Re-read, as in the original
                const CVector prev = m_pointRoute->m_Entries[m_pointRoute->m_NumEntries - 2];
                CVector       e    = last - prev;
                NormaliseOriginal(e);
                NormaliseOriginal(d);
                const double dot = ((double)e.y * d.y + (double)e.x * d.x) + (double)e.z * d.z;
                if (dot >= (double)0.95f) { // 0x858EF0; NaN keeps the point
                    m_pointRoute->ResizeTo(m_pointRoute->m_NumEntries - 1); // 0x5F13A0 (RemoveAt last)
                }
            }
        } else {
            addPoint = false;
        }
    }
    if (addPoint) {
        m_pointRoute->AddUnlessFull(targetPos); // 0x48E180
    }

    // Total "length": sum of the squared distances of all following points to the first one (sic)
    const CVector first = m_pointRoute->m_Entries[0];
    float         total = 0.f;
    for (uint32 i = 1; i < m_pointRoute->m_NumEntries; i++) {
        const CVector v = m_pointRoute->m_Entries[i] - first;
        total = (float)(SqMagnitudeOriginal(v) + (double)total);
    }

    const CVector toTarget = pedPos - m_targetPed->GetPosition();
    const double  sum      = SqMagnitudeOriginal(toTarget) + (double)total;
    constexpr float RUN_DIST = 3.5f; // 0x870C2C
    m_moveState = ((double)RUN_DIST * RUN_DIST < sum) ? PEDMOVE_RUN : PEDMOVE_WALK;

    if (m_pSubTask->GetTaskType() == TASK_SIMPLE_GO_TO_POINT) {
        static_cast<CTaskSimpleGoToPoint*>(m_pSubTask)->m_moveState = (eMoveState)m_moveState;
    }
    return ret;
}

// 0x695E40
CTask* CTaskComplexFollowPedFootsteps::CreateSubTask(eTaskType taskType, CPed* ped) {
    if (!m_targetPed || !m_pointRoute) {
        return nullptr;
    }
    switch (taskType) {
    case TASK_SIMPLE_GO_TO_POINT: {
        const CVector pos = m_pointRoute->m_Entries[0];
        { // 0x5F13A0 (RemoveAt(0))
            const auto n = m_pointRoute->m_NumEntries;
            for (int32 i = 0; i < (int32)n - 1; i++) {
                m_pointRoute->m_Entries[i] = m_pointRoute->m_Entries[i + 1];
            }
            m_pointRoute->m_NumEntries--;
        }
        return new CTaskSimpleGoToPoint{ (eMoveState)m_moveState, pos, 0.01f, false, false }; // 0x870C28
    }
    case TASK_SIMPLE_STAND_STILL:
        return new CTaskSimpleStandStill{ 10'000, false, false, 8.f };
    case TASK_SIMPLE_HIT_HEAD:
        return new CTaskSimpleHitHead{};
    case TASK_COMPLEX_SEEK_ENTITY: {
        const float radius = m_targetPed->bInVehicle
            ? 1.f * 4.f // 0x85BACC * 0x858B90
            : 1.f;      // 0x85BACC
        return new CTaskComplexSeekEntity<CEntitySeekPosCalculatorStandard>{ m_targetPed, 50'000, 1'000, radius, 2.f, 2.f, true, true }; // 0x859E30, 0x859E34
    }
    default:
        return nullptr;
    }
}

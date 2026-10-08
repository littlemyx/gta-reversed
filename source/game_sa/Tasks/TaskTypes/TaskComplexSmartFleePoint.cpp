#include "StdInc.h"
#include "TaskComplexSmartFleePoint.h"
#include "TaskComplexWander.h"
#include "TaskComplexWanderFlee.h"
#include "TaskComplexLeaveAnyCar.h"
#include "TaskComplexSequence.h"
#include "TaskSimpleStandStill.h"
#include "TaskSimpleRunAnim.h"
#include "TaskSimpleTired.h"
#include "Ragdoll/IKChainManager.h"
#include "General.h"

void CTaskComplexSmartFleePoint::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexSmartFleePoint, 0x86f744, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x65BD20);
    RH_ScopedInstall(Destructor, 0x65BDB0);

    RH_ScopedInstall(SetDefaultTaskWanderDir, 0x65BE00);
    RH_ScopedInstall(ComputeFleeDir, 0x65BE40);
    RH_ScopedInstall(CreateSubTask, 0x65BE80);
    RH_ScopedInstall(SetFleePosition, 0x65C3C0);

    RH_ScopedVMTInstall(Clone, 0x65CED0);
    RH_ScopedVMTInstall(GetTaskType, 0x65BDA0);
    RH_ScopedVMTInstall(MakeAbortable, 0x65BDC0);
    RH_ScopedVMTInstall(CreateNextSubTask, 0x65C0C0);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x65C140);
    RH_ScopedVMTInstall(ControlSubTask, 0x65C1E0);
}

// 0x65BD20
CTaskComplexSmartFleePoint::CTaskComplexSmartFleePoint(CVector const& fleePos, bool doScream, float safeDist, int32 fleeTimeMs) :
    m_fleePoint{fleePos},
    m_doScream{doScream},
    m_safeDist{safeDist},
    m_fleeTimeMs{fleeTimeMs}
{
    if (m_fleeTimeMs != -1) {
        m_timer.Start(m_fleeTimeMs);
    }
}

CTaskComplexSmartFleePoint::CTaskComplexSmartFleePoint(const CTaskComplexSmartFleePoint& o)
    : CTaskComplexSmartFleePoint{o.m_fleePoint, o.m_doScream, o.m_safeDist, o.m_fleeTimeMs}
{
}

// 0x65BE00
void CTaskComplexSmartFleePoint::SetDefaultTaskWanderDir(CPed* ped) {
    // The wander task (if any) that is the ped's default task takes over our subtask's nodes/dir
    const auto defaultTask = ped->GetTaskManager().m_aPrimaryTasks[TASK_PRIMARY_DEFAULT];
    if (!defaultTask || defaultTask->GetTaskType() != TASK_COMPLEX_WANDER) {
        return;
    }

    // NOTSA: Original code didn't check the subtask type, nor if it's non-null
    const auto dst = static_cast<CTaskComplexWander*>(defaultTask);
    const auto src = static_cast<CTaskComplexWander*>(m_pSubTask);
    if (dst->m_LastNode != src->m_LastNode || dst->m_NextNode != src->m_NextNode) { // 0x669D50 inlined
        dst->m_LastNode = src->m_LastNode;
        dst->m_NextNode = src->m_NextNode;
        dst->m_nDir     = src->m_nDir;
        dst->m_bNewNodes = true;
    }
}

// 0x65BE40
uint8 CTaskComplexSmartFleePoint::ComputeFleeDir(CPed* ped) {
    const auto& pos = ped->GetPosition();
    return (uint8)CGeneral::GetNodeHeadingFromVector(pos.x - m_fleePoint.x, pos.y - m_fleePoint.y);
}

// 0x65BE80
CTask* CTaskComplexSmartFleePoint::CreateSubTask(eTaskType taskType, CPed* ped) {
    switch (taskType) {
    case TASK_COMPLEX_LEAVE_ANY_CAR:
        return new CTaskComplexLeaveAnyCar{0, false, true};
    case TASK_SIMPLE_STAND_STILL:
        SetDefaultTaskWanderDir(ped);
        return new CTaskSimpleStandStill{0, false, false, 8.f};
    case TASK_COMPLEX_SEQUENCE: {
        if (m_moveState != PEDMOVE_RUN) {
            return nullptr;
        }
        const auto seq = new CTaskComplexSequence{};
        seq->AddTask(new CTaskSimpleRunAnim{ANIM_GROUP_DEFAULT, ANIM_ID_FLEE_LKAROUND_01, 4.f, false});
        seq->AddTask(new CTaskSimpleTired{2000});
        return seq;
    }
    case TASK_COMPLEX_WANDER:
        return new CTaskComplexWanderFlee{m_moveState, m_fleeDir};
    default: // Includes TASK_FINISHED
        return nullptr;
    }
}

// 0x65C3C0
void CTaskComplexSmartFleePoint::SetFleePosition(CVector const& pos, float safeDist, bool doScream) {
    if (m_fleePoint.x == pos.x && m_fleePoint.y == pos.y && m_fleePoint.z == pos.z && m_safeDist == safeDist) {
        m_doScream = doScream;
        return;
    }
    m_fleePoint = pos;
    m_safeDist  = safeDist;
    m_hasFleePointChanged = true;
    m_doScream  = doScream;
}

// 0x65BDC0
bool CTaskComplexSmartFleePoint::MakeAbortable(CPed* ped, eAbortPriority priority, CEvent const* event) {
    if (priority == ABORT_PRIORITY_LEISURE) {
        // Make the timer run out right away (not using `Start` as it rejects negative intervals)
        m_fleeTimeMs = -1;
        m_timer.m_nStartTime = CTimer::GetTimeInMS();
        m_timer.m_nInterval  = -1;
        m_timer.m_bStarted   = true;
    }
    return m_pSubTask->MakeAbortable(ped, priority, event);
}

// 0x65C0C0
CTask* CTaskComplexSmartFleePoint::CreateNextSubTask(CPed* ped) {
    switch (m_pSubTask->GetTaskType()) {
    case TASK_SIMPLE_STAND_STILL:
        return CreateSubTask(TASK_COMPLEX_SEQUENCE, ped);
    case TASK_COMPLEX_SEQUENCE:
        return CreateSubTask(TASK_FINISHED, ped);
    case TASK_COMPLEX_LEAVE_ANY_CAR:
        m_fleeDir = ComputeFleeDir(ped);
        return CreateSubTask(TASK_COMPLEX_WANDER, ped);
    default:
        return nullptr;
    }
}

// 0x65C140
CTask* CTaskComplexSmartFleePoint::CreateFirstSubTask(CPed* ped) {
    m_initalPos = ped->GetPosition();

    if (ped->bInVehicle && ped->m_pVehicle) { // IsInVehicle()
        return CreateSubTask(TASK_COMPLEX_LEAVE_ANY_CAR, ped);
    }

    const auto& pos = ped->GetPosition();
    m_fleeDir = (uint8)CGeneral::GetNodeHeadingFromVector(pos.x - m_fleePoint.x, pos.y - m_fleePoint.y);
    return CreateSubTask(TASK_COMPLEX_WANDER, ped);
}

// 0x65C1E0
CTask* CTaskComplexSmartFleePoint::ControlSubTask(CPed* ped) {
    if (m_pSubTask->GetTaskType() != TASK_COMPLEX_WANDER) {
        if (g_ikChainMan.IsLooking(ped)) {
            g_ikChainMan.AbortLookAt(ped, 250u);
        }
        return m_pSubTask;
    }

    if (m_doScream) {
        ped->Say(CTX_GLOBAL_PAIN_PANIC, 0, 0.1f, false, false, false);
    }

    const auto wander = static_cast<CTaskComplexWander*>(m_pSubTask);
    wander->m_nMoveState = m_moveState;

    CTask* newSubTask = nullptr; // nullptr => keep the current one
    if (m_hasFleePointChanged) {
        m_hasFleePointChanged = false;
        if (m_timer.m_bStarted) { // Restart the timer
            m_timer.m_nStartTime = CTimer::GetTimeInMS();
            m_timer.m_nInterval  = m_fleeTimeMs;
            m_timer.m_bStarted   = true;
        }
        const auto dir = ComputeFleeDir(ped);
        if (m_fleeDir != dir) {
            m_fleeDir = dir;
            if (wander->m_nDir != dir) { // 0x669D30 inlined
                wander->m_nDir = dir;
                wander->m_bNewDir = true;
            }
        }
    } else {
        if (m_timer.m_bStarted && m_timer.IsOutOfTime()) {
            newSubTask = CreateSubTask(TASK_SIMPLE_STAND_STILL, ped);
        } else {
            const auto& pedPos = ped->GetPosition();
            const auto  safeDistSq = m_safeDist * m_safeDist;
            if ((m_fleePoint - pedPos).SquaredMagnitude() > safeDistSq) {
                if ((m_initalPos - pedPos).SquaredMagnitude() > safeDistSq) {
                    newSubTask = CreateSubTask(TASK_SIMPLE_STAND_STILL, ped);
                }
            }
        }
    }
    const auto ret = newSubTask ? newSubTask : m_pSubTask;

    if (m_moveState == PEDMOVE_RUN && !g_ikChainMan.IsLooking(ped)) {
        if (CGeneral::GetRandomNumberInRange(0, 100) <= 5) {
            g_ikChainMan.LookAt(
                "TaskSmartFleePoint",
                ped,
                nullptr,
                2000,
                BONE_UNKNOWN,
                &m_fleePoint,
                false,
                0.25f,
                500,
                3,
                false
            );
        }
    }

    return ret;
}

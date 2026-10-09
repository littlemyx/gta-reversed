#include "StdInc.h"

#include "TaskComplexFollowPatrolRoute.h"
#include "PatrolRoute.h"
#include "TaskSimpleGoToPoint.h"
#include "TaskComplexGoToPointAndStandStill.h"
#include "TaskComplexFollowNodeRoute.h"
#include "TaskSimpleRunNamedAnim.h"
#include "TaskSimpleNone.h"

namespace {
//! 0x59C910 - `CVector::Normalise` as the original evaluates it: the sum of squares and the reciprocal root stay in the FPU
//! (extended precision), every component is stored as float. A length of 0 (or less) yields (1, y, z) - only x is written.
void NormaliseExt(CVector& v) {
    const double sq = ((double)v.x * v.x + (double)v.y * v.y) + (double)v.z * v.z;
    if (sq <= 0.0) { // FCOM + JP: NaN takes the sqrt path
        v.x = 1.0f;
        return;
    }
    const double inv = 1.0 / std::sqrt(sq);
    v.x = (float)(v.x * inv);
    v.y = (float)(v.y * inv);
    v.z = (float)(v.z * inv);
}
}; // namespace

void CTaskComplexFollowPatrolRoute::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexFollowPatrolRoute, 0x870114, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x674930);
    RH_ScopedInstall(ComputeNextSubTaskType, 0x670A70);
    RH_ScopedInstall(CreateSubTask, 0x670B00);
    RH_ScopedVMTDestructorInstall(0x672BB0);
    RH_ScopedVMTInstall(Clone, 0x674BB0);
    RH_ScopedVMTInstall(GetTaskType, 0x670A60);
    RH_ScopedVMTInstall(MakeAbortable, 0x66BA90);
    RH_ScopedVMTInstall(CreateNextSubTask, 0x670D80);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x670E20);
    RH_ScopedVMTInstall(ControlSubTask, 0x66BB00);
}

// 0x674930
CTaskComplexFollowPatrolRoute::CTaskComplexFollowPatrolRoute(int16 moveState, const CPatrolRoute* route, eMode mode, float radius, float moveStateRadius) :
    m_Mode{ mode },
    m_MoveState{ moveState },
    m_Radius{ radius },
    m_MoveStateRadius{ moveStateRadius },
    m_Route{ new CPatrolRoute{} } // 0x41B810, 0x66D440
{
    m_Route->Set(*route); // 0x66D4B0
    m_bRestart = true;
}

// 0x6709F0
CTaskComplexFollowPatrolRoute::~CTaskComplexFollowPatrolRoute() {
    delete m_Route; // 0x41B820
}

// 0x674BB0
CTask* CTaskComplexFollowPatrolRoute::Clone() const {
    return new CTaskComplexFollowPatrolRoute{ m_MoveState, m_Route, m_Mode, m_Radius, m_MoveStateRadius };
}

// 0x66BA90
bool CTaskComplexFollowPatrolRoute::MakeAbortable(CPed* ped, eAbortPriority priority, const CEvent* event) {
    if (priority == ABORT_PRIORITY_LEISURE) {
        m_Route->m_NumNodes = 0; // Stop patrolling once the sub-task is done
    }
    m_PedPos = ped->GetPosition();
    const auto aborted = m_pSubTask->MakeAbortable(ped, priority, event);
    m_bFollowNodeRoute = aborted;
    return aborted;
}

// 0x670D80
CTask* CTaskComplexFollowPatrolRoute::CreateNextSubTask(CPed* ped) {
    if (m_pSubTask->GetTaskType() != TASK_COMPLEX_FOLLOW_NODE_ROUTE) {
        if (m_pSubTask->GetTaskType() == TASK_NONE) {
            return CreateSubTask(TASK_FINISHED);
        }
        // Reached a node => play it's anim (if any)
        if (m_pSubTask->GetTaskType() != TASK_SIMPLE_NAMED_ANIM) {
            if (m_Route->m_Anims[m_CurNodeIdx].m_AnimName[0]) { // The original compares the 1st byte to the one of an empty string (0x858B54)
                return CreateSubTask(TASK_SIMPLE_NAMED_ANIM);
            }
        }
        m_CurNodeIdx++;
    }
    return CreateSubTask(ComputeNextSubTaskType());
}

// 0x670E20
CTask* CTaskComplexFollowPatrolRoute::CreateFirstSubTask(CPed* ped) {
    m_bRestart = false;

    const auto numNodes = m_Route->m_NumNodes;
    if (numNodes == 0) {
        return CreateSubTask(TASK_NONE);
    }

    const auto& pedPos  = ped->GetPosition();
    int32       bestIdx = -1;
    float       best    = std::numeric_limits<float>::max(); // 0x7F7FFFFF

    // Find the closest route segment the ped is next to (project the ped on the segment)
    // NOTE: The original stores the *squared* distance in `best`, but then squares it again when comparing it (Kept)
    for (int32 i = 0; i < numNodes; i++) {
        const auto  nextIdx = (i + 1) % numNodes;
        const auto& cur     = m_Route->m_Pos[i % numNodes];
        const auto& next    = m_Route->m_Pos[nextIdx];

        // Direction + length of the segment
        const double dzExt = (double)next.z - (double)cur.z;
        CVector      dir{
            (float)((double)next.x - (double)cur.x),
            (float)((double)next.y - (double)cur.y),
            (float)dzExt
        };
        const float lenSq = (float)(((dzExt * (double)dir.z) + (double)dir.x * dir.x) + (double)dir.y * dir.y); // `fmul` on the (float) memory operand => z is `dzExt * dir.z`
        NormaliseExt(dir); // 0x59C910

        // Where on the segment is the closest point to the ped?
        const double dxp = (double)pedPos.x - (double)cur.x;
        const double dyp = (double)pedPos.y - (double)cur.y;
        const double dzp = (double)pedPos.z - (double)cur.z;
        const double t   = (dzp * dir.z + dxp * dir.x) + dyp * dir.y;
        const float  tF  = (float)t;
        if (!(t > 0.0)) { // FCOMP + JNE
            continue;
        }
        if (!(std::sqrt((double)lenSq) > (double)tF)) { // FCOMP (vs the float version)
            continue;
        }

        // Closest point on the segment
        const float  pX   = (float)((double)dir.x * tF + (double)cur.x);
        const double pY   = (double)dir.y * tF + (double)cur.y;
        const float  dirZt = (float)((double)dir.z * tF);
        const double pZ   = (double)dirZt + (double)cur.z;

        const float  dX = (float)((double)pedPos.x - (double)pX);
        const float  dY = (float)((double)pedPos.y - pY);
        const double dZ = (double)pedPos.z - pZ;

        const double distSq = (dZ * dZ + (double)dY * dY) + (double)dX * dX;
        if (distSq < (double)best * (double)best) { // FCOMPP + JP
            best    = (float)distSq;
            bestIdx = nextIdx;
        }
    }

    // No segment found => use the closest node
    // NOTE: Same squaring issue as above
    if (bestIdx == -1) {
        float best2 = std::numeric_limits<float>::max(); // 0x863A3C
        for (int32 k = 0; k < numNodes; k++) { // (Unrolled by 4 in the original)
            const auto&  pos = m_Route->m_Pos[k];
            const double dx  = (double)pedPos.x - (double)pos.x;
            const double dy  = (double)pedPos.y - (double)pos.y;
            const double dz  = (double)pedPos.z - (double)pos.z;
            const double s   = (dz * dz + dy * dy) + dx * dx;
            if (s < (double)best2 * (double)best2) {
                best2   = (float)s;
                bestIdx = k;
            }
        }
    }

    m_CurNodeIdx = (int16)bestIdx;

    if (m_bFollowNodeRoute) { // We were aborted => get back to the route using nodes
        m_bFollowNodeRoute = false;
        return CreateSubTask(TASK_COMPLEX_FOLLOW_NODE_ROUTE);
    }
    return CreateSubTask(ComputeNextSubTaskType());
}

// 0x66BB00
CTask* CTaskComplexFollowPatrolRoute::ControlSubTask(CPed* ped) {
    if (!m_bRestart && !m_bFollowNodeRoute) {
        return m_pSubTask;
    }
    m_CurNodeIdx = 0;
    return CreateFirstSubTask(ped); // Virtual call (slot 9)
}

// 0x670A70
eTaskType CTaskComplexFollowPatrolRoute::ComputeNextSubTaskType() {
    const auto numNodes = m_Route->m_NumNodes;
    if (numNodes == 0) {
        return TASK_FINISHED;
    }

    const int32 cur = m_CurNodeIdx;
    if (cur + 1 < numNodes) {
        return TASK_SIMPLE_GO_TO_POINT;
    }
    if (cur + 1 == numNodes) { // Final point
        return TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL;
    }
    if (cur != numNodes) {
        return TASK_NONE;
    }

    // Reached the end of the route
    m_NumLoops++;
    switch ((uint32)(int32)m_Mode) { // Original compares as unsigned
    case (uint32)eMode::ONCE:
        return TASK_FINISHED;
    case (uint32)eMode::BACK_ONCE:
        if (m_NumLoops != 1) {
            return TASK_FINISHED;
        }
        [[fallthrough]];
    case (uint32)eMode::PING_PONG:
        m_Route->Reverse(); // 0x66D550
        m_CurNodeIdx = 0;
        return ComputeNextSubTaskType();
    case (uint32)eMode::LOOP:
        m_CurNodeIdx = 0;
        return ComputeNextSubTaskType();
    default:
        return TASK_NONE;
    }
}

// 0x670B00
CTask* CTaskComplexFollowPatrolRoute::CreateSubTask(eTaskType taskType) {
    switch (taskType) {
    case TASK_SIMPLE_GO_TO_POINT:
        return new CTaskSimpleGoToPoint{ (eMoveState)m_MoveState, m_Route->m_Pos[m_CurNodeIdx], m_Radius, false, false };
    case TASK_SIMPLE_NAMED_ANIM: {
        const auto& anim = m_Route->m_Anims[m_CurNodeIdx];
        return new CTaskSimpleRunNamedAnim{ anim.m_AnimName, anim.m_AnimGroupName, 0x58, 4.f, (uint32)-1, false, false, false, false };
    }
    case TASK_NONE:
        return new CTaskSimpleNone{};
    case TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL: {
        auto moveStateRadius = m_MoveStateRadius;
        switch (m_Mode) {
        case eMode::BACK_ONCE:
            if (m_NumLoops != 0) {
                break;
            }
            [[fallthrough]];
        case eMode::PING_PONG:
        case eMode::LOOP:
            moveStateRadius = 0.f;
            break;
        default:
            break;
        }
        return new CTaskComplexGoToPointAndStandStill{ (eMoveState)m_MoveState, m_Route->m_Pos[m_CurNodeIdx], m_Radius, moveStateRadius, false, false };
    }
    case TASK_COMPLEX_FOLLOW_NODE_ROUTE: {
        const auto numNodes = m_Route->m_NumNodes;
        if (numNodes == 0) {
            return nullptr;
        }
        if (m_CurNodeIdx == numNodes) {
            m_CurNodeIdx--;
        }
        const CVector pos = m_Route->m_Pos[m_CurNodeIdx];
        return new CTaskComplexFollowNodeRoute{ (eMoveState)m_MoveState, pos, 0.5f, 3.f, 2.f, false, -1, true }; // 0x86FCA0, 0x86FCA4, 0x86FCA8
    }
    default: // Including `TASK_FINISHED`
        return nullptr;
    }
}

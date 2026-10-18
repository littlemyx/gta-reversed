#include "StdInc.h"

#include "TaskInteriorGoToInfo.h"
#include "TaskComplexFollowNodeRoute.h"
#include "TaskSimpleAchieveHeading.h"
#include "TaskSimpleGoToPoint.h"

#include "Interior_c.h"
#include "InteriorInfo_t.h"
#include "InteriorManager_c.h"

void CTaskInteriorGoToInfo::InjectHooks() {
    RH_ScopedVirtualClass(CTaskInteriorGoToInfo, 0x8702bc, 11);
    RH_ScopedCategory("Tasks/TaskTypes/Interior");

    RH_ScopedInstall(Constructor, 0x6757D0);
    RH_ScopedInstall(Destructor, 0x675810);

    RH_ScopedVMTInstall(Clone, 0x675820);
    RH_ScopedVMTInstall(GetTaskType, 0x675800);
    RH_ScopedVMTInstall(CreateNextSubTask, 0x675890);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x676640);
    RH_ScopedVMTInstall(ControlSubTask, 0x675A30);
}

// 0x6757D0
CTaskInteriorGoToInfo::CTaskInteriorGoToInfo(InteriorInfo_t* interiorInfo, Interior_c* interior, bool bDoInstantly) :
    m_Int{interior},
    m_IntInfo{interiorInfo},
    m_bDoInstantly{bDoInstantly}
{
}

// 0x675820
CTaskInteriorGoToInfo::CTaskInteriorGoToInfo(const CTaskInteriorGoToInfo& o) :
    CTaskInteriorGoToInfo{o.m_IntInfo, o.m_Int, o.m_bDoInstantly}
{
}   

// 0x675890
CTask* CTaskInteriorGoToInfo::CreateNextSubTask(CPed* ped) {
    if (!m_IntInfo) {
        return nullptr;
    }
    switch (m_pSubTask->GetTaskType()) {
    case TASK_COMPLEX_FOLLOW_NODE_ROUTE:
    case TASK_SIMPLE_GO_TO_POINT: {
        if ((m_IntInfo->Pos - ped->GetPosition()).SquaredMagnitude2D() > sq(0.2f)) {
            return new CTaskComplexFollowNodeRoute{ PEDMOVE_WALK, m_IntInfo->Pos, 0.2f };
        }
        // 0x675940: LimitRadianAngle(GetRadianAngleBetweenPoints(Dir.x, Dir.y, 0, 0)), not CVector::Heading
        return new CTaskSimpleAchieveHeading{ CGeneral::LimitRadianAngle(CGeneral::GetRadianAngleBetweenPoints(m_IntInfo->Dir.x, m_IntInfo->Dir.y, 0.f, 0.f)) };
    }
    default:
        return nullptr;
    }
}

// 0x676640
CTask* CTaskInteriorGoToInfo::CreateFirstSubTask(CPed* ped) {
    if (!m_IntInfo) {
        return nullptr;
    }

    if (m_bDoInstantly) {
        ped->SetPosn(m_IntInfo->Pos);

        const auto rz = CGeneral::LimitRadianAngle(CGeneral::GetRadianAngleBetweenPoints(m_IntInfo->Dir.x, m_IntInfo->Dir.y, 0.f, 0.f)); // 0x6766E7, 0x6766F5 (not CVector::Heading)
        ped->m_fCurrentRotation = rz;
        ped->m_fAimingRotation = rz;
        ped->SetHeading(rz);

        return nullptr;
    }

    if (g_interiorMan.GetPedsInterior(ped) == m_Int) {
        return new CTaskSimpleGoToPoint{ PEDMOVE_WALK,  m_IntInfo->Pos, 0.2f };
    }

    const auto tFollowNodeRoute = new CTaskComplexFollowNodeRoute{ PEDMOVE_WALK, ThePaths.GetPathNode(m_Int->GetNodeAddress())->GetPosition(), 0.2f};
    tFollowNodeRoute->SetStartNode(m_Int->GetNodeAddress());
    return tFollowNodeRoute;
}

// 0x675A30
CTask* CTaskInteriorGoToInfo::ControlSubTask(CPed* ped) {
    return m_IntInfo
        ? m_pSubTask
        : nullptr;
}

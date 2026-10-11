#include "StdInc.h"

#include "TaskComplexWalkRoundObject.h"
// #include "PointRoute.h"
#include "PedGeometryAnalyser.h"
#include "TaskComplexFollowPointRoute.h"
#include "TaskComplexGoToPointAndStandStill.h"
#include "TaskSimpleStandStill.h"
#include "TaskSimpleAchieveHeading.h"
#include "General.h"
#include "game_sa/DetachedShared.h"
#line 11

void CTaskComplexWalkRoundObject::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexWalkRoundObject, 0x86F364, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x655020);

    RH_ScopedInstall(CreateRouteTask, 0x655140);
    RH_ScopedInstall(ComputeRoute, 0x6551D0);
    RH_ScopedInstall(CreateSubTask, 0x655290);

    RH_ScopedVMTInstall(CreateNextSubTask, 0x657220);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x657380);
    RH_ScopedVMTInstall(ControlSubTask, 0x6575F0);
}

// 0x655020
CTaskComplexWalkRoundObject::CTaskComplexWalkRoundObject(int32 moveState, const CVector& targetPoint, CEntity* object) : CTaskComplex() {
    m_moveState   = moveState;
    m_targetPoint = targetPoint;
    m_object      = object;
    field_24      = 0;
    field_28      = 0;
    field_2C      = 0;
    field_2D      = 0;

    CEntity::SafeRegisterRef(m_object);

    m_pointRoute = new CPointRoute();
}

CTaskComplexWalkRoundObject::~CTaskComplexWalkRoundObject() {
    CEntity::SafeCleanUpRef(m_object);

    delete m_pointRoute;
    // todo: m_pointRoute = nullptr;
}

CTaskComplexWalkRoundObject* CTaskComplexWalkRoundObject::Constructor(int32 moveState, const CVector& targetPoint, CEntity* object) {
    this->CTaskComplexWalkRoundObject::CTaskComplexWalkRoundObject(moveState, targetPoint, object);
    return this;
}

//! 0x59C970 - `CVector::NormaliseAndMag` as the original evaluates it (extended precision for the sum of squares and the reciprocal root)
static float NormaliseAndMagOriginal(CVector& v) {
    const double sumSq = ((double)v.x * v.x + (double)v.y * v.y) + (double)v.z * v.z;
    if (sumSq <= 0.0) { // NaN takes the sqrt path
        v.x = 1.0f;
        return 1.0f;
    }
    const double recip = 1.0 / std::sqrt(sumSq);
    v.x = (float)(v.x * recip);
    v.y = (float)(v.y * recip);
    v.z = (float)(v.z * recip);
    return (float)(1.0 / recip);
}

// 0x6551D0
float CTaskComplexWalkRoundObject::ComputeRoute(CPed* ped) {
    //! Margin by which the bounding boxes are inflated by (`CPedGeometryAnalyser`, 0x8D22B0, normally 0.35f)
    static NOTSA_GLOBAL_ALIAS(s_BoundingBoxMargin, 0x8D22B0, (float), notsa::shared::BoundingBoxMargin);

    const auto oldMargin = std::exchange(s_BoundingBoxMargin, 0.7f);
    CPedGeometryAnalyser::ComputeRouteRoundEntityBoundingBox(*ped, *m_object, m_targetPoint, *m_pointRoute, 0);
    s_BoundingBoxMargin = oldMargin;

    const auto& pedPos = ped->GetPosition();

    CVector planes[4];
    float   dots[4];
    CPedGeometryAnalyser::ComputeEntityBoundingBoxPlanes(pedPos.z, *m_object, &planes, dots);

    const auto side = CPedGeometryAnalyser::ComputeEntityHitSide(pedPos, *m_object);

    // x87: all in extended precision
    const auto& n = planes[side];
    return (float)(((double)n.z * pedPos.z + (double)n.y * pedPos.y + (double)n.x * pedPos.x) + dots[side]);
}

// 0x655290
CTask* CTaskComplexWalkRoundObject::CreateSubTask(eTaskType taskType, CPed* ped) {
    switch (taskType) {
    case TASK_SIMPLE_STAND_STILL:
        return new CTaskSimpleStandStill{ 500, false, false, 8.f };
    case TASK_COMPLEX_FOLLOW_POINT_ROUTE:
        return CreateRouteTask(ped);
    case TASK_FINISHED:
    default:
        return nullptr;
    }
}

// 0x6575F0
CTask* CTaskComplexWalkRoundObject::ControlSubTask(CPed* ped) {
    const auto Abort = [&]() -> CTask* { // 0x657718
        if (m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_URGENT, nullptr)) {
            return CreateSubTask(TASK_SIMPLE_STAND_STILL, ped);
        }
        return m_pSubTask;
    };

    if (!m_object) {
        return Abort();
    }

    if (m_pSubTask->GetTaskType() == TASK_COMPLEX_FOLLOW_POINT_ROUTE && field_2C) {
        if (field_2D) {
            field_24 = CTimer::GetTimeInMS();
            field_2D = 0;
        }
        if (CTimer::GetTimeInMS() >= (uint32)(field_28 + field_24)) { // Timed out
            return Abort();
        }
    }

    constexpr int32 MAX_STATIC_COUNTER = 30; // 0x86C938
    if (ped->GetIntelligence()->m_AnotherStaticCounter > MAX_STATIC_COUNTER) {
        return Abort();
    }

    if (m_pSubTask->GetTaskType() == TASK_SIMPLE_STAND_STILL) {
        return m_pSubTask;
    }

    // Has the object moved/rotated significantly since the route was computed?
    auto& objMat = m_object->GetMatrix(); // Allocates the matrix if there's none
    const auto& objPos = m_object->GetPosition();
    const double dx = (double)field_30.x - objPos.x;
    const double dy = (double)field_30.y - objPos.y;
    const double dz = (double)field_30.z - objPos.z;
    if (((dz * dz + dy * dy) + dx * dx) > (double)0.0625f) { // 0x86F15C
        return Abort();
    }

    const auto& objFwd = objMat.GetForward();
    const double fwdDot = ((double)objFwd.z * field_3C.z + (double)objFwd.y * field_3C.y) + (double)field_3C.x * objFwd.x;
    if (fwdDot < 0.9f) { // 0x86F160
        return Abort();
    }

    const auto& objRight = objMat.GetRight();
    const double rightDot = (double)field_48.z * objRight.z + (double)field_48.y * objRight.y + (double)field_48.x * objRight.x; // 0x40FDB0
    if (!(rightDot >= 0.9f) && !std::isnan(rightDot)) { // JP: NaN => OK
        return Abort();
    }

    return m_pSubTask;
}

// 0x657220
CTask* CTaskComplexWalkRoundObject::CreateNextSubTask(CPed* ped) {
    if (!m_object) {
        return nullptr;
    }
    switch (m_pSubTask->GetTaskType()) {
    case TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL:
        return CreateRouteTask(ped);
    case TASK_SIMPLE_ACHIEVE_HEADING: {
        if (m_pointRoute->m_NumEntries != 0) {
            return CreateRouteTask(ped);
        }

        CVector pt{};
        CPedGeometryAnalyser::ComputeClosestSurfacePoint(*ped, *m_object, pt);
        const auto pedSide    = CPedGeometryAnalyser::ComputeEntityHitSide(pt, *m_object);
        const auto targetSide = CPedGeometryAnalyser::ComputeEntityHitSide(m_targetPoint, *m_object);
        if (pedSide == targetSide) { // Same side => just go straight there
            m_pointRoute->AddUnlessFull(pt);
            CPedGeometryAnalyser::ComputeClosestSurfacePoint(m_targetPoint, *m_object, pt);
            m_pointRoute->AddUnlessFull(pt);
            return CreateRouteTask(ped);
        }
        return new CTaskComplexGoToPointAndStandStill{ PEDMOVE_WALK, pt, 0.5f, 2.f, false, false }; // 0x86FC84, 0x86FC88
    }
    case TASK_SIMPLE_STAND_STILL:
    case TASK_COMPLEX_FOLLOW_POINT_ROUTE:
    default:
        return nullptr;
    }
}

// 0x657380
CTask* CTaskComplexWalkRoundObject::CreateFirstSubTask(CPed* ped) {
    if (!m_object) {
        return nullptr;
    }

    m_pointRoute->Clear();
    const float routeDist = ComputeRoute(ped);
    if (m_pointRoute->m_NumEntries == 0) {
        return nullptr;
    }

    field_24 = CTimer::GetTimeInMS();
    field_28 = m_moveState == PEDMOVE_WALK ? 8000 : 4000;
    field_2C = 1;

    field_30 = m_object->GetPosition();
    field_3C = m_object->GetMatrix().GetForward(); // Allocates the matrix if there's none
    field_48 = m_object->GetMatrix().GetRight();

    // Direction from the ped to the first point of the route
    const auto& pedPos = ped->GetPosition();
    CVector     dirToRoute = m_pointRoute->m_Entries[0] - pedPos;
    const float distToRoute = NormaliseAndMagOriginal(dirToRoute);

    const auto& pedFwd = ped->GetMatrix().GetForward();
    const double dot = ((double)dirToRoute.z * pedFwd.z + (double)dirToRoute.y * pedFwd.y) + (double)dirToRoute.x * pedFwd.x;

    const float distThreshold = m_moveState == PEDMOVE_WALK
        ? 2.f  // 0x858CA0
        : m_moveState == PEDMOVE_RUN
            ? 4.f // 0x858B90
            : 6.f; // 0x858B44

    if (distToRoute > distThreshold && routeDist > distThreshold && dot > 0.0) {
        return CreateRouteTask(ped);
    }

    // Otherwise first turn towards the route
    const auto& firstPt = m_pointRoute->m_Entries[0];
    const auto  heading = CGeneral::LimitRadianAngle(
        CGeneral::GetRadianAngleBetweenPoints(firstPt.x - ped->GetPosition().x, firstPt.y - ped->GetPosition().y, 0.f, 0.f)
    );
    return new CTaskSimpleAchieveHeading{ heading, 1.f, 0.2f };
}

// 0x655140
CTask* CTaskComplexWalkRoundObject::CreateRouteTask(CPed* ped) {
    if (m_pointRoute->m_NumEntries == 0) {
        return nullptr;
    }
    return new CTaskComplexFollowPointRoute{ (eMoveState)m_moveState, *m_pointRoute, CTaskComplexFollowPointRoute::Mode::ONE_WAY, 0.5f, 0.f, true, false, false }; // 0x86FC98
}

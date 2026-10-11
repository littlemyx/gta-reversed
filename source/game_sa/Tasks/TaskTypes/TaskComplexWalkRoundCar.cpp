#include "StdInc.h"

#include "TaskComplexFollowPointRoute.h"
#include "TaskComplexWalkRoundCar.h"
#include "TaskSimpleStandStill.h"
#include "TaskSimpleAchieveHeading.h"
#include "TaskComplexEnterCar.h"
#include "TaskComplexEnterCarAsDriver.h"
#include "PedGeometryAnalyser.h"
#include "CarEnterExit.h"
#include "General.h"
#include "game_sa/DetachedShared.h"
#line 12

void CTaskComplexWalkRoundCar::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexWalkRoundCar, 0x86f308, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x6541B0);
    RH_ScopedInstall(Destructor, 0x656B00);

    RH_ScopedInstall(SetNewVehicle, 0x654290);
    RH_ScopedInstall(CreateRouteTask, 0x6542E0);
    RH_ScopedInstall(ComputeRouteRoundSmallCar, 0x6544F0);
    RH_ScopedInstall(GoingForDoor, 0x654720);
    RH_ScopedInstall(ComputeRouteRoundBigCar, 0x656BB0);
    RH_ScopedInstall(ComputeRoute, 0x657B80);

    RH_ScopedVMTInstall(Clone, 0x655B00);
    RH_ScopedVMTInstall(GetTaskType, 0x654280);
    RH_ScopedVMTInstall(CreateNextSubTask, 0x656B70);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x658200);
    RH_ScopedVMTInstall(ControlSubTask, 0x654370);
}

// 0x6541B0
CTaskComplexWalkRoundCar::CTaskComplexWalkRoundCar(eMoveState moveState, CVector const& targetPt, CVehicle* vehicle, bool isPedGoingForCarDoor, uint8 forceThisDirectionRoundCar) :
    m_MoveState{(uint8)moveState},
    m_bIsPedGoingForCarDoor{isPedGoingForCarDoor},
    m_ForceThisDirectionRoundCar{forceThisDirectionRoundCar},
    m_TargetPt{targetPt},
    m_Veh{vehicle},
    m_Route{new CPointRoute{}}
{
    CEntity::SafeRegisterRef(m_Veh);
}

CTaskComplexWalkRoundCar::CTaskComplexWalkRoundCar(const CTaskComplexWalkRoundCar& o) :
    CTaskComplexWalkRoundCar{
        (eMoveState)o.m_MoveState,
        o.m_TargetPt,
        o.m_Veh,
        o.m_bIsPedGoingForCarDoor,
        o.m_ForceThisDirectionRoundCar
    }
{
}

// 0x656B00
CTaskComplexWalkRoundCar::~CTaskComplexWalkRoundCar() {
    CEntity::SafeCleanUpRef(m_Veh);
    delete m_Route;
}


// 0x654290
void CTaskComplexWalkRoundCar::SetNewVehicle(CVehicle * vehicle, uint8 forceThisDirectionRoundCar) {
    if (notsa::IsFixBugs()) {
        if (vehicle == m_Veh) {
            return;
        }
    }
    CEntity::ChangeEntityReference(m_Veh, vehicle);
    m_ForceThisDirectionRoundCar    = forceThisDirectionRoundCar;
    m_bFirstSubTaskNeedsToBeCreated = true;
    m_Route->Clear();
}

// 0x6542E0
CTask* CTaskComplexWalkRoundCar::CreateRouteTask(CPed*) const {
    if (m_Route->IsEmpty()) {
        return nullptr;
    }
    return new CTaskComplexFollowPointRoute{ (eMoveState)m_MoveState, *m_Route };
}

// 0x6544F0
float CTaskComplexWalkRoundCar::ComputeRouteRoundSmallCar(CPed* ped) {
    const auto& pedPos = ped->GetPosition();

    CVector corners[4];
    CPedGeometryAnalyser::ComputeEntityBoundingBoxCorners(pedPos.z, *m_Veh, corners);

    CVector planes[4];
    float   planeDots[4];
    CPedGeometryAnalyser::ComputeEntityBoundingBoxPlanes(ped->GetPosition().z, *m_Veh, &planes, planeDots);

    // Distance of the ped from the side of the vehicle it is hitting
    float distToVeh = 0.f;
    if (const auto side = CPedGeometryAnalyser::ComputeEntityHitSide(*ped, *m_Veh); side != -1) {
        const auto& n = planes[side];
        distToVeh = n.z * pedPos.z + n.y * pedPos.y + n.x * pedPos.x + planeDots[side];
    }

    if (m_bIsPedGoingForCarDoor) {
        // Find the first plane the target is in front of
        float  distToPlane[4]{};
        int32  i = 0;
        const auto& t = m_TargetPt;
        // NOTE: The original has the loop unrolled, and the order of the float ops differs between the planes
        distToPlane[0] = planes[0].x * t.x + planes[0].z * t.z + planes[0].y * t.y + planeDots[0];
        if (!(distToPlane[0] >= 0.f)) {
            distToPlane[1] = planes[1].y * t.y + planes[1].x * t.x + planes[1].z * t.z + planeDots[1];
            if (!(distToPlane[1] >= 0.f)) {
                distToPlane[2] = planes[2].y * t.y + planes[2].x * t.x + planes[2].z * t.z + planeDots[2];
                if (!(distToPlane[2] >= 0.f)) {
                    distToPlane[3] = planes[3].y * t.y + planes[3].x * t.x + planes[3].z * t.z + planeDots[3];
                    i = (distToPlane[3] >= 0.f) ? 3 : 4;
                } else {
                    i = 2;
                }
            } else {
                i = 1;
            }
        }

        // Target is behind all the planes (inside of the vehicle), push it out to the closest side
        if (i == 4) {
            if (const auto side = CPedGeometryAnalyser::ComputeEntityHitSide(m_TargetPt, *m_Veh); side != -1) {
                const float d = -distToPlane[side] + 0.05f;
                const auto& n = planes[side];
                const float dy = d * n.y;
                const float dz = d * n.z;
                m_TargetPt.x = d * n.x + m_TargetPt.x;
                m_TargetPt.y = dy + m_TargetPt.y;
                m_TargetPt.z = dz + m_TargetPt.z;
            }
        }
    }

    // `(int32)(int8)(x << 4) >> 4` => sign-extend the 4 bit value
    const int32 forcedDir = (int8)(m_ForceThisDirectionRoundCar << 4) >> 4;
    m_DirectionGoingRoundCar = CPedGeometryAnalyser::ComputeRouteRoundEntityBoundingBox(*ped, *m_Veh, m_TargetPt, *m_Route, forcedDir);

    return distToVeh;
}

// 0x656BB0
float CTaskComplexWalkRoundCar::ComputeRouteRoundBigCar(CPed* ped) {
    // Margin by which the bounding boxes are inflated by (`CPedGeometryAnalyser`, 0x8D22B0, normally 0.35f)
    static NOTSA_GLOBAL_ALIAS(s_BoundingBoxMargin, 0x8D22B0, (float), notsa::shared::BoundingBoxMargin);

    const auto pedSide    = CPedGeometryAnalyser::ComputeEntityHitSide(*ped, *m_Veh);
    const auto targetSide = CPedGeometryAnalyser::ComputeEntityHitSide(m_TargetPt, *m_Veh);

    CVector lastPt{};
    if (pedSide == targetSide) {
        // Both are on the same side, so go straight there
        CVector pedClosestPt{};
        const auto oldMargin = std::exchange(s_BoundingBoxMargin, 0.7f);
        CPedGeometryAnalyser::ComputeClosestSurfacePoint(*ped, *m_Veh, pedClosestPt);
        CPedGeometryAnalyser::ComputeClosestSurfacePoint(m_TargetPt, *m_Veh, lastPt);
        s_BoundingBoxMargin = oldMargin;

        m_Route->AddUnlessFull(pedClosestPt);
    } else {
        CVector pedClosestPt{}, targetClosestPt{};
        auto oldMargin = std::exchange(s_BoundingBoxMargin, 0.7f);
        CPedGeometryAnalyser::ComputeClosestSurfacePoint(*ped, *m_Veh, pedClosestPt);
        CPedGeometryAnalyser::ComputeClosestSurfacePoint(m_TargetPt, *m_Veh, targetClosestPt);
        s_BoundingBoxMargin = oldMargin;

        // Temporarily move the ped and the target, and compute the route round the car between them
        const CVector savedPedPos = ped->GetPosition();
        ped->GetPosition() = pedClosestPt;

        const CVector savedTargetPt = m_TargetPt;
        m_TargetPt = targetClosestPt;

        ComputeRouteRoundSmallCar(ped);

        m_TargetPt = savedTargetPt;
        ped->GetPosition() = savedPedPos;

        // Rebuild the route, so that it starts with the point closest to the ped
        const CPointRoute routeCopy = *m_Route;
        m_Route->Clear();
        m_Route->AddUnlessFull(pedClosestPt);
        for (auto i = 0u; i < routeCopy.m_NumEntries; i++) {
            m_Route->AddUnlessFull(routeCopy.m_Entries[i]);
        }

        oldMargin = std::exchange(s_BoundingBoxMargin, 0.7f);
        CPedGeometryAnalyser::ComputeClosestSurfacePoint(m_TargetPt, *m_Veh, lastPt);
        s_BoundingBoxMargin = oldMargin;
    }

    m_Route->AddUnlessFull(lastPt);
    return 0.f;
}

// 0x657B80
float CTaskComplexWalkRoundCar::ComputeRoute(CPed* ped) {
    return m_Veh->vehicleFlags.bIsBig
        ? ComputeRouteRoundBigCar(ped)
        : ComputeRouteRoundSmallCar(ped);
}

// 0x656B70
CTask* CTaskComplexWalkRoundCar::CreateNextSubTask(CPed* ped) {
    switch (m_pSubTask->GetTaskType()) {
    case TASK_SIMPLE_ACHIEVE_HEADING:
        return CreateRouteTask(ped);
    }
    return nullptr;
}

// 0x658200
CTask* CTaskComplexWalkRoundCar::CreateFirstSubTask(CPed* ped) {
    m_Route->Clear();

    const float distToVeh = ComputeRoute(ped);
    if (m_Route->IsEmpty()) {
        return nullptr;
    }

    // Time we have to get around the vehicle
    int32 time = m_MoveState == PEDMOVE_WALK ? 20000 : 15000;
    if (m_Veh->m_pVehicleBeingTowed || m_Veh->m_pTowingVehicle) { // Has a trailer/is a trailer
        time = (int32)((double)time * 4.f);
    }
    m_Timer.Start(time);

    m_VehPos      = m_Veh->GetPosition();
    m_VehMatFwd   = m_Veh->GetForward();
    m_VehMatRight = m_Veh->GetRight();

    // Direction from the ped to the first point of the route
    const auto& pedPos = ped->GetPosition();
    CVector     dirToRoute = m_Route->m_Entries[0] - pedPos;
    const float distToRoute = dirToRoute.NormaliseAndMag();
    const float dot = dirToRoute.z * ped->GetForward().z + dirToRoute.y * ped->GetForward().y + dirToRoute.x * ped->GetForward().x;

    const float distThreshold = m_MoveState == PEDMOVE_WALK
        ? 2.f
        : m_MoveState == PEDMOVE_RUN
            ? 4.f
            : 6.f;

    if (ped->IsPlayer()) {
        if (const auto task = ped->GetTaskManager().FindTaskByType(TASK_PRIMARY_PRIMARY, TASK_COMPLEX_ENTER_CAR_AS_DRIVER)) {
            m_EnterCarStartTime = static_cast<CTaskComplexEnterCar*>(task)->GetEnterCarStartTime();
        }
    }

    if (!m_Veh->vehicleFlags.bIsBig && distToRoute > distThreshold && distToVeh > distThreshold && dot > 0.f) {
        return CreateRouteTask(ped);
    }

    // Otherwise first turn towards the route
    CTaskSimpleStandStill standStill{0, false, false, 8.f};
    standStill.ProcessPed(ped);

    const auto& firstPt = m_Route->m_Entries[0];
    const auto  heading = CGeneral::LimitRadianAngle(
        CGeneral::GetRadianAngleBetweenPoints(firstPt.x - ped->GetPosition().x, firstPt.y - ped->GetPosition().y, 0.f, 0.f)
    );
    return new CTaskSimpleAchieveHeading{heading, 1.f, 0.1f};
}

// 0x654370
CTask* CTaskComplexWalkRoundCar::ControlSubTask(CPed* ped) {
    if (m_bFirstSubTaskNeedsToBeCreated) {
        m_bFirstSubTaskNeedsToBeCreated = false;
        return CreateFirstSubTask(ped);
    }

    auto newTaskType = TASK_NONE;
    bool bPlayerQuitEnter = false;
    if (ped->IsPlayer() && m_Veh && m_EnterCarStartTime != -1 && CCarEnterExit::IsPlayerToQuitCarEnter(ped, m_Veh, m_EnterCarStartTime, m_pSubTask) && m_pSubTask->GetTaskType() == TASK_COMPLEX_FOLLOW_POINT_ROUTE) {
        bPlayerQuitEnter = true;
        newTaskType      = TASK_FINISHED;
    } else if (m_pSubTask->GetTaskType() == TASK_COMPLEX_FOLLOW_POINT_ROUTE && m_Timer.IsOutOfTime()) {
        newTaskType      = TASK_FINISHED;
    }

    // Has the vehicle moved/rotated significantly since the route was computed?
    if (m_Veh) {
        const auto& vehPos = m_Veh->GetPosition();
        const auto& vehFwd = m_Veh->GetForward();
        const auto& vehRight = m_Veh->GetRight();

        const float dx = m_VehPos.x - vehPos.x;
        const float dy = m_VehPos.y - vehPos.y;
        const float dz = m_VehPos.z - vehPos.z;
        if (dz * dz + dy * dy + dx * dx <= 0.0625f
            && !(vehFwd.z * m_VehMatFwd.z + vehFwd.y * m_VehMatFwd.y + vehFwd.x * m_VehMatFwd.x < 0.9f)
            && !(m_VehMatRight.z * vehRight.z + m_VehMatRight.y * vehRight.y + vehRight.x * m_VehMatRight.x < 0.9f)
            && newTaskType == TASK_NONE
        ) {
            return m_pSubTask;
        }
    }

    if (m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_URGENT, nullptr)) {
        if (bPlayerQuitEnter) {
            if (const auto task = ped->GetTaskManager().m_aPrimaryTasks[TASK_PRIMARY_PRIMARY]) {
                if (task->GetTaskType() == TASK_COMPLEX_ENTER_CAR_AS_DRIVER) {
                    task->MakeAbortable(ped, ABORT_PRIORITY_LEISURE, nullptr);
                }
            }
        }
        return nullptr;
    }
    return m_pSubTask;
}

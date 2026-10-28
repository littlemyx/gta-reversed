#include "StdInc.h"
#include "TaskComplexFollowPointRoute.h"
#include "TaskSimpleGoToPointFine.h"
#include "TaskComplexGoToPointAndStandStill.h"
#include "TaskSimpleGoToPoint.h"
#include "TaskSimpleLeaveGroup.h"
#include "TaskSimpleStandStill.h"
#include "TaskComplexLeaveCar.h"
#include "TaskComplexWalkRoundCar.h"

void CTaskComplexFollowPointRoute::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexFollowPointRoute, 0x870078, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x671510);

    RH_ScopedInstall(GetSubTaskType, 0x668FE0);
    RH_ScopedInstall(CreateTask, 0x669090);
    RH_ScopedInstall(GetLastWaypoint, 0x669340);
    RH_ScopedInstall(GetNextWaypoint, 0x669420);
    RH_ScopedInstall(CalcGoToTaskType, 0x66E150);
    RH_ScopedInstall(CalcBlendRatio, 0x66E710);

    RH_ScopedVMTInstall(Clone, 0x673BE0);
    RH_ScopedVMTInstall(GetTaskType, 0x66E0B0);
    RH_ScopedVMTInstall(MakeAbortable, 0x668EC0);
    RH_ScopedVMTInstall(CreateNextSubTask, 0x671630);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x66E320);
    RH_ScopedVMTInstall(ControlSubTask, 0x66E860);
}

// 0x671510
CTaskComplexFollowPointRoute::CTaskComplexFollowPointRoute(
    eMoveState moveState,
    CPointRoute const& route,
    Mode mode,
    float targetRadius,
    float slowDownDist,
    bool overshootTarget,
    bool useBlending,
    bool standStillAfterMove
) :
    m_moveState{moveState},
    m_route{ new CPointRoute{route} },
    m_mode{mode},
    m_targetRadius{targetRadius},
    m_slowDownDistance{slowDownDist},

    m_bMustOvershootTarget{overshootTarget},
    m_bUseBlending{useBlending},
    m_bStandStillAfterMove{standStillAfterMove},
    m_bSpeedingUp{ useBlending }
{
}

CTaskComplexFollowPointRoute::~CTaskComplexFollowPointRoute() {
    delete m_route;
}

CTaskComplexFollowPointRoute::CTaskComplexFollowPointRoute(const CTaskComplexFollowPointRoute& o) :
    CTaskComplexFollowPointRoute{
        o.m_moveState,
       *o.m_route,
        o.m_mode,
        o.m_targetRadius,
        o.m_slowDownDistance,
        o.m_bMustOvershootTarget,
        o.m_bUseBlending,
        o.m_bStandStillAfterMove
    }
{
}

// 0x668FE0
eTaskType CTaskComplexFollowPointRoute::GetSubTaskType() {
    if (m_route->IsEmpty()) {
        return TASK_FINISHED;
    }

    if (m_currPointIdx + 1 < m_route->m_NumEntries) { // Current is not the last
        return TASK_SIMPLE_GO_TO_POINT;
    }

    if (m_currPointIdx + 1 == m_route->m_NumEntries) { // Current will the last point
        return m_bStandStillAfterMove ? TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL : TASK_SIMPLE_GO_TO_POINT;
    }

    if (m_currPointIdx == m_route->m_NumEntries) { // Current is the last point
        m_nRouteTraversals++;

        switch (m_mode) {
        case Mode::ONE_WAY: {
            if (m_bUseBlending) {
                m_bWillSlowDown = m_bSlowingDown = m_bSpeedingUp = false;
            }
            return TASK_FINISHED;
        }
        case Mode::RETURN: {
            if (m_nRouteTraversals != 1) {
                return TASK_FINISHED;
            }
            [[fallthrough]];
        }
        case Mode::BACK_AND_FORTH: {
            m_route->Reverse();
            [[fallthrough]];
        }
        case Mode::LOOP: {
            m_currPointIdx = 0;
            return GetSubTaskType(); // Recurse
        }
        }
    }

    return TASK_NONE;
}

// 0x669090
CTask* CTaskComplexFollowPointRoute::CreateTask(eTaskType taskType, CPed* ped) {
    switch (taskType) {
    case TASK_SIMPLE_GO_TO_POINT_FINE: {
        const auto task = new CTaskSimpleGoToPointFine{ CTaskSimpleGoToPointFine::BaseRatio(m_moveState), m_route->GetAll()[m_currPointIdx] };
        if (m_bSlowingDown) {
            task->gotoFlags.m_targetCircledFlags = 0;
        }
        return task;
    }
    case TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL:
        return new CTaskComplexGoToPointAndStandStill{
            m_moveState,
            m_route->GetAll()[m_currPointIdx],
            m_targetRadius,
            [this] {
                switch (m_mode) {
                case Mode::RETURN:
                    return m_nRouteTraversals ? m_slowDownDistance : 0.f;
                case Mode::LOOP:
                case Mode::BACK_AND_FORTH:
                    return 0.f;
                default:
                    return m_slowDownDistance;
                }
            }(),
            false,
            m_Parent && m_Parent->GetTaskType() == TASK_COMPLEX_WALK_ROUND_CAR && static_cast<CTaskComplexWalkRoundCar*>(m_Parent)->GoingForDoor()
        };
    case TASK_SIMPLE_GO_TO_POINT:
        return new CTaskSimpleGoToPoint{
            m_moveState,
            m_route->m_Entries[this->m_currPointIdx],
            0.5f,
            false,
            m_bMustOvershootTarget
        };
    case TASK_NONE:
        return new CTaskSimpleLeaveGroup{};
    case TASK_SIMPLE_STAND_STILL:
        return new CTaskSimpleStandStill{ STAND_STILL_TIME };
    case TASK_COMPLEX_LEAVE_CAR:
        return new CTaskComplexLeaveCar{ ped->m_pVehicle, TARGET_DOOR_FRONT_LEFT, 0, true, false };
    case TASK_FINISHED:
        return nullptr;
    default:
        NOTSA_UNREACHABLE();
        return nullptr; // 0x669322: default => edi = 0
    }
}

// 0x669340
CVector CTaskComplexFollowPointRoute::GetLastWaypoint(CPed* ped) {
    if (m_route->m_NumEntries == 1) {
        return m_route->GetAll().front();
    }

    switch (m_mode) {
    case Mode::ONE_WAY:
    case Mode::RETURN:
    case Mode::BACK_AND_FORTH:
    case Mode::LOOP:
        break;
    default:
        NOTSA_UNREACHABLE(); // TODO: If this isn't ever reached remove the switch
        return {}; // SA
    }

    if (m_currPointIdx) {
        return (*m_route)[m_currPointIdx - 1];
    }

    // Ternary inverted
    return m_nRouteTraversals
        ? m_route->GetAll().back() // Last point was the previous
        : ped->GetPosition();         // Unlikely scenario, but just in case
}

// 0x669420
CVector CTaskComplexFollowPointRoute::GetNextWaypoint(CPed* ped) {
    if (m_route->m_NumEntries == 1) {
        return m_route->GetAll().front();
    }

    // 0x669448..0x66946A: LOOP (mode 3) is handled like the other modes (wraps to entry 0 when `next == numEntries`, 0x669461 je 0x66942b); only a mode outside 0..3 returns the zero vector (0x669497)
    const auto next = m_currPointIdx + 1;
    switch (m_mode) {
    case Mode::ONE_WAY:
    case Mode::RETURN:
    case Mode::BACK_AND_FORTH:
        return (*m_route)[next < m_route->m_NumEntries ? next : m_currPointIdx - 1];
    case Mode::LOOP:
        return (*m_route)[next == m_route->m_NumEntries ? 0 : next];
    default:
        NOTSA_UNREACHABLE();
        return {}; // SA
    }
}

// 0x66E150
eTaskType CTaskComplexFollowPointRoute::CalcGoToTaskType(CPed* ped, eTaskType subTaskType) {
    // NOTE: rewritten from the asm - the previous version cleared the wrong flag (m_bUseBlending instead of m_bWillSlowDown), never raised m_bWillSlowDown/m_bUseBlending where the exe does,
    // tested m_bWillSlowDown (0x10) instead of m_bSlowingDown (0x20) at the end and never copied the decrease amount to the increase amount.
    m_speedIncreaseAmt = m_speedDecreaseAmt; // 0x66E157: [this+38h] = [this+34h] (before the decrease amount is recomputed)
    m_bWillSlowDown    = false;              // 0x66E163: flags &= ~0x10

    if (m_moveState > PEDMOVE_WALK) {
        const int32 numPts = (int32)m_route->m_NumEntries;
        int32       currPt = (int32)m_currPointIdx;
        if (currPt >= numPts) { // 0x66E17A
            currPt = numPts - 1;
        }
        if (currPt < 0) { // 0x66E183 (returns without touching the flags / the tail below)
            return subTaskType;
        }

        const CVector currPtPos = (*m_route)[currPt];

        CVector prevToCurr = currPtPos - GetLastWaypoint(ped); // 0x66E1A5
        prevToCurr.z       = 0.f;
        prevToCurr.Normalise();

        CVector currToNext = GetNextWaypoint(ped) - currPtPos; // 0x66E202
        currToNext.z       = 0.f;
        currToNext.Normalise();

        // 0x66E22A: x87 order of operations
        const float dot = (prevToCurr.y * currToNext.y + prevToCurr.x * currToNext.x) + prevToCurr.z * currToNext.z;
        if (dot < 0.707f) { // 0x863C1C: ~cos(45deg); sharper turn than that => slow down
            float t = 1.f - (dot + 1.f) * 0.58582306f; // 0x858624 (1.0), 0x8700A4
            if (t < 0.f) {
                t = 0.f;
            } else if (t > 1.f) {
                t = 1.f;
            }
            if (m_moveState == PEDMOVE_SPRINT) {
                m_speedDecreaseDist = m_speedIncreaseDist = 5.f;
                m_speedDecreaseAmt  = t * 3.f;
            } else {
                m_speedDecreaseDist = m_speedIncreaseDist = 4.f;
                m_speedDecreaseAmt  = t * 1.5f;
            }
            m_bWillSlowDown = true; // 0x66E2B0 / 0x66E2D1 (or 0x10)
        }
    }

    if (m_bSlowingDown && (subTaskType == TASK_SIMPLE_GO_TO_POINT || subTaskType == TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL)) { // 0x66E2DF
        m_bSlowingDown = false;
        m_bSpeedingUp  = true;
        return TASK_SIMPLE_GO_TO_POINT_FINE;
    }
    m_bSlowingDown = m_bSpeedingUp = false; // 0x66E30C: flags &= ~0x60
    return subTaskType;
}

// 0x66E710
float CTaskComplexFollowPointRoute::CalcBlendRatio(CPed* ped, bool slowing) {
    const auto Calculate = [this, ped](float requiredDist, const CVector& relativeToPoint, float amount) {
        const auto currDistSq = (ped->GetPosition() - relativeToPoint).SquaredMagnitude();
        if (currDistSq >= sq(requiredDist)) {
            return -1.f;
        }
        const auto distProg  = std::clamp(currDistSq / sq(requiredDist), 0.f, 1.f);
        const float mult      = X87R(x87::cos(distProg * PI) / 2.f) + 0.5f;
        return std::max(
            CTaskSimpleGoToPointFine::BaseRatio(m_moveState) - (amount * mult),
            CTaskSimpleGoToPointFine::BaseRatio(PEDMOVE_WALK)
        );
    };
    if (slowing) {
        return Calculate(
            m_speedDecreaseDist,
            (*m_route)[m_currPointIdx],
            m_speedDecreaseAmt
        );
    } else {
        return Calculate(
            m_speedIncreaseDist,
            GetLastWaypoint(ped),
            m_speedIncreaseAmt
        );
    }
}

// 0x668EC0
bool CTaskComplexFollowPointRoute::MakeAbortable(CPed* ped, eAbortPriority priority, CEvent const* event) {
    
    // Adjust GoTo tatk's radius if event is `CEventVehicleCollision` or `CEventPotentialWalkIntoVehicle`
    if (event && m_currPointIdx + 1 < m_route->m_NumEntries) {
        const auto vehicle =
            [event]() -> CVehicle* {
            switch (event->GetEventType()) {
            case EVENT_VEHICLE_COLLISION:
                return static_cast<const CEventVehicleCollision*>(event)->m_vehicle;
            case EVENT_POTENTIAL_WALK_INTO_VEHICLE:
                return static_cast<const CEventPotentialWalkIntoVehicle*>(event)->m_vehicle;
            default:
                return nullptr;
            }
        }();
        if (vehicle) {
            switch (m_pSubTask->GetTaskType()) {
            case TASK_SIMPLE_GO_TO_POINT:
            case TASK_SIMPLE_GO_TO_POINT_FINE: {
                const auto gotoPointTask = static_cast<CTaskSimpleGoToPoint*>(m_pSubTask);
                if (vehicle->m_vecMoveSpeed.SquaredMagnitude() < sq(0.125f)) {
                    if (CPedGeometryAnalyser::IsEntityBlockingTarget(
                        vehicle,
                        gotoPointTask->m_vecTargetPoint,
                        gotoPointTask->m_fRadius
                    )) {
                        gotoPointTask->m_fRadius = (vehicle->GetPosition() - ped->GetPosition()).Magnitude2D();
                    }
                }
                break;
            }
            }
        }
    }

    if (priority == ABORT_PRIORITY_LEISURE) {
        m_route->Clear();
    }

    return  m_pSubTask->MakeAbortable(ped, priority, event);
}

// 0x671630
CTask* CTaskComplexFollowPointRoute::CreateNextSubTask(CPed* ped) {
    switch (m_pSubTask->GetTaskType()) {
    case TASK_FINISHED:
        return nullptr;
    case TASK_COMPLEX_LEAVE_CAR:
        return ped->bInVehicle ? nullptr : CreateFirstSubTask(ped);
    case TASK_NONE:               // 0x67167A: after "none" / stand-still the task is over (the port ran these through the route test below and advanced past the end)
    case TASK_SIMPLE_STAND_STILL: // 0x67169E
        return CreateTask(TASK_FINISHED, ped);
    case TASK_SIMPLE_GO_TO_POINT: { // 0x6716C2
        if (m_currPointIdx + 1 == m_route->m_NumEntries && !m_bStandStillAfterMove) {
            return CreateTask(TASK_FINISHED, ped);
        }
        break;
    }
    }

    // Otherwise advance to the next point..
    m_currPointIdx++;

    const auto nextSubTask = GetSubTaskType();
    return CreateTask(
        m_bUseBlending ? CalcGoToTaskType(ped, nextSubTask) : nextSubTask,
        ped
    );
}

// 0x66E320
CTask* CTaskComplexFollowPointRoute::CreateFirstSubTask(CPed* ped) {
    m_bNewRoute = false;

    if (m_bUseBlending) {
        m_speedDecreaseDist = m_speedIncreaseDist = m_speedDecreaseAmt = m_speedIncreaseAmt = 0.f;
    }

    if (m_route->IsEmpty()) {
        return CreateTask(TASK_NONE, ped);
    }

    if (ped->bInVehicle) {
        return CreateTask(TASK_COMPLEX_LEAVE_CAR, ped);
    }


    // Find next point to follow
    if (m_mode != Mode::ONE_WAY) {
        auto closestNextPtIdx    = -1;

        const auto& pedPos = ped->GetPosition();
        const auto  npts = m_route->m_NumEntries;
        {
            auto closestNextPtDistSq = std::numeric_limits<float>::max();
            for (auto i = 0u; i < npts; i++) {
                const auto nextPtIdx       = (i + 1) % npts;

                const auto& thisPtPos       = (*m_route)[i]; // No need for `%` here, trust me
                const auto  thisToNextPt    = (*m_route)[nextPtIdx] - thisPtPos;
                const auto  thisToNextPtDir = thisToNextPt.Normalized();
                const auto  dot             = (pedPos - thisPtPos).Dot(thisToNextPtDir); // NOTE: Since the first argument isn't normalized the dot will be scaled by it's magnitue.

                if (dot <= 0.f) { 
                    continue; // Ped is in the opposite direction compared to the next point relative to the current point.
                }

                if (thisToNextPt.SquaredMagnitude() <= sq(dot)) { // Simplified the square-root by squaring both sides
                    continue;
                }

                const auto proj = thisPtPos + thisToNextPtDir * dot; // Project `thisToNextPtDir` onto the segment between the `thisPtPos` and `pedPos`
                const auto dist = (pedPos - proj).SquaredMagnitude();
                if (dist < sq(closestNextPtDistSq)) { // NOTE: I think they did a fuckup again? Why square it?...
                    closestNextPtDistSq = dist; // ...here it's not square-rooted, so
                    closestNextPtIdx = (int32)nextPtIdx;
                }
            }
        }

        // If above loop didn't manage to find a point, try this
        if (closestNextPtIdx == -1) {
            auto closestDistSq = std::numeric_limits<float>::max();
        
            for (auto i = 0u; i < npts; i++) {
                const auto dist = (pedPos - (*m_route)[i]).SquaredMagnitude();
                if (dist < sq(closestDistSq)) { // Noo, they must be doing this on purpose (squaring the previous closest point's dist)
                    closestDistSq = dist;
                    closestNextPtIdx = (int32)i;
                }
            }
        }

        assert(closestNextPtIdx >= 0); // Since there's an early out for `m_route->IsEmpty()` there has to be one point.
        m_currPointIdx = closestNextPtIdx;
    }


    // Same code found in `CreateNextSubTask`
    const auto nextSubTask = GetSubTaskType();
    return CreateTask(
        m_bUseBlending ? CalcGoToTaskType(ped, nextSubTask) : nextSubTask,
        ped
    );
}

// 0x66E860
CTask* CTaskComplexFollowPointRoute::ControlSubTask(CPed* ped) {
    const auto subTaskType = m_pSubTask->GetTaskType();

    if (subTaskType == TASK_COMPLEX_LEAVE_CAR) {
        return m_pSubTask;
    }

    if (ped->GetIntelligence()->m_AnotherStaticCounter > 30 || m_route->IsEmpty()) {
        return nullptr;
    }

    if (m_bNewRoute) { // exe 0x66E89A-0x66E8B1: (missing in the port) a new route restarts the sub-task chain
        return CreateFirstSubTask(ped);
    }

    if (m_bUseBlending && subTaskType == TASK_NONE) {
        m_bWillSlowDown = m_bSlowingDown = m_bSpeedingUp = false;
        return m_pSubTask;
    }

    if (m_bUseBlending) {
        const auto gotoPointFine = static_cast<CTaskSimpleGoToPointFine*>(m_pSubTask);

        if (subTaskType == TASK_SIMPLE_GO_TO_POINT_FINE) { // Move this out here, as it's common for both
            if (m_bSlowingDown) {
                gotoPointFine->SetMoveRatio([this, ped] {
                    if (const auto ratio = CalcBlendRatio(ped, true); ratio >= 0.f) {
                        return ratio;
                    }
                    return CTaskSimpleGoToPointFine::BaseRatio(m_moveState);
                }());
            } else if (m_bSpeedingUp) {
                if (const auto moveRatio = CalcBlendRatio(ped, false); moveRatio < 0.f) { // exe 0x66E9BF: pushes 0 (not 1) for the speeding-up case
                    m_bSpeedingUp = false;
                    gotoPointFine->SetMoveRatio(gotoPointFine->BaseRatio(m_moveState));
                    return CreateTask(TASK_SIMPLE_GO_TO_POINT, ped);
                } else {
                    gotoPointFine->SetMoveRatio(moveRatio); // exe 0x66E9DA: ratio >= 0 => it is applied (falls into the shared SetMoveRatio at 0x66E92B)
                }
            }
        }

        if (m_bWillSlowDown && !m_bSlowingDown) {
            if (const auto moveRatio = CalcBlendRatio(ped, true); moveRatio >= 0.f) {
                m_bWillSlowDown = false; // exe 0x66E96B-0x66E971: `and ~0x50; or 0x20` => will-slow-down and speeding-up cleared, slowing-down SET
                m_bSpeedingUp   = false;
                m_bSlowingDown  = true;
                if (subTaskType == TASK_SIMPLE_GO_TO_POINT_FINE) { // Inverted
                    gotoPointFine->SetMoveRatio(moveRatio);
                } else {
                    const auto task = static_cast<CTaskSimpleGoToPointFine*>(CreateTask(TASK_SIMPLE_GO_TO_POINT_FINE, ped));
                    task->SetMoveRatio(moveRatio);
                    return task;
                }
            }
        }

    }
    return m_pSubTask;
}

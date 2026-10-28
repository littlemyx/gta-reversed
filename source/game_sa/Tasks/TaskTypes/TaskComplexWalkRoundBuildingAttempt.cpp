#include "StdInc.h"

#include "TaskComplexWalkRoundBuildingAttempt.h"
#include "TaskSimpleAchieveHeading.h"
#include "TaskComplexFollowPointRoute.h"
#include "TaskSimpleGoToPoint.h"
#include "TaskSimpleStandStill.h"
#include "TaskSimpleHitWall.h"
#include "TaskSimpleScratchHead.h"
#include "TaskSimpleLookAbout.h"

void CTaskComplexWalkRoundBuildingAttempt::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexWalkRoundBuildingAttempt, 0x86f334, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x654740);
    RH_ScopedInstall(Destructor, 0x654950);

    RH_ScopedInstall(ComputeMoveDir, 0x654B80);
    RH_ScopedInstall(CreateSubTask, 0x654C80);
    RH_ScopedInstall(ComputeCrapRoute, 0x656F80);
    RH_ScopedInstall(ComputeRoute, 0x657BA0);

    RH_ScopedVMTInstall(Clone, 0x655B90);
    RH_ScopedVMTInstall(GetTaskType, 0x654830);
    RH_ScopedVMTInstall(MakeAbortable, 0x6549D0);
    RH_ScopedVMTInstall(CreateNextSubTask, 0x656E70);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x654A00);
    RH_ScopedVMTInstall(ControlSubTask, 0x658020);
}

// 0x654740
CTaskComplexWalkRoundBuildingAttempt::CTaskComplexWalkRoundBuildingAttempt(eMoveState moveState, CVector const& targetPos, CVector const& pos, CVector const& normal, bool flag_0x1) :
    m_targetPos{targetPos},
    m_pos{pos},
    m_normal{normal},
    m_bIsHeadOnCollision{flag_0x1},
    m_moveState{(int8)moveState}
{
    m_normal.z = 0.f;
    m_normal.Normalise();
}

// 0x654840
CTaskComplexWalkRoundBuildingAttempt::CTaskComplexWalkRoundBuildingAttempt(eMoveState moveState, CEntity* target, CVector const& offset, CVector const& pos, CVector const& normal, bool bIsHeadOnCollision) :
    m_pos{pos},
    m_normal{normal},
    m_targetEntity{target},
    m_offset{offset},
    m_moveState{(int8)moveState},
    m_bIsHeadOnCollision{bIsHeadOnCollision},
    m_isWalkingAroundEntity{true}
{
    m_normal.z = 0.f;
    m_normal.Normalise();

    CEntity::RegisterReference(m_targetEntity);
}

CTaskComplexWalkRoundBuildingAttempt::CTaskComplexWalkRoundBuildingAttempt(const CTaskComplexWalkRoundBuildingAttempt& o) :
    CTaskComplexWalkRoundBuildingAttempt{(eMoveState)o.m_moveState, o.m_targetPos, o.m_pos, o.m_normal, o.m_bIsHeadOnCollision}
{
}

// 0x654950
CTaskComplexWalkRoundBuildingAttempt::~CTaskComplexWalkRoundBuildingAttempt() {
    for (auto route : { m_crapRoute, m_route }) {
        delete route;
    }
    CEntity::SafeCleanUpRef(m_targetEntity);
}

// 0x654B80
void CTaskComplexWalkRoundBuildingAttempt::ComputeMoveDir(CPed const& ped, CVector& outDir) {
    // 0x654B80..0x654C2C: the exe measures from the PED's position (matrix+0x30, or ped+4 without a matrix), never reads m_pos, projects the offset off m_normal (2D),
    // normalises the result (CVector::Normalise 0x59C910) and, when the squared length is not above 1e-8 (0x86F360), returns the ped's right vector instead (0x654C11..0x654C24)
    const CVector& p = ped.GetPosition();
    const float dx = m_targetPos.x - p.x;
    const float dy = m_targetPos.y - p.y;
    const float dot = dx * m_normal.x + dy * m_normal.y + m_normal.z * 0.f;
    outDir = m_normal;
    outDir.z = 0.f;
    outDir.x = dx - dot * m_normal.x;
    outDir.y = dy - dot * m_normal.y;
    if (outDir.x * outDir.x + outDir.y * outDir.y > 1e-8f) {
        outDir.Normalise();
    } else {
        outDir = ped.GetRight();
    }
}

// 0x654C80
CTask* CTaskComplexWalkRoundBuildingAttempt::CreateSubTask(eTaskType taskType, CPed* ped) {
    switch (taskType) {
    case TASK_SIMPLE_ACHIEVE_HEADING: {
        // Achieve heading to the first point of the point route
        // 0x654E91..0x654EDE: normalise the 3D offset (CVector::Normalise 0x59C910), then LimitRadianAngle(GetRadianAngleBetweenPoints(x, y, 0, 0)); not CVector::Heading
        CVector dir = m_route->m_Entries[0] - ped->GetPosition();
        dir.Normalise();
        return new CTaskSimpleAchieveHeading{
            CGeneral::LimitRadianAngle(CGeneral::GetRadianAngleBetweenPoints(dir.x, dir.y, 0.f, 0.f)),
            1.f,
            0.1f
        };
    }
    case TASK_COMPLEX_FOLLOW_POINT_ROUTE:
        return new CTaskComplexFollowPointRoute{
            (eMoveState)m_moveState,
            *m_route,
            CTaskComplexFollowPointRoute::Mode::ONE_WAY,
            0.5f,
            0.5f,
            false,
            false,
            true
        };
    case TASK_SIMPLE_GO_TO_POINT:
        return new CTaskSimpleGoToPoint{
            (eMoveState)m_moveState,
            m_targetPos,
            0.5f,
            false,
            false
        };
    case TASK_SIMPLE_STAND_STILL:
        return new CTaskSimpleStandStill{1000};
    case TASK_SIMPLE_HIT_WALL:
        return new CTaskSimpleHitWall{};
    case TASK_SIMPLE_SCRATCH_HEAD:
        return new CTaskSimpleScratchHead{};
    case TASK_SIMPLE_LOOK_ABOUT:
        return new CTaskSimpleLookAbout{1000};
    case TASK_FINISHED:
        return nullptr;
    default:
        NOTSA_UNREACHABLE();
        return nullptr; // 0x654E20: default => eax = 0
    }
}

// 0x656F80
void CTaskComplexWalkRoundBuildingAttempt::ComputeCrapRoute(CPed const& ped) {
    // 0x656F89..0x65705E: both rand() draws happen unconditionally and in THIS order: first the normal's scale [1, 2), then the right vector's [-1, 1)
    // (no early-out when the route is full: the exe only skips the Add)
    const auto  nrmScale   = CGeneral::GetRandomNumberInRange(1.f, 2.f);
    const auto  nrmPart    = m_normal * nrmScale;
    const auto  right      = CrossProduct(m_normal, CVector::ZAxisVector());
    const auto  rightScale = CGeneral::GetRandomNumberInRange(-1.f, 1.f); // => right or left
    const auto  rightPart  = right * rightScale;
    const auto  pedPos     = ped.GetPosition() - CVector{ 0.f, 0.f, 0.5f }; // Ped's position is it's center, so adjust it to be on the ground
    const auto  target     = (nrmPart + rightPart) + pedPos; // exe association: (normal + right) + pedPos

    CColPoint cp{};
    CEntity* hitEntity{};
    if (CWorld::ProcessLineOfSight(pedPos, target, cp, hitEntity, true, false, false, false, false, false, false, false)) {
        auto cpToPedDir = cp.m_vecPoint - pedPos;
        if (!(cpToPedDir.NormaliseAndMag() >= 0.35f)) { // 0x65714D: `fcomp` + `test ah, 1` => also taken for NaN
            return;
        }
        m_crapRoute->AddUnlessFull(cp.m_vecPoint - cpToPedDir * 0.35f);
    } else {
        m_crapRoute->AddUnlessFull(target);
    }

    m_crapRouteHasPoints = true;
}

// 0x657BA0
void CTaskComplexWalkRoundBuildingAttempt::ComputeRoute(CPed const& ped) {
    m_route->Clear(); // 0x657BBC
    ++m_nAttempts;

    // If there were too many attempts just use crap route
    if (m_nAttempts >= 20) { // 0x657BD7: `cmp al, 0x14; jge` on the already incremented counter
        if (!m_crapRouteHasPoints) {
            ComputeCrapRoute(ped);
        }
        *m_route = *m_crapRoute;
        m_routeHasPoints = true;
        return;
    }

    const auto normal2x = m_normal * 2.f;

    size_t numBlocked{}; // 0x657F96: number of iterations whose ped => target line of sight was blocked
    for (int32 num{1}; num >= -1; num -= m_bIsHeadOnCollision ? 2 : 3) {
        CVector moveDir{};
        ComputeMoveDir(ped, moveDir);
        moveDir *= CGeneral::GetRandomNumberInRange(-0.2f, 0.2f) + (float)m_nAttempts; // Apply some semi-random spread
        moveDir *= (float)num;
        moveDir += m_normal * 0.35f;

        // 0x657D4A..0x657D76: the target is measured from `m_pos` (NOT from the ped), the ped's position is only used as the line-of-sight origin
        const CVector target{
            moveDir.x + m_pos.x,
            moveDir.y + m_pos.y,
            (moveDir.z + m_pos.z) - 0.5f // Position is a center, so adjust it to be on the ground
        };
        const auto origin = target - Normalized(moveDir) * 0.7f;
        const auto pedPos = ped.GetPosition() - CVector{ 0.f, 0.f, 0.5f };

        if (!CWorld::GetIsLineOfSightClear(pedPos, target, true, false, false, false, false, false, false)) { // 0x657DE6 (the last arg is 0 in the exe)
            numBlocked++;
            continue;
        }

        if (CWorld::GetIsLineOfSightClear(target, m_targetPos, true, false, false, false, false, false, false)) {
            m_route->AddUnlessFull(target);
            m_routeHasPoints = true;
            return;
        }

        const auto targetOffseted = target - normal2x;

        if (!CWorld::GetIsLineOfSightClear(target, targetOffseted, true, false, false, false, false, false, false)) {
            continue;
        }

        if (!CWorld::GetIsLineOfSightClear(origin, origin - normal2x, true, false, false, false, false, false, false)) {
            continue;
        }

        if (!m_crapRouteHasPoints) {
            m_crapRoute->Clear(); // 0x657ECF
            m_crapRoute->AddMultipleUnlessFull(target, targetOffseted);
            m_crapRouteHasPoints = true;
        }

        if (CWorld::GetIsLineOfSightClear(targetOffseted, m_targetPos, true, false, false, false, false, false, false)) {
            m_route->AddMultipleUnlessFull(target, targetOffseted);
            m_routeHasPoints = true;
        }
    }

    if (m_nAttempts == 1) { // Current is the first attempt
        if (numBlocked == (m_bIsHeadOnCollision ? 2u : 1u)) {
            m_nAttempts = 20;
        }
    }
}

// 0x6549D0
bool CTaskComplexWalkRoundBuildingAttempt::MakeAbortable(CPed* ped, eAbortPriority priority, CEvent const* event) {
    if (m_pSubTask->MakeAbortable(ped, priority, event)) {
        ped->bIgnoreHeightCheckOnGotoPointTask = false;
        return true;
    }
    return false;
}

// 0x656E70
CTask* CTaskComplexWalkRoundBuildingAttempt::CreateNextSubTask(CPed* ped) {
    const auto TaskFinished = [ped, this] {
        ped->bIgnoreHeightCheckOnGotoPointTask = false;
        return CreateSubTask(TASK_FINISHED, ped);
    };

    if (m_justAbort) {
        return TaskFinished();
    }

    switch (m_pSubTask->GetTaskType()) {
    case TASK_SIMPLE_ACHIEVE_HEADING:
        return CreateSubTask(TASK_COMPLEX_FOLLOW_POINT_ROUTE, ped);
    case TASK_SIMPLE_GO_TO_POINT:
    case TASK_SIMPLE_SCRATCH_HEAD:
    case TASK_SIMPLE_LOOK_ABOUT:
    case TASK_SIMPLE_HIT_WALL:
        return CreateSubTask(m_routeHasPoints ? TASK_SIMPLE_ACHIEVE_HEADING : TASK_SIMPLE_LOOK_ABOUT, ped);
    case TASK_SIMPLE_STAND_STILL:
    case TASK_COMPLEX_FOLLOW_POINT_ROUTE:
        return TaskFinished();
    default:
        NOTSA_UNREACHABLE();
        return TaskFinished(); // 0x656F35: default => clears bIgnoreHeightCheckOnGotoPointTask, returns nullptr
    }
}

// 0x654A00
CTask* CTaskComplexWalkRoundBuildingAttempt::CreateFirstSubTask(CPed* ped) {
    if (m_isWalkingAroundEntity) {
        if (!m_targetEntity) {
            ped->bIgnoreHeightCheckOnGotoPointTask = false;
            return nullptr;
        }
        m_targetPos = m_targetEntity->GetMatrix().TransformPoint(m_offset);
    }

    ped->bIgnoreHeightCheckOnGotoPointTask = true;

    switch (m_moveState) {
    case PEDMOVE_STILL:
    case PEDMOVE_TURN_L:
    case PEDMOVE_TURN_R:
    case PEDMOVE_WALK:
        return new CTaskSimpleScratchHead{};
    case PEDMOVE_RUN:
    case PEDMOVE_SPRINT:
        return new CTaskSimpleHitWall{};
    default:
        ped->bIgnoreHeightCheckOnGotoPointTask = false;
        return nullptr;
    }
}

// 0x658020
CTask* CTaskComplexWalkRoundBuildingAttempt::ControlSubTask(CPed* ped) {
    if (m_justAbort) {
        return m_pSubTask;
    }

    if (m_isWalkingAroundEntity) {
        // 0x658061..0x6580C2: |M * offset - targetPos|^2 (z,y,x sum order, in the FPU registers) compared STRICTLY above 16 (`fcomp` + `jne` on C0|C3: equal and NaN keep going)
        if (!m_targetEntity) {
            m_justAbort = true;
            return CreateSubTask(TASK_SIMPLE_STAND_STILL, ped);
        }
        const auto entityPt = m_targetEntity->GetMatrix().TransformPoint(m_offset);
        const x87d dx = (x87d)entityPt.x - m_targetPos.x;
        const x87d dy = (x87d)entityPt.y - m_targetPos.y;
        const x87d dz = (x87d)entityPt.z - m_targetPos.z;
        if ((dz * dz + dy * dy) + dx * dx > (x87d)sq(4.f)) {
            m_justAbort = true;
            return CreateSubTask(TASK_SIMPLE_STAND_STILL, ped);
        }
    }

    if (!m_routeHasPoints
     || m_isFollowingRouteNow
     || !m_pSubTask->MakeAbortable(ped)
    ) {
        if (!m_routeHasPoints) {
            ComputeRoute(*ped);
        }

        if (m_pSubTask->GetTaskType() != TASK_COMPLEX_FOLLOW_POINT_ROUTE
         || ped->GetIntelligence()->m_AnotherStaticCounter <= 30
         || !m_pSubTask->MakeAbortable(ped)
        ) {
            return m_pSubTask;
        }
    } else {
        m_isFollowingRouteNow = true;
        if (!m_route->IsEmpty()) {
            return CreateSubTask(TASK_COMPLEX_FOLLOW_POINT_ROUTE, ped);
        }
    }

    m_justAbort = true;
    return CreateSubTask(TASK_SIMPLE_HIT_WALL, ped);
}

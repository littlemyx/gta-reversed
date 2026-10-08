#include "StdInc.h"

#include "TaskComplexKillPedOnFootArmed.h"
#include "TaskSimpleGoToPoint.h"
#include "TaskSimpleDuck.h"
#include "TaskSimpleGunControl.h"
#include "TaskSimpleUseGun.h"
#include "TaskSimpleStandStill.h"
#include "TaskSimplePause.h"
#include "TaskSimpleThrowControl.h"
#include "TaskComplexGoToPointAndStandStill.h"
#include "SeekEntity/TaskComplexSeekEntityStandard.h"
#include "Cover.h"
#include "CoverPoint.h"
#include <extensions/utility.hpp>

void CTaskComplexKillPedOnFootArmed::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexKillPedOnFootArmed, 0x86d918, 11);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(Constructor, 0x621190);
    RH_ScopedInstall(Destructor, 0x621250);

    RH_ScopedInstall(LineOfSightClearForAttack, 0x621500);
    RH_ScopedInstall(IsPedInLeaderFiringLine, 0x621300);
    RH_ScopedInstall(CreateSubTask, 0x626FC0);

    RH_ScopedVMTInstall(Clone, 0x6234C0);
    RH_ScopedVMTInstall(GetTaskType, 0x621240);
    RH_ScopedVMTInstall(MakeAbortable, 0x6212B0);
    RH_ScopedVMTInstall(CreateNextSubTask, 0x62C190);
    RH_ScopedVMTInstall(CreateFirstSubTask, 0x62BF00);
    RH_ScopedVMTInstall(ControlSubTask, 0x62CCE0);
}

// 0x621190
CTaskComplexKillPedOnFootArmed::CTaskComplexKillPedOnFootArmed(
    CPed*  target,
    uint32 duckingConditions,
    uint32 duckTime,
    uint32 duckChancePerc,
    int8   competence
) :
    m_target{ target },
    m_duckingConditions{ duckingConditions },
    m_lengthOfDuck{ duckTime },
    m_duckChancePerc{ duckChancePerc },
    m_competence{ competence }
{
    CEntity::SafeRegisterRef(m_target);
}

// notsa
CTaskComplexKillPedOnFootArmed::CTaskComplexKillPedOnFootArmed(const CTaskComplexKillPedOnFootArmed& o) :
    CTaskComplexKillPedOnFootArmed{
        o.m_target,
        o.m_duckingConditions,
        o.m_lengthOfDuck,
        o.m_duckChancePerc,
        o.m_competence
    }
{
    m_aimImmediate = o.m_aimImmediate;
}

// 0x621250
CTaskComplexKillPedOnFootArmed::~CTaskComplexKillPedOnFootArmed() {
    CEntity::SafeCleanUpRef(m_target);
}

// 0x621500
bool CTaskComplexKillPedOnFootArmed::LineOfSightClearForAttack(CPed* ped) { // ped is the task owner ped
    /*
    * TODO:
    * here's some code to calculate the values to use (based on some variables)
    * nothing special, but it's messy, and it's late, so I won't bother
    */
    const auto reqDistDeltaSq = sq(2.f);
    const auto reqTimeDelta   = 5000;

    //> If the LOS was recently clear, let's consider it still is
    if (CTimer::GetTimeInMS() - m_losClearTime < reqTimeDelta) {
        return true;
    }

    //> Perhaps check if the entities are still kinda around the same position as the last time the LOS was blocked...
    if (CTimer::GetTimeInMS() - m_losBlockedTime < reqTimeDelta) {
        if (reqDistDeltaSq >= (m_target->GetPosition() - m_losBlockedTargetPos).SquaredMagnitude()) {
            if (reqDistDeltaSq >= (ped->GetPosition() - m_losBlockedOurPos).SquaredMagnitude()) {
                return false;
            }
        }
    }

    //> 0x6216DD
    // Temporarily disable the target ped vehicle's collision (To ignore it)
    // Perhaps, `CWorld::pIgnoreEntity` could be used?
    const auto targetVeh = m_target->GetVehicleIfInOne();
    const notsa::ScopeGuard restore{[targetVeh, had = targetVeh && targetVeh->GetUsesCollision()] {
        if (targetVeh) {
            targetVeh->SetUsesCollision(had);
        }
    }};

    if (targetVeh) {
        targetVeh->SetUsesCollision(false);
    }

    const auto GetPedHeadPos = [](CPed* headOf) {
        CVector inout{ 0.1f, 0.f, 0.f };
        headOf->GetTransformedBonePosition(inout, BONE_HEAD);
        return inout;
    };

    if (CWorld::GetIsLineOfSightClear(
        ped->bIsDucking
            ? GetPedHeadPos(ped)
            : ped->GetPosition() + CVector{0.f, 0.f, 0.25f},
        GetPedHeadPos(m_target), // Always use head position
        true,
        true,
        false,
        true,
        false,
        true,
        false
    )) {
        m_losClearTime   = CTimer::GetTimeInMS();
        m_losBlockedTime = 0;
        return true;
    } else {
        m_losBlockedTime      = CTimer::GetTimeInMS();
        m_losClearTime        = 0;
        m_losBlockedOurPos    = ped->GetPosition();
        m_losBlockedTargetPos = m_target->GetPosition();
        return false;
    }
}

// 0x621300
bool CTaskComplexKillPedOnFootArmed::IsPedInLeaderFiringLine(CPed* ped) {
    const auto pedGrp = ped->GetGroup();
    if (!pedGrp) {
        return false;
    }

    const auto grpLeaderPlyr = pedGrp->GetMembership().GetLeader();
    if (!grpLeaderPlyr || !grpLeaderPlyr->IsPlayer()) {
        return false;
    }

    if (!grpLeaderPlyr->m_pTargetedObject || grpLeaderPlyr->GetActiveWeapon().IsTypeMelee()) {
        return false;
    }

    const auto &leaderPos2D      = grpLeaderPlyr->GetPosition2D();
    const auto &leaderPos        = grpLeaderPlyr->GetPosition();
    const auto leaderToPed       = ped->GetPosition() - leaderPos, // 0x6213BD
               leaderToTargetDir = (grpLeaderPlyr->m_pTargetedObject->GetPosition() - leaderPos).Normalized(); // 0x621394

    /* clang-format off
     * --[projPointOnLeaderToTargetRay2D]-->[leaderPos]---[leaderToTargetDir]-->[Target]
     *                                          /
     *                                         /
     *                                        /
     *                                       /
     *                                 [leaderToPed]
     *                                     /
     *                                    /
     *                                  \|/
     *                                [Ped]
     * clang-format on */

    //> 0x6213AE
    const auto projPointOnLeaderToTargetRay2D = leaderPos + CVector2D{ leaderToPed }.ProjectOnToNormal(leaderToTargetDir);
    if ((projPointOnLeaderToTargetRay2D - ped->GetPosition2D()).SquaredMagnitude() >= sq(2.f)) {
        return false;
    }

    //> 0x0621482
    if (leaderToTargetDir.Dot(leaderToPed) <= 0.f) { // Ped is "behind" leader (Like on the ASCII art above)
        return false;
    }

    //> 0x6214CD
    if (leaderToPed.SquaredMagnitude() >= sq(10.f)) {
        return false;
    }

    return true;
}

// 0x626FC0
CTask* CTaskComplexKillPedOnFootArmed::CreateSubTask(eTaskType taskType, CPed* ped) {
    const auto& ourWep = ped->GetActiveWeapon().GetWeaponInfo(ped);

    switch (taskType) {
    case TASK_SIMPLE_DUCK:
        return new CTaskSimpleDuck{ DUCK_STANDALONE, (uint16)m_lengthOfDuck, -1 };
    case TASK_SIMPLE_STAND_STILL:
        return new CTaskSimpleStandStill{ 20000, false, false, 8.f }; // 0x86DB24 = 20000
    case TASK_SIMPLE_PAUSE: {
        CTaskSimpleStandStill{ 0, false, false, 8.f }.ProcessPed(ped); // Makes the ped stand still
        return new CTaskSimplePause{ 100 };
    }
    case TASK_COMPLEX_SEEK_ENTITY: {
        CTask* task{};
        const auto SeekTarget = [&](float maxEntityDist2D) {
            return new CTaskComplexSeekEntityStandard{ m_target, 50000, 1000, maxEntityDist2D, 2.f, 2.f, true, true }; // 0x859E30, 0x859E34 = 2.f
        };

        if (m_needToMoveInCloserTime == 0) {
            task = SeekTarget(6.f);
        } else {
            const float timeSince = (float)(uint32)(CTimer::GetTimeInMS() - m_needToMoveInCloserTime);
            if (timeSince < 3000.f) {
                task = SeekTarget(6.f);
            } else if (timeSince > 8000.f) {
                // Try going to the side of the target
                const auto& pedPos    = ped->GetPosition();
                const auto& targetPos = m_target->GetPosition();

                CVector dir{ targetPos.x - pedPos.x, targetPos.y - pedPos.y, 0.f };
                dir.Normalise();

                const float  sideX = -dir.y * 1.5f;                 // Stored as a float
                const double sideY = (double)dir.x * (double)1.5f; // x87: Kept in extended precision
                const CVector pos1{ sideX + targetPos.x, (float)(sideY + (double)targetPos.y), targetPos.z };
                const CVector pos2{ targetPos.x - sideX, (float)((double)targetPos.y - sideY), targetPos.z };

                if (CWorld::GetIsLineOfSightClear(targetPos, pos1, true, true, false, false, true, false, false)) {
                    task = new CTaskComplexGoToPointAndStandStill{ PEDMOVE_RUN, pos1, 0.5f, 2.f, false, false }; // 0x86FC84, 0x86FC88
                } else if (CWorld::GetIsLineOfSightClear(targetPos, pos2, true, true, false, false, true, false, false)) {
                    task = new CTaskComplexGoToPointAndStandStill{ PEDMOVE_RUN, pos2, 0.5f, 2.f, false, false };
                } else {
                    task = SeekTarget(1.f);
                }
            } else {
                task = SeekTarget((float)(6.0 - ((double)timeSince - 3000.0) * (double)0.001f));
            }
        }

        //> 0x6273DB - Make some noise
        if (m_target && m_target->IsPlayer()) {
            if (ped->m_nPedType == PED_TYPE_COP) {
                if (FindPlayerWanted(-1)->m_NumCopsInPursuit <= 1) {
                    if (ped->Say(CTX_GLOBAL_SOLO) >= 0) {
                        m_target->Say(CTX_GLOBAL_CHASED, 3500);
                    }
                } else {
                    if (ped->Say(CTX_GLOBAL_CHASE_FOOT) >= 0) {
                        m_target->Say(CTX_GLOBAL_CHASED, 3500);
                    }
                }
            } else if (ped->GetGroup()) {
                ped->Say(CTX_GLOBAL_CHASE_FOOT);
            }
        }
        return task;
    }
    case TASK_SIMPLE_GUN_CTRL: {
        CTask* task{};
        if (ourWep.flags.bThrow) {
            task = new CTaskSimpleThrowControl{ m_target, nullptr };
        } else {
            const auto cmd = LineOfSightClearForAttack(ped) ? eGunCommand::FIREBURST : eGunCommand::NONE;
            const auto gunCtrl = new CTaskSimpleGunControl{ m_target, CVector{}, CVector{}, cmd, 5, -1 };
            gunCtrl->m_aimImmidiately = m_aimImmediate;
            m_aimImmediate = false;
            m_shootTimer = CTimer::GetTimeInMS() + (uint32)CGeneral::GetRandomNumberInRange(4000, 8000);
            if (ped->GetGroup()) {
                ped->Say(CTX_GLOBAL_SURROUNDED);
            }
            task = gunCtrl;
        }
        m_lastAttackTime = CTimer::GetTimeInMS();
        m_losBlockedTime = 0;
        return task;
    }
    default:
        return nullptr;
    }
}

// 0x6212B0
bool CTaskComplexKillPedOnFootArmed::MakeAbortable(CPed* ped, eAbortPriority priority, CEvent const* event) {
    switch (priority) {
    case ABORT_PRIORITY_URGENT: {
        if (const auto aimedAtEvent = notsa::dyn_cast_if_present<const CEventGunAimedAt>(event)) {
            if (aimedAtEvent->m_AimedBy == m_target) {
                return false;
            }
        }
        break;
    }
    case ABORT_PRIORITY_IMMEDIATE:
        break;
    case ABORT_PRIORITY_LEISURE:
    default:
        return false;
    }
    return m_pSubTask->MakeAbortable(ped, priority, event);
}

// 0x62C190
CTask* CTaskComplexKillPedOnFootArmed::CreateNextSubTask(CPed* ped) {
    if (!m_target) {
        return nullptr;
    }

    const bool  notMelee = !ped->GetActiveWeapon().IsTypeMelee();
    const auto& ourWep   = ped->GetActiveWeapon().GetWeaponInfo(ped);
    const float range    = ourWep.m_fTargetRange;

    const auto& ourPos    = ped->GetPosition();
    const auto& targetPos = m_target->GetPosition();

    // x87: Sum of squares & sqrt is in extended precision, final result is a float
    const float dist = [&] {
        const double dx = (double)targetPos.x - (double)ourPos.x;
        const double dy = (double)targetPos.y - (double)ourPos.y;
        const double dz = (double)targetPos.z - (double)ourPos.z;
        return (float)std::sqrt(dx * dx + dy * dy + dz * dz);
    }();

    const auto SeekTarget = [&](float maxEntityDist2D) -> CTask* {
        return new CTaskComplexSeekEntityStandard{ m_target, 50000, 1000, maxEntityDist2D, 2.f, 2.f, true, true };
    };

    CTask* task{};

    // Main task selection (Based on the current sub-task's type)
    switch (m_pSubTask->GetTaskType()) {
    case TASK_SIMPLE_DUCK: {
        const auto newTask = CreateSubTask(LineOfSightClearForAttack(ped) ? TASK_SIMPLE_GUN_CTRL : TASK_SIMPLE_PAUSE, ped);
        ped->bIsDucking = false;
        m_needToMoveInCloserTime = 0;
        return newTask;
    }
    case TASK_SIMPLE_PAUSE:
    case TASK_SIMPLE_STAND_STILL: {
        if (ped->bStayInSamePlace || ped->bKindaStayInSamePlace) {
            const auto diff = targetPos - ourPos;
            // x87: kept in extended precision
            const double diffSq   = (double)diff.x * (double)diff.x + (double)diff.y * (double)diff.y + (double)diff.z * (double)diff.z;
            const auto   newTask  = CreateSubTask(
                ((double)range * (double)range <= diffSq || !LineOfSightClearForAttack(ped)) ? TASK_SIMPLE_PAUSE : TASK_SIMPLE_GUN_CTRL,
                ped
            );
            m_needToMoveInCloserTime = 0;
            return newTask;
        }
        ped->Say(CTX_GLOBAL_MOVE_IN);
        const auto newTask = CreateSubTask(TASK_COMPLEX_SEEK_ENTITY, ped);
        if (!m_needToMoveInCloserTime) {
            m_needToMoveInCloserTime = CTimer::GetTimeInMS();
        }
        return newTask;
    }
    case TASK_SIMPLE_GO_TO_POINT: {
        const auto newTask = CreateSubTask(LineOfSightClearForAttack(ped) ? TASK_SIMPLE_GUN_CTRL : TASK_SIMPLE_PAUSE, ped);
        m_needToMoveInCloserTime = 0;
        return newTask;
    }
    case TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL:
    case TASK_COMPLEX_SEEK_ENTITY: {
        if (LineOfSightClearForAttack(ped)) {
            const auto newTask = CreateSubTask(TASK_SIMPLE_GUN_CTRL, ped);
            m_needToMoveInCloserTime = 0;
            return newTask;
        }
        const auto newTask = CreateSubTask(TASK_SIMPLE_PAUSE, ped);
        if (!m_needToMoveInCloserTime) {
            m_needToMoveInCloserTime = CTimer::GetTimeInMS();
        }
        return newTask;
    }
    case TASK_FINISHED: {
        m_needToMoveInCloserTime = 0;
        return nullptr;
    }
    case TASK_COMPLEX_GO_TO_POINT_SHOOTING:
    case TASK_SIMPLE_GUN_CTRL:
    case TASK_SIMPLE_THROW_CTRL:
        break; // Handled below
    default: {
        m_needToMoveInCloserTime = 0;
        return nullptr;
    }
    }

    //> 0x62C411 - Shooting related tasks
    // Note: The labels below mirror the original's control flow (hence the `goto`s)
    if (ped->bStayInSamePlace) {
        goto PauseStep;
    }

    if (dist < 3.f) {
        task = CreateSubTask(TASK_SIMPLE_GUN_CTRL, ped);
        goto AfterTask;
    }

    {
        // x87: kept in extended precision
        double maxDist = (double)range * (double)0.8f;
        if (23.0 <= maxDist) {
            maxDist = 23.0;
        }

        if ((double)dist > maxDist) {
            if (ped->bKindaStayInSamePlace) {
                goto PauseStep;
            }
            double v = (double)range * (double)0.6f;
            task = SeekTarget(v >= 20.0 ? 20.f : (float)v);
            goto AfterTask;
        }
    }

    if (m_pSubTask->GetTaskType() == TASK_SIMPLE_GUN_CTRL
        && notsa::cast<CTaskSimpleGunControl>(m_pSubTask)->m_isLOSBlocked
        && !ped->bStayInSamePlace && !ped->bKindaStayInSamePlace
    ) {
        ped->Say(CTX_GLOBAL_MOVE_IN);
        task = SeekTarget(2.f);
        if (task) {
            goto CoverRand;
        }
        goto PostCover;
    }

    //> 0x62C5F8 - Try using the cover point we have
    if (notMelee && ped->m_pCoverPoint
        && CCover::DoesCoverPointStillProvideCover(ped->m_pCoverPoint, targetPos)
    ) {
        CVector coverPos;
        if (CCover::FindCoordinatesCoverPoint(*ped->m_pCoverPoint, ped, targetPos, coverPos)) {
            const CVector2D pedToCover{ ourPos.x - coverPos.x, ourPos.y - coverPos.y };
            // x87: sqrt is kept in extended precision
            if (std::sqrt((double)pedToCover.y * (double)pedToCover.y + (double)pedToCover.x * (double)pedToCover.x) < (double)0.75f) {
                if (ped->m_pCoverPoint->GetUsage() == CCoverPoint::eUsage::LOWCOVER) {
                    if (dist < 12.f
                        && !ped->bNotAllowedToDuck
                        && (rand() & 3) == 0
                        && !ped->GetTaskManager().GetTaskSecondary(TASK_SECONDARY_DUCK)
                    ) {
                        ped->Say(CTX_GLOBAL_DUCK);
                        task = new CTaskSimpleDuck{ DUCK_STANDALONE, 2500, -1 };
                        goto AfterCoverTask;
                    }
                } else {
                    if ((rand() & 1) == 0 && !ourWep.flags.bThrow) {
                        const auto gunCtrl = new CTaskSimpleGunControl{ m_target, CVector{}, CVector{}, eGunCommand::FIREBURST, 5, -1 };
                        gunCtrl->m_aimImmidiately = m_aimImmediate;
                        m_aimImmediate            = false;
                        m_shootTimer              = CTimer::GetTimeInMS() + 5000;
                        task                      = gunCtrl;
                        m_lastAttackTime          = CTimer::GetTimeInMS();
                        m_lastStrafeTime          = CTimer::GetTimeInMS() + 2500;
                        m_strafeDir               = eStrafeDir::RIGHT;
                        m_bStrafeBack             = true;
                        if (ped->m_pCoverPoint->GetUsage() == CCoverPoint::eUsage::WALLTOLEFT) {
                            m_strafeDir = eStrafeDir::LEFT;
                        }
                        ped->GetIntelligence()->ClearTaskDuckSecondary();
                        goto AfterCoverTask;
                    }
                }
            }
            ped->ReleaseCoverPoint();
            goto PostCover;
        }
    }
    goto PostCover;

AfterCoverTask:
    if (task) {
        goto CoverRand;
    }
    ped->ReleaseCoverPoint();

PostCover:
    //> 0x62C849 - Try finding a (new) cover point
    if (m_competence > 0 && dist > 6.f && notMelee && (rand() & 1)) {
        ped->ReleaseCoverPoint();
        ped->m_pCoverPoint = CCover::FindAndReserveCoverPoint(ped, targetPos, m_competence == 2);
        if (ped->m_pCoverPoint) {
            CVector coverPos;
            CCover::FindCoordinatesCoverPoint(*ped->m_pCoverPoint, ped, targetPos, coverPos);
            coverPos.z += 1.f;

            const CVector2D coverToTarget{ coverPos.x - targetPos.x, coverPos.y - targetPos.y };
            // x87: sqrt is kept in extended precision
            if (std::sqrt((double)coverToTarget.x * (double)coverToTarget.x + (double)coverToTarget.y * (double)coverToTarget.y) < (double)range * (double)0.75f) {
                if (CWorld::GetIsLineOfSightClear(coverPos, ourPos, true, true, false, false, false, false, false)) {
                    if (ped->GetGroup()) {
                        ped->Say(CTX_GLOBAL_COVER_ME);
                    }
                    ped->GetIntelligence()->SetTaskDuckSecondary(6000);
                    task = new CTaskSimpleGoToPoint{ PEDMOVE_RUN, coverPos, 0.5f, true, false };
                    if (task) {
                        goto CoverRand;
                    }
                    goto AfterPostCover;
                }
            }
            ped->ReleaseCoverPoint();
        }
    }

AfterPostCover:
    //> 0x62C9E5
    if (dist > 10.f) {
        switch (rand() & 3) {
        case 0: {
            if (!ped->bKindaStayInSamePlace) {
                ped->Say(CTX_GLOBAL_MOVE_IN);
                task = SeekTarget(dist - 4.f);
            }
            break;
        }
        case 1: {
            if (!ped->bKindaStayInSamePlace) {
                m_strafeDir      = eStrafeDir::FORWARD;
                m_lastStrafeTime = CTimer::GetTimeInMS() + 2000;
                m_bStrafeBack    = false;
            }
            break;
        }
        }
    }

    if (dist > 5.f && (rand() & 3) == 0) {
        m_strafeDir = eStrafeDir::LEFT;
        if (rand() & 1) {
            m_strafeDir = eStrafeDir::RIGHT;
        }

        const CVector pos       = ourPos;
        const CVector sideOffs  = ped->GetMatrix().GetRight() * 2.5f;
        if (m_strafeDir == eStrafeDir::RIGHT) {
            const CVector end = pos + sideOffs;
            if (!CWorld::GetIsLineOfSightClear(pos, end, true, true, false, true, false, false, false)) {
                m_strafeDir = eStrafeDir::LEFT;
            }
        } else {
            const CVector end = pos - sideOffs;
            if (!CWorld::GetIsLineOfSightClear(pos, end, true, true, false, true, false, false, false)) {
                m_strafeDir = eStrafeDir::RIGHT;
            }
        }
        m_lastStrafeTime = CTimer::GetTimeInMS() + 2000;
        m_bStrafeBack    = false;
    }

    if (task) {
        goto CoverRand;
    }

    if (LineOfSightClearForAttack(ped)) {
        task = CreateSubTask(TASK_SIMPLE_GUN_CTRL, ped);
        goto AfterTask;
    }
    if ((rand() & 3) == 0) {
        m_strafeDir      = (rand() < 0x3FFF) ? eStrafeDir::RIGHT : eStrafeDir::LEFT;
        m_lastStrafeTime = CTimer::GetTimeInMS() + 2000;
        m_bStrafeBack    = false;
    }
    goto PauseStep;

AfterTask:
    if (task) {
        goto CoverRand;
    }

PauseStep:
    task             = CreateSubTask(TASK_SIMPLE_PAUSE, ped);
    m_lastAttackTime = CTimer::GetTimeInMS();

CoverRand:
    //> 0x62CC6C - Maybe duck
    if ((rand() & 1)
        && m_competence > 0
        && !ped->bNotAllowedToDuck
        && (!ped->m_pCoverPoint || (ped->m_pCoverPoint->GetUsage() != CCoverPoint::eUsage::WALLTOLEFT && ped->m_pCoverPoint->GetUsage() != CCoverPoint::eUsage::WALLTORIGHT))
    ) {
        ped->GetIntelligence()->SetTaskDuckSecondary(6000);
    }
    m_needToMoveInCloserTime = 0;
    return task;
}

// 0x62BF00
CTask* CTaskComplexKillPedOnFootArmed::CreateFirstSubTask(CPed* ped) {
    if (!m_target) {
        return nullptr;
    }

    const auto &ourPos    = ped->GetPosition(),
               &targetPos = m_target->GetPosition();

    const auto targetToOurPedDistSq = (ourPos - targetPos).SquaredMagnitude();

    if (   !ped->bStayInSamePlace
        && CGeneral::RandomBool(50)
        && m_competence > 0
        && (targetToOurPedDistSq >= sq(30.f) || targetToOurPedDistSq >= sq(6.f) && !m_target->GetActiveWeapon().IsTypeMelee()) 
    ) {
        ped->ReleaseCoverPoint();
        if (ped->m_pCoverPoint = CCover::FindAndReserveCoverPoint(ped, targetPos, false)) {
            CVector coverPos{};
            if (!notsa::IsFixBugs() || CCover::FindCoordinatesCoverPoint(*ped->m_pCoverPoint, ped, targetPos, coverPos)) {
                if (CWorld::GetIsLineOfSightClear(coverPos, ourPos, true, true, false, false, false, false, false)) {
                    ped->GetIntelligence()->SetTaskDuckSecondary(6000);
                    if (const auto task = new CTaskSimpleGoToPoint{
                        PEDMOVE_RUN,
                        coverPos,
                        0.5f,
                        true
                    }) { // I can't believe my eyes.... error handling?
                        return task;
                    }
                } else {
                    ped->ReleaseCoverPoint();
                }
            } else {
                NOTSA_LOG_WARN("Can't find cover point's coordinates!"); // Originally the game has left `coverPos` uninitialized in this case
            }
        }
    }

    return CreateSubTask([&, this] {
        const auto& ourActiveWep = ped->GetActiveWeapon().GetWeaponInfo(ped);

        if (targetToOurPedDistSq <= sq(ourActiveWep.m_fTargetRange / 4.f) && LineOfSightClearForAttack(ped)) {
            return TASK_SIMPLE_GUN_CTRL;
        }

        if (!ped->bStayInSamePlace && !ped->bKindaStayInSamePlace) {
            return TASK_COMPLEX_SEEK_ENTITY;
        }

        if (LineOfSightClearForAttack(ped) || ourActiveWep.flags.bThrow) {
            return TASK_SIMPLE_PAUSE;
        }

        return TASK_SIMPLE_GUN_CTRL;
    }(), ped);
}

// 0x62CCE0
CTask* CTaskComplexKillPedOnFootArmed::ControlSubTask(CPed* ped) {
    if (m_newTarget) {
        return CreateFirstSubTask(ped);
    }

    if (!m_target || m_target->m_fHealth <= 0.f) {
        return nullptr;
    }
    const auto ogSubTask = m_pSubTask;

    const auto &ourPos    = ped->GetPosition(),
               &targetPos = m_target->GetPosition();

    const auto targetToOurPedDistSq = (ourPos - targetPos).SquaredMagnitude();

    //> 0x62CD55
    if (   (m_duckingConditions & 4) == 0
        || CTimer::GetTimeInMS() <= m_lastDuckTime + m_lengthOfDuck + 2000
        || ped->bIsDucking
    ) {
        if (   m_duckingConditions & 1
            && m_bShotFiredByPlayer
            && CGeneral::RandomBool((float)m_duckChancePerc)
        ) {
            if (const auto quack = ped->GetIntelligence()->GetTaskDuck()) {
                quack->SetDuckTimer((uint16)m_lengthOfDuck);
            } else {
                ped->GetIntelligence()->SetTaskDuckSecondary((uint16)m_lengthOfDuck);
#ifdef FIX_BUGS
                m_lastDuckTime = CTimer::GetTimeInMS(); // Not actually sure if this is necessary tbh
#endif
            }
        }
    } else {
        if (CGeneral::RandomBool((float)m_duckChancePerc)) {
            if (targetToOurPedDistSq <= sq(20.f)) {
                ped->GetIntelligence()->SetTaskDuckSecondary((uint16)m_lengthOfDuck);
            }
        }
        m_lastDuckTime = CTimer::GetTimeInMS();
    }

    //> 0x62CE56
    const auto quack = ped->GetIntelligence()->GetTaskDuck();
    if (quack) {
        quack->m_bIsInControl = true;
    }

    //> 0x62CE6E
    if (m_pSubTask != ogSubTask) { // ?????
        return ogSubTask;
    }

    const auto& ourwi = ped->GetActiveWeapon().GetWeaponInfo(ped);
    switch (m_pSubTask->GetTaskType()) { //> 0x62CE7E
    case TASK_COMPLEX_SEEK_ENTITY: {
        if ([&, this]{
            if (IsPedInLeaderFiringLine(ped)) {
                return true;
            }
            if (m_target->physicalFlags.bSubmergedInWater && LineOfSightClearForAttack(ped)) {
                return true;
            }
            if (ped->bStayInSamePlace) {
                return LineOfSightClearForAttack(ped);
            } else if (sq(ourwi.m_fTargetRange / 2.f) >= targetToOurPedDistSq) {
                if (CTimer::GetTimeInMS() - m_lastAttackTime >= 2000 || m_target->GetMoveSpeed().Dot(ped->GetForward()) < 0.f) {
                    return LineOfSightClearForAttack(ped);
                }
            }
            return false;
        }()) {
            return CreateSubTask(TASK_SIMPLE_GUN_CTRL, ped);
        } else {
            if (ped->GetGroup()) {
                ped->Say(CTX_GLOBAL_COVER_ME);
            }
        }
        break;
    }
    case TASK_SIMPLE_GUN_CTRL: { //> 0x62CE87
        if (   targetToOurPedDistSq >= sq(ourwi.m_fTargetRange)
            || targetToOurPedDistSq >= sq(ourwi.m_fTargetRange / 2.f) && !ped->bStayInSamePlace && CTimer::GetTimeInMS() - m_lastAttackTime >= 2000
            || targetToOurPedDistSq >= sq(4.f) && !ped->bStayInSamePlace && CTimer::GetTimeInMS() >= m_shootTimer
            || !LineOfSightClearForAttack(ped)
        ) {
            const auto gctrl = notsa::cast<CTaskSimpleGunControl>(m_pSubTask);
            if (gctrl->m_firingTask != eGunCommand::END_LEISURE) {
                gctrl->m_nextAtkTimeMs = 0;
                gctrl->m_firingTask    = eGunCommand::END_LEISURE;
            }
        }

        const auto ugun = ped->GetIntelligence()->GetTaskUseGun();
        if (!ugun) {
            break;
        }

        if (ped->GetGroup()) {
            ped->Say(CTX_GLOBAL_SURROUNDED);
        }

        //> 0x62CF88
        const auto actionDir = [&, this]() -> CVector2D {
            if (m_target->physicalFlags.bSubmergedInWater) {
                return { 0.f, 0.f };
            }
            if (CTimer::GetTimeInMS() >= m_lastStrafeTime || !ped->bStayInSamePlace) {
                if (targetToOurPedDistSq <= sq(4.f) && ped->bStayInSamePlace) {
                    return { 0.f, 1.f }; // Forwards
                }
                if (m_bStrafeBack) {
                    m_bStrafeBack    = false;
                    m_lastStrafeTime = CTimer::GetTimeInMS() + 2500;
                    m_strafeDir      = [&, this] { // Strafe back to where we came from the last time
                        switch (m_strafeDir)
                        {
                        case eStrafeDir::LEFT:    return eStrafeDir::RIGHT;
                        case eStrafeDir::RIGHT:   return eStrafeDir::LEFT;
                        case eStrafeDir::FORWARD: return eStrafeDir::BACK;
                        case eStrafeDir::BACK:    return eStrafeDir::FORWARD;
                        default:                  NOTSA_UNREACHABLE();
                        }
                    }();
                }
                return { 0.f, 0.f };
            } else {
                switch (m_strafeDir)
                {
                case eStrafeDir::LEFT:    return { -1.f,  0.f };
                case eStrafeDir::RIGHT:   return {  1.f,  0.f };
                case eStrafeDir::FORWARD: return {  0.f, -1.f };
                case eStrafeDir::BACK:    return {  0.f,  1.f };
                default:                  NOTSA_UNREACHABLE();
                }
            }
        }();

        //> 0x62D080
        if (quack) {
            quack->ControlDuckMove([&, this] {
                if (notsa::contains(std::to_array({ eStrafeDir::LEFT, eStrafeDir::RIGHT }), m_strafeDir)) {
                    if (m_lastRollTime + 3000 >= CTimer::GetTimeInMS()) {
                        return CVector2D{ 0.f, 0.f };
                    } else {
                        m_lastRollTime = CTimer::GetTimeInMS();
                    }
                }
                return actionDir;
            }());
        } else {
            ugun->ControlGunMove(actionDir);
        }

        break;
    }
    }

    return m_pSubTask;
}

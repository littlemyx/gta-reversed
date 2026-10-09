#include "StdInc.h"

#include "TaskComplexWanderStandard.h"

#include "EventChatPartner.h"
#include "EventSexyVehicle.h"
#include "EventAcquaintancePedDislike.h"
#include "PedGroups.h"
#include "SurfaceInfos_c.h"
#include "GameLogic.h"
#include "Streaming.h"

void CTaskComplexWanderStandard::InjectHooks() {
    RH_ScopedVirtualClass(CTaskComplexWanderStandard, 0x85A200, 15);
    RH_ScopedCategory("Tasks/TaskTypes");

    RH_ScopedInstall(WillChat, 0x66AED0);
    RH_ScopedInstall(SetNextMinScanTime, 0x66AF60);
    RH_ScopedInstall(LookForSexyCars, 0x66AFD0);
    RH_ScopedInstall(LookForChatPartners, 0x66FDA0);
    RH_ScopedInstall(LookForGangMembers, 0x670100);
}

// 0x48E4F0
CTaskComplexWanderStandard::CTaskComplexWanderStandard(eMoveState MoveState, uint8 Dir, bool bWanderSensibly) :
    CTaskComplexWander(MoveState, Dir, bWanderSensibly),
    m_nMinNextScanTime{ 0 }
{
}

// 0x672600
void CTaskComplexWanderStandard::ScanForStuff(CPed* ped) {
    if (!m_TaskTimer.m_bStarted) {
        m_TaskTimer.m_nStartTime = CTimer::GetTimeInMS();
        m_TaskTimer.m_nInterval = 50;
        m_TaskTimer.m_bStarted = true;
    }

    if (CTimer::GetTimeInMS() < m_nMinNextScanTime)
        return;

    if (m_TaskTimer.m_bStarted) { // V547 Expression 'm_TaskTimer.m_bStarted' is always true.
        if (m_TaskTimer.m_bStopped) {
            m_TaskTimer.m_nStartTime = CTimer::GetTimeInMS();
            m_TaskTimer.m_bStopped = false;
        }

        if (CTimer::GetTimeInMS() >= m_TaskTimer.m_nStartTime + m_TaskTimer.m_nInterval) {
            m_TaskTimer.m_nInterval = 50;
            m_TaskTimer.m_nStartTime = CTimer::GetTimeInMS();
            m_TaskTimer.m_bStarted = true;
            if (!LookForGangMembers(ped) && !LookForChatPartners(ped)) {
                CTaskComplexWanderStandard::LookForSexyCars(ped);
            }
        }
    }
}

// 0x66AED0
bool CTaskComplexWanderStandard::WillChat(const CPed& first, const CPed& second) {
    if (first.m_nPedType == PED_TYPE_CRIMINAL || second.m_nPedType == PED_TYPE_CRIMINAL) {
        return false;
    }
    if (first.m_nPedType == PED_TYPE_COP || second.m_nPedType == PED_TYPE_COP) {
        return false;
    }
    if (first.IsPlayer() || second.IsPlayer()) {
        return false;
    }
    if (IsPedTypeGang(first.m_nPedType) || IsPedTypeGang(second.m_nPedType)) { // 0x5FE9C0
        return false;
    }
    return CPedIntelligence::AreFriends(first, second);
}

// 0x66AF60
void CTaskComplexWanderStandard::SetNextMinScanTime(CPed* ped) {
    auto& taskMgr = ped->GetIntelligence()->GetTaskManager();

    const auto active = taskMgr.GetActiveTask();
    if (!active) {
        return;
    }
    if (active->GetTaskType() != GetTaskType()) {
        return;
    }
    if (static_cast<CTaskComplexWander*>(active)->GetWanderType() != GetWanderType()) {
        return;
    }

    static_cast<CTaskComplexWanderStandard*>(taskMgr.GetActiveTask())->m_nMinNextScanTime = CTimer::GetTimeInMS() + 100'000;
}

// 0x66AFD0
bool CTaskComplexWanderStandard::LookForSexyCars(CPed* ped) {
    const auto& pedPos = ped->GetPosition();

    for (auto* const entity : ped->GetIntelligence()->m_vehicleScanner.m_apEntities) {
        auto* const veh = static_cast<CVehicle*>(entity);
        if (!veh || veh == ped->m_pVehicle) {
            continue;
        }
        if (veh->m_pHandlingData->m_nMonetaryValue <= 40'000u) {
            continue;
        }
        if (!(veh->m_fHealth > 500.0f)) { // 0x858B58
            continue;
        }

        // Original keeps these on the x87 stack (extended precision)
        const auto&  vehPos = veh->GetPosition();
        const double dx     = (double)vehPos.x - (double)pedPos.x;
        const double dy     = (double)vehPos.y - (double)pedPos.y;
        const double dz     = (double)vehPos.z - (double)pedPos.z;

        constexpr float RANGE = 5.0f; // 0x86FCD8
        if (!((dz * dz + dy * dy) + dx * dx < (double)RANGE * (double)RANGE)) {
            continue;
        }
        const auto& pedFwd = ped->GetForward(); // NOTE: the original reads the matrix only here
        if (!((dz * pedFwd.z + dy * pedFwd.y) + dx * pedFwd.x > 0.0)) { // 0x858B50
            continue;
        }
        if (!CWorld::GetIsLineOfSightClear(pedPos, vehPos, true, false, false, true, false, false, false)) {
            continue;
        }

        CEventSexyVehicle event{veh};
        ped->GetIntelligence()->m_eventGroup.Add(&event, false);
        SetNextMinScanTime(ped);
        return true;
    }
    return false;
}

// 0x66FDA0
bool CTaskComplexWanderStandard::LookForChatPartners(CPed* ped) {
    if (!g_surfaceInfos.IsPavement(ped->m_nContactSurface)) {
        return false;
    }
    if ((int32)m_nMoveState > PEDMOVE_WALK) {
        return false;
    }
    {
        const auto* const playerPed = CWorld::Players[CWorld::PlayerInFocus].m_pPed;
        const auto* const playerVeh = playerPed->m_pVehicle;
        if (playerPed->bInVehicle && playerVeh) {
            const auto& ms = playerVeh->m_vecMoveSpeed;
            // x87: (x^2 + y^2) + z^2 in extended precision; fails only if strictly greater
            const double speed2 = ((double)ms.x * ms.x + (double)ms.y * ms.y) + (double)ms.z * ms.z;
            if (speed2 > (double)0.04f) { // 0x863244
                return false;
            }
        }
    }
    if (CStreaming::IsVeryBusy()) {
        return false;
    }
    if (CGameLogic::LaRiotsActiveHere()) {
        return false;
    }

    const auto& pedPos = ped->GetPosition();
    for (auto* const partner : ped->GetIntelligence()->m_pedScanner.m_apEntities) {
        auto* const other = static_cast<CPed*>(partner);
        if (!other) {
            continue;
        }
        if (!g_surfaceInfos.IsPavement(other->m_nContactSurface)) {
            continue;
        }
        const auto otherActive = other->GetIntelligence()->GetTaskManager().GetActiveTask();
        if (!otherActive) {
            continue;
        }
        if (otherActive->GetTaskType() != GetTaskType()) {
            continue;
        }
        if (ped->GetIntelligence()->FindTaskByType(TASK_COMPLEX_PARTNER_CHAT)
            || other->GetIntelligence()->FindTaskByType(TASK_COMPLEX_PARTNER_CHAT)) {
            continue;
        }
        if (ped->GetIntelligence()->m_eventGroup.GetEventOfType(EVENT_CHAT_PARTNER)
            || other->GetIntelligence()->m_eventGroup.GetEventOfType(EVENT_CHAT_PARTNER)) {
            continue;
        }
        if (ped->GetIntelligence()->FindTaskByType(TASK_COMPLEX_BE_IN_COUPLE)
            || other->GetIntelligence()->FindTaskByType(TASK_COMPLEX_BE_IN_COUPLE)) {
            continue;
        }
        if (!WillChat(*ped, *other)) {
            continue;
        }

        const auto& otherPos = other->GetPosition();
        const CVector diff   = otherPos - pedPos; // 0x40FE00
        // 0x406DA0 (extended precision: (x^2 + y^2) + z^2)
        const double distSq = ((double)diff.x * diff.x + (double)diff.y * diff.y) + (double)diff.z * diff.z;
        constexpr float RANGE = 10.0f; // 0x86FCD4
        if (!(distSq < (double)RANGE * (double)RANGE)) {
            continue;
        }

        // Ped must be facing the partner, and the partner must be facing the ped
        const auto& pedFwd   = ped->GetForward();
        const auto& otherFwd = other->GetForward();
        if (!((double)diff.y * pedFwd.y + (double)diff.z * pedFwd.z + (double)diff.x * pedFwd.x > 0.0)) { // 0x858B50
            continue;
        }
        if (!((double)diff.y * otherFwd.y + (double)diff.z * otherFwd.z + (double)diff.x * otherFwd.x < 0.0)) {
            continue;
        }
        if (!CWorld::GetIsLineOfSightClear(pedPos, otherPos, true, false, false, true, false, false, false)) {
            continue;
        }

        CEventChatPartner evForPed{true, other};
        ped->GetIntelligence()->m_eventGroup.Add(&evForPed, false);
        CEventChatPartner evForOther{false, ped};
        other->GetIntelligence()->m_eventGroup.Add(&evForOther, false);
        SetNextMinScanTime(ped);
        SetNextMinScanTime(other);
        return true;
    }
    return false;
}

// 0x670100
bool CTaskComplexWanderStandard::LookForGangMembers(CPed* ped) {
    if (CPedGroups::GetPedsGroup(ped)) {
        return false;
    }

    CVector targetPos;
    ComputeTargetPos(ped, targetPos, m_NextNode); // 0x669F60

    const auto& pedPos = ped->GetPosition();
    for (auto* const entity : ped->GetIntelligence()->m_pedScanner.m_apEntities) {
        auto* const other = static_cast<CPed*>(entity);
        if (!other) {
            continue;
        }

        // Original: the ped's delta stays in extended precision, the other ped's delta is stored to floats,
        // except for z which is used unrounded in the dot product, but rounded in the length
        const double sx = (double)pedPos.x - (double)targetPos.x;
        const double sy = (double)pedPos.y - (double)targetPos.y;
        const double sz = (double)pedPos.z - (double)targetPos.z;

        const auto&  otherPos = other->GetPosition();
        const float  ox       = (float)((double)otherPos.x - (double)targetPos.x);
        const float  oy       = (float)((double)otherPos.y - (double)targetPos.y);
        const double ozExt    = (double)otherPos.z - (double)targetPos.z;
        const float  oz       = (float)ozExt;

        if (!(ozExt * sz + (double)oy * sy + (double)ox * sx > 0.0)) { // 0x858B50
            continue;
        }
        if (!(((double)oz * oz + (double)oy * oy) + (double)ox * ox < (double)25.0f)) { // 0x858FE8
            continue;
        }

        auto* const otherGrp = CPedGroups::GetPedsGroup(other);
        if (!otherGrp || otherGrp->GetMembership().CountMembersExcludingLeader() <= 0) {
            continue;
        }

        CEventAcquaintancePedDislike event{other, (eTaskType)940}; // vtable 0x86CAC8
        ped->GetIntelligence()->m_eventGroup.Add(&event, false);
        SetNextMinScanTime(ped);
        return true;
    }
    return false;
}

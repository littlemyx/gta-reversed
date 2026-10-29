#include "StdInc.h"

#include "PedGroup.h"
#include <TaskSimpleCarSetPedOut.h>
#include <TaskComplexFollowLeaderInFormation.h>
#include "Formation.h"
#include "PedList.h"
#include "TaskSimpleGoToPoint.h"
#include "TaskComplexEnterCar.h"

//! @addr 0x5FB010
//! @returns Distance of the furthers member from the leader
float CPedGroup::FindDistanceToFurthestMember() {
    float furthest = 0.f;
    const auto leader = GetMembership().GetLeader();
    if (!leader) {
        return furthest;
    }
    const auto& leaderPos = leader->GetPosition();
    for (const auto mem : GetMembership().GetFollowers()) {
        const auto d = mem->GetPosition() - leaderPos;
        const auto dist = std::sqrt(d.z * d.z + d.x * d.x + d.y * d.y); // Order of operations as in the original
        if (furthest <= dist) {
            furthest = dist;
        }
    }
    return furthest;
}

// 0x5FB0A0
float CPedGroup::FindDistanceToNearestMember(CPed** ppOutNearestMember) {
    float nearest = 1.0e10f; // 0x86C84C
    const auto leader = GetMembership().GetLeader();
    if (!leader) {
        return nearest;
    }
    const auto& leaderPos = leader->GetPosition();
    for (const auto mem : GetMembership().GetFollowers()) {
        const auto d = mem->GetPosition() - leaderPos;
        const auto dist = std::sqrt(d.z * d.z + d.x * d.x + d.y * d.y); // Order of operations as in the original
        if (nearest >= dist) { // fcom: a NaN distance never replaces the minimum
            nearest = dist;
        }
        // NOTE: the original stores the member into the out parameter for EVERY follower (the store sits after the compare, not inside the branch): the caller receives the last follower, not the nearest one
        if (ppOutNearestMember) {
            *ppOutNearestMember = mem;
        }
    }
    return nearest;
}

// 0x5FACD0
CPed* CPedGroup::GetClosestGroupPed(CPed* ped, float* pOutDistSq) {
    const auto [closest, distSq] = GetMembership().GetMemberClosestTo(ped);
    if (closest) {
        if (pOutDistSq) {
            *pOutDistSq = distSq;
        }
    }
    return closest;
}

// 0x5FB790
void CPedGroup::Flush() {
    m_groupMembership.Flush();
    m_groupIntelligence.Flush();
    m_bIsMissionGroup = false;
}

// 0x5F7DB0
bool CPedGroup::IsAnyoneUsingCar(const CVehicle* vehicle) {
    assert(vehicle);

    for (auto* const mem : m_groupMembership.GetMembers()) {
        if (mem->GetVehicleIfInOne() == vehicle) {
            return true;
        }
        // 0x5F7DE5..0x5F7E0F: both the EnterAsDriver and the EnterAsPassenger task are checked, each against the vehicle
        // (GetEnteringVehicle() would return the target of the first task found only)
        for (const auto taskType : { TASK_COMPLEX_ENTER_CAR_AS_DRIVER, TASK_COMPLEX_ENTER_CAR_AS_PASSENGER }) {
            if (const auto task = mem->GetIntelligence()->FindTaskByType(taskType)) {
                if (static_cast<CTaskComplexEnterCar*>(task)->GetTargetCar() == vehicle) {
                    return true;
                }
            }
        }
    }
    return false;
}

// 0x5F7CC0
void CPedGroup::PlayerGaveCommand_Attack(CPed* playerPed, CPed* target) {
    if (!m_groupIntelligence.AddEvent(CEventGroupEvent{ playerPed, new CEventPlayerCommandToGroup{PLAYER_GROUP_COMMAND_ATTACK, target} })) {
        return;
    }
    if (target && target->m_nPedType != PED_TYPE_GANG2) {
        playerPed->Say(target->IsGangster() ? CTX_GLOBAL_ORDER_ATTACK_MANY : CTX_GLOBAL_ORDER_ATTACK_SINGLE); // 0x5F7D82: `mov ecx, esi` = the commanding ped, not the target
    }
}

// 0x5FAB60
void CPedGroup::PlayerGaveCommand_Gather(CPed* ped) {
    CPedList pedList;
    pedList.Empty();
    pedList.BuildListFromGroup_NotInCar_NoLeader(&GetMembership());

    if (ped->GetIntelligence()->IsInACarOrEnteringOne() && ped->m_pVehicle) {
        CFormation::GenerateGatherDestinations_AroundCar(pedList, ped->m_pVehicle);
    } else {
        CFormation::GenerateGatherDestinations(pedList, ped);
    }
    CFormation::DistributeDestinations(pedList);

    for (int32 i = 0; i < (int32)pedList.m_count; i++) {
        const auto groupPed = pedList.m_peds[i];
        CVector    dest;
        if (CFormation::ReturnDestinationForPed(groupPed, &dest)) {
            const CTaskSimpleGoToPoint task{ PEDMOVE_SPRINT, dest, 2.f, true, false };
            auto& intel = GetIntelligence();
            intel.SetTask(groupPed, task, intel.GetPedTaskPairs(), TASK_SECONDARY_INVALID, false);
        }
    }
}

// 0x5FC7E0
void CPedGroup::Process() {
    m_groupMembership.Process();
    m_groupIntelligence.Process();
}

// 0x5FB7D0
void CPedGroup::RemoveAllFollowers() {
    GetMembership().RemoveAllFollowers(false);
}

// 0x5F7AD0
void CPedGroup::Teleport(const CVector& pos) {
    const auto leader = GetMembership().GetLeader();

    if (leader && leader->IsAlive()) {
        leader->Teleport(pos, false);
    }

    // 0x5F7B22: no current event => carry on; the current group event wrapping a leader-entry-exit event => nothing more to do
    // (the port returned for NO event and tested the wrapper's type, which is always EVENT_GROUP_EVENT)
    if (const auto ge = m_groupIntelligence.GetCurrentEvent(); ge && ge->GetEvent().GetEventType() == EVENT_LEADER_ENTRY_EXIT) {
        return;
    }

    // Set *followers* out of the vehicle
    if (leader && !leader->bInVehicle) {
        for (auto* const f : GetMembership().GetFollowers()) {
            if (!f->IsAlive() || !f->bInVehicle || f->IsCreatedByMission()) {
                continue;
            }
            CTaskSimpleCarSetPedOut{
                f->m_pVehicle,
                (eTargetDoor)CCarEnterExit::ComputeTargetDoorToExit(f->m_pVehicle, f),
                false
            }.ProcessPed(f);
        }
    }

    // Teleport *followers*
    const auto& offsets = CTaskComplexFollowLeaderInFormation::ms_offsets.Offsets;
    // exe 0x5F7BE0..0x5F7C9B: the offset index is the member SLOT (0..6, null slots skipped but counted), not the rank among the present followers
    for (int32 offsetIdx = 0; offsetIdx < TOTAL_PED_GROUP_FOLLOWERS; offsetIdx++) {
        CPed* const f = GetMembership().GetMember(offsetIdx);
        if (!f || !f->IsAlive()) {
            continue;
        }
        f->Teleport(pos + CVector{ offsets[offsetIdx] }, false);
        f->PositionAnyPedOutOfCollision();
        f->GetTaskManager().AbortFirstPrimaryTaskIn({ TASK_PRIMARY_PHYSICAL_RESPONSE, TASK_PRIMARY_EVENT_RESPONSE_TEMP, TASK_PRIMARY_EVENT_RESPONSE_NONTEMP }, f);
    }
}

int32 CPedGroup::GetId() const {
    return CPedGroups::GetGroupId(this);
}

bool CPedGroup::IsActive() const {
    return CPedGroups::ms_activeGroups[GetId()];
}

void CPedGroup::InjectHooks() {
    RH_ScopedClass(CPedGroup);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Constructor, 0x5FC150);
    RH_ScopedInstall(Destructor, 0x5FC190);

    RH_ScopedInstall(Teleport, 0x5F7AD0);
    RH_ScopedInstall(PlayerGaveCommand_Gather, 0x5FAB60);
    RH_ScopedInstall(PlayerGaveCommand_Attack, 0x5F7CC0);
    RH_ScopedInstall(IsAnyoneUsingCar, 0x5F7DB0);
    RH_ScopedInstall(GetClosestGroupPed, 0x5FACD0);
    RH_ScopedInstall(FindDistanceToFurthestMember, 0x5FB010);
    RH_ScopedInstall(FindDistanceToNearestMember, 0x5FB0A0);
    RH_ScopedInstall(Flush, 0x5FB790);
    RH_ScopedInstall(Process, 0x5FC7E0);
    RH_ScopedInstall(RemoveAllFollowers, 0x5FB7D0);
}

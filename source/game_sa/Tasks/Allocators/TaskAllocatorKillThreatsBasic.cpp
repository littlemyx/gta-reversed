#include "StdInc.h"

#include "TaskAllocatorKillThreatsBasic.h"
#include <InterestingEvents.h>
#include <TaskComplexKillPedGroupOnFoot.h>
#include <TaskComplexSequence.h>
#include <TaskSimpleLookAbout.h>

// 0x69C710
CTaskAllocatorKillThreatsBasic::CTaskAllocatorKillThreatsBasic(CPed* threat) :
    m_Threat{threat}
{
}

// 0x69D170
void CTaskAllocatorKillThreatsBasic::AllocateTasks(CPedGroupIntelligence* intel) {
    intel->FlushTasks(intel->GetPedTaskPairs(), nullptr);
    intel->FlushTasks(intel->GetSecondaryPedTaskPairs(), nullptr);

    if (!m_Threat) {
        return;
    }

    auto* const group = &intel->GetPedGroup();
    if (auto* const threatsGroup = m_Threat->GetGroup()) {
        if (threatsGroup == group) {
            NOTSA_LOG_DEBUG("ComputeKillThreatsBasicResponse() - threat ped already in group"); // vanilla
        } else {
            CPed* closest[TOTAL_PED_GROUP_MEMBERS]{};
            ComputeClosestPeds(*group, *threatsGroup, closest);
            for (int32 i = 0; i < TOTAL_PED_GROUP_MEMBERS; i++) {
                auto* mem = group->GetMembership().GetMember(i);
                if (!mem || mem->IsPlayer()) {
                    continue;
                }
                intel->SetEventResponseTask(
                    mem,
                    CTaskComplexKillPedGroupOnFoot{ CPedGroups::GetGroupId(threatsGroup), closest[i] }
                );
            }
            g_InterestingEvents.Add(CInterestingEvents::GANG_FIGHT, group->GetMembership().GetLeader()); // 0x69D436
        }
    } else { // 0x69D2EB
        for (auto* const mem : group->GetMembership().GetMembers()) {
            if (mem->IsPlayer()) {
                continue;
            }
            intel->SetEventResponseTask(
                mem,
                CTaskComplexSequence{
                    new CTaskComplexKillPedOnFoot{ m_Threat },
                    new CTaskSimpleLookAbout{ CGeneral::GetRandomNumberInRange(1'000u, 2'000u) },
                }
            );
        }
        g_InterestingEvents.Add(CInterestingEvents::GANG_ATTACKING_PED, group->GetMembership().GetLeader()); // 0x69D436
    }
}

// 0x69C7E0
CTaskAllocator* CTaskAllocatorKillThreatsBasic::ProcessGroup(CPedGroupIntelligence* intel) {
    m_Timer.StartIfNotAlready(0);
    if (m_Timer.IsOutOfTime()) {
        m_Timer.Start(5'000);
        AllocateTasks(intel);
    }
    return this;
}

// 0x69C850
void CTaskAllocatorKillThreatsBasic::ComputeClosestPeds(CPedGroup& group1, CPedGroup& group2, CPed** peds) {
    std::fill_n(peds, TOTAL_PED_GROUP_MEMBERS, nullptr);

    // dists[member of `group1`][member of `group2`]
    float dists[TOTAL_PED_GROUP_MEMBERS][TOTAL_PED_GROUP_MEMBERS];
    for (auto& row : dists) {
        std::fill(std::begin(row), std::end(row), FLT_MAX);
    }

    auto& mem1 = group1.GetMembership();
    auto& mem2 = group2.GetMembership();

    for (int32 i = 0; i < TOTAL_PED_GROUP_MEMBERS; i++) {
        auto* const a = mem1.GetMember(i);
        if (!a || !a->IsAlive() || a->IsPlayer()) {
            continue;
        }
        for (int32 j = 0; j < TOTAL_PED_GROUP_MEMBERS; j++) {
            auto* const b = mem2.GetMember(j);
            if (!b || !b->IsAlive()) {
                continue;
            }
            const auto& aPos = a->GetPosition();
            const auto& bPos = b->GetPosition();
            // x87: Sum is kept in extended precision (and in this order)
            const double dx = (double)aPos.x - (double)bPos.x;
            const double dy = (double)aPos.y - (double)bPos.y;
            const double dz = (double)aPos.z - (double)bPos.z;
            dists[i][j] = (float)(dz * dz + dx * dx + dy * dy);
        }
    }

    // Greedily pair up the closest peds (each `group1` member gets a unique `group2` member)
    for (int32 n = 0; n < TOTAL_PED_GROUP_MEMBERS; n++) {
        float  best = FLT_MAX; // 0x863A3C
        int32  bi   = -1;
        int32  bj   = -1;
        for (int32 i = 0; i < TOTAL_PED_GROUP_MEMBERS; i++) {
            for (int32 j = 0; j < TOTAL_PED_GROUP_MEMBERS; j++) {
                if (dists[i][j] < best) {
                    best = dists[i][j];
                    bi   = i;
                    bj   = j;
                }
            }
        }
        if (bi < 0 || bj < 0) {
            continue;
        }
        for (int32 k = 0; k < TOTAL_PED_GROUP_MEMBERS; k++) {
            dists[bi][k] = FLT_MAX;
            dists[k][bj] = FLT_MAX;
        }
        peds[bi] = mem2.GetMember(bj);
    }

    // Everyone that wasn't paired will target the leader (or the first alive member if there's no (alive) leader)
    CPed* target = mem2.GetLeader();
    if (!target || !target->IsAlive()) {
        target = nullptr;
        // BUG: Only checks the first 7 members
        for (int32 i = 0; i < TOTAL_PED_GROUP_MEMBERS - 1; i++) {
            auto* const m = mem2.GetMember(i);
            if (m && m->IsAlive()) {
                target = m;
                break;
            }
        }
        if (!target) {
            return;
        }
    }
    for (int32 i = 0; i < TOTAL_PED_GROUP_MEMBERS; i++) {
        if (mem1.GetMember(i) && !peds[i]) {
            peds[i] = target;
        }
    }
}

void CTaskAllocatorKillThreatsBasic::InjectHooks() {
    RH_ScopedVirtualClass(CTaskAllocatorKillThreatsBasic, 0x870e90, 6);
    RH_ScopedCategory("Tasks/Allocators");

    RH_ScopedInstall(Constructor, 0x69C710);
    RH_ScopedInstall(Destructor, 0x69C780);

    RH_ScopedGlobalInstall(ComputeClosestPeds, 0x69C850);
    RH_ScopedVMTInstall(GetType, 0x69C770);
    RH_ScopedVMTInstall(AllocateTasks, 0x69D170);
    RH_ScopedVMTInstall(ProcessGroup, 0x69C7E0);
}

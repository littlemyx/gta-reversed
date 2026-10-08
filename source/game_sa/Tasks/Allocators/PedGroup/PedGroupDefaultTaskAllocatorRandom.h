#pragma once

#include <Base.h>
#include <PluginBase.h>
#include <reversiblehooks/ReversibleHooks.h>

#include "./PedGroupDefaultTaskAllocator.h"
#include "TaskComplexGangFollower.h"
#include "TaskComplexGangLeader.h"
#include "TaskComplexFollowLeaderInFormation.h"

class NOTSA_EXPORT_VTABLE CPedGroupDefaultTaskAllocatorRandom final : public CPedGroupDefaultTaskAllocator {
public:
    /* no virtual destructor */

    // 0x5F6530
    ePedGroupDefaultTaskAllocatorType GetType() const override { return ePedGroupDefaultTaskAllocatorType::RANDOM; };

    // 0x5F6E90
    void AllocateDefaultTasks(CPedGroup* pedGroup, CPed* ped) const override {
        auto& taskPairs = pedGroup->GetIntelligence().GetDefaultPedTaskPairs();

        // Followers
        for (auto&& [i, tp] : rngv::enumerate(taskPairs | rngv::take(TOTAL_PED_GROUP_FOLLOWERS))) {
            if (!tp.Ped) {
                continue;
            }
            if (ped && tp.Ped != ped) {
                continue;
            }
            // NOTE: Original code doesn't check for an existing task (it'd be leaked)
            const auto& offset = CTaskComplexFollowLeaderInFormation::ms_offsets.Offsets[i]; // 0xC196E8 + i * 8
            const auto  task   = new CTaskComplexGangFollower{
                pedGroup,
                pedGroup->GetMembership().GetLeader(),
                (uint8)i,
                CVector{offset.x, offset.y, 0.f},
                10.f
            }; // 0x5F6F2F
            tp.Task = task;
            task->m_bUseSeekEntity = pedGroup->m_bMembersEnterLeadersVehicle; // 0x5F6F52 (0x65EE90)
        }

        // Leader
        auto& leaderTP = taskPairs[CPedGroupMembership::LEADER_MEM_ID];
        if (leaderTP.Ped && (!ped || leaderTP.Ped == ped)) {
            leaderTP.Task = new CTaskComplexGangLeader{pedGroup}; // 0x5F6F93
        }
    };

public:
    static inline void InjectHooks() {
        RH_ScopedVirtualClass(CPedGroupDefaultTaskAllocatorRandom, 0x86C77C, 2);
        RH_ScopedCategory("Tasks/Allocators/PedGroup");

        RH_ScopedVMTInstall(AllocateDefaultTasks, 0x5F6E90);
        RH_ScopedVMTInstall(GetType, 0x5F6530);
    }
};
VALIDATE_SIZE(CPedGroupDefaultTaskAllocatorRandom, sizeof(void*)); /* vtable only */

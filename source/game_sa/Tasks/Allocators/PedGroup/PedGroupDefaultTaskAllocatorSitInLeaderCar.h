#pragma once

#include <Base.h>
#include <PluginBase.h>
#include <reversiblehooks/ReversibleHooks.h>
#include "CarEnterExit.h"

#include "./PedGroupDefaultTaskAllocator.h"
#include "TaskComplexEnterCarAsDriver.h"
#include "TaskComplexEnterCarAsPassenger.h"
#include "TaskSimpleCarDrive.h"

class NOTSA_EXPORT_VTABLE CPedGroupDefaultTaskAllocatorSitInLeaderCar final : public CPedGroupDefaultTaskAllocator {
public:
    /* no virtual destructor */

    // 0x5F6560
    ePedGroupDefaultTaskAllocatorType GetType() const override { return ePedGroupDefaultTaskAllocatorType::SIT_IN_LEADER_CAR; };

    // 0x5F6FC0
    void AllocateDefaultTasks(CPedGroup* pedGroup, CPed* ped) const override {
        const auto leader = pedGroup->GetMembership().GetLeader();
        if (!leader) {
            return;
        }
        const auto veh = leader->m_pVehicle;
        if (!veh) {
            return;
        }
        const auto SetPedDefaultTask = [&](int32 i, CTask* task) {
            auto& tp = pedGroup->GetIntelligence().GetDefaultPedTaskPairs()[i];
            if (tp.Ped && (!ped || tp.Ped == ped)) {
                tp.Task = task; // NOTE: Original code doesn't check for an existing task (it'd be leaked)
            } else {
                delete task;
            }
        };
        SetPedDefaultTask(CPedGroupMembership::LEADER_MEM_ID, new CTaskComplexSequence{
            new CTaskComplexEnterCarAsDriver{veh}, // 0x5F703E
            new CTaskSimpleCarDrive{veh} // 0x5F7074
        });
        int32 seat{};
        // Iterates over the member slots (not a filtered list), as the slot index is also the task pair index
        for (int32 i = 0; i < TOTAL_PED_GROUP_FOLLOWERS; i++) {
            if (!pedGroup->GetMembership().GetMember(i) || seat >= (int32)veh->m_nMaxPassengers) {
                continue;
            }
            SetPedDefaultTask(i, new CTaskComplexSequence{
                new CTaskComplexEnterCarAsPassenger{veh, CCarEnterExit::ComputeTargetDoorToEnterAsPassenger(veh, seat++)}, // 0x5F714B
                new CTaskSimpleCarDrive{veh} // 0x5F7184
            });
        }
    };

public:
    static inline void InjectHooks() {
        RH_ScopedVirtualClass(CPedGroupDefaultTaskAllocatorSitInLeaderCar, 0x86C784, 2);
        RH_ScopedCategory("Tasks/Allocators/PedGroup");

        RH_ScopedVMTInstall(AllocateDefaultTasks, 0x5F6FC0);
        RH_ScopedVMTInstall(GetType, 0x5F6560);
    }
};
VALIDATE_SIZE(CPedGroupDefaultTaskAllocatorSitInLeaderCar, sizeof(void*)); /* vtable only */

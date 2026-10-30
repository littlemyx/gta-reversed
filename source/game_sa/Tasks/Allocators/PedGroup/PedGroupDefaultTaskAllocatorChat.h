#pragma once

#include <Base.h>
#include <PluginBase.h>
#include <reversiblehooks/ReversibleHooks.h>

#include "./PedGroupDefaultTaskAllocator.h"
#include "TaskComplexSequence.h"
#include "TaskComplexGoToPointAndStandStill.h"
#include "TaskSimpleAchieveHeading.h"
#include "TaskSimpleRunTimedAnim.h"
#include "TaskSimpleStandStill.h"
#include "TaskSimpleChat.h"

class NOTSA_EXPORT_VTABLE CPedGroupDefaultTaskAllocatorChat final : public CPedGroupDefaultTaskAllocator {
public:
    /* no virtual destructor */

    // 0x5F6500
    ePedGroupDefaultTaskAllocatorType GetType() const override { return ePedGroupDefaultTaskAllocatorType::CHAT; };

    // 0x5F8180
    void AllocateDefaultTasks(CPedGroup* pedGroup, CPed* ped) const override {
        auto& membership = pedGroup->GetMembership();

        // 0x5F81A9: the present member slots (the slot index is also the index of the sequence / task pair)
        std::array<int32, TOTAL_PED_GROUP_MEMBERS> slots{};
        int32 numSlots{};
        for (int32 i = 0; i < TOTAL_PED_GROUP_MEMBERS; i++) {
            if (membership.GetMember(i)) {
                slots[numSlots++] = i;
            }
        }

        // 0x5F8243: one sequence per member slot
        CTaskComplexSequence seqs[TOTAL_PED_GROUP_MEMBERS];

        // 0x5F8270: every member walks to where it stands and turns to its heading, the sequence is repeated
        for (int32 p = 0; p < numSlots; p++) {
            const auto  slot = slots[p];
            auto* const mem  = membership.GetMember(slot);
            auto&       seq  = seqs[slot];
            seq.AddTask(new CTaskComplexGoToPointAndStandStill{ // 0x5F82B8
                PEDMOVE_WALK,
                mem->GetPosition(),
                0.5f,
                2.f
            });
            seq.AddTask(new CTaskSimpleAchieveHeading{ // 0x5F8311
                mem->m_fCurrentRotation
            });
            seq.SetRepeatMode(true); // 0x5F8338
        }

        // 0x5F8350: ONE round per member; in every round a single member (picked at random) chats, all the others stand still,
        // all for the same random duration. (The port gave every member its own partner / time and a single round.)
        for (int32 round = 0; round < numSlots; round++) {
            const auto chatter  = CGeneral::GetRandomNumberInRange(0, numSlots); // 0x5F8360
            const auto duration = CGeneral::GetRandomNumberInRange(3000, 6000);  // 0x5F8389 (3000 - (int)(rand01 * -3000))
            for (int32 p = 0; p < numSlots; p++) {
                if (p == chatter) {
                    seqs[slots[p]].AddTask(new CTaskSimpleChat{ (uint32)duration }); // 0x5F83BF
                } else {
                    seqs[slots[p]].AddTask(new CTaskSimpleStandStill{ duration, false, false, 8.f }); // 0x5F8425
                }
            }
        }

        // 0x5F8480: hand a copy of the sequence to the member's default task pair
        // NOTE: Original code doesn't check for an existing task (it'd be leaked)
        auto& taskPairs = pedGroup->GetIntelligence().GetDefaultPedTaskPairs();
        for (int32 p = 0; p < numSlots; p++) {
            const auto slot = slots[p];
            auto&      tp   = taskPairs[slot];
            if (!ped || tp.Ped == ped) {
                tp.Task = seqs[slot].Clone();
            }
        }
    };

public:
    static inline void InjectHooks() {
        RH_ScopedVirtualClass(CPedGroupDefaultTaskAllocatorChat, 0x86C774, 2);
        RH_ScopedCategory("Tasks/Allocators/PedGroup");

        RH_ScopedVMTInstall(AllocateDefaultTasks, 0x5F8180);
        RH_ScopedVMTInstall(GetType, 0x5F6500);
    }
};
VALIDATE_SIZE(CPedGroupDefaultTaskAllocatorChat, sizeof(void*)); /* vtable only */

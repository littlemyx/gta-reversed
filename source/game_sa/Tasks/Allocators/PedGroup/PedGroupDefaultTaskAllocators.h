#pragma once

#include <reversiblehooks/ReversibleHooks.h>

#include "./PedGroupDefaultTaskAllocator.h"
#include "./PedGroupDefaultTaskAllocatorType.h"
#include "./PedGroupDefaultTaskAllocatorFollowAnyMeans.h"
#include "./PedGroupDefaultTaskAllocatorFollowLimited.h"
#include "./PedGroupDefaultTaskAllocatorStandStill.h"
#include "./PedGroupDefaultTaskAllocatorChat.h"
#include "./PedGroupDefaultTaskAllocatorSitInLeaderCar.h"
#include "./PedGroupDefaultTaskAllocatorRandom.h"

class CPedGroupDefaultTaskAllocators {
private:
    static inline const NOTSA_GLOBAL(ms_FollowAnyMeansAllocator, 0xC09908, (CPedGroupDefaultTaskAllocatorFollowAnyMeans), {}); static_assert(sizeof(ms_FollowAnyMeansAllocator) == 4);
    static inline const NOTSA_GLOBAL(ms_FollowLimitedAllocator,  0xC0990C, (CPedGroupDefaultTaskAllocatorFollowLimited), {}); static_assert(sizeof(ms_FollowLimitedAllocator) == 4);
    static inline const NOTSA_GLOBAL(ms_StandStillAllocator,     0xC09910, (CPedGroupDefaultTaskAllocatorStandStill), {}); static_assert(sizeof(ms_StandStillAllocator) == 4);
    static inline const NOTSA_GLOBAL(ms_ChatAllocator,           0xC09914, (CPedGroupDefaultTaskAllocatorChat), {}); static_assert(sizeof(ms_ChatAllocator) == 4);
    static inline const NOTSA_GLOBAL(ms_SitInLeaderCarAllocator, 0xC0991C, (CPedGroupDefaultTaskAllocatorSitInLeaderCar), {}); static_assert(sizeof(ms_SitInLeaderCarAllocator) == 4);
    static inline const NOTSA_GLOBAL(ms_RandomAllocator,         0xC09918, (CPedGroupDefaultTaskAllocatorRandom), {}); static_assert(sizeof(ms_RandomAllocator) == 4);

public:
    static const CPedGroupDefaultTaskAllocator& Get(ePedGroupDefaultTaskAllocatorType type) {
        using enum ePedGroupDefaultTaskAllocatorType;
        switch (type) {
        case FOLLOW_ANY_MEANS:  return ms_FollowAnyMeansAllocator;
        case FOLLOW_LIMITED:    return ms_FollowLimitedAllocator;
        case STAND_STILL:       return ms_StandStillAllocator;
        case CHAT:              return ms_ChatAllocator;
        case SIT_IN_LEADER_CAR: return ms_SitInLeaderCarAllocator;
        case RANDOM:            return ms_RandomAllocator;
        default:                NOTSA_UNREACHABLE();
        }
    }

public:
    static void InjectHooks() {
        RH_ScopedClass(CPedGroupDefaultTaskAllocators);
        RH_ScopedCategory("Tasks/Allocators/PedGroup");

        //RH_ScopedInstall(Get, 0x5F7200, { .State = HS::RedirectToGTA, .Locked = true }); // Spoils <ecx>, can't hook
    }
};

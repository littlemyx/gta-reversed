/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#pragma once

#include "Base.h"
#include "PedGroup.h"

class CPed;

class CPedGroups {
public:
    static inline NOTSA_GLOBAL(ScriptReferenceIndex, 0xC098D0, (std::array<uint16, 8>), {});
    static inline NOTSA_GLOBAL(ms_activeGroups, 0xC098E0, (std::array<char, 8>), {});
    static inline NOTSA_GLOBAL(ms_bIsPlayerOnAMission, 0xC098E8, (bool), {});
    static inline NOTSA_GLOBAL(ms_iNoOfPlayerKills, 0xC098EC, (uint32), {});
    static inline NOTSA_GLOBAL(ms_groups, 0xC09920, (std::array<CPedGroup, 8>), {}); static_assert(sizeof(ms_groups) == 5792);

public:
    static void InjectHooks();

    static int32 AddGroup();
    static void RemoveGroup(int32 groupID);
    static void RemoveAllFollowersFromGroup(int32 groupId);

    static void Init();

    static void RegisterKillByPlayer();

    static void CleanUpForShutDown();

    static bool IsGroupLeader(CPed* ped);

    static CPedGroup* GetPedsGroup(const CPed* ped);
    static int32 GetGroupId(const CPedGroup* pedGroup);

    static void Process();

    static bool AreInSameGroup(const CPed* ped1, const CPed* ped2);
    static bool IsInPlayersGroup(CPed* ped);

    // inlined
    static CPedGroup& GetGroup(int32 groupId);

    static auto GetActiveGroupsWithIDs() {
        return ms_groups
            | rngv::enumerate
            | rngv::filter([](auto&& p) { return ms_activeGroups[std::get<0>(p)]; });
    }
};

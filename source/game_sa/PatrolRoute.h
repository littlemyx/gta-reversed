#pragma once

#include "Vector.h"

//! A patrol route (As used by `CTaskComplexFollowPatrolRoute`): Up to 8 points, each with an (optional) animation to play when reaching it.
//! NOTE: The original allocates these from `CPatrolRoutePool` (0x41B810 / 0x41B820). That pool is declared as `CPool<void*>` here (so its
//! slots are 4 bytes, not 0x1A4), which is why this class uses the global allocator instead (There's no other user of that pool).
class CPatrolRoute {
public:
    static constexpr size_t MAX_NODES = 8;

    struct NodeAnim {
        char m_AnimName[24];      //< Empty if there is no anim to play
        char m_AnimGroupName[16];
    };
    VALIDATE_SIZE(NodeAnim, 0x28);

public:
    static void InjectHooks();

    CPatrolRoute(); // 0x66D440
    CPatrolRoute(const CPatrolRoute&) = delete;

    //! Copy the route from another one. Strings are copied up to (and including) their NUL, only the first `m_NumNodes` of the points are copied.
    void Set(const CPatrolRoute& src); // 0x66D4B0

    //! Reverse the order of the points (and their anims)
    void Reverse(); // 0x66D550

private:
    CPatrolRoute* Constructor() { this->CPatrolRoute::CPatrolRoute(); return this; }

public:
    int32    m_NumNodes{};
    NodeAnim m_Anims[MAX_NODES]{};
    CVector  m_Pos[MAX_NODES]{};
};
VALIDATE_SIZE(CPatrolRoute, 0x1A4);

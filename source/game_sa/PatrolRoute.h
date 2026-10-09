#pragma once

#include "Vector.h"

//! A patrol route (As used by `CTaskComplexFollowPatrolRoute`): Up to 8 points, each with an (optional) animation to play when reaching it.
//! NOTE: Allocated from `CPatrolRoutePool` (0x41B810 / 0x41B820), 32 slots of 0x1A4 bytes.
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

    static void* operator new(size_t size); // 0x41B810
    static void operator delete(void* ptr); // 0x41B820

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

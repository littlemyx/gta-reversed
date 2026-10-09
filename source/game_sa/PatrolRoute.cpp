#include "StdInc.h"

#include "PatrolRoute.h"

void CPatrolRoute::InjectHooks() {
    RH_ScopedClass(CPatrolRoute);
    RH_ScopedCategory("Core");

    RH_ScopedInstall(operator new, 0x41B810);
    RH_ScopedInstall(operator delete, 0x41B820);
    RH_ScopedInstall(Constructor, 0x66D440);
    RH_ScopedInstall(Set, 0x66D4B0);
    RH_ScopedInstall(Reverse, 0x66D550);
}

// 0x41B810
void* CPatrolRoute::operator new(size_t) {
    return GetPatrolRoutePool()->New();
}

// 0x41B820
void CPatrolRoute::operator delete(void* ptr) {
    GetPatrolRoutePool()->Delete(static_cast<CPatrolRoute*>(ptr));
}

// 0x66D440
CPatrolRoute::CPatrolRoute() {
    // Rest is done in the header (The original only clears the strings, the points are left as-is)
}

// 0x66D4B0
void CPatrolRoute::Set(const CPatrolRoute& src) {
    m_NumNodes = src.m_NumNodes;
    for (auto i = 0; i < m_NumNodes; i++) {
        m_Pos[i] = src.m_Pos[i];
        strcpy(m_Anims[i].m_AnimName, src.m_Anims[i].m_AnimName);
        strcpy(m_Anims[i].m_AnimGroupName, src.m_Anims[i].m_AnimGroupName);
    }
}

// 0x66D550
void CPatrolRoute::Reverse() {
    for (auto i = 0, j = m_NumNodes - 1; i < j; i++, j--) {
        std::swap(m_Pos[i], m_Pos[j]);

        // Swap strings (Using temporaries like the original does)
        NodeAnim tmp;
        strcpy(tmp.m_AnimName, m_Anims[j].m_AnimName);
        strcpy(tmp.m_AnimGroupName, m_Anims[j].m_AnimGroupName);

        strcpy(m_Anims[j].m_AnimName, m_Anims[i].m_AnimName);
        strcpy(m_Anims[j].m_AnimGroupName, m_Anims[i].m_AnimGroupName);

        strcpy(m_Anims[i].m_AnimName, tmp.m_AnimName);
        strcpy(m_Anims[i].m_AnimGroupName, tmp.m_AnimGroupName);
    }
}

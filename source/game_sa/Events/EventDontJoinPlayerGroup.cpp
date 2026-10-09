#include "StdInc.h"
#include "EventDontJoinPlayerGroup.h"


void CEventDontJoinPlayerGroup::InjectHooks()
{
    RH_ScopedVirtualClass(CEventDontJoinPlayerGroup, 0x86D0E0, 16);
    RH_ScopedCategory("Events");

    RH_ScopedInstall(Constructor, 0x6090E0);

    RH_ScopedVMTDestructorInstall(0x609230);
    RH_ScopedVMTInstall(GetEventType, 0x6091A0);
    RH_ScopedVMTInstall(GetEventPriority, 0x609320);
    RH_ScopedVMTInstall(GetLifeTime, 0x6091B0);
    RH_ScopedVMTInstall(Clone, 0x6091C0);
    RH_ScopedVMTInstall(AffectsPed, 0x609220);
}

// 0x6090E0
CEventDontJoinPlayerGroup::CEventDontJoinPlayerGroup(CPed* player)
{
    m_player = player;
    CEntity::SafeRegisterRef(m_player);
}

CEventDontJoinPlayerGroup::~CEventDontJoinPlayerGroup()
{
    CEntity::SafeCleanUpRef(m_player);
}

// 0x6090E0
CEventDontJoinPlayerGroup* CEventDontJoinPlayerGroup::Constructor(CPed* player)
{
    this->CEventDontJoinPlayerGroup::CEventDontJoinPlayerGroup(player);
    return this;
}


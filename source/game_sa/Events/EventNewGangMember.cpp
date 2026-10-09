#include "StdInc.h"
#include "EventNewGangMember.h"


void CEventNewGangMember::InjectHooks()
{
    RH_ScopedVirtualClass(CEventNewGangMember, 0x86D0A0, 16);
    RH_ScopedCategory("Events");

    RH_ScopedInstall(Constructor, 0x608F70);

    RH_ScopedVMTDestructorInstall(0x609060);
    RH_ScopedVMTInstall(GetEventType, 0x608FD0);
    RH_ScopedVMTInstall(GetEventPriority, 0x609310);
    RH_ScopedVMTInstall(GetLifeTime, 0x608FE0);
    RH_ScopedVMTInstall(Clone, 0x608FF0);
    RH_ScopedVMTInstall(AffectsPed, 0x609050);
}

// 0x608F70
CEventNewGangMember::CEventNewGangMember(CPed* member)
{
    m_member = member;
    CEntity::SafeRegisterRef(m_member);
}

CEventNewGangMember::~CEventNewGangMember()
{
    CEntity::SafeCleanUpRef(m_member);
}

// 0x608F70
CEventNewGangMember* CEventNewGangMember::Constructor(CPed* member)
{
    this->CEventNewGangMember::CEventNewGangMember(member);
    return this;
}


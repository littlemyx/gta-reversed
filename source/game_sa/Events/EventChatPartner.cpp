#include "StdInc.h"
#include "EventChatPartner.h"

void CEventChatPartner::InjectHooks() {
    RH_ScopedVirtualClass(CEventChatPartner, 0x85B070, 16);
    RH_ScopedCategory("Events");

    RH_ScopedVMTDestructorInstall(0x4B60D0);
    RH_ScopedVMTInstall(GetEventType, 0x4AED30);
    RH_ScopedVMTInstall(GetEventPriority, 0x4AED50);
    RH_ScopedVMTInstall(GetLifeTime, 0x4AED40);
    RH_ScopedVMTInstall(Clone, 0x4B7210);
    RH_ScopedVMTInstall(AffectsPed, 0x4AEDC0);
}


CEventChatPartner::CEventChatPartner(bool leadSpeaker, CPed* partner) : CEvent() {
    m_leadSpeaker = leadSpeaker;
    m_partner = partner;
    CEntity::SafeRegisterRef(m_partner);
}

CEventChatPartner::~CEventChatPartner() {
    CEntity::SafeCleanUpRef(m_partner);
}

bool CEventChatPartner::AffectsPed(CPed* ped) {
    return ped->IsAlive() && m_partner;
}


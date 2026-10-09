#include "StdInc.h"

#include "EventPedToChase.h"

void CEventPedToChase::InjectHooks() {
    RH_ScopedVirtualClass(CEventPedToChase, 0x85B138, 16);
    RH_ScopedCategory("Events");

    RH_ScopedVMTDestructorInstall(0x4B61C0);
    RH_ScopedVMTInstall(GetEventType, 0x4AF190);
    RH_ScopedVMTInstall(GetEventPriority, 0x4AF1B0);
    RH_ScopedVMTInstall(GetLifeTime, 0x4AF1A0);
    RH_ScopedVMTInstall(Clone, 0x4B7360);
    RH_ScopedVMTInstall(AffectsPed, 0x4AF220);
}

// 0x4AF130
CEventPedToChase::CEventPedToChase(CPed* ped) {
    m_ped = ped;
    CEntity::SafeRegisterRef(m_ped);
}

// 0x4AF1C0
CEventPedToChase::~CEventPedToChase() {
    CEntity::SafeCleanUpRef(m_ped);
}

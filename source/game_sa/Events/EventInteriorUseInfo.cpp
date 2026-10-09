#include "StdInc.h"
#include "EventInteriorUseInfo.h"

void CEventInteriorUseInfo::InjectHooks() {
    RH_ScopedVirtualClass(CEventInteriorUseInfo, 0x8701A8, 16);
    RH_ScopedCategory("Events");

    RH_ScopedVMTDestructorInstall(0x674F90);
    RH_ScopedVMTInstall(GetEventType, 0x674EE0);
    RH_ScopedVMTInstall(GetEventPriority, 0x674FB0);
    RH_ScopedVMTInstall(GetLifeTime, 0x674EF0);
    RH_ScopedVMTInstall(Clone, 0x674F00);
    RH_ScopedVMTInstall(AffectsPed, 0x674F80);
    RH_ScopedVMTInstall(IsValid, 0x4B1640);
}


CEventInteriorUseInfo::CEventInteriorUseInfo(InteriorInfo_t* interiorInfo, Interior_c* interior, uint32 animTime, int8 loopAction) : CEvent() {
    m_InteriorInfo = interiorInfo;
    m_nLoopAction = loopAction;
    m_Interior = interior;
    m_ActionAnimTime = animTime;
}

bool CEventInteriorUseInfo::IsValid(CPed* ped) {
    if (ped)
        return ped->IsAlive();
    return CEvent::IsValid(ped);
}

bool CEventInteriorUseInfo::AffectsPed(CPed* ped) {
    return true;
}


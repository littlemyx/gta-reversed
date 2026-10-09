#include "StdInc.h"
#include "EventCreatePartnerTask.h"

void CEventCreatePartnerTask::InjectHooks() {
    RH_ScopedVirtualClass(CEventCreatePartnerTask, 0x86C6D0, 16);
    RH_ScopedCategory("Events");

    RH_ScopedVMTDestructorInstall(0x5F6300);
    RH_ScopedVMTInstall(GetEventType, 0x5F6260);
    RH_ScopedVMTInstall(GetEventPriority, 0x5F6410);
    RH_ScopedVMTInstall(GetLifeTime, 0x5F6270);
    RH_ScopedVMTInstall(Clone, 0x5F6280);
    RH_ScopedVMTInstall(AffectsPed, 0x5F62F0);
}


CEventCreatePartnerTask::CEventCreatePartnerTask(int32 type, CPed* partner, bool isLeadSpeaker, float meetDist) :
    m_partnerType{type},
    m_partner{partner},
    m_isLeadSpeaker{isLeadSpeaker},
    m_meetDist{meetDist}
{
    CEntity::SafeRegisterRef(m_partner);
}

CEventCreatePartnerTask::~CEventCreatePartnerTask() {
    CEntity::SafeCleanUpRef(m_partner);
}


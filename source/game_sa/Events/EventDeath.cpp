#include "StdInc.h"

#include "EventDeath.h"

void CEventDeath::InjectHooks() {
    RH_ScopedVirtualClass(CEventDeath, 0x85ADC0, 16);
    RH_ScopedCategory("Events");

    RH_ScopedVMTDestructorInstall(0x4B5DB0);
    RH_ScopedVMTInstall(GetEventType, 0x4ADE20);
    RH_ScopedVMTInstall(GetEventPriority, 0x4ADE40);
    RH_ScopedVMTInstall(GetLifeTime, 0x4ADE30);
    RH_ScopedVMTInstall(Clone, 0x4B6E30);
    RH_ScopedVMTInstall(AffectsPed, 0x4ADE80);
}

// 0x4ADE50, `time` should be int32
CEventDeath::CEventDeath(bool bDrowning, uint32 deathTimeInMs) : CEvent() {
    m_bDrowning = bDrowning;
    m_deathTimeInMs = deathTimeInMs;
}

// 0x4ADDF0
CEventDeath::CEventDeath(bool bDrowning) : CEvent() {
    m_bDrowning = bDrowning;
    m_deathTimeInMs = CTimer::GetTimeInMS();
}

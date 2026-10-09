#include "StdInc.h"

#include "EventSource.h"
#include "Events/Event.h"

void CEventSource::InjectHooks() {
    RH_ScopedClass(CEventSource);
    RH_ScopedCategory("Events");

    RH_ScopedInstall(ComputeEventSourceType, 0x4ABAC0);
}

// 0x4ABAC0
int32 CEventSource::ComputeEventSourceType(const CEvent& event, const CPed& ped) {
    const auto* const src = event.GetSourceEntity();
    if (!src || !src->GetIsTypePed()) {
        return 0;
    }
    const auto* const srcPed = src->AsPed();
    if (ped.GetIntelligence()->IsThreatenedBy(*srcPed)) { // 0x601C30
        return 3;
    }
    if (ped.GetIntelligence()->IsFriendlyWith(*srcPed)) { // 0x601BC0
        return 2;
    }
    if (srcPed->IsPlayer()) { // 0x5DF8F0
        return 1;
    }
    return 0;
}

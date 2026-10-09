#include "StdInc.h"

#include "PedTriggerScriptAttractor.h"

void CPedTriggerScriptAttractor::InjectHooks() {
    RH_ScopedVirtualClass(CPedTriggerScriptAttractor, 0x86C5CC, 6);
    RH_ScopedCategory("Attractors");

    RH_ScopedVMTInstall(GetType, 0x5EE8F0);
}

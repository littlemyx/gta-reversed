#include "StdInc.h"

#include "PedPizzaAttractor.h"

void CPedPizzaAttractor::InjectHooks() {
    RH_ScopedVirtualClass(CPedPizzaAttractor, 0x86C598, 7);
    RH_ScopedCategory("Attractors");

    RH_ScopedVMTInstall(GetType, 0x5EE740);
    RH_ScopedVMTInstall(GetHeadOfQueueWaitTime, 0x5EE750);
}

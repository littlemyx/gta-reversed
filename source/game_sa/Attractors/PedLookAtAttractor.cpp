#include "StdInc.h"

#include "PedLookAtAttractor.h"

void CPedLookAtAttractor::InjectHooks() {
    RH_ScopedVirtualClass(CPedLookAtAttractor, 0x86C5E4, 6);
    RH_ScopedCategory("Attractors");

    RH_ScopedVMTInstall(GetType, 0x5EE9C0);
}

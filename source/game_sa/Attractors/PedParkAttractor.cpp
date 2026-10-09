#include "StdInc.h"

#include "PedParkAttractor.h"

void CPedParkAttractor::InjectHooks() {
    RH_ScopedVirtualClass(CPedParkAttractor, 0x86C614, 6);
    RH_ScopedCategory("Attractors");

    RH_ScopedVMTInstall(GetType, 0x5EEB60);
}

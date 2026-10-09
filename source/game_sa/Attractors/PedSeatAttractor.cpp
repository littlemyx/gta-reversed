#include "StdInc.h"

#include "PedSeatAttractor.h"

void CPedSeatAttractor::InjectHooks() {
    RH_ScopedVirtualClass(CPedSeatAttractor, 0x86C568, 6);
    RH_ScopedCategory("Attractors");

    RH_ScopedVMTInstall(GetType, 0x5EE5A0);
}

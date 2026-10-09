#include "StdInc.h"

#include "PedAtmAttractor.h"

void CPedATMAttractor::InjectHooks() {
    RH_ScopedVirtualClass(CPedATMAttractor, 0x86C550, 6);
    RH_ScopedCategory("Attractors");

    RH_ScopedVMTInstall(GetType, 0x5EE4D0);
}

#include "StdInc.h"

#include "PedStepAttractor.h"

void CPedStepAttractor::InjectHooks() {
    RH_ScopedVirtualClass(CPedStepAttractor, 0x86C62C, 6);
    RH_ScopedCategory("Attractors");

    RH_ScopedVMTInstall(GetType, 0x5EEC30);
}

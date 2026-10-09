#include "StdInc.h"

#include "PedStopAttractor.h"

void CPedStopAttractor::InjectHooks() {
    RH_ScopedVirtualClass(CPedStopAttractor, 0x86C580, 6);
    RH_ScopedCategory("Attractors");

    RH_ScopedVMTInstall(GetType, 0x5EE670);
}

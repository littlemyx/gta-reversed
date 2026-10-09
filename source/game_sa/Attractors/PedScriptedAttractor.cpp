#include "StdInc.h"

#include "PedScriptedAttractor.h"

void CPedScriptedAttractor::InjectHooks() {
    RH_ScopedVirtualClass(CPedScriptedAttractor, 0x86C5FC, 6);
    RH_ScopedCategory("Attractors");

    RH_ScopedVMTInstall(GetType, 0x5EEA90);
}

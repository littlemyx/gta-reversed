#include "StdInc.h"

#include "DummyPed.h"

void CDummyPed::InjectHooks() {
    RH_ScopedVirtualClass(CDummyPed, 0x86C198, 22);
    RH_ScopedCategory("Entity/Dummy");

    RH_ScopedVMTDestructorInstall(0x5DE310);
}

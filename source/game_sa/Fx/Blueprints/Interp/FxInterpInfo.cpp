#include "StdInc.h"

#include "FxInterpInfo.h"

void FxInterpInfo_c::InjectHooks() {
    RH_ScopedVirtualClass(FxInterpInfo_c, 0x85A968, 2);
    RH_ScopedCategory("Fx");

    RH_ScopedVMTDestructorInstall(0x4A8D10);
}

// 0x4A8410
FxInterpInfo_c::FxInterpInfo_c() {
    m_bLooped = false;
    m_pTimes  = nullptr;
}

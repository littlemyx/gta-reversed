#include "StdInc.h"

#include "FxInfoUnderwater.h"
#include "MovementInfo.h"

void FxInfoUnderwater_c::InjectHooks() {
    RH_ScopedVirtualClass(FxInfoUnderwater_c, 0x85A8C0, 3);
    RH_ScopedCategory("Fx");

    RH_ScopedVMTDestructorInstall(0x4A7550);
    RH_ScopedVMTInstall(Load, 0x4A5E70);
    RH_ScopedVMTInstall(GetValue, 0x4A5E90);
}

// 0x4A5E20
FxInfoUnderwater_c::FxInfoUnderwater_c() : FxInfo_c() {
    m_nType = FX_INFO_UNDERWATER_DATA;
    m_InterpInfo.Allocate(0);
}

void FxInfoUnderwater_c::Load(FILESTREAM file, int32 version) {
    m_InterpInfo.Load(file);
}

// 0x4A5E90
void FxInfoUnderwater_c::GetValue(float currentTime, float mult, float totalTime, float length, bool useConst, void* info) {
    static_cast<MovementInfo_t*>(info)->m_bHasUnderwaterInfo = true;
}

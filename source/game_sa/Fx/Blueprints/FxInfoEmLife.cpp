#include "StdInc.h"

#include "FxInfoEmLife.h"

void FxInfoEmLife_c::InjectHooks() {
    RH_ScopedVirtualClass(FxInfoEmLife_c, 0x85A818, 3);
    RH_ScopedCategory("Fx");

    RH_ScopedVMTDestructorInstall(0x4A6CA0);
    RH_ScopedVMTInstall(Load, 0x4A4FC0);
    RH_ScopedVMTInstall(GetValue, 0x4A4FE0);
}

// 0x4A4F50
FxInfoEmLife_c::FxInfoEmLife_c() : FxInfo_c() {
    m_nType = FX_INFO_EMLIFE_DATA;
    m_InterpInfo.Allocate(2);
}

// 0x4A4FC0
void FxInfoEmLife_c::Load(FILESTREAM file, int32 version) {
    m_InterpInfo.Load(file);
}

// 0x4A4FE0
void FxInfoEmLife_c::GetValue(float currentTime, float mult, float totalTime, float len, bool useConst, void* info) {
    float values[2];
    m_InterpInfo.GetVal(values, currentTime);

    auto& emission = *static_cast<EmissionInfo_t*>(info);
    emission.m_fLife     = values[0];
    emission.m_fLifeBias = values[1];
}

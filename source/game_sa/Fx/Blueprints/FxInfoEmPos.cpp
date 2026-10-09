#include "StdInc.h"

#include "FxInfoEmPos.h"

void FxInfoEmPos_c::InjectHooks() {
    RH_ScopedVirtualClass(FxInfoEmPos_c, 0x85A824, 3);
    RH_ScopedCategory("Fx");

    RH_ScopedVMTDestructorInstall(0x4A6D10);
    RH_ScopedVMTInstall(Load, 0x4A5080);
    RH_ScopedVMTInstall(GetValue, 0x4A50A0);
}

// 0x4A5010
FxInfoEmPos_c::FxInfoEmPos_c() : FxInfo_c() {
    m_nType = FX_INFO_EMPOS_DATA;
    m_InterpInfo.Allocate(3);
}

// 0x4A5080
void FxInfoEmPos_c::Load(FILESTREAM file, int32 version) {
    m_InterpInfo.Load(file);
}

// 0x4A50A0
void FxInfoEmPos_c::GetValue(float currentTime, float mult, float totalTime, float len, bool useConst, void* info) {
    float values[3];
    m_InterpInfo.GetVal(values, currentTime);

    static_cast<EmissionInfo_t*>(info)->m_Pos = *(CVector*)&values[0];
}

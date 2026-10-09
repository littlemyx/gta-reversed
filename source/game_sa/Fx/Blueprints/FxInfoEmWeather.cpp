#include "StdInc.h"

#include "FxInfoEmWeather.h"
#include "EmissionInfo.h"

void FxInfoEmWeather_c::InjectHooks() {
    RH_ScopedVirtualClass(FxInfoEmWeather_c, 0x85A830, 3);
    RH_ScopedCategory("Fx");

    RH_ScopedVMTDestructorInstall(0x4A6D80);
    RH_ScopedVMTInstall(Load, 0x4A5150);
    RH_ScopedVMTInstall(GetValue, 0x4A5170);
}

// 0x4A50E0
FxInfoEmWeather_c::FxInfoEmWeather_c() : FxInfo_c(), m_InterpInfo() {
    m_nType = FX_INFO_EMWEATHER_DATA;
    m_InterpInfo.Allocate(4);
}

// 0x4A5150
void FxInfoEmWeather_c::Load(FILESTREAM file, int32 version) {
    m_InterpInfo.Load(file);
}

// 0x4A5170
void FxInfoEmWeather_c::GetValue(float currentTime, float mult, float totalTime, float len, bool useConst, void* info) {
    float values[4];
    m_InterpInfo.GetVal(values, currentTime);

    auto& emission = *static_cast<EmissionInfo_t*>(info);
    emission.m_fMinWind = values[0];
    emission.m_fMaxWind = values[1];
    emission.m_fMinRain = values[2];
    emission.m_fMaxRain = values[3];
}

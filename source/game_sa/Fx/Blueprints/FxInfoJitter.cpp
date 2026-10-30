#include "StdInc.h"

#include "FxInfoJitter.h"

void FxInfoJitter_c::InjectHooks() {
    RH_ScopedVirtualClass(FxInfoJitter_c, 0x85A89C, 3);
    RH_ScopedCategory("Fx");

    RH_ScopedVMTDestructorInstall(0x4A7400);
    RH_ScopedVMTInstall(Load, 0x4A5B90);
    RH_ScopedVMTInstall(GetValue, 0x4A5BB0);
}

// 0x4A5B20
FxInfoJitter_c::FxInfoJitter_c() : FxInfo_c() {
    m_nType = FX_INFO_JITTER_DATA;
    m_InterpInfo.Allocate(1);
}

// 0x4A5B90
void FxInfoJitter_c::Load(FILESTREAM file, int32 version) {
    m_InterpInfo.Load(file);
}

// 0x4A5BB0
void FxInfoJitter_c::GetValue(float currentTime, float mult, float totalTime, float len, bool useConst, void* info) {
    if (!m_bTimeModeParticle) {
        mult = currentTime / len;
    }

    float value;
    m_InterpInfo.GetVal(&value, mult);

    // 0x4A5BE2/0x4A5C07/0x4A5C2C: three rand() calls (x, y, z in this order), then RW's table-based RwV3dNormalize (0x7ED9B0), not CVector::Normalise
    CVector in;
    in.x = (float)(CGeneral::GetRandomNumber() % 10'000) * ExeRecip(5000.0f) - 1.0f;
    in.y = (float)(CGeneral::GetRandomNumber() % 10'000) * ExeRecip(5000.0f) - 1.0f;
    in.z = (float)(CGeneral::GetRandomNumber() % 10'000) * ExeRecip(5000.0f) - 1.0f;
    RwV3dNormalize(&in, &in);
    in *= value * totalTime;

    auto& movement = *static_cast<MovementInfo_t*>(info);
    movement.m_Pos += in;
}

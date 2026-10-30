#include "StdInc.h"

#include "FxInfoNoise.h"

void FxInfoNoise_c::InjectHooks() {
    RH_ScopedVirtualClass(FxInfoNoise_c, 0x85A848, 3);
    RH_ScopedCategory("Fx");

    RH_ScopedVMTDestructorInstall(0x4A6E60);
    RH_ScopedVMTInstall(Load, 0x4A52E0);
    RH_ScopedVMTInstall(GetValue, 0x4A5300);
}

FxInfoNoise_c::FxInfoNoise_c() : FxInfo_c() {
    m_nType = FX_INFO_NOISE_DATA;
    m_InterpInfo.Allocate(1);
}

void FxInfoNoise_c::Load(FILESTREAM file, int32 version) {
    m_InterpInfo.Load(file);
}

// 0x4A5300
void FxInfoNoise_c::GetValue(float currentTime, float mult, float totalTime, float len, bool useConst, void* info) {
    auto& movement = *static_cast<MovementInfo_t*>(info);
    if (!m_bTimeModeParticle) {
        mult = currentTime / len;
    }

    float value;
    m_InterpInfo.GetVal(&value, mult);

    // 0x4A5334/0x4A535E/0x4A5383: three rand() calls (x, y, z in this order); RW's table-based RwV3dNormalize (0x7ED9B0) / RwV3dLength (0x7EDAC0), not CVector::Normalise / Magnitude
    CVector in;
    in.x = (float)(CGeneral::GetRandomNumber() % 10'000) * ExeRecip(5000.0f) - 1.0f;
    in.y = (float)(CGeneral::GetRandomNumber() % 10'000) * ExeRecip(5000.0f) - 1.0f;
    in.z = (float)(CGeneral::GetRandomNumber() % 10'000) * ExeRecip(5000.0f) - 1.0f;
    RwV3dNormalize(&in, &in);
    in *= value * totalTime;

    const float length = RwV3dLength(&movement.m_Vel);
    movement.m_Vel += in;

    if (RwV3dLength(&movement.m_Vel) > 0.0f) { // fcomp + test ah,0x41 / jne: NaN skips too
        RwV3dNormalize(&movement.m_Vel, &movement.m_Vel);
    }

    movement.m_Vel *= length;
}

#include "StdInc.h"

#include "FxInfoAttractPt.h"

void FxInfoAttractPt_c::InjectHooks() {
    RH_ScopedVirtualClass(FxInfoAttractPt_c, 0x85A86C, 3);
    RH_ScopedCategory("Fx");

    RH_ScopedVMTDestructorInstall(0x4A6FB0);
    RH_ScopedVMTInstall(Load, 0x4A56C0);
    RH_ScopedVMTInstall(GetValue, 0x4A56E0);
}

// 0x4A5650
FxInfoAttractPt_c::FxInfoAttractPt_c() : FxInfo_c() {
    m_nType = FX_INFO_ATTRACTPT_DATA;
    m_InterpInfo.Allocate(4);
}

// 0x4A56C0
void FxInfoAttractPt_c::Load(FILESTREAM file, int32 version) {
    m_InterpInfo.Load(file);
}

// 0x4A56E0
void FxInfoAttractPt_c::GetValue(float currentTime, float mult, float totalTime, float len, bool useConst, void* info) {
    if (!m_bTimeModeParticle) {
        mult = currentTime / len;
    }

    float values[4];
    m_InterpInfo.GetVal(values, mult);

    auto& movement = *static_cast<MovementInfo_t*>(info);
    // The exe uses RenderWare's table-based RwV3dLength (0x7EDAC0) / RwV3dNormalize (0x7ED9B0), not sqrtf, and scales the direction by (values[0] * totalTime)
    CVector in{ values[0] - movement.m_Pos.x, values[1] - movement.m_Pos.y, values[2] - movement.m_Pos.z };
    const float length = RwV3dLength(&in);
    if (length > 0.0f) {
        RwV3dNormalize(&in, &in);
    }
    if (length > 0.1f) {
        const float strength = values[0] * totalTime;
        movement.m_Vel.x += in.x * strength;
        movement.m_Vel.y += in.y * strength;
        movement.m_Vel.z += in.z * strength;
    }
}

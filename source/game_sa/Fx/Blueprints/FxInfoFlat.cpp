#include "StdInc.h"

#include "FxInfoFlat.h"
#include "FxManager.h"

void FxInfoFlat_c::InjectHooks() {
    RH_ScopedVirtualClass(FxInfoFlat_c, 0x85A908, 3);
    RH_ScopedCategory("Fx");

    RH_ScopedVMTDestructorInstall(0x4A77F0);
    RH_ScopedVMTInstall(Load, 0x4A6390);
    RH_ScopedVMTInstall(GetValue, 0x4A63B0);
}

// 0x4A6320
FxInfoFlat_c::FxInfoFlat_c() : FxInfo_c(), m_InterpInfo() {
    m_nType = FX_INFO_FLAT_DATA;
    m_InterpInfo.Allocate(9);
}

// 0x4A6390
void FxInfoFlat_c::Load(FILESTREAM file, int32 version) {
    m_InterpInfo.Load(file);
}

// 004A63B0
void FxInfoFlat_c::GetValue(float currentTime, float mult, float totalTime, float len, bool useConst, void* info) {
    if ( !m_bTimeModeParticle) {
        mult = currentTime / len;
    }

    float values[9];
    m_InterpInfo.GetVal(values, mult);

    auto& render = *static_cast<RenderInfo_t*>(info);
    render.m_FlatMatrix.right = *(RwV3d *)&values[0];
    render.m_FlatMatrix.up    = *(RwV3d *)&values[3];
    render.m_FlatMatrix.at    = *(RwV3d *)&values[6];
    render.m_bIsFlat = true;
    RwMatrixUpdate(&render.m_FlatMatrix);
}

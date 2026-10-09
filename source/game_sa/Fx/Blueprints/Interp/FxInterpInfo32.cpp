#include "StdInc.h"

#include "FxInterpInfo32.h"
#include "FxManager.h"
#include "FxInterpInfoGetVal.h"

void FxInterpInfo32_c::InjectHooks() {
    RH_ScopedVirtualClass(FxInterpInfo32_c, 0x85A980, 2);
    RH_ScopedCategory("Fx");

    RH_ScopedInstall(GetVal, 0x4A89C0);

    RH_ScopedVMTDestructorInstall(0x4A8D70);
    RH_ScopedVMTInstall(Load, 0x5C1B10);
}

// 0x4A8990
FxInterpInfo32_c::FxInterpInfo32_c() : FxInterpInfo_c() {
    m_Keys = nullptr;
}

// 0x5C1B10
void FxInterpInfo32_c::Load(FILESTREAM file) {
    for (auto i = 0; i < m_nCount; i++) {
        ReadField<void>(file);
        ReadField<void>(file, "FX_INTERP_DATA:");

        m_bLooped  = ReadField<bool>(file);
        m_nNumKeys = ReadField<int8>(file);

        if (i == 0) {
            m_pTimes = g_fxMan.Allocate<uint16>(m_nNumKeys);
        }

        m_Keys[i] = g_fxMan.Allocate<int16>(m_nNumKeys);
        for (auto j = 0; j < m_nNumKeys; j++) {
            ReadField<void>(file, "FX_KEYFLOAT_DATA:");
            m_pTimes[j]     = uint16(ReadField<float>(file) * 256.f);
            m_Keys[i][j] = int16(ReadField<float>(file) * 1000.f);
        }
    }
}

// NOTSA
void FxInterpInfo32_c::Allocate(int32 count) {
    m_nCount  = count;
    m_Keys = g_fxMan.Allocate<int16*>(count);
}

// 0x4A89C0
void FxInterpInfo32_c::GetVal(float* outValues, float delta) {
    notsa::detail::FxInterpInfoGetVal(*this, m_Keys, 0.0010000000474974513, outValues, delta);
}

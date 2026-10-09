#include "StdInc.h"

#include "FxInterpInfoU255.h"
#include "FxManager.h"
#include "FxInterpInfoGetVal.h"

void FxInterpInfoU255_c::InjectHooks() {
    RH_ScopedVirtualClass(FxInterpInfoU255_c, 0x85A978, 2);
    RH_ScopedCategory("Fx");

    RH_ScopedInstall(GetVal, 0x4A8800);

    RH_ScopedVMTDestructorInstall(0x4A8D50);
    RH_ScopedVMTInstall(Load, 0x5C18F0);
}

// 0x4A87D0
FxInterpInfoU255_c::FxInterpInfoU255_c() : FxInterpInfo_c() {
    m_Keys = nullptr;
}

// 0x5C18F0
void FxInterpInfoU255_c::Load(FILESTREAM file) {
    for (auto i = 0; i < m_nCount; i++) {
        ReadField<void>(file);
        ReadField<void>(file, "FX_INTERP_DATA:");

        m_bLooped  = ReadField<bool>(file);
        m_nNumKeys = ReadField<int8>(file);

        if (i == 0) {
            m_pTimes = g_fxMan.Allocate<uint16>(m_nNumKeys);
        }

        m_Keys[i] = g_fxMan.Allocate<uint16>(m_nNumKeys);
        for (auto j = 0; j < m_nNumKeys; j++) {
            ReadField<void>(file, "FX_KEYFLOAT_DATA:");
            m_pTimes[j] = uint16(ReadField<float>(file) * 256.f);
            m_Keys[i][j] = uint16(ReadField<float>(file) * 256.f);
        }
    }
}

// NOTSA
void FxInterpInfoU255_c::Allocate(int32 count) {
    m_nCount = count;
    m_Keys = g_fxMan.Allocate<uint16*>(count);
}

// 0x4A8800
void FxInterpInfoU255_c::GetVal(float* outValues, float delta) {
    notsa::detail::FxInterpInfoGetVal(*this, m_Keys, 0.00390625, outValues, delta);
}

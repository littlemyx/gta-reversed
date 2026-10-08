#include "StdInc.h"

#include "FxInterpInfoFloat.h"
#include "FxManager.h"
#include "FxInterpInfoGetVal.h"

void FxInterpInfoFloat_c::InjectHooks() {
    RH_ScopedClass(FxInterpInfoFloat_c);
    RH_ScopedCategory("Fx");

    RH_ScopedOverloadedInstall(GetVal, "", 0x4A8470, void(FxInterpInfoFloat_c::*)(float*, float));
}

// 0x4A8440
FxInterpInfoFloat_c::FxInterpInfoFloat_c() : FxInterpInfo_c() {
    m_Keys = nullptr;
}

// 0x5C16F0
void FxInterpInfoFloat_c::Load(FILESTREAM file) {
    for (auto i = 0; i < m_nCount; i++) {
        ReadField<void>(file);
        ReadField<void>(file, "FX_INTERP_DATA:");

        m_bLooped  = ReadField<bool>(file);
        m_nNumKeys = ReadField<int8>(file);

        if (i == 0) {
            m_pTimes = g_fxMan.Allocate<uint16>(m_nNumKeys);
        }

        m_Keys[i] = g_fxMan.Allocate<float>(m_nNumKeys);
        for (auto j = 0; j < m_nNumKeys; j++) {
            ReadField<void>(file, "FX_KEYFLOAT_DATA:");
            m_pTimes[j] = uint16(ReadField<float>(file) * 256.f);
            m_Keys[i][j] = ReadField<float>(file);
        }
    }
}

// NOTSA
void FxInterpInfoFloat_c::Allocate(int32 count) {
    m_nCount = count;
    m_Keys = g_fxMan.Allocate<float*>(count);
}

// 0x4A85C0
float FxInterpInfoFloat_c::GetVal(int32 attrib, float time, float deltaTime) {
    return plugin::CallMethodAndReturn<float, 0x4A85C0, FxInterpInfoFloat_c*, int32, float, float>(this, attrib, time, deltaTime);
}

// 0x4A8470
void FxInterpInfoFloat_c::GetVal(float* outValues, float delta) {
    notsa::detail::FxInterpInfoGetVal(*this, m_Keys, 1.0, outValues, delta);
}

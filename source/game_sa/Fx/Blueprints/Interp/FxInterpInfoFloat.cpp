#include "StdInc.h"

#include "FxInterpInfoFloat.h"
#include "FxManager.h"
#include "FxInterpInfoGetVal.h"

void FxInterpInfoFloat_c::InjectHooks() {
    RH_ScopedVirtualClass(FxInterpInfoFloat_c, 0x85A970, 2);
    RH_ScopedCategory("Fx");

    RH_ScopedOverloadedInstall(GetVal, "", 0x4A8470, void(FxInterpInfoFloat_c::*)(float*, float));
    RH_ScopedOverloadedInstall(GetVal, "integral", 0x4A85C0, float(FxInterpInfoFloat_c::*)(int32, float, float));

    RH_ScopedVMTDestructorInstall(0x4A8D30);
    RH_ScopedVMTInstall(Load, 0x5C16F0);
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
    // Integrates the piecewise linear key curve of `attrib` over [time - deltaTime, time] (trapezoid rule per segment).
    // Everything is kept on the x87 stack in the original => `double` here.
    constexpr double TIME_SCALE = 0.00390625; // 0x859AA0 (1/256)
    constexpr double HALF       = 0.5;        // 0x858B8C

    const float* const keys = m_Keys[attrib];
    if (m_nNumKeys == 1) {
        return (float)((double)deltaTime * (double)keys[0]);
    }

    const int32 n = m_nNumKeys;
    double      t = (double)time - (double)deltaTime;
    double      acc = 0.0; // 0x858B50

    // Find the first key with `t < keyTime`
    int32 j = 0;
    for (; j < n; j++) {
        if (t < (double)m_pTimes[j] * TIME_SCALE) {
            break;
        }
    }

    if (j == n) { // `t` is past the last key
        return (float)((double)deltaTime * (double)keys[n - 1]);
    }
    if (j > n) { // n < 0
        return 0.0f;
    }

    // Value at `t`
    double S;
    if (j > 0) {
        const double T0 = (double)m_pTimes[j - 1] * TIME_SCALE;
        const double T1 = (double)m_pTimes[j] * TIME_SCALE;
        const double r  = (t - T0) / (T1 - T0);
        S               = r * ((double)keys[j] - (double)keys[j - 1]) + (double)keys[j - 1];
    } else {
        S = (double)keys[0];
    }

    for (; j < n; j++) {
        const auto Tj = (float)((double)m_pTimes[j] * TIME_SCALE);
        // BUG: For j == 0 this reads `m_pTimes[-1]` and `keys[-1]` (out of bounds), but only if `time` is before the first key
        const auto Tp = (float)((double)m_pTimes[j - 1] * TIME_SCALE);
        if (Tj == time) { // 0x4A8732
            const double w = ((double)keys[j] - S) * HALF + S;
            acc += w * ((double)Tj - t);
            return (float)acc;
        }
        if (Tj < time) { // 0x4A86E9
            const double w = ((double)keys[j] - S) * HALF + S;
            acc += w * ((double)Tj - t);
            S = (double)keys[j];
            t = (double)Tj;
        } else if (Tj > time) { // 0x4A874B
            const double r  = ((double)time - (double)Tp) / ((double)Tj - (double)Tp);
            const double v  = r * ((double)keys[j] - (double)keys[j - 1]) + (double)keys[j - 1];
            const double w  = (v - S) * HALF + S;
            acc += w * ((double)time - t);
            return (float)acc;
        }
        // (unordered => just go to the next key)
    }

    // `time` is past the last key
    const double w = ((double)keys[n - 1] - S) * HALF + S;
    acc += w * ((double)m_pTimes[n - 1] * TIME_SCALE - t);
    return (float)acc;
}

// 0x4A8470
void FxInterpInfoFloat_c::GetVal(float* outValues, float delta) {
    notsa::detail::FxInterpInfoGetVal(*this, m_Keys, 1.0, outValues, delta);
}

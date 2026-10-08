#pragma once

#include "FxInterpInfo.h"
#include "FxFtol.h"

namespace notsa::detail {
/*!
 * Shared body of FxInterpInfo{Float,32,255,U255}_c::GetVal(float*, float)
 * (0x4A8470 / 0x4A89C0 / 0x4A8B80 / 0x4A8800). The four originals differ only in the key type and the
 * scale applied to the keys (key * scale is done on the x87 stack => double here).
 */
template<typename TKey>
void FxInterpInfoGetVal(const FxInterpInfo_c& self, TKey* const* keys, double scale, float* outValues, float delta) {
    constexpr double TIME_SCALE = 0.00390625; // 0x859AA0 (1/256)

    const auto KeyAt = [&](int32 i, int32 k) { return (double)keys[i][k] * scale; };

    if (self.m_nNumKeys == 1) {
        for (int32 i = 0; i < self.m_nCount; i++) {
            outValues[i] = (float)KeyAt(i, 0);
        }
        return;
    }

    if (self.m_bLooped) {
        const double totalTime = (double)self.m_pTimes[self.m_nNumKeys - 1] * TIME_SCALE;
        const auto   numLoops  = Ftol((double)delta / totalTime);
        delta = (float)((double)delta - (double)numLoops * totalTime);
    }

    for (int32 k = 1; k < self.m_nNumKeys; k++) {
        const double t1 = (double)self.m_pTimes[k] * TIME_SCALE;
        if (delta < t1) {
            const double t0 = (double)self.m_pTimes[k - 1] * TIME_SCALE;
            const float  a  = (float)(((double)delta - t0) / (t1 - t0));
            for (int32 i = 0; i < self.m_nCount; i++) {
                const double k0 = KeyAt(i, k - 1);
                const double k1 = KeyAt(i, k);
                outValues[i]    = (float)((k1 - k0) * a + k0);
            }
            return;
        }
    }

    for (int32 i = 0; i < self.m_nCount; i++) {
        outValues[i] = (float)KeyAt(i, self.m_nNumKeys - 1);
    }
}
}; // namespace notsa::detail

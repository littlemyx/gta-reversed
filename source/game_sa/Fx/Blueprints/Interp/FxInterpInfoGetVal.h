#pragma once

#include "FxInterpInfo.h"
#include "FxFtol.h"

namespace notsa::detail {
/*!
 * Shared body of FxInterpInfo{Float,32,255,U255}_c::GetVal(float*, float)
 * (0x4A8470 / 0x4A89C0 / 0x4A8B80 / 0x4A8800). The four originals differ only in the key type and the
 * scale applied to the keys (key * scale is done on the x87 stack => x87d here (PC24, per-op float rounding in portable)).
 */
template<typename TKey>
void FxInterpInfoGetVal(const FxInterpInfo_c& self, TKey* const* keys, x87d scale, float* outValues, float delta) {
    constexpr x87d TIME_SCALE = (x87d)0.00390625; // 0x859AA0 (1/256)

    // The original reads the (int16 in the header) count as a signed byte (movsx r32, byte ptr [this+6])
    const int32 count = (int8)self.m_nCount;

    const auto KeyAt = [&](int32 i, int32 k) { return (x87d)keys[i][k] * scale; };

    if (self.m_nNumKeys == 1) {
        for (int32 i = 0; i < count; i++) {
            outValues[i] = (float)KeyAt(i, 0);
        }
        return;
    }

    if (self.m_bLooped) {
        const x87d totalTime = (x87d)self.m_pTimes[self.m_nNumKeys - 1] * TIME_SCALE;
        const auto   numLoops  = Ftol((x87d)delta / totalTime);
        delta = (float)((x87d)delta - (x87d)numLoops * totalTime);
    }

    for (int32 k = 1; k < self.m_nNumKeys; k++) {
        const x87d t1 = (x87d)self.m_pTimes[k] * TIME_SCALE;
        if (delta < t1) {
            const x87d t0 = (x87d)self.m_pTimes[k - 1] * TIME_SCALE;
            const float  a  = (float)(((x87d)delta - t0) / (t1 - t0));
            for (int32 i = 0; i < count; i++) {
                const x87d k0 = KeyAt(i, k - 1);
                const x87d k1 = KeyAt(i, k);
                outValues[i]    = (float)((k1 - k0) * a + k0);
            }
            return;
        }
    }

    for (int32 i = 0; i < count; i++) {
        outValues[i] = (float)KeyAt(i, self.m_nNumKeys - 1);
    }
}
}; // namespace notsa::detail

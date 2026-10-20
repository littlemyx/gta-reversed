// Optimised helpers of the exe-oracle harness (W2 / W3 targets). The oracle targets are built /Od /Ob0 (the port functions must keep the exe-shaped x87 code), which makes the harness's own
// per-case work dominate: the 4 MB data-image compare (CRT memcmp = a call per dword, then SSE intrinsics at /Od still ~1 byte / cycle) and FillGarbage of the 128 KB scratch block (~5 M cycles).
// This TU is compiled /O2 (see source/CMakeLists.txt: ORACLE_FAST_SOURCE) and holds only integer code plus float products that are exact before the single rounding to float, so the
// results are bit-identical to the /Od versions in wave1_oracle_common.inc / wave2_oracle_common.inc (checked at start-up: OracleFastSelfTest). Opt in with -DORACLE_FAST_HELPERS.
#include <cstdint>
#include <cstring>
#include <emmintrin.h>
#include "oracle_fast.h"

// the target is /Od /Ob0 /RTC1: switch the run-time checks off and optimise from here on (the options /O2 / /Ob2 cannot be given per source with /RTC1 on the command line)
#pragma runtime_checks("scu", off)
#pragma optimize("gt", on)
#pragma inline_depth(8)

namespace oraclefast {

bool BytesDiffer(const void* a, const void* b, size_t n) {
    const uint8_t* x = static_cast<const uint8_t*>(a); const uint8_t* y = static_cast<const uint8_t*>(b);
    for (; n >= 64; n -= 64, x += 64, y += 64) {
        const __m128i d0 = _mm_xor_si128(_mm_loadu_si128(reinterpret_cast<const __m128i*>(x)),      _mm_loadu_si128(reinterpret_cast<const __m128i*>(y)));
        const __m128i d1 = _mm_xor_si128(_mm_loadu_si128(reinterpret_cast<const __m128i*>(x + 16)), _mm_loadu_si128(reinterpret_cast<const __m128i*>(y + 16)));
        const __m128i d2 = _mm_xor_si128(_mm_loadu_si128(reinterpret_cast<const __m128i*>(x + 32)), _mm_loadu_si128(reinterpret_cast<const __m128i*>(y + 32)));
        const __m128i d3 = _mm_xor_si128(_mm_loadu_si128(reinterpret_cast<const __m128i*>(x + 48)), _mm_loadu_si128(reinterpret_cast<const __m128i*>(y + 48)));
        if (_mm_movemask_epi8(_mm_cmpeq_epi8(_mm_or_si128(_mm_or_si128(d0, d1), _mm_or_si128(d2, d3)), _mm_setzero_si128())) != 0xFFFF) return true;
    }
    for (; n; --n, ++x, ++y) if (*x != *y) return true;
    return false;
}


// the harness Rng (wave1_oracle_test.cpp) as macros: this TU is built /Ob0, so member functions would not be inlined. s = the xorshift state (uint64_t lvalue)
#define XS(s)      ((s) ^= (s) << 13, (s) ^= (s) >> 7, (s) ^= (s) << 17, (uint32_t)((s) >> 16))
#define F01(s)     ((float)(XS(s) & 0xFFFFFF) * (1.0f / 16777216.0f))
#define BELOW(s, n) ((int)(XS(s) % (uint32_t)(n)))
namespace {
enum : unsigned { S_NAN = 1, S_ZERO = 2, S_DEN = 4, S_HUGE = 8, S_INF = 16, S_TINY = 32 };
float BitsF(uint32_t b) { float f; std::memcpy(&f, &b, 4); return f; }
// = GenF of wave1_oracle_test.cpp (special: set when a special value is returned = g_special)
float GenF(uint64_t& s, int sp, bool& special, float scale, unsigned mask) {
    if (sp && BELOW(s, 100) < sp) {
        for (int tries = 0; tries < 8; ++tries) {
            const unsigned k = 1u << BELOW(s, 6);
            if (!(mask & k)) continue;
            const bool neg = BELOW(s, 2);
            float v = 0.f;
            switch (k) {
            case S_NAN:  v = BitsF(BELOW(s, 2) ? 0x7FC00000u : (0x7F800001u + (XS(s) & 0x3FFFFF))); break;
            case S_ZERO: v = 0.f; break;
            case S_DEN:  v = BitsF(1 + (XS(s) & 0x7FFFFF)); break;
            case S_HUGE: v = 1e30f * (1.0f + F01(s) * 2.0f); break;
            case S_INF:  v = BitsF(0x7F800000u); break;
            case S_TINY: v = 1e-20f * (1.0f + F01(s) * 9.0f); break;
            }
            special = true;
            return neg ? -v : v;
        }
    }
    return (F01(s) * 2.0f - 1.0f) * scale;
}
} // namespace

// = FillGarbage of wave1_oracle_common.inc (NoNaN inlined); returns true when a special value was produced (the caller sets g_special)
bool FillGarbage(uint64_t* state, int sp, void* p, size_t n) {
    uint64_t s = *state; bool special = false;
    auto* w = static_cast<uint32_t*>(p);
    for (size_t i = 0; i + 4 <= n; i += 4) {
        uint32_t v;
        switch (BELOW(s, 8)) {
        case 0: v = 0; break;
        case 1: v = 0x80000000u; break;
        case 2: { const float f = GenF(s, sp, special, 100.f, S_ZERO | S_DEN | S_TINY); std::memcpy(&v, &f, 4); break; }
        case 3: v = (uint32_t)BELOW(s, 256); break;
        case 4: { const float f = GenF(s, sp, special, 3.f, S_ZERO | S_DEN | S_TINY); std::memcpy(&v, &f, 4); break; }
        default: { v = XS(s) * 2654435761u; if ((v & 0x7F800000u) == 0x7F800000u) v &= ~0x40000000u; break; }
        }
        w[i / 4] = v;
    }
    *state = s;
    return special;
}

// ---- delta snapshot ----------------------------------------------------------------------------------------------------------------------------
static inline bool Block64Differs(const uint8_t* x, const uint8_t* y) {
    const __m128i d0 = _mm_xor_si128(_mm_loadu_si128(reinterpret_cast<const __m128i*>(x)),      _mm_loadu_si128(reinterpret_cast<const __m128i*>(y)));
    const __m128i d1 = _mm_xor_si128(_mm_loadu_si128(reinterpret_cast<const __m128i*>(x + 16)), _mm_loadu_si128(reinterpret_cast<const __m128i*>(y + 16)));
    const __m128i d2 = _mm_xor_si128(_mm_loadu_si128(reinterpret_cast<const __m128i*>(x + 32)), _mm_loadu_si128(reinterpret_cast<const __m128i*>(y + 32)));
    const __m128i d3 = _mm_xor_si128(_mm_loadu_si128(reinterpret_cast<const __m128i*>(x + 48)), _mm_loadu_si128(reinterpret_cast<const __m128i*>(y + 48)));
    return _mm_movemask_epi8(_mm_cmpeq_epi8(_mm_or_si128(_mm_or_si128(d0, d1), _mm_or_si128(d2, d3)), _mm_setzero_si128())) != 0xFFFF;
}
void Capture(const uint8_t* before, const uint8_t* now, size_t n, std::vector<Blk>& out) {
    out.clear();
    for (size_t o = 0; o < n; o += 64)
        if (Block64Differs(before + o, now + o)) { Blk b; b.off = (uint32_t)o; std::memcpy(b.d, now + o, 64); out.push_back(b); }
}
void Restore(uint8_t* now, const uint8_t* before, const std::vector<Blk>& chg) {
    for (const Blk& b : chg) std::memcpy(now + b.off, before + b.off, 64);
}
bool Equal(const uint8_t* before, const uint8_t* now, size_t n, const std::vector<Blk>& chg) {
    size_t pos = 0;   // everything below pos has been checked
    for (const Blk& b : chg) {
        if (b.off > pos && BytesDiffer(before + pos, now + pos, b.off - pos)) return false;   // untouched by the first side: must be untouched by the second
        if (BytesDiffer(b.d, now + b.off, 64)) return false;                                  // touched: the same bytes
        pos = (size_t)b.off + 64;
    }
    return pos >= n || !BytesDiffer(before + pos, now + pos, n - pos);
}
void Apply(uint8_t* buf, const std::vector<Blk>& chg) {
    for (const Blk& b : chg) std::memcpy(buf + b.off, b.d, 64);
}

} // namespace oraclefast

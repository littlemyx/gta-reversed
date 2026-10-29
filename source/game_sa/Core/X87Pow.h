#pragma once
// x87::pow(x, y): the exe's CRT pow (_CIpow 0x822130 / cdecl pow 0x8220F0), bit-exact. Included at the end of X87Intrinsics.h.
//
// _CIpow (arguments x = ST1, y = ST0) is the MSVC "libm" entry: when __sse2_available (0xC9C400, set by the CRT start-up on every SSE2 machine) is set and MXCSR + the x87 control word have all
// exceptions masked (the game: CW = 0x7F after D3D9 CreateDevice, MXCSR default), it jumps to 0x829B80 -> 0x829B99, the table driven SSE2 kernel below; only a CPU without SSE2 or a thread with
// unmasked exceptions takes the old x87 path (fyl2x / f2xm1, 0x82218D) - not modelled. The kernel works on x = m * 2^e: log2(m) from three reciprocal tables (T1/T2 8 bit, T3/T4 and T5/T6 9 bit)
// plus a short polynomial correction, v = y * log2(x) is split into hi + lo, k = round(128 * v), and 2^v = 2^(k>>7) * E2[k & 127] * (1 + p(r)) from the 2^(j/128) table E2 and the polynomial A..D.
// Every operation is a plain IEEE double operation of SSE2 (NOT x87: precision control is irrelevant), so the portable build reproduces it with plain doubles (-ffp-contract=off) and the Win32
// reference (/arch:IA32, x87 codegen) with SSE2 intrinsics. The port follows the machine code instruction by instruction (labels are the exe's addresses); the special cases (x = 0 / denormal /
// negative / inf / nan, y = 0 / inf / nan, over- and underflow) return what the kernel returns BEFORE the CRT's matherr hook (errno / _matherr side effects are omitted, like x87::asin).
// tests/standalone/x87_oracle_prims.inc: "x87::pow vs the exe's _CIpow" rows; tools/standalone/gen_x87_pow_tables.py regenerates X87PowTables.inc from the exe.
#include <bit>
#include <cmath>
#include <cstdint>
#if NOTSA_X87_ASM
#include <emmintrin.h>
#endif
#if defined(_MSC_VER) && !defined(__clang__)
#define NOTSA_X87POW_NOINLINE __declspec(noinline)
#else
#define NOTSA_X87POW_NOINLINE __attribute__((noinline))
#endif
#if defined(__clang__)
#pragma clang fp contract(off)
#endif

namespace x87 {
namespace powdetail {
#include "X87PowTables.inc"

using u64 = uint64_t;
using u32 = uint32_t;
using i32 = int32_t;
inline u64    Bits(double d) { return std::bit_cast<u64>(d); }
inline double F(u64 b) { return std::bit_cast<double>(b); }
#if NOTSA_X87_ASM
// /arch:IA32: plain double arithmetic would be x87 at the game's PC24 - force the SSE2 scalar instructions
inline double Add(double a, double b) { return _mm_cvtsd_f64(_mm_add_sd(_mm_set_sd(a), _mm_set_sd(b))); }
inline double Sub(double a, double b) { return _mm_cvtsd_f64(_mm_sub_sd(_mm_set_sd(a), _mm_set_sd(b))); }
inline double Mul(double a, double b) { return _mm_cvtsd_f64(_mm_mul_sd(_mm_set_sd(a), _mm_set_sd(b))); }
inline i32    CvtSd2Si(double a) { return _mm_cvtsd_si32(_mm_set_sd(a)); }
#else
inline double Add(double a, double b) { return a + b; }
inline double Sub(double a, double b) { return a - b; }
inline double Mul(double a, double b) { return a * b; }
inline i32    CvtSd2Si(double a) { return (a > -2147483649.0 && a < 2147483648.0) ? (i32)std::nearbyint(a) : INT32_MIN; }   // cvtsd2si, round to nearest even
#endif
// SSE2 addsd with a NaN operand: the first NaN operand (quieted) is the result
inline double AddNaN(double a, double b) {
    if (a != a) return F(Bits(a) | 0x0008000000000000ull);
    if (b != b) return F(Bits(b) | 0x0008000000000000ull);
    return Add(a, b);
}
inline u64 Psllq(u64 v, u32 count) { return count > 63 ? 0 : v << count; }

inline constexpr u64 kMant = 0x000FFFFFFFFFFFFFull, kOne = 0x3FF0000000000000ull, kAbs = 0x7FFFFFFFFFFFFFFFull;
inline constexpr u64 kHiMask = 0xFFFFFFFFF8000000ull;                 // 0x88BDD8: top 37 bits
inline constexpr u64 kNegLog2e = 0xBFF7154740000000ull;               // 0x88BDE0: -1.442694902420044
inline constexpr u64 kMagic = 0x4338000000000000ull;                  // 0x88BD70: 1.5 * 2^52 (round to integer)
inline constexpr u64 kLn2Hi = 0x3FE62E42FEFA39EFull;                  // 0x88BDD0
inline constexpr u64 kPosInf = 0x7FF0000000000000ull, kNegInf = 0xFFF0000000000000ull, kNegZero = 0x8000000000000000ull, kDefaultNaN = 0xFFF8000000000000ull;

NOTSA_X87POW_NOINLINE inline double Pow(double x, double y) {
    const u64 xb = Bits(x), yb = Bits(y);
    u64 x0b;
    double x0, x1, x2, x3, x4, x5, x6, x6h, x7, n;
    u32 eax, ecx, edx, esi, edi;
    bool intZero = false, fromSpecialX = false;

    x0b = xb >> 44;
    eax = (u32)(x0b & 0xffff) & 0xff;
    x7 = F((xb & kMant) | kOne);
    x4 = x;
    ecx = (u32)(xb >> 48);
    eax = (eax + 1) & 0x1fe;
    x5 = F(kPowT1[eax >> 1]);
    x7 = Mul(x7, x5);
    x6 = F(kPowT2[eax]); x6h = F(kPowT2[eax + 1]);
    edx = 0x7fef - ecx;
    ecx = (ecx - 0x10) | edx;
    if (ecx >= 0x80000000u) goto L829ee7;
    ecx = 0;
    edx = 0x3fe7f;
L829c16:
    n = (double)(i32)(u32)((x0b - (u64)edx) >> 8);
    x1 = F(kHiMask);
    x3 = x7;
    eax = (u32)((Bits(x7) >> 38) & 0xffff) & 0xff;
    x4 = F(Bits(x4) & kMant);
    eax = (eax + 1) & 0x1fe;
    x3 = Mul(x3, F(kPowT3[eax >> 1]));
    x5 = Mul(x5, F(kPowT3[eax >> 1]));
    x6 = Add(x6, F(kPowT4[eax])); x6h = Add(x6h, F(kPowT4[eax + 1]));
    x4 = F(Bits(x4) | kOne);
    x6 = Add(x6, n);
    x1 = F(Bits(x1) & Bits(x4));
    x2 = x3;
    eax = (u32)((Bits(x3) >> 31) & 0xffff);
    x0 = F(kHiMask);
    x4 = Sub(x4, x1);
    x7 = F(kNegLog2e);
    eax = ((eax & 0x1ff) + 1) & 0x3fe;
    x5 = Mul(x5, F(kPowT5[eax >> 1]));
    x2 = Mul(x2, F(kPowT5[eax >> 1]));
    x6 = Add(x6, F(kPowT6[eax])); x6h = Add(x6h, F(kPowT6[eax + 1]));
    x0 = F(Bits(x0) & Bits(x5));
    x5 = Sub(x5, x0);
    x7 = Add(x7, x2);
    x3 = x0;
    x0 = Mul(x0, x1);
    x1 = Mul(x1, x5);
    x3 = Mul(x3, x4);
    x2 = Sub(x2, x0);
    x4 = Mul(x4, x5);
    x0 = x6;
    x2 = Sub(x2, x1);
    x6 = Add(x6, x7);
    x1 = y;
    eax = (u32)(yb >> 48);
    x2 = Sub(x2, x3);
    x0 = Sub(x0, x6);
    x3 = F(kHiMask);
    edx = (u32)(Bits(x6) >> 48);
    x2 = Sub(x2, x4);
    x4 = x6;
    x0 = Add(x0, x7);
    x7 = Sub(x7, x2);
    x6 = Sub(x6, x2);
    eax &= 0x7ff0;
    if (eax >= 0x7ff0) goto L82a258;
    edx &= 0x7ff0;
    eax = eax - 0x3ff0 + edx;
    edx = 0x40a0 - eax;
    eax -= 0x3c70;
    edx |= eax;
    if (edx >= 0x80000000u) goto L82a641;
L829d68:
    x4 = Sub(x4, x6);
    x2 = Sub(x2, x4);
    x4 = F(kHiMask);
    x3 = F(Bits(x3) & yb);
    x4 = F(Bits(x4) & Bits(x6));
    x0 = Sub(x0, x2);
    x2 = F(0x4060000000000000ull);                                    // 128.0
    x5 = x3;
    x3 = Mul(x3, x4);
    x6 = Sub(x6, x4);
    x1 = Sub(x1, x5);
    x3 = Mul(x3, x2);
    x5 = Mul(x5, x6);
    x4 = Mul(x4, x1);
    eax = (u32)CvtSd2Si(x3);
    x6 = Mul(x6, x1);
    x5 = Add(x5, x4);
    x4 = x6h;
    x5 = Add(x5, x6);
    edx = 0x1ff7f - eax;
    eax += 0x1e1ff;
    edx |= eax;
    eax -= 0x1e1ff;
    if ((i32)edx <= 0) goto L82a3d1;
    {
        ecx += eax;
        eax &= 0x7f;
        ecx &= 0xffffff80u;
        ecx += 0x1ff80;
        x4 = Add(x4, x0);
        x0 = x3;
        x3 = Add(x3, F(kMagic));
        double a0 = Mul(F(kPowA[0]), x7), a1 = Mul(F(kPowA[1]), x7);
        x3 = Sub(x3, F(kMagic));
        const double d2 = Mul(x7, x7);
        x0 = Sub(x0, x3);
        double e0 = F(kPowE2[2 * eax]), e1 = F(kPowE2[2 * eax + 1]);
        double c0 = F(kPowC[0]), c1 = F(kPowC[1]);
        a0 = Add(a0, Mul(F(kPowB[0]), d2)); a1 = Add(a1, Mul(F(kPowB[1]), d2));
        a0 = Mul(a0, d2);
        a0 = Add(a0, a1);
        a0 = Add(a0, x4);
        x4 = F(kLn2Hi);
        const double S = F((u64)ecx << 45);
        a0 = Mul(a0, y);
        x0 = Mul(x0, F(0x3F80000000000000ull));                       // 2^-7
        double d0 = F(kPowD[0]), d1 = F(kPowD[1]);
        x5 = Add(x5, a0);
        e0 = Mul(e0, S); e1 = Mul(e1, S);
        x0 = Add(x0, x5);
        c0 = Mul(c0, x0); c1 = Mul(c1, x0);
        x4 = Mul(x4, x0);
        const double q = Mul(x0, x0);
        d0 = Add(d0, c0); d1 = Add(d1, c1);
        d0 = Mul(d0, q); d1 = Mul(d1, q);
        x0 = Mul(q, e0);
        double t6 = d1;
        x0 = Mul(x0, d0);
        t6 = Mul(t6, e0);
        x4 = Mul(x4, e0);
        x0 = Add(x0, e1);
        x0 = Add(x0, t6);
        x0 = Add(x0, x4);
        x0 = Add(x0, e0);
        return x0;
    }

L82a3d1:                                                              // |result| outside the normal range, or too close to it
    if ((i32)eax <= 0) goto L82a3fe;
    if (eax >= 0x40000) goto L82a61d;
    edx = eax; eax &= 0x7f; ecx += 0x3ff00; edx -= 0x80; edx &= 0xffffff80u; edi = 0x3ff0;
    goto L82a424;
L82a3fe:
    if ((i32)eax <= (i32)0xfffc0200) goto L82a5fa;
    edx = eax; eax &= 0x7f; ecx += 0x80; edx &= 0xffffff80u; edx += 0x3fe80; edi = 0;
L82a424:
    {
        x4 = Add(x4, x0);
        x0 = x3;
        x3 = Add(x3, F(kMagic));
        esi = edx - 0x1ff80;
        double a0 = Mul(F(kPowA[0]), x7), a1 = Mul(F(kPowA[1]), x7);
        x3 = Sub(x3, F(kMagic));
        const double d2 = Mul(x7, x7);
        x0 = Sub(x0, x3);
        double e0 = F(kPowE2[2 * eax]), e1 = F(kPowE2[2 * eax + 1]);
        double c0 = F(kPowC[0]), c1 = F(kPowC[1]);
        a0 = Add(a0, Mul(F(kPowB[0]), d2)); a1 = Add(a1, Mul(F(kPowB[1]), d2));
        a0 = Mul(a0, d2);
        a0 = Add(a0, a1);
        a0 = Add(a0, x4);
        x4 = F(kLn2Hi);
        const double S = F((u64)edx << 45);
        edx = (u32)(-(i32)(edx - 0x1ff80));
        edx = (u32)((i32)edx >> 7);
        edx += 2;
        eax = edx & 0x20;
        edx += eax;
        a0 = Mul(a0, y);
        x0 = Mul(x0, F(0x3F80000000000000ull));
        double d0 = F(kPowD[0]), d1 = F(kPowD[1]);
        x5 = Add(x5, a0);
        e0 = Mul(e0, S); e1 = Mul(e1, S);
        x0 = Add(x0, x5);
        c0 = Mul(c0, x0); c1 = Mul(c1, x0);
        x4 = Mul(x4, x0);
        const double q = Mul(x0, x0);
        d0 = Add(d0, c0); d1 = Add(d1, c1);
        d0 = Mul(d0, q); d1 = Mul(d1, q);
        x0 = Mul(q, e0);
        double t6 = d1;
        x0 = Mul(x0, d0);
        t6 = Mul(t6, e0);
        x4 = Mul(x4, e0);
        x7 = F((u64)ecx << 45);
        const u64 mask = Psllq(~0ull, edx);
        x0 = Add(x0, e1);
        x0 = Add(x0, t6);
        x1 = F(mask & Bits(e0));
        x0 = Add(x0, x4);
        const double x3b = Sub(e0, x1);
        x2 = x1;
        x1 = Add(x1, x0);
        x1 = F(Bits(x1) & mask);
        x6 = F((u64)(edi & 0xffff) << 48);
        x2 = Sub(x2, x1);
        x0 = Add(x0, x2);
        x0 = Add(x0, x3b);
        if ((i32)esi > 0) {
            x0 = Add(x0, x1);
            x0 = Mul(x0, x7);
            x6 = Mul(x6, x0);
            x0 = Add(x0, x6);
            return x0;
        }
        x0 = Mul(x0, x7);
        x1 = Mul(x1, x7);
        x0 = Add(x0, x1);
        x6 = Mul(x6, x0);
        x0 = Add(x0, x6);
        return x0;
    }
L82a5fa:                                                              // underflow: 0 with the sign of the odd-integer flag
    x0 = Mul(F(0x0010000000000000ull), F(0x0010000000000000ull));
    return F(Bits(x0) | ((u64)ecx << 45));
L82a61d:                                                              // overflow
    return F(ecx == 0 ? kPosInf : kNegInf);
L82a641:                                                              // y * log2(x) may be out of range: recheck with the products
    {
        const double p = Mul(y, x6);
        eax = (u32)(Bits(p) >> 48) & 0x7ff0;
        edx = 0x40a0 - eax;
        eax -= 0x3c70;
        edx |= eax;
        if (edx < 0x80000000u) { x4 = p; goto L829d68; }              // the kernel continues with xmm4 = y * L here
        if (eax >= 0x80000000u) { ecx |= 0x1ff80; return F((u64)ecx << 45); }   // |y log2 x| tiny: +-1
        edx = ((u32)(xb >> 48) & 0x7ff0) - 0x3ff0;
        eax = ((u32)(yb >> 48) ^ edx) & 0x8000;
        if (eax != 0) goto L82a5fa;
        goto L82a61d;
    }

// ---- special operands ----
L82a258:                                                              // y = inf / nan, x positive normal
    if ((u32)xb != 0) goto L82a2ac;
    edx = (u32)(xb >> 32);
    if (edx == 0x3ff00000u) goto L82a38a;
    goto L82a2ac;
L82a2ac:
    if ((yb & kMant) != 0) goto L82a325;                              // y = nan
    if (fromSpecialX && (((u32)xb) | (((u32)(xb >> 32)) ^ 0xbff00000u)) == 0) goto L82a38a;   // x = -1
    if ((yb >> 48 & 0x8000) == 0) {                                   // y = +inf: |x| < 1 -> 0, else inf
        return ((u32)(xb >> 48) & 0x7ff0) >= 0x3ff0 ? F(kPosInf) : 0.0;
    }
    return ((u32)(xb >> 48) & 0x7ff0) < 0x3ff0 ? F(kPosInf) : 0.0;     // y = -inf
L82a325:
    return AddNaN(y, y);
L82a38a:
    return AddNaN(y, F(kDefaultNaN));
L82a1ff:
    return AddNaN(x, x);

L829ee7:                                                              // x <= 0, denormal, inf, nan
    {
        const u64 ay = yb & kAbs;
        eax = (u32)yb;
        ecx = (u32)(ay >> 32);
        if (ecx >= 0x7ff00000u) goto L829ff0;
        eax |= ecx;
        if (eax == 0) goto L82a334;
        if ((i32)edx >= 0) goto L82a02f;
        edx = (u32)(-(i32)edx) + 0x7fef;                              // the high word of x
        {
            const i32 s = (i32)(ay >> 52) - 0x3f3;
            intZero = Psllq(yb | 0xFFF0000000000000ull, s < 0 ? 0u : (u32)s) == 0;   // y has no fraction bits
        }
        ecx = edx;
        edx &= 0x7fff;
        if (edx >= 0x7ff0) goto L82a118;
        if (!intZero) goto L82a211;
        {
            const u32 s2 = (u32)((i32)(ay >> 52) - 0x3f4);
            ecx = Psllq(yb, s2) == kNegZero ? 0x40000u : 0u;          // odd integer: the sign flag of the result
        }
        if (edx < 0x10) goto L82a034;
        edx = 0xbfe7f;
        goto L829c16;
    }
L82a02f:
    ecx = 0;
L82a034:
    x0 = Mul(F(0x43F0000000000000ull), x4);                           // x * 2^64
    edx = (u32)Bits(x4);
    eax = (u32)(Bits(x4) >> 32);
    if (edx == 0) goto L82a0ba;
L82a068:
    x7 = F(Bits(x0) & kMant);
    x4 = x0;
    x0b = (Bits(x0) & kAbs) >> 44;
    eax = (u32)(x0b & 0xffff) & 0xff;
    x7 = F(Bits(x7) | kOne);
    eax = (eax + 1) & 0x1fe;
    x5 = F(kPowT1[eax >> 1]);
    x7 = Mul(x7, x5);
    x6 = F(kPowT2[eax]); x6h = F(kPowT2[eax + 1]);
    edx = 0x43e7f;
    goto L829c16;
L82a0ba:                                                              // x = +-0 (or a denormal when the scaled high word is not 0)
    edx = eax & 0x7fffffffu;
    if (edx != 0) goto L82a068;
    if ((u32)(yb >> 32) & 0x80000000u) {                              // y < 0: +-inf (divide by zero), the sign if x = -0 and y odd
        ecx <<= 13; eax &= ecx;
        return F((u64)(0x7ff00000u | eax) << 32);
    }
    ecx <<= 13; eax &= ecx;
    return eax != 0 ? F(kNegZero) : 0.0;
L82a118:                                                              // x = +-inf / nan, y finite non-zero
    if ((xb & kMant) != 0) goto L82a1ff;
    if (((u32)(xb >> 48) & 0x8000) == 0) goto L82a1e3;
    if (!intZero) goto L82a1c7;
    if (Psllq(yb, (u32)((i32)((yb & kAbs) >> 52) - 0x3f4)) == 0) goto L82a1c7;   // even integer
    return (yb >> 48 & 0x8000) == 0 ? F(kNegInf) : F(kNegZero);      // x = -inf, y odd integer
L82a1c7:
L82a1e3:
    return (yb >> 48 & 0x8000) == 0 ? F(kPosInf) : 0.0;
L82a211:                                                              // negative x, non-integer y
    if ((xb & kAbs) == 0) { eax = 0; ecx = 0; goto L82a0ba; }
    return F(kDefaultNaN);
L829ff0:                                                              // y = inf / nan
    {
        edx = (u32)xb; eax = (u32)(xb >> 32); ecx = eax; eax &= 0x7fffffffu;
        fromSpecialX = true;
        if (eax < 0x7ff00000u) goto L82a2ac;
        if (eax > 0x7ff00000u) goto L82a1ff;
        if (edx != 0) goto L82a1ff;
        goto L82a2ac;
    }
L82a334:                                                              // y = +-0
    return 1.0;
}
} // namespace powdetail

//! pow(x, y) of the exe's CRT (_CIpow 0x822130): the SSE2 kernel, result a plain double (the exe leaves it in ST0 as an exact double)
inline double pow(double x, double y) { return powdetail::Pow(x, y); }
} // namespace x87

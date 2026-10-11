#pragma once
// X87Soft.h - header-only, INTEGER-ONLY software model of the x87 instructions the game executes (fadd/fsub/fmul/fdiv/fsqrt at a
// precision control of 24/53/64 bits, fsin/fcos/fsincos/fptan/fpatan/fyl2x/fyl2xp1), for portable (ARM / x64 / clang) builds.
// No <cmath>, no host floating-point operation anywhere (a float/double is only ever re-interpreted as bits), no __int128, no
// _umul128: it compiles unchanged with MSVC x86, clang arm64 and clang x64 and does not depend on the host FP mode.
//
// The numbers it reproduces are those of the x87 of the reference machine (Wine / Rosetta 2 on Apple silicon, which emulates
// the Intel x87 including its 66-bit pi in the fsin/fcos/fptan argument reduction) - see .notes/reports/F1_soft.md for the measured
// behaviour, the envelope of what is bit-exact and what is not.
//
// Conventions
//  * F80 is the 80-bit register format: full 64-bit significand with the explicit integer bit, biased 15-bit exponent
//    (0 = zero / denormal, 0x7FFF = inf / NaN), sign. Operations never produce unnormals / pseudo-infinities; such INPUTS are an invalid
//    operation (real indefinite), like on the FPU. Pseudo-denormals (exp 0 with the integer bit set) are valid and behave as exp 1.
//  * Exceptions are masked (the game's CW): results follow the masked-response tables. Exception flags are not tracked, except the
//    C2 "operand out of range" outcome of fsin/fcos/fptan/fsincos (|x| >= 2^63) which is reported through a bool.
//  * Rounding is to nearest-even. The precision control only affects add/sub/mul/div/sqrt (as on the FPU). The exponent range is
//    always the extended one; masked overflow gives inf, underflow gives a denormal rounded at the coarser of (PC bits, denormal grid).
//  * fsin / fcos / fptan / fsincos / fpatan / fyl2x / fyl2xp1 return the result rounded to 64 bits whatever the PC is (like the
//    FPU: the caller's NEXT arithmetic op or the store rounds it).
//  * NaNs: a NaN operand is propagated (SNaN is quieted), two NaNs -> the one with the larger significand; an invalid operation
//    produces the "real indefinite" = negative quiet NaN with a zero payload (sign 1, exp 0x7FFF, mant 0xC000000000000000).
#include <bit>
#include <cstdint>

namespace notsa::fp::soft {

using u8  = std::uint8_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

// ----------------------------------------------------------------------------------------------------------------------------
// Portable 128-bit integer helpers (no __int128 / _umul128)
// ----------------------------------------------------------------------------------------------------------------------------
namespace detail {

struct U128 { u64 hi, lo; };

constexpr bool IsZero(U128 a) { return (a.hi | a.lo) == 0; }
constexpr bool Eq(U128 a, U128 b) { return a.hi == b.hi && a.lo == b.lo; }
constexpr bool Lt(U128 a, U128 b) { return a.hi < b.hi || (a.hi == b.hi && a.lo < b.lo); }
constexpr bool Ge(U128 a, U128 b) { return !Lt(a, b); }
constexpr U128 Add(U128 a, U128 b) { U128 r{ a.hi + b.hi, a.lo + b.lo }; if (r.lo < a.lo) ++r.hi; return r; }
constexpr U128 Sub(U128 a, U128 b) { U128 r{ a.hi - b.hi, a.lo - b.lo }; if (a.lo < b.lo) --r.hi; return r; }
constexpr U128 Neg(U128 a) { return Sub(U128{ 0, 0 }, a); }
constexpr U128 Not(U128 a) { return U128{ ~a.hi, ~a.lo }; }
// shifts: n in [0, 127] is exact, n >= 128 gives 0
constexpr U128 Shl(U128 a, unsigned n) {
    if (n == 0) return a;
    if (n >= 128) return U128{ 0, 0 };
    if (n >= 64) return U128{ a.lo << (n - 64), 0 };
    return U128{ (a.hi << n) | (a.lo >> (64 - n)), a.lo << n };
}
constexpr U128 Shr(U128 a, unsigned n) {
    if (n == 0) return a;
    if (n >= 128) return U128{ 0, 0 };
    if (n >= 64) return U128{ 0, a.hi >> (n - 64) };
    return U128{ a.hi >> n, (a.lo >> n) | (a.hi << (64 - n)) };
}
// true if any of the n low bits is set
constexpr bool LowBitsSet(U128 a, unsigned n) {
    if (n == 0) return false;
    if (n >= 128) return !IsZero(a);
    if (n >= 64) return a.lo != 0 || (a.hi & ((n == 64) ? 0 : ((u64(1) << (n - 64)) - 1))) != 0;
    return (a.lo & ((u64(1) << n) - 1)) != 0;
}
constexpr int Clz(U128 a) { return a.hi ? std::countl_zero(a.hi) : 64 + std::countl_zero(a.lo); }

// 64 x 64 -> 128
constexpr U128 Mul64(u64 a, u64 b) {
    const u64 M = 0xFFFFFFFFull;
    const u64 a0 = a & M, a1 = a >> 32, b0 = b & M, b1 = b >> 32;
    const u64 p00 = a0 * b0, p01 = a0 * b1, p10 = a1 * b0, p11 = a1 * b1;
    const u64 mid = (p00 >> 32) + (p01 & M) + (p10 & M);
    return U128{ p11 + (p01 >> 32) + (p10 >> 32) + (mid >> 32), (p00 & M) | (mid << 32) };
}
// top 128 bits of the 256-bit product a*b (truncated; error < 2 units of the last place)
constexpr U128 MulHi(U128 a, U128 b) {
    const U128 hh = Mul64(a.hi, b.hi), hl = Mul64(a.hi, b.lo), lh = Mul64(a.lo, b.hi);
    const u64 ll = Mul64(a.lo, b.lo).hi;
    u64 s = hl.lo + lh.lo;
    u64 c = (s < hl.lo) ? 1 : 0;
    const u64 s2 = s + ll;
    c += (s2 < s) ? 1 : 0;
    U128 r = Add(hh, U128{ 0, hl.hi });
    r = Add(r, U128{ 0, lh.hi });
    r = Add(r, U128{ 0, c });
    return r;
}

// Knuth algorithm D: u[0..m-1] / v[0..n-1] (little-endian 32-bit words, v[n-1] != 0, n >= 2, m >= n).
// q gets m-n+1 words, r (optional) n words.
inline void DivModWords(const u32* u, int m, const u32* v, int n, u32* q, u32* r) {
    u32 vn[8] = {}, un[16] = {};
    const int s = std::countl_zero(v[n - 1]);
    for (int i = n - 1; i > 0; --i) vn[i] = s ? (v[i] << s) | (v[i - 1] >> (32 - s)) : v[i];
    vn[0] = v[0] << s;
    un[m] = s ? (u[m - 1] >> (32 - s)) : 0;
    for (int i = m - 1; i > 0; --i) un[i] = s ? (u[i] << s) | (u[i - 1] >> (32 - s)) : u[i];
    un[0] = u[0] << s;
    const u64 b = u64(1) << 32;
    for (int j = m - n; j >= 0; --j) {
        const u64 num = (u64(un[j + n]) << 32) | un[j + n - 1];
        u64 qhat = num / vn[n - 1];
        u64 rhat = num - qhat * vn[n - 1];
        while (qhat >= b || qhat * vn[n - 2] > ((rhat << 32) | un[j + n - 2])) {
            --qhat; rhat += vn[n - 1];
            if (rhat >= b) break;
        }
        i64 borrow = 0;
        i64 t;
        for (int i = 0; i < n; ++i) {
            const u64 p = qhat * vn[i];
            t = i64(un[i + j]) - borrow - i64(p & 0xFFFFFFFFull);
            un[i + j] = u32(t);
            borrow = i64(p >> 32) - (t >> 32);
        }
        t = i64(un[j + n]) - borrow;
        un[j + n] = u32(t);
        q[j] = u32(qhat);
        if (t < 0) {
            --q[j];
            u64 k = 0;
            for (int i = 0; i < n; ++i) {
                const u64 t2 = u64(un[i + j]) + vn[i] + k;
                un[i + j] = u32(t2);
                k = t2 >> 32;
            }
            un[j + n] = u32(u64(un[j + n]) + k);
        }
    }
    if (r) {
        for (int i = 0; i < n; ++i) r[i] = s ? (un[i] >> s) | (u32(u64(un[i + 1]) << (32 - s))) : un[i];
    }
}

} // namespace detail

// ----------------------------------------------------------------------------------------------------------------------------
// F80
// ----------------------------------------------------------------------------------------------------------------------------
struct F80 {
    u64  mant = 0;      // bit 63 = explicit integer bit
    i32  exp  = 0;      // biased exponent field, 0..0x7FFF
    bool sign = false;

    constexpr bool operator==(const F80&) const = default;
};

enum class Class { Zero, Denormal, Normal, Inf, QNaN, SNaN, Unsupported };

constexpr Class Classify(const F80& a) {
    if (a.exp == 0x7FFF) {
        if (!(a.mant >> 63)) return Class::Unsupported;                    // pseudo-NaN / pseudo-infinity
        const u64 frac = a.mant << 1;
        if (frac == 0) return Class::Inf;
        return (a.mant >> 62) & 1 ? Class::QNaN : Class::SNaN;
    }
    if (a.exp == 0) {
        if (a.mant == 0) return Class::Zero;
        return (a.mant >> 63) ? Class::Normal : Class::Denormal;            // exp 0 + integer bit = pseudo-denormal = valid (as exp 1)
    }
    return (a.mant >> 63) ? Class::Normal : Class::Unsupported;             // unnormal
}
constexpr bool IsNaN(const F80& a) { const Class c = Classify(a); return c == Class::QNaN || c == Class::SNaN; }
constexpr bool IsInf(const F80& a) { return Classify(a) == Class::Inf; }
constexpr bool IsZero(const F80& a) { return Classify(a) == Class::Zero; }

constexpr F80 Zero(bool sign = false)     { return F80{ 0, 0, sign }; }
constexpr F80 Inf(bool sign = false)      { return F80{ 0x8000000000000000ull, 0x7FFF, sign }; }
constexpr F80 Indefinite()                { return F80{ 0xC000000000000000ull, 0x7FFF, true }; }   // "real indefinite": the default NaN
constexpr F80 One(bool sign = false)      { return F80{ 0x8000000000000000ull, 0x3FFF, sign }; }
constexpr F80 Neg(F80 a)                  { a.sign = !a.sign; return a; }
constexpr F80 Abs(F80 a)                  { a.sign = false; return a; }

// Constants as loaded by fldpi / fldlg2 / fldln2 / fldl2e / fldl2t / fldz / fld1 (round to nearest)
constexpr F80 kPi    { 0xC90FDAA22168C235ull, 0x4000, false };
constexpr F80 kHalfPi{ 0xC90FDAA22168C235ull, 0x3FFF, false };
constexpr F80 kLg2   { 0x9A209A84FBCFF799ull, 0x3FFD, false };
constexpr F80 kLn2   { 0xB17217F7D1CF79ACull, 0x3FFE, false };
constexpr F80 kL2e   { 0xB8AA3B295C17F0BCull, 0x3FFF, false };
constexpr F80 kL2t   { 0xD49A784BCD1B8AFEull, 0x4000, false };

// 10-byte little-endian image (what fstp tbyte ptr stores)
inline F80 FromBytes(const u8 b[10]) {
    u64 m = 0;
    for (int i = 7; i >= 0; --i) m = (m << 8) | b[i];
    const unsigned se = unsigned(b[8]) | (unsigned(b[9]) << 8);
    return F80{ m, i32(se & 0x7FFF), (se >> 15) != 0 };
}
inline void ToBytes(const F80& a, u8 b[10]) {
    for (int i = 0; i < 8; ++i) b[i] = u8(a.mant >> (8 * i));
    const unsigned se = unsigned(a.exp & 0x7FFF) | (a.sign ? 0x8000u : 0u);
    b[8] = u8(se); b[9] = u8(se >> 8);
}

// ----------------------------------------------------------------------------------------------------------------------------
// Rounding core. A value is (sign, m * 2^(e - 127)) with m 128-bit, bit 127 set (normalized), plus a sticky flag meaning "the true value
// is a little larger in magnitude than m" (non-zero bits below m).
// ----------------------------------------------------------------------------------------------------------------------------
namespace detail {

enum class RMode { Nearest, Odd };

struct Rounded {
    u64  q;        // significand, value = q * 2^lsbExp  (q == 0: zero)
    i32  lsbExp;
    bool inexact;
};

// Round to p significant bits (p <= 64), but never finer than 2^gridExp (the denormal grid of the target format).
inline Rounded RoundMag(i32 e, U128 m, bool sticky, int p, i32 gridExp, RMode mode) {
    i32 lsb = e - (p - 1);
    int k = p;
    if (lsb < gridExp) {                       // denormal territory: fewer bits fit
        k = int(e - gridExp + 1);
        lsb = gridExp;
        if (k < -1) k = -1;                    // everything below half of the smallest denormal -> 0 (flush through k<0 path)
    }
    u64 q;
    bool roundBit, rest;
    if (k >= 1) {
        q = Shr(m, unsigned(128 - k)).lo;
        roundBit = (Shr(m, unsigned(127 - k)).lo & 1) != 0;
        rest = sticky || LowBitsSet(m, unsigned(127 - k));
    } else if (k == 0) {                       // value in [0.5, 1) units of the grid: the round bit is the (always set) top bit
        q = 0;
        roundBit = true;
        rest = sticky || LowBitsSet(m, 127);
    } else {                                   // value < 0.5 units
        q = 0; roundBit = false; rest = true;
    }
    Rounded r{ q, lsb, roundBit || rest };
    if (mode == RMode::Odd) {
        if (r.inexact) r.q |= 1;
        return r;
    }
    if (roundBit && (rest || (q & 1))) {
        if (k == 64 && q == ~u64(0)) { r.q = u64(1) << 63; r.lsbExp += 1; }                  // carry out of the 64-bit significand
        else {
            r.q = q + 1;
            if (k == p && k < 64 && (r.q >> p)) { r.q >>= 1; r.lsbExp += 1; }                 // carry into a new binade
        }
    }
    return r;
}

struct Unr { bool sign; i32 e; U128 m; bool sticky; };   // normalized: m bit 127 set

inline F80 PackF80(bool sign, const Rounded& r) {
    if (r.q == 0) return Zero(sign);
    const int n = 64 - std::countl_zero(r.q);
    const i32 E = r.lsbExp + n - 1;
    if (E > 16383) return Inf(sign);
    if (E < -16382) {                                           // denormal: mantissa = q in units of 2^-16445
        const u64 mant = r.q << (r.lsbExp + 16445);
        return F80{ mant, (mant >> 63) ? 1 : 0, sign };
    }
    return F80{ r.q << (64 - n), E + 16383, sign };
}

// For a denormal result the FPU rounds at the FIXED bit position of the PC (the 64-bit significand register is rounded at bit 64-pc), so the
// denormal grid is 2^(-16445 + 64 - pc) (measured: denormal 1 + denormal 1 = 0 at PC24 / PC53).
inline F80 RoundToF80(const Unr& u, int pc) {
    return PackF80(u.sign, RoundMag(u.e, u.m, u.sticky, pc, -16445 + 64 - pc, RMode::Nearest));
}

// Finite non-zero F80 -> normalized (m64 bit 63 set, unbiased exponent of the leading bit)
struct Fin { bool sign; i32 e; u64 m; };
inline Fin Unpack(const F80& a) {
    if (a.exp == 0) {
        const int c = std::countl_zero(a.mant);
        return Fin{ a.sign, i32(-16382 - c), a.mant << c };
    }
    return Fin{ a.sign, a.exp - 16383, a.mant };
}

inline Unr Normalize(bool sign, i32 e, U128 m, bool sticky) {      // m != 0, e = exponent for bit 127 as it stands
    const int c = Clz(m);
    return Unr{ sign, e - c, Shl(m, unsigned(c)), sticky };
}

// ---- NaN handling ---------------------------------------------------------------------------------------------------------
constexpr F80 Quiet(F80 a) { a.mant |= u64(1) << 62; return a; }

// Result for an operation with at least one NaN / unsupported operand. a and b are the operands (b == a for unary ops)
constexpr F80 PropagateNaN(const F80& a, const F80& b) {
    const Class ca = Classify(a), cb = Classify(b);
    const bool na = ca == Class::QNaN || ca == Class::SNaN, nb = cb == Class::QNaN || cb == Class::SNaN;
    if (ca == Class::Unsupported || cb == Class::Unsupported) return Indefinite();           // an unsupported encoding beats everything (measured)
    if (na && nb) {
        // QNaN + SNaN: the QNaN; otherwise the larger significand
        if (ca == Class::QNaN && cb == Class::SNaN) return a;
        if (cb == Class::QNaN && ca == Class::SNaN) return b;
        if (a.mant > b.mant) return Quiet(a);
        if (b.mant > a.mant) return Quiet(b);
        return Quiet(a.sign ? b : a);   // equal significands: the positive one (rarely matters)
    }
    return Quiet(na ? a : b);
}

} // namespace detail

// ----------------------------------------------------------------------------------------------------------------------------
// Conversions
// ----------------------------------------------------------------------------------------------------------------------------
inline F80 FromDoubleBits(u64 bits) {
    const bool s = (bits >> 63) != 0;
    const unsigned e = unsigned(bits >> 52) & 0x7FF;
    const u64 f = bits & ((u64(1) << 52) - 1);
    if (e == 0x7FF) {
        if (f == 0) return Inf(s);
        return F80{ 0x8000000000000000ull | (f << 11) | (u64(1) << 62), 0x7FFF, s };    // fld m64 quiets an SNaN
    }
    if (e == 0) {
        if (f == 0) return Zero(s);
        const int c = std::countl_zero(f);                                              // f < 2^52 -> c >= 12
        return F80{ f << c, i32(-1022 - (c - 11) + 16383), s };
    }
    return F80{ (u64(1) << 63) | (f << 11), i32(e) - 1023 + 16383, s };
}
inline F80 FromFloatBits(u32 bits) {
    const bool s = (bits >> 31) != 0;
    const unsigned e = (bits >> 23) & 0xFF;
    const u64 f = bits & 0x7FFFFF;
    if (e == 0xFF) {
        if (f == 0) return Inf(s);
        return F80{ 0x8000000000000000ull | (f << 40) | (u64(1) << 62), 0x7FFF, s };
    }
    if (e == 0) {
        if (f == 0) return Zero(s);
        const int c = std::countl_zero(f);                                              // f < 2^23 -> c >= 41
        return F80{ f << c, i32(-126 - (c - 40) + 16383), s };
    }
    return F80{ (u64(1) << 63) | (f << 40), i32(e) - 127 + 16383, s };
}
inline F80 FromDouble(double d) { return FromDoubleBits(std::bit_cast<u64>(d)); }
inline F80 FromFloat(float f)   { return FromFloatBits(std::bit_cast<u32>(f)); }
inline F80 FromInt64(i64 v) {
    if (v == 0) return Zero(false);
    const bool s = v < 0;
    const u64 m = s ? (~u64(v) + 1) : u64(v);
    const int c = std::countl_zero(m);
    return F80{ m << c, i32(63 - c) + 16383, s };
}

namespace detail {
// F80 -> binary32/64 bits with the given significand width p, denormal grid and exponent bias; returns the packed (sign|exp|frac) value
template <int P, int ExpBits>
inline u64 ToBinaryBits(const F80& a, RMode mode) {
    constexpr int Bias = (1 << (ExpBits - 1)) - 1;
    constexpr int FracBits = P - 1;
    constexpr u64 ExpMask = (u64(1) << ExpBits) - 1;
    const u64 signBit = a.sign ? (u64(1) << (ExpBits + FracBits)) : 0;
    const u64 infBits = signBit | (ExpMask << FracBits);
    switch (Classify(a)) {
    case Class::Zero: return signBit;
    case Class::Inf: return infBits;
    case Class::QNaN: case Class::SNaN: {
        u64 frac = (a.mant << 1) >> (64 - FracBits);                               // top FracBits bits of the fraction
        frac |= u64(1) << (FracBits - 1);                                          // always quiet
        return infBits | frac;
    }
    case Class::Unsupported: return infBits | (u64(1) << (FracBits - 1)) | (u64(1) << (ExpBits + FracBits));      // real indefinite: negative quiet NaN
    default: break;
    }
    const Fin f = Unpack(a);
    const Rounded r = RoundMag(f.e, U128{ f.m, 0 }, false, P, -(Bias - 1) - FracBits, mode);
    if (r.q == 0) return signBit;
    const int n = 64 - std::countl_zero(r.q);
    const i32 E = r.lsbExp + n - 1;
    if (E > Bias) {
        if (mode == RMode::Odd) return signBit | ((ExpMask - 1) << FracBits) | ((u64(1) << FracBits) - 1);   // largest finite
        return infBits;
    }
    if (E < 1 - Bias) {
        const u64 frac = r.q << (r.lsbExp + (Bias - 1) + FracBits);
        return signBit | frac;                                                     // frac may carry into the exponent field: right
    }
    const u64 frac = (r.q << (P - n)) & ((u64(1) << FracBits) - 1);
    return signBit | (u64(E + Bias) << FracBits) | frac;
}
} // namespace detail

// fstp m64 / fstp m32 (round to nearest even; SNaN quieted)
inline u64 ToDoubleBits(const F80& a) { return detail::ToBinaryBits<53, 11>(a, detail::RMode::Nearest); }
inline u32 ToFloatBits(const F80& a)  { return u32(detail::ToBinaryBits<24, 8>(a, detail::RMode::Nearest)); }
inline double ToDouble(const F80& a)  { return std::bit_cast<double>(ToDoubleBits(a)); }
inline float  ToFloat(const F80& a)   { return std::bit_cast<float>(ToFloatBits(a)); }
// Round-to-odd to 53 bits: rounding the result further to <= 51 bits gives the same as rounding the 64-bit value directly
// (so double -> float of a ToDoubleRoundToOdd() value equals ToFloat() of the F80).
inline u64 ToDoubleRoundToOddBits(const F80& a) { return detail::ToBinaryBits<53, 11>(a, detail::RMode::Odd); }
inline double ToDoubleRoundToOdd(const F80& a)  { return std::bit_cast<double>(ToDoubleRoundToOddBits(a)); }
// fistp m64 with round-to-nearest-even (out of range / NaN -> INT64_MIN, the integer indefinite)
inline i64 ToInt64(const F80& a) {
    const Class c = Classify(a);
    if (c == Class::Zero || c == Class::Denormal) return 0;
    if (c != Class::Normal) return INT64_MIN;
    const i32 e = a.exp - 16383;                  // value = mant * 2^(e-63)
    if (e >= 63) return INT64_MIN;
    if (e < -1) return 0;
    const int sh = 63 - e;                        // 1..64
    u64 q = sh >= 64 ? 0 : (a.mant >> sh);
    const u64 rem = sh >= 64 ? a.mant : (a.mant & ((u64(1) << sh) - 1));
    const u64 half = u64(1) << (sh - 1);
    if (rem > half || (rem == half && (q & 1))) ++q;
    return a.sign ? i64(~q + 1) : i64(q);
}

// ----------------------------------------------------------------------------------------------------------------------------
// Basic arithmetic at precision control pc (24 / 53 / 64)
// ----------------------------------------------------------------------------------------------------------------------------
namespace detail {

inline bool IsSpecial(const F80& a) {
    const Class c = Classify(a);
    return c == Class::Inf || c == Class::QNaN || c == Class::SNaN || c == Class::Unsupported;
}
inline bool IsNaNOrBad(const F80& a) {
    const Class c = Classify(a);
    return c == Class::QNaN || c == Class::SNaN || c == Class::Unsupported;
}

// magnitude add (same signs) or subtract (|a| >= |b|, operands finite non-zero)
inline F80 AddMag(const Fin& a, const Fin& b, bool subtract, bool resultSign, int pc) {
    // a is the operand with the larger exponent (or equal exponent and larger mantissa)
    const unsigned d = unsigned(a.e - b.e);
    const U128 A{ a.m, 0 };
    U128 B = Shr(U128{ b.m, 0 }, d);
    const bool lost = LowBitsSet(U128{ b.m, 0 }, d);       // bits shifted out (only possible when d > 64)
    if (!subtract) {
        U128 S = Add(A, B);
        i32 e = a.e;
        bool sticky = lost;
        if (S.hi < A.hi || (S.hi == A.hi && S.lo < A.lo)) {                     // carry out of bit 127
            sticky = sticky || (S.lo & 1);
            S = Shr(S, 1); S.hi |= u64(1) << 63; ++e;
        }
        return RoundToF80(Unr{ resultSign, e, S, sticky }, pc);
    }
    U128 D = Sub(A, B);
    if (lost) D = Sub(D, U128{ 0, 1 });                      // true B was a bit larger than the truncated one
    if (IsZero(D) && !lost) return Zero(false);
    return RoundToF80(Normalize(resultSign, a.e, D, lost), pc);
}

} // namespace detail

inline F80 Add(F80 a, F80 b, int pc = 64) {
    using namespace detail;
    const Class ca = Classify(a), cb = Classify(b);
    if (IsNaNOrBad(a) || IsNaNOrBad(b)) return PropagateNaN(a, b);
    if (ca == Class::Inf || cb == Class::Inf) {
        if (ca == Class::Inf && cb == Class::Inf) return a.sign == b.sign ? a : Indefinite();
        return ca == Class::Inf ? a : b;
    }
    if (ca == Class::Zero && cb == Class::Zero) return Zero(a.sign && b.sign);
    if (ca == Class::Zero) return RoundToF80(Normalize(b.sign, Unpack(b).e, U128{ Unpack(b).m, 0 }, false), pc);
    if (cb == Class::Zero) return RoundToF80(Normalize(a.sign, Unpack(a).e, U128{ Unpack(a).m, 0 }, false), pc);
    Fin x = Unpack(a), y = Unpack(b);
    if (x.e < y.e || (x.e == y.e && x.m < y.m)) { const Fin t = x; x = y; y = t; }
    return AddMag(x, y, x.sign != y.sign, x.sign, pc);
}
inline F80 Sub(F80 a, F80 b, int pc = 64) { b.sign = !b.sign; if (IsNaN(b)) b.sign = !b.sign; return Add(a, b, pc); }

inline F80 Mul(F80 a, F80 b, int pc = 64) {
    using namespace detail;
    const Class ca = Classify(a), cb = Classify(b);
    if (IsNaNOrBad(a) || IsNaNOrBad(b)) return PropagateNaN(a, b);
    const bool s = a.sign != b.sign;
    if (ca == Class::Inf || cb == Class::Inf) {
        if (ca == Class::Zero || cb == Class::Zero) return Indefinite();
        return Inf(s);
    }
    if (ca == Class::Zero || cb == Class::Zero) return Zero(s);
    const Fin x = Unpack(a), y = Unpack(b);
    U128 p = Mul64(x.m, y.m);
    i32 e = x.e + y.e;
    if (p.hi >> 63) ++e; else p = Shl(p, 1);
    return RoundToF80(Unr{ s, e, p, false }, pc);
}

inline F80 Div(F80 a, F80 b, int pc = 64) {
    using namespace detail;
    const Class ca = Classify(a), cb = Classify(b);
    if (IsNaNOrBad(a) || IsNaNOrBad(b)) return PropagateNaN(a, b);
    const bool s = a.sign != b.sign;
    if (ca == Class::Inf) return cb == Class::Inf ? Indefinite() : Inf(s);
    if (cb == Class::Inf) return Zero(s);
    if (cb == Class::Zero) return ca == Class::Zero ? Indefinite() : Inf(s);
    if (ca == Class::Zero) return Zero(s);
    const Fin x = Unpack(a), y = Unpack(b);
    // (x.m * 2^128) / y.m
    u32 num[6] = { 0, 0, 0, 0, u32(x.m), u32(x.m >> 32) };
    const u32 den[2] = { u32(y.m), u32(y.m >> 32) };
    u32 q[5] = {}, r[2] = {};
    DivModWords(num, 6, den, 2, q, r);
    U128 Q{ (u64(q[3]) << 32) | q[2], (u64(q[1]) << 32) | q[0] };
    bool sticky = (r[0] | r[1]) != 0;
    i32 e = x.e - y.e;
    if (q[4]) {                                    // ratio >= 1: Q has 129 bits
        sticky = sticky || (Q.lo & 1);
        Q = Shr(Q, 1); Q.hi |= u64(1) << 63;
    } else {
        --e;
    }
    return RoundToF80(Unr{ s, e, Q, sticky }, pc);
}

inline F80 Sqrt(F80 a, int pc = 64) {
    using namespace detail;
    const Class ca = Classify(a);
    if (IsNaNOrBad(a)) return PropagateNaN(a, a);
    if (ca == Class::Zero) return a;
    if (a.sign) return Indefinite();
    if (ca == Class::Inf) return a;
    const Fin x = Unpack(a);
    // value = f * 2^t with f = x.m / 2^63 in [1, 2). Even t: M = x.m << 63 (sqrt(M) = sqrt(f) * 2^63); odd t: M = x.m << 64 (sqrt(2f) * 2^63).
    // floor(sqrt(M)) is then a 64-bit significand with the top bit set and the result exponent is floor(t / 2).
    const i32 t = x.e;
    const U128 M = (t & 1) ? Shl(U128{ 0, x.m }, 64) : Shl(U128{ 0, x.m }, 63);
    const i32 eRes = (t - (t & 1)) >> 1;
    U128 rem = M, res{ 0, 0 };
    U128 bit = Shl(U128{ 0, 1 }, 126);
    while (!IsZero(bit)) {
        const U128 s = Add(res, bit);
        if (Ge(rem, s)) { rem = Sub(rem, s); res = Add(Shr(res, 1), bit); }
        else res = Shr(res, 1);
        bit = Shr(bit, 2);
    }
    // sqrt(M) = res + f with f in [0, 1);  f >= 1/2  <=>  M - res^2 > res   (a tie is impossible)
    const bool halfUp = Lt(U128{ 0, res.lo }, rem);
    return RoundToF80(Unr{ false, eRes, U128{ res.lo, halfUp ? (u64(1) << 63) : 0 }, !IsZero(rem) }, pc);
}

} // namespace notsa::fp::soft

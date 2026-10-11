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

// ----------------------------------------------------------------------------------------------------------------------------
// Wide numbers for the transcendental functions: sign, exponent, 128-bit normalized significand (relative accuracy ~2^-124)
// ----------------------------------------------------------------------------------------------------------------------------
namespace detail {

struct W { bool neg; i32 e; U128 m; };           // value = (-1)^neg * m * 2^(e-127), m bit 127 set; m == 0 is zero

constexpr W WZero() { return W{ false, 0, U128{ 0, 0 } }; }
constexpr bool WIsZero(const W& a) { return IsZero(a.m); }
constexpr W WOne() { return W{ false, 0, U128{ u64(1) << 63, 0 } }; }
inline W WFromFin(const Fin& f) { return W{ f.sign, f.e, U128{ f.m, 0 } }; }
inline W WNormalize(bool neg, i32 e, U128 m) {
    if (IsZero(m)) return WZero();
    const int c = Clz(m);
    return W{ neg, e - c, Shl(m, unsigned(c)) };
}
inline W WNeg(W a) { a.neg = !a.neg; return a; }
inline W WAbs(W a) { a.neg = false; return a; }
inline W WMul(const W& a, const W& b) {
    if (WIsZero(a) || WIsZero(b)) return WZero();
    U128 p = MulHi(a.m, b.m);
    i32 e = a.e + b.e + 1;
    if (!(p.hi >> 63)) { p = Shl(p, 1); --e; }
    return W{ a.neg != b.neg, e, p };
}
inline W WAdd(const W& a, const W& b) {
    if (WIsZero(a)) return b;
    if (WIsZero(b)) return a;
    const bool aBig = a.e > b.e || (a.e == b.e && Ge(a.m, b.m));
    const W& x = aBig ? a : b;
    const W& y = aBig ? b : a;
    const unsigned d = unsigned(x.e - y.e);
    const U128 ym = Shr(y.m, d);
    if (x.neg == y.neg) {
        U128 s = Add(x.m, ym);
        i32 e = x.e;
        if (Lt(s, x.m)) { s = Shr(s, 1); s.hi |= u64(1) << 63; ++e; }
        return W{ x.neg, e, s };
    }
    return WNormalize(x.neg, x.e, Sub(x.m, ym));
}
inline W WSub(const W& a, const W& b) { return WAdd(a, WNeg(b)); }
inline W WDiv(const W& a, const W& b) {          // b != 0
    if (WIsZero(a)) return WZero();
    u32 num[8] = { 0, 0, 0, 0, u32(a.m.lo), u32(a.m.lo >> 32), u32(a.m.hi), u32(a.m.hi >> 32) };
    const u32 den[4] = { u32(b.m.lo), u32(b.m.lo >> 32), u32(b.m.hi), u32(b.m.hi >> 32) };
    u32 q[5] = {};
    DivModWords(num, 8, den, 4, q, nullptr);
    U128 Q{ (u64(q[3]) << 32) | q[2], (u64(q[1]) << 32) | q[0] };
    i32 e = a.e - b.e;
    if (q[4]) { Q = Shr(Q, 1); Q.hi |= u64(1) << 63; } else { --e; }
    return W{ a.neg != b.neg, e, Q };
}
inline W WFromQ(U128 q, bool neg = false) {      // fixed point value q * 2^-128
    if (IsZero(q)) return WZero();
    const int c = Clz(q);
    return W{ neg, -c - 1, Shl(q, unsigned(c)) };
}
inline U128 WToQ(const W& w) {                   // |w| < 1 as q * 2^-128
    if (WIsZero(w)) return U128{ 0, 0 };
    return Shr(w.m, unsigned(-(w.e + 1)));
}
inline F80 WToF80(const W& w) {                  // round to the 64-bit extended significand
    if (WIsZero(w)) return Zero(false);
    return RoundToF80(Unr{ w.neg, w.e, w.m, true }, 64);
}
// W from a F80 operand that is finite and non-zero
inline W WFromF80(const F80& a) { return WFromFin(Unpack(a)); }

// ---- Taylor tails in Q128 -----------------------------------------------------------------------------------------------------
// SIN_C[k-1] = 1/(2k+1)!, k = 1..16
inline constexpr U128 kSinC[16] = {
    { 0x2aaaaaaaaaaaaaaaull, 0xaaaaaaaaaaaaaaabull },
    { 0x0222222222222222ull, 0x2222222222222222ull },
    { 0x000d00d00d00d00dull, 0x00d00d00d00d00d0ull },
    { 0x00002e3bc74aad8eull, 0x671f5583911ca003ull },
    { 0x0000006b99159fd5ull, 0x138e3f9d1f92e0dfull },
    { 0x00000000b092309dull, 0x43684be51c198e92ull },
    { 0x0000000000d73f9full, 0x399dc0f88ec32b58ull },
    { 0x000000000000ca96ull, 0x3b81856a53593029ull },
    { 0x0000000000000097ull, 0xa4da340a0ab92651ull },
    { 0x0000000000000000ull, 0x5c6e3bdb73d5c630ull },
    { 0x0000000000000000ull, 0x002ec368262c7034ull },
    { 0x0000000000000000ull, 0x000013f3ccdd1660ull },
    { 0x0000000000000000ull, 0x0000000746ac70b7ull },
    { 0x0000000000000000ull, 0x00000000024b3f31ull },
    { 0x0000000000000000ull, 0x000000000000a1a7ull },
    { 0x0000000000000000ull, 0x0000000000000027ull } };
// COS_C[k-1] = 1/(2k)!, k = 1..17
inline constexpr U128 kCosC[17] = {
    { 0x8000000000000000ull, 0x0000000000000000ull },
    { 0x0aaaaaaaaaaaaaaaull, 0xaaaaaaaaaaaaaaabull },
    { 0x005b05b05b05b05bull, 0x05b05b05b05b05b0ull },
    { 0x0001a01a01a01a01ull, 0xa01a01a01a01a01aull },
    { 0x0000049f93edde27ull, 0xd71cbbc05b4fa99aull },
    { 0x00000008f76c77fcull, 0x6c4bdaa26d4c3d68ull },
    { 0x000000000c9cba54ull, 0x603e4e905d6f8a2full },
    { 0x00000000000d73f9ull, 0xf399dc0f88ec32b6ull },
    { 0x0000000000000b41ull, 0x3c31dcbecbbdd802ull },
    { 0x0000000000000007ull, 0x950ae900808941eaull },
    { 0x0000000000000000ull, 0x04338e5b6dfe14a5ull },
    { 0x0000000000000000ull, 0x0001f2cf01972f57ull },
    { 0x0000000000000000ull, 0x000000c4742fe352ull },
    { 0x0000000000000000ull, 0x0000000042862899ull },
    { 0x0000000000000000ull, 0x000000000013932cull },
    { 0x0000000000000000ull, 0x000000000000050dull },
    { 0x0000000000000000ull, 0x0000000000000001ull } };

// sin(r) = r * (1 - D(r^2)),  D = u*(1/3! - u*(1/5! - u*(1/7! - ...)))        (u = r^2 in Q128, u < 0.62)
inline U128 SinDeficit(U128 u) {
    U128 acc = kSinC[15];
    for (int k = 14; k >= 0; --k) acc = Sub(kSinC[k], MulHi(u, acc));
    return MulHi(u, acc);
}
// cos(r) = 1 - D(r^2),  D = u*(1/2! - u*(1/4! - u*(1/6! - ...)))
inline U128 CosDeficit(U128 u) {
    U128 acc = kCosC[16];
    for (int k = 15; k >= 0; --k) acc = Sub(kCosC[k], MulHi(u, acc));
    return MulHi(u, acc);
}
// |r| <= ~0.8 (r may be zero)
inline W SinW(const W& r) {
    if (WIsZero(r)) return r;
    const U128 rq = WToQ(WAbs(r));
    const U128 D = SinDeficit(MulHi(rq, rq));
    if (IsZero(D)) return r;
    return WSub(r, WMul(r, WFromQ(D)));
}
inline W CosW(const W& r) {
    if (WIsZero(r)) return WOne();
    const U128 rq = WToQ(WAbs(r));
    const U128 D = CosDeficit(MulHi(rq, rq));
    if (IsZero(D)) return WOne();
    return W{ false, -1, Neg(D) };               // 1 - D with D < 1/2
}

// ---- fsin / fcos / fptan argument reduction: x = q * (pi66 / 2) + r with the Intel 66-bit pi ------------------------------------
// pi66 = 3.243F6A8885A308D3 (hex) * 2^0 ... = 0x3243F6A8885A308D3 / 2^64: pi truncated to a 66-bit significand. The FPU reduces with this constant,
// so for x near a multiple of pi/2 the result carries the (famous) error of ~2^-66 * q relative to the true value.
constexpr u32 kPi66[3] = { 0x85A308D3u, 0x243F6A88u, 3u };            // P66m = pi66 * 2^64 (little-endian words); H = P66m / 2^65 = pi66 / 2

// x finite, non-zero, |x| < 2^63. Gives quadrant q & 3 and the reduced argument r (|r| <= pi66/4) of |x|.
inline void ReduceTrig(const Fin& f, int& quad, W& r) {
    if (f.e < -1) { quad = 0; r = W{ false, f.e, U128{ f.m, 0 } }; return; }          // |x| < 0.5: nothing to reduce
    const U128 X = Shl(U128{ 0, f.m }, unsigned(f.e + 2));                                // |x| * 2^65, exact (f.e <= 62)
    const u32 u[4] = { u32(X.lo), u32(X.lo >> 32), u32(X.hi), u32(X.hi >> 32) };
    u32 q[2] = {}, rem[3] = {};
    DivModWords(u, 4, kPi66, 3, q, rem);
    U128 R{ u64(rem[2]), (u64(rem[1]) << 32) | rem[0] };
    const U128 P{ 3, 0x243F6A8885A308D3ull };
    u64 qlo = (u64(q[1]) << 32) | q[0];
    bool neg = false;
    if (Ge(Shl(R, 1), P)) { R = Sub(P, R); neg = true; ++qlo; }                          // round to the nearest multiple: r = rem - P < 0
    quad = int(qlo & 3);
    // r = R * 2^-65
    if (IsZero(R)) { r = WZero(); return; }
    const int c = Clz(R);
    r = W{ neg, 62 - c, Shl(R, unsigned(c)) };
}

} // namespace detail

struct SinCosResult { F80 sin, cos; bool c2; };

// fsin (c2 != nullptr: set when |x| >= 2^63 and the operand is returned unchanged)
inline F80 Sin(const F80& x, bool* c2 = nullptr) {
    using namespace detail;
    if (c2) *c2 = false;
    const Class cl = Classify(x);
    if (cl == Class::QNaN || cl == Class::SNaN || cl == Class::Unsupported) return PropagateNaN(x, x);
    if (cl == Class::Inf) return Indefinite();
    if (cl == Class::Zero) return x;
    const Fin f = Unpack(x);
    if (f.e >= 63) { if (c2) *c2 = true; return x; }
    int quad; W r;
    ReduceTrig(f, quad, r);
    W res = (quad & 1) ? CosW(r) : SinW(r);
    if (quad & 2) res = WNeg(res);
    if (x.sign) res = WNeg(res);
    return WToF80(res);
}
inline F80 Cos(const F80& x, bool* c2 = nullptr) {
    using namespace detail;
    if (c2) *c2 = false;
    const Class cl = Classify(x);
    if (cl == Class::QNaN || cl == Class::SNaN || cl == Class::Unsupported) return PropagateNaN(x, x);
    if (cl == Class::Inf) return Indefinite();
    if (cl == Class::Zero) return One();
    const Fin f = Unpack(x);
    if (f.e >= 63) { if (c2) *c2 = true; return x; }
    int quad; W r;
    ReduceTrig(f, quad, r);
    W res = (quad & 1) ? SinW(r) : CosW(r);
    if (((quad + 1) & 2)) res = WNeg(res);                      // cos: + - - +
    return WToF80(res);
}

namespace detail {

// ---- constants for atan / log -----------------------------------------------------------------------------------------------
// kAtanC[n-1] = 1/(2n+1), n = 1..13
inline constexpr U128 kAtanC[13] = {
    { 0x5555555555555555ull, 0x5555555555555555ull },
    { 0x3333333333333333ull, 0x3333333333333333ull },
    { 0x2492492492492492ull, 0x4924924924924925ull },
    { 0x1c71c71c71c71c71ull, 0xc71c71c71c71c71cull },
    { 0x1745d1745d1745d1ull, 0x745d1745d1745d17ull },
    { 0x13b13b13b13b13b1ull, 0x3b13b13b13b13b14ull },
    { 0x1111111111111111ull, 0x1111111111111111ull },
    { 0x0f0f0f0f0f0f0f0full, 0x0f0f0f0f0f0f0f0full },
    { 0x0d79435e50d79435ull, 0xe50d79435e50d794ull },
    { 0x0c30c30c30c30c30ull, 0xc30c30c30c30c30cull },
    { 0x0b21642c8590b216ull, 0x42c8590b21642c86ull },
    { 0x0a3d70a3d70a3d70ull, 0xa3d70a3d70a3d70aull },
    { 0x097b425ed097b425ull, 0xed097b425ed097b4ull } };
struct WConst { i32 e; U128 m; };      // positive value m * 2^(e-127)
// kAtanTab[k] = atan(k/16), k = 0..16 (k = 0 unused)
inline constexpr WConst kAtanTab[17] = {
    { 0, { 0, 0 } },
    { -5, { 0xffaaddb967ef4e36ull, 0xcb2792dc0e2e0d51ull } },
    { -4, { 0xfeadd4d5617b6e32ull, 0xc897989f3e888ef8ull } },
    { -3, { 0xbdcbda5e72d81134ull, 0x7b0b4f881c9c7488ull } },
    { -3, { 0xfadbafc96406eb15ull, 0x6dc79ef5f7a217e6ull } },
    { -2, { 0x9b13b9b83f5e5e69ull, 0xc5abb498d27af328ull } },
    { -2, { 0xb7b0ca0f26f78473ull, 0x8aa32122dcfe4483ull } },
    { -2, { 0xd327761e611fe5b6ull, 0x427c95e9001e7136ull } },
    { -2, { 0xed63382b0dda7b45ull, 0x6fe445ecbc3a8d03ull } },
    { -1, { 0x832bf4a6d9867e2aull, 0x4b6a09cb61a515c1ull } },
    { -1, { 0x8f005d5ef7f59f9bull, 0x5c835e1665c43748ull } },
    { -1, { 0x9a2f80e671bdda20ull, 0x4226f8e2204ff3bdull } },
    { -1, { 0xa4bc7d1934f70924ull, 0x19a87f2a457dac9full } },
    { -1, { 0xaeac4c38b4d8c080ull, 0x14725e2f3e52070aull } },
    { -1, { 0xb8053e2bc2319e73ull, 0xcb2da55210a4443dull } },
    { -1, { 0xc0ce85b8ac526640ull, 0x89dd62c46e92fa25ull } },
    { -1, { 0xc90fdaa22168c234ull, 0xc4c6628b80dc1cd1ull } } };
// kLnTab[k+9] = ln(1 + k/32), k = -9..13
inline constexpr WConst kLnTab[23] = {
    { -2, { 0xa9157039c51ebe70ull, 0x8164c759686a2209ull } } /* negative */,
    { -2, { 0x934b1089a6dc93c1ull, 0xdf5bb3b60554e152ull } } /* negative */,
    { -3, { 0xfcc8e3659d9bcbecull, 0xca0cdf301431b60full } } /* negative */,
    { -3, { 0xd49f69e456cf1b79ull, 0x5f53bd2e406e66e7ull } } /* negative */,
    { -3, { 0xadfa035aa1ed8fdcull, 0x149767e410316d2cull } } /* negative */,
    { -3, { 0x88bc74113f23def1ull, 0x9c5a0fe396f40f1eull } } /* negative */,
    { -4, { 0xc99af2eaca4c4570ull, 0xeaf51f66692844baull } } /* negative */,
    { -4, { 0x842cc5acf1d03445ull, 0x1fecdfa819b96098ull } } /* negative */,
    { -5, { 0x820aec4f3a222380ull, 0xb9e3aea6c444ef07ull } } /* negative */,
    { 0, { 0, 0 } },
    { -6, { 0xfc14d873c1980267ull, 0xc7e09e3de453f5d6ull } },
    { -5, { 0xf85186008b15330bull, 0xe64b8b775997898dull } },
    { -4, { 0xb78694572b5a5cdfull, 0x24cdcf68cdb20673ull } },
    { -4, { 0xf1383b7157972f4full, 0x543fff0ff4f0aaeeull } },
    { -3, { 0x94aa97c0ffa91a60ull, 0x2ee3880fb7d34428ull } },
    { -3, { 0xaff983853c9e9e43ull, 0x9f105039091dd7f3ull } },
    { -3, { 0xca92d4e7a2b5a3b2ull, 0x0983a9c5c4b3b133ull } },
    { -3, { 0xe47fbe3cd4d10d61ull, 0x2ec0f797fdcd1257ull } },
    { -3, { 0xfdc8c36af1f1546aull, 0xaa3361bca6965049ull } },
    { -2, { 0x8b3ae55d5d30701cull, 0xe63eab883717047eull } },
    { -2, { 0x974715d708e984e1ull, 0x6648d42840d9e6f7ull } },
    { -2, { 0xa30c5e10e2f613e8ull, 0x5bd9bd99e39a20afull } },
    { -2, { 0xae8dedfac04e5284ull, 0x6c707b8ffc22b3e7ull } } };
inline constexpr WConst kLog2e = { 0, { 0xb8aa3b295c17f0bbull, 0xbe87fed0691d3e89ull } };          // 1/ln 2
inline constexpr WConst kLn2W = { -1, { 0xb17217f7d1cf79abull, 0xc9e3b39803f2f6afull } };
inline constexpr WConst kPiW = { 1, { 0xc90fdaa22168c234ull, 0xc4c6628b80dc1cd1ull } };          // pi

inline W WFromConst(const WConst& c, bool neg = false) { return W{ neg, c.e, c.m }; }
inline W WFromInt(i64 v) {                               // small integers
    if (v == 0) return WZero();
    const bool neg = v < 0;
    return WNormalize(neg, 127, U128{ 0, neg ? (~u64(v) + 1) : u64(v) });
}
// nearest integer of |w| (w small), ties up: only used where ties cannot matter
inline int WRoundAbs(const W& w) {
    if (WIsZero(w) || w.e < -1) return 0;
    if (w.e > 24) return 1 << 24;
    const u64 t2 = Shr(w.m, unsigned(127 - w.e - 1)).lo;       // floor(2|w|)
    return int((t2 + 1) >> 1);
}
inline bool WLess(const W& a, const W& b) {                    // |a| < |b|
    if (WIsZero(a)) return !WIsZero(b);
    if (WIsZero(b)) return false;
    return a.e < b.e || (a.e == b.e && Lt(a.m, b.m));
}

// atan(t) = t * (1 - D),  D = u*(1/3 - u*(1/5 - u*(1/7 - ...))),  u = t^2 <= 2^-10
inline U128 AtanDeficit(U128 u) {
    U128 acc = kAtanC[12];
    for (int k = 11; k >= 0; --k) acc = Sub(kAtanC[k], MulHi(u, acc));
    return MulHi(u, acc);
}
// atan of 0 <= z <= 1 (W, z may be exactly 1)
inline W AtanUnit(const W& z) {
    if (WIsZero(z)) return z;
    const unsigned sh = z.e >= -6 ? unsigned(122 - z.e) : 128u;
    const int k = int((((sh >= 128) ? u64(0) : Shr(z.m, sh).lo) + 1) >> 1);        // round(16 z)
    W t = z;
    if (k != 0) {
        const W c = WNormalize(false, 123, U128{ 0, u64(k) });                       // k/16
        t = WDiv(WSub(z, c), WAdd(WOne(), WMul(z, c)));
    }
    W at = t;
    if (!WIsZero(t)) {
        const U128 tq = WToQ(WAbs(t));
        const U128 D = AtanDeficit(MulHi(tq, tq));
        if (!IsZero(D)) at = WSub(t, WMul(t, WFromQ(D)));
    }
    return k ? WAdd(WFromConst(kAtanTab[k]), at) : at;
}
inline W PiW(int pow2 = 0) { return W{ false, kPiW.e + pow2, kPiW.m }; }

// |y| / |x| atan2 for finite non-zero operands (W magnitudes), x's sign selects the half plane, result carries y's sign
inline W Atan2W(const W& ay, const W& ax, bool xNeg, bool yNeg) {
    W a;
    if (WLess(ax, ay)) a = WSub(PiW(-1), AtanUnit(WDiv(ax, ay)));
    else               a = AtanUnit(WDiv(ay, ax));
    if (xNeg) a = WSub(PiW(0), a);
    a.neg = yNeg;
    return a;
}

// ln of 1 + small / table reduction: returns log2(mw) for a positive W (any exponent)
inline W Log2W(const W& mw0) {
    i32 e = mw0.e;
    W mw{ false, 0, mw0.m };                                                      // significand in [1, 2)
    if (mw.m.hi > 0xB504F333F9DE6484ull || (mw.m.hi == 0xB504F333F9DE6484ull && mw.m.lo > 0xE00000000000000ull)) { mw.e = -1; ++e; }   // > sqrt(2): halve
    const W d = WSub(mw, WOne());
    const int kAbs = WRoundAbs(WMul(d, W{ false, 5, U128{ u64(1) << 63, 0 } }));  // d * 32
    const int k = d.neg ? -kAbs : kAbs;
    W lnm = WZero();
    W s;
    if (k == 0) {
        s = WDiv(d, WAdd(mw, WOne()));
    } else {
        const W c = WNormalize(false, 122, U128{ 0, u64(32 + k) });                // 1 + k/32
        s = WDiv(WSub(mw, c), WAdd(mw, c));
        lnm = WFromConst(kLnTab[k + 9], k < 0);
    }
    if (!WIsZero(s)) {
        const U128 sq = WToQ(WAbs(s));
        const U128 u = MulHi(sq, sq);
        U128 acc = kAtanC[9];
        for (int j = 8; j >= 0; --j) acc = Add(kAtanC[j], MulHi(u, acc));          // all positive: 1/3 + u(1/5 + u(...))
        const W series = WMul(s, WAdd(WOne(), WFromQ(MulHi(u, acc))));            // atanh(s)
        W twice = series; ++twice.e;
        lnm = WAdd(lnm, twice);
    }
    return WAdd(WFromInt(e), WMul(lnm, WFromConst(kLog2e)));
}
// log2(1 + x) for |x| < 2^-5 (or any x > -1 with small error growth): s = x / (2 + x), ln(1+x) = 2 atanh(s)
inline W Log2OnePlusSmall(const W& x) {
    const W s = WDiv(x, WAdd(WFromInt(2), x));
    const U128 sq = WToQ(WAbs(s));
    const U128 u = MulHi(sq, sq);
    U128 acc = kAtanC[9];
    for (int j = 8; j >= 0; --j) acc = Add(kAtanC[j], MulHi(u, acc));
    W ln = WMul(s, WAdd(WOne(), WFromQ(MulHi(u, acc))));
    ++ln.e;
    return WMul(ln, WFromConst(kLog2e));
}

} // namespace detail

// fpatan: atan2(y, x), ST(1) = y, ST(0) = x. Rounded to 64 bits whatever the PC.
inline F80 Atan2(const F80& y, const F80& x) {
    using namespace detail;
    const Class cy = Classify(y), cx = Classify(x);
    if (IsNaNOrBad(y) || IsNaNOrBad(x)) return PropagateNaN(y, x);
    const F80 pi = WToF80(PiW(0)), pi2 = WToF80(PiW(-1)), pi4 = WToF80(PiW(-2));
    const F80 pi34 = WToF80(WSub(PiW(0), PiW(-2)));
    const bool ys = y.sign;
    auto signed_ = [&](F80 v) { v.sign = ys; return v; };
    if (cy == Class::Inf) {
        if (cx == Class::Inf) return signed_(x.sign ? pi34 : pi4);
        return signed_(pi2);
    }
    if (cx == Class::Inf) return x.sign ? signed_(pi) : Zero(ys);
    if (cy == Class::Zero) return x.sign ? signed_(pi) : Zero(ys);      // x = +-0 or finite: -0 counts as negative
    if (cx == Class::Zero) return signed_(pi2);
    return WToF80(Atan2W(WAbs(WFromF80(y)), WAbs(WFromF80(x)), x.sign, ys));
}
inline F80 Atan(const F80& x) { return Atan2(x, One()); }

// fptan: tan(x), then 1.0 is pushed. pushed == the value of the new ST(0) (1.0, or the same NaN / indefinite for invalid operands);
// c2 set (|x| >= 2^63): the operand is returned unchanged in `tan` and NOTHING is pushed.
struct TanResult { F80 tan, pushed; bool c2; };
inline TanResult TanPush(const F80& x) {
    using namespace detail;
    TanResult res{ Zero(), One(), false };
    const Class cl = Classify(x);
    if (cl == Class::QNaN || cl == Class::SNaN || cl == Class::Unsupported) { res.tan = res.pushed = PropagateNaN(x, x); return res; }
    if (cl == Class::Inf) { res.tan = res.pushed = Indefinite(); return res; }
    if (cl == Class::Zero) { res.tan = x; return res; }
    const Fin f = Unpack(x);
    if (f.e >= 63) { res.tan = x; res.c2 = true; return res; }
    int quad; W r;
    ReduceTrig(f, quad, r);
    const W sr = SinW(r), cr = CosW(r);
    W t = (quad & 1) ? WNeg(WDiv(cr, sr)) : WDiv(sr, cr);
    if (x.sign) t = WNeg(t);
    res.tan = WToF80(t);
    return res;
}
inline F80 Tan(const F80& x, bool* c2 = nullptr) { const TanResult r = TanPush(x); if (c2) *c2 = r.c2; return r.tan; }
// fsincos
inline SinCosResult SinCos(const F80& x) {
    using namespace detail;
    SinCosResult res{ Zero(), Zero(), false };
    const Class cl = Classify(x);
    if (cl == Class::QNaN || cl == Class::SNaN || cl == Class::Unsupported) { res.sin = res.cos = PropagateNaN(x, x); return res; }
    if (cl == Class::Inf) { res.sin = res.cos = Indefinite(); return res; }
    if (cl == Class::Zero) { res.sin = x; res.cos = One(); return res; }
    const Fin f = Unpack(x);
    if (f.e >= 63) { res.sin = x; res.cos = x; res.c2 = true; return res; }
    int quad; W r;
    ReduceTrig(f, quad, r);
    const W sr = SinW(r), cr = CosW(r);
    W s = (quad & 1) ? cr : sr, c = (quad & 1) ? sr : cr;
    if (quad & 2) s = WNeg(s);
    if (((quad + 1) & 2)) c = WNeg(c);
    if (x.sign) s = WNeg(s);
    res.sin = WToF80(s); res.cos = WToF80(c);
    return res;
}

// fyl2x: ST(1) * log2(ST(0)) with ST(1) = y, ST(0) = x. (log2 = fld1 first, log10 = fldlg2, ln = fldln2)
inline F80 Yl2x(const F80& y, const F80& x) {
    using namespace detail;
    const Class cy = Classify(y), cx = Classify(x);
    if (IsNaNOrBad(y) || IsNaNOrBad(x)) return PropagateNaN(y, x);
    if (cx == Class::Zero) {                                           // log2(+-0) = -inf
        if (cy == Class::Zero) return Indefinite();
        return Inf(!y.sign);
    }
    if (x.sign) return Indefinite();                                   // log of a negative number (incl. -inf)
    if (cx == Class::Inf) {
        if (cy == Class::Zero) return Indefinite();
        return Inf(y.sign);
    }
    // x finite > 0
    const bool isOne = x.exp == 0x3FFF && x.mant == (u64(1) << 63);
    if (cy == Class::Inf) return isOne ? Indefinite() : Inf(y.sign != (x.exp < 0x3FFF));
    if (cy == Class::Zero) return Zero(y.sign != (!isOne && x.exp < 0x3FFF && x.mant != 0));    // sign of log2(x) * sign(y)
    if (isOne) return Zero(y.sign);
    const W l = Log2W(WFromF80(x));
    return WToF80(WMul(WFromF80(y), l));
}
// fyl2xp1: ST(1) * log2(ST(0) + 1).  The reference FPU forms 1 + x ROUNDED TO 64 BITS first and then takes the logarithm (measured: for |x| < 2^-64 the
// result is +-0, for small x the result has only the precision of 1 + x), so this is done here too. x <= -1 is outside the instruction's
// domain (architecturally undefined; the reference returns meaningless values such as -2^31) and gives -inf*sign here.
inline F80 Yl2xp1(const F80& y, const F80& x) {
    using namespace detail;
    const Class cy = Classify(y), cx = Classify(x);
    if (IsNaNOrBad(y) || IsNaNOrBad(x)) return PropagateNaN(y, x);
    if (cx == Class::Inf) return (cy == Class::Zero || (cy == Class::Inf && x.sign)) ? Indefinite() : Inf(y.sign != x.sign);
    if (cx == Class::Zero) return cy == Class::Inf ? Indefinite() : Zero(y.sign != x.sign);
    const F80 m = Add(One(), x, 64);
    if (m.sign || IsZero(m)) return cy == Class::Zero ? Zero(!y.sign) : Inf(!y.sign);
    if (m.exp == 0x3FFF && m.mant == (u64(1) << 63) && cy != Class::Inf) return Zero(y.sign != x.sign);       // 1 + x rounded to 1: a zero with the sign of y*x (the reference leaves +-2^-135 noise for some y)
    return Yl2x(y, m);
}

} // namespace notsa::fp::soft

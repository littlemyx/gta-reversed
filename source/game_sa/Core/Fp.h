#pragma once
// notsa::fp - floating point policy for code that must behave like the original x87 code at precision control PC=24 without using the x87.
// Theory, proofs and oracle evidence: .notes/STREAM_F_PLAN.md (rows T1..T13) and tests/standalone/fp_theory_test.cpp.
//
// The original (MSVC 7, x87) runs the game thread at PC=24: EVERY add/sub/mul/div/sqrt result is rounded to a 24 bit significand, but the EXPONENT range is the
// extended one (15 bit). For operands that are floats this is exactly IEEE single precision except where an intermediate leaves the float exponent range
// (overflow -> inf / underflow -> denormal or 0 happen on the x87 only when the value is STORED to a float, not while it sits on the FPU stack).
//   * plain `float` arithmetic (SSE2, arm64 with -ffp-contract=off, FLT_EVAL_METHOD 0) is bit-identical to the x87 whenever no intermediate leaves
//     [2^-126, 2^128] (checked by the oracle on random + special inputs); this is the DEFAULT of the portable build.
//   * F24 below is the exact model of an x87 PC24 register: a double (11 bit exponent: |x| up to 1e308, fine for anything fed by floats) whose significand is
//     kept at 24 bits after every op. Figueroa (1995): for + - * / sqrt, computing in a format with q >= 2p + 2 bits (53 >= 50) and rounding the result to p = 24 bits
//     equals the correctly rounded p-bit result -> `Round24(double op)` IS the x87 result, also for denormal/overflowing float intermediates, inf and NaN (payload aside).
//   * values loaded from true doubles (constants that are not exactly floats, e.g. 0.01, 1.0/3.0, members of type double) keep 53 bits until the next op: use
//     MulD / AddD below (single rounding of the exact result), never `F24 * double`.
//   * transcendental x87 instructions (fsin, fcos, fptan, fpatan, fyl2x, f2xm1) and the exe's _ftol2: x87::* in X87Intrinsics.h (Win32 reference) / X87Soft.h, Ftol below.
// Builds that still contain x87 code (the Win32 reference, /arch:IA32 at PC24) compile F24 to x87 as well: Round24 is then a no-op, the result stays identical.
// Caveats: (1) MulD / AddD / Round24Mul / TwoProductErr rely on strict IEEE double arithmetic: use them in SSE2 / arm64 TUs only, never in an /arch:IA32 TU.
// (2) MSVC may compile a float-RETURNING function of an /arch:SSE2 TU to x87 code (the result travels in ST0): the guarded Impl functions return bool / out-parameters.
// (3) Never `std::fma` (the 32 bit CRT's software fma uses the x87), never `-ffp-contract=on`, `-ffast-math`, `/fp:fast`.
#include <bit>
#include <cmath>
#include <cstdint>

#if defined(_M_X64) || defined(__SSE2__) || (defined(_M_IX86_FP) && _M_IX86_FP >= 1)
#define NOTSA_FP_SSE 1
#include <xmmintrin.h>
#elif defined(__aarch64__) || defined(_M_ARM64)
#define NOTSA_FP_SSE 0
#define NOTSA_FP_ARM64 1
#else
#define NOTSA_FP_SSE 0
#endif

namespace notsa::fp {

//! Round to nearest even to a 24 bit significand. Keeps the (double) exponent. Valid for normal doubles (|x| >= 2^-1022: always true for products of floats).
//! Inf/NaN pass through unchanged. The carry of the rounding increment propagates into the exponent (and overflows to inf) like a real rounder.
[[nodiscard]] inline double Round24(double x) {
    uint64_t b = std::bit_cast<uint64_t>(x);
    if ((b & 0x7FF0000000000000ull) == 0x7FF0000000000000ull) {
        return x;
    }
    b += 0x0FFFFFFFull + ((b >> 29) & 1);
    b &= ~0x1FFFFFFFull;
    return std::bit_cast<double>(b);
}

//! Round-to-odd: `p` is RN(x) and `e` = x - p (both exact, e.g. from TwoSum/TwoProduct). Returns the odd neighbour when x is inexact. Rounding the result to
//! <= 51 bits afterwards equals rounding x directly (no double rounding).
[[nodiscard]] inline double RoundToOdd(double p, double e) {
    if (e != 0.0) {
        uint64_t b = std::bit_cast<uint64_t>(p);
        if (!(b & 1)) {
            b += ((e > 0.0) == (p >= 0.0)) ? 1ull : ~0ull; // away from / toward zero by one ulp
            p = std::bit_cast<double>(b);
        }
    }
    return p;
}

#if defined(__clang__)
#pragma clang fp contract(off) // Dekker's algorithm below needs every product/sum rounded on its own
#endif
//! Exact error of the double product p = RN(a * b) (Dekker/Veltkamp; no FMA instruction and no <cmath> fma: the 32 bit CRT's software fma uses the x87 and breaks at PC=24).
//! Valid when nothing overflows/underflows (|a|, |b| < 2^995, |a*b| > 2^-900).
[[nodiscard]] inline double TwoProductErr(double a, double b, double p) {
    constexpr double kSplit = 134217729.0; // 2^27 + 1
    const double ca = kSplit * a, ah = ca - (ca - a), al = a - ah;
    const double cb = kSplit * b, bh = cb - (cb - b), bl = b - bh;
    return ((ah * bh - p) + ah * bl + al * bh) + al * bl;
}

//! Exactly rounded (RN, 24 bit) product of two doubles of up to 53 bits each, e.g. `float * double-constant` or `(double)int32 * float`.
[[nodiscard]] inline double Round24Mul(double a, double b) {
    const double p = a * b;
    if (!std::isfinite(p) || p == 0.0) {
        return p;
    }
    return Round24(RoundToOdd(p, TwoProductErr(a, b, p)));
}
//! Exactly rounded (RN, 24 bit) sum of two doubles of up to 53 bits each.
[[nodiscard]] inline double Round24Add(double a, double b) {
    const double s = a + b;
    if (!std::isfinite(s)) {
        return s;
    }
    const double bb = s - a;
    const double e  = (a - (s - bb)) + (b - bb); // TwoSum
    return Round24(RoundToOdd(s, e));
}

//! One x87 register at PC=24: significand <= 24 bits, exponent range of a double. Construct only from floats (exact), or explicitly from an already rounded double.
struct F24 {
    double v{};

    constexpr F24() = default;
    constexpr F24(float f) : v(f) {}
    F24(double) = delete; // a double constant has 53 bits: use MulD/AddD/FromD so the choice is explicit
    F24(int) = delete;
    [[nodiscard]] static constexpr F24 Raw(double d) { F24 r; r.v = d; return r; }  // d must already have a <= 24 bit significand
    [[nodiscard]] static F24 FromD(double d) { return Raw(Round24(d)); }           // fstp/fld of a double value followed by the next 24 bit op

    [[nodiscard]] explicit operator float() const { return (float)v; }              // fstp dword (rounds once more only for float denormals/overflow, like the x87)
    [[nodiscard]] explicit operator double() const { return v; }

    [[nodiscard]] friend F24 operator+(F24 a, F24 b) { return Raw(Round24(a.v + b.v)); }
    [[nodiscard]] friend F24 operator-(F24 a, F24 b) { return Raw(Round24(a.v - b.v)); }
    [[nodiscard]] friend F24 operator*(F24 a, F24 b) { return Raw(Round24(a.v * b.v)); }
    [[nodiscard]] friend F24 operator/(F24 a, F24 b) { return Raw(Round24(a.v / b.v)); }
    [[nodiscard]] friend F24 operator-(F24 a) { return Raw(-a.v); }
    F24& operator+=(F24 o) { return *this = *this + o; }
    F24& operator-=(F24 o) { return *this = *this - o; }
    F24& operator*=(F24 o) { return *this = *this * o; }
    F24& operator/=(F24 o) { return *this = *this / o; }
    [[nodiscard]] friend bool operator<(F24 a, F24 b) { return a.v < b.v; }
    [[nodiscard]] friend bool operator>(F24 a, F24 b) { return a.v > b.v; }
    [[nodiscard]] friend bool operator<=(F24 a, F24 b) { return a.v <= b.v; }
    [[nodiscard]] friend bool operator>=(F24 a, F24 b) { return a.v >= b.v; }
    [[nodiscard]] friend bool operator==(F24 a, F24 b) { return a.v == b.v; }
    [[nodiscard]] friend bool operator!=(F24 a, F24 b) { return a.v != b.v; }
};
[[nodiscard]] inline F24 MulD(F24 a, double c) { return F24::Raw(Round24Mul(a.v, c)); }   // fld a; fmul qword [c]
[[nodiscard]] inline F24 AddD(F24 a, double c) { return F24::Raw(Round24Add(a.v, c)); }   // fld a; fadd qword [c]

// ---- helpers usable with both `float` and F24 (generic code: `template<class T>`) -------------------------------------------------------------------------
[[nodiscard]] inline float sqrt(float x) { return std::sqrt(x); }
[[nodiscard]] inline F24   sqrt(F24 x) { return F24::Raw(Round24(std::sqrt(x.v))); }
[[nodiscard]] inline float abs(float x) { return std::fabs(x); }
[[nodiscard]] inline F24   abs(F24 x) { return F24::Raw(std::fabs(x.v)); }
[[nodiscard]] inline float ToFloat(float x) { return x; }
[[nodiscard]] inline float ToFloat(F24 x) { return (float)x.v; }

//! The 8 bit exponent field of a float is within [127 - Range, 127 + Range] (or the value is +-0): a cheap guard for "plain float arithmetic cannot overflow/underflow
//! in the chain of <= 4 multiplications/divisions after this input" (the callers' comments carry the proof: 4 * Range + the chain's growth must stay below 126).
template<int Range = 16>
[[nodiscard]] inline bool InSafeRange(float f) {
    const uint32_t b = std::bit_cast<uint32_t>(f) & 0x7FFFFFFFu;
    return b == 0 || (b - ((127u - Range) << 23)) < ((2u * Range + 1u) << 23);
}

// ---- float fast path guard ---------------------------------------------------------------------------------------------------------------------------
#if defined(_MSC_VER) && !defined(__clang__)
#define NOTSA_FP_NOINLINE __declspec(noinline)
#else
#define NOTSA_FP_NOINLINE __attribute__((noinline))
#endif

//! Detects whether a computation done in plain `float` left the float exponent range (overflow, inexact underflow, denormal operand): the only way plain float
//! differs from the x87 at PC=24 for float inputs (see the file comment). Usage (the guarded function must be NOTSA_FP_NOINLINE):
//!     RangeGuard g;  bool r = Impl<float>(..., out);  if (!g.Clean(&r, &out)) r = Impl<F24>(..., out);
//! Protocol: the sticky exception flags are expected to be CLEAR when a guarded region starts; Clean() reads them, and when one is set clears them and returns false.
//! Stale flags left by other code can therefore only cause one spurious slow-path call, never a missed excursion. The flags are read through an opaque NOINLINE function
//! that takes the addresses of the results: the compiler can neither CSE it nor move a (side-effect-free, hence reorderable) Impl<float> call behind it. Found by the
//! oracle: with an inlined `_mm_getcsr` MSVC /O2 let the guard silently never fire for functions without output parameters; `#pragma fenv_access(on)` is correct but
//! makes the whole TU 16x slower.
//! In a TU compiled for the x87 (/arch:IA32) the float operations ARE the x87 ones: Clean() is always true there.
#if NOTSA_FP_SSE
namespace detail {
NOTSA_FP_NOINLINE inline unsigned ReadFpFlags(const void*, const void*, const void*) { return _mm_getcsr(); }
NOTSA_FP_NOINLINE inline void     ClearFpFlags(unsigned v) { _mm_setcsr(v); }
}
class RangeGuard {
public:
    static constexpr unsigned kMask = 0x1A; // MXCSR: DE (denormal operand) | OE | UE
    [[nodiscard]] bool Clean(const void* a = nullptr, const void* b = nullptr, const void* c = nullptr) const {
        const unsigned csr = detail::ReadFpFlags(a, b, c);
        if (csr & kMask) {
            detail::ClearFpFlags(csr & ~kMask);
            return false;
        }
        return true;
    }
};
#elif defined(NOTSA_FP_ARM64)
namespace detail {
NOTSA_FP_NOINLINE inline uint64_t ReadFpFlags(const void*, const void*, const void*) { uint64_t v; __asm__ volatile("mrs %0, fpsr" : "=r"(v)); return v; }
NOTSA_FP_NOINLINE inline void     ClearFpFlags(uint64_t v) { __asm__ volatile("msr fpsr, %0" : : "r"(v)); }
}
class RangeGuard {
public:
    static constexpr uint64_t kMask = 0x8C; // FPSR: IDC | UFC | OFC
    [[nodiscard]] bool Clean(const void* a = nullptr, const void* b = nullptr, const void* c = nullptr) const {
        const uint64_t v = detail::ReadFpFlags(a, b, c);
        if (v & kMask) {
            detail::ClearFpFlags(v & ~kMask);
            return false;
        }
        return true;
    }
};
#else // x87 TU (or an architecture without a known flag register): plain float cannot be told apart from the x87 here
class RangeGuard {
public:
    [[nodiscard]] bool Clean(const void* = nullptr, const void* = nullptr, const void* = nullptr) const { return true; }
};
#endif

//! Telemetry: how many guarded calls fell back to the exact F24 path (expected: ~0 in play; the oracle/bench print it).
inline uint32_t& SlowPathCount() { static uint32_t c = 0; return c; }
#if NOTSA_FP_SSE
//! The SSE environment the plain-float fast paths assume: round to nearest, no flush-to-zero / denormals-are-zero (a driver or audio library may have changed it).
[[nodiscard]] inline bool SseModeIsGameCompatible() { return (_mm_getcsr() & (0x8000u | 0x0040u | 0x6000u)) == 0; }
inline void SetSseModeGameCompatible() { _mm_setcsr(_mm_getcsr() & ~(0x8000u | 0x0040u | 0x6000u)); }
#endif

// ---- exe float -> int conversion ----------------------------------------------------------------------------------------------------------------------
//! The exe's `_ftol2` (0x821B40, x87 `fistp qword` with truncation, LOW dword returned): NaN, +-inf and |x| >= 2^63 give the integer indefinite 0x8000000000000000
//! (-> 0), 2^31 <= |x| < 2^63 wraps. A plain C++ cast is UB there and differs per architecture (x86 cvttss2si: 0x80000000, arm64: saturates).
[[nodiscard]] inline int64_t Ftol64(double x) {
    if (!(std::fabs(x) < 9223372036854775808.0)) { // NaN fails
        return INT64_MIN;
    }
    return (int64_t)x;
}
[[nodiscard]] inline int32_t Ftol(double x) { return (int32_t)(uint32_t)(uint64_t)Ftol64(x); }
[[nodiscard]] inline uint32_t FtolU(double x) { return (uint32_t)(uint64_t)Ftol64(x); }

} // namespace notsa::fp

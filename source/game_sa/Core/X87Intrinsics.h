#pragma once
// The exe was compiled by MSVC 7 with intrinsics: sin/cos/atan2/tan/sqrt/log10 in the game code are the INLINE x87 instructions
// fsin / fcos / fpatan / fptan / fsqrt / fyl2x, not calls into the CRT. They differ from the UCRT's std::sin & co:
//  * fsin / fcos leave |x| >= 2^63 unchanged (C2 set, no reduction) - the CRT reduces the argument,
//  * the result is a 64-bit-mantissa extended value that stays on the FPU stack and is rounded by the NEXT arithmetic op at the current precision
//    control (PC24 in game mode) or by the store - a CRT call returns a value already rounded to double,
//  * fsin / fcos are only accurate to ~1 extended ulp near multiples of pi / 2 (66-bit pi in the reduction), the CRT is correctly rounded there,
//  * NaN / inf: fsin(inf) = invalid + default NaN, the CRT returns a (quiet) NaN with errno set.
// Every helper takes doubles and RETURNS ITS RESULT UNROUNDED IN ST(0) (naked, cdecl): the caller's following fmul / fadd / fstp rounds it
// exactly like the exe's code does. Do not store the result into a `double` local if the exe keeps it on the stack (the store rounds to 53 bits).
// Outside MSVC x86 (clang, 64-bit) the helpers fall back to <cmath>.
#include <cmath>
#ifdef NOTSA_PORTABLE
#include "X87Soft.h"
#include "X87Lift.h"
#include "X87Sites.h"
#endif

namespace x87 {
#if defined(_MSC_VER) && !defined(__clang__) && defined(_M_IX86)
#define NOTSA_X87_ASM 1
// fld x; fsin
__declspec(naked) inline double __cdecl sin(double) { __asm { fld qword ptr [esp + 4]
                                                              fsin
                                                              ret } }
__declspec(naked) inline double __cdecl cos(double) { __asm { fld qword ptr [esp + 4]
                                                              fcos
                                                              ret } }
// fptan pushes 1.0 on top of tan(x): the compiler pops it (fstp st(0)) right after
__declspec(naked) inline double __cdecl tan(double) { __asm { fld qword ptr [esp + 4]
                                                              fptan
                                                              fstp st(0)
                                                              ret } }
// atan2(y, x): fld y; fld x; fpatan  (fpatan computes atan(st1 / st0))
__declspec(naked) inline double __cdecl atan2(double, double) { __asm { fld qword ptr [esp + 4]
                                                                        fld qword ptr [esp + 12]
                                                                        fpatan
                                                                        ret } }
__declspec(naked) inline double __cdecl sqrt(double) { __asm { fld qword ptr [esp + 4]
                                                               fsqrt
                                                               ret } }
// log2(x) = fld1; fld x; fyl2x      log10(x) = fldlg2; fld x; fyl2x      ln(x) = fldln2; fld x; fyl2x
__declspec(naked) inline double __cdecl log2(double) { __asm { fld1
                                                               fld qword ptr [esp + 4]
                                                               fyl2x
                                                               ret } }
__declspec(naked) inline double __cdecl log10(double) { __asm { fldlg2
                                                                fld qword ptr [esp + 4]
                                                                fyl2x
                                                                ret } }
__declspec(naked) inline double __cdecl ln(double) { __asm { fldln2
                                                             fld qword ptr [esp + 4]
                                                             fyl2x
                                                             ret } }

// atan(x): fld x; fld1; fpatan
__declspec(naked) inline double __cdecl atan(double) { __asm { fld qword ptr [esp + 4]
                                                               fld1
                                                               fpatan
                                                               ret } }

// The CRT's asin / acos (0x821E70 / 0x822380, _CIasin / _CIacos): for |x| < 1 a plain x87 sequence at the CURRENT precision control (so PC24 rounds every step!):
//   asin: atan2(x, sqrt((1 + x) * (1 - x)))    acos: atan2(sqrt((1 + x) * (1 - x)), x)
// |x| == 1 returns the extended constants (+-pi/2, 0 / pi), NaN input is returned quieted, everything else the real indefinite (-nan). errno / matherr side effects omitted.
namespace detail {
// 80-bit constants: pi/2 (0x8E313A) and the real indefinite (0x8E3130)
alignas(16) inline const unsigned char kHalfPi[10]     = { 0x35, 0xC2, 0x68, 0x21, 0xA2, 0xDA, 0x0F, 0xC9, 0xFF, 0x3F };
alignas(16) inline const unsigned char kIndefinite[10] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC0, 0xFF, 0xFF };
}
__declspec(naked) inline double __cdecl asin(double) {
    __asm {
        mov   eax, dword ptr [esp + 8]
        and   eax, 0x7ff00000
        cmp   eax, 0x7ff00000
        je    L_special
        cmp   eax, 0x3ff00000
        jb    L_calc
        ja    L_invalid
        mov   eax, dword ptr [esp + 8]                 // 1 <= |x| < 2
        mov   ecx, eax
        and   eax, 0xfffff
        or    eax, dword ptr [esp + 4]
        jne   L_invalid
        fld   tbyte ptr [detail::kHalfPi]              // |x| == 1: +-pi/2
        test  ecx, 0x80000000
        je    L_done
        fchs
    L_done:
        ret
    L_calc:
        fld   qword ptr [esp + 4]
        fld1
        fadd  st(0), st(1)
        fld1
        fsub  st(0), st(2)
        fmulp st(1), st(0)
        fsqrt
        fpatan
        ret
    L_special:                                         // NaN (mantissa != 0) is returned quieted, +-inf is a domain error
        mov   eax, dword ptr [esp + 8]
        and   eax, 0xfffff
        or    eax, dword ptr [esp + 4]
        je    L_invalid
        fld   qword ptr [esp + 4]
        ret
    L_invalid:
        fld   tbyte ptr [detail::kIndefinite]
        ret
    }
}
__declspec(naked) inline double __cdecl acos(double) {
    __asm {
        mov   eax, dword ptr [esp + 8]
        and   eax, 0x7ff00000
        cmp   eax, 0x7ff00000
        je    L_special
        cmp   eax, 0x3ff00000
        jb    L_calc
        ja    L_invalid
        mov   eax, dword ptr [esp + 8]                 // 1 <= |x| < 2
        mov   ecx, eax
        and   eax, 0xfffff
        or    eax, dword ptr [esp + 4]
        jne   L_invalid
        test  ecx, 0x80000000                          // |x| == 1: x = +1 -> 0, x = -1 -> pi
        je    L_zero
        fldpi
        ret
    L_zero:
        fldz
        ret
    L_calc:
        fld   qword ptr [esp + 4]
        fld1
        fadd  st(0), st(1)
        fld1
        fsub  st(0), st(2)
        fmulp st(1), st(0)
        fsqrt
        fxch  st(1)
        fpatan
        ret
    L_special:
        mov   eax, dword ptr [esp + 8]
        and   eax, 0xfffff
        or    eax, dword ptr [esp + 4]
        je    L_invalid
        fld   qword ptr [esp + 4]
        ret
    L_invalid:
        fld   tbyte ptr [detail::kIndefinite]
        ret
    }
}
// fsincos: sin and cos from one instruction (the cos is the top of the stack afterwards)
inline void sincos(double x, double& s, double& c) {
    double resSin, resCos;
    __asm {
        fld   qword ptr [x]
        fsincos
        fstp  qword ptr [resCos]
        fstp  qword ptr [resSin]
    }
    s = resSin; c = resCos;
}
#elif defined(NOTSA_PORTABLE)
// Portable build (arm64 / x64 / clang): the instructions are reproduced by the integer-only software x87 (X87Soft.h, .notes/reports/F1_soft.md). The arithmetic steps of the CRT
// asin / acos sequences are rounded at the game's precision control (24 bits); the transcendental results are 64 bit values, returned as a round-to-odd double so that the
// caller's NEXT rounding to float (or to 24 bits) is identical to rounding the 80 bit value. Bit-exact vs the Win32 asm at float level (0 mismatches in > 10^7 cases per
// function, F1_soft.md) - at the 80 / 53 bit level 88..99% (the reference FPU is not correctly rounded). tests/portable/f4a_lift_test.cpp: x87:: rows.
#define NOTSA_X87_ASM 0
namespace soft_detail {
inline constexpr int kGamePc = 24;                                 // x87 precision control of the game thread (set by D3D9 CreateDevice)
inline notsa::fp::soft::F80 In(double x) { return notsa::fp::soft::FromDouble(x); }
inline double Out(const notsa::fp::soft::F80& v) { return notsa::fp::soft::ToDoubleRoundToOdd(v); }
// the CRT asin / acos (0x821E70 / 0x822380): |x| < 1 => atan2 of (x, sqrt((1 + x) * (1 - x))) at the current PC; |x| == 1 => +-pi/2 / (0, pi); NaN => quieted NaN; else the real indefinite
inline double AsinAcos(double x, bool isAcos) {
    using namespace notsa::fp::soft;
    const uint64_t bits = std::bit_cast<uint64_t>(x);
    const uint32_t hi = uint32_t(bits >> 32), lo = uint32_t(bits);
    const uint32_t e = hi & 0x7ff00000u;
    const bool mantZero = ((hi & 0xfffffu) | lo) == 0;
    if (e == 0x7ff00000u) return mantZero ? Out(Indefinite()) : Out(FromDouble(x));    // inf: domain error; NaN: returned quieted
    if (e > 0x3ff00000u) return Out(Indefinite());
    if (e == 0x3ff00000u) {                                        // 1 <= |x| < 2
        if (!mantZero) return Out(Indefinite());
        const bool neg = (hi & 0x80000000u) != 0;
        if (!isAcos) return Out(neg ? Neg(kHalfPi) : kHalfPi);
        return Out(neg ? kPi : Zero());
    }
    const F80 xv = FromDouble(x), one = One();
    const F80 r = Sqrt(Mul(Add(one, xv, kGamePc), Sub(one, xv, kGamePc), kGamePc), kGamePc);
    return Out(isAcos ? Atan2(r, xv) : Atan2(xv, r));
}
}
inline double sin(double x)               { return soft_detail::Out(notsa::fp::soft::Sin(soft_detail::In(x))); }
inline double cos(double x)               { return soft_detail::Out(notsa::fp::soft::Cos(soft_detail::In(x))); }
// fptan with |x| >= 2^63 sets C2 and pushes nothing: the helper's `fstp st(0)` then pops the operand and returns an EMPTY stack (real indefinite)
inline double tan(double x)               { bool c2; const auto t = notsa::fp::soft::Tan(soft_detail::In(x), &c2); return soft_detail::Out(c2 ? notsa::fp::soft::Indefinite() : t); }
inline double atan2(double y, double x)   { return soft_detail::Out(notsa::fp::soft::Atan2(soft_detail::In(y), soft_detail::In(x))); }
inline double sqrt(double x)              { return soft_detail::Out(notsa::fp::soft::Sqrt(soft_detail::In(x), soft_detail::kGamePc)); }
inline double atan(double x)              { return soft_detail::Out(notsa::fp::soft::Atan(soft_detail::In(x))); }
inline double asin(double x)              { return soft_detail::AsinAcos(x, false); }
inline double acos(double x)              { return soft_detail::AsinAcos(x, true); }
inline double log2(double x)              { return soft_detail::Out(notsa::fp::soft::Yl2x(notsa::fp::soft::One(), soft_detail::In(x))); }
inline double log10(double x)             { return soft_detail::Out(notsa::fp::soft::Yl2x(notsa::fp::soft::kLg2, soft_detail::In(x))); }
inline double ln(double x)                { return soft_detail::Out(notsa::fp::soft::Yl2x(notsa::fp::soft::kLn2, soft_detail::In(x))); }
// fsincos; fstp qword [cos]; fstp qword [sin]: both results are STORED as doubles (round to nearest, not the round-to-odd of the float-level helpers above); |x| >= 2^63 pushes nothing, so the
// first fstp stores x and the second pops an empty stack (real indefinite). (MAC-F4a-REV: was sin()/cos() = round-to-odd doubles; tests/portable/f4a_lift_test.cpp rows x87::sincos_*)
inline void   sincos(double x, double& s, double& c) {
    const auto r = notsa::fp::soft::SinCos(soft_detail::In(x));
    if (r.c2) { s = notsa::fp::soft::ToDouble(notsa::fp::soft::Indefinite()); c = x; return; }
    s = notsa::fp::soft::ToDouble(r.sin); c = notsa::fp::soft::ToDouble(r.cos);
}
#else
#define NOTSA_X87_ASM 0
inline double sin(double x)               { return std::sin(x); }
inline double cos(double x)               { return std::cos(x); }
inline double tan(double x)               { return std::tan(x); }
inline double atan2(double y, double x)   { return std::atan2(y, x); }
inline double sqrt(double x)              { return std::sqrt(x); }
inline double atan(double x)              { return std::atan(x); }
inline double asin(double x)              { return std::asin(x); }
inline double acos(double x)              { return std::acos(x); }
inline double log2(double x)              { return std::log2(x); }
inline double log10(double x)             { return std::log10(x); }
inline double ln(double x)                { return std::log(x); }
inline void   sincos(double x, double& s, double& c) { s = std::sin(x); c = std::cos(x); }
#endif
// float overloads would silently pick the double ones (promotion), which is what the exe does as well (fld dword): nothing to add.
} // namespace x87
#include "X87Pow.h"   // x87::pow: the CRT pow of the exe (_CIpow 0x822130), bit-exact SSE2 kernel

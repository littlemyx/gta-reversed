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

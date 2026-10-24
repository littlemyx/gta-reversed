#pragma once
// X87Sites.h - portable (soft x87) implementations of the exe's small hand-written x87 routines that are naked asm in the Win32 build (MAC-F4a). Header-only so that the game sources
// (Radar.cpp, AEAudioUtility.cpp) and tests/portable/f4a_lift_test.cpp (which compares them with the original asm under Wine) use the same code.
#include "X87Lift.h"

namespace notsa::fp::sites {

// CRadar's SetRadarAngle / SetRadarAngleAtan2 (0x5837B5 / 0x5837F5): the radar orientation is the angle (atan2 result, still unrounded on the stack) stored as a float; its sin / cos are
// computed from the UNROUNDED value (fld st0; fsin; fstp cachedSin; fcos; fstp cachedCos).
inline void RadarAngleFromTop(lift::Fpu& f, float* orientation, float* sinOut, float* cosOut) {
    *orientation = f.StF32();   // fst dword
    f.LdSt(0);
    f.Sin();
    *sinOut = f.StpF32();       // fstp dword
    f.Cos();
    *cosOut = f.StpF32();
}
inline void RadarAngleAtan2(float y, float x, float* orientation, float* sinOut, float* cosOut) {
    lift::Fpu f(24);
    f.LdF32(y);
    f.LdF32(x);
    f.Patan();
    RadarAngleFromTop(f, orientation, sinOut, cosOut);
}
inline void RadarAngle(float a, float* orientation, float* sinOut, float* cosOut) {
    lift::Fpu f(24);
    f.LdF32(a);
    RadarAngleFromTop(f, orientation, sinOut, cosOut);
}

// CAEAudioUtility::AudioLog10 (0x4D9E50): `p < min` or NaN (fcomp unordered sets C0) -> `floor`, else fldlg2; fld p; fyl2x. The exe leaves the UNROUNDED 64 bit result in ST0
// (every caller multiplies by 10 / 20 and rounds once); this returns it rounded to float (F-4 TODO: not bit-exact in the caller's product, see f4a_lift_test AudioLog10_x* rows).
inline float AudioLog10(float p, float min, float floor) {
    lift::Fpu f(24);
    f.LdF32(p);
    f.ComF32(min, 1);
    if (f.Sw() & 0x0100) { return floor; }
    f.Ldlg2();
    f.LdF32(p);
    f.Yl2x();
    return f.StF32();
}

// ---- routines that exist as MSVC-x86-only inline asm next to a libm fallback (IdleCam, BoneNode, PostEffects, AEPedAudioEntity, AEVehicleAudioEntity): the same instruction sequences
// on the soft x87 (PC24). `test ah, 5; jp skip` executes the following fchs only when C0 xor C2 is set (ST0 < operand; unordered sets both).
namespace detail { inline bool ChsAfterTest5(uint16_t sw) { const unsigned r = (sw >> 8) & 5u; return r == 1u || r == 4u; } }

//! IdleCam.cpp X87SinP1Half: (fsin(x) + 1) * 0.5 with the extended sine unrounded until the `fadd` (PC24), stored as a double
inline double SinP1Half(double x) {
    lift::Fpu f(24);
    f.LdF64(x);
    f.Sin();
    f.ArithF32(lift::Op::Add, 1.0f);
    f.ArithF32(lift::Op::Mul, 0.5f);
    return f.StpF64();
}
//! BoneNode.cpp AtanDeg: atan2(y, x) * 180 * (1/pi) (float constants), fpatan unrounded into the first fmul
inline float AtanDeg(double y, double x) {
    lift::Fpu f(24);
    f.LdF64(y);
    f.LdF64(x);
    f.Patan();
    f.ArithF32(lift::Op::Mul, 180.0f);
    f.ArithF32(lift::Op::Mul, std::bit_cast<float>(0x3EA2F983u));
    return f.StpF32();
}
//! PostEffects.cpp ExeRasterLog2: ftol(ln(w) / ln(2)) with both logarithms as fyl2x(ln2, .) values and the fdiv rounded at PC24; _ftol truncation (low dword; NaN / overflow -> 0)
inline int ExeRasterLog2(int w) {
    lift::Fpu f(24);
    f.Ldln2();
    f.Ld1();
    f.ArithSt(lift::Op::Add, 0, 0);          // 2.0
    f.Yl2x();                                 // ln2 * log2(2)
    f.Ldln2();
    f.LdI32(w);
    f.Yl2x();                                 // ln2 * log2(w)
    f.ArithSt(lift::Op::Div, 0, 1);          // st0 = st0 / st1
    const float q = f.StF32();                // the quotient is a 24 bit value after the PC24 fdiv
    if (!(q > -2147483648.0f && q < 2147483648.0f)) { return 0; }
    return (int)q;
}
//! AEPedAudioEntity.cpp JetPackSinSpeeds: |fsin(angle)| * factor + 1 from the UNROUNDED sine (speed0), |float(fsin)| from the float copy (speed1)
inline void JetPackSinSpeeds(float angle, float factor, float& speed0, float& speed1) {
    lift::Fpu f(24);
    f.LdF32(angle);
    f.Sin();
    f.ComF32(0.0f);
    const float tmp = f.StF32();              // fst dword
    if (detail::ChsAfterTest5(f.Sw())) { f.Chs(); }
    f.ArithF32(lift::Op::Mul, factor);
    f.ArithF32(lift::Op::Add, 1.0f);
    speed0 = f.StpF32();
    f.LdF32(tmp);
    f.ComF32(0.0f, 1);                        // fcomp
    f.LdF32(tmp);
    if (detail::ChsAfterTest5(f.Sw())) { f.Chs(); }
    speed1 = f.StpF32();
}
//! AEVehicleAudioEntity.cpp BikeLeanTerm: |fsin(lean)| * factor, the sine unrounded into the fmul
inline float BikeLeanTerm(float lean, float factor) {
    lift::Fpu f(24);
    f.LdF32(lean);
    f.Sin();
    f.ComF32(0.0f);
    if (detail::ChsAfterTest5(f.Sw())) { f.Chs(); }
    f.ArithF32(lift::Op::Mul, factor);
    return f.StpF32();
}

} // namespace notsa::fp::sites

// x87-exact helpers of the exe's inlined RenderWare macros, shared by rtquat.cpp and hanim.cpp (RpHAnimKeyFrameBlend inlines the same RwACos / RwSin
// polynomials as RtQuatSetupSlerpCache). Constants are the exe's .rdata values; every float local / (float) cast is a store to memory in the exe, the
// double temporaries are x87 registers (rounded to the 24-bit precision control the game runs with).
#pragma once
#include <bit>
#include <cmath>
#include <cstdint>

#include "fakerw.h"
#include "rwmath_exact.h"

namespace rwtrig {
inline double Sqrt(float x) { return (double)rwx::Sqrt(x); }   // RwSqrt 0x7EDB30: the exe's table based approximation (01r2; bit-exact vs the exe, rw_math_stream_test)
constexpr uint32_t Bits(float f) { return std::bit_cast<uint32_t>(f); }
constexpr float    FromBits(uint32_t u) { return std::bit_cast<float>(u); }

// constants from the exe's .rdata
constexpr float K_0p5  = 0.5f;
constexpr float K_1    = 1.0f;
constexpr float K_2    = 2.0f;
constexpr float kDegToHalfRad = FromBits(0x3c0efa35u);   // 0x8631D4  pi / 360
// acos approximation (FreeBSD e_acosf.c: pS0..pS5 / qS1..qS4, pio2_hi/lo), inlined in RtQuatSetupSlerpCache
constexpr float K_pS0 = FromBits(0x3e2aaaabu);   // 0x85F0A0  1/6
constexpr float K_pS1 = FromBits(0x3ea6b090u);   // 0x874E80
constexpr float K_pS2 = FromBits(0x3e4e0aa8u);   // 0x874E84
constexpr float K_pS3 = FromBits(0x3d241146u);   // 0x874E88
constexpr float K_pS4 = FromBits(0x3a4f7f04u);   // 0x874E8C
constexpr float K_pS5 = FromBits(0x3811ef08u);   // 0x874E90
constexpr float K_qS1 = FromBits(0x4019d139u);   // 0x874E70
constexpr float K_qS2 = FromBits(0x3f303361u);   // 0x874E78
constexpr float K_qS3 = FromBits(0x4001572du);   // 0x874E74
constexpr float K_qS4 = FromBits(0x3d9dc62eu);   // 0x874E7C
constexpr float K_pio2_lo  = FromBits(0x33a22168u);   // 0x874E6C
constexpr float K_pio2_hi  = FromBits(0x3fc90fdau);   // 0x874E68
constexpr float K_pi_hi    = FromBits(0x40490fdau);   // 0x874E64  (2 * pio2_hi)
constexpr uint32_t kPiBits    = 0x40490fdbu;
constexpr uint32_t kPio2Bits  = 0x3fc90fdbu;
constexpr float K_nearlyOne   = FromBits(0x3f7fff58u);   // 0x871258  0.99999
// sin minimax coefficients (rwplcore.h _RW_S1.._RW_S6), as loaded by the exe
constexpr float K_S6 = FromBits(0x2f2ec9d3u);   // 0x86D2B0
constexpr float K_S5 = FromBits(0x32d72f34u);   // 0x86D2AC (subtracted: sign folded)
constexpr float K_S4 = FromBits(0x3638ef1bu);   // 0x86D2A8
constexpr float K_S3 = FromBits(0x39500d01u);   // 0x86D2A4 (subtracted)
constexpr float K_S2 = FromBits(0x3c088889u);   // 0x864E0C

// numerator / denominator of the acos rational approximation r(z) = p(z) / q(z) (z in float memory, evaluated in double like the exe)
inline double AcosRatio(double z) {
    const double p = (((((K_pS5 * z + K_pS4) * z - K_pS3) * z + K_pS2) * z - K_pS1) * z + K_pS0) * z;
    const double q = ((((z * K_qS4 - K_qS2) * z + K_qS3) * z - K_qS1) * z + K_1);
    return p / q;
}

// RwSinMinusPiToPiMacro as the exe evaluates it (z = x*x, sin = x + (poly * (z*x)))
inline double SinMinusPiToPi(double x) {
    const double z = x * x;
    const double poly = (((((K_S6 * z - K_S5) * z + K_S4) * z - K_S3) * z + K_S2) * z) - K_pS0;
    return x + poly * (z * x);
}

// RwACos as inlined in the exe (FreeBSD e_acosf.c structure on the bit pattern of `c`), result as the x87 register holds it (float precision).
inline float ACos(float c) {
    const uint32_t cb   = Bits(c);
    const uint32_t absb = cb & 0x7fffffffu;
    if (absb >= 0x3f800000u) {   // |c| >= 1 (and NaN)
        return ((int32_t)cb > 0) ? 0.0f : FromBits(kPiBits);
    }
    if (absb < 0x3f000000u) {   // |c| < 0.5
        if (absb <= 0x23000000u) {
            return FromBits(kPio2Bits);
        }
        const double x = c;
        const double r = AcosRatio(x * x);
        return (float)((double)K_pio2_hi - ((double)c - ((double)K_pio2_lo - r * c)));
    }
    if ((int32_t)cb < 0) {   // c <= -0.5
        const float  z = (float)(((double)c + (double)K_1) * (double)K_0p5);
        const double s = Sqrt(z);
        const double r = AcosRatio(z);
        const double w = r * s - (double)K_pio2_lo;
        return (float)((double)K_pi_hi - ((w + s) + (w + s)));   // the exe computes (r * s - pio2_lo) + s, doubles it and subtracts from pi
    }
    // c >= 0.5
    const float  z  = (float)(((double)K_1 - (double)c) * (double)K_0p5);
    const double s  = Sqrt(z);
    const float  df = FromBits(Bits((float)s) & 0xfffff000u);
    const double r  = AcosRatio(z);
    const double cc = ((double)z - (double)df * df) / ((double)df + s);
    const double v  = (r * s + cc) + df;
    return (float)(v + v);
}
} // namespace rwtrig

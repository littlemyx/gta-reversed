// P2B-05c: RtQuat* (rtquat.h / rtslerp.h) for the standalone build.
//   RtQuatConvertFromMatrix 0x7EB5C0 (+ branch bodies 0x7EB6A0 / 0x7EB700 / 0x7EB760), RtQuatRotate 0x7EB7C0, RtQuatTransformVectors 0x7EBBB0,
//   RtQuatSetupSlerpCache 0x7EC220: real exe functions, ported from the asm.
//   RtQuatConvertToMatrix / RtQuatUnitConvertToMatrix / RtQuatSlerp: macros in the SDK headers (the exe has no function for them); the game's call sites
//   inline them. Ported from those inlinings: UnitConvertToMatrix from RtAnimBlendKeyFrameApply 0x4D5FA0, ConvertToMatrix from
//   CPhysical::PositionAttachedEntity 0x5471CF.., Slerp from BoneNode_c::BlendKeyframe 0x616F08...
// Every function was lifted from the x87 code with a small symbolic lifter (the SSA temporaries below are the x87 registers: `double`, i.e. no rounding;
// `float` locals / `(float)` casts are the points where the exe stores to memory). The game sets the FPU precision control to 24 bits at start-up (D7), under which
// the double temporaries round to float anyway; the explicit form here keeps the port exact on a plain SSE2 build too.
// RwSqrt / RwV3dNormalize are the exe's table-based RW approximations (7EDB30 / 7ED9B0, relative error ~1e-4): rwx::Sqrt (rwmath_exact.h) here, RwV3dNormalize
// from math.cpp. The exe calls them at 0x7EB5F9 / 0x7EB6BC / 0x7EB71C / 0x7EB77C (ConvertFromMatrix), 0x7EB85D / 0x7EB94C / 0x7EBA1B (Rotate, normalize) and
// 0x7EC3DB / 0x7EC47F (SetupSlerpCache). Verified bit-exact against the exe's code by tests/standalone/rw_math_stream_test.cpp (RW_EXE_ORACLE).
#ifdef NOTSA_RW_LIBRW
#include "fakerw.h"
#include "rwtrig.h"

#include <bit>
#include <cmath>
#include <cstdint>

namespace {
using namespace rwtrig;
}

// ---------------------------------------------------------------------------------------------------------------------------------
// 0x7EB5C0. Returns 0 for a NULL argument (the exe raises an RW error first).
RwBool RtQuatConvertFromMatrix(RtQuat* const qpQuat, const RwMatrix* const mpMatrix) {
    if (!qpQuat || !mpMatrix) {
        return FALSE;
    }
    const RwMatrix* m = mpMatrix;
    const x87d trace = ((x87d)m->at.z + m->right.x) + m->up.y;
    if (trace > 0.0) {
        const float  arg = (float)(trace + (x87d)K_1);
        const x87d s   = Sqrt(arg);
        qpQuat->real     = (float)((x87d)K_0p5 * s);
        const x87d f   = (x87d)K_0p5 / s;
        qpQuat->imag.x   = (float)(((x87d)m->up.z - m->at.y) * f);
        qpQuat->imag.y   = (float)(((x87d)m->at.x - m->right.z) * f);
        qpQuat->imag.z   = (float)(((x87d)m->right.y - m->up.x) * f);
        return TRUE;
    }
    // largest diagonal element
    if (m->right.x > m->up.y) {
        if (m->right.x > m->at.z) {   // 0x7EB6A0: x is the largest
            const float  arg = (float)((((x87d)m->right.x - ((x87d)m->at.z + m->up.y)) + (x87d)K_1));
            const x87d s   = Sqrt(arg);
            qpQuat->imag.x   = (float)((x87d)K_0p5 * s);
            const x87d f   = (x87d)K_0p5 / s;
            qpQuat->real     = (float)(((x87d)m->up.z - m->at.y) * f);
            qpQuat->imag.y   = (float)(((x87d)m->up.x + m->right.y) * f);
            qpQuat->imag.z   = (float)(((x87d)m->at.x + m->right.z) * f);
            return TRUE;
        }
    } else if (m->up.y > m->at.z) {   // 0x7EB700: y is the largest
        const float  arg = (float)((((x87d)m->up.y - ((x87d)m->at.z + m->right.x)) + (x87d)K_1));
        const x87d s   = Sqrt(arg);
        qpQuat->imag.y   = (float)((x87d)K_0p5 * s);
        const x87d f   = (x87d)K_0p5 / s;
        qpQuat->real     = (float)(((x87d)m->at.x - m->right.z) * f);
        qpQuat->imag.z   = (float)(((x87d)m->at.y + m->up.z) * f);
        qpQuat->imag.x   = (float)(((x87d)m->up.x + m->right.y) * f);
        return TRUE;
    }
    {   // 0x7EB760: z is the largest (also the fall-through of the x / y tests)
        const float  arg = (float)((((x87d)m->at.z - ((x87d)m->up.y + m->right.x)) + (x87d)K_1));
        const x87d s   = Sqrt(arg);
        qpQuat->imag.z   = (float)((x87d)K_0p5 * s);
        const x87d f   = (x87d)K_0p5 / s;
        qpQuat->real     = (float)(((x87d)m->right.y - m->up.x) * f);
        qpQuat->imag.x   = (float)(((x87d)m->at.x + m->right.z) * f);
        qpQuat->imag.y   = (float)(((x87d)m->at.y + m->up.z) * f);
        return TRUE;
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------
// 0x7EB7C0. angle in degrees. NULL quat / axis or an unknown combine op: NULL (the exe raises an RW error first).
RtQuat* RtQuatRotate(RtQuat* quat, const RwV3d* axis, RwReal angle, RwOpCombineType combineOp) {
    if (!quat || !axis) {
        return nullptr;
    }
    const x87d h = (x87d)angle * kDegToHalfRad;
    switch (combineOp) {
    case rwCOMBINEREPLACE: {   // 0x7EBA02
        const float sinA = (float)std::sin(X87W(h));
        quat->real = (float)std::cos(X87W(h));
        RwV3dNormalize(&quat->imag, axis);
        quat->imag.x = (float)((x87d)sinA * quat->imag.x);
        quat->imag.y = (float)((x87d)quat->imag.y * sinA);
        quat->imag.z = (float)((x87d)quat->imag.z * sinA);
        return quat;
    }
    case rwCOMBINEPRECONCAT:   // 0x7EB911: q = q * r
    case rwCOMBINEPOSTCONCAT: { // 0x7EB822: q = r * q
        const float sinA = (float)std::sin(X87W(h));
        const float cosA = (float)std::cos(X87W(h));
        const float Qx = quat->imag.x, Qy = quat->imag.y, Qz = quat->imag.z, Qw = quat->real;
        RwV3d       N;
        RwV3dNormalize(&N, axis);
        const x87d Sx = (x87d)sinA * N.x;
        const x87d Sy = (x87d)N.y * sinA;
        const x87d Sz = (x87d)N.z * sinA;
        if (combineOp == rwCOMBINEPOSTCONCAT) {
            const x87d w = (x87d)Qw * cosA;
            const x87d t5 = (x87d)Qy * Sy, t6 = (x87d)Qx * Sx;
            const x87d t8 = (x87d)Qz * Sz;
            quat->real     = (float)(w - ((t5 + t6) + t8));
            const x87d x = (((x87d)Qz * Sy - Sz * Qy) + Sx * Qw) + (x87d)Qx * cosA;
            quat->imag.x   = (float)x;
            const x87d y = (((x87d)Qx * Sz - (x87d)Qz * Sx) + (x87d)Qy * cosA) + Sy * Qw;
            quat->imag.y   = (float)y;
            const x87d z = (((x87d)Qy * Sx - Sy * Qx) + Sz * Qw) + (x87d)Qz * cosA;
            quat->imag.z   = (float)z;
        } else {
            const x87d w = (x87d)cosA * Qw;
            const x87d t5 = Sy * Qy, t6 = Sx * Qx;
            const x87d t8 = Sz * Qz;
            quat->real     = (float)(w - ((t5 + t6) + t8));
            const x87d x = (((x87d)Qy * Sz - (x87d)Qz * Sy) + (x87d)Qx * cosA) + Sx * Qw;
            quat->imag.x   = (float)x;
            const x87d y = ((((x87d)Qz * Sx - Sz * Qx) + (x87d)Qy * cosA)) + Sy * Qw;
            quat->imag.y   = (float)y;
            const x87d z = (((Sy * Qx - (x87d)Qy * Sx) + (x87d)Qz * cosA)) + Sz * Qw;
            quat->imag.z   = (float)z;
        }
        return quat;
    }
    default:
        return nullptr;
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------
// 0x7EBBB0. out may alias in (the exe reads each input component at the point the code below does; the order matters for the aliased case).
RwV3d* RtQuatTransformVectors(RwV3d* vectorsOut, const RwV3d* vectorsIn, const RwInt32 numPoints, const RtQuat* quat) {
    const x87d qx = quat->imag.x, qy = quat->imag.y, qz = quat->imag.z, qw = quat->real;
    const float  k   = (float)(qw * qw - ((qx * qx + qy * qy) + qz * qz));
    const float  x2  = (float)(qx + qx);
    const float  y2  = (float)(qy + qy);
    const float  z2  = (float)(qz + qz);
    const float  wx2 = (float)(qw * x2);
    const float  wy2 = (float)(qw * y2);
    const float  wz2 = (float)(qw * z2);
    for (RwInt32 i = 0; i < numPoints; i++) {
        const RwV3d* in  = &vectorsIn[i];
        RwV3d*       out = &vectorsOut[i];
        const x87d d   = ((x87d)in->y * y2 + (x87d)x2 * in->x) + (x87d)in->z * z2;
        const float  cx  = (float)((x87d)in->z * wy2 - (x87d)in->y * wz2);
        const float  cy  = (float)((x87d)wz2 * in->x - (x87d)in->z * wx2);
        const float  cz  = (float)((x87d)in->y * wx2 - (x87d)wy2 * in->x);
        out->x = cx;
        out->y = cy;
        out->z = cz;
        out->x = (float)(d * quat->imag.x + out->x);
        out->y = (float)((x87d)quat->imag.y * d + out->y);
        out->z = (float)((x87d)quat->imag.z * d + out->z);
        out->x = (float)((x87d)k * in->x + out->x);
        out->y = (float)((x87d)in->y * k + out->y);
        out->z = (float)((x87d)in->z * k + out->z);
    }
    return vectorsOut;
}

// ---------------------------------------------------------------------------------------------------------------------------------
// 0x7EC220 (RtQuatSetupSlerpCache; the acos and sin polynomials are inlined in the exe).
void RtQuatSetupSlerpCache(RtQuat* qpFrom, RtQuat* qpTo, RtQuatSlerpCache* sCache) {
    sCache->raFrom = *qpFrom;
    const x87d dot = (((x87d)qpFrom->imag.y * qpTo->imag.y + (x87d)qpFrom->imag.x * qpTo->imag.x) + (x87d)qpFrom->imag.z * qpTo->imag.z) +
                       (x87d)qpFrom->real * qpTo->real;
    float c = (float)dot;
    if (dot < 0.0) {
        c = (c < -1.0f) ? 1.0f : -c;
        sCache->raTo.real   = -qpTo->real;
        sCache->raTo.imag.x = -qpTo->imag.x;
        sCache->raTo.imag.y = -qpTo->imag.y;
        sCache->raTo.imag.z = -qpTo->imag.z;
    } else {   // also NaN
        if (c > 1.0f) {
            c = 1.0f;
        }
        sCache->raTo = *qpTo;
    }

    const float omega = ACos(c);
    sCache->omega = omega;

    const bool nearlyZero = !(c < K_nearlyOne) && !std::isnan(c);   // fcomp C0 is also set for unordered
    sCache->nearlyZeroOm  = nearlyZero ? 1 : 0;
    if (!nearlyZero) {
        const x87d x = omega;
        const x87d z = x * x;
        const x87d poly = (((((K_S6 * z - K_S5) * z + K_S4) * z - K_S3) * z + K_S2) * z) - K_pS0;
        const x87d sinOm = x + poly * (z * x);
        const x87d scale = (x87d)K_1 / sinOm;
        sCache->raFrom.real   = (float)(scale * sCache->raFrom.real);
        sCache->raFrom.imag.x = (float)(scale * sCache->raFrom.imag.x);
        sCache->raFrom.imag.y = (float)((x87d)sCache->raFrom.imag.y * scale);
        sCache->raFrom.imag.z = (float)((x87d)sCache->raFrom.imag.z * scale);
        sCache->raTo.real     = (float)(scale * sCache->raTo.real);
        sCache->raTo.imag.x   = (float)((x87d)sCache->raTo.imag.x * scale);
        sCache->raTo.imag.y   = (float)((x87d)sCache->raTo.imag.y * scale);
        sCache->raTo.imag.z   = (float)((x87d)sCache->raTo.imag.z * scale);
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------
// RtQuatSlerpMacro as inlined in BoneNode_c::BlendKeyframe (0x616F08..0x617039)
void RtQuatSlerp(RtQuat* qpResult, const RtQuat* qpFrom, const RtQuat* qpTo, RwReal rT, RtQuatSlerpCache* sCache) {
    if (rT <= 0.0f) {
        *qpResult = *qpFrom;
    } else if (1.0f <= rT) {
        *qpResult = *qpTo;
    } else {
        x87d sclFrom = (x87d)K_1 - rT;
        x87d sclTo   = rT;
        if (!sCache->nearlyZeroOm) {
            sclFrom = SinMinusPiToPi(sclFrom * sCache->omega);
            sclTo   = SinMinusPiToPi((x87d)sCache->omega * rT);
        }
        qpResult->imag.x = (float)(sCache->raFrom.imag.x * sclFrom);
        qpResult->imag.y = (float)(sCache->raFrom.imag.y * sclFrom);
        qpResult->imag.z = (float)(sCache->raFrom.imag.z * sclFrom);
        qpResult->imag.x = (float)(sCache->raTo.imag.x * sclTo + qpResult->imag.x);
        qpResult->imag.y = (float)(sCache->raTo.imag.y * sclTo + qpResult->imag.y);
        qpResult->imag.z = (float)(sCache->raTo.imag.z * sclTo + qpResult->imag.z);
        qpResult->real   = (float)(sCache->raTo.real * sclTo + sCache->raFrom.real * sclFrom);
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------
// RtQuatUnitConvertToMatrix, from RtAnimBlendKeyFrameApply (0x4D5FA0): spills x*x, y*y, z*z, y*z, z*x, x*y, w*x to float; w*y and w*z stay in registers.
void RtQuatUnitConvertToMatrix(const RtQuat* qpQuat, RwMatrix* mpMatrix) {
    const float x = qpQuat->imag.x, y = qpQuat->imag.y, z = qpQuat->imag.z, w = qpQuat->real;
    const float sqx = (float)((x87d)x * x);
    const float sqy = (float)((x87d)y * y);
    const float sqz = (float)((x87d)z * z);
    const float crx = (float)((x87d)y * z);   // cross.x
    const float cry = (float)((x87d)z * x);   // cross.y
    const float crz = (float)((x87d)x * y);   // cross.z
    const float wx  = (float)((x87d)w * x);
    const x87d wy = (x87d)w * y;
    const x87d wz = (x87d)w * z;
    const x87d t19 = (x87d)sqz + sqy;
    mpMatrix->right.x = (float)((x87d)K_1 - (t19 + t19));
    const x87d t22 = (x87d)crz + wz;
    mpMatrix->right.y = (float)(t22 + t22);
    const x87d t24 = (x87d)cry - wy;
    mpMatrix->right.z = (float)(t24 + t24);
    const x87d t26 = (x87d)crz - wz;
    mpMatrix->up.x = (float)(t26 + t26);
    const x87d t28 = (x87d)sqz + sqx;
    mpMatrix->up.y = (float)((x87d)K_1 - (t28 + t28));
    const x87d t31 = (x87d)wx + crx;
    mpMatrix->up.z = (float)(t31 + t31);
    const x87d t33 = wy + cry;
    mpMatrix->at.x = (float)(t33 + t33);
    const x87d t35 = (x87d)crx - wx;
    mpMatrix->at.y = (float)(t35 + t35);
    const x87d t37 = (x87d)sqy + sqx;
    mpMatrix->at.z = (float)((x87d)K_1 - (t37 + t37));
    mpMatrix->pos.x = 0.0f;
    mpMatrix->pos.y = 0.0f;
    mpMatrix->pos.z = 0.0f;
    mpMatrix->flags = 3;
}

// RtQuatConvertToMatrix, from CPhysical::PositionAttachedEntity (0x5471CF..0x54734B)
void RtQuatConvertToMatrix(const RtQuat* qpQuat, RwMatrix* mpMatrix) {
    const float x = qpQuat->imag.x, y = qpQuat->imag.y, z = qpQuat->imag.z, w = qpQuat->real;
    const x87d modSq = (((x87d)x * x + (x87d)y * y) + (x87d)z * z) + (x87d)w * w;
    const x87d rS    = (x87d)K_2 / modSq;
    const x87d vx    = rS * x;                    // rV (x and z stay in registers, y is spilled)
    const float  vy    = (float)(rS * y);
    const x87d vz    = rS * z;
    const float  wx    = (float)(vx * w);           // rW
    const float  wy    = (float)((x87d)vy * w);
    const float  wz    = (float)(vz * w);
    const float  sqx   = (float)(vx * x);           // square
    const float  sqy   = (float)((x87d)vy * y);
    const float  sqz   = (float)(vz * z);
    const float  cx    = (float)(vz * y);           // cross.x = y * rV.z
    const x87d cy    = vx * z;                    // cross.y = z * rV.x
    const x87d cz    = (x87d)vy * x;            // cross.z = x * rV.y
    mpMatrix->right.x = (float)((x87d)K_1 - ((x87d)sqz + sqy));
    mpMatrix->right.y = (float)((x87d)wz + cz);
    mpMatrix->right.z = (float)(cy - wy);
    mpMatrix->up.x    = (float)(cz - wz);
    mpMatrix->up.y    = (float)((x87d)K_1 - ((x87d)sqz + sqx));
    mpMatrix->up.z    = (float)((x87d)cx + wx);
    mpMatrix->at.x    = (float)((x87d)wy + cy);
    mpMatrix->at.y    = (float)((x87d)cx - wx);
    mpMatrix->at.z    = (float)((x87d)K_1 - ((x87d)sqy + sqx));
    mpMatrix->pos.x = 0.0f;
    mpMatrix->pos.y = 0.0f;
    mpMatrix->pos.z = 0.0f;
    mpMatrix->flags = 3;
}
#endif // NOTSA_RW_LIBRW

// P2B-05c: RtQuat* (rtquat.h / rtslerp.h) for the standalone build.
//   RtQuatConvertFromMatrix 0x7EB5C0 (+ branch bodies 0x7EB6A0 / 0x7EB700 / 0x7EB760), RtQuatRotate 0x7EB7C0, RtQuatTransformVectors 0x7EBBB0,
//   RtQuatSetupSlerpCache 0x7EC220: real exe functions, ported from the asm.
//   RtQuatConvertToMatrix / RtQuatUnitConvertToMatrix / RtQuatSlerp: macros in the SDK headers (the exe has no function for them); the game's call sites
//   inline them. Ported from those inlinings: UnitConvertToMatrix from RtAnimBlendKeyFrameApply 0x4D5FA0, ConvertToMatrix from
//   CPhysical::PositionAttachedEntity 0x5471CF.., Slerp from BoneNode_c::BlendKeyframe 0x616F08...
// Every function was lifted from the x87 code with a small symbolic lifter (the SSA temporaries below are the x87 registers: `double`, i.e. no rounding;
// `float` locals / `(float)` casts are the points where the exe stores to memory). The game sets the FPU precision control to 24 bits at start-up (D7), under which
// the double temporaries round to float anyway; the explicit form here keeps the port exact on a plain SSE2 build too.
// Deliberate difference: the exe's RwSqrt / RwV3dNormalize are the table-based RW approximations (7EDB30 / 7ED9B0, tables built at engine start); the shim uses the
// exact square root like math.cpp's RwV3dNormalize (known fidelity TODO D8: ~1e-4 relative error in the original).
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
    const double trace = ((double)m->at.z + m->right.x) + m->up.y;
    if (trace > 0.0) {
        const float  arg = (float)(trace + (double)K_1);
        const double s   = Sqrt(arg);
        qpQuat->real     = (float)((double)K_0p5 * s);
        const double f   = (double)K_0p5 / s;
        qpQuat->imag.x   = (float)(((double)m->up.z - m->at.y) * f);
        qpQuat->imag.y   = (float)(((double)m->at.x - m->right.z) * f);
        qpQuat->imag.z   = (float)(((double)m->right.y - m->up.x) * f);
        return TRUE;
    }
    // largest diagonal element
    if (m->right.x > m->up.y) {
        if (m->right.x > m->at.z) {   // 0x7EB6A0: x is the largest
            const float  arg = (float)((((double)m->right.x - ((double)m->at.z + m->up.y)) + (double)K_1));
            const double s   = Sqrt(arg);
            qpQuat->imag.x   = (float)((double)K_0p5 * s);
            const double f   = (double)K_0p5 / s;
            qpQuat->real     = (float)(((double)m->up.z - m->at.y) * f);
            qpQuat->imag.y   = (float)(((double)m->up.x + m->right.y) * f);
            qpQuat->imag.z   = (float)(((double)m->at.x + m->right.z) * f);
            return TRUE;
        }
    } else if (m->up.y > m->at.z) {   // 0x7EB700: y is the largest
        const float  arg = (float)((((double)m->up.y - ((double)m->at.z + m->right.x)) + (double)K_1));
        const double s   = Sqrt(arg);
        qpQuat->imag.y   = (float)((double)K_0p5 * s);
        const double f   = (double)K_0p5 / s;
        qpQuat->real     = (float)(((double)m->at.x - m->right.z) * f);
        qpQuat->imag.z   = (float)(((double)m->at.y + m->up.z) * f);
        qpQuat->imag.x   = (float)(((double)m->up.x + m->right.y) * f);
        return TRUE;
    }
    {   // 0x7EB760: z is the largest (also the fall-through of the x / y tests)
        const float  arg = (float)((((double)m->at.z - ((double)m->up.y + m->right.x)) + (double)K_1));
        const double s   = Sqrt(arg);
        qpQuat->imag.z   = (float)((double)K_0p5 * s);
        const double f   = (double)K_0p5 / s;
        qpQuat->real     = (float)(((double)m->right.y - m->up.x) * f);
        qpQuat->imag.x   = (float)(((double)m->at.x + m->right.z) * f);
        qpQuat->imag.y   = (float)(((double)m->at.y + m->up.z) * f);
        return TRUE;
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------
// 0x7EB7C0. angle in degrees. NULL quat / axis or an unknown combine op: NULL (the exe raises an RW error first).
RtQuat* RtQuatRotate(RtQuat* quat, const RwV3d* axis, RwReal angle, RwOpCombineType combineOp) {
    if (!quat || !axis) {
        return nullptr;
    }
    const double h = (double)angle * kDegToHalfRad;
    switch (combineOp) {
    case rwCOMBINEREPLACE: {   // 0x7EBA02
        const float sinA = (float)std::sin(h);
        quat->real = (float)std::cos(h);
        RwV3dNormalize(&quat->imag, axis);
        quat->imag.x = (float)((double)sinA * quat->imag.x);
        quat->imag.y = (float)((double)quat->imag.y * sinA);
        quat->imag.z = (float)((double)quat->imag.z * sinA);
        return quat;
    }
    case rwCOMBINEPRECONCAT:   // 0x7EB911: q = q * r
    case rwCOMBINEPOSTCONCAT: { // 0x7EB822: q = r * q
        const float sinA = (float)std::sin(h);
        const float cosA = (float)std::cos(h);
        const float Qx = quat->imag.x, Qy = quat->imag.y, Qz = quat->imag.z, Qw = quat->real;
        RwV3d       N;
        RwV3dNormalize(&N, axis);
        const double Sx = (double)sinA * N.x;
        const double Sy = (double)N.y * sinA;
        const double Sz = (double)N.z * sinA;
        if (combineOp == rwCOMBINEPOSTCONCAT) {
            const double w = (double)Qw * cosA;
            const double t5 = (double)Qy * Sy, t6 = (double)Qx * Sx;
            const double t8 = (double)Qz * Sz;
            quat->real     = (float)(w - ((t5 + t6) + t8));
            const double x = (((double)Qz * Sy - Sz * Qy) + Sx * Qw) + (double)Qx * cosA;
            quat->imag.x   = (float)x;
            const double y = (((double)Qx * Sz - (double)Qz * Sx) + (double)Qy * cosA) + Sy * Qw;
            quat->imag.y   = (float)y;
            const double z = (((double)Qy * Sx - Sy * Qx) + Sz * Qw) + (double)Qz * cosA;
            quat->imag.z   = (float)z;
        } else {
            const double w = (double)cosA * Qw;
            const double t5 = Sy * Qy, t6 = Sx * Qx;
            const double t8 = Sz * Qz;
            quat->real     = (float)(w - ((t5 + t6) + t8));
            const double x = (((double)Qy * Sz - (double)Qz * Sy) + (double)Qx * cosA) + Sx * Qw;
            quat->imag.x   = (float)x;
            const double y = ((((double)Qz * Sx - Sz * Qx) + (double)Qy * cosA)) + Sy * Qw;
            quat->imag.y   = (float)y;
            const double z = (((Sy * Qx - (double)Qy * Sx) + (double)Qz * cosA)) + Sz * Qw;
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
    const double qx = quat->imag.x, qy = quat->imag.y, qz = quat->imag.z, qw = quat->real;
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
        const double d   = ((double)in->y * y2 + (double)x2 * in->x) + (double)in->z * z2;
        const float  cx  = (float)((double)in->z * wy2 - (double)in->y * wz2);
        const float  cy  = (float)((double)wz2 * in->x - (double)in->z * wx2);
        const float  cz  = (float)((double)in->y * wx2 - (double)wy2 * in->x);
        out->x = cx;
        out->y = cy;
        out->z = cz;
        out->x = (float)(d * quat->imag.x + out->x);
        out->y = (float)((double)quat->imag.y * d + out->y);
        out->z = (float)((double)quat->imag.z * d + out->z);
        out->x = (float)((double)k * in->x + out->x);
        out->y = (float)((double)in->y * k + out->y);
        out->z = (float)((double)in->z * k + out->z);
    }
    return vectorsOut;
}

// ---------------------------------------------------------------------------------------------------------------------------------
// 0x7EC220 (RtQuatSetupSlerpCache; the acos and sin polynomials are inlined in the exe).
void RtQuatSetupSlerpCache(RtQuat* qpFrom, RtQuat* qpTo, RtQuatSlerpCache* sCache) {
    sCache->raFrom = *qpFrom;
    const double dot = (((double)qpFrom->imag.y * qpTo->imag.y + (double)qpFrom->imag.x * qpTo->imag.x) + (double)qpFrom->imag.z * qpTo->imag.z) +
                       (double)qpFrom->real * qpTo->real;
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
        const double x = omega;
        const double z = x * x;
        const double poly = (((((K_S6 * z - K_S5) * z + K_S4) * z - K_S3) * z + K_S2) * z) - K_pS0;
        const double sinOm = x + poly * (z * x);
        const double scale = (double)K_1 / sinOm;
        sCache->raFrom.real   = (float)(scale * sCache->raFrom.real);
        sCache->raFrom.imag.x = (float)(scale * sCache->raFrom.imag.x);
        sCache->raFrom.imag.y = (float)((double)sCache->raFrom.imag.y * scale);
        sCache->raFrom.imag.z = (float)((double)sCache->raFrom.imag.z * scale);
        sCache->raTo.real     = (float)(scale * sCache->raTo.real);
        sCache->raTo.imag.x   = (float)((double)sCache->raTo.imag.x * scale);
        sCache->raTo.imag.y   = (float)((double)sCache->raTo.imag.y * scale);
        sCache->raTo.imag.z   = (float)((double)sCache->raTo.imag.z * scale);
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
        double sclFrom = (double)K_1 - rT;
        double sclTo   = rT;
        if (!sCache->nearlyZeroOm) {
            sclFrom = SinMinusPiToPi(sclFrom * sCache->omega);
            sclTo   = SinMinusPiToPi((double)sCache->omega * rT);
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
    const float sqx = (float)((double)x * x);
    const float sqy = (float)((double)y * y);
    const float sqz = (float)((double)z * z);
    const float crx = (float)((double)y * z);   // cross.x
    const float cry = (float)((double)z * x);   // cross.y
    const float crz = (float)((double)x * y);   // cross.z
    const float wx  = (float)((double)w * x);
    const double wy = (double)w * y;
    const double wz = (double)w * z;
    const double t19 = (double)sqz + sqy;
    mpMatrix->right.x = (float)((double)K_1 - (t19 + t19));
    const double t22 = (double)crz + wz;
    mpMatrix->right.y = (float)(t22 + t22);
    const double t24 = (double)cry - wy;
    mpMatrix->right.z = (float)(t24 + t24);
    const double t26 = (double)crz - wz;
    mpMatrix->up.x = (float)(t26 + t26);
    const double t28 = (double)sqz + sqx;
    mpMatrix->up.y = (float)((double)K_1 - (t28 + t28));
    const double t31 = (double)wx + crx;
    mpMatrix->up.z = (float)(t31 + t31);
    const double t33 = wy + cry;
    mpMatrix->at.x = (float)(t33 + t33);
    const double t35 = (double)crx - wx;
    mpMatrix->at.y = (float)(t35 + t35);
    const double t37 = (double)sqy + sqx;
    mpMatrix->at.z = (float)((double)K_1 - (t37 + t37));
    mpMatrix->pos.x = 0.0f;
    mpMatrix->pos.y = 0.0f;
    mpMatrix->pos.z = 0.0f;
    mpMatrix->flags = 3;
}

// RtQuatConvertToMatrix, from CPhysical::PositionAttachedEntity (0x5471CF..0x54734B)
void RtQuatConvertToMatrix(const RtQuat* qpQuat, RwMatrix* mpMatrix) {
    const float x = qpQuat->imag.x, y = qpQuat->imag.y, z = qpQuat->imag.z, w = qpQuat->real;
    const double modSq = (((double)x * x + (double)y * y) + (double)z * z) + (double)w * w;
    const double rS    = (double)K_2 / modSq;
    const double vx    = rS * x;                    // rV (x and z stay in registers, y is spilled)
    const float  vy    = (float)(rS * y);
    const double vz    = rS * z;
    const float  wx    = (float)(vx * w);           // rW
    const float  wy    = (float)((double)vy * w);
    const float  wz    = (float)(vz * w);
    const float  sqx   = (float)(vx * x);           // square
    const float  sqy   = (float)((double)vy * y);
    const float  sqz   = (float)(vz * z);
    const float  cx    = (float)(vz * y);           // cross.x = y * rV.z
    const double cy    = vx * z;                    // cross.y = z * rV.x
    const double cz    = (double)vy * x;            // cross.z = x * rV.y
    mpMatrix->right.x = (float)((double)K_1 - ((double)sqz + sqy));
    mpMatrix->right.y = (float)((double)wz + cz);
    mpMatrix->right.z = (float)(cy - wy);
    mpMatrix->up.x    = (float)(cz - wz);
    mpMatrix->up.y    = (float)((double)K_1 - ((double)sqz + sqx));
    mpMatrix->up.z    = (float)((double)cx + wx);
    mpMatrix->at.x    = (float)((double)wy + cy);
    mpMatrix->at.y    = (float)((double)cx - wx);
    mpMatrix->at.z    = (float)((double)K_1 - ((double)sqy + sqx));
    mpMatrix->pos.x = 0.0f;
    mpMatrix->pos.y = 0.0f;
    mpMatrix->pos.z = 0.0f;
    mpMatrix->flags = 3;
}
#endif // NOTSA_RW_LIBRW

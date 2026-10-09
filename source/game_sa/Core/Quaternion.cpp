/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/

#include "StdInc.h"

#include <numbers>

#include "Quaternion.h"

void CQuaternion::InjectHooks() {
    RH_ScopedClass(CQuaternion);
    RH_ScopedCategory("Core");

    RH_ScopedOverloadedInstall(Get, "", 0x59C080, void (CQuaternion::*)(RwMatrix*) const);
    RH_ScopedOverloadedInstall(Get, "Euler", 0x59C160, void (CQuaternion::*)(float*, float*, float*));
    RH_ScopedOverloadedInstall(Get, "AxisAngle", 0x59C230, void (CQuaternion::*)(RwV3d*, float*));
    RH_ScopedInstall(Multiply, 0x59C270);
    RH_ScopedOverloadedInstall(Slerp, "Core", 0x59C300, void (CQuaternion::*)(const CQuaternion&, const CQuaternion&, float, float, float));
    RH_ScopedOverloadedInstall(Set, "Matrix", 0x59C3E0, void (CQuaternion::*)(const RwMatrix&));
    RH_ScopedOverloadedInstall(Set, "Euler", 0x59C530, void (CQuaternion::*)(float, float, float));
    RH_ScopedOverloadedInstall(Set, "AxisAngle", 0x59C600, void (CQuaternion::*)(RwV3d*, float));
    RH_ScopedOverloadedInstall(Slerp, "", 0x59C630, void (CQuaternion::*)(const CQuaternion&, const CQuaternion&, float));
    RH_ScopedInstall(Conjugate, 0x4D37D0);
    RH_ScopedInstall(Scale, 0x4CF9B0);
    RH_ScopedInstall(Copy, 0x4CF9E0);
}

// Quat to matrix
void CQuaternion::Get(RwMatrix* out) const {
    auto vecImag2 = imag + imag;
    auto x2x = vecImag2.x * imag.x;
    auto y2x = vecImag2.y * imag.x;
    auto z2x = vecImag2.z * imag.x;

    auto y2y = vecImag2.y * imag.y;
    auto z2y = vecImag2.z * imag.y;
    auto z2z = vecImag2.z * imag.z;

    auto x2r = vecImag2.x * real;
    auto y2r = vecImag2.y * real;
    auto z2r = vecImag2.z * real;

    CVector right{1.0F - (z2z + y2y), z2r + y2x, z2x - y2r}, up{y2x - z2r, 1.0F - (z2z + x2x), x2r + z2y}, at{y2r + z2x, z2y - x2r, 1.0F - (y2y + x2x)};
    RwV3dAssign(RwMatrixGetRight(out), &right);
    RwV3dAssign(RwMatrixGetUp(out), &up);
    RwV3dAssign(RwMatrixGetAt(out), &at);
}

// Quat to euler angles
void CQuaternion::Get(float* x, float* y, float* z) { // 0x59C160
    RwMatrix m; // Original only fills right/up/at here (stack garbage elsewhere)
    Get(&m);

    constexpr double TWO_PI = 2.0f * std::numbers::pi_v<float>; // 0x858CBC

    // Stores `angle` as float, adds 2pi if it's negative (compared at full precision, as on the x87 stack)
    const auto StoreWrapped = [](float* out, double angle) {
        *out = (float)angle;
        if (angle < 0.0) {
            *out = (float)(angle + TWO_PI);
        }
    };

    const double rx = m.right.x, ry = m.right.y, rz = m.right.z;
    const double ux = m.up.x, uy = m.up.y, uz = m.up.z;
    const double ay = m.at.y;

    StoreWrapped(z, std::atan2(ry, uy));
    const double sz = std::sin((double)*z), cz = std::cos((double)*z); // Re-read from memory (float)

    StoreWrapped(x, std::atan2(-ay, sz * ry + cz * uy));

    // = -(rz * cz - uz * sz), = rx * cz - ux * sz
    StoreWrapped(y, std::atan2(-(rz * cz - uz * sz), rx * cz - ux * sz));
}

// Quat to axis & angle
void CQuaternion::Get(RwV3d* axis, float* angle) { // 0x59C230
    const double theta = std::acos((double)w + (double)w);
    *angle = (float)theta;
    const double k = 1.0 / std::sin(theta);
    axis->x = (float)(k * x);
    axis->y = (float)(k * y);
    axis->z = (float)(k * z);
}

// Stores result of quat multiplication
void CQuaternion::Multiply(const CQuaternion& a, const CQuaternion& b) { // 0x59C270
    // NOTE: Original writes `this` progressively and re-reads a/b, so aliasing behaviour is kept
    x = (float)((double)b.z * a.y - (double)a.z * b.y);
    y = (float)((double)a.z * b.x - (double)a.x * b.z);
    z = (float)((double)a.x * b.y - (double)b.x * a.y);

    x = (float)(((double)b.w * a.x + (double)a.w * b.x) + x);
    y = (float)(((double)a.w * b.y + (double)b.w * a.y) + y);
    z = (float)(((double)a.z * b.w + (double)a.w * b.z) + z);
    w = (float)((double)a.w * b.w - (((double)a.x * b.x + (double)a.z * b.z) + (double)a.y * b.y));
}

// Spherical linear interpolation
void CQuaternion::Slerp(const CQuaternion& from, const CQuaternion& to, float halftheta, float sintheta_inv, float t) { // 0x59C300
    if (halftheta == 0.0f) { // fcomp 0; test ah, 0x44; jp => NaN goes to the interpolation below, as in the original
        Copy(to);
        return;
    }

    // The exe's code verbatim (fsin is the x87 instruction: arguments beyond 2^63 are left unchanged, a CRT sin() would reduce them; every product / sum is rounded at the
    // current precision control, the two weights stay on the FPU stack)
    static const float halfPi = std::numbers::pi_v<float> / 2.0f; // 0x858FE4
    static const float pi     = std::numbers::pi_v<float>;        // 0x858CB8
    static const float one    = 1.0f;                             // 0x858624
    CQuaternion*       self   = this;
    const CQuaternion* pFrom  = &from;
    const CQuaternion* pTo    = &to;
    __asm {
        fld   dword ptr [halftheta]
        fcomp dword ptr [halfPi]
        fnstsw ax
        test  ah, 0x41
        jne   L_normal
        fld   dword ptr [pi]
        fsub  dword ptr [halftheta]
        fstp  dword ptr [halftheta]
        fld   dword ptr [one]
        fsub  dword ptr [t]
        fmul  dword ptr [halftheta]
        fsin
        fmul  dword ptr [sintheta_inv]
        fld   dword ptr [halftheta]
        fmul  dword ptr [t]
        fsin
        fmul  dword ptr [sintheta_inv]
        fchs
        jmp   L_blend
    L_normal:
        fld   dword ptr [one]
        fsub  dword ptr [t]
        fmul  dword ptr [halftheta]
        fsin
        fmul  dword ptr [sintheta_inv]
        fld   dword ptr [halftheta]
        fmul  dword ptr [t]
        fsin
        fmul  dword ptr [sintheta_inv]
    L_blend:                       // st0 = b (weight of `to`), st1 = a (weight of `from`)
        mov   eax, pFrom
        fld   st(1)
        fmul  dword ptr [eax]
        mov   edx, pTo
        fld   st(1)
        fmul  dword ptr [edx]
        faddp st(1), st(0)
        mov   ecx, self
        fstp  dword ptr [ecx]
        fld   st(0)
        fmul  dword ptr [edx + 4]
        fld   st(2)
        fmul  dword ptr [eax + 4]
        faddp st(1), st(0)
        fstp  dword ptr [ecx + 4]
        fld   st(0)
        fmul  dword ptr [edx + 8]
        fld   st(2)
        fmul  dword ptr [eax + 8]
        faddp st(1), st(0)
        fstp  dword ptr [ecx + 8]
        fmul  dword ptr [edx + 0xC]
        fxch  st(1)
        fmul  dword ptr [eax + 0xC]
        faddp st(1), st(0)
        fstp  dword ptr [ecx + 0xC]
    }
}

// Quat from matrix
void CQuaternion::Set(const RwMatrix& m) { // 0x59C3E0
    const double rx = m.right.x, ry = m.right.y, rz = m.right.z;
    const double ux = m.up.x, uy = m.up.y, uz = m.up.z;
    const double ax = m.at.x, ay = m.at.y, az = m.at.z;

    if (const double tr = uy + rx + az; tr >= 0.0) {
        const double s = std::sqrt(tr + 1.0);
        const double k = 0.5 / s;
        w = (float)(0.5 * s);
        x = (float)((uz - ay) * k);
        y = (float)((ax - rz) * k);
        z = (float)((ry - ux) * k);
        return;
    }
    if (const double t = rx - uy - az; t >= 0.0) {
        const double s = std::sqrt(t + 1.0);
        const double k = 0.5 / s;
        x = (float)(0.5 * s);
        y = (float)((ux + ry) * k);
        z = (float)((ax + rz) * k);
        w = (float)((uz - ay) * k);
        return;
    }
    if (const double t = uy - rx - az; t >= 0.0) {
        const double s = std::sqrt(t + 1.0);
        const double k = 0.5 / s;
        y = (float)(0.5 * s);
        w = (float)((ax - rz) * k);
        x = (float)((ux - ry) * k);
        z = (float)((ay + uz) * k);
        return;
    }
    {
        const double s = std::sqrt((az - (uy + rx)) + 1.0);
        const double k = 0.5 / s;
        z = (float)(0.5 * s);
        w = (float)((ry - ux) * k);
        x = (float)((ax + rz) * k);
        y = (float)((ay + uz) * k);
    }
}

// Quat from euler angles
void CQuaternion::Set(float ex, float ey, float ez) { // 0x59C530
    const double a1 = (double)ex * 0.5f, a2 = (double)ey * 0.5f, a3 = (double)ez * 0.5f;
    const double ca1 = std::cos(a1), ca2 = std::cos(a2);
    const float  c3 = (float)std::cos(a3);
    const float  s1 = (float)std::sin(a1);
    const float  s2 = (float)std::sin(a2);
    const float  s3 = (float)std::sin(a3);

    const float P = (float)(ca2 * ca1);
    const float Q = s2 * s1; // product of two floats, spilled to float
    const float R = (float)(s1 * ca2);
    const double S = ca1 * s2;

    w = (float)((double)Q * s3 + (double)P * c3);
    x = (float)((double)P * s3 - (double)Q * c3);
    y = (float)((double)s3 * S + (double)R * c3);
    z = (float)(S * c3 - (double)R * s3);
}

// Quat from axis & angle
void CQuaternion::Set(RwV3d* axis, float angle) { // 0x59C600
    const double half = (double)angle * 0.5f;
    const double s = std::sin(half);
    x = (float)(s * axis->x);
    y = (float)(s * axis->y);
    z = (float)(s * axis->z);
    w = (float)std::cos(half);
}

// Spherical linear interpolation
void CQuaternion::Slerp(const CQuaternion& from, const CQuaternion& to, float t) { // 0x59C630
    // 0x4D00E0 (cdecl: from, to, float* theta, float* sinThetaInv), then 0x59C300
    // NOTE: 0x4D00E0 is a cdecl helper (a, b, float* theta, float* invSinTheta); it is hooked as `CalcThetaFromQuats` in AnimBlendNode.cpp (CAnimBlendNode::CalcTheta is a NOTSA wrapper around it), not here.
    float halftheta, sintheta_inv;
    CalcThetaFromQuats(from, to, halftheta, sintheta_inv);
    Slerp(from, to, halftheta, sintheta_inv, t);
}

void CQuaternion::CalcThetaFromQuats(const CQuaternion& a, const CQuaternion& b, float& theta, float& invSinTheta) { // 0x4D00E0
    // Dot is accumulated in extended precision in this order (w, z, y, x), stored as float
    float dot = (float)(((double)a.w * b.w + (double)a.z * b.z + (double)a.y * b.y) + (double)b.x * a.x);
    if (dot > 1.0f) { // Original: FCOMP + `test ah, 0x41; jne` => clamp only if dot > 1 (NaN is not clamped)
        dot = 1.0f;
    }
    // The CRT acos (0x82239D): |x| < 1 => atan2(sqrt((1 + x) * (1 - x)), x) on the x87 stack; x == 1 => 0, x == -1 => pi (fldpi), anything else (|x| > 1, NaN) => the
    // negative QNaN constant at 0x8E3130 (including +-inf); a NaN input is returned as is (quieted, payload and sign kept)
    const float ad   = std::fabs(dot);
    const int   mode = ad < 1.0f ? 0 : dot == 1.0f ? 1 : dot == -1.0f ? 2 : dot != dot ? 4 : 3;
    static const unsigned char qnan[10] = { 0, 0, 0, 0, 0, 0, 0, 0xC0, 0xFF, 0xFF };
    static const float         zero     = 0.0f; // 0x858B50
    static const float         one      = 1.0f; // 0x858624
    float*                     pTheta   = &theta;
    float*                     pInv     = &invSinTheta;
    __asm {
        fld   dword ptr [dot]
        mov   eax, mode
        test  eax, eax
        jne   L_special
        fld1
        fadd  st(0), st(1)
        fld1
        fsub  st(0), st(2)
        fmulp st(1), st(0)
        fsqrt
        fxch  st(1)
        fpatan
        jmp   L_have
    L_special:
        cmp   eax, 4
        je    L_have
        fstp  st(0)
        cmp   eax, 1
        jne   L_not1
        fldz
        jmp   L_have
    L_not1:
        cmp   eax, 2
        jne   L_nan
        fldpi
        jmp   L_have
    L_nan:
        fld   tbyte ptr [qnan]
    L_have:
        fld   st(0)
        mov   eax, pTheta
        fstp  dword ptr [eax]
        fcom  dword ptr [zero]
        fnstsw ax
        test  ah, 0x44
        jnp   L_zero
        fsin
        mov   ecx, pInv
        fdivr dword ptr [one]
        fstp  dword ptr [ecx]
        jmp   L_done
    L_zero:
        fstp  st(0)
        mov   ecx, pInv
        mov   dword ptr [ecx], 0
    L_done:
    }
}

// Conjugate of a quat
void CQuaternion::Conjugate() { // 0x4D37D0
    x = -x;
    y = -y;
    z = -z;
}

// Squared length of a quat
float CQuaternion::GetLengthSquared() const {
    // Originally NOP.
    return sq(x) + sq(y) + sq(z) + sq(w);
}

// Multiplies quat by a floating point value
void CQuaternion::Scale(float multiplier) { // 0x4CF9B0
    x = multiplier * x;
    y = multiplier * y;
    z = multiplier * z;
    w = multiplier * w;
}

// Copies value from other quat
void CQuaternion::Copy(const CQuaternion& from) { // 0x4CF9E0
    x = from.x;
    y = from.y;
    z = from.z;
    w = from.w;
}

// Gets a dot product for quats
float CQuaternion::Dot(const CQuaternion& rhs) {
    return this->w * rhs.w + this->z * rhs.z + this->y * rhs.y + this->x * rhs.x;
}

// Normalises a quat
void CQuaternion::Normalise() {
    const auto sqMag = GetLengthSquared();
    if (sqMag == 0.f) {
        w = 1.0;
    } else {
        *this = *this / std::sqrt(sqMag);
    }
}

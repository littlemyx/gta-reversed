/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/
#include "StdInc.h"
#include "Vector2D.h"
#include "Vector.h"
#include "General.h"

void CVector2D::InjectHooks() {
    RH_ScopedClass(CVector2D);
    RH_ScopedCategory("Core");

    RH_ScopedInstall(Magnitude, 0x420860);
    //RH_ScopedInstall(Normalise, 0x44E480); - Added extra parameter, must unhook
    RH_ScopedInstall(operator=, 0x43E110);
}

CVector2D::CVector2D(const CVector& v3) :
    CVector2D{v3.x, v3.y}
{
}

// 0x44E480 (not hooked: the port has the extra `outMag` parameter)
// The exe keeps the squared length and the reciprocal length unrounded on the x87 stack (same as CVector::Normalise),
// the zero test is `sqMag <= 0` on the SQUARED length with NaN NOT handled (goes on to the sqrt path => NaN components),
// the components are only rounded when stored.
void CVector2D::Normalise(float* outMag) {
    const x87d sqMag = (x87d)x * x + (x87d)y * y;
    if (outMag) {
        *outMag = (float)std::sqrt(sqMag);
    }
    if (sqMag <= 0.0) {
        x = 1.0f;
        return;
    }
    const x87d recip = (x87d)1.0 / std::sqrt(sqMag);
    x = (float)(x * recip);
    y = (float)(y * recip);
}

uint32 CVector2D::NodeHeading() const {
    return CGeneral::GetNodeHeadingFromVector(x, y);
}

CVector2D CVector2D::RotatedBy(float rad) const {
    const auto s = x87::sin(rad), c = x87::cos(rad);
    return {
        (float)(X87R(x * c) - X87R(y * s)),
        (float)(X87R(x * s) - X87R(y * c)),
    };
}

bool CVector2D::ApproxEqualTo(CVector2D o, float epsilon) {
    return approxEqual(x, o.x, epsilon) && approxEqual(y, o.y, epsilon);
}

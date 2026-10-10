/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/

#include "StdInc.h"

#include "Vector.h"
#include "Vector2D.h"

void CVector::InjectHooks()
{
    RH_ScopedClass(CVector);
    RH_ScopedCategory("Core");

    RH_ScopedInstall(Magnitude, 0x4082C0);
    RH_ScopedInstall(Magnitude2D, 0x406D50);
    RH_ScopedInstall(Normalise, 0x59C910);
    RH_ScopedInstall(NormaliseAndMag, 0x59C970);
    RH_ScopedInstall(Cross_OG, 0x70F890);
    RH_ScopedInstall(Sum, 0x40FDD0);
    RH_ScopedInstall(Difference, 0x40FE00);
    RH_ScopedInstall(FromMultiply, 0x59C670);
    RH_ScopedInstall(FromMultiply3x3, 0x59C6D0);
    RH_ScopedGlobalOverloadedInstall(CrossProduct, "out", 0x59C730, CVector*(*)(CVector*, CVector*, CVector*));
    RH_ScopedGlobalOverloadedInstall(DotProduct, "v3d*v3d*", 0x40FDB0, float(*)(CVector*, CVector*));
}

/*!
* @brief From a 2D vector and Z position
*/
CVector::CVector(const CVector2D& v2d, float Z) :
    RwV3d{ v2d.x, v2d.y, Z }
{
}

/*!
* @returns A vector with each of its components set to a number in the given range [min, max)
*/
CVector CVector::Random(CVector min, CVector max) {
    const auto Get = [=](float fmin, float fmax) { return CGeneral::GetRandomNumberInRange(fmin, fmax); };
    return { Get(min.x, max.x), Get(min.y, max.y), Get(min.z, max.z) };
}

CVector CVector::Random(float min, float max) {
    const auto Get = [=] { return CGeneral::GetRandomNumberInRange(min, max); };
    return { Get(), Get(), Get() };
}

// 0x4082C0 - Returns length of vector
// The original squares, adds and takes the root in extended precision (x87, term order ((x*x + y*y) + z*z)) and only the caller rounds to float
float CVector::Magnitude() const
{
    return (float)std::sqrt(((double)x * x + (double)y * y) + (double)z * z);
}

// 0x406D50 (see Magnitude for the evaluation)
float CVector::Magnitude2D() const
{
    return (float)std::sqrt((double)x * x + (double)y * y);
}

// 0x59C910
// Normalises a vector
// NOTE: The original keeps the squared magnitude and the reciprocal length unrounded on the x87 stack (extended precision),
//       components are only rounded when stored => `double` intermediates, fixed term order (x, y, z)
void CVector::Normalise()
{
    const double sqMag = (double)x * x + (double)y * y + (double)z * z;
    if (sqMag <= 0.0) // NaN is NOT handled here (goes on to the division)
    {
        x = 1.0F;
        return;
    }

    const double recip = 1.0 / std::sqrt(sqMag);
    x = (float)(x * recip);
    y = (float)(y * recip);
    z = (float)(z * recip);
}

// 0x59C970
// Normalises a vector and returns length (see `Normalise` for the precision notes)
float CVector::NormaliseAndMag()
{
    const double sqMag = (double)x * x + (double)y * y + (double)z * z;
    if (sqMag <= 0.0)
    {
        x = 1.0F;
        return 1.0F;
    }

    const double recip = 1.0 / std::sqrt(sqMag);
    x = (float)(x * recip);
    y = (float)(y * recip);
    z = (float)(z * recip);

    return (float)(1.0 / recip);
}

auto CVector::Dot(const CVector& o) const -> float{
    return DotProduct(*this, o);
}

auto CVector::Dot2D(const CVector& o) const -> float {
    return y * o.y
         + x * o.x;
}

// notsa
CVector CVector::Cross(const CVector& o) const {
    return {
        y * o.z - z * o.y,
        z * o.x - x * o.z,
        x * o.y - y * o.x
    };
}

// 0x70F890
void CVector::Cross_OG(const CVector& a, const CVector& b) {
    *this = a.Cross(b);
}

// Adds left + right and stores result
void CVector::Sum(const CVector& left, const CVector &right)
{
    x = left.x + right.x;
    y = left.y + right.y;
    z = left.z + right.z;
}

// Subtracts left - right and stores result
void CVector::Difference(const CVector& left, const CVector &right)
{
    x = left.x - right.x;
    y = left.y - right.y;
    z = left.z - right.z;
}

// Adds value from the second vector.
void CVector::operator+=(const CVector& right)
{
    x += right.x;
    y += right.y;
    z += right.z;
}

void CVector::operator-=(const CVector& right)
{
    x -= right.x;
    y -= right.y;
    z -= right.z;
}

void CVector::operator*=(const CVector& right)
{
    x *= right.x;
    y *= right.y;
    z *= right.z;
}

// Multiplies vector by a floating point value
void CVector::operator *= (float multiplier)
{
    x *= multiplier;
    y *= multiplier;
    z *= multiplier;
}

// Divides vector by a floating point value
void CVector::operator /= (float divisor)
{
    x /= divisor;
    y /= divisor;
    z /= divisor;
}

void CVector::FromMultiply(const CMatrix& matrix, const CVector& vector) {
    *this = matrix.TransformPoint(vector);
}

void CVector::FromMultiply3x3(const CMatrix& matrix, const CVector& vector) {
    *this = matrix.TransformVector(vector);
}

float CVector::Heading(bool limitAngle) const {
    const auto radians = x87::atan2(-x, y);
    if (limitAngle) {
        return CGeneral::LimitRadianAngle(radians);
    }
    return radians;
}

CVector* CrossProduct(CVector* out, CVector* a, CVector* b)
{
    *out = a->Cross(*b);
    return out;
}

CVector CrossProduct(const CVector& a, const CVector& b)
{
    return a.Cross(b);
}

float DotProduct(CVector* v1, CVector* v2)
{
    return v1->z * v2->z + v1->y * v2->y + v1->x * v2->x;
}

float DotProduct(const CVector& v1, const CVector& v2)
{
    return v1.z * v2.z + v1.y * v2.y + v1.x * v2.x;
}

float DotProduct2D(const CVector& v1, const CVector& v2)
{
    return v1.y * v2.y + v1.x * v2.x;
}

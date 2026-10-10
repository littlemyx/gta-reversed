#include "StdInc.h"

#include "ColSphere.h"

void CColSphere::InjectHooks() {
    RH_ScopedClass(CColSphere);
    RH_ScopedCategory("Collision");

    RH_ScopedInstall(Set, 0x40FD10);
    RH_ScopedInstall(IntersectRay, 0x40FF20);
    RH_ScopedInstall(IntersectEdge, 0x4100E0);
    RH_ScopedInstall(IntersectPoint, 0x410040);
    RH_ScopedInstall(IntersectSphere, 0x410090);
}

// 0x40FD10
void CColSphere::Set(float radius, const CVector& center, eSurfaceType material, uint8 pieceType, tColLighting lighting) {
    m_fRadius = radius;
    m_vecCenter = center;
    m_Surface.m_nMaterial = material;
    m_Surface.m_nPiece = pieceType;
    m_Surface.m_nLighting = lighting;
}

// 0x40FF20
bool CColSphere::IntersectRay(const CVector& rayOrigin, const CVector& direction, CVector& intersectPoint1, CVector& intersectPoint2) {
    // The exe keeps the distance on the x87 stack (no float rounding of the components' exponent range) and sums in z, x, y order (0x40FF49..)
    const double dx = (double)rayOrigin.x - m_vecCenter.x;
    const double dy = (double)rayOrigin.y - m_vecCenter.y;
    const double dz = (double)rayOrigin.z - m_vecCenter.z;
    const float  b  = (float)(2.0 * (dx * direction.x + dz * direction.z + dy * direction.y));   // stored to a float (0x40FF5F)
    const float  c  = (float)((dz * dz + dx * dx + dy * dy) - (double)m_fRadius * m_fRadius);     // stored to a float (0x40FF7D)
    float t0 = 0.0f, t1 = 0.0f;
    if (CGeneral::SolveQuadratic(1.0f, b, c, t0, t1)) {
        intersectPoint1 = (t0 * direction) + rayOrigin;
        intersectPoint2 = (t1 * direction) + rayOrigin;
        return true;
    }
    return false;
}

// 0x4100E0
bool CColSphere::IntersectEdge(const CVector& startPoint, const CVector& endPoint, CVector& intersectPoint1, CVector& intersectPoint2) {
    const CVector oc{ startPoint.x - m_vecCenter.x, startPoint.y - m_vecCenter.y, startPoint.z - m_vecCenter.z };   // stored as floats (0x4100F8..)
    CVector rayDirection{ endPoint.x - startPoint.x, endPoint.y - startPoint.y, endPoint.z - startPoint.z };
    const float rayLength = (float)std::sqrt((double)rayDirection.z * rayDirection.z + (double)rayDirection.y * rayDirection.y + (double)rayDirection.x * rayDirection.x); // 0x410130: z, y, x
    rayDirection.Normalise();
    // 0x410153: `a` (= dir . dir) is not used by the exe (the direction is normalised); b / c / the discriminant live on the x87 stack, summed in z, x, y order
    const double b = 2.0 * ((double)rayDirection.z * oc.z + (double)rayDirection.x * oc.x + (double)rayDirection.y * oc.y);
    const double c = ((double)oc.z * oc.z + (double)oc.x * oc.x + (double)oc.y * oc.y) - (double)m_fRadius * m_fRadius;
    const double discriminant = b * b - c * 4.0;
    if (discriminant < 0.0) {
        return false;
    }

    const double discriminantSquareRoot = std::sqrt(discriminant);
    const float numerator2 = (float)((discriminantSquareRoot - b) * 0.5);
    const float numerator1 = (float)(((-b) - discriminantSquareRoot) * 0.5);
    if (numerator1 > rayLength || numerator2 < 0.0f) {
        return false;
    }

    intersectPoint2 = endPoint;
    if (numerator2 < rayLength) {
        intersectPoint2 = (rayDirection * numerator2) + startPoint;
    }

    intersectPoint1 = startPoint;
    if (numerator1 > 0.0f) {
        intersectPoint1 = (rayDirection * numerator1) + startPoint;
    }
    return true;
}

// 0x410040
bool CColSphere::IntersectPoint(const CVector& point) {
    CVector distance = m_vecCenter - point;
    return m_fRadius * m_fRadius > distance.SquaredMagnitude();
}

// 0x410090
bool CColSphere::IntersectSphere(const CColSphere& right) const {
    // 0x410090: everything stays on the x87 stack (extended exponent range); the squared distance is summed in z, x, y order
    const double dx = (double)m_vecCenter.x - right.m_vecCenter.x;
    const double dy = (double)m_vecCenter.y - right.m_vecCenter.y;
    const double dz = (double)m_vecCenter.z - right.m_vecCenter.z;
    const double radii = (double)right.m_fRadius + m_fRadius;
    return radii * radii > dz * dz + dx * dx + dy * dy;
}

auto TransformObject(const CColSphere& sp, const CMatrix& transform) -> CColSphere {
    return CColSphere{
        TransformObject(static_cast<const CSphere&>(sp), transform),
        sp.m_Surface
    };
}

#include "StdInc.h"

#include "ColTrianglePlane.h"
#include "ColTriangle.h"

void CColTrianglePlane::InjectHooks()
{
    RH_ScopedClass(CColTrianglePlane);
    RH_ScopedCategory("Collision");

    RH_ScopedOverloadedInstall(GetNormal, "Out", 0x411610, void(CColTrianglePlane::*)(CVector&));
    RH_ScopedInstall(Set, 0x411660);
}

CColTrianglePlane::CColTrianglePlane(const CColTriangle& tri, const CompressedVector* vertices) :
    CColTrianglePlane{
        vertices[tri.vA],
        vertices[tri.vB],
        vertices[tri.vC]
    }
{
}

CColTrianglePlane::CColTrianglePlane(const CStoredCollPoly& poly) :
    CColTrianglePlane{
        poly.verts[0],
        poly.verts[1],
        poly.verts[2]
    }
{
}

CColTrianglePlane::CColTrianglePlane(const CVector& vA, const CVector& vB, const CVector& vC) {
    const auto normal = (vC - vA).Cross(vB - vA).Normalized();

    m_normal = normal;

    // 0x411660: from here on the exe works with the QUANTIZED normal (int16 / 4096), not with the float normal: the plane offset is
    // (vA.z * nz + vA.y * ny) + vA.x * nx, evaluated in the order z, y, x and the dominant axis / sign are taken from the quantized components
    const float qx = m_normal.x, qy = m_normal.y, qz = m_normal.z;
    m_normalOffset = vA.z * qz + vA.y * qy + vA.x * qx;

    m_orientation = [&, this] {
        const auto nx = std::abs(qx);
        const auto ny = std::abs(qy);
        const auto nz = std::abs(qz);

        using enum Orientation;
        if (nx > ny && nx > nz) {
            return qx <= 0.0f ? NEG_X : POS_X;
        }
        if (ny > nz) {
            return qy <= 0.0f ? NEG_Y : POS_Y;
        }
        return qz <= 0.0f ? NEG_Z : POS_Z;
    }();
} 

void CColTrianglePlane::GetNormal(CVector& out) {
    out = GetNormal();
}

void CColTrianglePlane::Set(const CompressedVector* vertices, const CColTriangle& triangle) {
    *this = CColTrianglePlane{ triangle, vertices };
}

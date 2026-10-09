#include "StdInc.h"

#include "FxFrustumInfo.h"

void FxFrustumInfo_c::InjectHooks() {
    RH_ScopedClass(FxFrustumInfo_c);
    RH_ScopedCategory("Fx");

    RH_ScopedInstall(IsCollision, 0x4AA030);
}

// 0x4AA030
bool FxFrustumInfo_c::IsCollision(FxSphere_c& sphere) {
    // NOTE: Inlined `FxSphere_c::IsCollision` (0x4A9FC0), but all calculated in extended precision (z, y, x term order)
    {
        const double dx = (double)sphere.m_vecCenter.x - (double)m_Sphere.m_vecCenter.x;
        const double dy = (double)sphere.m_vecCenter.y - (double)m_Sphere.m_vecCenter.y;
        const double dz = (double)sphere.m_vecCenter.z - (double)m_Sphere.m_vecCenter.z;
        const double r  = (double)m_Sphere.m_fRadius + (double)sphere.m_fRadius;
        if (!(r * r > dz * dz + dy * dy + dx * dx)) {
            return false;
        }
    }

    // Start with the plane that rejected the sphere last time (no early out - tries all 4 planes, but starts at the last rejecting one)
    auto planeIdx = sphere.m_nNumPlanesPassed;
    for (auto i = 0; i < 4; i++, planeIdx++) {
        const auto& plane = m_Planes[planeIdx & 3];
        const double dist = (double)plane.normal.y * (double)sphere.m_vecCenter.y
                          + (double)plane.normal.x * (double)sphere.m_vecCenter.x
                          + (double)plane.normal.z * (double)sphere.m_vecCenter.z
                          - (double)plane.distance;
        if (dist > (double)sphere.m_fRadius) {
            sphere.m_nNumPlanesPassed = planeIdx & 3;
            return false;
        }
    }
    return true;
}

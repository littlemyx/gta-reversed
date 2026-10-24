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
        const x87d dx = (x87d)sphere.m_vecCenter.x - (x87d)m_Sphere.m_vecCenter.x;
        const x87d dy = (x87d)sphere.m_vecCenter.y - (x87d)m_Sphere.m_vecCenter.y;
        const x87d dz = (x87d)sphere.m_vecCenter.z - (x87d)m_Sphere.m_vecCenter.z;
        const x87d r  = (x87d)m_Sphere.m_fRadius + (x87d)sphere.m_fRadius;
        if (!(r * r > dz * dz + dy * dy + dx * dx)) {
            return false;
        }
    }

    // Start with the plane that rejected the sphere last time (no early out - tries all 4 planes, but starts at the last rejecting one)
    auto planeIdx = sphere.m_nNumPlanesPassed;
    for (auto i = 0; i < 4; i++, planeIdx++) {
        const auto& plane = m_Planes[planeIdx & 3];
        const x87d dist = (x87d)plane.normal.y * (x87d)sphere.m_vecCenter.y
                          + (x87d)plane.normal.x * (x87d)sphere.m_vecCenter.x
                          + (x87d)plane.normal.z * (x87d)sphere.m_vecCenter.z
                          - (x87d)plane.distance;
        if (dist > (double)sphere.m_fRadius) {
            sphere.m_nNumPlanesPassed = planeIdx & 3;
            return false;
        }
    }
    return true;
}

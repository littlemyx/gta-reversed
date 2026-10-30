#include "StdInc.h"

#include "CollisionData.h"
#include "ColHelpers.h"

void CCollisionData::InjectHooks() {
    RH_ScopedClass(CCollisionData);
    RH_ScopedCategory("Collision");

    RH_ScopedInstall(RemoveCollisionVolumes, 0x40F070);
    RH_ScopedInstall(RemoveTrianglePlanes, 0x40F6A0);
    RH_ScopedInstall(Copy, 0x40F120);
    RH_ScopedInstall(GetTrianglePoint, 0x40F5E0);
    RH_ScopedInstall(GetShadTrianglePoint, 0x40F640);
    RH_ScopedInstall(CalculateTrianglePlanes, 0x40F590);
    RH_ScopedInstall(SetLinkPtr, 0x40F6C0);
    RH_ScopedInstall(GetLinkPtr, 0x40F6E0);
}

// 0x40F030
CCollisionData::CCollisionData() {
    bUsesDisks = false;
    bHasFaceGroups = false;
    bHasShadowInfo = false;

    m_nNumSpheres = 0;
    m_nNumBoxes = 0;
    m_nNumTriangles = 0;
    m_nNumLines = 0;

    m_pSpheres = nullptr;
    m_pBoxes = nullptr;
    m_pLines = nullptr;
    m_pVertices = nullptr;
    m_pTriangles = nullptr;
    m_pTrianglePlanes = nullptr;

    m_nNumShadowTriangles = 0;
    m_nNumShadowVertices = 0;
    m_pShadowTriangles = nullptr;
    m_pShadowVertices = nullptr;
    /*
     * todo: initialize bHasFaceGroups, bHasShadow, m_pDisks as NOTSA or something
     * */
}

// 0x40F070
void CCollisionData::RemoveCollisionVolumes() {
    CMemoryMgr::Free(m_pSpheres);
    CMemoryMgr::Free(m_pLines);
    CMemoryMgr::Free(m_pBoxes);
    CMemoryMgr::Free(m_pVertices);
    CMemoryMgr::Free(m_pTriangles);
    CMemoryMgr::Free(m_pShadowTriangles);
    CMemoryMgr::Free(m_pShadowVertices);

    CCollision::RemoveTrianglePlanes(this);

    m_nNumSpheres = 0;
    m_nNumLines = 0;
    m_nNumBoxes = 0;
    m_nNumTriangles = 0;
    m_pSpheres = nullptr;
    m_pLines = nullptr;
    m_pBoxes = nullptr;
    m_pVertices = nullptr;
    m_pTriangles = nullptr;
    m_pShadowTriangles = nullptr;
    m_pShadowVertices = nullptr;
}

// 0x40F120
void CCollisionData::Copy(const CCollisionData& src) {
    assert(!bHasFaceGroups); // Avoid possible random bugs - See header for more info.

    // ----- SPHERES -----
    if (m_nNumSpheres != src.m_nNumSpheres || !src.m_nNumSpheres) {
        m_nNumSpheres = src.m_nNumSpheres;
        CMemoryMgr::Free(m_pSpheres);
        m_pSpheres = nullptr;

        if (src.m_nNumSpheres)
            m_pSpheres = static_cast<CColSphere*>(CMemoryMgr::Malloc(m_nNumSpheres * sizeof(CColSphere)));
    }
    for (auto i = 0; i < m_nNumSpheres; ++i) {
        m_pSpheres[i] = src.m_pSpheres[i];
    }

    // ----- LINES / DISKS -----
    if (m_nNumLines != src.m_nNumLines || !src.m_nNumLines || bUsesDisks != src.bUsesDisks) {
        bUsesDisks = src.bUsesDisks;
        m_nNumLines = src.m_nNumLines;
        CMemoryMgr::Free(m_pLines);
        m_pLines = nullptr;

        if (src.m_nNumLines) {
            if (src.bUsesDisks)
                m_pDisks = static_cast<CColDisk*>(CMemoryMgr::Malloc(m_nNumLines * sizeof(CColDisk)));
            else
                m_pLines = static_cast<CColLine*>(CMemoryMgr::Malloc(m_nNumLines * sizeof(CColLine)));
        }
    }
    for (auto i = 0; i < m_nNumLines; ++i) {
        if (bUsesDisks)
            m_pDisks[i] = src.m_pDisks[i];
        else
            m_pLines[i] = src.m_pLines[i];
    }

    // The exe copies the flag bits 0 (bUsesDisks) and 2 (bHasShadowInfo) unconditionally here (0x40F297..0x40F2B1), between the lines and the boxes
    bUsesDisks     = src.bUsesDisks;
    bHasShadowInfo = src.bHasShadowInfo;

    // ----- BOXES -----
    if (m_nNumBoxes != src.m_nNumBoxes || !src.m_nNumBoxes) {
        m_nNumBoxes = src.m_nNumBoxes;
        CMemoryMgr::Free(m_pBoxes);
        m_pBoxes = nullptr;

        if (src.m_nNumBoxes) {
            m_pBoxes = static_cast<CColBox*>(CMemoryMgr::Malloc(m_nNumBoxes * sizeof(CColBox)));
        }
    }
    for (auto i = 0; i < m_nNumBoxes; ++i) {
        m_pBoxes[i] = src.m_pBoxes[i];
    }

    // ----- TRIANGLES & VERTICES -----
    // exe 0x40F32C..0x40F442: the vertex array is ALWAYS re-allocated + copied when the source has triangles (also when the triangle count is
    // equal; the port kept the old vertices then), sized by the highest vertex index + 1 (an all-zero index list still gets 1 vertex).
    if (!src.m_nNumTriangles) {
        m_nNumTriangles = 0;
        CMemoryMgr::Free(m_pTriangles);
        m_pTriangles = nullptr;
        CMemoryMgr::Free(m_pVertices);
        m_pVertices = nullptr;
    } else {
        uint32 highest = 0;
        for (auto i = 0; i < (int16)src.m_nNumTriangles; ++i) {
            highest = std::max<uint32>(highest, src.m_pTriangles[i].vA);
            highest = std::max<uint32>(highest, src.m_pTriangles[i].vB);
            highest = std::max<uint32>(highest, src.m_pTriangles[i].vC);
        }
        const auto numVerts = (uint16)(highest + 1);
        CMemoryMgr::Free(m_pVertices);
        m_pVertices = nullptr;
        if (numVerts) {
            m_pVertices = static_cast<decltype(m_pVertices)>(CMemoryMgr::Malloc(numVerts * sizeof(*m_pVertices)));
            for (auto i = 0; i < numVerts; ++i) {
                m_pVertices[i] = src.m_pVertices[i];
            }
        }
        if (m_nNumTriangles != src.m_nNumTriangles) {
            m_nNumTriangles = src.m_nNumTriangles;
            CMemoryMgr::Free(m_pTriangles);
            m_pTriangles = static_cast<CColTriangle*>(CMemoryMgr::Malloc(m_nNumTriangles * sizeof(CColTriangle)));
        }
        for (auto i = 0; i < m_nNumTriangles; ++i) {
            m_pTriangles[i] = src.m_pTriangles[i];
        }
    }

    // ----- SHADOW TRIANGLES & SHADOW VERTICES -----
    // exe 0x40F444..0x40F575: only when the source has shadow triangles AND bHasShadowInfo; otherwise everything shadow-related is wiped (counts and pointers).
    if (src.m_nNumShadowTriangles && src.bHasShadowInfo) {
        uint32 highest = 0;
        for (uint32 i = 0; i < src.m_nNumShadowTriangles; ++i) {
            highest = std::max<uint32>(highest, src.m_pShadowTriangles[i].vA);
            highest = std::max<uint32>(highest, src.m_pShadowTriangles[i].vB);
            highest = std::max<uint32>(highest, src.m_pShadowTriangles[i].vC);
        }
        const auto numVerts = (uint16)(highest + 1);
        CMemoryMgr::Free(m_pShadowVertices);
        m_pShadowVertices = nullptr;
        if (numVerts) {
            m_pShadowVertices = static_cast<decltype(m_pShadowVertices)>(CMemoryMgr::Malloc(numVerts * sizeof(*m_pShadowVertices)));
            for (auto i = 0; i < numVerts; ++i) {
                m_pShadowVertices[i] = src.m_pShadowVertices[i];
            }
        }
        if (m_nNumShadowTriangles != src.m_nNumShadowTriangles) {
            m_nNumShadowTriangles = src.m_nNumShadowTriangles;
            CMemoryMgr::Free(m_pShadowTriangles);
            m_pShadowTriangles = static_cast<CColTriangle*>(CMemoryMgr::Malloc(m_nNumShadowTriangles * sizeof(CColTriangle)));
        }
        for (uint32 i = 0; i < m_nNumShadowTriangles; ++i) {
            m_pShadowTriangles[i] = src.m_pShadowTriangles[i];
        }
        m_nNumShadowVertices = src.m_nNumShadowVertices;
    } else {
        m_nNumShadowTriangles = 0;
        m_nNumShadowVertices = 0;
        CMemoryMgr::Free(m_pShadowTriangles);
        m_pShadowTriangles = nullptr;
        CMemoryMgr::Free(m_pShadowVertices);
        m_pShadowVertices = nullptr;
    }
}

// Memory layout: | CColTrianglePlane[] | (4 Byte aligned)CLink<CCollisionData*>* |
// 0x40F590
void CCollisionData::CalculateTrianglePlanes() {
    m_pTrianglePlanes = static_cast<CColTrianglePlane*>(CMemoryMgr::Malloc((m_nNumTriangles + 1) * sizeof(CColTrianglePlane)));
    for (auto i = 0; i < m_nNumTriangles; ++i) {
        m_pTrianglePlanes[i].Set(m_pVertices, m_pTriangles[i]);
    }
}

// 0x40F6A0
void CCollisionData::RemoveTrianglePlanes() {
    CMemoryMgr::Free(m_pTrianglePlanes);
    m_pTrianglePlanes = nullptr;
}

// 0x40F5E0
void CCollisionData::GetTrianglePoint(CVector& outVec, int32 vertId) {
    outVec = m_pVertices[vertId];
}

// 0x40F640
void CCollisionData::GetShadTrianglePoint(CVector& outVec, int32 vertId) {
    outVec = m_pShadowVertices[vertId];
}

// 0x40F6C0
void CCollisionData::SetLinkPtr(CLink<CCollisionData*>* link) {
    // Original calculation method:
    // const auto dwLinkAddress = (reinterpret_cast<uint32>(&m_pTrianglePlanes[m_nNumTriangles]) + 3) & 0xFFFFFFFC; // 4 bytes aligned address

    auto* linkPtr = static_cast<void*>(&m_pTrianglePlanes[m_nNumTriangles]);
    auto space = sizeof(CColTrianglePlane);
    auto* alignedAddress = std::align(4, sizeof(CLink<CCollisionData*>*), linkPtr, space); // 4 bytes aligned address
    *static_cast<CLink<CCollisionData*>**>(alignedAddress) = link;
}

// 0x40F6E0
CLink<CCollisionData*>* CCollisionData::GetLinkPtr() {
    // Original calculation method:
    // const auto dwLinkAddress = (reinterpret_cast<uint32>(&m_pTrianglePlanes[m_nNumTriangles]) + 3) & 0xFFFFFFFC; // 4 bytes aligned address

    auto* linkPtr = static_cast<void*>(&m_pTrianglePlanes[m_nNumTriangles]);
    auto space = sizeof(CColTrianglePlane);
    auto* alignedAddress = std::align(4, sizeof(CLink<CCollisionData*>*), linkPtr, space); // 4 bytes aligned address
    const auto l = *static_cast<CLink<CCollisionData*>**>(alignedAddress);
    assert(l->data == this); // Sanity check
    return l;
}

auto CCollisionData::GetNumFaceGroups() const -> uint32 {
    if (bHasFaceGroups) {
        assert(m_pTriangles);
        return *reinterpret_cast<uint32*>(reinterpret_cast<byte*>(m_pTriangles) - sizeof(uint32)); // See `CCollisionData` header for explanation :)
    }
    return 0;
}

auto CCollisionData::GetFaceGroups() const -> std::span<ColHelpers::TFaceGroup> {
    using namespace ColHelpers;
    if (const auto numfg = GetNumFaceGroups()) {
        assert(numfg);
        return std::span{ // See `CCollisionData` header for explanation
            reinterpret_cast<TFaceGroup*>(reinterpret_cast<uint8*>(m_pTriangles) - sizeof(uint32) - sizeof(TFaceGroup) * numfg),
            numfg
        };
    }
    return {};
}

auto CCollisionData::GetTriVertices(const CColTriangle& tri) const->std::array<CVector, 3> {
    std::array<CVector, 3> verts;
    for (auto&& [i, j] : rngv::enumerate(tri.m_vertIndices)) {
        verts[i] = m_pVertices[j];
    }
    return verts;
}

// NOTSA
void CCollisionData::AllocateLines(uint32 num) {
    m_nNumLines = num;
    m_pLines = static_cast<CColLine*>(CMemoryMgr::Malloc(sizeof(CColLine) * num));
}

// NOTSA
void CCollisionData::SetSpheres(const CColSphere* spheres) {
    for (auto i = 0; i < m_nNumSpheres; ++i) {
        m_pSpheres[i] = spheres[i];
    }
}

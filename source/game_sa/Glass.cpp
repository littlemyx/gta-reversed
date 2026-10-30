#include "StdInc.h"

#include "Glass.h"
#include "Fx/FxFtol.h"
#include "FallingGlassPane.h"
#include "Shadows.h"

void CGlass::InjectHooks() {
    RH_ScopedClass(CGlass);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(Init, 0x71A8D0);
    RH_ScopedInstall(HasGlassBeenShatteredAtCoors, 0x71CB70);
    RH_ScopedInstall(CarWindscreenShatters, 0x71C2B0);
    RH_ScopedInstall(WasGlassHitByBullet, 0x71C0D0);
    RH_ScopedInstall(WindowRespondsToCollision, 0x71BC40);
    RH_ScopedInstall(GeneratePanesForWindow, 0x71B620);
    RH_ScopedInstall(Update, 0x71B0D0);
    RH_ScopedInstall(Render, 0x71CE20);
    RH_ScopedInstall(FindWindowSectorList<CPtrListSingleLink<CPhysical*>>, 0x71AFC0);
    RH_ScopedInstall(RenderReflectionPolys, 0x71AED0);
    RH_ScopedInstall(RenderShatteredPolys, 0x71AE30);
    RH_ScopedInstall(RenderHiLightPolys, 0x71ADA0);
    RH_ScopedInstall(CalcAlphaWithNormal, 0x71ACF0);
    RH_ScopedInstall(AskForObjectToBeRenderedInGlass, 0x71ACD0);
    RH_ScopedInstall(FindFreePane, 0x71ACA0);
    RH_ScopedInstall(WindowRespondsToSoftCollision, 0x71AF70);
    RH_ScopedInstall(BreakGlassPhysically, 0x71CF50);
    RH_ScopedInstall(WindowRespondsToExplosion, 0x71C1A0);
    RH_ScopedInstall(IsObjectGlass, 0x46A760);
}

// 0x71A8D0
void CGlass::Init() {
    for (auto& pane : aGlassPanes) {
        pane.m_bExist = false;
    }

    for (auto i = 0u; i < std::size(PanePolyPositions); i++) {
        const auto& poly = PanePolyPositions[i];

        // 0x71A8D0: centre of each pane polygon = (v0 + v1 + v2) * 0.33333334f (0x859040) per component
        PanePolyCenterPositions[i] = CVector2D{ (poly[0].x + poly[1].x + poly[2].x) * 0.33333334f, (poly[0].y + poly[1].y + poly[2].y) * 0.33333334f };
    }
}

// (float, float, float ) & unused
// 0x71CB70
bool CGlass::HasGlassBeenShatteredAtCoors(CVector point) {
    const int32 startSectorX = std::max(CWorld::GetSectorX(point.x - 30.f), 0);
    const int32 startSectorY = std::max(CWorld::GetSectorY(point.y - 30.f), 0);
    const int32 endSectorX   = std::min(CWorld::GetSectorX(point.x + 30.f), MAX_SECTORS_X - 1);
    const int32 endSectorY   = std::min(CWorld::GetSectorY(point.y + 30.f), MAX_SECTORS_Y - 1);

    CWorld::AdvanceCurrentScanCode();

    float maxDist = 20.f;
    CEntity* entity{};
    for (int32 sectorY = startSectorY; sectorY <= endSectorY; ++sectorY) {
        for (int32 sectorX = startSectorX; sectorX <= endSectorX; ++sectorX) {
            FindWindowSectorList(CWorld::GetRepeatSector(sectorX, sectorY).Objects, maxDist, entity, point);
            FindWindowSectorList(CWorld::GetSector(sectorX, sectorY).Dummies, maxDist, entity, point);
        }
    }
    // exe 0x71CD8A-0x71CDF0: `(m_nObjectFlags >> 5) & 1` = bGlassBrokenAltogether (the port tested bHasBrokenGlass, bit 4)
    return entity && !entity->GetIsTypeDummy() && entity->AsObject()->objectFlags.bGlassBrokenAltogether;
}

// 0x71C2B0
void CGlass::CarWindscreenShatters(CVehicle* vehicle) {
    const auto colModel = vehicle->GetColModel();
    if (!colModel || colModel->GetTriCount() < 2) {
        return;
    }

    const auto& triangles = colModel->m_pColData->m_pTriangles;

    // Find the triangle of the glass rectangle
    // They're after each other, so we just have to find the first one here
    uint32 glassTriIdx{};
    for (; glassTriIdx < colModel->GetTriCount(); glassTriIdx++) {
        if (g_surfaceInfos.IsGlass(triangles[glassTriIdx].m_nMaterial)) {
            break;
        }
    }

    // `glassTriIdx` is the first triangle of the glass rectangle, the second one is the one after it.
    // So, logically, if `glassTriIdx + 1` is valid, `glassTriIdx` is going to be valid as well.
    if (glassTriIdx + 1 >= colModel->GetTriCount())
        return;

    CCollision::CalculateTrianglePlanes(colModel);

    auto& vehMat = (CMatrix&)vehicle->GetMatrix();

    // Grab normal and transform it to world space
    const auto normal = vehMat.TransformVector(colModel->m_pColData->m_pTrianglePlanes[glassTriIdx].GetNormal());

    // Calculate direction vectors
    const auto fwd   = Normalized(CrossProduct(vehMat.GetRight(), normal));
    const auto right = Normalized(CrossProduct(vehMat.GetUp(), normal));

    // Store world space vertex positions of both triangles
    // Should be more like [2][3] but a multdim array would've been way harder to deal with
    // so we're going to use a flattened [6] array
    CVector triVertices[6]{};
    for (auto t = 0; t < 2; t++) { // t - triangle idx
        const auto& tri = triangles[glassTriIdx + t];
        for (auto v = 0; v < 3; v++) { // v - vertex idx of this triangle
            triVertices[t * 3 + v] = vehMat.TransformPoint(colModel->m_pColData->m_pVertices[tri.m_vertIndices[v]]);
        }
    }

    // Calculate dot products for both directions
    // The dot product of the position of a vertex and `right` will give it's "position" on the `right` axis
    // same goes for `fwd`
    // By this we basically get a 2D position of each vertex on the plane of the glass.

    const auto CalculateDotProducts = [&](CVector direction) {
        std::array<float, 6> out{};
        rng::transform(triVertices, out.begin(), [&](auto&& v) { return DotProduct(v, direction); });
        return out;
    };

    // Calculate dot products for all vertices on both direction vectors
    // In 2D terms right would be `x` and `fwd` would be `y`
    const auto rightDots = CalculateDotProducts(right);
    const auto fwdDots   = CalculateDotProducts(fwd);

    // Find max values in each direction
    // This will give us the top right point basically
    const float maxDotFwd   = *rng::max_element(fwdDots);
    const float maxDotRight = *rng::max_element(rightDots);

    // Calculate bottom left corner
    float minRightFwdDotSum = FLT_MAX; // todo: FLT_MAX is not NOTSA ?
    uint32 minRightFwdDotSumIdx{};
    for (auto i = 0u; i < 6u; i++) { // todo: should be start from 1 ?
        const auto rightDot = rightDots[i], fwdDot = fwdDots[i];
        if (rightDot + fwdDot < minRightFwdDotSum) {
            minRightFwdDotSum = rightDot + fwdDot;
            minRightFwdDotSumIdx = i;
        }
    }

    assert(0 <= minRightFwdDotSumIdx && minRightFwdDotSumIdx < std::size(rightDots));
    // Size of pane in directions
    auto extentRight = maxDotRight - rightDots[minRightFwdDotSumIdx];
    auto extentFwd = maxDotFwd - fwdDots[minRightFwdDotSumIdx];

    const auto blPos = triVertices[minRightFwdDotSumIdx];
    GeneratePanesForWindow(
        ePaneType::CAR,
        blPos,
        fwd * extentFwd,
        right * extentRight, vehicle->m_vecMoveSpeed,
        blPos + fwd * extentFwd / 2.f + right * extentRight / 2.f,
        0.1f,
        false,
        false,
        2,
        true
    );
}

bool IsGlassObjectWithCol(CEntity* entity) {
    if (entity->GetIsTypeObject() && entity->GetUsesCollision()) {
        if (const auto ami = entity->GetModelInfo()->AsAtomicModelInfoPtr()) {
            return ami->IsGlass();
        }
    }
    return false;
}

// 0x71C0D0
void CGlass::WasGlassHitByBullet(CEntity* entity, CVector hitPos) {
    if (!IsGlassObjectWithCol(entity))
        return;

    const auto object = entity->AsObject();
    if (object->objectFlags.bHasBrokenGlass) {
        if (CGeneral::GetRandomNumber() % 4 == 2) {
            WindowRespondsToCollision(entity, 0.0f, {}, hitPos, false);
        }
    } else {
        object->objectFlags.bHasBrokenGlass = true; // Just mark it as broken
    }
}

template<size_t N>
std::pair<float, float> FindMinMaxZOfVertices(CVector (&vertices)[N]) {
    const auto [min, max] = rng::minmax(vertices | rng::views::transform([](auto&& v) { return v.z; }));
    return { min, max };
}

// Index of the vertex furthest (2D) from vertex 0, exe 0x71BD69-0x71BDEC (and the copy at 0x71D23D-0x71D2B0 in BreakGlassPhysically): the search starts at vertex 1
// (vertex 0 is never a candidate), the distance is sqrt(dy^2 + dx^2) (unrounded), the best one is kept as a float, and only a strictly greater distance replaces it.
static size_t FindFurthestVertexIdx2D(const CVector (&verts)[4]) {
    const auto Dist2D = [&](size_t i) {
        const x87d dx = (x87d)verts[i].x - verts[0].x, dy = (x87d)verts[i].y - verts[0].y;
        return (x87d)std::sqrt(dy * dy + dx * dx);
    };
    size_t idx  = 1;
    float  best = (float)Dist2D(1);
    for (size_t i = 2; i < 4; i++) {
        const x87d d = Dist2D(i);
        if (d > best) {
            best = (float)d;
            idx  = i;
        }
    }
    return idx;
}

// 0x71BC40
void CGlass::WindowRespondsToCollision(CEntity* entity, float fDamageIntensity, CVector vecMoveSpeed, CVector vecPoint, bool max1PaneSection) {
    auto object = entity->AsObject();
    if (object->objectFlags.bGlassBrokenAltogether)
        return;

    object->objectFlags.bHasBrokenGlass = true;

    if (const auto cd = object->GetColModel()->m_pColData; cd && cd->m_nNumTriangles == 2) {
        // Object space vertices
        CVector verticesOS[4];
        rng::copy(std::span{ cd->m_pVertices, 4 }, verticesOS);

        const auto [minZ, maxZ] = FindMinMaxZOfVertices(verticesOS);

        const auto vertFurthestFromV0 = &verticesOS[FindFurthestVertexIdx2D(verticesOS)];

        // Transform vertices to world space
        const auto vert0Pos          = object->GetMatrix().TransformPoint({ verticesOS[0].x, verticesOS[0].y, minZ });
        const auto furthestFromV0Pos = object->GetMatrix().TransformPoint({ vertFurthestFromV0->x, vertFurthestFromV0->y, minZ });

        AudioEngine.ReportGlassCollisionEvent(AE_GLASS_BREAK_FAST, object->GetPosition());
        GeneratePanesForWindow(
            !(fDamageIntensity > 300.f) ? ePaneType::DELAYED : ePaneType::DEFAULT, // exe 0x71BE79: `jne` on (C0|C3) -> NaN takes the DELAYED arm
            vert0Pos,
            { 0.f, 0.f, maxZ - minZ },
            furthestFromV0Pos - vert0Pos,
            vecMoveSpeed,
            vecPoint,
            0.1f,
            //object->objectFlags.bGlassBroken,
            (bool)((object->m_nObjectFlags >> 4) & 0xFFFFFF01),
            max1PaneSection,
            1,
            false
        );
    }

    object->SetUsesCollision(false);
    object->SetIsVisible(false);
    object->objectFlags.bGlassBrokenAltogether = true;
}

/*
 * 0x71B620
 *
 * Corners:
 * TL ----- TR
 * |  0 | 1 |
 * | ------ | fwd
 * |  2 | 3 |
 * BL ----- BR
 *  right
 *
 * - Neither 'size' vectors are normalized!
 *
 * type                    - @ePaneType
 * point                   - BL
 * fwd, right              - As illustrated above
 * center                  - The centre of the above rectangle (each pane is a piece of it)
 * velocity                - How fast the panes fly
 * velocityCenterDragCoeff - Modify the velocity's direction to be more towards the center point
 * bShatter                - Render shatter polys
 * numSectionsMax1         - Limit no. of sections to 1 - Unsure what's it use-case as setting `numSections` to 1 achieves the same
 * numSections             - No. of sections of each axis, the total number of sections will be `numSections ^ 2` (squared)
**/
void CGlass::GeneratePanesForWindow(ePaneType type, CVector point, CVector fwd, CVector right, CVector velocity, CVector center, float velocityCenterDragCoeff,
                                    bool bShatter, bool numSectionsMax1, int32 numSections, bool unk) {
    // exe 0x71B620-0x71BC3C. The lengths are sqrt((z^2 + y^2) + x^2) (inlined, NOT CVector::Magnitude's x,y,z order)
    const auto LengthZYX = [](const CVector& v) { return (float)std::sqrt(((x87d)v.z * v.z + (x87d)v.y * v.y) + (x87d)v.x * v.x); };
    const float totalSizeY = LengthZYX(fwd), totalSizeX = LengthZYX(right);

    // Calculate no. of sections: _ftol2(max(1.0, size + 0.75)) * numSections (a window thinner than 0.25 still has one section), at most 3, at most 1 for `numSectionsMax1`
    const auto CalculateCountOfSections = [&](float axisSize) {
        const x87d size = (x87d)axisSize + 0.75f;
        uint32 count = NOTSA_FTOL(uint32, ((x87d)1.0 > size) ? (x87d)1.0 : size) * (uint32)numSections;
        count = std::min(count, 3u);
        if (numSectionsMax1) {
            count = std::min(count, 1u);
        }
        return count;
    };
    const uint32 countY = CalculateCountOfSections(totalSizeY); // fwd is evaluated first in the exe, no side effects
    const uint32 countX = CalculateCountOfSections(totalSizeX);

    const float sizeY = totalSizeY / (float)countY;
    const float sizeX = totalSizeX / (float)countX;

    bool hitGround{};
    float groundZ = CWorld::FindGroundZFor3DCoord(point, &hitGround, nullptr);
    if (!hitGround)
        groundZ = point.z - 2.f;

    for (auto posY = 0u; posY < countY; posY++) {
        // exe 0x71B7D7: posY * totalSizeY / count, stored as a float
        const float offsetY = (float)((x87d)posY * totalSizeY / (x87d)(float)countY);

        for (auto posX = 0u; posX < countX; posX++) {
            const float offsetX = (float)((x87d)posX * totalSizeX / (x87d)(float)countX);

            for (auto piece = 0u; piece < std::size(PanePolyPositions); piece++) {
                CFallingGlassPane* pane = FindFreePane();
                if (!pane)
                    continue;

                pane->m_nPieceIndex = piece;

                // Calculate matrix: `right` = (1 / |right|) * right * sizeX, `up` = (1 / |fwd|) * fwd * sizeY. The reciprocal of the right axis is kept unrounded for
                // x and re-read as a float for y/z (`fst` + `fld`), the one of the up axis is a float (`fstp` + `fld`) for all three components.
                auto& mat = pane->m_Matrix;
                const x87d invRightE = (x87d)1.0 / totalSizeX;
                const float invRight = (float)invRightE;
                const float invFwd   = (float)((x87d)1.0 / totalSizeY);
                mat.GetRight() = CVector{
                    (float)((invRightE * right.x) * sizeX),
                    (float)(((x87d)invRight * right.y) * sizeX),
                    (float)(((x87d)invRight * right.z) * sizeX)
                };
                mat.GetUp() = CVector{
                    (float)(((x87d)invFwd * fwd.x) * sizeY),
                    (float)(((x87d)invFwd * fwd.y) * sizeY),
                    (float)(((x87d)invFwd * fwd.z) * sizeY)
                };
                mat.GetForward() = Normalized(CrossProduct(mat.GetRight(), mat.GetUp()));

                // Matrix's position is the center of the glass pane: exe 0x71B8A5-0x71B9E0 (sums unrounded, one float rounding per component)
                const x87d paneX = (x87d)sizeX * PanePolyCenterPositions[piece].x + offsetX;
                const x87d paneY = (x87d)sizeY * PanePolyCenterPositions[piece].y + offsetY;
                mat.GetPosition() = CVector{
                    (float)((((x87d)invFwd * fwd.x) * paneY + ((x87d)invRight * right.x) * paneX) + point.x),
                    (float)((((x87d)invFwd * fwd.y) * paneY + ((x87d)invRight * right.y) * paneX) + point.y),
                    (float)((((x87d)invFwd * fwd.z) * paneY + ((x87d)invRight * right.z) * paneX) + point.z)
                };

                {
                    // exe 0x71B9E0-0x71BA6C: velocity.x + r1 * 0.0015f (r1 stored as a float), velocity.y + r2 * 0.0015f (the product stays unrounded)
                    const float r1 = (float)((CGeneral::GetRandomNumber() % 128) - 64) * 0.0015f;
                    const int32 r2 = (CGeneral::GetRandomNumber() % 128) - 64;
                    pane->m_Velocity = CVector{ velocity.x + r1, (float)((x87d)velocity.y + (x87d)r2 * 0.0015f), velocity.z };
                }

                if (velocityCenterDragCoeff != 0.0f) {
                    // exe 0x71BAB0-0x71BB10: x/y use the unrounded product, z the float-rounded one
                    const auto dir = Normalized(mat.GetPosition() - center);
                    pane->m_Velocity.x = (float)((x87d)dir.x * velocityCenterDragCoeff + pane->m_Velocity.x);
                    pane->m_Velocity.y = (float)((x87d)dir.y * velocityCenterDragCoeff + pane->m_Velocity.y);
                    pane->m_Velocity.z = (float)(dir.z * velocityCenterDragCoeff) + pane->m_Velocity.z;
                }

                {
                    // exe 0x71BB10-0x71BB6A: the FIRST rand() goes to y, the second to x, the third to z ((rand % 128 - 64) * 0.002f)
                    const float ry = (float)((CGeneral::GetRandomNumber() % 128) - 64) * 0.002f;
                    const float rx = (float)((CGeneral::GetRandomNumber() % 128) - 64) * 0.002f;
                    const float rz = (float)((CGeneral::GetRandomNumber() % 128) - 64) * 0.002f;
                    pane->m_RandomNumbers = CVector{ rx, ry, rz };
                }

                switch (type) {
                case ePaneType::DELAYED: {
                    // exe 0x71BBDB: _ftol2(sqrt((dz^2 + dy^2) + dx^2) * 100.0 + time as x87 integer), the length unrounded
                    const auto d = mat.GetPosition() - center;
                    const x87d len = std::sqrt(((x87d)d.z * d.z + (x87d)d.y * d.y) + (x87d)d.x * d.x);
                    pane->m_nCreatedTime = NOTSA_FTOL(uint32, len * (x87d)100.0f + (x87d)CTimer::GetTimeInMS());
                    break;
                }
                case ePaneType::CAR:
                case ePaneType::DEFAULT:{
                    pane->m_nCreatedTime = CTimer::GetTimeInMS();
                    break;
                }
                }

                pane->m_fGroundZ = groundZ;
                pane->m_bRenderShatter = bShatter;
                pane->m_fSize = sizeY;
                pane->m_f6F = unk;
                pane->m_bExist = true;
            }
        }
    }
}

// 0x71B0D0
void CGlass::Update() {
    ZoneScoped;

    for (auto& pane : aGlassPanes) {
        if (pane.m_bExist) {
            pane.Update();
        }
    }
}

// 0x71CE20
void CGlass::Render() {
    ZoneScoped;

    H1iLightPolyVerticesIdx = 0;
    HiLightPolyIndicesIdx = 0;

    ShatteredVerticesBaseIdx = 1024;
    ShatteredIndicesBaseIdx = 2048;
    ReflectionPolyVertexBaseIdx = 1536;
    ReflectionPolyIndexBaseIdx = 3072;

    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,      RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATETEXTUREFILTER,     RWRSTATE(rwFILTERLINEAR));
    RwRenderStateSet(rwRENDERSTATEFOGENABLE,         RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATEFOGCOLOR,          RWRSTATE(CTimeCycle::m_CurrentColours.GetSkyBottom(0).ToIntARGB()));
    RwRenderStateSet(rwRENDERSTATESRCBLEND,          RWRSTATE(rwBLENDONE));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND,         RWRSTATE(rwBLENDONE));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(TRUE));

    for (auto& pane : aGlassPanes) {
        if (pane.m_bExist) {
            pane.Render();
        }
    }

    RenderHiLightPolys();
    RenderShatteredPolys();
    RenderReflectionPolys();

    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,  RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATESRCBLEND,     RWRSTATE(rwBLENDSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND,    RWRSTATE(rwBLENDINVSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEFOGENABLE,    RWRSTATE(FALSE));
}

// (..., CEntity**, float, float, float)
// 0x71AFC0
template<typename PtrListType>
void CGlass::FindWindowSectorList(PtrListType& objList, float& outDist, CEntity*& outEntity, CVector point) {
    if (!objList.GetNode())
        return;

    for (auto* const entity : objList) {
        if (entity->IsScanCodeCurrent())
            continue;

        if (!entity->GetIsTypeObject())
            continue;

        const auto object = entity->AsObject();
        if (const auto ami = CModelInfo::GetModelInfo(entity->m_nModelIndex)->AsAtomicModelInfoPtr()) {
            switch (ami->nSpecialType) {
            case eModelInfoSpecialType::GLASS_TYPE_1:
            case eModelInfoSpecialType::GLASS_TYPE_2: {
                object->SetScanCode(CWorld::ms_nCurrentScanCode);
                // exe 0x71B054-0x71B08E: sqrt((dz^2 + dy^2) + dx^2), the differences, squares and sums unrounded
                const auto& objPos = object->GetPosition();
                const x87d dx = (x87d)point.x - objPos.x, dy = (x87d)point.y - objPos.y, dz = (x87d)point.z - objPos.z;
                const x87d dist = std::sqrt((dz * dz + dy * dy) + dx * dx);
                if (dist < outDist) {
                    outEntity = entity;
                    outDist = (float)dist;
                }
                break;
            }
            }
        }
    }
}

// 0x71AED0
void CGlass::RenderReflectionPolys() {
    if (ReflectionPolyVertexBaseIdx != 1536) {

        RwRenderStateSet(rwRENDERSTATETEXTURERASTER, RWRSTATE(RwTextureGetRaster(gpShadowHeadLightsTex)));
        RwRenderStateSet(rwRENDERSTATESRCBLEND,      RWRSTATE(rwBLENDSRCALPHA));
        RwRenderStateSet(rwRENDERSTATEDESTBLEND,     RWRSTATE(rwBLENDINVSRCALPHA));

        if (RwIm3DTransform(ReflectionPolyVertexBuffer.data(), ReflectionPolyVertexBaseIdx - 1536, nullptr, rwIM3D_VERTEXUV))
        {
            RwIm3DRenderIndexedPrimitive(rwPRIMTYPETRILIST, &aTempBufferIndices[3072], ReflectionPolyIndexBaseIdx - 3072);
            RwIm3DEnd();
        }

        ReflectionPolyVertexBaseIdx = 1536;
        ReflectionPolyIndexBaseIdx = 3072;
    }
}

// 0x71AE30
void CGlass::RenderShatteredPolys() {
    if (ShatteredVerticesBaseIdx != 1024)
    {
        RwRenderStateSet(rwRENDERSTATETEXTURERASTER, RWRSTATE(RwTextureGetRaster(gpCrackedGlassTex)));
        RwRenderStateSet(rwRENDERSTATESRCBLEND,      RWRSTATE(rwBLENDSRCALPHA));
        RwRenderStateSet(rwRENDERSTATEDESTBLEND,     RWRSTATE(rwBLENDINVSRCALPHA));

        if (RwIm3DTransform(ShatteredPolyVertexBuffer.data(), ShatteredVerticesBaseIdx - 1024, nullptr, rwIM3D_VERTEXUV))
        {
            RwIm3DRenderIndexedPrimitive(rwPRIMTYPETRILIST, &aTempBufferIndices[2048], ShatteredIndicesBaseIdx - 2048);
            RwIm3DEnd();
        }

        ShatteredVerticesBaseIdx = 1024;
        ShatteredIndicesBaseIdx = 2048;
    }
}

// 0x71ADA0
void CGlass::RenderHiLightPolys() {
    if (H1iLightPolyVerticesIdx != 0)
    {
        RwRenderStateSet(rwRENDERSTATESRCBLEND,      RWRSTATE(rwBLENDONE));
        RwRenderStateSet(rwRENDERSTATEDESTBLEND,     RWRSTATE(rwBLENDONE));
        RwRenderStateSet(rwRENDERSTATETEXTURERASTER, RWRSTATE(RwTextureGetRaster(gpShadowExplosionTex)));

        if (RwIm3DTransform(TempBufferVertices.m_3d, H1iLightPolyVerticesIdx, nullptr, rwIM3D_VERTEXUV)) {
            RwIm3DRenderIndexedPrimitive(rwPRIMTYPETRILIST, aTempBufferIndices, HiLightPolyIndicesIdx);
            RwIm3DEnd();
        }

        HiLightPolyIndicesIdx = 0;
        H1iLightPolyVerticesIdx = 0;
    }
}

// 0x71ACF0
uint8 CGlass::CalcAlphaWithNormal(const CVector& normal) {
    // 0x71ACF0, step by step in the exe's order (game mode: every x87 operation rounds to float): reflect = fwd - 2 * dot(fwd, N) * N, then
    // specular = (reflect.x + reflect.y) * 0.57 - reflect.z * 0.57 (the light direction (0.57, 0.57, -0.57) is not normalised in the exe), alpha = _ftol2(specular^6 * 235 + 20),
    // the sixth power by five multiplications (the old port called pow()).
    const auto& fwd = TheCamera.GetForward();

    const float dot  = (fwd.y * normal.y + fwd.z * normal.z) + fwd.x * normal.x;
    const float dot2 = dot + dot;
    const float px   = dot2 * normal.x;
    const float py   = dot2 * normal.y;
    const float pz   = dot2 * normal.z;
    const float rx   = fwd.x - px;
    const float ry   = fwd.y - py;
    const float rz   = fwd.z - pz;

    // FIX_BUGS: the normalised light direction (1, 1, -1) / sqrt(3) instead of the exe's 0.57
    const float k        = notsa::IsFixBugs() ? 0.57735026f : 0.57f;
    const float specular = (rx + ry) * k - rz * k;

    // negative dot products give positive highlights (even power)
    const float sq    = specular * specular;
    const float cube  = sq * specular;
    const float p4    = cube * specular;
    const float p5    = p4 * specular;
    const float p6    = p5 * specular;
    return (uint8)notsa::detail::Ftol((double)(p6 * 235.0f + 20.0f));
}

// 0x71ACD0
void CGlass::AskForObjectToBeRenderedInGlass(CEntity* entity) {
    if (NumGlassEntities < std::size(apEntitiesToBeRendered) - 1) {
        apEntitiesToBeRendered[NumGlassEntities++] = entity;
    }
}

// 0x71ACA0
CFallingGlassPane* CGlass::FindFreePane() {
    for (auto& pane : aGlassPanes) {
        if (!pane.m_bExist)
            return &pane;
    }
    return nullptr;
}

// 0x71AF70
void CGlass::WindowRespondsToSoftCollision(CEntity* entity, float fDamageIntensity) {
    if (entity->GetUsesCollision() && fDamageIntensity > 50.f && !entity->AsObject()->objectFlags.bHasBrokenGlass) {
        AudioEngine.ReportGlassCollisionEvent(AE_GLASS_HIT, entity->GetPosition());
        entity->AsObject()->objectFlags.bHasBrokenGlass = true;
    }
}

// 0x71CF50
void CGlass::BreakGlassPhysically(CVector point, float radius) {
    if (CTimer::GetTimeInMS() < LastColCheckMS + 1000 && CTimer::GetTimeInMS() >= LastColCheckMS)
        return;

    for (auto& object : NOTSA_EXE_OR(GetObjectPool()->GetAllValidReverse(), GetObjectPool()->GetAllValid())) {
        if (!IsGlassObjectWithCol(&object))
            continue;

        const auto colModel = object.GetColModel();
        if (!colModel)
            continue;

        const auto colData = colModel->m_pColData;
        if (!colData)
            continue;

        if (colModel->GetTriCount() < 2)
            continue;

        const auto& objPos = object.GetPosition();
        // Test if point touches any of the model's triangles
        {
            const CColSphere sphere{
                object.GetMatrix().TransformVector(point - objPos),
                radius
            };
            CCollision::CalculateTrianglePlanes(colModel);

            // TODO / NOTE: Shouldn't the loop stop the first time it hits a triangle? Unsure why they didn't do it like that?
            bool hasHit{};
            for (auto tri = 0u; tri < colModel->GetTriCount(); tri++) {
                if (CCollision::TestSphereTriangle(sphere, colData->m_pVertices, colData->m_pTriangles[tri], colData->m_pTrianglePlanes[tri])) {
                    hasHit = true;
                }
            }
            if (!hasHit)
                continue;
        }

        LastColCheckMS = CTimer::GetTimeInMS();

        if (!object.objectFlags.bHasBrokenGlass) {
            AudioEngine.ReportGlassCollisionEvent(AE_GLASS_HIT, objPos);
            object.objectFlags.bHasBrokenGlass = true;
            return;
        }

        CVector verticesObjSpace[4];
        for (auto i = 0u; i < std::size(verticesObjSpace); ++i) {
            verticesObjSpace[i] = colData->m_pVertices[i];
        }

        const auto [minZ, maxZ] = FindMinMaxZOfVertices(verticesObjSpace);

        // Vertex furthest of vertex 0
        const auto furthestOfV0 = rng::max_element(verticesObjSpace, {},
            [&](const auto& v) { return DistanceBetweenPoints2D(verticesObjSpace[0], v); }
        );

        const auto v0Pos = object.GetMatrix().TransformPoint({ verticesObjSpace[0].x, verticesObjSpace[0].y, minZ });
        const auto furthestOfV0Pos = object.GetMatrix().TransformPoint({ furthestOfV0->x, furthestOfV0->y, minZ });

        AudioEngine.ReportGlassCollisionEvent(AE_GLASS_HIT, objPos);

        GeneratePanesForWindow(
            ePaneType::DELAYED,
            v0Pos,
            { 0.f, 0.f, maxZ - minZ },
            furthestOfV0Pos - v0Pos,
            {},
            point,
            0.1f,
            object.objectFlags.bHasBrokenGlass,
            false,
            1,
            false
        );
        object.SetUsesCollision(false);
        object.SetIsVisible(false);
        object.objectFlags.bHasBrokenGlass = true;
    }
}

// 0x71C1A0
void CGlass::WindowRespondsToExplosion(CEntity* entity, CVector pos) {
    if (!entity->GetUsesCollision()) {
        return;
    }

    const auto& entityPos = entity->GetPosition();
    const auto entityToPosDir = entityPos - pos;
    // exe 0x71C1D0-0x71C20E: the length is sqrt((z^2 + y^2) + x^2), and `dist < 10` is the strict ordered test (NaN takes the `>= 10` arm via jp)
    const float dist = (float)std::sqrt(((x87d)entityToPosDir.z * entityToPosDir.z + (x87d)entityToPosDir.y * entityToPosDir.y) + (x87d)entityToPosDir.x * entityToPosDir.x);
    if (!(dist < 10.f)) {
        if (dist < 30.f) {
            entity->AsObject()->objectFlags.bHasBrokenGlass = true;
        }
    } else {
        WindowRespondsToCollision(entity, 10000.f, entityToPosDir * (0.3f / dist), entityPos, true);
    }
}

// 0x46A760
bool CGlass::IsObjectGlass(CEntity* entity) {
    if (!entity->GetIsTypeObject()) {
        return false;
    }

    auto mi = CModelInfo::GetModelInfo(entity->GetModelIndex());
    if (!mi->AsAtomicModelInfoPtr()) {
        return false;
    }

    return mi->IsGlass();
}

// Initial values of the globals declared with NOTSA_GLOBAL_DECL in Glass.h (detached mode; ignored in address mode)
NOTSA_GLOBAL_DEF(CGlass, PanePolyPositions, 0x8D5CD8, (CVector2D[5][3]), {
    { { 0.0f, 0.0f }, { 0.0f, 1.0f }, { 0.4f, 0.5f } },
    { { 0.0f, 1.0f }, { 1.0f, 1.0f }, { 0.4f, 0.5f } },
    { { 0.0f, 0.0f }, { 0.4f, 0.5f }, { 0.7f, 0.0f } },
    { { 0.7f, 0.0f }, { 0.4f, 0.5f }, { 1.0f, 1.0f } },
    { { 0.7f, 0.0f }, { 1.0f, 1.0f }, { 1.0f, 0.0f } }
});

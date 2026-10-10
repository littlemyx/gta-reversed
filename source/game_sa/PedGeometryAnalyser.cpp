#include "StdInc.h"

#include <cfloat>

#include "PedGeometryAnalyser.h"
#include "InteriorManager_c.h"

//! Margin by which the bounding boxes are inflated by (0x8D22B0, 0.35f)
//! NOTE: `ComputeRouteRoundEntityBoundingBox` temporarily overrides this global
static auto& s_BoundingBoxMargin = StaticRef<float>(0x8D22B0);

void CPedGeometryAnalyser::InjectHooks() {
    RH_ScopedClass(CPedGeometryAnalyser);
    RH_ScopedCategoryGlobal();

    RH_ScopedOverloadedInstall(CanPedJumpObstacle, "", 0x5F1B00, bool(*)(const CPed&,const CEntity&));
    RH_ScopedOverloadedInstall(CanPedJumpObstacle, "contacted", 0x5F32D0, bool(*)(const CPed&,const CEntity&,const CVector&,const CVector&));
    RH_ScopedInstall(CanPedTargetPed, 0x5F1C40);
    RH_ScopedInstall(CanPedTargetPoint, 0x5F1B70);
    RH_ScopedInstall(ComputeBuildingHitPoints, 0x5F1E30);
    RH_ScopedInstall(ComputeClearTarget, 0x5F5D80);
    RH_ScopedOverloadedInstall(ComputeClosestSurfacePoint, "ped", 0x5F3B70, bool (*)(const CPed& ped, CEntity& entity, CVector& point));
    RH_ScopedOverloadedInstall(ComputeClosestSurfacePoint, "posn", 0x5F36F0, bool(*)(const CVector&,CEntity&,CVector&));
    RH_ScopedOverloadedInstall(ComputeClosestSurfacePoint, "rect", 0x5F2C10, bool(*)(const CVector&,const CVector*,CVector&));
    RH_ScopedInstall(ComputeEntityBoundingBoxCentreUncached, 0x5F1600);
    RH_ScopedInstall(ComputeEntityBoundingBoxCentreUncachedAll, 0x5F3B40);
    RH_ScopedInstall(ComputeEntityBoundingBoxCorners, 0x5F3650);
    RH_ScopedInstall(ComputeEntityBoundingBoxCornersUncached, 0x5F1FA0);
    RH_ScopedInstall(ComputeEntityBoundingBoxPlanes, 0x5F3660);
    RH_ScopedInstall(ComputeEntityBoundingBoxPlanesUncached, 0x5F1670);
    RH_ScopedInstall(ComputeEntityBoundingBoxPlanesUncachedAll, 0x5F2B80);
    RH_ScopedInstall(ComputeEntityBoundingBoxSegmentPlanes, 0x5F36A0);
    RH_ScopedInstall(ComputeEntityBoundingBoxSegmentPlanesUncached, 0x5F1750);
    RH_ScopedInstall(ComputeEntityBoundingBoxSegmentPlanesUncachedAll, 0x5F2BC0);
    RH_ScopedInstall(ComputeEntityBoundingSphere, 0x5F3C20);
    RH_ScopedInstall(ComputeMoveDirToAvoidEntity, 0x5F3730);
    RH_ScopedInstall(ComputeEntityDirs, 0x5F1500);
    RH_ScopedOverloadedInstall(ComputeEntityHitSide, "1", 0x5F3BC0, int32 (*)(const CPed& ped, CEntity& entity));
    RH_ScopedOverloadedInstall(ComputeEntityHitSide, "2", 0x5F1450, int32 (*)(const CVector& point1, const CVector* point2, const float* x));
    RH_ScopedOverloadedInstall(ComputeEntityHitSide, "3", 0x5F3AC0, int32 (*)(const CVector& point, CEntity& entity));
    RH_ScopedOverloadedInstall(ComputePedHitSide, "physical", 0x5F3640, int32(*)(const CPed&,const CPhysical&));
    RH_ScopedOverloadedInstall(ComputePedHitSide, "posn", 0x5F1E70, int32(*)(const CPed&,const CVector&));
    RH_ScopedInstall(ComputePedShotSide, 0x5F13F0);
    RH_ScopedOverloadedInstall(ComputeRouteRoundEntityBoundingBox, "1", 0x5F6110, int32(*)(const CPed&,CEntity&,const CVector&,CPointRoute&,int32));
    RH_ScopedOverloadedInstall(ComputeRouteRoundEntityBoundingBox, "2", 0x5F3DD0, int32(*)(const CPed&,const CVector&,CEntity&,const CVector&,CPointRoute&,int32));
    RH_ScopedInstall(ComputeRouteRoundSphere, 0x5F1890);
    RH_ScopedOverloadedInstall(GetIsLineOfSightClear, "ped", 0x5F5A30, bool(*)(const CPed&,const CVector&,CEntity&,float&));
    RH_ScopedOverloadedInstall(GetIsLineOfSightClear, "v3d", 0x5F2F00, bool(*)(const CVector&,const CVector&,CEntity&));
    RH_ScopedInstall(GetNearestPed, 0x5F3590);
    RH_ScopedInstall(IsEntityBlockingTarget, 0x5F3970);
    RH_ScopedInstall(IsInAir, 0x5F1CB0);
    RH_ScopedInstall(IsWanderPathClear, 0x5F2F70);
    RH_ScopedInstall(LiesInsideBoundingBox, 0x5F3880);
}

// 0x5F1B00
bool CPedGeometryAnalyser::CanPedJumpObstacle(const CPed& ped, const CEntity& entity) {
    if (entity.m_bIsTempBuilding) {
        return false;
    }

    const auto& pedPos = ped.GetPosition();
    const CVector target = pedPos + ped.GetForward(); // NOTE: The original reads the forward vector straight from the matrix (would crash without one)
    return CWorld::GetIsLineOfSightClear(pedPos, target, true, false, false, true, false, false, false);
}

// 0x5F32D0
bool CPedGeometryAnalyser::CanPedJumpObstacle(const CPed& ped, const CEntity& entity, const CVector& contactNormal, const CVector& contactPos) {
    // NOTE: `contactPos` is not used by the original code

    CVector pos = ped.GetPosition();
    CVector dir = ped.GetForward(); // NOTE: Same as above, the original reads this straight from the matrix

    if (entity.m_bIsTempBuilding) {
        return false;
    }

    if (g_surfaceInfos.IsShallowWater(ped.m_nContactSurface)) {
        return true;
    }

    if (contactNormal.z <= 0.17f) { // 0x86C6AC
        if (!CPedGroups::IsInPlayersGroup(const_cast<CPed*>(&ped))) {
            pos.z -= 0.15f; // 0x858FCC
        }
    } else {
        if (contactNormal.z > 0.9f) { // 0x858C20
            return false;
        }

        // Raise the origin to the middle of the ped's first col sphere (adjusted by the slope)
        const auto& sphere = ped.GetColModel()->m_pColData->m_pSpheres[0];
        pos.z += sphere.m_vecCenter.z - sphere.m_fRadius * contactNormal.z;

        const auto horizontalLen = std::sqrt(contactNormal.y * contactNormal.y + contactNormal.x * contactNormal.x);
        if (contactNormal.z <= 0.5f) { // 0x858B8C
            dir += (dir * horizontalLen) * sphere.m_fRadius;
        } else {
            dir = CVector{ -contactNormal.x, -contactNormal.y, 0.f };
            dir *= 1.f / horizontalLen; // 0x411A30 - BUG: Division by zero is possible here (vertical surface)
            dir += (dir * horizontalLen) * sphere.m_fRadius;
            dir *= std::min(2.f / horizontalLen, 4.f); // 0x858CA0, 0x858B90
        }
    }

    const CVector target = pos + dir;
    if (!CWorld::GetIsLineOfSightClear(pos, target, true, false, false, true, false, false, false)) {
        return false;
    }

    // Check if there's a ground to land on
    const CVector probe = pos + dir * 3.f; // 0x858B3C
    bool          foundGround{};
    const auto    groundZ = CWorld::FindGroundZFor3DCoord(probe, &foundGround, nullptr);
    return foundGround && probe.z - groundZ < 3.f;
}

// 0x5F1C40
bool CPedGeometryAnalyser::CanPedTargetPed(CPed& ped, CPed& targetPed, bool checkDirection) {
    return CanPedTargetPoint(
        ped,
        targetPed.GetPosition() + CVector{ 0.f, 0.f, targetPed.GetTaskManager().GetTaskSecondary(TASK_SECONDARY_DUCK) ? -0.25f : 0.75f }, // 0.75f - 1.f = -0.25f
        checkDirection
    );
}

// 0x5F1B70
bool CPedGeometryAnalyser::CanPedTargetPoint(const CPed& ped, const CVector& point, bool checkDirection) {
    const auto& pedPos = ped.GetPosition();
    const CVector delta = point - pedPos;

    if (checkDirection) {
        const auto& fwd = ped.GetForward();
        if (delta.z * fwd.z + delta.y * fwd.y + delta.x * fwd.x < 0.f) {
            return false;
        }
    }

    if (delta.z * delta.z + delta.y * delta.y + delta.x * delta.x > 40.f * 40.f) { // 0x86C69C
        return false;
    }

    const CVector origin = { pedPos.x, pedPos.y, pedPos.z + 0.75f }; // 0x858F34
    return CWorld::GetIsLineOfSightClear(origin, point, true, false, false, true, false, true, false);
}

// 0x5F1E30
// unused
int32 CPedGeometryAnalyser::ComputeBuildingHitPoints(const CVector& a1, const CVector& a2) {
    CEntity *outEntity;
    CColPoint v4;

    CWorld::ProcessLineOfSight(a1, a2, v4, outEntity, true, false, false, false, true, false, false, false);
    return CWorld::ms_iProcessLineNumCrossings;
}

// 0x5F5D80
void CPedGeometryAnalyser::ComputeClearTarget(const CPed& ped, const CVector& target, CVector& outTarget) {
    outTarget = target;

    const auto& pedPos = ped.GetPosition();
    auto* const intel  = ped.GetIntelligence();

    // Push the target out of the entities (in front of the ped) that would block the line of sight
    float distToEdge{};
    const auto PushTargetOutOf = [&](CEntity* entity) {
        if (!entity) {
            return;
        }
        if (DistanceBetweenPointsSquared(entity->GetPosition(), outTarget) >= 5.f * 5.f) { // 0x86C6A4
            return;
        }
        if (!LiesInsideBoundingBox(ped, outTarget, *entity)) {
            return;
        }
        if (GetIsLineOfSightClear(ped, outTarget, *entity, distToEdge)) {
            return;
        }
        CVector dir = outTarget - pedPos;
        dir.Normalise();
        outTarget -= dir * (s_BoundingBoxMargin + distToEdge);
    };
    for (auto* const entity : intel->m_vehicleScanner.m_apEntities) {
        PushTargetOutOf(entity);
    }
    for (auto* const entity : intel->m_pedScanner.m_apEntities) {
        PushTargetOutOf(entity);
    }

    // Move the target towards the ped until it's not inside of a building (by crossing a wall)
    CVector step = pedPos - outTarget;
    step.Normalise();
    step *= s_BoundingBoxMargin;

    const auto maxIterations = (int32)(5.f / s_BoundingBoxMargin) + 1;
    for (auto i = 0; i < maxIterations;) {
        i++;

        const CVector toPed = pedPos - outTarget;
        if (toPed.SquaredMagnitude() >= 5.f * 5.f) {
            break;
        }
        if (toPed.Dot(step) < 0.f) {
            break;
        }

        CColPoint colPoint;
        CEntity*  hitEntity{};
        CWorld::ProcessLineOfSight(pedPos, outTarget, colPoint, hitEntity, true, false, false, false, true, false, false, false);
        if (CWorld::ms_iProcessLineNumCrossings % 2 != 1) { // Is the target inside of a building?
            break;
        }
        outTarget += step;
    }
}

// 0x5F3B70
bool CPedGeometryAnalyser::ComputeClosestSurfacePoint(const CPed& ped, CEntity& entity, CVector& point) {
    CVector corners[4];
    const auto& posn = ped.GetPosition();
    ComputeEntityBoundingBoxCornersUncached(posn.z, entity, corners);
    return ComputeClosestSurfacePoint(posn, corners, point);
}

// 0x5F36F0
bool CPedGeometryAnalyser::ComputeClosestSurfacePoint(const CVector& posn, CEntity& entity, CVector& point) {
    CVector corners[4];
    ComputeEntityBoundingBoxCornersUncached(posn.z, entity, corners);
    return ComputeClosestSurfacePoint(posn, corners, point);
}

// 0x5F2C10
bool CPedGeometryAnalyser::ComputeClosestSurfacePoint(const CVector& posn, const CVector* corners, CVector& point) {
    bool  found{};
    float closestDistSq = FLT_MAX;

    // Closest point on each of the edges
    for (auto i = 0u; i < 4; i++) {
        const auto& a = corners[i % 4];
        const auto& b = corners[(i + 1) % 4];

        const float edgeX = b.x - a.x;
        const float edgeY = b.y - a.y;
        const float edgeZ = b.z - a.z;
        const float len   = std::sqrt(edgeX * edgeX + edgeY * edgeY + edgeZ * edgeZ);
        const float inv   = 1.f / len;

        const float dirZ = edgeZ * inv;
        const float t    = (posn.x - a.x) * edgeX * inv + (posn.y - a.y) * edgeY * inv + (posn.z - a.z) * dirZ; // Distance along the edge
        if (t >= 0.f && t <= len) {
            const CVector p = {
                edgeX * inv * t + a.x,
                edgeY * inv * t + a.y,
                dirZ * t + a.z,
            };
            const float dx = posn.x - p.x;
            const float dy = posn.y - p.y;
            const float distSq = dx * dx + dy * dy + (posn.z - p.z) * (posn.z - p.z);
            if (distSq < closestDistSq) {
                point         = p;
                found         = true;
                closestDistSq = distSq;
            }
        }
    }

    // If no edge was closer, try the corners
    if (!found) {
        for (auto i = 0u; i < 4; i++) {
            const auto& c = corners[i];
            const float distSq = (c.x - posn.x) * (c.x - posn.x) + (c.y - posn.y) * (c.y - posn.y) + (c.z - posn.z) * (c.z - posn.z);
            if (distSq < closestDistSq) {
                point         = c;
                found         = true;
                closestDistSq = distSq;
            }
        }
    }

    return found;
}

// inlined into CPedGeometryAnalyser::ComputeEntityBoundingSphere
void CPedGeometryAnalyser::ComputeEntityBoundingBoxCentre(float zPos, CEntity& entity, CVector& center) {
    ComputeEntityBoundingBoxCentreUncachedAll(zPos, entity, center);
}

// 0x5F1600
void CPedGeometryAnalyser::ComputeEntityBoundingBoxCentreUncached(float zPos, const CVector* corners, CVector& center) {
    center.Set(0.0f, 0.0f, zPos);

    center.x = corners[0].x;
    center.y = corners[0].y;

    center.x += corners[1].x;
    center.y += corners[1].y;

    center.x += corners[2].x;
    center.y += corners[2].y;

    center.x += corners[3].x;
    center.y += corners[3].y;

    center.x *= 0.25f;
    center.y *= 0.25f;
}

// 0x5F3B40
void CPedGeometryAnalyser::ComputeEntityBoundingBoxCentreUncachedAll(float zPos, CEntity& entity, CVector& center) {
    CVector corners[4];
    ComputeEntityBoundingBoxCornersUncached(zPos, entity, corners);
    ComputeEntityBoundingBoxCentreUncached(zPos, corners, center);
}

// 0x5F3650
void CPedGeometryAnalyser::ComputeEntityBoundingBoxCorners(float zPos, CEntity& entity, CVector* corners) {
    ComputeEntityBoundingBoxCornersUncached(zPos, entity, corners);
}

// 0x5F1FA0
void CPedGeometryAnalyser::ComputeEntityBoundingBoxCornersUncached(float zPos, CEntity& entity, CVector* corners) {
    // Interior furniture
    if (entity.GetIsTypeBuilding() && entity.m_bIsTempBuilding) {
        if (g_interiorMan.GetBoundingBox(&entity, corners)) {
            for (auto i = 0u; i < 4; i++) {
                corners[i].z = zPos;
            }
            return;
        }
    }

    auto&       mat      = entity.GetMatrix();
    auto* const colModel = entity.GetModelInfo()->GetColModel(); // NOTE: Not `entity.GetColModel()`, as the original doesn't handle the special vehicle col models
    auto* const colData  = colModel->m_pColData;

    // Local space bounding box (min/max)
    CVector bbMin, bbMax;
    if (entity.GetIsTypeObject()
        && colData
        && colModel->m_boundBox.m_vecMax.z - colModel->m_boundBox.m_vecMin.z > 6.f // 0x86C6A8
        && (colData->m_nNumBoxes || colData->m_nNumSpheres)
    ) {
        // Tall object: only take the collision volumes that are around the `zPos` into account
        bbMin = CVector{ FLT_MAX };
        bbMax = CVector{ -FLT_MAX };

        const float zLow  = zPos - 1.f;
        const float zHigh = zPos + 1.f;
        const auto IsInZRange = [&](const CVector& localMin, const CVector& localMax) {
            const float z1 = mat.TransformPoint(localMin).z;
            const float z2 = mat.TransformPoint(localMax).z;
            return (zLow <= z1 || zLow <= z2) && (z1 <= zHigh || z2 <= zHigh);
        };
        const auto StretchBB = [&](const CVector& localMin, const CVector& localMax) {
            bbMin.x = std::min(bbMin.x, localMin.x);
            bbMin.y = std::min(bbMin.y, localMin.y);
            bbMin.z = std::min(bbMin.z, localMin.z);
            bbMax.x = std::max(bbMax.x, localMax.x);
            bbMax.y = std::max(bbMax.y, localMax.y);
            bbMax.z = std::max(bbMax.z, localMax.z);
        };

        for (const auto& box : std::span{ colData->m_pBoxes, colData->m_nNumBoxes }) {
            if (IsInZRange(box.m_vecMin, box.m_vecMax)) {
                StretchBB(box.m_vecMin, box.m_vecMax);
            }
        }
        for (const auto& sphere : std::span{ colData->m_pSpheres, colData->m_nNumSpheres }) {
            const auto& c = sphere.m_vecCenter;
            const auto  r = sphere.m_fRadius;
            const CVector localMin{ c.x - r, c.y - r, c.z - r };
            const CVector localMax{ c.x + r, c.y + r, c.z + r };
            if (IsInZRange(localMin, localMax)) {
                StretchBB(localMin, localMax);
            }
        }
    } else {
        bbMin = colModel->m_boundBox.m_vecMin;
        bbMax = colModel->m_boundBox.m_vecMax;
    }

    // Calculate the extents of the (margin-inflated) box, and its center
    const float margin = s_BoundingBoxMargin;
    const auto& right  = mat.GetRight();
    const auto& fwd    = mat.GetForward();
    const auto& up     = mat.GetUp();

    const float rightLen2D = right.y * right.y + right.x * right.x;
    const float fwdLen2D   = fwd.y * fwd.y + fwd.x * fwd.x;
    const float upLen2D    = up.y * up.y + up.x * up.x;

    const float halfX = ((margin + bbMax.x) - (bbMin.x - margin)) * 0.5f;
    const float halfY = ((margin + bbMax.y) - (bbMin.y - margin)) * 0.5f;
    const float halfZ = ((margin + bbMax.z) - (bbMin.z - margin)) * 0.5f;

    const CVector localCenter{
        (margin + bbMax.x + (bbMin.x - margin)) * 0.5f,
        (margin + bbMax.y + (bbMin.y - margin)) * 0.5f,
        (margin + bbMax.z + (bbMin.z - margin)) * 0.5f
    };
    const CVector center = mat.TransformPoint(localCenter);

    // Find the axis of the entity (right, forward or up) that is the longest when projected onto the XY plane
    const float projX = halfX * rightLen2D * 2.f;
    const float projY = halfY * fwdLen2D * 2.f;
    const float projZ = halfZ * upLen2D * 2.f;

    enum class eAxis { RIGHT, FORWARD, UP } axis;
    if (projX <= projY || projX <= projZ) {
        axis = projY <= projZ ? eAxis::UP : eAxis::FORWARD;
    } else {
        axis = eAxis::RIGHT;
    }

    // Axis (not normalized!) that the box is oriented along, and the half length along it
    CVector axisDir;
    float   axisHalfLen{};
    switch (axis) {
    case eAxis::RIGHT:   axisDir = right; axisHalfLen = halfX; break;
    case eAxis::FORWARD: axisDir = fwd;   axisHalfLen = halfY; break;
    case eAxis::UP:      axisDir = up;    axisHalfLen = halfZ; break;
    }
    axisDir.z = 0.f;

    // Both ends of the box (along its main axis)
    const float endPlusX  = axisDir.x * axisHalfLen + center.x;
    const float endPlusY  = axisDir.y * axisHalfLen + center.y;
    const float endMinusX = center.x - axisDir.x * axisHalfLen;
    const float endMinusY = center.y - axisDir.y * axisHalfLen;

    CVector dir = axisDir;
    dir.Normalise();

    // Project the other two axes onto the main axis (`along`) and its perpendicular (`across`)
    const auto Dot2D   = [&](const CVector& v) { return dir.x * v.x + dir.y * v.y; };
    const auto Cross2D = [&](const CVector& v) { return dir.x * v.y - dir.y * v.x; };

    float along1{}, across1{}, along2{}, across2{};
    switch (axis) {
    case eAxis::RIGHT:
        along1 = Dot2D(fwd) * halfY; across1 = Cross2D(fwd) * halfY;
        along2 = Dot2D(up)  * halfZ; across2 = Cross2D(up)  * halfZ;
        break;
    case eAxis::FORWARD:
        along1 = Dot2D(right) * halfX; across1 = Cross2D(right) * halfX;
        along2 = Dot2D(up)    * halfZ; across2 = Cross2D(up)    * halfZ;
        break;
    case eAxis::UP:
        along1 = Dot2D(right) * halfX; across1 = Cross2D(right) * halfX;
        along2 = Dot2D(fwd)   * halfY; across2 = Cross2D(fwd)   * halfY;
        break;
    }

    const float extraLen   = std::abs(along2) + std::abs(along1);
    const float extraWidth = std::abs(across2) + std::abs(across1);

    const float alongX = extraLen * dir.x;
    const float alongY = extraLen * dir.y;
    const float sideX  = extraWidth * dir.y;
    const float sideY  = -dir.x * extraWidth;

    corners[0] = CVector{ (alongX + endPlusX) - sideX,   (endPlusY + alongY) - sideY,   zPos };
    corners[1] = CVector{ (endMinusX - alongX) - sideX,  (endMinusY - alongY) - sideY,  zPos };
    corners[2] = CVector{ (endMinusX - alongX) + sideX,  (endMinusY - alongY) + sideY,  zPos };
    corners[3] = CVector{ alongX + endPlusX + sideX,     alongY + endPlusY + sideY,     zPos };
}

// 0x5F3660
void CPedGeometryAnalyser::ComputeEntityBoundingBoxPlanes(float zPos, CEntity& entity, CVector(*outPlanes)[4], float* outPlanesDot) {
    CVector corners[4];
    ComputeEntityBoundingBoxCornersUncached(zPos, entity, corners);
    ComputeEntityBoundingBoxPlanesUncached(zPos, corners, outPlanes, outPlanesDot);
}

// 0x5F1670
void CPedGeometryAnalyser::ComputeEntityBoundingBoxPlanesUncached(float zPos, const CVector* corners, CVector(*outPlanes)[4], float* outPlanesDot) {
    const CVector* corner2 = &corners[3];
    for (auto i = 0; i < 4; i++) {
        const CVector& corner = corners[i];
        CVector& plane = (*outPlanes)[i];
        CVector direction = corner - *corner2;
        direction.Normalise();
        plane.x = direction.y;
        plane.y = -direction.x;
        plane.z = 0.0f;
        // point-normal plane equation:
        // ax + by + cz + d = 0
        // d = - n . P
        outPlanesDot[i] = -DotProduct(plane, *corner2);

        corner2 = &corner;
    }
}

// 0x5F2B80
void CPedGeometryAnalyser::ComputeEntityBoundingBoxPlanesUncachedAll(float zPos, CEntity& entity, CVector (*outPlanes)[4], float* outPlanesDot) {
    CVector corners[4];
    CPedGeometryAnalyser::ComputeEntityBoundingBoxCornersUncached(zPos, entity, corners);
    CPedGeometryAnalyser::ComputeEntityBoundingBoxPlanesUncached(zPos, corners, outPlanes, outPlanesDot);
}

// 0x5F36A0
void CPedGeometryAnalyser::ComputeEntityBoundingBoxSegmentPlanes(float zPos, CEntity& entity, CVector* normals, float* dots) {
    ComputeEntityBoundingBoxSegmentPlanesUncachedAll(zPos, entity, normals, dots);
}

// 0x5F1750
CVector* CPedGeometryAnalyser::ComputeEntityBoundingBoxSegmentPlanesUncached(const CVector* corners, CVector& center, CVector* normals, float* dots) {
    // Planes going through the center of the box and each of the corners
    for (auto i = 0; i < 4; i++) {
        const auto& corner = corners[i];
        auto&       normal = normals[i];

        normal.x = -(corner.y - center.y);
        normal.y = corner.x - center.x;
        normal.z = 0.f;

        dots[i] = -(corner.z * normal.z + normal.y * corner.y + corner.x * normal.x);
    }

    // NOTE: The original function returns (by accident) the `corners` pointer
    return const_cast<CVector*>(corners);
}

// 0x5F2BC0
CVector* CPedGeometryAnalyser::ComputeEntityBoundingBoxSegmentPlanesUncachedAll(float zPos, CEntity& entity, CVector* a3, float* a4) {
    CVector corners[4];
    CVector center;

    ComputeEntityBoundingBoxCornersUncached(zPos, entity, corners);
    ComputeEntityBoundingBoxCentreUncached(zPos, corners, center);
    return ComputeEntityBoundingBoxSegmentPlanesUncached(corners, center, a3, a4);
}

// 0x5F3C20
void CPedGeometryAnalyser::ComputeEntityBoundingSphere(const CPed& ped, CEntity& entity, CColSphere& sphere) {
    const auto zPos = ped.GetPosition().z;

    CVector corners[4];
    ComputeEntityBoundingBoxCornersUncached(zPos, entity, corners);

    CVector center;
    ComputeEntityBoundingBoxCentre(zPos, entity, center); // Inlined in the original (recomputes the corners)

    // Find the corner furthest away from the center
    float maxDistSq = 0.f;
    for (const auto& corner : corners) {
        const float distSq = (corner.x - center.x) * (corner.x - center.x)
                           + (corner.y - center.y) * (corner.y - center.y)
                           + (corner.z - center.z) * (corner.z - center.z);
        if (maxDistSq < distSq) {
            maxDistSq = distSq;
        }
    }

    sphere.Set(std::sqrt(maxDistSq) * 1.1f, center, SURFACE_DEFAULT); // 0x858F14
}

// 0x5F3730
void CPedGeometryAnalyser::ComputeMoveDirToAvoidEntity(const CPed& ped, CEntity& entity, CVector& outDirToAvoidEntity) {
    const auto& pedPos = ped.GetPosition();

    CVector corners[4];
    ComputeEntityBoundingBoxCornersUncached(pedPos.z, entity, corners);

    CVector planes[4];
    float   dots[4];
    ComputeEntityBoundingBoxPlanesUncached(pedPos.z, corners, &planes, dots);

    // Distance to the planes on the sides of the box
    const float dist1 = planes[1].y * pedPos.y + planes[1].z * pedPos.z + planes[1].x * pedPos.x + dots[1];
    const float dist3 = planes[3].y * pedPos.y + planes[3].z * pedPos.z + planes[3].x * pedPos.x + dots[3];

    if (dist1 > 0.f) {
        outDirToAvoidEntity = planes[1];
    } else if (dist3 > 0.f) {
        outDirToAvoidEntity = planes[3];
    } else if (dist3 < dist1) { // Behind both: pick the closer one
        outDirToAvoidEntity = planes[1];
    } else {
        outDirToAvoidEntity = planes[3];
    }
}

//! @notsa
CVector CPedGeometryAnalyser::ComputeEntityDir(const CEntity& entity, eDirection dir) {
    switch (dir) {
    case eDirection::FORWARD:  return entity.GetForward();
    case eDirection::LEFT:     return -entity.GetRight();
    case eDirection::BACKWARD: return -entity.GetForward();
    case eDirection::RIGHT:    return entity.GetRight();
    default:                   NOTSA_UNREACHABLE();
    }
}

// 0x5F1500
CVector* CPedGeometryAnalyser::ComputeEntityDirs(const CEntity& entity, CVector* outDirs) {
    CVector fwd, right;
    if (const auto* const mat = entity.m_matrix) {
        fwd   = mat->GetForward();
        right = mat->GetRight();
    } else {
        const auto heading = entity.m_placement.m_fHeading;
        fwd   = CVector{ (float)(-x87::sin(heading)), (float)(x87::cos(heading)), 0.f };
        right = CVector{ (float)(x87::cos(heading)), (float)(x87::sin(heading)), 0.f };
    }

    outDirs[0] = fwd;                    // eDirection::FORWARD
    outDirs[1] = right * -1.f;           // eDirection::LEFT
    outDirs[2] = fwd * -1.f;             // eDirection::BACKWARD
    outDirs[3] = right;                  // eDirection::RIGHT

    return &outDirs[3]; // NOTE: Original returns this by accident (it's the last written vector)
}

// 0x5F3BC0
int32 CPedGeometryAnalyser::ComputeEntityHitSide(const CPed& ped, CEntity& entity) {
    return ComputeEntityHitSide(ped.GetPosition(), entity);
}

// 0x5F1450
int32 CPedGeometryAnalyser::ComputeEntityHitSide(const CVector& point, const CVector* planes, const float* dots) {
    const auto DistToPlane = [&](uint32 i) {
        return point.y * planes[i].y + planes[i].z * point.z + point.x * planes[i].x + dots[i];
    };

    // Find the plane that the point is in front of (the previous plane being behind it)
    for (auto i = 0u; i < 4; i++) {
        const auto prev = (i + 3) % 4;
        if (DistToPlane(prev) >= 0.f && DistToPlane(i) < 0.f) {
            return (int32)i;
        }
    }
    return 0;
}

// 0x5F3AC0
int32 CPedGeometryAnalyser::ComputeEntityHitSide(const CVector& point, CEntity& entity) {
    const auto zPos = point.z;

    CVector corners[4];
    ComputeEntityBoundingBoxCornersUncached(zPos, entity, corners);

    CVector center;
    ComputeEntityBoundingBoxCentreUncached(zPos, corners, center);

    CVector planes[4];
    float   dots[4];
    ComputeEntityBoundingBoxSegmentPlanesUncached(corners, center, planes, dots);

    return ComputeEntityHitSide(point, planes, dots);
}

// 0x5F3640
int32 CPedGeometryAnalyser::ComputePedHitSide(const CPed& ped, const CPhysical& physical) {
    return ComputePedHitSide(ped, physical.m_vecMoveSpeed);
}

// 0x5F1E70
int32 CPedGeometryAnalyser::ComputePedHitSide(const CPed& ped, const CVector& velocity) {
    CVector toSource = velocity * -1.f;
    toSource.Normalise();

    CVector dirs[4];
    ComputeEntityDirs(ped, dirs);

    // Find the direction that is the closest to `toSource` (if there's a tie, the last one wins)
    int32 side{};
    float closest = -1.f;
    for (auto i = 0; i < 4; i++) {
        const float dot = dirs[i].z * toSource.z + dirs[i].y * toSource.y + dirs[i].x * toSource.x;
        if (dot >= closest) {
            side    = i;
            closest = dot;
        }
    }
    return side;
}

// 0x5F13F0
int32 CPedGeometryAnalyser::ComputePedShotSide(const CPed& ped, const CVector& posn) {
    constexpr double QUARTER_PI  = 0.78539818525314331; // 0x859AB0 (float)
    constexpr double TWO_PI_F    = 6.2831854820251465;  // 0x858CBC (float)
    constexpr double TWO_OVER_PI = 0.63661974668502808; // 0x858FB8 (float)

    const auto& pedPos = ped.GetPosition();

    double angle = x87::atan2(-((double)posn.x - (double)pedPos.x), (double)posn.y - (double)pedPos.y); // Heading from the ped to `posn`
    angle = angle - (double)ped.m_fCurrentRotation + QUARTER_PI;
    if (angle < 0.0) {
        angle += TWO_PI_F;
    }
    return (int32)(angle * TWO_OVER_PI); // Quadrant => `eDirection`
}

// 0x5F6110
int32 CPedGeometryAnalyser::ComputeRouteRoundEntityBoundingBox(const CPed& ped, CEntity& entity, const CVector& posn, CPointRoute& pointRoute, int32 a5) {
    return ComputeRouteRoundEntityBoundingBox(ped, ped.GetPosition(), entity, posn, pointRoute, a5);
}

// 0x5F3DD0
int32 CPedGeometryAnalyser::ComputeRouteRoundEntityBoundingBox(const CPed& ped, const CVector& from, CEntity& entity, const CVector& to, CPointRoute& route, int32 mode) {
    route.Clear();

    const float pedZ = ped.GetPosition().z;

    CVector corners[4];
    CVector planes[4];
    float   planesDots[4];

    // Check if the whole line is in front of (outside) of one of the planes of the (smaller) bounding box
    {
        const auto oldMargin = std::exchange(s_BoundingBoxMargin, 0.175f);
        ComputeEntityBoundingBoxCornersUncached(pedZ, entity, corners);
        ComputeEntityBoundingBoxPlanesUncached(pedZ, corners, &planes, planesDots);
        s_BoundingBoxMargin = oldMargin;
    }

    const auto DistToPlane = [&](uint32 i, const CVector& p) {
        return planes[i].z * p.z + planes[i].y * p.y + p.x * planes[i].x + planesDots[i];
    };
    for (auto i = 0u; i < 4; i++) {
        if (DistToPlane(i, from) > 0.f && DistToPlane(i, to) > 0.f) {
            return 0;
        }
    }

    // Check if the line goes through the box (the points are on a different side of it)
    ComputeEntityBoundingBoxCornersUncached(pedZ, entity, corners);
    {
        CVector center;
        ComputeEntityBoundingBoxCentreUncached(pedZ, corners, center);

        CVector segPlanes[4];
        float   segDots[4];
        ComputeEntityBoundingBoxSegmentPlanesUncached(corners, center, segPlanes, segDots);

        const auto fromSide = ComputeEntityHitSide(from, segPlanes, segDots);
        const auto toSide   = ComputeEntityHitSide(to, segPlanes, segDots);
        if (fromSide == toSide) {
            return 0;
        }
    }

    ComputeEntityBoundingBoxPlanesUncached(pedZ, corners, &planes, planesDots);

    // Clip the line to the (normal margin) box
    CVector p = from, q = to;
    CVector dir = q - p;
    dir.Normalise();
    const CVector pOrig = p;

    const auto ClipToPlane = [&](uint32 i) { // Returns the point where the line (starting at `pOrig`) crosses the plane
        const float t = -(DistToPlane(i, pOrig) / (dir.y * planes[i].y + dir.x * planes[i].x + dir.z * planes[i].z));
        return CVector{ dir.x * t + pOrig.x, dir.y * t + pOrig.y, dir.z * t + pOrig.z };
    };
    const auto Classify = [](float dist) { // 1 = in front of the plane (outside), 0 = near the plane, -1 = behind it (inside)
        if (dist > 0.2f) { // 0x858CC4
            return 1;
        }
        return dist >= -0.2f /* 0x858FDC */ ? 0 : -1;
    };

    bool isOutsideOnSomePlane{};
    for (auto i = 0u; i < 4; i++) {
        const auto& n = planes[i];

        const float distP = p.y * n.y + p.x * n.x + p.z * n.z + planesDots[i];
        const float distQ = q.y * n.y + q.x * n.x + q.z * n.z + planesDots[i];

        const auto classP = Classify(distP);
        const auto classQ = Classify(distQ);

        if (classP == -1) {
            if (classQ == 1) {
                q = ClipToPlane(i);
            }
        } else if (classQ == -1) {
            if (classP == 1) {
                p = ClipToPlane(i);
            }
        } else {
            isOutsideOnSomePlane = true;
            if (classP == 0) {
                p += n * (0.2f - distP);
            }
            if (classQ == 0) {
                q += n * (0.2f - distQ);
            }
        }
    }

    if (!isOutsideOnSomePlane) {
        // Use the bounding sphere to clip the line
        CColSphere sphere;
        ComputeEntityBoundingSphere(ped, entity, sphere);

        CVector dir2D{ to.x - from.x, to.y - from.y, 0.f };
        if (std::sqrt(dir2D.y * dir2D.y + dir2D.x * dir2D.x) == 0.f) {
            return 0;
        }
        dir2D.Normalise();

        if (!sphere.IntersectRay(from, dir2D, p, q)) {
            return 0;
        }
    }

    // Find the planes where the line enters and leaves the box
    dir = q - p;
    const CVector pSnapshot = p;
    dir.Normalise();

    int32 entryIdx = -1, exitIdx = -1;
    for (auto i = 0u; i < 4; i++) {
        const auto& n = planes[i];

        const float distP = p.y * n.y + p.z * n.z + p.x * n.x + planesDots[i];
        const float distQ = q.y * n.y + q.z * n.z + q.x * n.x + planesDots[i];

        if (distP == 0.f && distQ == 0.f) {
            continue;
        }

        const auto CrossingPoint = [&] {
            const float t = -((pSnapshot.y * n.y + pSnapshot.z * n.z + pSnapshot.x * n.x + planesDots[i]) / (dir.y * n.y + dir.z * n.z + dir.x * n.x));
            return CVector{ dir.x * t + pSnapshot.x, dir.y * t + pSnapshot.y, dir.z * t + pSnapshot.z };
        };
        if (distP < 0.f || distQ > 0.f) {
            if (distP <= 0.f && distQ >= 0.f) { // Leaves the box
                exitIdx = i;
                q       = CrossingPoint();
            }
        } else { // Enters the box (distP >= 0 && distQ <= 0)
            entryIdx = i;
            p        = CrossingPoint();
        }
    }
    if (exitIdx < 0 || entryIdx < 0) {
        return 0;
    }

    // Build the 2 possible paths around the box (one in each direction)
    std::array<CVector, 6> pathA, pathB;
    uint32                 numA{}, numB{};

    pathA[numA++] = from;
    {
        auto cur = (uint32)(entryIdx + 3) % 4;
        pathA[numA++] = corners[cur];
        if (cur != (uint32)exitIdx) {
            do {
                cur = (cur + 3) % 4;
                pathA[numA++] = corners[cur];
            } while (cur != (uint32)exitIdx);
        }
        pathA[numA++] = to;
    }

    pathB[numB++] = from;
    {
        auto       cur    = (uint32)entryIdx;
        const auto endIdx = (uint32)(exitIdx + 3) % 4;
        pathB[numB++] = corners[cur];
        if (cur != endIdx) {
            do {
                cur = (cur + 1) % 4;
                pathB[numB++] = corners[cur];
            } while (cur != endIdx);
        }
        pathB[numB++] = to;
    }

    // Choose which path to use
    int32 chosenPath; // 1 = A, 2 = B
    if (mode == 0) {
        // Center of the box
        const CVector boxCenter = {
            (corners[0].x + corners[1].x + corners[2].x + corners[3].x) * 0.25f,
            (corners[0].y + corners[1].y + corners[2].y + corners[3].y) * 0.25f,
            (corners[0].z + corners[1].z + corners[2].z + corners[3].z) * 0.25f,
        };

        // Check if the path is blocked
        struct PathInfo {
            bool     isClear{ true };
            uint32   blockedAt{}; // Index of the point the path was blocked on the way to
            CEntity* blockedBy{}; // NOTE: Also set to a non-null value if the path is clear (if it hits something that is ignored)
            float    length{};
        };
        const auto CheckPath = [&](const std::array<CVector, 6>& path, uint32 numPoints) {
            PathInfo info{ .blockedAt = numPoints };

            CVector cur = path[0];
            for (auto k = 1u; k < numPoints; k++) {
                const CVector next = path[k];

                // Offset the line to the outside of the box (to check for something blocking it with a width)
                CVector d{ next.x - cur.x, next.y - cur.y, 0.f };
                d.Normalise();

                CVector offset{ d.y * 0.5f, -(d.x * 0.5f), 0.f };
                if ((next.z - boxCenter.z) * 0.f + (next.y - boxCenter.y) * offset.y + (next.x - boxCenter.x) * offset.x < 0.f) {
                    offset = offset * -1.f;
                }

                CColPoint colPoint;
                const auto IsBlocked = [&](const CVector& a, const CVector& b) {
                    return CWorld::ProcessLineOfSight(a, b, colPoint, info.blockedBy, true, true, true, true, false, false, false, false);
                };
                if (IsBlocked(cur, next) || IsBlocked(cur + offset, next + offset)) {
                    const auto hit = info.blockedBy;
                    if (hit != &entity && hit != &ped) {
                        // Ignore peds that are moving in the direction of the path
                        bool ignore{};
                        if (hit->GetIsTypePed() && hit->AsPed()->m_nMoveState != PEDMOVE_STILL) {
                            const auto& hitFwd = hit->GetMatrix().GetForward();
                            ignore = (next.x - cur.x) * hitFwd.x + (next.y - cur.y) * hitFwd.y + (next.z - cur.z) * hitFwd.z >= 0.f;
                        }
                        if (!ignore) {
                            info.isClear   = false;
                            info.blockedAt = k;
                            break;
                        }
                    } else {
                        info.blockedBy = nullptr;
                    }
                }
                cur = next;
            }

            for (auto k = 0u; k + 1 < numPoints; k++) {
                info.length += (path[k + 1] - path[k]).Magnitude();
            }
            return info;
        };
        const auto infoA = CheckPath(pathA, numA);
        const auto infoB = CheckPath(pathB, numB);

        // If both are blocked by the same thing (or can't be decided otherwise), choose the shorter path
        const auto ChooseByBlockerOrLength = [&] {
            if (infoA.blockedBy == infoB.blockedBy) {
                return infoA.length < infoB.length ? 1 : 2;
            }
            const auto GetRadius = [](CEntity* e) { return e->GetModelInfo()->GetColModel()->GetBoundRadius(); };
            return GetRadius(infoA.blockedBy) < GetRadius(infoB.blockedBy) ? 1 : 2; // Go around the smaller blocker
        };

        if (!infoA.isClear) {
            if (infoB.isClear) {
                chosenPath = 2;
            } else if (infoA.blockedAt == 1) {
                chosenPath = infoB.blockedAt < 2 ? ChooseByBlockerOrLength() : 2;
            } else if (infoA.blockedAt > 1 && infoB.blockedAt == 1) {
                chosenPath = 1;
            } else {
                chosenPath = ChooseByBlockerOrLength();
            }
        } else if (!infoB.isClear) {
            chosenPath = 1;
        } else {
            chosenPath = infoA.length < infoB.length ? 1 : 2;
        }
    } else {
        chosenPath = mode != 1 ? 2 : 1;
    }

    // Add the points of the chosen path (without the first and last one) to the route
    {
        const auto& path      = chosenPath == 1 ? pathA : pathB;
        const auto  numPoints = chosenPath == 1 ? numA : numB;
        for (auto k = 1u; k + 1 < numPoints; k++) {
            route.AddUnlessFull(path[k]);
        }
    }
    route.AddUnlessFull(to);

    // Remove the first point if it's on the same side of the box as the start
    if (route.GetSize() > 1) {
        const auto GetSide = [&](const CVector& point) {
            CVector sideCorners[4];
            ComputeEntityBoundingBoxCornersUncached(point.z, entity, sideCorners);

            CVector center;
            ComputeEntityBoundingBoxCentreUncached(point.z, sideCorners, center);

            CVector sidePlanes[4];
            float   sideDots[4];
            ComputeEntityBoundingBoxSegmentPlanesUncached(sideCorners, center, sidePlanes, sideDots);

            return ComputeEntityHitSide(point, sidePlanes, sideDots);
        };
        const auto startSide = GetSide(from);
        const auto firstSide = GetSide(route.m_Entries[1]);
        if (startSide == firstSide) {
            for (auto i = 0u; i < route.GetSize() - 1; i++) {
                route.m_Entries[i] = route.m_Entries[i + 1];
            }
            route.m_NumEntries--;
        }
    }
    route.m_NumEntries--; // The last point (target) isn't part of the route

    return chosenPath;
}

// 0x5F1890
bool CPedGeometryAnalyser::ComputeRouteRoundSphere(const CPed& ped, const CColSphere& sphereIn, const CVector& a3, const CVector& a4, CVector& a5, CVector& a6) {
    CColSphere sphere = sphereIn; // The `CColSphere` functions aren't const, so work on a copy

    const auto& pedPos = ped.GetPosition();

    a5 = a4;
    if (sphere.IntersectPoint(a4)) {
        CVector dir = a4 - a3;
        dir.Normalise();

        CVector p1, p2;
        if (sphere.IntersectRay(pedPos, dir, p1, p2)) {
            a5 = p2;
        }
    }

    CVector dir = a5 - pedPos;
    dir.Normalise();

    CVector p1, p2;
    if (!sphere.IntersectRay(a5, dir, p1, p2)) {
        a6 = a5;
        return false;
    }

    // Is `a5` closer to the ped than the first intersection point?
    const float distA5Sq = (a5.x - pedPos.x) * (a5.x - pedPos.x) + (a5.y - pedPos.y) * (a5.y - pedPos.y) + (a5.z - pedPos.z) * (a5.z - pedPos.z);
    const float distP1Sq = (p1.x - pedPos.x) * (p1.x - pedPos.x) + (p1.y - pedPos.y) * (p1.y - pedPos.y) + (p1.z - pedPos.z) * (p1.z - pedPos.z);
    if (distA5Sq < distP1Sq) {
        a6 = a5;
        return false;
    }

    if (sphere.IntersectRay(pedPos, dir, p1, p2)) {
        // Find the point on the ray that is the closest to the center of the sphere
        const float t = (sphere.m_vecCenter.x - pedPos.x) * dir.x + (sphere.m_vecCenter.y - pedPos.y) * dir.y + (sphere.m_vecCenter.z - pedPos.z) * dir.z;
        const CVector closestOnRay = {
            dir.x * t + pedPos.x,
            dir.y * t + pedPos.y,
            dir.z * t + pedPos.z,
        };

        // Then offset it to the surface of the sphere
        CVector fromCenter = closestOnRay - sphere.m_vecCenter;
        fromCenter.Normalise();
        a6 = fromCenter * sphere.m_fRadius + sphere.m_vecCenter;
    }
    return true;
}

// 0x5F5A30
bool CPedGeometryAnalyser::GetIsLineOfSightClear(const CPed& ped, const CVector& target, CEntity& entity, float& outDist) {
    CVector start = ped.GetPosition();
    CVector end   = target;

    CColSphere sphere;
    ComputeEntityBoundingSphere(ped, entity, sphere);

    CVector dir = end - start;
    dir.NormaliseAndMag();

    CVector hit1, hit2;
    if (!sphere.IntersectRay(start, dir, hit1, hit2)) {
        return true;
    }

    const float zPos = ped.GetPosition().z;

    CVector corners[4];
    ComputeEntityBoundingBoxCornersUncached(zPos, entity, corners);

    CVector planes[4];
    float   planesDots[4];
    ComputeEntityBoundingBoxPlanesUncached(zPos, corners, &planes, planesDots);

    outDist = 0.f;

    const float margin = s_BoundingBoxMargin;

    // Clip the line with the planes
    for (auto i = 0; i < 4; i++) {
        const auto& n = planes[i];

        const float distStart = start.x * n.x + start.y * n.y + start.z * n.z + planesDots[i];
        const float distEnd   = end.x * n.x + end.y * n.y + end.z * n.z + planesDots[i];

        const auto Classify = [margin](float dist) { // 1 = in front of the plane, -1 = behind it, 0 = near
            if (dist > margin) {
                return 1;
            }
            return dist < -margin ? -1 : 0;
        };
        const auto classStart = Classify(distStart);
        const auto classEnd   = Classify(distEnd);

        const float dirDotN = dir.x * n.x + dir.y * n.y + dir.z * n.z;

        bool clipEnd = false;
        if (classStart < 0) {
            clipEnd = true;
        } else {
            if (classEnd >= 0) {
                return true; // Both points are outside (or near) of the plane
            }
            if (classStart >= 1) { // Start is outside
                if (dirDotN > 0.001f) { // 0x858CDC
                    const float t = (-1.f / dirDotN) * distStart;
                    start = CVector{ dir.x * t + start.x, dir.y * t + start.y, start.z + dir.z * t };
                }
            } else {
                clipEnd = true;
            }
        }
        if (clipEnd && classEnd > 0) { // End is outside
            if (dirDotN > 0.001f) {
                const float t = (-1.f / dirDotN) * distStart;
                end = CVector{ dir.x * t + start.x, dir.y * t + start.y, dir.z * t + start.z };
            }
        }
    }

    outDist = (end - start).Magnitude();
    return false;
}

// 0x5F2F00
bool CPedGeometryAnalyser::GetIsLineOfSightClear(const CVector& start, const CVector& end, CEntity& entity) {
    const CColLine line{ start, end };
    auto* const    colModel = entity.GetModelInfo()->GetColModel();
    return !CCollision::TestLineOfSight(line, entity.GetMatrix(), *colModel, false, false);
}

// 0x5F3590
CPed* CPedGeometryAnalyser::GetNearestPed(const CVector& point) {
    CPed* nearest{};
    float closestDistSq = FLT_MAX;
    for (auto i = GetPedPool()->GetSize(); i --> 0;) { // Original iterates backwards
        auto* const ped = GetPedPool()->GetAt(i);
        if (!ped) {
            continue;
        }
        const auto& pedPos = ped->GetPosition();
        const float distSq = (point.y - pedPos.y) * (point.y - pedPos.y)
                           + (point.x - pedPos.x) * (point.x - pedPos.x)
                           + (point.z - pedPos.z) * (point.z - pedPos.z);
        if (distSq < closestDistSq) {
            closestDistSq = distSq;
            nearest       = ped;
        }
    }
    return nearest;
}

// 0x5F3970
bool CPedGeometryAnalyser::IsEntityBlockingTarget(CEntity* entity, const CVector& point, float distance) {
    const auto& entityPos = entity->GetPosition();

    const float dx = entityPos.x - point.x;
    const float dy = entityPos.y - point.y;
    if (std::abs(entityPos.z - point.z) > 3.f) { // 0x858B3C
        return false;
    }

    const float radius = entity->GetModelInfo()->GetColModel()->GetBoundRadius();
    // BUG: Compares a squared value to a non-squared one
    if (distance * distance + radius * radius < std::sqrt(dy * dy + dx * dx)) {
        return false;
    }

    const float margin = distance * 0.5f; // 0x858B8C

    CVector corners[4];
    ComputeEntityBoundingBoxCornersUncached(entityPos.z, *entity, corners);

    CVector planes[4];
    float   planesDots[4];
    ComputeEntityBoundingBoxPlanesUncached(entityPos.z, corners, &planes, planesDots);

    // Is the point inside the (inflated) box?
    for (auto i = 0; i < 4; i++) {
        const float dist = planes[i].x * point.x + planes[i].z * point.z + point.y * planes[i].y + planesDots[i] + margin;
        if (dist > 0.f) {
            return false;
        }
    }
    return true;
}

// 0x5F1CB0
bool CPedGeometryAnalyser::IsInAir(const CPed& ped) {
    if (ped.bInVehicle) {
        return false;
    }

    auto& taskMgr = ped.GetTaskManager();
    if (taskMgr.GetActiveTask()) {
        auto* const intel = ped.GetIntelligence();
        if (intel->GetTaskSwim()
            || intel->GetTaskJetPack()
            || taskMgr.GetSimplestActiveTask()->GetTaskType() == TASK_SIMPLE_CLIMB
        ) {
            return false;
        }
    }

    bool isJumping{};
    if (const auto activeTask = taskMgr.GetActiveTask()) {
        isJumping = activeTask->GetTaskType() == TASK_COMPLEX_JUMP;
    }

    const CVector pos = ped.GetPosition();

    // Is there something below the ped?
    CColPoint colPoint;
    CEntity*  hitEntity{};
    bool      isOnGround = CWorld::ProcessVerticalLine(pos, pos.z - 1.5f, colPoint, hitEntity, true, true, false, true, false, false, nullptr); // 0x86C6A0
    if (!isOnGround && !isJumping) {
        isOnGround = CWorld::TestSphereAgainstWorld({ pos.x, pos.y, pos.z - 1.f }, 0.15f, const_cast<CPed*>(&ped), true, false, false, false, false, false) != nullptr;
    }
    return !isOnGround;
}

// 0x5F2F70
CPedGeometryAnalyser::WanderPathClearness CPedGeometryAnalyser::IsWanderPathClear(const CVector& from, const CVector& to, float maxHeightChange, int32 maxSamples) {
    if (std::abs(from.z - to.z) > maxHeightChange) {
        return WanderPathClearness::BLOCKED_HEIGHT;
    }

    // Check for buildings on the way (using the lowest Z coord)
    const float minZ = from.z < to.z ? from.z : to.z;
    if (!CWorld::GetIsLineOfSightClear({ from.x, from.y, minZ }, { to.x, to.y, minZ }, true, false, false, false, false, false, false)) {
        return WanderPathClearness::BLOCKED_LOS;
    }

    CVector dir = to - from;
    const auto numSamples = std::min((int32)std::floor(dir.Magnitude()), maxSamples);
    if (numSamples == 0) {
        return WanderPathClearness::CLEAR;
    }
    dir.Normalise();

    // Check for water
    for (auto i = 1; i < numSamples; i++) {
        const CVector p = from + dir * (float)i;

        float waterZ;
        if (CWaterLevel::GetWaterLevel(p.x, p.y, p.z, waterZ, 0, nullptr)) {
            // If there's no ground above the water, then it's a water
            const float maxZ = from.z <= to.z ? to.z : from.z;

            CColPoint colPoint;
            CEntity*  hitEntity{};
            if (!CWorld::ProcessVerticalLine({ p.x, p.y, waterZ }, maxZ, colPoint, hitEntity, true, false, false, false, false)) {
                return WanderPathClearness::BLOCKED_WATER;
            }
        }
    }

    // Check for sudden drops
    CColPoint colPoint;
    CEntity*  hitEntity{};
    if (!CWorld::ProcessVerticalLine(from, from.z - 5.f, colPoint, hitEntity, true, false, false, false, false, false, nullptr)) { // 0x858C80
        return WanderPathClearness::BLOCKED_SHARP_DROP;
    }

    float prevZ = colPoint.m_vecPoint.z + 0.5f;
    for (auto i = 1; i < numSamples; i++) {
        const CVector p = { dir.x * (float)i + from.x, dir.y * (float)i + from.y, prevZ };
        if (!CWorld::ProcessVerticalLine(p, prevZ - 2.f, colPoint, hitEntity, true, false, false, false, false, false, nullptr)) { // 0x858CA0
            return WanderPathClearness::BLOCKED_SHARP_DROP;
        }
        if (std::abs(colPoint.m_vecPoint.z - prevZ) > 1.f) { // 0x858624
            return WanderPathClearness::BLOCKED_SHARP_DROP;
        }
        prevZ = colPoint.m_vecPoint.z + 0.5f;
    }

    return WanderPathClearness::CLEAR;
}

// 0x5F3880
bool CPedGeometryAnalyser::LiesInsideBoundingBox(const CPed& ped, const CVector& posn, CEntity& entity) {
    const auto& entityPos = entity.GetPosition();

    // Quick check using the bounding sphere
    const float radius = entity.GetModelInfo()->GetColModel()->GetBoundRadius();
    const float distSq = (posn.x - entityPos.x) * (posn.x - entityPos.x)
                       + (posn.y - entityPos.y) * (posn.y - entityPos.y)
                       + (posn.z - entityPos.z) * (posn.z - entityPos.z);
    if (radius * radius <= distSq) {
        return false;
    }

    const float zPos = ped.GetPosition().z;

    CVector corners[4];
    ComputeEntityBoundingBoxCornersUncached(zPos, entity, corners);

    CVector planes[4];
    float   planesDots[4];
    ComputeEntityBoundingBoxPlanesUncached(zPos, corners, &planes, planesDots);

    // BUG(?): Returns true if the point is behind ANY plane (not all of them)
    for (auto i = 0; i < 4; i++) {
        const float dist = planes[i].x * posn.x + planes[i].z * posn.z + posn.y * planes[i].y + planesDots[i];
        if (dist < 0.f) {
            return true;
        }
    }
    return false;
}

// 0x41B7C0
void* CPointRoute::operator new(uint32 size) {
    return GetPointRoutePool()->New();
}

// 0x41B7D0
void CPointRoute::operator delete(void* ptr, size_t sz) {
    GetPointRoutePool()->Delete(reinterpret_cast<CPointRoute*>(ptr));
}

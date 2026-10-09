/*
    Plugin-SDK file
    Authors: GTA Community. See more here
    https://github.com/DK22Pac/plugin-sdk
    Do not delete this comment block. Respect others' work!
*/


#include "StdInc.h"

#include <numbers>
#include <bit>

#include "Collision.h"
#include "ColHelpers.h"
#include "PedModelInfo.h"
#include "TaskSimpleHoldEntity.h"

#include "TaskComplexEnterCarAsDriver.h"
#include "TaskComplexEnterCarAsPassenger.h"

#define NOTSA_VANILLA_COLLISIONS // TODO: move to config.h?

using Shape = CCollision::DebugSettings::ShapeShapeCollision::Shape;

namespace {
// The original keeps these expressions on the x87 stack (extended precision, fixed term order)
// => use `double` intermediates and round to float only where the original stores to a float.

//! x*x + y*y + z*z (0x406DA0 CVector::SquaredMagnitude, result stays unrounded on the x87 stack)
double SquaredMagnitudeD(const CVector& v) {
    return (double)v.x * v.x + (double)v.y * v.y + (double)v.z * v.z;
}

//! 0x59C790 `CMatrix * CVector` (3x3 part only). Unrounded accumulation in this exact term order, rounded when stored
CVector TransformVectorOG(const CMatrix& m, const CVector& v) {
    return {
        (float)((double)m.GetUp().x * v.z + (double)m.GetForward().x * v.y + (double)m.GetRight().x * v.x),
        (float)((double)m.GetUp().y * v.z + (double)m.GetRight().y * v.x + (double)m.GetForward().y * v.y),
        (float)((double)m.GetUp().z * v.z + (double)m.GetRight().z * v.x + (double)m.GetForward().z * v.y),
    };
}

//! 0x59C890 `CMatrix * CVector` (+ translation)
CVector TransformPointOG(const CMatrix& m, const CVector& v) {
    return {
        (float)(((double)m.GetUp().x * v.z + (double)m.GetForward().x * v.y + (double)m.GetRight().x * v.x) + m.GetPosition().x),
        (float)(((double)m.GetUp().y * v.z + (double)m.GetRight().y * v.x + (double)m.GetForward().y * v.y) + m.GetPosition().y),
        (float)(((double)m.GetUp().z * v.z + (double)m.GetRight().z * v.x + (double)m.GetForward().z * v.y) + m.GetPosition().z),
    };
}

//! Inverse of the rotation + translation of `m` applied to `p` (0x417BF0 inlines this): `(p - pos) . axis`, unrounded accumulation (terms: x, z, y)
CVector InverseTransformPointOG(const CMatrix& m, const CVector& p) {
    const double dx = (double)p.x - m.GetPosition().x, dy = (double)p.y - m.GetPosition().y, dz = (double)p.z - m.GetPosition().z;
    const auto Dot = [&](const CVector& axis) { return (float)(dx * axis.x + dz * axis.z + dy * axis.y); };
    return { Dot(m.GetRight()), Dot(m.GetForward()), Dot(m.GetUp()) };
}

//! 0x4119D0 `operator/(CVector, float)` => multiplies by a (float rounded) reciprocal, it is NOT a component-wise division
CVector DivideByReciprocal(const CVector& v, float divisor) {
    const float inv = 1.f / divisor;
    return { v.x * inv, v.y * inv, v.z * inv };
}
}

/*!
* @addr 0x416260
*/
void CCollision::Init() {
    ZoneScoped;

    ms_colModelCache.Init(50);
    ms_collisionInMemory = 0;
    CColStore::Initialise();
}

// 0x4162E0
void CCollision::Shutdown() {
    for (auto i = ms_colModelCache.usedListTail.prev; i != &ms_colModelCache.usedListHead; i = i->prev) { // Original: starts at the LEAST recently used link
        if (i->data) {
            // Original (0x4162E0): calls the *member* function, the links must NOT be moved to the free list while iterating
            i->data->RemoveTrianglePlanes();
        }
    }
    ms_colModelCache.Shutdown();
    CColStore::Shutdown();
}

// 0x411E20
void CCollision::Update() {
    // empty
}

// 0x411E30
void CCollision::SortOutCollisionAfterLoad() {
    CColStore::LoadCollision(TheCamera.m_mCameraMatrix.GetPosition(), false);
    CStreaming::LoadAllRequestedModels(false);
}


// 0x416330
void CCollision::CalculateTrianglePlanes(CCollisionData* colData) {
    ZoneScoped;

    if (!colData->m_nNumTriangles) { // No triangles => no planes to calculate
        return;
    }

    if (colData->m_pTrianglePlanes) { // Planes already calculated?
        ms_colModelCache.Insert(*colData->GetLinkPtr()); // Re-insert link at front
    } else {
        auto l = ms_colModelCache.Insert(colData);
        if (!l) { // No more free space?
            // Remove least-recently used item
            const auto llr = ms_colModelCache.GetTail();
            llr->data->RemoveTrianglePlanes();
            ms_colModelCache.Remove(llr);

            // This should succeed now
            VERIFY(l = ms_colModelCache.Insert(colData));
        }
        colData->CalculateTrianglePlanes();
        colData->SetLinkPtr(l);
    }
}

// 0x416400
void CCollision::RemoveTrianglePlanes(CCollisionData* colData) {
    ZoneScoped;

    if (!colData->m_pTrianglePlanes) {
        return;
    }

    const auto l = colData->GetLinkPtr();
    ms_colModelCache.Remove(l);
    colData->RemoveTrianglePlanes();
}

// 0x411E70
bool CCollision::TestSphereSphere(CColSphere const& sphere1, CColSphere const& sphere2) { // NOTE: it's a plain __cdecl (`ret`)
    ZoneScoped;

    // Original (0x411E70): everything stays in extended precision, the compare is STRICT (`FCOMPP` + `test ah, 0x41` => false on `<=` and NaN)
    const double dx = (double)sphere1.m_vecCenter.x - sphere2.m_vecCenter.x;
    const double dy = (double)sphere1.m_vecCenter.y - sphere2.m_vecCenter.y;
    const double dz = (double)sphere1.m_vecCenter.z - sphere2.m_vecCenter.z;
    const double sumR = (double)sphere1.m_fRadius + sphere2.m_fRadius;
    return sumR * sumR > dz * dz + dx * dx + dy * dy;
}

// 0x411EC0
void CalculateColPointInsideBox(CBox const& box, CVector const& point, CColPoint& colPoint) {
    // Original (0x411EC0): `(max + min) * 0.5`: the X sum stays unrounded until the product is spilled, the Y sum + product stay
    // on the x87 stack, the Z sum is spilled to a float (before the product). Then `point - center` is spilled to floats.
    const float  cx = (float)(((double)box.m_vecMax.x + box.m_vecMin.x) * 0.5);
    const double cy = ((double)box.m_vecMax.y + box.m_vecMin.y) * 0.5;
    const double cz = (double)(float)((double)box.m_vecMax.z + box.m_vecMin.z) * 0.5;
    const CVector pointToCenter{ (float)((double)point.x - cx), (float)((double)point.y - cy), (float)((double)point.z - cz) };

    // Distance of the point to the face on each axis (the face on the side of the point's offset from the center).
    // Original: the X distance stays on the x87 stack (unrounded), Y and Z are spilled to floats.
    const auto DistOnAxis = [&](float toCenter, float p, float mn, float mx) -> double {
        return !(toCenter > 0.f) ? (double)p - mn : (double)mx - p; // `FCOMP` + `test ah, 0x41` + `jne` => <= 0 or NaN
    };
    const double xDist = DistOnAxis(pointToCenter.x, point.x, box.m_vecMin.x, box.m_vecMax.x);
    const float  yDist = (float)DistOnAxis(pointToCenter.y, point.y, box.m_vecMin.y, box.m_vecMax.y);
    const float  zDist = (float)DistOnAxis(pointToCenter.z, point.z, box.m_vecMin.z, box.m_vecMax.z);

    const auto CalcNormal = [](float a) {
        return a > 0.f ? 1.f : -1.f;
    };

    // Original only writes the point, the normal and the depth (all other fields are left untouched)
    colPoint.m_vecPoint = point;

    // Pick the axis with the SMALLEST distance to a face (shallowest exit)
    if (xDist < yDist && xDist < zDist) {
        colPoint.m_vecNormal = CVector{ CalcNormal(pointToCenter.x), 0.f, 0.f };
        colPoint.m_fDepth    = (float)xDist;
    } else if ((double)yDist < xDist && yDist < zDist) {
        colPoint.m_vecNormal = CVector{ 0.f, CalcNormal(pointToCenter.y), 0.f };
        colPoint.m_fDepth    = yDist;
    } else {
        colPoint.m_vecNormal = CVector{ 0.f, 0.f, CalcNormal(pointToCenter.z) };
        colPoint.m_fDepth    = zDist;
    }
}

/*!
* @addr 0x4120C0
* @brief Tests if the \a bb is fully inside \a sphere
*/
bool CCollision::TestSphereBox(CSphere const& sphere, CBox const& box) {
    ZoneScoped;

    // Original: the sums stay unrounded on the x87 stack. NaN => the axis passes (`FCOMP` + `test ah, 5` / `0x41`)
    for (auto i = 0u; i < 3u; i++) {
        if ((double)sphere.m_vecCenter[i] + sphere.m_fRadius < box.m_vecMin[i] ||
            (double)sphere.m_vecCenter[i] - sphere.m_fRadius > box.m_vecMax[i]
        ) {
            return false;
        }
    }
    return true;
}

/*!
* @addr 0x412130
*/
bool CCollision::ProcessSphereBox(CColSphere const& sph, CColBox const& box, CColPoint & colp, float& minDistSq) {
    ZoneScoped;

	// GTA's code is too complicated, uses a huge 3x3x3 if statement
	// we can simplify the structure a lot
    // Some of the original code, to give you an idea:
    /*
    if (sphere.m_vecCenter.x + sphere.m_fRadius < bb.m_vecMin.x)
        return false;

    if (sphere.m_vecCenter.x + sphere.m_fRadius > bb.m_vecMax.x)
        return false;

    CVector colPos{};

    if (sphere.m_vecCenter.x >= bb.m_vecMin.x) {
        if (sphere.m_vecCenter.x <= bb.m_vecMax.x) {
            if (sphere.m_vecCenter.y + sphere.m_fRadius > bb.m_vecMin.y)
                return false;

            if (sphere.m_vecCenter.y - sphere.m_fRadius > bb.m_vecMax.y)
                return false;

            if (sphere.m_vecCenter.y >= bb.m_vecMin.y) {
                if (sphere.m_vecCenter.y <= bb.m_vecMax.y) {
                    if (sphere.m_vecCenter.z + sphere.m_fRadius > bb.m_vecMin.z)
                        return false;

                    if (sphere.m_vecCenter.z - sphere.m_fRadius > bb.m_vecMax.z)
                        return false;

                    if (sphere.m_vecCenter.z >= bb.m_vecMin.z) {
                        if (sphere.m_vecCenter.z <= bb.m_vecMax.z) {
                            CColPoint boxCP{};
                            CalculateColPointInsideBox(bb, sphere.m_vecCenter, boxCP);

                            diskColPoint.m_vecPoint = boxCP.m_vecPoint - boxCP.m_vecNormal * sphere.m_fRadius;
                            diskColPoint.m_fDepth = boxCP.m_fDepth;

                            diskColPoint.m_nLightingA = sphere.ligthing;
                            diskColPoint.m_nSurfaceTypeA = sphere.m_nMaterial;

                            diskColPoint.m_nLightingB = bb.ligthing;
                            diskColPoint.m_nSurfaceTypeB = bb.m_nMaterial;

                            maxTouchDistance = 0.f;

                            return true;
                        } else {
                            colPos.x = sphere.m_vecCenter.x;
                            colPos.y = sphere.m_vecCenter.y;
                            colPos.z = bb.m_vecMax.z;
                        }
                    } else {
                        colPos.x = sphere.m_vecCenter.x;
                        colPos.y = sphere.m_vecCenter.y;
                        colPos.z = bb.m_vecMin.z;
                    }
                } else {
                    if (sphere.m_vecCenter.z + sphere.m_fRadius > bb.m_vecMin.z)
                        return false;

                    if (sphere.m_vecCenter.z - sphere.m_fRadius > bb.m_vecMax.z)
                        return false;

                    if (sphere.m_vecCenter.z >= bb.m_vecMin.z) {
                        if (sphere.m_vecCenter.z <= bb.m_vecMax.z) {
                            colPos.x = sphere.m_vecCenter.x;
                            colPos.y = bb.m_vecMax.y;

                            if (sphere.m_vecCenter.z > bb.m_vecMax.z)
                                colPos.y = bb.m_vecMax.y;

                            if (sphere.m_vecCenter.z <= bb.m_vecMax.z) {
                                colPos.z = sphere.m_vecCenter.z;
                                colPos.y = bb.m_vecMax.y;
                            } else {
                                colPos.z = bb.m_vecMax.z;
                            }
                        } else {

                        }
                    } else {
                        colPos.y = bb.m_vecMax.y;
                        colPos.x = sphere.m_vecCenter.x;
                        colPos.z = bb.m_vecMin.z;
                    }
                }
            } else {
                if (sphere.m_vecCenter.z + sphere.m_fRadius > bb.m_vecMin.z)
                    return false;

                if (sphere.m_vecCenter.z - sphere.m_fRadius > bb.m_vecMax.z)
                    return false;

                if (sphere.m_vecCenter.z)
            }
        }
    }
    */

	// First make sure we have a collision at all
    if (!TestSphereBox(sph, box))
        return false;

	// Now find out where the sphere center lies in relation to all the sides
    // (NaN => INSIDE: neither `c < min` nor `c > max` hold)
    enum class ClosestCorner {
        INSIDE,
        MIN,    
        MAX,
    };
    using enum ClosestCorner;

    ClosestCorner axies[3];
    for (auto i = 0; i < 3; i++) {
        axies[i] = sph.m_vecCenter[i] < box.m_vecMin[i] ? MIN :
                   sph.m_vecCenter[i] > box.m_vecMax[i] ? MAX :
                   INSIDE;
    }

    // Original surface copy: 3 bytes (material, piece, lighting) of each shape's surface
    const auto CopySurfaces = [&] {
        colp.m_nSurfaceTypeA = sph.m_Surface.m_nMaterial;
        colp.m_nPieceTypeA   = sph.m_Surface.m_nPiece;
        colp.m_nLightingA    = sph.m_Surface.m_nLighting;

        colp.m_nSurfaceTypeB = box.m_Surface.m_nMaterial;
        colp.m_nPieceTypeB   = box.m_Surface.m_nPiece;
        colp.m_nLightingB    = box.m_Surface.m_nLighting;
    };

	if(axies[0] == INSIDE && axies[1] == INSIDE && axies[2] == INSIDE) { // Sphere center is inside the bb
        // Original (0x412130): writes directly into `colp`: face-normal from CalculateColPointInsideBox, depth includes radius
        CalculateColPointInsideBox(box, sph.m_vecCenter, colp);

        colp.m_fDepth        = colp.m_fDepth + sph.m_fRadius;
        colp.m_vecPoint      = colp.m_vecPoint - colp.m_vecNormal * sph.m_fRadius;
        CopySurfaces();

        minDistSq            = 0.f; // Original sets it to 0 for inside hits
        return true;
    } else { // Sphere centre is outside on at least one axis

        // Position of closest corner:
        const CVector p{
            axies[0] == MIN ? box.m_vecMin.x : axies[0] == MAX ? box.m_vecMax.x
                                                               : sph.m_vecCenter.x,

            axies[1] == MIN ? box.m_vecMin.y : axies[1] == MAX ? box.m_vecMax.y
                                                               : sph.m_vecCenter.y,

            axies[2] == MIN ? box.m_vecMin.z : axies[2] == MAX ? box.m_vecMax.z
                                                               : sph.m_vecCenter.z,
        };

        const auto dir    = sph.m_vecCenter - p;
        const auto distSqD = SquaredMagnitudeD(dir); // Unrounded on the x87 stack for the compare below
        const auto distSq = (float)distSqD;          // ... but spilled to a float for everything else
        if (distSqD < minDistSq) { // NaN => false
            const double distD = std::sqrt((double)distSq); // Original: `fsqrt` of the spilled float, compared before it's spilled itself
            if (!(distD <= sph.m_fRadius)) { // Original: `dist > radius || NaN` => false (equal passes!)
                return false;
            }
            const auto dist = (float)distD;

            colp.m_vecPoint      = p;
            colp.m_vecNormal     = DivideByReciprocal(dir, dist);
            CopySurfaces();
            colp.m_fDepth        = sph.m_fRadius - dist;

            minDistSq            = distSq;

            return true;
        }
    }
    return false;
}

/*!
* Check if point is within the triangle
* Unused function - Most likely inlined
* @addr 0x412700
*/
bool __stdcall CCollision::PointInTriangle(CVector const& point, CVector const* triPoints) {
    ZoneScoped;

    // Original (0x412700): the x87 stack keeps v1 and the two dot products with `point - tri[0]` unrounded, v2/p and the
    // v1/v2 products are spilled to floats. Term order is fixed => `double` intermediates, rounding to float at the spills.
    const CVector& p0 = triPoints[0];

    // v1 = tri[1] - tri[0] (extended)
    const double v1x = (double)triPoints[1].x - p0.x, v1y = (double)triPoints[1].y - p0.y, v1z = (double)triPoints[1].z - p0.z;
    // v2 = tri[2] - tri[0] (float spill)
    const float  v2x = triPoints[2].x - p0.x, v2y = triPoints[2].y - p0.y, v2z = triPoints[2].z - p0.z;

    const float v1_magSq  = (float)(v1z * v1z + v1x * v1x + v1y * v1y);
    const float v2_magSq  = (float)((double)v2z * v2z + (double)v2y * v2y + (double)v2x * v2x);
    const float v1_dot_v2 = (float)(v2z * v1z + v2y * v1y + v2x * v1x);

    // p = point - tri[0] (float spill, except for `p.z` which is used unrounded for v1 dot p)
    const float  px = point.x - p0.x, py = point.y - p0.y, pz = point.z - p0.z;
    const double pzExt = (double)point.z - p0.z;

    const double v1_dot_p = pzExt * v1z + (double)px * v1x + (double)py * v1y;
    const double v2_dot_p = (double)pz * v2z + (double)py * v2y + (double)px * v2x;

    const double a = v1_dot_p * v2_magSq - v2_dot_p * v1_dot_v2;
    if (a < 0.0) { // NaN passes
        return false;
    }
    const double b = v2_dot_p * v1_magSq - v1_dot_p * v1_dot_v2;
    if (b < 0.0) { // NaN passes
        return false;
    }
    const double c = (double)v2_magSq * v1_magSq - (double)v1_dot_v2 * v1_dot_v2;
    return !(c < (double)(float)a + b); // `a` is spilled to a float for the sum
}

namespace {
//! 0x412850 + 0x417610 share the evaluation (but for the dot product's terms), the result stays unrounded in the exe until the caller stores it
double DistToLineSqrD(CVector const& ln0, CVector const& ln1, CVector const& pt, bool unroundedZ) {
    // Make line end (l) and pt (pl_ip) relative to ln0 (by this ln0 becomes the space origin)
    // Original (0x412850): `l` stays unrounded on the x87 stack (extended), `p` is spilled to floats.
    // Term orders are fixed by the asm (see below).
    const double lx = (double)ln1.x - ln0.x, ly = (double)ln1.y - ln0.y, lz = (double)ln1.z - ln0.z;
    const float  px = pt.x - ln0.x, py = pt.y - ln0.y, pz = pt.z - ln0.z;

    //        * P
    //      / |
    //   c /  | a
    //    /   |
    // O *----+--------* L
    //     pl  IP
    //
    // O    - Origin (line start)
    // L    - Line end
    // P    - Point
    // IP   - Intersection pt
    // b, c - Triangle sides
    // a    - The distance we want to find out :D

    // Dot product `p . l`. 0x412850 (DistToLineSqr): terms x, z, y of the SPILLED p (0x41288B); 0x417610 (DistToLine): `fst` keeps the unrounded p.z on the FPU stack and
    // multiplies it first (0x41764B): terms z, x, y. The sum is rounded at the current precision control either way.
    const double pzE = (double)pt.z - ln0.z;
    const double plD = unroundedZ ? (pzE * lz + (double)px * lx) + (double)py * ly : ((double)px * lx + (double)pz * lz) + (double)py * ly;
    const float  pl  = (float)plD;                                   // ... is spilled to a float, but the 1st compare uses the unrounded value

    if (plD <= 0.0) { // Before origin (NaN => no)
        return (double)pz * pz + (double)py * py + (double)px * px; // Dist to origin
    }

    const double ll = lx * lx + lz * lz + ly * ly; // Line mag. sq.

    if (pl >= ll) { // After end (original 0x4128F0: `fcomp; test ah, 1; jne middle` - C0 is also set for unordered, so NaN goes to the MIDDLE branch)
        const double ex = (double)pt.x - ln1.x, ey = (double)pt.y - ln1.y, ez = (double)pt.z - ln1.z;
        return ez * ez + ey * ey + ex * ex; // Dist to end
    }

    // Simple Pythagorean here, we gotta find `a^2`
    const double cSq = (double)pz * pz + (double)py * py + (double)px * px;

    // Clever trick to divide by |l| without taking it's sqrt
    // We have to do this, because `pl` is multiplied by |l|
    // (Result of the dot product)
    const double bSq = (double)pl * pl / ll;

    const double aSq = cSq - bSq;
    return aSq <= 0.0 ? 0.0 : aSq; // Original: `<= 0` => +0.0 (NaN is returned as is)
}
}

/*!
* @addr 0x412850
*
* @param ln0 Origin of line seg.
* @param ln1 End of line seg.
* @param pt  The point
* 
* @returns Sq. dist. from `pt` to point closest to `pt` on the line segment (ln0, ln1) 
*/
float CCollision::DistToLineSqr(CVector const& ln0, CVector const& ln1, CVector const& pt) {
    ZoneScoped;

    return (float)DistToLineSqrD(ln0, ln1, pt, false);
}

// 0x417610
float CCollision::DistToLine(const CVector& lineStart, const CVector& lineEnd, const CVector& point) {
    ZoneScoped;

    // Original (0x417610): the same evaluation as `DistToLineSqr` except for the dot product's terms (see there), and the squared distance is not rounded to a float before the sqrt
    return (float)std::sqrt(DistToLineSqrD(lineStart, lineEnd, point, true));
}

/*!
* @addr 0x412970
* @brief Similar to DistToLineSqr it always returns the distance to the projected intersection point.
*/
float CCollision::DistToMathematicalLine(CVector const* lineStart, CVector const* lineEnd, CVector const* point) {
    ZoneScoped;

    // Original (0x412970): `l` and `p.x` stay unrounded on the x87 stack, `p.y`/`p.z` are spilled to floats
    const double lx = (double)lineEnd->x - lineStart->x, ly = (double)lineEnd->y - lineStart->y, lz = (double)lineEnd->z - lineStart->z;
    const double px = (double)point->x - lineStart->x;
    const float  py = point->y - lineStart->y, pz = point->z - lineStart->z;

    // See `DistToLineSqr` for a nice illustration.
    // Simple Pythagorean here, we gotta find side `a`

    const float  dot   = (float)(px * lx + (double)pz * lz + (double)py * ly); // `p . l`
    const double pMagSq = (double)pz * pz + (double)py * py + px * px;
    const double lMagSq = lx * lx + lz * lz + ly * ly; // NOTE: the dot product is scaled by `|l|^2` (NOT `|p|^2`)

    const float aSq = (float)(pMagSq - (double)dot * dot / lMagSq);
    return aSq <= 0.f ? 0.f : std::sqrt(aSq); // Original: `<= 0` => 0, NaN goes into sqrt
}

/*!
* @addr 0x412A30
* @brief Same as DistToMathematicalLine but in 2D
*/
float CCollision::DistToMathematicalLine2D(float lineStartX, float lineStartY, float lineEndX, float lineEndY, float pointX, float pointY) {
    ZoneScoped;

    // Original (0x412A30): everything stays unrounded on the x87 stack
    const double px  = (double)pointX - lineStartX, py = (double)pointY - lineStartY;
    const double dot = px * lineEndX + py * lineEndY;
    const double distSq = py * py + px * px - dot * dot;
    return distSq <= 0.0 ? 0.f : (float)std::sqrt(distSq); // NaN goes into sqrt
}

/*!
* @addr 0x412A80
* @brief TODO
*/
float CCollision::DistAlongLine2D(float lineX, float lineY, float lineDirX, float lineDirY, float pointX, float pointY) {
    ZoneScoped;

    return (float)(((double)pointX - lineX) * lineDirX + ((double)pointY - lineY) * lineDirY); // Unrounded on the x87 stack
}


/*!
* Calculate point closest to `point` on line (l0 - l1)
*
* @notsa
*/
CVector CCollision::GetClosestPtOnLine(const CVector& l0, const CVector& l1, const CVector& point) {
    ZoneScoped;

    const auto lnMagSq = (l1 - l0).SquaredMagnitude();
	const auto dot = (point - l0).Dot(l1 - l0);
    if (dot <= 0.0f) {
		return l0;
    }
    if (dot >= lnMagSq) {
		return l1;
    }
    return lerp(l0, l1, dot / lnMagSq);
}


// 0x417FD0
// NOTE: The exe's argument order is (point, lineStart, lineEnd, closest) - the header names are misleading
void CCollision::ClosestPointOnLine(const CVector& point, const CVector& lineStart, const CVector& lineEnd, CVector& closest) {
    ZoneScoped;

    // a = point - lineStart (extended), l = lineEnd - lineStart (extended)
    const double ax = (double)point.x - lineStart.x, ay = (double)point.y - lineStart.y, az = (double)point.z - lineStart.z;
    const double lx = (double)lineEnd.x - lineStart.x, ly = (double)lineEnd.y - lineStart.y, lz = (double)lineEnd.z - lineStart.z;
    const float  lzF = (float)lz;

    const float len = (float)std::sqrt(lz * lzF + ly * ly + lx * lx);
    const float inv = (float)(1.0 / len);

    // Direction (normalized): `z` is spilled to a float, but used unrounded for the dot product
    const float  dirX = (float)(lx * inv), dirY = (float)(ly * inv);
    const float  dirZ = lzF * inv;
    const double dirZE = (double)lzF * inv;

    const float t = (float)(dirZE * az + (double)dirY * ay + (double)dirX * ax);

    if (t < 0.f) {
        closest = lineStart;
    } else if (t > len) {
        closest = lineEnd;
    } else {
        closest = CVector{
            (float)((double)dirX * t + lineStart.x),
            (float)((double)dirY * t + lineStart.y),
            (float)((double)(float)(dirZ * t) + lineStart.z)
        };
    }
}

// 0x415950
// Unused in the exe (only called by `ClosestPointOnPoly`).
// Overwrites `pt` with the one of the 3 points (`pts[0..2]`) that's closest to it
void CCollision::Closest3(CVector* pts, CVector* pt) {
    ZoneScoped;

    // Original: d0 is kept unrounded on the x87 stack (term order z, x, y), d1/d2 are spilled to floats (z, y, x)
    const double d0x = (double)pt->x - pts[0].x, d0y = (double)pt->y - pts[0].y, d0z = (double)pt->z - pts[0].z;
    const double d0  = d0z * d0z + d0x * d0x + d0y * d0y;

    const auto DistSq = [&](const CVector& p) {
        const double dx = (double)pt->x - p.x, dy = (double)pt->y - p.y, dz = (double)pt->z - p.z;
        return (float)(dz * dz + dy * dy + dx * dx);
    };
    const float d1 = DistSq(pts[1]);
    const float d2 = DistSq(pts[2]);

    if (d0 < d1) {
        *pt = d0 < d2 ? pts[0] : pts[2];
    } else {
        *pt = d1 < d2 ? pts[1] : pts[2];
    }
}

/*!
* Computes closest points C1 and C2 of S1(s)=P1+s*d1 and
* S2(t)=P2+t*d2, returning s and t.
*
* @param       s1p0 Origin of ln. seg 1
* @param       d1 Direction of ln. seg. 1 (Not normalized!)
* @param       a  Sq. mag. of ln. seg 1
* @param       s2p1 As s1p0 ln. seg. 2
* @param       d1 As d1 ln. seg. 2
* @param       e  As a for ln. seg. 2
* @param [out] s  Intersection parameter on ln. seg. 1
* @param [out] t  Intersection parameter on ln. seg. 2
* @param [out] c1 Point on ln. seg. 1
* @param [out] c2 Point on ln. seg. 2
* 
* @returns squared distance between between S1(s) and S2(t)
*
* Credit: Code from "Real-Time Collision Detection" by Christer Ericson, published by Morgan Kaufmann Publishers, © 2005 Elsevier Inc
*/
float CCollision::ClosestPtSegmentSegment(
    CVector p1, CVector d1, float a,
    CVector p2, CVector d2, float e,
    float& s, float& t,
    CVector& c1, CVector& c2
) {
    ZoneScoped;

    // Must be non-negative!
    assert(a >= 0.f);
    assert(e >= 0.f);

    constexpr auto EPSILON = std::numeric_limits<float>::epsilon();

    const auto r = p1 - p2;
    const auto f = d2.Dot(r);

    // Check if either or both segments degenerate into points
    if (a <= EPSILON && e <= EPSILON) {
        // Both segments degenerate into points
        s = t = 0.0f;
        c1 = p1;
        c2 = p2;
        return (c1 - c2).SquaredMagnitude();
    }

    if (a <= EPSILON) {
        // First segment degenerates into a point
        s = 0.0f;
        t = f / e;
        // s = 0 => t = (b*s + f) / e = f / e
        t = std::clamp(t, 0.0f, 1.0f);
    } else {
        const auto c = d1.Dot(r);
        if (e <= EPSILON) {
            // Second segment degenerates into a point
            t = 0.0f;
            s = std::clamp(-c / a, 0.0f, 1.0f);
            // t = 0 => s = (b*t - c) / a = -c / a
        } else {
            // The general nondegenerate case starts here
            const auto b = d1.Dot(d2);
            const auto denom = a * e - b * b;
            // Always nonnegative
            // If segments not parallel, compute closest point on L1 to L2 and
            // clamp to segment S1. Else pick arbitrary s (here 0)
            if (denom != 0.0f) {
                s = std::clamp((b * f - c * e) / denom, 0.0f, 1.0f);
            } else {
                s = 0.0f;
            }
            // Compute point on L2 closest to S1(s) using
            // t = Dot((P1 + D1*s) - P2,D2) / Dot(D2,D2) = (b*s + f) / e
            t = (b * s + f) / e;
            // If t in [0,1] done. Else clamp t, recompute s for the new value
            // of t using s = Dot((P2 + D2*t) - P1,D1) / Dot(D1,D1)= (t*b - c) / a
            // and clamp s to [0, 1]
            if (t < 0.0f) {
                t = 0.0f;
                s = std::clamp(-c / a, 0.0f, 1.0f);
            } else if (t > 1.0f) {
                t = 1.0f;
                s = std::clamp((b - c) / a, 0.0f, 1.0f);
            }
        }
    }

    c1 = p1 + d1 * s;
    c2 = p2 + d2 * t;
    return (c1 - c2).SquaredMagnitude();
}

// 0x415A40
/*!
* Algorithm of Dan Sunday (see https://web.archive.org/web/20210330143700/http://geomalgorithms.com/a07-_distance.html), but with the tweaks of the original:
* the values are rounded to float where the original stores them to the stack, the rest stays in the (extended precision) x87 registers => `double`.
*
* @param s1p0 Seg. 1 origin
* @param s2p0 Seg. 2 origin
* @param s2p1 Seg. 2 end
* @param u    Dir. of seg. 1 (Unnormalized!)
* @param a    Sq. mag. of seg 1
*
* @returns Sq. dist. of the closest points on the 2 line segments
*          NOTE: The original returns it in st0 (NOT rounded to float) => this returns a `double` (the callers might compare it unrounded)
*/
double ClosestSquaredDistanceBetweenFiniteLinesOG(
    const CVector& s1p0,
    const CVector& s2p0, const CVector& s2p1,
    const CVector& u, float a
) {
    const auto EPSILON = std::bit_cast<float>(0x3727C5ACu); // 0x858C14

    const CVector v{ s2p1.x - s2p0.x, s2p1.y - s2p0.y, s2p1.z - s2p0.z };
    const CVector w{ s1p0.x - s2p0.x, s1p0.y - s2p0.y, s1p0.z - s2p0.z };

    const float b = (float)(((double)v.z * u.z + (double)v.y * u.y) + (double)v.x * u.x);
    const float c = (float)(((double)v.x * v.x + (double)v.z * v.z) + (double)v.y * v.y);
    const float d = (float)(((double)w.z * u.z + (double)w.y * u.y) + (double)w.x * u.x);
    const float e = (float)(((double)w.x * v.x + (double)w.z * v.z) + (double)w.y * v.y);
    const float D = (float)((double)c * a - (double)b * b);

    float  sD = D, tD = D, tN;
    double sN; // Stays in the x87 stack (not rounded)
    if (D < EPSILON) { // The lines are almost parallel
        sD = 1.f;
        sN = 0.0;
        tN = e;
        tD = c;
    } else {
        sN = (double)e * b - (double)d * c;
        tN = (float)((double)e * a - (double)d * b);
        if (sN < 0.0) {
            sN = 0.0;
            tN = e;
            tD = c;
        } else if (sN > D) {
            sN = D;
            tN = (float)((double)e + b);
            tD = c;
        }
    }

    // Recompute `sN` (and `sD`) for the edge
    const auto Recompute = [&](double x) {
        if (x < 0.0) {
            sN = 0.0;
        } else if (!(x > a)) {
            sN = x;
            sD = a;
        } else {
            sN = sD;
        }
    };
    if (tN < 0.f) {
        tN = 0.f;
        Recompute(-(double)d);
    } else if (tN > tD) {
        tN = tD;
        Recompute((double)b - d);
    }

    const double sc = std::abs(sN) < EPSILON ? 0.0 : sN / sD;
    const double tc = std::abs((double)tN) < EPSILON ? 0.0 : (double)tN / tD;

    const float  tvx = (float)((double)v.x * tc);
    const float  tvy = (float)((double)v.y * tc);
    const double tvz = tc * v.z;
    const float  suz = (float)(sc * u.z);
    const float  px  = (float)(sc * u.x + w.x);
    const double py  = sc * u.y + w.y;
    const float  pz  = (float)((double)suz + w.z);
    const float  dx  = (float)((double)px - tvx);
    const float  dy  = (float)(py - tvy);
    const double dz  = (double)pz - tvz;
    return (dz * dz + (double)dy * dy) + (double)dx * dx;
}

float ClosestSquaredDistanceBetweenFiniteLines(
    const CVector& s1p0,
    const CVector& s2p0, const CVector& s2p1,
    const CVector& u, float a
) {
    return (float)ClosestSquaredDistanceBetweenFiniteLinesOG(s1p0, s2p0, s2p1, u, a);
}

/*!
* Calculate clamped barycentric coordinates of a point (p) on a triangle (a, b, c)
* See: https://gamedev.stackexchange.com/a/23745
*
* @notsa
*/
CVector CCollision::GetBaryCoordsOnTriangle(CVector a, CVector b, CVector c, CVector p) {
    ZoneScoped;

    const auto vab = b - a, vac = c - a, vap = p - a;
    const auto bb  = vab.Dot(vab);
    const auto bc  = vab.Dot(vac);
    const auto cc  = vac.Dot(vac);
    const auto pb  = vap.Dot(vab);
    const auto pc  = vap.Dot(vac);
    const auto d   = bb * cc - bc * bc;
    const auto v   = (cc * pb - bc * pc) / d;
    const auto w   = (bb * pc - bc * pb) / d;
    const auto u   = 1.0f - v - w;
    return { u, v, w };
}

/*!
* Get barycentric coords of point (p) clamped onto triangle (a, b, c)
* See: https://stackoverflow.com/a/37923949
*
* @notsa
*/
CVector CCollision::GetClampedBaryCoordsIntoTriangle(CVector a, CVector b, CVector c, CVector p) {
    ZoneScoped;

    // Calculate barycentric coords
    const auto [u, v, w] = GetBaryCoordsOnTriangle(a, b, c, p);

    // Calculate new `t` 
    const auto Get = [&](CVector v1, CVector v2) {
        const auto d = v1 - v2;
        return std::clamp((p - v2).Dot(d) / d.Dot(d), 0.f, 1.f);
    };

    if (u < 0.f) {
        const auto t = Get(c, b);
        return { 0.0f, 1.0f - t, t };
    }
    
    if (v < 0.f) {
        const auto t = Get(a, c);
        return { t, 0.0f, 1.0f - t };
    }

    
    if (w < 0.f) {
        const auto t = Get(b, a);
        return { 1.0f - t, t, 0.0f };
    }

    return { u, v, w }; // Point was in the triangle
}

/*!
* Calculate the point closest to `p` on the triangle (a, b, c)
* Basically same as `ClosestPtPointTriangle`.
* I'm leaving it here (even though it's unused), as it might actually be faster than the abovementioned function .
*/
CVector CCollision::GetCoordsClampedIntoTriangle(CVector a, CVector b, CVector c, CVector p) {
    ZoneScoped;

    const auto [u, v, w] = GetClampedBaryCoordsIntoTriangle(a, b, c, p);
    return a * u + b * v + c * w;
}

/*!
* Calculate the point closest to `p` on the triangle (a, b, c)
* 
* Credit: Code from "Real-Time Collision Detection" by Christer Ericson, published by Morgan Kaufmann Publishers, © 2005 Elsevier Inc
*/
CVector ClosestPtPointTriangle(
    CVector a, CVector b, CVector c,
    CVector p
) {
    // Check if P in vertex region outside A
    const auto ab = b - a;
    const auto ac = c - a;
    const auto ap = p - a;

    const auto d1 = ab.Dot(ap);
    const auto d2 = ac.Dot(ap);

    if (d1 <= 0.0f && d2 <= 0.0f) {
        return a; // barycentric coordinates (1,0,0)
    }

    // Check if P in vertex region outside B
    const auto bp = p - b;
    const auto d3 = ab.Dot(bp);
    const auto d4 = ac.Dot(bp);
    if (d3 >= 0.0f && d4 <= d3) {
        return b; // barycentric coordinates (0,1,0)
    }

    // Check if P in edge region of AB, if so return projection of P onto AB
    const auto vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
        const auto v = d1 / (d1 - d3);
        return a + v * ab; // barycentric coordinates (1-v,v,0)
    }

    // Check if P in vertex region outside C
    const auto cp = p - c;
    const auto d5 = ab.Dot(cp);
    const auto d6 = ac.Dot(cp);
    if (d6 >= 0.0f && d5 <= d6) {
        return c; // barycentric coordinates (0,0,1)
    }

    // Check if P in edge region of AC, if so return projection of P onto AC
    const auto vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
        const auto w = d2 / (d2 - d6);
        return a + w * ac; // barycentric coordinates (1-d1,0,d1)
    }

    // Check if P in edge region of BC, if so return projection of P onto BC
    const auto va = d3 * d6 - d5 * d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
        const auto w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        return b + w * (c - b); // barycentric coordinates (0,1-d1,d1)
    }

    // P inside face region. Compute Q through its barycentric coordinates (r,v,d1)
    const auto denom = va + vb + vc;
    const auto v     = vb / denom;
    const auto w     = vc / denom;
    return a + ab * v + ac * w; // = r*a + v*b + d1*c, r = va * denom = 1.0f-v-d1
}

/*!
* @addr 0x412AA0
* @brief Process line sphere intersection - Doesn'maxTouchDist deal well with cases where line starts/ends inside the sphere.
*
* @param[in,out] depth `t` parameter - relative distance on line from it's origin (`line.start`)
*/
bool CCollision::ProcessLineSphere(CColLine const& line, CColSphere const& sphere, CColPoint& colPoint, float& depth) {
    ZoneScoped;

    // NOTSA: Debug setting (enabled by default)
    if (!s_DebugSettings.ShapeShapeCollision.IsEnabled(Shape::SSPHERE, Shape::SLINE)) {
        return false;
    }

    // Quadratic: (d.d)t^2 - 2(d.s)t + (s.s - r^2) = 0, where d = line dir (unnormalized), s = sphere center - line start
    // The original keeps most things unrounded on the x87 stack => `double`, floats where the asm spills

    // d = end - start (extended)
    const double dx = (double)line.m_vecEnd.x - line.m_vecStart.x;
    const double dy = (double)line.m_vecEnd.y - line.m_vecStart.y;
    const double dz = (double)line.m_vecEnd.z - line.m_vecStart.z;
    const float  a  = (float)(dz * dz + dx * dx + dy * dy);

    // s = center - start (x, y spilled to floats; z is spilled, but the dot product uses the unrounded one)
    const float  sx  = sphere.m_vecCenter.x - line.m_vecStart.x;
    const float  sy  = sphere.m_vecCenter.y - line.m_vecStart.y;
    const double szE = (double)sphere.m_vecCenter.z - line.m_vecStart.z;
    const float  sz  = (float)szE;

    const double B   = -(szE * dz + (double)sy * dy + (double)sx * dx);
    const double s2  = (double)sz * sz + (double)sy * sy + (double)sx * sx;
    const double r   = sphere.m_fRadius;

    const double disc = B * B - (s2 - r * r) * a;
    if (disc < 0.0) { // NaN passes
        return false;
    }

    const double tE = (-B - std::sqrt(disc)) / a; // The first compare (`< 0`) uses the unrounded value, the others the float spill
    const float  t  = (float)tE;
    if (tE < 0.0 || t > 1.f || !(t < depth)) {
        return false;
    }

    // Intersection point
    const float dzF = (float)dz;
    const CVector point{
        (float)((double)(float)(dx * t) + line.m_vecStart.x),
        (float)(dy * t + line.m_vecStart.y),
        (float)((double)dzF * t + line.m_vecStart.z)
    };

    CVector normal = point - sphere.m_vecCenter;
    normal.Normalise();

    colPoint.m_vecPoint = point;
    colPoint.m_vecNormal = normal;

    // Original copies all 3 bytes (material, piece, lighting) of the sphere's surface to side B, side A's type and piece are zeroed (A's lighting is left untouched)
    colPoint.m_nSurfaceTypeB = sphere.m_Surface.m_nMaterial;
    colPoint.m_nPieceTypeB   = sphere.m_Surface.m_nPiece;
    colPoint.m_nLightingB    = sphere.m_Surface.m_nLighting;
    colPoint.m_nSurfaceTypeA = {};
    colPoint.m_nPieceTypeA   = 0;

    depth = t;

    ms_iProcessLineNumCrossings += 2;

    return true;
}

// 0x417470
bool CCollision::TestLineSphere(
    const CColLine& line,
    const CColSphere& sphere
) {
    ZoneScoped;

    // NOTSA: Debug setting (enabled by default)
    if (!s_DebugSettings.ShapeShapeCollision.IsEnabled(Shape::SSPHERE, Shape::SLINE)) {
        return false;
    }

    // d = end - start (x, y spilled to floats, z is spilled but multiplied unrounded by the spill)
    const float  dxF = line.m_vecEnd.x - line.m_vecStart.x;
    const float  dyF = line.m_vecEnd.y - line.m_vecStart.y;
    const double dzE = (double)line.m_vecEnd.z - line.m_vecStart.z;
    const float  dzF = (float)dzE;
    const float  len = (float)std::sqrt(dzE * dzF + (double)dxF * dxF + (double)dyF * dyF);

    // m = center - start (extended)
    const double mx = (double)sphere.m_vecCenter.x - line.m_vecStart.x;
    const double my = (double)sphere.m_vecCenter.y - line.m_vecStart.y;
    const double mz = (double)sphere.m_vecCenter.z - line.m_vecStart.z;
    const double r  = sphere.m_fRadius;

    if (len < 0.000001f) { // Degenerate line (a point) => is it inside the sphere
        const double m2 = mz * mz + my * my + mx * mx;
        return r * r >= m2; // `FCOMPP` + `test ah, 1` => false if `r^2 < m2` or NaN
    }

    // Quadratic: A t^2 + B t + C = 0
    const float  B = (float)((mz * dzF + mx * dxF + my * dyF) * -2.0);
    const float  A = (float)((double)len * len);

    const auto& c = sphere.m_vecCenter;
    const auto& s = line.m_vecStart;
    const double cc = (double)c.z * c.z + (double)c.y * c.y + (double)c.x * c.x;
    const double ss = (double)s.z * s.z + (double)s.y * s.y + (double)s.x * s.x;
    const double sc = (double)s.z * c.z + (double)s.x * c.x + (double)s.y * c.y;
    const double C  = ((ss + cc) - (sc + sc)) - r * r;

    const float disc = (float)((double)B * B - (C * A) * 4.0);
    if (disc < 0.f) { // NaN passes
        return false;
    }

    const double t = ((double)-B - std::sqrt((double)disc)) / (2.0 * A);
    return !(t < 0.0) && !(t > 1.0);
}

// 0x412C70
// Cohen-Sutherland style outcode test, followed by a test of the segment against the box planes the outcodes tell us to check
bool CCollision::TestLineBox_DW(CColLine const& line, CBox const& box) {
    ZoneScoped;

    const CVector& s = line.m_vecStart;
    const CVector& e = line.m_vecEnd;

    // Original gathers the sign bits of the 12 float differences below into two 6 bit codes
    // (bit: 0 = x < min, 1 = x > max, 2 = y < min, 3 = y > max, 4 = z < min, 5 = z > max)
    const auto MakeCode = [&](const CVector& p) {
        return (uint32)std::signbit(p.x - box.m_vecMin.x) << 0
             | (uint32)std::signbit(box.m_vecMax.x - p.x) << 1
             | (uint32)std::signbit(p.y - box.m_vecMin.y) << 2
             | (uint32)std::signbit(box.m_vecMax.y - p.y) << 3
             | (uint32)std::signbit(p.z - box.m_vecMin.z) << 4
             | (uint32)std::signbit(box.m_vecMax.z - p.z) << 5;
    };
    const uint32 startCode = MakeCode(s);
    const uint32 endCode   = MakeCode(e);

    if (startCode & endCode) { // Both outside on the same side
        return false;
    }
    if (startCode == 0 || endCode == 0) { // One of the points is inside
        return true;
    }
    const uint32 code = startCode | endCode;

    const CVector d{ e.x - s.x, e.y - s.y, e.z - s.z };

    // Is `a` strictly inside (min, max) (NaN => false)
    const auto Inside = [](float a, float mn, float mx) {
        return a > mn && a < mx;
    };

    // `t` is calculated in extended precision (=> double), the intersection coordinates are rounded to floats
    const auto Coord = [&](int32 axis, double t) {
        return (float)((double)d[axis] * t + s[axis]);
    };

    // X planes
    if (code & 0x3) {
        const float invDx = 1.f / d.x;
        if (code & 0x1) { // min.x plane
            const double t = -((double)(s.x - box.m_vecMin.x) * invDx);
            if (Inside(Coord(1, t), box.m_vecMin.y, box.m_vecMax.y) && Inside(Coord(2, t), box.m_vecMin.z, box.m_vecMax.z)) {
                return true;
            }
        }
        if (code & 0x2) { // max.x plane
            const double t = (double)(box.m_vecMax.x - s.x) * invDx;
            if (Inside(Coord(1, t), box.m_vecMin.y, box.m_vecMax.y) && Inside(Coord(2, t), box.m_vecMin.z, box.m_vecMax.z)) {
                return true;
            }
        }
    }

    // Y planes
    if (code & 0xC) {
        const float invDy = 1.f / d.y;
        if (code & 0x4) { // min.y plane
            const double t = -((double)(s.y - box.m_vecMin.y) * invDy);
            if (Inside(Coord(0, t), box.m_vecMin.x, box.m_vecMax.x) && Inside(Coord(2, t), box.m_vecMin.z, box.m_vecMax.z)) {
                return true;
            }
        }
        if (code & 0x8) { // max.y plane
            const double t = (double)(box.m_vecMax.y - s.y) * invDy;
            if (Inside(Coord(0, t), box.m_vecMin.x, box.m_vecMax.x) && Inside(Coord(2, t), box.m_vecMin.z, box.m_vecMax.z)) {
                return true;
            }
        }
    }

    // Z planes
    if (code & 0x30) {
        const float invDz = 1.f / d.z;
        if (code & 0x10) { // min.z plane
            const double t = -((double)(s.z - box.m_vecMin.z) * invDz);
            if (Inside(Coord(0, t), box.m_vecMin.x, box.m_vecMax.x) && Inside(Coord(1, t), box.m_vecMin.y, box.m_vecMax.y)) {
                return true;
            }
        }
        if (code & 0x20) { // max.z plane
            const double t = (double)(box.m_vecMax.z - s.z) * invDz;
            if (Inside(Coord(0, t), box.m_vecMin.x, box.m_vecMax.x) && Inside(Coord(1, t), box.m_vecMin.y, box.m_vecMax.y)) {
                return true;
            }
        }
    }

    return false;
}

// 0x413070
bool CCollision::TestLineBox(CColLine const& line, CBox const& box) {
    ZoneScoped;

    return TestLineBox_DW(line, box);
}

/*!
* @addr 0x413080
* @brief Test vertical \a line against \a bb
*/
bool CCollision::TestVerticalLineBox(CColLine const& line, CBox const& box) {
    ZoneScoped;

    // Original (0x413080): NaN handling is part of the comparisons (`!(start > min)`, `!(start < max)`)
    for (auto i = 0u; i < 2u; i++) { // Deal with x, y axies
        if (!(line.m_vecStart[i] > box.m_vecMin[i])) {
            return false;
        }
    }
    for (auto i = 0u; i < 2u; i++) {
        if (!(line.m_vecStart[i] < box.m_vecMax[i])) {
            return false;
        }
    }

    // Line might go from top to bottom, or from bottom to top, so we have to account for both cases
    const bool startIsLower = line.m_vecStart.z < line.m_vecEnd.z;
    const float minz = startIsLower ? line.m_vecStart.z : line.m_vecEnd.z;
    const float maxz = startIsLower ? line.m_vecEnd.z   : line.m_vecStart.z;
    return !(minz > box.m_vecMax.z) && !(maxz < box.m_vecMin.z);
}

// 0x413100
/*!
* @addr 0x413100
* @brief Process \a line and \a bb collision.
*
* @param[out]    diskColPoint         Collision point
* @param[in,out] maxTouchDistance Collision point depth inside bb - If calculated value is higher than this value the function will return false, and no colpoint will be set.
*
* @returns If there was a collision or not. If there was a collision, but calculated depth is bigger than `maxTouchDistance` it returns false regardless.
*/
bool CCollision::ProcessLineBox(CColLine const& line, CColBox const& box, CColPoint& colPoint, float& maxTouchDistance) {
    ZoneScoped;

    const CVector& s = line.m_vecStart;
    const CVector& e = line.m_vecEnd;

    // Original copies all 3 bytes (material, piece, lighting) of the box's surface to side B,
    // side A's type and piece are zeroed (A's lighting is left untouched)
    const auto SetSurfaces = [&] {
        colPoint.m_nSurfaceTypeA = eSurfaceType::SURFACE_DEFAULT;
        colPoint.m_nPieceTypeA   = 0;
        colPoint.m_nSurfaceTypeB = box.m_Surface.m_nMaterial;
        colPoint.m_nPieceTypeB   = box.m_Surface.m_nPiece;
        colPoint.m_nLightingB    = box.m_Surface.m_nLighting;
    };

    // Line starts strictly inside of the box?
    if (   s.x > box.m_vecMin.x && s.y > box.m_vecMin.y && s.z > box.m_vecMin.z
        && s.x < box.m_vecMax.x && s.y < box.m_vecMax.y && s.z < box.m_vecMax.z
    ) {
        // Line leaves the box => 1 crossing
        if (   e.x < box.m_vecMin.x || e.y < box.m_vecMin.y || e.z < box.m_vecMin.z
            || e.x > box.m_vecMax.x || e.y > box.m_vecMax.y || e.z > box.m_vecMax.z
        ) {
            ms_iProcessLineNumCrossings++;
        }

        CalculateColPointInsideBox(box, s, colPoint);
        SetSurfaces();
        maxTouchDistance = 0.f;
        return true;
    }

    // NOTSA: Original leaves these uninitialized (only used if a plane was hit)
    CVector p{}, normal{};
    float   mint = 1.f; // Original keeps this on the x87 stack

    // Tests the line against the min/max plane of `axis`.
    // Original: `t` is calculated in extended precision and rounded to a float, intersection coordinates likewise.
    const auto TryPlane = [&](int32 axis, bool isMax) {
        const int32 o1 = (axis + 1) % 3, o2 = (axis + 2) % 3; // The other 2 axies (in ascending order: (y,z), (x,z), (x,y))

        const double a = isMax ? (double)s[axis] - box.m_vecMax[axis] : (double)box.m_vecMin[axis] - s[axis];
        const double b = isMax ? (double)e[axis] - box.m_vecMax[axis] : (double)box.m_vecMin[axis] - e[axis];
        if (!(b * a < 0.0)) { // Points are not on opposite sides of the plane
            return;
        }
        const float t = (float)(a / (a - b));

        const auto Coord = [&](int32 o) { return (float)(((double)e[o] - s[o]) * t + s[o]); };
        const float c1 = Coord(o1), c2 = Coord(o2);

        if (   c1 > box.m_vecMin[o1] && c1 < box.m_vecMax[o1]
            && c2 > box.m_vecMin[o2] && c2 < box.m_vecMax[o2]
            && t < mint
        ) {
            mint        = t;
            p[axis]     = isMax ? box.m_vecMax[axis] : box.m_vecMin[axis];
            p[o1]       = c1;
            p[o2]       = c2;
            normal      = CVector{};
            normal[axis] = isMax ? 1.f : -1.f;
        }
    };
    TryPlane(0, false); // min x
    TryPlane(0, true);  // max x
    TryPlane(1, false); // min y
    TryPlane(1, true);  // max y
    TryPlane(2, false); // min z
    TryPlane(2, true);  // max z

    if (!(mint < maxTouchDistance)) {
        return false;
    }

    colPoint.m_vecPoint  = p;
    colPoint.m_vecNormal = normal;
    SetSurfaces();
    maxTouchDistance = mint;

    // Line ends inside of the box => 1 crossing, otherwise 2
    if (   !(e.x < box.m_vecMin.x) && !(e.y < box.m_vecMin.y) && !(e.z < box.m_vecMin.z)
        && !(e.x > box.m_vecMax.x) && !(e.y > box.m_vecMax.y) && !(e.z > box.m_vecMax.z)
    ) {
        ms_iProcessLineNumCrossings += 1;
    } else {
        ms_iProcessLineNumCrossings += 2;
    }

    return true;
}

/*!
* @addr 0x4138D0
* @returns If there was an intersection - TODO: What if lines are colinear?
*/
bool CCollision::Test2DLineAgainst2DLine(float line1StartX, float line1StartY, float line1EndX, float line1EndY, float line2StartX, float line2StartY, float line2EndX, float line2EndY) {
    ZoneScoped;

    // Original (0x4138D0): evaluated entirely in extended precision. NaN => the `> 0` tests fail, so NaN counts as "intersecting"
    const double ax = (double)line2StartX - line1StartX, ay = (double)line2StartY - line1StartY;
    const double p1 = ((ax + line2EndX) * line1EndY - (ay + line2EndY) * line1EndX) * (ax * line1EndY - ay * line1EndX);
    if (p1 > 0.0) {
        return false;
    }

    const double bx = (double)line1StartX - line2StartX, by = (double)line1StartY - line2StartY;
    const double p2 = ((bx + line1EndX) * line2EndY - (by + line1EndY) * line2EndX) * (bx * line2EndY - by * line2EndX);
    return !(p2 > 0.0);
}

/*
* Process disk-colpoint or otherwise line-colpoint collision.
* @addr 0x413960
* @param tempTriCol    Colpoint with a triangle (Space B)
* @param matBA         Transformation matrix from B's space into A's (The space we're in)
* @param disk          The disk (Space A)
* @param diskColPoint  Disk collision point (Space A)
* @param lineCollision If there was a line collision (Only checked if no discr collision)
* @param lineRatio     Not sure
* @param lineColPoint  Line colpoint (Only valid if `lineCollision` was set)
* @returns If the diskColPoint collides with the discr
*/
// 0x413960
bool CCollision::ProcessDiscCollision(
    CColPoint& tempTriCol,
    const CMatrix& matBA,
    const CColDisk& disk,
    CColPoint& diskColPoint,
    bool& lineCollision,
    float& lineRatio,
    CColPoint& lineColPoint
) {
    ZoneScoped;

    // Original (0x413960): 0x59C890 / 0x59C790 accumulate unrounded, results are stored as floats
    const auto cp       = TransformPointOG(matBA, tempTriCol.m_vecPoint);
    const auto cpNormal = TransformVectorOG(matBA, tempTriCol.m_vecNormal);

    // Dot of the (transformed) normal with the disk's thickness vector (terms: y, z, x)
    const double normalDot = (double)cpNormal.y * disk.m_vThickness.y + (double)cpNormal.z * disk.m_vThickness.z + (double)cpNormal.x * disk.m_vThickness.x;

    // Offset of the point from the disk center, `x`/`y` are unrounded for the thickness test, but rounded when used for the radius test
    const double dxE = (double)cp.x - disk.m_vecCenter.x;
    const double dyE = (double)cp.y - disk.m_vecCenter.y;
    const double dzE = (double)cp.z - disk.m_vecCenter.z;

    // Note: The 2nd test is only evaluated if the 1st one passed
    if (!(std::abs(normalDot) < 0.77f) ||
        !(std::abs(dzE * disk.m_vThickness.z + dyE * disk.m_vThickness.y + dxE * disk.m_vThickness.x) < disk.m_fThickness)
    ) {
        if (disk.m_Surface.m_nPiece < 17 && tempTriCol.m_fDepth > diskColPoint.m_fDepth) {
            diskColPoint = tempTriCol;
            //diskColPoint.m_fDepth = tempTriCol.m_fDepth; // Done in operator=
            diskColPoint.m_nSurfaceTypeB = SURFACE_WHEELBASE;
            return true;
        }
    } else {
        const double dx = (float)dxE;
        const double dy = (float)dyE;
        const double rr = (double)disk.m_fRadius * disk.m_fRadius;
        // NOTE: No clamping of the sqrt argument => NaN if the point is outside of the disk radius (=> `>=` below is false)
        const double lineRatioNow = std::sqrt((rr - dy * dy) - dx * dx) + cp.z;
        if (lineRatioNow >= lineRatio) {
            lineCollision = true;
            lineRatio     = (float)lineRatioNow;
            lineColPoint  = tempTriCol;
            // lineColPoint.m_fDepth = tempTriCol.m_fDepth;  // Done in operator=
            return false; // False is returned here, but `lineCollision` was set to true.
        }
    }
    return false;
}


/*!
* Process line-triangle intersection, internal function (used to implement `ProcessLineTriangle` and `TestLineTriangle`)
* @tparam TestOnly If we're only doing an intersection check, and are not interested in the intersection point, normal, etc. In this case the last 3 arguments can be nullptr.
* @notsa
*/
template<bool TestOnly>
bool NOTSA_FORCEINLINE ProcessLineTriangle_Internal(
    const CColLine& line,
    const CStoredCollPoly& poly,
    const CColTrianglePlane& plane,
    float* inOutMaxTouchDist,
    CVector* outIP,
    CVector* outPlNorm
) {
    if (!CCollision::s_DebugSettings.ShapeShapeCollision.IsEnabled(Shape::SLINE, Shape::STRI)) {
        return false;
    }

    const auto &va = poly.verts[0],
               &vb = poly.verts[1],
               &vc = poly.verts[2];

#ifdef NOTSA_VANILLA_COLLISIONS
    // If the line is vertical, we can do some quick bound checks
    if (line.IsVertical() && !CColTriangle::GetBoundingRect(va, vb, vc).IsPointInside(line.m_vecStart)) {
        return false;
    }

    const auto plNorm = plane.GetNormal();
    if constexpr (!TestOnly) {
        *outPlNorm = plNorm;
    }

    // Origin of line on the plane
    const auto plNormDotLnOrigin = plane.GetPtDotNormal(line.m_vecStart);

    // Check if both points are above or below the plane, if so, no chance of intersection
    if (std::signbit(plNormDotLnOrigin) == std::signbit(plane.GetPtDotNormal(line.m_vecEnd))) {
return false;
    }

    // Magnitude of line on plane
    const auto plLnMag = -(line.m_vecEnd - line.m_vecStart).Dot(plNorm);

#ifdef FIX_BUGS
    // Line is (near-)parallel to the plane, no intersection.
    // `plLnMag ~ 0` makes `t = plNormDotLnOrigin / plLnMag` explode to NaN/Inf, which then
    // slips past the `t >= maxTouchDist` compare (NaN compares false) and corrupts the
    // wheel contact / touch distance downstream. Reject near-parallel lines outright.
    if (std::abs(plLnMag) < 1e-6f) {
        return false;
    }
#endif

    const auto t = plNormDotLnOrigin / plLnMag;
    if constexpr (!TestOnly) {
        if (t >= *inOutMaxTouchDist) {
            return false;
        }
    }

	// Find point of intersection
    const auto ip = lerp(line.m_vecStart, line.m_vecEnd, t);
    if constexpr (!TestOnly) {
        *outIP = ip;
    }

    // Get the points relative to the plane's orientation
    // This way the bound checks can be done in 2D
    const auto [pl_va, pl_vb, pl_vc, pl_ip] = [&]() -> std::tuple<CVector2D, CVector2D, CVector2D, CVector2D> {
	    // We do the test in 2D.
        // With the plane direction we can figure out how to project the vectors.
	    // normal = (c - a) x (b - a)
        using enum CColTrianglePlane::Orientation;
	    switch (plane.m_orientation){
	    case POS_X: return {
            {va.y, va.z},
            {vc.y, vc.z},
            {vb.y, vb.z},
            {ip.y, ip.z}
        };
        case NEG_X: return {
            {va.y, va.z},
            {vb.y, vb.z},
            {vc.y, vc.z},
            {ip.y, ip.z}
        };
        case POS_Y: return {
            {va.z, va.x},
            {vc.z, vc.x},
            {vb.z, vb.x},
            {ip.z, ip.x}
        };
        case NEG_Y: return {
            {va.z, va.x},
            {vb.z, vb.x},
            {vc.z, vc.x},
            {ip.z, ip.x}
        };
        case POS_Z: return {
            {va.x, va.y},
            {vc.x, vc.y},
            {vb.x, vb.y},
            {ip.x, ip.y}
        };
        case NEG_Z: return {
			{va.x, va.y},
			{vb.x, vb.y},
			{vc.x, vc.y},
			{ip.x, ip.y}
		};
	    default: NOTSA_UNREACHABLE();
	    }
    }();

	// This is our triangle:
	// pl_vc---pl_vb
	//    \     /
	//     \   /
	//      \ /
	//     pl_va
	// We can use the "2v2 cross product" to check on which side
	// a vector is of another. Test is true if point is inside of all edges.
    const auto pl_ip_a = pl_ip - pl_va;
    if ((pl_vb - pl_va).Cross(pl_ip_a) >= 0.0f && (pl_vc - pl_va).Cross(pl_ip_a) <= 0.0f && (pl_vc - pl_vb).Cross(pl_ip - pl_vb) >= 0.0f) {
        if (inOutMaxTouchDist) {
            *inOutMaxTouchDist = t;
        }
        return true;
    }
    return false;
#else // Not really tested (might not work properly)
    // https://stackoverflow.com/a/42752998
    const auto eB = vb - va,
               eC = vc - va,
               n  = eC.Cross(eB);

    const auto lnseg = line.m_vecEnd - line.m_vecStart;
    
    const auto det = -lnseg.Dot(n);
    if (det < 1e-6f) {
        return false;
    }

    const auto AO = line.m_vecStart - va;
    const auto t  = AO.Dot(n) / det;
    if (t < 0.f || t > 1.f) {
        return false;
    }

    if constexpr (!TestOnly) {
        if (t >= *inOutMaxTouchDist) {
            return false;
        }
    }

    const auto DAO = lnseg.Cross(AO);

    const auto u = eC.Dot(DAO) / det;
    if (u > 1.f) {
        return false;
    }

    const auto v = -eB.Dot(DAO) / det;
    if (v > 1.f) {
        return false;
    }

    if (u + v > 1.f) {
        return false;
    }

    if constexpr (!TestOnly) {
        *inOutMaxTouchDist = t;
        *outIP             = line.m_vecStart + lnseg * t;
        *outPlNorm         = plane.GetNormal();
    }
    return true;
#endif
}

namespace {
//! Unpack the 3 vertices of `tri` (the exe converts `int16 * 0.0078125`, which is exact)
void UnpackTriangle(const CompressedVector* verts, const CColTriangle& tri, CVector& a, CVector& b, CVector& c) {
    a = CVector{ verts[tri.vA] };
    b = CVector{ verts[tri.vB] };
    c = CVector{ verts[tri.vC] };
}

//! Original (0x413AC0, 0x4140F0, 0x4147E0): the plane's normal and offset, exact (int16 / 4096, int16 / 128)
struct LineTriPlane {
    explicit LineTriPlane(const CColTrianglePlane& pl) :
        nx{ pl.m_normal.x }, ny{ pl.m_normal.y }, nz{ pl.m_normal.z }, d{ (float)pl.m_normalOffset }
    { }
    float nx, ny, nz, d;
};

/*!
* The 2D "is the intersection point inside of the triangle" test shared by `TestLineTriangle`, `ProcessLineTriangle` and `ProcessVerticalLineTriangle`.
* The exe projects onto the plane that is perpendicular to the dominant axis of the normal. For odd orientations vertices B and C are swapped.
* Everything stays in extended precision (=> `double`), a NaN passes every check (so the point is considered inside).
*/
bool LineTriangle_IsPointInside2D(CColTrianglePlane::Orientation orientation, const CVector& A, const CVector& B, const CVector& C, const CVector& ip) {
    using enum CColTrianglePlane::Orientation;

    int32 P, Q; // The 2 axes of the projection
    switch (orientation) {
    case POS_X: case NEG_X: P = 2; Q = 1; break;
    case POS_Y: case NEG_Y: P = 0; Q = 2; break;
    case POS_Z: case NEG_Z: P = 1; Q = 0; break;
    default:                NOTSA_UNREACHABLE();
    }
    const bool  odd = ((uint8)orientation & 1) != 0;
    const auto& Bc  = odd ? C : B;
    const auto& Cc  = odd ? B : C;

    const double uP = (double)ip[P] - A[P];
    const double uQ = (double)ip[Q] - A[Q];

    if (((double)Cc[Q] - A[Q]) * uP - ((double)Cc[P] - A[P]) * uQ < 0.0) {
        return false;
    }
    if (((double)Bc[Q] - A[Q]) * uP - ((double)Bc[P] - A[P]) * uQ > 0.0) {
        return false;
    }
    return !(((double)ip[P] - Cc[P]) * ((double)Bc[Q] - Cc[Q]) - ((double)ip[Q] - Cc[Q]) * ((double)Bc[P] - Cc[P]) < 0.0);
}

/*!
* Intersect the line with the plane of a triangle.
* @returns false if both line points are on the same side of the plane
* @param outT  Line parameter of the intersection
* @param outIP Intersection point (extended precision based, only used for the 2D test)
*/
bool LineTriangle_IntersectPlane(const CColLine& line, const LineTriPlane& pl, float& outT, CVector& outIP) {
    const CVector& s = line.m_vecStart;
    const CVector& e = line.m_vecEnd;

    // Products with the start point are spilled to floats
    const float nxsx = pl.nx * s.x, nysy = pl.ny * s.y, nzsz = pl.nz * s.z;

    const double dE = ((double)pl.nz * e.z + (double)pl.ny * e.y + (double)pl.nx * e.x) - pl.d;
    const double dS = (((double)nxsx + nysy) + nzsz) - pl.d;
    if (!(dS * dE <= 0.0)) { // Both points on the same side of the plane (or NaN)
        return false;
    }

    const double dx  = (double)e.x - s.x;
    const double dy  = (double)e.y - s.y;
    const float  dzF = e.z - s.z;

    const double num = (((double)pl.d - nxsx) - nysy) - nzsz;
    const double den = (double)dzF * pl.nz + dy * pl.ny + dx * pl.nx;
    const float  t   = (float)(num / den);

    outT  = t;
    outIP = CVector{
        (float)((double)(float)(dx * t) + s.x),
        (float)(dy * t + s.y),
        (float)((double)dzF * t + s.z)
    };
    return true;
}

//! `start + (end - start) * t`, using float ops (CVector operators in the exe: 0x40FE60, 0x40FEC0, 0x40FE30)
CVector LineTriangle_PointAt(const CColLine& line, float t) {
    const CVector d{ line.m_vecEnd.x - line.m_vecStart.x, line.m_vecEnd.y - line.m_vecStart.y, line.m_vecEnd.z - line.m_vecStart.z };
    const CVector dt{ t * d.x, t * d.y, t * d.z };
    return { line.m_vecStart.x + dt.x, line.m_vecStart.y + dt.y, line.m_vecStart.z + dt.z };
}

//! The colpoint output of `ProcessLineTriangle` / `ProcessVerticalLineTriangle`
void LineTriangle_StoreResult(
    const CColLine& line, float t, const LineTriPlane& pl, const CColTriangle& tri,
    const CVector& A, const CVector& B, const CVector& C,
    CColPoint& colPoint, float& maxTouchDistance, CStoredCollPoly* collPoly
) {
    colPoint.m_vecPoint  = LineTriangle_PointAt(line, t);
    colPoint.m_vecNormal = CVector{ pl.nx, pl.ny, pl.nz };

    colPoint.m_nSurfaceTypeB = tri.m_nMaterial;
    colPoint.m_nPieceTypeB   = 0;
    colPoint.m_nLightingB    = tri.m_nLight;
    colPoint.m_nSurfaceTypeA = SURFACE_DEFAULT; // NOTE: Lighting A is left untouched
    colPoint.m_nPieceTypeA   = 0;

    if (collPoly) {
        collPoly->verts[0] = A;
        collPoly->verts[1] = B;
        collPoly->verts[2] = C;
        collPoly->valid    = true;
        // Original (0x4147A5): `movzx edx, byte [tri+7]; mov dword [poly+0x28], edx` - the whole dword is written, the 3 bytes after the 1 byte lighting are zeroed
        *reinterpret_cast<uint32*>(&collPoly->ligthing) = tri.m_nLight.value;
    }

    maxTouchDistance = t;
}
}

/*!
* @addr 0x413AC0
*/
bool CCollision::TestLineTriangle(const CColLine& line, const CompressedVector* verts, const CColTriangle& tri, const CColTrianglePlane& plane) {
    ZoneScoped;

    // NOTSA: Debug setting (enabled by default)
    if (!s_DebugSettings.ShapeShapeCollision.IsEnabled(Shape::SLINE, Shape::STRI)) {
        return false;
    }

    float   t;
    CVector ip;
    if (!LineTriangle_IntersectPlane(line, LineTriPlane{ plane }, t, ip)) {
        return false;
    }

    CVector A, B, C;
    UnpackTriangle(verts, tri, A, B, C);
    return LineTriangle_IsPointInside2D(plane.m_orientation, A, B, C, ip);
}

/*!
* Processes `line \a tri collision.
* 
* @addr 0x4140F0
*
* @param[out]    diskColPoint     Collision point
* @param[in,out] maxTouchDistance Distance from line origin to intersection point
* @param[out]    collPoly         If given (can be null) stored the uncompressed vertices of the triangle and set's it's `actual` field to `true`
*
* @returns If there was a collision that was closer to the beginning of the line than `maxTouchDistance`
*/
bool CCollision::ProcessLineTriangle(const CColLine& line, const CompressedVector* verts, const CColTriangle& tri, const CColTrianglePlane& plane, CColPoint& colPoint, float& maxTouchDistance, CStoredCollPoly* collPoly) {
    ZoneScoped;

    // NOTSA: Debug setting (enabled by default)
    if (!s_DebugSettings.ShapeShapeCollision.IsEnabled(Shape::SLINE, Shape::STRI)) {
        return false;
    }

    const LineTriPlane pl{ plane };

    float   t;
    CVector ip;
    if (!LineTriangle_IntersectPlane(line, pl, t, ip)) {
        return false;
    }

    CVector A, B, C;
    UnpackTriangle(verts, tri, A, B, C);
    if (!LineTriangle_IsPointInside2D(plane.m_orientation, A, B, C, ip)) {
        return false;
    }

    if (!(t < maxTouchDistance)) { // NaN => false
        return false;
    }

    LineTriangle_StoreResult(line, t, pl, tri, A, B, C, colPoint, maxTouchDistance, collPoly);
    return true;
}

// 0x4147E0
bool CCollision::ProcessVerticalLineTriangle(
    const CColLine& line,
    const CompressedVector* verts,
    const CColTriangle& tri,
    const CColTrianglePlane& plane,
    CColPoint& colPoint,
    float& maxTouchDistance,
    CStoredCollPoly* collPoly
) {
    ZoneScoped;

    // NOTSA: Debug setting (enabled by default)
    if (!s_DebugSettings.ShapeShapeCollision.IsEnabled(Shape::SLINE, Shape::STRI)) {
        return false;
    }

    const CVector& s = line.m_vecStart;
    const CVector& e = line.m_vecEnd;

    CVector A, B, C;
    UnpackTriangle(verts, tri, A, B, C);

    // Quick reject: the (vertical) line's xy is outside of the triangle's bounding rect.
    // Original (0x4147E0): this is NOT a plain min/max check, see the asm: it only rejects if all 3 vertices are on one side
    // (the 2nd branch isn't even checking against the 1st vertex)
    for (int32 i = 0; i < 2; i++) {
        const bool reject = s[i] < A[i]
            ? (s[i] < B[i] && s[i] < C[i])
            : (s[i] > B[i] && s[i] > C[i]);
        if (reject) {
            return false;
        }
    }

    const LineTriPlane pl{ plane };

    // Original: `nx * sx` and `ny * sy` stay unrounded, only `nz * sz` is spilled to a float
    const float  nzsz = pl.nz * s.z;
    const double dE   = ((double)pl.nz * e.z + (double)pl.ny * e.y + (double)pl.nx * e.x) - pl.d;
    const double dS   = (((double)pl.nx * s.x + (double)pl.ny * s.y) + nzsz) - pl.d;
    if (!(dS * dE <= 0.0)) {
        return false;
    }

    const double dz  = (double)e.z - s.z;
    const double num = (((double)pl.d - (double)pl.nx * s.x) - (double)pl.ny * s.y) - nzsz;
    const float  t   = (float)(num / ((double)pl.nz * dz));

    // The intersection (only z needs to be calculated)
    const CVector ip{ s.x, s.y, (float)((double)t * dz + s.z) };
    if (!LineTriangle_IsPointInside2D(plane.m_orientation, A, B, C, ip)) {
        return false;
    }

    if (!(t < maxTouchDistance)) { // NaN => false
        return false;
    }

    LineTriangle_StoreResult(line, t, pl, tri, A, B, C, colPoint, maxTouchDistance, collPoly);
    return true;
}

// 0x416450
bool CCollision::ProcessSphereSphere(const CColSphere& spA, const CColSphere& spB, CColPoint& colPoint, float& maxTouchDistance) {
    ZoneScoped;

    // Original (0x416450): `d.z` is multiplied unrounded by its (rounded) spill, the sum order is z, x, y
    const float  dx  = spA.m_vecCenter.x - spB.m_vecCenter.x;
    const float  dy  = spA.m_vecCenter.y - spB.m_vecCenter.y;
    const double dzE = (double)spA.m_vecCenter.z - spB.m_vecCenter.z;
    const float  dz  = (float)dzE;

    const float u     = (float)(std::sqrt(dzE * dz + (double)dx * dx + (double)dy * dy) - spB.m_fRadius); // Unclamped touch distance
    const float depth = spA.m_fRadius - u;
    const float touchDist = u < 0.f ? 0.f : u; // NaN stays NaN
    const double touchDistSq = (double)touchDist * touchDist; // Unrounded for the compare

    if (!(touchDistSq < maxTouchDistance)) { // NaN => false
        return false;
    }

    if (!(touchDist < spA.m_fRadius)) { // NaN => false
        return false;
    }

    CVector normal{ dx, dy, dz };
    normal.Normalise();

    colPoint.m_vecPoint = CVector{
        (float)((double)spA.m_vecCenter.x - (double)normal.x * touchDist),
        (float)((double)spA.m_vecCenter.y - (double)normal.y * touchDist),
        spA.m_vecCenter.z - normal.z * touchDist
    };
    colPoint.m_vecNormal = normal;

    colPoint.m_nSurfaceTypeA = spA.m_Surface.m_nMaterial;
    colPoint.m_nPieceTypeA   = spA.m_Surface.m_nPiece;
    colPoint.m_nLightingA    = spA.m_Surface.m_nLighting;

    colPoint.m_nSurfaceTypeB = spB.m_Surface.m_nMaterial;
    colPoint.m_nPieceTypeB   = spB.m_Surface.m_nPiece;
    colPoint.m_nLightingB    = spB.m_Surface.m_nLighting;

    colPoint.m_fDepth = depth;

    maxTouchDistance = (float)touchDistSq;

    return true;
}

/*!
* @addr 0x4165B0
*/
bool CCollision::TestSphereTriangle(
    const CColSphere& sphere,
    const CompressedVector* verts,
    const CColTriangle& tri,
    const CColTrianglePlane& plane
) {
    ZoneScoped;

    // NOTSA: Debug setting (enabled by default)
    if (!CCollision::s_DebugSettings.ShapeShapeCollision.IsEnabled(Shape::SSPHERE, Shape::STRI)) {
        return false;
    }

    // The exe calculates the closest distance of the sphere's center to the triangle in a coordinate system `(e, M)` in the triangle's plane
    // (e = unit vector of A->B, M = e x N), where A is at the origin, B at (|AB|, 0), C at (Ce, Cm), and the center is at (Pe, Pm).
    // Mostly extended precision (=> double) with float spills (see comments), the term orders are the exe's.

    const auto& c = sphere.m_vecCenter;
    const float r = sphere.m_fRadius;

    const float nx = plane.m_normal.x, ny = plane.m_normal.y, nz = plane.m_normal.z;
    const float planeOffset = (float)plane.m_normalOffset;

    // Signed distance of the center from the plane
    const double planeDistE = (((double)nx * c.x + (double)nz * c.z) + (double)ny * c.y) - planeOffset;
    if (std::abs(planeDistE) > r) { // NaN passes
        return false;
    }
    const float planeDist    = (float)planeDistE;
    const float absPlaneDist = (float)std::abs(planeDistE);

    CVector A, B, C;
    UnpackTriangle(verts, tri, A, B, C);

    // Unit vector A->B
    const double abx = (double)B.x - A.x, aby = (double)B.y - A.y, abz = (double)B.z - A.z;
    const float  abzF = (float)abz;
    const float  len  = (float)std::sqrt(abz * abzF + aby * aby + abx * abx);
    const float  invLen = (float)(1.0 / len);
    const float  ex = (float)(abx * invLen), ey = (float)(aby * invLen), ez = abzF * invLen;

    // M = e x N (CrossProduct: 0x59C730)
    const float mx = (float)((double)nz * ey - (double)ez * ny);
    const float my = (float)((double)ez * nx - (double)nz * ex);
    const float mz = (float)((double)ex * ny - (double)nx * ey);

    // C in the plane's coordinate system
    const double cax = (double)C.x - A.x, cay = (double)C.y - A.y, caz = (double)C.z - A.z;
    const float  Ce  = (float)((cay * ey + cax * ex) + caz * ez);
    const float  Cm  = (float)((cax * mx + (double)mz * caz) + (double)my * cay);

    // The sphere's center in the plane's coordinate system
    const double pax = (double)c.x - A.x, pay = (double)c.y - A.y, paz = (double)c.z - A.z;
    const float  Pe  = (float)((pax * ex + paz * ez) + pay * ey);
    const float  Pm  = (float)((pax * mx + paz * mz) + pay * my);

    // Which side of the 3 edges is the center on? (all 3 => inside of the triangle)
    int32 numPassed = 0;
    bool  abPassed = false, acPassed = false;

    const double abSide = (double)Pm * len - (double)Pe * 0.0;
    const float  abSideF = (float)abSide; // Spilled
    if (abSide >= 0.0) { // NaN fails
        numPassed++;
        abPassed = true;
    }

    const float pec = (float)((double)Pe * Cm); // Pe * Cm
    const float pmc = (float)((double)Pm * Ce); // Pm * Ce
    if ((double)pec - pmc >= 0.0) { // NaN fails
        numPassed++;
        acPassed = true;
    }

    const float ceMinusLen = Ce - len;
    const float peMinusLen = Pe - len;
    if ((double)ceMinusLen * Pm - (double)peMinusLen * Cm >= 0.0) { // NaN fails
        numPassed++;
    }

    // Distance to a vertex (`CVector::Magnitude` of `center - vertex`, order x, y, z)
    const auto DistToVertex = [&](const CVector& v) {
        const CVector d{ c.x - v.x, c.y - v.y, c.z - v.z };
        return std::sqrt((double)d.x * d.x + (double)d.y * d.y + (double)d.z * d.z);
    };

    // Distance to the infinite edge line + the plane distance
    const auto DistToEdgeLine = [&](double perp) {
        return std::sqrt(perp * perp + (double)planeDist * planeDist);
    };

    // Projection (of the center) onto an edge: <= 0 => start vertex, >= 1 => end vertex (NaN => perpendicular)
    double dist;
    switch (numPassed) {
    case 0: // Not possible geometrically
        return false;
    case 3: // Inside of the triangle => the distance to the plane
        dist = absPlaneDist;
        break;
    case 2: {
        if (!abPassed) { // Edge AB
            const float abLenSq = (float)((double)len * len);
            const double s = ((double)Pe * len + (double)Pm * 0.0) / abLenSq;
            if (s <= 0.0) {
                dist = DistToVertex(A);
            } else if (s >= 1.0) {
                dist = DistToVertex(B);
            } else {
                dist = DistToEdgeLine((double)abSideF / std::sqrt((double)abLenSq));
            }
        } else if (!acPassed) { // Edge AC
            const float acLenSq = (float)((double)Cm * Cm + (double)Ce * Ce);
            const double s = ((double)Pm * Cm + (double)Pe * Ce) / acLenSq;
            if (s <= 0.0) {
                dist = DistToVertex(A);
            } else if (s >= 1.0) {
                dist = DistToVertex(C);
            } else {
                dist = DistToEdgeLine(((double)pmc - pec) / std::sqrt((double)acLenSq));
            }
        } else { // Edge BC, from B to C: (Ce - len, Cm)
            const float bcX = ceMinusLen, bcY = Cm;
            const float bcLenSq = (float)((double)bcY * bcY + (double)bcX * bcX);
            const double s = ((double)peMinusLen * bcX + (double)Pm * bcY) / bcLenSq;
            if (s <= 0.0) {
                dist = DistToVertex(B);
            } else if (s >= 1.0) {
                dist = DistToVertex(C);
            } else {
                dist = DistToEdgeLine(((double)Pm * bcX - (double)peMinusLen * bcY) / std::sqrt((double)bcLenSq));
            }
        }
        break;
    }
    case 1: // Closest to a vertex
        dist = abPassed ? DistToVertex(C) : acPassed ? DistToVertex(B) : DistToVertex(A);
        break;
    default:
        NOTSA_UNREACHABLE();
    }

    return dist < r; // NaN => false
}

// 0x416BA0
bool CCollision::ProcessSphereTriangle(
    const CColSphere& sphere,
    const CompressedVector* verts,
    const CColTriangle& tri,
    const CColTrianglePlane& plane,
    CColPoint& colPoint,
    float& maxTouchDistance
) {
    ZoneScoped;

    // NOTSA: Debug setting (enabled by default)
    if (!CCollision::s_DebugSettings.ShapeShapeCollision.IsEnabled(Shape::SSPHERE, Shape::STRI)) {
        return false;
    }

    // Same method as `TestSphereTriangle` (see there), but with slightly different term orders, and it also calculates the closest point.
    // Original (0x416BA0): mostly extended precision (=> double) with float spills (see comments).

    const auto& c = sphere.m_vecCenter;
    const float r = sphere.m_fRadius;

    const float nx = plane.m_normal.x, ny = plane.m_normal.y, nz = plane.m_normal.z;
    const float planeOffset = (float)plane.m_normalOffset;

    // Signed distance of the center from the plane
    const double planeDistE = (((double)nz * c.z + (double)ny * c.y) + (double)nx * c.x) - planeOffset;
    if (std::abs(planeDistE) > r) { // NaN passes
        return false;
    }
    const float planeDist    = (float)planeDistE;
    const float absPlaneDist = (float)std::abs(planeDistE);
    const float planeDistSq  = (float)((double)planeDist * planeDist);
    if ((double)planeDist * planeDist > maxTouchDistance) { // NaN passes
        return false;
    }

    CVector A, B, C;
    UnpackTriangle(verts, tri, A, B, C);

    // Unit vector A->B (and the float versions of AB and AC)
    const double abx = (double)B.x - A.x, aby = (double)B.y - A.y, abz = (double)B.z - A.z;
    const CVector abF{ (float)abx, (float)aby, (float)abz };
    const float  len = (float)std::sqrt(abx * abx + (double)abF.z * abF.z + aby * aby);
    const float  invLen = (float)(1.0 / len);
    const float  ex = (float)(abx * invLen), ey = (float)(aby * invLen), ez = abF.z * invLen;

    // M = e x N (CrossProduct: 0x59C730)
    const float mx = (float)((double)nz * ey - (double)ez * ny);
    const float my = (float)((double)ez * nx - (double)nz * ex);
    const float mz = (float)((double)ex * ny - (double)nx * ey);

    // C in the plane's coordinate system
    const double cax = (double)C.x - A.x, cay = (double)C.y - A.y, caz = (double)C.z - A.z;
    const CVector acF{ (float)cax, (float)cay, (float)caz };
    const float   Ce = (float)((cax * ex + caz * ez) + cay * ey);
    const float   Cm = (float)((cax * mx + (double)mz * caz) + (double)my * cay);

    // The sphere's center in the plane's coordinate system
    const double pax = (double)c.x - A.x, pay = (double)c.y - A.y, paz = (double)c.z - A.z;
    const float  Pe  = (float)((pax * ex + paz * ez) + pay * ey);
    const float  Pm  = (float)((pax * mx + paz * mz) + pay * my);

    // Which side of the 3 edges is the center on? (all 3 => inside of the triangle)
    int32 numPassed = 0;
    bool  abPassed = false, acPassed = false;

    const double abSide  = (double)Pm * len - (double)Pe * 0.0;
    const float  abSideF = (float)abSide;
    if (abSide >= 0.0) { // NaN fails
        numPassed++;
        abPassed = true;
    }

    const float pec = (float)((double)Pe * Cm);
    const float pmc = (float)((double)Pm * Ce);
    if ((double)pec - pmc >= 0.0) { // NaN fails
        numPassed++;
        acPassed = true;
    }

    const float ceMinusLen = Ce - len;
    const float peMinusLen = Pe - len;
    if ((double)ceMinusLen * Pm - (double)peMinusLen * Cm >= 0.0) { // NaN fails
        numPassed++;
    }

    // Distance to the closest point (`CVector::Magnitude` of `center - point`, order x, y, z), rounded to a float
    const auto DistTo = [&](const CVector& v) {
        const CVector d{ c.x - v.x, c.y - v.y, c.z - v.z };
        return (float)std::sqrt((double)d.x * d.x + (double)d.y * d.y + (double)d.z * d.z);
    };

    // `from + dir * s` using float ops (0x40FEC0 + 0x40FE30)
    const auto PointOnEdge = [](const CVector& from, const CVector& dir, float s) {
        const CVector ds{ s * dir.x, s * dir.y, s * dir.z };
        return CVector{ from.x + ds.x, from.y + ds.y, from.z + ds.z };
    };

    // Distance to the edge line + the plane distance (the exe uses the squared plane distance rounded to a float)
    const auto DistToEdgeLine = [&](double perp) {
        return (float)std::sqrt(perp * perp + planeDistSq);
    };

    CVector closest;
    float   dist;
    switch (numPassed) {
    case 3: { // Inside of the triangle => the closest point is the center projected onto the plane
        dist = absPlaneDist;
        closest = CVector{
            (float)((double)c.x - (double)nx * planeDist),
            (float)((double)c.y - (double)ny * planeDist),
            c.z - nz * planeDist
        };
        break;
    }
    case 2: {
        if (!abPassed) { // Edge AB
            const float abLenSq = (float)((double)len * len);
            const double sE     = ((double)Pe * len + (double)Pm * 0.0) / abLenSq; // Compared unrounded with 0, but spilled to a float for the rest
            const float  s      = (float)sE;
            if (sE <= 0.0) {
                closest = A;
                dist    = DistTo(A);
            } else if (s >= 1.f) {
                closest = B;
                dist    = DistTo(B);
            } else {
                dist    = DistToEdgeLine((double)abSideF / std::sqrt((double)abLenSq));
                closest = PointOnEdge(A, abF, s);
            }
        } else if (!acPassed) { // Edge AC
            const float acLenSq = (float)((double)Cm * Cm + (double)Ce * Ce);
            const double sE     = ((double)Pm * Cm + (double)Pe * Ce) / acLenSq;
            const float  s      = (float)sE;
            if (sE <= 0.0) {
                closest = A;
                dist    = DistTo(A);
            } else if (s >= 1.f) {
                closest = C;
                dist    = DistTo(C);
            } else {
                dist    = DistToEdgeLine(((double)pmc - pec) / std::sqrt((double)acLenSq));
                closest = PointOnEdge(A, acF, s);
            }
        } else { // Edge BC, from B to C: (Ce - len, Cm)
            const float bcX = ceMinusLen, bcY = Cm;
            const float bcLenSq = (float)((double)bcY * bcY + (double)bcX * bcX);
            const double sE     = ((double)peMinusLen * bcX + (double)Pm * bcY) / bcLenSq;
            const float  s      = (float)sE;
            if (sE <= 0.0) {
                closest = B;
                dist    = DistTo(B);
            } else if (s >= 1.f) {
                closest = C;
                dist    = DistTo(C);
            } else {
                dist = DistToEdgeLine(((double)Pm * bcX - (double)peMinusLen * bcY) / std::sqrt((double)bcLenSq));
                const CVector bc{ C.x - B.x, C.y - B.y, C.z - B.z };
                closest = PointOnEdge(B, bc, s);
            }
        }
        break;
    }
    case 1: // Closest to a vertex
        closest = abPassed ? C : acPassed ? B : A;
        dist    = DistTo(closest);
        break;
    default: // 0: Not possible geometrically. NOTSA: The original uses uninitialized data here
        return false;
    }

    const float distSq = (float)((double)dist * dist);
    if (!(dist < r) || !(distSq < maxTouchDistance)) { // NaN => false
        return false;
    }

    CVector normal = c - closest;
    normal.Normalise();

    colPoint.m_vecPoint  = closest;
    colPoint.m_vecNormal = normal;

    // NOTE: Lighting B is not written by the original
    colPoint.m_nSurfaceTypeA = sphere.m_Surface.m_nMaterial;
    colPoint.m_nPieceTypeA   = sphere.m_Surface.m_nPiece;
    colPoint.m_nLightingA    = sphere.m_Surface.m_nLighting;
    colPoint.m_nSurfaceTypeB = tri.m_nMaterial;
    colPoint.m_nPieceTypeB   = 0;
    colPoint.m_fDepth        = r - dist;

    maxTouchDistance = distSq;

    return true;
}

/*!
* @addr 0x417730
* @brief Is there anything (that isn't see-/shoot-through if requested) on the line? Everything is tested in the model's space.
*/
bool CCollision::TestLineOfSight(const CColLine& lnws, const CMatrix& transform, CColModel& cm, bool doSeeThroughCheck, bool doShootThroughCheck) {
    ZoneScoped;

    const auto cd = cm.m_pColData;
    if (!cd) {
        return false;
    }

    // Transform line into object space
    const auto invTransform = Invert(transform);
    const CColLine lnos{ TransformPointOG(invTransform, lnws.m_vecStart), TransformPointOG(invTransform, lnws.m_vecEnd) };

    // If we don't intersect with the bounding box, no chance on the rest
    if (!TestLineBox_DW(lnos, cm.GetBoundingBox())) {
        return false;
    }

    // Original: surfaces that are see-/shoot-through are SKIPPED (if the corresponding check is requested)
    const auto ShouldSkip = [=](eSurfaceType surf) {
        return (doSeeThroughCheck && g_surfaceInfos.IsSeeThrough(surf))
            || (doShootThroughCheck && g_surfaceInfos.IsShootThrough(surf));
    };

    for (auto i = 0; i < cd->m_nNumSpheres; i++) {
        const auto& sphere = cd->m_pSpheres[i];
        if (!ShouldSkip(sphere.m_Surface.m_nMaterial) && TestLineSphere(lnos, sphere)) {
            return true;
        }
    }

    for (auto i = 0; i < cd->m_nNumBoxes; i++) {
        const auto& box = cd->m_pBoxes[i];
        if (!ShouldSkip(box.m_Surface.m_nMaterial) && TestLineBox_DW(lnos, box)) {
            return true;
        }
    }

    CalculateTrianglePlanes(cd);

    for (auto i = 0; i < cd->m_nNumTriangles; i++) {
        const auto& tri = cd->m_pTriangles[i];
        if (!ShouldSkip(tri.m_nMaterial) && TestLineTriangle(lnos, cd->m_pVertices, tri, cd->m_pTrianglePlanes[i])) {
            return true;
        }
    }

    return false;
}

// 0x417950
bool CCollision::ProcessLineOfSight(const CColLine& lnws, const CMatrix& transform, CColModel& colModel, CColPoint& colPoint, float& maxTouchDistance, bool doSeeThroughCheck,
                                    bool doShootThroughCheck) {
    ZoneScoped;

    const auto colData = colModel.m_pColData;
    if (!colData) {
        return false;
    }

    // Transform line into object space
    const auto invTransform = Invert(transform);
    const CColLine line_OS{ TransformPointOG(invTransform, lnws.m_vecStart), TransformPointOG(invTransform, lnws.m_vecEnd) };

    if (!TestLineBox_DW(line_OS, colModel.GetBoundingBox())) {
        return false;
    }

    // Original: surfaces that are see-/shoot-through are SKIPPED (if the corresponding check is requested)
    const auto ShouldSkip = [=](eSurfaceType surf) {
        return (doSeeThroughCheck && g_surfaceInfos.IsSeeThrough(surf))
            || (doShootThroughCheck && g_surfaceInfos.IsShootThrough(surf));
    };

    float localMinTouchDist = maxTouchDistance;

    // NOTE: The results of the Process* functions are ignored, `localMinTouchDist` is what matters
    for (auto i = 0; i < colData->m_nNumSpheres; i++) {
        const auto& sphere = colData->m_pSpheres[i];
        if (!ShouldSkip(sphere.m_Surface.m_nMaterial)) {
            ProcessLineSphere(line_OS, sphere, colPoint, localMinTouchDist);
        }
    }

    for (auto i = 0; i < colData->m_nNumBoxes; i++) {
        const auto& box = colData->m_pBoxes[i];
        if (!ShouldSkip(box.m_Surface.m_nMaterial)) {
            ProcessLineBox(line_OS, box, colPoint, localMinTouchDist);
        }
    }

    CalculateTrianglePlanes(colData);

    for (auto i = 0; i < colData->m_nNumTriangles; i++) {
        const auto& tri = colData->m_pTriangles[i];
        if (!ShouldSkip(tri.m_nMaterial)) {
            if (ProcessLineTriangle(line_OS, colData->m_pVertices, tri, colData->m_pTrianglePlanes[i], colPoint, localMinTouchDist, nullptr)) {
                ms_iProcessLineNumCrossings++;
            }
        }
    }

    if (!(localMinTouchDist < maxTouchDistance)) { // NaN => false
        return false;
    }

    colPoint.m_vecPoint  = TransformPointOG(transform, colPoint.m_vecPoint);
    colPoint.m_vecNormal = TransformVectorOG(transform, colPoint.m_vecNormal);
    maxTouchDistance = localMinTouchDist;
    return true;
}

// 0x417BF0
bool CCollision::ProcessVerticalLine(
    const CColLine& lnws,
    const CMatrix& transform,
    CColModel& cm,
    CColPoint& cp,
    float& maxTouchDistance,
    bool doSeeThroughCheck,
    bool doShootThroughCheck,
    CStoredCollPoly* outColPoly
) {
    ZoneScoped;

    const auto cd = cm.GetData();
    if (!cd) {
        return false;
    }

    // Transform line to object space (NOTE: The line is NOT forced to be vertical in object space, and `Invert` isn't used)
    const CColLine lnos{ InverseTransformPointOG(transform, lnws.m_vecStart), InverseTransformPointOG(transform, lnws.m_vecEnd) };

    if (!TestLineBox_DW(lnos, cm.GetBoundingBox())) {
        return false;
    }

    float localMaxTouchDist = maxTouchDistance;

    // Original: see-through surfaces are SKIPPED (shoot-through is ignored, `doShootThroughCheck` is unused)
    const auto ShouldSkip = [=](eSurfaceType surf) {
        return doSeeThroughCheck && g_surfaceInfos.IsSeeThrough(surf);
    };

    for (auto i = 0; i < cd->m_nNumSpheres; i++) {
        const auto& sphere = cd->m_pSpheres[i];
        if (!ShouldSkip(sphere.m_Surface.m_nMaterial)) {
            ProcessLineSphere(lnos, sphere, cp, localMaxTouchDist);
        }
    }

    for (auto i = 0; i < cd->m_nNumBoxes; i++) {
        const auto& box = cd->m_pBoxes[i];
        if (!ShouldSkip(box.m_Surface.m_nMaterial)) {
            ProcessLineBox(lnos, box, cp, localMaxTouchDist);
        }
    }

    // Lastly, triangles
    CalculateTrianglePlanes(cd);

    // Original: static (the `valid` flag is reset on every call, the rest is only read if it's set)
    static auto& storedColPoly = StaticRef<CStoredCollPoly>(0x9659FC);
    storedColPoly.valid = false;

    for (auto i = 0; i < cd->m_nNumTriangles; i++) {
        const auto& tri = cd->m_pTriangles[i];
        if (!ShouldSkip(tri.m_nMaterial)) {
            ProcessLineTriangle(lnos, cd->m_pVertices, tri, cd->m_pTrianglePlanes[i], cp, localMaxTouchDist, &storedColPoly);
        }
    }

    if (!(localMaxTouchDist < maxTouchDistance)) { // NaN => false
        return false; // No collisions closer to line origin than originally
    }

    // Transform back from object space
    cp.m_vecPoint  = TransformPointOG(transform, cp.m_vecPoint);
    cp.m_vecNormal = TransformVectorOG(transform, cp.m_vecNormal);

    if (storedColPoly.valid && outColPoly) {
        *outColPoly = storedColPoly;
        for (auto& vtx : outColPoly->verts) {
            vtx = TransformPointOG(transform, vtx); // Transform back from object space
        }
    }

    maxTouchDistance = localMaxTouchDist;
    return true;
}

// 0x417F20
bool CCollision::SphereCastVsSphere(const CColSphere& spA, const CColSphere& spB, const CColSphere& spS) {
    ZoneScoped;

    // Original (0x417F20): the 1st test is an inlined TestSphereSphere (term order x, y, z)
    {
        const double dx = (double)spA.m_vecCenter.x - spS.m_vecCenter.x;
        const double dy = (double)spA.m_vecCenter.y - spS.m_vecCenter.y;
        const double dz = (double)spA.m_vecCenter.z - spS.m_vecCenter.z;
        const double sumR = (double)spS.m_fRadius + spA.m_fRadius;
        if (sumR * sumR > dx * dx + dz * dz + dy * dy) { // Term order: x, z, y
            return true;
        }
    }
    if (TestSphereSphere(spB, spS)) {
        return true;
    }
    return CCollision::TestLineSphere(
        { spA.m_vecCenter, spB.m_vecCenter },
        { spS.m_vecCenter, spS.m_fRadius + spA.m_fRadius }
    );
}

// 0x418100 // unused
// `arg0` is unused, `tri` points to the 3 vertices, `out` receives the closest point on each of the 3 edges (v1-v0, v2-v1, v0-v2)
void CCollision::ClosestPointsOnPoly(CColTriangle* arg0, CVector* tri, CVector* point, CVector* out) {
    ZoneScoped;

    ClosestPointOnLine(*point, tri[1], tri[0], out[0]);
    ClosestPointOnLine(*point, tri[2], tri[1], out[1]);
    ClosestPointOnLine(*point, tri[0], tri[2], out[2]);
}

// 0x418150 // unused
// `arg0` is unused, `tri` points to the 3 vertices, `pt` is overwritten with the closest point on the triangle's edges
void CCollision::ClosestPointOnPoly(CColTriangle* arg0, CVector* tri, CVector* pt) {
    ZoneScoped;

    CVector closest[3];
    ClosestPointOnLine(*pt, tri[1], tri[0], closest[0]);
    ClosestPointOnLine(*pt, tri[2], tri[1], closest[1]);
    ClosestPointOnLine(*pt, tri[0], tri[2], closest[2]);
    Closest3(closest, pt);
}

// 0x418580
void CCollision::CalculateTrianglePlanes(CColModel* colModel) {
    ZoneScoped;

    if (colModel->m_pColData) {
        CalculateTrianglePlanes(colModel->m_pColData);
    }
    if (colModel->m_pColData && colModel->m_pColData->m_pTriangles) {
        assert(colModel->m_pColData->m_pTrianglePlanes); // If model has triangles it should also have triPls by now (otherwise random crashes will occour)
    }
}

// 0x4185A0
void CCollision::RemoveTrianglePlanes(CColModel* colModel) {
    if (colModel->m_pColData) {
        RemoveTrianglePlanes(colModel->m_pColData);
    }
}

// TODO: This function could be refactored to use ranges instead of these ugly static variables :D
/*!
 * @brief 0x4185C0 Calculate collisions between \a cmA and \a cmB.
 *
 * Collisions calculated:
 *  A\B  Tri Sp  Box Lines
 * Tri       +
 * Sp    +   +   +
 * Box       +
 * Lines +   +   +
 *
 * Note: Originally the game calculated some disk stuff as well, but SA doesn'maxTouchDist use disks (they're disabled when loadedin CFileLoader)
 * thus I omitted the code for it (it would've made the already messy code even worse)
 *
 * @param         transformA           Transformation matrix of model A - Usually owning entity's matrix.
 * @param         cmA                  Col model A
 * @param         transformB           Transformation matrix of model B - Usually owning entity's matrix.
 * @param         cmA                  Col model B
 * @param[out]    lineCPs              Line collision points (At most 16 - It can be null if you're sure the model has no lines)
 * @param[out]    sphereCPs            Sphere collision points (At most 32)
 * @param[in,out] maxTouchDistances    Only used if model has lines - If you're sure it has none it can be null. It has to be an array of the same size as the number of lines .
 * @param         bReturnAllCollisions              
 *
 * @returns Number of sphere collision points found (At most 31 = `std::size(sphereCPs) - 1`; each
 *          accepted collision also pre-arms the *next* array slot's `m_fDepth` to `-1.f` as a
 *          "not-yet-written" sentinel, so one trailing index must always stay writeable)
 */
int32 CCollision::ProcessColModels(const CMatrix& transformA, CColModel& cmA,
    const CMatrix& transformB, CColModel& cmB,
    std::array<CColPoint, 32>& sphereCPs,
    CColPoint* lineCPs,
    float* maxTouchDistances,
    bool bReturnAllCollisions
) {
    ZoneScoped;

    // Don't think this should ever happen, but okay?
    if (!cmA.m_pColData) {
        return 0;
    }

    if (!cmB.m_pColData) {
        return 0;
    }

    const auto& cdA = *cmA.m_pColData;
    const auto& cdB = *cmB.m_pColData;

    // Highest sphere-CP count the original emits. `1` is subtracted from `std::size(sphereCPs)`
    // because each accepted collision also writes `m_fDepth = -1.f` into the *next* slot
    // (`sphereCPs[nNumSphereCPs + 1].m_fDepth`) as a "not-yet-written" sentinel - one trailing
    // index has to stay in-bounds for that.
    const auto maxSphereCPs = sphereCPs.size() - 1;

    // Transform matrix from A's space to B's
    const auto transformAtoB = Invert(transformB) * transformA;

    // A's bounding bb in B's space
    const CColSphere colABoundSphereSpaceB{ TransformPointOG(transformAtoB, cmA.m_boundSphere.m_vecCenter), cmA.m_boundSphere.m_fRadius };

    if (!TestSphereBox(colABoundSphereSpaceB, cmB.m_boundBox)) {
        return 0;
    }

    // Transform matrix from B's space to A's
    const auto transformBtoA = Invert(transformA) * transformB;

    // Now, transform each cm's spheres and test them against each other's bounding bb

    // TODO: Should probably move these out somewhere..
    constexpr auto MAX_SPHERES{ 128u }; // Max no. of spheres colliding with other model's bounding sphere. - If more - Possible crash
    constexpr auto MAX_BOXES{ 64u };    // Same, but for boxes      - If more, all following are ignored.
    constexpr auto MAX_TRIS{ 600u };    // Same, but for triangles  - If more, all following are ignored.
    constexpr auto MAX_LINES{ 16u };    // Game didn't originally check for this, so I assume no models ever have more than 16 lines.
    constexpr auto TRIS_LIMIT{ 0x257u }; // The exe stops collecting triangles once it has 599 (`cmp eax, 0x257 / jge`), the arrays hold 600

    // Transform `spheres` center position using `transform` and store them in `outSpheres`
    const auto TransformSpheres = []<size_t n>(auto&& spheres, const CMatrix& transform, CColSphere(&outSpheres)[n]) {
        std::ranges::transform(spheres, outSpheres, [&](const auto& sp) {
            CColSphere transformed  = sp;                                       // Copy sphere
            transformed.m_vecCenter = TransformPointOG(transform, sp.m_vecCenter); // Set copy's center as the transformed point
            return transformed;
        });
    };

    // Test `spheres` against bounding bb `bb` and store all colliding sphere's indices in `collidedIdxs`
    const auto TestSpheresAgainstBB = []<size_t n>(auto&& spheres, const auto& bb, uint32& numCollided, uint32(&collidedIdxs)[n]) {
        for (const auto& [triIdx, sp] : rngv::enumerate(spheres)) {
            if (TestSphereBox(sp, bb)) {
                assert(numCollided < n); // Avoid out-of-bounds (Game originally didn't check)
                collidedIdxs[numCollided++] = (uint32)triIdx;
            }
        }
    };

    // Transform both model's spheres into each other's space
    // then store all sphere's indices colliding with the other's bounding bb

    // Process A

    // 0x418722
    static CColSphere sphA[MAX_SPHERES];
    TransformSpheres(cdA.GetSpheres(), transformAtoB, sphA);

    // 0x4187A1
    static uint32 collSphA[MAX_SPHERES];
    uint32        numCollSphA{};
    TestSpheresAgainstBB(std::span{ sphA, cdA.m_nNumSpheres }, cmB.GetBoundingBox(), numCollSphA, collSphA);

    // Process B

    // 0x4187ED
    static CColSphere sphB[MAX_SPHERES];
    TransformSpheres(cdB.GetSpheres(), transformBtoA, sphB);

    // 0x418862
    static uint32 collSphB[MAX_SPHERES];
    uint32        numCollSphB{};
    TestSpheresAgainstBB(std::span{ sphB, cdB.m_nNumSpheres }, cmA.GetBoundingBox(), numCollSphB, collSphB);

    if (!numCollSphA && !cdA.m_nNumLines && !numCollSphB) {
        return 0;
    }

    // 0x418902
    // B's disks: transformed into A's space right after B's regular spheres (`sphB[numSpheres + i]`), and the ones touching A's
    // bounding box are counted in `numDisksB`. QUIRK (reproduced): the exe writes the indices of those disks into the list
    // `collSphB` starting at its FIRST slot (a pointer reset to the list start, not appended after the real sphere hits) and
    // leaves `numCollSphB` untouched. The trailing "B-spheres vs A-tris/boxes" block iterates `numCollSphB - numDisksB` entries of
    // that list (0x419B91: `[esp+0x48] - [esp+0x50]`). Nothing else ever looks at B's disks.
    uint32 numDisksB{};
    if (cdB.bUsesDisks && cdB.m_nNumLines) {
        for (auto diskIdx = 0u; diskIdx < cdB.m_nNumLines; diskIdx++) {
            const auto& disk{ cdB.m_pDisks[diskIdx] };

            auto& sph{ sphB[cdB.m_nNumSpheres + diskIdx] };
            sph.m_vecCenter = TransformPointOG(transformBtoA, disk.m_vecCenter);
            sph.m_fRadius   = disk.m_fRadius;
            sph.m_Surface   = disk.m_Surface;
        }
        for (auto diskIdx = 0u; diskIdx < cdB.m_nNumLines; diskIdx++) {
            if (TestSphereBox(sphB[cdB.m_nNumSpheres + diskIdx], cmA.GetBoundingBox())) {
                collSphB[numDisksB++] = diskIdx;
                if (numDisksB >= MAX_SPHERES) { // 0x418A29: `cmp edi, 0x967340`
                    break;
                }
            }
        }
    }
    // Number of entries of `collSphB` the trailing block (B's spheres vs A's triangles/boxes) walks
    const int32 numCollSphBTail = (int32)numCollSphB - (int32)numDisksB;

    // 0x418A4E
    // Test B's boxes against A's bounding sphere
    static uint32 collBoxB[MAX_BOXES]; // Indices of B's boxes colliding with A's bounding sphere
    uint32        numCollBoxB{};
    // 0x418A54: this inlined copy of TestSphereBox spills the first sum (`x + radius`) to a float temporary, the other five tests stay unrounded
    const auto TestSphereBoxSpilledMinX = [](const CSphere& sph, const CBox& box) {
        const auto& c = sph.m_vecCenter;
        const auto  r = sph.m_fRadius;
        return !((double)(float)((double)c.x + r) < box.m_vecMin.x
              || (double)c.x - r > box.m_vecMax.x
              || (double)c.y + r < box.m_vecMin.y
              || (double)c.y - r > box.m_vecMax.y
              || (double)c.z + r < box.m_vecMin.z
              || (double)c.z - r > box.m_vecMax.z);
    };
    for (auto&& [triIdx, bb] : rngv::enumerate(cdB.GetBoxes())) {
        if (TestSphereBoxSpilledMinX(colABoundSphereSpaceB, bb)) {
            collBoxB[numCollBoxB++] = triIdx;
            if (numCollBoxB >= MAX_BOXES) {
                break;
            }
        }
    }

    // 0x418B0F
    // Test B's triangles against A's bounding sphere
    static uint32 collTrisB[MAX_TRIS];
    uint32        numCollTrisB{};

    if (cdB.m_nNumTriangles) {
        CalculateTrianglePlanes(&cmB); // Moved check inside if (Doesn't make a difference practically)
        assert(cdB.m_pTrianglePlanes);

        // Process a single triangle, returns true if it was added to the list
        const auto ProcessOneTri = [&](uint32 triIdx) {
            if (TestSphereTriangle(colABoundSphereSpaceB, cdB.m_pVertices, cdB.m_pTriangles[triIdx], cdB.m_pTrianglePlanes[triIdx])) {
                collTrisB[numCollTrisB++] = triIdx;
                return true;
            }
            return false;
        };

        if (cdB.bHasFaceGroups) { // Test by using face groups - Thanks to those who helped me figure this out :)
            // 0x418B23
            // The exe walks the groups from the one nearest to the triangles downwards (group `i` lives at `m_pTriangles - 4 - 28 * (i + 1)`),
            // i.e. in REVERSE of the memory order `GetFaceGroups()` returns => the order of the collected triangles (and so of the
            // contact points when all collisions are returned) depends on it.
            for (auto&& group : rngv::reverse(cdB.GetFaceGroups())) {
                if (TestSphereBox(colABoundSphereSpaceB, group.bb)) {      // Quick BB check
                    for (auto triIdx{ (int32)(int16)group.first }; triIdx <= (int32)(int16)group.last; triIdx++) { // Check all triangles in this group (signed 16 bit, `movsx`)
                        if (ProcessOneTri((uint32)triIdx) && numCollTrisB >= TRIS_LIMIT) { // Only checked after a hit; only leaves this group
                            break;
                        }
                    }
                }
            }
        } else { // Game checked here if B.m_nNumTriangles > 0, but that is a redundant check.
            // 0x418C40
            for (auto triIdx = 0u; triIdx < cdB.m_nNumTriangles && numCollTrisB < TRIS_LIMIT; triIdx++) {
                ProcessOneTri(triIdx);
            }
        }
    }

    if (!numCollSphB && !numCollBoxB && !numCollTrisB) {
        return 0;
    }

    // 0x418CAD
    // Process all of A's colliding spheres against all of B's colliding spheres, boxes and triangles
    sphereCPs[0].m_fDepth = -1.f;
    uint32 nNumSphereCPs{};
    if (numCollSphA) {
        for (auto sphereAIdx : std::span{ collSphA, numCollSphA }) {
            assert(cdA.m_pSpheres);
            assert(sphereAIdx < cdA.m_nNumSpheres);
            const auto& sphereA{ sphA[sphereAIdx] }; // Sphere in B's space

            bool  advanceColPointIdx{};
            float minTouchDist{ 1e24f };

            // 0x418CF9
            // Spheres
            for (auto sphereBIdx : std::span{ collSphB, numCollSphB }) {
                // 0x418D00: indices past the regular spheres (only possible through the disk-index quirk, see above) are disks, if any
                if (sphereBIdx >= cdB.m_nNumSpheres && !(cdB.bUsesDisks && cdB.m_nNumLines)) {
                    continue;
                }
                const auto& sphereB{ sphereBIdx < cdB.m_nNumSpheres ? cdB.m_pSpheres[sphereBIdx] : static_cast<const CColSphere&>(cdB.m_pDisks[sphereBIdx - cdB.m_nNumSpheres]) };

                if (ProcessSphereSphere(sphereA, sphereB, sphereCPs[nNumSphereCPs], minTouchDist)) {
                    advanceColPointIdx = true;
                    // These tests are cheap, so continue processing
                }
            }

            // 0x418D86
            // Boxes
            for (auto boxIdx : std::span{ collBoxB, numCollBoxB }) {
                const auto& bb{ cdB.m_pBoxes[boxIdx] };
                auto&       cp = sphereCPs[nNumSphereCPs];

                if (ProcessSphereBox(sphereA, bb, cp, minTouchDist)) {
                    // Original: ProcessSphereBox itself sets all surface fields (A=sphere, B=box);
                    // the call-site assignments here in the original are redundant re-copies.
                    if (bReturnAllCollisions && sphereA.m_Surface.m_nPiece <= 2 && nNumSphereCPs < maxSphereCPs) { // Original: `v61 < 31` (must leave room for +1 sentinel)
                        advanceColPointIdx                    = false;
                        minTouchDist                          = 1e24f;
                        sphereCPs[nNumSphereCPs + 1].m_fDepth = -1.f;
                        nNumSphereCPs++;
                    } else {
                        advanceColPointIdx = true;
                    }
                }
            }

            // 0x418E44
            // Triangles
            for (auto triIdx : std::span{ collTrisB, numCollTrisB }) {
                auto& cp = sphereCPs[nNumSphereCPs];

                if (ProcessSphereTriangle(sphereA, cdB.m_pVertices, cdB.m_pTriangles[triIdx], cdB.m_pTrianglePlanes[triIdx], cp, minTouchDist)) {
                    if (bReturnAllCollisions && sphereA.m_Surface.m_nPiece <= 2 && nNumSphereCPs < maxSphereCPs) { // Original: `v61 < 31` (must leave room for +1 sentinel)
                        advanceColPointIdx                    = false;
                        minTouchDist                          = 1e24f;
                        sphereCPs[nNumSphereCPs + 1].m_fDepth = -1.f;
                        nNumSphereCPs++;
                    } else {
                        advanceColPointIdx = true;
                    }
                }
            }

            // 0x418EFC
            if (advanceColPointIdx) {
                if (nNumSphereCPs >= maxSphereCPs) { // Original: `if (v61 >= 31) break;` caps total at 31 CPs
                    break;
                }
                ++nNumSphereCPs;
                sphereCPs[nNumSphereCPs].m_fDepth = -1.f; // Set next (future) CP's depth sentinel
            }
        }

        // 0x41996E
        // Transform all colpoints into world space (Originally not here)
        for (auto&& cp : std::span{ sphereCPs.data(), nNumSphereCPs }) {
            cp.m_vecPoint  = TransformPointOG(transformB, cp.m_vecPoint);
            cp.m_vecNormal = TransformVectorOG(transformB, cp.m_vecNormal);
        }
    }

    // Track how many sphere CPs existed BEFORE the disk/line loop starts, so we can
    // post-transform any CPs the DISK path (via `emitCP`) appends afterwards. The
    // original performs world-transforms inline during the disk loop's emit handling;
    // our impl mirrors that with a single post-pass over the disk-only tail.
    const auto diskPhaseCPBegin = nNumSphereCPs;

    // 0x418F4C (disk path) / 0x4196B9 (line path)
    // Test all of A's lines (or disks) against all of B's colliding spheres, boxes and triangles,
    // and store colpoints for all lines (even if they didn't collide)
    // (I really don't understand how the caller will know which lines have collided?)
    if (cdA.m_nNumLines) {
        assert(maxTouchDistances);
        assert(lineCPs);
        assert(cdA.m_nNumLines <= MAX_LINES);

        // 0x419731 - Moved logic into loop (storing all lines in a separate array isn't necessary at all)
        // 0x419752 - Skipped this, as it just filled an array with 1:1 index mapping - Useless - They probably had some BB checking logic here?

        // Disk contact height: `sqrt(r^2 - dy^2 - dx^2) + z`, all of it unrounded on the x87 stack and NOT clamped (a negative radicand gives NaN
        // and then fails the comparisons below), the differences are not rounded to float either (0x4191C9, 0x419364)
        const auto DiskHitK = [](const CVector& cpInA, const CColDisk& disk) -> double {
            const double dy = (double)cpInA.y - disk.m_vecCenter.y;
            const double dx = (double)cpInA.x - disk.m_vecCenter.x;
            const double r  = disk.m_fRadius;
            return std::sqrt(r * r - dy * dy - dx * dx) + cpInA.z;
        };

        if (cdA.bUsesDisks) { // 0x418F5B

            // Mirror the original's quirk: the binary *writes* the caller's
            // seed touchDist into disk.m_vecCenter.z of the LIVE colModel and never
            // restores it. This poison MUST happen BEFORE the disk spheres are built,
            // because the original transforms the already-poisoned disk center into
            // `sphA` (0x418F5B poisons, THEN 0x418FBx transforms). All subsequent hitK / slab
            // tests inside PDC also read this poisoned .z. (The leak is observable
            // state - callers depend on it being left in place.)
            for (auto diskIdx = 0u; diskIdx < cdA.m_nNumLines; diskIdx++) {
                cdA.m_pDisks[diskIdx].m_vecCenter.z = maxTouchDistances[diskIdx];
            }

            // Append each disk's sphere (transformed to B's space) to the tail of `sphA` after the regular spheres.
            // NOTE: `disk.m_vecCenter.z` is already poisoned above, so `sphereAinB` is placed at the
            // caller's seed height - exactly as the binary does it.
            assert(cdA.m_nNumSpheres + cdA.m_nNumLines <= MAX_SPHERES);
            for (auto diskIdx = 0u; diskIdx < cdA.m_nNumLines; diskIdx++) {
                const auto& disk{ cdA.m_pDisks[diskIdx] };

                auto& sph{ sphA[cdA.m_nNumSpheres + diskIdx] };
                sph.m_vecCenter = TransformPointOG(transformAtoB, disk.m_vecCenter);
                sph.m_fRadius   = disk.m_fRadius;
                sph.m_Surface   = disk.m_Surface;
            }

            for (auto diskIdx = 0u; diskIdx < cdA.m_nNumLines; diskIdx++) {
                const auto& disk{ cdA.m_pDisks[diskIdx] };
                const auto& sphereAinB{ sphA[cdA.m_nNumSpheres + diskIdx] };

                auto& thisLineCP{ lineCPs[diskIdx] };
                auto& cp{ sphereCPs[nNumSphereCPs] };

                bool  emitCP{};
                bool  lineCollision{};
                float bestBoxDist{ 1e24f }; // `v205` closest accepted box touchDistSq (only for accepted-as-CP box hits)

                // Original resets the disk's CP depth sentinel EVERY iteration (0x418F2E / *local_d4 = 0xBF800000).
                cp.m_fDepth = -1.0f;

                // Quick reject this disk against B's bounding box
                if (TestSphereBox(sphereAinB, cmB.GetBoundingBox())) {
                    // Spheres: accepted hits only produce a LINE contact (like wheels pressing down)
                    for (auto sphBIdx : std::span{ collSphB, numCollSphB }) {
                        const auto& sphereB{ cdB.m_pSpheres[sphBIdx] }; // 0x419159: no disk lookup here

                        float minTouchDist{ 1e24f };
                        if (ProcessSphereSphere(sphereAinB, sphereB, cp, minTouchDist)) {
                            const auto cpInA{ TransformPointOG(transformBtoA, cp.m_vecPoint) };
                            const auto hitK = DiskHitK(cpInA, disk);
                            if (maxTouchDistances[diskIdx] <= hitK) { // Original: `!(hitK < max)`, NaN rejects
                                maxTouchDistances[diskIdx] = (float)hitK;
                                thisLineCP                 = cp;
                                thisLineCP.m_nSurfaceTypeA = disk.m_Surface.m_nMaterial;
                                thisLineCP.m_nPieceTypeA   = disk.m_Surface.m_nPiece;
                                thisLineCP.m_nLightingA    = disk.m_Surface.m_nLighting;
                                lineCollision              = true;
                            }
                        }
                    }

                    // Boxes
                    for (auto boxIdx : std::span{ collBoxB, numCollBoxB }) {
                        const auto& bb{ cdB.m_pBoxes[boxIdx] };
                        float       minTouchDist{ 1e24f };

                        if (ProcessSphereBox(sphereAinB, bb, cp, minTouchDist)) {
                            const auto cpInA     = TransformPointOG(transformBtoA, cp.m_vecPoint);
                            const auto normalInA = TransformVectorOG(transformBtoA, cp.m_vecNormal);

                            if (!(cpInA.z < disk.m_vecCenter.z) || !(normalInA.z > 0.5f)) { // NaN => first branch (0x419346, 0x41935E)
                                // CP candidate: become the emitted CP only if closer than the previous best
                                if (minTouchDist < bestBoxDist) {
                                    emitCP      = true;
                                    bestBoxDist = minTouchDist;
                                }
                            } else {
                                // Below the disk's plane & normal mostly vertical: project onto the disk plane & treat as line (wheel) hit
                                const auto hitK = DiskHitK(cpInA, disk);

                                if (maxTouchDistances[diskIdx] <= hitK) { // Original: `!(hitK < max)`, NaN rejects
                                    if (hitK - (double)maxTouchDistances[diskIdx] > (double)0.35f * disk.m_fRadius) { // 0x858C0C = 0.35f
                                        emitCP      = true;
                                        bestBoxDist = minTouchDist;
                                    }
                                    lineCollision              = true;
                                    maxTouchDistances[diskIdx] = (float)hitK;
                                    thisLineCP                 = cp;
                                    thisLineCP.m_nSurfaceTypeA = disk.m_Surface.m_nMaterial;
                                    thisLineCP.m_nPieceTypeA   = disk.m_Surface.m_nPiece;
                                    thisLineCP.m_nLightingA    = disk.m_Surface.m_nLighting;
                                }
                            }
                        }
                    }

                    // Triangles (via ProcessDiscCollision does the disk-plane projection inside)
                    // Original (IDA): `qmemcpy(v230, v80, sizeof(v230))` copies the working sphere/box
                    // CP into the scratch `v230` ONCE, before the loop. Then each iteration resets ONLY
                    // the depth field (`v230[10] = -1.0f`, offset 0x28 = m_fDepth) - the point/normal/
                    // surface of the scratch ACCUMULATE across triangles. `v204` (maxTouchDist) stays at
                    // 1e24 from the last box-iteration reset and accumulates (min race) across triangles.
                    //
                    // Do NOT re-copy the whole working CP each iteration: that both destroys the
                    // accumulation the original relies on AND seeds m_fDepth with the box/sphere depth
                    // instead of -1, which flips ProcessSphereTriangle/ProcessDiscCollision and emits
                    // spurious sphere CPs (observed as extra collision points that kill the wheel bounce).
                    CColPoint triCP{ cp };            // `v230` - seeded from the working CP ONCE (0x419236)
                    float     diskCPMaxDist{ 1e24f }; // `v204`
                    for (auto triIdx : std::span{ collTrisB, numCollTrisB }) {
                        triCP.m_fDepth = -1.0f; // 0x41931B - reset ONLY the depth sentinel; the rest accumulates
                        if (ProcessSphereTriangle(sphereAinB, cdB.m_pVertices, cdB.m_pTriangles[triIdx], cdB.m_pTrianglePlanes[triIdx], triCP, diskCPMaxDist)) {
                            // Original: when the wheel rests on a roughly-horizontal face of an upright B (e.g. the roof
                            // of a car still on its wheels), override the contact normal with the triangle's PLANE normal
                            // before PDC. PDC's `|cpNormal * m_vThickness| < 0.77` test then classifies it as a wheel-tread
                            // (line) hit rather than a disk-edge hit, letting the truck settle onto and climb the car.
                            // Constants: 0.3 (0x858c24), 0.9 (0x858c20); B's up.z is transformB matrix offset 0x28.
                            const auto planeNormal = cdB.m_pTrianglePlanes[triIdx].GetNormal();
                            if (planeNormal.z > 0.3f && transformB.GetUp().z > 0.9f) {
                                triCP.m_vecNormal = planeNormal;
                            }
                            if (ProcessDiscCollision(triCP, transformBtoA, disk, cp, lineCollision, maxTouchDistances[diskIdx], thisLineCP)) {
                                emitCP = true;
                            }
                        }
                    }
                }

                if (emitCP && nNumSphereCPs < maxSphereCPs) {
                    ++nNumSphereCPs;
                    sphereCPs[nNumSphereCPs].m_fDepth = -1.f; // Set next (future) CP's depth sentinel
                }


                if (lineCollision) {
                    thisLineCP.m_vecPoint  = TransformPointOG(transformB, thisLineCP.m_vecPoint);
                    thisLineCP.m_vecNormal = TransformVectorOG(transformB, thisLineCP.m_vecNormal);
                } else if (TestSphereBox(sphereAinB, cmB.GetBoundingBox())) {
                    // bbox passed but no sphere/box/tri produced a line contact:
                    // original writes -1e8f here (tells caller the line is entirely miss).
                    // (When the outer bbox test fails, no write happens at all the
                    // caller's seed value is preserved. This matches FUN_004185c0
                    // decompile at `*local_e4 = -1e+08;` only inside the bbox-success
                    // branch.)
                    maxTouchDistances[diskIdx] = -1e8f;
                }
            }

            // NOTE: we intentionally do NOT restore disk.center.z the original leaks it.

            // 0x418F5B (disk emitCPs are left in B's local space the first-wave
            // world-transform ran BEFORE this loop, so the disk-emitted tail must be
            // transformed here. The original performs equivalent transforms inline
            // during the disk loop's emit handling.)
            for (auto& cp : std::span{ sphereCPs.data() + diskPhaseCPBegin, nNumSphereCPs - diskPhaseCPBegin }) {
                cp.m_vecPoint  = TransformPointOG(transformB, cp.m_vecPoint);
                cp.m_vecNormal = TransformVectorOG(transformB, cp.m_vecNormal);
            }
        } else { // 0x4196B9
            for (auto lineIdx = 0u; lineIdx < cdA.m_nNumLines; lineIdx++) {
                const CColLine lineA{
                    // A's line in B's space
                    TransformPointOG(transformAtoB, cdA.m_pLines[lineIdx].m_vecStart),
                    TransformPointOG(transformAtoB, cdA.m_pLines[lineIdx].m_vecEnd),
                };

                // if (!CCollision::TestLineSphere(line, CColSphere{ cmB.m_boundSphere })) { // NOTSA: Quick check to (possibly) speed things up
                //     continue;
                // }

                auto& thisLineCP{ lineCPs[lineIdx] };
                auto& thisLineTochDist{ maxTouchDistances[lineIdx] };

                bool hasCollided{}; // Instead of the static array we just use a variable (Same functionality)

                // 0x419799
                // Spheres
                for (auto&& spIdx : std::span{ collSphB, numCollSphB }) {
                    hasCollided |= ProcessLineSphere(lineA, cdB.m_pSpheres[spIdx], thisLineCP, thisLineTochDist);
                }

                // 0x419803
                // Boxes
                for (auto&& boxIdx : std::span{ collBoxB, numCollBoxB }) {
                    hasCollided |= ProcessLineBox(lineA, cdB.m_pBoxes[boxIdx], thisLineCP, thisLineTochDist);
                }

                // 0x419865
                // Triangles
                for (auto&& triIdx : std::span{ collTrisB, numCollTrisB }) {
                    hasCollided |= ProcessLineTriangle(lineA, cdB.m_pVertices, cdB.m_pTriangles[triIdx], cdB.m_pTrianglePlanes[triIdx], thisLineCP, thisLineTochDist, nullptr);
                }

                // 0x4198DA
                // Now, transform colpoint if it the line collided into world space
                if (hasCollided) {
                    thisLineCP.m_vecPoint  = TransformPointOG(transformB, thisLineCP.m_vecPoint);
                    thisLineCP.m_vecNormal = TransformVectorOG(transformB, thisLineCP.m_vecNormal);
                }
            }
        }
    }

    // 0x4185C0
    // Originally game transformed all sphereCPs to world space here, but I've moved that into the above `if` stuff

    // 0x4199E5
    // Find all A's triangles and boxes colliding with B's b.sphere
    // Then process them all against B's colliding spheres
    if (numCollSphB && (cdA.m_nNumTriangles || cdA.m_nNumBoxes)) {
        const CColSphere colBSphereInASpace{ TransformPointOG(transformBtoA, cmB.m_boundSphere.m_vecCenter), cmB.m_boundSphere.m_fRadius };

        const auto numCPsPrev{ nNumSphereCPs };

        // Process all of A's triangles against B's b.sphere
        static uint32 collTriA[MAX_TRIS];
        uint32        numCollTriA{};
        if (cdA.m_nNumTriangles) {
            CalculateTrianglePlanes(&cmA);
            assert(cdA.m_pTrianglePlanes);

            // NOTE/TODO: Weird how they didn't use the facegroup stuff here as well.
            //            Should probably implement it here some day too, as it speeds up the process quite a bit.
            for (auto triIdx = 0; triIdx < cdA.m_nNumTriangles; triIdx++) {
                if (TestSphereTriangle(colBSphereInASpace, cdA.m_pVertices, cdA.m_pTriangles[triIdx], cdA.m_pTrianglePlanes[triIdx])) {
                    collTriA[numCollTriA++] = triIdx;
                    if (numCollTriA >= TRIS_LIMIT) { // 0x419A99
                        break;
                    }
                }
            }
        }

        // 0x419AB6
        // Process all of A's boxes against B's b.sphere
        static uint32 collBoxA[MAX_BOXES];
        uint32        numCollBoxA{};
        for (auto triIdx = 0; triIdx < cdA.m_nNumBoxes; triIdx++) {
            if (TestSphereBox(colBSphereInASpace, cdA.m_pBoxes[triIdx])) {
                collBoxA[numCollBoxA++] = triIdx;
                if (numCollBoxA >= MAX_BOXES) {
                    break;
                }
            }
        }

        // 0x419B76
        // Process all of B's colliding spheres against all of A's colliding triangles
        // Original (0x419B71): resets the working CP's depth sentinel to -1 here, BEFORE the loop.
        // We must do it explicitly: when `numCollSphA == 0` the first block never ran, so
        // `sphereCPs[nNumSphereCPs].m_fDepth` would otherwise be stale entering this block.
        // NOTE: `numCollSphB - numDisksB` entries of the list, see the quirk note at the disk block
        // (the exe only does it if there are triangles to process, 0x419B76)
        if (numCollTriA) {
            sphereCPs[nNumSphereCPs].m_fDepth = -1.f;
        }
        for (auto sphereIdx : std::span{ collSphB, (size_t)std::max(numCollSphBTail, 0) }) {
            auto minTouchDist{ 1e24f };
            bool anyCollided{};
            for (auto triIdx : std::span{ collTriA, numCollTriA }) {
                if (ProcessSphereTriangle(sphB[sphereIdx], // B's sph in A's space
                                                      cdA.m_pVertices,
                                                      cdA.m_pTriangles[triIdx],
                                                      cdA.m_pTrianglePlanes[triIdx],
                                                      sphereCPs[nNumSphereCPs],
                                                      minTouchDist)) {
                    anyCollided = true;
                }
            }

            if (anyCollided) {
                sphereCPs[nNumSphereCPs].m_vecNormal *= -1.f;
                if (nNumSphereCPs >= maxSphereCPs) { // Original: `if (v200 >= 31) break;` caps total at 31 CPs
                    break;
                }
                ++nNumSphereCPs;
                sphereCPs[nNumSphereCPs].m_fDepth = -1.f; // Set next (future) CP's depth sentinel
            }
        }

        // 0x419CC4
        // Process all of B's colliding spheres against all of A's colliding boxes
        // Same entry count as above (0x419CCE)
        for (auto sphereIdx : std::span{ collSphB, (size_t)std::max(numCollSphBTail, 0) }) {
            const auto& sphere{ sphB[sphereIdx] }; // B's sphere in A's space

            float minTouchDist{ 1e24f };
            for (auto boxIdx : std::span{ collBoxA, numCollBoxA }) {
                // NOTE: unlike the triangle loop above there is NO check before the call (0x419D30): with 31 CPs already
                //       collected the exe still calls ProcessSphereBox (writing into the 32nd slot) and only stops
                //       this sphere's box loop after a hit (the next sphere is processed regardless).
                const auto& bb{ cdA.m_pBoxes[boxIdx] };
                auto&       cp = sphereCPs[nNumSphereCPs];
                if (ProcessSphereBox(sphere, bb, cp, minTouchDist)) {
                    cp.m_nSurfaceTypeA = bb.m_Surface.m_nMaterial;
                    cp.m_nPieceTypeA   = bb.m_Surface.m_nPiece;
                    cp.m_nLightingA    = bb.m_Surface.m_nLighting;

                    cp.m_nSurfaceTypeB = sphere.m_Surface.m_nMaterial;
                    cp.m_nPieceTypeB   = sphere.m_Surface.m_nPiece;
                    cp.m_nLightingB    = sphere.m_Surface.m_nLighting;

                    cp.m_vecNormal *= -1.f; // Invert direction

                    if (nNumSphereCPs >= maxSphereCPs) { // 0x419DDE: `cmp eax, 0x1f / jge` - leaves only this sphere's box loop
                        break;
                    }
                    ++nNumSphereCPs;
                    sphereCPs[nNumSphereCPs].m_fDepth = -1.f; // Set next (future) CP's depth sentinel
                }
            }
        }

        // 0x419E56
        // Transform added colpoints into world space
        if (numCPsPrev != nNumSphereCPs) {                                                            // If we've processed any items..
            for (auto& cp : std::span{ sphereCPs.data() + numCPsPrev, nNumSphereCPs - numCPsPrev }) { // Transform all newly added colpoints
                cp.m_vecPoint  = TransformPointOG(transformA, cp.m_vecPoint);
                cp.m_vecNormal = TransformVectorOG(transformA, cp.m_vecNormal);

                // Original does a full 3-byte swap of {surface,piece,lighting} A<->B:
                //   v175=*(u16*)&sA; v176=lA; *(u16*)&sA=*(u16*)&sB; lA=lB; *(u16*)&sB=v175; lB=v176;
                std::swap(cp.m_nSurfaceTypeA, cp.m_nSurfaceTypeB);
                std::swap(cp.m_nPieceTypeA, cp.m_nPieceTypeB);
                std::swap(cp.m_nLightingA, cp.m_nLightingB);
            }
        }
    }

    // Originally here there was a loop to transform all CPs that were added in the above section
    // I moved it into the above section to keep things clear.

    return (int32)nNumSphereCPs;
}


// 0x414D70
namespace {
//! 0x59C730 `CrossProduct(out, a, b)` - the products stay in the FPU (extended precision), rounded to float only when stored
CVector CrossProductOG(const CVector& a, const CVector& b) {
    return {
        (float)((double)b.z * a.y - (double)a.z * b.y),
        (float)((double)a.z * b.x - (double)b.z * a.x),
        (float)((double)a.x * b.y - (double)b.x * a.y),
    };
}
}

bool CCollision::IsStoredPolyStillValidVerticalLine(const CVector& lineOrigin, float lnMag, CColPoint& colPoint, CStoredCollPoly* collPoly) {
    ZoneScoped;

    // NOTE: `lnMag` is really the Z of the second point of the (vertical) line: the line goes from `lineOrigin` to (lineOrigin.x, lineOrigin.y, lnMag)
    // NOTE: Every failure after the `valid` check clears `collPoly->valid`

    if (!collPoly->valid) { // 0x414D79
        return false;
    }

    const auto& v0 = collPoly->verts[0];
    const auto& v1 = collPoly->verts[1];
    const auto& v2 = collPoly->verts[2];
    const auto& P  = lineOrigin;

    // 0x414D86 - plane of the triangle (note the operand order of the cross product: (v2 - v0) x (v1 - v0))
    const CVector e1{ v1.x - v0.x, v1.y - v0.y, v1.z - v0.z };
    const CVector e2{ v2.x - v0.x, v2.y - v0.y, v2.z - v0.z };
    CVector n = CrossProductOG(e2, e1);
    n.Normalise();
    const float d = (float)(((double)n.z * v0.z + (double)n.y * v0.y) + (double)n.x * v0.x); // 0x414E22

    // 0x414E3E - Find the dominant axis (and the sign of the normal along it)
    const float ax = n.x < 0.f ? -n.x : n.x;
    const float ay = n.y < 0.f ? -n.y : n.y;
    const float az = n.z < 0.f ? -n.z : n.z;
    uint8 axis;
    if (ax > ay && ax > az) {
        axis = n.x > 0.f ? 0 : 1;
    } else if (ay > az) {
        axis = n.y > 0.f ? 2 : 3;
    } else {
        axis = n.z > 0.f ? 4 : 5;
    }

    // 0x414F36 - Which side of the plane are the two end points of the line on?
    const float  Py_ny = (float)((double)P.y * n.y);
    const float  Px_nx = (float)((double)P.x * n.x);
    const double Pz_nz = (double)P.z * n.z;
    const double s = (((double)P.y * n.y + (double)P.x * n.x) + (double)lnMag * n.z) - d; // line end point (P.x, P.y, lnMag)
    const double r = (((double)Px_nx + Py_ny) + Pz_nz) - d;                               // line origin
    if (!(s * r <= 0.0)) { // jp after `test ah, 0x41` => taken if (s * r > 0) or NaN
        collPoly->valid = false;
        return false;
    }

    // 0x414F97 - Intersection of the line with the plane
    const double dx  = (double)P.x - P.x;
    const double dy  = (double)P.y - P.y;
    const float  dzf = (float)((double)lnMag - P.z);
    const double num = (((double)d - Px_nx) - Py_ny) - Pz_nz;
    const float  t   = (float)(num / (((double)dzf * n.z + dy * n.y) + dx * n.x));
    const float  hxs = (float)(dx * t);
    CVector hit{
        (float)(hxs + P.x),
        (float)(dy * t + P.y),
        (float)((double)dzf * t + P.z)
    };

    // 0x415021 - Project everything onto the plane of the dominant axis (odd cases = flipped winding => v1 and v2 swapped)
    float v0a, v0b, v1a, v1b, v2a, v2b, Ha, Hb;
    switch (axis) {
    case 0: v0a = v0.y; v0b = v0.z; v1a = v1.y; v1b = v1.z; v2a = v2.y; v2b = v2.z; Ha = hit.y; Hb = hit.z; break;
    case 1: v0a = v0.y; v0b = v0.z; v1a = v2.y; v1b = v2.z; v2a = v1.y; v2b = v1.z; Ha = hit.y; Hb = hit.z; break;
    case 2: v0a = v0.z; v0b = v0.x; v1a = v1.z; v1b = v1.x; v2a = v2.z; v2b = v2.x; Ha = hit.z; Hb = hit.x; break;
    case 3: v0a = v0.z; v0b = v0.x; v1a = v2.z; v1b = v2.x; v2a = v1.z; v2b = v1.x; Ha = hit.z; Hb = hit.x; break;
    case 4: v0a = v0.x; v0b = v0.y; v1a = v1.x; v1b = v1.y; v2a = v2.x; v2b = v2.y; Ha = hit.x; Hb = hit.y; break;
    case 5: v0a = v0.x; v0b = v0.y; v1a = v2.x; v1b = v2.y; v2a = v1.x; v2b = v1.y; Ha = hit.x; Hb = hit.y; break;
    default: NOTSA_UNREACHABLE();
    }

    // 0x415158 - Point in triangle test (in 2D)
    const double dHa = (double)Ha - v0a;
    const double dHb = (double)Hb - v0b;
    const double c1  = ((double)v2a - v0a) * dHb - ((double)v2b - v0b) * dHa;
    if (c1 < 0.0) { // jp (test ah, 5) => continue if !(c1 < 0)
        collPoly->valid = false;
        return false;
    }
    const double c2 = ((double)v1a - v0a) * dHb - ((double)v1b - v0b) * dHa;
    if (c2 > 0.0) { // je (test ah, 0x41) => fail if c2 > 0
        collPoly->valid = false;
        return false;
    }
    const double c3 = ((double)Hb - v2b) * ((double)v1a - v2a) - ((double)Ha - v2a) * ((double)v1b - v2b);
    if (c3 < 0.0) { // jnp (test ah, 5) => fail if c3 < 0
        collPoly->valid = false;
        return false;
    }

    colPoint.m_vecPoint = hit; // 0x4151F0
    return true;
}

// 0x415230
CColBox CCollision::GetBoundingBoxFromTwoSpheres(const CColSphere& spA, const CColSphere& spB) {
    ZoneScoped;

    CVector min, max;
    for (size_t i = 0; i < 3; i++) {
        // Original: `if (A < B) { min = A - rA; max = rA + B; } else { min = B - rA; max = rA + A; }` (extended precision, rounded when stored)
        // (NOT `std::minmax` - they differ if there is a NaN)
        const auto a = spA.m_vecCenter[i], b = spB.m_vecCenter[i];
        const auto lo = a < b ? a : b;
        const auto hi = a < b ? b : a;
        min[i] = (float)((double)lo - spA.m_fRadius);
        max[i] = (float)((double)spA.m_fRadius + hi); // NOTE: They assume both spheres have the same spRadius, but that might not be the case. Really should be using max(spA.spRadius, spB.spRadius) instead!
    }
    return CColBox{ CBox{min, max} };
}

// 0x4152C0
bool CCollision::IsThisVehicleSittingOnMe(CVehicle* veh, CVehicle* vehOnMe) {
    ZoneScoped;

    if (!veh || !vehOnMe) {
        return false;
    }
    const auto Check = [veh](auto& wheelColEntities) {
        return notsa::contains(wheelColEntities, veh);
    };
    switch (vehOnMe->m_nVehicleType) {
    case VEHICLE_TYPE_AUTOMOBILE: return Check(vehOnMe->AsAutomobile()->m_apWheelCollisionEntity);
    case VEHICLE_TYPE_BIKE:       return Check(vehOnMe->AsBike()->m_aGroundPhysicalPtrs);
    default:                      return false;
    }
}

float GetNearestDistanceOfPedSphereToCameraNearClip(CPed* ped) {
    const auto mi = ped->GetPedModelInfo();

    // Calculate hit colmodel
    mi->AnimatePedColModelSkinnedWorld(ped->GetRpClump());
    const auto hitCM = mi->m_pHitColModel;
    assert(hitCM->GetData()->m_nNumSpheres == 12); // In theory it should have 12 spheres

    // Calculate some other shite
    auto&       cam     = TheCamera.GetActiveCamera();
    // 0x50D5E1: the camera's `front . source` is accumulated unrounded (terms z, y, x) and spilled to a float
    const float offset  = (float)((double)cam.m_vecFront.z * cam.m_vecSource.z + (double)cam.m_vecFront.y * cam.m_vecSource.y + (double)cam.m_vecFront.x * cam.m_vecSource.x);
    const auto  nearClp = RwCameraGetNearClipPlane(Scene.m_pRwCamera);

    // Now find the closest sphere's distance sq
    float ret = 1000000.f; // 0x50D5AA: 0x497423F0, NOT FLT_MAX
    for (auto& sp : hitCM->GetData()->GetSpheres()) {
        // 0x50D620: `center . front` (terms z, x, y) unrounded, then `- offset - radius - nearClip`, rounded to a float when spilled
        const double dot = (double)sp.m_vecCenter.z * cam.m_vecFront.z + (double)sp.m_vecCenter.x * cam.m_vecFront.x + (double)cam.m_vecFront.y * sp.m_vecCenter.y;
        const float  v   = (float)(((dot - offset) - sp.m_fRadius) - nearClp);
        if (v < ret) { // NaN keeps the old value
            ret = v;
        }
    }
    return ret;
}

// 0x415320
bool CCollision::CheckCameraCollisionPeds(
    int32 sectorX,
    int32 sectorY,
    const CVector& pos,
    const CVector& /*unused*/,
    float& /*unused*/
) {
    ZoneScoped;

    constexpr auto gPedCylinderWidth = 1.f; // 0x8CCB8C (float, never written)
    const float    radiusSq          = gPedCylinderWidth * gPedCylinderWidth; // 0x415327

    // NOTE: The original also normalises a copy of the (unused) direction here (0x41537A), it has no effect

    bool addedAny = false;

    auto& sector = CWorld::ms_aRepeatSectors[sectorY & 0xF][sectorX & 0xF]; // Original uses `& 0xF` (even for negative values)
    for (auto* const ped : sector.Peds) {
        if (ped->IsScanCodeCurrent()) {
            continue;
        }

        ped->SetCurrentScanCode();

        if (!ped->GetIsVisible() || CWorld::pIgnoreEntity == ped || ped->IsPlayer()) {
            continue;
        }

        // 0x4153D6 - extended precision, strict `<` (=> skips if NaN)
        const auto centre = ped->GetBoundCentre();
        const double dx   = (double)pos.x - centre.x;
        const double dy   = (double)pos.y - centre.y;
        if (!(dy * dy + dx * dx < (double)radiusSq)) {
            continue;
        }

        if (!(GetNearestDistanceOfPedSphereToCameraNearClip(ped) <= 0.f)) { // 0x415407 (jp after `test ah, 0x41` => skip if > 0 or NaN)
            continue;
        }

        // 0x41541C - Remember the ped (its visibility is cleared at the very end)
        {
            auto& ref = gpMadeInvisibleEntities[gNumEntitiesSetInvisible];
            ref = ped;
            CEntity::RegisterReference(ref);
            gNumEntitiesSetInvisible++;
        }

        // 0x415438 - Add entity the peds holds too (if any)
        // BUG-NOTE: The original passes `false` for `bIgnoreCheckingForSimplestActiveTask` (the default of our declaration is `true`)
        if (const auto task = ped->GetIntelligence()->GetTaskHold(false)) {
            if (const auto ent = task->m_pEntityToHold) {
                if (ent->GetIsVisible()) {
                    ent->SetIsVisible(false);

                    auto& ref = gpMadeInvisibleEntities[gNumEntitiesSetInvisible];
                    ref = ent;
                    CEntity::RegisterReference(ref);
                    gNumEntitiesSetInvisible++;
                }
            }
        }

        ped->SetIsVisible(false); // 0x41547F
        addedAny = true;
    }
    return addedAny;
}

// 0x415540
void ResetMadeInvisibleObjects() {
    for (uint32 i = 0; i < gNumEntitiesSetInvisible; i++) {
        auto& slot = gpMadeInvisibleEntities[i]; // NOTE: Must use the slot itself (not a copy), as the reference system registered the address of the slot
        if (!slot) { // Must check, as the reference system might've cleared it
            continue;
        }
        slot->SetIsVisible(true);
        if (slot) { // 0x41555F - Reloaded
            CEntity::CleanUpOldReference(slot);
        }
        slot = nullptr; // 0x41556B
    }
    gNumEntitiesSetInvisible = 0;
}

// 0x415620
// Unused in the exe. Pushes `A` along `D` onto the plane (normal `N`) through `P`, if `A` is in front of it. Returns true if `A` was moved
bool CCollision::RayPolyPOP(CVector* A, CVector* D, CColTriangle* arg2, CVector* N, CVector* P) {
    ZoneScoped;

    // Original: all extended precision, term order z, x, y
    const double vx = (double)P->x - A->x, vy = (double)P->y - A->y, vz = (double)P->z - A->z;
    const double sE = vz * N->z + vx * N->x + vy * N->y; // (P - A) . N
    if (sE > 0.0) {
        return false;
    }
    const float s = (float)sE; // Spilled to a float

    const double q = (double)N->z * D->z + (double)N->y * D->y + (double)D->x * N->x; // D . N
    if (s <= q) { // NaN passes
        return false;
    }

    const double t = s / q;
    const float  dy = (float)(t * D->y);
    const float  dz = (float)(t * D->z);
    A->x = (float)(t * D->x + A->x);
    A->y = (float)((double)dy + A->y);
    A->z = (float)((double)dz + A->z);
    return true;
}

// 0x4156D0
int32 CCollision::GetPrincipleAxis(const CVector& normal) {
    ZoneScoped;

    const auto nx = std::abs(normal.x),
               ny = std::abs(normal.y),
               nz = std::abs(normal.z);
    if (nx > ny && nx > nz) {
        return 0; // X
    }
    return ny > nz // NaN => Z (original: `je` after `test ah, 0x41`)
        ? 2  // Y
        : 4; // Z
}

// 0x415730
bool CCollision::PointInPoly(
    const CVector& testPt,
    const CColTriangle& /*unused*/,
    const CVector& normal,
    const CVector* verts // Uncompressed vertices
) {
    ZoneScoped;

    // Projects onto the plane of the principal axis, then checks which side of each edge the point is on.
    // `side` is the `(P.a - va) * (vn.b - va.b) - (P.b - va.b) * (vn.a - va.a)` test of an edge
    const auto Edge = [](double Pa, double Pb, float va, float vb, float na, float nb) {
        return (Pa - va) * ((double)nb - vb) - (Pb - vb) * ((double)na - va);
    };
    const auto& v0 = verts[0];
    const auto& v1 = verts[1];
    const auto& v2 = verts[2];

    bool f1, f2, f3;
    switch (GetPrincipleAxis(normal)) {
    case 0: { // X => (y, z). `>= 0` (not `!(< 0)`: NaN => false)
        f1 = Edge(testPt.y, testPt.z, v0.y, v0.z, v1.y, v1.z) >= 0.0;
        f2 = Edge(testPt.y, testPt.z, v1.y, v1.z, v2.y, v2.z) >= 0.0;
        f3 = Edge(testPt.y, testPt.z, v2.y, v2.z, v0.y, v0.z) >= 0.0;
        break;
    }
    case 2: { // Y => (x, z). `<= 0`
        f1 = Edge(testPt.x, testPt.z, v0.x, v0.z, v1.x, v1.z) <= 0.0;
        f2 = Edge(testPt.x, testPt.z, v1.x, v1.z, v2.x, v2.z) <= 0.0;
        f3 = Edge(testPt.x, testPt.z, v2.x, v2.z, v0.x, v0.z) <= 0.0;
        break;
    }
    case 4: { // Z => (x, y). `>= 0`
        f1 = Edge(testPt.x, testPt.y, v0.x, v0.y, v1.x, v1.y) >= 0.0;
        f2 = Edge(testPt.x, testPt.y, v1.x, v1.y, v2.x, v2.y) >= 0.0;
        f3 = Edge(testPt.x, testPt.y, v2.x, v2.y, v0.x, v0.y) >= 0.0;
        break;
    }
    default:
        NOTSA_UNREACHABLE(); // GetPrincipleAxis only returns 0, 2 or 4
    }
    return f1 == f2 && f1 == f3;
}

// 0x415CF0
bool CCollision::SphereCastVersusVsPoly(
    const CColSphere& spA,
    const CColSphere& spB,
    const CColTriangle& tri,
    const CColTrianglePlane& triPlane,
    CompressedVector* verts
) {
    ZoneScoped;

    // NOTE: The original keeps the intermediates in the x87 registers (extended precision) => `double`, rounded to float where the original stores to the stack

    const auto& A = spA.m_vecCenter;

    // 0x415CF0 - spA => spB (AKA velocity)
    const double ABzExact = (double)spB.m_vecCenter.z - A.z; // `fst` => the z² uses the unrounded value times the rounded one
    const CVector spAToB{
        spB.m_vecCenter.x - A.x,
        spB.m_vecCenter.y - A.y,
        (float)ABzExact
    };
    const float spAToBDistSq = (float)((ABzExact * spAToB.z + (double)spAToB.y * spAToB.y) + (double)spAToB.x * spAToB.x);

    const auto plNorm = triPlane.GetNormal();

    const auto spARadius = spA.m_fRadius;

    // 0x415DBF - `GetPtDotNormal` (not rounded)
    const double plSpCenterDist = (((double)A.z * plNorm.z + (double)A.y * plNorm.y) + (double)A.x * plNorm.x) - (float)triPlane.m_normalOffset;
    const bool   isSpTouchingPl = !(std::abs(plSpCenterDist) > spARadius); // jne after `test ah, 0x41`

    // Sphere's center projected onto the plane's normal
    const double projDist = isSpTouchingPl ? plSpCenterDist : (double)spARadius;
    const float  projZOff = (float)(plNorm.z * projDist);
    const CVector spAProj0{
        (float)(A.x - plNorm.x * projDist),
        (float)(A.y - plNorm.y * projDist),
        (float)((double)A.z - projZOff)
    };
    CVector spAProjPl = spAProj0;

    const CVector vA = verts[tri.vA];

    if (!isSpTouchingPl) {
        // 0x415E7E
        const double vtxAToSpDistSqOnPl = ((((double)vA.z - spAProj0.z) * plNorm.z + ((double)vA.y - spAProj0.y) * plNorm.y) + ((double)vA.x - spAProj0.x) * plNorm.x);
        if (vtxAToSpDistSqOnPl > 0.0) {
            return false;
        }
        const float  vtxDist           = (float)vtxAToSpDistSqOnPl; // Spilled to the stack
        const double spAToBDistSqOnPl  = ((double)plNorm.z * spAToB.z + (double)plNorm.y * spAToB.y) + (double)plNorm.x * spAToB.x;
        if ((double)vtxDist <= spAToBDistSqOnPl) {
            return false; // If spA was closer than spB then there's no way spB would touch it, so we're finished
        }
        const double ratio = (double)vtxDist / spAToBDistSqOnPl;
        const float  ry    = (float)(spAToB.y * ratio);
        const float  rz    = (float)(spAToB.z * ratio);
        spAProjPl = CVector{
            (float)(spAToB.x * ratio + spAProj0.x),
            (float)((double)ry + spAProj0.y),
            (float)((double)rz + spAProj0.z)
        };
    }

    const CVector vB = verts[tri.vB], vC = verts[tri.vC];

    const CVector cverts[]{vA, vB, vC};
    if (PointInPoly(spAProjPl, tri, plNorm, cverts)) {
        return true;
    }

    // 0x416050 - The result of `ClosestSquaredDistanceBetweenFiniteLines` is compared unrounded (st0)
    const auto& pos       = spA.m_vecCenter;
    const float maxDistSq = (float)((double)spARadius * spARadius);
    return ClosestSquaredDistanceBetweenFiniteLinesOG(pos, vA, vB, spAToB, spAToBDistSq) < maxDistSq
        || ClosestSquaredDistanceBetweenFiniteLinesOG(pos, vC, vB, spAToB, spAToBDistSq) < maxDistSq
        || ClosestSquaredDistanceBetweenFiniteLinesOG(pos, vA, vC, spAToB, spAToBDistSq) < maxDistSq;
}

/*!
* Below is used for the camera anti-clipping bullshittery.
* (When you move the camera and instead of clipping thru the
* object it slides along it's collision)
*
* Honestly, as much as I bash the devs from 2003, this one
* is quite well done (Other than using static variables xD)
*
* Definitions:
* spA - Sphere representing the camera
* spB - `spA` but it's center offset by the spBToA of the player (Basically, where the camera would be the next timestep)
* ws  - "World space"
* os  - "Object space"
* bs  - Bullshit (You probably knew this one already, extensively used in the code)
*
* TODO:
* Sometime in the distant future we should get rid of the global variables... :)
*/

//! A singular entry in the ColCache used for the anti-clipping bs
struct CColCacheEntry {
    enum class eType : uint8 {
        NONE,
        TRIANGLE,
        SPHERE,
        BOX,
        // BACKSIDE_TRIANGLE // TODO: Add this. Search for `BULLSHIT_DETECTOR`
    };

    CColCacheEntry() = default;

    CColCacheEntry(CEntity* ent, eType type = eType::NONE, uint16 idx = 0) :
        ent{ent},
        type{type}
    {
        using enum eType;
        switch (type) {
        case TRIANGLE: triIdx = idx; break;
        case SPHERE:   sphIdx = idx; break;
        case BOX:      boxIdx = idx; break;
        }
    }

    eType     type{};
    CEntity*  ent{};
    uint16    triIdx{}; // Why the fuck would they use 3 ints instead of 1 for this is beyond any imgination  - TODO: Use 1 singular uint32 here, and refactor the code accordingly!
    uint16    sphIdx{};
    uint16    boxIdx{};
};
//! Value should be in range (0, 1] (Default: 0.001)
constexpr auto gLimitPrecisionOfBinarySearch = 0.001f;

//! Size of the collision cache used - Larger values should allow more complex collisions (Default: 100)
constexpr size_t COL_CACHE_SIZE = 100;

/*!
* The cache has regions.
* Each region starts with an entry that has `ent` set.
* All following entries (up until the next such entry) belong to it.
*/
using ColCache = std::array<CColCacheEntry, COL_CACHE_SIZE>;

//! Cache used currently
static inline auto& gpColCache = StaticRef<ColCache*>(0x9655CC);

//! Entries in the cache that is used currently
static inline auto& gColCacheNumEntries = StaticRef<uint32>(0x9655D8);

//! Unused
static inline auto& gpColCache2 = StaticRef<ColCache*>(0x9655C8);

//! Fuck knows
static inline auto& gbTryDoubleSidedCollision = StaticRef<bool>(0x9655E4);

//! Last "distance" that wasn'plSpCenterDist colliding - result from the binary search
static inline auto& gLastRadiusUsedInCollisionPreventionOfCamera = StaticRef<float>(0xB6EC6C);

// 0x415590
bool CCollision::SphereCastVsBBox(
    const CColSphere& spA,
    const CColSphere& spB,
    const CColBox& box
) {
    ZoneScoped;

    const auto     r = spA.m_fRadius;
    const CVector  vRadius{ r, r, r };
    return TestLineBox_DW(
        CColLine{ spA.m_vecCenter, spB.m_vecCenter },
        CColBox{ box.m_vecMin - vRadius, box.m_vecMax + vRadius } // Not sure what's going on
    );
}

/*!
* The input to this function is basically generated by `SphereCastVsEntity`
* The output format (written to `out`) is exactly the same as the input.
*
* @param       spAws    As discussed.
* @param       spBToA Player's spBToA
* @param       numIn    Number of entries in the `in` cache
* @param       in       Input cache (To process)
* @param [out] numOut   Number of entries written to the `out` cache
* @param [out] out      Output cache (Of entries [from `in`] that had collided)
* @addr 0x4181B0
*/
bool CCollision::SphereCastVsCaches(
    const CColSphere& spAws,
    const CVector& velocity,

    int32 numIn,
    CColCacheEntry* in,

    int32& numOut,
    CColCacheEntry* out
) {
    ZoneScoped;

    const CColSphere spBws{ spAws.m_vecCenter + velocity, spAws.m_fRadius }; // 0x4181F9 (float adds)

    CColSphere       spAos, spBos; // Working copies (in the space of the entity of the current region)
    CCollisionData*  ecd = nullptr;

    // Process entities now
    for (int32 i = 0; i < numIn; i++) {
        const auto& entry  = in[i];
        const auto  entity = entry.ent;

        if (entity) { // 0x418279 - New region
            spAos = spAws;
            spBos = spBws;

            auto& entMat = entity->GetMatrix(); // 0x4182AC - Allocates the matrix if needed (even if there's no col data)

            ecd = entity->GetColData(); // 0x4182D0
            if (!ecd) {
                continue; // 0x4182DE
            }

            // Transform speheres into entity's (object) space (0x418319)
            // The original transforms the end points of a line (`0x59C890`, extended precision)
            const auto invEntMat = Invert(entMat);
            spBos.m_vecCenter = TransformPointOG(invEntMat, spBws.m_vecCenter);
            spAos.m_vecCenter = TransformPointOG(invEntMat, spAws.m_vecCenter);

            // Have to push this nevertheless
            // If there are no collisions it will be overwritten
            out[numOut].ent  = entity; // 0x4183C2
            out[numOut].type = CColCacheEntry::eType::NONE;
        } else if (!ecd) { // NOTSA: The original would crash here
            continue;
        }

        // Now do the test - note: the first hit of a region overwrites the entry pushed above (=> keeps `ent` set)
        using enum CColCacheEntry::eType;
        switch (entry.type) {
        case TRIANGLE: { // 0x418480
            const auto triIdx   = entry.triIdx;
            const bool backside = !(triIdx < (uint16)SHRT_MAX); // I don'plSpCenterDist have a damn clue why complicate shit so much instead of using a 4th entry type (like `BACKSIDE_TRIANGLE`)
            const auto idx      = backside
                ? (uint16)(0xFFFFu - triIdx) // Search in file for: BULLSHIT_DETECTOR
                : triIdx;
            // 0x4184D0 - NOTE: For the backside entries the original passes the spheres the other way around (B, A), see `SphereCastVsEntity` where these are added
            const bool hit = backside
                ? SphereCastVersusVsPoly(spBos, spAos, ecd->m_pTriangles[idx], ecd->m_pTrianglePlanes[idx], ecd->m_pVertices)
                : SphereCastVersusVsPoly(spAos, spBos, ecd->m_pTriangles[idx], ecd->m_pTrianglePlanes[idx], ecd->m_pVertices);
            if (!hit) {
                continue;
            }
            auto& dst = out[numOut++];
            dst.triIdx = triIdx;
            dst.type   = TRIANGLE;
            break;
        }
        case SPHERE: { // 0x41843B
            if (!SphereCastVsSphere(spAos, spBos, ecd->m_pSpheres[entry.sphIdx])) {
                continue;
            }
            auto& dst = out[numOut++];
            dst.sphIdx = entry.sphIdx;
            dst.type   = SPHERE;
            break;
        }
        case BOX: { // 0x4183F7
            if (!SphereCastVsBBox(spAos, spBos, ecd->m_pBoxes[entry.boxIdx])) {
                continue;
            }
            auto& dst = out[numOut++];
            dst.boxIdx = entry.boxIdx;
            dst.type   = BOX;
            break;
        }
        default:
            continue;
        }

        // 0x418524 - Terminate
        out[numOut].ent  = nullptr;
        out[numOut].type = NONE;
    }

    return numOut > 0;
}

/*!
* Similar to `SphereCastVsCaches`, but only against a single entity.
* Result is written to `gpColCache`, and the number of entries to `gColCacheNumEntries`
* @addr 0x419F00
*/ 
bool CCollision::SphereCastVsEntity(
    const CColSphere& spAws,
    const CColSphere& spBws,
    CEntity* entity
) {
    ZoneScoped;

    if (!entity->GetUsesCollision() || TheCamera.IsExtraEntityToIgnore(entity)) { // 0x419F23
        return false;
    }

    // 0x419F55 - Allocates the matrix if needed (even if there's no col data)
    const auto invEntMat = Invert(entity->GetMatrix());

    // 0x419F9A - Transform into object space (os) (the end points of a line are transformed, `0x59C890`)
    // BUG: The original uses the radius of `spAws` for both spheres
    CColSphere spAos{ TransformPointOG(invEntMat, spAws.m_vecCenter), spAws.m_fRadius },
               spBos{ TransformPointOG(invEntMat, spBws.m_vecCenter), notsa::IsFixBugs() ? spBws.m_fRadius : spAws.m_fRadius };

    const auto ecm = entity->GetColModel(); // 0x41A085
    const auto ecd = ecm->m_pColData;
    if (!ecd) {
        return false;
    }

    if (!SphereCastVsBBox(spAos, spBos, ecm->GetBoundingBox())) {
        return false;
    }

    using enum CColCacheEntry::eType;

    auto* const cache = (ColCache*)gpColCache; // 0x41A0D6 - read once

    // Note: All the loops below go from the LAST element to the first one (as the original does), as the order of the entries in the cache matters!
    auto anyCollisionsDetected = false;
    const auto AddEntryToColCache = [&, entity](CColCacheEntry::eType type, uint16 idx) {
        if ((int32)gColCacheNumEntries >= (int32)COL_CACHE_SIZE - 1) { // TODO: Magic number
            return true;
        }
        auto& e = (*cache)[gColCacheNumEntries];
        e.type  = NONE;
        e.ent   = anyCollisionsDetected ? nullptr : entity; // Only the first entry has the entity set, subsequent ones have nullptr
        anyCollisionsDetected = true;

        auto& e2 = (*cache)[gColCacheNumEntries++];
        switch (type) {
        case SPHERE:   e2.sphIdx = idx; break;
        case TRIANGLE: e2.triIdx = idx; break;
        case BOX:      e2.boxIdx = idx; break;
        default:       NOTSA_UNREACHABLE();
        }
        e2.type = type;
        return false;
    };

    // Process spheres
    for (int32 idx = (int16)ecd->m_nNumSpheres - 1; idx >= 0; idx--) {
        if (!SphereCastVsSphere(spAos, spBos, ecd->m_pSpheres[idx])) {
            continue;
        }
        if (AddEntryToColCache(SPHERE, (uint16)idx)) {
            return true;
        }
    }

    // Process triangles
    {
        const bool tryDoubleSided = gbTryDoubleSidedCollision; // 0x41A16D

        CalculateTrianglePlanes(ecd);

        const auto verts  = ecd->m_pVertices;
        const auto tris   = ecd->m_pTriangles;
        const auto triPls = ecd->m_pTrianglePlanes;

        const auto ProcessTri = [&](int32 triIdx) { // If `true` is returned the calle should `return true` too, otherwise nothing.
            const auto& tri   = tris[triIdx];
            const auto& triPl = triPls[triIdx];

            if (SphereCastVersusVsPoly(spAos, spBos, tri, triPl, verts)) {
                if (AddEntryToColCache(TRIANGLE, (uint16)triIdx)) {
                    return true;
                }
            }

            // 0x41A2C8
            if (tryDoubleSided && std::abs((float)triPl.m_normal.z) < 0.05f && SphereCastVersusVsPoly(spBos, spAos, tri, triPl, verts)) {
                if (AddEntryToColCache(TRIANGLE, (uint16)(0xFFFFu - (uint32)triIdx))) { // Search in file for: BULLSHIT_DETECTOR
                    return true;
                }
            }

            return false;
        };

        if ((int16)ecd->m_nNumTriangles != 0) {
            if (ecd->bHasFaceGroups) {
                // The face groups are stored (in reverse order) right before the triangles (the number of them is right before them)
                const auto numFGs = *reinterpret_cast<int32*>(reinterpret_cast<uint8*>(tris) - sizeof(uint32));
                for (int32 j = 0; j < numFGs; j++) {
                    const auto& fg = *reinterpret_cast<const ColHelpers::TFaceGroup*>(reinterpret_cast<const uint8*>(tris) - 0x20 - j * (int32)sizeof(ColHelpers::TFaceGroup));
                    if (!SphereCastVsBBox(spAos, spBos, fg.bb)) {
                        continue;
                    }

                    for (int32 triIdx = (int16)fg.first; triIdx <= (int16)fg.last; triIdx++) {
                        if (ProcessTri(triIdx)) {
                            return true;
                        }
                    }
                }
            } else {
                for (int32 triIdx = (int16)ecd->m_nNumTriangles - 1; triIdx >= 0; triIdx--) {
                    if (ProcessTri(triIdx)) {
                        return true;
                    }
                }
            }
        }
    }

    // Process boxes
    for (int32 idx = (int16)ecd->m_nNumBoxes - 1; idx >= 0; idx--) {
        if (SphereCastVsBBox(spAos, spBos, ecd->m_pBoxes[idx])) {
            if (AddEntryToColCache(BOX, (uint16)idx)) {
                return true;
            }
        }
    }

    return anyCollisionsDetected;
}

namespace {
//! The original reads the radius from the collision model of the model info (NOT `CEntity::GetColModel`, as that might be a special col model of a vehicle)
CColSphere GetWorldBoundSphereOfEntity(CEntity* entity) {
    const float radius = entity->GetModelInfo()->GetColModel()->m_boundSphere.m_fRadius;
    return { entity->GetBoundCentre(), radius };
}

//! The test that is inlined in `CheckCameraCollisionBuildings` and `CheckCameraCollisionObjects` (STRICT; unrounded)
bool IsEntityBoundSphereCloseTo(const CColSphere& spS, CEntity* entity) {
    const auto sp = GetWorldBoundSphereOfEntity(entity);
    const double dx = (double)spS.m_vecCenter.x - sp.m_vecCenter.x;
    const double dy = (double)spS.m_vecCenter.y - sp.m_vecCenter.y;
    const double dz = (double)spS.m_vecCenter.z - sp.m_vecCenter.z;
    const double r  = (double)sp.m_fRadius + spS.m_fRadius;
    return r * r > (dz * dz + dy * dy) + dx * dx; // `fcompp` + `test ah, 0x41` + `jne` => skip if !(r^2 > dist^2)
}
}

// 0x41A820
bool CCollision::CheckCameraCollisionBuildings(
    int32 X,
    int32 Y,
    const CColBox& pBox, // unused
    const CColSphere& spS,
    const CColSphere& spA,
    const CColSphere& spB
) {
    ZoneScoped;

    const auto plyrVeh = FindPlayerVehicle(-1, false);
    const auto checkFlyerCollision = plyrVeh && plyrVeh->physicalFlags.bDontCollideWithFlyers;

    bool anyCollided = false;
    for (auto* const entity : CWorld::GetSector(X, Y).Buildings) { // `GetSector` clamps => same as the original
        if (!entity->ProcessScan()) {
            continue;
        }

        if (checkFlyerCollision && entity->DoesNotCollideWithFlyers()) {
            continue;
        }

        if (CWorld::pIgnoreEntity == entity) {
            continue;
        }

        if (!IsEntityBoundSphereCloseTo(spS, entity)) {
            continue;
        }

        if (SphereCastVsEntity(spA, spB, entity)) {
            anyCollided = true;
        }
    }
    return anyCollided;
}

// 0x41A990
bool CCollision::CheckCameraCollisionVehicles(
    int32 X,
    int32 Y,
    const CColBox& bbSpAB, // unused
    const CColSphere& spS,
    const CColSphere& spA,
    const CColSphere& spB,
    const CVector* plyrVehVel
) {
    ZoneScoped;

    static auto& gpLastSittingOnEntity   = StaticRef<CEntity*>(0x9689D8);
    static auto& gFramesSittingOnTimeOut = StaticRef<int32>(0x9689D4);
    static auto& gFramesToConsiderSittingOnStillTrue = StaticRef<int32>(0x8A5B1C); // 30

    bool anyCollided = false;
    for (auto* const entity : CWorld::ms_aRepeatSectors[Y & 0xF][X & 0xF].Vehicles) { // Original uses `& 0xF` (even for negative values)
        if (!entity->ProcessScan()) {
            continue;
        }

        if (CWorld::pIgnoreEntity == entity) {
            continue;
        }

        // NOTE: `pIgnoreEntity` is passed as is (it might not be a vehicle, or null)
        if (IsThisVehicleSittingOnMe(static_cast<CVehicle*>(CWorld::pIgnoreEntity), entity)) {
            gFramesSittingOnTimeOut = gFramesToConsiderSittingOnStillTrue;
            gpLastSittingOnEntity = entity;
            continue;
        }

        if (gpLastSittingOnEntity == entity) {
            if (gFramesSittingOnTimeOut-- == 0) { // TODO: FPS bug
                gpLastSittingOnEntity = nullptr;
            }
            continue;
        }

        if (plyrVehVel) {
            const auto& ms = entity->GetMoveSpeed();
            const double dx = (double)plyrVehVel->x - ms.x;
            const double dy = (double)plyrVehVel->y - ms.y;
            const double dz = (double)plyrVehVel->z - ms.z;
            if (!((dz * dz + dy * dy) + dx * dx < relVelCamCollisionVehiclesSqr)) { // jp after `test ah, 5` => continue if !(a < b)
                continue;
            }
        }

        if (!TestSphereSphere(spS, GetWorldBoundSphereOfEntity(entity))) {
            continue;
        }

        if (SphereCastVsEntity(spA, spB, entity)) {
            anyCollided = true;
        }
    }
    return anyCollided;
}

// 0x41AB20
bool CCollision::CheckCameraCollisionObjects(
    int32 X,
    int32 Y,
    const CColBox& pBox, // unused
    const CColSphere& spS,
    const CColSphere& spA,
    const CColSphere& spB
) {
    ZoneScoped;

    // Pirulax: At this point I'm certain R* devs were paid by lines written

    bool anyCollided = false;
    for (auto* const entity : CWorld::ms_aRepeatSectors[Y & 0xF][X & 0xF].Objects) { // Original uses `& 0xF` (even for negative values)
        if (!entity->ProcessScan()) {
            continue;
        }

        if (CWorld::CameraToIgnoreThisObject(entity) || CWorld::pIgnoreEntity == entity) {
            continue;
        }

        if (!IsEntityBoundSphereCloseTo(spS, entity)) {
            continue;
        }

        if (SphereCastVsEntity(spA, spB, entity)) {
            anyCollided = true;
        }
    }
    return anyCollided;
}

// Ah, yes, the ultimate solution, just use static variables!
static inline auto& gnBottom = StaticRef<int32>(0x965598);
static inline auto& gnTop = StaticRef<int32>(0x965590);
static inline auto& gnRight = StaticRef<int32>(0x965594);
static inline auto& gnLeft = StaticRef<int32>(0x96559C);

// 0x4154A0
bool CCollision::CheckPeds(
    const CVector& src,
    const CVector& normal, /*unused*/
    float& nearest /*unused*/
) {
    ZoneScoped;

    if (!bCamCollideWithPeds) {
        return false;
    }

    // NOTE: Not using `CWorld::IterateSectors` as it asserts on an empty range, the original just does nothing
    const auto left = gnLeft, right = gnRight, bottom = gnBottom, top = gnTop; // 0x4154B2
    bool anyCollides = false;
    for (auto sy = bottom; sy <= top; sy++) {
        for (auto sx = left; sx <= right; sx++) {
            if (CheckCameraCollisionPeds(sx, sy, src, normal, nearest)) {
                anyCollides = true;
            }
        }
    }
    return anyCollides;
}

// 0x41AC40
bool CCollision::BuildCacheOfCameraCollision(
    const CColSphere& spA,
    const CColSphere& spB
) {
    ZoneScoped;

    const auto spABBox = GetBoundingBoxFromTwoSpheres(spA, spB);

    // 0x41AC5B - Sphere around the box (extended precision, rounded when stored, see the original for what is rounded and what isn't)
    const CColSphere spABBSp = [&] {
        const double sx = (double)spABBox.m_vecMax.x - spABBox.m_vecMin.x;
        const double sy = (double)spABBox.m_vecMax.y - spABBox.m_vecMin.y;
        const double sz = (double)spABBox.m_vecMax.z - spABBox.m_vecMin.z;
        const float  hz = (float)(sz * 0.5); // Spilled to the stack
        const CVector center{
            (float)(sx * 0.5 + spABBox.m_vecMin.x),
            (float)(sy * 0.5 + spABBox.m_vecMin.y),
            (float)((double)hz + spABBox.m_vecMin.z)
        };
        const float radius = (float)(std::sqrt((sz * sz + sy * sy) + sx * sx) * 0.5);
        return CColSphere{ CSphere{ center, radius }, (eSurfaceType)0, 0, tColLighting{ 0xFF } };
    }();

    // 0x41ACE9 - `CWorld::GetSectorX/Y` inlined: `(int)floor((double)(v * 0.02f + 60.0f))`.
    // The unrounded (x87) value is used to check the range, while the value that is used is calculated from the value spilled to the stack (float)
    const auto GetSectorCoord = [](float v, auto&& Clamp) {
        const double ext = (double)v * (double)0.02f + 60.0;
        const auto   a   = (int32)std::floor(ext);
        const auto   b   = (int32)std::floor((double)(float)ext);
        return Clamp(a, b);
    };
    const auto ClampLow  = [](int32 a, int32 b) { return a > 0 ? b : 0; };       // 0x41AD12 jle
    const auto ClampHigh = [](int32 a, int32 b) { return a >= 0x77 ? 0x77 : b; }; // 0x41ADA7 jge
    const int32 left   = GetSectorCoord(spABBox.m_vecMin.x, ClampLow);
    const int32 bottom = GetSectorCoord(spABBox.m_vecMin.y, ClampLow);
    const int32 right  = GetSectorCoord(spABBox.m_vecMax.x, ClampHigh);
    const int32 top    = GetSectorCoord(spABBox.m_vecMax.y, ClampHigh);
    gnLeft   = left;
    gnBottom = bottom;
    gnRight  = right;
    gnTop    = top;

    CWorld::AdvanceCurrentScanCode();

    const bool doVehicles  = bCamCollideWithVehicles;
    const bool doBuildings = bCamCollideWithBuildings;
    const bool doObjects   = bCamCollideWithObjects;

    gColCacheNumEntries = 0;

    const auto ogpIgnoreEntity = CWorld::pIgnoreEntity;
    if (!ogpIgnoreEntity) {
        auto& plyrtm = FindPlayerPed(0)->GetTaskManager();

        // NOTE: Order matters (driver is checked first)
        auto task = plyrtm.FindActiveTaskByType(TASK_COMPLEX_ENTER_CAR_AS_DRIVER); // 0x2BD
        if (!task) {
            task = plyrtm.FindActiveTaskByType(TASK_COMPLEX_ENTER_CAR_AS_PASSENGER); // 0x2BC
        }
        if (task) {
            CWorld::pIgnoreEntity = static_cast<CTaskComplexEnterCar*>(task)->GetCameraAvoidVehicle();
        }
    }

    CVector plyrVelCopy;
    const CVector* plyrVehVel = nullptr;
    if (FindPlayerVehicle(0, false)) {
        plyrVelCopy = FindPlayerSpeed(0);
        plyrVehVel  = &plyrVelCopy;
    }

    bool anyCollision = false;
    for (auto sy = bottom; sy <= top; sy++) {
        for (auto sx = left; sx <= right; sx++) {
            if (doBuildings) {
                gbTryDoubleSidedCollision = true;
                if (CheckCameraCollisionBuildings(sx, sy, spABBox, spABBSp, spA, spB)) {
                    anyCollision = true;
                }
                gbTryDoubleSidedCollision = false;
            }
            if (doVehicles) {
                if (CheckCameraCollisionVehicles(sx, sy, spABBox, spABBSp, spA, spB, plyrVehVel)) {
                    anyCollision = true;
                }
            }
            if (doObjects) {
                if (CheckCameraCollisionObjects(sx, sy, spABBox, spABBSp, spA, spB)) {
                    anyCollision = true;
                }
            }
        }
    }

    CWorld::pIgnoreEntity = ogpIgnoreEntity;

    return anyCollision;
}

/*!
* Cast the cone of the camera against the world
* Honestly, as much as I bash the devs from 2003, this one
* is quite well done (Other than using static variables xD)
* This function is responsible for the anit-clipping
* (When you move the camera and instead of clipping thru the
* object it slides along it's collision)
* 
* @addr 0x41B000
*
* @param spA     Sphere representing the camera
* @param spB     `spA` but it's center offset by the spBToA of the player (Basically, where the camera would be the next timestep)
* @param dst     Minimum distance that doesn'plSpCenterDist collide ("Distance" is a bad word tbh, it's more like a scale from [minDist, 1])
* @param minDist See `dst`
*/
bool CCollision::CameraConeCastVsWorldCollision(
    const CColSphere& spA,
    const CColSphere& spB,
    float& dst,
    float minDist
) {
    ZoneScoped;

    ColCache caches[2]{};
    gpColCache  = &caches[0];
    gpColCache2 = &caches[1];

    if (!BuildCacheOfCameraCollision(spA, spB)) {
        return false;
    }

    // Reminder: The 2 spheres are offset by the spBToA of the player...
    const CVector velocity{
        spB.m_vecCenter.x - spA.m_vecCenter.x,
        spB.m_vecCenter.y - spA.m_vecCenter.y,
        spB.m_vecCenter.z - spA.m_vecCenter.z
    };

    // Radius of the badass spehere we're going to use
    const auto spRadius = spA.m_fRadius;

    // Badass sphere that represents the camera
    // (gets smaller and smaller as we progress with the binary search)
    CColSphere spCam{ spA.m_vecCenter, spRadius };

    // Since writing to overlapping arrays is a bad idea we use 2 caches
    // one is the current, other one is the next one to use
    ColCache* in  = &caches[0];
    ColCache* out = &caches[1];
    int32 numIn   = (int32)gColCacheNumEntries; // NOTE: The original keeps the count in a register (the global isn't updated)

    // Now, we do a badass binary search to find the closest 
    // possible distance to the collision such that the camera
    // (or well, the sphere representing it `spCam`)
    // isn'plSpCenterDist clipping into it
    // 0x8A5B10 (float, = 0.001f)
    const float limit = gLimitPrecisionOfBinarySearch;
    float max = 1.f, min = minDist;
    double rng = 1.0 - (double)minDist; // Unrounded, as the x87 stack
    float dstF;
    do {
        const double dstExt = rng * 0.5 + min;
        dstF = (float)dstExt;

        spCam.m_fRadius = (float)(dstExt * spRadius);

        const CVector scaledVel{ (float)((double)velocity.x * dstF), (float)((double)velocity.y * dstF), (float)((double)velocity.z * dstF) };

        int32 numOut = 0;
        if (SphereCastVsCaches(spCam, scaledVel, numIn, in->data(), numOut, out->data())) {
            numIn = numOut;
            std::swap(in, out);
            max = dstF;
        } else {
            min = dstF;
        }
        rng = (double)max - min;
    } while (rng > limit); // Uses the NEW range

    dst = dstF;
    gLastRadiusUsedInCollisionPreventionOfCamera = spCam.m_fRadius;

    return true;
}

// 0x41A5A0
bool CCollision::SphereVsEntity(CColSphere* sphere, CEntity* entity) {
    ZoneScoped;

    // Unused in the exe (no callers). Note that the original never reads `sphere`: the "object space" sphere it tests
    // against is an uninitialized stack variable (the transformation code is missing from the binary).
    // NOTSA: We use a zeroed sphere instead of undefined data.
    const CColSphere sphereOS{};

    if (!entity->GetUsesCollision() || TheCamera.IsExtraEntityToIgnore(entity)) {
        return false;
    }

    const auto invMat = Invert(entity->GetMatrix()); // Also unused, other than being constructed/destructed
    (void)invMat;

    const auto cd = entity->GetColModel()->m_pColData;
    if (!cd) {
        return false;
    }

    // Spheres (backwards)
    for (auto i = (int32)cd->m_nNumSpheres - 1; i >= 0; i--) {
        const auto& sp = cd->m_pSpheres[i];
        // Original: unrounded, term order z, y, x. Strict compare (`FCOMPP` + `test ah, 0x41` + `je`)
        const double dx = (double)sphereOS.m_vecCenter.x - sp.m_vecCenter.x;
        const double dy = (double)sphereOS.m_vecCenter.y - sp.m_vecCenter.y;
        const double dz = (double)sphereOS.m_vecCenter.z - sp.m_vecCenter.z;
        const double sumR = (double)sphereOS.m_fRadius + sp.m_fRadius;
        if (sumR * sumR > dz * dz + dy * dy + dx * dx) {
            return true;
        }
    }

    // Boxes (backwards)
    for (auto i = (int32)cd->m_nNumBoxes - 1; i >= 0; i--) {
        if (TestSphereBox(sphereOS, cd->m_pBoxes[i])) {
            return true;
        }
    }

    // Triangles
    CalculateTrianglePlanes(cd);

    if (!cd->m_nNumTriangles) {
        return false;
    }

    if (cd->bHasFaceGroups) {
        // Original goes through the face groups in memory-descending order (`GetFaceGroups()` is ascending) - doesn't matter here
        const auto groups = cd->GetFaceGroups();
        for (auto gi = (int32)groups.size() - 1; gi >= 0; gi--) {
            const auto& group = groups[gi];
            if (!TestSphereBox(sphereOS, group.bb)) {
                continue;
            }
            for (auto ti = (int32)group.first; ti <= (int32)group.last; ti++) {
                if (TestSphereTriangle(sphereOS, cd->m_pVertices, cd->m_pTriangles[ti], cd->m_pTrianglePlanes[ti])) {
                    return true;
                }
            }
        }
    } else {
        for (auto i = (int32)cd->m_nNumTriangles - 1; i >= 0; i--) {
            if (TestSphereTriangle(sphereOS, cd->m_pVertices, cd->m_pTriangles[i], cd->m_pTrianglePlanes[i])) {
                return true;
            }
        }
    }

    return false;
}

void CCollision::InjectHooks() {
#ifdef TEST_COLLISION_FUNCS
    // Must be done before hooks are injected
    for (auto i = 0; i < 20; i++) {
       Tests(i);
    }
#endif

    RH_ScopedClass(CCollision);
    RH_ScopedCategoryGlobal();

    ////
    // Test & Process
    ////

    // NOTE: These used to be disabled (redirected to the original code) because of the bad performance in debug mode
    // of the DLL build. The standalone exe has no original code => all of these are ours now and are reviewed against the exe.
    // Define `NOTSA_DEBUG_SLOW_COLLISION_OFF` to redirect them to the original code again (DLL builds only).
#ifdef NOTSA_DEBUG_SLOW_COLLISION_OFF
    const auto state = HS::RedirectToGTA;
    const auto locked = true;
#else
    const auto state = HS::RedirectToOurs;
    const auto locked = false;
#endif
    
    RH_ScopedInstall(Test2DLineAgainst2DLine, 0x4138D0, { .State = state, .Locked = locked });

    RH_ScopedInstall(ProcessDiscCollision, 0x413960, { .State = state, .Locked = locked });

    RH_ScopedInstall(TestLineBox_DW, 0x412C70, { .State = state, .Locked = locked });
    RH_ScopedInstall(TestLineBox, 0x413070, { .State = state, .Locked = locked });
    RH_ScopedInstall(ProcessLineBox, 0x413100, { .State = state, .Locked = locked });
    RH_ScopedInstall(TestVerticalLineBox, 0x413080, { .State = state, .Locked = locked });

    RH_ScopedInstall(TestLineTriangle, 0x413AC0, { .State = state, .Locked = locked });
    RH_ScopedInstall(ProcessLineTriangle, 0x4140F0, { .State = state, .Locked = locked });
    RH_ScopedInstall(ProcessVerticalLineTriangle, 0x4147E0, { .State = state, .Locked = locked });

    RH_ScopedInstall(TestLineSphere, 0x417470, { .State = state, .Locked = locked });
    RH_ScopedInstall(ProcessLineSphere, 0x412AA0, { .State = state, .Locked = locked });

    RH_ScopedInstall(TestSphereBox, 0x4120C0, { .State = state, .Locked = locked });
    RH_ScopedInstall(ProcessSphereBox, 0x412130, { .State = state, .Locked = locked });

    RH_ScopedInstall(TestSphereSphere, 0x411E70, { .State = state, .Locked = locked });
    RH_ScopedInstall(ProcessSphereSphere, 0x416450, { .State = state, .Locked = locked });

    RH_ScopedInstall(TestSphereTriangle, 0x4165B0, { .State = state, .Locked = locked });
    RH_ScopedInstall(ProcessSphereTriangle, 0x416BA0, { .State = state, .Locked = locked });

    RH_ScopedInstall(ProcessColModels, 0x4185C0, { .State = state, .Locked = locked });

    RH_ScopedInstall(TestLineOfSight, 0x417730, { .State = state, .Locked = locked });
    RH_ScopedInstall(ProcessLineOfSight, 0x417950, { .State = state, .Locked = locked });
    RH_ScopedInstall(ProcessVerticalLine, 0x417BF0, { .State = state, .Locked = locked });

    ////
    // Rest
    ////

    RH_ScopedInstall(Init, 0x416260, { .State = state, .Locked = locked });
    RH_ScopedInstall(Shutdown, 0x4162E0, { .State = state, .Locked = locked });
    RH_ScopedInstall(Update, 0x411E20, { .State = state, .Locked = locked });
    RH_ScopedInstall(SortOutCollisionAfterLoad, 0x411E30, { .State = state, .Locked = locked });
    RH_ScopedGlobalInstall(CalculateColPointInsideBox, 0x411EC0, { .State = state, .Locked = locked });
    RH_ScopedInstall(PointInTriangle, 0x412700, { .State = state, .Locked = locked });
    RH_ScopedInstall(DistToLineSqr, 0x412850, { .State = state, .Locked = locked });
    RH_ScopedInstall(DistToMathematicalLine, 0x412970, { .State = state, .Locked = locked });
    RH_ScopedInstall(DistToMathematicalLine2D, 0x412A30, { .State = state, .Locked = locked });
    RH_ScopedInstall(DistAlongLine2D, 0x412A80, { .State = state, .Locked = locked });
    RH_ScopedInstall(IsStoredPolyStillValidVerticalLine, 0x414D70, { .State = state, .Locked = locked });
    RH_ScopedInstall(GetBoundingBoxFromTwoSpheres, 0x415230, { .State = state, .Locked = locked });
    RH_ScopedInstall(IsThisVehicleSittingOnMe, 0x4152C0, { .State = state, .Locked = locked });
    RH_ScopedInstall(CheckCameraCollisionPeds, 0x415320, { .State = state, .Locked = locked });
    RH_ScopedInstall(CheckPeds, 0x4154A0, { .State = state, .Locked = locked });
    RH_ScopedGlobalInstall(ResetMadeInvisibleObjects, 0x415540, { .State = state, .Locked = locked });
    RH_ScopedInstall(SphereCastVsBBox, 0x415590, { .State = state, .Locked = locked });
    RH_ScopedInstall(RayPolyPOP, 0x415620, { .State = state, .Locked = locked });
    RH_ScopedInstall(GetPrincipleAxis, 0x4156D0, { .State = state, .Locked = locked });
    RH_ScopedInstall(PointInPoly, 0x415730, { .State = state, .Locked = locked });
    RH_ScopedInstall(Closest3, 0x415950, { .State = state, .Locked = locked });
    RH_ScopedGlobalInstall(ClosestSquaredDistanceBetweenFiniteLines, 0x415A40, { .State = state, .Locked = locked });
    RH_ScopedInstall(SphereCastVersusVsPoly, 0x415CF0, { .State = state, .Locked = locked });
    RH_ScopedInstall(DistToLine, 0x417610, { .State = state, .Locked = locked }); 
    RH_ScopedInstall(SphereCastVsSphere, 0x417F20, { .State = state, .Locked = locked });
    RH_ScopedInstall(ClosestPointOnLine, 0x417FD0, { .State = state, .Locked = locked });
    RH_ScopedInstall(ClosestPointsOnPoly, 0x418100, { .State = state, .Locked = locked });
    RH_ScopedInstall(ClosestPointOnPoly, 0x418150, { .State = state, .Locked = locked });
    RH_ScopedInstall(SphereCastVsCaches, 0x4181B0, { .State = state, .Locked = locked });
    RH_ScopedInstall(SphereCastVsEntity, 0x419F00, { .State = state, .Locked = locked });
    RH_ScopedInstall(SphereVsEntity, 0x41A5A0, { .State = state, .Locked = locked });
    RH_ScopedInstall(CheckCameraCollisionBuildings, 0x41A820, { .State = state, .Locked = locked });
    RH_ScopedInstall(CheckCameraCollisionVehicles, 0x41A990, { .State = state, .Locked = locked });
    RH_ScopedInstall(CheckCameraCollisionObjects, 0x41AB20, { .State = state, .Locked = locked });
    RH_ScopedInstall(BuildCacheOfCameraCollision, 0x41AC40, { .State = state, .Locked = locked });
    RH_ScopedInstall(CameraConeCastVsWorldCollision, 0x41B000, { .State = state, .Locked = locked });

    RH_ScopedOverloadedInstall(CalculateTrianglePlanes, "colData", 0x416330, void (*)(CCollisionData*), { .State = state, .Locked = locked });
    RH_ScopedOverloadedInstall(RemoveTrianglePlanes, "colData", 0x416400, void (*)(CCollisionData*), { .State = state, .Locked = locked });
    RH_ScopedOverloadedInstall(CalculateTrianglePlanes, "colModel", 0x418580, void (*)(CColModel*), { .State = state, .Locked = locked });
    RH_ScopedOverloadedInstall(RemoveTrianglePlanes, "colModel", 0x4185A0, void (*)(CColModel*), { .State = state, .Locked = locked });
}

void CCollision::Tests(int32 i) {
#ifdef TEST_COLLISION_FUNCS
    const auto seed = (uint32)time(nullptr) + i;
    srand(seed);
    std::cout << "CCollision::Tests seed: " << seed << std::endl;

    const auto VectorEq = [](const CVector& lhs, const CVector& rhs, float epsilon = 0.01f) {
        for (auto i = 0u; i < 3u; i++) {
            if (!approxEqual(lhs[i], rhs[i], epsilon)) {
                return false;
            }
        }
        return true;
    };

    const auto ColPointEq = [&](const CColPoint& lhs, const CColPoint& rhs) {
        return VectorEq(lhs.m_vecPoint, rhs.m_vecPoint)
            && approxEqual(lhs.m_fDepth, rhs.m_fDepth, 0.01f)
            && VectorEq(lhs.m_vecNormal, rhs.m_vecNormal);
    };

    const auto RandomVector = [](float min = -100.f, float max = 100.f) {
        return CVector{
            CGeneral::GetRandomNumberInRange(min, max),
            CGeneral::GetRandomNumberInRange(min, max),
            CGeneral::GetRandomNumberInRange(min, max)
        };
    };

    const auto RandomNormal = [&]() {
        return RandomVector().Normalized();
    };

    const auto RandomSphere = [&](float min = -100.f, float max = 100.f) {
        return CColSphere{ RandomVector(min, max), CGeneral::GetRandomNumberInRange(min, max) };
    };

    const auto RandomBox = [&](float min = -100.f, float max = 100.f) {
        CColBox cb{ RandomVector(min, max), RandomVector(min, max) };
        cb.Recalc();
        return cb;
    };

    const auto RandomLine = [&](float min = -100.f, float max = 100.f) {
        return CColLine{ RandomVector(min, max) , RandomVector(min, max) };
    };

    const auto RandomVerticalLine = [&](float min = -100.f, float max = 100.f) {
        const auto pos = RandomVector(min, max);
        return CColLine{ {pos.x, pos.y, pos.z + fabs(pos.x)}, {pos.x, pos.y, pos.z - fabs(pos.x)} };
    };

    const auto RandomTriangleVertices = [&](float min = -100.f, float max = 100.f) {
        const auto vtxA = RandomVector(min, max);
        const auto norm = RandomNormal();
        return std::array<CompressedVector, 3>{
            vtxA,
            vtxA.Cross(norm),
            norm.Cross(vtxA)
        };
    };

    const auto Test = [](auto name, auto org, auto rev, auto cmp, auto&&... args) {
        const auto orgResult = org(args...);
        const auto revResult = rev(args...);
        if (!cmp(orgResult, revResult)) {
            std::cerr << "[CCollision::Tests]: " << name << " failed. " << std::endl;
            assert(0);
        }
    };
#if 1
    // TestSphereSphere
    {
        auto sp1 = RandomSphere(), sp2 = RandomSphere();
        auto Original = plugin::CallAndReturn<bool, 0x411E70, CColSphere const&, CColSphere const&>;
        Test("TestSphereSphere", Original, TestSphereSphere, std::equal_to<>{}, sp1, sp2);
    }

    // CalculateColPointInsideBox
    {
        const auto Org = [&](auto box, auto point) {
            CColPoint cp{};
            plugin::Call<0x411EC0, CBox const&, CVector const&, CColPoint&>(box, point, cp);
            return cp;
        };

        const auto Rev = [](auto box, auto point) {
            CColPoint cp{};
            CalculateColPointInsideBox(box, point, cp);
            return cp;
        };

        Test("CalculateColPointInsideBox", Org, Rev, ColPointEq, RandomBox(), RandomVector());
    }

    // TestSphereBox
    {
        const auto Org = plugin::CallAndReturn<bool, 0x4120C0, CSphere const&, CBox const&>;
        Test("TestSphereBox", Org, TestSphereBox, std::equal_to{}, RandomSphere(), RandomBox());
    }

    // CalculateColPointInsideBox
    {
        // both will be of type pair<CColPoint, float> 
        const auto CmpEq = [&](auto o, auto r) {
            return o.second == r.second && ColPointEq(o.first, r.first);
        };

        const auto Org = [&](auto sp, auto box) {
            CColPoint cp{};
            float dist{1.f};
            plugin::Call<0x412130, CColSphere const&, CColBox const&, CColPoint&, float&>(sp, box, cp, dist);
            return std::make_pair(cp, dist);
        };

        const auto Rev = [](auto sp, auto box) {
            CColPoint cp{};
            float dist{1.f};
            ProcessSphereBox(sp, box, cp, dist);
            return std::make_pair(cp, dist);
        };

        Test("CalculateColPointInsideBox", Org, Rev, CmpEq, RandomSphere(), RandomBox());
    }

    // PointInTriangle
    {
        const CVector tri[3]{ RandomVector(), RandomVector(), RandomVector() };
        const auto Org = (bool(__stdcall*)(CVector const&, CVector const*))0x412700;
        Test("PointInTriangle", Org, PointInTriangle, std::equal_to{}, RandomVector(), tri);
    }

    // DistToLineSqr
    {
        const auto Org = plugin::CallAndReturn<float, 0x412850, CVector const*, CVector const*, CVector const*>;
        const auto ls{ RandomVector() }, le{ RandomVector() }, p{ RandomVector() };
        const auto CmpEq = [](float org, float rev) {
            return approxEqual(org, rev, 0.02f);
        };
        Test("DistToLineSqr", Org, DistToLineSqr, CmpEq, &ls, &le, &p);
    }

    // DistToMathematicalLine2D
    {
        const auto Org = plugin::CallAndReturn<float, 0x412A30, float, float, float, float, float, float>;
        const auto ls{ RandomVector() }, le{ RandomVector() }, p{ RandomVector() };
        const auto CmpEq = [](float org, float rev) {
            return approxEqual(org, rev, 0.02f);
        };
        Test("DistToLineSqr", Org, DistToMathematicalLine2D, CmpEq, ls.x, ls.y, le.x, le.y, p.x, p.y);
    }

    // ClosestSquaredDistanceBetweenFiniteLines
    {
        const auto Org = plugin::CallAndReturn<float, 0x415A40, const CVector&, const CVector&, const CVector&, const CVector&, float>;

        const auto DoLnTest = [&](CColLine lnA, CColLine lnB) {
            Test(
                "ClosestSquaredDistanceBetweenFiniteLines",
                Org, ClosestSquaredDistanceBetweenFiniteLines, std::equal_to{},
                lnA.m_vecStart, lnA.m_vecEnd,
                lnB.m_vecStart, lnB.m_vecEnd - lnB.m_vecStart, (lnB.m_vecEnd - lnB.m_vecStart).SquaredMagnitude()
            );
        };

        // Vertical Parallel lines, expected: 100 [GTA: 0]
        DoLnTest(
            { { -5.f, 0.f, 0.f, }, CVector{ -5.f, 0.f, 100.f, } },
            { { +5.f, 0.f, 0.f, }, CVector{ +5.f, 0.f, 100.f, } }
        );

        // Vertical lines intersecting at the top (line end), expected: 0 [GTA: 100]
        DoLnTest(
            { { -5.f, 0.f, 0.f, }, CVector{ +5.f, 0.f, 100.f, } },
            { { +5.f, 0.f, 0.f, }, CVector{ +5.f, 0.f, 100.f, } }
        );

        // Vertical lines intersecting at the bottom (line origin), expected: 0 [GTA: 0]
        DoLnTest(
            { { +5.f, 0.f, 0.f, }, CVector{ -5.f, 0.f, 100.f, } },
            { { +5.f, 0.f, 0.f, }, CVector{ +5.f, 0.f, 100.f, } }
        );

        // Vertical lines intersecting at middle, expected: 0 [GTA: 100]
        DoLnTest(
            { { -5.f, 0.f, 0.f, }, CVector{ +10.f, 0.f, 100.f, } },
            { { +5.f, 0.f, 0.f, }, CVector{ +5.f, 0.f, 100.f, } }
        );

        /*
        const auto lnA = RandomLine();
        const auto lnB = RandomLine();

        Test(
            "ClosestSquaredDistanceBetweenFiniteLines",
            Org, ClosestSquaredDistanceBetweenFiniteLines, std::equal_to{},
            lnA.m_vecStart, lnA.m_vecEnd,
            lnB.m_vecStart, lnB.m_vecEnd - lnB.m_vecStart, (lnB.m_vecEnd - lnB.m_vecStart).SquaredMagnitude()
        );
        */
    }

    // ProcessLineSphere
    {
        const auto Org = [&](auto line, auto sp) {
            CColPoint cp{};
            float depth{ 100.f };
            const bool s = plugin::CallAndReturn<bool, 0x412AA0, CColLine const&, CColSphere const&, CColPoint&, float&>(line, sp, cp, depth);
            return std::make_tuple(cp, depth, s);
        };

        const auto Rev = [](auto line, auto sp) {
            CColPoint cp{};
            float depth{ 100.f };
            const bool s = ProcessLineSphere(line, sp, cp, depth);
            return std::make_tuple(cp, depth, s);
        };

        const auto CmpEq = [&](auto org, auto rev) {
            auto [org_cp, org_d, org_s] = org;
            auto [rev_cp, rev_d, rev_s] = rev;
            return org_s == rev_s && approxEqual(rev_d, org_d, 0.001f) && ColPointEq(org_cp, rev_cp);
        };

        Test("ProcessLineSphere", Org, Rev, CmpEq, RandomLine(), RandomSphere());
    }

    // TestLineSphere
    {
        const auto ln = RandomLine();
        const auto sp = RandomSphere();

        const auto Org = plugin::CallAndReturn<bool, 0x417470, const CColLine&, const CColSphere&>;
        Test("TestLineSphere", Org, TestLineSphere, std::equal_to{}, ln, sp);
    }

    // TestLineBox
    {
        const auto Org = plugin::CallAndReturn<bool, 0x413070, CColLine const&, CBox const&>;
        Test("TestLineBox", Org, TestLineBox, std::equal_to{}, RandomLine(), RandomBox());
    }

    // TestVerticalLineBox
    {
        const auto Org = plugin::CallAndReturn<bool, 0x413080, CColLine const&, CBox const&>;
        Test("TestVerticalLineBox", Org, TestVerticalLineBox, std::equal_to{}, RandomVerticalLine(), RandomBox());
    }

    // TestLineTriangle
    {
        const auto vtxs  = RandomTriangleVertices();
        const auto tri   = CColTriangle{ 0, 1, 2, SURFACE_CAR_PANEL, {} };
        const auto tripl = tri.GetPlane(vtxs.data());

        const auto line  = RandomLine();

        const auto Org = plugin::CallAndReturn<bool, 0x413AC0, const CColLine&, const CompressedVector*, const CColTriangle&, const CColTrianglePlane&>;
        /*
        const auto Benchmark = [&](auto fn, const char* title) {
            using namespace std::chrono;
            const auto begin = high_resolution_clock::now();
            for (auto triIdx = 0; triIdx < 100'000'000; triIdx++) {
                const auto volatile v1 = fn(line, vtxs.data(), tri, triPl);
            }
            printf("[%s]: Took %llu ms\n", title, duration_cast<milliseconds>(high_resolution_clock::now() - begin).count());
            //std::cout << "Took " << duration_cast<milliseconds>(high_resolution_clock::now() - begin) << " ms" << std::endl;
        };
        if (triIdx % 2) {
            Benchmark(TestLineTriangle, "TestLineTriangle");
            Benchmark(Org, "Org");
        } else {
            Benchmark(Org, "Org");
            Benchmark(TestLineTriangle, "TestLineTriangle");
        }
        printf("\n\n");
        */
        Test("TestLineTriangle", Org, TestLineTriangle, std::equal_to{}, line, vtxs.data(), tri, tripl);
    }

    // TestSphereTriangle
    {
        const auto sp = RandomSphere();

        const auto vtxs  = RandomTriangleVertices();
        const auto tri   = CColTriangle{ 0, 1, 2, SURFACE_CAR_PANEL, {} };
        const auto tripl = tri.GetPlane(vtxs.data());

        // Our version seems to fail sometimes, but I'v2 assume they're edge cases
        const auto Org = plugin::CallAndReturn<bool, 0x4165B0, const CColSphere&, const CompressedVector*, const CColTriangle&, const CColTrianglePlane&>;
        Test("TestSphereTriangle", Org, TestSphereTriangle, std::equal_to{}, sp, vtxs.data(), tri, tripl);
    }

    // PointInPoly
    {
        const auto vtxs = RandomTriangleVertices();
        const auto tri = CColTriangle{ 0, 1, 2, SURFACE_CAR_PANEL, {} };
        const auto tripl = tri.GetPlane(vtxs.data());
        const auto pt = RandomVector();
        const CVector ucverts[]{
            vtxs[tri.vA],
            vtxs[tri.vB],
            vtxs[tri.vC]
        };

        const auto Org = plugin::CallAndReturn<bool, 0x415730, const CVector&, const CColTriangle&, const CVector&, const CVector*>;
        Test("PointInPoly", Org, PointInPoly, std::equal_to{}, pt, tri, tripl.GetNormal(), ucverts);
    }

#endif

    // ProcessLineBox
    /*{
        const auto Org = [&](auto line, auto bb) {
            CColPoint cp{};
            float depth{ 100.f };
            const bool s = plugin::CallAndReturn<bool, 0x413100, CColLine const&, CColBox const&, CColPoint&, float&>(line, bb, cp, depth);
            return std::make_tuple(cp, depth, s);
        };

        const auto Rev = [](auto line, auto bb) {
            CColPoint cp{};
            float depth{ 100.f };
            const bool s = ProcessLineBox(line, bb, cp, depth);
            return std::make_tuple(cp, depth, s);
        };

        const auto CmpEq = [&](auto org, auto rev) {
            auto [org_cp, org_d, org_s] = org;
            auto [rev_cp, rev_d, rev_s] = rev;
            return org_s == rev_s && approxEqual(rev_d, org_d, 0.001f) && ColPointEq(org_cp, rev_cp);
        };

        Test("ProcessLineBox", Org, Rev, CmpEq, RandomLine(), RandomBox());
    }*/
#endif
}

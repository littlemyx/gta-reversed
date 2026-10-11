/*
    The sphere / line / triangle collision primitives of CCollision (stream F pilot, .notes/STREAM_F_PLAN.md).

    This translation unit is compiled for SSE2 (/arch:SSE2, see source/CMakeLists.txt) while the rest of the game is x87 (/arch:IA32, PC=24). The original runs every
    arithmetic result rounded to 24 bits with the extended exponent range; plain `float` arithmetic is bit-identical to that unless an intermediate leaves the float
    exponent range. Every function is therefore written ONCE as a template over the arithmetic type and instantiated twice:
        T = float        the fast path (SSE2 / arm64 float, no x87 at all)
        T = notsa::fp::F24   the exact model of an x87 PC24 register (a double rounded to 24 bits after every op, 11 bit exponent) - used only when the fast path
                         reports (MXCSR / FPSR flags) that an intermediate overflowed, underflowed or was denormal.
    `Spill(x)` marks the places where the original stores an intermediate to a float variable (an fstp/fld dword pair: only visible for out-of-float-range values);
    `Fl(x)` converts a T result to the float that is finally stored. The term orders are those of the exe (see the comments of the old x87 versions in git history:
    Collision.cpp before the commit that introduced this file).

    ODR WARNING: this TU must not use inline functions that contain `double` arithmetic (they would be emitted here as SSE2 COMDATs and the linker may pick them for
    the x87 TUs): only pure float helpers below.
*/
#include "StdInc.h"

#include <numbers>
#include <bit>

#include "Collision.h"
#include "ColHelpers.h"
#include "Core/Fp.h"

#if defined(_MSC_VER) && !defined(__clang__)
#pragma float_control(precise, on)
#pragma fp_contract(off)
#elif defined(__clang__)
#pragma STDC FP_CONTRACT OFF
#endif
// NOT `#pragma fenv_access(on)`: it is correct but makes the TU ~16x slower (Fp.h reads the flags through opaque NOTSA_FP_NOINLINE functions instead).
// Every Impl<float> instantiation is NOTSA_FP_NOINLINE so its FP operations are complete before the flags are read.
#define FP_NOINLINE NOTSA_FP_NOINLINE

using Shape = CCollision::DebugSettings::ShapeShapeCollision::Shape;

namespace {
using notsa::fp::F24;

inline float Spill(float x) { return x; }
inline F24   Spill(F24 x) { return F24((float)x.v); }  // fstp dword + fld dword
inline float Fl(float x) { return x; }
inline float Fl(F24 x) { return (float)x.v; }
using notsa::fp::sqrt;
using notsa::fp::abs;

template<class T> struct V3 { T x, y, z; };

//! `CVector::Normalise` (0x59C910): `sqMag <= 0` (NaN is NOT caught) sets x = 1 and returns, otherwise multiplies by 1 / sqrt(sqMag) (the reciprocal stays unspilled)
template<class T>
void Normalise(V3<T>& v) {
    const T sqMag = v.x * v.x + v.y * v.y + v.z * v.z;
    if (sqMag <= T(0.0f)) {
        v.x = T(1.0f);
        return;
    }
    const T recip = T(1.0f) / sqrt(sqMag);
    v.x = Spill(v.x * recip);
    v.y = Spill(v.y * recip);
    v.z = Spill(v.z * recip);
}

template<class T> V3<T> FromFloat(const CVector& v) { return { T(v.x), T(v.y), T(v.z) }; }
template<class T> CVector ToFloat3(const V3<T>& v) { return { Fl(v.x), Fl(v.y), Fl(v.z) }; }

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

// ---- CCollision::TestSphereSphere (0x411E70) -------------------------------------------------------------------------------------------------------------
// Everything stays in extended precision, the compare is STRICT (`FCOMPP` + `test ah, 0x41` => false on `<=` and NaN)
template<class T> FP_NOINLINE bool TestSphereSphereT(const CColSphere& s1, const CColSphere& s2) {
    const T dx = T(s1.m_vecCenter.x) - s2.m_vecCenter.x;
    const T dy = T(s1.m_vecCenter.y) - s2.m_vecCenter.y;
    const T dz = T(s1.m_vecCenter.z) - s2.m_vecCenter.z;
    const T sumR = T(s1.m_fRadius) + s2.m_fRadius;
    return sumR * sumR > dz * dz + dx * dx + dy * dy;
}

// ---- CCollision::ProcessSphereSphere (0x416450) ----------------------------------------------------------------------------------------------------------
struct SphereSphereOut { CVector point, normal; float depth, touchDistSq; };
template<class T> FP_NOINLINE bool ProcessSphereSphereT(const CColSphere& spA, const CColSphere& spB, float maxTouchDistance, SphereSphereOut& o) {
    // `d.z` is multiplied unrounded by its (rounded) spill, the sum order is z, x, y
    const T dx  = Spill(T(spA.m_vecCenter.x) - spB.m_vecCenter.x);
    const T dy  = Spill(T(spA.m_vecCenter.y) - spB.m_vecCenter.y);
    const T dzE = T(spA.m_vecCenter.z) - spB.m_vecCenter.z;
    const T dz  = Spill(dzE);

    const T u         = Spill(sqrt(dzE * dz + dx * dx + dy * dy) - spB.m_fRadius); // Unclamped touch distance
    const T depth     = T(spA.m_fRadius) - u;
    const T touchDist = u < T(0.0f) ? T(0.0f) : u;                                  // NaN stays NaN
    const T touchDistSq = touchDist * touchDist;                                    // Unrounded for the compare

    if (!(touchDistSq < T(maxTouchDistance))) { // NaN => false
        return false;
    }
    if (!(touchDist < T(spA.m_fRadius))) { // NaN => false
        return false;
    }

    V3<T> normal{ dx, dy, dz };
    Normalise(normal);

    o.point = CVector{
        Fl(T(spA.m_vecCenter.x) - normal.x * touchDist),
        Fl(T(spA.m_vecCenter.y) - normal.y * touchDist),
        Fl(T(spA.m_vecCenter.z) - Spill(normal.z * touchDist)) // 0x41651B: the z product is spilled to a float
    };
    o.normal      = ToFloat3(normal);
    o.depth       = Fl(depth);
    o.touchDistSq = Fl(touchDistSq);
    return true;
}

// ---- CCollision::TestLineSphere (0x417470) ---------------------------------------------------------------------------------------------------------------
template<class T> FP_NOINLINE bool TestLineSphereT(const CColLine& line, const CColSphere& sphere) {
    // d = end - start (x, y spilled to floats, z is spilled but multiplied unrounded by the spill)
    const T dxF = Spill(T(line.m_vecEnd.x) - line.m_vecStart.x);
    const T dyF = Spill(T(line.m_vecEnd.y) - line.m_vecStart.y);
    const T dzE = T(line.m_vecEnd.z) - line.m_vecStart.z;
    const T dzF = Spill(dzE);
    const T len = Spill(sqrt(dzE * dzF + dxF * dxF + dyF * dyF));

    // m = center - start (extended)
    const T mx = T(sphere.m_vecCenter.x) - line.m_vecStart.x;
    const T my = T(sphere.m_vecCenter.y) - line.m_vecStart.y;
    const T mz = T(sphere.m_vecCenter.z) - line.m_vecStart.z;
    const T r  = T(sphere.m_fRadius);

    if (len < T(0.000001f)) { // Degenerate line (a point) => is it inside the sphere
        const T m2 = mz * mz + my * my + mx * mx;
        return r * r >= m2; // `FCOMPP` + `test ah, 1` => false if `r^2 < m2` or NaN
    }

    // Quadratic: A t^2 + B t + C = 0
    const T B = Spill((mz * dzF + mx * dxF + my * dyF) * T(-2.0f));
    const T A = Spill(len * len);

    const auto& c = sphere.m_vecCenter;
    const auto& s = line.m_vecStart;
    const T cc = T(c.z) * c.z + T(c.y) * c.y + T(c.x) * c.x;
    const T ss = T(s.z) * s.z + T(s.y) * s.y + T(s.x) * s.x;
    const T sc = T(s.z) * c.z + T(s.x) * c.x + T(s.y) * c.y;
    const T C  = ((ss + cc) - (sc + sc)) - r * r;

    const T disc = Spill(B * B - (C * A) * T(4.0f));
    if (disc < T(0.0f)) { // NaN passes
        return false;
    }

    const T t = (-B - sqrt(disc)) / (T(2.0f) * A);
    return !(t < T(0.0f)) && !(t > T(1.0f));
}

// ---- CCollision::ProcessLineSphere (0x412AA0) ------------------------------------------------------------------------------------------------------------
struct LineSphereOut { CVector point, normal; float t; };
template<class T> FP_NOINLINE bool ProcessLineSphereT(const CColLine& line, const CColSphere& sphere, float depth, LineSphereOut& o) {
    // Quadratic: (d.d)t^2 - 2(d.s)t + (s.s - r^2) = 0, where d = line dir (unnormalized), s = sphere center - line start
    // d = end - start (extended)
    const T dx = T(line.m_vecEnd.x) - line.m_vecStart.x;
    const T dy = T(line.m_vecEnd.y) - line.m_vecStart.y;
    const T dz = T(line.m_vecEnd.z) - line.m_vecStart.z;
    const T a  = Spill(dz * dz + dx * dx + dy * dy);

    // s = center - start (x, y spilled to floats; z is spilled, but the dot product uses the unrounded one)
    const T sx  = Spill(T(sphere.m_vecCenter.x) - line.m_vecStart.x);
    const T sy  = Spill(T(sphere.m_vecCenter.y) - line.m_vecStart.y);
    const T szE = T(sphere.m_vecCenter.z) - line.m_vecStart.z;
    const T sz  = Spill(szE);

    const T B  = -(szE * dz + sy * dy + sx * dx);
    const T s2 = sz * sz + sy * sy + sx * sx;
    const T r  = T(sphere.m_fRadius);

    const T disc = B * B - (s2 - r * r) * a;
    if (disc < T(0.0f)) { // NaN passes
        return false;
    }

    const T tE = (-B - sqrt(disc)) / a; // The first compare (`< 0`) uses the unrounded value, the others the float spill
    const T t  = Spill(tE);
    if (tE < T(0.0f) || t > T(1.0f) || !(t < T(depth))) {
        return false;
    }

    // Intersection point
    const T dzF = Spill(dz);
    const V3<T> point{
        Spill(Spill(dx * t) + line.m_vecStart.x),
        Spill(dy * t + line.m_vecStart.y),
        Spill(dzF * t + line.m_vecStart.z)
    };

    V3<T> normal{ Spill(point.x - sphere.m_vecCenter.x), Spill(point.y - sphere.m_vecCenter.y), Spill(point.z - sphere.m_vecCenter.z) };
    Normalise(normal);

    o.point  = ToFloat3(point);
    o.normal = ToFloat3(normal);
    o.t      = Fl(t);
    return true;
}

// ---- line vs triangle (0x413AC0, 0x4140F0, 0x4147E0) -----------------------------------------------------------------------------------------------------
/*!
* The 2D "is the intersection point inside of the triangle" test shared by `TestLineTriangle`, `ProcessLineTriangle` and `ProcessVerticalLineTriangle`.
* The exe projects onto the plane that is perpendicular to the dominant axis of the normal. For odd orientations vertices B and C are swapped.
* Everything stays in extended precision, a NaN passes every check (so the point is considered inside).
*/
template<class T>
bool LineTriangle_IsPointInside2D(CColTrianglePlane::Orientation orientation, const CVector& A, const CVector& B, const CVector& C, const V3<T>& ip) {
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

    const T ipP = P == 0 ? ip.x : P == 1 ? ip.y : ip.z;
    const T ipQ = Q == 0 ? ip.x : Q == 1 ? ip.y : ip.z;

    const T uP = ipP - A[P];
    const T uQ = ipQ - A[Q];

    if ((T(Cc[Q]) - A[Q]) * uP - (T(Cc[P]) - A[P]) * uQ < T(0.0f)) {
        return false;
    }
    if ((T(Bc[Q]) - A[Q]) * uP - (T(Bc[P]) - A[P]) * uQ > T(0.0f)) {
        return false;
    }
    return !((ipP - Cc[P]) * (T(Bc[Q]) - Cc[Q]) - (ipQ - Cc[Q]) * (T(Bc[P]) - Cc[P]) < T(0.0f));
}

/*!
* Intersect the line with the plane of a triangle.
* @returns false if both line points are on the same side of the plane
* @param outT  Line parameter of the intersection
* @param outIP Intersection point (extended precision based, only used for the 2D test)
*/
template<class T>
bool LineTriangle_IntersectPlane(const CColLine& line, const LineTriPlane& pl, T& outT, V3<T>& outIP) {
    const CVector& s = line.m_vecStart;
    const CVector& e = line.m_vecEnd;

    // Products with the start point are spilled to floats
    const T nxsx = Spill(T(pl.nx) * s.x), nysy = Spill(T(pl.ny) * s.y), nzsz = Spill(T(pl.nz) * s.z);

    const T dE = ((T(pl.nz) * e.z + T(pl.ny) * e.y + T(pl.nx) * e.x)) - pl.d;
    const T dS = ((nxsx + nysy) + nzsz) - pl.d;
    if (!(dS * dE <= T(0.0f))) { // Both points on the same side of the plane (or NaN)
        return false;
    }

    const T dx  = T(e.x) - s.x;
    const T dy  = T(e.y) - s.y;
    const T dzF = Spill(T(e.z) - s.z);

    const T num = (((T(pl.d) - nxsx) - nysy) - nzsz);
    const T den = dzF * pl.nz + dy * pl.ny + dx * pl.nx;
    const T t   = Spill(num / den);

    outT  = t;
    outIP = V3<T>{
        Spill(Spill(dx * t) + s.x),
        Spill(dy * t + s.y),
        Spill(dzF * t + s.z)
    };
    return true;
}

//! `start + (end - start) * t`, using float ops (CVector operators in the exe: 0x40FE60, 0x40FEC0, 0x40FE30)
template<class T>
CVector LineTriangle_PointAt(const CColLine& line, T t) {
    const T dx = Spill(T(line.m_vecEnd.x) - line.m_vecStart.x), dy = Spill(T(line.m_vecEnd.y) - line.m_vecStart.y), dz = Spill(T(line.m_vecEnd.z) - line.m_vecStart.z);
    const T tx = Spill(t * dx), ty = Spill(t * dy), tz = Spill(t * dz);
    return { Fl(T(line.m_vecStart.x) + tx), Fl(T(line.m_vecStart.y) + ty), Fl(T(line.m_vecStart.z) + tz) };
}

struct LineTriOut { CVector point; float t; };

template<class T> FP_NOINLINE bool TestLineTriangleT(const CColLine& line, const CompressedVector* verts, const CColTriangle& tri, const CColTrianglePlane& plane) {
    T t;
    V3<T> ip;
    if (!LineTriangle_IntersectPlane(line, LineTriPlane{ plane }, t, ip)) {
        return false;
    }
    CVector A, B, C;
    UnpackTriangle(verts, tri, A, B, C);
    return LineTriangle_IsPointInside2D(plane.m_orientation, A, B, C, ip);
}

template<class T> FP_NOINLINE bool ProcessLineTriangleT(const CColLine& line, const CompressedVector* verts, const CColTriangle& tri, const CColTrianglePlane& plane, float maxTouchDistance, LineTriOut& o) {
    T t;
    V3<T> ip;
    if (!LineTriangle_IntersectPlane(line, LineTriPlane{ plane }, t, ip)) {
        return false;
    }
    CVector A, B, C;
    UnpackTriangle(verts, tri, A, B, C);
    if (!LineTriangle_IsPointInside2D(plane.m_orientation, A, B, C, ip)) {
        return false;
    }
    if (!(t < T(maxTouchDistance))) { // NaN => false
        return false;
    }
    o.point = LineTriangle_PointAt(line, t);
    o.t     = Fl(t);
    return true;
}

template<class T> FP_NOINLINE bool ProcessVerticalLineTriangleT(const CColLine& line, const CompressedVector* verts, const CColTriangle& tri, const CColTrianglePlane& plane, float maxTouchDistance, LineTriOut& o) {
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
    const T nzsz = Spill(T(pl.nz) * s.z);
    const T dE   = ((T(pl.nz) * e.z + T(pl.ny) * e.y + T(pl.nx) * e.x)) - pl.d;
    const T dS   = ((T(pl.nx) * s.x + T(pl.ny) * s.y) + nzsz) - pl.d;
    if (!(dS * dE <= T(0.0f))) {
        return false;
    }

    const T dz  = T(e.z) - s.z;
    const T num = (((T(pl.d) - T(pl.nx) * s.x) - T(pl.ny) * s.y) - nzsz);
    const T t   = Spill(num / (T(pl.nz) * dz));

    // The intersection (only z needs to be calculated)
    const V3<T> ip{ T(s.x), T(s.y), Spill(t * dz + s.z) };
    if (!LineTriangle_IsPointInside2D(plane.m_orientation, A, B, C, ip)) {
        return false;
    }
    if (!(t < T(maxTouchDistance))) { // NaN => false
        return false;
    }
    o.point = LineTriangle_PointAt(line, t);
    o.t     = Fl(t);
    return true;
}

// ---- CCollision::TestSphereTriangle (0x4165B0) -----------------------------------------------------------------------------------------------------------
// The exe calculates the closest distance of the sphere's center to the triangle in a coordinate system `(e, M)` in the triangle's plane
// (e = unit vector of A->B, M = e x N), where A is at the origin, B at (|AB|, 0), C at (Ce, Cm), and the center is at (Pe, Pm).
template<class T> FP_NOINLINE bool TestSphereTriangleT(const CColSphere& sphere, const CompressedVector* verts, const CColTriangle& tri, const CColTrianglePlane& plane) {
    const auto& c = sphere.m_vecCenter;
    const T r = T(sphere.m_fRadius);

    const float nx = plane.m_normal.x, ny = plane.m_normal.y, nz = plane.m_normal.z;
    const float planeOffset = (float)plane.m_normalOffset;

    // Signed distance of the center from the plane
    const T planeDistE = ((T(nx) * c.x + T(nz) * c.z) + T(ny) * c.y) - planeOffset;
    if (abs(planeDistE) > r) { // NaN passes
        return false;
    }
    const T planeDist    = Spill(planeDistE);
    const T absPlaneDist = Spill(abs(planeDistE));

    CVector A, B, C;
    UnpackTriangle(verts, tri, A, B, C);

    // Unit vector A->B
    const T abx = T(B.x) - A.x, aby = T(B.y) - A.y, abz = T(B.z) - A.z;
    const T abzF = Spill(abz);
    const T len  = Spill(sqrt(abz * abzF + aby * aby + abx * abx));
    const T invLen = Spill(T(1.0f) / len);
    const T ex = Spill(abx * invLen), ey = Spill(aby * invLen), ez = Spill(abzF * invLen);

    // M = e x N (CrossProduct: 0x59C730)
    const T mx = Spill(T(nz) * ey - ez * ny);
    const T my = Spill(ez * nx - T(nz) * ex);
    const T mz = Spill(ex * ny - T(nx) * ey);

    // C in the plane's coordinate system
    const T cax = T(C.x) - A.x, cay = T(C.y) - A.y, caz = T(C.z) - A.z;
    const T Ce  = Spill((cay * ey + cax * ex) + caz * ez);
    const T Cm  = Spill((cax * mx + mz * caz) + my * cay);

    // The sphere's center in the plane's coordinate system
    const T pax = T(c.x) - A.x, pay = T(c.y) - A.y, paz = T(c.z) - A.z;
    const T Pe  = Spill((pax * ex + paz * ez) + pay * ey);
    const T Pm  = Spill((pax * mx + paz * mz) + pay * my);

    // Which side of the 3 edges is the center on? (all 3 => inside of the triangle)
    int32 numPassed = 0;
    bool  abPassed = false, acPassed = false;

    const T abSide = Pm * len - Pe * T(0.0f);
    const T abSideF = Spill(abSide);
    if (abSide >= T(0.0f)) { // NaN fails
        numPassed++;
        abPassed = true;
    }

    const T pec = Spill(Pe * Cm); // Pe * Cm
    const T pmc = Spill(Pm * Ce); // Pm * Ce
    if (pec - pmc >= T(0.0f)) { // NaN fails
        numPassed++;
        acPassed = true;
    }

    const T ceMinusLen = Spill(Ce - len);
    const T peMinusLen = Spill(Pe - len);
    if (ceMinusLen * Pm - peMinusLen * Cm >= T(0.0f)) { // NaN fails
        numPassed++;
    }

    // Distance to a vertex (`CVector::Magnitude` of `center - vertex`, order x, y, z)
    const auto DistToVertex = [&](const CVector& v) {
        const T dx = Spill(T(c.x) - v.x), dy = Spill(T(c.y) - v.y), dz = Spill(T(c.z) - v.z);
        return sqrt(dx * dx + dy * dy + dz * dz);
    };

    // Distance to the infinite edge line + the plane distance
    const auto DistToEdgeLine = [&](T perp) {
        return sqrt(perp * perp + planeDist * planeDist);
    };

    // Projection (of the center) onto an edge: <= 0 => start vertex, >= 1 => end vertex (NaN => perpendicular)
    T dist;
    switch (numPassed) {
    case 0: // Not possible geometrically
        return false;
    case 3: // Inside of the triangle => the distance to the plane
        dist = absPlaneDist;
        break;
    case 2: {
        if (!abPassed) { // Edge AB
            const T abLenSq = Spill(len * len);
            const T s = (Pe * len + Pm * T(0.0f)) / abLenSq;
            if (s <= T(0.0f)) {
                dist = DistToVertex(A);
            } else if (s >= T(1.0f)) {
                dist = DistToVertex(B);
            } else {
                dist = DistToEdgeLine(abSideF / sqrt(abLenSq));
            }
        } else if (!acPassed) { // Edge AC
            const T acLenSq = Spill(Cm * Cm + Ce * Ce);
            const T s = (Pm * Cm + Pe * Ce) / acLenSq;
            if (s <= T(0.0f)) {
                dist = DistToVertex(A);
            } else if (s >= T(1.0f)) {
                dist = DistToVertex(C);
            } else {
                dist = DistToEdgeLine((pmc - pec) / sqrt(acLenSq));
            }
        } else { // Edge BC, from B to C: (Ce - len, Cm)
            const T bcX = ceMinusLen, bcY = Cm;
            const T bcLenSq = Spill(bcY * bcY + bcX * bcX);
            const T s = (peMinusLen * bcX + Pm * bcY) / bcLenSq;
            if (s <= T(0.0f)) {
                dist = DistToVertex(B);
            } else if (s >= T(1.0f)) {
                dist = DistToVertex(C);
            } else {
                dist = DistToEdgeLine((Pm * bcX - peMinusLen * bcY) / sqrt(bcLenSq));
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

// ---- CCollision::ProcessSphereTriangle (0x416BA0) --------------------------------------------------------------------------------------------------------
struct SphereTriOut { CVector point, normal; float depth, distSq; };
template<class T> FP_NOINLINE bool ProcessSphereTriangleT(const CColSphere& sphere, const CompressedVector* verts, const CColTriangle& tri, const CColTrianglePlane& plane, float maxTouchDistance, SphereTriOut& o) {
    // Same method as `TestSphereTriangle`, but with slightly different term orders, and it also calculates the closest point.
    const auto& c = sphere.m_vecCenter;
    const T r = T(sphere.m_fRadius);

    const float nx = plane.m_normal.x, ny = plane.m_normal.y, nz = plane.m_normal.z;
    const float planeOffset = (float)plane.m_normalOffset;

    // Signed distance of the center from the plane
    const T planeDistE = ((T(nz) * c.z + T(ny) * c.y) + T(nx) * c.x) - planeOffset;
    if (abs(planeDistE) > r) { // NaN passes
        return false;
    }
    const T planeDist    = Spill(planeDistE);
    const T absPlaneDist = Spill(abs(planeDistE));
    const T planeDistSqE = planeDist * planeDist;
    const T planeDistSq  = Spill(planeDistSqE);
    if (planeDistSqE > T(maxTouchDistance)) { // NaN passes
        return false;
    }

    CVector A, B, C;
    UnpackTriangle(verts, tri, A, B, C);

    // Unit vector A->B (and the float versions of AB and AC)
    const T abx = T(B.x) - A.x, aby = T(B.y) - A.y, abz = T(B.z) - A.z;
    const V3<T> abF{ Spill(abx), Spill(aby), Spill(abz) };
    const T len    = Spill(sqrt(abx * abx + abF.z * abF.z + aby * aby));
    const T invLen = Spill(T(1.0f) / len);
    const T ex = Spill(abx * invLen), ey = Spill(aby * invLen), ez = Spill(abF.z * invLen);

    // M = e x N (CrossProduct: 0x59C730)
    const T mx = Spill(T(nz) * ey - ez * ny);
    const T my = Spill(ez * nx - T(nz) * ex);
    const T mz = Spill(ex * ny - T(nx) * ey);

    // C in the plane's coordinate system
    const T cax = T(C.x) - A.x, cay = T(C.y) - A.y, caz = T(C.z) - A.z;
    const V3<T> acF{ Spill(cax), Spill(cay), Spill(caz) };
    const T Ce = Spill((cax * ex + caz * ez) + cay * ey);
    const T Cm = Spill((cax * mx + mz * caz) + my * cay);

    // The sphere's center in the plane's coordinate system
    const T pax = T(c.x) - A.x, pay = T(c.y) - A.y, paz = T(c.z) - A.z;
    const T Pe  = Spill((pax * ex + paz * ez) + pay * ey);
    const T Pm  = Spill((pax * mx + paz * mz) + pay * my);

    // Which side of the 3 edges is the center on? (all 3 => inside of the triangle)
    int32 numPassed = 0;
    bool  abPassed = false, acPassed = false;

    const T abSide  = Pm * len - Pe * T(0.0f);
    const T abSideF = Spill(abSide);
    if (abSide >= T(0.0f)) { // NaN fails
        numPassed++;
        abPassed = true;
    }

    const T pec = Spill(Pe * Cm);
    const T pmc = Spill(Pm * Ce);
    if (pec - pmc >= T(0.0f)) { // NaN fails
        numPassed++;
        acPassed = true;
    }

    const T ceMinusLen = Spill(Ce - len);
    const T peMinusLen = Spill(Pe - len);
    if (ceMinusLen * Pm - peMinusLen * Cm >= T(0.0f)) { // NaN fails
        numPassed++;
    }

    // Distance to the closest point (`CVector::Magnitude` of `center - point`, order x, y, z), rounded to a float
    const auto DistTo = [&](const V3<T>& v) {
        const T dx = Spill(T(c.x) - v.x), dy = Spill(T(c.y) - v.y), dz = Spill(T(c.z) - v.z);
        return Spill(sqrt(dx * dx + dy * dy + dz * dz));
    };

    // `from + dir * s` using float ops (0x40FEC0 + 0x40FE30)
    const auto PointOnEdge = [](const V3<T>& from, const V3<T>& dir, T s) {
        const T dx = Spill(s * dir.x), dy = Spill(s * dir.y), dz = Spill(s * dir.z);
        return V3<T>{ Spill(from.x + dx), Spill(from.y + dy), Spill(from.z + dz) };
    };

    // Distance to the edge line + the plane distance (the exe uses the squared plane distance rounded to a float)
    const auto DistToEdgeLine = [&](T perp) {
        return Spill(sqrt(perp * perp + planeDistSq));
    };

    const V3<T> Av = FromFloat<T>(A), Bv = FromFloat<T>(B), Cv = FromFloat<T>(C);
    V3<T> closest;
    T     dist;
    switch (numPassed) {
    case 3: { // Inside of the triangle => the closest point is the center projected onto the plane
        dist = absPlaneDist;
        closest = V3<T>{
            Spill(T(c.x) - T(nx) * planeDist),
            Spill(T(c.y) - T(ny) * planeDist),
            Spill(T(c.z) - Spill(T(nz) * planeDist)) // 0x416F04..0x416F0F: the z product is spilled to a float [esp+0x50], x and y stay unrounded
        };
        break;
    }
    case 2: {
        if (!abPassed) { // Edge AB
            const T abLenSq = Spill(len * len);
            const T sE      = (Pe * len + Pm * T(0.0f)) / abLenSq; // Compared unrounded with 0, but spilled to a float for the rest
            const T s       = Spill(sE);
            if (sE <= T(0.0f)) {
                closest = Av;
                dist    = DistTo(Av);
            } else if (s >= T(1.0f)) {
                closest = Bv;
                dist    = DistTo(Bv);
            } else {
                dist    = DistToEdgeLine(abSideF / sqrt(abLenSq));
                closest = PointOnEdge(Av, abF, s);
            }
        } else if (!acPassed) { // Edge AC
            const T acLenSq = Spill(Cm * Cm + Ce * Ce);
            const T sE      = (Pm * Cm + Pe * Ce) / acLenSq;
            const T s       = Spill(sE);
            if (sE <= T(0.0f)) {
                closest = Av;
                dist    = DistTo(Av);
            } else if (s >= T(1.0f)) {
                closest = Cv;
                dist    = DistTo(Cv);
            } else {
                dist    = DistToEdgeLine((pmc - pec) / sqrt(acLenSq));
                closest = PointOnEdge(Av, acF, s);
            }
        } else { // Edge BC, from B to C: (Ce - len, Cm)
            const T bcX = ceMinusLen, bcY = Cm;
            const T bcLenSq = Spill(bcY * bcY + bcX * bcX);
            const T sE      = (peMinusLen * bcX + Pm * bcY) / bcLenSq;
            const T s       = Spill(sE);
            if (sE <= T(0.0f)) {
                closest = Bv;
                dist    = DistTo(Bv);
            } else if (s >= T(1.0f)) {
                closest = Cv;
                dist    = DistTo(Cv);
            } else {
                dist = DistToEdgeLine((Pm * bcX - peMinusLen * bcY) / sqrt(bcLenSq));
                const V3<T> bc{ Spill(Cv.x - Bv.x), Spill(Cv.y - Bv.y), Spill(Cv.z - Bv.z) };
                closest = PointOnEdge(Bv, bc, s);
            }
        }
        break;
    }
    case 1: // Closest to a vertex
        closest = abPassed ? Cv : acPassed ? Bv : Av;
        dist    = DistTo(closest);
        break;
    default: // 0: Not possible geometrically. NOTSA: The original uses uninitialized data here
        return false;
    }

    const T distSq = Spill(dist * dist);
    if (!(dist < r) || !(distSq < T(maxTouchDistance))) { // NaN => false
        return false;
    }

    V3<T> normal{ Spill(T(c.x) - closest.x), Spill(T(c.y) - closest.y), Spill(T(c.z) - closest.z) };
    Normalise(normal);

    o.point  = ToFloat3(closest);
    o.normal = ToFloat3(normal);
    o.depth  = Fl(r - dist);
    o.distSq = Fl(distSq);
    return true;
}

} // namespace

// ---- members --------------------------------------------------------------------------------------------------------------------------------------------
// 0x411E70
bool CCollision::TestSphereSphere(CColSphere const& sphere1, CColSphere const& sphere2) { // NOTE: it's a plain __cdecl (`ret`)
    ZoneScoped;

    notsa::fp::RangeGuard g;
    const bool res = TestSphereSphereT<float>(sphere1, sphere2);
    if (g.Clean(&res)) {
        return res;
    }
    ++notsa::fp::SlowPathCount();
    return TestSphereSphereT<F24>(sphere1, sphere2);
}

// 0x416450
bool CCollision::ProcessSphereSphere(const CColSphere& spA, const CColSphere& spB, CColPoint& colPoint, float& maxTouchDistance) {
    ZoneScoped;

    SphereSphereOut o;
    notsa::fp::RangeGuard g;
    bool res = ProcessSphereSphereT<float>(spA, spB, maxTouchDistance, o);
    if (!g.Clean(&res, &o)) {
        ++notsa::fp::SlowPathCount();
        res = ProcessSphereSphereT<F24>(spA, spB, maxTouchDistance, o);
    }
    if (!res) {
        return false;
    }

    colPoint.m_vecPoint  = o.point;
    colPoint.m_vecNormal = o.normal;

    colPoint.m_nSurfaceTypeA = spA.m_Surface.m_nMaterial;
    colPoint.m_nPieceTypeA   = spA.m_Surface.m_nPiece;
    colPoint.m_nLightingA    = spA.m_Surface.m_nLighting;

    colPoint.m_nSurfaceTypeB = spB.m_Surface.m_nMaterial;
    colPoint.m_nPieceTypeB   = spB.m_Surface.m_nPiece;
    colPoint.m_nLightingB    = spB.m_Surface.m_nLighting;

    colPoint.m_fDepth = o.depth;

    maxTouchDistance = o.touchDistSq;

    return true;
}

// 0x417470
bool CCollision::TestLineSphere(const CColLine& line, const CColSphere& sphere) {
    ZoneScoped;

    // NOTSA: Debug setting (enabled by default)
    if (!s_DebugSettings.ShapeShapeCollision.IsEnabled(Shape::SSPHERE, Shape::SLINE)) {
        return false;
    }

    notsa::fp::RangeGuard g;
    const bool res = TestLineSphereT<float>(line, sphere);
    if (g.Clean(&res)) {
        return res;
    }
    ++notsa::fp::SlowPathCount();
    return TestLineSphereT<F24>(line, sphere);
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

    LineSphereOut o;
    notsa::fp::RangeGuard g;
    bool res = ProcessLineSphereT<float>(line, sphere, depth, o);
    if (!g.Clean(&res, &o)) {
        ++notsa::fp::SlowPathCount();
        res = ProcessLineSphereT<F24>(line, sphere, depth, o);
    }
    if (!res) {
        return false;
    }

    colPoint.m_vecPoint  = o.point;
    colPoint.m_vecNormal = o.normal;

    // Original copies all 3 bytes (material, piece, lighting) of the sphere's surface to side B, side A's type and piece are zeroed (A's lighting is left untouched)
    colPoint.m_nSurfaceTypeB = sphere.m_Surface.m_nMaterial;
    colPoint.m_nPieceTypeB   = sphere.m_Surface.m_nPiece;
    colPoint.m_nLightingB    = sphere.m_Surface.m_nLighting;
    colPoint.m_nSurfaceTypeA = {};
    colPoint.m_nPieceTypeA   = 0;

    depth = o.t;

    ms_iProcessLineNumCrossings += 2;

    return true;
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

    notsa::fp::RangeGuard g;
    const bool res = TestLineTriangleT<float>(line, verts, tri, plane);
    if (g.Clean(&res)) {
        return res;
    }
    ++notsa::fp::SlowPathCount();
    return TestLineTriangleT<F24>(line, verts, tri, plane);
}

namespace {
//! The colpoint output of `ProcessLineTriangle` / `ProcessVerticalLineTriangle`
void LineTriangle_StoreResult(
    const LineTriOut& o, const LineTriPlane& pl, const CColTriangle& tri,
    const CompressedVector* verts,
    CColPoint& colPoint, float& maxTouchDistance, CStoredCollPoly* collPoly
) {
    colPoint.m_vecPoint  = o.point;
    colPoint.m_vecNormal = CVector{ pl.nx, pl.ny, pl.nz };

    colPoint.m_nSurfaceTypeB = tri.m_nMaterial;
    colPoint.m_nPieceTypeB   = 0;
    colPoint.m_nLightingB    = tri.m_nLight;
    colPoint.m_nSurfaceTypeA = SURFACE_DEFAULT; // NOTE: Lighting A is left untouched
    colPoint.m_nPieceTypeA   = 0;

    if (collPoly) {
        CVector A, B, C;
        UnpackTriangle(verts, tri, A, B, C);
        collPoly->verts[0] = A;
        collPoly->verts[1] = B;
        collPoly->verts[2] = C;
        collPoly->valid    = true;
        // Original (0x4147A5): `movzx edx, byte [tri+7]; mov dword [poly+0x28], edx` - the whole dword is written, the 3 bytes after the 1 byte lighting are zeroed
        *reinterpret_cast<uint32*>(&collPoly->ligthing) = tri.m_nLight.value;
    }

    maxTouchDistance = o.t;
}
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

    LineTriOut o;
    notsa::fp::RangeGuard g;
    bool res = ProcessLineTriangleT<float>(line, verts, tri, plane, maxTouchDistance, o);
    if (!g.Clean(&res, &o)) {
        ++notsa::fp::SlowPathCount();
        res = ProcessLineTriangleT<F24>(line, verts, tri, plane, maxTouchDistance, o);
    }
    if (!res) {
        return false;
    }
    LineTriangle_StoreResult(o, LineTriPlane{ plane }, tri, verts, colPoint, maxTouchDistance, collPoly);
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

    LineTriOut o;
    notsa::fp::RangeGuard g;
    bool res = ProcessVerticalLineTriangleT<float>(line, verts, tri, plane, maxTouchDistance, o);
    if (!g.Clean(&res, &o)) {
        ++notsa::fp::SlowPathCount();
        res = ProcessVerticalLineTriangleT<F24>(line, verts, tri, plane, maxTouchDistance, o);
    }
    if (!res) {
        return false;
    }
    LineTriangle_StoreResult(o, LineTriPlane{ plane }, tri, verts, colPoint, maxTouchDistance, collPoly);
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

    notsa::fp::RangeGuard g;
    const bool res = TestSphereTriangleT<float>(sphere, verts, tri, plane);
    if (g.Clean(&res)) {
        return res;
    }
    ++notsa::fp::SlowPathCount();
    return TestSphereTriangleT<F24>(sphere, verts, tri, plane);
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

    SphereTriOut o;
    notsa::fp::RangeGuard g;
    bool res = ProcessSphereTriangleT<float>(sphere, verts, tri, plane, maxTouchDistance, o);
    if (!g.Clean(&res, &o)) {
        ++notsa::fp::SlowPathCount();
        res = ProcessSphereTriangleT<F24>(sphere, verts, tri, plane, maxTouchDistance, o);
    }
    if (!res) {
        return false;
    }

    colPoint.m_vecPoint  = o.point;
    colPoint.m_vecNormal = o.normal;

    // NOTE: Lighting B is not written by the original
    colPoint.m_nSurfaceTypeA = sphere.m_Surface.m_nMaterial;
    colPoint.m_nPieceTypeA   = sphere.m_Surface.m_nPiece;
    colPoint.m_nLightingA    = sphere.m_Surface.m_nLighting;
    colPoint.m_nSurfaceTypeB = tri.m_nMaterial;
    colPoint.m_nPieceTypeB   = 0;
    colPoint.m_fDepth        = o.depth;

    maxTouchDistance = o.distSq;

    return true;
}

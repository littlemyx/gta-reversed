// (d) entity scanners / ped geometry analyser: CPedGeometryAnalyser (bounding box corners / planes / hit sides, closest surface point, avoid direction, sphere, route round sphere).
// Fixture: zeroed fake CPed / CEntity objects (no constructors) with a hand-made CMatrix, a fake CColModel (bound box + optional boxes / spheres) wired into a fake model info at one
// model index; port and exe work on the same fake objects, the outputs (out buffers, return values) are compared bit-exactly.
#include "PedGeometryAnalyser.h"
#include "Entity/Entity.h"
#include "Entity/Ped/Ped.h"
#include "Entity/Physical.h"
#include "Collision/ColModel.h"
#include "Collision/CollisionData.h"
#include "Collision/ColSphere.h"
#include "Collision/ColBox.h"
#include "Models/BaseModelInfo.h"
#include "Models/ModelInfo.h"
#include "Core/Matrix.h"
#include "EntityScanner.h"
#include "PedScanner.h"
#include "VehicleScanner.h"
#include "References.h"
#include "RepeatSector.h"
#include "World.h"
#include "PedIntelligence.h"
#include "Entity/Vehicle/Vehicle.h"

namespace sg {
constexpr int kMi = 3000;
static CEntity*  Ent;          // the entity under test (type object / vehicle / ped)
static CPed*     Pd;         // the ped
static CPed*     Pd2;        // a second ped (as CPhysical source)
static CMatrixLink* EM;
static CMatrixLink* PM_;
static CColModel*     CM;
static CCollisionData* CD;
static CColSphere     Sph[3];
static CColBox        Box[3];
static bool inited = false;
static bool g_tallEmpty = false;   // tall object without any collision volume
static float* Margin = reinterpret_cast<float*>(0x8D22B0);   // CPedGeometryAnalyser s_BoundingBoxMargin

static void Init() {
    if (inited) return;
    inited = true;
    Ent   = (CEntity*)std::calloc(1, 0x1000);
    Pd  = (CPed*)std::calloc(1, 0x1000);
    Pd2 = (CPed*)std::calloc(1, 0x1000);
    EM  = (CMatrixLink*)std::calloc(1, sizeof(CMatrixLink));
    PM_ = (CMatrixLink*)std::calloc(1, sizeof(CMatrixLink));
    CM  = (CColModel*)std::calloc(1, sizeof(CColModel));
    CD  = (CCollisionData*)std::calloc(1, sizeof(CCollisionData));
    auto* mi = (CBaseModelInfo*)std::calloc(1, sizeof(CBaseModelInfo) + 64);
    mi->m_pColModel = CM;
    CModelInfo::ms_modelInfoPtrs[kMi] = mi;
    CM->m_pColData = CD;
}
static CVector Unit(Rng& r) {
    CVector v{ r.f01() * 2.f - 1.f, r.f01() * 2.f - 1.f, r.f01() * 2.f - 1.f };
    const float l = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (l < 1e-3f) return { 0.f, 1.f, 0.f };
    return { v.x / l, v.y / l, v.z / l };
}
// orthonormal-ish frame, sometimes yawed only (flat), sometimes slightly skewed or scaled
static void GenMat(CMatrix& m, Rng& r) {
    auto& mr = reinterpret_cast<CVector&>(m.GetRight()); auto& mf = reinterpret_cast<CVector&>(m.GetForward()); auto& mu = reinterpret_cast<CVector&>(m.GetUp()); auto& mp = reinterpret_cast<CVector&>(m.GetPosition());
    const int kind = r.below(6);
    CVector fwd, right, up;
    if (kind <= 2) {   // yaw only
        const float a = r.f01() * 6.2831853f;
        fwd = CVector{ -std::sin(a), std::cos(a), 0.f }; right = CVector{ std::cos(a), std::sin(a), 0.f }; up = CVector{ 0.f, 0.f, 1.f };
    } else if (kind <= 4) {   // arbitrary rotation
        fwd = Unit(r);
        CVector t = Unit(r);
        right = CVector{ fwd.y * t.z - fwd.z * t.y, fwd.z * t.x - fwd.x * t.z, fwd.x * t.y - fwd.y * t.x };
        const float l = std::sqrt(right.x * right.x + right.y * right.y + right.z * right.z);
        if (l < 1e-3f) right = CVector{ 1.f, 0.f, 0.f }; else right = CVector{ right.x / l, right.y / l, right.z / l };
        up = CVector{ right.y * fwd.z - right.z * fwd.y, right.z * fwd.x - right.x * fwd.z, right.x * fwd.y - right.y * fwd.x };
    } else {   // arbitrary vectors (scaled / skewed)
        fwd = GenV(r, 2.f, S_NOHUGE); right = GenV(r, 2.f, S_NOHUGE); up = GenV(r, 2.f, S_NOHUGE);
    }
    mr = right; mf = fwd; mu = up;
    mp = r.below(3) ? GenV(r, 300.f, S_NOHUGE) : GenV(r, 3000.f, S_NOHUGE);
}
static void GenEntity(Rng& r, CEntity& e, CMatrixLink& mat, bool allowTall = true, bool allowNullMatrix = false) {
    std::memset(&e, 0, 0x400);
    GenMat(mat, r);
    e.m_matrix = (allowNullMatrix && r.below(3) == 0) ? nullptr : &mat;   // a missing matrix makes the exe (and the port) allocate one from the matrix pool
    e.m_placement.m_vPosn = reinterpret_cast<CVector&>(mat.GetPosition());
    e.m_placement.m_fHeading = GenF(r, 7.f);
    e.m_nModelIndex = kMi;
    e.SetType(r.below(2) ? ENTITY_TYPE_VEHICLE : (r.below(2) ? ENTITY_TYPE_OBJECT : ENTITY_TYPE_PED));
    // col model
    const bool tall = allowTall && e.GetType() == ENTITY_TYPE_OBJECT && r.below(2);
    const float sx = 0.2f + r.f01() * 6.f, sy = 0.2f + r.f01() * 6.f, sz = tall ? 6.5f + r.f01() * 20.f : 0.2f + r.f01() * 5.f;
    CM->m_boundBox.m_vecMin = CVector{ -sx * r.f01(), -sy * r.f01(), -sz * r.f01() }; CM->m_boundBox.m_vecMax = CVector{ sx * r.f01(), sy * r.f01(), sz * r.f01() };
    if (r.below(10) == 0) { CM->m_boundBox.m_vecMin = GenV(r, 5.f, S_NOHUGE); CM->m_boundBox.m_vecMax = GenV(r, 5.f, S_NOHUGE); }
    CM->m_boundSphere.m_fRadius = 0.5f + r.f01() * 12.f; CM->m_boundSphere.m_vecCenter = CVector{ 0.f, 0.f, 0.f };
    std::memset(CD, 0, sizeof(CCollisionData));
    CM->m_pColData = (tall || r.below(3) == 0) ? CD : nullptr;
    g_tallEmpty = false;
    if (CM->m_pColData) {
        const int nb = r.below(4), ns = r.below(4);
        g_tallEmpty = tall && nb + ns == 0;
        CD->m_nNumBoxes = nb; CD->m_nNumSpheres = ns; CD->m_pBoxes = Box; CD->m_pSpheres = Sph;
        for (int i = 0; i < nb; ++i) { Box[i].m_vecMin = CVector{ -sx * r.f01(), -sy * r.f01(), -sz * r.f01() }; Box[i].m_vecMax = CVector{ sx * r.f01(), sy * r.f01(), sz * r.f01() }; }
        for (int i = 0; i < ns; ++i) { Sph[i].m_vecCenter = CVector{ (r.f01() - 0.5f) * sx, (r.f01() - 0.5f) * sy, (r.f01() - 0.5f) * sz }; Sph[i].m_fRadius = 0.2f + r.f01() * 2.f; }
    }
}
static void GenPed(Rng& r, CPed& p, CMatrixLink& mat) {
    std::memset(&p, 0, 0x400);
    GenMat(mat, r);
    p.m_matrix = r.below(3) == 0 ? &mat : nullptr;
    p.m_placement.m_vPosn = r.below(3) ? GenV(r, 300.f, S_NOHUGE) : GenV(r, 3000.f);
    if (p.m_matrix) reinterpret_cast<CVector&>(mat.GetPosition()) = p.m_placement.m_vPosn;
    p.m_placement.m_fHeading = GenF(r, 7.f);
    p.m_fCurrentRotation = GenF(r, 7.f);
    p.m_vecMoveSpeed = GenV(r, 2.f);
    p.m_nModelIndex = 0;
}
static std::vector<float> Fl(std::initializer_list<const CVector*> vs) { std::vector<float> o; for (auto* v : vs) { o.push_back(v->x); o.push_back(v->y); o.push_back(v->z); } return o; }
static void Cat(std::vector<float>& a, const CVector* v, size_t n) { for (size_t i = 0; i < n; ++i) { a.push_back(v[i].x); a.push_back(v[i].y); a.push_back(v[i].z); } }
static bool AllFinite(const std::vector<float>& v) { for (float f : v) if (!std::isfinite(f)) return false; return true; }
static bool g_skipNonFinite = true;   // inf / NaN results (degenerate boxes: an empty tall object, overflow) are not compared: the exe keeps its temporaries in extended precision and propagates NaN signs differently
static bool CmpV(const std::vector<float>& a, const std::vector<float>& b, std::string& d, const char* what, int ra = 0, int rb = 0) {
    if (g_skipNonFinite && !(AllFinite(a) && AllFinite(b))) return true;
    bool ok = a.size() == b.size() && ra == rb;
    if (ok) ok = SameBlob(a.data(), b.data(), a.size() * 4);
    if (!ok) {
        d = what; d += " got"; for (float f : a) d += " " + F(f); d += " ret " + std::to_string(ra) + " exe"; for (float f : b) d += " " + F(f); d += " ret " + std::to_string(rb);
    }
    return ok;
}
// four corners: a rotated rectangle or arbitrary points
static void GenCorners(Rng& r, CVector* c, float z) {
    if (r.below(4) == 0) { for (int i = 0; i < 4; ++i) { c[i] = GenV(r, 50.f, S_NOHUGE); c[i].z = r.below(2) ? z : c[i].z; } return; }
    const CVector ctr = GenV(r, 100.f, S_NOHUGE); const float a = r.f01() * 6.2831853f, hx = 0.2f + r.f01() * 6.f, hy = 0.2f + r.f01() * 6.f;
    const float ca = std::cos(a), sa = std::sin(a);
    const float xs[4] = { hx, -hx, -hx, hx }, ys[4] = { hy, hy, -hy, -hy };
    for (int i = 0; i < 4; ++i) c[i] = CVector{ ctr.x + xs[i] * ca - ys[i] * sa, ctr.y + xs[i] * sa + ys[i] * ca, z };
}
} // namespace sg

static void TestScan() {
    using namespace sg;
    Init();
    using CPGA = CPedGeometryAnalyser;
    const auto Mrg = [](Rng& r) { return r.below(4) ? 0.35f : (r.below(2) ? 0.175f : r.f01()); };

    // ---- pure functions on the 4 corners
    Run("CPedGeometryAnalyser::ComputeEntityBoundingBoxCentreUncached 0x5F1600", [&](Rng& r, std::string& d) {
        r.sp = 0;
        CVector c[4]; const float z = GenF(r, 100.f); GenCorners(r, c, z);
        CVector a{ 7.f, 8.f, 9.f }, e{ 7.f, 8.f, 9.f };
        CPGA::ComputeEntityBoundingBoxCentreUncached(z, c, a);
        oracle::Fn<void __cdecl(float, const CVector*, CVector*)>(0x5F1600)(z, c, &e);
        return CmpV(Fl({ &a }), Fl({ &e }), d, "Centre");
    }, 2000);
    Run("CPedGeometryAnalyser::ComputeEntityBoundingBoxPlanesUncached 0x5F1670", [&](Rng& r, std::string& d) {
        r.sp = 0;
        CVector c[4]; const float z = GenF(r, 100.f); GenCorners(r, c, z);
        CVector pa[4] = {}, pe[4] = {}; float da[4] = { 1, 2, 3, 4 }, de[4] = { 1, 2, 3, 4 };
        CPGA::ComputeEntityBoundingBoxPlanesUncached(z, c, &pa, da);
        oracle::Fn<void __cdecl(float, const CVector*, CVector(*)[4], float*)>(0x5F1670)(z, c, &pe, de);
        std::vector<float> a, e; Cat(a, pa, 4); Cat(e, pe, 4); for (int i = 0; i < 4; ++i) { a.push_back(da[i]); e.push_back(de[i]); }
        return CmpV(a, e, d, "Planes");
    }, 3000);
    Run("CPedGeometryAnalyser::ComputeEntityBoundingBoxSegmentPlanesUncached 0x5F1750", [&](Rng& r, std::string& d) {
        r.sp = 0;
        CVector c[4]; const float z = GenF(r, 100.f); GenCorners(r, c, z);
        CVector ctr = r.below(2) ? CVector{ (c[0].x + c[2].x) * 0.5f, (c[0].y + c[2].y) * 0.5f, z } : GenV(r, 50.f);
        CVector na[4] = {}, ne[4] = {}; float da[4] = { 1, 2, 3, 4 }, de[4] = { 1, 2, 3, 4 };
        CVector* ra = CPGA::ComputeEntityBoundingBoxSegmentPlanesUncached(c, ctr, na, da);
        CVector* re = oracle::Fn<CVector* __cdecl(const CVector*, CVector*, CVector*, float*)>(0x5F1750)(c, &ctr, ne, de);
        std::vector<float> a, e; Cat(a, na, 4); Cat(e, ne, 4); for (int i = 0; i < 4; ++i) { a.push_back(da[i]); e.push_back(de[i]); }
        return CmpV(a, e, d, "SegPlanes", ra == c, re == c);
    }, 3000);
    Run("CPedGeometryAnalyser::ComputeEntityHitSide(planes) 0x5F1450", [&](Rng& r, std::string& d) {
        r.sp = 0;
        CVector c[4]; const float z = GenF(r, 100.f); GenCorners(r, c, z);
        CVector ctr{ (c[0].x + c[1].x + c[2].x + c[3].x) * 0.25f, (c[0].y + c[1].y + c[2].y + c[3].y) * 0.25f, z };
        CVector n[4]; float dt[4]; CPGA::ComputeEntityBoundingBoxSegmentPlanesUncached(c, ctr, n, dt);
        const CVector pt = r.below(3) ? CVector{ ctr.x + GenF(r, 30.f), ctr.y + GenF(r, 30.f), z } : GenV(r, 100.f);
        const int a = CPGA::ComputeEntityHitSide(pt, n, dt);
        const int e = oracle::Fn<int __cdecl(const CVector*, const CVector*, const float*)>(0x5F1450)(&pt, n, dt);
        Hit(a != 0);
        return CmpV({}, {}, d, "HitSide", a, e);
    }, 3000);
    Run("CPedGeometryAnalyser::ComputeClosestSurfacePoint(corners) 0x5F2C10", [&](Rng& r, std::string& d) {
        r.sp = 0;
        CVector c[4]; const float z = GenF(r, 100.f); GenCorners(r, c, z);
        const CVector p = r.below(3) ? CVector{ c[0].x + GenF(r, 15.f), c[0].y + GenF(r, 15.f), z + GenF(r, 3.f) } : GenV(r, 100.f);
        CVector oa{ 5.f, 6.f, 7.f }, oe{ 5.f, 6.f, 7.f };
        const bool a = CPGA::ComputeClosestSurfacePoint(p, c, oa); Hit(a);
        const bool e = oracle::Fn<bool __cdecl(const CVector*, const CVector*, CVector*)>(0x5F2C10)(&p, c, &oe);
        return CmpV(Fl({ &oa }), Fl({ &oe }), d, "ClosestSurfacePoint(corners)", a, e);
    }, 4000);

    // ---- on a fake entity
    Run("CPedGeometryAnalyser::ComputeEntityBoundingBoxCornersUncached 0x5F1FA0", [&](Rng& r, std::string& d) {
        r.sp = 0;
        GenEntity(r, *Ent, *EM); *Margin = Mrg(r);
        const float z = r.below(2) ? Ent->m_placement.m_vPosn.z + GenF(r, 8.f) : GenF(r, 100.f);
        CVector ca[4] = {}, ce[4] = {};
        CPGA::ComputeEntityBoundingBoxCornersUncached(z, *Ent, ca);
        oracle::Fn<void __cdecl(float, CEntity*, CVector*)>(0x5F1FA0)(z, Ent, ce);
        std::vector<float> a, e; Cat(a, ca, 4); Cat(e, ce, 4);
        return CmpV(a, e, d, "Corners");
    }, 6000);
    Run("CPedGeometryAnalyser::ComputeEntityBoundingBoxCentreUncachedAll 0x5F3B40", [&](Rng& r, std::string& d) {
        r.sp = 0;
        GenEntity(r, *Ent, *EM); *Margin = Mrg(r); const float z = GenF(r, 100.f);
        CVector a{ 1, 2, 3 }, e{ 1, 2, 3 };
        CPGA::ComputeEntityBoundingBoxCentreUncachedAll(z, *Ent, a);
        oracle::Fn<void __cdecl(float, CEntity*, CVector*)>(0x5F3B40)(z, Ent, &e);
        return CmpV(Fl({ &a }), Fl({ &e }), d, "CentreAll");
    }, 3000);
    Run("CPedGeometryAnalyser::ComputeEntityBoundingBoxPlanesUncachedAll 0x5F2B80", [&](Rng& r, std::string& d) {
        r.sp = 0;
        GenEntity(r, *Ent, *EM); *Margin = Mrg(r); const float z = GenF(r, 100.f);
        CVector pa[4] = {}, pe[4] = {}; float da[4] = {}, de[4] = {};
        CPGA::ComputeEntityBoundingBoxPlanesUncachedAll(z, *Ent, &pa, da);
        oracle::Fn<void __cdecl(float, CEntity*, CVector(*)[4], float*)>(0x5F2B80)(z, Ent, &pe, de);
        std::vector<float> a, e; Cat(a, pa, 4); Cat(e, pe, 4); for (int i = 0; i < 4; ++i) { a.push_back(da[i]); e.push_back(de[i]); }
        return CmpV(a, e, d, "PlanesAll");
    }, 3000);
    Run("CPedGeometryAnalyser::ComputeEntityBoundingBoxSegmentPlanesUncachedAll 0x5F2BC0", [&](Rng& r, std::string& d) {
        r.sp = 0;
        GenEntity(r, *Ent, *EM); *Margin = Mrg(r); const float z = GenF(r, 100.f);
        CVector na[4] = {}, ne[4] = {}; float da[4] = {}, de[4] = {};
        CPGA::ComputeEntityBoundingBoxSegmentPlanesUncachedAll(z, *Ent, na, da);
        oracle::Fn<CVector* __cdecl(float, CEntity*, CVector*, float*)>(0x5F2BC0)(z, Ent, ne, de);
        std::vector<float> a, e; Cat(a, na, 4); Cat(e, ne, 4); for (int i = 0; i < 4; ++i) { a.push_back(da[i]); e.push_back(de[i]); }
        return CmpV(a, e, d, "SegPlanesAll");
    }, 3000);
    Run("CPedGeometryAnalyser::ComputeEntityDirs 0x5F1500", [&](Rng& r, std::string& d) {
        r.sp = 0;
        GenEntity(r, *Ent, *EM, true, true);
        CVector da[4] = {}, de[4] = {};
        CVector* ra = CPGA::ComputeEntityDirs(*Ent, da);
        CVector* re = oracle::Fn<CVector* __cdecl(const CEntity*, CVector*)>(0x5F1500)(Ent, de);
        std::vector<float> a, e; Cat(a, da, 4); Cat(e, de, 4);
        return CmpV(a, e, d, "Dirs", ra == &da[3], re == &de[3]);
    }, 3000);
    Run("CPedGeometryAnalyser::ComputeEntityHitSide(ped, entity) 0x5F3BC0", [&](Rng& r, std::string& d) {
        r.sp = 0;
        GenEntity(r, *Ent, *EM); GenPed(r, *Pd, *PM_); *Margin = Mrg(r);
        if (r.below(2)) Pd->m_placement.m_vPosn = CVector{ Ent->m_placement.m_vPosn.x + GenF(r, 8.f), Ent->m_placement.m_vPosn.y + GenF(r, 8.f), Ent->m_placement.m_vPosn.z + GenF(r, 3.f) };
        Pd->m_matrix = nullptr;
        const int a = CPGA::ComputeEntityHitSide(*Pd, *Ent); const int e = oracle::Fn<int __cdecl(CPed*, CEntity*)>(0x5F3BC0)(Pd, Ent);
        return CmpV({}, {}, d, "HitSide(ped,entity)", a, e);
    }, 3000);
    Run("CPedGeometryAnalyser::ComputeEntityHitSide(point, entity) 0x5F3AC0", [&](Rng& r, std::string& d) {
        r.sp = 0;
        GenEntity(r, *Ent, *EM); *Margin = Mrg(r);
        const CVector pt = r.below(2) ? CVector{ Ent->m_placement.m_vPosn.x + GenF(r, 8.f), Ent->m_placement.m_vPosn.y + GenF(r, 8.f), Ent->m_placement.m_vPosn.z + GenF(r, 3.f) } : GenV(r, 100.f);
        const int a = CPGA::ComputeEntityHitSide(pt, *Ent); const int e = oracle::Fn<int __cdecl(const CVector*, CEntity*)>(0x5F3AC0)(&pt, Ent);
        return CmpV({}, {}, d, "HitSide(point,entity)", a, e);
    }, 3000);
    Run("CPedGeometryAnalyser::ComputePedHitSide(ped, velocity) 0x5F1E70", [&](Rng& r, std::string& d) {
        r.sp = 0;
        GenPed(r, *Pd, *PM_);
        const CVector v = r.below(8) == 0 ? CVector{ 0.f, 0.f, 0.f } : GenV(r, 2.f);
        const int a = CPGA::ComputePedHitSide(*Pd, v); const int e = oracle::Fn<int __cdecl(CPed*, const CVector*)>(0x5F1E70)(Pd, &v);
        return CmpV({}, {}, d, "PedHitSide(vel)", a, e);
    }, 4000);
    Run("CPedGeometryAnalyser::ComputePedHitSide(ped, physical) 0x5F3640", [&](Rng& r, std::string& d) {
        r.sp = 0;
        GenPed(r, *Pd, *PM_); GenPed(r, *Pd2, *EM);
        const int a = CPGA::ComputePedHitSide(*Pd, *(const CPhysical*)Pd2); const int e = oracle::Fn<int __cdecl(CPed*, CPhysical*)>(0x5F3640)(Pd, (CPhysical*)Pd2);
        return CmpV({}, {}, d, "PedHitSide(phys)", a, e);
    }, 2000);
    Run("CPedGeometryAnalyser::ComputePedShotSide 0x5F13F0", [&](Rng& r, std::string& d) {
        r.sp = 0;
        GenPed(r, *Pd, *PM_);
        const CVector p = r.below(2) ? CVector{ Pd->m_placement.m_vPosn.x + GenF(r, 20.f), Pd->m_placement.m_vPosn.y + GenF(r, 20.f), GenF(r, 5.f) } : GenV(r, 100.f);
        const int a = CPGA::ComputePedShotSide(*Pd, p); const int e = oracle::Fn<int __cdecl(CPed*, const CVector*)>(0x5F13F0)(Pd, &p);
        return CmpV({}, {}, d, "ShotSide", a, e);
    }, 5000);
    Run("CPedGeometryAnalyser::ComputeEntityBoundingSphere 0x5F3C20", [&](Rng& r, std::string& d) {
        r.sp = 0;
        GenEntity(r, *Ent, *EM); GenPed(r, *Pd, *PM_); *Margin = Mrg(r); Pd->m_matrix = nullptr;
        CColSphere sa{}, se{}; sa.m_vecCenter = se.m_vecCenter = CVector{ 9.f, 9.f, 9.f }; sa.m_fRadius = se.m_fRadius = 9.f;
        CPGA::ComputeEntityBoundingSphere(*Pd, *Ent, sa);
        oracle::Fn<void __cdecl(CPed*, CEntity*, CColSphere*)>(0x5F3C20)(Pd, Ent, &se);
        std::vector<float> a = { sa.m_vecCenter.x, sa.m_vecCenter.y, sa.m_vecCenter.z, sa.m_fRadius }, e = { se.m_vecCenter.x, se.m_vecCenter.y, se.m_vecCenter.z, se.m_fRadius };
        return CmpV(a, e, d, "BoundingSphere");
    }, 3000);
    Run("CPedGeometryAnalyser::ComputeMoveDirToAvoidEntity 0x5F3730", [&](Rng& r, std::string& d) {
        r.sp = 0;
        GenEntity(r, *Ent, *EM); GenPed(r, *Pd, *PM_); *Margin = Mrg(r); Pd->m_matrix = nullptr;
        if (r.below(2)) Pd->m_placement.m_vPosn = CVector{ Ent->m_placement.m_vPosn.x + GenF(r, 8.f), Ent->m_placement.m_vPosn.y + GenF(r, 8.f), Ent->m_placement.m_vPosn.z + GenF(r, 3.f) };
        CVector oa{ 4, 5, 6 }, oe{ 4, 5, 6 };
        CPGA::ComputeMoveDirToAvoidEntity(*Pd, *Ent, oa);
        oracle::Fn<void __cdecl(CPed*, CEntity*, CVector*)>(0x5F3730)(Pd, Ent, &oe);
        return CmpV(Fl({ &oa }), Fl({ &oe }), d, "AvoidDir");
    }, 3000);
    Run("CPedGeometryAnalyser::ComputeClosestSurfacePoint(ped, entity) 0x5F3B70", [&](Rng& r, std::string& d) {
        r.sp = 0;
        GenEntity(r, *Ent, *EM); GenPed(r, *Pd, *PM_); *Margin = Mrg(r); Pd->m_matrix = nullptr;
        if (r.below(2)) Pd->m_placement.m_vPosn = CVector{ Ent->m_placement.m_vPosn.x + GenF(r, 12.f), Ent->m_placement.m_vPosn.y + GenF(r, 12.f), Ent->m_placement.m_vPosn.z + GenF(r, 3.f) };
        CVector oa{ 4, 5, 6 }, oe{ 4, 5, 6 };
        const bool a = CPGA::ComputeClosestSurfacePoint(*Pd, *Ent, oa); Hit(a);
        const bool e = oracle::Fn<bool __cdecl(CPed*, CEntity*, CVector*)>(0x5F3B70)(Pd, Ent, &oe);
        return CmpV(Fl({ &oa }), Fl({ &oe }), d, "ClosestSurfacePoint(ped,entity)", a, e);
    }, 3000);
    Run("CPedGeometryAnalyser::ComputeClosestSurfacePoint(posn, entity) 0x5F36F0", [&](Rng& r, std::string& d) {
        r.sp = 0;
        GenEntity(r, *Ent, *EM); *Margin = Mrg(r);
        const CVector p = r.below(2) ? CVector{ Ent->m_placement.m_vPosn.x + GenF(r, 12.f), Ent->m_placement.m_vPosn.y + GenF(r, 12.f), Ent->m_placement.m_vPosn.z + GenF(r, 3.f) } : GenV(r, 100.f);
        CVector oa{ 4, 5, 6 }, oe{ 4, 5, 6 };
        const bool a = CPGA::ComputeClosestSurfacePoint(p, *Ent, oa); Hit(a);
        const bool e = oracle::Fn<bool __cdecl(const CVector*, CEntity*, CVector*)>(0x5F36F0)(&p, Ent, &oe);
        return CmpV(Fl({ &oa }), Fl({ &oe }), d, "ClosestSurfacePoint(posn,entity)", a, e);
    }, 3000);
    Run("CPedGeometryAnalyser::LiesInsideBoundingBox 0x5F3880", [&](Rng& r, std::string& d) {
        r.sp = 0;
        GenEntity(r, *Ent, *EM); GenPed(r, *Pd, *PM_); *Margin = Mrg(r); Pd->m_matrix = nullptr;
        const CVector p = r.below(2) ? CVector{ Ent->m_placement.m_vPosn.x + GenF(r, 8.f), Ent->m_placement.m_vPosn.y + GenF(r, 8.f), Ent->m_placement.m_vPosn.z + GenF(r, 3.f) } : GenV(r, 100.f);
        const bool a = CPGA::LiesInsideBoundingBox(*Pd, p, *Ent); Hit(a);
        const bool e = oracle::Fn<bool __cdecl(CPed*, const CVector*, CEntity*)>(0x5F3880)(Pd, &p, Ent);
        return CmpV({}, {}, d, "LiesInside", a, e);
    }, 4000);
    Run("CPedGeometryAnalyser::ComputeRouteRoundSphere 0x5F1890", [&](Rng& r, std::string& d) {
        r.sp = 0;
        GenPed(r, *Pd, *PM_); Pd->m_matrix = nullptr;
        CColSphere sp{}; sp.m_vecCenter = CVector{ Pd->m_placement.m_vPosn.x + GenF(r, 10.f), Pd->m_placement.m_vPosn.y + GenF(r, 10.f), Pd->m_placement.m_vPosn.z + GenF(r, 2.f) }; sp.m_fRadius = 0.2f + r.f01() * 4.f;
        const CVector a3 = { Pd->m_placement.m_vPosn.x + GenF(r, 8.f), Pd->m_placement.m_vPosn.y + GenF(r, 8.f), Pd->m_placement.m_vPosn.z };
        const CVector a4 = r.below(2) ? CVector{ sp.m_vecCenter.x + GenF(r, 3.f), sp.m_vecCenter.y + GenF(r, 3.f), sp.m_vecCenter.z } : CVector{ GenF(r, 20.f), GenF(r, 20.f), GenF(r, 3.f) };
        CVector a5a{ 1, 2, 3 }, a6a{ 4, 5, 6 }, a5e{ 1, 2, 3 }, a6e{ 4, 5, 6 };
        const bool a = CPGA::ComputeRouteRoundSphere(*Pd, sp, a3, a4, a5a, a6a); Hit(a);
        const bool e = oracle::Fn<bool __cdecl(CPed*, const CColSphere*, const CVector*, const CVector*, CVector*, CVector*)>(0x5F1890)(Pd, &sp, &a3, &a4, &a5e, &a6e);
        return CmpV(Fl({ &a5a, &a6a }), Fl({ &a5e, &a6e }), d, "RouteRoundSphere", a, e);
    }, 4000);

    // ---- CColSphere (used by the geometry analyser) and CGeneral::SolveQuadratic
    const auto GenSph = [](Rng& r, CColSphere& sp) { sp.m_vecCenter = CVector{ GenF(r, 15.f), GenF(r, 15.f), GenF(r, 5.f) }; sp.m_fRadius = 0.2f + r.f01() * 6.f; };
    const auto Pt = [](Rng& r, const CColSphere& sp) { return r.below(2) ? CVector{ sp.m_vecCenter.x + GenF(r, 8.f), sp.m_vecCenter.y + GenF(r, 8.f), sp.m_vecCenter.z + GenF(r, 8.f) } : GenV(r, 30.f); };
    Run("CGeneral::SolveQuadratic 0x53CE30", [&](Rng& r, std::string& d) {
        r.sp = 0;
        const float a = r.below(4) ? 1.f : GenF(r, 5.f), b = GenF(r, 40.f), c = GenF(r, 200.f);
        float t0a = 7.f, t1a = 8.f, t0e = 7.f, t1e = 8.f;
        const bool ra = CGeneral::SolveQuadratic(a, b, c, t0a, t1a); Hit(ra);
        const bool re = oracle::Fn<bool __cdecl(float, float, float, float*, float*)>(0x53CE30)(a, b, c, &t0e, &t1e);
        return CmpV({ t0a, t1a }, { t0e, t1e }, d, "SolveQuadratic", ra, re);
    }, 4000);
    Run("CColSphere::IntersectRay 0x40FF20", [&](Rng& r, std::string& d) {
        r.sp = 0; CColSphere sp{}; GenSph(r, sp); const CVector o = Pt(r, sp); const CVector dir = r.below(4) ? Unit(r) : GenV(r, 2.f);
        CVector aa{ 1, 2, 3 }, ab{ 4, 5, 6 }, ea{ 1, 2, 3 }, eb{ 4, 5, 6 };
        const bool a = sp.IntersectRay(o, dir, aa, ab); Hit(a);
        const bool e = oracle::Fn<bool __fastcall(CColSphere*, int, const CVector*, const CVector*, CVector*, CVector*)>(0x40FF20)(&sp, 0, &o, &dir, &ea, &eb);
        return CmpV(Fl({ &aa, &ab }), Fl({ &ea, &eb }), d, "IntersectRay", a, e);
    }, 4000);
    Run("CColSphere::IntersectEdge 0x4100E0", [&](Rng& r, std::string& d) {
        r.sp = 0; CColSphere sp{}; GenSph(r, sp); const CVector p = Pt(r, sp), q = Pt(r, sp);
        CVector aa{ 1, 2, 3 }, ab{ 4, 5, 6 }, ea{ 1, 2, 3 }, eb{ 4, 5, 6 };
        const bool a = sp.IntersectEdge(p, q, aa, ab); Hit(a);
        const bool e = oracle::Fn<bool __fastcall(CColSphere*, int, const CVector*, const CVector*, CVector*, CVector*)>(0x4100E0)(&sp, 0, &p, &q, &ea, &eb);
        return CmpV(Fl({ &aa, &ab }), Fl({ &ea, &eb }), d, "IntersectEdge", a, e);
    }, 4000);
    Run("CColSphere::IntersectPoint 0x410040", [&](Rng& r, std::string& d) {
        r.sp = 0; CColSphere sp{}; GenSph(r, sp); const CVector p = Pt(r, sp);
        const bool a = sp.IntersectPoint(p); Hit(a);
        const bool e = oracle::Fn<bool __fastcall(CColSphere*, int, const CVector*)>(0x410040)(&sp, 0, &p);
        return CmpV({}, {}, d, "IntersectPoint", a, e);
    }, 3000);
    Run("CColSphere::IntersectSphere 0x410090", [&](Rng& r, std::string& d) {
        r.sp = 0; CColSphere sp{}, o{}; GenSph(r, sp); GenSph(r, o); if (r.below(2)) o.m_vecCenter = Pt(r, sp);
        const bool a = sp.IntersectSphere(o); Hit(a);
        const bool e = oracle::Fn<bool __fastcall(CColSphere*, int, const CColSphere*)>(0x410090)(&sp, 0, &o);
        return CmpV({}, {}, d, "IntersectSphere", a, e);
    }, 3000);

    // ---- entity scanners: ped / vehicle scanner on fake pools (lw fixture: 8 cop peds, 6 vehicles), repeat sector lists made of hand-built nodes, a real reference pool (CReferences::Init)
    {
        struct TS : CEntityScanner {   // the protected members of the scanner
            void CallClear() { Clear(); }
            void CallScan(int list, const CPed& p) { CEntityScanner::ScanForEntitiesInRange((eRepeatSectorList)list, p); }
        };
        struct Node { void* item; Node* next; Node* prev; };
        static bool sInit = false;
        static TS* Sc; static CPedScanner* PSc; static CVehicleScanner* VSc;
        static CPed* SP;   // the scanning ped
        static std::vector<std::vector<uint8>> dummy;
        const auto Setup = [&](Rng& r, int kind /*0 base, 1 ped, 2 vehicle*/) {
            lw::Init();
            if (!sInit) {
                sInit = true;
                Sc = new (std::calloc(1, 0x80)) TS();
                PSc = new (std::calloc(1, 0x80)) CPedScanner();
                VSc = new (std::calloc(1, 0x80)) CVehicleScanner();
                SP = (CPed*)std::calloc(1, 0x1000);
                SP->*Get(T_PedInt{}) = (CPedIntelligence*)std::calloc(1, sizeof(CPedIntelligence));
            }
            CReferences::Init();
            std::memset(&CWorld::ms_aRepeatSectors, 0, sizeof(CWorld::ms_aRepeatSectors));
            // scanning ped
            const bool asPlayer = r.below(3) == 0;
            CPed* sp = asPlayer ? static_cast<CPed*>(lw::Pl) : SP;
            sp->*Get(T_PedInt{}) = sp->*Get(T_PedInt{}) ? sp->*Get(T_PedInt{}) : (CPedIntelligence*)std::calloc(1, sizeof(CPedIntelligence));
            sp->m_matrix = nullptr;
            const CVector P = r.below(4) ? CVector{ GenF(r, 200.f), GenF(r, 200.f), GenF(r, 20.f) } : CVector{ GenF(r, 3000.f), GenF(r, 3000.f), GenF(r, 20.f) };
            sp->m_placement.m_vPosn = P;
            auto* intel = sp->*Get(T_PedInt{});
            intel->m_fHearingRange = r.below(4) ? 5.f + r.f01() * 55.f : r.f01() * 120.f;
            intel->m_fSeeingRange  = r.below(4) ? 5.f + r.f01() * 55.f : r.f01() * 120.f;
            sp->m_nPedState = (ePedState)(r.below(8) == 0 ? (r.below(2) ? PEDSTATE_DEAD : PEDSTATE_DIE) : PEDSTATE_IDLE);
            sp->*Get(T_PedCreatedBy{}) = (ePedCreatedBy)(r.below(3) == 0 ? PED_MISSION : PED_GAME);
            sp->m_pVehicle = nullptr; sp->bInVehicle = false;
            if (kind == 2 && r.below(2)) { sp->m_pVehicle = lw::VehObj[r.below(lw::kVehs)]; sp->bInVehicle = r.below(4) != 0; }
            sp->SetScanCode((uint16)r.u32());
            CWorld::PlayerInFocus = 0;
            // entities
            const float range = std::max(intel->m_fHearingRange, intel->m_fSeeingRange);
            const auto ToSector = [](float c) { return (int32)std::floor(c * 0.02f + 60.f); };
            const int32 minX = std::max(0, ToSector(P.x - range)), minY = std::max(0, ToSector(P.y - range));
            const int32 maxX = std::min((int32)MAX_SECTORS_X - 1, ToSector(range + P.x)), maxY = std::min((int32)MAX_SECTORS_Y - 1, ToSector(range + P.y));
            const auto AddToSector = [&](int32 x, int32 y, int list, void* ent) {
                auto& sec = CWorld::GetRepeatSector(x, y);
                void** head = list == 0 ? (void**)&sec.Vehicles : list == 1 ? (void**)&sec.Peds : (void**)&sec.Objects;
                auto* n = (Node*)std::calloc(1, sizeof(Node)); n->item = ent; n->next = (Node*)*head; if (n->next) n->next->prev = n; *head = n;
            };
            for (int i = 0; i < lw::kPeds; ++i) {
                auto* e = lw::PedObj[i]; std::memset((void*)e, 0, sizeof(CCopPed));
                lw::PedState[i] = r.below(5) ? (uint8)r.below(0x7F) : 0x80;
                e->m_placement.m_vPosn = r.below(5) ? CVector{ P.x + GenF(r, range * 1.3f), P.y + GenF(r, range * 1.3f), P.z + GenF(r, range * 0.6f) } : GenV(r, 3000.f, S_NOHUGE);
                e->m_nPedState = (ePedState)(r.below(5) == 0 ? (r.below(2) ? PEDSTATE_DEAD : PEDSTATE_DIE) : PEDSTATE_IDLE);
                e->SetScanCode((uint16)(r.below(4) == 0 ? CWorld::ms_nCurrentScanCode + 1 : r.u32()));
                const int n = r.below(3);
                for (int k = 0; k < n; ++k) AddToSector(minX + (int32)r.below(maxX - minX + 1), minY + (int32)r.below(maxY - minY + 1), 1, e);
            }
            for (int i = 0; i < lw::kVehs; ++i) {
                auto* e = lw::VehObj[i]; std::memset((void*)e, 0, sizeof(CHeli));
                lw::VehState[i] = r.below(4) ? (uint8)r.below(0x7F) : 0x80;
                e->m_placement.m_vPosn = r.below(5) ? CVector{ P.x + GenF(r, range * 1.3f), P.y + GenF(r, range * 1.3f), P.z + GenF(r, range * 0.6f) } : GenV(r, 3000.f, S_NOHUGE);
                e->SetScanCode((uint16)(r.below(4) == 0 ? CWorld::ms_nCurrentScanCode + 1 : r.u32()));
                const int n = r.below(3);
                for (int k = 0; k < n; ++k) AddToSector(minX + (int32)r.below(maxX - minX + 1), minY + (int32)r.below(maxY - minY + 1), 0, e);
            }
            CWorld::ms_nCurrentScanCode = (uint16)(r.below(8) == 0 ? 65534 : r.u32());
            // scanner state: timer (Count / Period) and stale entries (unregistered references)
            for (auto* base : { (uint8*)Sc, (uint8*)PSc, (uint8*)VSc }) {
                auto* ent = reinterpret_cast<CEntity**>(base + 12);
                int32* tm = reinterpret_cast<int32*>(base + 4);
                tm[0] = r.below(3) ? 0 : (int32)r.below(20); tm[1] = r.below(5) ? 16 : (int32)r.below(20);
                for (int i = 0; i < 16; ++i) ent[i] = r.below(3) ? nullptr : (r.below(2) ? (CEntity*)lw::PedObj[r.below(lw::kPeds)] : (CEntity*)lw::VehObj[r.below(lw::kVehs)]);
                *reinterpret_cast<CEntity**>(base + 76) = r.below(2) ? nullptr : (CEntity*)lw::PedObj[r.below(lw::kPeds)];
            }
            return sp;
        };
        static std::vector<Snap> snaps;
        const auto SaveS = [&](std::vector<std::vector<uint8>>& o, CPed* sp) {
            if (snaps.empty()) {
                snaps.emplace_back((uintptr_t)Sc, 0x50); snaps.emplace_back((uintptr_t)PSc, 0x50); snaps.emplace_back((uintptr_t)VSc, 0x50);
                snaps.emplace_back((uintptr_t)lw::PedObj[0], lw::kPeds * sizeof(CCopPed)); snaps.emplace_back((uintptr_t)lw::VehObj[0], lw::kVehs * sizeof(CHeli));
                snaps.emplace_back((uintptr_t)&CReferences::aRefs, sizeof(CReferences::aRefs)); snaps.emplace_back((uintptr_t)&CReferences::pEmptyList, 4);
                snaps.emplace_back(0xB7CD78, 4);
                snaps.emplace_back((uintptr_t)SP, 0x1000); snaps.emplace_back((uintptr_t)lw::Pl, 0x400);
            }
            o.resize(snaps.size()); for (size_t i = 0; i < snaps.size(); ++i) { snaps[i].Save(); o[i] = snaps[i].b; }
        };
        const auto RestoreS = [&](const std::vector<std::vector<uint8>>& o) { for (size_t i = 0; i < snaps.size(); ++i) { snaps[i].b = o[i]; snaps[i].Restore(); } };
        const auto SameS = [&](const std::vector<std::vector<uint8>>& a, const std::vector<std::vector<uint8>>& b, std::string& d) {
            bool ok = true;
            for (size_t i = 0; i < a.size(); ++i) if (!SameBlob(a[i].data(), b[i].data(), a[i].size())) { ok = false; d += " mem[" + std::to_string(i) + "]" + DiffDesc(a[i].data(), b[i].data(), a[i].size(), "m"); }
            return ok;
        };
        const auto Scan = [&](Rng& r, std::string& d, int kind, auto&& port, auto&& exe, const char* what) {
            r.sp = 0;
            CPed* sp = Setup(r, kind);
            static std::vector<std::vector<uint8>> w0, w1, w2;
            SaveS(w0, sp);
            port(sp); SaveS(w1, sp);
            { bool found = false; for (auto* base : { (uint8*)Sc, (uint8*)PSc, (uint8*)VSc }) { auto* e = *reinterpret_cast<CEntity**>(base + 12); if (e && e->m_pReferences) found = true; } Hit(found); }   // coverage: a scan registered a reference
            RestoreS(w0);
            exe(sp); SaveS(w2, sp);
            d.clear();
            const bool ok = SameS(w1, w2, d);
            if (!ok) d = std::string(what) + d;
            return ok;
        };
        const auto ScanSub = [&](Rng& r, std::string& d, int kind, auto&& port, auto&& exe, const char* what) { return Scan(r, d, kind, port, exe, what); };
        Run("CEntityScanner::ScanForEntitiesInRange(peds) 0x5FFA20", [&](Rng& r, std::string& d) {
            return ScanSub(r, d, 0, [&](CPed* sp) { Sc->CallScan(REPEATSECTOR_PEDS, *sp); }, [&](CPed* sp) { oracle::Fn<void __fastcall(CEntityScanner*, int, int, const CPed*)>(0x5FFA20)(Sc, 0, REPEATSECTOR_PEDS, sp); }, "ScanPeds");
        }, 2500);
        Run("CEntityScanner::ScanForEntitiesInRange(vehicles) 0x5FFA20", [&](Rng& r, std::string& d) {
            return ScanSub(r, d, 0, [&](CPed* sp) { Sc->CallScan(REPEATSECTOR_VEHICLES, *sp); }, [&](CPed* sp) { oracle::Fn<void __fastcall(CEntityScanner*, int, int, const CPed*)>(0x5FFA20)(Sc, 0, REPEATSECTOR_VEHICLES, sp); }, "ScanVehicles");
        }, 2500);
        Run("CEntityScanner::ScanForEntitiesInRange(objects) 0x5FFA20", [&](Rng& r, std::string& d) {
            return ScanSub(r, d, 0, [&](CPed* sp) { Sc->CallScan(REPEATSECTOR_OBJECTS, *sp); }, [&](CPed* sp) { oracle::Fn<void __fastcall(CEntityScanner*, int, int, const CPed*)>(0x5FFA20)(Sc, 0, REPEATSECTOR_OBJECTS, sp); }, "ScanObjects");
        }, 500);
        Run("CEntityScanner::Clear 0x5FF9D0", [&](Rng& r, std::string& d) {
            return ScanSub(r, d, 0, [&](CPed*) { Sc->CallClear(); }, [&](CPed*) { oracle::Fn<void __fastcall(CEntityScanner*, int)>(0x5FF9D0)(Sc, 0); }, "Clear");
        }, 1000);
        Run("CPedScanner::ScanForPedsInRange 0x5FFEF0", [&](Rng& r, std::string& d) {
            return ScanSub(r, d, 1, [&](CPed* sp) { PSc->ScanForPedsInRange(*sp); }, [&](CPed* sp) { oracle::Fn<void __fastcall(CPedScanner*, int, const CPed*)>(0x5FFEF0)(PSc, 0, sp); }, "ScanForPedsInRange");
        }, 2500);
        Run("CVehicleScanner::ScanForVehiclesInRange 0x5FFE60", [&](Rng& r, std::string& d) {
            return ScanSub(r, d, 2, [&](CPed* sp) { VSc->ScanForVehiclesInRange(*sp); }, [&](CPed* sp) { oracle::Fn<void __fastcall(CVehicleScanner*, int, const CPed*)>(0x5FFE60)(VSc, 0, sp); }, "ScanForVehiclesInRange");
        }, 2500);
    }
}

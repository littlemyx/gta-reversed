// (a) peds: CPedGeometryAnalyser pure helpers, CPedIK::MoveLimb, ... (see gameplay_oracle_test.cpp)
#include "Entity/Ped/Ped.h"
#include "PedGeometryAnalyser.h"
#include "Collision/ColSphere.h"
#include "PedIK.h"
#include "Replay.h"
#include "Timer.h"
#include "Pad.h"
#include "AnimBlendAssociation.h"
#include "PedIntelligence.h"

// the fake ped's intelligence is all zero: no secondary tasks (the exe's getters read the same zeroed buffer)
static void* __fastcall HostNullTask(CPedIntelligence*, int) { return nullptr; }
static void InstallPedStubs() {
    static bool done = false; if (done) return; done = true;
    PatchHostFn(PmAddr(&CPedIntelligence::GetTaskFighting), (void*)&HostNullTask);
    PatchHostFn(PmAddr(&CPedIntelligence::GetTaskUseGun), (void*)&HostNullTask);
}
struct PedAcc : CPed { using CPed::m_pIntelligence; using CPed::m_nCreatedBy; };
// raw entity buffer with the real layout (CPed is 0x79C bytes; the matrix pointer lives at +0x14, the placement (pos, heading) at +0x4)
struct PedFx {
    alignas(16) uint8 e[0x800] {};
    alignas(16) uint8 m[0x80] {};
    CPed&    ped()   { return *reinterpret_cast<CPed*>(e); }
    CEntity& ent()   { return *reinterpret_cast<CEntity*>(e); }
    void Rewire(bool withMatrix) { reinterpret_cast<CPlaceable*>(e)->m_matrix = withMatrix ? reinterpret_cast<CMatrixLink*>(m) : nullptr; }
    bool HasMatrix() const { return reinterpret_cast<const CPlaceable*>(e)->m_matrix != nullptr; }
    PedFx() = default;
    PedFx(const PedFx& o) { std::memcpy(this, &o, sizeof *this); Rewire(o.HasMatrix()); }
};
// matrices: mostly near-orthonormal rotations, sometimes arbitrary / special
static void GenMatrix(Rng& r, uint8* m, float posScale) {
    float* f = reinterpret_cast<float*>(m);
    for (int i = 0; i < 16; ++i) f[i] = GenF(r, 1.f);
    if (r.below(4) != 0) {   // rotation about a random axis built from a heading + tilt
        const float h = r.f01() * 6.2831853f, p = (r.f01() - 0.5f) * 1.2f;
        const CVector fwd{ -std::sin(h) * std::cos(p), std::cos(h) * std::cos(p), std::sin(p) };
        const CVector right{ std::cos(h), std::sin(h), 0.f };
        const CVector up = right.Cross(fwd);
        std::memcpy(f + 0, &right, 12); std::memcpy(f + 4, &fwd, 12); std::memcpy(f + 8, &up, 12);
    }
    const CVector pos = GenV(r, posScale);
    std::memcpy(f + 12, &pos, 12);
}
static void BuildPedFx(Rng& r, PedFx& f, float posScale = 100.f) {
    uint32_t* w = reinterpret_cast<uint32_t*>(f.e);
    for (size_t i = 0; i < sizeof f.e / 4; ++i) { const float v = GenF(r, 10.f); std::memcpy(&w[i], &v, 4); }
    GenMatrix(r, f.m, posScale);
    auto& pl = reinterpret_cast<CPlaceable*>(f.e)->m_placement;
    pl.m_vPosn = GenV(r, posScale);
    pl.m_fHeading = r.below(4) ? (r.f01() - 0.5f) * 12.f : GenF(r, 8.f, S_NAN | S_ZERO | S_DEN | S_TINY);
    f.Rewire(r.below(4) != 0);
}
static std::string PedDesc(PedFx& f) {
    auto& pl = reinterpret_cast<CPlaceable*>(f.e)->m_placement;
    std::string s = "ped matrix=" + std::to_string(f.HasMatrix()) + " pos " + V(pl.m_vPosn) + " heading " + F(pl.m_fHeading);
    if (f.HasMatrix()) s += " M " + Hex(f.m, 64);
    return s;
}
static bool SameV3Arr(const CVector* a, const CVector* b, int n) { bool ok = true; for (int i = 0; i < n; ++i) ok &= SameV(a[i], b[i]); return ok; }
static std::string V3Arr(const CVector* a, int n) { std::string s; for (int i = 0; i < n; ++i) s += V(a[i]); return s; }
static void GenCorners(Rng& r, CVector* c) {   // a (possibly rotated, possibly degenerate) rectangle
    const float sc = r.below(3) ? 5.f : PickScale(r);
    const CVector o = GenV(r, sc);
    const float h = r.f01() * 6.2831853f, w = std::fabs(GenF(r, sc * 0.5f)) + 0.1f, l = std::fabs(GenF(r, sc * 0.5f)) + 0.1f;
    const CVector ax{ std::cos(h), std::sin(h), 0.f }, ay{ -std::sin(h), std::cos(h), 0.f };
    c[0] = o + ax * w + ay * l; c[1] = o - ax * w + ay * l; c[2] = o - ax * w - ay * l; c[3] = o + ax * w - ay * l;
    if (r.below(5) == 0) for (int i = 0; i < 4; ++i) c[i] = GenV(r, sc);
    if (r.below(6) == 0) c[r.below(4)] = GenV(r, sc, S_ALL);
}

static void TestPedGeometry() {
    Run("CPedGeometryAnalyser::ComputeEntityDirs 0x5F1500", [](Rng& r, std::string& d) {
        PedFx a; BuildPedFx(r, a); PedFx e(a);
        CVector pa[4], pe[4];
        const CVector* ra = CPedGeometryAnalyser::ComputeEntityDirs(a.ent(), pa);
        const CVector* re = oracle::Fn<CVector* __cdecl(const CEntity*, CVector*)>(0x5F1500)(&e.ent(), pe);
        if (SameV3Arr(pa, pe, 4) && (ra - pa) == (re - pe)) return true;
        d = PedDesc(a) + " got " + V3Arr(pa, 4) + " exe " + V3Arr(pe, 4);
        return false;
    });
    Run("CPedGeometryAnalyser::ComputeEntityHitSide(planes) 0x5F1450", [](Rng& r, std::string& d) {
        CVector pt = GenV(r, 20.f), planes[4]; float dots[4];
        for (int i = 0; i < 4; ++i) { planes[i] = r.below(3) ? GenDir(r) : GenV(r, 2.f); dots[i] = GenF(r, 30.f); }
        const int a = CPedGeometryAnalyser::ComputeEntityHitSide(pt, planes, dots);
        const int b = oracle::Fn<int __cdecl(const CVector*, const CVector*, const float*)>(0x5F1450)(&pt, planes, dots);
        Hit(a != 0);
        if (a == b) return true;
        d = "pt " + V(pt) + " planes " + V3Arr(planes, 4) + " dots " + F(dots[0]) + F(dots[1]) + F(dots[2]) + F(dots[3]) + " got " + std::to_string(a) + " exe " + std::to_string(b);
        return false;
    });
    Run("CPedGeometryAnalyser::ComputePedHitSide(vel) 0x5F1E70", [](Rng& r, std::string& d) {
        PedFx a; BuildPedFx(r, a); PedFx e(a);
        const CVector vel = GenV(r, r.below(2) ? 1.f : PickScale(r), S_NAN | S_ZERO | S_DEN | S_TINY);   // inf / NaN velocity: the exe returns an uninitialised register (all dots NaN)
        if (!std::isfinite(vel.x * vel.x + vel.y * vel.y + vel.z * vel.z)) return true;
        { CVector dd[4]; CPedGeometryAnalyser::ComputeEntityDirs(a.ent(), dd); for (auto& v : dd) if (!std::isfinite(v.x + v.y + v.z)) return true; }
        const int ra = CPedGeometryAnalyser::ComputePedHitSide(a.ped(), vel);
        const int re = oracle::Fn<int __cdecl(const CPed*, const CVector*)>(0x5F1E70)(&e.ped(), &vel);
        Hit(ra != 0);
        if (ra == re) return true;
        d = PedDesc(a) + " vel " + V(vel) + " got " + std::to_string(ra) + " exe " + std::to_string(re);
        return false;
    });
    Run("CPedGeometryAnalyser::ComputePedHitSide(physical) 0x5F3640", [](Rng& r, std::string& d) {
        PedFx a; BuildPedFx(r, a); PedFx e(a);
        alignas(16) uint8 phys[0x138] {};
        const CVector vel = GenV(r, r.below(2) ? 1.f : PickScale(r), S_NAN | S_ZERO | S_DEN | S_TINY);
        if (!std::isfinite(vel.x * vel.x + vel.y * vel.y + vel.z * vel.z)) return true;
        std::memcpy(phys + 0x44, &vel, 12);   // CPhysical::m_vecMoveSpeed
        static_assert(offsetof(CPhysical, m_vecMoveSpeed) == 0x44);
        { CVector dd[4]; CPedGeometryAnalyser::ComputeEntityDirs(a.ent(), dd); for (auto& v : dd) if (!std::isfinite(v.x + v.y + v.z)) return true; }
        const int ra = CPedGeometryAnalyser::ComputePedHitSide(a.ped(), *reinterpret_cast<CPhysical*>(phys));
        const int re = oracle::Fn<int __cdecl(const CPed*, const CPhysical*)>(0x5F3640)(&e.ped(), reinterpret_cast<CPhysical*>(phys));
        if (ra == re) return true;
        d = PedDesc(a) + " vel " + V(vel) + " got " + std::to_string(ra) + " exe " + std::to_string(re);
        return false;
    });
    Run("CPedGeometryAnalyser::ComputePedShotSide 0x5F13F0", [](Rng& r, std::string& d) {
        PedFx a; BuildPedFx(r, a); PedFx e(a);
        const CVector p = GenV(r, 50.f);
        const int ra = CPedGeometryAnalyser::ComputePedShotSide(a.ped(), p);
        const int re = oracle::Fn<int __cdecl(const CPed*, const CVector*)>(0x5F13F0)(&e.ped(), &p);
        Hit(ra != 0);
        if (ra == re) return true;
        d = PedDesc(a) + " rot " + F(a.ped().m_fCurrentRotation) + " p " + V(p) + " got " + std::to_string(ra) + " exe " + std::to_string(re);
        return false;
    });
    Run("CPedGeometryAnalyser::ComputeClosestSurfacePoint(corners) 0x5F2C10", [](Rng& r, std::string& d) {
        CVector c[4]; GenCorners(r, c);
        const CVector p = GenV(r, 10.f);
        CVector oa{ 7, 8, 9 }, oe{ 7, 8, 9 };
        const bool ra = CPedGeometryAnalyser::ComputeClosestSurfacePoint(p, c, oa);
        const bool re = oracle::Fn<bool __cdecl(const CVector*, const CVector*, CVector*)>(0x5F2C10)(&p, c, &oe);
        Hit(ra);
        if (ra == re && SameV(oa, oe)) return true;
        d = "p " + V(p) + " corners " + V3Arr(c, 4) + " got " + std::to_string(ra) + V(oa) + " exe " + std::to_string(re) + V(oe);
        return false;
    });
    Run("CPedGeometryAnalyser::ComputeEntityBoundingBoxCentreUncached 0x5F1600", [](Rng& r, std::string& d) {
        CVector c[4]; GenCorners(r, c); const float z = GenF(r, 10.f);
        CVector oa{ 7, 8, 9 }, oe{ 7, 8, 9 };
        CPedGeometryAnalyser::ComputeEntityBoundingBoxCentreUncached(z, c, oa);
        oracle::Fn<void __cdecl(float, const CVector*, CVector*)>(0x5F1600)(z, c, &oe);
        if (SameV(oa, oe)) return true;
        d = "z " + F(z) + " corners " + V3Arr(c, 4) + " got " + V(oa) + " exe " + V(oe);
        return false;
    });
    Run("CPedGeometryAnalyser::ComputeEntityBoundingBoxPlanesUncached 0x5F1670", [](Rng& r, std::string& d) {
        CVector c[4]; GenCorners(r, c); const float z = GenF(r, 10.f);
        CVector pa[4], pe[4]; float da[4] = { 1, 2, 3, 4 }, de[4] = { 1, 2, 3, 4 };
        for (int i = 0; i < 4; ++i) pa[i] = CVector{ 7, 8, 9 }, pe[i] = CVector{ 7, 8, 9 };
        CPedGeometryAnalyser::ComputeEntityBoundingBoxPlanesUncached(z, c, reinterpret_cast<CVector(*)[4]>(pa), da);
        oracle::Fn<void __cdecl(float, const CVector*, CVector*, float*)>(0x5F1670)(z, c, pe, de);
        if (SameV3Arr(pa, pe, 4) && SameBlob(da, de, 16)) return true;
        d = "z " + F(z) + " corners " + V3Arr(c, 4) + " got " + V3Arr(pa, 4) + F(da[0]) + F(da[1]) + F(da[2]) + F(da[3]) + " exe " + V3Arr(pe, 4) + F(de[0]) + F(de[1]) + F(de[2]) + F(de[3]);
        return false;
    });
    Run("CPedGeometryAnalyser::ComputeEntityBoundingBoxSegmentPlanesUncached 0x5F1750", [](Rng& r, std::string& d) {
        CVector c[4]; GenCorners(r, c); CVector ctr = GenV(r, 10.f);
        CVector pa[4], pe[4]; float da[4] = { 1, 2, 3, 4 }, de[4] = { 1, 2, 3, 4 };
        for (int i = 0; i < 4; ++i) pa[i] = CVector{ 7, 8, 9 }, pe[i] = CVector{ 7, 8, 9 };
        const CVector* ra = CPedGeometryAnalyser::ComputeEntityBoundingBoxSegmentPlanesUncached(c, ctr, pa, da);
        const CVector* re = oracle::Fn<CVector* __cdecl(const CVector*, CVector*, CVector*, float*)>(0x5F1750)(c, &ctr, pe, de);
        if (SameV3Arr(pa, pe, 4) && SameBlob(da, de, 16) && ra == re) return true;
        d = "ctr " + V(ctr) + " corners " + V3Arr(c, 4) + " ret " + std::to_string(ra == re) + DiffDesc(pa, pe, sizeof pa, "n") + DiffDesc(da, de, 16, "dot");
        return false;
    });
    Run("CPedGeometryAnalyser::ComputeRouteRoundSphere 0x5F1890", [](Rng& r, std::string& d) {
        PedFx a; BuildPedFx(r, a, 20.f); PedFx e(a);
        alignas(16) uint8 sph[sizeof(CColSphere)] {};
        auto& s = *reinterpret_cast<CColSphere*>(sph);
        s.m_vecCenter = GenV(r, 20.f); s.m_fRadius = r.below(8) ? r.f01() * 6.f + 0.2f : GenF(r, 10.f);
        const CVector a3 = GenV(r, 20.f), a4 = GenV(r, 20.f);
        CVector o5a{ 7, 8, 9 }, o6a{ 7, 8, 9 }, o5e{ 7, 8, 9 }, o6e{ 7, 8, 9 };
        const bool ra = CPedGeometryAnalyser::ComputeRouteRoundSphere(a.ped(), s, a3, a4, o5a, o6a);
        const bool re = oracle::Fn<bool __cdecl(const CPed*, const CColSphere*, const CVector*, const CVector*, CVector*, CVector*)>(0x5F1890)(&e.ped(), &s, &a3, &a4, &o5e, &o6e);
        Hit(ra);
        if (ra == re && SameV(o5a, o5e) && SameV(o6a, o6e)) return true;
        d = PedDesc(a) + " sphere " + V(s.m_vecCenter) + F(s.m_fRadius) + " a3 " + V(a3) + " a4 " + V(a4) + " got " + std::to_string(ra) + V(o5a) + V(o6a) + " exe " + std::to_string(re) + V(o5e) + V(o6e);
        return false;
    });
}

static void TestPedIK() {
    for (int norm = 0; norm < 2; ++norm) {
        Run(norm ? "CPedIK::MoveLimb(normalized) 0x5FDB60" : "CPedIK::MoveLimb 0x5FDA60", [=](Rng& r, std::string& d) {
            alignas(16) uint8 ikb[sizeof(CPedIK)] {};
            auto& ik = *reinterpret_cast<CPedIK*>(ikb);
            LimbOrientation la{ GenF(r, 3.f), GenF(r, 3.f) }, le = la;
            LimbMovementInfo mi{ GenF(r, 3.f), GenF(r, 3.f), r.below(8) ? r.f01() * 0.5f : GenF(r, 1.f), GenF(r, 3.f), GenF(r, 3.f), r.below(8) ? r.f01() * 0.5f : GenF(r, 1.f) };
            if (r.below(2)) { mi.minYaw = -std::fabs(mi.maxYaw); mi.minPitch = -std::fabs(mi.maxPitch); }   // mostly sane limits
            const float ty = r.below(2) ? GenF(r, 3.f) : la.m_fYaw + GenF(r, 0.5f), tp = r.below(2) ? GenF(r, 3.f) : la.m_fPitch + GenF(r, 0.5f);
            const float nrm = r.below(4) ? r.f01() * 1.5f : GenF(r, 2.f);
            int ra, re;
            if (norm) {
                ra = ik.MoveLimb(la, ty, tp, mi, nrm);
                re = oracle::Fn<int __fastcall(CPedIK*, int, LimbOrientation*, float, float, LimbMovementInfo*, float)>(0x5FDB60)(&ik, 0, &le, ty, tp, &mi, nrm);
            } else {
                ra = ik.MoveLimb(la, ty, tp, mi);
                re = oracle::Fn<int __fastcall(CPedIK*, int, LimbOrientation*, float, float, LimbMovementInfo*)>(0x5FDA60)(&ik, 0, &le, ty, tp, &mi);
            }
            Hit(ra == 2);
            if (ra == re && SameF(la.m_fYaw, le.m_fYaw) && SameF(la.m_fPitch, le.m_fPitch)) return true;
            d = "limb " + F(le.m_fYaw) + F(le.m_fPitch) + " (before) ty " + F(ty) + " tp " + F(tp) + " nrm " + F(nrm) + " info " + Hex(&mi, sizeof mi) + " got " + std::to_string(ra) + F(la.m_fYaw) + F(la.m_fPitch) + " exe " + std::to_string(re) + F(le.m_fYaw) + F(le.m_fPitch);
            return false;
        });
    }
}


static bool SamePedFx(PedFx& a, PedFx& b) {
    PedFx x(a), y(b);   // m_matrix pointers differ by construction
    reinterpret_cast<CPlaceable*>(x.e)->m_matrix = nullptr; reinterpret_cast<CPlaceable*>(y.e)->m_matrix = nullptr;
    return SameBlob(x.e, y.e, sizeof x.e) & SameBlob(x.m, y.m, sizeof x.m);
}
static std::string PedDiff(PedFx& a, PedFx& b) {
    PedFx x(a), y(b);
    reinterpret_cast<CPlaceable*>(x.e)->m_matrix = nullptr; reinterpret_cast<CPlaceable*>(y.e)->m_matrix = nullptr;
    return DiffDesc(x.e, y.e, sizeof x.e, "ped") + DiffDesc(x.m, y.m, sizeof x.m, "mat");
}
// CPed state used by the movement helpers: sane ranges for the enums / flags most of the time
static alignas(16) uint8 g_fakeIntel[0x400];
static alignas(16) uint8 g_lookTarget[0x100];
static void BuildMovingPed(Rng& r, PedFx& f) {
    BuildPedFx(r, f, 100.f);
    auto& p = f.ped();
    std::memset(g_fakeIntel, 0, sizeof g_fakeIntel);
    static_cast<PedAcc&>(p).m_pIntelligence = reinterpret_cast<CPedIntelligence*>(g_fakeIntel);
    p.m_nPedState = (ePedState)(r.below(4) ? r.below(60) : (int)r.u32());
    p.m_nMoveState = (eMoveState)r.below(10);
    p.m_nPedType = (ePedType)(r.below(3) == 0 ? r.below(2) : r.below(30));
    p.m_nContactSurface = (eSurfaceType)r.below(TOTAL_NUM_SURFACE_TYPES);
    p.m_fCurrentRotation = r.below(4) ? (r.f01() - 0.5f) * 12.f : GenF(r, 8.f, S_NOHUGE);
    p.m_fAimingRotation = r.below(4) ? (r.f01() - 0.5f) * 12.f : GenF(r, 8.f, S_NOHUGE);
    p.m_fHeadingChangeRate = r.below(4) ? r.f01() * 25.f : GenF(r, 40.f);
    p.m_fMoveAnim = r.below(4) ? (r.f01() - 0.5f) * 2.f : GenF(r, 3.f);
    p.field_578 = r.below(2) ? CVector{ std::sin(r.f01() * 6.28f) * 0.3f, std::cos(r.f01() * 6.28f) * 0.3f, 0.95f + r.f01() * 0.05f } : GenV(r, 1.f);
    p.m_vecAnimMovingShiftLocal = { GenF(r, 0.2f), GenF(r, 0.2f) };
    p.m_pedIK.m_fSlopePitch = GenF(r, 0.6f); p.m_pedIK.m_fSlopeRoll = GenF(r, 0.6f);
    p.m_pedIK.m_nFlags = r.u32();
    p.m_pLookTarget = nullptr;
    CTimer::ms_fTimeStep = r.below(8) ? r.f01() * 3.f : GenF(r, 4.f, S_NOHUGE);   // a NaN / zero / denormal step; inf is not a frame time
    CTimer::bSlowMotionActive = r.below(6) == 0;
}

static void TestPedMovement() {
    Run("CPed::TurnBody 0x5E4000", [](Rng& r, std::string& d) {
        PedFx a; BuildMovingPed(r, a);
        if (r.below(3) == 0) { auto* lt = reinterpret_cast<CPlaceable*>(g_lookTarget); std::memset(g_lookTarget, 0, sizeof g_lookTarget); lt->m_placement.m_vPosn = GenV(r, 100.f); lt->m_matrix = nullptr; a.ped().m_pLookTarget = reinterpret_cast<CEntity*>(g_lookTarget); }
        a.ped().m_fLookDirection = r.below(4) ? (r.f01() - 0.5f) * 14.f : GenF(r, 9.f, S_NOHUGE);
        PedFx e(a);
        const bool ra = a.ped().TurnBody();
        const bool re = oracle::Fn<bool __fastcall(CPed*, int)>(0x5E4000)(&e.ped(), 0);
        Hit(ra);
        if (ra == re && SamePedFx(a, e)) return true;
        d = PedDesc(a) + " ret " + std::to_string(ra) + "/" + std::to_string(re) + PedDiff(a, e);
        return false;
    });
    Run("CPed::Look 0x5E7B20", [](Rng& r, std::string& d) {
        PedFx a; BuildMovingPed(r, a);
        a.ped().m_fLookDirection = r.below(4) ? (r.f01() - 0.5f) * 14.f : GenF(r, 9.f, S_NOHUGE);
        PedFx e(a);
        a.ped().Look();
        oracle::Fn<void __fastcall(CPed*, int)>(0x5E7B20)(&e.ped(), 0);
        if (SamePedFx(a, e)) return true;
        d = PedDesc(a) + PedDiff(a, e);
        return false;
    });
    Run("CPed::GetLocalDirection 0x5DEF60", [](Rng& r, std::string& d) {
        PedFx a; BuildMovingPed(r, a); PedFx e(a);
        const CVector2D pt{ GenF(r, r.below(2) ? 3.f : PickScale(r)), GenF(r, r.below(2) ? 3.f : PickScale(r)) };
        const int ra = a.ped().GetLocalDirection(pt);
        const int re = oracle::Fn<int __fastcall(const CPed*, int, const CVector2D*)>(0x5DEF60)(&e.ped(), 0, &pt);
        Hit(ra != 0);
        if (ra == re) return true;
        d = "rot " + F(a.ped().m_fCurrentRotation) + " pt " + F(pt.x) + F(pt.y) + " got " + std::to_string(ra) + " exe " + std::to_string(re);
        return false;
    });
    Run("CPed::SetMoveAnimSpeed 0x5DEC10", [](Rng& r, std::string& d) {
        PedFx a; BuildMovingPed(r, a);
        static_cast<PedAcc&>(a.ped()).m_nCreatedBy = (ePedCreatedBy)(r.below(2) ? 2 : r.below(4));
        a.ped().m_nRandomSeed = (int16)r.u32();
        PedFx e(a);
        alignas(16) uint8 assoc[sizeof(CAnimBlendAssociation)] {};
        auto& as = *reinterpret_cast<CAnimBlendAssociation*>(assoc);
        as.m_Speed = GenF(r, 5.f);
        alignas(16) uint8 assoc2[sizeof(CAnimBlendAssociation)]; std::memcpy(assoc2, assoc, sizeof assoc);
        a.ped().SetMoveAnimSpeed(&as);
        oracle::Fn<void __fastcall(CPed*, int, CAnimBlendAssociation*)>(0x5DEC10)(&e.ped(), 0, reinterpret_cast<CAnimBlendAssociation*>(assoc2));
        if (SameF(as.m_Speed, reinterpret_cast<CAnimBlendAssociation*>(assoc2)->m_Speed) && SamePedFx(a, e)) return true;
        d = "slope " + F(a.ped().m_pedIK.m_fSlopePitch) + " createdBy " + std::to_string((int)static_cast<PedAcc&>(a.ped()).m_nCreatedBy) + " seed " + std::to_string(a.ped().m_nRandomSeed) + " got " + F(as.m_Speed) + " exe " + F(reinterpret_cast<CAnimBlendAssociation*>(assoc2)->m_Speed);
        return false;
    });
    Run("CPed::CalculateNewOrientation 0x5E52E0", [](Rng& r, std::string& d) {
        PedFx a; BuildMovingPed(r, a); a.Rewire(true); PedFx e(a);
        CReplay::Mode = r.below(8) == 0 ? (eReplayMode)1 : (eReplayMode)0;
        a.ped().CalculateNewOrientation();
        oracle::Fn<void __fastcall(CPed*, int)>(0x5E52E0)(&e.ped(), 0);
        if (SamePedFx(a, e)) return true;
        d = PedDesc(a) + " state " + std::to_string((int)a.ped().m_nPedState) + " rot " + F(a.ped().m_fCurrentRotation) + PedDiff(a, e);
        return false;
    });
    // The slope update calls asin / pow (CRT _CIasin / _CIpow in the exe, UCRT in the port: owned by the x87 intrinsic audit). The first variant keeps the code on
    // paths without transcendental calls (exact compare); the second runs the slope paths and compares the two slope floats to 8 ulps + 2e-6 (everything else exact).
    for (int slope = 0; slope < 2; ++slope) {
        Run(slope ? "CPed::CalculateNewVelocity(slope, asin/pow 8ulp) 0x5E4C50" : "CPed::CalculateNewVelocity 0x5E4C50", [=](Rng& r, std::string& d) {
            PedFx a; BuildMovingPed(r, a); a.Rewire(true);
            auto& pk = a.ped().m_pedIK;
            if (!slope) { pk.bSlopePitch = false; pk.m_fSlopeRoll = GenF(r, 0.02f, S_NOHUGE); }
            PedFx e(a);
            const float ts = CTimer::ms_fTimeStep; const bool sm = CTimer::bSlowMotionActive;
            a.ped().CalculateNewVelocity();
            CTimer::ms_fTimeStep = ts; CTimer::bSlowMotionActive = sm;
            oracle::Fn<void __fastcall(CPed*, int)>(0x5E4C50)(&e.ped(), 0);
            bool ok;
            if (!slope) ok = SamePedFx(a, e);
            else {
                float* fa[2] = { &a.ped().m_pedIK.m_fSlopePitch, &a.ped().m_pedIK.m_fSlopeRoll };
                float* fe[2] = { &e.ped().m_pedIK.m_fSlopePitch, &e.ped().m_pedIK.m_fSlopeRoll };
                bool nearOk = true;
                for (int i = 0; i < 2; ++i) {
                    const uint32_t x = FBits(*fa[i]), y = FBits(*fe[i]);
                    if (x != y && !(IsNanBits(x) && IsNanBits(y)) && !(std::fabs((double)*fa[i] - *fe[i]) <= 2e-6 + 8.0 * std::fabs(*fe[i]) * 1.2e-7)) nearOk = false;
                    *fa[i] = *fe[i];   // compare the rest exactly
                }
                ok = nearOk && SamePedFx(a, e);
            }
            if (ok) return true;
            d = PedDesc(a) + " ts " + F(ts) + " state " + std::to_string((int)a.ped().m_nPedState) + " move " + std::to_string((int)a.ped().m_nMoveState) + PedDiff(a, e);
            return false;
        });
    }
}

static void TestColSphere() {
    auto genSphere = [](Rng& r, uint8* buf) {
        std::memset(buf, 0, sizeof(CColSphere));
        auto& s = *reinterpret_cast<CColSphere*>(buf);
        s.m_vecCenter = GenV(r, 20.f); s.m_fRadius = r.below(8) ? r.f01() * 6.f + 0.1f : GenF(r, 10.f);
    };
    Run("CColSphere::IntersectRay 0x40FF20", [=](Rng& r, std::string& d) {
        alignas(16) uint8 sb[sizeof(CColSphere)]; genSphere(r, sb); auto& s = *reinterpret_cast<CColSphere*>(sb);
        const CVector o = GenV(r, 20.f), dir = GenDir(r);
        CVector a1{ 7, 8, 9 }, a2{ 7, 8, 9 }, e1{ 7, 8, 9 }, e2{ 7, 8, 9 };
        const bool ra = s.IntersectRay(o, dir, a1, a2);
        const bool re = oracle::Fn<bool __fastcall(CColSphere*, int, const CVector*, const CVector*, CVector*, CVector*)>(0x40FF20)(&s, 0, &o, &dir, &e1, &e2);
        Hit(ra);
        if (ra == re && SameV(a1, e1) && SameV(a2, e2)) return true;
        d = "sphere " + V(s.m_vecCenter) + F(s.m_fRadius) + " o " + V(o) + " dir " + V(dir) + " got " + std::to_string(ra) + V(a1) + V(a2) + " exe " + std::to_string(re) + V(e1) + V(e2);
        return false;
    });
    Run("CColSphere::IntersectPoint 0x410040", [=](Rng& r, std::string& d) {
        alignas(16) uint8 sb[sizeof(CColSphere)]; genSphere(r, sb); auto& s = *reinterpret_cast<CColSphere*>(sb);
        const CVector p = r.below(2) ? s.m_vecCenter + GenV(r, 8.f) : GenV(r, 20.f);
        const bool ra = s.IntersectPoint(p);
        const bool re = oracle::Fn<bool __fastcall(CColSphere*, int, const CVector*)>(0x410040)(&s, 0, &p);
        Hit(ra);
        if (ra == re) return true;
        d = "sphere " + V(s.m_vecCenter) + F(s.m_fRadius) + " p " + V(p) + " got " + std::to_string(ra) + " exe " + std::to_string(re);
        return false;
    });
    Run("CColSphere::IntersectEdge 0x4100E0", [=](Rng& r, std::string& d) {
        alignas(16) uint8 sb[sizeof(CColSphere)]; genSphere(r, sb); auto& s = *reinterpret_cast<CColSphere*>(sb);
        const CVector a = GenV(r, 20.f), b = r.below(2) ? a + GenV(r, 15.f) : GenV(r, 20.f);
        CVector a1{ 7, 8, 9 }, a2{ 7, 8, 9 }, e1{ 7, 8, 9 }, e2{ 7, 8, 9 };
        const bool ra = s.IntersectEdge(a, b, a1, a2);
        const bool re = oracle::Fn<bool __fastcall(CColSphere*, int, const CVector*, const CVector*, CVector*, CVector*)>(0x4100E0)(&s, 0, &a, &b, &e1, &e2);
        Hit(ra);
        if (ra == re && SameV(a1, e1) && SameV(a2, e2)) return true;
        d = "sphere " + V(s.m_vecCenter) + F(s.m_fRadius) + " a " + V(a) + " b " + V(b) + " got " + std::to_string(ra) + V(a1) + V(a2) + " exe " + std::to_string(re) + V(e1) + V(e2);
        return false;
    });
    Run("CColSphere::IntersectSphere 0x410090", [=](Rng& r, std::string& d) {
        alignas(16) uint8 sb[sizeof(CColSphere)], sb2[sizeof(CColSphere)]; genSphere(r, sb); genSphere(r, sb2);
        auto& s = *reinterpret_cast<CColSphere*>(sb); auto& s2 = *reinterpret_cast<CColSphere*>(sb2);
        if (r.below(2)) s2.m_vecCenter = s.m_vecCenter + GenV(r, 8.f);
        const bool ra = s.IntersectSphere(s2);
        const bool re = oracle::Fn<bool __fastcall(CColSphere*, int, const CColSphere*)>(0x410090)(&s, 0, &s2);
        Hit(ra);
        if (ra == re) return true;
        d = "sphere " + V(s.m_vecCenter) + F(s.m_fRadius) + " s2 " + V(s2.m_vecCenter) + F(s2.m_fRadius) + " got " + std::to_string(ra) + " exe " + std::to_string(re);
        return false;
    });
}

// ---- entity bounding-box helpers: an entity with a fake model info -> col model -> col data (boxes / spheres), shared by both implementations
#include "Models/ModelInfo.h"
#include "Collision/ColModel.h"
#include "Collision/CollisionData.h"
#include "Collision/ColBox.h"
#include "Entity/Entity.h"
#include "PedGeometryAnalyser.h"
static float PickScaleSane(Rng& r) { static const float sc[] = { 1.f, 1.f, 10.f, 100.f, 3000.f, 1e5f, 1e-3f }; return sc[r.below(7)]; }
namespace efx {
static constexpr int kModel = 19990;
alignas(16) static uint8 g_info[0x40];
alignas(16) static uint8 g_col[0x30];
alignas(16) static uint8 g_data[0x30];
alignas(16) static uint8 g_boxes[4 * sizeof(CColBox)];
alignas(16) static uint8 g_spheres[4 * sizeof(CColSphere)];
alignas(16) static uint8 g_route[0x100];
}
static void BuildEntityFx(Rng& r, PedFx& f, bool pedAsOrigin = false) {
    using namespace efx;
    BuildPedFx(r, f, 30.f);
    f.Rewire(true);   // GetMatrix() would allocate a CMatrixLink (pool) for a null matrix
    auto& e = f.ent();
    e.m_nModelIndex = kModel;
    e.m_bIsTempBuilding = false;
    e.m_bUsesCollision = true;
    static const eEntityType types[] = { ENTITY_TYPE_BUILDING, ENTITY_TYPE_VEHICLE, ENTITY_TYPE_PED, ENTITY_TYPE_OBJECT, ENTITY_TYPE_OBJECT, ENTITY_TYPE_OBJECT, ENTITY_TYPE_DUMMY };
    e.SetType(types[r.below(7)]);
    std::memset(g_info, 0, sizeof g_info); std::memset(g_col, 0, sizeof g_col); std::memset(g_data, 0, sizeof g_data);
    std::memset(g_boxes, 0, sizeof g_boxes); std::memset(g_spheres, 0, sizeof g_spheres);
    *reinterpret_cast<void**>(g_info + 0x14) = g_col;
    CModelInfo::ms_modelInfoPtrs[kModel] = reinterpret_cast<CBaseModelInfo*>(g_info);
    auto& cm = *reinterpret_cast<CColModel*>(g_col);
    const float sc = r.below(3) ? 3.f : PickScaleSane(r);
    const CVector mn = { -std::fabs(GenF(r, sc)) - 0.1f, -std::fabs(GenF(r, sc)) - 0.1f, -std::fabs(GenF(r, sc)) - 0.1f };
    CVector mx = { std::fabs(GenF(r, sc)) + 0.1f, std::fabs(GenF(r, sc)) + 0.1f, std::fabs(GenF(r, sc)) + 0.1f };
    if (r.below(3) == 0) mx.z += 6.f + r.f01() * 10.f;   // tall (> 6): the per-volume path
    cm.m_boundBox.m_vecMin = mn; cm.m_boundBox.m_vecMax = mx;
    if (r.below(10) == 0) { cm.m_boundBox.m_vecMin = GenV(r, sc); cm.m_boundBox.m_vecMax = GenV(r, sc); }
    if (r.below(4)) {
        auto& cd = *reinterpret_cast<CCollisionData*>(g_data);
        cd.m_nNumBoxes = (uint16)r.below(4); cd.m_nNumSpheres = (uint16)r.below(4);
        cd.m_pBoxes = reinterpret_cast<CColBox*>(g_boxes); cd.m_pSpheres = reinterpret_cast<CColSphere*>(g_spheres);
        for (unsigned i = 0; i < cd.m_nNumBoxes; ++i) { cd.m_pBoxes[i].m_vecMin = CVector{ -std::fabs(GenF(r, sc)), -std::fabs(GenF(r, sc)), mn.z + r.f01() * (mx.z - mn.z) }; cd.m_pBoxes[i].m_vecMax = CVector{ std::fabs(GenF(r, sc)), std::fabs(GenF(r, sc)), cd.m_pBoxes[i].m_vecMin.z + r.f01() * 3.f }; }
        for (unsigned i = 0; i < cd.m_nNumSpheres; ++i) { cd.m_pSpheres[i].m_vecCenter = CVector{ GenF(r, sc), GenF(r, sc), mn.z + r.f01() * (mx.z - mn.z) }; cd.m_pSpheres[i].m_fRadius = r.f01() * 2.f; }
        cm.m_pColData = &cd;
    }
}
static std::string EntDesc(PedFx& f) {
    using namespace efx;
    auto& cm = *reinterpret_cast<CColModel*>(g_col);
    return PedDesc(f) + " type " + std::to_string((int)f.ent().GetType()) + " bb " + V(cm.m_boundBox.m_vecMin) + V(cm.m_boundBox.m_vecMax) + " data " + (cm.m_pColData ? std::to_string(cm.m_pColData->m_nNumBoxes) + "b" + std::to_string(cm.m_pColData->m_nNumSpheres) + "s" : "none");
}

struct FiniteSpecials { unsigned old; FiniteSpecials() : old(g_defMask) { g_defMask = S_ZERO | S_DEN | S_TINY; } ~FiniteSpecials() { g_defMask = old; } };   // NaN / inf / huge excluded (x87 compare / overflow semantics of absurd boxes)
static void TestEntityBBox() {
    FiniteSpecials fin;
    Run("CPedGeometryAnalyser::ComputeEntityBoundingBoxCornersUncached 0x5F1FA0", [](Rng& r, std::string& d) {
        PedFx a; BuildEntityFx(r, a); PedFx e(a);
        const float z = GenF(r, 30.f);
        CVector ca[4], ce[4]; for (int i = 0; i < 4; ++i) ca[i] = CVector{ 7, 8, 9 }, ce[i] = CVector{ 7, 8, 9 };
        CPedGeometryAnalyser::ComputeEntityBoundingBoxCornersUncached(z, a.ent(), ca);
        oracle::Fn<void __cdecl(float, CEntity*, CVector*)>(0x5F1FA0)(z, &e.ent(), ce);
        if (SameV3Arr(ca, ce, 4) && SamePedFx(a, e)) return true;
        d = EntDesc(a) + " z " + F(z) + " got " + V3Arr(ca, 4) + " exe " + V3Arr(ce, 4);
        return false;
    });
    Run("CPedGeometryAnalyser::ComputeEntityBoundingBoxPlanesUncachedAll 0x5F2B80", [](Rng& r, std::string& d) {
        PedFx a; BuildEntityFx(r, a); PedFx e(a);
        const float z = GenF(r, 30.f);
        CVector pa[4], pe[4]; float da[4] = { 1, 2, 3, 4 }, de[4] = { 1, 2, 3, 4 };
        for (int i = 0; i < 4; ++i) pa[i] = CVector{ 7, 8, 9 }, pe[i] = CVector{ 7, 8, 9 };
        CPedGeometryAnalyser::ComputeEntityBoundingBoxPlanesUncachedAll(z, a.ent(), reinterpret_cast<CVector(*)[4]>(pa), da);
        oracle::Fn<void __cdecl(float, CEntity*, CVector*, float*)>(0x5F2B80)(z, &e.ent(), pe, de);
        if (SameV3Arr(pa, pe, 4) && SameBlob(da, de, 16)) return true;
        d = EntDesc(a) + " z " + F(z) + " got " + V3Arr(pa, 4) + " exe " + V3Arr(pe, 4);
        return false;
    });
    Run("CPedGeometryAnalyser::ComputeEntityBoundingBoxSegmentPlanesUncachedAll 0x5F2BC0", [](Rng& r, std::string& d) {
        PedFx a; BuildEntityFx(r, a); PedFx e(a);
        const float z = GenF(r, 30.f);
        CVector pa[4], pe[4]; float da[4] = { 1, 2, 3, 4 }, de[4] = { 1, 2, 3, 4 };
        for (int i = 0; i < 4; ++i) pa[i] = CVector{ 7, 8, 9 }, pe[i] = CVector{ 7, 8, 9 };
        CPedGeometryAnalyser::ComputeEntityBoundingBoxSegmentPlanesUncachedAll(z, a.ent(), pa, da);
        oracle::Fn<void __cdecl(float, CEntity*, CVector*, float*)>(0x5F2BC0)(z, &e.ent(), pe, de);
        if (SameV3Arr(pa, pe, 4) && SameBlob(da, de, 16)) return true;
        d = EntDesc(a) + " z " + F(z) + " got " + V3Arr(pa, 4) + " exe " + V3Arr(pe, 4);
        return false;
    });
    Run("CPedGeometryAnalyser::ComputeEntityBoundingBoxCentreUncachedAll 0x5F3B40", [](Rng& r, std::string& d) {
        PedFx a; BuildEntityFx(r, a); PedFx e(a);
        const float z = GenF(r, 30.f);
        CVector oa{ 7, 8, 9 }, oe{ 7, 8, 9 };
        CPedGeometryAnalyser::ComputeEntityBoundingBoxCentreUncachedAll(z, a.ent(), oa);
        oracle::Fn<void __cdecl(float, CEntity*, CVector*)>(0x5F3B40)(z, &e.ent(), &oe);
        if (SameV(oa, oe)) return true;
        d = EntDesc(a) + " z " + F(z) + " got " + V(oa) + " exe " + V(oe);
        return false;
    });
    Run("CPedGeometryAnalyser::ComputeEntityHitSide(point, entity) 0x5F3AC0", [](Rng& r, std::string& d) {
        PedFx a; BuildEntityFx(r, a); PedFx e(a);
        const CVector pt = GenV(r, 30.f);
        const int ra = CPedGeometryAnalyser::ComputeEntityHitSide(pt, a.ent());
        const int re = oracle::Fn<int __cdecl(const CVector*, CEntity*)>(0x5F3AC0)(&pt, &e.ent());
        Hit(ra != 0);
        if (ra == re) return true;
        d = EntDesc(a) + " pt " + V(pt) + " got " + std::to_string(ra) + " exe " + std::to_string(re);
        return false;
    });
    Run("CPedGeometryAnalyser::ComputeEntityHitSide(ped, entity) 0x5F3BC0", [](Rng& r, std::string& d) {
        PedFx a; BuildEntityFx(r, a); PedFx e(a);
        PedFx pa; BuildPedFx(r, pa, 30.f); PedFx pe(pa);
        const int ra = CPedGeometryAnalyser::ComputeEntityHitSide(pa.ped(), a.ent());
        const int re = oracle::Fn<int __cdecl(const CPed*, CEntity*)>(0x5F3BC0)(&pe.ped(), &e.ent());
        Hit(ra != 0);
        if (ra == re) return true;
        d = EntDesc(a) + " ped " + PedDesc(pa) + " got " + std::to_string(ra) + " exe " + std::to_string(re);
        return false;
    });
    Run("CPedGeometryAnalyser::ComputeClosestSurfacePoint(posn, entity) 0x5F36F0", [](Rng& r, std::string& d) {
        PedFx a; BuildEntityFx(r, a); PedFx e(a);
        const CVector p = GenV(r, 30.f);
        CVector oa{ 7, 8, 9 }, oe{ 7, 8, 9 };
        const bool ra = CPedGeometryAnalyser::ComputeClosestSurfacePoint(p, a.ent(), oa);
        const bool re = oracle::Fn<bool __cdecl(const CVector*, CEntity*, CVector*)>(0x5F36F0)(&p, &e.ent(), &oe);
        Hit(ra);
        if (ra == re && SameV(oa, oe)) return true;
        d = EntDesc(a) + " p " + V(p) + " got " + std::to_string(ra) + V(oa) + " exe " + std::to_string(re) + V(oe);
        return false;
    });
    Run("CPedGeometryAnalyser::ComputeClosestSurfacePoint(ped, entity) 0x5F3B70", [](Rng& r, std::string& d) {
        PedFx a; BuildEntityFx(r, a); PedFx e(a);
        PedFx pa; BuildPedFx(r, pa, 30.f); PedFx pe(pa);
        CVector oa{ 7, 8, 9 }, oe{ 7, 8, 9 };
        const bool ra = CPedGeometryAnalyser::ComputeClosestSurfacePoint(pa.ped(), a.ent(), oa);
        const bool re = oracle::Fn<bool __cdecl(const CPed*, CEntity*, CVector*)>(0x5F3B70)(&pe.ped(), &e.ent(), &oe);
        Hit(ra);
        if (ra == re && SameV(oa, oe)) return true;
        d = EntDesc(a) + " ped " + PedDesc(pa) + " got " + std::to_string(ra) + V(oa) + " exe " + std::to_string(re) + V(oe);
        return false;
    });
    Run("CPedGeometryAnalyser::ComputeEntityBoundingSphere 0x5F3C20", [](Rng& r, std::string& d) {
        PedFx a; BuildEntityFx(r, a); PedFx e(a);
        PedFx pa; BuildPedFx(r, pa, 30.f); PedFx pe(pa);
        alignas(16) uint8 sa[sizeof(CColSphere)], se[sizeof(CColSphere)]; std::memset(sa, 0, sizeof sa); std::memset(se, 0, sizeof se);
        CPedGeometryAnalyser::ComputeEntityBoundingSphere(pa.ped(), a.ent(), *reinterpret_cast<CColSphere*>(sa));
        oracle::Fn<void __cdecl(const CPed*, CEntity*, CColSphere*)>(0x5F3C20)(&pe.ped(), &e.ent(), reinterpret_cast<CColSphere*>(se));
        if (SameBlob(sa, se, sizeof sa)) return true;
        d = EntDesc(a) + " ped " + PedDesc(pa) + " got " + Hex(sa, sizeof sa) + " exe " + Hex(se, sizeof se);
        return false;
    });
    Run("CPedGeometryAnalyser::IsEntityBlockingTarget 0x5F3970", [](Rng& r, std::string& d) {
        PedFx a; BuildEntityFx(r, a); PedFx e(a);
        const CVector p = GenV(r, 30.f); const float dist = r.below(4) ? r.f01() * 4.f : GenF(r, 10.f);
        const bool ra = CPedGeometryAnalyser::IsEntityBlockingTarget(&a.ent(), p, dist);
        const bool re = oracle::Fn<bool __cdecl(CEntity*, const CVector*, float)>(0x5F3970)(&e.ent(), &p, dist);
        Hit(ra);
        if (ra == re) return true;
        d = EntDesc(a) + " p " + V(p) + " dist " + F(dist) + " got " + std::to_string(ra) + " exe " + std::to_string(re);
        return false;
    });
    Run("CPedGeometryAnalyser::ComputeMoveDirToAvoidEntity 0x5F3730", [](Rng& r, std::string& d) {
        PedFx a; BuildEntityFx(r, a); PedFx e(a);
        PedFx pa; BuildPedFx(r, pa, 30.f); PedFx pe(pa);
        CVector oa{ 7, 8, 9 }, oe{ 7, 8, 9 };
        CPedGeometryAnalyser::ComputeMoveDirToAvoidEntity(pa.ped(), a.ent(), oa);
        oracle::Fn<void __cdecl(const CPed*, CEntity*, CVector*)>(0x5F3730)(&pe.ped(), &e.ent(), &oe);
        if (SameV(oa, oe)) return true;
        d = EntDesc(a) + " ped " + PedDesc(pa) + " got " + V(oa) + " exe " + V(oe);
        return false;
    });
    for (int ov = 0; ov < 2; ++ov) {
        Run(ov ? "CPedGeometryAnalyser::ComputeRouteRoundEntityBoundingBox(from) 0x5F3DD0" : "CPedGeometryAnalyser::ComputeRouteRoundEntityBoundingBox 0x5F6110", [=](Rng& r, std::string& d) {
            using namespace efx;
            PedFx a; BuildEntityFx(r, a); PedFx e(a);
            PedFx pa; BuildPedFx(r, pa, 30.f); pa.Rewire(true); PedFx pe(pa);
            PedFx oa; BuildPedFx(r, oa, 30.f); oa.Rewire(true); oa.ent().m_nModelIndex = kModel; oa.ent().SetType(r.below(2) ? ENTITY_TYPE_PED : ENTITY_TYPE_OBJECT); PedFx oe(oa);   // a blocker
            static_cast<PedAcc&>(oa.ped()).m_pIntelligence = nullptr;
            const CVector from = r.below(3) ? pa.ent().GetPosition() : GenV(r, 30.f), to = GenV(r, 30.f);
            const int mode = r.below(3);
            los::script.clear();
            for (int k = 0; k < 12; ++k) { const bool hit = r.below(3) == 0; los::script.push_back({ hit, hit ? 1 + (int)r.below(4) : 0, GenV(r, 30.f), GenV(r, 1.f) }); }
            alignas(16) uint8 ra_[0x100] {}, re_[0x100] {};
            los::Reset(); los::roles[1] = &a.ent(); los::roles[2] = &pa.ent(); los::roles[3] = &oa.ent(); los::roles[4] = &oa.ent();
            int xa, xe;
            if (ov) xa = CPedGeometryAnalyser::ComputeRouteRoundEntityBoundingBox(pa.ped(), from, a.ent(), to, *reinterpret_cast<CPointRoute*>(ra_), mode);
            else    xa = CPedGeometryAnalyser::ComputeRouteRoundEntityBoundingBox(pa.ped(), a.ent(), to, *reinterpret_cast<CPointRoute*>(ra_), mode);
            const auto callsA = los::calls;
            los::Reset(); los::roles[1] = &e.ent(); los::roles[2] = &pe.ped(); los::roles[3] = &oe.ent(); los::roles[4] = &oe.ent();
            if (ov) xe = oracle::Fn<int __cdecl(const CPed*, const CVector*, CEntity*, const CVector*, CPointRoute*, int)>(0x5F3DD0)(&pe.ped(), &from, &e.ent(), &to, reinterpret_cast<CPointRoute*>(re_), mode);
            else    xe = oracle::Fn<int __cdecl(const CPed*, CEntity*, const CVector*, CPointRoute*, int)>(0x5F6110)(&pe.ped(), &e.ent(), &to, reinterpret_cast<CPointRoute*>(re_), mode);
            const auto callsE = los::calls;
            Hit(xa != 0);
            if (xa == xe && SameBlob(ra_, re_, 0x64) && los::SameCalls(callsA, callsE)) return true;
            d = EntDesc(a) + " ped " + PedDesc(pa) + " from " + V(from) + " to " + V(to) + " mode " + std::to_string(mode) + " got " + std::to_string(xa) + Hex(ra_, 0x64) + " " + los::Describe(callsA) + " exe " + std::to_string(xe) + Hex(re_, 0x64) + " " + los::Describe(callsE);
            return false;
        });
    }
}

static void TestPedMisc() {
    Run("CPed::CanSeeEntity 0x5E0730", [](Rng& r, std::string& d) {
        PedFx a; BuildMovingPed(r, a); a.Rewire(r.below(2));
        PedFx ea; BuildPedFx(r, ea, 100.f); ea.Rewire(r.below(2));
        a.ped().m_fCurrentRotation = r.below(2) ? r.f01() * 6.5f - 0.1f : GenF(r, 8.f, S_NOHUGE);
        PedFx b(a), eb(ea);
        const float lim = r.below(4) ? r.f01() * 3.2f : GenF(r, 8.f, S_NOHUGE);
        const bool ra = a.ped().CanSeeEntity(&ea.ent(), lim);
        const bool re = oracle::Fn<bool __fastcall(CPed*, int, CEntity*, float)>(0x5E0730)(&b.ped(), 0, &eb.ent(), lim);
        Hit(ra);
        if (ra == re) return true;
        d = PedDesc(a) + " ent " + PedDesc(ea) + " rot " + F(a.ped().m_fCurrentRotation) + " lim " + F(lim) + " got " + std::to_string(ra) + " exe " + std::to_string(re);
        return false;
    });
    Run("CPedIntelligence::IsInHearingRange 0x600C00", [](Rng& r, std::string& d) {
        PedFx a; BuildMovingPed(r, a); a.Rewire(true);
        alignas(16) uint8 ib[sizeof(CPedIntelligence)] {};
        auto* in = reinterpret_cast<CPedIntelligence*>(ib);
        in->m_pPed = &a.ped();
        in->m_fHearingRange = r.below(4) ? r.f01() * 80.f : GenF(r, 200.f, S_NOHUGE); in->m_fSeeingRange = r.below(4) ? r.f01() * 80.f : GenF(r, 200.f, S_NOHUGE);
        const CVector p = r.below(2) ? a.ped().GetPosition() + GenV(r, 80.f, S_NOHUGE) : GenV(r, 100.f, S_NOHUGE);
        const bool ra = in->IsInHearingRange(p);
        const bool re = oracle::Fn<bool __fastcall(CPedIntelligence*, int, const CVector*)>(0x600C00)(in, 0, &p);
        Hit(ra);
        const bool sa = in->IsInSeeingRange(p);
        const bool se = oracle::Fn<bool __fastcall(CPedIntelligence*, int, const CVector*)>(0x600C60)(in, 0, &p);
        Hit(sa);
        if (ra == re && sa == se) return true;
        d = PedDesc(a) + " ranges " + F(in->m_fHearingRange) + "," + F(in->m_fSeeingRange) + " p " + V(p) + " hear " + std::to_string(ra) + "/" + std::to_string(re) + " see " + std::to_string(sa) + "/" + std::to_string(se);
        return false;
    });
    Run("CPed::SortPeds 0x5E17E0", [](Rng& r, std::string& d) {
        if (!IsPC24()) return true;   // the quicksort compares a recomputed distance with a float-spilled pivot: out of bounds at 53-bit precision (in the exe, too)
        PedFx self; BuildMovingPed(r, self); self.Rewire(true);
        constexpr int N = 12;
        PedFx peds[N]; CPed* la[N]; CPed* le[N];
        const int n = 1 + r.below(N);
        for (int i = 0; i < n; ++i) { BuildPedFx(r, peds[i], 60.f); peds[i].Rewire(r.below(3) != 0); la[i] = le[i] = &peds[i].ped(); if (r.below(6) == 0 && i) peds[i].ped().m_matrix = peds[r.below(i)].ped().m_matrix; }
        PedFx selfE(self);
        self.ped().SortPeds(la, 0, n - 1);
        oracle::Fn<void __fastcall(CPed*, int, CPed**, int, int)>(0x5E17E0)(&selfE.ped(), 0, le, 0, n - 1);
        bool ok = true; for (int i = 0; i < n; ++i) ok &= (la[i] == le[i]);
        if (ok) return true;
        d = "n " + std::to_string(n) + " order got/exe:"; for (int i = 0; i < n; ++i) d += " " + std::to_string((int)(la[i] - &peds[0].ped())) + "/" + std::to_string((int)(le[i] - &peds[0].ped()));
        return false;
    });
}

static void TestPeds() {
    InstallPedStubs();
    TestPedMisc();
    TestColSphere();
    TestEntityBBox();
    TestPedGeometry();
    TestPedIK();
    TestPedMovement();
}

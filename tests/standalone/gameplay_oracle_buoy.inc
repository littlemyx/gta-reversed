// (d) cBuoyancy (water physics for peds / vehicles / boats): CalcBuoyancyForce, PreCalcSetup, SimpleSumBuoyancyData, FindWaterLevel(Norm), SimpleCalcBuoyancy, ProcessBuoyancy,
// ProcessBuoyancyBoat on a fake CPhysical / CVehicle. Water = the scripted CWaterLevel stubs of gameplay_oracle_camera.inc (cwf::), AddSplashParticles / ApplyTurnForce are recorded.
#include "Buoyancy.h"
#include "tHandlingData.h"
#include "Entity/Vehicle/Vehicle.h"
#include "Entity/Physical.h"

namespace bfx {
struct Splash { CVector from, to, dir; int reduce; int role; };
struct Turn { CVector force, point; int role; };
inline std::vector<Splash> splashes; inline std::vector<Turn> turns;
inline CEntity* roleEnt = nullptr;
}
static void __fastcall HostSplash(cBuoyancy*, int, CPhysical* e, CVector from, CVector to, CVector dir, uint8 reduce) { bfx::splashes.push_back({ from, to, dir, reduce, e == (CPhysical*)bfx::roleEnt ? 1 : 2 }); }
static void __fastcall HostTurn(CPhysical* e, int, CVector force, CVector point) { bfx::turns.push_back({ force, point, e == (CPhysical*)bfx::roleEnt ? 1 : 2 }); }
static bool SameBuoyRec() { return true; }

struct BuoyRun {
    Snap in{ 0xC1C890, 0xD0 }, pa{ 0xC1C890, 0xD0 }, pe{ 0xC1C890, 0xD0 };
    Snap cin{ 0xC1C858, 0x1C }, cpa{ 0xC1C858, 0x1C }, cpe{ 0xC1C858, 0x1C };
    Snap vin{ 0x8D32C8, 4 }, vpa{ 0x8D32C8, 4 }, vpe{ 0x8D32C8, 4 };
    void Begin(Rng& r) {
        auto* w = reinterpret_cast<uint32_t*>(0xC1C890);
        for (size_t i = 0; i < 0xD0 / 4; ++i) { const float v = GenF(r, 5.f, S_ZERO); std::memcpy(&w[i], &v, 4); }
        auto& b = mod_Buoyancy;
        std::memset(reinterpret_cast<uint8*>(&b.m_EntityMatrix) + 0x40, 0, 8);   // the CMatrix attach pointer / owns flag must not be garbage
        b.m_bInWater = r.below(2); b.m_bFlipUnknVector = r.below(2); b.m_bProcessingBoat = r.below(2);
        b.m_fNumCheckedPoints = r.below(4) ? 1.f + r.below(9) : GenF(r, 12.f, S_ZERO);
        b.m_fWaterLevel = GenF(r, 30.f, S_ZERO); b.m_fUnkn2 = r.below(2) ? 0.f : GenF(r, 1.f, S_ZERO);
        b.m_vecBoundingMin = CVector{ -std::fabs(GenF(r, 3.f, S_ZERO)), -std::fabs(GenF(r, 6.f, S_ZERO)), -std::fabs(GenF(r, 2.f, S_ZERO)) };
        b.m_vecBoundingMax = CVector{ std::fabs(GenF(r, 3.f, S_ZERO)), std::fabs(GenF(r, 6.f, S_ZERO)), std::fabs(GenF(r, 2.f, S_ZERO)) };
        b.m_vecCenterOffset = (b.m_vecBoundingMax - b.m_vecBoundingMin) * 0.5f;
        b.m_vecNormalizedCenterOffset = CVector{ r.f01(), r.f01(), r.f01() };
        auto* c = reinterpret_cast<uint32_t*>(0xC1C858);
        for (size_t i = 0; i < 7; ++i) { const float v = GenF(r, 3.f, S_ZERO); std::memcpy(&c[i], &v, 4); }
        *reinterpret_cast<uint32_t*>(0xC1C870) = r.below(2);   // the static-init guard dword (bit 0) / bBuoyancyDataSummed
        *reinterpret_cast<float*>(0x8D32C8) = r.below(3) ? 0.5f + r.f01() * 0.5f : GenF(r, 2.f, S_ZERO);
        CTimer::ms_fTimeStep = r.below(8) ? r.f01() * 3.f : GenF(r, 4.f, S_ZERO);
        cwf::Reset(); bfx::splashes.clear(); bfx::turns.clear();
    }
    void Mark()     { in.Save(); cin.Save(); vin.Save(); }
    void PortDone() { pa.Save(); cpa.Save(); vpa.Save(); in.Restore(); cin.Restore(); vin.Restore(); }
    void ExeDone()  { pe.Save(); cpe.Save(); vpe.Save(); in.Restore(); cin.Restore(); vin.Restore(); }
    bool Same()     { return pa.SameAs(pe) && cpa.SameAs(cpe) && vpa.SameAs(vpe); }
    std::string Diff() { return DiffDesc(pa.b.data(), pe.b.data(), pa.b.size(), "buoy") + DiffDesc(cpa.b.data(), cpe.b.data(), cpa.b.size(), "calc") + DiffDesc(vpa.b.data(), vpe.b.data(), 4, "pvm"); }
};
static bool SameSplashes(const std::vector<bfx::Splash>& a, const std::vector<bfx::Splash>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) if (!SameV(a[i].from, b[i].from) || !SameV(a[i].to, b[i].to) || !SameV(a[i].dir, b[i].dir) || a[i].reduce != b[i].reduce || a[i].role != b[i].role) return false;
    return true;
}
static bool SameZ(const CVector& x, const CVector& y) {   // bit-identical, except that +0 and -0 are the same force
    const auto f = [](float a, float b) { return (a == 0.f && b == 0.f) || FBits(a) == FBits(b) || (IsNanBits(FBits(a)) && IsNanBits(FBits(b))); };
    return f(x.x, y.x) && f(x.y, y.y) && f(x.z, y.z);
}
static bool SameTurns(const std::vector<bfx::Turn>& a, const std::vector<bfx::Turn>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) if (!SameZ(a[i].force, b[i].force) || !SameV(a[i].point, b[i].point) || a[i].role != b[i].role) return false;
    return true;
}
static std::string TurnsDiff(const std::vector<bfx::Turn>& a, const std::vector<bfx::Turn>& b) {
    for (size_t i = 0; i < a.size() && i < b.size(); ++i) if (!SameV(a[i].force, b[i].force) || !SameV(a[i].point, b[i].point) || a[i].role != b[i].role) return " turn[" + std::to_string(i) + "] got " + V(a[i].force) + V(a[i].point) + " exe " + V(b[i].force) + V(b[i].point);
    return "";
}
static std::string SplashDesc(const std::vector<bfx::Splash>& s) { std::string r = std::to_string(s.size()) + " splashes"; for (size_t i = 0; i < s.size() && i < 4; ++i) r += " [" + V(s[i].from) + V(s[i].to) + V(s[i].dir) + "]"; return r; }

// physical entity with a col model (BuildEntityFx), random vehicle fields and model index
alignas(16) static uint8 g_buoyHandling[sizeof(tHandlingData)];
static void BuildBuoyEntity(Rng& r, PedFx& f, bool vehicle) {
    BuildEntityFx(r, f);
    auto& e = f.ent();
    static const int16 models[] = { 417, 460, 446, 452, 453, 498, 454, 472, 473, 484, 595, 400, 500, 520, 0 };
    const int16 model = models[r.below(15)];
    e.m_nModelIndex = model;
    CModelInfo::ms_modelInfoPtrs[model] = reinterpret_cast<CBaseModelInfo*>(efx::g_info);
    e.SetType(vehicle ? ENTITY_TYPE_VEHICLE : (r.below(2) ? ENTITY_TYPE_PED : ENTITY_TYPE_OBJECT));
    auto& p = *reinterpret_cast<CPhysical*>(f.e);
    p.m_fMass = r.below(8) ? 1.f + r.f01() * 3000.f : GenF(r, 100.f, S_ZERO);
    p.m_vecMoveSpeed = GenV(r, r.below(2) ? 0.5f : 5.f);
    if (vehicle) {
        auto& v = *reinterpret_cast<CVehicle*>(f.e);
        v.m_nVehicleType = (eVehicleType)r.below(12); v.m_nVehicleSubType = (eVehicleType)r.below(12);
        if (r.below(2)) v.m_nVehicleType = VEHICLE_TYPE_BOAT;
        v.m_vehicleSpecialColIndex = -1;
        std::memset(g_buoyHandling, 0, sizeof g_buoyHandling);
        reinterpret_cast<tHandlingData*>(g_buoyHandling)->m_fSuspensionDampingLevel = r.below(2) ? 0.1f + r.f01() * 0.3f : GenF(r, 1.f, S_ZERO);
        v.m_pHandlingData = reinterpret_cast<tHandlingData*>(g_buoyHandling);
    }
}

static void TestBuoyancy() {
    ZeroSpecials fin;   // denormal products of tiny values: 1 ulp (x87 registers vs float spills) in the exe's x87 sequences
    PatchHostFn(PmAddr(&cBuoyancy::AddSplashParticles), (void*)&HostSplash);
    oracle::Patch(0x6C34E0, (void*)&HostSplash);
    PatchHostFn(PmAddr(&CPhysical::ApplyTurnForce), (void*)&HostTurn);
    oracle::Patch(0x542A50, (void*)&HostTurn);
    Run("cBuoyancy::CalcBuoyancyForce 0x6C2750", [](Rng& r, std::string& d) {
        BuoyRun b; b.Begin(r);
        PedFx a; BuildBuoyEntity(r, a, r.below(2)); PedFx e(a);
        CVector ta{ 7, 8, 9 }, fa{ 7, 8, 9 }, te{ 7, 8, 9 }, fe{ 7, 8, 9 };
        b.Mark();
        const bool ra = mod_Buoyancy.CalcBuoyancyForce(reinterpret_cast<CPhysical*>(a.e), &ta, &fa); b.PortDone();
        const bool re = oracle::Fn<bool __fastcall(cBuoyancy*, int, CPhysical*, CVector*, CVector*)>(0x6C2750)(&mod_Buoyancy, 0, reinterpret_cast<CPhysical*>(e.e), &te, &fe); b.ExeDone();
        Hit(ra);
        if (ra == re && SameV(ta, te) && SameV(fa, fe) && b.Same()) return true;
        d = "mass " + F(reinterpret_cast<CPhysical*>(a.e)->m_fMass) + " ts " + F(CTimer::ms_fTimeStep) + " got " + std::to_string(ra) + V(ta) + V(fa) + " exe " + std::to_string(re) + V(te) + V(fe) + b.Diff();
        return false;
    });
    Run("cBuoyancy::PreCalcSetup 0x6C2B90", [](Rng& r, std::string& d) {
        BuoyRun b; b.Begin(r);
        PedFx a; BuildBuoyEntity(r, a, r.below(2)); PedFx e(a);
        const float buoy = GenF(r, 5.f);
        b.Mark();
        mod_Buoyancy.PreCalcSetup(reinterpret_cast<CPhysical*>(a.e), buoy); b.PortDone();
        oracle::Fn<void __fastcall(cBuoyancy*, int, CPhysical*, float)>(0x6C2B90)(&mod_Buoyancy, 0, reinterpret_cast<CPhysical*>(e.e), buoy); b.ExeDone();
        if (b.Same()) return true;
        d = EntDesc(a) + " model " + std::to_string(a.ent().m_nModelIndex) + " vtype " + std::to_string((int)reinterpret_cast<CVehicle*>(a.e)->m_nVehicleType) + "/" + std::to_string((int)reinterpret_cast<CVehicle*>(a.e)->m_nVehicleSubType) + b.Diff();
        return false;
    });
    Run("cBuoyancy::SimpleSumBuoyancyData 0x6C2970", [](Rng& r, std::string& d) {
        BuoyRun b; b.Begin(r);
        CVector off = GenV(r, 3.f); const int st = r.below(3);
        CVector offE = off;
        b.Mark();
        const float ra = mod_Buoyancy.SimpleSumBuoyancyData(&off, (tWaterLevel)st); b.PortDone();
        const float re = oracle::Fn<float __fastcall(cBuoyancy*, int, CVector*, int)>(0x6C2970)(&mod_Buoyancy, 0, &offE, st); b.ExeDone();
        if (SameF(ra, re) && SameV(off, offE) && b.Same()) return true;
        d = "off " + V(off) + " st " + std::to_string(st) + " got " + F(ra) + " exe " + F(re) + b.Diff();
        return false;
    });
    for (int norm = 0; norm < 2; ++norm) {
        Run(norm ? "cBuoyancy::FindWaterLevelNorm 0x6C28C0" : "cBuoyancy::FindWaterLevel 0x6C2810", [=](Rng& r, std::string& d) {
            BuoyRun b; b.Begin(r); CamWorldScript::Roll(r); cwf::Reset();
            GenMatrix(r, reinterpret_cast<uint8*>(&mod_Buoyancy.m_EntityMatrix), 0.f);
            mod_Buoyancy.m_vecPos = GenV(r, 30.f);
            const CVector zp = GenV(r, 5.f);
            CVector oa = GenV(r, 4.f), oe = oa, na{ 7, 8, 9 }, ne{ 7, 8, 9 }; tWaterLevel sa = (tWaterLevel)7, se = (tWaterLevel)7;
            b.Mark();
            if (norm) mod_Buoyancy.FindWaterLevelNorm(zp, &oa, &sa, &na); else mod_Buoyancy.FindWaterLevel(zp, &oa, &sa);
            const auto recA = cwf::g_rec; b.PortDone(); cwf::Reset();
            if (norm) oracle::Fn<void __fastcall(cBuoyancy*, int, const CVector*, CVector*, tWaterLevel*, CVector*)>(0x6C28C0)(&mod_Buoyancy, 0, &zp, &oe, &se, &ne);
            else      oracle::Fn<void __fastcall(cBuoyancy*, int, const CVector*, CVector*, tWaterLevel*)>(0x6C2810)(&mod_Buoyancy, 0, &zp, &oe, &se);
            const auto recE = cwf::g_rec; b.ExeDone();
            if (SameV(oa, oe) && sa == se && (!norm || SameV(na, ne)) && cwf::SameRec(recA, recE) && b.Same()) return true;
            d = "zp " + V(zp) + " off " + V(oa) + "/" + V(oe) + " st " + std::to_string((int)sa) + "/" + std::to_string((int)se) + (norm ? " n " + V(na) + V(ne) : "") + " " + cwf::Desc(recA) + " / " + cwf::Desc(recE) + b.Diff();
            return false;
        });
    }
    Run("cBuoyancy::SimpleCalcBuoyancy 0x6C3B00", [](Rng& r, std::string& d) {
        BuoyRun b; b.Begin(r); CamWorldScript::Roll(r); cwf::Reset();
        PedFx a; BuildBuoyEntity(r, a, r.below(2)); PedFx e(a);
        GenMatrix(r, reinterpret_cast<uint8*>(&mod_Buoyancy.m_EntityMatrix), 0.f);
        mod_Buoyancy.m_vecPos = GenV(r, 30.f);
        mod_Buoyancy.m_vecInitialZPos = CVector{ 0, 0, mod_Buoyancy.m_vecPos.z };
        b.Mark();
        bfx::roleEnt = &a.ent();
        mod_Buoyancy.SimpleCalcBuoyancy(reinterpret_cast<CPhysical*>(a.e)); const auto recA = cwf::g_rec; const auto spA = bfx::splashes; b.PortDone();
        cwf::Reset(); bfx::splashes.clear(); bfx::roleEnt = &e.ent();
        oracle::Fn<void __fastcall(cBuoyancy*, int, CPhysical*)>(0x6C3B00)(&mod_Buoyancy, 0, reinterpret_cast<CPhysical*>(e.e)); const auto recE = cwf::g_rec; const auto spE = bfx::splashes; b.ExeDone();
        if (b.Same() && cwf::SameRec(recA, recE) && SameSplashes(spA, spE)) return true;
        d = EntDesc(a) + " " + SplashDesc(spA) + " / exe " + SplashDesc(spE) + " " + cwf::Desc(recA) + " / " + cwf::Desc(recE) + b.Diff();
        return false;
    });
    Run("cBuoyancy::SimpleCalcBuoyancy(natural state) 0x6C3B00", [](Rng& r, std::string& d) {
        BuoyRun b; b.Begin(r); CamWorldScript::Roll(r); cwf::Reset();
        PedFx a; BuildBuoyEntity(r, a, r.below(2)); PedFx e(a);
        GenMatrix(r, reinterpret_cast<uint8*>(&mod_Buoyancy.m_EntityMatrix), 0.f);
        mod_Buoyancy.m_vecPos = GenV(r, 30.f);
        mod_Buoyancy.m_vecInitialZPos = CVector{ 0, 0, mod_Buoyancy.m_vecPos.z };
        oracle::Fn<void __fastcall(cBuoyancy*, int, CPhysical*, float)>(0x6C2B90)(&mod_Buoyancy, 0, reinterpret_cast<CPhysical*>(a.e), r.below(4) ? r.f01() * 30.f : GenF(r, 30.f));   // the state PreCalcSetup leaves (exe), identical for both sides
        b.Mark();
        bfx::roleEnt = &a.ent();
        mod_Buoyancy.SimpleCalcBuoyancy(reinterpret_cast<CPhysical*>(a.e)); const auto recA = cwf::g_rec; const auto spA = bfx::splashes; b.PortDone();
        cwf::Reset(); bfx::splashes.clear(); bfx::roleEnt = &e.ent();
        oracle::Fn<void __fastcall(cBuoyancy*, int, CPhysical*)>(0x6C3B00)(&mod_Buoyancy, 0, reinterpret_cast<CPhysical*>(e.e)); const auto recE = cwf::g_rec; const auto spE = bfx::splashes; b.ExeDone();
        if (b.Same() && cwf::SameRec(recA, recE) && SameSplashes(spA, spE)) return true;
        d = EntDesc(a) + " " + SplashDesc(spA) + " / exe " + SplashDesc(spE) + " " + cwf::Desc(recA) + " / " + cwf::Desc(recE) + b.Diff();
        return false;
    });
    Run("cBuoyancy::ProcessBuoyancy 0x6C3EF0", [](Rng& r, std::string& d) {
        BuoyRun b; b.Begin(r); CamWorldScript::Roll(r); cwf::Reset();
        PedFx a; BuildBuoyEntity(r, a, r.below(2)); PedFx e(a);
        const float buoy = r.below(4) ? r.f01() * 30.f : GenF(r, 30.f);
        CVector ta{ 7, 8, 9 }, fa{ 7, 8, 9 }, te{ 7, 8, 9 }, fe{ 7, 8, 9 };
        b.Mark();
        bfx::roleEnt = &a.ent();
        const bool ra = mod_Buoyancy.ProcessBuoyancy(reinterpret_cast<CPhysical*>(a.e), buoy, &ta, &fa); const auto recA = cwf::g_rec; const auto spA = bfx::splashes; b.PortDone();
        cwf::Reset(); bfx::splashes.clear(); bfx::roleEnt = &e.ent();
        const bool re = oracle::Fn<bool __fastcall(cBuoyancy*, int, CPhysical*, float, CVector*, CVector*)>(0x6C3EF0)(&mod_Buoyancy, 0, reinterpret_cast<CPhysical*>(e.e), buoy, &te, &fe); const auto recE = cwf::g_rec; const auto spE = bfx::splashes; b.ExeDone();
        Hit(ra);
        if (ra == re && SameV(ta, te) && SameV(fa, fe) && b.Same() && cwf::SameRec(recA, recE) && SameSplashes(spA, spE) && SamePedFx(a, e)) return true;
        d = EntDesc(a) + " buoy " + F(buoy) + " got " + std::to_string(ra) + V(ta) + V(fa) + " exe " + std::to_string(re) + V(te) + V(fe) + " " + SplashDesc(spA) + " / " + SplashDesc(spE) + " " + cwf::Desc(recA) + " / " + cwf::Desc(recE) + b.Diff();
        return false;
    });
    Run("cBuoyancy::ProcessBuoyancyBoat 0x6C3030", [](Rng& r, std::string& d) {
        BuoyRun b; b.Begin(r); CamWorldScript::Roll(r); cwf::Reset();
        PedFx a; BuildBuoyEntity(r, a, true); reinterpret_cast<CVehicle*>(a.e)->m_nVehicleType = VEHICLE_TYPE_BOAT; PedFx e(a);
        const float buoy = r.below(4) ? r.f01() * 30.f : GenF(r, 30.f);
        const bool under = r.below(3) == 0;
        CVector ta{ 7, 8, 9 }, fa{ 1, 2, 3 }, te{ 7, 8, 9 }, fe{ 1, 2, 3 };
        b.Mark();
        bfx::roleEnt = &a.ent();
        const bool ra = mod_Buoyancy.ProcessBuoyancyBoat(reinterpret_cast<CVehicle*>(a.e), buoy, &ta, &fa, under); const auto recA = cwf::g_rec; const auto tuA = bfx::turns; b.PortDone();
        cwf::Reset(); bfx::turns.clear(); bfx::roleEnt = &e.ent();
        const bool re = oracle::Fn<bool __fastcall(cBuoyancy*, int, CVehicle*, float, CVector*, CVector*, bool)>(0x6C3030)(&mod_Buoyancy, 0, reinterpret_cast<CVehicle*>(e.e), buoy, &te, &fe, under); const auto recE = cwf::g_rec; const auto tuE = bfx::turns; b.ExeDone();
        Hit(ra);
        if (ra == re && SameV(ta, te) && SameV(fa, fe) && b.Same() && cwf::SameRec(recA, recE) && SameTurns(tuA, tuE) && SamePedFx(a, e)) return true;
        d = EntDesc(a) + " model " + std::to_string(a.ent().m_nModelIndex) + " buoy " + F(buoy) + " under " + std::to_string(under) + " got " + std::to_string(ra) + V(ta) + V(fa) + " exe " + std::to_string(re) + V(te) + V(fe) + " turns " + std::to_string(tuA.size()) + "/" + std::to_string(tuE.size()) + TurnsDiff(tuA, tuE) + " " + cwf::Desc(recA) + " / " + cwf::Desc(recE) + b.Diff();
        return false;
    });
}

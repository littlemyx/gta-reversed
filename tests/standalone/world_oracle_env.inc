// (c) world / environment: CMaths LUT, CWaterLevel, (CWeather / CFire / CCullZones / CPopulation / CStreaming / CGarages / CPickups below), see world_oracle_test.cpp
#include "Maths.h"
#include "WaterLevel.h"
#include "Weather.h"
#include "Timer.h"

// ------------------------------------------------------------------------------------------------------------------------------
// water fixture: random well-formed quads / triangles with their vertices, block table entries (single / combo), time and wave statics
namespace wfx {
using PI = CWaterLevel::PolyInfo;
static void Clear() {
    std::memset(&CWaterLevel::m_aVertices, 0, sizeof CWaterLevel::m_aVertices);
    std::memset(&CWaterLevel::WaterQuads, 0, sizeof CWaterLevel::WaterQuads);
    std::memset(&CWaterLevel::WaterTriangles, 0, sizeof CWaterLevel::WaterTriangles);
    std::memset(&CWaterLevel::m_BlockPolyInfo, 0, sizeof CWaterLevel::m_BlockPolyInfo);
    std::memset(&CWaterLevel::m_PolyCombos, 0, sizeof CWaterLevel::m_PolyCombos);
}
static void SetVert(int idx, int x, int y, Rng& r) {
    auto& v = CWaterLevel::m_aVertices[idx];
    v.x = (int16)x; v.y = (int16)y;
    v.rp.z = r.below(4) ? GenF(r, 30.f) : GenF(r, PickScale(r));
    v.rp.bigWaves = r.below(4) ? r.f01() * 2.f : GenF(r, 3.f);
    v.rp.smallWaves = r.below(4) ? r.f01() * 2.f : GenF(r, 3.f);
    v.rp.flowX = (int8)r.u32(); v.rp.flowY = (int8)r.u32();
}
static void SetupTime(Rng& r) {
    CTimer::m_snTimeInMilliseconds = r.below(6) ? (r.u32() % 4000000) : r.u32();
    CWaterLevel::m_nWaterTimeOffset = r.below(3) ? (r.u32() % 100000) : r.u32();
    CWeather::Wavyness = r.below(4) ? r.f01() * 2.f : GenF(r, 4.f);
    CWeather::SunGlare = r.below(4) ? r.f01() : GenF(r, 2.f);
}
struct Poly { bool quad; int id; int x0, y0, x1, y1; };
// builds the tables; returns a query position inside (or near) one of the polygons, with its block pointing at it (single entry or a combo)
static CVector Setup(Rng& r) {
    Clear();
    SetupTime(r);
    std::vector<Poly> polys;
    const int nq = 1 + r.below(8), nt = r.below(5);
    int vi = 0;
    for (int q = 0; q < nq; ++q) {
        const int x0 = ((int)(r.u32() % 5800) - 2900) & ~1, y0 = ((int)(r.u32() % 5800) - 2900) & ~1, w = 2 * (1 + r.below(60)), h = 2 * (1 + r.below(60));
        SetVert(vi + 0, x0, y0, r); SetVert(vi + 1, x0 + w, y0, r); SetVert(vi + 2, x0, y0 + h, r); SetVert(vi + 3, x0 + w, y0 + h, r);
        auto& pq = CWaterLevel::WaterQuads[q];
        for (int k = 0; k < 4; ++k) pq.verts[k] = (uint16)(vi + k);
        pq.bToBeRendered = (r.below(2) != 0); pq.bInvisible = (r.below(4) == 0); pq.bLimitedDepth = (r.below(3) == 0);
        polys.push_back({ true, q, x0, y0, x0 + w, y0 + h });
        vi += 4;
    }
    for (int t = 0; t < nt; ++t) {
        const int x0 = ((int)(r.u32() % 5800) - 2900) & ~1, y0 = ((int)(r.u32() % 5800) - 2900) & ~1, w = 2 * (1 + r.below(60)), h = 2 * (1 + r.below(60));
        const bool leftEdge = r.below(2);
        SetVert(vi + 0, x0, y0, r); SetVert(vi + 1, x0 + w, y0, r); SetVert(vi + 2, leftEdge ? x0 : x0 + w, y0 + h, r);
        auto& pt = CWaterLevel::WaterTriangles[t];
        for (int k = 0; k < 3; ++k) pt.verts[k] = (uint16)(vi + k);
        pt.bToBeRendered = (r.below(2) != 0); pt.bInvisible = (r.below(4) == 0); pt.bLimitedDepth = (r.below(3) == 0);
        polys.push_back({ false, t, x0, y0, x0 + w, y0 + h });
        vi += 3;
    }
    const Poly& p = polys[r.below((int)polys.size())];
    CVector pos;
    if (r.below(6) == 0) {   // arbitrary position (outside the world, empty blocks ...)
        pos = CVector(GenF(r, r.below(2) ? 3500.f : 100.f), GenF(r, r.below(2) ? 3500.f : 100.f), GenF(r, 100.f));
    } else {
        const float mx = r.below(8) ? 0.f : 10.f;
        pos.x = p.x0 - mx + r.f01() * (p.x1 - p.x0 + 2 * mx);
        pos.y = p.y0 - mx + r.f01() * (p.y1 - p.y0 + 2 * mx);
        const auto& v0 = CWaterLevel::m_aVertices[p.quad ? CWaterLevel::WaterQuads[p.id].verts[0] : CWaterLevel::WaterTriangles[p.id].verts[0]];
        pos.z = r.below(3) ? v0.rp.z + GenF(r, r.below(2) ? 10.f : 40.f, S_NAN | S_ZERO | S_DEN | S_TINY) : GenF(r, 100.f);
    }
    if (r.below(12) == 0) pos = CVector(GenF(r, 3000.f), GenF(r, 3000.f), GenF(r, 50.f));
    // the block of `pos` (floor(x * 0.002 + 6)) refers to the polygon
    const int bx = (int)std::floor(pos.x * 0.002f + 6.0f), by = (int)std::floor(pos.y * 0.002f + 6.0f);
    if (bx >= 0 && bx < 12 && by >= 0 && by < 12) {
        const auto ptype = p.quad ? PI::PType::SINGLE_QUAD : PI::PType::SINGLE_TRI;
        if (r.below(2)) {
            CWaterLevel::m_BlockPolyInfo[bx][by] = PI((uint16)p.id, ptype);
        } else {
            const int start = r.below(20) * 8;
            int k = start;
            const int pre = r.below(4);
            for (int j = 0; j < pre; ++j) { const auto& o = polys[r.below((int)polys.size())]; CWaterLevel::m_PolyCombos[k++] = PI((uint16)o.id, o.quad ? PI::PType::SINGLE_QUAD : PI::PType::SINGLE_TRI); }
            CWaterLevel::m_PolyCombos[k++] = PI((uint16)p.id, ptype);
            CWaterLevel::m_PolyCombos[k++] = PI(0, PI::PType::NONE);
            CWaterLevel::m_BlockPolyInfo[bx][by] = PI((uint16)start, PI::PType::COMBO);
        }
        // a few more random blocks (for the line / flow / mark tests)
        for (int j = 0; j < 6; ++j) { const auto& o = polys[r.below((int)polys.size())]; CWaterLevel::m_BlockPolyInfo[r.below(12)][r.below(12)] = PI((uint16)o.id, o.quad ? PI::PType::SINGLE_QUAD : PI::PType::SINGLE_TRI); }
    }
    return pos;
}
// whole water state compared byte-wise (flags of the polygons, counters, block lists)
static constexpr uintptr_t WBEG = 0xC214D0, WEND = 0xC22900;
}   // namespace wfx

static void TestEnv() {
    {   // CMaths: the exe fills the sine table at start-up (0x59AC90); the port computes it in a static initialiser; afterwards the exe's table is the one in memory
        Snap portLut(0xBB3E00, 1024), exeLut(0xBB3E00, 1024);
        portLut.Save();
        oracle::Fn<void __cdecl()>(0x59AC90)();
        exeLut.Save();
        Run("CMaths::InitMathsTables (sine LUT) 0x59AC90", [&](Rng&, std::string& d) {
            if (portLut.SameAs(exeLut)) return true;
            d = "LUT differs: " + DiffDesc(portLut.b.data(), exeLut.b.data(), 1024, "lut");
            return false;
        }, 1);
        exeLut.Restore();
    }
    Run("CMaths::GetSinFast 0x4A1340", [](Rng& r, std::string& d) {
        const float x = r.below(3) ? GenF(r, 20.f) : GenF(r, PickScale(r));
        const float a = CMaths::GetSinFast(x), e = oracle::Fn<float __cdecl(float)>(0x4A1340)(x);
        if (SameF(a, e)) return true;
        d = "x " + F(x) + " got " + F(a) + " exe " + F(e); return false;
    });
    Run("CMaths::GetCosFast 0x4A1360", [](Rng& r, std::string& d) {
        const float x = r.below(3) ? GenF(r, 20.f) : GenF(r, PickScale(r));
        const float a = CMaths::GetCosFast(x), e = oracle::Fn<float __cdecl(float)>(0x4A1360)(x);
        if (SameF(a, e)) return true;
        d = "x " + F(x) + " got " + F(a) + " exe " + F(e); return false;
    });

    // ---- CWaterLevel: waves
    Run("CWaterLevel::CalculateWavesOnlyForCoordinate2 0x6E7210", [](Rng& r, std::string& d) {
        wfx::SetupTime(r);
        const int x = r.below(3) ? (int)(r.u32() % 6000) - 3000 : (int)r.u32(), y = r.below(3) ? (int)(r.u32() % 6000) - 3000 : (int)r.u32();
        const float big = r.below(4) ? r.f01() * 2.f : GenF(r, 4.f), smallA = r.below(4) ? r.f01() * 2.f : GenF(r, 4.f);
        float a = r.below(3) ? 0.f : GenF(r, 5.f), e = a;
        CWaterLevel::CalculateWavesOnlyForCoordinate2(x, y, big, smallA, &a);
        oracle::Fn<void __cdecl(int, int, float, float, float*)>(0x6E7210)(x, y, big, smallA, &e);
        if (SameF(a, e)) return true;
        d = "x " + std::to_string(x) + " y " + std::to_string(y) + " big " + F(big) + " smallA " + F(smallA) + " t " + std::to_string(CTimer::m_snTimeInMilliseconds) + " off " + std::to_string(CWaterLevel::m_nWaterTimeOffset) + " wavy " + F(CWeather::Wavyness) + " got " + F(a) + " exe " + F(e);
        return false;
    });
    Run("CWaterLevel::CalculateWavesOnlyForCoordinate 0x6E6EF0", [](Rng& r, std::string& d) {
        wfx::SetupTime(r);
        const int x = r.below(3) ? (int)(r.u32() % 6000) - 3000 : (int)r.u32(), y = r.below(3) ? (int)(r.u32() % 6000) - 3000 : (int)r.u32();
        const float big = r.below(4) ? r.f01() * 2.f : GenF(r, 4.f), smallA = r.below(4) ? r.f01() * 2.f : GenF(r, 4.f);
        float wa = r.below(3) ? 0.f : GenF(r, 5.f), we = wa, ca = 7.f, ce = 7.f, ga = 8.f, ge = 8.f;
        CVector na{ 1, 2, 3 }, ne{ 1, 2, 3 };
        CWaterLevel::CalculateWavesOnlyForCoordinate(x, y, big, smallA, wa, ca, ga, na);
        oracle::Fn<void __cdecl(int, int, float, float, float*, float*, float*, CVector*)>(0x6E6EF0)(x, y, big, smallA, &we, &ce, &ge, &ne);
        if (SameF(wa, we) & SameF(ca, ce) & SameF(ga, ge) & SameV(na, ne)) return true;
        d = "x " + std::to_string(x) + " y " + std::to_string(y) + " big " + F(big) + " smallA " + F(smallA) + " wave got " + F(wa) + " exe " + F(we) + " colmul got " + F(ca) + " exe " + F(ce) + " glare got " + F(ga) + " exe " + F(ge) + " n got " + V(na) + " exe " + V(ne);
        return false;
    });
    Run("CWaterLevel::AddWaveToResult 0x6E81E0", [](Rng& r, std::string& d) {
        wfx::SetupTime(r);
        const float x = r.below(3) ? GenF(r, 3000.f) : GenF(r, PickScale(r)), y = r.below(3) ? GenF(r, 3000.f) : GenF(r, PickScale(r));
        const float f1 = r.below(4) ? r.f01() * 2.f : GenF(r, 4.f), f2 = r.below(4) ? r.f01() * 2.f : GenF(r, 4.f);
        float la = r.below(3) ? 0.f : GenF(r, 20.f), le = la;
        const bool withN = r.below(2);
        CVector na{ 1, 2, 3 }, ne{ 1, 2, 3 };
        CWaterLevel::AddWaveToResult(x, y, &la, f1, f2, withN ? &na : nullptr);
        oracle::Fn<void __cdecl(float, float, float*, float, float, CVector*)>(0x6E81E0)(x, y, &le, f1, f2, withN ? &ne : nullptr);
        if (SameF(la, le) && SameV(na, ne)) return true;
        d = "x " + F(x) + " y " + F(y) + " f1 " + F(f1) + " f2 " + F(f2) + " withN " + std::to_string(withN) + " lvl got " + F(la) + " exe " + F(le) + " n got " + V(na) + " exe " + V(ne);
        return false;
    });

    // ---- CWaterLevel: tables
    Run("CWaterQuad::GetWaterLevel 0x6E5BB0", [](Rng& r, std::string& d) {
        const CVector pos = wfx::Setup(r);
        const int q = r.below(8);
        const auto& quad = CWaterLevel::WaterQuads[q];
        float la = 5.f, ba = 6.f, sa = 7.f, le = 5.f, be = 6.f, se = 7.f;
        const bool withW = r.below(4);
        const bool a = quad.GetWaterLevel(pos.x, pos.y, pos.z, &la, withW ? &ba : nullptr, withW ? &sa : nullptr);
        const bool e = oracle::Fn<int __cdecl(const void*, float, float, float, float*, float*, float*)>(0x6E5BB0)(&quad, pos.x, pos.y, pos.z, &le, withW ? &be : nullptr, withW ? &se : nullptr) & 0xFF;
        Hit(a);
        if (a == e && SameF(la, le) && SameF(ba, be) && SameF(sa, se)) return true;
        d = "pos " + V(pos) + " q " + std::to_string(q) + " ret " + std::to_string(a) + "/" + std::to_string(e) + " lvl " + F(la) + "/" + F(le) + " big " + F(ba) + "/" + F(be) + " smallA " + F(sa) + "/" + F(se);
        return false;
    });
    Run("CWaterTriangle::GetWaterLevel 0x6E5E90", [](Rng& r, std::string& d) {
        const CVector pos = wfx::Setup(r);
        const int t = r.below(5);
        const auto& tri = CWaterLevel::WaterTriangles[t];
        float la = 5.f, ba = 6.f, sa = 7.f, le = 5.f, be = 6.f, se = 7.f;
        const bool withW = r.below(4);
        const bool a = tri.GetWaterLevel(pos.x, pos.y, pos.z, &la, withW ? &ba : nullptr, withW ? &sa : nullptr);
        const bool e = oracle::Fn<int __cdecl(const void*, float, float, float, float*, float*, float*)>(0x6E5E90)(&tri, pos.x, pos.y, pos.z, &le, withW ? &be : nullptr, withW ? &se : nullptr) & 0xFF;
        Hit(a);
        if (a == e && SameF(la, le) && SameF(ba, be) && SameF(sa, se)) return true;
        d = "pos " + V(pos) + " t " + std::to_string(t) + " ret " + std::to_string(a) + "/" + std::to_string(e) + " lvl " + F(la) + "/" + F(le) + " big " + F(ba) + "/" + F(be) + " smallA " + F(sa) + "/" + F(se);
        return false;
    });
    Run("CWaterLevel::GetWaterLevelNoWaves 0x6E8580", [](Rng& r, std::string& d) {
        const CVector pos = wfx::Setup(r);
        float la = 5.f, ba = 6.f, sa = 7.f, le = 5.f, be = 6.f, se = 7.f;
        const bool withW = r.below(4);
        const bool a = CWaterLevel::GetWaterLevelNoWaves(pos, &la, withW ? &ba : nullptr, withW ? &sa : nullptr);
        const bool e = oracle::Fn<int __cdecl(float, float, float, float*, float*, float*)>(0x6E8580)(pos.x, pos.y, pos.z, &le, withW ? &be : nullptr, withW ? &se : nullptr) & 0xFF;
        Hit(a);
        if (a == e && SameF(la, le) && SameF(ba, be) && SameF(sa, se)) return true;
        d = "pos " + V(pos) + " ret " + std::to_string(a) + "/" + std::to_string(e) + " lvl " + F(la) + "/" + F(le) + " big " + F(ba) + "/" + F(be) + " smallA " + F(sa) + "/" + F(se);
        return false;
    });
    Run("CWaterLevel::GetWaterLevel 0x6EB690", [](Rng& r, std::string& d) {
        const CVector pos = wfx::Setup(r);
        float la = 5.f, le = 5.f;
        const uint8 touching = r.below(2);
        const bool withN = r.below(2);
        CVector na{ 1, 2, 3 }, ne{ 1, 2, 3 };
        const bool a = CWaterLevel::GetWaterLevel(pos.x, pos.y, pos.z, la, touching, withN ? &na : nullptr);
        const bool e = oracle::Fn<int __cdecl(float, float, float, float*, int, CVector*)>(0x6EB690)(pos.x, pos.y, pos.z, &le, touching, withN ? &ne : nullptr) & 0xFF;
        Hit(a);
        if (a == e && SameF(la, le) && SameV(na, ne)) return true;
        d = "pos " + V(pos) + " touching " + std::to_string(touching) + " ret " + std::to_string(a) + "/" + std::to_string(e) + " lvl " + F(la) + "/" + F(le) + " n " + V(na) + "/" + V(ne);
        return false;
    });
    Run("CWaterLevel::TestLineAgainstWater 0x6E61B0", [](Rng& r, std::string& d) {
        const CVector p = wfx::Setup(r);
        CVector s = p, e = p;
        s.z = GenF(r, 30.f) + 1.f; e.z = -GenF(r, 30.f) - 1.f;   // crosses z = 0 mostly
        if (r.below(2)) { s.x += GenF(r, 300.f); s.y += GenF(r, 300.f); e.x += GenF(r, 300.f); e.y += GenF(r, 300.f); }
        if (r.below(8) == 0) { s.z = GenF(r, 5.f); e.z = GenF(r, 5.f); }
        if (g_trace) std::printf("   s %s e %s\n", V(s).c_str(), V(e).c_str());
        CVector ha{ 1, 2, 3 }, he{ 1, 2, 3 };
        const bool withH = true;   // the callers always pass the out pointer (both sides write it unconditionally)
        const bool a = CWaterLevel::TestLineAgainstWater(s, e, withH ? &ha : nullptr);
        const bool x = oracle::Fn<int __cdecl(float, float, float, float, float, float, CVector*)>(0x6E61B0)(s.x, s.y, s.z, e.x, e.y, e.z, withH ? &he : nullptr) & 0xFF;
        Hit(a);
        if (a == x && SameV(ha, he)) return true;
        d = "s " + V(s) + " e " + V(e) + " ret " + std::to_string(a) + "/" + std::to_string(x) + " hit got " + V(ha) + " exe " + V(he);
        return false;
    });
    Run("CWaterLevel::AddPolyToBlock 0x6E5750", [](Rng& r, std::string& d) {
        wfx::Setup(r);
        CWaterLevel::NumWaterZonePolys = 150 + r.below(500);
        const int bx = r.below(12), by = r.below(12), id = r.below(301), type = r.below(4);
        Snap init(wfx::WBEG, wfx::WEND - wfx::WBEG), ga(wfx::WBEG, wfx::WEND - wfx::WBEG), ge(wfx::WBEG, wfx::WEND - wfx::WBEG); init.Save();
        CWaterLevel::AddPolyToBlock(bx, by, (uint32)id, (uint32)type);
        ga.Save(); init.Restore();
        oracle::Fn<void __cdecl(int, int, int, int)>(0x6E5750)(bx, by, id, type);
        ge.Save();
        if (ga.SameAs(ge)) return true;
        d = "bx " + std::to_string(bx) + " by " + std::to_string(by) + " type " + std::to_string(type) + " id " + std::to_string(id) + DiffDesc(ga.b.data(), ge.b.data(), ga.b.size(), "w");
        return false;
    });
    Run("CWaterLevel::MarkQuadsAndPolysToBeRendered 0x6E5810", [](Rng& r, std::string& d) {
        wfx::Setup(r);
        const int bx = r.below(12), by = r.below(12);
        const bool interior = r.below(2);
        Snap init(wfx::WBEG, wfx::WEND - wfx::WBEG), ga(wfx::WBEG, wfx::WEND - wfx::WBEG), ge(wfx::WBEG, wfx::WEND - wfx::WBEG); init.Save();
        CWaterLevel::MarkQuadsAndPolysToBeRendered(bx, by, interior);
        ga.Save(); init.Restore();
        oracle::Fn<void __cdecl(int, int, int)>(0x6E5810)(bx, by, interior);
        ge.Save();
        if (ga.SameAs(ge)) return true;
        d = "bx " + std::to_string(bx) + " by " + std::to_string(by) + " interior " + std::to_string(interior) + DiffDesc(ga.b.data(), ge.b.data(), ga.b.size(), "w");
        return false;
    });
}

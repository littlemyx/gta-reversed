// (c) world / environment: CMaths LUT, CWaterLevel, (CWeather / CFire / CCullZones / CPopulation / CStreaming / CGarages / CPickups below), see world_oracle_test.cpp
#include "Maths.h"
#include "WaterLevel.h"
#include "Weather.h"
#include "Timer.h"
#include "CullZones.h"
#include "Clock.h"
#include "Game.h"
#include "Audio/Entities/AEWeatherAudioEntity.h"

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

// ------------------------------------------------------------------------------------------------------------------------------
// CCullZones: random zone tables (valid-looking zones also BEYOND the counts: the exe's loops stop at the counts)
namespace czfx {
static constexpr uintptr_t BEG = 0xC815C0, END = 0xC87AD0;   // mirror / tunnel / attribute zone tables, counts, current flags
static void InitDef(CZoneDef& z, Rng& r, const CVector& centre, bool wide) {
    const float sc = wide ? 400.f : 40.f;
    z.Init(centre, 1.f + r.f01() * sc, 1.f + r.f01() * sc, GenF(r, 50.f, S_ZERO) - 5.f, r.below(3) ? 0.f : r.f01() * sc, r.below(3) ? 0.f : r.f01() * sc, 5.f + r.f01() * 100.f);
}
static CVector Setup(Rng& r) {
    std::memset(reinterpret_cast<void*>(BEG), 0, END - BEG);
    CVector q = GenV(r, 800.f, S_NOHUGE); q.z = r.f01() * 60.f - 10.f;
    const auto zone = [&](CZoneDef& z) { InitDef(z, r, r.below(3) ? q + CVector(GenF(r, 30.f, S_ZERO), GenF(r, 30.f, S_ZERO), 0.f) : GenV(r, 800.f, S_NOHUGE), r.below(4) == 0); };
    CCullZones::NumAttributeZones = r.below(30); CCullZones::NumTunnelAttributeZones = r.below(10); CCullZones::NumMirrorAttributeZones = r.below(10);
    const int na = CCullZones::NumAttributeZones + r.below(5) * (r.below(3) == 0), nt = CCullZones::NumTunnelAttributeZones + r.below(3) * (r.below(3) == 0), nm = CCullZones::NumMirrorAttributeZones + r.below(3) * (r.below(3) == 0);
    for (int i = 0; i < na; ++i) { zone(CCullZones::aAttributeZones[i].zoneDef); CCullZones::aAttributeZones[i].flags = (eZoneAttributes)(r.u32() & 0xFFFF); }
    for (int i = 0; i < nt; ++i) { zone(CCullZones::aTunnelAttributeZones[i].zoneDef); CCullZones::aTunnelAttributeZones[i].flags = (eZoneAttributes)(r.u32() & 0xFFFF); }
    for (int i = 0; i < nm; ++i) { auto& m = CCullZones::aMirrorAttributeZones[i]; zone(m.zoneDef); m.mirrorV = GenF(r, 100.f); m.mirrorNormalX = (int8)r.u32(); m.mirrorNormalY = (int8)r.u32(); m.mirrorNormalZ = (int8)r.u32(); m.flags = (eMirrorFlags)(r.u32() & 0x7F); }
    CCullZones::CurrentFlags_Player = (int32)r.u32(); CCullZones::CurrentFlags_Camera = (int32)r.u32();
    return q;
}
}   // namespace czfx

// ------------------------------------------------------------------------------------------------------------------------------
// CWeather fixture. The weather audio entity (Service) is stubbed on both sides; Update's AddRain is skipped by a camera above z = 900 (its first early-out)
void CAEWeatherAudioEntity::Service() {}
static void __fastcall HostNoop(void*) {}
namespace wxfx {
static constexpr uintptr_t BEG = 0xC812A8, END = 0xC81460;
alignas(16) static uint8 g_fakePlayerPed[0x800];
alignas(16) static uint8 g_fakePedIntel[0x400];
static void Setup(Rng& r) {
    std::memset(reinterpret_cast<void*>(BEG), 0, END - BEG);
    for (uintptr_t a = BEG; a < END; a += 4) if (!(a >= 0xC81360 && a < 0xC813E0)) *reinterpret_cast<uint32_t*>(a) = r.below(2) ? 0 : r.u32();
    std::memset(reinterpret_cast<void*>(0xC81420), 0, 0x20);   // the tunnel points + their init flag: function-local statics the exe constructs on its first call
    const auto wt = [&] { return (eWeatherType)r.below(23); };
    CWeather::OldWeatherType = wt(); CWeather::NewWeatherType = wt();
    CWeather::ForcedWeatherType = r.below(3) ? WEATHER_UNDEFINED : wt();
    CWeather::WeatherTypeInList = r.below(64);
    CWeather::WeatherRegion = (eWeatherRegion)r.below(5);
    CWeather::InterpolationValue = r.below(6) ? r.f01() : GenF(r, 1.5f);
    auto fl = [&](float& v, float sc = 1.f) { v = r.below(4) ? r.f01() * sc : GenF(r, sc * 2.f); };
    fl(CWeather::Rain); fl(CWeather::Sandstorm); fl(CWeather::WetRoads); fl(CWeather::UnderWaterness); fl(CWeather::InTunnelness); fl(CWeather::Foggyness); fl(CWeather::SunGlare); fl(CWeather::HeatHaze);
    fl(CWeather::WaterFogFXFade); fl(CWeather::HeatHazeFXFade); fl(CWeather::Wind, 1.5f); fl(CWeather::Wavyness);
    CWeather::LightningBurst = r.below(3) == 0; CWeather::LightningFlash = r.below(2); CWeather::WhenToPlayLightningSound = 0; CWeather::LightningStart = r.below(2000); CWeather::LightningDuration = r.below(30);
    CWeather::LightningFlashLastChange = (int32)(r.u32() % 1000000); CWeather::WaterFogFXFadingOut = r.below(2); CWeather::HeatHazeFXLastMinute = r.below(62) - 1; CWeather::bScriptsForceRain = r.below(2);
    CWeather::CurrentRainParticleStrength = r.below(111); CWeather::StreamAfterRainTimer = (int32)r.below(900);
    CClock::ms_nGameClockHours = (uint8)r.below(24); CClock::ms_nGameClockMinutes = (uint8)r.below(60); CClock::ms_nGameClockSeconds = (uint16)r.below(60);
    CTimer::m_FrameCounter = r.below(2) ? r.below(100) * 16 : r.u32() % 100000;
    CTimer::m_snTimeInMilliseconds = r.u32() % 2000000000u;
    CTimer::ms_fTimeStep = r.below(8) ? r.f01() * 3.f : GenF(r, 4.f);
    CGame::currArea = (eAreaCodes)(r.below(4) ? 0 : r.below(18));
    *reinterpret_cast<uint32_t*>(0xB6F028 + 0x14) = 0;   // TheCamera.m_matrix = null: position from the placement
    const CVector cp(GenF(r, 3000.f), GenF(r, 3000.f), 901.f + r.f01() * 47.f); std::memcpy(reinterpret_cast<void*>(0xB6F028 + 4), &cp, 12);
    CCullZones::CurrentFlags_Camera = r.below(3) ? 0 : (int32)(r.u32() & 0x2008); CCullZones::CurrentFlags_Player = r.below(3) ? 0 : (int32)(r.u32() & 0x8);
    CTimeCycle::m_CurrentStoredValue = r.below(16); CTimeCycle::m_VectorToSun[CTimeCycle::m_CurrentStoredValue] = GenV(r, 1.f);
    CTimeCycle::m_CurrentColours.m_nWaterFogAlpha = r.below(3) ? 0 : (uint16)r.below(120);
    *reinterpret_cast<uint8*>(0xC7C714) = r.below(2);   // CSpecialFX::bSnapShotActive
    *reinterpret_cast<int32_t*>(0xA43088) = 0;          // CReplay::Mode = MODE_OFF
    // player ped (Players[0].m_pPed) with a random area code
    std::memset(g_fakePlayerPed, 0, sizeof g_fakePlayerPed);
    g_fakePlayerPed[0x2F] = (uint8)(r.below(3) ? 0 : r.below(18));   // CEntity::m_nAreaCode
    std::memset(g_fakePedIntel, 0, sizeof g_fakePedIntel); *reinterpret_cast<void**>(g_fakePlayerPed + 0x47C) = g_fakePedIntel;   // CPed::m_pIntelligence (the exe walks a 5-pointer array in it, a no-op in the port)
    *reinterpret_cast<void**>(0xB7CD98) = g_fakePlayerPed; *reinterpret_cast<int32_t*>(0xB7CD74) = 0;
}
}   // namespace wxfx
// host stand-ins for the world / game services the port's CWeather::Update calls (not part of this test)
CPlayerPed* FindPlayerPed(int) { return reinterpret_cast<CPlayerPed*>(wxfx::g_fakePlayerPed); }
CVector     FindPlayerCoors(int) { return {}; }
CVehicle*   FindPlayerVehicle(int, bool) { return nullptr; }
bool CGame::CanSeeOutSideFromCurrArea() { return oracle::Fn<int __cdecl()>(0x53C4A0)() & 0xFF; }

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

    // ---- CCullZones
    Run("CZoneDef::IsPointWithin 0x72D850", [](Rng& r, std::string& d) {
        const CVector q = czfx::Setup(r);
        CZoneDef z{}; czfx::InitDef(z, r, r.below(2) ? q : GenV(r, 800.f, S_NOHUGE), r.below(4) == 0);
        if (r.below(16) == 0) { z.m_cornerX = (int16)r.u32(); z.m_vec1X = (int16)r.u32(); z.m_vec2Y = (int16)r.u32(); z.m_maxZ = (int16)r.u32(); }   // raw int16 extremes (the squares overflow int16, not int32)
        const CVector p = r.below(2) ? q : GenV(r, 900.f);
        const bool a = z.IsPointWithin(p);
        const bool e = oracle::Fn<int __fastcall(const CZoneDef*, int, float, float, float)>(0x72D850)(&z, 0, p.x, p.y, p.z) & 0xFF;
        Hit(a);
        if (a == e) return true;
        d = "p " + V(p) + " zone " + Hex(&z, sizeof z) + " got " + std::to_string(a) + " exe " + std::to_string(e); return false;
    });
    Run("CCullZones::FindAttributesForCoors 0x72D970", [](Rng& r, std::string& d) {
        const CVector q = czfx::Setup(r);
        const auto a = CCullZones::FindAttributesForCoors(q);
        const auto e = (eZoneAttributes)(oracle::Fn<int __cdecl(float, float, float)>(0x72D970)(q.x, q.y, q.z) & 0xFFFF);
        Hit(a != 0);
        if (a == e) return true;
        d = "q " + V(q) + " n " + std::to_string(CCullZones::NumAttributeZones) + " got " + std::to_string(a) + " exe " + std::to_string(e); return false;
    });
    Run("CCullZones::FindTunnelAttributesForCoors 0x72D9F0", [](Rng& r, std::string& d) {
        const CVector q = czfx::Setup(r);
        const auto a = CCullZones::FindTunnelAttributesForCoors(q);
        const auto e = (eZoneAttributes)(oracle::Fn<int __cdecl(float, float, float)>(0x72D9F0)(q.x, q.y, q.z) & 0xFFFF);
        Hit(a != 0);
        if (a == e) return true;
        d = "q " + V(q) + " n " + std::to_string(CCullZones::NumTunnelAttributeZones) + " got " + std::to_string(a) + " exe " + std::to_string(e); return false;
    });
    Run("CCullZones::FindMirrorAttributesForCoors 0x72DA70", [](Rng& r, std::string& d) {
        const CVector q = czfx::Setup(r);
        const auto a = CCullZones::FindMirrorAttributesForCoors(q);
        const auto e = oracle::Fn<CMirrorAttributeZone* __cdecl(float, float, float)>(0x72DA70)(q.x, q.y, q.z);
        Hit(a != nullptr);
        if (a == e) return true;
        d = "q " + V(q) + " n " + std::to_string(CCullZones::NumMirrorAttributeZones) + " got " + std::to_string((int)(((uintptr_t)a - 0xC815C0) / 0x18)) + " exe " + std::to_string((int)(((uintptr_t)e - 0xC815C0) / 0x18)); return false;
    });
    {   // flag getters: random flag words, exact bool result
        struct G { const char* n; unsigned va; bool (*port)(); };
        static const G gs[] = {
            { "CamCloseInForPlayer", 0x72DD20, [] { return CCullZones::CamCloseInForPlayer(); } }, { "CamStairsForPlayer", 0x72DD30, [] { return CCullZones::CamStairsForPlayer(); } },
            { "Cam1stPersonForPlayer", 0x72DD40, [] { return CCullZones::Cam1stPersonForPlayer(); } }, { "NoPolice", 0x72DD50, [] { return CCullZones::NoPolice(); } },
            { "PoliceAbandonCars", 0x72DD60, [] { return CCullZones::PoliceAbandonCars(); } }, { "InRoomForAudio", 0x72DD70, [] { return CCullZones::InRoomForAudio(); } },
            { "FewerCars", 0x72DD80, [] { return CCullZones::FewerCars(); } }, { "FewerPeds", 0x72DD90, [] { return CCullZones::FewerPeds(); } },
            { "DoINeedToLoadCollision", 0x72DDA0, [] { return CCullZones::DoINeedToLoadCollision(); } }, { "CamNoRain", 0x72DDB0, [] { return CCullZones::CamNoRain(); } },
            { "PlayerNoRain", 0x72DDC0, [] { return CCullZones::PlayerNoRain(); } }, { "DoExtraAirResistanceForPlayer", 0x72DDD0, [] { return CCullZones::DoExtraAirResistanceForPlayer(); } },
        };
        for (const auto& g : gs) {
            Run(std::string("CCullZones::") + g.n + " 0x" + HexVA(g.va), [&g](Rng& r, std::string& d) {
                CCullZones::CurrentFlags_Player = (int32)r.u32(); CCullZones::CurrentFlags_Camera = (int32)r.u32();
                const bool a = g.port(), e = oracle::Fn<int __cdecl()>(g.va)() & 0xFF;
                Hit(a);
                if (a == e) return true;
                d = "player " + std::to_string(CCullZones::CurrentFlags_Player) + " cam " + std::to_string(CCullZones::CurrentFlags_Camera) + " got " + std::to_string(a) + " exe " + std::to_string(e); return false;
            }, 200);
        }
    }
    Run("CCullZones::AddCullZone 0x72DF70", [](Rng& r, std::string& d) {
        czfx::Setup(r);
        const CVector pos = GenV(r, r.below(2) ? 800.f : PickScale(r));
        const float v1x = GenF(r, 200.f), v1y = GenF(r, 200.f), minZ = GenF(r, 100.f), v2x = GenF(r, 200.f), v2y = GenF(r, 200.f), maxZ = GenF(r, 100.f);
        const uint16 flags = r.below(3) ? (uint16)(r.u32() & 0xFFFF) : (uint16)((r.below(2) ? 0x800 : 0x80) | (r.u32() & 0x7F00));
        Snap init(czfx::BEG, czfx::END - czfx::BEG), ga(czfx::BEG, czfx::END - czfx::BEG), ge(czfx::BEG, czfx::END - czfx::BEG); init.Save();
        CCullZones::AddCullZone(pos, v1x, v1y, minZ, v2x, v2y, maxZ, flags);
        ga.Save(); init.Restore();
        oracle::Fn<void __cdecl(const CVector*, float, float, float, float, float, float, int)>(0x72DF70)(&pos, v1x, v1y, minZ, v2x, v2y, maxZ, flags);
        ge.Save();
        if (ga.SameAs(ge)) return true;
        d = "pos " + V(pos) + " flags " + std::to_string(flags) + DiffDesc(ga.b.data(), ge.b.data(), ga.b.size(), "z"); return false;
    });
    Run("CCullZones::AddMirrorAttributeZone 0x72DC10", [](Rng& r, std::string& d) {
        czfx::Setup(r);
        const CVector pos = GenV(r, r.below(2) ? 800.f : PickScale(r));
        const float v1x = GenF(r, 200.f), v1y = GenF(r, 200.f), minZ = GenF(r, 100.f), v2x = GenF(r, 200.f), v2y = GenF(r, 200.f), maxZ = GenF(r, 100.f), mv = GenF(r, 50.f), nx = GenF(r, 1.5f), ny = GenF(r, 1.5f), nz = GenF(r, 1.5f);
        const uint16 flags = (uint16)(r.u32() & 0xFFFF);
        Snap init(czfx::BEG, czfx::END - czfx::BEG), ga(czfx::BEG, czfx::END - czfx::BEG), ge(czfx::BEG, czfx::END - czfx::BEG); init.Save();
        CCullZones::AddMirrorAttributeZone(pos, v1x, v1y, minZ, v2x, v2y, maxZ, (eZoneAttributes)flags, mv, nx, ny, nz);
        ga.Save(); init.Restore();
        oracle::Fn<void __cdecl(const CVector*, float, float, float, float, float, float, int, float, float, float, float)>(0x72DC10)(&pos, v1x, v1y, minZ, v2x, v2y, maxZ, flags, mv, nx, ny, nz);
        ge.Save();
        if (ga.SameAs(ge)) return true;
        d = "pos " + V(pos) + " flags " + std::to_string(flags) + " n " + F(nx) + F(ny) + F(nz) + DiffDesc(ga.b.data(), ge.b.data(), ga.b.size(), "z"); return false;
    });

    // ---- CWeather
    oracle::Patch(0x5052F0, (void*)&HostNoop);   // CAEWeatherAudioEntity::Service (exe side)
    Run("CWeather::Update 0x72B850", [](Rng& r, std::string& d) {
        wxfx::Setup(r);
        const uint32 seed = r.u32();
        SeedBoth(seed);   // the port uses the game-owned LCG, the exe its CRT rand on the fake per-thread block: same seed, each side consumes its own
        Snap init(wxfx::BEG, wxfx::END - wxfx::BEG), ga(wxfx::BEG, wxfx::END - wxfx::BEG), ge(wxfx::BEG, wxfx::END - wxfx::BEG);
        Snap i2(0x8D5EAC, 4), a2(0x8D5EAC, 4), e2(0x8D5EAC, 4), i3(0x8D6078, 4), a3(0x8D6078, 4), e3(0x8D6078, 4);
        init.Save(); i2.Save(); i3.Save();
        CWeather::Update();
        ga.Save(); a2.Save(); a3.Save();
        init.Restore(); i2.Restore(); i3.Restore();
        oracle::Fn<void __cdecl()>(0x72B850)();
        ge.Save(); e2.Save(); e3.Save();
        for (auto* s : { &ga, &ge }) std::memset(s->b.data() + (0xC81420 - wxfx::BEG), 0, 0x20);   // function-local statics (tunnel points) the exe constructs on its first call
        if (ga.SameAs(ge) && a2.SameAs(e2) && a3.SameAs(e3)) return true;
        d = "old " + std::to_string(CWeather::OldWeatherType) + " new " + std::to_string(CWeather::NewWeatherType) + DiffDesc(ga.b.data(), ge.b.data(), ga.b.size(), "w");
        return false;
    });

    Run("CWeather::UpdateInTunnelness 0x72B630", [](Rng& r, std::string& d) {
        wxfx::Setup(r);
        CCullZones::CurrentFlags_Camera = r.below(5) ? 0x2000 : 0;
        const CVector cp = r.below(2) ? CVector(GenF(r, 3000.f), GenF(r, 3000.f), GenF(r, 100.f)) : CVector(85.f + GenF(r, 400.f), -1020.f + GenF(r, 400.f), 20.f);
        std::memcpy(reinterpret_cast<void*>(0xB6F028 + 4), &cp, 12);
        alignas(16) static uint8 s_camMat[0x80];
        std::memset(s_camMat, 0, sizeof s_camMat);
        const CVector fw = r.below(2) ? GenV(r, 1.f) : GenDir(r);
        if (r.below(2)) {   // m_matrix set: forward = matrix->y-axis (+0x10), position = matrix->pos (+0x30)
            std::memcpy(s_camMat + 0x10, &fw, 12); std::memcpy(s_camMat + 0x30, &cp, 12);
            *reinterpret_cast<void**>(0xB6F028 + 0x14) = s_camMat;
        } else {            // placement only: heading at +0x10
            *reinterpret_cast<float*>(0xB6F028 + 0x10) = r.below(8) ? GenF(r, 7.f) : GenF(r, PickScale(r));
        }
        CTimer::ms_fTimeStep = r.below(8) ? r.f01() * 4.f : GenF(r, 6.f);
        const float cur = CWeather::InTunnelness;
        CWeather::UpdateInTunnelness(); const float a = CWeather::InTunnelness;
        CWeather::InTunnelness = cur;
        oracle::Fn<void __cdecl()>(0x72B630)(); const float e = CWeather::InTunnelness;
        if (SameF(a, e)) return true;
        d = "cam " + V(cp) + " fwd " + V(fw) + " heading " + F(*reinterpret_cast<float*>(0xB6F028 + 0x10)) + " m_matrix " + std::to_string(*reinterpret_cast<uint32_t*>(0xB6F028 + 0x14) != 0) + " ts " + F(CTimer::ms_fTimeStep) + " cur " + F(cur) + " got " + F(a) + " exe " + F(e);
        return false;
    });

    Run("CCollision::DistToLine (tunnel use) 0x417610", [](Rng& r, std::string& d) {
        const CVector p(r.below(2) ? 85.f : 1683.f, r.below(2) ? -1020.f : -1956.f, 0.f);
        CVector dir = GenV(r, 1.f); dir.Normalise();
        const CVector a(p.x + GenF(r, 300.f), p.y + GenF(r, 300.f), 0.f), b(a.x + dir.x * 100.f, a.y + dir.y * 100.f, dir.z * 100.f);
        const float x = CCollision::DistToLine(a, b, p), e = oracle::Fn<float __cdecl(const CVector*, const CVector*, const CVector*)>(0x417610)(&a, &b, &p);
        if (SameF(x, e)) return true;
        d = "a " + V(a) + " b " + V(b) + " got " + F(x) + " exe " + F(e); return false;
    });

    Run("CVector::Normalise 0x59C910", [](Rng& r, std::string& d) {
        CVector a = r.below(2) ? GenV(r, 1.f) : GenV(r, PickScale(r)), e = a;
        a.Normalise();
        oracle::Fn<void __fastcall(CVector*, int)>(0x59C910)(&e, 0);
        if (SameV(a, e)) return true;
        d = "got " + V(a) + " exe " + V(e); return false;
    });
}

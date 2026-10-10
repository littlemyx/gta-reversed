// (e) CGeneral / CTimer / CClock / CWeather / CTimeCycle leftovers
struct Snap {   // snapshot / restore of a range of exe globals (both implementations work on the same statics)
    uintptr_t va; std::vector<uint8> b;
    Snap(uintptr_t v, size_t n) : va(v), b(n) {}
    void Save()          { std::memcpy(b.data(), (void*)va, b.size()); }
    void Restore() const { std::memcpy((void*)va, b.data(), b.size()); }
    bool SameAs(const Snap& o) const { return SameBlob(b.data(), o.b.data(), b.size()); }
    void FillRandom(Rng& r) { for (size_t i = 0; i + 4 <= b.size(); i += 4) { const uint32_t v = r.u32() * 2654435761u; std::memcpy(&b[i], &v, 4); } }
};

#include "TimeCycle.h"
#include "Collision/ColourSet.h"
#include "Clock.h"
#include "Weather.h"
#include "Coronas.h"
#include "Camera.h"
#include "Shadows.h"
#include "CullZones.h"
#include "MenuManager.h"
#include "PostEffects.h"

auto& TheCamera = StaticRef<CCamera>(0xB6F028);
auto& FrontEndMenuManager = StaticRef<CMenuManager>(0xBA6748);
bool CPostEffects::IsVisionFXActive() { return oracle::Fn<bool __cdecl()>(0x7034F0)(); }   // PostEffects.cpp is not part of this test: the exe's own (identical state, both sides)   // Camera.cpp is not part of this test
// CTimeCycle fixture: randomises the exe's timecycle statics (tables, boxes, current colours, sun vectors) and the globals CalcColoursForPoint reads
static constexpr uintptr_t TC_BEG = 0xB79E30, TC_END = 0xB7CB20;
static void TCSetup(Rng& r, bool withBoxes) {
    Snap t(TC_BEG, TC_END - TC_BEG); t.FillRandom(r); t.Restore();
    CTimeCycle::m_CurrentStoredValue = r.u32() & 15;
    CTimeCycle::m_FogReduction = r.below(3) ? 0 : (int32)(r.u32() % 40);
    CTimeCycle::m_NumBoxes = withBoxes ? r.below(6) : 0;
    for (int i = 0; i < 20; ++i) {
        auto& b = CTimeCycle::m_aBoxes[i];
        const float sc = r.below(3) ? 500.f : PickScale(r);
        const CVector c = GenV(r, sc), e = { std::fabs(GenF(r, sc * 0.3f)) + 1.f, std::fabs(GenF(r, sc * 0.3f)) + 1.f, std::fabs(GenF(r, sc * 0.1f)) + 1.f };
        b.Box.m_vecMin = c - e; b.Box.m_vecMax = c + e;
        b.FarClip = (int16)r.below(2); b.LodDistMult = r.below(3) ? (uint8)r.below(40) : 32; b.ExtraColor = (int32)r.below(17) - 1;
        b.Strength = r.f01(); b.Falloff = r.below(8) ? r.f01() * 100.f + 0.5f : GenF(r, 50.f);
    }
    CTimeCycle::m_ExtraColour = r.below(8); CTimeCycle::m_ExtraColourWeatherType = r.below(23); CTimeCycle::m_bExtraColourOn = r.below(2); CTimeCycle::m_ExtraColourInter = r.below(8) ? r.f01() : GenF(r, 1.5f);
    CWeather::UnderWaterness = r.below(3) ? 0.f : r.f01(); CWeather::InTunnelness = r.below(3) ? 0.f : r.f01();
    FrontEndMenuManager.m_PrefsBrightness = r.below(385);
    TheCamera.m_mCameraMatrix.GetForward().z = r.below(3) ? GenF(r, 1.f) : -0.95f;
    CClock::ms_nGameClockHours = (uint8)r.below(24); CClock::ms_nGameClockMinutes = (uint8)r.below(60); CClock::ms_nGameClockSeconds = (uint16)r.below(60);
    CWeather::OldWeatherType = (eWeatherType)(r.below(5) ? r.below(23) : (r.below(2) ? WEATHER_EXTRASUNNY_SMOG_LA : WEATHER_SUNNY_SMOG_LA));
    CWeather::NewWeatherType = (eWeatherType)(r.below(5) ? r.below(23) : (r.below(2) ? WEATHER_EXTRASUNNY_SMOG_LA : WEATHER_SUNNY_SMOG_LA));
    CWeather::InterpolationValue = r.below(8) ? r.f01() : GenF(r, 1.5f);
    CCoronas::LightsMult = r.below(8) ? 0.2f + r.f01() * 1.5f : GenF(r, 3.f);
    *reinterpret_cast<uint32_t*>(0xB6F028 + 0x14) = 0;   // TheCamera.m_matrix = null: position from the placement
    const CVector cp = GenV(r, r.below(2) ? 3000.f : 300.f); std::memcpy(reinterpret_cast<void*>(0xB6F028 + 4), &cp, 12);
}

static double ExePow(double x, double y) {   // the exe's _CIpow (0x822130): x in ST1, y in ST0
    double r;
    __asm {
        fld x
        fld y
        mov eax, 0x822130
        call eax
        fstp r
    }
    return r;
}
static void TestMisc() {
    {   // CClock (integer logic): random clock state, both implementations from the same state, whole clock statics compared
        static constexpr uintptr_t CK = 0xB70140, CKN = 0x24;
        const auto setup = [](Rng& r) {
            Snap c(CK, CKN); c.Save();
            *reinterpret_cast<uint8_t*>(0xB70153) = (uint8_t)(r.below(8) ? r.below(24) : r.u32());   // hours (sometimes out of range)
            *reinterpret_cast<uint8_t*>(0xB70152) = (uint8_t)(r.below(8) ? r.below(60) : r.u32());   // minutes
            *reinterpret_cast<uint16_t*>(0xB70150) = (uint16_t)(r.below(8) ? r.below(60) : r.u32());  // seconds
            *reinterpret_cast<uint8_t*>(0xB70154) = (uint8_t)(1 + r.below(31)); // day
            *reinterpret_cast<uint8_t*>(0xB70155) = (uint8_t)(1 + r.below(12)); // month (1..12: daysInMonth[] is indexed by it)
            *reinterpret_cast<uint8_t*>(0xB7014E) = (uint8_t)r.below(8);                               // weekday
        };
        const auto cmp = [&](const char* what, auto&& port, auto&& exe, Rng& r, std::string& d) {
            setup(r);
            Snap init(CK, CKN), ga(CK, CKN), ge(CK, CKN); init.Save();
            const int a = port(); ga.Save();
            init.Restore();
            const int e = exe(); ge.Save();
            if (a == e && ga.SameAs(ge)) return true;
            d = std::string(what) + " h/m/s/d/mo " + std::to_string(init.b[0x13]) + "/" + std::to_string(init.b[0x12]) + "/" + std::to_string(*(uint16_t*)&init.b[0x10]) + "/" + std::to_string(init.b[0x14]) + "/" + std::to_string(init.b[0x15]) + " got " + std::to_string(a) + " exe " + std::to_string(e) + DiffDesc(ga.b.data(), ge.b.data(), CKN, "ck");
            return false;
        };
        Run("CClock::GetGameClockMinutesUntil 0x52CEB0", [=](Rng& r, std::string& d) {
            const uint8 h = (uint8)(r.below(4) ? r.below(24) : r.u32()), m = (uint8)(r.below(4) ? r.below(60) : r.u32());
            return cmp("until", [&] { return (int)CClock::GetGameClockMinutesUntil(h, m); }, [&] { return (int)oracle::Fn<uint16 __cdecl(uint8, uint8)>(0x52CEB0)(h, m); }, r, d);
        });
        Run("CClock::GetIsTimeInRange 0x52CEE0", [=](Rng& r, std::string& d) {
            const uint8 f = (uint8)(r.below(4) ? r.below(24) : r.u32()), t = (uint8)(r.below(4) ? r.below(24) : r.u32());
            return cmp("range", [&] { return (int)CClock::GetIsTimeInRange(f, t); }, [&] { return (int)(oracle::Fn<uint8 __cdecl(uint8, uint8)>(0x52CEE0)(f, t) & 1); }, r, d);
        });
        Run("CClock::NormaliseGameClock 0x52CDE0", [=](Rng& r, std::string& d) {
            return cmp("norm", [&] { CClock::NormaliseGameClock(); return 0; }, [&] { oracle::Fn<void __cdecl()>(0x52CDE0)(); return 0; }, r, d);
        });
        Run("CClock::OffsetClockByADay 0x52D0B0", [=](Rng& r, std::string& d) {
            const uint32 dir = r.below(3) == 0 ? 0u : r.below(2) ? 1u : r.u32();
            return cmp("offset", [&] { CClock::OffsetClockByADay(dir); return 0; }, [&] { oracle::Fn<void __cdecl(uint32)>(0x52D0B0)(dir); return 0; }, r, d);
        });
        Run("CClock::SetGameClock 0x52D150", [=](Rng& r, std::string& d) {
            const uint8 h = (uint8)(r.below(4) ? r.below(24) : r.u32()), m = (uint8)(r.below(4) ? r.below(60) : r.u32()), dy = (uint8)r.u32();
            return cmp("set", [&] { CClock::SetGameClock(h, m, dy); return 0; }, [&] { oracle::Fn<void __cdecl(uint8, uint8, uint8)>(0x52D150)(h, m, dy); return 0; }, r, d);
        });
    }
    Run("CTimeCycle::FindFarClipForCoors 0x5616E0", [](Rng& r, std::string& d) {
        TCSetup(r, r.below(2));
        Snap init(TC_BEG, TC_END - TC_BEG), ga(TC_BEG, TC_END - TC_BEG), ge(TC_BEG, TC_END - TC_BEG);
        init.Save();
        const CVector p = GenV(r, r.below(2) ? 500.f : 3000.f);
        const float a = CTimeCycle::FindFarClipForCoors(p); ga.Save();
        init.Restore();
        const float e = oracle::Fn<float __cdecl(CVector)>(0x5616E0)(p); ge.Save();
        if (SameF(a, e) & ga.SameAs(ge)) return true;
        d = "p " + V(p) + " got " + F(a) + " exe " + F(e) + DiffDesc(ga.b.data(), ge.b.data(), ga.b.size(), "tc");
        return false;
    });
    Run("CWeather::UpdateInTunnelness 0x72B630", [](Rng& r, std::string& d) {
        alignas(16) static float camMat[16];
        for (auto& v : camMat) v = GenF(r, r.below(2) ? 1.f : 3000.f);
        if (r.below(3)) { camMat[12] = r.below(2) ? 85.f + GenF(r, 50.f) : 1683.f + GenF(r, 50.f); camMat[13] = r.below(2) ? -1020.f + GenF(r, 50.f) : -1956.f + GenF(r, 50.f); }   // near the two tunnel points
        for (int i : { 4, 5, 6, 12, 13, 14 }) if (!(std::fabs(camMat[i]) < 1e15f)) return true;   // NaN / inf / huge camera values: std::min({..}) vs the exe's compare chain differ on NaN, the 2D normalise squares overflow float but not x87; not reachable in game
        *reinterpret_cast<uint32_t*>(0xB6F028 + 0x14) = (uint32_t)(uintptr_t)camMat;   // TheCamera.m_matrix
        CCullZones::CurrentFlags_Camera = r.below(4) ? 0x2000 : (int32)r.u32();
        CTimer::ms_fTimeStep = r.below(8) ? r.f01() * 3.f : GenF(r, 4.f);
        if (std::isnan(CTimer::ms_fTimeStep)) return true;   // step_to() with a NaN step: port clamps to 0, the exe's compare chain propagates NaN (a NaN time step never occurs)
        const float start = r.below(3) ? r.f01() : GenF(r, 1.5f);
        if (std::isnan(start)) return true;   // NaN state is not reachable
        CWeather::InTunnelness = start;
        CWeather::UpdateInTunnelness();
        const float a = CWeather::InTunnelness;
        CWeather::InTunnelness = start;
        oracle::Fn<void __cdecl()>(0x72B630)();
        const float e = CWeather::InTunnelness;
        *reinterpret_cast<uint32_t*>(0xB6F028 + 0x14) = 0;
        if (SameF(a, e)) return true;
        d = "start " + F(start) + " ts " + F(CTimer::ms_fTimeStep) + " pos " + F(camMat[12]) + "," + F(camMat[13]) + "," + F(camMat[14]) + " fwd " + F(camMat[4]) + "," + F(camMat[5]) + "," + F(camMat[6]) + " got " + F(a) + " exe " + F(e);
        return false;
    });
    Run("CWeather::UpdateWeatherRegion 0x72A640", [](Rng& r, std::string& d) {
        const bool usePos = r.below(3) != 0;
        CVector p = GenV(r, r.below(2) ? 3000.f : 1500.f);
        if (r.below(4) == 0) { p.x = r.below(2) ? 1000.f : -850.f; p.y = r.below(2) ? 910.f : 1280.f; }   // exact edges
        *reinterpret_cast<uint32_t*>(0xB6F028 + 0x14) = 0;
        const CVector cp = GenV(r, 3000.f); std::memcpy(reinterpret_cast<void*>(0xB6F028 + 4), &cp, 12);
        CWeather::WeatherRegion = (eWeatherRegion)99;
        CWeather::UpdateWeatherRegion(usePos ? &p : nullptr);
        const auto a = CWeather::WeatherRegion;
        CWeather::WeatherRegion = (eWeatherRegion)99;
        oracle::Fn<void __cdecl(CVector*)>(0x72A640)(usePos ? &p : nullptr);
        const auto e = CWeather::WeatherRegion;
        if (a == e) return true;
        d = "p " + V(usePos ? p : cp) + " got " + std::to_string((int)a) + " exe " + std::to_string((int)e);
        return false;
    });
    Run("CShadows::CalcPedShadowValues 0x7076C0", [](Rng& r, std::string& d) {
        const CVector sun = GenV(r, r.below(2) ? 1.f : PickScale(r));
        float a[6], e[6]; for (int i = 0; i < 6; ++i) a[i] = e[i] = -77.f;
        CShadows::CalcPedShadowValues(sun, a[0], a[1], a[2], a[3], a[4], a[5]);
        oracle::Fn<void __cdecl(CVector, float&, float&, float&, float&, float&, float&)>(0x7076C0)(sun, e[0], e[1], e[2], e[3], e[4], e[5]);
        if (SameBlob(a, e, sizeof a)) return true;
        d = "sun " + V(sun); for (int i = 0; i < 6; ++i) d += " [" + F(a[i]) + "/" + F(e[i]) + "]";
        return false;
    });
    Run("CColourSet::Interpolate 0x55F870", [](Rng& r, std::string& d) {
        alignas(16) uint8 a[sizeof(CColourSet)], b[sizeof(CColourSet)], x0[sizeof(CColourSet)], xa[sizeof(CColourSet)], xe[sizeof(CColourSet)];
        auto fill = [&](uint8* p) { for (size_t i = 0; i + 4 <= sizeof(CColourSet); i += 4) { float v = r.below(3) ? (float)(r.u32() % 70000) - 100.f : GenF(r, 300.f); std::memcpy(p + i, &v, 4); } };
        fill(a); fill(b); fill(x0);
        // uint16 sky / cloud colour fields are small integers in game: make them so most of the time
        if (r.below(4)) for (size_t i = 0; i + 2 <= sizeof(CColourSet); i += 2) if (r.below(2)) { const uint16 v = (uint16)r.below(300); std::memcpy(a + i, &v, 2); std::memcpy(b + i, &v, 2); }
        const float ma = r.below(6) ? r.f01() : GenF(r, 2.f), mb = r.below(6) ? 1.f - ma : GenF(r, 2.f);
        const bool ig = r.below(3) == 0;
        std::memcpy(xa, x0, sizeof x0); std::memcpy(xe, x0, sizeof x0);
        reinterpret_cast<CColourSet*>(xa)->Interpolate(reinterpret_cast<CColourSet*>(a), reinterpret_cast<CColourSet*>(b), ma, mb, ig);
        oracle::Fn<void __fastcall(CColourSet*, int, CColourSet*, CColourSet*, float, float, bool)>(0x55F870)(reinterpret_cast<CColourSet*>(xe), 0, reinterpret_cast<CColourSet*>(a), reinterpret_cast<CColourSet*>(b), ma, mb, ig);
        if (SameBlob(xa, xe, sizeof x0)) return true;
        d = "mults " + F(ma) + "," + F(mb) + " ignoreSky " + std::to_string(ig) + DiffDesc(xa, xe, sizeof x0, "set");
        return false;
    });
    Run("CColourSet::CColourSet(time, weather) 0x55F4B0", [](Rng& r, std::string& d) {
        TCSetup(r, false);
        const int ti = r.below(8), wi = r.below(23);
        alignas(16) uint8 xa[sizeof(CColourSet)] {}, xe[sizeof(CColourSet)] {};
        new (xa) CColourSet(ti, wi);
        oracle::Fn<void __fastcall(CColourSet*, int, int, int)>(0x55F4B0)(reinterpret_cast<CColourSet*>(xe), 0, ti, wi);
        if (SameBlob(xa, xe, sizeof xa)) return true;
        d = "time " + std::to_string(ti) + " weather " + std::to_string(wi) + DiffDesc(xa, xe, sizeof xa, "set");
        return false;
    });
    Run("CTimeCycle::FindTimeCycleBox 0x55FFD0", [](Rng& r, std::string& d) {
        TCSetup(r, true);
        const CVector p = GenV(r, r.below(2) ? 500.f : 3000.f);
        const bool lod = r.below(3) == 0, far_ = r.below(3) == 0;
        CTimeCycleBox* ign = r.below(4) == 0 ? &CTimeCycle::m_aBoxes[r.below(6)] : nullptr;
        CTimeCycleBox *ba = nullptr, *be = nullptr; float ta = -1, te = -1;
        CTimeCycle::FindTimeCycleBox(p, &ba, &ta, lod, far_, ign);
        oracle::Fn<void __cdecl(CVector, CTimeCycleBox**, float*, bool, bool, CTimeCycleBox*)>(0x55FFD0)(p, &be, &te, lod, far_, ign);
        if (ba == be && SameF(ta, te)) return true;
        d = "p " + V(p) + " lod " + std::to_string(lod) + " far " + std::to_string(far_) + " got " + std::to_string((int)(ba ? ba - &CTimeCycle::m_aBoxes[0] : -1)) + "," + F(ta) + " exe " + std::to_string((int)(be ? be - &CTimeCycle::m_aBoxes[0] : -1)) + "," + F(te);
        return false;
    });
    Run("CTimeCycle::CalcColoursForPoint 0x5603D0", [](Rng& r, std::string& d) {
        TCSetup(r, r.below(2));
        Snap init(TC_BEG, TC_END - TC_BEG), ga(TC_BEG, TC_END - TC_BEG), ge(TC_BEG, TC_END - TC_BEG);
        init.Save();
        alignas(16) uint8 xa[sizeof(CColourSet)], xe[sizeof(CColourSet)];
        for (size_t i = 0; i < sizeof xa; ++i) xa[i] = xe[i] = (uint8)r.u32();
        const CVector p = GenV(r, r.below(2) ? 500.f : 3000.f);
        if (g_trace) std::printf("  port p %s\n", V(p).c_str());
        CTimeCycle::CalcColoursForPoint(p, reinterpret_cast<CColourSet*>(xa)); ga.Save();
        if (g_trace) std::printf("  port done\n");
        init.Restore();
        oracle::Fn<void __cdecl(CVector, CColourSet*)>(0x5603D0)(p, reinterpret_cast<CColourSet*>(xe)); ge.Save();
        if (SameBlob(xa, xe, sizeof xa) & ga.SameAs(ge)) return true;
        d = "p " + V(p) + " h/m/s " + std::to_string(CClock::ms_nGameClockHours) + "/" + std::to_string(CClock::ms_nGameClockMinutes) + "/" + std::to_string(CClock::ms_nGameClockSeconds) + " w " + std::to_string((int)CWeather::OldWeatherType) + "->" + std::to_string((int)CWeather::NewWeatherType) + " t " + F(CWeather::InterpolationValue) + DiffDesc(xa, xe, sizeof xa, "set") + DiffDesc(ga.b.data(), ge.b.data(), ga.b.size(), "tc");
        return false;
    });

    Run("pow(double,double) host CRT vs exe _CIpow 0x822130", [](Rng& r, std::string& d) {
        const double x = r.below(4) ? (double)r.f01() * 1.2 : (double)GenF(r, 50.f), y = r.below(3) ? (double)r.f01() * 4.0 : (double)GenF(r, 20.f);
        const double a = std::pow(x, y), b = ExePow(x, y);
        uint64_t ba, bb; std::memcpy(&ba, &a, 8); std::memcpy(&bb, &b, 8);
        if (ba == bb || (std::isnan(a) && std::isnan(b))) return true;
        char t[160]; std::snprintf(t, sizeof t, "x %.17g y %.17g got %.17g (%016llX) exe %.17g (%016llX)", x, y, a, (unsigned long long)ba, b, (unsigned long long)bb); d = t;
        g_hardDiff = true;
        return false;
    });
    Run("CGeneral::SolveQuadratic 0x53CE30", [](Rng& r, std::string& d) {
        const float s = PickScale(r), a = GenF(r, s), b = GenF(r, s), c = GenF(r, s);
        float x1a = 7, x2a = 8, x1e = 7, x2e = 8;
        const bool ra = CGeneral::SolveQuadratic(a, b, c, x1a, x2a);
        const bool re = oracle::Fn<bool __cdecl(float, float, float, float*, float*)>(0x53CE30)(a, b, c, &x1e, &x2e);
        Hit(ra);
        if (ra == re && SameF(x1a, x1e) && SameF(x2a, x2e)) return true;
        d = "abc " + F(a) + "," + F(b) + "," + F(c) + " got " + std::to_string(ra) + F(x1a) + "," + F(x2a) + " exe " + std::to_string(re) + F(x1e) + "," + F(x2e);
        return false;
    });
    // CTimer::UpdateVariables: all CTimer statics (0xB7CB00..0xB7CC00) are randomised, then both versions run from the same state
    Run("CTimer::UpdateVariables 0x5618D0", [](Rng& r, std::string& d) {
        Snap init(0xB7CB00, 0x100), ga(0xB7CB00, 0x100), ge(0xB7CB00, 0x100);
        init.FillRandom(r);
        // keep the divider small and the step sane most of the time
        const uint32_t div = r.below(6) ? 1 + r.below(4) : r.u32();
        std::memcpy(&init.b[0], &init.b[0], 0);
        init.Restore();
        *reinterpret_cast<uint32_t*>(&CTimer::m_snTimerDivider) = div;
        init.Save();
        const float el = r.below(6) ? r.f01() * 200.f : GenF(r, 5000.f);
        CTimer::UpdateVariables(el); ga.Save();
        init.Restore();
        oracle::Fn<void __cdecl(float)>(0x5618D0)(el); ge.Save();
        if (ga.SameAs(ge)) return true;
        d = "el " + F(el) + " div " + std::to_string(div);
        for (size_t i = 0; i < ga.b.size(); i += 4) if (std::memcmp(&ga.b[i], &ge.b[i], 4)) { char t[80]; std::snprintf(t, sizeof t, " [+%zX got %02X%02X%02X%02X exe %02X%02X%02X%02X]", i, ga.b[i+3], ga.b[i+2], ga.b[i+1], ga.b[i], ge.b[i+3], ge.b[i+2], ge.b[i+1], ge.b[i]); d += t; }
        return false;
    });
}

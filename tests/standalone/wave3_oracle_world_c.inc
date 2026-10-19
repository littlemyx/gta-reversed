// ORACLE-W3-WORLD rows C: timer / time cycle / draw / grid ref / common / zones / cull zones (see wave3_oracle_world.inc). ROW / IC / Tw are defined there.
#include "Draw.h"
#include "GridRef.h"
#include "common.h"
#include "MenuManager.h"

// MenuManager.cpp is not linked (it defines the global): the reference the port's TUs use points at the exe's object
CMenuManager& FrontEndMenuManager = *reinterpret_cast<CMenuManager*>(0xBA6748);

static void TestW3WorldC() {
    using namespace w2; using namespace w3w;
    // ---- CTimer (all globals at 0xB7CB28..0xB7CB8C)
    const auto TimerFx = [](Rng& rr) {
        FillGarbage(rr, 0xB7CB28, 0x68);
        for (uintptr_t a : { 0xB7CB40, 0xB7CB48, 0xB7CB49, 0xB7CB88, 0xB7CB89 }) At<uint8_t>(a) = (uint8_t)rr.below(2);
        CTimer::m_snTimerDivider = 1 + rr.below(rr.below(2) ? 20000 : 0xFFFFFF);   // cycles per ms: far below 2^24 (above it the exe's x87 product is not rounded like a float: not a game state)
    };
    ROW("CTimer::Shutdown", 0x5618C0, 40) { Tw tw(TimerFx); IC(CTimer::Shutdown(); return 0;, oracle::Fn<void __cdecl()>(0x5618C0)(); return 0;); } ENDROW;
    ROW("CTimer::Stop", 0x561AA0, 60) { Tw tw(TimerFx); IC(CTimer::Stop(); return 0;, oracle::Fn<void __cdecl()>(0x561AA0)(); return 0;); } ENDROW;
    ROW("CTimer::StartUserPause", 0x561AF0, 40) { Tw tw(TimerFx); IC(CTimer::StartUserPause(); return 0;, oracle::Fn<void __cdecl()>(0x561AF0)(); return 0;); } ENDROW;
    ROW("CTimer::EndUserPause", 0x561B00, 40) { Tw tw(TimerFx); IC(CTimer::EndUserPause(); return 0;, oracle::Fn<void __cdecl()>(0x561B00)(); return 0;); } ENDROW;
    ROW("CTimer::GetCyclesPerMillisecond", 0x561A40, 200) { Tw tw(TimerFx); IC(return (uint64_t)CTimer::GetCyclesPerMillisecond();, return (uint64_t)oracle::Fn<uint32_t __cdecl()>(0x561A40)();); } ENDROW;
    ROW("CTimer::GetCyclesPerFrame", 0x561A50, 400) { Tw tw(TimerFx); IC(return (uint64_t)CTimer::GetCyclesPerFrame();, return (uint64_t)oracle::Fn<uint32_t __cdecl()>(0x561A50)();); } ENDROW;
    ROW("CTimer::GetIsSlowMotionActive", 0x561AD0, 200) {
        Tw tw([&](Rng& rr) { TimerFx(rr); if (rr.below(3) == 0) CTimer::ms_fSlowMotionScale = 1.f; else if (rr.below(3) == 0) CTimer::ms_fSlowMotionScale = Fl(rr, 3.f, 4); });
        IC(return B8(CTimer::GetIsSlowMotionActive());, return (uint64_t)oracle::Fn<uint8_t __cdecl()>(0x561AD0)(););
    } ENDROW;

    // ---- CTimeCycle ambient getters (a random current colour set and light multiplier)
    const auto TcFx = [](Rng& rr) { FillGarbage(rr, 0xB7C4A0, sizeof(CColourSet)); gfLaRiotsLightMult = rr.below(4) ? 1.f : GF(rr, 3.f); };
    struct Tc { const char* name; uintptr_t addr; float (*port)(); };
    static const Tc tcs[] = {
        { "CTimeCycle::GetAmbientRed", 0x560330, &CTimeCycle::GetAmbientRed }, { "CTimeCycle::GetAmbientGreen", 0x560340, &CTimeCycle::GetAmbientGreen }, { "CTimeCycle::GetAmbientBlue", 0x560350, &CTimeCycle::GetAmbientBlue },
        { "CTimeCycle::GetAmbientRed_Obj", 0x560360, &CTimeCycle::GetAmbientRed_Obj }, { "CTimeCycle::GetAmbientGreen_Obj", 0x560370, &CTimeCycle::GetAmbientGreen_Obj }, { "CTimeCycle::GetAmbientBlue_Obj", 0x560380, &CTimeCycle::GetAmbientBlue_Obj },
        { "CTimeCycle::GetAmbientRed_BeforeBrightness", 0x560390, &CTimeCycle::GetAmbientRed_BeforeBrightness }, { "CTimeCycle::GetAmbientGreen_BeforeBrightness", 0x5603A0, &CTimeCycle::GetAmbientGreen_BeforeBrightness },
        { "CTimeCycle::GetAmbientBlue_BeforeBrightness", 0x5603B0, &CTimeCycle::GetAmbientBlue_BeforeBrightness },
    };
    for (const auto& t : tcs) {
        const std::string nm = std::string(t.name) + " 0x" + [&] { char b[16]; std::snprintf(b, sizeof b, "%X", (unsigned)t.addr); return std::string(b); }();
        RunScaledW(nm, [&](Rng& r, std::string& d) -> bool {
            Tw tw(TcFx);
            IC(return FB(t.port());, return FB(oracle::Fn<float __cdecl()>(t.addr)()););
        }, 150);
    }
    ROW("CTimeCycle::InitForRestart", 0x5601F0, 50) { Tw tw([](Rng& rr) { FillGarbage(rr, 0xB79E40, 4); FillGarbage(rr, 0xB7C484, 4); }); IC(CTimeCycle::InitForRestart(); return 0;, oracle::Fn<void __cdecl()>(0x5601F0)(); return 0;); } ENDROW;

    // ---- CDraw
    ROW("CDraw::SetFOV", 0x6FF410, 100) { const float f = Fl(r, 120.f, 6); IC(CDraw::SetFOV(f); return 0;, oracle::Fn<void __cdecl(float)>(0x6FF410)(f); return 0;); } ENDROW;
    ROW("CDraw::CalculateAspectRatio", 0x6FF420, 200) {
        Tw tw([](Rng& rr) { FrontEndMenuManager.m_bWidescreenOn = rr.below(2); TheCamera.m_bWideScreenOn = rr.below(2); });
        IC(CDraw::CalculateAspectRatio(); return 0;, oracle::Fn<void __cdecl()>(0x6FF420)(); return 0;);
    } ENDROW;

    // ---- CGridRef
    ROW("CGridRef::GetArtistBugstarID", 0x71D650, 200) {
        const uint8_t x = r.below(4) ? r.below(14) : r.u32(), y = r.below(4) ? r.below(14) : r.u32();
        const auto Id = [](const char* p) -> uint64_t { return p ? 1 + (uint64_t)(uint8_t)*p : 0; };   // the exe returns a pointer into its data (an empty string)
        IC(return Id(CGridRef::GetArtistBugstarID(x, y));, return Id(oracle::Fn<const char* __cdecl(uint8_t, uint8_t)>(0x71D650)(x, y)););
    } ENDROW;
    ROW("CGridRef::GetGridRefPositions", 0x71D5E0, 600) {
        Tw tw([](Rng& rr) { for (int k = 0; k < 2; ++k) if (!CWorld::Players[k].m_pPed) CWorld::Players[k].m_pPed = reinterpret_cast<CPlayerPed*>(PedP(k)); });
        IC(CGridRef::GetGridRefPositions(S(oOut), S(oOut + 1)); return 0;, oracle::Fn<void __cdecl(uint8_t*, uint8_t*)>(0x71D5E0)(S(oOut), S(oOut + 1)); return 0;);
    } ENDROW;

    // ---- common.cpp
    ROW("MakeUpperCase", 0x7186E0, 300) {
        const int n = r.below(24);
        Tw tw([n](Rng& rr) { for (int i = 0; i < n; ++i) S(oBuf)[i] = (char)(rr.below(4) ? 0x20 + rr.below(0x5F) : rr.below(256)); S(oBuf)[n] = 0; for (int i = 0; i < n; ++i) if (!S(oBuf)[i]) S(oBuf)[i] = 'a'; });
        IC(return U(MakeUpperCase((char*)S(oBuf + 0x100), (const char*)S(oBuf))) - (uint64_t)(uintptr_t)S(oBuf + 0x100);, return (uint64_t)oracle::Fn<uint32_t __cdecl(char*, const char*)>(0x7186E0)((char*)S(oBuf + 0x100), (const char*)S(oBuf)) - (uint64_t)(uintptr_t)S(oBuf + 0x100););
    } ENDROW;
    ROW("LittleTest", 0x541330, 30) { IC(LittleTest(); return 0;, oracle::Fn<void __cdecl()>(0x541330)(); return 0;); } ENDROW;
    const auto ScreenFx = [](Rng& rr) {
        RsGlobal.maximumWidth = rr.below(8) ? 640 + (int)rr.below(1500) : (int)rr.below(4000) - 100; RsGlobal.maximumHeight = rr.below(8) ? 448 + (int)rr.below(1000) : (int)rr.below(3000) - 100;
        CDraw::ms_fFOV = rr.below(5) ? 20.f + rr.f01() * 80.f : Fl(rr, 100.f, 4);
        float* m = reinterpret_cast<float*>(&TheCamera.m_mViewMatrix);   // CMatrix: 3 axes + pos (+ pad) = 16 floats at the start
        for (int i = 0; i < 16; ++i) m[i] = GF(rr, i % 4 == 3 ? 1.f : 20.f);
    };
    ROW("CalcScreenCoors(VV)", 0x71DAB0, 1200) {
        Tw tw(ScreenFx);
        const CVector in(Fl(r, 100.f, 12), Fl(r, 100.f, 12), Fl(r, 100.f, 12));
        IC(return B8(CalcScreenCoors(in, *reinterpret_cast<CVector*>(S(oOut)))) ^ (FB(reinterpret_cast<CVector*>(S(oOut))->z) << 1);, return (uint64_t)oracle::Fn<uint8_t __cdecl(const CVector&, CVector&)>(0x71DAB0)(in, *reinterpret_cast<CVector*>(S(oOut))) ^ (FB(reinterpret_cast<CVector*>(S(oOut))->z) << 1););
    } ENDROW;
    ROW("CalcScreenCoors(VVff)", 0x71DA00, 1200) {
        Tw tw(ScreenFx);
        const CVector in(Fl(r, 100.f, 12), Fl(r, 100.f, 12), Fl(r, 100.f, 12));
        IC(return B8(CalcScreenCoors(in, *reinterpret_cast<CVector*>(S(oOut)), *reinterpret_cast<float*>(S(oOut + 16)), *reinterpret_cast<float*>(S(oOut + 20))));,
           return (uint64_t)oracle::Fn<uint8_t __cdecl(const CVector&, CVector&, float&, float&)>(0x71DA00)(in, *reinterpret_cast<CVector*>(S(oOut)), *reinterpret_cast<float*>(S(oOut + 16)), *reinterpret_cast<float*>(S(oOut + 20))););
    } ENDROW;
    ROW("DoesInfiniteLineTouchScreen", 0x71DB80, 2500) {
        Tw tw(ScreenFx);
        const float w = (float)RsGlobal.maximumWidth, h = (float)RsGlobal.maximumHeight; (void)w; (void)h;
        const float sx = r.below(3) ? 1500.f : 6000.f;
        const auto Sp = [&](float sc) { return r.below(14) == 0 ? GenF(r, sc, S_NAN | S_ZERO | S_INF) : (r.f01() * 2.f - 1.f) * sc; };   // (no denormal / tiny specials: the exe spills x87 temporaries as floats there, 1e-20 products are not screen coordinates)
        const CVector2D o(Sp(sx), Sp(sx)), dir(Sp(5.f), Sp(5.f));
        const bool ok = w3w::WCase(r, d, [&]() -> uint64_t { return B8(DoesInfiniteLineTouchScreen(o, dir)); }, [&]() -> uint64_t { return (uint64_t)oracle::Fn<uint8_t __cdecl(float, float, float, float)>(0x71DB80)(o.x, o.y, dir.x, dir.y); });
        if (!ok) d += " o=" + F(o.x) + "," + F(o.y) + " dir=" + F(dir.x) + "," + F(dir.y) + " W=" + std::to_string(RsGlobal.maximumWidth) + " H=" + std::to_string(RsGlobal.maximumHeight);
        return ok;
    } ENDROW;
}

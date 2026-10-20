// ORACLE-W3-UI rows C: Radar.cpp blip table (ms_RadarTrace), legend list, map centre (see wave3_oracle_ui.inc).
#include "Scripts/TheScripts.h"
namespace w3uc {
using namespace w3u; using namespace w2;
struct EntAcc : CEntity { static size_t Off() { return offsetof(EntAcc, m_AreaCode); } };
static float r_f(Rng& r) { return 0.5f + r.f01() * 4.f; }
static uint8_t* TB(int i) { return reinterpret_cast<uint8_t*>(&CRadar::ms_RadarTrace[i]); }
static void PutRadar(Rng& r) {
    FillGarbage(r, &CRadar::ms_RadarTrace, sizeof(CRadar::ms_RadarTrace));
    for (int i = 0; i < (int)CRadar::MAX_RADAR_TRACES; ++i) {
        auto& t = CRadar::ms_RadarTrace[i];
        t.m_nCounter = r.below(10) == 0 ? (uint16_t)(0xFFFC + r.below(4)) : (uint16_t)r.below(40);
        uint8_t* b = TB(i);
        if (r.below(4) != 0) b[0x25] |= 2; else b[0x25] &= ~2;     // tracking bit
        t.m_nColour = (eBlipColour)r.below(30);
        t.m_nEntityHandle = r.below(12);
        t.m_vPosition = CVector(GF(r, 3000.f), GF(r, 3000.f), GF(r, 100.f));
        t.m_nBlipSprite = (eRadarSprite)(r.below(6) == 0 ? (int)r.below(80) - 8 : (int)r.below(64));
        t.m_pEntryExit = nullptr;
    }
    for (auto& s : CRadar::MapLegendList) s = (int16_t)(r.below(3) ? 0 : (int)r.below(70) - 8);
    CRadar::MapLegendCounter = (uint16_t)r.below(40);
    CRadar::m_radarRange = Fl(r, 5000.f, 14);
}
// a blip handle: valid (index + current counter), stale (wrong counter), -1 or a garbage counter
static int32_t Handle(Rng& r) {
    const int k = r.below(10);
    if (k == 0) return -1;
    const int i = (int)r.below(CRadar::MAX_RADAR_TRACES);
    const uint16_t c = CRadar::ms_RadarTrace[i].m_nCounter;
    if (k <= 2) return (int32_t)(i | ((uint16_t)(c + 1 + r.below(3)) << 16));
    return (int32_t)(i | ((uint32_t)c << 16));
}
static void PutFrontEnd(Rng& r) {
    FrontEndMenuManager.m_ShowLocationsBlips = r.below(2); FrontEndMenuManager.m_ShowContactsBlips = r.below(2); FrontEndMenuManager.m_ShowOtherBlips = r.below(2);
    FrontEndMenuManager.m_bDrawingMap = r.below(3) != 0;
    CGame::currArea = (eAreaCodes)(r.below(3) == 0 ? r.below(4) : 0);
    CWorld::PlayerInFocus = (uint8)r.below(2);
    for (auto& p : CWorld::Players) p.m_pPed = nullptr;
    CWorld::Players[0].m_pPed = reinterpret_cast<CPlayerPed*>(S(oBuf)); CWorld::Players[1].m_pPed = reinterpret_cast<CPlayerPed*>(S(oBuf));
    if (r.below(5) == 0) CWorld::Players[CWorld::PlayerInFocus].m_pPed = nullptr;
    CEntryExitManager::ms_entryExitStackPosn = 0;
    std::memset(S(oBuf), 0, 0x800);
    SAt<uint8_t>(oBuf + EntAcc::Off()) = (uint8_t)(r.below(3) == 0 ? r.below(4) : 0);
}
} // namespace w3uc

static void TestW3UiC() {
    using namespace w2; using namespace w3u; using namespace w3uc;
    ROW("CRadar::ChangeBlipBrightness", 0x583C70, 700) {
        Tu tu([](Rng& rr) { PutRadar(rr); }); const int32_t h = Handle(r); const int br = r.below(3);
        return UCase(r, d, [&]() -> uint64_t { CRadar::ChangeBlipBrightness(h, br); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl(int, int)>(0x583C70)(h, br); return 0; });
    } ENDROW;
    ROW("CRadar::ChangeBlipColour", 0x583AB0, 700) {
        Tu tu([](Rng& rr) { PutRadar(rr); }); const int32_t h = Handle(r); const int c = r.below(40);
        return UCase(r, d, [&]() -> uint64_t { CRadar::ChangeBlipColour(h, (eBlipColour)c); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl(int, int)>(0x583AB0)(h, c); return 0; });
    } ENDROW;
    ROW("CRadar::ChangeBlipDisplay", 0x583D20, 700) {
        Tu tu([](Rng& rr) { PutRadar(rr); }); const int32_t h = Handle(r); const int c = r.below(4);
        return UCase(r, d, [&]() -> uint64_t { CRadar::ChangeBlipDisplay(h, (eBlipDisplay)c); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl(int, int)>(0x583D20)(h, c); return 0; });
    } ENDROW;
    ROW("CRadar::SetBlipFade", 0x583E00, 700) {
        Tu tu([](Rng& rr) { PutRadar(rr); }); const int32_t h = Handle(r); const bool f = r.below(2);
        return UCase(r, d, [&]() -> uint64_t { CRadar::SetBlipFade(h, f); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl(int, int)>(0x583E00)(h, f ? 1 : 0); return 0; });
    } ENDROW;
    ROW("CRadar::SetBlipFriendly", 0x583EB0, 700) {
        Tu tu([](Rng& rr) { PutRadar(rr); }); const int32_t h = Handle(r); const bool f = r.below(2);
        return UCase(r, d, [&]() -> uint64_t { CRadar::SetBlipFriendly(h, f); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl(int, int)>(0x583EB0)(h, f ? 1 : 0); return 0; });
    } ENDROW;
    ROW("CRadar::SetBlipSprite", 0x583D70, 700) {
        Tu tu([](Rng& rr) { PutRadar(rr); }); const int32_t h = Handle(r); const int s = (int)r.below(70) - 3;
        return UCase(r, d, [&]() -> uint64_t { CRadar::SetBlipSprite(h, (eRadarSprite)s); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl(int, int)>(0x583D70)(h, s); return 0; });
    } ENDROW;
    ROW("CRadar::ClearBlip", 0x587CE0, 700) {
        Tu tu([](Rng& rr) { PutRadar(rr); }); const int32_t h = Handle(r);
        return UCase(r, d, [&]() -> uint64_t { CRadar::ClearBlip(h); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl(int)>(0x587CE0)(h); return 0; });
    } ENDROW;
    ROW("CRadar::ClearActualBlip", 0x587C10, 700) {
        Tu tu([](Rng& rr) { PutRadar(rr); }); const int idx = r.below(8) == 0 ? (int)r.below(190) - 10 : (int)r.below(CRadar::MAX_RADAR_TRACES);
        return UCase(r, d, [&]() -> uint64_t { CRadar::ClearActualBlip(idx); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl(int)>(0x587C10)(idx); return 0; });
    } ENDROW;
    ROW("CRadar::ClearBlipForEntity", 0x587C60, 700) {
        const int pick = (int)r.below(CRadar::MAX_RADAR_TRACES); const bool hit = r.below(3) != 0; const int rt = r.below(16), rh = r.below(14);
        Tu tu([](Rng& rr) { PutRadar(rr); });
        return UCase(r, d, [&]() -> uint64_t {
            const int type = hit ? (TB(pick)[0x26] >> 2) & 0xF : rt; const int h = hit ? CRadar::ms_RadarTrace[pick].m_nEntityHandle : rh;
            CRadar::ClearBlipForEntity((eBlipType)type, h); return 0; },
            [&]() -> uint64_t {
            const int type = hit ? (TB(pick)[0x26] >> 2) & 0xF : rt; const int h = hit ? CRadar::ms_RadarTrace[pick].m_nEntityHandle : rh;
            oracle::Fn<void __cdecl(int, int)>(0x587C60)(type, h); return 0; });
    } ENDROW;
    ROW("CRadar::GetNewUniqueBlipIndex", 0x582820, 700) {
        Tu tu([](Rng& rr) { PutRadar(rr); }); const int idx = (int)r.below(CRadar::MAX_RADAR_TRACES);
        return UCase(r, d, [&]() -> uint64_t { return (uint32_t)CRadar::GetNewUniqueBlipIndex(idx); }, [&]() -> uint64_t { return oracle::Fn<uint32_t __cdecl(int)>(0x582820)(idx); });
    } ENDROW;
    ROW("CRadar::SetCoordBlip", 0x583820, 1000) {
        Tu tu([](Rng& rr) { PutRadar(rr); }); const int type = r.below(16); const CVector p(Fl(r, 3000.f, 8), Fl(r, 3000.f, 8), Fl(r, 100.f, 8)); const int col = r.below(30), disp = r.below(4);
        return UCase(r, d, [&]() -> uint64_t { return (uint32_t)CRadar::SetCoordBlip((eBlipType)type, p, (eBlipColour)col, (eBlipDisplay)disp); },
            [&]() -> uint64_t { return oracle::Fn<uint32_t __cdecl(int, float, float, float, int, int)>(0x583820)(type, p.x, p.y, p.z, col, disp); });
    } ENDROW;
    ROW("CRadar::SetShortRangeCoordBlip", 0x583920, 1000) {
        Tu tu([](Rng& rr) { PutRadar(rr); }); const int type = r.below(16); const CVector p(Fl(r, 3000.f, 8), Fl(r, 3000.f, 8), Fl(r, 100.f, 8)); const int col = r.below(30), disp = r.below(4);
        return UCase(r, d, [&]() -> uint64_t { return (uint32_t)CRadar::SetShortRangeCoordBlip((eBlipType)type, p, (eBlipColour)col, (eBlipDisplay)disp); },
            [&]() -> uint64_t { return oracle::Fn<uint32_t __cdecl(int, float, float, float, int, int)>(0x583920)(type, p.x, p.y, p.z, col, disp); });
    } ENDROW;
    ROW("CRadar::DisplayThisBlip", 0x583B40, 3000) {
        Tu tu([](Rng& rr) { PutRadar(rr); PutFrontEnd(rr); }); const int sprite = (int)r.below(75) - 5; const int8_t prio = (int8_t)((int)r.below(8) - 4);
        return UCase(r, d, [&]() -> uint64_t { return B8(CRadar::DisplayThisBlip((eRadarSprite)sprite, prio)); },
            [&]() -> uint64_t { return oracle::Fn<uint32_t __cdecl(int, int)>(0x583B40)(sprite, prio) & 0xFF; });
    } ENDROW;
    ROW("CRadar::InitFrontEndMap", 0x585960, 100) {
        Tu tu([](Rng& rr) { PutRadar(rr); FillGarbage(rr, &CRadar::ArrowBlipColour, sizeof(CRadar::ArrowBlipColour)); CRadar::cachedSin = Fl(rr, 2.f); CRadar::cachedCos = Fl(rr, 2.f); CRadar::m_fRadarOrientation = Fl(rr, 7.f); });
        return UCase(r, d, [&]() -> uint64_t { CRadar::InitFrontEndMap(); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl()>(0x585960)(); return 0; });
    } ENDROW;
    ROW("CRadar::AddBlipToLegendList", 0x5859F0, 2000) {
        const bool noSprite = r.below(2); const int idx = noSprite ? (int)r.below(CRadar::MAX_RADAR_TRACES) : (int)r.below(64);
        Tu tu([](Rng& rr) { PutRadar(rr); PutFrontEnd(rr); FillGarbage(rr, &CRadar::ArrowBlipColour, sizeof(CRadar::ArrowBlipColour)); for (auto& s : CRadar::MapLegendList) s = rr.below(3) ? 0 : (int16_t)((int)rr.below(70) - 8); CRadar::MapLegendCounter = (uint16_t)rr.below(CRadar::MAX_RADAR_TRACES - 2); });
        return UCase(r, d, [&]() -> uint64_t { CRadar::AddBlipToLegendList(noSprite, idx); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl(int, int)>(0x5859F0)(noSprite ? 1 : 0, idx); return 0; });
    } ENDROW;
    ROW("CRadar::SetMapCentreToPlayerCoords", 0x585B20, 500) {
        Tu tu([](Rng& rr) { PutRadar(rr); PutFrontEnd(rr); CRadar::cachedSin = Fl(rr, 1.f); CRadar::cachedCos = Fl(rr, 1.f); CRadar::m_fRadarOrientation = Fl(rr, 7.f); FrontEndMenuManager.m_fMapZoom = r_f(rr); CTheScripts::HideAllFrontEndMapBlips = rr.below(4) == 0; CTheScripts::bPlayerIsOffTheMap = rr.below(4) == 0; });
        return UCase(r, d, [&]() -> uint64_t { CRadar::SetMapCentreToPlayerCoords(); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl()>(0x585B20)(); return 0; });
    } ENDROW;
}

// ORACLE-W3-UI rows B: Hud.cpp state setters and DrawFadeState (see wave3_oracle_ui.inc).
#include "Garages.h"
#include "Replay.h"
#include "CutsceneMgr.h"
#include "Game.h"
namespace w3ub {
using namespace w3u; using namespace w2;
constexpr uintptr_t kHudLo = 0xBAA380, kHudHi = 0xBAB230;
static void PutHud(Rng& r) {
    FillGarbage(r, (void*)kHudLo, kHudHi - kHudLo);
    CReplay::Mode = (eReplayMode)(r.below(6) == 0 ? 1 : r.below(5) == 0 ? 2 : 0);
    CCutsceneMgr::ms_cutsceneProcessing = r.below(8) == 0;
    CCutsceneMgr::ms_running = (int8)(r.below(8) == 0);
    CGarages::MessageIDString[0] = r.below(8) == 0 ? 'X' : 0;
    CHud::m_BigMessage[STYLE_MIDDLE_SMALLER_HIGHER][0] = r.below(8) == 0 ? 'Y' : 0;
    CGame::currArea = (eAreaCodes)(r.below(3) == 0 ? r.below(4) : 0);
    const auto st = [&]() -> uint32 { return r.below(10) == 0 ? r.u32() : r.below(7); };
    const auto tm = [&]() -> uint32 { return r.below(8) == 0 ? r.u32() : r.below(16000); };
    const auto ft = [&]() -> uint32 { return r.below(8) == 0 ? r.u32() : r.below(1500); };
    CHud::m_WantedState = st(); CHud::m_WantedTimer = tm(); CHud::m_WantedFadeTimer = ft();
    CHud::m_EnergyLostState = st(); CHud::m_EnergyLostTimer = tm(); CHud::m_EnergyLostFadeTimer = ft();
    CHud::m_DisplayScoreState = st(); CHud::m_DisplayScoreTimer = tm(); CHud::m_DisplayScoreFadeTimer = ft();
    CHud::m_WeaponState = st(); CHud::m_WeaponTimer = tm(); CHud::m_WeaponFadeTimer = ft();
    CHud::m_ZoneState = (eNameState)(r.below(6)); CHud::m_nHelpMessageState = (int32)r.below(4);
    CTimer::ms_fTimeStep = r.below(6) == 0 ? Fl(r, 100.f, 1) : (r.f01() * 3.f);
    for (auto* s : { CHud::m_pHelpMessageToPrint, CHud::m_pLastHelpMessage, CHud::m_pHelpMessage, CHud::m_Message }) {
        const int len = (int)r.below(8); for (int i = 0; i < 400; ++i) s[i] = i < len ? (GxtChar)('a' + r.below(26)) : 0;
        if (r.below(4) == 0) s[0] = 0;
    }
    std::memset(gString, 0, sizeof(gString));
}
static void SetPtrs(Rng& r) { CHud::m_pZoneName = r.below(3) ? PickStr(r) : nullptr; CHud::m_pVehicleName = r.below(3) ? PickStr(r) : nullptr; }
} // namespace w3ub

static void TestW3UiB() {
    using namespace w2; using namespace w3u; using namespace w3ub;
    ROW("CHud::SetHelpMessageStatUpdate", 0x588D40, 1500) {
        const int state = (int)r.below(3); const uint16_t statId = (uint16_t)(r.below(4) ? r.below(100) : r.u32()); const float diff = Fl(r, 100.f, 4); const float mx = r.below(5) == 0 ? Fl(r, 80000.f, 3) : (float)r.below(1200);
        Tu tu([](Rng& rr) { PutHud(rr); });
        return UCase(r, d, [&]() -> uint64_t { CHud::SetHelpMessageStatUpdate((eStatUpdateState)state, statId, diff, mx); return 0; },
            [&]() -> uint64_t { oracle::Fn<void __cdecl(int, int, float, float)>(0x588D40)(state, statId, diff, mx); return 0; });
    } ENDROW;
    ROW("CHud::SetHelpMessageWithNumber", 0x588E30, 1500) {
        const GxtChar* text = PickStr(r); const int number = r.below(4) ? (int)r.below(100000) : (int)r.u32(); const bool quick = r.below(2), perm = r.below(2);
        Tu tu([](Rng& rr) { PutHud(rr); });
        return UCase(r, d, [&]() -> uint64_t { CHud::SetHelpMessageWithNumber(text, number, quick, perm); return 0; },
            [&]() -> uint64_t { oracle::Fn<void __cdecl(const GxtChar*, int, int, int)>(0x588E30)(text, number, quick ? 1 : 0, perm ? 1 : 0); return 0; });
    } ENDROW;
    ROW("CHud::SetVehicleName", 0x588F50, 100) {
        const GxtChar* name = PickStr(r);
        Tu tu([](Rng& rr) { PutHud(rr); SetPtrs(rr); });
        return UCase(r, d, [&]() -> uint64_t { CHud::SetVehicleName(name); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl(const GxtChar*)>(0x588F50)(name); return 0; });
    } ENDROW;
    ROW("CHud::SetZoneName", 0x588BB0, 600) {
        const GxtChar* name = r.below(8) == 0 ? nullptr : PickStr(r); const bool now = r.below(2);
        Tu tu([](Rng& rr) { PutHud(rr); SetPtrs(rr); });
        return UCase(r, d, [&]() -> uint64_t { CHud::SetZoneName(name, now); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl(const GxtChar*, int)>(0x588BB0)(name, now ? 1 : 0); return 0; });
    } ENDROW;
    ROW("CHud::DrawFadeState", 0x58D580, 3000) {
        const int which = (int)(r.below(10) == 0 ? r.below(8) : r.below(4)); const int force = r.below(3) == 0 ? (r.below(2) ? 1 : (int)r.u32()) : 0;
        Tu tu([](Rng& rr) { PutHud(rr); });
        return UCase(r, d, [&]() -> uint64_t { return FB(CHud::DrawFadeState((DRAW_FADE_STATE)which, force)); },
            [&]() -> uint64_t { return FB(oracle::Fn<float __cdecl(int, int)>(0x58D580)(which, force)); });
    } ENDROW;
    ROW("CHud::GetRidOfAllHudMessages", 0x588A50, 200) {
        const bool flag = r.below(2);
        Tu tu([](Rng& rr) { PutHud(rr); SetPtrs(rr); });
        return UCase(r, d, [&]() -> uint64_t { CHud::GetRidOfAllHudMessages(flag); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl(int)>(0x588A50)(flag ? 1 : 0); return 0; });
    } ENDROW;
}

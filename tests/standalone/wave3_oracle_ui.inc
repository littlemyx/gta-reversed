// ORACLE-W3-UI: exe-oracle rows for the HUD / radar / text root files (Messages.cpp, Hud.cpp, Radar.cpp, Font.cpp, Sprite2d.cpp, Sprite.cpp, MenuManager.cpp, Text/*), classes A + B.
// Included by wave2_oracle_all.inc when W3_UI_ONLY is defined (executable wave3_ui_oracle_test = the W2 harness: deterministic arena, whole exe data image + the scratch block compared after every case).
//
// Fixture: the globals under test live at their exe addresses on both sides (NOTSA_ORACLE_ADDRESS_GLOBALS): CMessages arrays, CHud strings / timers, CRadar blips, CFont state ...; a scratch block g_sc
// holds the strings the message / text rows point at and out buffers. Every case rebuilds the fixture from the seed; both sides see identical memory; the image + scratch are compared after the call.
// Draw / RenderWare calls are recorded by stubs on both sides (the call log is part of the compared scratch block; see the F/G files).
#include "Messages.h"
#include "Hud.h"
#include "Radar.h"
#include "Font.h"
#include "Sprite2d.h"
#include "Sprite.h"
#include "MenuManager.h"
#include "Text/Text.h"
#include "Timer.h"
#include "Camera.h"
#include "World.h"
#include "PlayerInfo.h"
#include "Entity/Ped/Ped.h"
#include "Entity/Ped/PlayerPed.h"
#include "Entity/Vehicle/Vehicle.h"

namespace w3u {
using namespace w2;
constexpr size_t kScSize = 0x20000;
alignas(16) static uint8_t g_sc[kScSize];
constexpr size_t oStr = 0x0 /*32 strings x 0x100*/, oOut = 0x2000 /*0x2000*/, oBuf = 0x4000;
static uint8_t* S(size_t off) { return g_sc + off; }
template<class T> static T& At(uintptr_t va) { return *reinterpret_cast<T*>(va); }
template<class T> static T& SAt(size_t off) { return *reinterpret_cast<T*>(g_sc + off); }
static uint32_t P32(const void* p) { return (uint32_t)(uintptr_t)p; }
static uint64_t U(const void* p) { return (uint64_t)(uintptr_t)p; }
static inline uint64_t B8(bool b) { return b ? 1 : 0; }
static inline uint64_t FB(float f) { uint32_t b; std::memcpy(&b, &f, 4); return b; }
static std::function<void(Rng&)> g_tweak;     // row-specific fixture work after the standard build
static float Fl(Rng& r, float scale, int special = 14) {
    if (special && r.below((uint32)special) == 0) return GenF(r, scale, S_NAN | S_ZERO | S_DEN | S_TINY | S_INF);
    return GF(r, scale);
}
// the strings the rows point at (a pool of GXT-like strings; the same text lives at several addresses, upper / lower case twins, strings with ~1~ markers, an empty string)
static const char* const kStr[] = {
    "ABC", "ABC", "abc", "", "HELLO", "~1~", "A ~1~ B ~1~ C", "x~1~", "~1~~1~~1~", "NO MARKER HERE", "~1", "1~", "~2~ ~1~", "~k~~PED_FIREWEAPON~ press", "Z", "ABCD", "HELLO ~1~ WORLD",
    "ABC ", " ABC", "0123456789012345678901234567890123456789", "~", "~~", "~~~", "~1~ ~1~ ~1~ ~1~ ~1~ ~1~", "tail~1~", "~1~head", "~a~", "FOO ~a~ BAR", "~a~~a~", "~a~ and ~1~", "x~a", "~a",
};
constexpr int kNStr = (int)(sizeof(kStr) / sizeof(kStr[0]));
static GxtChar* Str(int k) { return reinterpret_cast<GxtChar*>(S(oStr + (size_t)k * 0x100)); }
static void BuildStrings() {
    for (int k = 0; k < 32; ++k) { std::memset(Str(k), 0, 0x100); if (k < kNStr) std::memcpy(Str(k), kStr[k], std::strlen(kStr[k]) + 1); }
}
static GxtChar* PickStr(Rng& r) { return Str((int)r.below(r.below(8) == 0 ? 32 : kNStr)); }
static void BuildAll(Rng& r) {
    FillGarbage(r, g_sc, kScSize);
    BuildStrings();
    if (g_tweak) g_tweak(r);
}
template<class PF, class EF>
static bool UCase(Rng& r, std::string& d, PF&& pf, EF&& ef) {
    const uint32_t seed = r.u32() | 1;
    BuildAll(r);
    Reg sc{ (uintptr_t)g_sc, kScSize };
    Reg img{ kImgLo, kImgHi - kImgLo };
    return BothA(d, seed, { sc, img }, std::forward<PF>(pf), std::forward<EF>(ef));
}
} // namespace w3u

// Game.cpp is not linked (it pulls in the whole game): the one tiny callee DisplayThisBlip needs, with the port body
bool CGame::CanSeeOutSideFromCurrArea() { return currArea == AREA_CODE_NORMAL_WORLD; }

#include "EntryExitManager.h"
// EntryExitManager.cpp is not linked: FindPlayerCentreOfWorldForMap shifts the position by the interior stack, which the fixture keeps empty (ms_entryExitStackPosn == 0: the exe loop does nothing either)
void CEntryExitManager::GetPositionRelativeToOutsideWorld(CVector&) {}

struct Tu { explicit Tu(std::function<void(Rng&)> f) { w3u::g_tweak = std::move(f); } ~Tu() { w3u::g_tweak = nullptr; } };
template<class Fn> static void RunScaledU(const std::string& name, Fn&& fn, int cases) {
    static const int scale = std::getenv("W3U_SCALE") ? std::max(1, std::atoi(std::getenv("W3U_SCALE"))) : 1;
    static const int div = std::getenv("W3U_DIV") ? std::max(1, std::atoi(std::getenv("W3U_DIV"))) : 1;   // quick runs while developing
    Run(name, std::forward<Fn>(fn), std::max(10, cases * scale / div));
}
#define ROW(NAME, ADDR, CASES) { const int _cs = (CASES); const std::string _nm = std::string(NAME " " #ADDR); RunScaledU(_nm, [&](Rng& r, std::string& d) -> bool
#define ENDROW , _cs); }
#include "wave3_oracle_ui_a.inc"      // Messages.cpp
#include "wave3_oracle_ui_b.inc"      // Hud.cpp
#include "wave3_oracle_ui_c.inc"      // Radar.cpp
#include "wave3_oracle_ui_d.inc"      // Font.cpp
#include "wave3_oracle_ui_e.inc"      // Text / Sprite2d / MenuManager
#include "wave3_oracle_ui_f.inc"      // recorded draw / debug calls
static void TestW3UiAll() {
    (void)spdlog::default_logger_raw();   // the default logger is created lazily by the first NOTSA_LOG_* of the port: do it outside the redirected allocator
    TestW3UiA();
    TestW3UiB();
    TestW3UiC();
    TestW3UiD();
    TestW3UiE();
    TestW3UiF();
}

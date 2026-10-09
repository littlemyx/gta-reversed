// S6-C: differential test of the SCRIPT COMMAND HANDLERS of groups 13..15 (ids 1300..1599, source/game_sa/Scripts/Commands/Ported/Group13_15.cpp) against the original
// machine code (exe oracle, see game_oracle.h). Same machinery as game_oracle_test.cpp / script_oracle_test.cpp: the handler TU is #included (its handlers live in an
// anonymous namespace), a fake CRunningScript with a synthetic operand buffer is run through (1) the exe's group processor and (2) the CommandParser instantiation of the
// C++ handler; IP, compare-flag state and the result variables are compared bit-exact (PC=24 game mode and PC=53).
// usage: script_oracle_g13_15_test.exe [-v] [-v53] [-vn] [-n cases] [name-substring ...]     env RW_EXE_ORACLE=<path of gta_sa_compact.exe>
#include "game_oracle.h"

#include "General.h"
#include "Matrix.h"
#include "standalone/Fixups.h"
#include "MemoryMgr.h"

#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <float.h>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------------------------------------------------------------
// stubs of the symbols the compiled game TUs reference but the test never calls
namespace notsa::standalone::detail { bool g_DataImageLoaded = true; }
namespace notsa::standalone::Fixups {
void RegisterFunction(uint32_t, void*, const char*) {}
void RegisterUnverified(uint32_t, const char*, int, bool, uint32_t) {}
}
void ReversibleHooks::RHManager::AddHookToCategory(std::string_view, HookInstallOptions, std::shared_ptr<ReversibleHook::TwoWayHook>) {}
void* CMemoryMgr::Malloc(uint32 size, uint32) { return std::malloc(size); }
void* CMemoryMgr::Malloc(uint32 size) { return std::malloc(size); }
void  CMemoryMgr::Free(void* p) { std::free(p); }

// ---------------------------------------------------------------------------------------------------------------------------------
struct Rng {
    uint64_t s;
    int      sp = 0;      // percentage of "special" floats (NaN, +-0, denormal, huge, tiny, inf) produced by Spec()
    explicit Rng(uint64_t seed) : s(seed * 0x9E3779B97F4A7C15ull + 0x1234567ull) { for (int i = 0; i < 4; ++i) u32(); }
    uint32_t u32() { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (uint32_t)(s >> 16); }
    float    f01() { return (u32() & 0xFFFFFF) * (1.0f / 16777216.0f); }
    int      below(int n) { return (int)(u32() % (uint32_t)n); }
};
static bool g_nanDiff = false, g_hardDiff = false, g_special = false;   // per-case classification: payload-only NaN difference / real difference / special inputs used
static float BitsF(uint32_t b) { float f; std::memcpy(&f, &b, 4); return f; }
static uint32_t FBits(float f) { uint32_t b; std::memcpy(&b, &f, 4); return b; }
enum : unsigned { S_NAN = 1, S_ZERO = 2, S_DEN = 4, S_HUGE = 8, S_INF = 16, S_TINY = 32, S_ALL = 63, S_NOHUGE = S_NAN | S_ZERO | S_DEN | S_TINY };

// uniform in [-scale, scale], or (with probability rng.sp %) a special value from `mask`
static float GenF(Rng& r, float scale, unsigned mask = S_ALL) {
    if (r.sp && r.below(100) < r.sp) {
        for (int tries = 0; tries < 8; ++tries) {
            const unsigned k = 1u << r.below(6);
            if (!(mask & k)) continue;
            const bool neg = r.below(2);
            float v = 0.f;
            switch (k) {
            case S_NAN:  v = BitsF(r.below(2) ? 0x7FC00000u : (0x7F800001u + (r.u32() & 0x3FFFFF))); break;
            case S_ZERO: v = 0.f; break;
            case S_DEN:  v = BitsF(1 + (r.u32() & 0x7FFFFF)); break;
            case S_HUGE: v = 1e30f * (1.0f + r.f01() * 2.0f); break;
            case S_INF:  v = BitsF(0x7F800000u); break;
            case S_TINY: v = 1e-20f * (1.0f + r.f01() * 9.0f); break;
            }
            g_special = true;
            return neg ? -v : v;
        }
    }
    return (r.f01() * 2.0f - 1.0f) * scale;
}
// magnitudes spread over many decades (squares of large values overflow float but not double / extended)
static float PickScale(Rng& r) {
    static const float sc[] = { 1.f, 1.f, 10.f, 100.f, 3000.f, 1e5f, 1e10f, 1e19f, 1e-3f };
    return sc[r.below(9)];
}
static CVector GenV(Rng& r, float scale, unsigned mask = S_ALL) { return { GenF(r, scale, mask), GenF(r, scale, mask), GenF(r, scale, mask) }; }

static std::string F(float f) { char b[40]; std::snprintf(b, sizeof b, "%08X(%g)", FBits(f), (double)f); return b; }
static std::string V(const CVector& v) { return "{" + F(v.x) + "," + F(v.y) + "," + F(v.z) + "}"; }
static std::string Q(const CQuaternion& v) { return "{" + F(v.x) + "," + F(v.y) + "," + F(v.z) + "," + F(v.w) + "}"; }
static std::string Hex(const void* p, size_t n) {
    std::string s; char b[4];
    for (size_t i = 0; i < n; ++i) { std::snprintf(b, sizeof b, "%02X", ((const uint8*)p)[i]); s += b; }
    return s;
}
static bool IsNanBits(uint32_t b) { return (b & 0x7F800000u) == 0x7F800000u && (b & 0x7FFFFFu) != 0; }
static bool SameBits(uint32_t a, uint32_t b) {
    if (a == b) return true;
    if (IsNanBits(a) && IsNanBits(b)) g_nanDiff = true; else g_hardDiff = true;   // NaN vs NaN: only sign / payload / quiet bit (x87 picks the operand with the larger significand; fld+fstp quiets a signalling NaN, a plain mov does not)
    return false;
}
static bool SameF(float a, float b) { return SameBits(FBits(a), FBits(b)); }
// dword-wise comparison of raw memory (floats / ints / packed bytes)
static bool SameBlob(const void* a, const void* b, size_t n) {
    bool ok = true;
    for (size_t i = 0; i + 4 <= n; i += 4) { uint32_t x, y; std::memcpy(&x, (const uint8*)a + i, 4); std::memcpy(&y, (const uint8*)b + i, 4); ok &= SameBits(x, y); }
    return ok;
}
static bool SameV(const CVector& a, const CVector& b) { return SameF(a.x, b.x) && SameF(a.y, b.y) && SameF(a.z, b.z); }
static bool SameQ(const CQuaternion& a, const CQuaternion& b) { return SameF(a.x, b.x) && SameF(a.y, b.y) && SameF(a.z, b.z) && SameF(a.w, b.w); }

// ---------------------------------------------------------------------------------------------------------------------------------
static int  g_cases = 4000;
static bool g_verbose = false, g_verbose53 = false, g_verboseNan = false;
static std::vector<std::string> g_filters;
static int g_hits = 0;
static void Hit(bool b) { g_hits += b; }   // coverage: number of `true` results of a bool-returning function (per precision mode)
struct Row { std::string name; int hits = -1; int cases = 0, bad24 = 0, bad53 = 0, hardReg24 = 0, hardSpec24 = 0, nan24 = 0, hardReg53 = 0, hardSpec53 = 0, nan53 = 0; };
static std::vector<Row> g_rows;

static bool Wanted(const std::string& name) {
    if (g_filters.empty()) return true;
    for (auto& f : g_filters) if (name.find(f) != std::string::npos) return true;
    return false;
}
static void SetPC(int bits) { unsigned cw; _controlfp_s(&cw, bits == 24 ? _PC_24 : _PC_53, _MCW_PC); }

// fn(rng, desc) -> true when port and exe agree; on mismatch it describes the inputs/outputs in `desc`
template<class Fn>
static void Run(const std::string& name, Fn&& fn, int cases = 0) {
    if (!Wanted(name)) return;
    const int N = cases ? cases : g_cases;
    Row row{ name, -1, N };
    oracle::g_where = name.c_str();
    for (int pc : { 24, 53 }) {
        SetPC(pc);
        int shown = 0;
        g_hits = 0;
        for (int i = 0; i < N; ++i) {
            Rng r(0xC0FFEEull + (uint64_t)i * 7919);
            static const int spTab[] = { 0, 3, 10, 0, 25, 6 };
            r.sp = spTab[i % 6];
            std::string d;
            g_nanDiff = g_hardDiff = g_special = false;
            if (!fn(r, d)) {
                const bool payloadOnly = g_nanDiff && !g_hardDiff;
                (pc == 24 ? row.bad24 : row.bad53)++;
                int& cat = payloadOnly ? (pc == 24 ? row.nan24 : row.nan53) : g_special ? (pc == 24 ? row.hardSpec24 : row.hardSpec53) : (pc == 24 ? row.hardReg24 : row.hardReg53);
                cat++;
                if ((shown < 2 || g_verbose) && (!payloadOnly || g_verboseNan) && (pc == 24 || g_verbose || g_verbose53)) { ++shown; std::printf("    MISMATCH PC=%d case %d%s: %s\n", pc, i, g_special ? " (special inputs)" : "", d.c_str()); }
            }
        }
    }
    SetPC(53);
    row.hits = g_hits;
    std::printf("%-58s %5d%s  PC24 %4d [reg %d spec %d nanpayload %d]  PC53 %4d [reg %d spec %d nanpayload %d]\n", name.c_str(), N, row.hits ? (" true:" + std::to_string(row.hits)).c_str() : "", row.bad24, row.hardReg24, row.hardSpec24, row.nan24,
                row.bad53, row.hardReg53, row.hardSpec53, row.nan53);
    std::fflush(stdout);
    g_rows.push_back(row);
}

static void __cdecl HostMathErr(int, int, int, int) {}   // CRT math error reporter (errno / matherr via _getptd): floor(NaN / huge) calls it, the numeric result is unaffected
// the exe's CRT keeps per-thread data (_tiddata, rand seed at +0x14, errno, ...) behind _getptd (0x827B3D -> TLS/FLS imports, not available here): serve a zeroed block instead
static uint32_t g_fakePtd[0x100];
static void* __cdecl HostGetPtd() { return g_fakePtd; }
// the exe's rand() (0x821B1E) then runs unmodified on the fake block; the game port uses the game-owned LCG (standalone/GameRand.h, same constants): seed both
static void SeedBoth(uint32_t seed) { notsa::GameSRand(seed); g_fakePtd[5] = seed; }


#include "../../source/game_sa/Scripts/Commands/Ported/Group13_15.cpp"

// CRunningScript members used by the parser / handlers (RunningScript.cpp is not part of this test)
uint8 CRunningScript::ScriptArgCharNextFreeBuffer = 0;
std::array<std::array<char, COMMANDS_CHAR_BUFFER_SIZE>, COMMANDS_CHAR_BUFFERS_COUNT> CRunningScript::ScriptArgCharBuffers = {};
void CRunningScript::UpdateCompareFlag(bool state) { // verbatim from RunningScript.cpp
    if (m_NotFlag) state = !state;
    if (m_AndOrState == ANDOR_NONE) { m_CondResult = state; return; }
    if (m_AndOrState >= ANDS_1 && m_AndOrState <= ANDS_8) { m_CondResult &= state; m_AndOrState = (m_AndOrState == ANDS_1) ? ANDOR_NONE : m_AndOrState - 1; return; }
    if (m_AndOrState >= ORS_1 && m_AndOrState <= ORS_8) { m_CondResult |= state; m_AndOrState = (m_AndOrState == ORS_1) ? ANDOR_NONE : m_AndOrState - 1; return; }
}

namespace {
struct Code {
    std::vector<uint8_t> b;
    Code& I(int32_t v) { b.push_back(1); append(&v); return *this; }          // SCRIPT_PARAM_STATIC_INT_32BITS
    Code& F(float v) { b.push_back(6); append(&v); return *this; }            // SCRIPT_PARAM_STATIC_FLOAT
    Code& Out(uint16_t idx) { b.push_back(3); append(&idx); return *this; }   // SCRIPT_PARAM_LOCAL_NUMBER_VARIABLE (a result target)
    template<class T> void append(const T* p) { const auto* c = reinterpret_cast<const uint8_t*>(p); b.insert(b.end(), c, c + sizeof(T)); }
};

struct Rig {
    alignas(16) uint8_t sa[sizeof(CRunningScript)] {};
    alignas(16) uint8_t sb[sizeof(CRunningScript)] {};
    uint8_t ca[128] {}, cb[128] {};
    CRunningScript& A() { return *reinterpret_cast<CRunningScript*>(sa); }
    CRunningScript& B() { return *reinterpret_cast<CRunningScript*>(sb); }
    void Reset(const Code& code) {
        std::memset(sa, 0, sizeof sa); std::memset(sb, 0, sizeof sb);
        std::memset(ca, 0, sizeof ca); std::memset(cb, 0, sizeof cb);
        std::memcpy(ca, code.b.data(), code.b.size()); std::memcpy(cb, code.b.data(), code.b.size());
        for (auto* s : { &A(), &B() }) { std::memset(&s->m_LocalVars, 0xCD, sizeof s->m_LocalVars); }
        A().m_IP = ca; B().m_IP = cb;
    }
    bool Same(std::string& d) const {
        auto& a = const_cast<Rig*>(this)->A(); auto& b = const_cast<Rig*>(this)->B();
        bool ok = (a.m_IP - ca) == (b.m_IP - cb) && a.m_CondResult == b.m_CondResult && a.m_AndOrState == b.m_AndOrState;
        ok &= SameBlob(&a.m_LocalVars, &b.m_LocalVars, 8 * 4);
        if (!ok) d += " exe{ip+" + std::to_string(a.m_IP - ca) + " cond " + std::to_string(a.m_CondResult) + " locals " + Hex(&a.m_LocalVars, 16) + "} port{ip+" + std::to_string(b.m_IP - cb) + " cond " + std::to_string(b.m_CondResult) + " locals " + Hex(&b.m_LocalVars, 16) + "}";
        return ok;
    }
    void RunExe(int grp, int cmd) {
        static const unsigned fns[] = { 0, 0x48CDD0, 0x48EAA0, 0x490DB0 };
        oracle::Fn<char __fastcall(CRunningScript*, int, int)>(fns[grp - 12])(&A(), 0, cmd);
    }
    void RunPort(notsa::script::CommandHandlerFunction h) { h(&B()); }
};

// floats as the test generator produces them: mostly a grid (to hit equalities), sometimes random / special
float GenS(Rng& r, float scale) {
    switch (r.below(4)) {
    case 0:  return (float)(r.below(9) - 4) * 0.5f;
    case 1:  return (r.f01() * 2.f - 1.f) * scale;
    default: return GenF(r, scale);
    }
}
std::string Fl(std::initializer_list<float> l) { std::string s; for (float f : l) s += F(f) + " "; return s; }
}

static void TestScriptCommands() {
    static Rig rig;

    auto fcase = [&](const char* name, int grp, int cmd, notsa::script::CommandHandlerFunction port, int nIn, int nOut, bool flag) {
        Run(name, [=](Rng& r, std::string& d) {
            float v[8]; Code c;
            const float scale = PickScale(r);
            for (int i = 0; i < nIn; ++i) { v[i] = GenS(r, scale); c.F(v[i]); }
            for (int i = 0; i < nOut; ++i) c.Out((uint16_t)i);
            rig.Reset(c);
            rig.RunExe(grp, cmd); rig.RunPort(port);
            Hit(rig.A().m_CondResult);
            std::string e;
            if (rig.Same(e)) return true;
            d = "in " + Fl({ v[0], v[1], v[2], v[3], nIn > 4 ? v[4] : 0, nIn > 5 ? v[5] : 0, nIn > 6 ? v[6] : 0, nIn > 7 ? v[7] : 0 }) + e;
            (void)flag;
            return false;
        });
    };
    fcase("script 1434 IS_AUSTRALIAN_GAME", 14, 1434, &notsa::script::detail::CommandParser<COMMAND_IS_AUSTRALIAN_GAME, IsAustralianGame>, 0, 0, true);
    fcase("script 1444 GET_ANGLE_BETWEEN_2D_VECTORS", 14, 1444, &notsa::script::detail::CommandParser<COMMAND_GET_ANGLE_BETWEEN_2D_VECTORS, GetAngleBetween2DVectors>, 4, 1, false);
    fcase("script 1445 DO_2D_RECTANGLES_COLLIDE", 14, 1445, &notsa::script::detail::CommandParser<COMMAND_DO_2D_RECTANGLES_COLLIDE, Do2DRectanglesCollide>, 8, 0, true);
    fcase("script 1456 GET_2D_LINES_INTERSECT_POINT", 14, 1456, &notsa::script::detail::CommandParser<COMMAND_GET_2D_LINES_INTERSECT_POINT, Get2DLinesIntersectPoint>, 8, 2, true);
    fcase("script 1540 GET_HEADING_FROM_VECTOR_2D", 15, 1540, &notsa::script::detail::CommandParser<COMMAND_GET_HEADING_FROM_VECTOR_2D, GetHeadingFromVector2D>, 2, 1, false);

    // card decks: the exe's rand (0x821B1E) runs on the fake _tiddata (SeedBoth), the port uses the game LCG
    Run("script 1437+1438 SHUFFLE_CARD_DECKS / FETCH_NEXT_CARD", [](Rng& r, std::string& d) {
        const int decks = 1 + r.below(6);
        const uint32_t seed = r.u32();
        Code c; c.I(decks);
        rig.Reset(c);
        SeedBoth(seed); rig.RunExe(14, 1437);
        uint8_t exeCards[0x270 + 8]; std::memcpy(exeCards, reinterpret_cast<void*>(0xA44210), 0x278);
        SeedBoth(seed); rig.RunPort(&notsa::script::detail::CommandParser<COMMAND_SHUFFLE_CARD_DECKS, ShuffleCardDecks>);
        bool ok = std::memcmp(exeCards, reinterpret_cast<void*>(0xA44210), 0x278) == 0 && rig.Same(d);
        if (!ok) { d += " decks " + std::to_string(decks) + " seed " + std::to_string(seed); return false; }
        // fetch a few cards (position wraps / stops at 0 slots)
        for (int k = 0; k < 60 + decks * 60; ++k) {
            Code f; f.Out(0);
            rig.Reset(f);
            const int16_t pos0 = *reinterpret_cast<int16_t*>(0xA44210);
            rig.RunExe(14, 1438);
            const int16_t posExe = *reinterpret_cast<int16_t*>(0xA44210);
            *reinterpret_cast<int16_t*>(0xA44210) = pos0;
            rig.RunPort(&notsa::script::detail::CommandParser<COMMAND_FETCH_NEXT_CARD, FetchNextCard>);
            if (!(rig.Same(d) && posExe == *reinterpret_cast<int16_t*>(0xA44210))) { d += " fetch " + std::to_string(k) + " pos " + std::to_string(pos0); return false; }
        }
        return true;
    }, 300);
}

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "-v")) g_verbose = true;
        else if (!std::strcmp(argv[i], "-v53")) g_verbose53 = true;
        else if (!std::strcmp(argv[i], "-vn")) g_verboseNan = true;
        else if (!std::strcmp(argv[i], "-n") && i + 1 < argc) g_cases = std::atoi(argv[++i]);
        else g_filters.push_back(argv[i]);
    }
    const char* exe = std::getenv("RW_EXE_ORACLE");
    if (!exe) { std::printf("script_oracle_g13_15_test: set RW_EXE_ORACLE=<path of gta_sa_compact.exe>; skipped\n"); return 0; }
    if (!oracle::Map(exe)) { std::printf("FATAL: cannot map the exe\n"); return 2; }
    *reinterpret_cast<int*>(0xC9C400) = 1; // __sse2_available
    oracle::Patch(0x82872C, (void*)&HostMathErr);
    oracle::Patch(0x827B3D, (void*)&HostGetPtd);
    std::printf("script_oracle_g13_15_test: %d cases per command and precision mode; PC24 = game mode, PC53 = CRT default\n", g_cases);
    TestScriptCommands();
    int hard24 = 0;
    for (auto& r : g_rows) hard24 += r.hardReg24 + r.hardSpec24;
    std::printf("\n%zu commands, PC24 mismatches (excluding NaN-payload-only): %d\n", g_rows.size(), hard24);
    return hard24 ? 1 : 0;
}

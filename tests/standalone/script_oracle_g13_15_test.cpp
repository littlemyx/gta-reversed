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
#include "../../source/game_sa/Tasks/TaskSequences.cpp"   // CTaskSequences::GetAvailableSlot (0x632E00) is tested through 1557
#include <functional>

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


// =================================================================================================================================
// Second part (S6-C review): handlers that touch game memory (fake entities in the real pools, the sequence-task / ped-group / 2d-effect
// tables of the data image) and call game functions. The callees are replaced by host recorders on BOTH sides (exe: oracle::Patch,
// C++: the definitions below); compared: the watched memory (dword-wise, NaN payload differences are classified separately), the call log,
// the script state (IP, compare flag, result variables).
// =================================================================================================================================
static std::vector<uint32_t> g_mlog;
static uint32_t g_hostRet;
static uint32_t U(const void* p) { return (uint32_t)(uintptr_t)p; }
template<class... A> static void ML(uint32_t tag, A... a) { g_mlog.push_back(tag); (g_mlog.push_back((uint32_t)(a)), ...); }

static void __fastcall H_Skip(void* self, int) { ML(1, U(self)); }
static void __fastcall H_AddMoving(void* self, int) { ML(2, U(self)); }
static void __cdecl    H_WorldRemove(void* e) { ML(3, U(e)); }
static void __cdecl    H_WorldAdd(void* e) { ML(4, U(e)); }
static void __fastcall H_RegRef(void* self, int, void** ref) { ML(5, U(self), U(ref)); }
static void __fastcall H_ClearWeapon(void* self, int, int t) { ML(6, U(self), t); }
static void __fastcall H_ReqNodes(void* self, int, float a, float b, float c, float d) { ML(7, FBits(a), FBits(b), FBits(c), FBits(d)); }
static void __fastcall H_RelNodes(void* self, int) { ML(8); }
static void __cdecl    H_RecStart(void* v, int f, int ai, int loop) { ML(9, U(v), f, (uint8_t)ai, (uint8_t)loop); }
static void __cdecl    H_RecStop(void* v) { ML(10, U(v)); }
static void __cdecl    H_RecPause(void* v) { ML(11, U(v)); }
static void __cdecl    H_RecUnpause(void* v) { ML(12, U(v)); }
static char __cdecl    H_RecIs(void* v) { ML(13, U(v)); return (char)(g_hostRet & 1); }
static int  __cdecl    H_GAST(int ref, int type) { ML(14, ref, (uint8_t)type); return ref; }                       // identity: the test chooses the (in)valid index
static int  __cdecl    H_GNU(int idx, int type) { ML(15, idx, (uint8_t)type); return idx | (type << 16) | 0x1000000; }
static void __fastcall H_CleanAdd(void* self, int, int h, int t) { ML(16, h, t); }
static void __fastcall H_CleanRemove(void* self, int, int h, int t) { ML(17, h, t); }
static void __fastcall H_Flush(void* self, int) { ML(18, U(self)); }
static void __cdecl    H_RemoveGroup(int g) { ML(19, g); }
static void __cdecl    H_RemoveFollowers(int g) { ML(20, g); }
static int  __cdecl    H_AddFx(float radius) { ML(22, FBits(radius)); return (int)(int8_t)g_hostRet; }
static void __fastcall H_SetIsStatic(void* self, int, int flag) { ML(21, U(self), flag & 0xFF); auto* b = (uint8_t*)self + 0x1C; *b = (uint8_t)((*b & ~4) | ((flag & 1) << 2)); }

void    CPhysical::SkipPhysics() { ML(1, U(this)); }
void    CPhysical::AddToMovingList() { ML(2, U(this)); }
void    CWorld::Remove(CEntity* e) { ML(3, U(e)); }
void    CWorld::Add(CEntity* e) { ML(4, U(e)); }
void    CEntity::RegisterReference(CEntity** r) { ML(5, U(this), U(r)); }
void    CPed::ClearWeapon(eWeaponType t) { ML(6, U(this), (int)t); }
void    CPathFind::MakeRequestForNodesToBeLoaded(float a, float b, float c, float d) { ML(7, FBits(a), FBits(b), FBits(c), FBits(d)); }
void    CPathFind::ReleaseRequestedNodes() { ML(8); }
void    CVehicleRecording::StartPlaybackRecordedCar(CVehicle* v, int32 f, bool ai, bool loop) { ML(9, U(v), f, (uint8_t)ai, (uint8_t)loop); }
void    CVehicleRecording::StopPlaybackRecordedCar(CVehicle* v) { ML(10, U(v)); }
void    CVehicleRecording::PausePlaybackRecordedCar(CVehicle* v) { ML(11, U(v)); }
void    CVehicleRecording::UnpausePlaybackRecordedCar(CVehicle* v) { ML(12, U(v)); }
bool    CVehicleRecording::IsPlaybackGoingOnForCar(CVehicle* v) { ML(13, U(v)); return g_hostRet & 1; }
int32   CTheScripts::GetActualScriptThingIndex(int32 ref, eScriptThingType type) { ML(14, ref, (uint8_t)type); return ref; }
int32   CTheScripts::GetNewUniqueScriptThingIndex(int32 idx, eScriptThingType type) { ML(15, idx, (uint8_t)type); return idx | ((int)type << 16) | 0x1000000; }
void    CMissionCleanup::AddEntityToList(int32 h, MissionCleanUpEntityType t) { ML(16, h, (int)t); }
void    CMissionCleanup::RemoveEntityFromList(int32 h, MissionCleanUpEntityType t) { ML(17, h, (int)t); }
void    CTaskComplexSequence::Flush() { ML(18, U(this)); }
void    CPedGroups::RemoveGroup(int32 g) { ML(19, g); }
void    CPedGroups::RemoveAllFollowersFromGroup(int32 g) { ML(20, g); }
C2dEffectPedAttractor* CScripted2dEffects::GetEffect(int32 index) { return reinterpret_cast<C2dEffectPedAttractor*>(&ms_effects[index]); }
int32   CScripted2dEffects::AddScripted2DEffect(float radius) { ML(22, FBits(radius)); return (int)(int8_t)g_hostRet; }
void    CObject::SetIsStatic(bool f) { H_SetIsStatic(this, 0, f); }   // (never reached: the fake object's vtable points at the host recorder)
// accessors that are not part of this test: forward to the exe's own
bool    CPed::IsPlayer() const { return oracle::Fn<char __fastcall(const CPed*, int)>(0x5DF8F0)(this, 0) != 0; }
CPed*   CPedGroupMembership::GetLeader() const { return oracle::Fn<CPed* __fastcall(const CPedGroupMembership*, int)>(0x5F69A0)(this, 0); }

int32 CRunningScript::CollectNextParameterWithoutIncreasingPC() { return oracle::Fn<int __fastcall(CRunningScript*, int)>(0x464250)(this, 0); }   // (RunningScript.cpp is not part of this test)
CMatrix& CPlaceable::GetMatrix() { return *m_matrix; }   // (the test objects always have a matrix)
CPedPool*     GetPedPool()     { return *reinterpret_cast<CPedPool**>(0xB74490); }
CVehiclePool* GetVehiclePool() { return *reinterpret_cast<CVehiclePool**>(0xB74494); }
CObjectPool*  GetObjectPool()  { return *reinterpret_cast<CObjectPool**>(0xB7449C); }

static CVehicle* g_veh; static CVehicle* g_veh2; static int g_vehRef, g_veh2Ref;
static CObject*  g_obj; static int g_objRef;
static CPed*     g_ped; static CPed* g_ped2; static int g_pedRef, g_ped2Ref;
static CMatrix*  g_mat;
static void*     g_fakeVtbl[256];
static void*     g_fakeObjVtbl[256];
static void*     g_fakePedVtbl[256];

static void RFill(void* p, size_t n, Rng& r) { auto* b = (uint8_t*)p; for (size_t i = 0; i < n; ++i) b[i] = (uint8_t)r.u32(); }
static float RF(Rng& r, float scale) { return GenF(r, scale); }
static void RType(void* e, Rng& r, int type) { auto* b = (uint8_t*)e + 0x36; if (r.below(10)) *b = (uint8_t)((*b & ~7) | type); }   // entity type bits (mostly the right one)
static CEntity* RPick(Rng& r, bool allowNull = true) {
    CEntity* c[] = { nullptr, g_ped, g_ped2, g_veh, g_veh2, g_obj };
    return c[r.below(allowNull ? 6 : 5) + (allowNull ? 0 : 1)];
}
static void FillVeh(CVehicle* v, Rng& r) {
    RFill(v, sizeof(CAutomobile), r);
    *reinterpret_cast<void**>(v) = g_fakeVtbl;
    v->m_matrix = reinterpret_cast<decltype(v->m_matrix)>(g_mat);
    RType(v, r, 2);
    v->m_pLastDamageEntity = RPick(r);
    v->m_vecMoveSpeed = CVector{ RF(r, 1.f), RF(r, 1.f), RF(r, 1.f) };
    v->m_vecTurnSpeed = CVector{ RF(r, 1.f), RF(r, 1.f), RF(r, 1.f) };
    v->m_autoPilot.m_TargetEntity = (CVehicle*)RPick(r);
    const int k = r.below(4);
    v->m_autoPilot.m_nCarMission = (eCarMission)(k == 0 ? 0x39 : k == 1 ? 0x3A : (int)(r.u32() & 0xFF));
    if (r.below(3) == 0) v->m_nFlags &= ~4u;
    if (r.below(3) == 0) v->m_nFlags |= 4u;
    if (r.below(3) == 0) v->m_nFlags &= ~0x40000u;
}
static void FillPed(CPed* p, Rng& r) {
    RFill(p, sizeof(CPed), r);
    *reinterpret_cast<void**>(p) = g_fakePedVtbl;
    p->m_matrix = reinterpret_cast<decltype(p->m_matrix)>(g_mat);
    RType(p, r, 3);
    const int k = r.below(3);
    p->m_pVehicle = k == 0 ? nullptr : k == 1 ? g_veh : g_veh2;
    *reinterpret_cast<uint32_t*>((uint8_t*)p + 0x598) = (uint32_t)r.below(5);   // m_nPedType (IsPlayer: 0 / 1)
}
static void FillObj(CObject* o, Rng& r) {
    RFill(o, sizeof(CObject), r);
    *reinterpret_cast<void**>(o) = g_fakeObjVtbl;
    o->m_matrix = reinterpret_cast<decltype(o->m_matrix)>(g_mat);
    RType(o, r, 4);
    const int sc = r.below(3);
    o->m_vecMoveSpeed = CVector{ RF(r, 1.f), RF(r, 1.f), RF(r, 1.f) };
    if (sc == 0) o->m_vecMoveSpeed = CVector{ 0.f, 0.f, RF(r, 1.f) > 0 ? 1.f : (r.below(2) ? 2.5f : -0.3f) };
    if (sc == 1) o->m_vecMoveSpeed = CVector{ 0.f, 0.f, 0.f };
    o->m_vecTurnSpeed = CVector{ RF(r, 1.f), RF(r, 1.f), RF(r, 1.f) };
    if (r.below(3) == 0) o->m_nFlags &= ~4u;
    if (r.below(3) == 0) o->m_nFlags |= 4u;
    CTimer::ms_fTimeStep = r.below(10) ? r.f01() * 4.f + 0.01f : RF(r, 5.f);
}
static void FillMat(Rng& r) {
    RFill(g_mat, sizeof(CMatrix), r);
    float* f = reinterpret_cast<float*>(g_mat);
    for (int i = 0; i < 16; ++i) f[i] = RF(r, r.below(4) ? 1.5f : 100.f);
}
static void FillWorld(Rng& r) {
    FillMat(r);
    FillVeh(g_veh, r); FillVeh(g_veh2, r); FillPed(g_ped, r); FillPed(g_ped2, r); FillObj(g_obj, r);
}
static void SetupFakeWorld() {
    auto* vp = new CVehiclePool(4, "oracle_veh"); auto* op = new CObjectPool(4, "oracle_obj"); auto* pp = new CPedPool(4, "oracle_ped");
    *reinterpret_cast<CVehiclePool**>(0xB74494) = vp; *reinterpret_cast<CObjectPool**>(0xB7449C) = op; *reinterpret_cast<CPedPool**>(0xB74490) = pp;
    g_veh = vp->New(); g_veh2 = vp->New(); g_vehRef = vp->GetRef(g_veh); g_veh2Ref = vp->GetRef(g_veh2);
    g_obj = op->New(); g_objRef = op->GetRef(g_obj);
    g_ped = pp->New(); g_ped2 = pp->New(); g_pedRef = pp->GetRef(g_ped); g_ped2Ref = pp->GetRef(g_ped2);
    g_mat = new CMatrix();
    g_fakeObjVtbl[0x10 / 4] = (void*)&H_SetIsStatic;
}

struct MCtx {
    Rng* r{};
    Code code;
    std::vector<std::pair<void*, size_t>> watch;
    bool cleanup{};
    bool notFlag{};
};

static void MTest(const char* name, int grp, int cmd, notsa::script::CommandHandlerFunction port, std::function<void(MCtx&)> gen, int cases = 3000, bool flagCmd = false) {
    if (!Wanted(name)) return;
    static Rig rig;
    Row row{ name, -1, cases };
    oracle::g_where = name;
    SetPC(24);
    int shown = 0; g_hits = 0;
    for (int i = 0; i < cases; ++i) {
        Rng r(0xBEEF00ull + (uint64_t)cmd * 104729 + (uint64_t)i * 7919);
        static const int spTab[] = { 0, 3, 10, 0, 25, 6 };
        r.sp = spTab[i % 6];
        MCtx c; c.r = &r; c.cleanup = r.below(2); c.notFlag = r.below(5) == 0;
        g_nanDiff = g_hardDiff = g_special = false;
        gen(c);
        std::vector<uint8_t> pre;
        for (auto& w : c.watch) pre.insert(pre.end(), (uint8_t*)w.first, (uint8_t*)w.first + w.second);
        rig.Reset(c.code);
        for (auto* sc : { &rig.A(), &rig.B() }) { sc->m_UsesMissionCleanup = c.cleanup; sc->m_NotFlag = c.notFlag; }
        g_mlog.clear();
        static std::string phase; phase = std::string(name) + " [exe]"; oracle::g_where = phase.c_str();
        rig.RunExe(grp, cmd);
        std::vector<uint8_t> postExe;
        for (auto& w : c.watch) postExe.insert(postExe.end(), (uint8_t*)w.first, (uint8_t*)w.first + w.second);
        const auto logExe = g_mlog;
        Hit(rig.A().m_CondResult);
        size_t o = 0;
        for (auto& w : c.watch) { std::memcpy(w.first, pre.data() + o, w.second); o += w.second; }
        g_mlog.clear();
        phase = std::string(name) + " [port]"; oracle::g_where = phase.c_str();
        rig.RunPort(port);
        std::vector<uint8_t> postPort;
        for (auto& w : c.watch) postPort.insert(postPort.end(), (uint8_t*)w.first, (uint8_t*)w.first + w.second);
        std::string d;
        bool ok = rig.Same(d);
        ok &= SameBlob(postExe.data(), postPort.data(), postExe.size() & ~3u);
        const bool logSame = logExe == g_mlog;
        ok &= logSame;
        if (!ok) {
            const bool payloadOnly = g_nanDiff && !g_hardDiff && logSame;
            row.bad24++;
            (payloadOnly ? row.nan24 : g_special ? row.hardSpec24 : row.hardReg24)++;
            if (shown < 2 && !payloadOnly) {
                ++shown;
                std::string t = " memdiff:";
                for (size_t k = 0; k + 4 <= postExe.size() && t.size() < 200; k += 4) if (std::memcmp(&postExe[k], &postPort[k], 4)) { char b[64]; std::snprintf(b, sizeof b, " +0x%zx exe %08X port %08X", k, *(uint32_t*)&postExe[k], *(uint32_t*)&postPort[k]); t += b; }
                std::string l = logSame ? "" : " LOG exe[" + std::to_string(logExe.size()) + "] port[" + std::to_string(g_mlog.size()) + "]";
                for (size_t k = 0; !logSame && k < logExe.size() && k < g_mlog.size(); ++k) if (logExe[k] != g_mlog[k]) { char b[64]; std::snprintf(b, sizeof b, " first diff [%zu] exe %X port %X", k, logExe[k], g_mlog[k]); l += b; break; }
                std::printf("    MISMATCH case %d (cleanup %d not %d)%s: %s%s%s\n", i, c.cleanup, c.notFlag, g_special ? " (special)" : "", d.c_str(), t.c_str(), l.c_str());
            }
        }
    }
    SetPC(53);
    row.hits = g_hits;
    std::printf("%-58s %5d%s  PC24 %4d [reg %d spec %d nanpayload %d]\n", name, cases, (flagCmd ? (" true:" + std::to_string(row.hits)).c_str() : ""), row.bad24, row.hardReg24, row.hardSpec24, row.nan24);
    g_rows.push_back(row);
}

#define MT(id, NAME, FN, GRP, ...) MTest("script " #id " " #NAME, GRP, id, &notsa::script::detail::CommandParser<COMMAND_##NAME, FN>, __VA_ARGS__)

static int PickVehRef(Rng& r, bool allowBad = false) {
    const int k = r.below(allowBad ? 6 : 2);
    if (k == 0) return g_vehRef; if (k == 1) return g_veh2Ref;
    return k == 2 ? -1 : (int)((r.below(4) << 8) | r.below(256));   // stale / free slot handle (the pools are not bounds-checked)
}
static int PickPedRef(Rng& r) { return r.below(2) ? g_pedRef : g_ped2Ref; }


static double ExeMagnitude(const CVector* pv) {
    double ex;
    __asm { mov ecx, pv
            mov eax, 0x4082C0
            call eax
            fstp qword ptr [ex] }
    return ex;
}
// ---- the local re-implementations of the original CVector / CMatrix helpers (Group13_15.cpp) against the exe's own functions
static void TestHelpers() {
    Run("helper 0x59C910 NormaliseOriginal", [](Rng& r, std::string& d) {
        const float sc = PickScale(r); CVector v = GenV(r, sc), a = v, b = v;
        oracle::Fn<void __fastcall(CVector*, int)>(0x59C910)(&a, 0);
        NormaliseOriginal(b);
        if (SameV(a, b)) return true; d = "in " + V(v) + " exe " + V(a) + " port " + V(b); return false; });
    Run("helper 0x59C730 CrossProductOriginal", [](Rng& r, std::string& d) {
        const float sc = PickScale(r); CVector a = GenV(r, sc), b = GenV(r, sc), x;
        oracle::Fn<CVector* __cdecl(CVector*, const CVector*, const CVector*)>(0x59C730)(&x, &a, &b);
        const CVector y = CrossProductOriginal(a, b);
        if (SameV(x, y)) return true; d = "in " + V(a) + V(b) + " exe " + V(x) + " port " + V(y); return false; });
    Run("helper 0x59C790 TransformVectorOriginal", [](Rng& r, std::string& d) {
        CMatrix m; float* f = reinterpret_cast<float*>(&m); for (int i = 0; i < 16; ++i) f[i] = GenF(r, 1.5f);
        const CVector v = GenV(r, 3.f); CVector x;
        oracle::Fn<CVector* __cdecl(CVector*, const CMatrix*, const CVector*)>(0x59C790)(&x, &m, &v);
        const CVector y = TransformVectorOriginal(m, v);
        if (SameV(x, y)) return true; d = "exe " + V(x) + " port " + V(y); return false; });
    Run("helper 0x59C810 Multiply3x3VMOriginal", [](Rng& r, std::string& d) {
        CMatrix m; float* f = reinterpret_cast<float*>(&m); for (int i = 0; i < 16; ++i) f[i] = GenF(r, 1.5f);
        const CVector v = GenV(r, 3.f); CVector x;
        oracle::Fn<CVector* __cdecl(CVector*, const CVector*, const CMatrix*)>(0x59C810)(&x, &v, &m);
        const CVector y = Multiply3x3VMOriginal(v, m);
        if (SameV(x, y)) return true; d = "exe " + V(x) + " port " + V(y); return false; });
    Run("helper 0x59C890 TransformPointOriginal", [](Rng& r, std::string& d) {
        CMatrix m; float* f = reinterpret_cast<float*>(&m); for (int i = 0; i < 16; ++i) f[i] = GenF(r, 1.5f);
        const CVector v = GenV(r, 3.f); CVector x;
        oracle::Fn<CVector* __cdecl(CVector*, const CMatrix*, const CVector*)>(0x59C890)(&x, &m, &v);
        const CVector y = TransformPointOriginal(m, v);
        if (SameV(x, y)) return true; d = "exe " + V(x) + " port " + V(y); return false; });
    Run("helper 0x4082C0 MagnitudeOriginal", [](Rng& r, std::string& d) {
        const float sc = PickScale(r); const CVector v = GenV(r, sc); const double ex = ExeMagnitude(&v);
        const double po = MagnitudeOriginal(v);
        uint64_t a, b; std::memcpy(&a, &ex, 8); std::memcpy(&b, &po, 8);
        if (a == b || (ex != ex && po != po)) return true;
        char t[120]; std::snprintf(t, sizeof t, "exe %.17g port %.17g", ex, po); d = std::string("in ") + V(v) + t; g_hardDiff = true; return false; });
}

static void TestMemCommands() {
    auto WatchAll = [](MCtx& c) {
        c.watch = { { g_veh, sizeof(CAutomobile) }, { g_veh2, sizeof(CAutomobile) }, { g_ped, sizeof(CPed) }, { g_ped2, sizeof(CPed) }, { g_obj, sizeof(CObject) } };
    };
    auto Veh = [&](int h, auto&& extra) { return [=](MCtx& c) { FillWorld(*c.r); WatchAll(c); c.code.I(PickVehRef(*c.r)); extra(c); }; };
    (void)Veh;
    // ---- vehicles
    MT(1305, FREEZE_CAR_POSITION, FreezeCarPosition, 13, [&](MCtx& c) { FillWorld(*c.r); WatchAll(c); c.code.I(PickVehRef(*c.r)).I(c.r->below(3) ? (int)c.r->below(2) : (int)c.r->u32()); });
    MT(1308, HAS_CAR_BEEN_DAMAGED_BY_CHAR, HasCarBeenDamagedByChar, 13, [&](MCtx& c) {
        FillWorld(*c.r); WatchAll(c);
        auto* veh = c.r->below(2) ? g_veh : g_veh2; auto* ped = c.r->below(2) ? g_ped : g_ped2;
        if (c.r->below(2)) veh->m_pLastDamageEntity = ped; else if (c.r->below(2)) veh->m_pLastDamageEntity = ped->m_pVehicle;
        c.code.I(veh == g_veh ? g_vehRef : g_veh2Ref).I(c.r->below(4) ? (ped == g_ped ? g_pedRef : g_ped2Ref) : -1); }, 3000, true);
    MT(1309, HAS_CAR_BEEN_DAMAGED_BY_CAR, HasCarBeenDamagedByCar, 13, [&](MCtx& c) {
        FillWorld(*c.r); WatchAll(c);
        auto* veh = c.r->below(2) ? g_veh : g_veh2; auto* other = c.r->below(2) ? g_veh : g_veh2;
        if (c.r->below(2)) veh->m_pLastDamageEntity = other;
        c.code.I(veh == g_veh ? g_vehRef : g_veh2Ref).I(c.r->below(4) ? (other == g_veh ? g_vehRef : g_veh2Ref) : (c.r->below(2) ? -1 : (int)((c.r->below(4) << 8) | c.r->below(256)))); }, 3000, true);
    MT(1343, SET_CAN_BURST_CAR_TYRES, SetCanBurstCarTyres, 13, [&](MCtx& c) { FillWorld(*c.r); WatchAll(c); c.code.I(PickVehRef(*c.r)).I(c.r->below(3) ? (int)c.r->below(2) : (int)c.r->u32()); });
    MT(1359, CLEAR_CAR_LAST_DAMAGE_ENTITY, ClearCarLastDamageEntity, 13, [&](MCtx& c) { FillWorld(*c.r); WatchAll(c); c.code.I(PickVehRef(*c.r, true)); });
    MT(1380, MAKE_HELI_COME_CRASHING_DOWN, MakeHeliComeCrashingDown, 13, [&](MCtx& c) { FillWorld(*c.r); WatchAll(c); c.code.I(PickVehRef(*c.r)); });
    MT(1390, DOES_VEHICLE_EXIST, DoesVehicleExist, 13, [&](MCtx& c) { FillWorld(*c.r); WatchAll(c); c.code.I(PickVehRef(*c.r, true)); }, 3000, true);
    MT(1396, FREEZE_CAR_POSITION_AND_DONT_LOAD_COLLISION, FreezeCarPositionAndDontLoadCollision, 13, [&](MCtx& c) { FillWorld(*c.r); WatchAll(c); c.code.I(PickVehRef(*c.r)).I(c.r->below(3) ? (int)c.r->below(2) : (int)c.r->u32()); });
    MT(1415, SET_LOAD_COLLISION_FOR_CAR_FLAG, SetLoadCollisionForCarFlag, 14, [&](MCtx& c) { FillWorld(*c.r); WatchAll(c); c.code.I(PickVehRef(*c.r)).I(c.r->below(3) ? (int)c.r->below(2) : (int)c.r->u32()); });
    MT(1521, SET_CAR_ESCORT_CAR_LEFT, SetCarEscortCar<MISSION_ESCORT_LEFT>, 15, [&](MCtx& c) { FillWorld(*c.r); WatchAll(c); c.code.I(g_vehRef).I(g_veh2Ref); });
    MT(1522, SET_CAR_ESCORT_CAR_RIGHT, SetCarEscortCar<MISSION_ESCORT_RIGHT>, 15, [&](MCtx& c) { FillWorld(*c.r); WatchAll(c); c.code.I(g_veh2Ref).I(g_vehRef); });
    MT(1523, SET_CAR_ESCORT_CAR_REAR, SetCarEscortCar<MISSION_ESCORT_REAR>, 15, [&](MCtx& c) { FillWorld(*c.r); WatchAll(c); c.code.I(g_vehRef).I(g_veh2Ref); });
    MT(1524, SET_CAR_ESCORT_CAR_FRONT, SetCarEscortCar<MISSION_ESCORT_FRONT>, 15, [&](MCtx& c) { FillWorld(*c.r); WatchAll(c); c.code.I(g_vehRef).I(g_veh2Ref); });
    MT(1365, REMOVE_WEAPON_FROM_CHAR, RemoveWeaponFromChar, 13, [&](MCtx& c) { FillWorld(*c.r); WatchAll(c); c.code.I(PickPedRef(*c.r)).I((int)c.r->u32() % 60); });
    // ---- vehicle recordings (callee log: the handle -> pointer conversion and the argument shapes)
    MT(1515, START_PLAYBACK_RECORDED_CAR, StartPlaybackRecordedCar, 15, [&](MCtx& c) { FillWorld(*c.r); WatchAll(c); c.code.I(PickVehRef(*c.r, true)).I((int)c.r->u32() % 1000); });
    MT(1516, STOP_PLAYBACK_RECORDED_CAR, StopPlaybackRecordedCar, 15, [&](MCtx& c) { FillWorld(*c.r); WatchAll(c); c.code.I(PickVehRef(*c.r, true)); });
    MT(1517, PAUSE_PLAYBACK_RECORDED_CAR, PausePlaybackRecordedCar, 15, [&](MCtx& c) { FillWorld(*c.r); WatchAll(c); c.code.I(PickVehRef(*c.r, true)); });
    MT(1518, UNPAUSE_PLAYBACK_RECORDED_CAR, UnpausePlaybackRecordedCar, 15, [&](MCtx& c) { FillWorld(*c.r); WatchAll(c); c.code.I(PickVehRef(*c.r, true)); });
    MT(1550, IS_PLAYBACK_GOING_ON_FOR_CAR, IsPlaybackGoingOnForCar, 15, [&](MCtx& c) { FillWorld(*c.r); WatchAll(c); g_hostRet = c.r->u32(); c.code.I(PickVehRef(*c.r, true)); }, 3000, true);
    // ---- path nodes
    MT(1542, LOAD_PATH_NODES_IN_AREA, LoadPathNodesInArea, 15, [&](MCtx& c) { auto& r = *c.r; const float s = PickScale(r); c.code.F(GenS(r, s)).F(GenS(r, s)).F(GenS(r, s)).F(GenS(r, s)); });
    MT(1543, RELEASE_PATH_NODES, ReleasePathNodes, 15, [&](MCtx& c) { (void)c; }, 200);
    // ---- objects
    MT(1360, FREEZE_OBJECT_POSITION, FreezeObjectPosition, 13, [&](MCtx& c) { FillWorld(*c.r); WatchAll(c); c.code.I(g_objRef).I(c.r->below(3) ? (int)c.r->below(2) : (int)c.r->u32()); });
    MT(1382, SET_OBJECT_AREA_VISIBLE, SetObjectAreaVisible, 13, [&](MCtx& c) { FillWorld(*c.r); WatchAll(c); c.code.I(g_objRef).I(c.r->below(2) ? (int)c.r->below(24) : (int)c.r->u32()); });
    MT(1441, ADD_TO_OBJECT_ROTATION_VELOCITY, AddToObjectRotationVelocity, 14, [&](MCtx& c) { FillWorld(*c.r); WatchAll(c); c.code.I(g_objRef).F(RF(*c.r, 3.f)).F(RF(*c.r, 3.f)).F(RF(*c.r, 3.f)); });
    MT(1442, SET_OBJECT_ROTATION_VELOCITY, SetObjectRotationVelocity, 14, [&](MCtx& c) { FillWorld(*c.r); WatchAll(c); c.code.I(g_objRef).F(RF(*c.r, 3.f)).F(RF(*c.r, 3.f)).F(RF(*c.r, 3.f)); });
    MT(1443, IS_OBJECT_STATIC, IsObjectStatic, 14, [&](MCtx& c) { FillWorld(*c.r); WatchAll(c); c.code.I(g_objRef); }, 3000, true);
    MT(1446, GET_OBJECT_ROTATION_VELOCITY, GetObjectRotationVelocity, 14, [&](MCtx& c) { FillWorld(*c.r); WatchAll(c); c.code.I(g_objRef).Out(0).Out(1).Out(2); });
    MT(1447, ADD_VELOCITY_RELATIVE_TO_OBJECT_VELOCITY, AddVelocityRelativeToObjectVelocity, 14, [&](MCtx& c) { FillWorld(*c.r); WatchAll(c); c.code.I(g_objRef).F(RF(*c.r, 30.f)).F(RF(*c.r, 30.f)).F(RF(*c.r, 30.f)); });
    MT(1448, GET_OBJECT_SPEED, GetObjectSpeed, 14, [&](MCtx& c) { FillWorld(*c.r); WatchAll(c); c.code.I(g_objRef).Out(0); });
    // ---- sequence tasks / attractors / groups: tables of the data image
    auto SeqWatch = [](MCtx& c) {
        c.watch = { { (void*)0xC17898, 64 }, { (void*)0x8D2E98, 4 }, { (void*)0xC178F0, 64 * 0x40 }, { (void*)0xA43F68, 64 * 4 }, { (void*)0xC3A1A0, 64 } };
    };
    auto SeqFill = [](MCtx& c) {
        auto& r = *c.r;
        RFill((void*)0xC17898, 64, r); for (int i = 0; i < 64; ++i) ((uint8_t*)0xC17898)[i] &= 1;
        RFill((void*)0xC178F0, 64 * 0x40, r);
        for (int i = 0; i < 64; ++i) { auto* b = (uint8_t*)0xC178F0 + i * 0x40; *(uint32_t*)(b + 0x10) = r.below(3) ? 0 : r.u32(); *(uint32_t*)(b + 0x3C) = r.below(2) ? 0 : r.below(3); *(uint8_t*)(b + 0x38) = (uint8_t)r.below(2); }
        RFill((void*)0xA43F68, 64 * 4, r); RFill((void*)0xC3A1A0, 64, r);
        *(int32_t*)0x8D2E98 = (int32_t)r.u32();
    };
    auto Idx = [](Rng& r) { return r.below(8) == 0 ? (int)r.below(200) - 70 : (int)r.below(64); };
    MT(1557, OPEN_SEQUENCE_TASK, OpenSequenceTask, 15, [&](MCtx& c) { SeqFill(c); SeqWatch(c); c.code.Out(0); }, 3000);
    MT(1558, CLOSE_SEQUENCE_TASK, CloseSequenceTask, 15, [&](MCtx& c) { SeqFill(c); SeqWatch(c); c.code.I(Idx(*c.r)); });
    MT(1563, CLEAR_SEQUENCE_TASK, ClearSequenceTask, 15, [&](MCtx& c) { SeqFill(c); SeqWatch(c); c.code.I(Idx(*c.r)); });
    MT(1566, CLEAR_ATTRACTOR, ClearAttractor, 15, [&](MCtx& c) { SeqFill(c); SeqWatch(c); c.code.I(Idx(*c.r)); });
    MT(1565, ADD_ATTRACTOR, AddAttractor, 15, [&](MCtx& c) {
        auto& r = *c.r; SeqFill(c); SeqWatch(c);
        c.watch.push_back({ (void*)0xC3AB00, 64 * 0x40 }); c.watch.push_back({ (void*)0xC3A020, 64 * 4 });
        RFill((void*)0xC3AB00, 64 * 0x40, r);
        g_hostRet = (uint32_t)(r.below(8) == 0 ? (int)r.below(200) - 70 : (int)r.below(64));
        const float sc = r.below(3) ? 4000.f : 40000.f;
        c.code.F(GenS(r, 3000.f)).F(GenS(r, 3000.f)).F(GenS(r, 300.f)).F(GenS(r, sc)).F(GenS(r, sc)).I(Idx(r)).Out(0); }, 3000);
    MT(1586, REMOVE_GROUP, RemoveGroup, 15, [&](MCtx& c) {
        auto& r = *c.r; FillWorld(r); WatchAll(c);
        c.watch.push_back({ (void*)0xC098E0, 8 });
        RFill((void*)0xC098E0, 8, r); for (int i = 0; i < 8; ++i) ((uint8_t*)0xC098E0)[i] &= 1;
        for (int i = 0; i < 8; ++i) *(CPed**)(0xC09920 + i * 0x2D4 + 0x28) = r.below(3) == 0 ? nullptr : r.below(2) ? g_ped : g_ped2;
        c.code.I(r.below(8) == 0 ? (int)r.below(30) - 10 : (int)r.below(8)); });
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
    oracle::Patch(0x5433B0, (void*)&H_Skip);
    oracle::Patch(0x542800, (void*)&H_AddMoving);
    oracle::Patch(0x563280, (void*)&H_WorldRemove);
    oracle::Patch(0x563220, (void*)&H_WorldAdd);
    oracle::Patch(0x571B70, (void*)&H_RegRef);
    oracle::Patch(0x5E62B0, (void*)&H_ClearWeapon);
    oracle::Patch(0x450D70, (void*)&H_ReqNodes);
    oracle::Patch(0x44DD00, (void*)&H_RelNodes);
    oracle::Patch(0x45A980, (void*)&H_RecStart);
    oracle::Patch(0x45A280, (void*)&H_RecStop);
    oracle::Patch(0x459740, (void*)&H_RecPause);
    oracle::Patch(0x459850, (void*)&H_RecUnpause);
    oracle::Patch(0x4594C0, (void*)&H_RecIs);
    oracle::Patch(0x4839A0, (void*)&H_GAST);
    oracle::Patch(0x483720, (void*)&H_GNU);
    oracle::Patch(0x4637E0, (void*)&H_CleanAdd);
    oracle::Patch(0x4654B0, (void*)&H_CleanRemove);
    oracle::Patch(0x632C10, (void*)&H_Flush);
    oracle::Patch(0x6FA7C0, (void*)&H_AddFx);
    oracle::Patch(0x5FB870, (void*)&H_RemoveGroup);
    oracle::Patch(0x5FB8A0, (void*)&H_RemoveFollowers);
    std::printf("script_oracle_g13_15_test: %d cases per command and precision mode; PC24 = game mode, PC53 = CRT default\n", g_cases);
    TestScriptCommands();
    SetupFakeWorld();
    static CRunningScript dummy; (void)dummy;
    TestHelpers();
    TestMemCommands();
    int hard24 = 0;
    for (auto& r : g_rows) hard24 += r.hardReg24 + r.hardSpec24;
    std::printf("\n%zu commands, PC24 mismatches (excluding NaN-payload-only): %d\n", g_rows.size(), hard24);
    return hard24 ? 1 : 0;
}

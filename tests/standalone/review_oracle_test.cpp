// REVIEW-ORACLE: differential test of the GAME functions whose InjectHooks() was enabled in 3e73083e (CHud, CMaths, CTaskTimer, CTaskSequences, CIdleCam, CCoverPoint, CConversations, CCredits, CVehicleSaveStructure) and of CPathFind::FindNodePairClosestToCoors.
// Same machinery as game_oracle_test.cpp (see there) -- original header follows.
// P2E: differential test of GAME functions against the original machine code (exe oracle, see game_oracle.h).
// The REAL game TUs (CCollision, CGeneral, CVector, CMatrix, CQuaternion, CAnimBlendNode) are compiled into this exe with the game's flags (x87,
// /arch:IA32) and PCH; the test maps gta_sa_compact.exe (env RW_EXE_ORACLE=<path>) at its original VAs and calls the exe function and the C++ port with the
// same random inputs (fixed seeds, NaN / +-0 / denormal / huge / tiny values included) and compares the results BIT-EXACT, once with the x87 precision
// control at 24 bits (the game runs like that after D3D CreateDevice) and once at 53 bits (the CRT default).
// usage: game_oracle_test.exe [-v] [-v53] [-vn (also NaN-payload-only)] [-n cases] [name-substring ...]     (exit code 0 = no real mismatch at PC=24; NaN-payload-only differences and PC=53 results are only reported)
#include "game_oracle.h"

#include "General.h"
#include "Matrix.h"
#include "Hud.h"
#include "IdleCam.h"
#include "Cam.h"
#include "PathFind.h"
#include "CoverPoint.h"
#include "Conversations/Conversations.h"
#include "Credits.h"
#include "Tasks/TaskTimer.h"
#include "Tasks/TaskSequences.h"
#include "Maths.h"
#include "VehicleSaveStructure.h"
#include "HandShaker.h"
#include "Ragdoll/BoneNode.h"
#include "Entity/Ped/Ped.h"
#include "Cam.h"
#include "Camera.h"
#include "Radar.h"
#include "MenuManager.h"
#include "PostEffects.h"
#include "Attractors/PedShelterAttractor.h"
#include "Curves.h"
#include "TrainNode.h"
#include "GridRef.h"
#include "Audio/AEAudioUtility.h"
#include "Font.h"
#include "TimeCycle.h"
#include "Entity/Vehicle/Train.h"
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

// ---------------------------------------------------------------------------------------------------------------------------------
// extra stubs / setup for this test
#pragma init_seg(compiler)   // run BEFORE the static initialisers of the game TUs (CMaths fills its sine table at 0xBB3E00 during static init, which is inside the not yet RWX pad image)
struct EarlyUnprotect { EarlyUnprotect() { DWORD o; VirtualProtect(reinterpret_cast<void*>(0x401000), 0xCB0000 - 0x401000, PAGE_EXECUTE_READWRITE, &o); } } g_earlyUnprotect;

CPlayerInfo& FindPlayerInfo(int32 playerId) { return CWorld::Players[playerId < 0 ? CWorld::PlayerInFocus : playerId]; }   // World.cpp is not part of this test (same as the exe: Players[PlayerInFocus])
void CHandShaker::Process(float) {}                                                                                         // HandShaker.cpp is not part of this test: the exe's version is patched to a no-op as well
static void __fastcall HostNoopProcess(void*, void*, float) {}

static float g_portSinTab[256];

static std::vector<uint8_t> Snap(unsigned va, size_t n) { std::vector<uint8_t> v(n); std::memcpy(v.data(), reinterpret_cast<void*>(va), n); return v; }
static void Put(unsigned va, const std::vector<uint8_t>& v) { std::memcpy(reinterpret_cast<void*>(va), v.data(), v.size()); }
static std::vector<uint8_t> RandBytes(Rng& r, size_t n) { std::vector<uint8_t> v(n); for (auto& b : v) b = (uint8_t)r.u32(); return v; }
static bool SameMem(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b, const char* what, std::string& d) {
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i] != b[i]) { char t[96]; std::snprintf(t, sizeof t, "%s differs at +0x%zX: got %02X exe %02X", what, i, a[i], b[i]); d += t; return false; }
    }
    return true;
}
template<class T> static T* PtrAt(unsigned va) { return reinterpret_cast<T*>(static_cast<uintptr_t>(va)); }

static constexpr unsigned HUD_BASE = 0xBAA380, HUD_SIZE = 0xF80;   // CHud globals (0xBAA388 .. 0xBAB230)

// ---------------------------------------------------------------------------------------------------------------------------------
static void TestMaths() {
    // the sine table: static-init (port, snapshot taken before Map) vs InitMathsTables (exe 0x59AC90)
    {
        oracle::Fn<void __cdecl()>(0x59AC90)();
        int bad = 0; std::string first;
        for (int i = 0; i < 256; ++i) {
            if (!SameBits(FBits(g_portSinTab[i]), FBits(PtrAt<float>(0xBB3E00)[i]))) { if (!bad++) first = "entry " + std::to_string(i) + " got " + F(g_portSinTab[i]) + " exe " + F(PtrAt<float>(0xBB3E00)[i]); }
        }
        std::printf("%-58s   256 entries: %d mismatches%s%s\n", "CMaths sine table (static init vs 0x59AC90)", bad, bad ? " first: " : "", first.c_str());
        Row row; row.name = "CMaths sine table"; row.cases = 256; row.bad24 = row.hardReg24 = bad; g_rows.push_back(row);
    }
    // from here on both sides read the exe's table
    auto gen = [](Rng& r) { return r.below(5) == 0 ? GenF(r, 1e7f) : GenF(r, PickScale(r) * 10.f); };
    Run("CMaths::GetSinFast 0x4A1340", [&](Rng& r, std::string& d) {
        const float x = gen(r);
        const float a = CMaths::GetSinFast(x), b = oracle::Fn<float __cdecl(float)>(0x4A1340)(x);
        if (SameF(a, b)) return true;
        d = "in " + F(x) + " got " + F(a) + " exe " + F(b); return false;
    });
    Run("CMaths::GetCosFast 0x4A1360", [&](Rng& r, std::string& d) {
        const float x = gen(r);
        const float a = CMaths::GetCosFast(x), b = oracle::Fn<float __cdecl(float)>(0x4A1360)(x);
        if (SameF(a, b)) return true;
        d = "in " + F(x) + " got " + F(a) + " exe " + F(b); return false;
    });
}

// ---------------------------------------------------------------------------------------------------------------------------------
static void TestTasks() {
    Run("CTaskTimer::IsOutOfTime 0x420E30", [](Rng& r, std::string& d) {
        const uint32 now = r.below(4) ? r.u32() : (r.below(2) ? 0xFFFFFFF0u + r.below(16) : r.below(40));
        *PtrAt<uint32>(0xB7CB84) = now;
        CTaskTimer a, b;
        a.m_nStartTime = r.below(3) ? now - r.below(200000) + 100000 : r.u32();
        a.m_nInterval  = r.below(3) ? (int32)r.below(100000) - 50000 : (int32)r.u32();
        a.m_bStarted   = r.below(5) != 0; a.m_bStopped = r.below(3) == 0;
        b = a;
        const bool ra = a.IsOutOfTime();
        const bool rb = oracle::Fn<uint32 __fastcall(CTaskTimer*, int)>(0x420E30)(&b, 0) & 0xFF;
        if (ra == rb && !std::memcmp(&a, &b, 10)) return true;
        d = "now " + std::to_string(now) + " got " + std::to_string(ra) + " exe " + std::to_string(rb); return false;
    });
    Run("CTaskSequences::GetAvailableSlot 0x632E00", [](Rng& r, std::string& d) {
        const int mode = r.below(3);
        for (int i = 0; i < 64; ++i) {
            PtrAt<uint8>(0xC17898)[i] = mode == 0 ? (uint8)(r.below(2)) : (uint8)(r.below(8) == 0);
            *PtrAt<uint32>(0xC17900 + i * 0x40) = (mode == 1 ? r.below(3) != 0 : r.below(2)) ? (uint32)r.u32() | 1u : 0u;   // m_Tasks[0] of sequence i
        }
        const uint32 slot = r.below(3) ? r.below(2) : r.u32();
        const int32 a = CTaskSequences::GetAvailableSlot((uint8)slot);
        const int32 b = oracle::Fn<int32 __cdecl(uint32)>(0x632E00)(slot);
        if (a == b) return true;
        d = "slot " + std::to_string(slot) + " got " + std::to_string(a) + " exe " + std::to_string(b); return false;
    });
}

// ---------------------------------------------------------------------------------------------------------------------------------
static void TestHud() {
    Run("CHud::GetYPosBasedOnHealth 0x588B60", [](Rng& r, std::string& d) {
        const uint32 pid = r.below(2);
        CWorld::Players[pid].m_nMaxHealth = (uint8)(r.below(3) ? 100 + r.below(5) : r.u32());
        RsGlobal.maximumHeight = 1 + (int32)r.below(r.below(2) ? 1500 : 9000);
        const float pos = GenF(r, PickScale(r) * 100.f);
        const int8 off = (int8)r.u32();
        const float a = CHud::GetYPosBasedOnHealth((uint8)pid, pos, off);
        const float b = oracle::Fn<float __cdecl(uint32, float, int32)>(0x588B60)(pid, pos, (int32)off);
        if (SameF(a, b)) return true;
        d = "pid " + std::to_string(pid) + " maxHealth " + std::to_string(CWorld::Players[pid].m_nMaxHealth) + " H " + std::to_string(RsGlobal.maximumHeight) + " pos " + F(pos) + " off " + std::to_string(off) + " got " + F(a) + " exe " + F(b);
        return false;
    });

    // calls fa (port) and fb (exe) on the same randomised HUD globals, compares the whole HUD global area afterwards
    const auto Both = [](Rng& r, std::string& d, const auto& fa, const auto& fb, bool floatsInBigMsgX) {
        auto init = RandBytes(r, HUD_SIZE);
        if (floatsInBigMsgX) { // BigMessageX[7] at 0xBAA3DC: 0 / NaN / value
            for (int i = 0; i < 7; ++i) { const float v = r.below(3) ? 0.f : (r.below(5) == 0 ? BitsF(0x7FC00000) : GenF(r, 100.f)); std::memcpy(&init[0xBAA3DC - HUD_BASE + i * 4], &v, 4); }
        }
        for (unsigned va : { 0xBAA3F8u, 0xBAA3F9u, 0xBAA3FAu, 0xBAA3FBu }) init[va - HUD_BASE] &= 1;   // bools
        const auto init2 = std::vector<uint8_t>{ init };
        const uint32 pf = r.below(2); *PtrAt<uint8>(0xB7CD74) = (uint8)pf;
        const auto info = RandBytes(r, 0x190); Put(0xB7CD98 + pf * 0x190, info);
        const uint32 now = r.u32(); *PtrAt<uint32>(0xB7CB84) = now;
        const auto extra = Snap(0x8D0934, 8);
        Put(HUD_BASE, init); fa();
        auto A = Snap(HUD_BASE, HUD_SIZE); const auto Ax = Snap(0x8D0934, 8);
        Put(0x8D0934, extra); Put(HUD_BASE, init2); fb();
        auto B = Snap(HUD_BASE, HUD_SIZE); const auto Bx = Snap(0x8D0934, 8);
        for (unsigned o = 0xBAA401 - HUD_BASE; o < 0xBAA404 - HUD_BASE; ++o) A[o] = B[o];   // padding behind bDrawClock: the exe stores a dword, the port a bool
        return SameMem(A, B, "HUD globals", d) && SameMem(Ax, Bx, "0x8D0934", d);
    };

    Run("CHud::ReInitialise 0x588880", [&](Rng& r, std::string& d) {
        return Both(r, d, [] { CHud::ReInitialise(); }, [] { oracle::Fn<void __cdecl()>(0x588880)(); }, true);
    });
    Run("CHud::ResetWastedText 0x589070", [&](Rng& r, std::string& d) {
        return Both(r, d, [] { CHud::ResetWastedText(); }, [] { oracle::Fn<void __cdecl()>(0x589070)(); }, true);
    });
    Run("CHud::GetRidOfAllHudMessages 0x588A50", [&](Rng& r, std::string& d) {
        const uint32 arg = r.below(2);
        return Both(r, d, [&] { CHud::GetRidOfAllHudMessages(arg != 0); }, [&] { oracle::Fn<void __cdecl(uint32)>(0x588A50)(arg); }, true);
    });
    const auto MakeMsg = [](Rng& r, size_t maxLen) {
        std::vector<uint8_t> m(maxLen + 8, 0);
        const size_t len = r.below(4) ? r.below(60) : (r.below(2) ? 380 + r.below(60) : 120 + r.below(20));
        for (size_t i = 0; i < len && i < maxLen + 4; ++i) m[i] = (uint8_t)(1 + r.below(255));
        return m;
    };
    Run("CHud::SetMessage 0x588F60", [&](Rng& r, std::string& d) {
        auto m1 = MakeMsg(r, 500), m2 = m1;
        const bool null = r.below(12) == 0;
        return Both(r, d, [&] { CHud::SetMessage(null ? nullptr : m1.data()); }, [&] { oracle::Fn<void __cdecl(const uint8*)>(0x588F60)(null ? nullptr : m2.data()); }, true) && m1 == m2;
    });
    Run("CHud::SetBigMessage 0x588FC0", [&](Rng& r, std::string& d) {
        auto m1 = MakeMsg(r, 200), m2 = m1;
        const uint32 style = r.below(7);
        const bool ok = Both(r, d, [&] { CHud::SetBigMessage(m1.data(), (eMessageStyle)style); }, [&] { oracle::Fn<void __cdecl(uint8*, uint32)>(0x588FC0)(m2.data(), style); }, true);
        if (!ok) { d += " (style " + std::to_string(style) + ")"; return false; }
        if (m1 != m2) { d = "source message differs after the call (style " + std::to_string(style) + ")"; return false; }
        return true;
    });
}

// ---------------------------------------------------------------------------------------------------------------------------------
static void TestMisc() {
    Run("CCredits::PrintCreditSpace 0x5A87C0", [](Rng& r, std::string& d) {
        const float sp = r.below(3) == 0 ? (r.below(2) ? 1.5f : 0.5f) : GenF(r, PickScale(r) * 4.f);
        uint32 a = r.below(3) ? r.below(100000) : (r.u32() >> 1), b = a;   // (the exe rounds positions >= 2^31 to float precision first: _ftol of an unsigned in x87)
        CCredits::PrintCreditSpace(sp, a);
        oracle::Fn<void __cdecl(float, uint32*)>(0x5A87C0)(sp, &b);
        if (a == b) return true;
        d = "space " + F(sp) + " got " + std::to_string(a) + " exe " + std::to_string(b); return false;
    });
    Run("CCoverPoint::CanAccommodateAnotherPed 0x698E70", [](Rng& r, std::string& d) {
        alignas(16) uint8 blob[0x1C]{};
        blob[0] = (uint8)(r.below(3) ? r.below(4) : r.u32());
        for (unsigned o : { 0x14u, 0x18u }) *reinterpret_cast<uint32*>(blob + o) = r.below(2) ? 0u : (uint32)r.u32() | 0x10000u;
        const bool a = reinterpret_cast<const CCoverPoint*>(blob)->CanAccommodateAnotherPed();
        const bool b = oracle::Fn<uint32 __fastcall(void*, int)>(0x698E70)(blob, 0) & 0xFF;
        if (a == b) return true;
        d = "type " + std::to_string((int8)blob[0]) + " got " + std::to_string(a) + " exe " + std::to_string(b); return false;
    });
    Run("CConversations::IsConversationGoingOn 0x43AAC0", [](Rng& r, std::string& d) {
        for (int i = 0; i < 14; ++i) *PtrAt<uint32>(0x9691D8 + i * 0x1C + 0x14) = r.below(4) == 0 ? (uint32)r.below(4) : 0u;
        if (r.below(3) == 0) for (int i = 0; i < 14; ++i) *PtrAt<uint32>(0x9691D8 + i * 0x1C + 0x14) = 0;
        const bool a = CConversations::IsConversationGoingOn();
        const bool b = oracle::Fn<uint32 __cdecl()>(0x43AAC0)() & 0xFF;
        if (a == b) return true;
        d = "got " + std::to_string(a) + " exe " + std::to_string(b); return false;
    });
    Run("CVehicleSaveStructure::Construct 0x5D2330", [](Rng& r, std::string& d) {
        alignas(16) uint8 veh[0x5C0]{};
        alignas(16) uint8 mat[0x48]{};
        const auto rb = RandBytes(r, sizeof veh); std::memcpy(veh, rb.data(), sizeof veh);
        const auto mb = RandBytes(r, sizeof mat); std::memcpy(mat, mb.data(), sizeof mat);
        *reinterpret_cast<uint32*>(mat + 0x40) = 0;                       // no attached RwMatrix (otherwise 0x59AD70 would run)
        *reinterpret_cast<uint32*>(mat + 0x44) = 0;
        *reinterpret_cast<void**>(veh + 0x14) = mat;                      // m_matrix
        alignas(16) uint8 a[0x68]{}, b[0x68]{};
        std::memset(a, 0x5A, sizeof a); std::memset(b, 0x5A, sizeof b);
        *reinterpret_cast<uint32*>(a + 0x40) = *reinterpret_cast<uint32*>(b + 0x40) = 0;   // destination matrix: no attached RwMatrix (m_pRwMatrix, 0x59BBC0 would call 0x59AD70)
        *reinterpret_cast<uint32*>(a + 0x44) = *reinterpret_cast<uint32*>(b + 0x44) = 0;
        for (unsigned o : { 0x494u, 0x49Cu, 0x4A0u }) { const float v = GenF(r, 1.f, S_ZERO | S_TINY); std::memcpy(veh + o, &v, 4); }   // (float members are copied as dwords by the exe, a float copy may quiet a signalling NaN)
        reinterpret_cast<CVehicleSaveStructure*>(a)->Construct(reinterpret_cast<CVehicle*>(veh));
        oracle::Fn<void __fastcall(void*, int, void*)>(0x5D2330)(b, 0, veh);
        std::string dd;
        for (size_t i = 0; i < sizeof a; ++i) if (a[i] != b[i]) { d = "differs at +0x" + std::to_string(i); return false; }
        return true;
    });
}

// ---------------------------------------------------------------------------------------------------------------------------------
// CIdleCam
static_assert(offsetof(CCam, m_vecSource) == 0x19C && offsetof(CCam, m_vecFront) == 0x190 && offsetof(CCam, m_vecUp) == 0x1B4);
static void MakeEntity(Rng& r, uint8* e, uint8* mat, bool ped) {
    std::memset(e, 0, 0x7A0); std::memset(mat, 0, 0x48);
    const CVector pos = GenV(r, 300.f, S_NAN | S_ZERO | S_TINY);
    if (r.below(3)) { *reinterpret_cast<void**>(e + 0x14) = mat; std::memcpy(mat + 0x30, &pos, 12); } else std::memcpy(e + 4, &pos, 12);
    e[0x36] = (uint8)((ped ? 3 : r.below(8)) | (r.u32() & 0xF8));
    *reinterpret_cast<uint32*>(e + 0x598) = r.below(2) ? (r.below(2) ? 5 : 22) : r.below(30);
}
static void TestIdleCam() {
    Run("CIdleCam::GetLookAtPositionOnTarget 0x50EAE0", [](Rng& r, std::string& d) {
        alignas(16) uint8 e[0x7A0], m[0x48];
        MakeEntity(r, e, m, r.below(3) != 0);
        CVector a{ 1, 2, 3 }, b{ 1, 2, 3 };
        gIdleCam.GetLookAtPositionOnTarget(reinterpret_cast<CEntity*>(e), a);
        oracle::Fn<void __fastcall(CIdleCam*, int, CEntity*, CVector*)>(0x50EAE0)(&gIdleCam, 0, reinterpret_cast<CEntity*>(e), &b);
        if (SameV(a, b)) return true;
        d = "got " + V(a) + " exe " + V(b); return false;
    });
    Run("CIdleCam::ProcessSlerp 0x5179E0", [](Rng& r, std::string& d) {
        alignas(16) uint8 e[0x7A0], m[0x48], cam[0x238]{}, idle[sizeof(CIdleCam)]{};
        MakeEntity(r, e, m, r.below(3) != 0);
        auto* c = reinterpret_cast<CIdleCam*>(idle);
        static const float scales[] = { 1.f, 10.f, 200.f, 3000.f, 1e5f, 1e-3f };   // (positions beyond ~1e6 do not occur in the game; the exe misbehaves for 1e18+ vectors)
        const float sc = scales[r.below(6)];
        *PtrAt<uint32>(0xB7CB84) = r.below(4) ? r.below(500000) : r.u32();
        const float now = (float)*PtrAt<uint32>(0xB7CB84);
        c->m_Target = reinterpret_cast<CEntity*>(e);
        c->m_PositionToSlerpFrom = GenV(r, sc); c->m_LastIdlePos = GenV(r, sc);
        c->m_TimeLastTargetSelected = now - (r.below(4) ? r.f01() * 4000.f : GenF(r, 1e5f));
        c->m_SlerpDuration = r.below(8) ? 2000.f : GenF(r, 5000.f);
        c->m_TargetLOSFramestoReject = 14; c->m_TargetLOSCounter = (int32)r.below(30);
        c->m_Cam = reinterpret_cast<CCam*>(cam);
        const CVector src = GenV(r, sc); std::memcpy(cam + 0x19C, &src, 12);
        alignas(16) uint8 idle2[sizeof(CIdleCam)]; std::memcpy(idle2, idle, sizeof idle);
        float ax = -1, az = -1, bx = -1, bz = -1;
        const float ra = reinterpret_cast<CIdleCam*>(idle)->ProcessSlerp(ax, az);
        const float rb = oracle::Fn<float __fastcall(CIdleCam*, int, float*, float*)>(0x5179E0)(reinterpret_cast<CIdleCam*>(idle2), 0, &bx, &bz);
        if (SameF(ra, rb) && SameF(ax, bx) && SameF(az, bz)) return true;
        d = "got " + F(ra) + "," + F(ax) + "," + F(az) + " exe " + F(rb) + "," + F(bx) + "," + F(bz) + " src " + V(src) + " from " + V(c->m_PositionToSlerpFrom) + " los " + std::to_string(c->m_TargetLOSCounter);
        return false;
    });
    Run("CIdleCam::FinaliseIdleCamera 0x50E760", [](Rng& r, std::string& d) {
        alignas(16) uint8 cam1[0x238]{}, idle1[sizeof(CIdleCam)]{};
        const auto cb = RandBytes(r, 0x238); std::memcpy(cam1, cb.data(), 0x238);
        for (unsigned o = 0x190; o < 0x1C0; o += 4) { const float v = GenF(r, 5.f, S_ZERO | S_TINY); std::memcpy(cam1 + o, &v, 4); }
        alignas(16) uint8 cam2[0x238], idle2[sizeof(CIdleCam)]{};
        std::memcpy(cam2, cam1, 0x238);
        reinterpret_cast<CIdleCam*>(idle1)->m_Cam = reinterpret_cast<CCam*>(cam1);
        reinterpret_cast<CIdleCam*>(idle2)->m_Cam = reinterpret_cast<CCam*>(cam2);
        reinterpret_cast<CIdleCam*>(idle1)->m_DegreeShakeIdleCam = reinterpret_cast<CIdleCam*>(idle2)->m_DegreeShakeIdleCam = r.below(4) ? 1.f : GenF(r, 3.f);
        auto hs = Snap(0xB6ECA0, 0x94);
        for (unsigned o = 0; o < 0x94; o += 4) { const float v = (o >= 0x64 && o < 0x74) ? 0.f : GenF(r, 1.f, S_ZERO); std::memcpy(&hs[o], &v, 4); }
        Put(0xB6ECA0, hs);
        const float X = GenF(r, 3.2f, S_ZERO), Y = GenF(r, 6.4f, S_ZERO), sh = r.below(3) ? r.f01() : GenF(r, 2.f);
        reinterpret_cast<CIdleCam*>(idle1)->FinaliseIdleCamera(X, Y, sh);
        oracle::Fn<void __fastcall(CIdleCam*, int, float, float, float)>(0x50E760)(reinterpret_cast<CIdleCam*>(idle2), 0, X, Y, sh);
        if (SameBlob(cam1 + 0x190, cam2 + 0x190, 0x30) && SameBlob(idle1, idle2, 0x98)) return true;   // (m_Cam differs by construction)
        d = "lastIdle got " + V(reinterpret_cast<CIdleCam*>(idle1)->m_LastIdlePos) + " exe " + V(reinterpret_cast<CIdleCam*>(idle2)->m_LastIdlePos) + " cam-diff " + std::to_string(!SameBlob(cam1 + 0x190, cam2 + 0x190, 0x30)) + " args " + F(X) + "," + F(Y) + "," + F(sh) + " front got " + V(*reinterpret_cast<CVector*>(cam1 + 0x190)) + " exe " + V(*reinterpret_cast<CVector*>(cam2 + 0x190)) + " up got " + V(*reinterpret_cast<CVector*>(cam1 + 0x1B4)) + " exe " + V(*reinterpret_cast<CVector*>(cam2 + 0x1B4));
        return false;
    });
    Run("CIdleCam::ProcessIdleCamTicker 0x50A200", [](Rng& r, std::string& d) {
        alignas(16) uint8 i1[sizeof(CIdleCam)]{}, i2[sizeof(CIdleCam)]{};
        const uint32 touched = r.u32(); *PtrAt<uint32>(reinterpret_cast<uintptr_t>(&CPad::GetPad(0)->LastTimeTouched)) = r.below(2) ? touched : r.u32();
        auto* c1 = reinterpret_cast<CIdleCam*>(i1); c1->m_LastTimePadTouched = (int32)touched; c1->m_IdleTickerFrames = (int32)r.below(200000);
        std::memcpy(i2, i1, sizeof i1);
        CTimer::ms_fTimeStep = r.below(3) ? r.f01() * 6.f : GenF(r, 1e6f);
        c1->ProcessIdleCamTicker();
        oracle::Fn<void __fastcall(CIdleCam*, int)>(0x50A200)(reinterpret_cast<CIdleCam*>(i2), 0);
        if (!std::memcmp(i1, i2, sizeof i1)) return true;
        d = "ts " + F(CTimer::ms_fTimeStep) + " got ticker " + std::to_string(c1->m_IdleTickerFrames) + " exe " + std::to_string(reinterpret_cast<CIdleCam*>(i2)->m_IdleTickerFrames); return false;
    });
}

// ---------------------------------------------------------------------------------------------------------------------------------
// CPathFind::FindNodePairClosestToCoors on a fake node set
static void TestPath() {
    constexpr int AREAS = 4, NODES = 14, LINKS = 90;
    static std::vector<uint8_t> pathStore(sizeof(CPathFind) + 16);
    Run("CPathFind::FindNodePairClosestToCoors 0x44FEE0", [](Rng& r, std::string& d) {
        auto* paths = reinterpret_cast<CPathFind*>(pathStore.data());
        std::memset(pathStore.data(), 0, pathStore.size());
        static CPathNode nodes[AREAS][NODES];
        static CNodeAddress links[AREAS][LINKS];
        for (int a = 0; a < AREAS; ++a) {
            const bool loaded = r.below(10) != 0;
            paths->m_pPathNodes[a] = loaded ? nodes[a] : nullptr;
            paths->m_pNodeLinks[a] = links[a];
            paths->m_anNumNodes[a] = NODES;
            paths->m_anNumVehicleNodes[a] = r.below(NODES + 1);
            const float world = r.below(3) ? 120.f : 2000.f;
            for (int n = 0; n < NODES; ++n) {
                auto& nd = nodes[a][n];
                std::memset(&nd, 0, sizeof nd);
                auto* raw = reinterpret_cast<uint8_t*>(&nd);
                for (int o = 0; o < 6; ++o) raw[8 + o] = (uint8_t)r.u32();
                const int16 x = (int16)((r.f01() * 2 - 1) * world * 8), y = (int16)((r.f01() * 2 - 1) * world * 8), z = (int16)((r.f01() * 2 - 1) * 80);
                std::memcpy(raw + 8, &x, 2); std::memcpy(raw + 10, &y, 2); std::memcpy(raw + 12, &z, 2);
                const int16 base = (int16)(r.below(LINKS - 17)); std::memcpy(raw + 0x10, &base, 2);
                raw[0x18] = (uint8_t)((r.below(16) & 0xF) | (r.below(5) == 0 ? 0x20 : 0) | (r.below(6) == 0 ? 0x80 : 0) | ((r.u32() & 0x50)));   // links, switched off (0x20), water (0x80)
                if (r.below(8) == 0) raw[0x18] &= 0xF0;
            }
            for (int l = 0; l < LINKS; ++l) { links[a][l].m_wAreaId = (uint16)r.below(AREAS + (r.below(20) == 0 ? 0 : 0)); links[a][l].m_wNodeId = (uint16)r.below(NODES); }
        }
        CVector pos = GenV(r, r.below(3) ? 150.f : 2500.f, S_NAN | S_ZERO | S_TINY);
        pos.z = GenF(r, 100.f, S_NAN | S_ZERO);
        const uint32 nodeType = r.below(4) == 0 ? 1 : 0;
        const float minDist = r.below(4) ? (float)r.below(40) : GenF(r, 80.f, S_NAN | S_ZERO);
        const float maxDist = r.below(5) == 0 ? GenF(r, 9000.f, S_NAN | S_ZERO | S_TINY) : (float)(5 + r.below(r.below(2) ? 600 : 9990));
        const uint32 low = r.below(2), unusedFlag = r.below(2), boats = r.below(4) == 0;
        CNodeAddress f1{ 0x1111, 0x2222 }, s1{ 0x3333, 0x4444 }, f2 = f1, s2 = s1;
        float dist1 = BitsF(0xDEADBEEF), dist2 = dist1;
        paths->FindNodePairClosestToCoors(pos, (uint8)nodeType, &f1, &s1, &dist1, minDist, maxDist, low != 0, unusedFlag != 0, boats != 0);
        oracle::Fn<void __fastcall(CPathFind*, int, float, float, float, uint32, CNodeAddress*, CNodeAddress*, float*, float, float, uint32, uint32, uint32)>(0x44FEE0)(paths, 0, pos.x, pos.y, pos.z, nodeType, &f2, &s2, &dist2, minDist, maxDist, low, unusedFlag, boats);
        Hit(f1.m_wAreaId != 0xFFFF);
        if (!std::memcmp(&f1, &f2, 4) && !std::memcmp(&s1, &s2, 4) && SameF(dist1, dist2)) return true;
        d = "pos " + V(pos) + " type " + std::to_string(nodeType) + " min " + F(minDist) + " max " + F(maxDist) + " low " + std::to_string(low) + " boats " + std::to_string(boats) +
            " got " + std::to_string(f1.m_wAreaId) + ":" + std::to_string(f1.m_wNodeId) + "/" + std::to_string(s1.m_wAreaId) + ":" + std::to_string(s1.m_wNodeId) + " " + F(dist1) +
            " exe " + std::to_string(f2.m_wAreaId) + ":" + std::to_string(f2.m_wNodeId) + "/" + std::to_string(s2.m_wAreaId) + ":" + std::to_string(s2.m_wNodeId) + " " + F(dist2);
        return false;
    });
}

// ---------------------------------------------------------------------------------------------------------------------------------
// common.h fidelity (SCREEN_STRETCH / SCREEN_SCALE macros, PI family, deg/rad factors): functions whose result depends on them, port vs exe
// BoneNode_c::QuatToEuler / EulerToQuat are private statics: explicit instantiation may name them
using BoneQuatToEulerFn = void (*)(const RtQuat&, CVector&);
using BoneEulerToQuatFn = void (*)(const CVector&, RtQuat&);
template<BoneQuatToEulerFn P> struct HoldQuatToEuler { friend BoneQuatToEulerFn StealQuatToEuler() { return P; } };
template<BoneEulerToQuatFn P> struct HoldEulerToQuat { friend BoneEulerToQuatFn StealEulerToQuat() { return P; } };
template struct HoldQuatToEuler<&BoneNode_c::QuatToEuler>;
template struct HoldEulerToQuat<&BoneNode_c::EulerToQuat>;
BoneQuatToEulerFn StealQuatToEuler();
BoneEulerToQuatFn StealEulerToQuat();

static void TestFidelity() {
    const auto GenDim = [](Rng& r, bool width) {   // 640x448 is the special case of CMenuManager::Stretch*, the rest are arbitrary resolutions
        if (r.below(5) == 0) return width ? 640 : 448;
        static const int w[] = { 800, 1024, 1280, 1366, 1600, 1920, 2560, 3840, 7, 101 }, h[] = { 600, 768, 720, 1050, 1080, 1440, 2160, 5, 333 };
        return r.below(3) ? (width ? w[r.below(10)] : h[r.below(9)]) : 1 + (int)r.below(width ? 5000 : 3000);
    };
    const auto SetRes = [&](Rng& r) { RsGlobal.maximumWidth = GenDim(r, true); RsGlobal.maximumHeight = GenDim(r, false); };

    Run("CMenuManager::StretchX 0x5733E0", [&](Rng& r, std::string& d) {
        SetRes(r);
        const float x = GenF(r, PickScale(r) * 100.f);
        const float a = FrontEndMenuManager.StretchX(x), b = oracle::Fn<float __fastcall(CMenuManager*, int, float)>(0x5733E0)(&FrontEndMenuManager, 0, x);
        if (SameF(a, b)) return true;
        d = "W " + std::to_string(RsGlobal.maximumWidth) + " x " + F(x) + " got " + F(a) + " exe " + F(b); return false;
    });
    Run("CMenuManager::StretchY 0x573410", [&](Rng& r, std::string& d) {
        SetRes(r);
        const float y = GenF(r, PickScale(r) * 100.f);
        const float a = FrontEndMenuManager.StretchY(y), b = oracle::Fn<float __fastcall(CMenuManager*, int, float)>(0x573410)(&FrontEndMenuManager, 0, y);
        if (SameF(a, b)) return true;
        d = "H " + std::to_string(RsGlobal.maximumHeight) + " y " + F(y) + " got " + F(a) + " exe " + F(b); return false;
    });
    Run("CRadar::LimitToMap 0x583350", [&](Rng& r, std::string& d) {
        SetRes(r);
        FrontEndMenuManager.m_bMapLoaded = r.below(2) != 0;
        FrontEndMenuManager.m_fMapZoom   = std::fabs(GenF(r, r.below(2) ? 400.f : 32000.f, S_NAN | S_ZERO | S_TINY));   // (non-negative: std::clamp asserts lo <= hi in the debug STL)
        FrontEndMenuManager.m_vMapOrigin = { GenF(r, 400.f, S_NAN | S_ZERO | S_TINY), GenF(r, 400.f, S_NAN | S_ZERO | S_TINY) };
        float x1 = GenF(r, 1500.f, S_NAN | S_ZERO | S_TINY), y1 = GenF(r, 1500.f, S_NAN | S_ZERO | S_TINY), x2 = x1, y2 = y1;
        CRadar::LimitToMap(x1, y1);
        oracle::Fn<void __cdecl(float*, float*)>(0x583350)(&x2, &y2);
        if (SameF(x1, x2) && SameF(y1, y2)) return true;
        d = "W " + std::to_string(RsGlobal.maximumWidth) + " H " + std::to_string(RsGlobal.maximumHeight) + " zoom " + F(FrontEndMenuManager.m_fMapZoom) + " in " + F(x2) + "," + F(y2) + " got " + F(x1) + "," + F(y1);
        return false;
    });
    Run("CRadar::TransformRadarPointToScreenSpace 0x583480", [&](Rng& r, std::string& d) {
        SetRes(r);
        FrontEndMenuManager.m_bDrawingMap = r.below(4) == 0;
        FrontEndMenuManager.m_fMapZoom    = GenF(r, 400.f, S_NAN | S_ZERO | S_TINY);
        FrontEndMenuManager.m_vMapOrigin  = { GenF(r, 400.f, S_NAN | S_ZERO | S_TINY), GenF(r, 400.f, S_NAN | S_ZERO | S_TINY) };
        const CVector2D in{ GenF(r, r.below(2) ? 1.5f : 300.f, S_NAN | S_ZERO | S_TINY), GenF(r, r.below(2) ? 1.5f : 300.f, S_NAN | S_ZERO | S_TINY) };
        const CVector2D a = CRadar::TransformRadarPointToScreenSpace(in);
        CVector2D b{ BitsF(0xDEADBEEF), BitsF(0xDEADBEEF) };
        oracle::Fn<void __cdecl(CVector2D*, const CVector2D*)>(0x583480)(&b, &in);
        if (SameF(a.x, b.x) && SameF(a.y, b.y)) return true;
        d = "W " + std::to_string(RsGlobal.maximumWidth) + " H " + std::to_string(RsGlobal.maximumHeight) + " map " + std::to_string(FrontEndMenuManager.m_bDrawingMap) + " in " + F(in.x) + "," + F(in.y) + " got " + F(a.x) + "," + F(a.y) + " exe " + F(b.x) + "," + F(b.y);
        return false;
    });
    Run("CCamera::GetScreenRect 0x50AB50", [&](Rng& r, std::string& d) {
        SetRes(r);
        TheCamera.m_bWideScreenOn = r.below(2) != 0;
        TheCamera.m_fScreenReductionPercentage = r.below(3) ? r.f01() * 100.f : GenF(r, 300.f, S_NAN | S_ZERO | S_TINY);
        CRect a{ 1.f, 2.f, 3.f, 4.f }, b = a;
        TheCamera.GetScreenRect(&a);
        oracle::Fn<void __fastcall(CCamera*, int, CRect*)>(0x50AB50)(&TheCamera, 0, &b);
        if (SameBlob(&a, &b, sizeof(CRect))) return true;
        d = "W " + std::to_string(RsGlobal.maximumWidth) + " H " + std::to_string(RsGlobal.maximumHeight) + " wide " + std::to_string(TheCamera.m_bWideScreenOn) + " red " + F(TheCamera.m_fScreenReductionPercentage) +
            " got {" + F(a.left) + "," + F(a.bottom) + "," + F(a.right) + "," + F(a.top) + "} exe {" + F(b.left) + "," + F(b.bottom) + "," + F(b.right) + "," + F(b.top) + "}";
        return false;
    });
    Run("CPostEffects::HeatHazeFXInit 0x701450", [&](Rng& r, std::string& d) {
        // the whole block of globals it writes: hpS / hpY / hpX (0xC3F868 .. 0xC3FEA8) and the effect state (0xC402BC .. 0xC40314), + the type bookkeeping at 0x8D50E4..0x8D50F4
        constexpr unsigned LO = 0xC3F868, HI = 0xC40314, TLO = 0x8D50E4, THI = 0x8D50F4;
        static std::vector<uint8_t> rasterStore(0x100);
        std::memset(rasterStore.data(), 0, rasterStore.size());
        const int32 rw = 8 + (int32)r.below(2000), rh = 8 + (int32)r.below(2000);
        auto* raster = reinterpret_cast<RwRaster*>(rasterStore.data());
        raster->width = rw; raster->height = rh;                                       // the port's view of the raster
        std::memcpy(&rasterStore[0xC], &rw, 4); std::memcpy(&rasterStore[0x10], &rh, 4); // the exe's view (RwRaster: width +0xC, height +0x10)
        SetRes(r);
        const auto saveA = Snap(LO, HI - LO), saveB = Snap(TLO, THI - TLO);
        auto a = RandBytes(r, HI - LO); auto b = RandBytes(r, THI - TLO);
        const int32 type = (int32)r.below(5);   // the 5 cases of the jump table (the game only ever uses 0; anything else would feed garbage ranges to the 32 bit `max - min` of the random helpers)
        const int32 last = r.below(8) == 0 ? type : -1;
        const uint32 rasterPtr = (uint32)reinterpret_cast<uintptr_t>(rasterStore.data());
        std::memcpy(&a[0xC402BC - LO], &type, 4); std::memcpy(&a[0xC402D8 - LO], &rasterPtr, 4); std::memcpy(&b[0], &last, 4);
        const uint32 seed = r.u32();
        Put(LO, a); Put(TLO, b);
        SeedBoth(seed); CPostEffects::HeatHazeFXInit();
        const auto A1 = Snap(LO, HI - LO), B1 = Snap(TLO, THI - TLO);
        Put(LO, a); Put(TLO, b);
        SeedBoth(seed); oracle::Fn<void __cdecl()>(0x701450)();
        const auto A2 = Snap(LO, HI - LO), B2 = Snap(TLO, THI - TLO);
        Put(LO, saveA); Put(TLO, saveB);
        const bool ok = SameMem(A1, A2, "HeatHaze globals (0xC3F868..)", d) && SameMem(B1, B2, "HeatHaze globals (0x8D50E4..)", d);
        if (!ok) d += " (type " + std::to_string(type) + " W " + std::to_string(RsGlobal.maximumWidth) + " H " + std::to_string(RsGlobal.maximumHeight) + ")";
        return ok;
    });

    // PI family
    Run("BoneNode_c::QuatToEuler 0x617080", [&](Rng& r, std::string& d) {
        RtQuat q;
        const int mode = r.below(6);
        if (mode == 0) { q.imag.x = 0.f; q.imag.y = r.below(2) ? 0.5f : -0.5f; q.imag.z = 0.f; q.real = 1.f; }   // 2 * w * y == +-1 exactly: the gimbal-lock branch
        else if (mode == 1) { q.imag = { GenF(r, 1.f), GenF(r, 1.f), GenF(r, 1.f) }; q.real = GenF(r, 1.f); }
        else { const float sc = PickScale(r); q.imag = { GenF(r, sc), GenF(r, sc), GenF(r, sc) }; q.real = GenF(r, sc); }
        CVector a{ BitsF(0xDEADBEEF), BitsF(0xDEADBEEF), BitsF(0xDEADBEEF) }, b = a;
        StealQuatToEuler()(q, a);
        oracle::Fn<void __cdecl(const RtQuat*, CVector*)>(0x617080)(&q, &b);
        if (SameV(a, b)) return true;
        d = "q {" + F(q.imag.x) + "," + F(q.imag.y) + "," + F(q.imag.z) + "," + F(q.real) + "} got " + V(a) + " exe " + V(b); return false;
    });
    Run("BoneNode_c::EulerToQuat 0x6171F0", [&](Rng& r, std::string& d) {
        const CVector ang = GenV(r, r.below(3) ? 180.f : PickScale(r) * 100.f, S_NAN | S_ZERO | S_TINY);
        RtQuat a{}, b{};
        StealEulerToQuat()(ang, a);
        oracle::Fn<void __cdecl(const CVector*, RtQuat*)>(0x6171F0)(&ang, &b);
        if (SameBlob(&a, &b, sizeof(RtQuat))) return true;
        d = "angles " + V(ang) + " got {" + F(a.imag.x) + "," + F(a.imag.y) + "," + F(a.imag.z) + "," + F(a.real) + "} exe {" + F(b.imag.x) + "," + F(b.imag.y) + "," + F(b.imag.z) + "," + F(b.real) + "}"; return false;
    });
    Run("CPed::GetLocalDirection 0x5DEF60", [&](Rng& r, std::string& d) {
        static std::vector<uint8_t> pedStore(sizeof(CPed) + 16);
        auto* ped = reinterpret_cast<CPed*>(pedStore.data());
        ped->m_fCurrentRotation = r.below(3) ? (r.f01() * 2.f - 1.f) * 7.f : GenF(r, 1000.f, S_NAN | S_ZERO | S_TINY);
        const CVector2D pt{ GenF(r, PickScale(r), S_NAN | S_ZERO | S_TINY), GenF(r, PickScale(r), S_NAN | S_ZERO | S_TINY) };
        const int32 a = ped->GetLocalDirection(pt);
        const int32 b = oracle::Fn<int32 __fastcall(CPed*, int, const CVector2D*)>(0x5DEF60)(ped, 0, &pt);
        if (a == b) return true;
        d = "rot " + F(ped->m_fCurrentRotation) + " pt " + F(pt.x) + "," + F(pt.y) + " got " + std::to_string(a) + " exe " + std::to_string(b); return false;
    });
    Run("CCam::ClipBeta 0x509C50", [&](Rng& r, std::string& d) {
        static std::vector<uint8_t> camStore(sizeof(CCam) + 16);
        auto* cam = reinterpret_cast<CCam*>(camStore.data());
        const float v = r.below(4) == 0 ? ((r.below(2) ? 1.f : -1.f) * (std::numbers::pi_v<float> + (float)((int)r.below(5) - 2) * 1e-6f * (float)r.below(8))) : r.below(2) ? (r.f01() * 2.f - 1.f) * 20.f : GenF(r, 200.f, S_NAN | S_ZERO | S_TINY);
        cam->m_fHorizontalAngle = v;
        cam->ClipBeta();
        const float a = cam->m_fHorizontalAngle;
        cam->m_fHorizontalAngle = v;
        oracle::Fn<void __fastcall(CCam*, int)>(0x509C50)(cam, 0);
        const float b = cam->m_fHorizontalAngle;
        if (SameF(a, b)) return true;
        d = "in " + F(v) + " got " + F(a) + " exe " + F(b); return false;
    });
    Run("CPedShelterAttractor::ComputeAttractHeading 0x5E9690", [&](Rng& r, std::string& d) {
        static std::vector<uint8_t> attrStore(sizeof(CPedShelterAttractor) + 16);
        auto* at = reinterpret_cast<CPedShelterAttractor*>(attrStore.data());
        const uint32 seed = r.u32();
        float a = 0.f, b = 0.f;
        SeedBoth(seed); at->CPedShelterAttractor::ComputeAttractHeading(0, a);
        SeedBoth(seed); oracle::Fn<void __fastcall(CPedShelterAttractor*, int, int, float*)>(0x5E9690)(at, 0, 0, &b);
        if (SameF(a, b)) return true;
        d = "seed " + std::to_string(seed) + " got " + F(a) + " exe " + F(b); return false;
    });
}

// ---------------------------------------------------------------------------------------------------------------------------------
// reciprocal-constant fidelity: the exe multiplies by a FLOAT reciprocal constant (1/N rounded to float, in .rdata) where the source has `x / N` or `x * (1.f / N)`
// (ExeRecip in common.h). The functions with the most such sites that can be driven in isolation.
static void TestRecip() {
    const auto Dir = [](Rng& r, float& x, float& y, float ang) { x = std::cos(ang); y = std::sin(ang); (void)r; };
    const auto GenDirPair = [&](Rng& r, float& sx, float& sy, float& ex, float& ey) {
        const float a = r.f01() * 6.2831853f;
        const float d = r.below(3) ? (r.f01() - 0.5f) * (r.below(2) ? 1.2f : 6.2831853f) : (r.f01() - 0.5f) * 0.05f;   // mostly near-parallel (dot > 0.7) and in the interpolation range
        Dir(r, sx, sy, a); Dir(r, ex, ey, a + d);
        if (r.sp && r.below(100) < r.sp) { sx = GenF(r, 2.f); sy = GenF(r, 2.f); }
    };

    Run("CCurves::CalcSpeedVariationInBend 0x43C660", [&](Rng& r, std::string& d) {
        const CVector s = GenV(r, r.below(2) ? 100.f : 3000.f), e = GenV(r, r.below(2) ? 100.f : 3000.f);
        float sx, sy, ex, ey; GenDirPair(r, sx, sy, ex, ey);
        const float a = CCurves::CalcSpeedVariationInBend(s, e, sx, sy, ex, ey);
        const float b = oracle::Fn<float __cdecl(const CVector*, const CVector*, float, float, float, float)>(0x43C660)(&s, &e, sx, sy, ex, ey);
        if (SameF(a, b)) return true;
        d = "s " + V(s) + " e " + V(e) + " dirs " + F(sx) + "," + F(sy) + " " + F(ex) + "," + F(ey) + " got " + F(a) + " exe " + F(b); return false;
    });
    Run("CTrainNode::GetDistanceFromStart 0x6F54B0", [&](Rng& r, std::string& d) {
        CTrainNode n{}; n.m_nDistanceFromStart = (uint16)r.u32();
        const float a = n.GetDistanceFromStart(), b = oracle::Fn<float __fastcall(CTrainNode*, int)>(0x6F54B0)(&n, 0);
        if (SameF(a, b)) return true;
        d = "dist " + std::to_string(n.m_nDistanceFromStart) + " got " + F(a) + " exe " + F(b); return false;
    });
    Run("CGridRef::GetGridRefPositions(vec) 0x71D5A0", [&](Rng& r, std::string& d) {
        const CVector p{ GenF(r, r.below(4) ? 3500.f : 20000.f), GenF(r, r.below(4) ? 3500.f : 20000.f), 0.f };
        uint8 ax = 0xCC, ay = 0xCC, bx = 0xDD, by = 0xDD;
        CGridRef::GetGridRefPositions(p, &ax, &ay);
        oracle::Fn<void __cdecl(CVector, uint8*, uint8*)>(0x71D5A0)(p, &bx, &by);
        if (ax == bx && ay == by) return true;
        d = "p " + V(p) + " got " + std::to_string(ax) + "," + std::to_string(ay) + " exe " + std::to_string(bx) + "," + std::to_string(by); return false;
    });
    Run("CAEAudioUtility::ConvertFromBytesToMS 0x4D9EF0", [&](Rng& r, std::string& d) {
        const uint32 bytes = r.u32() >> r.below(20), rate = 8000 + r.below(48000); const uint16 ch = 1 + (uint16)r.below(2);
        const uint32 a = CAEAudioUtility::ConvertFromBytesToMS(bytes, rate, ch), b = oracle::Fn<uint32 __cdecl(uint32, uint32, uint16)>(0x4D9EF0)(bytes, rate, ch);
        if (a == b) return true;
        d = std::to_string(bytes) + " " + std::to_string(rate) + " " + std::to_string(ch) + " got " + std::to_string(a) + " exe " + std::to_string(b); return false;
    });
    Run("CAEAudioUtility::ConvertFromMSToBytes 0x4D9F40", [&](Rng& r, std::string& d) {
        const uint32 ms = r.u32() >> r.below(20), freq = 8000 + r.below(48000); const uint16 mult = 1 + (uint16)r.below(2);
        const uint32 a = CAEAudioUtility::ConvertFromMSToBytes(ms, freq, mult), b = oracle::Fn<uint32 __cdecl(uint32, uint32, uint16)>(0x4D9F40)(ms, freq, mult);
        if (a == b) return true;
        d = std::to_string(ms) + " " + std::to_string(freq) + " " + std::to_string(mult) + " got " + std::to_string(a) + " exe " + std::to_string(b); return false;
    });
    Run("CRadar::CalculateBlipAlpha 0x583420", [&](Rng& r, std::string& d) {
        FrontEndMenuManager.m_bDrawingMap = false;
        const float dist = std::fabs(GenF(r, r.below(3) ? 60.f : 3000.f));
        const uint8 a = CRadar::CalculateBlipAlpha(dist), b = oracle::Fn<uint8 __cdecl(float)>(0x583420)(dist);
        if (a == b) return true;
        d = "dist " + F(dist) + " got " + std::to_string(a) + " exe " + std::to_string(b); return false;
    });
    Run("CTimeCycle::AddOne 0x55FF40", [&](Rng& r, std::string& d) {
        CBox box{ GenV(r, 3000.f, S_NOHUGE), GenV(r, 3000.f, S_NOHUGE) };
        const int16 farClip = (int16)r.u32(); const int32 extra = (int32)r.u32();
        const float strength = GenF(r, r.below(2) ? 100.f : 1e5f, S_NOHUGE | S_INF), falloff = GenF(r, 100.f), lod = GenF(r, 6.f, S_NOHUGE | S_INF);
        const auto saveN = CTimeCycle::m_NumBoxes; auto saveBoxes = Snap(0xB7C550, sizeof CTimeCycle::m_aBoxes);
        CTimeCycle::m_NumBoxes = 3; CTimeCycle::AddOne(box, farClip, extra, strength, falloff, lod);
        auto ra = Snap(0xB7C550, sizeof CTimeCycle::m_aBoxes); const auto na = CTimeCycle::m_NumBoxes;
        Put(0xB7C550, saveBoxes); CTimeCycle::m_NumBoxes = 3;
        oracle::Fn<void __cdecl(CBox*, int16, int32, float, float, float)>(0x55FF40)(&box, farClip, extra, strength, falloff, lod);
        auto rb = Snap(0xB7C550, sizeof CTimeCycle::m_aBoxes); const auto nb = CTimeCycle::m_NumBoxes;
        Put(0xB7C550, saveBoxes); CTimeCycle::m_NumBoxes = saveN;
        if (na == nb && SameMem(ra, rb, "m_aBoxes", d)) return true;
        d += " strength " + F(strength) + " lod " + F(lod); return false;
    });
    Run("CTrain::SetTrainSpeed 0x6F5E20", [&](Rng& r, std::string& d) {
        alignas(16) static uint8 bufA[sizeof(CTrain)], bufB[sizeof(CTrain)];
        std::memset(bufA, 0, sizeof bufA);
        auto* ta = reinterpret_cast<CTrain*>(bufA);
        ta->trainFlags.bClockwiseDirection = r.below(2);
        std::memcpy(bufB, bufA, sizeof bufA);
        auto* tb = reinterpret_cast<CTrain*>(bufB);
        const float speed = GenF(r, r.below(2) ? 60.f : 1e4f);
        CTrain::SetTrainSpeed(ta, speed);
        oracle::Fn<void __cdecl(CTrain*, float)>(0x6F5E20)(tb, speed);
        if (SameF(ta->m_fTrainSpeed, tb->m_fTrainSpeed)) return true;
        d = "speed " + F(speed) + " cw " + std::to_string(ta->trainFlags.bClockwiseDirection) + " got " + F(ta->m_fTrainSpeed) + " exe " + F(tb->m_fTrainSpeed); return false;
    });
    Run("CCamera::CamShake 0x50A9F0", [&](Rng& r, std::string& d) {
        auto& cam = TheCamera.GetActiveCam();
        cam.m_vecSource = GenV(r, 200.f, S_NOHUGE);
        const CVector from = GenV(r, 200.f, S_NOHUGE);
        const float strength = r.f01() * 20.f;
        const float force0 = r.f01() * 3.f; const uint32 start0 = r.u32() >> 4, now = start0 + r.below(8000);
        CTimer::m_snTimeInMilliseconds = now;
        TheCamera.m_fCamShakeForce = force0; TheCamera.m_nCamShakeStart = start0;
        TheCamera.CamShake(strength, from);
        const float fa = TheCamera.m_fCamShakeForce; const uint32 sa = TheCamera.m_nCamShakeStart;
        TheCamera.m_fCamShakeForce = force0; TheCamera.m_nCamShakeStart = start0;
        oracle::Fn<void __fastcall(CCamera*, int, float, CVector)>(0x50A9F0)(&TheCamera, 0, strength, from);
        if (SameF(fa, TheCamera.m_fCamShakeForce) && sa == TheCamera.m_nCamShakeStart) return true;
        d = "src " + V(cam.m_vecSource) + " from " + V(from) + " now-start " + std::to_string(now - start0) + " force0 " + F(force0) + " strength " + F(strength) + " got " + F(fa) + " exe " + F(TheCamera.m_fCamShakeForce); return false;
    });
    Run("CFont::SetColor 0x719430", [&](Rng& r, std::string& d) {
        CFont::m_fFontAlpha = r.below(4) ? r.f01() * 255.f : 255.f + r.f01() * 50.f;
        const CRGBA c{ (uint8)r.u32(), (uint8)r.u32(), (uint8)r.u32(), (uint8)r.u32() };
        CFont::SetColor(c); const CRGBA ra = CFont::m_Color;
        CFont::m_Color = {};
        oracle::Fn<void __cdecl(CRGBA)>(0x719430)(c);
        if (!std::memcmp(&ra, &CFont::m_Color, sizeof ra)) return true;
        d = "alpha " + F(CFont::m_fFontAlpha) + " a " + std::to_string(c.a) + " got " + std::to_string(ra.a) + " exe " + std::to_string(CFont::m_Color.a); return false;
    });
}

// ---------------------------------------------------------------------------------------------------------------------------------
// float -> int conversions: MSVC compiles `(int)f` / `(short)f` to _ftol2_sse (cvttsd2si: 0x80000000 for NaN / out of range), the exe's CRT _ftol2 (0x821B40) returns the low dword of
// a truncating fistp qword (NaN / inf -> 0, 2^31..2^63 wraps). source/standalone/FtolExe.cpp replaces _ftol2_sse with the exe's semantics; this proves it.
static int32_t ExeFtol(double v) {
    int32_t r;
    __asm {
        fld qword ptr [v]
        mov eax, 0x821B40
        call eax
        mov r, eax
    }
    return r;
}
static void TestFtol() {
    const auto Gen = [](Rng& r) {
        static const float sc[] = { 1.f, 1000.f, 3e4f, 2.1e9f, 3e9f, 4.4e9f, 1e10f, 9e18f, 1e19f, 1e30f };
        return GenF(r, sc[r.below(10)]);
    };
    Run("_ftol2 override: (int)float / (int16)float / (int)double", [&](Rng& r, std::string& d) {
        volatile float f = Gen(r);
        volatile double dd = (double)Gen(r) * (r.below(2) ? 1.0 : (double)Gen(r));
        const int32_t e1 = ExeFtol(f), e2 = ExeFtol(dd);
        const int32_t a1 = (int32_t)f, a2 = (int32_t)dd;
        const int16_t a3 = (int16_t)f, a4 = (int16_t)dd;
        const uint32_t a5 = (uint32_t)dd; const uint8_t a6 = (uint8_t)f; const int64_t a7 = (int64_t)dd;
        if (a1 == e1 && a2 == e2 && a3 == (int16_t)e1 && a4 == (int16_t)e2 && a5 == (uint32_t)e2 && a6 == (uint8_t)e1 && (int32_t)a7 == e2) return true;
        char b[200]; std::snprintf(b, sizeof b, "f %s d %g: int(f) %08X/%08X int(d) %08X/%08X s16 %04X/%04X u32(d) %08X u8 %02X i64lo %08X", F(f).c_str(), dd, a1, e1, a2, e2, (uint16_t)a3, (uint16_t)e1, a5, a6, (uint32_t)a7);
        d = b; return false;
    });
}

// ---------------------------------------------------------------------------------------------------------------------------------
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
    if (!exe) { std::printf("review_oracle_test: set RW_EXE_ORACLE=<path of gta_sa_compact.exe>; skipped\n"); return 0; }
    std::memcpy(g_portSinTab, reinterpret_cast<void*>(0xBB3E00), sizeof g_portSinTab);   // the port's table, written by CMaths' static initialiser
    if (!oracle::Map(exe)) { std::printf("FATAL: cannot map the exe\n"); return 2; }
    *reinterpret_cast<int*>(0xC9C400) = 1;
    oracle::Patch(0x82872C, (void*)&HostMathErr);
    oracle::Patch(0x827B3D, (void*)&HostGetPtd);
    oracle::Patch(0x50D930, (void*)&HostNoopProcess);   // CHandShaker::Process
    std::printf("review_oracle_test: %d cases per function and precision mode\n", g_cases);
    TestMaths();
    TestTasks();
    TestHud();
    TestMisc();
    TestIdleCam();
    TestPath();
    TestFidelity();
    TestRecip();
    TestFtol();
    int bad24 = 0, bad53 = 0, hard24 = 0, hard53 = 0;
    for (auto& r : g_rows) { bad24 += r.bad24; bad53 += r.bad53; hard24 += r.hardReg24 + r.hardSpec24; hard53 += r.hardReg53 + r.hardSpec53; }
    std::printf("\n%zu functions, mismatches (strict / excluding NaN-payload-only): PC24 %d / %d, PC53 %d / %d\n", g_rows.size(), bad24, hard24, bad53, hard53);
    return hard24 ? 1 : 0;
}

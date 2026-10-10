// x87 intrinsic fidelity oracle: the exe compiled sin/cos/atan2/tan/sqrt/log10 INLINE (fsin, fcos, fpatan, fptan, fsqrt, fyl2x) while the ports used the UCRT's
// std:: versions. This test (1) quantifies the primitive differences (INFO rows, not counted), (2) compares the CRT entry points the exe really CALLS (pow, asin, acos,
// tan, floor, ceil, modf: exe machine code) with the UCRT, (3) runs ported game functions that contain such math against the exe, bit-exact, at PC=24 and PC=53.
// usage: x87_oracle_test.exe [-v] [-v53] [-vn] [-n cases] [name-substring ...]    (exit code 0 = no real mismatch at PC=24 in the non-INFO rows)
#include "game_oracle.h"

#include "General.h"
#include "Matrix.h"
#include "Quaternion.h"
#include "standalone/Fixups.h"
#include "MemoryMgr.h"
#include "Core/X87Intrinsics.h"
#include "CoverPoint.h"
#include "Cover.h"
#include "Curves.h"
#include "cHandlingDataMgr.h"
#include "Cam.h"
#include "Camera.h"
#include "Radar.h"
#include "MenuManager.h"
#include "Checkpoints.h"
#include "Entity/Placeable.h"
#include "Maths.h"
#include "HandShaker.h"
#include "standalone/GameRand.h"

#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <float.h>
#include <string>
#include <vector>

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
static volatile uint64_t g_progress = 0; static volatile int g_caseIdx = 0, g_pcMode = 0;
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
            ++g_progress; g_caseIdx = i; g_pcMode = pc;
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
#pragma init_seg(compiler)   // run BEFORE the static initialisers of the game TUs (CMaths fills its sine table at 0xBB3E00 during static init, which is inside the not yet RWX pad image)
struct EarlyUnprotect { EarlyUnprotect() { DWORD o; VirtualProtect(reinterpret_cast<void*>(0x401000), 0xCB0000 - 0x401000, PAGE_EXECUTE_READWRITE, &o); } } g_earlyUnprotect;
CPlayerInfo& FindPlayerInfo(int32 playerId) { return CWorld::Players[playerId < 0 ? CWorld::PlayerInFocus : playerId]; }
void CHandShaker::Process(float) {}
static void __fastcall HostNoopProcess(void*, void*, float) {}
static std::vector<uint8_t> Snap(unsigned va, size_t n) { std::vector<uint8_t> v(n); std::memcpy(v.data(), reinterpret_cast<void*>(va), n); return v; }
static void Put(unsigned va, const std::vector<uint8_t>& v) { std::memcpy(reinterpret_cast<void*>(va), v.data(), v.size()); }
template<class T> static T* PtrAt(unsigned va) { return reinterpret_cast<T*>(static_cast<uintptr_t>(va)); }
static bool SameMem(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b, const char* what, std::string& d) {
    for (size_t i = 0; i < a.size(); ++i) if (a[i] != b[i]) { char t[96]; std::snprintf(t, sizeof t, "%s differs at +0x%zX: got %02X exe %02X", what, i, a[i], b[i]); d += t; return false; }
    return true;
}

static double DBits(uint64_t b) { double d; std::memcpy(&d, &b, 8); return d; }
static uint64_t DB(double d) { uint64_t b; std::memcpy(&b, &d, 8); return b; }
static std::string D(double d) { char b[48]; std::snprintf(b, sizeof b, "%016llX(%.17g)", (unsigned long long)DB(d), d); return b; }
static bool SameD(double a, double b) {
    if (DB(a) == DB(b)) return true;
    const bool na = a != a, nb = b != b;
    if (na && nb) g_nanDiff = true; else g_hardDiff = true;
    return false;
}
#include "x87_oracle_prims.inc"
#include "x87_oracle_funcs.inc"

// watchdog: a case that does not finish within 60 s (an exe loop on a NaN / huge input, a CRT lock) is reported with its row / case index and the process exits
static DWORD WINAPI Watchdog(LPVOID) {
    uint64_t last = ~0ull; int stuck = 0;
    for (;;) {
        Sleep(5000);
        if (g_progress != last) { last = g_progress; stuck = 0; continue; }
        if (++stuck >= 12) { std::printf("\n  WATCHDOG: no progress for 60 s in '%s' case %d (PC%d) - exe hang?\n", oracle::g_where, g_caseIdx, g_pcMode); std::fflush(stdout); ExitProcess(4); }
    }
}
int main(int argc, char** argv) {
    CreateThread(nullptr, 0, Watchdog, nullptr, 0, nullptr);
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "-v")) g_verbose = true;
        else if (!std::strcmp(argv[i], "-v53")) g_verbose53 = true;
        else if (!std::strcmp(argv[i], "-vn")) g_verboseNan = true;
        else if (!std::strcmp(argv[i], "-n") && i + 1 < argc) g_cases = std::atoi(argv[++i]);
        else g_filters.push_back(argv[i]);
    }
    const char* exe = std::getenv("RW_EXE_ORACLE");
    if (!exe) { std::printf("x87_oracle_test: set RW_EXE_ORACLE=<path of gta_sa_compact.exe>; skipped\n"); return 0; }
    if (!oracle::Map(exe)) { std::printf("FATAL: cannot map the exe\n"); return 2; }
    *reinterpret_cast<int*>(0xC9C400) = 1;   // __sse2_available: the CRT's pow / floor / ceil / asin take the SSE2 path, as on every real machine
    oracle::Patch(0x82872C, (void*)&HostMathErr);
    oracle::Patch(0x827B3D, (void*)&HostGetPtd);
    oracle::Patch(0x50D930, (void*)&HostNoopProcess);   // CHandShaker::Process
    std::printf("x87_oracle_test: %d cases per function and precision mode; PC24 = game mode (D3D CreateDevice), PC53 = CRT default\n", g_cases);
    TestPrimitives();
    TestFuncs();
    int bad24 = 0, bad53 = 0, hard24 = 0, hard53 = 0;
    for (auto& r : g_rows) { if (r.name.rfind("INFO", 0) == 0) continue; bad24 += r.bad24; bad53 += r.bad53; hard24 += r.hardReg24 + r.hardSpec24; hard53 += r.hardReg53 + r.hardSpec53; }
    std::printf("\n%zu rows, mismatches (strict / excluding NaN-payload-only, INFO rows excluded): PC24 %d / %d, PC53 %d / %d\n", g_rows.size(), bad24, hard24, bad53, hard53);
    return hard24 ? 1 : 0;
}

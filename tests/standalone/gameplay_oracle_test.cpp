// P2E-3: differential test of gameplay GAME code (peds / IK / tasks, camera, weapons, vehicles) against the original machine code (exe oracle, see game_oracle.h;
// harness copied from physics_oracle_test.cpp). Fixtures live in gameplay_oracle_<area>.inc; callees outside the area are stubbed/recorded on BOTH sides.
// Bit-exact compare at PC=24 (game) and PC=53 (info).
// usage: gameplay_oracle_test.exe [-v] [-v53] [-vn] [-trace (print every case index)] [-n cases] [name-substring ...]     (exit code 0 = no real mismatch at PC=24)
#include "game_oracle.h"

#include "Collision.h"
#include "General.h"
#include "Matrix.h"
#include "Quaternion.h"
#include "Entity/Physical.h"
#include "Timer.h"
#include "standalone/Fixups.h"
#include "MemoryMgr.h"

#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <float.h>
#include <crtdbg.h>
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
static unsigned g_defMask = S_ALL;   // default special-value mask of GenF / GenV (a test group may restrict it, e.g. to finite specials)
static float GenF(Rng& r, float scale, unsigned mask = g_defMask) {
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
static CVector GenV(Rng& r, float scale, unsigned mask = g_defMask) { return { GenF(r, scale, mask), GenF(r, scale, mask), GenF(r, scale, mask) }; }

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
static bool g_trace = false;
static int  g_onlyCase = -1;   // -case N: run only case N (debugging with temporary prints in the port)
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
// watchdog: a case that runs longer than 8 s is reported with the main thread's EIP / stack (map the address with the linker .map) and the test exits
static volatile DWORD g_caseTick = 0;
static HANDLE g_mainThread = nullptr;
static DWORD WINAPI WatchdogProc(LPVOID) {
    for (;;) {
        Sleep(1000);
        const DWORD t = g_caseTick;
        if (t && GetTickCount() - t > 8000) {
            SuspendThread(g_mainThread);
            CONTEXT cx{}; cx.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
            GetThreadContext(g_mainThread, &cx);
            std::printf("\n  HANG in '%s': EIP=%08lX ESP=%08lX EBP=%08lX stack:", oracle::g_where, cx.Eip, cx.Esp, cx.Ebp);
            for (int i = 0; i < 12; ++i) std::printf(" %08lX", ((const unsigned long*)cx.Esp)[i]);
            std::printf("\n"); std::fflush(stdout);
            std::exit(4);
        }
    }
}
static bool IsPC24() { unsigned cw = 0; _controlfp_s(&cw, 0, 0); return (cw & _MCW_PC) == _PC_24; }   // for tests whose algorithm is only well defined at the game's precision
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
            if (g_onlyCase >= 0 && i != g_onlyCase) continue;
            Rng r(0xC0FFEEull + (uint64_t)i * 7919);
            static const int spTab[] = { 0, 3, 10, 0, 25, 6 };
            r.sp = spTab[i % 6];
            std::string d;
            g_nanDiff = g_hardDiff = g_special = false;
            if (g_trace) std::printf("[%s PC%d case %d]\n", name.c_str(), pc, i);
            g_caseTick = GetTickCount();
            if (!fn(r, d)) {
                const bool payloadOnly = g_nanDiff && !g_hardDiff;
                (pc == 24 ? row.bad24 : row.bad53)++;
                int& cat = payloadOnly ? (pc == 24 ? row.nan24 : row.nan53) : g_special ? (pc == 24 ? row.hardSpec24 : row.hardSpec53) : (pc == 24 ? row.hardReg24 : row.hardReg53);
                cat++;
                if ((shown < 2 || g_verbose) && (!payloadOnly || g_verboseNan) && (pc == 24 || g_verbose || g_verbose53)) { ++shown; std::printf("    MISMATCH PC=%d case %d%s: %s\n", pc, i, g_special ? " (special inputs)" : "", d.c_str()); }
            }
        }
    }
    g_caseTick = 0;
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

static CVector GenDir(Rng& r) {   // mostly unit-ish 2D directions, sometimes arbitrary
    if (r.below(5) == 0) return GenV(r, 2.f);
    const float a = r.f01() * 6.2831853f;
    CVector v{ std::cos(a), std::sin(a), 0.f };
    if (r.below(4) == 0) v.z = GenF(r, 0.5f);
    if (r.sp && r.below(100) < r.sp) v.x = GenF(r, 1.f);
    return v;
}

struct Snap {   // snapshot / restore of a range of exe globals (both implementations work on the same statics)
    uintptr_t va; std::vector<uint8> b;
    Snap(uintptr_t v, size_t n) : va(v), b(n) {}
    void Save()          { std::memcpy(b.data(), (void*)va, b.size()); }
    void Restore() const { std::memcpy((void*)va, b.data(), b.size()); }
    bool SameAs(const Snap& o) const { return SameBlob(b.data(), o.b.data(), b.size()); }
    void FillRandom(Rng& r) { for (size_t i = 0; i + 4 <= b.size(); i += 4) { const uint32_t v = r.u32() * 2654435761u; std::memcpy(&b[i], &v, 4); } }
};
static std::string DiffDesc(const void* a, const void* b, size_t n, const char* tag) {   // differing dwords only
    std::string s; int shown = 0;
    for (size_t i = 0; i + 4 <= n && shown < 8; i += 4) {
        uint32_t x, y; std::memcpy(&x, (const uint8*)a + i, 4); std::memcpy(&y, (const uint8*)b + i, 4);
        if (x != y) { char t[80]; std::snprintf(t, sizeof t, " %s+%02zX got %08X exe %08X", tag, i, x, y); s += t; ++shown; }
    }
    return s;
}

// ---------------------------------------------------------------------------------------------------------------------------------
// shared stubs for symbols of TUs this test does not link (one definition for all areas - add yours here, not in the .inc files)
#include "Entity/Entity.h"
#include "Models/ModelInfo.h"
#include "World.h"
#include "Collision/ColPoint.h"


// redirects a port function (member or free) to a host function with the same calling convention by overwriting its entry with a JMP (as physics_oracle_weapon.inc does)
static void PatchHostFn(void* fn, void* host) {
    DWORD old = 0; VirtualProtect(fn, 16, PAGE_EXECUTE_READWRITE, &old);
    auto* p = reinterpret_cast<uint8_t*>(fn); p[0] = 0xE9; *reinterpret_cast<int32_t*>(p + 1) = (int32_t)((uintptr_t)host - ((uintptr_t)fn + 5));
    FlushInstructionCache(GetCurrentProcess(), fn, 16);
}
template<class PM> static void* PmAddr(PM pm) { void* p; static_assert(sizeof(pm) == sizeof(p)); std::memcpy(&p, &pm, sizeof p); return p; }

// Scripted CWorld::ProcessLineOfSight (port: the symbol below, exe: 0x56BA00 patched in main()). `script` is consumed in call order; the entity of an entry is a ROLE
// (the port and the exe work on separate copies of the fixture, so each side maps the roles to its own copies), every call's endpoints are recorded for comparison.
namespace los {
struct Entry { bool ret = false; int role = 0; CVector point{}, normal{ 0, 0, 1 }; };   // role 0 = no entity, 1..4 = roles[1..4]
struct Call { CVector a, b; };
inline std::vector<Entry> script;
inline size_t             pos = 0;
inline CEntity*           roles[5] {};
inline std::vector<Call>  calls;
inline void Reset() { pos = 0; calls.clear(); }
inline bool Do(const CVector& a, const CVector& b, CColPoint& cp, CEntity*& ent) {
    calls.push_back({ a, b });
    const Entry e = pos < script.size() ? script[pos++] : Entry{};
    ent = e.role ? roles[e.role] : nullptr;
    cp.m_vecPoint = e.point; cp.m_vecNormal = e.normal;
    return e.ret;
}
inline bool SameCalls(const std::vector<Call>& x, const std::vector<Call>& y) {
    if (x.size() != y.size()) return false;
    for (size_t i = 0; i < x.size(); ++i) if (!SameV(x[i].a, y[i].a) || !SameV(x[i].b, y[i].b)) return false;
    return true;
}
inline std::string Describe(const std::vector<Call>& c) { std::string s = std::to_string(c.size()) + " LOS calls"; for (size_t i = 0; i < c.size() && i < 3; ++i) s += " [" + V(c[i].a) + "->" + V(c[i].b) + "]"; return s; }
}
bool CWorld::ProcessLineOfSight(const CVector& a, const CVector& b, CColPoint& cp, CEntity*& ent, bool, bool, bool, bool, bool, bool, bool, bool) { return los::Do(a, b, cp, ent); }
static bool __cdecl ExeProcessLineOfSight(const CVector& a, const CVector& b, CColPoint& cp, CEntity*& ent, bool, bool, bool, bool, bool, bool, bool, bool) { return los::Do(a, b, cp, ent); }

#include "gameplay_oracle_peds.inc"
#include "gameplay_oracle_camera.inc"
#include "gameplay_oracle_weapon.inc"
#include "gameplay_oracle_vehicle.inc"
#include "gameplay_oracle_buoy.inc"

static int __cdecl AssertHook(int, char* msg, int*) {   // prints the call stack of a failed assert (map the addresses with the linker .map)
    void* fr[16]; const USHORT n = RtlCaptureStackBackTrace(0, 16, fr, nullptr);
    std::fprintf(stderr, "ASSERT: %s\n  frames:", msg);
    for (USHORT i = 0; i < n; ++i) std::fprintf(stderr, " %p", fr[i]);
    std::fprintf(stderr, "\n"); std::fflush(stderr);
    return 0;
}
int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "-v")) g_verbose = true;
        else if (!std::strcmp(argv[i], "-v53")) g_verbose53 = true;
        else if (!std::strcmp(argv[i], "-vn")) g_verboseNan = true;
        else if (!std::strcmp(argv[i], "-trace")) g_trace = true;
        else if (!std::strcmp(argv[i], "-case") && i + 1 < argc) g_onlyCase = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "-n") && i + 1 < argc) g_cases = std::atoi(argv[++i]);
        else g_filters.push_back(argv[i]);
    }
    _CrtSetReportHook(AssertHook);
    _set_error_mode(_OUT_TO_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE); _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE); _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
    const char* exe = std::getenv("RW_EXE_ORACLE");
    if (!exe) { std::printf("gameplay_oracle_test: set RW_EXE_ORACLE=<path of gta_sa_compact.exe>; skipped\n"); return 0; }
    if (!oracle::Map(exe)) { std::printf("FATAL: cannot map the exe\n"); return 2; }
    *reinterpret_cast<int*>(0xC9C400) = 1;
    oracle::Patch(0x82872C, (void*)&HostMathErr);
    oracle::Patch(0x827B3D, (void*)&HostGetPtd);
    oracle::Patch(0x56BA00, (void*)&ExeProcessLineOfSight);   // CWorld::ProcessLineOfSight -> scripted (los::)
    DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &g_mainThread, 0, FALSE, DUPLICATE_SAME_ACCESS);
    CreateThread(nullptr, 0, WatchdogProc, nullptr, 0, nullptr);
    std::printf("gameplay_oracle_test: %d cases per function and precision mode; PC24 = game mode (D3D CreateDevice), PC53 = CRT default\n", g_cases);
    TestPeds();
    TestCamera();
    TestWeapons();
    TestVehicles();
    TestBuoyancy();
    int bad24 = 0, bad53 = 0, hard24 = 0, hard53 = 0;
    for (auto& r : g_rows) { bad24 += r.bad24; bad53 += r.bad53; hard24 += r.hardReg24 + r.hardSpec24; hard53 += r.hardReg53 + r.hardSpec53; }
    std::printf("\n%zu functions, mismatches (strict / excluding NaN-payload-only): PC24 %d / %d, PC53 %d / %d\n", g_rows.size(), bad24, hard24, bad53, hard53);
    return hard24 ? 1 : 0;
}

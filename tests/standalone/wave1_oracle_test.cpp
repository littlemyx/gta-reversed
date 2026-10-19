// ORACLE-W1 wave 1: differential test of ported game functions that had no exe-oracle row yet (physics-adjacent helpers, AI decisions, camera, surfaces, events ...).
// Harness copied from logic_oracle_test.cpp (exe oracle, see game_oracle.h); fixtures live in wave1_oracle_<area>.inc, the coverage map in .notes/ORACLE_COVERAGE.md.
// Both sides run on the SAME statics (the port's globals sit at the exe's addresses in this test, NOTSA_ORACLE_ADDRESS_GLOBALS); objects are raw buffers with the real layout, filled
// with random / poisoned data, copied for the exe side. Bit-exact compare at PC=24 (game) and PC=53 (info).
// usage: wave1_oracle_test.exe [-v] [-v53] [-vn] [-trace (print every case index)] [-n cases] [name-substring ...]     (exit code 0 = no real mismatch at PC=24)
#include "game_oracle.h"
#include "oracle_alloc.h"

#include "General.h"
#include "Matrix.h"
#include "Timer.h"
#include "Camera.h"
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

// Port TUs with dynamic initialisers that write exe-owned memory (Maths.cpp: the sine LUT at 0xBB3E00) run before main(), i.e. before oracle::Map() makes the placeholder image writable:
// make it RWX from an earlier initialisation segment (lib runs before the user segment of the game TUs)
#pragma warning(disable : 4075)
#pragma init_seg(lib)
static struct PadWritable { PadWritable() { DWORD old = 0; VirtualProtect(reinterpret_cast<void*>(0x401000), 0xCB0000 - 0x401000, PAGE_EXECUTE_READWRITE, &old); } } g_padWritable;

// ---------------------------------------------------------------------------------------------------------------------------------
// stubs of the symbols the compiled game TUs reference but the test never calls
namespace notsa::standalone::detail { bool g_DataImageLoaded = true; }
namespace notsa::standalone::Fixups {
void RegisterFunction(uint32_t, void*, const char*) {}
void RegisterUnverified(uint32_t, const char*, int, bool, uint32_t) {}
}
void ReversibleHooks::RHManager::AddHookToCategory(std::string_view, HookInstallOptions, std::shared_ptr<ReversibleHook::TwoWayHook>) {}
void* CMemoryMgr::Malloc(uint32 size, uint32) { return oracle::alloc::g_route ? oracle::alloc::Alloc(size) : std::malloc(size); }   // ORACLE-W2: the arena while a deterministic window is open
void* CMemoryMgr::Malloc(uint32 size) { return oracle::alloc::g_route ? oracle::alloc::Alloc(size) : std::malloc(size); }
void  CMemoryMgr::Free(void* p) { oracle::alloc::Free(p); }
void* CMemoryMgr::MallocAlign(uint32 size, uint32 align) { void* m = Malloc(size + align); auto* r = reinterpret_cast<void*>((uint32(reinterpret_cast<uint8*>(m) + align)) & ~(align - 1)); *(reinterpret_cast<void**>(r) - 1) = m; return r; }   // ORACLE-W3-ANIM2: the exe 0x72F4C0 / 0x72F4F0 (malloc(size + align), aligned, base pointer at result[-1])
void  CMemoryMgr::FreeAlign(void* memory) { Free(*(reinterpret_cast<void**>(memory) - 1)); }
#ifdef W2_ONLY
ORACLE_ALLOC_HOST_OPERATORS()   // the port's `new` / `delete` go to the deterministic arena inside an oracle::alloc::Begin()/End() window
#endif

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
static bool g_trace = false;
static bool g_verbose = false, g_verbose53 = false, g_verboseNan = false;
static std::vector<std::string> g_filters;
static int g_hits = 0;
static void Hit(bool b) { g_hits += b; }   // coverage: number of `true` results of a bool-returning function (per precision mode)
struct Row { std::string name; int hits = -1; int cases = 0, bad24 = 0, bad53 = 0, hardReg24 = 0, hardSpec24 = 0, nan24 = 0, hardReg53 = 0, hardSpec53 = 0, nan53 = 0; bool fault = false; };
static std::vector<Row> g_rows;

static std::vector<std::string> g_skipRows;   // ORACLE-W2 dev aid: rows listed in the file named by env W2_SKIPFILE (one exact row name per line) are not run - lets a driver script step over rows that crash the process (stack overflow)
static bool Skipped(const std::string& name) {
    static bool loaded = false;
    if (!loaded) {
        loaded = true;
        if (const char* f = std::getenv("W2_SKIPFILE")) {
            if (FILE* fh = std::fopen(f, "r")) {
                char b[512];
                while (std::fgets(b, sizeof b, fh)) {
                    std::string l(b);
                    while (!l.empty() && (l.back() == '\n' || l.back() == '\r')) l.pop_back();
                    if (!l.empty()) g_skipRows.push_back(l);
                }
                std::fclose(fh);
            }
        }
    }
    // W2_START=<row name>: skip every row before it (resume after a crash without replaying the earlier rows)
    static const char* start = std::getenv("W2_START");
    static bool reached = !start;
    if (!reached) { if (name == start) reached = true; else return true; }
    for (auto& x : g_skipRows) if (x == name) return true;
    return false;
}
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
static void SetPC(int bits) { unsigned cw; _controlfp_s(&cw, bits == 24 ? _PC_24 : _PC_53, _MCW_PC); }

// ORACLE-W2: soft fault mode. A fault inside a case (the exe function needs state the fixture does not provide, an unresolved port symbol ...) ends the ROW, not the process: the row is listed as
// "NOT ORACLE-ABLE" with the faulting address and counts as a failure of the test (the committed test has none: such functions are listed in the generator's skip table with the reason).
static DWORD g_faultCode = 0; static void* g_faultEip = nullptr; static void* g_faultAddr = nullptr; static unsigned g_faultEax = 0, g_faultEcx = 0; static unsigned g_faultStk[4] = {};
static void (*g_onFault)() = nullptr;
static bool g_agreedFault = false;   // ORACLE-W2: set by BothA when port AND exe faulted in this case (same behaviour: no information, not a mismatch)
static int  g_agreedFaults = 0;
static const char* g_curRow = "";
static LONG WINAPI LastChance(EXCEPTION_POINTERS* ep) { std::printf("\nUNHANDLED exception 0x%08lX at EIP=%p in row '%s'\n", ep->ExceptionRecord->ExceptionCode, ep->ExceptionRecord->ExceptionAddress, g_curRow); std::fflush(stdout); std::exit(6); }
static int FaultFilter(EXCEPTION_POINTERS* ep) {
    const DWORD c = ep->ExceptionRecord->ExceptionCode;
    if (!oracle::g_softFault) return EXCEPTION_CONTINUE_SEARCH;
    if (c == EXCEPTION_STACK_OVERFLOW || (((c & 0xF0000000u) != 0xC0000000u) && c != EXCEPTION_BREAKPOINT && c != EXCEPTION_SINGLE_STEP && c != 0xE06D7363u && c != 0xE0000001u)) return EXCEPTION_CONTINUE_SEARCH;   // hardware faults, int3 in garbage code, an uncaught C++ exception (arena overflow)
    g_faultCode = c; g_faultEip = ep->ExceptionRecord->ExceptionAddress; g_faultAddr = c == EXCEPTION_ACCESS_VIOLATION ? (void*)ep->ExceptionRecord->ExceptionInformation[1] : nullptr;
    g_faultEax = ep->ContextRecord->Eax; g_faultEcx = ep->ContextRecord->Ecx; for (int i = 0; i < 4; ++i) { __try { g_faultStk[i] = ((const unsigned*)ep->ContextRecord->Esp)[i]; } __except (EXCEPTION_EXECUTE_HANDLER) { g_faultStk[i] = 0; } }
    return EXCEPTION_EXECUTE_HANDLER;
}
template<class Fn> static int SafeInvoke(Fn& fn, Rng& r, std::string& d) {   // 1 = agree, 0 = mismatch, -1 = fault (no C++ objects with destructors in this frame: __try)
    __try { return fn(r, d) ? 1 : 0; } __except (FaultFilter(GetExceptionInformation())) { return -1; }
}

// fn(rng, desc) -> true when port and exe agree; on mismatch it describes the inputs/outputs in `desc`
template<class Fn>
static void Run(const std::string& name, Fn&& fn, int cases = 0) {
    if (!Wanted(name) || Skipped(name)) return;
    const int N = cases ? cases : g_cases;
    Row row{ name, -1, N };
    oracle::g_where = name.c_str();
    g_curRow = name.c_str();
    if (std::getenv("W2_PROGRESS")) std::printf(">> %s\n", name.c_str());
    int agreed = 0;
    for (int pc : { 24, 53 }) {
        if (row.fault) break;
        SetPC(pc);
        int shown = 0;
        g_hits = 0;
        for (int i = 0; i < N; ++i) {
            Rng r(0xC0FFEEull + (uint64_t)i * 7919);
            static const int spTab[] = { 0, 3, 10, 0, 25, 6 };
            r.sp = spTab[i % 6];
            std::string d;
            g_nanDiff = g_hardDiff = g_special = false;
            g_agreedFault = false;
            if (g_trace) std::printf("[%s PC%d case %d]\n", name.c_str(), pc, i);
            g_caseTick = GetTickCount();
            const int rc = SafeInvoke(fn, r, d);
            if (rc < 0) {
                row.fault = true; row.bad24++; row.hardReg24++;
                std::printf("    NOT ORACLE-ABLE: fault 0x%08lX at EIP=%p (access %p) EAX=%08X ECX=%08X stack %08X %08X %08X, PC=%d case %d: %s\n", g_faultCode, g_faultEip, g_faultAddr, g_faultEax, g_faultEcx, g_faultStk[0], g_faultStk[1], g_faultStk[2], pc, i, name.c_str());
                if (g_onFault) g_onFault();
                break;
            }
            if (g_agreedFault) ++agreed;
            if (rc == 0) {
                const bool payloadOnly = g_nanDiff && !g_hardDiff;
                (pc == 24 ? row.bad24 : row.bad53)++;
                int& cat = payloadOnly ? (pc == 24 ? row.nan24 : row.nan53) : g_special ? (pc == 24 ? row.hardSpec24 : row.hardSpec53) : (pc == 24 ? row.hardReg24 : row.hardReg53);
                cat++;
                if ((shown < 2 || g_verbose) && (!payloadOnly || g_verboseNan) && (pc == 24 || g_verbose || g_verbose53)) { ++shown; std::printf("    MISMATCH PC=%d case %d%s: %s\n", pc, i, g_special ? " (special inputs)" : "", d.c_str()); }
            }
        }
    }
    if (agreed >= 2 * N) { row.fault = true; row.bad24++; row.hardReg24++; std::printf("    NOT ORACLE-ABLE: every case faulted identically in the port and in the exe (fixture lacks the object they dereference): %s\n", name.c_str()); }
    else if (agreed) std::printf("    (%d of %d cases faulted identically in port and exe: skipped)\n", agreed, 2 * N);
    g_caseTick = 0;
    SetPC(53);
    row.hits = g_hits;
    if (row.fault) { g_rows.push_back(row); return; }
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

static std::string HexVA(unsigned va) { char b[16]; std::snprintf(b, 16, "%X", va); return b; }

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

#include "world_oracle_inits.inc"   // the exe's float C++ start-up initialisers (same list as wave1_oracle_test)
// ORACLE-W1 wave 1: areas live in wave1_oracle_<area>.inc; W1_SKIP_<AREA> (compile definition) leaves one out
#include "wave1_oracle_common.inc"
#ifdef W2_ONLY
#include "wave2_oracle_all.inc"      // ORACLE-W2: allocator-backed rows (task / event constructors, Clone, sub-task factories ...), own executable (wave2_*_oracle_test)
#elif defined(W1_CONSTS_ONLY)
#include "wave1_oracle_consts.inc"   // generated: ~440 constant-returning overrides, own executable (wave1_consts_oracle_test)
#else
#include "wave1_oracle_surface.inc"
#include "wave1_oracle_camera.inc"
#include "wave1_oracle_peds.inc"
#include "wave1_oracle_ai.inc"
#include "wave1_oracle_entities.inc"
#include "wave1_oracle_prearray.inc"
#endif

// first-chance handler: prints every stack word that looks like a return address into the exe image / the test image (resolve with the linker .map / the exe disassembly) before oracle::FaultHandler reports
static LONG CALLBACK DeepStackHandler(EXCEPTION_POINTERS* ep) {
    const DWORD c = ep->ExceptionRecord->ExceptionCode;
    if (c != EXCEPTION_ACCESS_VIOLATION && c != EXCEPTION_ILLEGAL_INSTRUCTION && c != EXCEPTION_STACK_OVERFLOW) return EXCEPTION_CONTINUE_SEARCH;
    static bool once = false; if (once) return EXCEPTION_CONTINUE_SEARCH; once = true;
    const unsigned long* sp = (const unsigned long*)ep->ContextRecord->Esp;
    std::printf("\n  deep stack (EIP=%08lX):", ep->ContextRecord->Eip);
    int shown = 0;
    for (int i = 0; i < 1024 && shown < 40; ++i) { __try { const unsigned long v = sp[i]; if (v >= 0x401000 && v < 0x1800000) { std::printf(" %08lX", v); ++shown; } } __except (EXCEPTION_EXECUTE_HANDLER) { break; } }
    std::printf("\n"); std::fflush(stdout);
    return EXCEPTION_CONTINUE_SEARCH;
}

static int __cdecl AssertHook(int, char* msg, int*) {   // prints the call stack of a failed assert (map the addresses with the linker .map)
    void* fr[16]; const USHORT n = RtlCaptureStackBackTrace(0, 16, fr, nullptr);
    if (oracle::g_softFault) { std::printf("    (assert in the case: %s)\n", msg); if (std::getenv("W2_ASSERT_STACK")) { std::printf("      frames:"); for (USHORT i = 0; i < n; ++i) std::printf(" %p", fr[i]); std::printf("\n"); } RaiseException(0xE0000001u, 0, 0, nullptr); }   // ORACLE-W2: an assert in the port / CRT ends the row like a fault
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
        else if (!std::strcmp(argv[i], "-n") && i + 1 < argc) g_cases = std::atoi(argv[++i]);
        else g_filters.push_back(argv[i]);
    }
    _CrtSetReportHook(AssertHook);
    _set_error_mode(_OUT_TO_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE); _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE); _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
    const char* exe = std::getenv("RW_EXE_ORACLE");
    if (!exe) { std::printf("wave1_oracle_test: set RW_EXE_ORACLE=<path of gta_sa_compact.exe>; skipped (exit 77 = SKIP)\n"); return 77; }
    if (!oracle::Map(exe)) { std::printf("FATAL: cannot map the exe\n"); return 2; }
    AddVectoredExceptionHandler(1, DeepStackHandler);
#ifdef W2_ONLY
    SetUnhandledExceptionFilter(LastChance);
#endif
    *reinterpret_cast<int*>(0xC9C400) = 1;
    oracle::Patch(0x82872C, (void*)&HostMathErr);
    oracle::Patch(0x827B3D, (void*)&HostGetPtd);
#ifdef W2_ONLY
    oracle::g_softFault = true;
    std::printf("wave2: %d exe CRT-heap / lock / TLS imports redirected to the deterministic arena\n", oracle::alloc::InstallExe(exe));
#endif
    for (const unsigned a : kExeFloatInits) reinterpret_cast<void (*)()>(static_cast<uintptr_t>(a))();   // the exe's derived float constants (as after its CRT start-up, PC=53)
    DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &g_mainThread, 0, FALSE, DUPLICATE_SAME_ACCESS);
    CreateThread(nullptr, 0, WatchdogProc, nullptr, 0, nullptr);
    std::printf("wave1_oracle_test: %d cases per function and precision mode; PC24 = game mode (D3D CreateDevice), PC53 = CRT default\n", g_cases);
#ifdef W2_ONLY
    TestW2();
#elif defined(W1_CONSTS_ONLY)
    TestConsts();
#else
    TestSurface();
    TestCamera();
    TestPeds();
    TestAi();
    TestEntities();
    TestFightPreArray();
    TestScriptResetIP();
#endif
    int bad24 = 0, bad53 = 0, hard24 = 0, hard53 = 0;
    int faults = 0;
    for (auto& r : g_rows) { bad24 += r.bad24; bad53 += r.bad53; hard24 += r.hardReg24 + r.hardSpec24; hard53 += r.hardReg53 + r.hardSpec53; faults += r.fault; }
    if (faults) std::printf("\n%d rows NOT ORACLE-ABLE (fault, see above)\n", faults);
    std::printf("\n%zu functions, mismatches (strict / excluding NaN-payload-only): PC24 %d / %d, PC53 %d / %d\n", g_rows.size(), bad24, hard24, bad53, hard53);
    return hard24 ? 1 : 0;
}

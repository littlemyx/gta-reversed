// FIXED-ORACLE: FixedFloat / FixedVector / FixedQuat users against the exe machine code: the exe decompresses with `fild; fmul [float 1/N]` (not a division by N) and
// compresses with `_ftol2` (truncation, low dword). Same machinery as review_oracle_test (see there and game_oracle.h).
// usage: fixed_oracle_test.exe [-v] [-v53] [-vn (also NaN-payload-only)] [-n cases] [name-substring ...]     (exit code 0 = no real mismatch at PC=24)
#include "game_oracle.h"

#include "General.h"
#include "Matrix.h"
#include "Cover.h"
#include "CoverPoint.h"
#include "Occlusion.h"
#include "Occluder.h"
#include "ActiveOccluder.h"
#include "Camera.h"
#include "Animation/AnimBlendSequence.h"
#include "CompressedMatrixNotAligned.h"
#include "Fx/FxPrimBP.h"
#include "Audio/Entities/AEVehicleAudioEntity.h"
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

CCamera& TheCamera = StaticRef<CCamera>(0xB6F028);   // Camera.cpp is not part of this test
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
#pragma init_seg(compiler)   // run BEFORE the static initialisers of the game TUs (CMaths fills its sine table at 0xBB3E00 during static init, which is inside the not yet RWX pad image)
struct EarlyUnprotect { EarlyUnprotect() { DWORD o; VirtualProtect(reinterpret_cast<void*>(0x401000), 0xCB0000 - 0x401000, PAGE_EXECUTE_READWRITE, &o); } } g_earlyUnprotect;


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


// ---------------------------------------------------------------------------------------------------------------------------------
static void TestOcclusion() {
    Run("COcclusion::AddOne 0x71DCD0", [](Rng& r, std::string& d) {
        const unsigned occ[2] = { 0xC73FA0, 0xC73CC8 };
        const bool interior = r.below(2);
        auto sc = [&] { return r.below(4) ? GenF(r, PickScale(r) * 1000.f) : GenF(r, 2000.f); };
        const float a[9] = { GenF(r, r.below(3) ? 3000.f : 9000.f), GenF(r, 3000.f), GenF(r, 500.f), sc(), sc(), sc(), GenF(r, 720.f, S_ZERO | S_TINY | S_DEN), GenF(r, 400.f, S_ZERO | S_TINY | S_DEN), GenF(r, 5000.f, S_ZERO | S_TINY | S_DEN) };
        const uint32 flags = r.below(2) ? 0 : r.u32();
        auto Reset = [&] {
            std::memset(PtrAt<void>(occ[interior]), 0, 0x12);
            *PtrAt<uint32>(0xC73F98) = 0; *PtrAt<uint32>(0xC73CC4) = 0; *PtrAt<int16>(0x8D5D68) = -1;
        };
        auto Grab = [&] { auto v = Snap(occ[interior], 0x12); const auto c = Snap(0xC73F98, 4), c2 = Snap(0xC73CC4, 4), f = Snap(0x8D5D68, 2); v.insert(v.end(), c.begin(), c.end()); v.insert(v.end(), c2.begin(), c2.end()); v.insert(v.end(), f.begin(), f.end()); return v; };
        Reset();
        COcclusion::AddOne(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], flags, interior);
        const auto p = Grab();
        Reset();
        oracle::Fn<void __cdecl(float, float, float, float, float, float, float, float, float, uint32, bool)>(0x71DCD0)(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], flags, interior);
        const auto e = Grab();
        if (SameMem(p, e, "occluder", d)) return true;
        d += " in {" + F(a[0]) + "," + F(a[1]) + "," + F(a[2]) + " size " + F(a[3]) + "," + F(a[4]) + "," + F(a[5]) + " rot " + F(a[6]) + "," + F(a[7]) + "," + F(a[8]) + "}"; return false;
    });
    Run("COccluder::NearCamera 0x71F960", [](Rng& r, std::string& d) {
        uint8 blob[0x12]{};
        const auto rb = RandBytes(r, sizeof blob); std::memcpy(blob, rb.data(), sizeof blob);
        const CVector cam = GenV(r, 3000.f, S_ZERO | S_TINY);
        *PtrAt<void*>(0xB6F028 + 0x14) = nullptr;                 // CPlaceable::m_matrix
        *PtrAt<CVector>(0xB6F028 + 4) = cam;                      // m_placement.m_vPosn
        if (r.below(3)) for (int i = 0; i < 3; ++i) reinterpret_cast<int16*>(blob)[i] = (int16)((int)(((float*)&cam)[i] * 4.f) + (int)r.below(2400) - 1200);   // near the camera
        if (r.below(3)) for (int i = 3; i < 6; ++i) reinterpret_cast<int16*>(blob)[i] = (int16)r.below(3000);
        const bool a = reinterpret_cast<const COccluder*>(blob)->NearCamera();
        const bool b = oracle::Fn<uint32 __fastcall(void*, int)>(0x71F960)(blob, 0) & 0xFF;
        Hit(a);
        if (a == b) return true;
        d = "cam " + V(cam) + " got " + std::to_string(a) + " exe " + std::to_string(b); return false;
    });
}


// shared set-up of the occluder fixtures: view matrix, screen size, scratch statics
static void SetupOcclView(Rng& r) {
    *PtrAt<int32>(0xC17044) = 640; *PtrAt<int32>(0xC17048) = 480; *PtrAt<float>(0x8D5038) = 70.f;
    CMatrix& vm = TheCamera.m_mViewMatrix;
    vm.GetRight() = GenV(r, 1.f, S_ZERO | S_TINY); vm.GetForward() = GenV(r, 1.f, S_ZERO | S_TINY); vm.GetUp() = GenV(r, 1.f, S_ZERO | S_TINY);
    vm.GetPosition() = GenV(r, 30.f, S_ZERO | S_TINY);
    vm.GetForward().y = 1.f - vm.GetForward().y * 0.2f;
}
static void TestOccluders() {
    Run("COccluder::ProcessLineSegment 0x71E130", [](Rng& r, std::string& d) {
        SetupOcclView(r);
        for (int i = 0; i < 8; ++i) {
            PtrAt<uint8>(0xC73CB0)[i] = r.below(3) != 0;
            PtrAt<CVector>(0xC798E0)[i] = GenV(r, 30.f, S_ZERO | S_TINY);
            PtrAt<CVector>(0xC79950)[i] = CVector{ GenF(r, 500.f, S_ZERO), GenF(r, 400.f, S_ZERO), 1.f + r.f01() * 300.f };
        }
        *PtrAt<CVector>(0xC79940) = CVector{ GenF(r, 500.f, S_ZERO), GenF(r, 400.f, S_ZERO), r.f01() * 300.f };
        *PtrAt<float>(0xC73CA0) = GenF(r, 500.f, S_ZERO); *PtrAt<float>(0xC73CA4) = GenF(r, 500.f, S_ZERO); *PtrAt<float>(0xC73CA8) = GenF(r, 500.f, S_ZERO); *PtrAt<float>(0xC73CAC) = GenF(r, 500.f, S_ZERO);
        alignas(16) uint8 act1[0xAC], act2[0xAC];
        const auto rb = RandBytes(r, 0xAC); std::memcpy(act1, rb.data(), 0xAC); act1[0x7A] = (uint8)r.below(6);   // m_LinesUsed
        std::memcpy(act2, act1, 0xAC);
        const int i0 = r.below(4), i1 = r.below(4);
        alignas(16) uint8 occ[0x12]{};
        const auto statics = Snap(0xC73CA0, 16);
        if (std::getenv("FIXED_TRACE")) std::printf("  [port ProcessLineSegment %d %d]\n", i0, i1);
        const bool a = reinterpret_cast<COccluder*>(occ)->ProcessLineSegment(i0, i1, reinterpret_cast<CActiveOccluder*>(act1));
        if (std::getenv("FIXED_TRACE")) std::printf("  [exe]\n");
        const auto sa = Snap(0xC73CA0, 16);
        Put(0xC73CA0, statics);
        const bool b = oracle::Fn<uint32 __fastcall(void*, int, int, int, void*)>(0x71E130)(occ, 0, i0, i1, act2) & 0xFF;
        const auto sb = Snap(0xC73CA0, 16);
        Hit(a);
        if (a != b) { d = "returned " + std::to_string(a) + " exe " + std::to_string(b); return false; }
        return SameMem(std::vector<uint8_t>(act1, act1 + 0xAC), std::vector<uint8_t>(act2, act2 + 0xAC), "activeOccluder", d) && SameMem(sa, sb, "min/max", d);
    });
    Run("COccluder::ProcessOneOccluder 0x71E5D0", [](Rng& r, std::string& d) {
        SetupOcclView(r);
        alignas(16) uint8 occ[0x12]{};
        auto* h = reinterpret_cast<int16*>(occ);
        const CVector c = GenV(r, 40.f, S_ZERO | S_TINY);
        h[0] = (int16)(c.x * 4.f); h[1] = (int16)(c.y * 4.f); h[2] = (int16)(c.z * 4.f);
        const int shape = r.below(4);   // 0: all dims, 1..3: one flat dimension
        for (int i = 3; i < 6; ++i) h[i] = (int16)(1 + r.below(r.below(4) ? 120 : 1200));
        if (shape) h[2 + shape] = 0;
        for (int i = 12; i < 15; ++i) occ[i] = (uint8)r.u32();
        alignas(16) uint8 act1[0xAC], act2[0xAC];
        const auto rb = RandBytes(r, 0xAC); std::memcpy(act1, rb.data(), 0xAC); std::memcpy(act2, act1, 0xAC);
        const auto statics = Snap(0xC73CA0, 0x100), arrays = Snap(0xC798E0, 0xB0);
        const bool a = reinterpret_cast<COccluder*>(occ)->ProcessOneOccluder(reinterpret_cast<CActiveOccluder*>(act1));
        const auto sa = Snap(0xC73CA0, 0x100), aa = Snap(0xC798E0, 0xB0);
        Put(0xC73CA0, statics); Put(0xC798E0, arrays);
        const bool b = oracle::Fn<uint32 __fastcall(void*, int, void*)>(0x71E5D0)(occ, 0, act2) & 0xFF;
        const auto sb = Snap(0xC73CA0, 0x100), ab = Snap(0xC798E0, 0xB0);
        Hit(a);
        if (a != b) { d = "returned " + std::to_string(a) + " exe " + std::to_string(b); return false; }
        return SameMem(std::vector<uint8_t>(act1, act1 + 0xAC), std::vector<uint8_t>(act2, act2 + 0xAC), "activeOccluder", d) && SameMem(sa, sb, "statics", d) && SameMem(aa, ab, "coors", d);
    });
}

// ---------------------------------------------------------------------------------------------------------------------------------
static void TestAnim() {
    for (const bool trans : { false, true }) {
        const std::string tag = trans ? " (trans)" : "";
        Run("CAnimBlendSequence::Uncompress 0x4D0D40" + tag, [trans](Rng& r, std::string& d) {
            const size_t n = 1 + r.below(4), inSz = trans ? 16 : 10, outSz = trans ? 0x20 : 0x14;
            auto in = RandBytes(r, n * inSz);
            if (r.below(3) == 0) for (auto& b : in) b = (uint8)(r.below(2) ? 0 : 0xFF);   // extremes
            std::vector<uint8_t> o1(n * outSz + 8, 0xAB), o2 = o1;
            alignas(16) uint8 s1[0xC]{}, s2[0xC]{};
            auto* q1 = reinterpret_cast<CAnimBlendSequence*>(s1);
            q1->m_bHasTranslation = trans; q1->m_bIsCompressed = true; q1->m_bUsingExternalMemory = true; q1->m_FramesNum = (uint16)n; q1->m_Frames = in.data();
            std::memcpy(s2, s1, sizeof s1);
            q1->Uncompress(o1.data());
            oracle::Fn<void __fastcall(void*, int, uint8*)>(0x4D0D40)(s2, 0, o2.data());
            if (!SameMem(o1, o2, "frames", d)) { d += " in " + Hex(in.data(), in.size()) + " got " + Hex(o1.data(), o1.size()) + " exe " + Hex(o2.data(), o2.size()); return false; }
            if (std::memcmp(s1, s2, 8)) { d = "sequence flags differ"; return false; }
            return true;
        });
        Run("CAnimBlendSequence::CompressKeyframes 0x4D0F40" + tag, [trans](Rng& r, std::string& d) {
            const size_t n = 1 + r.below(4), inSz = trans ? 0x20 : 0x14, outSz = trans ? 16 : 10;
            std::vector<float> fr(n * inSz / 4);
            for (size_t i = 0; i < n; ++i) {
                float* k = &fr[i * inSz / 4];
                for (int j = 0; j < 4; ++j) k[j] = GenF(r, r.below(4) ? 1.f : 20.f);
                k[4] = r.below(4) ? r.f01() * 2.f : GenF(r, PickScale(r) * 100.f);
                if (trans) for (int j = 5; j < 8; ++j) k[j] = GenF(r, PickScale(r) * 10.f);
            }
            std::vector<uint8_t> o1(n * outSz + 8, 0xAB), o2 = o1;
            alignas(16) uint8 s1[0xC]{}, s2[0xC]{};
            auto* q1 = reinterpret_cast<CAnimBlendSequence*>(s1);
            q1->m_bHasTranslation = trans; q1->m_bIsCompressed = false; q1->m_bUsingExternalMemory = true; q1->m_FramesNum = (uint16)n; q1->m_Frames = fr.data();
            std::memcpy(s2, s1, sizeof s1);
            q1->CompressKeyframes(o1.data());
            oracle::Fn<void __fastcall(void*, int, uint8*)>(0x4D0F40)(s2, 0, o2.data());
            if (!SameMem(o1, o2, "frames", d)) { char b[200]; std::snprintf(b, sizeof b, " frame0 %s %s %s %s dt %s", F(fr[0]).c_str(), F(fr[1]).c_str(), F(fr[2]).c_str(), F(fr[3]).c_str(), F(fr[4]).c_str()); d += b; return false; }
            if (std::memcmp(s1, s2, 8)) { d = "sequence flags differ"; return false; }
            return true;
        });
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------
static void TestMatrixCover() {
    Run("CCompressedMatrixNotAligned::CompressFromFullMatrix 0x59BAD0", [](Rng& r, std::string& d) {
        CMatrix m{};
        const float sc = r.below(3) ? 1.f : 1.5f;
        m.GetRight() = GenV(r, sc); m.GetForward() = GenV(r, sc); m.GetPosition() = GenV(r, 3000.f);
        alignas(16) uint8 a[20]{}, b[20]{};
        reinterpret_cast<CCompressedMatrixNotAligned*>(a)->CompressFromFullMatrix(m);
        oracle::Fn<void __fastcall(void*, int, const CMatrix*)>(0x59BAD0)(b, 0, &m);
        if (SameMem(std::vector<uint8_t>(a, a + 20), std::vector<uint8_t>(b, b + 20), "compressed", d)) return true;
        d += " right " + V(m.GetRight()) + " fwd " + V(m.GetForward()); return false;
    });
    Run("CCompressedMatrixNotAligned::DecompressIntoFullMatrix 0x59B9F0", [](Rng& r, std::string& d) {
        alignas(16) uint8 c[20]{};
        const auto rb = RandBytes(r, 20); std::memcpy(c, rb.data(), 20);
        if (r.below(4) == 0) std::memset(c + 12, r.below(2) ? 0x7F : 0x81, 8);
        const CVector pos = GenV(r, 100.f, S_ZERO | S_TINY | S_DEN); std::memcpy(c, &pos, 12);
        CMatrix a{}, b{};
        reinterpret_cast<const CCompressedMatrixNotAligned*>(c)->DecompressIntoFullMatrix(a);
        oracle::Fn<void __fastcall(const void*, int, CMatrix*)>(0x59B9F0)(c, 0, &b);
        if (SameBlob(&a, &b, 0x40)) return true;
        d = "right " + V(a.GetRight()) + " exe " + V(b.GetRight()); return false;
    });
    Run("FxPrimBP_c::GetRWMatrix 0x4A9DC0", [](Rng& r, std::string& d) {
        alignas(16) uint8 bp[0x100]{};
        int16 buf[12];
        for (auto& v : buf) v = (int16)(r.below(6) == 0 ? (r.below(2) ? 32767 : -32768) : r.u32());
        *reinterpret_cast<void**>(bp + 8) = r.below(8) ? (void*)buf : nullptr;     // m_pMatrixBuffered
        alignas(16) uint8 a[0x40], b[0x40];
        std::memset(a, 0xAB, sizeof a); std::memset(b, 0xAB, sizeof b);
        reinterpret_cast<FxPrimBP_c*>(bp)->GetRWMatrix(*reinterpret_cast<RwMatrix*>(a));
        oracle::Fn<void __fastcall(void*, int, void*)>(0x4A9DC0)(bp, 0, b);
        return SameMem(std::vector<uint8_t>(a, a + 0x40), std::vector<uint8_t>(b, b + 0x40), "RwMatrix", d) || (d += " raw0 " + std::to_string(buf[0]), false);
    });
    Run("CCover::AddCoverPoint 0x698F30", [](Rng& r, std::string& d) {
        auto* pts = PtrAt<uint8>(0xC197C8);
        std::memset(pts, 0, 100 * 0x1C);
        const int n = r.below(3) ? r.below(6) : r.below(100);
        const CVector base = GenV(r, 100.f, S_ZERO | S_TINY);
        uint32 used = 0;
        for (int i = 0; i < n; ++i) {
            uint8* p = pts + i * 0x1C;
            p[0] = (uint8)(r.below(4)); p[1] = (uint8)r.below(3); p[2] = (uint8)r.u32();
            const CVector pp = r.below(3) ? base + CVector{ (r.f01() - 0.5f) * 6.f, (r.f01() - 0.5f) * 6.f, (r.f01() - 0.5f) * 2.f } : GenV(r, 100.f);
            std::memcpy(p + 4, &pp, 12);
            used += p[0] != 0;
        }
        *PtrAt<uint32>(0xC197A4) = used;
        const CVector pos = r.below(3) ? base + CVector{ (r.f01() - 0.5f) * 5.f, (r.f01() - 0.5f) * 5.f, (r.f01() - 0.5f) * 2.f } : GenV(r, 100.f);
        const auto dir = (uint8)r.u32();
        const auto usage = (CCoverPoint::eUsage)r.below(3);
        const auto snapshot = Snap(0xC197C8, 100 * 0x1C); const auto cnt = Snap(0xC197A4, 4);
        CCoverPoint* ra = CCover::AddCoverPoint(CCoverPoint::eType::POINTONMAP, nullptr, &pos, usage, CCoverPoint::Dir{ dir });
        auto after1 = Snap(0xC197C8, 100 * 0x1C); const auto cnt1 = Snap(0xC197A4, 4);
        Put(0xC197C8, snapshot); Put(0xC197A4, cnt);
        CCoverPoint* rb = oracle::Fn<CCoverPoint* __cdecl(int, void*, const CVector*, int, uint32)>(0x698F30)(3, nullptr, &pos, (int)usage, dir);
        auto after2 = Snap(0xC197C8, 100 * 0x1C); const auto cnt2 = Snap(0xC197A4, 4);
        Hit(ra != nullptr && ra - reinterpret_cast<CCoverPoint*>(pts) < n);
        (void)rb;   // the exe returns whatever EAX holds (the function is `void` in effect: 0x699118 does not set it), only the resulting state is compared
        return SameMem(after1, after2, "points", d) && SameMem(cnt1, cnt2, "count", d);
    });
    Run("CAEVehicleAudioEntity::GetAccelAndBrake 0x4F5080 (replay)", [](Rng& r, std::string& d) {
        *PtrAt<uint8>(0xA43088) = 1;   // CReplay::Mode = MODE_PLAYBACK
        alignas(16) uint8 ent[0x200]{};
        alignas(16) uint8 veh[0x600]{};
        const float gas = r.below(5) ? GenF(r, 1.5f) : GenF(r, PickScale(r)), brake = r.below(5) ? GenF(r, 1.5f) : GenF(r, PickScale(r));
        std::memcpy(veh + 0x49C, &gas, 4); std::memcpy(veh + 0x4A0, &brake, 4);
        alignas(16) uint8 a[0x4C], b[0x4C];
        const auto rb = RandBytes(r, 0x4C); std::memcpy(a, rb.data(), 0x4C); std::memcpy(b, rb.data(), 0x4C);
        *reinterpret_cast<void**>(a + 0x10) = veh; *reinterpret_cast<void**>(b + 0x10) = veh;
        reinterpret_cast<CAEVehicleAudioEntity*>(ent)->GetAccelAndBrake(*reinterpret_cast<CAEVehicleAudioEntity::tVehicleParams*>(a));
        oracle::Fn<void __fastcall(void*, int, void*)>(0x4F5080)(ent, 0, b);
        *PtrAt<uint8>(0xA43088) = 0;
        if (SameMem(std::vector<uint8_t>(a, a + 0x4C), std::vector<uint8_t>(b, b + 0x4C), "tVehicleParams", d)) return true;
        d += " gas " + F(gas) + " brake " + F(brake); return false;
    });
    Run("CCover::FindDirFromVector 0x698D40", [](Rng& r, std::string& d) {
        const CVector v = GenV(r, PickScale(r));
        const uint8 a = std::bit_cast<uint8>(CCover::FindDirFromVector(v));
        const uint8 b = oracle::Fn<uint32 __cdecl(CVector)>(0x698D40)(v) & 0xFF;
        if (a == b) return true;
        d = "v " + V(v) + " got " + std::to_string(a) + " exe " + std::to_string(b); return false;
    });
    Run("CCover::FindVectorFromDir 0x698D60", [](Rng& r, std::string& d) {
        const uint8 raw = (uint8)r.u32();
        const CVector a = CCover::FindVectorFromDir(CCoverPoint::Dir{ raw });
        CVector b{};
        oracle::Fn<void* __cdecl(CVector*, uint32)>(0x698D60)(&b, raw);
        if (SameV(a, b)) return true;
        d = "raw " + std::to_string(raw) + " got " + V(a) + " exe " + V(b); return false;
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
    if (!exe) { std::printf("fixed_oracle_test: set RW_EXE_ORACLE=<path of gta_sa_compact.exe>; skipped\n"); return 0; }
    if (!oracle::Map(exe)) { std::printf("FATAL: cannot map the exe\n"); return 2; }
    *reinterpret_cast<int*>(0xC9C400) = 1;
    oracle::Patch(0x82872C, (void*)&HostMathErr);
    oracle::Patch(0x827B3D, (void*)&HostGetPtd);
    std::printf("fixed_oracle_test: %d cases per function and precision mode\n", g_cases);
    if (std::getenv("FIXED_SELFTEST")) {
        volatile int16 rr = 20588;
        FixedFloat<int16, 60.f, true> ff{ (int16)rr };
        volatile float rc = std::bit_cast<float>(0x3C888889u);
        const float viaClass = ff, viaMul = (float)rr * rc, viaDiv = (float)rr / 60.f;
        std::printf("selftest: class %s mul %s div %s\n", F(viaClass).c_str(), F(viaMul).c_str(), F(viaDiv).c_str());
    }
    TestOcclusion();
    TestOccluders();
    TestAnim();
    TestMatrixCover();
    int bad24 = 0, bad53 = 0, hard24 = 0, hard53 = 0;
    for (auto& r : g_rows) { bad24 += r.bad24; bad53 += r.bad53; hard24 += r.hardReg24 + r.hardSpec24; hard53 += r.hardReg53 + r.hardSpec53; }
    std::printf("\n%zu functions, mismatches (strict / excluding NaN-payload-only): PC24 %d / %d, PC53 %d / %d\n", g_rows.size(), bad24, hard24, bad53, hard53);
    return hard24 ? 1 : 0;
}

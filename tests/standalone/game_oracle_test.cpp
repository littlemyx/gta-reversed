// P2E: differential test of GAME functions against the original machine code (exe oracle, see game_oracle.h).
// The REAL game TUs (CCollision, CGeneral, CVector, CMatrix, CQuaternion, CAnimBlendNode) are compiled into this exe with the game's flags (x87,
// /arch:IA32) and PCH; the test maps gta_sa_compact.exe (env RW_EXE_ORACLE=<path>) at its original VAs and calls the exe function and the C++ port with the
// same random inputs (fixed seeds, NaN / +-0 / denormal / huge / tiny values included) and compares the results BIT-EXACT, once with the x87 precision
// control at 24 bits (the game runs like that after D3D CreateDevice) and once at 53 bits (the CRT default).
// usage: game_oracle_test.exe [-v] [-v53] [-vn (also NaN-payload-only)] [-n cases] [name-substring ...]     (exit code 0 = no real mismatch at PC=24; NaN-payload-only differences and PC=53 results are only reported)
#include "game_oracle.h"

#include "Collision.h"
#include "ColLine.h"
#include "ColSphere.h"
#include "ColBox.h"
#include "ColTriangle.h"
#include "ColTrianglePlane.h"
#include "ColPoint.h"
#include "General.h"
#include "Matrix.h"
#include "Quaternion.h"
#include "AnimBlendNode.h"
#include "AnimBlendSequence.h"
#include "AnimBlendAssociation.h"
#include "StoredCollPoly.h"
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
// CVector / CGeneral
static void TestVectorGeneral() {
    Run("CVector::Normalise 0x59C910", [](Rng& r, std::string& d) {
        CVector a = GenV(r, PickScale(r)), b = a;
        const CVector in = a;
        a.Normalise();
        oracle::Fn<void __fastcall(CVector*, int)>(0x59C910)(&b, 0);
        if (SameV(a, b)) return true;
        d = "in " + V(in) + " got " + V(a) + " exe " + V(b);
        return false;
    });
    Run("CVector::NormaliseAndMag 0x59C970", [](Rng& r, std::string& d) {
        CVector a = GenV(r, PickScale(r)), b = a, in = a;
        const float ma = a.NormaliseAndMag();
        const float mb = oracle::Fn<float __fastcall(CVector*, int)>(0x59C970)(&b, 0);
        if (SameV(a, b) && SameF(ma, mb)) return true;
        d = "in " + V(in) + " got " + V(a) + " mag " + F(ma) + " exe " + V(b) + " mag " + F(mb);
        return false;
    });
    Run("CVector::Magnitude 0x4082C0", [](Rng& r, std::string& d) {
        const CVector a = GenV(r, PickScale(r));
        const float ma = a.Magnitude();
        const float mb = oracle::Fn<float __fastcall(const CVector*, int)>(0x4082C0)(&a, 0);
        if (SameF(ma, mb)) return true;
        d = "in " + V(a) + " got " + F(ma) + " exe " + F(mb);
        return false;
    });

    // angle helpers (cdecl float(float...)); the limiters loop `value / 360` times => bounded inputs
    Run("CVector::Magnitude2D 0x406D50", [](Rng& r, std::string& d) {
        const CVector a = GenV(r, PickScale(r));
        const float ma = a.Magnitude2D();
        const float mb = oracle::Fn<float __fastcall(const CVector*, int)>(0x406D50)(&a, 0);
        if (SameF(ma, mb)) return true;
        d = "in " + V(a) + " got " + F(ma) + " exe " + F(mb);
        return false;
    });

    Run("CGeneral::LimitAngle 0x53CB00", [](Rng& r, std::string& d) {
        const float x = GenF(r, r.below(2) ? 1000.f : 1e5f, S_NOHUGE);
        const float a = CGeneral::LimitAngle(x), b = oracle::Fn<float __cdecl(float)>(0x53CB00)(x);
        if (SameF(a, b)) return true;
        d = "in " + F(x) + " got " + F(a) + " exe " + F(b);
        return false;
    });
    Run("CGeneral::LimitRadianAngle 0x53CB50", [](Rng& r, std::string& d) {
        const float x = GenF(r, r.below(2) ? 30.f : 1e5f);
        const float a = CGeneral::LimitRadianAngle(x), b = oracle::Fn<float __cdecl(float)>(0x53CB50)(x);
        if (SameF(a, b)) return true;
        d = "in " + F(x) + " got " + F(a) + " exe " + F(b);
        return false;
    });
    Run("CGeneral::GetRadianAngleBetweenPoints 0x53CBE0", [](Rng& r, std::string& d) {
        const float s = PickScale(r), p[4] = { GenF(r, s), GenF(r, s), GenF(r, s), GenF(r, s) };
        const float a = CGeneral::GetRadianAngleBetweenPoints(p[0], p[1], p[2], p[3]);
        const float b = oracle::Fn<float __cdecl(float, float, float, float)>(0x53CBE0)(p[0], p[1], p[2], p[3]);
        if (SameF(a, b)) return true;
        d = "in " + F(p[0]) + "," + F(p[1]) + "," + F(p[2]) + "," + F(p[3]) + " got " + F(a) + " exe " + F(b);
        return false;
    });
    Run("CGeneral::GetATanOfXY 0x53CC70", [](Rng& r, std::string& d) {
        const float s = PickScale(r), x = GenF(r, s), y = r.below(8) ? GenF(r, s) : x * (r.below(2) ? 1.f : -1.f);
        const float a = CGeneral::GetATanOfXY(x, y), b = oracle::Fn<float __cdecl(float, float)>(0x53CC70)(x, y);
        if (SameF(a, b)) return true;
        d = "in " + F(x) + "," + F(y) + " got " + F(a) + " exe " + F(b);
        return false;
    });
    Run("CGeneral::GetNodeHeadingFromVector 0x53CDC0", [](Rng& r, std::string& d) {
        const float s = PickScale(r), x = GenF(r, s), y = GenF(r, s);
        const uint32 a = CGeneral::GetNodeHeadingFromVector(x, y), b = oracle::Fn<uint32 __cdecl(float, float)>(0x53CDC0)(x, y);
        if (a == b) return true;
        char t[64]; std::snprintf(t, sizeof t, " got %u exe %u", a, b);
        d = "in " + F(x) + "," + F(y) + t;
        return false;
    });
    Run("CGeneral::GetAngleBetweenPoints 0x53CEA0", [](Rng& r, std::string& d) {
        const float s = PickScale(r), p[4] = { GenF(r, s), GenF(r, s), GenF(r, s), GenF(r, s) };
        const float a = CGeneral::GetAngleBetweenPoints(p[0], p[1], p[2], p[3]);
        const float b = oracle::Fn<float __cdecl(float, float, float, float)>(0x53CEA0)(p[0], p[1], p[2], p[3]);
        if (SameF(a, b)) return true;
        d = "in " + F(p[0]) + "," + F(p[1]) + "," + F(p[2]) + "," + F(p[3]) + " got " + F(a) + " exe " + F(b);
        return false;
    });

    // random numbers: both sides run the same LCG (the exe on a fake per-thread block), seeded identically
    Run("CGeneral::GetRandomNumberInRange<float> 0x41BD90", [](Rng& r, std::string& d) {
        const float s = PickScale(r), lo = GenF(r, s), hi = GenF(r, s);
        const unsigned seed = r.u32();
        SeedBoth(seed); const float a = CGeneral::GetRandomNumberInRange(lo, hi);
        SeedBoth(seed); const float b = oracle::Fn<float __cdecl(float, float)>(0x41BD90)(lo, hi);
        if (SameF(a, b)) return true;
        d = "seed " + std::to_string(seed) + " in " + F(lo) + "," + F(hi) + " got " + F(a) + " exe " + F(b);
        return false;
    });
    Run("CGeneral::GetRandomNumberInRange<int> 0x407180", [](Rng& r, std::string& d) {
        const int32 lo = (int32)r.u32() % (r.below(2) ? 100 : 100000), hi = lo + (int32)(r.u32() % (r.below(3) ? 50 : 70000)) - (r.below(8) == 0 ? 60 : 0);
        const unsigned seed = r.u32();
        SeedBoth(seed); const int32 a = CGeneral::GetRandomNumberInRange(lo, hi);
        SeedBoth(seed); const int32 b = oracle::Fn<int32 __cdecl(int32, int32)>(0x407180)(lo, hi);
        if (a == b) return true;
        d = "seed " + std::to_string(seed) + " in " + std::to_string(lo) + "," + std::to_string(hi) + " got " + std::to_string(a) + " exe " + std::to_string(b);
        return false;
    });
}

// ---------------------------------------------------------------------------------------------------------------------------------
// CMatrix / CQuaternion
struct RawMat { CVector right; uint32 f0; CVector fwd; uint32 f1; CVector up; uint32 f2; CVector pos; uint32 f3; };
static RawMat GenMat(Rng& r, float s) {
    RawMat m{};
    m.right = GenV(r, s); m.fwd = GenV(r, s); m.up = GenV(r, s); m.pos = GenV(r, s);
    return m;
}
static CMatrix ToCMatrix(const RawMat& m) { return CMatrix(m.pos, m.right, m.fwd, m.up); }

// dot == -1 exactly (antipodal quaternions): the CRT acos returns fldpi, and at PC != 53 its exit path 0x828F9B calls the inexact-exception handler 0x828EA7 only when the STICKY
// FPU status flag PE is set, which rounds theta to a double (fstp/fld qword) before the fsin => sin(pi) = 1.2e-16 or -5.4e-20 depending on process FPU state. Not reproducible, skipped.
static bool AntipodalDot(const CQuaternion& a, const CQuaternion& b) {
    float dot = (float)(((double)a.w * b.w + (double)a.z * b.z + (double)a.y * b.y) + (double)a.x * b.x);
    return dot == -1.0f;
}

static void TestMatrixQuat() {
    Run("CMatrix::TransformVector  vs Multiply3x3(out,m,v) 0x59C790", [](Rng& r, std::string& d) {
        const float s = PickScale(r); const RawMat m = GenMat(r, s); const CVector v = GenV(r, PickScale(r));
        const CMatrix cm = ToCMatrix(m);
        const CVector a = cm.TransformVector(v);
        CVector b;
        oracle::Fn<CVector * __cdecl(CVector*, const void*, const CVector*)>(0x59C790)(&b, &m, &v);
        if (SameV(a, b)) return true;
        d = "v " + V(v) + " got " + V(a) + " exe " + V(b);
        return false;
    });
    Run("CMatrix::InverseTransformVector vs Multiply3x3(out,v,m) 0x59C810", [](Rng& r, std::string& d) {
        const float s = PickScale(r); const RawMat m = GenMat(r, s); const CVector v = GenV(r, PickScale(r));
        const CMatrix cm = ToCMatrix(m);
        const CVector a = cm.InverseTransformVector(v);
        CVector b;
        oracle::Fn<CVector * __cdecl(CVector*, const CVector*, const void*)>(0x59C810)(&b, &v, &m);
        if (SameV(a, b)) return true;
        d = "v " + V(v) + " got " + V(a) + " exe " + V(b);
        return false;
    });
    Run("CMatrix::TransformPoint vs MultiplyMatrixWithVector 0x59C890", [](Rng& r, std::string& d) {
        const float s = PickScale(r); const RawMat m = GenMat(r, s); const CVector v = GenV(r, PickScale(r));
        const CMatrix cm = ToCMatrix(m);
        const CVector a = cm.TransformPoint(v);
        CVector b;
        oracle::Fn<CVector * __cdecl(CVector*, const void*, const CVector*)>(0x59C890)(&b, &m, &v);
        if (SameV(a, b)) return true;
        d = "v " + V(v) + " got " + V(a) + " exe " + V(b);
        return false;
    });

    auto genQuat = [](Rng& r) {
        CQuaternion q;
        const float s = r.below(4) ? 1.f : PickScale(r);
        q.x = GenF(r, s); q.y = GenF(r, s); q.z = GenF(r, s); q.w = GenF(r, s);
        if (r.below(3)) { // unit quaternion
            const double l = std::sqrt((double)q.x * q.x + (double)q.y * q.y + (double)q.z * q.z + (double)q.w * q.w);
            if (l > 0.0 && std::isfinite(l)) { q.x = (float)(q.x / l); q.y = (float)(q.y / l); q.z = (float)(q.z / l); q.w = (float)(q.w / l); }
        }
        return q;
    };
    Run("CQuaternion::Slerp(from,to,t) 0x59C630", [&](Rng& r, std::string& d) {
        CQuaternion from = genQuat(r), to = r.below(6) == 0 ? from : genQuat(r);
        const float t = r.below(2) ? r.f01() : GenF(r, 2.f);
        if (AntipodalDot(from, to)) return true;
        CQuaternion a, b;
        a.Slerp(from, to, t);
        oracle::Fn<void __fastcall(CQuaternion*, int, const CQuaternion*, const CQuaternion*, float)>(0x59C630)(&b, 0, &from, &to, t);
        if (SameQ(a, b)) return true;
        d = "from " + Q(from) + " to " + Q(to) + " t " + F(t) + " got " + Q(a) + " exe " + Q(b);
        return false;
    });
    Run("CQuaternion::Slerp(from,to,theta,invSin,t) 0x59C300", [&](Rng& r, std::string& d) {
        CQuaternion from = genQuat(r), to = genQuat(r);
        const float th = r.below(4) == 0 ? GenF(r, 4.f) : r.f01() * 3.2f, inv = r.below(4) == 0 ? GenF(r, 50.f) : 1.0f / std::sin(th), t = r.below(2) ? r.f01() : GenF(r, 2.f);
        CQuaternion a, b;
        a.Slerp(from, to, th, inv, t);
        oracle::Fn<void __fastcall(CQuaternion*, int, const CQuaternion*, const CQuaternion*, float, float, float)>(0x59C300)(&b, 0, &from, &to, th, inv, t);
        if (SameQ(a, b)) return true;
        d = "from " + Q(from) + " to " + Q(to) + " th " + F(th) + " inv " + F(inv) + " t " + F(t) + " got " + Q(a) + " exe " + Q(b);
        return false;
    });
    Run("CAnimBlendNode::CalcTheta vs CalcThetaFromQuats 0x4D00E0", [&](Rng& r, std::string& d) {
        CQuaternion from = genQuat(r), to = r.below(6) == 0 ? from : genQuat(r);
        if (AntipodalDot(from, to)) return true;
        alignas(16) uint8 nodeBuf[sizeof(CAnimBlendNode)]{};
        auto& node = *reinterpret_cast<CAnimBlendNode*>(nodeBuf);
        node.CalcTheta(from, to);
        float th = -1234.f, inv = -4321.f;
        oracle::Fn<void __cdecl(const CQuaternion*, const CQuaternion*, float*, float*)>(0x4D00E0)(&from, &to, &th, &inv);
        const float* mine = reinterpret_cast<const float*>(nodeBuf);   // m_Theta / m_InvSinTheta, looked up by value below
        (void)mine;
        float a0 = 0, a1 = 0;
        std::memcpy(&a0, nodeBuf + offsetof(CAnimBlendNode, m_Theta), 4);
        std::memcpy(&a1, nodeBuf + offsetof(CAnimBlendNode, m_InvSinTheta), 4);
        if (SameF(a0, th) && SameF(a1, inv)) return true;
        d = "from " + Q(from) + " to " + Q(to) + " got " + F(a0) + "," + F(a1) + " exe " + F(th) + "," + F(inv);
        return false;
    });
}


// ---------------------------------------------------------------------------------------------------------------------------------
// CAnimBlendNode key-frame math on hand-built sequences / associations (raw memory with the real layouts)
static void TestAnimNode() {
    struct NodeIn {
        alignas(16) uint8 seqBuf[sizeof(CAnimBlendSequence)] {};
        alignas(16) uint8 assocBuf[sizeof(CAnimBlendAssociation)] {};
        alignas(16) uint8 frames[6 * sizeof(KeyFrameTransCompressed) + 6 * sizeof(KeyFrameTrans)] {};
        CAnimBlendNode node{};
        float weight = 0.f;
    };
    auto build = [](Rng& r, NodeIn& n, bool compressed, bool* needFrames = nullptr) {
        auto& seq = *reinterpret_cast<CAnimBlendSequence*>(n.seqBuf);
        auto& assoc = *reinterpret_cast<CAnimBlendAssociation*>(n.assocBuf);
        const int nf = 1 + r.below(5);
        seq.m_bHasRotation = 1; seq.m_bHasTranslation = r.below(8) != 0; seq.m_bIsCompressed = compressed; seq.m_FramesNum = (uint16)nf; seq.m_Frames = n.frames;
        const float ts = r.below(3) ? 20.f : PickScale(r);
        if (compressed) {
            auto* kf = reinterpret_cast<uint8*>(n.frames);
            for (size_t i = 0; i < sizeof n.frames; ++i) kf[i] = (uint8)r.u32();
            if (r.below(4)) for (int i = 0; i < nf; ++i) { // mostly sane delta times / quaternions
                auto* k = reinterpret_cast<int16_t*>(seq.m_bHasTranslation ? (uint8*)n.frames + i * sizeof(KeyFrameTransCompressed) : (uint8*)n.frames + i * sizeof(KeyFrameCompressed));
                k[4] = (int16_t)(r.u32() % 400);
            }
        } else {
            auto* k = reinterpret_cast<KeyFrameTrans*>(n.frames);
            const size_t stride = seq.m_bHasTranslation ? sizeof(KeyFrameTrans) : sizeof(KeyFrame);
            for (int i = 0; i < nf; ++i) {
                auto* f = reinterpret_cast<KeyFrameTrans*>((uint8*)n.frames + i * stride);
                f->Rot = CQuaternion(GenF(r, 1.f), GenF(r, 1.f), GenF(r, 1.f), GenF(r, 1.f));
                f->DeltaTime = r.below(8) ? r.f01() * 0.5f : GenF(r, 2.f);
                if (seq.m_bHasTranslation) f->Trans = GenV(r, ts);
            }
            (void)k;
        }
        assoc.m_BlendAmount = r.below(6) ? r.f01() * 1.5f : GenF(r, 2.f);
        assoc.m_Flags = (uint16)(r.u32() & 0x7FFF);
        n.weight = r.below(4) ? r.f01() : GenF(r, 2.f);
        n.node.m_Seq = &seq; n.node.m_BlendAssoc = &assoc;
        n.node.m_KFCurr = (int16)r.below(nf); n.node.m_KFPrev = (int16)r.below(nf);
        n.node.m_KFRemainingTime = r.below(6) ? r.f01() * 0.5f : GenF(r, 2.f);
        (void)needFrames;
    };
    auto trans = [&](const char* name, unsigned va, bool compressed, void (*port)(CAnimBlendNode&, CVector&, float)) {
        Run(name, [=](Rng& r, std::string& d) {
            NodeIn n; build(r, n, compressed);
            CVector a{ 7, 8, 9 }, b{ 7, 8, 9 };
            port(n.node, a, n.weight);
            oracle::Fn<void __fastcall(CAnimBlendNode*, int, CVector*, float)>(va)(&n.node, 0, &b, n.weight);
            if (SameV(a, b)) return true;
            d = "node " + Hex(&n.node, sizeof n.node) + " assoc.blend " + F(reinterpret_cast<CAnimBlendAssociation*>(n.assocBuf)->m_BlendAmount) + " flags " + std::to_string(reinterpret_cast<CAnimBlendAssociation*>(n.assocBuf)->m_Flags) + " w " + F(n.weight) + " seq " + Hex(n.seqBuf, sizeof n.seqBuf) + " frames " + Hex(n.frames, sizeof n.frames) + " got " + V(a) + " exe " + V(b);
            return false;
        });
    };
    trans("CAnimBlendNode::GetCurrentTranslation 0x4CFC50", 0x4CFC50, false, [](CAnimBlendNode& n, CVector& t, float w) { n.GetCurrentTranslation(t, w); });
    trans("CAnimBlendNode::GetCurrentTranslationCompressed 0x4CFE60", 0x4CFE60, true, [](CAnimBlendNode& n, CVector& t, float w) { n.GetCurrentTranslationCompressed(t, w); });
    trans("CAnimBlendNode::GetEndTranslation 0x4CFD90", 0x4CFD90, false, [](CAnimBlendNode& n, CVector& t, float w) { n.GetEndTranslation(t, w); });
    trans("CAnimBlendNode::GetEndTranslationCompressed 0x4D0000", 0x4D0000, true, [](CAnimBlendNode& n, CVector& t, float w) { n.GetEndTranslationCompressed(t, w); });
}

#include "game_oracle_collision.inc"

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
    if (!exe) { std::printf("game_oracle_test: set RW_EXE_ORACLE=<path of gta_sa_compact.exe>; skipped\n"); return 0; }
    if (!oracle::Map(exe)) { std::printf("FATAL: cannot map the exe\n"); return 2; }
    // CRT state the exe's startup code would have initialised: __sse2_available (0xC9C400) = 1 (floor 0x8219F0 takes the SSE2 path, as on every real machine)
    *reinterpret_cast<int*>(0xC9C400) = 1;
    oracle::Patch(0x82872C, (void*)&HostMathErr);
    oracle::Patch(0x827B3D, (void*)&HostGetPtd);
    std::printf("game_oracle_test: %d cases per function and precision mode; PC24 = game mode (D3D CreateDevice), PC53 = CRT default\n", g_cases);
    TestVectorGeneral();
    TestMatrixQuat();
    TestAnimNode();
    TestCollision();
    int bad24 = 0, bad53 = 0, hard24 = 0, hard53 = 0;
    for (auto& r : g_rows) { bad24 += r.bad24; bad53 += r.bad53; hard24 += r.hardReg24 + r.hardSpec24; hard53 += r.hardReg53 + r.hardSpec53; }
    std::printf("\n%zu functions, mismatches (strict / excluding NaN-payload-only): PC24 %d / %d, PC53 %d / %d\n", g_rows.size(), bad24, hard24, bad53, hard53);
    return hard24 ? 1 : 0;
}

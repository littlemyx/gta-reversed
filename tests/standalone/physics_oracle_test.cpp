// P2E-2: differential test of numerically sensitive GAME subsystems against the original machine code (exe oracle, see game_oracle.h; harness as in game_oracle_test.cpp).
// Covered: CAnimBlendNode key-frame state machine, CCurves, CPhysical force/speed helpers on a fake entity (raw zeroed buffer with the real layout), plus whatever
// physics_oracle_cd.inc (CCarCtrl / CPathFind / CTimeCycle ... helpers) and physics_oracle_misc.inc add. Bit-exact compare at PC=24 (game) and PC=53 (info).
// usage: physics_oracle_test.exe [-v] [-v53] [-vn] [-trace (print every case index)] [-n cases] [name-substring ...]     (exit code 0 = no real mismatch at PC=24)
#include "game_oracle.h"

#include "Collision.h"
#include "General.h"
#include "Matrix.h"
#include "Quaternion.h"
#include "AnimBlendNode.h"
#include "AnimBlendSequence.h"
#include "AnimBlendAssociation.h"
#include "Curves.h"
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

// ---------------------------------------------------------------------------------------------------------------------------------
// (a) CAnimBlendNode: full state machine on raw sequences / associations (separate copies for the port and for the exe, compared afterwards)
struct NodeFx {
    alignas(16) uint8 seqBuf[sizeof(CAnimBlendSequence)] {};
    alignas(16) uint8 assocBuf[sizeof(CAnimBlendAssociation)] {};
    alignas(16) uint8 frames[8 * sizeof(KeyFrameTransCompressed) + 8 * sizeof(KeyFrameTrans)] {};
    alignas(16) uint8 nodeBuf[sizeof(CAnimBlendNode)] {};
    float weight = 0.f;
    CAnimBlendNode&        node()  { return *reinterpret_cast<CAnimBlendNode*>(nodeBuf); }
    CAnimBlendSequence&    seq()   { return *reinterpret_cast<CAnimBlendSequence*>(seqBuf); }
    CAnimBlendAssociation& assoc() { return *reinterpret_cast<CAnimBlendAssociation*>(assocBuf); }
    void Rewire() { seq().m_Frames = frames; node().m_Seq = &seq(); node().m_BlendAssoc = &assoc(); }
    NodeFx() = default;
    NodeFx(const NodeFx& o) { std::memcpy(this, &o, sizeof *this); Rewire(); }
};
static void BuildNode(Rng& r, NodeFx& n, bool compressed) {
    auto& seq = n.seq();
    auto& assoc = n.assoc();
    const int nf = 1 + r.below(6);
    seq.m_bHasRotation = r.below(10) != 0; seq.m_bHasTranslation = r.below(5) != 0; seq.m_bIsCompressed = compressed; seq.m_FramesNum = (uint16)nf; seq.m_Frames = n.frames;
    const bool looped = r.below(2);
    assoc.m_Flags = (uint16)((r.u32() & 0xFFFC) | (r.below(4) ? 1 : 0) | (looped ? 2 : 0));   // playing mostly
    const float ts = r.below(3) ? 20.f : PickScale(r);
    auto delta = [&](int i) {   // looped sequences: non-negative deltas with one clearly positive one (otherwise the exe loops forever)
        float v = r.below(8) ? r.f01() * 0.5f : GenF(r, 2.f, S_NAN | S_ZERO | S_DEN | S_TINY);
        if (looped) { if (v < 0) v = -v; if (i == 0) v = 0.05f + r.f01(); }
        return v;
    };
    if (compressed) {
        auto* kf = reinterpret_cast<uint8*>(n.frames);
        for (size_t i = 0; i < sizeof n.frames; ++i) kf[i] = (uint8)r.u32();
        const size_t stride = seq.m_bHasTranslation ? sizeof(KeyFrameTransCompressed) : sizeof(KeyFrameCompressed);
        for (int i = 0; i < nf; ++i) {
            auto* k = reinterpret_cast<int16_t*>((uint8*)n.frames + i * stride);
            if (r.below(4)) { k[0] = (int16_t)(r.u32() % 8192) - 4096; k[1] = (int16_t)(r.u32() % 8192) - 4096; k[2] = (int16_t)(r.u32() % 8192) - 4096; k[3] = (int16_t)(r.u32() % 8192) - 4096; }
            int dt = r.below(8) ? (int)(r.u32() % 400) : (int)(int16_t)r.u32();
            if (looped) { if (dt < 0) dt = -dt; if (i == 0) dt = 3 + (int)(r.u32() % 300); }
            k[4] = (int16_t)dt;
        }
    } else {
        const size_t stride = seq.m_bHasTranslation ? sizeof(KeyFrameTrans) : sizeof(KeyFrame);
        for (int i = 0; i < nf; ++i) {
            auto* f = reinterpret_cast<KeyFrameTrans*>((uint8*)n.frames + i * stride);
            f->Rot = CQuaternion(GenF(r, 1.f), GenF(r, 1.f), GenF(r, 1.f), GenF(r, 1.f));
            if (r.below(2)) { const double l = std::sqrt((double)f->Rot.x*f->Rot.x + (double)f->Rot.y*f->Rot.y + (double)f->Rot.z*f->Rot.z + (double)f->Rot.w*f->Rot.w); if (l > 0 && std::isfinite(l)) { f->Rot.x=(float)(f->Rot.x/l); f->Rot.y=(float)(f->Rot.y/l); f->Rot.z=(float)(f->Rot.z/l); f->Rot.w=(float)(f->Rot.w/l);} }
            f->DeltaTime = delta(i);
            if (seq.m_bHasTranslation) f->Trans = GenV(r, ts);
        }
    }
    assoc.m_BlendAmount = r.below(6) ? r.f01() * 1.5f : GenF(r, 2.f);
    // time step: usually small positive; 0 / negative / NaN occasionally (inf and huge excluded: the exe loops over `remaining <= 0` once per frame)
    assoc.m_TimeStep = r.below(5) ? r.f01() * 0.6f : GenF(r, 3.f, S_NAN | S_ZERO | S_DEN | S_TINY);
    n.weight = r.below(4) ? r.f01() : GenF(r, 2.f);
    auto& node = n.node();
    node.m_Theta = GenF(r, 4.f); node.m_InvSinTheta = GenF(r, 5.f);
    node.m_KFCurr = (int16)r.below(nf); node.m_KFPrev = (int16)r.below(nf);
    node.m_KFRemainingTime = r.below(6) ? r.f01() * 0.5f : GenF(r, 3.f, S_NAN | S_ZERO | S_DEN | S_TINY);
    n.Rewire();
}
static bool SameNode(NodeFx& a, NodeFx& b) {
    bool ok = SameBlob(a.nodeBuf + 0, b.nodeBuf + 0, 8);                    // theta, invSinTheta
    ok &= a.node().m_KFCurr == b.node().m_KFCurr && a.node().m_KFPrev == b.node().m_KFPrev;
    ok &= SameF(a.node().m_KFRemainingTime, b.node().m_KFRemainingTime);
    ok &= SameBlob(a.frames, b.frames, sizeof a.frames);
    if (!ok) g_hardDiff = g_hardDiff || !g_nanDiff;
    return ok;
}
static std::string NodeDesc(NodeFx& n) {
    return "node " + Hex(n.nodeBuf, sizeof n.nodeBuf) + " flags " + std::to_string(n.assoc().m_Flags) + " blend " + F(n.assoc().m_BlendAmount) + " ts " + F(n.assoc().m_TimeStep) + " w " + F(n.weight) + " seq " + Hex(n.seqBuf, sizeof n.seqBuf) + " frames " + Hex(n.frames, sizeof n.frames);
}

static void TestAnimNode2() {
    using NF = bool (CAnimBlendNode::*)();
    struct B { const char* n; unsigned va; bool comp; NF fn; };
    static const B bs[] = {
        { "CAnimBlendNode::NextKeyFrame 0x4D04A0", 0x4D04A0, false, &CAnimBlendNode::NextKeyFrame },
        { "CAnimBlendNode::NextKeyFrameCompressed 0x4D0570", 0x4D0570, true, &CAnimBlendNode::NextKeyFrameCompressed },
        { "CAnimBlendNode::NextKeyFrameNoCalc 0x4CFB90", 0x4CFB90, false, &CAnimBlendNode::NextKeyFrameNoCalc },
        { "CAnimBlendNode::UpdateTime 0x4D0160", 0x4D0160, false, &CAnimBlendNode::UpdateTime },
        { "CAnimBlendNode::SetupKeyFrameCompressed 0x4D0650", 0x4D0650, true, &CAnimBlendNode::SetupKeyFrameCompressed },
    };
    for (const auto& b : bs) {
        Run(b.n, [=](Rng& r, std::string& d) {
            NodeFx a; BuildNode(r, a, b.comp); NodeFx e(a);
            const bool ra = (a.node().*b.fn)();
            const bool re = oracle::Fn<bool __fastcall(CAnimBlendNode*, int)>(b.va)(&e.node(), 0);
            if (ra == re && SameNode(a, e)) return true;
            d = NodeDesc(e) + " ret " + std::to_string(ra) + "/" + std::to_string(re) + " got " + Hex(a.nodeBuf, sizeof a.nodeBuf) + " exe " + Hex(e.nodeBuf, sizeof e.nodeBuf);
            return false;
        });
    }
    Run("CAnimBlendNode::CalcDeltas 0x4D0190", [](Rng& r, std::string& d) {
        NodeFx a; BuildNode(r, a, false); NodeFx e(a);
        a.node().CalcDeltas();
        oracle::Fn<void __fastcall(CAnimBlendNode*, int)>(0x4D0190)(&e.node(), 0);
        if (SameNode(a, e)) return true;
        d = NodeDesc(e) + " got " + Hex(a.nodeBuf, sizeof a.nodeBuf) + " exe " + Hex(e.nodeBuf, sizeof e.nodeBuf);
        return false;
    });
    Run("CAnimBlendNode::CalcDeltasCompressed 0x4D0350", [](Rng& r, std::string& d) {
        NodeFx a; BuildNode(r, a, true); NodeFx e(a);
        a.node().CalcDeltasCompressed();
        oracle::Fn<void __fastcall(CAnimBlendNode*, int)>(0x4D0350)(&e.node(), 0);
        if (SameNode(a, e)) return true;
        d = NodeDesc(e) + " got " + Hex(a.nodeBuf, sizeof a.nodeBuf) + " exe " + Hex(e.nodeBuf, sizeof e.nodeBuf);
        return false;
    });
    Run("CAnimBlendNode::FindKeyFrame 0x4D0240", [](Rng& r, std::string& d) {
        NodeFx a; BuildNode(r, a, false);
        // looped: deltas are >= 0 with delta[0] > 0; frames are searched from index 1 though, so bound the time (every pass over the sequence consumes the sum of the deltas)
        float t = r.below(4) ? r.f01() * 3.f : GenF(r, 8.f, S_NAN | S_ZERO | S_DEN | S_TINY);
        if (a.assoc().m_Flags & 2) {   // looped: make sure the sum over frames 1..n-1 + frame 0 is >= 0.05 so the loop terminates (frame 0 is part of the wrap)
            auto* f0 = reinterpret_cast<KeyFrameTrans*>(a.frames); if (!(f0->DeltaTime >= 0.05f)) f0->DeltaTime = 0.05f + r.f01();
            for (int i = 1; i < a.seq().m_FramesNum; ++i) { auto* f = a.seq().GetUKeyFrame(i); if (f->DeltaTime < 0.05f) f->DeltaTime = 0.05f + r.f01(); }   // the search starts at frame 1 and skips frame 0 on wrap: frames 1.. must have positive deltas or it never ends
        }
        NodeFx e(a);
        const bool ra = a.node().FindKeyFrame(t);
        const bool re = oracle::Fn<bool __fastcall(CAnimBlendNode*, int, float)>(0x4D0240)(&e.node(), 0, t);
        if (ra == re && SameNode(a, e)) return true;
        d = NodeDesc(e) + " t " + F(t) + " ret " + std::to_string(ra) + "/" + std::to_string(re) + " got " + Hex(a.nodeBuf, sizeof a.nodeBuf) + " exe " + Hex(e.nodeBuf, sizeof e.nodeBuf);
        return false;
    });
    for (int comp = 0; comp < 2; ++comp) {
        Run(comp ? "CAnimBlendNode::UpdateCompressed 0x4D08D0" : "CAnimBlendNode::Update 0x4D06C0", [=](Rng& r, std::string& d) {
            NodeFx a; BuildNode(r, a, comp); NodeFx e(a);
            CVector ta{ 7, 8, 9 }, te{ 7, 8, 9 }; CQuaternion qa{ 1, 2, 3, 4 }, qe{ 1, 2, 3, 4 };
            const bool ra = comp ? a.node().UpdateCompressed(ta, qa, a.weight) : a.node().Update(ta, qa, a.weight);
            const bool re = oracle::Fn<bool __fastcall(CAnimBlendNode*, int, CVector*, CQuaternion*, float)>(comp ? 0x4D08D0 : 0x4D06C0)(&e.node(), 0, &te, &qe, a.weight);
            if (ra == re && SameV(ta, te) && SameQ(qa, qe) && SameNode(a, e)) return true;
            d = NodeDesc(e) + " ret " + std::to_string(ra) + "/" + std::to_string(re) + " trans " + V(ta) + "/" + V(te) + " rot " + Q(qa) + "/" + Q(qe) + " nodeA " + Hex(a.nodeBuf, sizeof a.nodeBuf) + " nodeE " + Hex(e.nodeBuf, sizeof e.nodeBuf);
            return false;
        });
    }
    Run("CAnimBlendNode::Init 0x4CFB70", [](Rng& r, std::string& d) {
        NodeFx a; BuildNode(r, a, false); NodeFx e(a);
        a.node().Init();
        oracle::Fn<void __fastcall(CAnimBlendNode*, int)>(0x4CFB70)(&e.node(), 0);
        if (SameBlob(a.nodeBuf, e.nodeBuf, sizeof a.nodeBuf)) return true;
        d = "got " + Hex(a.nodeBuf, sizeof a.nodeBuf) + " exe " + Hex(e.nodeBuf, sizeof e.nodeBuf);
        return false;
    }, 200);
}

// ---------------------------------------------------------------------------------------------------------------------------------
// (c) CCurves
static void TestCurves2() {
    Run("CCurves::DistForLineToCrossOtherLine 0x43C610", [](Rng& r, std::string& d) {
        const float s = PickScale(r); float p[8]; for (auto& x : p) x = GenF(r, s);
        if (r.below(8) == 0) { p[2] = p[6]; p[3] = p[7]; }
        const float a = CCurves::DistForLineToCrossOtherLine(p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);
        const float b = oracle::Fn<float __cdecl(float, float, float, float, float, float, float, float)>(0x43C610)(p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);
        if (SameF(a, b)) return true;
        d = "in " + F(p[0]) + "," + F(p[1]) + "," + F(p[2]) + "," + F(p[3]) + "," + F(p[4]) + "," + F(p[5]) + "," + F(p[6]) + "," + F(p[7]) + " got " + F(a) + " exe " + F(b);
        return false;
    });
    struct Cur { CVector s, e, sd, ed; };
    auto gen = [](Rng& r) { const float sc = r.below(3) ? 100.f : PickScale(r); return Cur{ GenV(r, sc), GenV(r, sc), GenDir(r), GenDir(r) }; };
    Run("CCurves::CalcSpeedVariationInBend 0x43C660", [=](Rng& r, std::string& d) {
        const Cur c = gen(r);
        const float a = CCurves::CalcSpeedVariationInBend(c.s, c.e, c.sd.x, c.sd.y, c.ed.x, c.ed.y);
        const float b = oracle::Fn<float __cdecl(const CVector&, const CVector&, float, float, float, float)>(0x43C660)(c.s, c.e, c.sd.x, c.sd.y, c.ed.x, c.ed.y);
        if (SameF(a, b)) return true;
        d = "s " + V(c.s) + " e " + V(c.e) + " sd " + V(c.sd) + " ed " + V(c.ed) + " got " + F(a) + " exe " + F(b);
        return false;
    });
    Run("CCurves::CalcSpeedScaleFactor 0x43C710", [=](Rng& r, std::string& d) {
        const Cur c = gen(r);
        const float a = CCurves::CalcSpeedScaleFactor(c.s, c.e, c.sd.x, c.sd.y, c.ed.x, c.ed.y);
        const float b = oracle::Fn<float __cdecl(const CVector&, const CVector&, float, float, float, float)>(0x43C710)(c.s, c.e, c.sd.x, c.sd.y, c.ed.x, c.ed.y);
        if (SameF(a, b)) return true;
        d = "s " + V(c.s) + " e " + V(c.e) + " sd " + V(c.sd) + " ed " + V(c.ed) + " got " + F(a) + " exe " + F(b);
        return false;
    });
    Run("CCurves::CalcCorrectedDist 0x43C880", [](Rng& r, std::string& d) {
        const float total = r.below(10) ? r.f01() * 200.f : GenF(r, 50.f), cur = r.below(4) ? r.f01() * total : GenF(r, 100.f), sv = r.below(3) ? r.f01() : GenF(r, 2.f);
        // |current / total| >= 2^63/pi is outside the x87 fsin/fcos domain (the instruction returns its operand with C2 set); the port's CRT sin/cos reduce properly. Never reached in game (t in 0..1)
        if (!(std::fabs(cur / total) < 1e15f)) return true;
        float oa = -1, ob = -1;
        const float a = CCurves::CalcCorrectedDist(cur, total, sv, &oa);
        const float b = oracle::Fn<float __cdecl(float, float, float, float*)>(0x43C880)(cur, total, sv, &ob);
        if (SameF(a, b) && SameF(oa, ob)) return true;
        d = "cur " + F(cur) + " total " + F(total) + " sv " + F(sv) + " got " + F(a) + "," + F(oa) + " exe " + F(b) + "," + F(ob);
        return false;
    });
    Run("CCurves::CalcCurvePoint 0x43C900", [=](Rng& r, std::string& d) {
        const Cur c = gen(r);
        const float t = r.below(6) ? r.f01() : GenF(r, 1.5f);
        const int32 ms = r.below(8) ? (int32)(r.u32() % 60000) : (int32)r.u32() % 100000;
        CVector ra{ 7, 8, 9 }, sa{ 7, 8, 9 }, re{ 7, 8, 9 }, se{ 7, 8, 9 };
        CCurves::CalcCurvePoint(c.s, c.e, c.sd, c.ed, t, ms, ra, sa);
        oracle::Fn<void __cdecl(const CVector&, const CVector&, const CVector&, const CVector&, float, int32, CVector&, CVector&)>(0x43C900)(c.s, c.e, c.sd, c.ed, t, ms, re, se);
        if (SameV(ra, re) && SameV(sa, se)) return true;
        d = "s " + V(c.s) + " e " + V(c.e) + " sd " + V(c.sd) + " ed " + V(c.ed) + " t " + F(t) + " ms " + std::to_string(ms) + " got " + V(ra) + V(sa) + " exe " + V(re) + V(se);
        return false;
    });
}

// ---------------------------------------------------------------------------------------------------------------------------------
// (b) CPhysical on a fake entity: raw buffer with the real layout; both sides get a private copy and the whole entity + matrix is compared afterwards
struct PhysFx {
    alignas(16) uint8 e[0x600] {};   // CPhysical + room for the CVehicle fields some helpers read (sub type at 0x594)
    alignas(16) uint8 m[0x80] {};
    CPhysical& p() { return *reinterpret_cast<CPhysical*>(e); }
    void Rewire() { reinterpret_cast<CPlaceable*>(e)->m_matrix = reinterpret_cast<CMatrixLink*>(m); }
    PhysFx() = default;
    PhysFx(const PhysFx& o) { std::memcpy(this, &o, sizeof *this); Rewire(); }
};
static void BuildPhys(Rng& r, PhysFx& f) {
    const float sc = r.below(3) ? 10.f : PickScale(r);
    uint32_t* w = reinterpret_cast<uint32_t*>(f.e);
    for (size_t i = 0; i < sizeof f.e / 4; ++i) { const float v = GenF(r, sc); std::memcpy(&w[i], &v, 4); }
    uint32_t* mw = reinterpret_cast<uint32_t*>(f.m);
    for (size_t i = 0; i < 16; ++i) { const float v = GenF(r, r.below(2) ? 1.f : sc); std::memcpy(&mw[i], &v, 4); }
    auto& p = f.p();
    p.m_vecMoveSpeed = GenV(r, r.below(2) ? 1.f : sc); p.m_vecTurnSpeed = GenV(r, r.below(2) ? 0.5f : sc);
    p.m_vecFrictionMoveSpeed = GenV(r, 0.5f); p.m_vecFrictionTurnSpeed = GenV(r, 0.5f);
    p.m_vecCentreOfMass = GenV(r, r.below(2) ? 1.f : 3.f);
    p.m_fMass = r.below(8) ? 1.f + r.f01() * 3000.f : GenF(r, 100.f); p.m_fTurnMass = r.below(8) ? 1.f + r.f01() * 8000.f : GenF(r, 100.f);
    uint32_t* fl = reinterpret_cast<uint32_t*>(&p.m_nPhysicalFlags);
    *fl = r.u32() & r.u32() & r.u32();   // sparse random flag bits (each ~1/8)
    if (r.below(4)) *fl &= ~(0x1000u | 0x8u);   // bInfiniteMass / bDisableMoveForce mostly off, so the usual path is covered too
    p.m_nFlags = r.u32();
    f.e[0x36] = (uint8)((f.e[0x36] & ~7) | (r.below(3) ? 2 : r.below(8)));   // entity type: vehicle most of the time
    *reinterpret_cast<uint32_t*>(f.e + 0x594) = r.below(3) == 0 ? 0 : r.below(2) ? 9 : r.below(12);   // vehicle sub type
    p.m_fAirResistance = r.below(2) ? r.f01() * 0.3f : GenF(r, 1.f);
    p.m_pDamageEntity = nullptr; p.m_pAttachedTo = nullptr; p.m_pEntityIgnoredCollision = nullptr;
    CTimer::ms_fTimeStep = r.below(8) ? r.f01() * 3.f : GenF(r, 4.f);
    f.Rewire();
}
static bool SamePhys(PhysFx& a, PhysFx& b) {
    PhysFx x(a), y(b);   // the m_matrix pointers differ by construction
    reinterpret_cast<CPlaceable*>(x.e)->m_matrix = nullptr; reinterpret_cast<CPlaceable*>(y.e)->m_matrix = nullptr;
    return SameBlob(x.e, y.e, sizeof x.e) & SameBlob(x.m, y.m, sizeof x.m);
}
static std::string DiffDesc(const void* a, const void* b, size_t n, const char* tag) {   // differing dwords only
    std::string s; int shown = 0;
    for (size_t i = 0; i + 4 <= n && shown < 8; i += 4) {
        uint32_t x, y; std::memcpy(&x, (const uint8*)a + i, 4); std::memcpy(&y, (const uint8*)b + i, 4);
        if (x != y) { char t[80]; std::snprintf(t, sizeof t, " %s+%02zX got %08X exe %08X", tag, i, x, y); s += t; ++shown; }
    }
    return s;
}
static std::string PhysDesc(PhysFx& f) { return "ts " + F(CTimer::ms_fTimeStep) + " flags " + std::to_string(*reinterpret_cast<uint32_t*>(&f.p().m_nPhysicalFlags)) + " mass " + F(f.p().m_fMass) + " tmass " + F(f.p().m_fTurnMass) + " com " + V(f.p().m_vecCentreOfMass) + " ms " + V(f.p().m_vecMoveSpeed) + " ts " + V(f.p().m_vecTurnSpeed); }

#include "physics_oracle_phys.inc"
#include "physics_oracle_cd.inc"
#include "physics_oracle_misc.inc"
#include "physics_oracle_weapon.inc"

// ---------------------------------------------------------------------------------------------------------------------------------
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
        else if (!std::strcmp(argv[i], "-n") && i + 1 < argc) g_cases = std::atoi(argv[++i]);
        else g_filters.push_back(argv[i]);
    }
    _CrtSetReportHook(AssertHook);
    _set_error_mode(_OUT_TO_STDERR);   // CRT assert()/abort: text on stderr instead of a MessageBox (which blocks forever)
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE); _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE); _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
    const char* exe = std::getenv("RW_EXE_ORACLE");
    if (!exe) { std::printf("physics_oracle_test: set RW_EXE_ORACLE=<path of gta_sa_compact.exe>; skipped\n"); return 0; }
    if (!oracle::Map(exe)) { std::printf("FATAL: cannot map the exe\n"); return 2; }
    *reinterpret_cast<int*>(0xC9C400) = 1;
    oracle::Patch(0x82872C, (void*)&HostMathErr);
    oracle::Patch(0x827B3D, (void*)&HostGetPtd);
    DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &g_mainThread, 0, FALSE, DUPLICATE_SAME_ACCESS);
    CreateThread(nullptr, 0, WatchdogProc, nullptr, 0, nullptr);
    std::printf("physics_oracle_test: %d cases per function and precision mode; PC24 = game mode (D3D CreateDevice), PC53 = CRT default\n", g_cases);
    TestAnimNode2();
    TestCurves2();
    TestPhysical();
    TestCD();
    TestMisc();
    TestWeapon();
    int bad24 = 0, bad53 = 0, hard24 = 0, hard53 = 0;
    for (auto& r : g_rows) { bad24 += r.bad24; bad53 += r.bad53; hard24 += r.hardReg24 + r.hardSpec24; hard53 += r.hardReg53 + r.hardSpec53; }
    std::printf("\n%zu functions, mismatches (strict / excluding NaN-payload-only): PC24 %d / %d, PC53 %d / %d\n", g_rows.size(), bad24, hard24, bad53, hard53);
    return hard24 ? 1 : 0;
}

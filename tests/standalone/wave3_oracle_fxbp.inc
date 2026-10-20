// ORACLE-W3-FXBP: exe-oracle rows for game_sa/Fx/Blueprints (FxInfo*::GetValue key-curve consumers, FxInfoAttractLine FindClosestPtOnLine, FxInfoSelfLit::Load, FxInfoManager::AddFxInfo)
// plus Fx_c RotateVecAboutVec and CCarFXRenderer (IsCCPCPipelineAttached, SetFxEnvMapLightMult).
// Included by wave3_oracle_fxs.inc (executable wave3_fxs_oracle_test), after the w3x fixture helpers and the XROW macros.
// Fixture (inside g_sc, compared as a whole after every case): an FxInfo object (vptr, m_nType, m_bTimeModeParticle, the embedded FxInterpInfo_c: looped / numKeys / count / times* / keys*),
// the uint16 time array, the per-attribute key arrays (float / int16 / uint16 storage = the three FxInterpInfo key types) and the EmissionInfo_t / MovementInfo_t / RenderInfo_t the function writes.
// Rows compare only this block (the functions touch no exe memory); the Wind row additionally reads g_fxMan.m_pfWindSpeed / m_pWindDir, which point into the block.
// The port methods are called non-virtually (P::GetValue) on the raw object, the exe's through its address with `this` = the same object (vptr bytes are never read by these functions).
#include "Fx/Blueprints/FxInfoAnimTexture.h"
#include "Fx/Blueprints/FxInfoAttractLine.h"
#include "Fx/Blueprints/FxInfoAttractPt.h"
#include "Fx/Blueprints/FxInfoColour.h"
#include "Fx/Blueprints/FxInfoColourBright.h"
#include "Fx/Blueprints/FxInfoColourRange.h"
#include "Fx/Blueprints/FxInfoDir.h"
#include "Fx/Blueprints/FxInfoEmAngle.h"
#include "Fx/Blueprints/FxInfoEmDir.h"
#include "Fx/Blueprints/FxInfoEmLife.h"
#include "Fx/Blueprints/FxInfoEmPos.h"
#include "Fx/Blueprints/FxInfoEmRate.h"
#include "Fx/Blueprints/FxInfoEmRotation.h"
#include "Fx/Blueprints/FxInfoEmSize.h"
#include "Fx/Blueprints/FxInfoEmSpeed.h"
#include "Fx/Blueprints/FxInfoEmWeather.h"
#include "Fx/Blueprints/FxInfoFlat.h"
#include "Fx/Blueprints/FxInfoFloat.h"
#include "Fx/Blueprints/FxInfoForce.h"
#include "Fx/Blueprints/FxInfoHeatHaze.h"
#include "Fx/Blueprints/FxInfoRotSpeed.h"
#include "Fx/Blueprints/FxInfoSelfLit.h"
#include "Fx/Blueprints/FxInfoSize.h"
#include "Fx/Blueprints/FxInfoSmoke.h"
#include "Fx/Blueprints/FxInfoSpriteRect.h"
#include "Fx/Blueprints/FxInfoTrail.h"
#include "Fx/Blueprints/FxInfoUnderwater.h"
#include "Fx/Blueprints/FxInfoWind.h"
#include "Fx/FxInfoManager.h"
#include "Fx/Fx.h"
#include "Fx/CarFXRenderer.h"

void FindClosestPtOnLine(CVector& out, CVector& lineA, CVector& lineB, CVector& point);   // FxInfoAttractLine.cpp (file-local declaration there)

namespace w3x {
constexpr size_t kInfo = 0x2800, kTimes = 0x2840, kKeyPtrs = 0x2860, kKeys = 0x2900, kOut = 0x2B00, kAux = 0x2C00, kAtom = 0x3000;
static int g_keyType = 0, g_attrs = 1;   // key storage (0 float, 1 int16, 2 uint16), attribute count (0 = no curve; -1 = FxInfoSize: 2 or 4)

static bool CleanFloatSpecial(Rng& r) { return r.below(16) == 0; }
// the FxInfo object over a random key curve: 1..6 keys (rarely 7..8), ascending times (rarely unordered / zero), looped 1 in 3, game-range keys with edge values
static void BuildCurve(Rng& r) {
    BuildAll(r);
    SAt<uint32_t>(kInfo) = 0;
    SAt<uint16_t>(kInfo + 4) = (uint16_t)r.u32();
    SAt<uint8_t>(kInfo + 6) = r.below(2) ? 1 : (r.below(3) ? 0 : (uint8_t)r.u32());   // m_bTimeModeParticle
    SAt<uint32_t>(kInfo + 8) = 0;
    SAt<uint8_t>(kInfo + 0xC) = r.below(3) == 0;                                       // m_bLooped
    const uint32_t kc = r.below(40);
    const int nk = kc < 5 ? 1 : (kc < 38 ? 2 + (int)r.below(5) : 7 + (int)r.below(2));
    SAt<int8_t>(kInfo + 0xD) = (int8_t)nk;
    int attrs = g_attrs;
    if (attrs < 0) attrs = r.below(2) ? 2 : 4;
    SAt<int16_t>(kInfo + 0xE) = (int16_t)attrs;
    SAt<uint32_t>(kInfo + 0x10) = (uint32_t)(uintptr_t)S(kTimes);
    SAt<uint32_t>(kInfo + 0x14) = (uint32_t)(uintptr_t)S(kKeyPtrs);
    uint32_t t = r.below(3) ? 0 : r.below(60);
    for (int i = 0; i < 8; ++i) {
        t += r.below(4) ? 1 + r.below(300) : r.below(2000);
        SAt<uint16_t>(kTimes + 2 * i) = (uint16_t)t;
    }
    if (r.below(12) == 0) SAt<uint16_t>(kTimes + 2 * r.below(8)) = (uint16_t)r.u32();
    for (int a = 0; a < 9; ++a) {
        SAt<uint32_t>(kKeyPtrs + 4 * a) = (uint32_t)(uintptr_t)S(kKeys + 0x20 * a);
        for (int k = 0; k < 8; ++k) {
            const size_t off = kKeys + 0x20 * a;
            if (g_keyType == 0) SAt<float>(off + 4 * k) = r.below(8) ? (CleanFloatSpecial(r) ? Fe(r, 5.f, 1) : Fg(r, 2.f)) : Fg(r, 1000.f);
            else if (g_keyType == 1) SAt<int16_t>(off + 2 * k) = (int16_t)(r.below(4) ? (int)r.below(2001) - 1000 : (int)r.u32());
            else SAt<uint16_t>(off + 2 * k) = (uint16_t)(r.below(4) ? r.below(2001) : r.u32());
        }
    }
    for (size_t o = 0; o < 0x100; o += 4) SAt<float>(kOut + o) = Fe(r, 20.f, 24);   // the info the function adds to / overwrites (floats; byte fields get the same bits)
    g_fxMan.m_pfWindSpeed = reinterpret_cast<float*>(S(kAux));
    g_fxMan.m_pWindDir    = reinterpret_cast<CVector*>(S(kAux + 0x10));
    SAt<float>(kAux) = Fe(r, 20.f, 12);
    PutVec(r, kAux + 0x10, 1.f, 12);
}
struct GVArgs { float ct, mult, tot, len; uint32_t uc; };
static GVArgs MakeArgs(Rng& r) {
    GVArgs a;
    a.ct   = r.below(5) ? r.f01() * 1.3f : Fe(r, 5.f, 1);
    a.mult = r.below(5) ? r.f01() * 1.3f : Fe(r, 5.f, 1);
    a.tot  = r.below(5) ? r.f01() * 0.1f : Fe(r, 2.f, 1);
    a.len  = r.below(6) ? 0.05f + r.f01() * 5.f : Fe(r, 5.f, 1);
    a.uc   = r.below(2);
    return a;
}
// block-only comparison (no 4 MB image): pure functions
template<class PF, class EF>
static bool XCaseP(Rng& r, std::string& d, PF&& pf, EF&& ef, void (*build)(Rng&), bool pool = false) {
    const uint32_t seed = r.u32() | 1;
    build(r);
    Reg sc{ (uintptr_t)g_sc, kScSize };
    if (pool) return BothA(d, seed, { sc, Reg{ 0xA9AF2C, 12 } }, std::forward<PF>(pf), std::forward<EF>(ef));   // g_fxMan.m_Pool header: the allocations move its offset
    return BothA(d, seed, { sc }, std::forward<PF>(pf), std::forward<EF>(ef));
}
template<class P>
static bool GVCase(Rng& r, std::string& d, uint32_t addr, int keyType, int attrs) {
    g_keyType = keyType; g_attrs = attrs;
    const GVArgs a = MakeArgs(r);
    void* const obj = S(kInfo); void* const out = S(kOut);
    const bool ok = XCaseP(r, d, [&]() -> uint64_t { static_cast<P*>(obj)->P::GetValue(a.ct, a.mult, a.tot, a.len, a.uc != 0, out); return 0; },
                                 [&]() -> uint64_t { ExeThis<void, float, float, float, float, uint32_t, void*>(addr, obj, a.ct, a.mult, a.tot, a.len, a.uc, out); return 0; }, &BuildCurve);
    if (!ok) {   // the arguments and the key curve of the failing case
        char t[400]; std::snprintf(t, sizeof t, " | args ct %08X mult %08X tot %08X len %08X uc %u tm %u; keys %d loop %d cnt %d times %04X %04X %04X %04X %04X %04X key0 %08X %08X %08X", (unsigned)FB(a.ct), (unsigned)FB(a.mult), (unsigned)FB(a.tot), (unsigned)FB(a.len), a.uc,
            SAt<uint8_t>(kInfo + 6), SAt<int8_t>(kInfo + 0xD), SAt<uint8_t>(kInfo + 0xC), SAt<int16_t>(kInfo + 0xE), SAt<uint16_t>(kTimes), SAt<uint16_t>(kTimes + 2), SAt<uint16_t>(kTimes + 4), SAt<uint16_t>(kTimes + 6), SAt<uint16_t>(kTimes + 8), SAt<uint16_t>(kTimes + 10),
            SAt<uint32_t>(kKeys), SAt<uint32_t>(kKeys + 4), SAt<uint32_t>(kKeys + 8));
        d += t;
    }
    return ok;
}
// the exe's RenderWare maths (RwV3dLength / RwV3dNormalize / RwMatrixUpdate) reads plugin data RwEngineStart would have built: the approximate-sqrt tables (plugin constructor 0x7EDE90 stores their offset into
// 0xC97934) and the matrix plugin data (RwMatrixOpen 0x7F16C0) - the same set-up as wave3_oracle_ik.inc (w3k::InitExeRw)
static uint8_t g_engine[0x1000];
static void* __cdecl FakeAlloc(size_t n, unsigned) { return std::calloc(1, n); }
static void  __cdecl FakeFree(void* p) { std::free(p); }
static void InitExeRw() {
    *reinterpret_cast<void**>(0xC97B24) = g_engine;
    *reinterpret_cast<void**>(g_engine + 0x134) = reinterpret_cast<void*>(&FakeAlloc);
    *reinterpret_cast<void**>(g_engine + 0x138) = reinterpret_cast<void*>(&FakeFree);
    oracle::Fn<void* __cdecl(void*, int, int)>(0x7EDE90)(nullptr, 0x400, 0);
    *reinterpret_cast<int32_t*>(0xC979BC) = 0x600;
    *reinterpret_cast<uint32_t*>(g_engine + 0x604) = 0x20000;
    *reinterpret_cast<void**>(g_engine + 0x608) = reinterpret_cast<void*>(0x7F12F0);
    for (int k = 0; k < 3; k++) *reinterpret_cast<uint32_t*>(g_engine + 0x60C + 4 * k) = 0x3C23D70Au;   // RwMatrixOpen's default tolerances (0.01f)
}
} // namespace w3x

#define GVROW(CLS, ADDR, KT, ATTRS, N) \
    XROW(#CLS "::GetValue", ADDR, 0) \
        return GVCase<CLS>(r, d, ADDR, KT, ATTRS); \
    XEND(N)

static void TestW3FxsBlueprints() {
    using namespace w2; using namespace w3x;
    InitExeRw();
    // emission infos
    GVROW(FxInfoEmRate_c,     0x4A4B60, 0, 1, 2000)
    GVROW(FxInfoEmSize_c,     0x4A4CB0, 1, 7, 1500)
    GVROW(FxInfoEmSpeed_c,    0x4A4D90, 1, 2, 1000)
    GVROW(FxInfoEmDir_c,      0x4A4E50, 1, 3, 1000)
    GVROW(FxInfoEmAngle_c,    0x4A4F20, 0, 2, 1000)
    GVROW(FxInfoEmLife_c,     0x4A4FE0, 2, 2, 1000)
    GVROW(FxInfoEmPos_c,      0x4A50A0, 0, 3, 1000)
    GVROW(FxInfoEmWeather_c,  0x4A5170, 0, 4, 1000)
    GVROW(FxInfoEmRotation_c, 0x4A5240, 0, 2, 1000)
    // movement infos
    GVROW(FxInfoForce_c,      0x4A54E0, 1, 3, 1500)
    GVROW(FxInfoAttractPt_c,  0x4A56E0, 1, 4, 3000)
    GVROW(FxInfoAttractLine_c, 0x4A5850, 1, 7, 3000)
    GVROW(FxInfoWind_c,       0x4A5AA0, 1, 1, 1500)
    GVROW(FxInfoRotSpeed_c,   0x4A5D40, 0, 4, 1000)
    GVROW(FxInfoUnderwater_c, 0x4A5E90, 2, 0, 100)
    GVROW(FxInfoFloat_c,      0x4A5E10, 0, 0, 100)
    // render infos
    GVROW(FxInfoColour_c,     0x4A5F30, 2, 4, 1500)
    GVROW(FxInfoSize_c,       0x4A6030, 1, -1, 1500)
    GVROW(FxInfoSpriteRect_c, 0x4A6140, 1, 4, 1000)
    GVROW(FxInfoHeatHaze_c,   0x4A6210, 1, 0, 100)
    GVROW(FxInfoTrail_c,      0x4A62B0, 2, 2, 1500)
    GVROW(FxInfoFlat_c,       0x4A63B0, 0, 9, 2000)
    GVROW(FxInfoDir_c,        0x4A64D0, 0, 3, 2000)
    GVROW(FxInfoAnimTexture_c, 0x4A65E0, 1, 1, 1500)
    GVROW(FxInfoColourRange_c, 0x4A66C0, 2, 7, 1500)
    GVROW(FxInfoSelfLit_c,    0x4A67C0, 2, 0, 100)
    GVROW(FxInfoColourBright_c, 0x4A6860, 2, 5, 1500)
    GVROW(FxInfoSmoke_c,      0x4A6970, 2, 8, 1500)
    XROW("FxInfoSelfLit_c::Load", 0x4A67B0, 0)
        void* const obj = S(kInfo); void* const file = S(kAux); const int32_t ver = (int32_t)r.u32();
        return XCaseP(r, d, [&]() -> uint64_t { static_cast<FxInfoSelfLit_c*>(obj)->FxInfoSelfLit_c::Load((FILESTREAM)file, ver); return 0; },
                            [&]() -> uint64_t { ExeThis<void, void*, int32_t>(0x4A67B0, obj, file, ver); return 0; }, &BuildCurve);
    XEND(100)
    XROW("FindClosestPtOnLine", 0x4A4880, 0)
        // out at kAux, A / B / point at kAux + 0x10 / 0x20 / 0x30; line endpoints near each other, degenerate (A == B), point on the segment or far away
        CVector* const out = reinterpret_cast<CVector*>(S(kAux)); CVector* const A = reinterpret_cast<CVector*>(S(kAux + 0x10)); CVector* const B = reinterpret_cast<CVector*>(S(kAux + 0x20)); CVector* const P = reinterpret_cast<CVector*>(S(kAux + 0x30));
        return XCaseP(r, d, [&]() -> uint64_t { FindClosestPtOnLine(*out, *A, *B, *P); return 0; },
                            [&]() -> uint64_t { oracle::Fn<void __cdecl(void*, void*, void*, void*)>(0x4A4880)(out, A, B, P); return 0; },
                            [](Rng& rr) {
                                BuildCurve(rr);
                                PutVec(rr, kAux + 0x10, 60.f, 14); PutVec(rr, kAux + 0x20, 60.f, 14); PutVec(rr, kAux + 0x30, 60.f, 14);
                                const uint32_t m = rr.below(10);
                                if (m == 0) std::memcpy(S(kAux + 0x20), S(kAux + 0x10), 12);                                                       // degenerate line
                                else if (m < 4) { for (int k = 0; k < 3; ++k) SAt<float>(kAux + 0x20 + 4 * k) = SAt<float>(kAux + 0x10 + 4 * k) + Fg(rr, 3.f); }   // short line
                                if (rr.below(3) == 0) { const float t = rr.f01() * 1.4f - 0.2f; for (int k = 0; k < 3; ++k) SAt<float>(kAux + 0x30 + 4 * k) = SAt<float>(kAux + 0x10 + 4 * k) + t * (SAt<float>(kAux + 0x20 + 4 * k) - SAt<float>(kAux + 0x10 + 4 * k)); }   // on the line
                            });
    XEND(4000)
    XROW("FxInfoManager_c::AddFxInfo", 0x4A7B00, 0)
        // every info type of the exe's switch, the gaps and some garbage; the object (and its embedded interp's) vptr is cleared on both sides: the vftables differ by construction
        static const int32_t types[] = { 0x1001, 0x1004, 0x1008, 0x1010, 0x1020, 0x1040, 0x1080, 0x1100, 0x1200, 0x2001, 0x2002, 0x2004, 0x2008, 0x2010, 0x2020, 0x2040, 0x2080, 0x2100, 0x2200, 0x2400,
                                         0x4001, 0x4002, 0x4004, 0x4008, 0x4010, 0x4020, 0x4040, 0x4080, 0x4100, 0x4200, 0x4400, 0x4800, 0x1002, 0x1400, 0x2800, 0x0, 0x1, 0x8000, 0x10000, -1,
                                         0x4040 + 1, 0x2081, 0x1201, 0x4401, 0x2100 + 0x80, 0x4000, 0x2000, 0x1000, 0x2201, 0x4101 };
        const int32_t ty = r.below(8) ? types[r.below((uint32_t)(sizeof types / sizeof types[0]))] : (int32_t)r.u32();
        FxInfoManager_c* const mgr = reinterpret_cast<FxInfoManager_c*>(S(kMgr));
        auto clear = [](void* o) { if (o) { *reinterpret_cast<uint32_t*>(o) = 0; *reinterpret_cast<uint16_t*>((uint8_t*)o + 6) = 0; *reinterpret_cast<uint32_t*>((uint8_t*)o + 8) = 0; *((uint8_t*)o + 0xF) = 0; } };   // + m_bTimeModeParticle / padding (left uninitialised by the exe, Load sets it; zeroed by the port's value-initialised base) and the high byte of m_nCount (the exe's Allocate stores one byte, readers do movsx byte)
        const bool ok = XCaseP(r, d, [&]() -> uint64_t { void* o = mgr->AddFxInfo(ty); clear(o); return U(o); },
                            [&]() -> uint64_t { void* o = ExeThis<void*, int32_t>(0x4A7B00, mgr, ty); clear(o); return U(o); },
                            [](Rng& rr) { BuildGlobalPool(rr); }, true);
        if (!ok) d += " | type " + std::to_string(ty);
        return ok;
    XEND(1500)
    XROW("RotateVecAboutVec", 0x4A1780, 0)
        static bool lut = false; if (!lut) { CMaths::InitMathsTables(); lut = true; }
        RwV3d* const res = reinterpret_cast<RwV3d*>(S(kAux)); const RwV3d* const vec = reinterpret_cast<const RwV3d*>(S(kAux + 0x10)); const RwV3d* const axis = reinterpret_cast<const RwV3d*>(S(kAux + 0x20));
        const float ang = r.below(8) ? (r.f01() * 2.f - 1.f) * 7.f : Fe(r, 30.f, 1);
        return XCaseP(r, d, [&]() -> uint64_t { RotateVecAboutVec(*res, *vec, *axis, ang); return 0; },
                            [&]() -> uint64_t { oracle::Fn<void __cdecl(void*, const void*, const void*, float)>(0x4A1780)(res, vec, axis, ang); return 0; },
                            [](Rng& rr) {
                                BuildCurve(rr);
                                PutVec(rr, kAux + 0x10, 20.f, 14);
                                PutVec(rr, kAux + 0x20, 1.f, 14);
                                if (rr.below(2)) { float* v = reinterpret_cast<float*>(S(kAux + 0x20)); const float m = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); if (m > 1e-6f) for (int k = 0; k < 3; ++k) v[k] /= m; }   // unit axis
                            });
    XEND(4000)
    XROW("CCarFXRenderer::SetFxEnvMapLightMult", 0x5D5BA0, 0)
        const float m = r.below(6) ? Fg(r, 4.f) : Fe(r, 100.f, 1);
        Reg g{ 0x8D12C0, 0x20 };
        const uint32_t seed = r.u32() | 1;
        BuildCurve(r);
        return BothA(d, seed, { g }, [&]() -> uint64_t { CCarFXRenderer::SetFxEnvMapLightMult(m); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl(float)>(0x5D5BA0)(m); return 0; });
    XEND(100)
    XROW("CCarFXRenderer::IsCCPCPipelineAttached", 0x5D5B80, 0)
        // the plugin offset (exe global 0x8D6080) selects where in the atomic block the pipeline id sits; ids: the car pipeline's, a near miss, garbage
        RpAtomic* const at = reinterpret_cast<RpAtomic*>(S(kAtom));
        const int32_t off = (int32_t)r.below(0x180);
        const uint32_t idv = r.below(3) == 0 ? 0x53F2009A : (r.below(2) ? 0x53F2009A ^ (1u << r.below(32)) : r.u32());
        Reg g{ 0x8D6080, 4 };
        const uint32_t seed = r.u32() | 1;
        BuildCurve(r);
        *reinterpret_cast<int32_t*>(0x8D6080) = off;
        SAt<uint32_t>(kAtom + off) = idv;
        return BothA(d, seed, { g, Reg{ (uintptr_t)g_sc + kAtom, 0x400 } }, [&]() -> uint64_t { return CCarFXRenderer::IsCCPCPipelineAttached(at) ? 1 : 0; },
                     [&]() -> uint64_t { return (uint8_t)oracle::Fn<uint32_t __cdecl(void*)>(0x5D5B80)(at); });
    XEND(300)
}

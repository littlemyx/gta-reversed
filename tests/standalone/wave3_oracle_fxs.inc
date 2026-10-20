// ORACLE-W3-FX: exe-oracle rows for game_sa/Fx (FxSystem_c, FxManager_c, FxMemoryPool_c, FxSphere_c, FxFrustumInfo_c, FxSystemBP_c, FxEmitterBP_c) on fixture blocks.
// Included by wave2_oracle_all.inc when W3_FXS_ONLY is defined (executable wave3_fxs_oracle_test = the W2 harness: deterministic arena, whole exe data image + the fixture block compared after every case).
// Fixture block g_sc (fake objects at fixed offsets; both sides run on identical bytes): an FxSystem_c over a blueprint, a prim pointer array, particle list nodes, RwMatrix / box / sphere / plane blobs,
// an FxManager_c, a camera + frame in BOTH RenderWare layouts (the exe's and the port's librw one), an emitter blueprint, and the memory pool data. g_fxMan (exe address 0xA9AE80) is used where the
// function reaches it by itself (FxSystemBP_c::SetBoundingSphere / operator new, FxEmitterBP_c::FreePrtFromPrim).
// vptrs of the fixture objects (FxSystem_c::m_FireAE, FxEmitterBP_c) differ by construction and are not compared.
#include "Fx/FxSystem.h"
#include "Fx/FxManager.h"
#include "Fx/FxMemoryPool.h"
#include "Fx/FxSphere.h"
#include "Fx/FxFrustumInfo.h"
#include "Fx/FxSystemBP.h"
#include "Fx/FxEmitterBP.h"
#include "Fx/FxEmitterPrt.h"
#include "Fx/FxPrim.h"
#include "Fx/FxBox.h"
#include "Fx/FxSphere.cpp"   // IsCollision / GetDistToPlane are declared `inline` in the header: the bodies must be visible in this TU (FxSphere.cpp is not in the target)

namespace w3x {
using namespace w2;
constexpr size_t kScSize = 0x4000;
alignas(16) static uint8_t g_sc[kScSize];
constexpr size_t kSys = 0x000, kBP = 0x120, kPrimArr = 0x160, kPrim = 0x180, kMA = 0x200, kMB = 0x240, kMC = 0x280, kBox = 0x2C0, kBoxPtr = 0x2D8, kPtcl = 0x2E0, kSphA = 0x320, kSphB = 0x340, kSphOut = 0x360,
                 kPlane = 0x380, kVec = 0x390, kMgr = 0x400, kNodes = 0x500, kNodeSz = 0x40, kEmBP = 0x800, kCam = 0x2000, kCamE = 0x2200, kFrame = 0x2400, kFrameE = 0x2480, kFI = 0xD00, kPool = 0x1000, kPoolObj = 0x0F00, kScalar = 0x3F0;
static uint8_t* S(size_t off) { return g_sc + off; }
template<class T> static T& SAt(size_t off) { return *reinterpret_cast<T*>(g_sc + off); }
static uint64_t U(const void* p) { return (uint64_t)(uintptr_t)p; }
static uint64_t FB(float f) { uint32_t b; std::memcpy(&b, &f, 4); return b; }
template<class R, class... A> static R ExeThis(uint32_t addr, void* self, A... a) { return oracle::Fn<R __fastcall(void*, int, A...)>(addr)(self, 0, a...); }

// game-range value, or (1 in `special`) one of the edge values (NaN, +-inf, 0, denormal, tiny)
static float Fe(Rng& r, float scale, int special = 10) {
    if (special && r.below((uint32_t)special) == 0) return GenF(r, scale, S_NAN | S_ZERO | S_DEN | S_TINY | S_INF | S_HUGE);
    return GenF(r, scale, S_ZERO);
}
static float Fg(Rng& r, float scale) { return GenF(r, scale, S_ZERO); }   // game-range, no specials
static void PutVec(Rng& r, size_t off, float scale, int special = 10) { for (int k = 0; k < 3; ++k) SAt<float>(off + 4 * k) = Fe(r, scale, special); }
static void PutMat(Rng& r, size_t off, float scale, int special = 10) { for (int k = 0; k < 16; ++k) SAt<float>(off + 4 * k) = Fe(r, k >= 12 ? scale : 1.f, special); }

static FxSystem_c* Sys() { return reinterpret_cast<FxSystem_c*>(S(kSys)); }
static FxManager_c* Mgr() { return reinterpret_cast<FxManager_c*>(S(kMgr)); }
static FxMemoryPool_c* Pool() { return reinterpret_cast<FxMemoryPool_c*>(S(kPoolObj)); }
static FxSphere_c* SphA() { return reinterpret_cast<FxSphere_c*>(S(kSphA)); }
static FxSphere_c* SphB() { return reinterpret_cast<FxSphere_c*>(S(kSphB)); }

// standard fixture: garbage everywhere, then the pointers a valid object holds
static void BuildAll(Rng& r) {
    FillGarbage(r, g_sc, kScSize);
    FxSystem_c* s = Sys();
    s->m_SystemBP = reinterpret_cast<FxSystemBP_c*>(S(kBP));
    s->m_ParentMatrix = r.below(4) ? reinterpret_cast<RwMatrix*>(S(kMA)) : nullptr;
    s->m_Prims = reinterpret_cast<FxPrim_c**>(S(kPrimArr));
    s->m_BoundingSphere = r.below(3) ? reinterpret_cast<FxSphere_c*>(S(kSphA)) : nullptr;
    const uint32_t st = r.below(8); s->m_nPlayStatus = (eFxSystemPlayStatus)(st < 6 ? st % 3 : r.below(256));
    for (int i = 0; i < 4; ++i) SAt<void*>(kPrimArr + 4 * i) = S(kPrim + 0x20 * i);
    SAt<FxBox_c*>(kBoxPtr) = reinterpret_cast<FxBox_c*>(S(kBox));
}
// uninitialised in the exe (its constructor does not touch them), zeroed by the port's default member initialisers: not compared in the constructor rows
static std::vector<std::pair<size_t, size_t>> g_unc;
template<class PF, class EF>
static bool XCase(Rng& r, std::string& d, PF&& pf, EF&& ef, void (*build)(Rng&) = &BuildAll) {
    const uint32_t seed = r.u32() | 1;
    build(r);
    Reg sc{ (uintptr_t)g_sc, kScSize };
    sc.mask.assign(kScSize, 0);
    for (size_t b = 0; b < 4; ++b) { sc.mask[kSys + 0x7C + b] = 1; sc.mask[kEmBP + b] = 1; }
    for (auto [a, n] : g_unc) for (size_t b = 0; b < n; ++b) sc.mask[a + b] = 1;
    Reg img{ kImgLo, kImgHi - kImgLo };
    return BothA(d, seed, { sc, img }, std::forward<PF>(pf), std::forward<EF>(ef));
}
// a singly built doubly linked list of `k` of the 8 nodes (random order) at list header `hdrOff` (head, tail, count); nodes carry m_System (+0x28) pointing at kSys / garbage
static void PutList(Rng& r, size_t hdrOff, int k, int total = 8) {
    int idx[8]; for (int i = 0; i < 8; ++i) idx[i] = i;
    for (int i = 7; i > 0; --i) std::swap(idx[i], idx[r.below(i + 1)]);
    auto Np = [&](int j) { return S(kNodes + idx[j] * kNodeSz); };
    for (int j = 0; j < k; ++j) {
        SAt<uint32_t>(Np(j) - g_sc + 0) = j ? (uint32_t)(uintptr_t)Np(j - 1) : 0;
        SAt<uint32_t>(Np(j) - g_sc + 4) = j + 1 < k ? (uint32_t)(uintptr_t)Np(j + 1) : 0;
        SAt<uint32_t>(Np(j) - g_sc + 0x28) = r.below(3) ? (uint32_t)(uintptr_t)S(kSys) : r.u32();
    }
    SAt<uint32_t>(hdrOff) = k ? (uint32_t)(uintptr_t)Np(0) : 0;
    SAt<uint32_t>(hdrOff + 4) = k ? (uint32_t)(uintptr_t)Np(k - 1) : 0;
    SAt<uint32_t>(hdrOff + 8) = (uint32_t)k;
    (void)total;
}
static int g_k = 0, g_pick = 0;
static void BuildMgrList(Rng& r) { BuildAll(r); PutList(r, kMgr + 0x1C, g_k); }
static void BuildSphereScene(Rng& r) {
    BuildAll(r);
    for (size_t off : { kSphA, kSphB }) {
        PutVec(r, off, 80.f, 14);
        SAt<float>(off + 12) = r.below(14) == 0 ? Fe(r, 5.f) : Fg(r, 1.f) * 0.5f + 0.5f;
        SAt<uint32_t>(off + 16) = r.below(3) ? r.below(4) : r.u32();
    }
    if (r.below(2)) {   // near each other: both outcomes
        for (int k = 0; k < 3; ++k) SAt<float>(kSphB + 4 * k) = SAt<float>(kSphA + 4 * k) + Fg(r, 3.f);
        SAt<float>(kSphA + 12) = std::fabs(Fg(r, 3.f)); SAt<float>(kSphB + 12) = std::fabs(Fg(r, 3.f));
    }
    for (int k = 0; k < 4; ++k) {   // frustum planes at kFI + 0x14, sphere at kFI
        float* p = reinterpret_cast<float*>(S(kFI + 0x14 + 16 * k));
        float n[3] = { Fg(r, 1.f), Fg(r, 1.f), Fg(r, 1.f) };
        const float m = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]) + 1e-3f;
        for (int a = 0; a < 3; ++a) p[a] = n[a] / m;
        p[3] = Fg(r, 60.f);
        if (r.below(20) == 0) p[r.below(4)] = Fe(r, 10.f, 1);
    }
    std::memcpy(S(kFI), S(kSphA), 0x14);
    if (r.below(2)) { for (int k = 0; k < 3; ++k) SAt<float>(kFI + 4 * k) = SAt<float>(kSphB + 4 * k) + Fg(r, 30.f); SAt<float>(kFI + 12) = 20.f + std::fabs(Fg(r, 400.f)); }
    PutVec(r, kPlane, 1.f, 14); SAt<float>(kPlane + 12) = Fe(r, 100.f, 14);
}
static void BuildPool(Rng& r) {
    BuildAll(r);
    SAt<uint32_t>(kPoolObj) = (uint32_t)(uintptr_t)S(kPool);
    SAt<uint32_t>(kPoolObj + 4) = 0x1000;
    SAt<uint32_t>(kPoolObj + 8) = r.below(4) ? r.below(0x600) : r.below(0x2000);
}
// g_fxMan.m_Pool (exe address 0xA9AF2C) over the fixture pool data
static void BuildGlobalPool(Rng& r) {
    BuildAll(r);
    auto* m = reinterpret_cast<uint32_t*>(0xA9AF2C);
    m[0] = (uint32_t)(uintptr_t)S(kPool); m[1] = 0x1000; m[2] = r.below(4) ? r.below(0x600) : r.below(0xF00);
}
} // namespace w3x

#define XROW(NAME, ADDR, CASES) Run(NAME " " #ADDR, [&](Rng& r, std::string& d) -> bool {
#define XEND(CASES) }, std::max(10, (CASES) / w3x_div()));
static int w3x_div() { static const int v = std::getenv("W3X_DIV") ? std::max(1, std::atoi(std::getenv("W3X_DIV"))) : 1; return v; }

static void TestW3FxsSystem() {
    using namespace w2; using namespace w3x;
    FxSystem_c* const s = Sys();
    XROW("FxSystem_c::Constructor", 0x4AAF00, 0)
        g_unc = { { kSys + 0x80, 0x0C }, { kSys + 0x90, 0x70 } };   // CAEAudioEntity members of m_FireAE other than the two the exe clears (+0x8C, +0x100)
        const bool ok = XCase(r, d, [&]() -> uint64_t { return U(s->Constructor()); }, [&]() -> uint64_t { return U(ExeThis<void*>(0x4AAF00, s)); });
        g_unc.clear(); return ok;
    XEND(100)
    XROW("FxSystem_c::Pause", 0x4AA370, 0)
        return XCase(r, d, [&]() -> uint64_t { s->Pause(); return 0; }, [&]() -> uint64_t { ExeThis<void>(0x4AA370, s); return 0; });
    XEND(200)
    XROW("FxSystem_c::GetPlayStatus", 0x4AA900, 0)
        return XCase(r, d, [&]() -> uint64_t { return (uint8_t)s->GetPlayStatus(); }, [&]() -> uint64_t { return (uint8_t)ExeThis<uint32_t>(0x4AA900, s); });
    XEND(100)
    XROW("FxSystem_c::EnablePrim", 0x4AA610, 0)
        const int idx = (int)r.below(4); const bool en = r.below(2);
        return XCase(r, d, [&]() -> uint64_t { s->EnablePrim(idx, en); return 0; }, [&]() -> uint64_t { ExeThis<void, int32_t, uint32_t>(0x4AA610, s, idx, en ? 1u : 0u); return 0; });
    XEND(300)
    XROW("FxSystem_c::SetMatrix", 0x4AA630, 0)
        RwMatrix* m = reinterpret_cast<RwMatrix*>(S(kMB));
        return XCase(r, d, [&]() -> uint64_t { s->SetMatrix(m); return 0; }, [&]() -> uint64_t { ExeThis<void, void*>(0x4AA630, s, m); return 0; });
    XEND(300)
    XROW("FxSystem_c::SetOffsetPos", 0x4AA660, 0)
        const CVector* p = reinterpret_cast<const CVector*>(S(kVec));
        return XCase(r, d, [&]() -> uint64_t { s->SetOffsetPos(*p); return 0; }, [&]() -> uint64_t { ExeThis<void, const void*>(0x4AA660, s, p); return 0; },
            [](Rng& rr) { BuildAll(rr); PutVec(rr, kVec, 200.f); PutMat(rr, kSys + offsetof(FxSystem_c, m_LocalMatrix), 100.f, 20); });
    XEND(400)
    XROW("FxSystem_c::AddOffsetPos", 0x4AA690, 0)
        const CVector* p = reinterpret_cast<const CVector*>(S(kVec));
        return XCase(r, d, [&]() -> uint64_t { s->AddOffsetPos(*p); return 0; }, [&]() -> uint64_t { ExeThis<void, const void*>(0x4AA690, s, p); return 0; },
            [](Rng& rr) { BuildAll(rr); PutVec(rr, kVec, 200.f); PutMat(rr, kSys + offsetof(FxSystem_c, m_LocalMatrix), 100.f, 20); });
    XEND(400)
    XROW("FxSystem_c::SetConstTime", 0x4AA6C0, 0)
        const bool on = r.below(2);
        float t; switch (r.below(6)) { case 0: t = Fe(r, 1e12f, 1); break; case 1: t = Fg(r, 1e7f); break; case 2: t = Fg(r, 300.f); break; default: t = Fg(r, 255.f); }
        return XCase(r, d, [&]() -> uint64_t { s->SetConstTime(on, t); return 0; }, [&]() -> uint64_t { ExeThis<void, uint32_t, float>(0x4AA6C0, s, on ? 1u : 0u, t); return 0; });
    XEND(600)
    XROW("FxSystem_c::SetRateMult", 0x4AA6F0, 0)
        float t; switch (r.below(6)) { case 0: t = Fe(r, 1e12f, 1); break; case 1: t = Fg(r, 1e7f); break; case 2: t = Fg(r, 70.f); break; default: t = Fg(r, 2.f); }
        return XCase(r, d, [&]() -> uint64_t { s->SetRateMult(t); return 0; }, [&]() -> uint64_t { ExeThis<void, float>(0x4AA6F0, s, t); return 0; });
    XEND(600)
    XROW("FxSystem_c::SetTimeMult", 0x4AA710, 0)
        float t; switch (r.below(6)) { case 0: t = Fe(r, 1e12f, 1); break; case 1: t = Fg(r, 1e7f); break; case 2: t = Fg(r, 70.f); break; default: t = Fg(r, 2.f); }
        return XCase(r, d, [&]() -> uint64_t { s->SetTimeMult(t); return 0; }, [&]() -> uint64_t { ExeThis<void, float>(0x4AA710, s, t); return 0; });
    XEND(600)
    XROW("FxSystem_c::SetVelAdd", 0x4AA730, 0)
        const CVector* p = reinterpret_cast<const CVector*>(S(kVec));
        return XCase(r, d, [&]() -> uint64_t { s->SetVelAdd(*p); return 0; }, [&]() -> uint64_t { ExeThis<void, const void*>(0x4AA730, s, p); return 0; });
    XEND(200)
    XROW("FxSystem_c::SetLocalParticles", 0x4AA910, 0)
        const bool b = r.below(2);
        return XCase(r, d, [&]() -> uint64_t { s->SetLocalParticles(b); return 0; }, [&]() -> uint64_t { ExeThis<void, uint32_t>(0x4AA910, s, b ? 1u : 0u); return 0; });
    XEND(200)
    XROW("FxSystem_c::SetZTestEnable", 0x4AAC50, 0)
        const bool b = r.below(2);
        return XCase(r, d, [&]() -> uint64_t { s->SetZTestEnable(b); return 0; }, [&]() -> uint64_t { ExeThis<void, uint32_t>(0x4AAC50, s, b ? 1u : 0u); return 0; });
    XEND(200)
    XROW("FxSystem_c::SetMustCreatePrts", 0x4AAC70, 0)
        const bool b = r.below(2);
        return XCase(r, d, [&]() -> uint64_t { s->SetMustCreatePrts(b); return 0; }, [&]() -> uint64_t { ExeThis<void, uint32_t>(0x4AAC70, s, b ? 1u : 0u); return 0; });
    XEND(200)
    XROW("FxSystem_c::GetBoundingSphereLcl", 0x4AAB50, 0)
        FxSphere_c* out = reinterpret_cast<FxSphere_c*>(S(kSphOut));
        return XCase(r, d, [&]() -> uint64_t { return s->GetBoundingSphereLcl(out) ? 1 : 0; }, [&]() -> uint64_t { return (uint8_t)ExeThis<uint32_t, void*>(0x4AAB50, s, out); });
    XEND(300)
    XROW("FxSystem_c::UpdateBoundingBoxCB", 0x4AA9A0, 0)
        Particle_c* pt = reinterpret_cast<Particle_c*>(S(kPtcl)); FxBox_c** bp = reinterpret_cast<FxBox_c**>(S(kBoxPtr));
        const int32 a2 = r.below(8) ? 0 : (int32)r.below(3);
        return XCase(r, d, [&]() -> uint64_t { FxSystem_c::UpdateBoundingBoxCB(pt, a2, bp); return 0; },
            [&]() -> uint64_t { oracle::Fn<void __cdecl(void*, int32_t, void*)>(0x4AA9A0)(pt, a2, bp); return 0; },
            [](Rng& rr) {
                BuildAll(rr);
                float pos[3]; for (float& v : pos) v = Fe(rr, 300.f, 12);
                std::memcpy(S(kPtcl + 0x10), pos, 12);
                for (int a = 0; a < 3; ++a) {   // box around the particle: min below / above, max above / below, equal, specials
                    for (int mm = 0; mm < 2; ++mm) {
                        float v;
                        switch (rr.below(5)) { case 0: v = pos[a]; break; case 1: v = Fe(rr, 300.f, 6); break; default: v = pos[a] + (mm ? 1.f : -1.f) * (rr.below(2) ? 1.f : -1.f) * std::fabs(Fg(rr, 50.f)); }
                        SAt<float>(kBox + 8 * a + 4 * mm) = v;
                    }
                }
            });
    XEND(1500)
}

static void TestW3FxsManager() {
    using namespace w2; using namespace w3x;
    FxManager_c* const m = Mgr();
    XROW("FxManager_c::Constructor", 0x4A9470, 0)
        g_unc = { { kMgr + 0x34, 0x10 }, { kMgr + 0x48, 0x40 } };   // frustum sphere centre / radius and planes: only m_nNumPlanesPassed is cleared by the exe
        const bool ok = XCase(r, d, [&]() -> uint64_t { return U(m->Constructor()); }, [&]() -> uint64_t { return U(ExeThis<void*>(0x4A9470, m)); });
        g_unc.clear(); return ok;
    XEND(100)
    XROW("FxManager_c::Destructor", 0x4A90A0, 0)   // the exe returns whatever eax holds (not compared); the lists' destructors are `ret`
        return XCase(r, d, [&]() -> uint64_t { m->Destructor(); return 0; }, [&]() -> uint64_t { ExeThis<void>(0x4A90A0, m); return 0; });
    XEND(60)
    XROW("FxManager_c::GetFrustumInfo", 0x4A9130, 0)
        return XCase(r, d, [&]() -> uint64_t { return U(m->GetFrustumInfo()); }, [&]() -> uint64_t { return U(ExeThis<void*>(0x4A9130, m)); });
    XEND(30)
    XROW("FxManager_c::SetWindData", 0x4A93E0, 0)
        auto* dir = reinterpret_cast<CVector*>(S(kVec)); auto* spd = reinterpret_cast<float*>(S(kScalar));
        return XCase(r, d, [&]() -> uint64_t { m->SetWindData(dir, spd); return 0; }, [&]() -> uint64_t { ExeThis<void, void*, void*>(0x4A93E0, m, dir, spd); return 0; });
    XEND(60)
    XROW("FxManager_c::FxRwMatrixCreate", 0x4A9440, 0)
        return XCase(r, d, [&]() -> uint64_t { return U(m->FxRwMatrixCreate()); }, [&]() -> uint64_t { return U(ExeThis<void*>(0x4A9440, m)); },
            [](Rng& rr) { BuildAll(rr); SAt<uint32_t>(kMgr + 0x88) = rr.below(8); });
    XEND(200)
    XROW("FxManager_c::FxRwMatrixDestroy", 0x4A9460, 0)
        RwMatrix* mt = reinterpret_cast<RwMatrix*>(S(kMA));
        return XCase(r, d, [&]() -> uint64_t { m->FxRwMatrixDestroy(mt); return 0; }, [&]() -> uint64_t { ExeThis<void, void*>(0x4A9460, m, mt); return 0; },
            [](Rng& rr) { BuildAll(rr); SAt<uint32_t>(kMgr + 0x88) = rr.below(9); });
    XEND(200)
    XROW("FxManager_c::ReturnParticle", 0x4A93B0, 0)
        g_k = (int)r.below(7); g_pick = (int)r.below(8);
        // the particle to return must not be in the list already (the real callers take it out first): an unlinked node, the list holds g_k of the other nodes
        FxEmitterPrt_c* em = reinterpret_cast<FxEmitterPrt_c*>(S(kNodes + 7 * kNodeSz));
        return XCase(r, d, [&]() -> uint64_t { m->ReturnParticle(em); return 0; }, [&]() -> uint64_t { ExeThis<void, void*>(0x4A93B0, m, em); return 0; },
            [](Rng& rr) { BuildAll(rr); PutList(rr, kMgr + 0x1C, g_k); /* may include node 7: remove it */
                for (size_t o = 0; o < 3; ++o) { (void)o; }
                // rebuild avoiding node 7: relink nodes 0..6 only
                int idx[7]; for (int i = 0; i < 7; ++i) idx[i] = i; for (int i = 6; i > 0; --i) std::swap(idx[i], idx[rr.below(i + 1)]);
                auto Np = [&](int j) { return S(kNodes + idx[j] * kNodeSz); };
                for (int j = 0; j < g_k; ++j) { SAt<uint32_t>(Np(j) - g_sc) = j ? (uint32_t)(uintptr_t)Np(j - 1) : 0; SAt<uint32_t>(Np(j) - g_sc + 4) = j + 1 < g_k ? (uint32_t)(uintptr_t)Np(j + 1) : 0; }
                SAt<uint32_t>(kMgr + 0x1C) = g_k ? (uint32_t)(uintptr_t)Np(0) : 0; SAt<uint32_t>(kMgr + 0x20) = g_k ? (uint32_t)(uintptr_t)Np(g_k - 1) : 0; SAt<uint32_t>(kMgr + 0x24) = (uint32_t)g_k; });
    XEND(300)
    XROW("FxManager_c::GetParticle", 0x4A93C0, 0)
        g_k = (int)r.below(8);
        return XCase(r, d, [&]() -> uint64_t { return U(m->GetParticle(0)); }, [&]() -> uint64_t { return U(ExeThis<void*, uint32_t>(0x4A93C0, m, 0u)); }, &BuildMgrList);
    XEND(300)
    XROW("FxManager_c::CalcFrustumInfo", 0x4A9140, 0)
        RwCamera* cam = reinterpret_cast<RwCamera*>(S(kCam)); void* camE = S(kCamE);
        return XCase(r, d, [&]() -> uint64_t { m->CalcFrustumInfo(cam); return 0; }, [&]() -> uint64_t { ExeThis<void, void*>(0x4A9140, m, camE); return 0; },
            [](Rng& rr) {
                BuildAll(rr);
                RwCamera* c = reinterpret_cast<RwCamera*>(S(kCam)); RwFrame* f = reinterpret_cast<RwFrame*>(S(kFrame));
                const float vx = Fg(rr, 1.2f), vy = Fg(rr, 1.2f), farp = rr.below(12) == 0 ? Fe(rr, 4000.f, 1) : 50.f + std::fabs(Fg(rr, 3000.f));
                float at[3] = { Fg(rr, 1.f), Fg(rr, 1.f), Fg(rr, 1.f) }, pos[3] = { Fg(rr, 3000.f), Fg(rr, 3000.f), Fg(rr, 200.f) };
                float pl[6][4]; for (auto& q : pl) for (float& v : q) v = Fg(rr, 500.f);
                // port camera (librw layout) through the accessors; the exe's camera is a separate buffer in the exe's layout:
                // parent frame +4, frame matrix at frame+0x10 (at +0x20, pos +0x30), view window +0x68 / +0x6C, far plane +0x84, frustum planes (0x14 apart, normal then distance) from +0x94
                SAt<uint32_t>(kCam + 4) = (uint32_t)(uintptr_t)f; SAt<uint32_t>(kCamE + 4) = (uint32_t)(uintptr_t)S(kFrameE);
                RwCameraGetViewWindow(c)->x = vx; RwCameraGetViewWindow(c)->y = vy; c->farPlane = farp;
                RwMatrix* pm = RwFrameGetMatrix(f); pm->at.x = at[0]; pm->at.y = at[1]; pm->at.z = at[2]; pm->pos.x = pos[0]; pm->pos.y = pos[1]; pm->pos.z = pos[2];
                SAt<float>(kCamE + 0x68) = vx; SAt<float>(kCamE + 0x6C) = vy; SAt<float>(kCamE + 0x84) = farp;
                for (int k = 0; k < 3; ++k) { SAt<float>(kFrameE + 0x10 + 0x20 + 4 * k) = at[k]; SAt<float>(kFrameE + 0x10 + 0x30 + 4 * k) = pos[k]; }
                for (int i = 0; i < 6; ++i) {
                    c->frustumPlanes[i].plane.normal.x = pl[i][0]; c->frustumPlanes[i].plane.normal.y = pl[i][1]; c->frustumPlanes[i].plane.normal.z = pl[i][2]; c->frustumPlanes[i].plane.distance = pl[i][3];
                    for (int k = 0; k < 4; ++k) SAt<float>(kCamE + 0x94 + 0x14 * i + 4 * k) = pl[i][k];
                }
            });
    XEND(1500)
}

static void TestW3FxsSmall() {
    using namespace w2; using namespace w3x;
    FxMemoryPool_c* const p = Pool();
    XROW("FxMemoryPool_c::Reset", 0x4A9C90, 0)
        return XCase(r, d, [&]() -> uint64_t { p->Reset(); return 0; }, [&]() -> uint64_t { ExeThis<void>(0x4A9C90, p); return 0; }, &BuildPool);
    XEND(30)
    XROW("FxMemoryPool_c::GetMem", 0x4A9CA0, 0)
        static const int32_t al[] = { 1, 2, 3, 4, 4, 4, 8, 8, 16, 32, 64, 7, 0x100 };
        const int32_t align = al[r.below((uint32_t)(sizeof al / sizeof al[0]))];
        const int32_t size = r.below(8) ? (int32_t)r.below(0x300) : (int32_t)r.u32();
        return XCase(r, d, [&]() -> uint64_t { return U(p->GetMem(size, align)); }, [&]() -> uint64_t { return U(ExeThis<void*, int32_t, int32_t>(0x4A9CA0, p, size, align)); }, &BuildPool);
    XEND(600)
    XROW("FxSystemBP_c::operator new", 0x4AA100, 0)
        const uint32_t sz = r.below(8) ? r.below(0x200) : r.u32();
        return XCase(r, d, [&]() -> uint64_t { return U(FxSystemBP_c::operator new(sz)); }, [&]() -> uint64_t { return U(oracle::Fn<void* __cdecl(uint32_t)>(0x4AA100)(sz)); }, &BuildGlobalPool);
    XEND(200)
    XROW("FxEmitterPrt_c::operator new[]", 0x4A18B0, 0)
        const uint32_t sz = r.below(8) ? r.below(0x200) : r.u32();
        return XCase(r, d, [&]() -> uint64_t { return U(FxEmitterPrt_c::operator new[](sz)); }, [&]() -> uint64_t { return U(oracle::Fn<void* __cdecl(uint32_t)>(0x4A18B0)(sz)); }, &BuildGlobalPool);
    XEND(200)
    XROW("FxSphere_c::IsCollision", 0x4A9FC0, 0)
        return XCase(r, d, [&]() -> uint64_t { return SphA()->IsCollision(*SphB()) ? 1 : 0; }, [&]() -> uint64_t { return (uint8_t)ExeThis<uint32_t, void*>(0x4A9FC0, SphA(), SphB()); }, &BuildSphereScene);
    XEND(3000)
    XROW("FxSphere_c::GetDistToPlane", 0x4AA010, 0)
        const FxPlane_c* pl = reinterpret_cast<const FxPlane_c*>(S(kPlane));
        return XCase(r, d, [&]() -> uint64_t { return FB(SphA()->GetDistToPlane(*pl)); }, [&]() -> uint64_t { return FB(ExeThis<float, const void*>(0x4AA010, SphA(), pl)); }, &BuildSphereScene);
    XEND(2000)
    XROW("FxFrustumInfo_c::IsCollision", 0x4AA030, 0)
        FxFrustumInfo_c* fi = reinterpret_cast<FxFrustumInfo_c*>(S(kFI));
        return XCase(r, d, [&]() -> uint64_t { return fi->IsCollision(*SphB()) ? 1 : 0; }, [&]() -> uint64_t { return (uint8_t)ExeThis<uint32_t, void*>(0x4AA030, fi, SphB()); }, &BuildSphereScene);
    XEND(4000)
    FxSystemBP_c* const bp = reinterpret_cast<FxSystemBP_c*>(S(kBP));
    XROW("FxSystemBP_c::Constructor", 0x4AA0D0, 0)
        return XCase(r, d, [&]() -> uint64_t { return U(bp->Constructor()); }, [&]() -> uint64_t { return U(ExeThis<void*>(0x4AA0D0, bp)); });
    XEND(60)
    XROW("FxSystemBP_c::Destructor", 0x4AA0F0, 0)
        return XCase(r, d, [&]() -> uint64_t { bp->Destructor(); return 0; }, [&]() -> uint64_t { ExeThis<void>(0x4AA0F0, bp); return 0; });
    XEND(30)
    XROW("FxSystemBP_c::Unload", 0x4AA120, 0)
        return XCase(r, d, [&]() -> uint64_t { bp->Unload(); return 0; }, [&]() -> uint64_t { ExeThis<void>(0x4AA120, bp); return 0; });
    XEND(30)
    XROW("FxSystemBP_c::GetBoundingSphere", 0x4AA1F0, 0)
        return XCase(r, d, [&]() -> uint64_t { return U(bp->GetBoundingSphere()); }, [&]() -> uint64_t { return U(ExeThis<void*>(0x4AA1F0, bp)); });
    XEND(60)
    XROW("FxSystemBP_c::SetBoundingSphere", 0x4AA200, 0)
        const CVector* c = reinterpret_cast<const CVector*>(S(kVec));
        float rad; switch (r.below(5)) { case 0: rad = Fe(r, 50.f, 1); break; case 1: rad = 0.f; break; default: rad = Fg(r, 40.f); }
        return XCase(r, d, [&]() -> uint64_t { bp->SetBoundingSphere(*c, rad); return 0; }, [&]() -> uint64_t { ExeThis<void, const void*, float>(0x4AA200, bp, c, rad); return 0; },
            [](Rng& rr) { BuildGlobalPool(rr); SAt<uint32_t>(kBP + 0x20) = rr.below(3) ? 0 : (uint32_t)(uintptr_t)S(kSphA); PutVec(rr, kVec, 300.f, 12); });
    XEND(500)
    FxEmitterBP_c* const eb = reinterpret_cast<FxEmitterBP_c*>(S(kEmBP));
    XROW("FxEmitterBP_c::Constructor", 0x4A18D0, 0)
        return XCase(r, d, [&]() -> uint64_t { return U(new (eb) FxEmitterBP_c()); }, [&]() -> uint64_t { return U(ExeThis<void*>(0x4A18D0, eb)); });
    XEND(60)
    XROW("FxEmitterBP_c::FreePrtFromPrim", 0x4A2510, 0)
        // prim list at +0x20 holds g_k nodes, some with m_System == the system asked for; the found node goes back to g_fxMan.m_FxEmitterParticles
        g_k = (int)r.below(7);
        FxSystem_c* sys = Sys();
        return XCase(r, d, [&]() -> uint64_t { return eb->FxEmitterBP_c::FreePrtFromPrim(sys) ? 1 : 0; }, [&]() -> uint64_t { return (uint8_t)ExeThis<uint32_t, void*>(0x4A2510, eb, sys); },
            [](Rng& rr) {
                BuildGlobalPool(rr);
                PutList(rr, kEmBP + 0x20, g_k);
                // g_fxMan's free-particle list: a few other nodes would alias the emitter's list nodes: keep it empty or a single foreign node
                auto* l = reinterpret_cast<uint32_t*>(0xA9AE9C);
                l[0] = l[1] = l[2] = 0;
                if (rr.below(2)) { uint8_t* fn = S(kNodes + 7 * kNodeSz); bool used = false; for (uint32_t q = SAt<uint32_t>(kEmBP + 0x20); q; q = *reinterpret_cast<uint32_t*>(q + 4)) used |= q == (uint32_t)(uintptr_t)fn; if (!used) { *reinterpret_cast<uint32_t*>(fn) = 0; *reinterpret_cast<uint32_t*>(fn + 4) = 0; l[0] = l[1] = (uint32_t)(uintptr_t)fn; l[2] = 1; } }
            });
    XEND(500)
}

#include "wave3_oracle_fxbp.inc"

static void TestW3FxsAll() {
    using namespace w3x;
    if (std::getenv("W3X_DBG")) {   // layout of the port's librw camera / frame (the camera fixture fills both layouts)
        RwCamera* c = reinterpret_cast<RwCamera*>(S(kCam)); RwFrame* f = reinterpret_cast<RwFrame*>(S(kFrame));
        std::printf("port layout: viewWindow +%X farPlane +%X frustumPlanes +%X parent +%X frameMatrix +%X\n", (unsigned)((uint8_t*)RwCameraGetViewWindow(c) - (uint8_t*)c), (unsigned)((uint8_t*)&c->farPlane - (uint8_t*)c),
                    (unsigned)((uint8_t*)&c->frustumPlanes[0] - (uint8_t*)c), (unsigned)((uint8_t*)&((RwObject*)c)->parent - (uint8_t*)c), (unsigned)((uint8_t*)RwFrameGetMatrix(f) - (uint8_t*)f));
    }
    (void)spdlog::default_logger_raw();   // the default logger is created lazily by the first NOTSA_LOG_* of the port: do it outside the redirected allocator
    TestW3FxsSystem();
    TestW3FxsManager();
    TestW3FxsSmall();
    TestW3FxsBlueprints();
}

// ORACLE-W3-VIS: exe-oracle rows for the A + B functions of game_sa/VisibilityPlugins.cpp (plugin accessors / constructors, alpha lists, fading alpha, camera distances, SetRenderWareCamera) and
// app/app_light.cpp (scene light colours / flags). Included by wave2_oracle_all.inc when W3_VIS_ONLY is defined (executable wave3_vis_oracle_test = the W2 harness: deterministic arena,
// the whole exe data image + the fixture block compared after every case).
// Fixture block g_sc: RW-plugin objects as raw blobs (atomic / clump / frame: the plugin offsets ms_*PluginOffset are random so the accessors are exercised at any offset), matrices,
// entity / model info / col model blobs, the five alpha link lists + the weapon-ped list over node pools, RpLight blobs, a camera + frame in BOTH RenderWare layouts.
// RenderWare is not run: the exe's RW entry points (RpLightSetColor 0x751A90, RpClumpForAllAtomics 0x749B70) are patched to host stubs and the port's fakerw entry points of the same names
// are defined in this TU with the same stub bodies; what is compared is the order / values of the RW calls plus every fixture byte and the whole exe data image.
// Port function pointers stored in fixture data (list node callbacks, the clump visibility callback) are mapped to the exe addresses of the same functions before the compare.
#include "VisibilityPlugins.h"
#include "ModelIndices.h"
#include "app/app_light.h"
#include "PostEffects.h"

// the plugin constructors / destructors and two helpers are private members of CVisibilityPlugins: explicit-instantiation access trick (no layout assumptions, no #define private public);
// its private static data is reached through StaticRef at the exe addresses (the port's globals sit there in this test)
#define VIS_STEAL(NAME, SIG) \
    struct T_##NAME { using type = SIG; friend type vis_steal(T_##NAME); }; \
    template<class Tag, typename Tag::type P> struct R_##NAME { friend typename Tag::type vis_steal(Tag) { return P; } }; \
    template struct R_##NAME<T_##NAME, &CVisibilityPlugins::NAME>;
VIS_STEAL(AtomicCopyConstructor, void* (*)(void*, const void*, RwInt32, RwInt32))
VIS_STEAL(AtomicDestructor, void* (*)(void*, RwInt32, RwInt32))
VIS_STEAL(ClumpConstructor, void* (*)(void*, RwInt32, RwInt32))
VIS_STEAL(ClumpCopyConstructor, void* (*)(void*, const void*, RwInt32, RwInt32))
VIS_STEAL(ClumpDestructor, void* (*)(void*, RwInt32, RwInt32))
VIS_STEAL(FrameConstructor, void* (*)(void*, RwInt32, RwInt32))
VIS_STEAL(FrameCopyConstructor, void* (*)(void*, const void*, RwInt32, RwInt32))
VIS_STEAL(FrameDestructor, void* (*)(void*, RwInt32, RwInt32))
VIS_STEAL(CalculateFadingAtomicAlpha, int32 (*)(CBaseModelInfo*, CEntity*, float))
VIS_STEAL(DefaultVisibilityCB, RwBool (*)(RpClump*))
VIS_STEAL(GetDotProductWithCameraVector, float (*)(RwMatrix*, RwMatrix*, uint32))
#define STL(NAME) vis_steal(T_##NAME{})
struct T_LodPtr { using type = CEntity* CEntity::*; friend type vis_steal_lod(T_LodPtr); };
template<class Tag, typename Tag::type P> struct R_LodPtr { friend typename Tag::type vis_steal_lod(Tag) { return P; } };
template struct R_LodPtr<T_LodPtr, &CEntity::m_pLod>;

namespace w3v {
using namespace w2;
constexpr size_t kScSize = 0x4000;
alignas(16) static uint8_t g_sc[kScSize];
constexpr size_t kAtA = 0x000, kAtB = 0x100, kClA = 0x200, kClB = 0x300, kFrA = 0x400, kFrB = 0x480, kM1 = 0x500, kM2 = 0x540, kM3 = 0x580, kV1 = 0x5C0, kV2 = 0x5D0, kRec = 0x600 /*0x100*/,
                 kLtA = 0x800, kLtD = 0x840, kLtE = 0x880 /*6 x 0x40*/, kCol = 0xA00, kEnt = 0xB00, kEnt2 = 0xB80, kMi = 0xC00, kColM = 0xC40, kMat = 0xCC0,
                 kCam = 0x1000, kCamE = 0x1200, kFrame = 0x1400, kFrameE = 0x1480, kNodes = 0x2000 /*5 lists x 0x100 + weapon list 0x100*/;
constexpr int kCap = 6;
static uint8_t* S(size_t off) { return g_sc + off; }
template<class T> static T& SAt(size_t off) { return *reinterpret_cast<T*>(g_sc + off); }
static uint64_t U(const void* p) { return (uint64_t)(uintptr_t)p; }
static uint64_t FB(float f) { uint32_t b; std::memcpy(&b, &f, 4); return b; }
template<class R, class... A> static R ExeC(uint32_t addr, A... a) { return oracle::Fn<R __cdecl(A...)>(addr)(a...); }
static float Fe(Rng& r, float scale, int special = 10) {
    if (special && r.below((uint32_t)special) == 0) return GenF(r, scale, S_NAN | S_ZERO | S_DEN | S_TINY | S_INF | S_HUGE);
    return GenF(r, scale, S_ZERO);
}
static float Fg(Rng& r, float scale) { return GenF(r, scale, S_ZERO); }
static float Fp(Rng& r, float scale, int special = 12) { return r.below((uint32_t)special) == 0 ? Fe(r, scale, 1) : std::fabs(Fg(r, scale)); }   // mostly positive game range

// ---- recorded RW calls (both sides run the same stubs) ----
static void Rec(uint32_t kind, uint32_t a, uint32_t b, uint32_t c = 0, uint32_t d = 0, uint32_t e = 0) {
    uint32_t& n = SAt<uint32_t>(kRec);
    if (n < 8) { uint32_t* p = &SAt<uint32_t>(kRec + 4 + n * 24); p[0] = kind; p[1] = a; p[2] = b; p[3] = c; p[4] = d; p[5] = e; }
    ++n;
}
// the exe's/port's callbacks for the atomic callbacks, mapped to the exe address
static uint32_t MapFn(const void* f) {
    if (f == reinterpret_cast<const void*>(&setClumpForAllAtomicsFlagCB)) return 0x7322D0;
    if (f == reinterpret_cast<const void*>(&clearClumpForAllAtomicsFlagCB)) return 0x732330;
    if (f == reinterpret_cast<const void*>((const void*)STL(DefaultVisibilityCB))) return 0x732A30;
    if (f == reinterpret_cast<const void*>(&CVisibilityPlugins::RenderEntity)) return 0x732B40;
    if (f == reinterpret_cast<const void*>(&CVisibilityPlugins::RenderAtomic)) return 0x732B30;
    return (uint32_t)(uintptr_t)f;
}
static void* StubSetColor(void* light, const float* c) {
    uint32_t b[4]; std::memcpy(b, c, 16);
    Rec(1, (uint32_t)(uintptr_t)light, b[0], b[1], b[2], b[3]);
    return light;
}
static void* StubForAllAtomics(void* clump, void* cb, void* data) {   // records the call, then runs the callback on the clump's first `n` fixture atomics (n at clump+0x20)
    Rec(2, (uint32_t)(uintptr_t)clump, MapFn(cb), (uint32_t)(uintptr_t)data);
    using CB = void* (*)(void*, void*);
    const uint32_t n = *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(clump) + 0x20) % 3;
    for (uint32_t i = 0; i < n; ++i) reinterpret_cast<CB>(cb)(g_sc + kAtA + 0x100 * i, data);
    return clump;
}
static void InstallStubs() {
    static bool done = false; if (done) return; done = true;
    oracle::Patch(0x751A90, (void*)&StubSetColor);
    oracle::Patch(0x749B70, (void*)&StubForAllAtomics);
}

// ---- link lists ----
using AOI = CVisibilityPlugins::AlphaObjectInfo;
static CLinkList<AOI>* const kLists[5] = { &StaticRef<CLinkList<AOI>>(0xC88070), &StaticRef<CLinkList<AOI>>(0xC880C8), &StaticRef<CLinkList<AOI>>(0xC88120), &StaticRef<CLinkList<AOI>>(0xC88178), &StaticRef<CLinkList<AOI>>(0xC881D0) };
template<class T> static void BuildList(Rng& r, CLinkList<T>& L, uint8_t* nodes, int used, bool sortedDist) {
    auto* ln = reinterpret_cast<CLink<T>*>(nodes);
    int idx[kCap]; for (int i = 0; i < kCap; ++i) idx[i] = i;
    for (int i = kCap - 1; i > 0; --i) std::swap(idx[i], idx[r.below(i + 1)]);
    auto link = [](CLink<T>* h, CLink<T>* t, CLink<T>** seq, int n) {
        CLink<T>* prev = h;
        for (int i = 0; i < n; ++i) { prev->next = seq[i]; seq[i]->prev = prev; prev = seq[i]; }
        prev->next = t; t->prev = prev; h->prev = nullptr; t->next = nullptr;
    };
    CLink<T>* u[kCap]; CLink<T>* f[kCap];
    for (int i = 0; i < kCap; ++i) { if (i < used) u[i] = &ln[idx[i]]; else f[i - used] = &ln[idx[i]]; }
    link(&L.usedListHead, &L.usedListTail, u, used);
    link(&L.freeListHead, &L.freeListTail, f, kCap - used);
    L.links = ln;
    if constexpr (std::is_same_v<T, AOI>) {
        L.usedListHead.data.m_distance = 0.0f; L.usedListTail.data.m_distance = 100000000.0f;
        float d = 0.f;
        for (int i = 0; i < used; ++i) { if (sortedDist) { d += r.below(4) ? Fp(r, 60.f, 1000) : 0.f; u[i]->data.m_distance = d; } }
    }
}
static void BuildAllLists(Rng& r) {
    for (int i = 0; i < 5; ++i) BuildList(r, *kLists[i], S(kNodes + 0x100 * i), (int)r.below(kCap + 1), true);
    BuildList(r, StaticRef<CLinkList<CPed*>>(0xC88224), S(kNodes + 0x500), (int)r.below(kCap + 1), false);
}
// callbacks stored by the port are port function addresses: map to the exe ones, for every node of the pool
static void MapNodes() {
    for (int l = 0; l < 5; ++l) for (int i = 0; i < kCap; ++i) {
        auto* n = reinterpret_cast<CLink<AOI>*>(S(kNodes + 0x100 * l) + i * sizeof(CLink<AOI>));
        n->data.m_pCallback = reinterpret_cast<CVisibilityPlugins::RenderFunction>((uintptr_t)MapFn(reinterpret_cast<const void*>(n->data.m_pCallback)));
    }
}

static void BuildAll(Rng& r) {
    FillGarbage(r, g_sc, kScSize);
    SAt<uint32_t>(kRec) = 0;
    StaticRef<int>(0x8D608C) = 0x40 + 4 * (int)r.below(16);
    StaticRef<int>(0x8D6090) = 0x40 + 4 * (int)r.below(16);
    StaticRef<int>(0x8D6094) = 0x40 + 4 * (int)r.below(16);
    StaticRef<RwV3d*>(0xC88050) = reinterpret_cast<RwV3d*>(S(kV1));
    for (int k = 0; k < 3; ++k) SAt<float>(kV1 + 4 * k) = Fe(r, 800.f, 12);
    BuildAllLists(r);
}
static std::vector<std::pair<size_t, size_t>> g_unc;
template<class PF, class EF>
static bool VCase(Rng& r, std::string& d, PF&& pf, EF&& ef, void (*build)(Rng&) = &BuildAll) {
    const uint32_t seed = r.u32() | 1;
    build(r);
    Reg sc{ (uintptr_t)g_sc, kScSize };
    sc.mask.assign(kScSize, 0);
    for (auto [a, n] : g_unc) for (size_t b = 0; b < n; ++b) sc.mask[a + b] = 1;
    Reg img{ kImgLo, kImgHi - kImgLo };
    return BothA(d, seed, { sc, img }, std::forward<PF>(pf), std::forward<EF>(ef));
}
static int g_idx = 0;
} // namespace w3v

// port-side RW entry points -> the shared stubs
RpLight* RpLightSetColor(RpLight* light, const RwRGBAReal* c) { return static_cast<RpLight*>(w3v::StubSetColor(light, reinterpret_cast<const float*>(c))); }
RpClump* RpClumpForAllAtomics(RpClump* clump, RpAtomicCallBack cb, void* data) { return static_cast<RpClump*>(w3v::StubForAllAtomics(clump, reinterpret_cast<void*>(cb), data)); }

#define VROW(NAME, ADDR) Run(NAME " " #ADDR, [&](Rng& r, std::string& d) -> bool {
#define VEND(CASES) }, std::max(10, (CASES) / w3v_div()));
static int w3v_div() { static const int v = std::getenv("W3V_DIV") ? std::max(1, std::atoi(std::getenv("W3V_DIV"))) : 1; return v; }

static void TestW3VisPlugins() {
    using namespace w2; using namespace w3v;
    using CVP = CVisibilityPlugins;
    RpAtomic* const atA = reinterpret_cast<RpAtomic*>(S(kAtA)); RpAtomic* const atB = reinterpret_cast<RpAtomic*>(S(kAtB));
    RpClump* const clA = reinterpret_cast<RpClump*>(S(kClA)); RpClump* const clB = reinterpret_cast<RpClump*>(S(kClB));
    RwFrame* const frA = reinterpret_cast<RwFrame*>(S(kFrA)); RwFrame* const frB = reinterpret_cast<RwFrame*>(S(kFrB));
    // plugin constructors / destructors: (object, offset, size) cdecl
    VROW("CVisibilityPlugins::AtomicCopyConstructor", 0x732170)
        const int off = (int)r.below(0x80), sz = (int)r.below(32);
        return VCase(r, d, [&]() -> uint64_t { return U(STL(AtomicCopyConstructor)(atA, atB, off, sz)); }, [&]() -> uint64_t { return U(ExeC<void*>(0x732170, (void*)atA, (const void*)atB, off, sz)); });
    VEND(300)
    VROW("CVisibilityPlugins::AtomicDestructor", 0x7321A0)
        const int off = (int)r.below(0x80), sz = (int)r.below(32);
        return VCase(r, d, [&]() -> uint64_t { return U(STL(AtomicDestructor)(atA, off, sz)); }, [&]() -> uint64_t { return U(ExeC<void*>(0x7321A0, (void*)atA, off, sz)); });
    VEND(100)
    VROW("CVisibilityPlugins::ClumpConstructor", 0x732E10)
        const int off = (int)r.below(0x80), sz = (int)r.below(32);
        return VCase(r, d, [&]() -> uint64_t { void* p = STL(ClumpConstructor)(clA, off, sz); const size_t o = (size_t)StaticRef<int>(0x8D6094); SAt<uint32_t>(kClA + o) = MapFn(reinterpret_cast<void*>((uintptr_t)SAt<uint32_t>(kClA + o))); return U(p); },
                                [&]() -> uint64_t { return U(ExeC<void*>(0x732E10, (void*)clA, off, sz)); });
    VEND(200)
    VROW("CVisibilityPlugins::ClumpCopyConstructor", 0x732200)
        const int off = (int)r.below(0x80), sz = (int)r.below(32);
        return VCase(r, d, [&]() -> uint64_t { return U(STL(ClumpCopyConstructor)(clA, clB, off, sz)); }, [&]() -> uint64_t { return U(ExeC<void*>(0x732200, (void*)clA, (const void*)clB, off, sz)); });
    VEND(300)
    VROW("CVisibilityPlugins::ClumpDestructor", 0x732220)
        const int off = (int)r.below(0x80), sz = (int)r.below(32);
        return VCase(r, d, [&]() -> uint64_t { return U(STL(ClumpDestructor)(clA, off, sz)); }, [&]() -> uint64_t { return U(ExeC<void*>(0x732220, (void*)clA, off, sz)); });
    VEND(100)
    VROW("CVisibilityPlugins::FrameConstructor", 0x7321B0)
        const int off = (int)r.below(0x80), sz = (int)r.below(32);
        return VCase(r, d, [&]() -> uint64_t { return U(STL(FrameConstructor)(frA, off, sz)); }, [&]() -> uint64_t { return U(ExeC<void*>(0x7321B0, (void*)frA, off, sz)); });
    VEND(200)
    VROW("CVisibilityPlugins::FrameCopyConstructor", 0x7321D0)
        const int off = (int)r.below(0x80), sz = (int)r.below(32);
        return VCase(r, d, [&]() -> uint64_t { return U(STL(FrameCopyConstructor)(frA, frB, off, sz)); }, [&]() -> uint64_t { return U(ExeC<void*>(0x7321D0, (void*)frA, (const void*)frB, off, sz)); });
    VEND(300)
    VROW("CVisibilityPlugins::FrameDestructor", 0x7321F0)
        const int off = (int)r.below(0x80), sz = (int)r.below(32);
        return VCase(r, d, [&]() -> uint64_t { return U(STL(FrameDestructor)(frA, off, sz)); }, [&]() -> uint64_t { return U(ExeC<void*>(0x7321F0, (void*)frA, off, sz)); });
    VEND(100)
    VROW("CVisibilityPlugins::DefaultVisibilityCB", 0x732A30)
        return VCase(r, d, [&]() -> uint64_t { return (uint8_t)STL(DefaultVisibilityCB)(clA); }, [&]() -> uint64_t { return (uint8_t)ExeC<uint32_t>(0x732A30, (void*)clA); });
    VEND(100)

    // ---- atomic / frame / clump accessors (plugin at a random offset) ----
    VROW("CVisibilityPlugins::SetAtomicFlag", 0x7322B0)
        const uint32_t fl = r.below(4) ? (r.u32() & 0xFFFF) : r.u32();
        return VCase(r, d, [&]() -> uint64_t { CVP::SetAtomicFlag(atA, (uint16)fl); return 0; }, [&]() -> uint64_t { ExeC<void, RpAtomic*, uint32_t>(0x7322B0, atA, fl); return 0; });
    VEND(400)
    VROW("CVisibilityPlugins::ClearAtomicFlag", 0x732310)
        const uint32_t fl = r.below(4) ? (r.u32() & 0xFFFF) : r.u32();
        return VCase(r, d, [&]() -> uint64_t { CVP::ClearAtomicFlag(atA, (uint16)fl); return 0; }, [&]() -> uint64_t { ExeC<void, RpAtomic*, uint32_t>(0x732310, atA, fl); return 0; });
    VEND(400)
    VROW("CVisibilityPlugins::SetAtomicId", 0x732290)
        const uint32_t id = r.u32();
        return VCase(r, d, [&]() -> uint64_t { CVP::SetAtomicId(atA, (int32)id); return 0; }, [&]() -> uint64_t { ExeC<void, RpAtomic*, uint32_t>(0x732290, atA, id); return 0; });
    VEND(300)
    VROW("CVisibilityPlugins::SetUserValue", 0x732380)
        const uint32_t v = r.u32();
        return VCase(r, d, [&]() -> uint64_t { CVP::SetUserValue(atA, (uint16)v); return 0; }, [&]() -> uint64_t { ExeC<void, RpAtomic*, uint32_t>(0x732380, atA, v); return 0; });
    VEND(300)
    VROW("CVisibilityPlugins::SetModelInfoIndex", 0x732230)
        const uint32_t v = r.below(3) ? r.below(200) : r.u32();
        return VCase(r, d, [&]() -> uint64_t { CVP::SetModelInfoIndex(atA, (int32)v); return 0; }, [&]() -> uint64_t { ExeC<void, RpAtomic*, uint32_t>(0x732230, atA, v); return 0; });
    VEND(300)
    VROW("CVisibilityPlugins::GetAtomicId", 0x732370)
        return VCase(r, d, [&]() -> uint64_t { return (uint32_t)CVP::GetAtomicId(atA); }, [&]() -> uint64_t { return ExeC<uint32_t, RpAtomic*>(0x732370, atA); });
    VEND(300)
    VROW("CVisibilityPlugins::GetUserValue", 0x7323A0)
        return VCase(r, d, [&]() -> uint64_t { return (uint16_t)CVP::GetUserValue(atA); }, [&]() -> uint64_t { return (uint16_t)ExeC<uint32_t, RpAtomic*>(0x7323A0, atA); });
    VEND(300)
    VROW("CVisibilityPlugins::GetModelInfoIndex", 0x732250)
        return VCase(r, d, [&]() -> uint64_t { return (uint32_t)CVP::GetModelInfoIndex(atA); }, [&]() -> uint64_t { return ExeC<uint32_t, RpAtomic*>(0x732250, atA); });
    VEND(300)
    VROW("CVisibilityPlugins::GetModelInfo", 0x732260)
        // atomic's model id (int16 at plugin +0): -1 -> null, else the exe's ms_modelInfoPtrs entry (first 64 entries random pointers / nulls)
        g_idx = r.below(8) == 0 ? (int)r.below(0x7FFF) : (int)r.below(66) - 2;
        return VCase(r, d, [&]() -> uint64_t { return U(CVP::GetModelInfo(atA)); }, [&]() -> uint64_t { return U(ExeC<void*, RpAtomic*>(0x732260, atA)); },
            [](Rng& rr) { BuildAll(rr); SAt<int16_t>(kAtA + (size_t)StaticRef<int>(0x8D608C)) = (int16_t)g_idx; for (int i = 0; i < 64; ++i) StaticRef<uint32_t>(0xA9B0C8 + 4 * i) = rr.below(4) ? rr.u32() | 1 : 0; });
    VEND(600)
    VROW("CVisibilityPlugins::GetFrameHierarchyId", 0x732A20)
        return VCase(r, d, [&]() -> uint64_t { return (uint32_t)CVP::GetFrameHierarchyId(frA); }, [&]() -> uint64_t { return ExeC<uint32_t, RwFrame*>(0x732A20, frA); });
    VEND(200)
    VROW("CVisibilityPlugins::SetFrameHierarchyId", 0x732A00)
        const uint32_t v = r.u32();
        return VCase(r, d, [&]() -> uint64_t { CVP::SetFrameHierarchyId(frA, (int32)v); return 0; }, [&]() -> uint64_t { ExeC<void, RwFrame*, uint32_t>(0x732A00, frA, v); return 0; });
    VEND(200)
    VROW("CVisibilityPlugins::GetClumpAlpha", 0x732B20)
        return VCase(r, d, [&]() -> uint64_t { return (uint32_t)CVP::GetClumpAlpha(clA); }, [&]() -> uint64_t { return ExeC<uint32_t, RpClump*>(0x732B20, clA); });
    VEND(200)
    VROW("CVisibilityPlugins::SetClumpAlpha", 0x732B00)
        const uint32_t v = r.below(3) ? r.below(300) : r.u32();
        return VCase(r, d, [&]() -> uint64_t { CVP::SetClumpAlpha(clA, (int32)v); return 0; }, [&]() -> uint64_t { ExeC<void, RpClump*, uint32_t>(0x732B00, clA, v); return 0; });
    VEND(200)
    VROW("CVisibilityPlugins::GetClumpModelInfo", 0x732AC0)
        return VCase(r, d, [&]() -> uint64_t { return U(CVP::GetClumpModelInfo(clA)); }, [&]() -> uint64_t { return U(ExeC<void*, RpClump*>(0x732AC0, clA)); },
            [](Rng& rr) { BuildAll(rr); SAt<uint32_t>(kClA + 4) = rr.below(8) ? (uint32_t)(uintptr_t)S(kFrA) : (uint32_t)(uintptr_t)S(kFrB); });
    VEND(200)
    // atomic callbacks over the clump's atomics (RpClumpForAllAtomics is the recording stub; it runs the callback on 0..2 fixture atomics)
    VROW("CVisibilityPlugins::SetClumpForAllAtomicsFlag", 0x7322F0)
        const uint32_t id = r.below(3) ? (r.u32() & 0xFFFF) : r.u32();
        return VCase(r, d, [&]() -> uint64_t { CVP::SetClumpForAllAtomicsFlag(clA, (int32)id); return 0; }, [&]() -> uint64_t { ExeC<void, RpClump*, uint32_t>(0x7322F0, clA, id); return 0; });
    VEND(300)
    VROW("CVisibilityPlugins::ClearClumpForAllAtomicsFlag", 0x732350)
        const uint32_t id = r.below(3) ? (r.u32() & 0xFFFF) : r.u32();
        return VCase(r, d, [&]() -> uint64_t { CVP::ClearClumpForAllAtomicsFlag(clA, (int32)id); return 0; }, [&]() -> uint64_t { ExeC<void, RpClump*, uint32_t>(0x732350, clA, id); return 0; });
    VEND(300)
    VROW("setClumpForAllAtomicsFlagCB", 0x7322D0)
        const uint32_t id = r.below(3) ? (r.u32() & 0xFFFF) : r.u32();
        return VCase(r, d, [&]() -> uint64_t { return U(setClumpForAllAtomicsFlagCB(atA, (void*)(uintptr_t)id)); }, [&]() -> uint64_t { return U(ExeC<void*, RpAtomic*, uint32_t>(0x7322D0, atA, id)); });
    VEND(300)
    VROW("clearClumpForAllAtomicsFlagCB", 0x732330)
        const uint32_t id = r.below(3) ? (r.u32() & 0xFFFF) : r.u32();
        return VCase(r, d, [&]() -> uint64_t { return U(clearClumpForAllAtomicsFlagCB(atA, (void*)(uintptr_t)id)); }, [&]() -> uint64_t { return U(ExeC<void*, RpAtomic*, uint32_t>(0x732330, atA, id)); });
    VEND(300)
    VROW("SetAlphaCB", 0x7323B0)
        RpMaterial* const mat = reinterpret_cast<RpMaterial*>(S(kMat));
        const uint32_t v = r.u32();
        return VCase(r, d, [&]() -> uint64_t { return U(SetAlphaCB(mat, (void*)(uintptr_t)v)); }, [&]() -> uint64_t { return U(ExeC<void*, RpMaterial*, uint32_t>(0x7323B0, mat, v)); });
    VEND(300)

    // ---- camera distances / dot products ----
    VROW("CVisibilityPlugins::GetDistanceSquaredFromCamera(RwV3d*)", 0x732CC0)
        RwV3d* const p = reinterpret_cast<RwV3d*>(S(kV2));
        return VCase(r, d, [&]() -> uint64_t { return FB(static_cast<float (*)(RwV3d*)>(&CVP::GetDistanceSquaredFromCamera)(p)); },
                           [&]() -> uint64_t { return FB(ExeC<float, RwV3d*>(0x732CC0, p)); },
            [](Rng& rr) { BuildAll(rr); for (int k = 0; k < 3; ++k) SAt<float>(kV2 + 4 * k) = Fe(rr, 800.f, 12); });
    VEND(1500)
    VROW("CVisibilityPlugins::GetDotProductWithCameraVector", 0x7326D0)
        RwMatrix* const m = reinterpret_cast<RwMatrix*>(S(kM1)); RwMatrix* const cm = reinterpret_cast<RwMatrix*>(S(kM2));
        // component ids: mostly one to three flag bits, sometimes any 16 bits
        const uint32_t id = r.below(4) ? (1u << r.below(16)) | (r.below(2) ? 1u << r.below(16) : 0u) | (r.below(3) == 0 ? 1u << r.below(16) : 0u) : (r.u32() & 0xFFFF);
        return VCase(r, d, [&]() -> uint64_t { return FB(STL(GetDotProductWithCameraVector)(m, cm, id)); },
                           [&]() -> uint64_t { return FB(ExeC<float, RwMatrix*, RwMatrix*, uint32_t>(0x7326D0, m, cm, id)); },
            [](Rng& rr) {
                BuildAll(rr);
                for (size_t off : { kM1, kM2 }) for (int k = 0; k < 16; ++k) SAt<float>(off + 4 * k) = Fe(rr, (off == kM1 && k >= 12) ? 800.f : 1.f, 14);
            });
    VEND(4000)

    // ---- alpha lists ----
    VROW("CVisibilityPlugins::InitAlphaAtomicList", 0x734530)
        return VCase(r, d, [&]() -> uint64_t { CVP::InitAlphaAtomicList(); return 0; }, [&]() -> uint64_t { ExeC<void>(0x734530); return 0; });
    VEND(200)
    VROW("CVisibilityPlugins::InitAlphaEntityList", 0x734540)
        return VCase(r, d, [&]() -> uint64_t { CVP::InitAlphaEntityList(); return 0; }, [&]() -> uint64_t { ExeC<void>(0x734540); return 0; });
    VEND(200)
    VROW("CVisibilityPlugins::InsertObjectIntoSortedList", 0x733DD0)
        void* const obj = (void*)(uintptr_t)r.u32(); void* const fn = (void*)(uintptr_t)(r.u32() | 1); const float dist = Fe(r, 400.f, 8);
        return VCase(r, d, [&]() -> uint64_t { const bool b = CVP::InsertObjectIntoSortedList(obj, dist, reinterpret_cast<CVP::RenderFunction>(fn)); MapNodes(); return (uint8_t)b; },
                           [&]() -> uint64_t { const uint32_t b = ExeC<uint32_t, void*, float, void*>(0x733DD0, obj, dist, fn); MapNodes(); return (uint8_t)b; });
    VEND(1500)
    VROW("CVisibilityPlugins::InsertEntityIntoUnderwaterList", 0x733D90)
        CEntity* const e = reinterpret_cast<CEntity*>(S(kEnt)); const float dist = Fe(r, 400.f, 8);
        return VCase(r, d, [&]() -> uint64_t { const bool b = CVP::InsertEntityIntoUnderwaterList(e, dist); MapNodes(); return (uint8_t)b; },
                           [&]() -> uint64_t { const uint32_t b = ExeC<uint32_t, CEntity*, float>(0x733D90, e, dist); MapNodes(); return (uint8_t)b; });
    VEND(1500)
    VROW("CVisibilityPlugins::InsertAtomicIntoReallyDrawLastList", 0x733E10)
        const float dist = Fe(r, 400.f, 8);
        return VCase(r, d, [&]() -> uint64_t { const bool b = CVP::InsertAtomicIntoReallyDrawLastList(atA, dist); MapNodes(); return (uint8_t)b; },
                           [&]() -> uint64_t { const uint32_t b = ExeC<uint32_t, RpAtomic*, float>(0x733E10, atA, dist); MapNodes(); return (uint8_t)b; });
    VEND(1500)
    VROW("CVisibilityPlugins::InsertEntityIntoReallyDrawLastList", 0x733E50)
        CEntity* const e = reinterpret_cast<CEntity*>(S(kEnt)); const float dist = Fe(r, 400.f, 8);
        return VCase(r, d, [&]() -> uint64_t { const bool b = CVP::InsertEntityIntoReallyDrawLastList(e, dist); MapNodes(); return (uint8_t)b; },
                           [&]() -> uint64_t { const uint32_t b = ExeC<uint32_t, CEntity*, float>(0x733E50, e, dist); MapNodes(); return (uint8_t)b; });
    VEND(1500)
    VROW("CVisibilityPlugins::InsertEntityIntoSortedList", 0x734570)
        CEntity* const e = reinterpret_cast<CEntity*>(S(kEnt)); const float dist = Fe(r, 400.f, 8);
        return VCase(r, d, [&]() -> uint64_t { const bool b = CVP::InsertEntityIntoSortedList(e, dist); MapNodes(); return (uint8_t)b; },
                           [&]() -> uint64_t { const uint32_t b = ExeC<uint32_t, CEntity*, float>(0x734570, e, dist); MapNodes(); return (uint8_t)b; },
            [](Rng& rr) {   // the model index equals ModelIndices::MI_GRASSHOUSE in a third of the cases; m_bUnderwater is garbage (half)
                BuildAll(rr);
                CEntity* en = reinterpret_cast<CEntity*>(S(kEnt));
                StaticRef<uint32_t>(0x8CD6F4) = rr.below(3) ? 1 + rr.below(5000) : 65535u;
                en->m_nModelIndex = rr.below(3) == 0 ? (uint16)StaticRef<uint32_t>(0x8CD6F4) : (uint16)rr.u32();
            });
    VEND(2000)
    VROW("CVisibilityPlugins::AddWeaponPedForPC", 0x5E46D0)
        CPed* const ped = (CPed*)(uintptr_t)r.u32();
        return VCase(r, d, [&]() -> uint64_t { CVP::AddWeaponPedForPC(ped); return 0; }, [&]() -> uint64_t { ExeC<void, CPed*>(0x5E46D0, ped); return 0; });
    VEND(600)

    // ---- fading alpha (draw distance of a model / entity) ----
    VROW("CVisibilityPlugins::CalculateFadingAtomicAlpha", 0x732500)
        CBaseModelInfo* const mi = reinterpret_cast<CBaseModelInfo*>(S(kMi)); CEntity* const e = reinterpret_cast<CEntity*>(S(kEnt));
        const float dist = r.below(10) == 0 ? Fe(r, 800.f, 1) : std::fabs(Fg(r, 700.f));
        return VCase(r, d, [&]() -> uint64_t { return (uint32_t)STL(CalculateFadingAtomicAlpha)(mi, e, dist); }, [&]() -> uint64_t { return ExeC<uint32_t, CBaseModelInfo*, CEntity*, float>(0x732500, mi, e, dist); },
            [](Rng& rr) {
                BuildAll(rr);
                CBaseModelInfo* m = reinterpret_cast<CBaseModelInfo*>(S(kMi)); CEntity* en = reinterpret_cast<CEntity*>(S(kEnt));
                m->m_pColModel = reinterpret_cast<CColModel*>(S(kColM));
                m->m_pColModel->m_boundSphere.m_fRadius = Fp(rr, 120.f, 20);
                m->m_fDrawDistance = rr.below(3) ? 20.f + std::fabs(Fg(rr, 300.f)) : Fp(rr, 600.f, 10);
                m->m_nAlpha = rr.below(3) ? 255 : (uint8_t)rr.u32();
                en->*vis_steal_lod(T_LodPtr{}) = rr.below(2) ? reinterpret_cast<CEntity*>(S(kEnt2)) : nullptr;
                CRenderer::ms_fFarClipPlane = rr.below(8) ? 100.f + std::fabs(Fg(rr, 1500.f)) : Fe(rr, 1500.f, 1);
                CRenderer::ms_lowLodDistScale = rr.below(2) ? 1.0f : 0.25f + std::fabs(Fg(rr, 3.f));
                TheCamera.m_fLODDistMultiplier = rr.below(10) ? 0.4f + std::fabs(Fg(rr, 3.f)) : Fe(rr, 3.f, 1);
            });
    VEND(4000)

    // ---- SetRenderWareCamera: the camera's frame position pointer + the squared LOD distances ----
    VROW("CVisibilityPlugins::SetRenderWareCamera", 0x7328C0)
        RwCamera* const cam = reinterpret_cast<RwCamera*>(S(kCam)); void* const camE = S(kCamE);
        const uint32_t posP = (uint32_t)(uintptr_t)RwMatrixGetPos(RwFrameGetMatrix(reinterpret_cast<RwFrame*>(S(kFrame)))), posE = (uint32_t)(uintptr_t)(S(kFrameE) + 0x10 + 0x30);
        return VCase(r, d, [&]() -> uint64_t {
                    CVP::SetRenderWareCamera(cam);
                    const bool okC = StaticRef<RwCamera*>(0xC8804C) == cam, okP = (uint32_t)(uintptr_t)StaticRef<RwV3d*>(0xC88050) == posP;
                    StaticRef<RwCamera*>(0xC8804C) = reinterpret_cast<RwCamera*>((uintptr_t)(okC ? 1u : 0xBAD0u)); StaticRef<RwV3d*>(0xC88050) = reinterpret_cast<RwV3d*>((uintptr_t)(okP ? 2u : 0xBAD1u)); return 0; },
                     [&]() -> uint64_t {
                    ExeC<void, void*>(0x7328C0, camE);
                    const bool okC = (void*)StaticRef<RwCamera*>(0xC8804C) == camE, okP = (uint32_t)(uintptr_t)StaticRef<RwV3d*>(0xC88050) == posE;
                    StaticRef<RwCamera*>(0xC8804C) = reinterpret_cast<RwCamera*>((uintptr_t)(okC ? 1u : 0xBAD0u)); StaticRef<RwV3d*>(0xC88050) = reinterpret_cast<RwV3d*>((uintptr_t)(okP ? 2u : 0xBAD1u)); return 0; },
            [](Rng& rr) {
                BuildAll(rr);
                SAt<uint32_t>(kCam + 4) = (uint32_t)(uintptr_t)S(kFrame); SAt<uint32_t>(kCamE + 4) = (uint32_t)(uintptr_t)S(kFrameE);   // parent frame (+4 in both layouts)
                TheCamera.m_fLODDistMultiplier = rr.below(8) ? 0.4f + std::fabs(Fg(rr, 3.f)) : Fe(rr, 3.f, 1);
                TheCamera.m_fGenerationDistMultiplier = rr.below(8) ? 0.4f + std::fabs(Fg(rr, 3.f)) : Fe(rr, 3.f, 1);
            });
    VEND(3000)
}

// ---------------------------------------------------------------- app_light.cpp
namespace w3v {
static void BuildLights(Rng& r) {
    FillGarbage(r, g_sc, kScSize);
    SAt<uint32_t>(kRec) = 0;
    pAmbient = reinterpret_cast<RpLight*>(S(kLtA)); pDirect = reinterpret_cast<RpLight*>(S(kLtD));
    for (int i = 0; i < 6; ++i) pExtraDirectionals[i] = reinterpret_cast<RpLight*>(S(kLtE + 0x40 * i));
    auto col = [&](RwRGBAReal& c) { c.red = Fe(r, 1.2f, 12); c.green = Fe(r, 1.2f, 12); c.blue = Fe(r, 1.2f, 12); c.alpha = Fe(r, 1.2f, 12); };
    col(AmbientLightColour); col(AmbientLightColourForFrame); col(AmbientLightColourForFrame_PedsCarsAndObjects); col(DirectionalLightColour); col(DirectionalLightColourForFrame);
    col(DirectAmbientLight[0]); col(DirectAmbientLight[1]); col(CPostEffects::m_fInfraredVisionHeatObjectCol);
    for (float& f : LightStrengths) f = Fe(r, 2.f, 12);
    NumExtraDirectionalLights = r.below(7);
    CTimer::m_snTimeInMilliseconds = r.u32();
    CTimeCycle::m_BrightnessAddedToAmbientRed = Fe(r, 0.5f, 12);
    for (int k = 0; k < 4; ++k) SAt<float>(kCol + 4 * k) = Fe(r, 1.2f, 12);
    CClock::ms_nGameClockHours = r.below(10) ? r.below(24) : r.below(256);
    CClock::ms_nGameClockMinutes = r.below(10) ? r.below(60) : r.below(256);
    CClock::ms_nGameClockSeconds = r.below(10) ? r.below(60) : r.below(65536);
}
}

static void TestW3VisLights() {
    using namespace w2; using namespace w3v;
    RpWorld* const world = reinterpret_cast<RpWorld*>((uintptr_t)0x12345678);
    RwRGBAReal* const colArg = reinterpret_cast<RwRGBAReal*>(S(kCol));
    const auto L = [&](Rng& rr, std::string& d, auto&& pf, auto&& ef) { return VCase(rr, d, pf, ef, &BuildLights); };
    VROW("Light::ActivateDirectional", 0x735C80)
        return L(r, d, [&]() -> uint64_t { ActivateDirectional(); return 0; }, [&]() -> uint64_t { ExeC<void>(0x735C80); return 0; });
    VEND(100)
    VROW("Light::DeActivateDirectional", 0x735C70)
        return L(r, d, [&]() -> uint64_t { DeActivateDirectional(); return 0; }, [&]() -> uint64_t { ExeC<void>(0x735C70); return 0; });
    VEND(100)
    VROW("Light::LightsEnable", 0x735720)
        const uint32_t v = r.u32();
        return L(r, d, [&]() -> uint64_t { LightsEnable((int32)v); return 0; }, [&]() -> uint64_t { ExeC<void, uint32_t>(0x735720, v); return 0; });
    VEND(60)
    VROW("Light::WorldReplaceNormalLightsWithScorched", 0x7357E0)
        const float f = Fe(r, 1.2f, 8);
        return L(r, d, [&]() -> uint64_t { WorldReplaceNormalLightsWithScorched(world, f); return 0; }, [&]() -> uint64_t { ExeC<void, RpWorld*, float>(0x7357E0, world, f); return 0; });
    VEND(500)
    VROW("Light::WorldReplaceScorchedLightsWithNormal", 0x735820)
        return L(r, d, [&]() -> uint64_t { WorldReplaceScorchedLightsWithNormal(world); return 0; }, [&]() -> uint64_t { ExeC<void, RpWorld*>(0x735820, world); return 0; });
    VEND(200)
    VROW("Light::RemoveExtraDirectionalLights", 0x7359E0)
        return L(r, d, [&]() -> uint64_t { RemoveExtraDirectionalLights(world); return 0; }, [&]() -> uint64_t { ExeC<void, RpWorld*>(0x7359E0, world); return 0; });
    VEND(200)
    VROW("Light::SetAmbientAndDirectionalColours", 0x735A20)
        const float f = r.below(4) ? r.f01() : Fe(r, 2.f, 3);
        return L(r, d, [&]() -> uint64_t { SetAmbientAndDirectionalColours(f); return 0; }, [&]() -> uint64_t { ExeC<void, float>(0x735A20, f); return 0; });
    VEND(1000)
    VROW("Light::ReSetAmbientAndDirectionalColours", 0x735C40)
        return L(r, d, [&]() -> uint64_t { ReSetAmbientAndDirectionalColours(); return 0; }, [&]() -> uint64_t { ExeC<void>(0x735C40); return 0; });
    VEND(200)
    VROW("Light::SetFlashyColours", 0x735AB0)
        const float f = r.below(4) ? r.f01() : Fe(r, 2.f, 3);
        return L(r, d, [&]() -> uint64_t { SetFlashyColours(f); return 0; }, [&]() -> uint64_t { ExeC<void, float>(0x735AB0, f); return 0; });
    VEND(1000)
    VROW("Light::SetFlashyColours_Mild", 0x735B40)
        const float f = r.below(4) ? r.f01() : Fe(r, 2.f, 3);
        return L(r, d, [&]() -> uint64_t { SetFlashyColours_Mild(f); return 0; }, [&]() -> uint64_t { ExeC<void, float>(0x735B40, f); return 0; });
    VEND(1000)
    VROW("Light::SetBrightMarkerColours", 0x735BD0)
        const float f = Fe(r, 2.f, 3);
        return L(r, d, [&]() -> uint64_t { SetBrightMarkerColours(f); return 0; }, [&]() -> uint64_t { ExeC<void, float>(0x735BD0, f); return 0; });
    VEND(300)
    VROW("Light::SetDirectionalColours", 0x735D70)
        return L(r, d, [&]() -> uint64_t { SetDirectionalColours(colArg); return 0; }, [&]() -> uint64_t { ExeC<void, RwRGBAReal*>(0x735D70, colArg); return 0; });
    VEND(300)
    VROW("Light::SetAmbientColoursToIndicateRoadGroup", 0x735C90)
        const uint32_t idx = r.below(8);   // the six road groups (+ the two unused slots); the table index is `idx & 7`
        return L(r, d, [&]() -> uint64_t { SetAmbientColoursToIndicateRoadGroup((int32)idx); return 0; }, [&]() -> uint64_t { ExeC<void, uint32_t>(0x735C90, idx); return 0; });
    VEND(400)
    VROW("Light::SetFullAmbient", 0x735D10)
        return L(r, d, [&]() -> uint64_t { SetFullAmbient(); return 0; }, [&]() -> uint64_t { ExeC<void>(0x735D10); return 0; });
    VEND(100)
    VROW("Light::SetAmbientColours()", 0x735D30)
        return L(r, d, [&]() -> uint64_t { static_cast<void (*)()>(&SetAmbientColours)(); return 0; }, [&]() -> uint64_t { ExeC<void>(0x735D30); return 0; });
    VEND(200)
    VROW("Light::SetAmbientColours(RwRGBAReal*)", 0x735D50)
        return L(r, d, [&]() -> uint64_t { static_cast<void (*)(RwRGBAReal*)>(&SetAmbientColours)(colArg); return 0; }, [&]() -> uint64_t { ExeC<void, RwRGBAReal*>(0x735D50, colArg); return 0; });
    VEND(300)
    VROW("Light::SetLightColoursForPedsCarsAndObjects", 0x735D90)
        const float f = r.below(4) ? r.f01() : Fe(r, 2.f, 3);
        return L(r, d, [&]() -> uint64_t { SetLightColoursForPedsCarsAndObjects(f); return 0; }, [&]() -> uint64_t { ExeC<void, float>(0x735D90, f); return 0; });
    VEND(1500)
    VROW("Light::SetLightsForInfraredVisionHeatObjects", 0x735E40)
        return L(r, d, [&]() -> uint64_t { SetLightsForInfraredVisionHeatObjects(); return 0; }, [&]() -> uint64_t { ExeC<void>(0x735E40); return 0; });
    VEND(200)
    VROW("Light::StoreAndSetLightsForInfraredVisionHeatObjects", 0x735E70)
        return L(r, d, [&]() -> uint64_t { StoreAndSetLightsForInfraredVisionHeatObjects(); return 0; }, [&]() -> uint64_t { ExeC<void>(0x735E70); return 0; });
    VEND(300)
    VROW("Light::RestoreLightsForInfraredVisionHeatObjects", 0x735EF0)
        return L(r, d, [&]() -> uint64_t { RestoreLightsForInfraredVisionHeatObjects(); return 0; }, [&]() -> uint64_t { ExeC<void>(0x735EF0); return 0; });
    VEND(200)
    VROW("Light::SetLightsForInfraredVisionDefaultObjects", 0x735F20)
        return L(r, d, [&]() -> uint64_t { SetLightsForInfraredVisionDefaultObjects(); return 0; }, [&]() -> uint64_t { ExeC<void>(0x735F20); return 0; });
    VEND(100)
    VROW("Light::SetLightsForNightVision", 0x735F70)
        return L(r, d, [&]() -> uint64_t { SetLightsForNightVision(); return 0; }, [&]() -> uint64_t { ExeC<void>(0x735F70); return 0; });
    VEND(100)
    VROW("Light::GetDayNightBalance", 0x6FAB30)
        return L(r, d, [&]() -> uint64_t { return FB(GetDayNightBalance()); }, [&]() -> uint64_t { return FB(ExeC<float>(0x6FAB30)); });
    VEND(3000)
}

static void TestW3VisAll() {
    using namespace w3v;
    (void)spdlog::default_logger_raw();
    InstallStubs();
    TestW3VisPlugins();
    TestW3VisLights();
}

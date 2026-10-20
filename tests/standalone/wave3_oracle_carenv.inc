// ORACLE-W3-CARENV: exe-oracle rows for the A + B functions of game_sa/Pipelines/CustomCarEnvMap (car paint env-map / specular material plugin data, atomic env-map data, the three data pools).
// Included by wave2_oracle_all.inc when W3_CARENV_ONLY is defined (executable wave3_carenv_oracle_test = the W2 harness: deterministic arena, the whole exe data image + the fixture block g_sc compared after every case).
// Fixture block g_sc: two raw RpMaterial blobs and two RpAtomic blobs whose plugin slots (ms_envMapPluginOffset 0x8D12C4, ms_envMapAtmPluginOffset 0x8D12C8, ms_specularMapPluginOffset 0x8D12CC, all randomised) hold
// null / the fake data (0xC02D18) / a used slot of the matching pool; the three CPool headers (0xC02D28 / 0xC02D2C / 0xC02D30) over small slot arrays with random used / free split, ref counters and first-free index
// (full and empty pools included); four texture blobs (name, filterAddressing, refCount at the RenderWare offsets); four matfx data blobs (the exe reads *(matfxData + 4) at the offset in 0xC9AB74).
// RenderWare is not run: RpMatFXMaterialGetEffects (0x812140) is Patch()ed to a host stub and the port's fakerw entry point of the same name has the same body; RpMatFXMaterialGetEnvMapTexture (the port calls it where the exe
// inlines `*(*(mat + matfxOffset) + 4)`) is defined here with the exe's formula. The port keeps RpMaterial in the librw layout (surfaceProps at +8, so the flag word `specular` is +0xC, the exe's +0x10): CustomPipeMaterialSetup moves it.
#include "CustomCarEnvMapPipeline.h"

namespace w3c {
using namespace w2;
using CCP = CCustomCarEnvMapPipeline;
constexpr size_t kScSize = 0x1000;
alignas(16) static uint8_t g_sc[kScSize];
constexpr size_t kMatA = 0x000, kMatB = 0x100, kAtmA = 0x200, kAtmB = 0x300, kTex = 0x400 /*4 x 0x60*/, kMfx = 0x600 /*4 x 0x20*/, kPool = 0x680 /*3 x 0x20*/,
                 kEnvSt = 0x700 /*6 x 12*/, kAtmSt = 0x780 /*6 x 12*/, kSpecSt = 0x800 /*6 x 8*/, kEnvFl = 0x840, kAtmFl = 0x850, kSpecFl = 0x860, kDef = 0x880 /*0x20*/, kRnd = 0x8C0;
constexpr uint32_t kFake = 0xC02D18, kEnvPoolG = 0xC02D28, kAtmPoolG = 0xC02D2C, kSpecPoolG = 0xC02D30;
static uint8_t* S(size_t off) { return g_sc + off; }
template<class T> static T& SAt(size_t off) { return *reinterpret_cast<T*>(g_sc + off); }
static uint32_t P(size_t off) { return (uint32_t)(uintptr_t)(g_sc + off); }
static uint64_t U(const void* p) { return (uint64_t)(uintptr_t)p; }
static uint64_t FB(float f) { uint32_t b; std::memcpy(&b, &f, 4); return b; }
template<class R, class... A> static R ExeC(uint32_t addr, A... a) { return oracle::Fn<R __cdecl(A...)>(addr)(a...); }
static float Fe(Rng& r, float scale, int special = 10) {
    if (special && r.below((uint32_t)special) == 0) return GenF(r, scale, S_NAN | S_ZERO | S_DEN | S_TINY | S_INF | S_HUGE);
    return GenF(r, scale, S_ZERO);
}

// ---- RW entry points (exe patched, port defined below): same bodies ----
static int StubEffects(const void* mat) { return *reinterpret_cast<const int*>(reinterpret_cast<const uint8_t*>(mat) + 0x20); }
static void* StubEnvTex(const void* mat) {   // the exe's inline: data = *(mat + [0xC9AB74]); return *(data + 4)
    const uint32_t off = StaticRef<uint32_t>(0xC9AB74);
    uint8_t* data = *reinterpret_cast<uint8_t* const*>(reinterpret_cast<const uint8_t*>(mat) + off);
    return *reinterpret_cast<void**>(data + 4);
}
static void InstallStubs() {
    static bool done = false; if (done) return; done = true;
    oracle::Patch(0x812140, (void*)&StubEffects);
}

// ---- pools ----
static void MakePool(Rng& r, size_t hdr, size_t store, size_t flags, uint32_t cap, uint32_t global) {
    SAt<uint32_t>(hdr + 0) = P(store); SAt<uint32_t>(hdr + 4) = P(flags); SAt<uint32_t>(hdr + 8) = cap;
    SAt<int32_t>(hdr + 0xC) = (int32_t)r.below(cap + 1) - 1;
    SAt<uint8_t>(hdr + 0x10) = 0; SAt<uint8_t>(hdr + 0x11) = 0;
    const uint32_t mode = r.below(6);
    for (uint32_t i = 0; i < 8; ++i) {
        const bool empty = mode == 0 ? false : mode == 1 ? true : r.below(2) != 0;
        SAt<uint8_t>(flags + i) = (uint8_t)((r.below(128) & 0x7F) | (empty ? 0x80 : 0));
    }
    StaticRef<uint32_t>(global) = P(hdr);
}
// a pointer to a used slot of the pool (forces one if all are free)
static uint32_t UsedSlot(Rng& r, size_t store, size_t flags, uint32_t cap, uint32_t objSize) {
    uint32_t cand[8]; uint32_t n = 0;
    for (uint32_t i = 0; i < cap; ++i) if (!(SAt<uint8_t>(flags + i) & 0x80)) cand[n++] = i;
    if (!n) { const uint32_t i = r.below(cap); SAt<uint8_t>(flags + i) &= 0x7F; return P(store + i * objSize); }
    return P(store + cand[r.below(n)] * objSize);
}
static uint32_t TexPtr(Rng& r) { const uint32_t k = r.below(6); return k < 2 ? 0 : P(kTex + 0x60 * (k - 2)); }

static uint32_t g_envCap, g_atmCap, g_specCap;
static void PlugOffsets(Rng& r) {   // 5 distinct 4-aligned offsets: env, atm, spec, matfx, extra
    uint32_t o[12]; for (uint32_t i = 0; i < 12; ++i) o[i] = 0x28 + 4 * i;
    for (int i = 11; i > 0; --i) std::swap(o[i], o[r.below(i + 1)]);
    StaticRef<int32_t>(0x8D12C4) = (int32_t)o[0]; StaticRef<int32_t>(0x8D12C8) = (int32_t)o[1]; StaticRef<int32_t>(0x8D12CC) = (int32_t)o[2]; StaticRef<uint32_t>(0xC9AB74) = o[3];
}
// slot value for the env-mat plugin: null / fake / used pool slot
static uint32_t EnvVal(Rng& r) { const uint32_t k = r.below(5); return k == 0 ? 0 : k == 1 ? kFake : UsedSlot(r, kEnvSt, kEnvFl, g_envCap, 12); }
static uint32_t AtmVal(Rng& r) { return r.below(4) == 0 ? 0 : UsedSlot(r, kAtmSt, kAtmFl, g_atmCap, 12); }
static uint32_t SpecVal(Rng& r) { return r.below(4) == 0 ? 0 : UsedSlot(r, kSpecSt, kSpecFl, g_specCap, 8); }

static void BuildAll(Rng& r) {
    FillGarbage(r, g_sc, kScSize);
    PlugOffsets(r);
    g_envCap = 1 + r.below(6); g_atmCap = 1 + r.below(6); g_specCap = 1 + r.below(6);
    MakePool(r, kPool + 0x00, kEnvSt, kEnvFl, g_envCap, kEnvPoolG);
    MakePool(r, kPool + 0x20, kAtmSt, kAtmFl, g_atmCap, kAtmPoolG);
    MakePool(r, kPool + 0x40, kSpecSt, kSpecFl, g_specCap, kSpecPoolG);
    // textures: name[0] x / a / 0 / garbage, small ref counts, random filter / addressing
    for (int i = 0; i < 4; ++i) {
        uint8_t* t = S(kTex + 0x60 * i);
        const uint32_t k = r.below(4); t[0x10] = k == 0 ? 'x' : k == 1 ? 'a' : k == 2 ? 0 : t[0x10];
        *reinterpret_cast<int32_t*>(t + 0x54) = (int32_t)r.below(1000);
    }
    for (int i = 0; i < 4; ++i) SAt<uint32_t>(kMfx + 0x20 * i + 4) = TexPtr(r);
    // pool contents: textures / shininess / specularity of the slots
    for (uint32_t i = 0; i < 6; ++i) {
        uint8_t* e = S(kEnvSt + 12 * i);
        if (r.below(3) == 0) e[4] = 0;
        *reinterpret_cast<uint32_t*>(e + 8) = TexPtr(r);
        uint8_t* s = S(kSpecSt + 8 * i);
        *reinterpret_cast<float*>(s) = r.below(4) == 0 ? 0.f : Fe(r, 2.f, 8);
        *reinterpret_cast<uint32_t*>(s + 4) = TexPtr(r);
    }
    uint8_t* f = reinterpret_cast<uint8_t*>(kFake);   // the fake data lives at its exe address (data image)
    for (int i = 0; i < 12; ++i) f[i] = (uint8_t)r.u32();
    if (r.below(3) == 0) f[4] = 0;
    *reinterpret_cast<uint32_t*>(f + 8) = TexPtr(r);
    // materials / atomics: the plugin slots
    const uint32_t eo = StaticRef<uint32_t>(0x8D12C4), ao = StaticRef<uint32_t>(0x8D12C8), so = StaticRef<uint32_t>(0x8D12CC), mo = StaticRef<uint32_t>(0xC9AB74);
    for (size_t m : { kMatA, kMatB }) {
        SAt<uint32_t>(m + eo) = EnvVal(r); SAt<uint32_t>(m + so) = SpecVal(r);
        SAt<uint32_t>(m + mo) = P(kMfx + 0x20 * r.below(4));
        SAt<int32_t>(m + 0x20) = r.below(2) ? 2 : (int32_t)r.below(6);   // matfx effects (rpMATFXEFFECTENVMAP == 2)
    }
    for (size_t a : { kAtmA, kAtmB }) { for (uint32_t i = 0; i < 12; ++i) SAt<uint32_t>(a + 0x28 + 4 * i) = AtmVal(r); SAt<uint32_t>(a + ao) = AtmVal(r); }   // every candidate slot holds valid data (the env-map atomic callbacks take the offset as an argument)
}
static std::vector<std::pair<size_t, size_t>> g_unc;
template<class PF, class EF>
static bool CCase(Rng& r, std::string& d, PF&& pf, EF&& ef, void (*build)(Rng&) = &BuildAll) {
    const uint32_t seed = r.u32() | 1;
    build(r);
    Reg sc{ (uintptr_t)g_sc, kScSize };
    sc.mask.assign(kScSize, 0);
    for (auto [a, n] : g_unc) for (size_t b = 0; b < n; ++b) sc.mask[a + b] = 1;
    Reg img{ kImgLo, kImgHi - kImgLo };
    return BothA(d, seed, { sc, img }, std::forward<PF>(pf), std::forward<EF>(ef));
}
} // namespace w3c

// port-side RW entry points -> the shared stubs
RpMatFXMaterialFlags RpMatFXMaterialGetEffects(const RpMaterial* m) { return static_cast<RpMatFXMaterialFlags>(w3c::StubEffects(m)); }
RwTexture* RpMatFXMaterialGetEnvMapTexture(const RpMaterial* m) { return static_cast<RwTexture*>(w3c::StubEnvTex(m)); }

#define CROW(NAME, ADDR) Run(NAME " " #ADDR, [&](Rng& r, std::string& d) -> bool { w3c::g_unc.clear();
#define CEND(CASES) }, std::max(10, (CASES) / w3c_div()));
static int w3c_div() { static const int v = std::getenv("W3C_DIV") ? std::max(1, std::atoi(std::getenv("W3C_DIV"))) : 1; return v; }

static void TestW3CarEnvRows() {
    using namespace w2; using namespace w3c;
    RpMaterial* const mA = reinterpret_cast<RpMaterial*>(S(kMatA)); RpMaterial* const mB = reinterpret_cast<RpMaterial*>(S(kMatB));
    RpAtomic* const aA = reinterpret_cast<RpAtomic*>(S(kAtmA)); RpAtomic* const aB = reinterpret_cast<RpAtomic*>(S(kAtmB));
    const auto eoff = [] { return StaticRef<int32_t>(0x8D12C4); };
    const auto aoff = [] { return StaticRef<int32_t>(0x8D12C8); };
    // the plugin callbacks get (object, offset, size): the env-map atomic ones use the offset ARGUMENT in the exe; it is the registered offset in the game, but a random one here

    // ---- A: constructors / size / defaults ----
    CROW("CCustomCarEnvMapPipeline::pluginEnvMatConstructorCB", 0x5D8BD0)
        const int32_t off = (int32_t)(0x28 + 4 * r.below(12)), sz = (int32_t)r.below(32);
        return CCase(r, d, [&]() -> uint64_t { return U(CCP::pluginEnvMatConstructorCB(mA, off, sz)); }, [&]() -> uint64_t { return U(ExeC<void*>(0x5D8BD0, (void*)mA, off, sz)); });
    CEND(150)
    CROW("CCustomCarEnvMapPipeline::pluginEnvAtmConstructorCB", 0x5D8D30)
        const bool useReg = r.below(2) != 0; const int32_t alt = (int32_t)(0x28 + 4 * r.below(12)), sz = (int32_t)r.below(32);
        const auto off = [&] { return useReg ? aoff() : alt; };
        return CCase(r, d, [&]() -> uint64_t { return U(CCP::pluginEnvAtmConstructorCB(aA, off(), sz)); }, [&]() -> uint64_t { return U(ExeC<void*>(0x5D8D30, (void*)aA, off(), sz)); });
    CEND(200)
    CROW("CCustomCarEnvMapPipeline::pluginEnvMatStreamGetSizeCB", 0x5D8D10)
        const uint32_t k = r.below(3); const void* obj = k == 0 ? nullptr : k == 1 ? (const void*)mA : (const void*)(uintptr_t)(r.u32() | 1);
        const int32_t off = (int32_t)r.u32(), sz = (int32_t)r.u32();
        return CCase(r, d, [&]() -> uint64_t { return (uint32_t)CCP::pluginEnvMatStreamGetSizeCB(obj, off, sz); }, [&]() -> uint64_t { return (uint32_t)ExeC<int32_t>(0x5D8D10, obj, off, sz); });
    CEND(100)
    CROW("CCustomCarEnvMapPipeline::pluginSpecMatStreamGetSizeCB", 0x5D8DD0)
        if (r.below(2)) StaticRef<int32_t>(0x8D12CC) = -1; else StaticRef<int32_t>(0x8D12CC) = (int32_t)r.u32();
        const int32_t off = (int32_t)r.u32(), sz = (int32_t)r.u32();
        return CCase(r, d, [&]() -> uint64_t { return (uint32_t)CCP::pluginSpecMatStreamGetSizeCB(mA, off, sz); }, [&]() -> uint64_t { return (uint32_t)ExeC<int32_t>(0x5D8DD0, (const void*)mA, off, sz); },
                     [](Rng&) {});   // keep the global chosen above (no BuildAll)
    CEND(100)
    CROW("CCustomCarEnvMapPipeline::SetCustomEnvMapPipeMaterialDataDefaults", 0x5D8BB0)
        g_unc.assign(1, { kDef + 5, 1 });   // exe quirk: byte 5 is padding, the port's value-initialisation zeroes it
        return CCase(r, d, [&]() -> uint64_t { CCP::SetCustomEnvMapPipeMaterialDataDefaults(reinterpret_cast<CustomEnvMapPipeMaterialData*>(S(kDef))); return 0; },
                           [&]() -> uint64_t { ExeC<void>(0x5D8BB0, (void*)S(kDef)); return 0; });
    CEND(100)
    CROW("CCustomCarEnvMapPipeline::pluginSpecMatConstructorCB", 0x5D8D40)
        const int32_t off = (int32_t)r.u32(), sz = (int32_t)r.u32();
        return CCase(r, d, [&]() -> uint64_t { return U(CCP::pluginSpecMatConstructorCB(mA, off, sz)); }, [&]() -> uint64_t { return U(ExeC<void*>(0x5D8D40, (void*)mA, off, sz)); });
    CEND(150)

    // ---- getters (null / fake / pool slot; float results compared bit for bit) ----
    CROW("CCustomCarEnvMapPipeline::GetFxEnvScaleX", 0x5D6F90)
        return CCase(r, d, [&]() -> uint64_t { return FB(CCP::GetFxEnvScaleX(mA)); }, [&]() -> uint64_t { return FB(ExeC<float>(0x5D6F90, (void*)mA)); });
    CEND(400)
    CROW("CCustomCarEnvMapPipeline::GetFxEnvScaleY", 0x5D6FC0)
        return CCase(r, d, [&]() -> uint64_t { return FB(CCP::GetFxEnvScaleY(mA)); }, [&]() -> uint64_t { return FB(ExeC<float>(0x5D6FC0, (void*)mA)); });
    CEND(400)
    CROW("CCustomCarEnvMapPipeline::GetFxEnvTransSclX", 0x5D7040)
        return CCase(r, d, [&]() -> uint64_t { return FB(CCP::GetFxEnvTransSclX(mA)); }, [&]() -> uint64_t { return FB(ExeC<float>(0x5D7040, (void*)mA)); });
    CEND(400)
    CROW("CCustomCarEnvMapPipeline::GetFxEnvTransSclY", 0x5D7070)
        return CCase(r, d, [&]() -> uint64_t { return FB(CCP::GetFxEnvTransSclY(mA)); }, [&]() -> uint64_t { return FB(ExeC<float>(0x5D7070, (void*)mA)); });
    CEND(400)
    CROW("CCustomCarEnvMapPipeline::GetFxEnvShininess", 0x5D8AD0)
        return CCase(r, d, [&]() -> uint64_t { return FB(CCP::GetFxEnvShininess(mA)); }, [&]() -> uint64_t { return FB(ExeC<float>(0x5D8AD0, (void*)mA)); });
    CEND(400)
    CROW("CCustomCarEnvMapPipeline::GetFxSpecSpecularity", 0x5D8B90)
        return CCase(r, d, [&]() -> uint64_t { return FB(CCP::GetFxSpecSpecularity(mA)); }, [&]() -> uint64_t { return FB(ExeC<float>(0x5D8B90, (const void*)mA)); });
    CEND(400)
    CROW("CCustomCarEnvMapPipeline::GetFxSpecTexture", 0x5D8B50)
        return CCase(r, d, [&]() -> uint64_t { return U(CCP::GetFxSpecTexture(mA)); }, [&]() -> uint64_t { return U(ExeC<void*>(0x5D8B50, (const void*)mA)); });
    CEND(300)

    // ---- setters (unshare the fake data through the pool; game-range floats mixed with edge values) ----
    const auto FArg = [](Rng& r) -> float {
        const uint32_t k = r.below(12);
        if (k == 0) return GenF(r, 1.f, S_NAN | S_ZERO | S_DEN | S_TINY | S_INF | S_HUGE);
        if (k == 1) return (r.below(2) ? 1.f : -1.f) * (float)(1ull << (31 + r.below(30)));   // beyond int32, inside int64 (the exe's _ftol is 64 bit)
        if (k == 2) return (float)((int32_t)r.below(600) - 300) * 0.5f;                    // outside the byte range
        return GenF(r, 6.f, S_ZERO);
    };
    CROW("CCustomCarEnvMapPipeline::SetFxEnvScale", 0x5D6F40)
        const float x = FArg(r), y = FArg(r);
        return CCase(r, d, [&]() -> uint64_t { CCP::SetFxEnvScale(mA, x, y); return 0; }, [&]() -> uint64_t { ExeC<void>(0x5D6F40, (void*)mA, x, y); return 0; });
    CEND(600)
    CROW("CCustomCarEnvMapPipeline::SetFxEnvTransScl", 0x5D6FF0)
        const float x = FArg(r), y = FArg(r);
        return CCase(r, d, [&]() -> uint64_t { CCP::SetFxEnvTransScl(mA, x, y); return 0; }, [&]() -> uint64_t { ExeC<void>(0x5D6FF0, (void*)mA, x, y); return 0; });
    CEND(600)
    CROW("CCustomCarEnvMapPipeline::SetFxEnvShininess", 0x5D70A0)
        const float x = r.below(2) ? FArg(r) : (float)(r.below(1100)) * 0.001f;
        return CCase(r, d, [&]() -> uint64_t { CCP::SetFxEnvShininess(mA, x); return 0; }, [&]() -> uint64_t { ExeC<void>(0x5D70A0, (void*)mA, x); return 0; });
    CEND(600)

    // ---- SetFxEnvTexture / CustomPipeMaterialSetup ----
    CROW("CCustomCarEnvMapPipeline::SetFxEnvTexture", 0x5DA230)
        const uint32_t tex = r.below(3) == 0 ? 0 : TexPtr(r);
        return CCase(r, d, [&]() -> uint64_t { CCP::SetFxEnvTexture(mA, reinterpret_cast<RwTexture*>((uintptr_t)tex)); return 0; }, [&]() -> uint64_t { ExeC<void>(0x5DA230, (void*)mA, (void*)(uintptr_t)tex); return 0; });
    CEND(800)
    CROW("CCustomCarEnvMapPipeline::CustomPipeMaterialSetup", 0x5DA560)
        return CCase(r, d, [&]() -> uint64_t {
                         const uint32_t keep = SAt<uint32_t>(kMatA + 0xC);   // librw layout: the flag word (surfaceProps.specular) is at +0xC, the exe's at +0x10
                         auto* res = CCP::CustomPipeMaterialSetup(mA, nullptr);
                         const uint32_t v = SAt<uint32_t>(kMatA + 0xC); SAt<uint32_t>(kMatA + 0xC) = keep; SAt<uint32_t>(kMatA + 0x10) = v;
                         return U(res); },
                     [&]() -> uint64_t { return U(ExeC<void*>(0x5DA560, (void*)mA, (void*)nullptr)); });
    CEND(800)

    // ---- destructors / copy constructors / allocation (pool bookkeeping inline in the exe) ----
    CROW("CCustomCarEnvMapPipeline::pluginEnvMatDestructorCB", 0x5D95B0)
        const int32_t sz = (int32_t)r.below(32);
        return CCase(r, d, [&]() -> uint64_t { return U(CCP::pluginEnvMatDestructorCB(mA, eoff(), sz)); }, [&]() -> uint64_t { return U(ExeC<void*>(0x5D95B0, (void*)mA, eoff(), sz)); });
    CEND(500)
    CROW("CCustomCarEnvMapPipeline::pluginEnvMatCopyConstructorCB", 0x5D9600)
        const uint32_t k = r.below(4); const int32_t sz = k < 2 ? 4 : (int32_t)r.below(13);   // <= the 12 byte slot (a larger copy overlaps the neighbouring slot: rep movs vs memcpy), unreachable: the plugin size is 4
        return CCase(r, d, [&]() -> uint64_t { return U(CCP::pluginEnvMatCopyConstructorCB(mA, mB, eoff(), sz)); }, [&]() -> uint64_t { return U(ExeC<void*>(0x5D9600, (void*)mA, (const void*)mB, eoff(), sz)); });
    CEND(800)
    CROW("CCustomCarEnvMapPipeline::pluginEnvAtmDestructorCB", 0x5D9730)
        const bool useReg = r.below(2) != 0; const int32_t alt = (int32_t)(0x28 + 4 * r.below(12)), sz = (int32_t)r.below(32);
        const auto off = [&] { return useReg ? aoff() : alt; };
        return CCase(r, d, [&]() -> uint64_t { return U(CCP::pluginEnvAtmDestructorCB(aA, off(), sz)); }, [&]() -> uint64_t { return U(ExeC<void*>(0x5D9730, (void*)aA, off(), sz)); });
    CEND(500)
    CROW("CCustomCarEnvMapPipeline::pluginEnvAtmCopyConstructorCB", 0x5D9780)
        const uint32_t k = r.below(4); const int32_t sz = k < 2 ? 4 : (int32_t)r.below(13);   // <= the 12 byte slot: a larger copy overlaps the neighbouring slot (rep movs vs memcpy), unreachable (the plugin size is 4)
        const bool useReg = r.below(2) != 0; const int32_t alt = (int32_t)(0x28 + 4 * r.below(12));
        const auto off = [&] { return useReg ? aoff() : alt; };
        return CCase(r, d, [&]() -> uint64_t { return U(CCP::pluginEnvAtmCopyConstructorCB(aA, aB, off(), sz)); }, [&]() -> uint64_t { return U(ExeC<void*>(0x5D9780, (void*)aA, (const void*)aB, off(), sz)); });
    CEND(800)
    CROW("CCustomCarEnvMapPipeline::AllocEnvMapPipeAtomicData", 0x5D96F0)
        return CCase(r, d, [&]() -> uint64_t { return U(CCP::AllocEnvMapPipeAtomicData(aA)); }, [&]() -> uint64_t { return U(ExeC<void*>(0x5D96F0, (void*)aA)); });
    CEND(500)
    CROW("CCustomCarEnvMapPipeline::pluginSpecMatCopyConstructorCB", 0x5D9830)
        const int32_t off = (int32_t)r.u32(), sz = (int32_t)r.u32();
        return CCase(r, d, [&]() -> uint64_t { return U(CCP::pluginSpecMatCopyConstructorCB(mA, mB, off, sz)); }, [&]() -> uint64_t { return U(ExeC<void*>(0x5D9830, (void*)mA, (const void*)mB, off, sz)); });
    CEND(800)
}

static void TestW3CarEnvAll() {
    (void)spdlog::default_logger_raw();
    w3c::InstallStubs();
    TestW3CarEnvRows();
}

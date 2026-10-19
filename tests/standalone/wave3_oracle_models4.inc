// ORACLE-W3-MODELS part 4 (included by wave3_oracle_models.inc): callbacks that call into RenderWare. Both sides call the SAME recording stubs instead of RW:
//  * the exe's RW entry points are patched (oracle::Patch) to the host stubs (RpMatFXMaterialGetEffects 0x812140, RpMatFXMaterialSetEnvMapCoefficient 0x812680, RpAtomicSetFrame 0x74BF20,
//    RwTexDictionaryFindNamedTexture 0x7F39F0, RwTexDictionaryGetCurrent 0x7F3A90, RwTextureGetFindCallBack 0x7F34F0, RwTextureSetFindCallBack 0x7F34D0);
//  * the port's fakerw entry points of the same names (their real definitions, standalone/rw/*.cpp, are not linked) are defined here and call the same stubs;
//  * RpAtomicSetRenderCallBack: the exe's CVisibilityPlugins::SetAtomicRenderCallback (0x7328A0) runs for real and stores the callback at atomic+0x48; the port's RpAtomicSetRenderCallBack is
//    defined here to store the callback at the same place (port callback functions mapped to the exe addresses of the same functions, the default callback to 0x7491C0).
// What is compared is exactly what each callback does with its arguments (the order and values of the RW calls, the fixture changes) - not the RW implementation.
namespace w3m {
constexpr size_t kRec = 0x3600, kTexs = 0x3800 /*8 x 0x40*/, kDictA = 0x3A00, kDictB = 0x3A40, kCbVar = 0x3A80;
static void Rec(uint32_t kind, uint32_t a, uint32_t b) {
    uint32_t& n = At<uint32_t>(kRec);
    if (n < 8) { At<uint32_t>(kRec + 4 + n * 12) = kind; At<uint32_t>(kRec + 8 + n * 12) = a; At<uint32_t>(kRec + 12 + n * 12) = b; }
    ++n;
}
static uint32_t MapFn(const void* f) {
    if (f == reinterpret_cast<const void*>(&CVisibilityPlugins::RenderHeliRotorAlphaCB)) return 0x7340B0;
    if (f == reinterpret_cast<const void*>(&CVisibilityPlugins::RenderHeliTailRotorAlphaCB)) return 0x734170;
    if (f == reinterpret_cast<const void*>(&AtomicDefaultRenderCallBack)) return 0x7491C0;
    if (f == reinterpret_cast<const void*>(&CVehicleModelInfo::FindTextureCB)) return 0x4C7510;
    return (uint32_t)(uintptr_t)f;
}
static uint32_t StubGetEffects(const void* m) { return *reinterpret_cast<const uint32_t*>(reinterpret_cast<const uint8_t*>(m) + 0x30); }
static void* StubSetCoef(void* m, float c) { uint32_t b; std::memcpy(&b, &c, 4); Rec(2, (uint32_t)(uintptr_t)m, b); return m; }
static void* StubSetFrame(void* a, void* f) { Rec(4, (uint32_t)(uintptr_t)a, (uint32_t)(uintptr_t)f); return a; }
static void* StubFindNamed(void* dict, const char* name) {
    const int base = dict == P(kDictA) ? 0 : dict == P(kDictB) ? 4 : -1;
    if (base < 0) return nullptr;
    for (int i = 0; i < 4; ++i) {
        auto* t = P(kTexs + (base + i) * 0x40);
        if (!std::strcmp(reinterpret_cast<const char*>(t + 0x10), name)) return t;
    }
    return nullptr;
}
static void* StubGetCurrent() { return P(kDictB); }
static void* StubGetFindCb() { return At<void*>(kCbVar); }
static int   StubSetFindCb(void* cb) { At<void*>(kCbVar) = reinterpret_cast<void*>((uintptr_t)MapFn(cb)); return 1; }
static void InstallStubs() {
    static bool done = false;
    if (done) return;
    done = true;
    oracle::Patch(0x812140, (void*)&StubGetEffects); oracle::Patch(0x812680, (void*)&StubSetCoef); oracle::Patch(0x74BF20, (void*)&StubSetFrame);
    oracle::Patch(0x7F39F0, (void*)&StubFindNamed); oracle::Patch(0x7F3A90, (void*)&StubGetCurrent); oracle::Patch(0x7F34F0, (void*)&StubGetFindCb); oracle::Patch(0x7F34D0, (void*)&StubSetFindCb);
}
static void BuildRw(Rng& r) {
    BuildMi(r);
    StaticRef<void*>(0xB4E688) = P(kDictA);   // vehicleTxd
    At<uint32_t>(kRec) = 0;
    // materials: the fake "effects" word at +0x30 (0 none, 1 bump, 2 env map, 3 ..., 4 uv anim)
    At<uint32_t>(kMat + 0x30) = r.below(3) == 0 ? 2 : r.below(6);
    // textures: A0..A3 (vehicle txd), B0..B3 (current txd), names from a small pool so that the lookups hit and miss
    static const char* const pool[] = { "remapa", "#remapa", "plain", "other", "remap", "remapb", "carlight", "#remapb" };
    for (int i = 0; i < 8; ++i) { std::memset(P(kTexs + i * 0x40 + 0x10), 0, 32); std::strcpy(reinterpret_cast<char*>(P(kTexs + i * 0x40 + 0x10)), pool[r.below(8)]); }
    char* nm = reinterpret_cast<char*>(P(kStr)); std::memset(nm, 0, 0x40); std::strcpy(nm, pool[r.below(8)]);
    // the find-callback variable (the RW global) and the saved callback of the vehicle model info
    At<uint32_t>(kCbVar) = r.u32();
    StaticRef<uint32_t>(0xB4E6A0) = r.below(2) ? 0 : r.u32();
    // heli rotor atomic: frame with a node name
    StaticRef<int32_t>(0xC87C5C) = 0x20;
    At<void*>(kAt + 4) = P(kFrame);
    static const char* const heli[] = { "toprotor", "rearrotor", "toprotor_x", "rearrotorx", "other", "", "toprotor1", "rearroto" };
    std::memset(P(kFrame + 0x20), 0, 24);
    std::strcpy(reinterpret_cast<char*>(P(kFrame + 0x20)), heli[r.below(8)]);
}
} // namespace w3m

// port-side RW entry points (fakerw names) -> the shared stubs
RpMatFXMaterialFlags RpMatFXMaterialGetEffects(const RpMaterial* m) { return static_cast<RpMatFXMaterialFlags>(w3m::StubGetEffects(m)); }
RpMaterial* RpMatFXMaterialSetEnvMapCoefficient(RpMaterial* m, RwReal c) { return static_cast<RpMaterial*>(w3m::StubSetCoef(m, c)); }
RpAtomic* RpAtomicSetFrame(RpAtomic* a, RwFrame* f) { return static_cast<RpAtomic*>(w3m::StubSetFrame(a, f)); }
RwTexture* RwTexDictionaryFindNamedTexture(RwTexDictionary* d, const RwChar* n) { return static_cast<RwTexture*>(w3m::StubFindNamed(d, n)); }
RwTexDictionary* RwTexDictionaryGetCurrent(void) { return static_cast<RwTexDictionary*>(w3m::StubGetCurrent()); }
RwTextureCallBackFind RwTextureGetFindCallBack(void) { return reinterpret_cast<RwTextureCallBackFind>(w3m::StubGetFindCb()); }
RwBool RwTextureSetFindCallBack(RwTextureCallBackFind cb) { return w3m::StubSetFindCb(reinterpret_cast<void*>(cb)); }
RpAtomic* RpAtomicSetRenderCallBack(RpAtomic* a, RpAtomicCallBackRender cb) { *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(a) + 0x48) = w3m::MapFn(reinterpret_cast<const void*>(cb)); return a; }

static void TestW3ModelsPart4() {
    using namespace w2; using namespace w3m;
    InstallStubs();
    Run("CVehicleModelInfo::FindTextureCB 0x4C7510", [&](Rng& r, std::string& d) {
        const char* nm = reinterpret_cast<const char*>(P(kStr));
        return MCase(r, d, [&]() -> uint64_t { return U(CVehicleModelInfo::FindTextureCB(nm)); }, [&]() -> uint64_t { return ExeC<uint32_t>(0x4C7510, nm); }, &BuildRw);
    }, 600);
    Run("CVehicleModelInfo::UseCommonVehicleTexDicationary 0x4C75A0", [&](Rng& r, std::string& d) {
        return MCase(r, d, [&]() -> uint64_t { CVehicleModelInfo::UseCommonVehicleTexDicationary(); return 0; }, [&]() -> uint64_t { ExeC<void>(0x4C75A0); return 0; }, &BuildRw);
    }, 100);
    Run("CVehicleModelInfo::StopUsingCommonVehicleTexDicationary 0x4C75C0", [&](Rng& r, std::string& d) {
        return MCase(r, d, [&]() -> uint64_t { CVehicleModelInfo::StopUsingCommonVehicleTexDicationary(); return 0; }, [&]() -> uint64_t { ExeC<void>(0x4C75C0); return 0; }, &BuildRw);
    }, 100);
    Run("CVehicleModelInfo::MoveObjectsCB 0x4C7700", [&](Rng& r, std::string& d) {
        return MCase(r, d, [&]() -> uint64_t { return U(CVehicleModelInfo::MoveObjectsCB(reinterpret_cast<RwObject*>(P(kAt)), P(kFrame))); }, [&]() -> uint64_t { return ExeC<uint32_t>(0x4C7700, P(kAt), P(kFrame)); }, &BuildRw);
    }, 100);
    Run("CVehicleModelInfo::GetMatFXEffectMaterialCB 0x4C8810", [&](Rng& r, std::string& d) {
        return MCase(r, d, [&]() -> uint64_t { return U(CVehicleModelInfo::GetMatFXEffectMaterialCB(reinterpret_cast<RpMaterial*>(P(kMat)), P(kOut))); },
                           [&]() -> uint64_t { return ExeC<uint32_t>(0x4C8810, P(kMat), P(kOut)); }, &BuildRw);
    }, 300);
    Run("CVehicleModelInfo::SetEnvMapCoeffCB 0x4C88B0", [&](Rng& r, std::string& d) {
        void* coef = reinterpret_cast<void*>((uintptr_t)(r.below(6) == 0 ? r.u32() : r.below(3) == 0 ? r.below(1001) : r.below(100000)));
        return MCase(r, d, [&]() -> uint64_t { return U(CVehicleModelInfo::SetEnvMapCoeffCB(reinterpret_cast<RpMaterial*>(P(kMat)), coef)); },
                           [&]() -> uint64_t { return ExeC<uint32_t>(0x4C88B0, P(kMat), coef); }, &BuildRw);
    }, 800);
    Run("CVehicleModelInfo::SetAtomicRendererCB_Heli 0x4C7A30", [&](Rng& r, std::string& d) {
        return MCase(r, d, [&]() -> uint64_t { return U(CVehicleModelInfo::SetAtomicRendererCB_Heli(reinterpret_cast<RpAtomic*>(P(kAt)), nullptr)); },
                           [&]() -> uint64_t { return ExeC<uint32_t>(0x4C7A30, P(kAt), nullptr); }, &BuildRw);
    }, 300);
    Run("CClumpModelInfo::SetAtomicRendererCB 0x4C5280", [&](Rng& r, std::string& d) {
        void* cb = r.below(4) ? reinterpret_cast<void*>(0x10000u + (uintptr_t)r.below(0x1000) * 16) : nullptr;   // null -> the default callback (exe 0x7491C0)
        return MCase(r, d, [&]() -> uint64_t { return U(CClumpModelInfo::SetAtomicRendererCB(reinterpret_cast<RpAtomic*>(P(kAt)), cb)); },
                           [&]() -> uint64_t { return ExeC<uint32_t>(0x4C5280, P(kAt), cb); }, &BuildRw);
    }, 200);
}

// ORACLE-W3-MODELS part 3 (included by wave3_oracle_models.inc): txd refcounts (fake CTxdPool at the exe's 0xC8800C), editable materials (raw RW-layout material / texture fixtures),
// HideDamagedAtomicCB (node-name plugin frame). See wave3_oracle_models.inc for the fixture description.
namespace w3m {
constexpr size_t kTxdPool = 0x3200, kTxdFlags = 0x3240, kTxdStore = 0x3280, kTexA = 0x3000, kTexB = 0x3040, kTexL = 0x3080, kTexOn = 0x30C0, kFrame = 0x3300;
static void BuildTxd(Rng& r) {
    BuildMi(r);
    constexpr int N = 8;
    new (P(kTxdPool)) CTxdPool(N, P(kTxdStore), P(kTxdFlags));
    int used = 0;
    for (int i = 0; i < N; ++i) { const bool u = r.below(3) != 0; used += u; At<uint8_t>(kTxdFlags + i) = (uint8_t)(r.below(0x7F) | (u ? 0 : 0x80)); }
    if (!used) At<uint8_t>(kTxdFlags) &= 0x7F;
    int slot = r.below(N);
    while (At<uint8_t>(kTxdFlags + slot) & 0x80) slot = (slot + 1) % N;   // the exe dereferences a null for a free slot
    At<int16_t>(kMi + 0xA) = (int16_t)slot;
    StaticRef<void*>(0xC8800C) = P(kTxdPool);   // CTxdStore::ms_pTxdPool
}
static void BuildMaterial(Rng& r) {
    BuildMi(r);
    std::memcpy(P(kTexA + 0x10), "#remap_x", 9); std::memcpy(P(kTexB + 0x10), "plain", 6); std::memcpy(P(kTexL + 0x10), "vehiclelights128", 17); std::memcpy(P(kTexOn + 0x10), "vehiclelightson128", 19);
    StaticRef<void*>(0xB4E47C) = r.below(4) ? P(kTexOn) : nullptr;                              // ms_pRemapTexture
    StaticRef<void*>(0xB4E68C) = P(kTexL);                                                      // ms_pLightsTexture
    StaticRef<void*>(0xB4E690) = P(kTexOn);                                                     // ms_pLightsOnTexture
    for (int i = 0; i < 4; ++i) { StaticRef<uint8_t>(0xB4E3E8 + i) = 0; StaticRef<uint8_t>(0xB4E3F0 + i) = (uint8_t)r.below(128); }   // ms_lightsOn all off: the lights-on branch writes the material surface properties (librw layout: +8 in the port, +0xC in the exe - the standalone RW is librw), so it is not comparable; ms_currentCol: indices into the colour table
    FillGarbage(r, (uintptr_t)0xB4E480, 512);                                                   // ms_vehicleColourTable
    FillGarbage(r, (uintptr_t)0x8A645C, 12);                                                    // gLightSurfProps
    void* texs[] = { nullptr, P(kTexA), P(kTexB), P(kTexL), P(kTexL), P(kTexL) };
    At<void*>(kMat) = texs[r.below(6)];
    static const uint8_t cols[4][3] = { { 255, 175, 0 }, { 0, 255, 200 }, { 185, 255, 0 }, { 255, 60, 0 } };
    if (r.below(2)) { const int k = r.below(4); for (int i = 0; i < 3; ++i) At<uint8_t>(kMat + 4 + i) = cols[k][i]; }
    At<void*>(kOut) = P(kEntries);   // *ppEntries: the restore list
}
static void BuildHideDamaged(Rng& r) {
    BuildMi(r);
    StaticRef<int32_t>(0x8D608C) = 0x50;      // CVisibilityPlugins::ms_atomicPluginOffset
    StaticRef<int32_t>(0xC87C5C) = 0x20;      // node-name plugin offset (RwFrame + 0x20: the name)
    At<void*>(kAt + 4) = P(kFrame);           // the atomic's frame (object.parent)
    static const char* const names[] = { "door_dam", "door_ok", "bonnet_dam_x", "xx", "_ok", "", "a_ok_dam", "dam", "chassis_vlo" };
    std::memset(P(kFrame + 0x20), 0, 24);
    std::strcpy(reinterpret_cast<char*>(P(kFrame + 0x20)), names[r.below(9)]);
}
} // namespace w3m

static void TestW3ModelsPart3() {
    using namespace w2; using namespace w3m;
    auto BM = [] { return reinterpret_cast<CBaseModelInfo*>(P(kMi)); };
    Run("CBaseModelInfo::AddTexDictionaryRef 0x4C4B80", [&](Rng& r, std::string& d) {
        return MCase(r, d, [&]() -> uint64_t { BM()->AddTexDictionaryRef(); return 0; }, [&]() -> uint64_t { ExeT<void>(0x4C4B80, P(kMi)); return 0; }, &BuildTxd);
    }, 200);
    Run("CBaseModelInfo::AddRef 0x4C4BA0", [&](Rng& r, std::string& d) {
        return MCase(r, d, [&]() -> uint64_t { BM()->AddRef(); return 0; }, [&]() -> uint64_t { ExeT<void>(0x4C4BA0, P(kMi)); return 0; }, &BuildTxd);
    }, 200);
    Run("CVehicleModelInfo::SetEditableMaterialsCB-RpMaterial 0x4C8220", [&](Rng& r, std::string& d) {
        return MCase(r, d, [&]() -> uint64_t { return U(CVehicleModelInfo::SetEditableMaterialsCB(reinterpret_cast<RpMaterial*>(P(kMat)), P(kOut))); },
                           [&]() -> uint64_t { return ExeC<uint32_t>(0x4C8220, P(kMat), P(kOut)); }, &BuildMaterial);
    }, 800);
    Run("CVehicleModelInfo::HideDamagedAtomicCB 0x4C7720", [&](Rng& r, std::string& d) {
        return MCase(r, d, [&]() -> uint64_t { return U(CVehicleModelInfo::HideDamagedAtomicCB(reinterpret_cast<RpAtomic*>(P(kAt)), nullptr)); },
                           [&]() -> uint64_t { return ExeC<uint32_t>(0x4C7720, P(kAt), nullptr); }, &BuildHideDamaged);
    }, 300);
}

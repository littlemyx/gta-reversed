// ORACLE-C-MODELS-2 part 6 (included by wave3_oracle_models.inc): frame walkers of CClumpModelInfo, SetFrameIds, SetHierarchyForSkinAtomic, CreateInstance(mat), atomic set-up callbacks, pipeline predicates.
// RenderWare is not run: the exe's RwFrameForAllChildren (0x7F0DC0), RpHAnimFrameGetHierarchy (0x7C5160) and RpSkinAtomicSetHAnimHierarchy (0x7C7520) are Patch()ed to host stubs and the port's fakerw
// entry points of the same names (their real definitions are not linked here) have the same bodies. The frames are hand-built (child/next at fixed offsets read only by the stub); the hierarchy-id
// (0x8D6090) and node-name (0xC87C5C) plugin offsets are the exe's own globals, so GetFrameHierarchyId / GetFrameNodeName / SetFrameHierarchyId run for real on both sides.
// The port's atomic / geometry are librw structs, the exe's the RW 3.6 ones: every field a row reads is written at BOTH offsets (offsetof of the port type and the exe offset).
namespace w3m {
constexpr size_t kFr = 0x8000, kFrStride = 0xB0, kNumFr = 12, kClump6 = 0x8C00, kSrch = 0x8C80, kAssoc = 0x8D00, kArr = 0x8F00, kRec6 = 0x8F80, kName6 = 0x9300, kGeo = 0x9400, kGeo2 = 0x9500,
                 kPlg = 0x9600, kFakeVt = 0x9700, kAt2 = 0x9800, kAssocStr = 0x9900, kInst = 0x9A00;
constexpr size_t kFrChild = 0x98, kFrNext = 0x9C, kFrAnim = 0x90;
static int g_nFr = 1;
static uint8_t* Fr(size_t i) { return P(kFr + i * kFrStride); }
static void Rec6(uint32_t a, uint32_t b) { uint32_t& n = At<uint32_t>(kRec6); if (n < 6) { At<uint32_t>(kRec6 + 4 + n * 8) = a; At<uint32_t>(kRec6 + 8 + n * 8) = b; } ++n; }
static void* StubForAllChildren(uint8_t* frame, void* (*cb)(void*, void*), void* data) {
    uint8_t* cur = *reinterpret_cast<uint8_t**>(frame + kFrChild);
    while (cur) { uint8_t* nx = *reinterpret_cast<uint8_t**>(cur + kFrNext); if (!cb(cur, data)) break; cur = nx; }
    return frame;
}
static void* StubAnimGet(uint8_t* frame) { return *reinterpret_cast<void**>(frame + kFrAnim); }
static void* StubSkinSet(void* atomic, void* hier) { Rec6((uint32_t)(uintptr_t)atomic, (uint32_t)(uintptr_t)hier); return atomic; }
// the exe's _stricmp (0x8229B6): the CRT locale / lock plumbing is not initialised in this harness, so the exe side gets the C-locale body
static int __cdecl StubStricmp(const char* a, const char* b) {
    for (;; ++a, ++b) { const int x = (unsigned char)(*a >= 'A' && *a <= 'Z' ? *a + 32 : *a), y = (unsigned char)(*b >= 'A' && *b <= 'Z' ? *b + 32 : *b); if (x != y) return x < y ? -1 : 1; if (!x) return 0; }
}
static void InstallStubs6() {
    static bool done = false;
    if (done) return;
    done = true;
    oracle::Patch(0x7F0DC0, (void*)&StubForAllChildren); oracle::Patch(0x7C5160, (void*)&StubAnimGet); oracle::Patch(0x7C7520, (void*)&StubSkinSet); oracle::Patch(0x8229B6, (void*)&StubStricmp);
}
static bool g_idSafe = false;   // FillFrameArray writes frames[id]: ids stay inside the 16-entry array
static void BuildFrames(Rng& r) {
    BuildMi(r);
    StaticRef<int32_t>(0x8D6090) = 0x40 + 4 * (int)r.below(16);
    StaticRef<int32_t>(0xC87C5C) = 0x20;
    static const char* const names[] = { "chassis", "door_lf_dummy", "door_rf_dummy", "wheel_lf_dummy", "bonnet_dummy", "boot_dummy", "", "chassis", "Chassis" };
    g_nFr = 1 + (int)r.below(kNumFr);
    for (size_t i = 0; i < kNumFr; ++i) {
        uint8_t* f = Fr(i);
        *reinterpret_cast<void**>(f + kFrChild) = nullptr; *reinterpret_cast<void**>(f + kFrNext) = nullptr;
        std::memset(f + 0x20, 0, 24); std::strcpy(reinterpret_cast<char*>(f + 0x20), names[r.below(9)]);
        int32_t id;
        switch (r.below(8)) { case 0: case 1: id = 0; break; case 2: id = -1 - (int32_t)r.below(3); break; case 3: id = g_idSafe ? 15 : (int32_t)r.u32(); break; default: id = (int32_t)r.below(g_idSafe ? 16 : 7); }
        *reinterpret_cast<int32_t*>(f + StaticRef<int32_t>(0x8D6090)) = id;
        *reinterpret_cast<void**>(f + kFrAnim) = r.below(3) == 0 ? nullptr : (void*)(uintptr_t)(0x1000u + 16 * r.below(64));
    }
    for (int i = 1; i < g_nFr; ++i) {   // frame i becomes the last child of a random earlier frame
        uint8_t* p = Fr(r.below(i));
        void** slot = reinterpret_cast<void**>(p + kFrChild);
        while (*slot) slot = reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(*slot) + kFrNext);
        *slot = Fr(i);
    }
    At<void*>(kClump6 + 4) = Fr(0);
    At<uint32_t>(kRec6) = 0;
    At<void*>(kMi + 0x1C) = P(kClump6);
    // the search structure {name or id, result frame = null}; the name is one of the pool (hits and misses)
    std::memset(P(kName6), 0, 0x40); std::strcpy(reinterpret_cast<char*>(P(kName6)), names[r.below(9)]);
    At<void*>(kSrch) = P(kName6); At<void*>(kSrch + 4) = nullptr;
    static const int32_t ids[] = { 0, 1, 2, 3, 5, 7, 15, -1, 0x7FFFFFFF };
    At<int32_t>(kSrch + 0x10) = ids[r.below(9)]; At<void*>(kSrch + 0x14) = nullptr;   // the by-id search structure
    for (int i = 0; i < 16; ++i) At<void*>(kArr + i * 4) = nullptr;
}
static void* __fastcall FakeCreateInstance(void*) { return P(kClump6); }
} // namespace w3m
RwFrame* RwFrameForAllChildren(RwFrame* f, RwFrameCallBack cb, void* d) { return static_cast<RwFrame*>(w3m::StubForAllChildren(reinterpret_cast<uint8_t*>(f), reinterpret_cast<void* (*)(void*, void*)>(cb), d)); }
RpHAnimHierarchy* RpHAnimFrameGetHierarchy(RwFrame* f) { return static_cast<RpHAnimHierarchy*>(w3m::StubAnimGet(reinterpret_cast<uint8_t*>(f))); }
RpAtomic* RpSkinAtomicSetHAnimHierarchy(RpAtomic* a, RpHAnimHierarchy* h) { return static_cast<RpAtomic*>(w3m::StubSkinSet(a, h)); }

namespace w3m {
// ---- atomic / geometry fixtures for the set-up callbacks: pipeline id (atomic plugin 0x8D6080), colours (exe geometry+0x30 / librw colors), night-colour pointer (geometry plugin 0x8D12BC), 2dfx plugin (0xC3A1E0)
static bool g_noPipe = true;   // true: never the pipeline set-up branches (they run RenderWare)
static void LayerAtomic(Rng& r, size_t atOff, size_t geoOff) {
    static const uint32_t pipes[] = { 0x53F2009C, 0x53F20098, 0x53F2009A, 0, 0x53F2009B, 0xFFFFFFFF, 0x12345678 };
    uint32_t pid = r.below(3) ? pipes[r.below(7)] : r.u32();
    bool colors = r.below(2), night = r.below(2);
    if (g_noPipe) { while (pid == 0x53F2009C || pid == 0x53F20098 || pid == 0x53F2009A) pid = r.u32() | 1; if (colors && night) night = false; }
    At<uint32_t>(atOff + StaticRef<int32_t>(0x8D6080)) = pid;
    uint8_t* g = P(geoOff);
    At<void*>(atOff + 0x18) = g; *reinterpret_cast<void**>(P(atOff) + offsetof(RpAtomic, geometry)) = g;
    void* col = colors ? (void*)(uintptr_t)(0x2000 + 16 * r.below(64)) : nullptr;
    At<void*>(geoOff + 0x30) = col; *reinterpret_cast<void**>(g + offsetof(RpGeometry, colors)) = col;
    *reinterpret_cast<void**>(g + StaticRef<uint32_t>(0x8D12BC)) = night ? (void*)(uintptr_t)(0x3000 + 16 * r.below(64)) : nullptr;
    *reinterpret_cast<void**>(g + StaticRef<uint32_t>(0xC3A1E0)) = r.below(4) ? P(kPlg) : nullptr;   // 2dfx plugin: pointer to {count, ...} or null
}
static void LayerAtomics(Rng& r) {
    StaticRef<int32_t>(0x8D6080) = 0x40 + 4 * (int)r.below(16);
    StaticRef<uint32_t>(0x8D12BC) = 0x80 + 4 * r.below(16);
    StaticRef<uint32_t>(0xC3A1E0) = 0xC0 + 4 * r.below(16);
    At<uint32_t>(kPlg) = r.below(4) ? r.below(6) : r.u32();
    LayerAtomic(r, kAt, kGeo);
    LayerAtomic(r, kAt2, kGeo2);
}
static void BuildAtomicGeo(Rng& r) { BuildMi(r); LayerAtomics(r); }
static void BuildSetAtomic(Rng& r) {
    BuildTxd(r);   // fake txd pool + the info's txd slot (info +0xA)
    LayerAtomics(r);
    uint16_t fl = (uint16_t)(r.u32() & 0x7FFF);
    if ((fl & 0x7800) == 0x3000) fl ^= 0x0800;   // not a tag model (CTagManager::SetupAtomic runs RenderWare)
    if (r.below(8) == 0) fl |= 0x8000;
    At<uint16_t>(kMi + 0x12) = fl;
    At<void*>(kMi + 0x1C) = r.below(2) ? nullptr : P(kAt2);   // an atomic already set: its 2dfx count is taken off first
}
} // namespace w3m

static void TestW3ModelsPart6() {
    using namespace w2; using namespace w3m;
    InstallStubs6();
    auto CM = [] { return reinterpret_cast<CClumpModelInfo*>(P(kMi)); };
    // ---- frame callbacks: (frame, search struct)
    Run("CClumpModelInfo::FindFrameFromNameCB 0x4C52A0", [&](Rng& r, std::string& d) {
        g_idSafe = false; const size_t fi = r.below(kNumFr);
        return MCase(r, d, [&]() -> uint64_t { uint8_t* f = Fr(fi % g_nFr); return U(CClumpModelInfo::FindFrameFromNameCB(reinterpret_cast<RwFrame*>(f), P(kSrch))); },
                           [&]() -> uint64_t { uint8_t* f = Fr(fi % g_nFr); return ExeC<uint32_t>(0x4C52A0, f, P(kSrch)); }, &BuildFrames);
    }, 400);
    Run("CClumpModelInfo::FindFrameFromNameWithoutIdCB 0x4C52F0", [&](Rng& r, std::string& d) {
        g_idSafe = false; const size_t fi = r.below(kNumFr);
        return MCase(r, d, [&]() -> uint64_t { uint8_t* f = Fr(fi % g_nFr); return U(CClumpModelInfo::FindFrameFromNameWithoutIdCB(reinterpret_cast<RwFrame*>(f), P(kSrch))); },
                           [&]() -> uint64_t { uint8_t* f = Fr(fi % g_nFr); return ExeC<uint32_t>(0x4C52F0, f, P(kSrch)); }, &BuildFrames);
    }, 400);
    Run("CClumpModelInfo::FindFrameFromIdCB 0x4C5350", [&](Rng& r, std::string& d) {
        g_idSafe = false; const size_t fi = r.below(kNumFr);
        return MCase(r, d, [&]() -> uint64_t { uint8_t* f = Fr(fi % g_nFr); return U(CClumpModelInfo::FindFrameFromIdCB(reinterpret_cast<RwFrame*>(f), P(kSrch + 0x10))); },
                           [&]() -> uint64_t { uint8_t* f = Fr(fi % g_nFr); return ExeC<uint32_t>(0x4C5350, f, P(kSrch + 0x10)); }, &BuildFrames);
    }, 400);
    Run("CClumpModelInfo::FillFrameArrayCB 0x4C5390", [&](Rng& r, std::string& d) {
        g_idSafe = true; const size_t fi = r.below(kNumFr);
        return MCase(r, d, [&]() -> uint64_t { uint8_t* f = Fr(fi % g_nFr); return U(CClumpModelInfo::FillFrameArrayCB(reinterpret_cast<RwFrame*>(f), P(kArr))); },
                           [&]() -> uint64_t { uint8_t* f = Fr(fi % g_nFr); return ExeC<uint32_t>(0x4C5390, f, P(kArr)); }, &BuildFrames);
    }, 400);
    // ---- clump level
    Run("CClumpModelInfo::GetFrameFromId 0x4C53C0", [&](Rng& r, std::string& d) {
        g_idSafe = false; const int32_t id = (int32_t)(r.below(3) ? r.below(7) : r.below(2) ? -1 : (int32_t)r.u32());
        return MCase(r, d, [&]() -> uint64_t { return U(CClumpModelInfo::GetFrameFromId(reinterpret_cast<RpClump*>(P(kClump6)), id)); },
                           [&]() -> uint64_t { return ExeC<uint32_t>(0x4C53C0, P(kClump6), id); }, &BuildFrames);
    }, 400);
    Run("CClumpModelInfo::GetFrameFromName 0x4C5400", [&](Rng& r, std::string& d) {
        g_idSafe = false;
        return MCase(r, d, [&]() -> uint64_t { return U(CClumpModelInfo::GetFrameFromName(reinterpret_cast<RpClump*>(P(kClump6)), reinterpret_cast<const char*>(P(kName6)))); },
                           [&]() -> uint64_t { return ExeC<uint32_t>(0x4C5400, P(kClump6), P(kName6)); }, &BuildFrames);
    }, 400);
    Run("CClumpModelInfo::FillFrameArray 0x4C5440", [&](Rng& r, std::string& d) {
        g_idSafe = true;
        return MCase(r, d, [&]() -> uint64_t { CClumpModelInfo::FillFrameArray(reinterpret_cast<RpClump*>(P(kClump6)), reinterpret_cast<RwFrame**>(P(kArr))); return 0; },
                           [&]() -> uint64_t { ExeC<void>(0x4C5440, P(kClump6), P(kArr)); return 0; }, &BuildFrames);
    }, 400);
    // ---- SetFrameIds: a table of {name, hierarchy id, flags} terminated by a null name
    Run("CClumpModelInfo::SetFrameIds 0x4C5460", [&](Rng& r, std::string& d) {
        g_idSafe = false;
        return MCase(r, d, [&]() -> uint64_t { CM()->SetFrameIds(reinterpret_cast<RwObjectNameIdAssocation*>(P(kAssoc))); return 0; },
                           [&]() -> uint64_t { ExeT<void>(0x4C5460, P(kMi), P(kAssoc)); return 0; },
                           [](Rng& rr) {
                               BuildFrames(rr);
                               static const char* const pool[] = { "chassis", "door_lf_dummy", "door_rf_dummy", "wheel_lf_dummy", "bonnet_dummy", "boot_dummy", "", "nothere", "Chassis" };
                               const int n = (int)rr.below(6);
                               for (int i = 0; i < 6; ++i) {
                                   char* s = reinterpret_cast<char*>(P(kAssocStr + i * 0x20)); std::memset(s, 0, 0x20); std::strcpy(s, pool[rr.below(9)]);
                                   At<void*>(kAssoc + i * 12) = i < n ? (void*)s : nullptr;
                                   At<int32_t>(kAssoc + i * 12 + 4) = rr.below(4) ? (int32_t)rr.below(20) : (int32_t)rr.u32();
                                   At<uint32_t>(kAssoc + i * 12 + 8) = rr.below(3) == 0 ? 1u : rr.below(3) == 0 ? rr.u32() : 0u;
                               }
                           });
    }, 500);
    // ---- SetHierarchyForSkinAtomic(atomic, data): data -> RpSkinAtomicSetHAnimHierarchy(atomic, data) and null; no data -> the hierarchy of the atomic's frame subtree
    Run("CClumpModelInfo::SetHierarchyForSkinAtomic 0x4C4EF0", [&](Rng& r, std::string& d) {
        g_idSafe = false; const size_t fi = r.below(kNumFr); void* data = r.below(2) ? nullptr : (void*)(uintptr_t)(0x5000 + 16 * r.below(32));
        return MCase(r, d, [&]() -> uint64_t { At<void*>(kAt + 4) = Fr(fi % g_nFr); return U(CClumpModelInfo::SetHierarchyForSkinAtomic(reinterpret_cast<RpAtomic*>(P(kAt)), data)); },
                           [&]() -> uint64_t { At<void*>(kAt + 4) = Fr(fi % g_nFr); return ExeC<uint32_t>(0x4C4EF0, P(kAt), data); }, &BuildFrames);
    }, 400);
    // ---- CreateInstance(matrix): virtual CreateInstance() (slot 11) is a host stub returning a fixture clump, then the 64-byte matrix is copied into the clump frame's modelling matrix
    Run("CClumpModelInfo::CreateInstance(mat) 0x4C5110", [&](Rng& r, std::string& d) {
        g_idSafe = false;
        return MCase(r, d, [&]() -> uint64_t {
                               // the port's RwFrame is librw's: its modelling matrix is not at frame+0x10 as in the exe's frame; move what the port wrote there so that the raw bytes compare
                               constexpr size_t kPm = offsetof(RwFrame, matrix);
                               uint8_t before[0x60]; std::memcpy(before, Fr(0), sizeof before);
                               At<void*>(kMi) = P(kFakeVt); const uint64_t v = U(CM()->CClumpModelInfo::CreateInstance(reinterpret_cast<RwMatrix*>(P(kInst)))); At<void*>(kMi) = nullptr;
                               if (!v) return v; uint8_t mat[64]; std::memcpy(mat, Fr(0) + kPm, 64);
                               std::memcpy(Fr(0) + kPm, before + kPm, 64);
                               std::memcpy(Fr(0) + 0x10, mat, 64);
                               return v; },
                           [&]() -> uint64_t { At<void*>(kMi) = P(kFakeVt); const uint64_t v = ExeT<uint32_t>(0x4C5110, P(kMi), P(kInst)); At<void*>(kMi) = nullptr; return v; },
                           [](Rng& rr) {
                               BuildFrames(rr);
                               for (int i = 0; i < 17; ++i) At<void*>(kFakeVt + i * 4) = reinterpret_cast<void*>(&FakeCreateInstance);   // every slot: the port's slot order of the two CreateInstance overloads is the compiler's, the exe's call is slot 11
                               At<void*>(kFakeVt + 11 * 4) = reinterpret_cast<void*>(&FakeCreateInstance);
                               if (rr.below(4) == 0) At<void*>(kMi + 0x1C) = nullptr;
                               FillGarbage(rr, reinterpret_cast<uintptr_t>(P(kInst)), 64);
                           });
    }, 200);
    // ---- AtomicSetupLightingCB / SetDamagedAtomic: with no pipeline of the building / car renderers attached only the predicates run
    Run("CClumpModelInfo::AtomicSetupLightingCB 0x4C4F30", [&](Rng& r, std::string& d) {
        return MCase(r, d, [&]() -> uint64_t { return U(CClumpModelInfo::AtomicSetupLightingCB(reinterpret_cast<RpAtomic*>(P(kAt)), nullptr)); },
                           [&]() -> uint64_t { return ExeC<uint32_t>(0x4C4F30, P(kAt), nullptr); }, &BuildAtomicGeo);
    }, 600);
    Run("CDamageAtomicModelInfo::SetDamagedAtomic 0x4C48D0", [&](Rng& r, std::string& d) {
        return MCase(r, d, [&]() -> uint64_t { reinterpret_cast<CDamageAtomicModelInfo*>(P(kMi))->SetDamagedAtomic(reinterpret_cast<RpAtomic*>(P(kAt))); return 0; },
                           [&]() -> uint64_t { ExeT<void>(0x4C48D0, P(kMi), P(kAt)); return 0; }, &BuildAtomicGeo);
    }, 600);
    // ---- CAtomicModelInfo::SetAtomic: 2dfx counts of the old and new atomic, txd ref, (anim ref never: GetAnimFileIndex of an atomic info is -1), pipeline predicates, tag flags, the pre-rendered flag
    Run("CAtomicModelInfo::SetAtomic 0x4C4360", [&](Rng& r, std::string& d) {
        return MCase(r, d, [&]() -> uint64_t { PutVt(P(kMi), kVtAtomic, 1); reinterpret_cast<CAtomicModelInfo*>(P(kMi))->SetAtomic(reinterpret_cast<RpAtomic*>(P(kAt))); PutVt(P(kMi), kVtAtomic, 0); return 0; },
                           [&]() -> uint64_t { PutVt(P(kMi), kVtAtomic, 2); ExeT<void>(0x4C4360, P(kMi), P(kAt)); PutVt(P(kMi), kVtAtomic, 0); return 0; }, &BuildSetAtomic);
    }, 600);
    // ---- the pipeline predicates themselves (every combination of pipeline id / colours / night colours)
    Run("CCustomBuildingRenderer::IsCBPCPipelineAttached 0x5D7F40", [&](Rng& r, std::string& d) {
        g_noPipe = false;
        const bool ok = MCase(r, d, [&]() -> uint64_t { return CCustomBuildingRenderer::IsCBPCPipelineAttached(reinterpret_cast<RpAtomic*>(P(kAt))) ? 1 : 0; },
                                    [&]() -> uint64_t { return ExeC<uint8_t>(0x5D7F40, P(kAt)) != 0 ? 1 : 0; }, &BuildAtomicGeo);
        g_noPipe = true; return ok;
    }, 600);
    Run("CCarFXRenderer::IsCCPCPipelineAttached 0x5D5B80", [&](Rng& r, std::string& d) {
        g_noPipe = false;
        const bool ok = MCase(r, d, [&]() -> uint64_t { return CCarFXRenderer::IsCCPCPipelineAttached(reinterpret_cast<RpAtomic*>(P(kAt))) ? 1 : 0; },
                                    [&]() -> uint64_t { return ExeC<uint8_t>(0x5D5B80, P(kAt)) != 0 ? 1 : 0; }, &BuildAtomicGeo);
        g_noPipe = true; return ok;
    }, 400);
}

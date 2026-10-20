// ORACLE-W3-STREAM: exe-oracle rows for the A + B functions of game_sa/IplStore.cpp (IPL slots: the pool at ms_pPool 0x8E3FB0, bounding boxes, "needed at posn" flags, required-callbacks of the quad tree),
// game_sa/TxdStore.cpp (the TXD slot pool at 0xC8800C, ref counts, parents, push/pop of the current RwTexDictionary) and game_sa/Streaming.cpp (request lists, loaded-vehicle bookkeeping, link list of
// the RW object instances). Included by wave2_oracle_all.inc when W3_STREAM_ONLY is defined (executable wave3_stream_oracle_test = the W2 harness: deterministic arena, the whole exe data image + the
// fixture block compared after every case).
// Fixtures: CPool headers (storage / flag bytes / capacity / first-free) over small slot arrays in the fixture block with a random used / free split, the exe's CBuilding / CDummy pool headers whose
// storage base is moved so that the entity's index covers the game range, RwTexDictionary blobs with the TxdStore parent plugin at a random offset. RenderWare is not run: RwTexDictionaryGetCurrent (0x7F3A90),
// RwTexDictionarySetCurrent (0x7F3A70) and RwTexDictionaryFindNamedTexture (0x7F39F0) are Patch()ed to host stubs and the port's fakerw entry points of the same names have the same bodies.
#include "IplStore.h"
#include "TxdStore.h"
#include "Pools/Pools.h"

namespace w3s {
using namespace w2;
constexpr size_t kScSize = 0x4000;
alignas(16) static uint8_t g_sc[kScSize];
constexpr size_t kIplHdr = 0x000, kIplFl = 0x020, kIplSt = 0x040 /*16 x 0x34 = 0x340*/, kTxdHdr = 0x380, kTxdFl = 0x3A0, kTxdSt = 0x3C0 /*16 x 0xC*/, kDict = 0x500 /*8 x 0x40*/, kStr = 0x700 /*0x100*/,
                 kVec = 0x800, kRec = 0x820, kCur = 0x860, kEnt = 0x900 /*4 x 0x40*/, kBHdr = 0xA00, kDHdr = 0xA20, kDef = 0xA40 /*0x40*/, kPos = 0xA80, kTex = 0xAA0 /*4 x 0x20*/;
constexpr uint32_t kNPoolSlots = 16;
static uint8_t* S(size_t off) { return g_sc + off; }
template<class T> static T& SAt(size_t off) { return *reinterpret_cast<T*>(g_sc + off); }
static uint32_t P(size_t off) { return (uint32_t)(uintptr_t)(g_sc + off); }
static uint64_t U(const void* p) { return (uint64_t)(uintptr_t)p; }
template<class R, class... A> static R ExeC(uint32_t addr, A... a) { return oracle::Fn<R __cdecl(A...)>(addr)(a...); }
static float Fe(Rng& r, float scale, int special = 10) {
    if (special && r.below((uint32_t)special) == 0) return GenF(r, scale, S_NAN | S_ZERO | S_DEN | S_TINY | S_INF | S_HUGE);
    return GenF(r, scale, S_ZERO);
}

// ---- RenderWare stubs (the same bodies behind the exe address and the port's fakerw name) ----
// current dictionary at kCur; FindNamedTexture(dict, name): the texture pointer stored at dict + 0x3C (0 = none), the call is recorded (count at kRec, last dict / name at +4 / +8)
static void* StubGetCurrent() { return SAt<void*>(kCur); }
static void* StubSetCurrent(void* d) { SAt<void*>(kCur) = d; return d; }
static void* StubFindNamed(void* dict, const char* name) {
    SAt<uint32_t>(kRec) += 1; SAt<void*>(kRec + 4) = dict; SAt<const void*>(kRec + 8) = name;
    return dict ? *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(dict) + 0x3C) : nullptr;
}
static void InstallStubs() {
    static bool done = false; if (done) return; done = true;
    oracle::Patch(0x7F3A90, (void*)&StubGetCurrent);
    oracle::Patch(0x7F3A70, (void*)&StubSetCurrent);
    oracle::Patch(0x7F39F0, (void*)&StubFindNamed);
}

// ---- pools: header (storage, flags, capacity, first free), `cap` slots, random used / free split ----
static void MakePool(Rng& r, size_t hdr, size_t flags, size_t store, uint32_t cap, uint32_t global, int allowFull = 1) {
    SAt<uint32_t>(hdr + 0) = P(store); SAt<uint32_t>(hdr + 4) = P(flags); SAt<uint32_t>(hdr + 8) = cap;
    SAt<int32_t>(hdr + 0xC) = (int32_t)r.below(cap + 1) - 1;
    SAt<uint8_t>(hdr + 0x10) = 0; SAt<uint8_t>(hdr + 0x11) = 0;
    const uint32_t mode = r.below(6);
    int nFree = 0;
    for (uint32_t i = 0; i < kNPoolSlots; ++i) {
        const bool empty = mode == 0 ? false : mode == 1 ? true : r.below(2) != 0;
        SAt<uint8_t>(flags + i) = (uint8_t)((r.below(128) & 0x7F) | (empty ? 0x80 : 0));
        if (i < cap && empty) ++nFree;
    }
    if (!allowFull && !nFree) SAt<uint8_t>(flags + r.below(cap)) |= 0x80;
    StaticRef<uint32_t>(global) = P(hdr);
}
// a used slot (forces one if all are free); -1 never
static int32_t UsedSlot(Rng& r, size_t flags, uint32_t cap) {
    int32_t cand[kNPoolSlots]; uint32_t n = 0;
    for (uint32_t i = 0; i < cap; ++i) if (!(SAt<uint8_t>(flags + i) & 0x80)) cand[n++] = (int32_t)i;
    if (!n) { const uint32_t i = r.below(cap); SAt<uint8_t>(flags + i) &= 0x7F; return (int32_t)i; }
    return cand[r.below(n)];
}
static int32_t AnySlot(Rng& r, uint32_t cap) { return (int32_t)r.below(cap); }

static uint32_t g_iplCap = 8, g_txdCap = 8;
static void FillIplDef(Rng& r, size_t at) {
    FillGarbage(r, S(at), 0x34);
    SAt<uint8_t>(at + 0x2C) = (uint8_t)r.below(2); SAt<uint8_t>(at + 0x2D) = (uint8_t)r.below(2); SAt<uint8_t>(at + 0x2E) = (uint8_t)r.below(2); SAt<uint8_t>(at + 0x2F) = (uint8_t)r.below(2);
}
static void BuildAll(Rng& r) {
    FillGarbage(r, g_sc, kScSize);
    SAt<uint32_t>(kRec) = 0; SAt<uint32_t>(kRec + 4) = 0; SAt<uint32_t>(kRec + 8) = 0;
    g_iplCap = 4 + r.below(kNPoolSlots - 3); g_txdCap = 4 + r.below(kNPoolSlots - 3);
    MakePool(r, kIplHdr, kIplFl, kIplSt, g_iplCap, 0x8E3FB0);
    MakePool(r, kTxdHdr, kTxdFl, kTxdSt, g_txdCap, 0xC8800C);
    for (uint32_t i = 0; i < kNPoolSlots; ++i) {
        // IplDef: random bounding box (game range / edge values), strings with a NUL in the name field, bool bytes 0 / 1
        FillIplDef(r, kIplSt + 0x34 * i);
        for (int k = 0; k < 4; ++k) SAt<float>(kIplSt + 0x34 * i + 4 * k) = Fe(r, 3000.f, 16);
        // TxdDef: dictionary pointer (blob or null), refs, parent slot (-1 or a slot), hash
        const uint32_t dk = r.below(10);
        SAt<uint32_t>(kTxdSt + 12 * i) = dk < 8 ? P(kDict + 0x40 * dk) : 0;
        SAt<int16_t>(kTxdSt + 12 * i + 4) = r.below(4) ? (int16_t)r.below(20) : (int16_t)r.u32();
        SAt<int16_t>(kTxdSt + 12 * i + 6) = r.below(3) == 0 ? (int16_t)-1 : (int16_t)r.below(g_txdCap);
        SAt<uint32_t>(kTxdSt + 12 * i + 8) = r.below(3) ? 0x1000u + r.below(6) : r.u32();
    }
    // dictionary blobs: the parent plugin at ms_txdPluginOffset (random 4-aligned), a texture result at +0x3C
    StaticRef<int32_t>(0xC88018) = 0x10 + 4 * (int32_t)r.below(11);        // 0x10..0x38: inside the 0x40 blob, clear of the texture result at +0x3C
    for (int k = 0; k < 8; ++k) {
        SAt<uint32_t>(kDict + 0x40 * k + 0x3C) = r.below(3) == 0 ? P(kTex + 0x20 * r.below(4)) : 0;
        const uint32_t pk = r.below(10);
        // parent chain: only to a higher index (no cycles), or none
        SAt<uint32_t>(kDict + 0x40 * k + StaticRef<int32_t>(0xC88018)) = pk < 7 && (k + 1) < 8 ? P(kDict + 0x40 * (k + 1 + r.below(8 - k - 1))) : 0;
    }
    SAt<uint32_t>(kCur) = r.below(4) ? P(kDict + 0x40 * r.below(8)) : r.below(2) ? 0 : r.u32();
    StaticRef<uint32_t>(0xC88010) = r.below(3) ? 0 : r.below(2) ? P(kDict + 0x40 * r.below(8)) : r.u32();            // CTxdStore::ms_pStoredTxd
    StaticRef<int32_t>(0x8E3EF8) = r.below(3) ? 0 : (int32_t)r.below(20);                                              // ms_currentIPLAreaCode
    StaticRef<uint32_t>(0xC88014) = r.below(10);                                                                        // ms_lastSlotFound
    for (int i = 0; i < 40; ++i) StaticRef<uint32_t>(0x8E3F08 + 4 * i) = r.u32();
    StaticRef<uint8_t>(0x8E3FA8) = (uint8_t)r.below(2);                                                                 // gbIplsNeededAtPosn
    for (int i = 0; i < 3; ++i) StaticRef<float>(0x8E3FD0 + 4 * i) = Fe(r, 3000.f, 8);                                  // gvecIplsNeededAtPosn
}

static std::function<void(Rng&)> g_post;
static void BuildPost(Rng& rr) { BuildAll(rr); if (g_post) g_post(rr); }
// scRegs: the regions the fixture block is compared in (empty = the whole block)
template<class PF, class EF>
static bool VCase(Rng& r, std::string& d, PF&& pf, EF&& ef, std::vector<Reg> scRegs = {}) {
    const uint32_t seed = r.u32() | 1;
    BuildPost(r);
    const Reg z{ (uintptr_t)g_sc, 0 };
    const Reg r0 = scRegs.empty() ? Reg{ (uintptr_t)g_sc, kScSize } : scRegs[0];
    const Reg r1 = scRegs.size() > 1 ? scRegs[1] : z, r2 = scRegs.size() > 2 ? scRegs[2] : z;
    return BothA(d, seed, { r0, r1, r2, Reg{ kImgLo, kImgHi - kImgLo } }, std::forward<PF>(pf), std::forward<EF>(ef));
}
} // namespace w3s

// the port's fakerw RenderWare entry points called by TxdStore.cpp: bodies = the host stubs (the standalone RW TUs are not linked into this test)
RwTexDictionary* RwTexDictionaryGetCurrent() { return (RwTexDictionary*)w3s::StubGetCurrent(); }
RwTexDictionary* RwTexDictionarySetCurrent(RwTexDictionary* d) { return (RwTexDictionary*)w3s::StubSetCurrent(d); }
RwTexture* RwTexDictionaryFindNamedTexture(RwTexDictionary* d, const RwChar* n) { return (RwTexture*)w3s::StubFindNamed(d, n); }

#define SROW(NAME, ADDR) Run(NAME " " #ADDR, [&](Rng& r, std::string& d) -> bool {
#define SEND(CASES) }, std::max(10, (CASES) / w3s_div()));
static int w3s_div() { static const int v = std::getenv("W3S_DIV") ? std::max(1, std::atoi(std::getenv("W3S_DIV"))) : 1; return v; }


#include "wave3_oracle_stream_a.inc"
#include "wave3_oracle_stream_b.inc"

static void TestW3StreamAll() {
    using namespace w3s;
    (void)spdlog::default_logger_raw();
    spdlog::set_level(spdlog::level::off);
    InstallStubs();
    InstallStubsB();
    TestW3sIpl();
    TestW3sTxd();
    TestW3sStreaming();
    std::memset((void*)0xA9B0C8, 0, 4 * 20000);                          // the fake model infos must not outlive the rows
    StaticRef<uint32_t>(0xB74498) = 0; StaticRef<uint32_t>(0xB744A0) = 0; StaticRef<uint32_t>(0x8E3FB0) = 0; StaticRef<uint32_t>(0xC8800C) = 0;
}

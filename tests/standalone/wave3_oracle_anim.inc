// ORACLE-W3-ANIM: exe-oracle rows for the uncovered class A/B functions of game_sa/Animation and game_sa/Plugins/RpAnimBlendPlugin on REAL animation fixtures:
// a fake clump (plugin data pointer at the exe's ClumpOffset 0xB5F878) with an RpAnimBlend clump data, a frame array and a doubly linked list of 0..7 blend associations (flags, blend amount / delta, hierarchy,
// anim id ...); animation hierarchies with sequences and (un)compressed key frames; association groups with static associations; the exe's own CAnimManager tables (ms_aAnimations, ms_aAnimBlocks ...).
// Included by wave2_oracle_all.inc when W3_ANIM_ONLY is defined (executable wave3_anim_oracle_test = the W2 harness: deterministic arena, whole exe data image compared after every case).
// The fixture memory g_fx is compared as a whole after every call.
#include "Animation/AnimManager.h"
#include "Animation/AnimBlendAssociation.h"
#include "Animation/AnimBlendAssocGroup.h"
#include "Animation/AnimBlendStaticAssociation.h"
#include "Animation/AnimBlendClumpData.h"
#include "Animation/AnimBlendHierarchy.h"
#include "Animation/AnimBlendSequence.h"
#include "Plugins/RpAnimBlendPlugin/RpAnimBlend.h"
#include "Plugins/NodeNamePlugin/NodeName.h"

void* ClumpAnimConstruct(void* object, RwInt32 offsetInObject, RwInt32 sizeInObject);
void* ClumpAnimCopy(void* dstObject, const void* srcObject, RwInt32 offsetInObject, RwInt32 sizeInObject);
void RpAnimBlendKeyFrameInterpolate(void* voidOut, void* voidIn1, void* voidIn2, float time, void* customData);

// private members through explicit instantiation (W2Rob, wave2_oracle_common.inc)
struct W3A_ClumpData_LoadFramesIntoSPR { using type = void (CAnimBlendClumpData::*)(); friend type W2Get(W3A_ClumpData_LoadFramesIntoSPR); };
template struct W2Rob<W3A_ClumpData_LoadFramesIntoSPR, &CAnimBlendClumpData::LoadFramesIntoSPR>;
struct W3A_ClumpData_ForAllFramesInSPR { using type = void (CAnimBlendClumpData::*)(void (*)(AnimBlendFrameData*, void*), void*, uint32); friend type W2Get(W3A_ClumpData_ForAllFramesInSPR); };
template struct W2Rob<W3A_ClumpData_ForAllFramesInSPR, &CAnimBlendClumpData::ForAllFramesInSPR>;

namespace w3a {
using namespace w2;
constexpr size_t kFxSize = 0x8000;
alignas(16) static uint8_t g_fx[kFxSize];
constexpr size_t kClump = 0x000, kCD = 0x100, kFr = 0x140, kAs = 0x200, kAsSz = 0x40, kMaxAs = 8, kHierTab = 0x600, kSeqArr = 0x700, kKf = 0x1000, kKfEnd = 0x5000, kGroups = 0x5800, kStat = 0x5900, kObj = 0x5A00, kHierObj = 0x5C00, kDef = 0x5E00, kBlk = 0x5E80, kOut = 0x5F00, kFrame = 0x5F80, kCache = 0x6000;
constexpr uint32_t kClumpOff = 0x40;   // plugin data offset inside the fake clump
static uint8_t* P(size_t off) { return g_fx + off; }
template<class T> static T& At(size_t off) { return *reinterpret_cast<T*>(g_fx + off); }
struct Fx { int pick = 0, nth = 0, nAs = 0; CAnimBlendAssociation* as[kMaxAs]{}; CAnimBlendClumpData* cd = nullptr; RpClump* clump = nullptr; CAnimBlendHierarchy* hier[4]{}; size_t kfTop = kKf; };
static Fx g_f;
static size_t g_nameLen = 0;
static float BlendVal(Rng& r) { switch (r.below(8)) { case 0: return 0.f; case 1: return 1.f; case 2: return -0.25f; case 3: return 0.5f; case 4: return 0.999f; default: return r.f01(); } }
static float TimeVal(Rng& r) { return r.below(6) == 0 ? 0.f : r.f01() * 3.f; }
static CQuaternion RandQuat(Rng& r) {
    CQuaternion q; q.x = r.f01() - 0.5f; q.y = r.f01() - 0.5f; q.z = r.f01() - 0.5f; q.w = r.f01() - 0.5f;
    const float m = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    if (m > 1e-6f) { q.x /= m; q.y /= m; q.z /= m; q.w /= m; }
    return q;
}

// sequence at `off` with `n` key frames (uncompressed or compressed, with or without translation) whose DeltaTime fields hold ascending time stamps, as the loader leaves them before CalcTotalTime
static void BuildSeq(Rng& r, size_t off, int n, bool trans, bool compressed, bool external = false) {
    auto* s = reinterpret_cast<CAnimBlendSequence*>(P(off));
    std::memset(s, 0, sizeof(CAnimBlendSequence));
    s->m_bHasRotation = true; s->m_bHasTranslation = trans; s->m_bIsCompressed = compressed; s->m_bUsingExternalMemory = external; s->m_FramesNum = (uint16)n;
    s->m_IsUsingBoneTag = r.below(2);
    { const uint32_t v = s->m_IsUsingBoneTag ? (uint32_t)r.below(40) : r.u32(); std::memcpy(s, &v, 4); }   // the union (bone tag / name hash) is the first dword; its members are private
    const size_t ksz = compressed ? (trans ? sizeof(KeyFrameTransCompressed) : sizeof(KeyFrameCompressed)) : (trans ? sizeof(KeyFrameTrans) : sizeof(KeyFrame));
    if (n == 0) { s->m_Frames = nullptr; return; }
    if (g_f.kfTop + ksz * n > kKfEnd) { s->m_FramesNum = 0; s->m_Frames = nullptr; return; }
    s->m_Frames = P(g_f.kfTop); g_f.kfTop += (ksz * n + 15) & ~size_t(15);
    float t = 0.f;
    for (int i = 0; i < n; ++i) {
        t += r.below(6) == 0 ? 0.f : 0.01f + r.f01() * 0.5f;
        CQuaternion q = RandQuat(r);
        if (r.below(3) == 0) { q.x = -q.x; q.y = -q.y; q.z = -q.z; q.w = -q.w; }   // sign flips for RemoveQuaternionFlips
        uint8_t* kf = static_cast<uint8_t*>(s->m_Frames) + ksz * i;
        if (!compressed) {
            auto* k = reinterpret_cast<KeyFrame*>(kf); k->Rot = q; k->DeltaTime = t;
            if (trans) reinterpret_cast<KeyFrameTrans*>(kf)->Trans = CVector((r.f01() - 0.5f) * 4.f, (r.f01() - 0.5f) * 4.f, (r.f01() - 0.5f) * 4.f);
        } else {
            for (size_t b = 0; b < ksz; ++b) kf[b] = (uint8_t)r.u32();   // raw int16 fields: any value is a valid stored key frame
            if (r.below(3)) { int16_t dt = (int16_t)(t * 60.f); std::memcpy(kf + 8, &dt, 2); }   // DeltaTime (the 5th int16): ascending most of the time
        }
    }
}
// hierarchy object at `off` with `nSeq` sequences (array at seqOff)
static void BuildHierObj(Rng& r, size_t off, size_t seqOff, int nSeq, bool compressed) {
    auto* h = reinterpret_cast<CAnimBlendHierarchy*>(P(off));
    std::memset(h, 0, sizeof(CAnimBlendHierarchy));
    h->m_hashKey = r.u32(); h->m_pSequences = nSeq ? reinterpret_cast<CAnimBlendSequence*>(P(seqOff)) : nullptr; h->m_nSeqCount = (uint16)nSeq;
    h->m_bIsCompressed = compressed; h->m_bKeepCompressed = compressed && r.below(2); h->m_nAnimBlockId = (int32)r.below(8) - 1; h->m_fTotalTime = r.below(3) ? 0.f : r.f01() * 4.f;
    const bool allTrans = r.below(2);
    for (int i = 0; i < nSeq; ++i) BuildSeq(r, seqOff + i * sizeof(CAnimBlendSequence), r.below(7) == 0 ? 0 : 1 + r.below(7), i == 0 ? allTrans : r.below(4) == 0, compressed);
}

// builds the clump + association list in g_fx
static void BuildClump(Rng& r) {
    std::memset(g_fx, 0, kFxSize);
    g_f = Fx{};
    g_f.nth = r.below(4);
    StaticRef<uint32_t>(0xB5F878) = kClumpOff;
    g_f.clump = reinterpret_cast<RpClump*>(P(kClump));
    g_f.cd = reinterpret_cast<CAnimBlendClumpData*>(P(kCD));
    At<void*>(kClump + kClumpOff) = g_f.cd;
    const uint32_t nFr = r.below(5) == 0 ? 0 : 1 + r.below(6);
    g_f.cd->m_NumFrameData = nFr;
    g_f.cd->m_FrameDatas = reinterpret_cast<AnimBlendFrameData*>(P(kFr));
    for (int i = 0; i < 4; ++i) {
        auto* h = g_f.hier[i] = reinterpret_cast<CAnimBlendHierarchy*>(P(kHierTab + i * 0x18));
        h->m_hashKey = r.u32(); h->m_nAnimBlockId = (int32)r.below(6); h->m_fTotalTime = r.below(5) == 0 ? 0.f : 0.2f + r.f01() * 3.f; h->m_bKeepCompressed = r.below(2);
    }
    const int n = r.below(5) == 0 ? 0 : 1 + r.below((int)kMaxAs);
    g_f.nAs = n;
    g_f.pick = r.below(n ? n : 1);
    CAnimBlendLink* prev = &g_f.cd->m_AnimList;
    for (int i = 0; i < g_f.nAs; ++i) {
        auto* a = reinterpret_cast<CAnimBlendAssociation*>(P(kAs + i * kAsSz));
        g_f.as[i] = a;
        a->m_Link.prev = prev; a->m_Link.next = nullptr; prev->next = &a->m_Link; prev = &a->m_Link;
        a->m_NumBlendNodes = 0; a->m_AnimGroupId = (AssocGroupId)r.below(10); a->m_BlendNodes = nullptr;
        a->m_BlendHier = r.below(8) ? g_f.hier[r.below(4)] : nullptr;
        a->m_BlendAmount = BlendVal(r); a->m_BlendDelta = r.below(3) ? 0.f : (r.f01() - 0.5f) * 2.f;
        a->m_CurrentTime = TimeVal(r); a->m_Speed = r.below(4) ? 1.f : r.f01() * 2.f; a->m_TimeStep = r.f01() * 0.05f;
        a->m_AnimId = (AnimationId)r.below(12);
        uint16_t fl = (uint16_t)r.u32();
        if (r.below(2)) fl = (uint16_t)(1u << r.below(16));
        if (r.below(3) == 0) fl |= ANIMATION_IS_PARTIAL; else if (r.below(2)) fl &= ~ANIMATION_IS_PARTIAL;
        a->m_Flags = fl;
    }
    FillGarbage(r, P(kOut), 16);
}
static uint32_t FlagArg(Rng& r) { switch (r.below(6)) { case 0: return 0; case 1: return ANIMATION_IS_PARTIAL; case 2: return ANIMATION_IS_PLAYING; case 3: return 1u << r.below(16); case 4: return r.u32() & 0xFFFF; default: return r.u32() & 0x1F0; } }
static void BuildGarbage(Rng& r) { std::memset(g_fx, 0, kFxSize); g_f = Fx{}; FillGarbage(r, g_fx + kObj, 0x200); FillGarbage(r, g_fx + kFrame, 0x80); }   // fresh garbage in the object areas (kObj..kObj+0x200, the fake frame)
static void BuildHierRow(Rng& r) {
    std::memset(g_fx, 0, kFxSize); g_f = Fx{};
    const bool comp = r.below(2);
    BuildHierObj(r, kHierObj, kSeqArr, r.below(6) == 0 ? 0 : 1 + r.below(5), comp);
}

static void BuildSeqU(Rng& r) { std::memset(g_fx, 0, kFxSize); g_f = Fx{}; BuildSeq(r, kObj, r.below(8), r.below(2), false); }
static void BuildSeqC(Rng& r) { std::memset(g_fx, 0, kFxSize); g_f = Fx{}; BuildSeq(r, kObj, r.below(8), r.below(2), true); }
static void BuildSeqAny(Rng& r) { std::memset(g_fx, 0, kFxSize); g_f = Fx{}; BuildSeq(r, kObj, r.below(8), r.below(2), r.below(2), r.below(3) == 0); }
static void BuildHierU(Rng& r) { std::memset(g_fx, 0, kFxSize); g_f = Fx{}; BuildHierObj(r, kHierObj, kSeqArr, r.below(6) == 0 ? 0 : 1 + r.below(5), false); }
static void BuildHierC(Rng& r) { std::memset(g_fx, 0, kFxSize); g_f = Fx{}; BuildHierObj(r, kHierObj, kSeqArr, r.below(6) == 0 ? 0 : 1 + r.below(5), true); }
// groups: 3 association groups with 0..4 static associations each; the exe's ms_aAnimAssocGroups pointer / ms_numAnimAssocDefinitions point at them
static void BuildGroups(Rng& r) {
    std::memset(g_fx, 0, kFxSize); g_f = Fx{};
    FillGarbage(r, g_fx + kStat, 0xF0);
    for (int g = 0; g < 3; ++g) {
        auto* grp = reinterpret_cast<CAnimBlendAssocGroup*>(P(kGroups + g * sizeof(CAnimBlendAssocGroup)));
        grp->m_AnimBlock = reinterpret_cast<CAnimBlock*>(P(kBlk)); grp->m_Anims = reinterpret_cast<CAnimBlendStaticAssociation*>(P(kStat + g * 0x50));
        grp->m_NumAnims = r.below(5); grp->m_IdOffset = r.below(60); grp->m_GroupID = (AssocGroupId)g;
    }
    StaticRef<void*>(0xB4EA34) = P(kGroups); StaticRef<uint32_t>(0xB4EA28) = 3;   // CAnimManager::ms_aAnimAssocGroups / ms_numAnimAssocDefinitions
    g_f.pick = r.below(3);
    auto* grp = reinterpret_cast<CAnimBlendAssocGroup*>(P(kGroups + g_f.pick * sizeof(CAnimBlendAssocGroup)));
    g_f.nth = grp->m_NumAnims ? (int)(grp->m_IdOffset + r.below((int)grp->m_NumAnims)) : (int)grp->m_IdOffset;
}
static void BuildDef(Rng& r) {
    std::memset(g_fx, 0, kFxSize); g_f = Fx{};
    auto* def = reinterpret_cast<AnimAssocDefinition*>(P(kDef));
    FillGarbage(r, P(kDef), 0x20); def->NumAnims = 1 + r.below(4);
    def->AnimNames = reinterpret_cast<const char**>(P(kDef + 0x30));
    const int used = r.below(def->NumAnims);   // at least one empty slot (the port asserts i < NumAnims)
    for (int i = 0; i < 4; ++i) {
        At<void*>(kDef + 0x30 + i * 4) = P(kObj + i * 24);
        if (i < used) { const int n = 1 + r.below(10); for (int k = 0; k < n; ++k) *P(kObj + i * 24 + k) = (uint8_t)('a' + r.below(26)); }
        else FillGarbage(r, P(kObj + i * 24 + 4), 20);   // garbage after the terminating 0 (the slot's first byte stays 0)
    }
    g_f.pick = used;
}
static void BuildBlock(Rng& r) {
    std::memset(g_fx, 0, kFxSize); g_f = Fx{};
    auto* b = reinterpret_cast<CAnimBlock*>(P(kBlk));
    b->FirstAnimIdx = r.below(200); b->NumAnims = r.below(20); b->IsLoaded = r.below(2); b->RefCnt = (int16)r.below(5);
    static const uint32_t hs[] = { 0x11111111u, 0x22222222u, 0x33333333u, 0xDEADBEEFu, 0 };
    for (uint32_t k = 0; k < b->NumAnims + 2; ++k) reinterpret_cast<CAnimBlendHierarchy*>(0xB4EA40)[b->FirstAnimIdx + k].m_hashKey = hs[r.below(5)];   // CAnimManager::ms_aAnimations
}
static void BuildCache(Rng& r) {
    std::memset(g_fx, 0, kFxSize); g_f = Fx{};
    auto& c = *reinterpret_cast<CLinkList<CAnimBlendHierarchy*>*>(0xB5EB20);   // CAnimManager::ms_AnimCache
    c.usedListHead.next = &c.usedListTail; c.usedListTail.prev = &c.usedListHead; c.usedListHead.prev = nullptr; c.usedListTail.next = nullptr;
    c.freeListHead.next = &c.freeListTail; c.freeListTail.prev = &c.freeListHead; c.freeListHead.prev = nullptr; c.freeListTail.next = nullptr;
    auto* ln = reinterpret_cast<CLink<CAnimBlendHierarchy*>*>(P(kCache));
    for (int i = 0; i < 6; ++i) { ln[i].data = nullptr; if (r.below(2)) ln[i].Insert(&c.usedListHead); else ln[i].Insert(&c.freeListHead); }
    c.links = ln;
    auto* h = reinterpret_cast<CAnimBlendHierarchy*>(P(kHierObj));
    h->m_hashKey = r.u32();
    const int k = r.below(7);   // 6 = no link
    if (k < 6) { h->m_Link = &ln[k]; ln[k].data = h; } else h->m_Link = nullptr;
}

static void OnFaultA() { oracle::alloc::End(); }

// one case: the fixture is built by `build`, port and exe run on identical bytes (fixture, arena, whole data image restored between the two runs)
// vptrOff: offset in g_fx of an object whose first dword (vptr) is not compared (constructor rows, lenient compare: bits only the port wrote are INFO)
template<class PF, class EF>
static bool ACase(Rng& r, std::string& d, PF&& pf, EF&& ef, void (*build)(Rng&) = &BuildClump, size_t vptrOff = ~size_t(0), size_t skipN = 4, bool lenient = false, std::pair<size_t, size_t> (*maskFn)() = nullptr) {
    const uint32_t seed = r.u32() | 1;
    build(r);
    StaticRef<uint32_t>(0xB4EA30) = 180;   // CAnimManager::ms_numAnimBlocks (the port's GetAnimBlocks() view is bounds-checked in this debug build)
    Reg fx{ (uintptr_t)g_fx, kFxSize };
    if (vptrOff != ~size_t(0)) { fx.lenient = true; fx.skipOff = vptrOff; fx.skipN = skipN; }
    if (lenient) fx.lenient = true;
    if (maskFn) { const auto mr = maskFn(); fx.mask.assign(kFxSize, 0); for (size_t k = 0; k < mr.second; ++k) fx.mask[mr.first + k] = 1; }
    return BothA(d, seed, { fx, Reg{ kImgLo, kImgHi - kImgLo } }, std::forward<PF>(pf), std::forward<EF>(ef));
}
static inline uint64_t U(const void* p) { return (uint64_t)(uintptr_t)p; }
} // namespace w3a

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static void TestW3AnimClump() {
    using namespace w2; using namespace w3a;
    using F1 = void* __cdecl(void*);
    auto Cl = [] { return static_cast<void*>(g_f.clump); };
    // ---- association list traversal / queries (RpAnimBlend)
    Run("RpAnimBlendClumpGetFirstAssociation 0x4D15E0", [&](Rng& r, std::string& d) {
        return ACase(r, d, [&]() -> uint64_t { return U(RpAnimBlendClumpGetFirstAssociation(g_f.clump)); },
            [&]() -> uint64_t { return U(oracle::Fn<F1>(0x4D15E0)(Cl())); });
    }, 400);
    Run("RpAnimBlendClumpGetFirstAssociation-Flags 0x4D6A70", [&](Rng& r, std::string& d) {
        const uint32_t fl = FlagArg(r);
        return ACase(r, d, [&]() -> uint64_t { return U(RpAnimBlendClumpGetFirstAssociation(g_f.clump, fl)); },
            [&]() -> uint64_t { return U(oracle::Fn<void* __cdecl(void*, uint32_t)>(0x4D6A70)(Cl(), fl)); });
    }, 400);
    Run("RpAnimBlendGetNextAssociation-Any 0x4D6AB0", [&](Rng& r, std::string& d) {
        return ACase(r, d, [&]() -> uint64_t { return g_f.nAs ? U(RpAnimBlendGetNextAssociation(g_f.as[g_f.pick])) : 0; },
            [&]() -> uint64_t { return g_f.nAs ? U(oracle::Fn<F1>(0x4D6AB0)(g_f.as[g_f.pick])) : 0; });
    }, 400);
    Run("RpAnimBlendGetNextAssociation-Flags 0x4D6AD0", [&](Rng& r, std::string& d) {
        const uint32_t fl = FlagArg(r) | (r.below(8) == 0 ? 0 : 1);   // the port asserts flags != 0
        return ACase(r, d, [&]() -> uint64_t { return g_f.nAs ? U(RpAnimBlendGetNextAssociation(g_f.as[g_f.pick], fl)) : 0; },
            [&]() -> uint64_t { return g_f.nAs ? U(oracle::Fn<void* __cdecl(void*, uint32_t)>(0x4D6AD0)(g_f.as[g_f.pick], fl)) : 0; });
    }, 400);
    Run("RpAnimBlendClumpGetMainAssociation 0x4D6910", [&](Rng& r, std::string& d) {
        const bool useP = r.below(5) != 0, useB = r.below(5) != 0;
        return ACase(r, d, [&]() -> uint64_t { return U(RpAnimBlendClumpGetMainAssociation(g_f.clump, useP ? &At<CAnimBlendAssociation*>(kOut) : nullptr, useB ? &At<float>(kOut + 4) : nullptr)); },
            [&]() -> uint64_t { return U(oracle::Fn<void* __cdecl(void*, void*, void*)>(0x4D6910)(Cl(), useP ? P(kOut) : nullptr, useB ? P(kOut + 4) : nullptr)); });
    }, 600);
    Run("RpAnimBlendClumpGetMainAssociation_N 0x4D6A30", [&](Rng& r, std::string& d) {
        const uint32_t n = r.below(4);
        return ACase(r, d, [&]() -> uint64_t { return U(RpAnimBlendClumpGetMainAssociation_N(g_f.clump, n)); },
            [&]() -> uint64_t { return U(oracle::Fn<void* __cdecl(void*, uint32_t)>(0x4D6A30)(Cl(), n)); });
    }, 400);
    Run("RpAnimBlendClumpGetMainPartialAssociation 0x4D69A0", [&](Rng& r, std::string& d) {
        return ACase(r, d, [&]() -> uint64_t { return U(RpAnimBlendClumpGetMainPartialAssociation(g_f.clump)); },
            [&]() -> uint64_t { return U(oracle::Fn<F1>(0x4D69A0)(Cl())); });
    }, 400);
    Run("RpAnimBlendClumpGetMainPartialAssociation_N 0x4D69F0", [&](Rng& r, std::string& d) {
        const int n = r.below(4);
        return ACase(r, d, [&]() -> uint64_t { return U(RpAnimBlendClumpGetMainPartialAssociation_N(g_f.clump, n)); },
            [&]() -> uint64_t { return U(oracle::Fn<void* __cdecl(void*, int)>(0x4D69F0)(Cl(), n)); });
    }, 400);
    Run("RpAnimBlendClumpGetAssociation-AnimId 0x4D68B0", [&](Rng& r, std::string& d) {
        const uint32_t id = r.below(14);
        return ACase(r, d, [&]() -> uint64_t { return U(RpAnimBlendClumpGetAssociation(g_f.clump, id)); },
            [&]() -> uint64_t { return U(oracle::Fn<void* __cdecl(void*, uint32_t)>(0x4D68B0)(Cl(), id)); });
    }, 400);
    Run("RpAnimBlendClumpGetAssociation-Hier 0x4D68E0", [&](Rng& r, std::string& d) {
        const int hi = r.below(5);
        auto H = [&]() -> CAnimBlendHierarchy* { return hi < 4 ? g_f.hier[hi] : nullptr; };
        return ACase(r, d, [&]() -> uint64_t { return U(RpAnimBlendClumpGetAssociation(g_f.clump, false, H())); },
            [&]() -> uint64_t { return U(oracle::Fn<void* __cdecl(void*, int, void*)>(0x4D68E0)(Cl(), 0, H())); });
    }, 400);
    Run("RpAnimBlendClumpIsInitialized 0x4D6760", [&](Rng& r, std::string& d) {
        return ACase(r, d, [&]() -> uint64_t { return RpAnimBlendClumpIsInitialized(g_f.clump) ? 1 : 0; },
            [&]() -> uint64_t { return (uint8_t)oracle::Fn<int __cdecl(void*)>(0x4D6760)(Cl()) ? 1 : 0; });
    }, 200);
    Run("RpAnimBlendClumpGetNumAssociations 0x4D6B60", [&](Rng& r, std::string& d) {
        return ACase(r, d, [&]() -> uint64_t { return RpAnimBlendClumpGetNumAssociations(g_f.clump); }, [&]() -> uint64_t { return (uint32_t)oracle::Fn<uint32_t __cdecl(void*)>(0x4D6B60)(Cl()); });
    }, 300);
    Run("RpAnimBlendClumpGetNumPartialAssociations 0x4D6B80", [&](Rng& r, std::string& d) {
        return ACase(r, d, [&]() -> uint64_t { return RpAnimBlendClumpGetNumPartialAssociations(g_f.clump); }, [&]() -> uint64_t { return (uint32_t)oracle::Fn<uint32_t __cdecl(void*)>(0x4D6B80)(Cl()); });
    }, 300);
    Run("RpAnimBlendClumpGetNumNonPartialAssociations 0x4D6BB0", [&](Rng& r, std::string& d) {
        return ACase(r, d, [&]() -> uint64_t { return RpAnimBlendClumpGetNumNonPartialAssociations(g_f.clump); }, [&]() -> uint64_t { return (uint32_t)oracle::Fn<uint32_t __cdecl(void*)>(0x4D6BB0)(Cl()); });
    }, 300);
    Run("RpAnimBlendClumpPauseAllAnimations 0x4D6B00", [&](Rng& r, std::string& d) {
        return ACase(r, d, [&]() -> uint64_t { RpAnimBlendClumpPauseAllAnimations(g_f.clump); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl(void*)>(0x4D6B00)(Cl()); return 0; });
    }, 300);
    Run("RpAnimBlendClumpUnPauseAllAnimations 0x4D6B30", [&](Rng& r, std::string& d) {
        return ACase(r, d, [&]() -> uint64_t { RpAnimBlendClumpUnPauseAllAnimations(g_f.clump); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl(void*)>(0x4D6B30)(Cl()); return 0; });
    }, 300);
    Run("RpAnimBlendClumpExtractAssociations 0x4D6BE0", [&](Rng& r, std::string& d) {
        return ACase(r, d, [&]() -> uint64_t { return U(RpAnimBlendClumpExtractAssociations(g_f.clump)); }, [&]() -> uint64_t { return U(oracle::Fn<F1>(0x4D6BE0)(Cl())); });
    }, 300);
    Run("RpAnimBlendClumpSetBlendDeltas 0x4D67E0", [&](Rng& r, std::string& d) {
        const uint32_t fl = FlagArg(r); const float dl = r.below(4) ? (r.f01() - 0.5f) * 20.f : (r.below(2) ? 0.f : -1.f);
        return ACase(r, d, [&]() -> uint64_t { RpAnimBlendClumpSetBlendDeltas(g_f.clump, fl, dl); return 0; },
            [&]() -> uint64_t { oracle::Fn<void __cdecl(void*, uint32_t, float)>(0x4D67E0)(Cl(), fl, dl); return 0; });
    }, 400);
    // ---- clump plugin construct / copy, key frame interpolation, frame names
    Run("ClumpAnimConstruct 0x4D5F40", [&](Rng& r, std::string& d) {
        const int32_t off = (int32_t)r.below(64), sz = (int32_t)r.below(64);
        return ACase(r, d, [&]() -> uint64_t { return U(ClumpAnimConstruct(Cl(), off, sz)); }, [&]() -> uint64_t { return U(oracle::Fn<void* __cdecl(void*, int, int)>(0x4D5F40)(Cl(), off, sz)); });
    }, 100);
    Run("ClumpAnimCopy 0x4D5F90", [&](Rng& r, std::string& d) {
        return ACase(r, d, [&]() -> uint64_t { return U(ClumpAnimCopy(P(kObj), P(kObj + 0x40), 4, 8)); }, [&]() -> uint64_t { return U(oracle::Fn<void* __cdecl(void*, const void*, int, int)>(0x4D5F90)(P(kObj), P(kObj + 0x40), 4, 8)); });
    }, 100);
    Run("RpAnimBlendKeyFrameInterpolate 0x4D60C0", [&](Rng& r, std::string& d) {
        const float t = r.f01();
        return ACase(r, d, [&]() -> uint64_t { RpAnimBlendKeyFrameInterpolate(P(kObj), P(kObj + 0x40), P(kObj + 0x80), t, P(kObj + 0xC0)); return 0; },
            [&]() -> uint64_t { oracle::Fn<void __cdecl(void*, void*, void*, float, void*)>(0x4D60C0)(P(kObj), P(kObj + 0x40), P(kObj + 0x80), t, P(kObj + 0xC0)); return 0; }, &BuildGarbage);
    }, 100);
    Run("RpAnimBlendFrameGetName 0x4D5EF0", [&](Rng& r, std::string& d) {
        const int32_t off = r.below(5) == 0 ? (int32_t)r.below(2) - 1 : 0x20;
        auto build = [](Rng& rr) { BuildGarbage(rr); };
        StaticRef<int32_t>(0xC87C5C) = off;
        return ACase(r, d, [&]() -> uint64_t { return U(RpAnimBlendFrameGetName(reinterpret_cast<RwFrame*>(P(kFrame)))); },
            [&]() -> uint64_t { return U(oracle::Fn<const char* __cdecl(void*)>(0x4D5EF0)(P(kFrame))); }, +build);
    }, 100);
    Run("RpAnimBlendFrameSetName 0x4D5F00", [&](Rng& r, std::string& d) {
        const int32_t off = r.below(6) == 0 ? (int32_t)r.below(2) - 1 : 0x20;
        char name[48]; const int len = r.below(4) == 0 ? 20 + r.below(26) : r.below(23); for (int i = 0; i < len; ++i) name[i] = (char)('A' + r.below(26)); name[len] = 0;
        StaticRef<int32_t>(0xC87C5C) = off;
        return ACase(r, d, [&]() -> uint64_t { RpAnimBlendFrameSetName(reinterpret_cast<RwFrame*>(P(kFrame)), name); return 0; },
            [&]() -> uint64_t { oracle::Fn<void __cdecl(void*, const char*)>(0x4D5F00)(P(kFrame), name); return 0; }, &BuildGarbage);
    }, 200);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static void TestW3AnimClasses() {
    using namespace w2; using namespace w3a;
    using FT2 = void __fastcall(void*, int, float, float);
    auto AP = [&]() -> CAnimBlendAssociation* { return g_f.nAs ? g_f.as[g_f.pick] : nullptr; };
    // ---- CAnimBlendAssociation
    Run("CAnimBlendAssociation::SetBlendTo 0x4CEB80", [&](Rng& r, std::string& d) {
        const float a = BlendVal(r), b = r.below(3) ? r.f01() * 10.f : (r.f01() - 0.5f) * 400.f;
        return ACase(r, d, [&]() -> uint64_t { if (auto* x = AP()) x->SetBlendTo(a, b); return 0; }, [&]() -> uint64_t { if (auto* x = AP()) oracle::Fn<FT2>(0x4CEB80)(x, 0, a, b); return 0; });
    }, 400);
    Run("CAnimBlendAssociation::SetBlend 0x4CEBA0", [&](Rng& r, std::string& d) {
        const float a = BlendVal(r), b = (r.f01() - 0.5f) * 20.f;
        return ACase(r, d, [&]() -> uint64_t { if (auto* x = AP()) x->SetBlend(a, b); return 0; }, [&]() -> uint64_t { if (auto* x = AP()) oracle::Fn<FT2>(0x4CEBA0)(x, 0, a, b); return 0; });
    }, 300);
    Run("CAnimBlendAssociation::SetDeleteCallback 0x4CEBC0", [&](Rng& r, std::string& d) {
        void* cb = reinterpret_cast<void*>((uintptr_t)r.u32()); void* dt = r.below(3) ? reinterpret_cast<void*>((uintptr_t)r.u32()) : nullptr;
        return ACase(r, d, [&]() -> uint64_t { if (auto* x = AP()) x->SetDeleteCallback(reinterpret_cast<void(*)(CAnimBlendAssociation*, void*)>(cb), dt); return 0; },
            [&]() -> uint64_t { if (auto* x = AP()) oracle::Fn<void __fastcall(void*, int, void*, void*)>(0x4CEBC0)(x, 0, cb, dt); return 0; });
    }, 200);
    Run("CAnimBlendAssociation::SetFinishCallback 0x4CEBE0", [&](Rng& r, std::string& d) {
        void* cb = reinterpret_cast<void*>((uintptr_t)r.u32()); void* dt = r.below(3) ? reinterpret_cast<void*>((uintptr_t)r.u32()) : nullptr;
        return ACase(r, d, [&]() -> uint64_t { if (auto* x = AP()) x->SetFinishCallback(reinterpret_cast<void(*)(CAnimBlendAssociation*, void*)>(cb), dt); return 0; },
            [&]() -> uint64_t { if (auto* x = AP()) oracle::Fn<void __fastcall(void*, int, void*, void*)>(0x4CEBE0)(x, 0, cb, dt); return 0; });
    }, 200);
    Run("CAnimBlendAssociation::UpdateTimeStep 0x4D13A0", [&](Rng& r, std::string& d) {
        const float a = r.below(4) ? 0.016f + r.f01() * 0.05f : (r.f01() - 0.5f) * 4.f, b = r.below(4) ? 1.f / (0.2f + r.f01() * 3.f) : r.f01() * 100.f;
        return ACase(r, d, [&]() -> uint64_t { if (auto* x = AP()) x->UpdateTimeStep(a, b); return 0; }, [&]() -> uint64_t { if (auto* x = AP()) oracle::Fn<FT2>(0x4D13A0)(x, 0, a, b); return 0; });
    }, 600);
    Run("CAnimBlendAssociation::ReferenceAnimBlock 0x4CEA50", [&](Rng& r, std::string& d) {
        return ACase(r, d, [&]() -> uint64_t { if (auto* x = AP()) x->ReferenceAnimBlock(); return 0; }, [&]() -> uint64_t { if (auto* x = AP()) oracle::Fn<void __fastcall(void*, int)>(0x4CEA50)(x, 0); return 0; });
    }, 300);
    Run("CAnimBlendAssociation::Constructor0 0x4CE9B0", [&](Rng& r, std::string& d) {
        return ACase(r, d, [&]() -> uint64_t { ::new (static_cast<void*>(P(kObj))) CAnimBlendAssociation(); return 0; }, [&]() -> uint64_t { oracle::Fn<void* __fastcall(void*, int)>(0x4CE9B0)(P(kObj), 0); return 0; }, &BuildGarbage, kObj, 4);
    }, 100);
    // ---- CAnimBlendAssocGroup / CAnimBlendStaticAssociation / CAnimBlendClumpData
    auto G = [] { return reinterpret_cast<CAnimBlendAssocGroup*>(P(kGroups + g_f.pick * sizeof(CAnimBlendAssocGroup))); };
    Run("CAnimBlendAssocGroup::GetNumAnimations 0x45B050", [&](Rng& r, std::string& d) {
        return ACase(r, d, [&]() -> uint64_t { return G()->GetNumAnimations(); }, [&]() -> uint64_t { return (uint32_t)oracle::Fn<uint32_t __fastcall(void*, int)>(0x45B050)(G(), 0); }, &BuildGroups);
    }, 100);
    Run("CAnimBlendAssocGroup::GetAnimBlock 0x45B060", [&](Rng& r, std::string& d) {
        return ACase(r, d, [&]() -> uint64_t { return U(G()->GetAnimBlock()); }, [&]() -> uint64_t { return U(oracle::Fn<void* __fastcall(void*, int)>(0x45B060)(G(), 0)); }, &BuildGroups);
    }, 100);
    Run("CAnimBlendAssocGroup::IsCreated 0x4D37A0", [&](Rng& r, std::string& d) {
        return ACase(r, d, [&]() -> uint64_t { return G()->IsCreated() ? 1 : 0; }, [&]() -> uint64_t { return (uint8_t)oracle::Fn<int __fastcall(void*, int)>(0x4D37A0)(G(), 0) ? 1 : 0; }, &BuildGroups);
    }, 100);
    Run("CAnimBlendAssocGroup::GetAnimation-AnimationId 0x4CE090", [&](Rng& r, std::string& d) {
        return ACase(r, d, [&]() -> uint64_t { return G()->m_NumAnims ? U(G()->GetAnimation((AnimationId)g_f.nth)) : 0; }, [&]() -> uint64_t { return G()->m_NumAnims ? U(oracle::Fn<void* __fastcall(void*, int, int)>(0x4CE090)(G(), 0, g_f.nth)) : 0; }, &BuildGroups);
    }, 200);
    Run("CAnimManager::GetAnimAssociation-by-id 0x4D3A60", [&](Rng& r, std::string& d) {
        auto NA = [] { return reinterpret_cast<CAnimBlendAssocGroup*>(P(kGroups + g_f.pick * sizeof(CAnimBlendAssocGroup)))->m_NumAnims; };
        return ACase(r, d, [&]() -> uint64_t { return NA() ? U(CAnimManager::GetAnimAssociation((AssocGroupId)g_f.pick, (AnimationId)g_f.nth)) : 0; },
            [&]() -> uint64_t { return NA() ? U(oracle::Fn<void* __cdecl(int, int)>(0x4D3A60)(g_f.pick, g_f.nth)) : 0; }, &BuildGroups);
    }, 200);
    Run("CAnimBlendAssocGroup::Constructor 0x4CDE70", [&](Rng& r, std::string& d) {
        return ACase(r, d, [&]() -> uint64_t { ::new (static_cast<void*>(P(kObj))) CAnimBlendAssocGroup(); return 0; }, [&]() -> uint64_t { oracle::Fn<void* __fastcall(void*, int)>(0x4CDE70)(P(kObj), 0); return 0; }, &BuildGarbage, ~size_t(0), 4, true);
    }, 100);
    Run("CAnimBlendStaticAssociation::Constructor1 0x4CE940", [&](Rng& r, std::string& d) {
        return ACase(r, d, [&]() -> uint64_t { ::new (static_cast<void*>(P(kObj))) CAnimBlendStaticAssociation(); return 0; }, [&]() -> uint64_t { oracle::Fn<void* __fastcall(void*, int)>(0x4CE940)(P(kObj), 0); return 0; }, &BuildGarbage, kObj, 4);
    }, 100);
    Run("CAnimBlendClumpData::Constructor 0x4CF0E0", [&](Rng& r, std::string& d) {
        return ACase(r, d, [&]() -> uint64_t { ::new (static_cast<void*>(P(kObj))) CAnimBlendClumpData(); return 0; }, [&]() -> uint64_t { oracle::Fn<void* __fastcall(void*, int)>(0x4CF0E0)(P(kObj), 0); return 0; }, &BuildGarbage, ~size_t(0), 4, true);
    }, 100);
    Run("CAnimBlendClumpData::LoadFramesIntoSPR 0x4CF1D0", [&](Rng& r, std::string& d) {
        auto pm = W2Get(W3A_ClumpData_LoadFramesIntoSPR{});
        return ACase(r, d, [&]() -> uint64_t { (reinterpret_cast<CAnimBlendClumpData*>(P(kObj))->*pm)(); return 0; }, [&]() -> uint64_t { oracle::Fn<void __fastcall(void*, int)>(0x4CF1D0)(P(kObj), 0); return 0; }, &BuildGarbage);
    }, 20);
    Run("CAnimBlendClumpData::ForAllFramesInSPR 0x4CF1E0", [&](Rng& r, std::string& d) {
        auto pm = W2Get(W3A_ClumpData_ForAllFramesInSPR{});
        return ACase(r, d, [&]() -> uint64_t { (reinterpret_cast<CAnimBlendClumpData*>(P(kObj))->*pm)(nullptr, nullptr, 1); return 0; }, [&]() -> uint64_t { oracle::Fn<void __fastcall(void*, int, void*, void*, uint32_t)>(0x4CF1E0)(P(kObj), 0, nullptr, nullptr, 1); return 0; }, &BuildGarbage);
    }, 20);
    // ---- CAnimBlendSequence
    auto S = [] { return reinterpret_cast<CAnimBlendSequence*>(P(kObj)); };
    Run("CAnimBlendSequence::GetUKeyFrame 0x4CF1F0", [&](Rng& r, std::string& d) {
        const int n = r.below(8);
        return ACase(r, d, [&]() -> uint64_t { return S()->m_FramesNum ? U(S()->GetUKeyFrame(n % S()->m_FramesNum)) : 0; }, [&]() -> uint64_t { return S()->m_FramesNum ? U(oracle::Fn<void* __fastcall(void*, int, uint32_t)>(0x4CF1F0)(S(), 0, n % S()->m_FramesNum)) : 0; }, &BuildSeqU);
    }, 200);
    Run("CAnimBlendSequence::GetCKeyFrame 0x4CF220", [&](Rng& r, std::string& d) {
        const int n = r.below(8);
        return ACase(r, d, [&]() -> uint64_t { return S()->m_FramesNum ? U(S()->GetCKeyFrame(n % S()->m_FramesNum)) : 0; }, [&]() -> uint64_t { return S()->m_FramesNum ? U(oracle::Fn<void* __fastcall(void*, int, uint32_t)>(0x4CF220)(S(), 0, n % S()->m_FramesNum)) : 0; }, &BuildSeqC);
    }, 200);
    Run("CAnimBlendSequence::GetDataSize 0x4D0C90", [&](Rng& r, std::string& d) {
        const bool c = r.below(2);
        return ACase(r, d, [&]() -> uint64_t { return S()->GetDataSize(c); }, [&]() -> uint64_t { return (uint32_t)oracle::Fn<uint32_t __fastcall(void*, int, int)>(0x4D0C90)(S(), 0, c); }, &BuildSeqAny);
    }, 200);
    Run("CAnimBlendSequence::SetBoneTag 0x4D0C70", [&](Rng& r, std::string& d) {
        static const int32_t tags[] = { -1, 0, 1, 2, 5, 8, 25, 26, 31, 32, 33, 300, 301, 1000 };   // the port compares in the 16-bit eBoneTag (65535 == BONE_UNKNOWN there); no real tag is that large
        const int32_t t = tags[r.below((int)std::size(tags))];
        return ACase(r, d, [&]() -> uint64_t { S()->SetBoneTag(eBoneTag32{ (eBoneTag)t }); return 0; }, [&]() -> uint64_t { oracle::Fn<void __fastcall(void*, int, int)>(0x4D0C70)(S(), 0, t); return 0; }, &BuildSeqAny);
    }, 300);
    Run("CAnimBlendSequence::Constructor 0x4D0C10", [&](Rng& r, std::string& d) {
        return ACase(r, d, [&]() -> uint64_t { ::new (static_cast<void*>(P(kObj))) CAnimBlendSequence(); return 0; }, [&]() -> uint64_t { oracle::Fn<void* __fastcall(void*, int)>(0x4D0C10)(P(kObj), 0); return 0; }, &BuildGarbage, ~size_t(0), 4, true);
    }, 100);
    Run("CAnimBlendSequence::Print 0x4D1180", [&](Rng& r, std::string& d) {
        return ACase(r, d, [&]() -> uint64_t { S()->Print(); return 0; }, [&]() -> uint64_t { oracle::Fn<void __fastcall(void*, int)>(0x4D1180)(S(), 0); return 0; }, &BuildSeqAny);
    }, 20);
    Run("CAnimBlendSequence::RemoveQuaternionFlips 0x4D1190", [&](Rng& r, std::string& d) {
        return ACase(r, d, [&]() -> uint64_t { S()->RemoveQuaternionFlips(); return 0; }, [&]() -> uint64_t { oracle::Fn<void __fastcall(void*, int)>(0x4D1190)(S(), 0); return 0; }, &BuildSeqU);
    }, 600);
    Run("CAnimBlendSequence::MoveMemory 0x4D1150", [&](Rng& r, std::string& d) {
        return ACase(r, d, [&]() -> uint64_t { return S()->MoveMemory() ? 1 : 0; }, [&]() -> uint64_t { return (uint8_t)oracle::Fn<int __fastcall(void*, int)>(0x4D1150)(S(), 0) ? 1 : 0; }, &BuildSeqAny);
    }, 100);
    // ---- CAnimBlendHierarchy
    auto H = [] { return reinterpret_cast<CAnimBlendHierarchy*>(P(kHierObj)); };
    Run("CAnimBlendHierarchy::CalcTotalTime 0x4CF2F0", [&](Rng& r, std::string& d) {
        return ACase(r, d, [&]() -> uint64_t { H()->CalcTotalTime(); return 0; }, [&]() -> uint64_t { oracle::Fn<void __fastcall(void*, int)>(0x4CF2F0)(H(), 0); return 0; }, &BuildHierU);
    }, 800);
    Run("CAnimBlendHierarchy::CalcTotalTimeCompressed 0x4CF3E0", [&](Rng& r, std::string& d) {
        return ACase(r, d, [&]() -> uint64_t { H()->CalcTotalTimeCompressed(); return 0; }, [&]() -> uint64_t { oracle::Fn<void __fastcall(void*, int)>(0x4CF3E0)(H(), 0); return 0; }, &BuildHierC);
    }, 800);
    Run("CAnimBlendHierarchy::RemoveQuaternionFlips 0x4CF4E0", [&](Rng& r, std::string& d) {
        return ACase(r, d, [&]() -> uint64_t { H()->RemoveQuaternionFlips(); return 0; }, [&]() -> uint64_t { oracle::Fn<void __fastcall(void*, int)>(0x4CF4E0)(H(), 0); return 0; }, &BuildHierU);
    }, 400);
    Run("CAnimBlendHierarchy::GetSequenceBlock 0x4CF560", [&](Rng& r, std::string& d) {
        return ACase(r, d, [&]() -> uint64_t { return H()->m_pSequences ? U(H()->GetSequenceBlock()) : 0; }, [&]() -> uint64_t { return H()->m_pSequences ? U(oracle::Fn<void* __fastcall(void*, int)>(0x4CF560)(H(), 0)) : 0; },
            [](Rng& rr) { BuildHierU(rr); auto* h = reinterpret_cast<CAnimBlendHierarchy*>(P(kHierObj)); if (rr.below(2) && h->m_pSequences) h->m_pSequences[0].m_bUsingExternalMemory = true; });
    }, 200);
    Run("CAnimBlendHierarchy::MoveMemory 0x4CF800", [&](Rng& r, std::string& d) {
        return ACase(r, d, [&]() -> uint64_t { return H()->MoveMemory() ? 1 : 0; }, [&]() -> uint64_t { return (uint8_t)oracle::Fn<int __fastcall(void*, int)>(0x4CF800)(H(), 0) ? 1 : 0; }, &BuildHierU);
    }, 100);
    Run("CAnimBlendHierarchy::Constructor 0x4CF270", [&](Rng& r, std::string& d) {
        return ACase(r, d, [&]() -> uint64_t { ::new (static_cast<void*>(P(kObj))) CAnimBlendHierarchy(); return 0; }, [&]() -> uint64_t { oracle::Fn<void* __fastcall(void*, int)>(0x4CF270)(P(kObj), 0); return 0; }, &BuildGarbage, ~size_t(0), 4, true);
    }, 100);
    // ---- CAnimManager statics
    Run("CAnimManager::GetAnimGroupName 0x4D3A20", [&](Rng& r, std::string& d) {
        const int id = r.below(NUM_ANIM_ASSOC_GROUPS_CAPACITY);
        return ACase(r, d, [&]() -> uint64_t { return U(CAnimManager::GetAnimGroupName((AssocGroupId)id)); }, [&]() -> uint64_t { return U(oracle::Fn<const char* __cdecl(int)>(0x4D3A20)(id)); }, &BuildGarbage);
    }, 100);
    Run("CAnimManager::GetAnimBlockName 0x4D3A30", [&](Rng& r, std::string& d) {
        const int id = r.below(NUM_ANIM_ASSOC_GROUPS_CAPACITY);
        return ACase(r, d, [&]() -> uint64_t { return U(CAnimManager::GetAnimBlockName((AssocGroupId)id)); }, [&]() -> uint64_t { return U(oracle::Fn<const char* __cdecl(int)>(0x4D3A30)(id)); }, &BuildGarbage);
    }, 100);
    Run("CAnimManager::AddAnimToAssocDefinition 0x4D3C80", [&](Rng& r, std::string& d) {
        char name[32]; const int len = 1 + r.below(22); for (int i = 0; i < len; ++i) name[i] = (char)('A' + r.below(26)); name[len] = 0; g_nameLen = (size_t)len;
        return ACase(r, d, [&]() -> uint64_t { CAnimManager::AddAnimToAssocDefinition(reinterpret_cast<AnimAssocDefinition*>(P(kDef)), name); return 0; },
            [&]() -> uint64_t { oracle::Fn<void __cdecl(void*, const char*)>(0x4D3C80)(P(kDef), name); return 0; }, &BuildDef, ~size_t(0), 4, false,
            []() -> std::pair<size_t, size_t> { return { kObj + (size_t)g_f.pick * 24 + g_nameLen + 1, 24 - g_nameLen - 1 }; });   // the filled slot's tail (the debug CRT's strcpy_s fills the rest of the buffer with 0xFE, the exe does not)
    }, 300);
    Run("CAnimManager::AddAnimBlockRef 0x4D3FB0", [&](Rng& r, std::string& d) {
        const int idx = r.below(NUM_ANIM_BLOCKS);
        return ACase(r, d, [&]() -> uint64_t { CAnimManager::AddAnimBlockRef(idx); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl(int)>(0x4D3FB0)(idx); return 0; }, &BuildGarbage);
    }, 200);
    Run("CAnimManager::RemoveAnimBlockRefWithoutDelete 0x4D3FF0", [&](Rng& r, std::string& d) {
        const int idx = r.below(NUM_ANIM_BLOCKS);
        return ACase(r, d, [&]() -> uint64_t { CAnimManager::RemoveAnimBlockRefWithoutDelete(idx); return 0; }, [&]() -> uint64_t { oracle::Fn<void __cdecl(int)>(0x4D3FF0)(idx); return 0; }, &BuildGarbage);
    }, 200);
    Run("CAnimManager::GetNumRefsToAnimBlock 0x4D4010", [&](Rng& r, std::string& d) {
        const int idx = r.below(NUM_ANIM_BLOCKS);
        return ACase(r, d, [&]() -> uint64_t { return (uint32_t)CAnimManager::GetNumRefsToAnimBlock(idx); }, [&]() -> uint64_t { return (uint32_t)oracle::Fn<int __cdecl(int)>(0x4D4010)(idx); }, &BuildGarbage);
    }, 200);
    Run("CAnimManager::GetAnimation-0 0x4D39F0", [&](Rng& r, std::string& d) {
        static const uint32_t hs[] = { 0x11111111u, 0x22222222u, 0x33333333u, 0xDEADBEEFu, 0 };
        const uint32_t h = hs[r.below(5)];
        return ACase(r, d, [&]() -> uint64_t { return U(CAnimManager::GetAnimation(h, reinterpret_cast<const CAnimBlock*>(P(kBlk)))); },
            [&]() -> uint64_t { return U(oracle::Fn<void* __cdecl(uint32_t, void*)>(0x4D39F0)(h, P(kBlk))); }, &BuildBlock);
    }, 400);
    Run("CAnimManager::RemoveFromUncompressedCache 0x4D42A0", [&](Rng& r, std::string& d) {
        return ACase(r, d, [&]() -> uint64_t { CAnimManager::RemoveFromUncompressedCache(reinterpret_cast<CAnimBlendHierarchy*>(P(kHierObj))); return 0; },
            [&]() -> uint64_t { oracle::Fn<void __cdecl(void*)>(0x4D42A0)(P(kHierObj)); return 0; }, &BuildCache);
    }, 300);
}

static void TestW3Anim() {
    TestW3AnimClump();
    TestW3AnimClasses();
}

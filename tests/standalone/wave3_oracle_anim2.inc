// ORACLE-W3-ANIM2: CAnimBlendAssociation SetCurrentTime / Start / SyncAnimation / UpdateTime / UpdateBlend / Init x3 / constructors / node arrays, CAnimBlendStaticAssociation, association list deletion
// (RemoveAssociations / RemoveAllAssociations / GiveAssociations), FindBone / FindFrame / FindFrameFromHashKey, CAnimBlendClumpData::SetNumberOfBones / ForAllFrames, InitEmptyAssociations.
// Included by wave3_oracle_anim.inc (same fixture g_fx, same harness). The associations that are deleted live in the deterministic arena (cloned by the case on each side with that side's vptr:
// exe 0x85C6D0 / the port's own), so `delete` is legal and the destructors run for real.
// RwHelper.cpp is not part of this target; the fixture clump has no atomics (empty lists), so GetFirstAtomic (0x734820) is null on both sides and IsClumpSkinned is false
RpAtomic* GetFirstAtomic(RpClump*) { return nullptr; }
namespace w3a {
constexpr size_t kNodes = 0x6100, kSeqs = 0x6600, kFrames2 = 0x6D00, kSeqPtrs = 0x5900;
constexpr uintptr_t kExeAssocVptr = 0x85C6D0, kExeDefaultCb = 0x4D1390;
static void CbStub(CAnimBlendAssociation* a, void* d) { auto* p = static_cast<uint32_t*>(d); p[0]++; p[1] = (uint32_t)(uintptr_t)a; }
static void FrameCb(AnimBlendFrameData* f, void* d) { auto* p = static_cast<uint32_t*>(d); p[0]++; p[1] = (uint32_t)(uintptr_t)f; p[2] += f->BoneTag; }
static std::vector<CAnimBlendAssociation*> g_clones;
static void NormClones() { for (auto* a : g_clones) *reinterpret_cast<uintptr_t*>(a) = 0xDEADBEEFu; }   // the two sides' vptrs differ by design
static uintptr_t PortAssocVptr() { static CAnimBlendAssociation proto; return *reinterpret_cast<uintptr_t*>(&proto); }
static void NormCb(CAnimBlendAssociation* a) {   // the default callback is a different address in the port and in the exe
    const auto f = (uintptr_t)a->m_pCallbackFunc;
    if (f == (uintptr_t)&CDefaultAnimCallback::DefaultAnimCB || f == kExeDefaultCb) a->m_pCallbackFunc = reinterpret_cast<void (*)(CAnimBlendAssociation*, void*)>(0xC0DEC0DEu);
}
static const char* const kNames[] = { "Root", "Pelvis", "Spine", "Spine1", "Neck", "Head", "L Clavicle", "R UpperArm" };
static void SetupFrames(Rng& r) {   // frame datas of the fixture clump: bone tags 0..7 (duplicates allowed), a fake RwFrame with a node name each
    StaticRef<int32_t>(0xC87C5C) = 0x40;
    const uint32_t n = g_f.cd->m_NumFrameData;
    for (uint32_t j = 0; j < n; ++j) {
        auto* fd = &g_f.cd->m_FrameDatas[j];
        fd->Flags = (uint8_t)r.u32(); fd->BoneTag = r.below(8);
        fd->Frame = reinterpret_cast<RwFrame*>(P(kFrames2 + j * 0x80));
        const char* nm = kNames[r.below(8)];
        std::strncpy(reinterpret_cast<char*>(P(kFrames2 + j * 0x80 + 0x40)), nm, 23);
    }
}
static void FixDeltas(CAnimBlendSequence& s) {   // positive time stamps: the looped FindKeyFrame search ends
    if (s.m_bIsCompressed) return;
    for (uint32_t i = 0; i < s.m_FramesNum; ++i) { auto* k = s.GetUKeyFrame(i); if (!(k->DeltaTime >= 0.05f)) k->DeltaTime = 0.05f + 0.01f * (float)i; }
}
// clump with associations + hierarchies with positive total time, uncompressed ones linked into the uncompressed-animation cache, nodes with sequences
static void BuildClumpT(Rng& r) {
    BuildClump(r);
    g_clones.clear(); g_clones.reserve(64);
    for (size_t o = 8; o <= 0x18; o += 8) { At<void*>(kClump + o) = P(kClump + o); At<void*>(kClump + o + 4) = P(kClump + o); }   // empty atomic / light / camera lists (GetFirstAtomic -> not skinned)
    auto& c = *reinterpret_cast<CLinkList<CAnimBlendHierarchy*>*>(0xB5EB20);
    c.usedListHead.next = &c.usedListTail; c.usedListTail.prev = &c.usedListHead; c.usedListHead.prev = nullptr; c.usedListTail.next = nullptr;
    c.freeListHead.next = &c.freeListTail; c.freeListTail.prev = &c.freeListHead; c.freeListHead.prev = nullptr; c.freeListTail.next = nullptr;
    auto* ln = reinterpret_cast<CLink<CAnimBlendHierarchy*>*>(P(kCache));
    for (int i = 0; i < 6; ++i) { ln[i].data = nullptr; ln[i].Insert(r.below(3) ? &c.usedListHead : &c.freeListHead); }
    c.links = ln;
    bool zeroTotal = false;
    for (int i = 0; i < 4; ++i) {
        auto* h = g_f.hier[i];
        h->m_bIsCompressed = h->m_bKeepCompressed ? r.below(2) : 0;
        h->m_fTotalTime = (h->m_bKeepCompressed && r.below(4) == 0) ? 0.f : 0.2f + r.f01() * 3.f;
        zeroTotal |= h->m_fTotalTime == 0.f;
        h->m_Link = nullptr;
        if (!h->m_bKeepCompressed && r.below(2)) { h->m_Link = &ln[i]; ln[i].data = h; }
        h->m_nSeqCount = 0; h->m_pSequences = nullptr;
    }
    SetupFrames(r);
    const uint32_t nFr = g_f.cd->m_NumFrameData;
    bool built[24] = {};
    for (int i = 0; i < g_f.nAs; ++i) {
        auto* a = g_f.as[i];
        int hi = 0; if (!a->m_BlendHier) a->m_BlendHier = g_f.hier[0];
        for (int k = 0; k < 4; ++k) if (g_f.hier[k] == a->m_BlendHier) hi = k;
        if (zeroTotal) a->m_Flags &= ~ANIMATION_IS_LOOPED;
        a->m_NumBlendNodes = (uint16_t)(r.below(4) == 0 ? 0 : nFr);
        auto* nodes = reinterpret_cast<CAnimBlendNode*>(P(kNodes + i * 0x90));
        a->m_BlendNodes = a->m_NumBlendNodes ? nodes : nullptr;
        for (uint32_t j = 0; j < a->m_NumBlendNodes; ++j) {
            nodes[j].Init(); nodes[j].m_BlendAssoc = a;
            if (r.below(5) == 0) continue;
            const size_t so = kSeqs + (hi * 6 + j) * sizeof(CAnimBlendSequence);
            if (!built[hi * 6 + j]) { built[hi * 6 + j] = true; BuildSeq(r, so, 1 + r.below(6), r.below(2), g_f.hier[hi]->m_bKeepCompressed); FixDeltas(*reinterpret_cast<CAnimBlendSequence*>(P(so))); }
            nodes[j].m_Seq = reinterpret_cast<CAnimBlendSequence*>(P(so));
        }
        a->m_nCallbackType = (eAnimBlendCallbackType)r.below(3); a->m_pCallbackFunc = a->m_nCallbackType ? &CbStub : nullptr; a->m_pCallbackData = P(kOut);
    }
}
// the clone of association i in the arena (legal to `delete`), relinked into the list; exe: that side's vptr; nodes: a node array of that side's allocator or none
static CAnimBlendAssociation* ArenaMove(int i, bool exe, bool nodes) {
    auto* src = reinterpret_cast<CAnimBlendAssociation*>(P(kAs + i * kAsSz));   // the fixture original (g_f.as[] is rewritten by the first side)
    auto* a = static_cast<CAnimBlendAssociation*>(oracle::alloc::Alloc(sizeof(CAnimBlendAssociation)));
    std::memcpy(static_cast<void*>(a), src, sizeof(CAnimBlendAssociation));
    *reinterpret_cast<uintptr_t*>(a) = exe ? kExeAssocVptr : PortAssocVptr();
    if (a->m_Link.prev) a->m_Link.prev->next = &a->m_Link;
    if (a->m_Link.next) a->m_Link.next->prev = &a->m_Link;
    a->m_BlendNodes = nullptr;
    if (nodes && a->m_NumBlendNodes) {
        if (exe) oracle::Fn<void __fastcall(void*, int, int)>(0x4CE9F0)(a, 0, (int)(int16_t)a->m_NumBlendNodes); else a->AllocateAnimBlendNodeArray(a->m_NumBlendNodes);
    }
    g_f.as[i] = a; g_clones.push_back(a);
    return a;
}
// all associations of the fixture list moved into the arena, the list rebuilt in order; returns a fresh unlinked association of the same side
static CAnimBlendAssociation* ArenaMoveAll(bool exe, bool nodes) {
    g_clones.clear(); g_clones.reserve(64);
    for (int i = 0; i < g_f.nAs; ++i) ArenaMove(i, exe, nodes);
    CAnimBlendLink* prev = &g_f.cd->m_AnimList;
    for (int i = 0; i < g_f.nAs; ++i) { prev->next = &g_f.as[i]->m_Link; g_f.as[i]->m_Link.prev = prev; g_f.as[i]->m_Link.next = nullptr; prev = &g_f.as[i]->m_Link; }
    auto* n = static_cast<CAnimBlendAssociation*>(oracle::alloc::Alloc(sizeof(CAnimBlendAssociation)));
    std::memset(static_cast<void*>(n), 0, sizeof *n); *reinterpret_cast<uintptr_t*>(n) = exe ? kExeAssocVptr : PortAssocVptr(); g_clones.push_back(n);
    return n;
}
static void BuildClumpDel(Rng& r) {
    BuildClumpT(r);
    for (int i = 0; i < g_f.nAs; ++i) {
        auto* a = g_f.as[i];
        if (r.below(2)) a->m_Flags = (uint16_t)((a->m_Flags & ~ANIMATION_IS_PARTIAL) | (r.below(2) ? ANIMATION_IS_PARTIAL : 0));
        if (r.below(3) == 0) a->m_Flags |= ANIMATION_REFERENCE_BLOCK; else a->m_Flags &= ~ANIMATION_REFERENCE_BLOCK;
    }
}
static void BuildClumpBlend(Rng& r) {
    BuildClumpT(r);
    if (!g_f.nAs) return;
    auto* a = g_f.as[g_f.pick];
    if (r.below(2)) a->m_BlendAmount = r.below(2) ? 0.f : r.f01() * 0.1f;
    if (r.below(4)) a->m_BlendDelta = -(r.f01() * 5.f);
    if (r.below(2)) a->m_Flags |= ANIMATION_IS_BLEND_AUTO_REMOVE; else a->m_Flags &= ~ANIMATION_IS_BLEND_AUTO_REMOVE;
    if (r.below(3) == 0) a->m_Flags |= ANIMATION_REFERENCE_BLOCK; else a->m_Flags &= ~ANIMATION_REFERENCE_BLOCK;
}
// Init-0 / Constructor1 / static Init: clump frames + a hierarchy (kHierObj) whose sequences name bones of the clump (or hash keys of their node names)
static void BuildInit(Rng& r) {
    BuildClumpT(r);
    const int nSeq = r.below(6) == 0 ? 0 : 1 + r.below(5);
    BuildHierObj(r, kHierObj, kSeqArr, nSeq, false);
    auto* h = reinterpret_cast<CAnimBlendHierarchy*>(P(kHierObj));
    h->m_nAnimBlockId = (int32_t)r.below(6);
    for (int i = 0; i < nSeq; ++i) {
        auto* s = reinterpret_cast<CAnimBlendSequence*>(P(kSeqArr + i * sizeof(CAnimBlendSequence)));
        s->m_IsUsingBoneTag = r.below(2);
        const uint32_t v = s->m_IsUsingBoneTag ? (uint32_t)r.below(8) : (r.below(3) ? CKeyGen::GetUppercaseKey(kNames[r.below(8)]) : r.u32());
        std::memcpy(s, &v, 4);
    }
}
// a static association at kObj+0x100 with a sequence pointer array (kSeqPtrs) into the clump's node sequences
static void StatSrc(Rng& r) {
    auto* sa = reinterpret_cast<CAnimBlendStaticAssociation*>(P(kObj + 0x100));
    sa->m_NumBlendNodes = (uint16_t)g_f.cd->m_NumFrameData; sa->m_AnimId = (AnimationId)r.below(12); sa->m_AnimGroupId = (AssocGroupId)r.below(10);
    sa->m_Flags = (uint16_t)((r.u32() & ~ANIMATION_REFERENCE_BLOCK) | (r.below(2) ? ANIMATION_REFERENCE_BLOCK : 0)); sa->m_BlendHier = g_f.hier[r.below(4)];
    auto** arr = reinterpret_cast<CAnimBlendSequence**>(P(kSeqPtrs));
    sa->m_BlendSeqs = arr;
    for (uint32_t j = 0; j < 6; ++j) arr[j] = r.below(3) ? reinterpret_cast<CAnimBlendSequence*>(P(kSeqs + (r.below(24)) * sizeof(CAnimBlendSequence))) : nullptr;
}
static void BuildClumpStat(Rng& r) { BuildClumpT(r); StatSrc(r); }
static void BuildGroupInit(Rng& rr) {   // 1..4 static associations (some with a sequence array already, some empty) of a group, one hierarchy with sequences
    BuildClumpT(rr);
    auto* grp = reinterpret_cast<CAnimBlendAssocGroup*>(P(kGroups));
    std::memset(static_cast<void*>(grp), 0, sizeof *grp);
    grp->m_Anims = reinterpret_cast<CAnimBlendStaticAssociation*>(P(kObj)); grp->m_NumAnims = 1 + rr.below(4);
    for (uint32_t i = 0; i < grp->m_NumAnims; ++i) {
        auto* sa = &grp->m_Anims[i]; std::memset(static_cast<void*>(sa), 0, sizeof *sa);
        sa->m_BlendHier = reinterpret_cast<CAnimBlendHierarchy*>(P(kHierObj)); if (rr.below(3) == 0) sa->m_BlendSeqs = reinterpret_cast<CAnimBlendSequence**>(P(kSeqPtrs));
    }
    BuildHierObj(rr, kHierObj, kSeqArr, 1 + rr.below(4), false);
}
} // namespace w3a

static void TestW3Anim2() {
    using namespace w2; using namespace w3a;
    auto AP = [&]() -> CAnimBlendAssociation* { return g_f.nAs ? g_f.as[g_f.pick] : nullptr; };
    PortAssocVptr(); g_clones.reserve(64);
    oracle::Patch(0x8229B6, (void*)+[](const char* a, const char* b) -> int { return _stricmp(a, b); });   // the exe CRT _stricmp (RpAnimBlendClumpFindFrame) needs its locale lock
    oracle::Patch(0x823A0E, (void*)+[](int c) -> int { return (c >= 'a' && c <= 'z') ? c - 32 : c; });   // the exe CRT toupper (CKeyGen::GetUppercaseKey) needs the locale lock: "C" locale
    Run("CAnimBlendAssociation::SetCurrentTime 0x4CEA80", [&](Rng& r, std::string& d) {
        const float t = r.below(5) ? r.f01() * 8.f : -r.f01();
        return ACase(r, d, [&]() -> uint64_t { if (auto* x = AP()) x->SetCurrentTime(t); return 0; }, [&]() -> uint64_t { if (auto* x = AP()) oracle::Fn<void __fastcall(void*, int, float)>(0x4CEA80)(x, 0, t); return 0; }, &BuildClumpT);
    }, 800);
    Run("CAnimBlendAssociation::Start 0x4CEB70", [&](Rng& r, std::string& d) {
        const float t = r.below(5) ? r.f01() * 8.f : 0.f;
        return ACase(r, d, [&]() -> uint64_t { if (auto* x = AP()) x->Start(t); return 0; }, [&]() -> uint64_t { if (auto* x = AP()) oracle::Fn<void __fastcall(void*, int, float)>(0x4CEB70)(x, 0, t); return 0; }, &BuildClumpT);
    }, 500);
    Run("CAnimBlendAssociation::SyncAnimation 0x4CEB40", [&](Rng& r, std::string& d) {
        const int o = r.below(8);
        auto O = [&]() -> CAnimBlendAssociation* { return g_f.nAs ? g_f.as[o % g_f.nAs] : nullptr; };
        return ACase(r, d, [&]() -> uint64_t { if (auto* x = AP()) x->SyncAnimation(O()); return 0; }, [&]() -> uint64_t { if (auto* x = AP()) oracle::Fn<void __fastcall(void*, int, void*)>(0x4CEB40)(x, 0, O()); return 0; }, &BuildClumpT);
    }, 500);
    Run("CAnimBlendAssociation::UpdateTime 0x4D13D0", [&](Rng& r, std::string& d) {
        const float a = r.f01(), b = r.f01() * 3.f;
        return ACase(r, d, [&]() -> uint64_t { auto* x = AP(); if (!x) return 0; const bool ok = x->UpdateTime(a, b); NormCb(x); return ok; },
            [&]() -> uint64_t { auto* x = AP(); if (!x) return 0; const bool ok = (uint8_t)oracle::Fn<int __fastcall(void*, int, float, float)>(0x4D13D0)(x, 0, a, b) != 0; NormCb(x); return ok; }, &BuildClumpT);
    }, 800);
    Run("CAnimBlendAssociation::UpdateBlend 0x4D1490", [&](Rng& r, std::string& d) {
        const float ts = r.below(4) ? r.f01() * 2.f : (r.f01() - 0.5f) * 4.f; const bool nodes = r.below(2);
        return ACase(r, d, [&]() -> uint64_t { if (!g_f.nAs) return 0; auto* x = ArenaMove(g_f.pick, false, nodes); const bool ok = x->UpdateBlend(ts); NormCb(x); NormClones(); return ok; },
            [&]() -> uint64_t { if (!g_f.nAs) return 0; auto* x = ArenaMove(g_f.pick, true, nodes); const bool ok = (uint8_t)oracle::Fn<int __fastcall(void*, int, float)>(0x4D1490)(x, 0, ts) != 0; NormCb(x); NormClones(); return ok; }, &BuildClumpBlend);
    }, 800);
    Run("CAnimBlendAssociation::AllocateAnimBlendNodeArray 0x4CE9F0", [&](Rng& r, std::string& d) {
        const int n = 1 + r.below(8);
        return ACase(r, d, [&]() -> uint64_t { if (auto* x = AP()) x->AllocateAnimBlendNodeArray(n); return 0; }, [&]() -> uint64_t { if (auto* x = AP()) oracle::Fn<void __fastcall(void*, int, int)>(0x4CE9F0)(x, 0, n); return 0; }, &BuildClumpT);
    }, 100);
    Run("CAnimBlendAssociation::FreeAnimBlendNodeArray 0x4CEA40", [&](Rng& r, std::string& d) {
        const int n = 1 + r.below(8); const bool pre = r.below(4) != 0;
        return ACase(r, d, [&]() -> uint64_t { auto* x = AP(); if (!x) return 0; x->m_BlendNodes = nullptr; if (pre) x->AllocateAnimBlendNodeArray(n); x->FreeAnimBlendNodeArray(); return 0; },
            [&]() -> uint64_t { auto* x = AP(); if (!x) return 0; x->m_BlendNodes = nullptr; if (pre) oracle::Fn<void __fastcall(void*, int, int)>(0x4CE9F0)(x, 0, n); oracle::Fn<void __fastcall(void*, int)>(0x4CEA40)(x, 0); return 0; }, &BuildClumpT);
    }, 100);
    // ---- Init / constructors
    Run("CAnimBlendAssociation::Init-clump_hier 0x4CED50", [&](Rng& r, std::string& d) {
        g_maskImg = true;
        return ACase(r, d, [&]() -> uint64_t { reinterpret_cast<CAnimBlendAssociation*>(P(kObj))->Init(g_f.clump, reinterpret_cast<CAnimBlendHierarchy*>(P(kHierObj))); return 0; },
            [&]() -> uint64_t { oracle::Fn<void __fastcall(void*, int, void*, void*)>(0x4CED50)(P(kObj), 0, g_f.clump, P(kHierObj)); return 0; }, &BuildInit);
    }, 500);
    Run("CAnimBlendAssociation::Init-assoc 0x4CEE40", [&](Rng& r, std::string& d) {
        return ACase(r, d, [&]() -> uint64_t { if (!AP()) return 0; reinterpret_cast<CAnimBlendAssociation*>(P(kObj))->Init(*AP()); return 0; },
            [&]() -> uint64_t { if (!AP()) return 0; oracle::Fn<void __fastcall(void*, int, void*)>(0x4CEE40)(P(kObj), 0, AP()); return 0; }, &BuildClumpT);
    }, 300);
    Run("CAnimBlendAssociation::Init-static 0x4CEEC0", [&](Rng& r, std::string& d) {
        return ACase(r, d, [&]() -> uint64_t { reinterpret_cast<CAnimBlendAssociation*>(P(kObj))->Init(*reinterpret_cast<CAnimBlendStaticAssociation*>(P(kObj + 0x100))); return 0; },
            [&]() -> uint64_t { oracle::Fn<void __fastcall(void*, int, void*)>(0x4CEEC0)(P(kObj), 0, P(kObj + 0x100)); return 0; }, &BuildClumpStat);
    }, 300);
    Run("CAnimBlendAssociation::Constructor1-clump_hier 0x4CEFC0", [&](Rng& r, std::string& d) {
        g_maskImg = true;
        return ACase(r, d, [&]() -> uint64_t { ::new (static_cast<void*>(P(kObj))) CAnimBlendAssociation(g_f.clump, reinterpret_cast<CAnimBlendHierarchy*>(P(kHierObj))); return 0; },
            [&]() -> uint64_t { oracle::Fn<void* __fastcall(void*, int, void*, void*)>(0x4CEFC0)(P(kObj), 0, g_f.clump, P(kHierObj)); return 0; }, &BuildInit, kObj, 4, true);
    }, 400);
    Run("CAnimBlendAssociation::Constructor2-blend 0x4CF020", [&](Rng& r, std::string& d) {
        return ACase(r, d, [&]() -> uint64_t { if (!AP()) return 0; ::new (static_cast<void*>(P(kObj))) CAnimBlendAssociation(*AP()); return 0; },
            [&]() -> uint64_t { if (!AP()) return 0; oracle::Fn<void* __fastcall(void*, int, void*)>(0x4CF020)(P(kObj), 0, AP()); return 0; }, &BuildClumpT, kObj, 4, true);
    }, 300);
    Run("CAnimBlendAssociation::Constructor3-static_blend 0x4CF080", [&](Rng& r, std::string& d) {
        return ACase(r, d, [&]() -> uint64_t { ::new (static_cast<void*>(P(kObj))) CAnimBlendAssociation(*reinterpret_cast<CAnimBlendStaticAssociation*>(P(kObj + 0x100))); return 0; },
            [&]() -> uint64_t { oracle::Fn<void* __fastcall(void*, int, void*)>(0x4CF080)(P(kObj), 0, P(kObj + 0x100)); return 0; }, &BuildClumpStat, kObj, 4, true);
    }, 300);
    Run("CAnimBlendStaticAssociation::Init 0x4CEC20", [&](Rng& r, std::string& d) {
        g_maskImg = true;
        const bool nullClump = r.below(8) == 0;
        return ACase(r, d, [&]() -> uint64_t { reinterpret_cast<CAnimBlendStaticAssociation*>(P(kObj + 0x100))->Init(nullClump ? nullptr : g_f.clump, reinterpret_cast<CAnimBlendHierarchy*>(P(kHierObj))); return 0; },
            [&]() -> uint64_t { oracle::Fn<void __fastcall(void*, int, void*, void*)>(0x4CEC20)(P(kObj + 0x100), 0, nullClump ? nullptr : g_f.clump, P(kHierObj)); return 0; }, &BuildInit);
    }, 500);
    Run("CAnimBlendStaticAssociation::Constructor2 0x4CEF60", [&](Rng& r, std::string& d) {
        g_maskImg = true;
        return ACase(r, d, [&]() -> uint64_t { ::new (static_cast<void*>(P(kObj + 0x100))) CAnimBlendStaticAssociation(g_f.clump, reinterpret_cast<CAnimBlendHierarchy*>(P(kHierObj))); return 0; },
            [&]() -> uint64_t { oracle::Fn<void* __fastcall(void*, int, void*, void*)>(0x4CEF60)(P(kObj + 0x100), 0, g_f.clump, P(kHierObj)); return 0; }, &BuildInit, kObj + 0x100, 4, true);
    }, 300);
    Run("CAnimBlendStaticAssociation::AllocateSequenceArray 0x4CE960", [&](Rng& r, std::string& d) {
        const int n = 1 + r.below(10);
        return ACase(r, d, [&]() -> uint64_t { reinterpret_cast<CAnimBlendStaticAssociation*>(P(kObj + 0x100))->AllocateSequenceArray(n); return 0; },
            [&]() -> uint64_t { oracle::Fn<void __fastcall(void*, int, int)>(0x4CE960)(P(kObj + 0x100), 0, n); return 0; }, &BuildClumpT);
    }, 100);
    // ---- association list deletion (the destructors run)
    Run("RpAnimBlendClumpRemoveAssociations 0x4D6820", [&](Rng& r, std::string& d) {
        const uint32_t fl = FlagArg(r); const bool nodes = r.below(2);
        return ACase(r, d, [&]() -> uint64_t { ArenaMoveAll(false, nodes); RpAnimBlendClumpRemoveAssociations(g_f.clump, fl); NormClones(); return 0; },
            [&]() -> uint64_t { ArenaMoveAll(true, nodes); oracle::Fn<void __cdecl(void*, uint32_t)>(0x4D6820)(g_f.clump, fl); NormClones(); return 0; }, &BuildClumpDel);
    }, 400);
    Run("RpAnimBlendClumpRemoveAllAssociations 0x4D6C00", [&](Rng& r, std::string& d) {
        const bool nodes = r.below(2);
        return ACase(r, d, [&]() -> uint64_t { ArenaMoveAll(false, nodes); RpAnimBlendClumpRemoveAllAssociations(g_f.clump); NormClones(); return 0; },
            [&]() -> uint64_t { ArenaMoveAll(true, nodes); oracle::Fn<void __cdecl(void*)>(0x4D6C00)(g_f.clump); NormClones(); return 0; }, &BuildClumpDel);
    }, 300);
    Run("RpAnimBlendClumpGiveAssociations 0x4D6C30", [&](Rng& r, std::string& d) {
        g_maskImg = true;
        const bool nodes = r.below(2);
        return ACase(r, d, [&]() -> uint64_t { auto* n = ArenaMoveAll(false, nodes); RpAnimBlendClumpGiveAssociations(g_f.clump, n); NormClones(); return U(n); },
            [&]() -> uint64_t { auto* n = ArenaMoveAll(true, nodes); oracle::Fn<void __cdecl(void*, void*)>(0x4D6C30)(g_f.clump, n); NormClones(); return U(n); }, &BuildClumpDel);
    }, 300);
    // ---- frame lookups
    Run("RpAnimBlendClumpFindBone 0x4D6400", [&](Rng& r, std::string& d) {
        g_maskImg = true;
        const int32_t tag = (int32_t)r.below(10);
        return ACase(r, d, [&]() -> uint64_t { return U(RpAnimBlendClumpFindBone(g_f.clump, eBoneTag32{ (eBoneTag)tag })); }, [&]() -> uint64_t { return U(oracle::Fn<void* __cdecl(void*, int)>(0x4D6400)(g_f.clump, tag)); }, &BuildClumpT);
    }, 300);
    Run("RpAnimBlendClumpFindFrameFromHashKey 0x4D6370", [&](Rng& r, std::string& d) {
        g_maskImg = true;
        const uint32_t key = r.below(4) ? CKeyGen::GetUppercaseKey(kNames[r.below(8)]) : r.u32();
        return ACase(r, d, [&]() -> uint64_t { return U(RpAnimBlendClumpFindFrameFromHashKey(g_f.clump, key)); }, [&]() -> uint64_t { return U(oracle::Fn<void* __cdecl(void*, uint32_t)>(0x4D6370)(g_f.clump, key)); }, &BuildClumpT);
    }, 300);
    Run("RpAnimBlendClumpFindFrame 0x4D62A0", [&](Rng& r, std::string& d) {
        g_maskImg = true;
        char nm[24]; std::strcpy(nm, kNames[r.below(8)]); if (r.below(2)) for (char* c = nm; *c; ++c) *c = (char)std::toupper((unsigned char)*c); if (r.below(8) == 0) std::strcpy(nm, "Nothing");
        return ACase(r, d, [&]() -> uint64_t { return U(RpAnimBlendClumpFindFrame(g_f.clump, nm)); }, [&]() -> uint64_t { return U(oracle::Fn<void* __cdecl(void*, const char*)>(0x4D62A0)(g_f.clump, nm)); }, &BuildClumpT);
    }, 300);
    // ---- CAnimBlendClumpData
    Run("CAnimBlendClumpData::SetNumberOfBones 0x4CF140", [&](Rng& r, std::string& d) {
        const uint32_t n1 = 1 + r.below(8), n2 = 1 + r.below(8);
        return ACase(r, d, [&]() -> uint64_t { g_f.cd->m_FrameDatas = nullptr; g_f.cd->SetNumberOfBones(n1); g_f.cd->SetNumberOfBones(n2); return 0; },
            [&]() -> uint64_t { g_f.cd->m_FrameDatas = nullptr; oracle::Fn<void __fastcall(void*, int, uint32_t)>(0x4CF140)(g_f.cd, 0, n1); oracle::Fn<void __fastcall(void*, int, uint32_t)>(0x4CF140)(g_f.cd, 0, n2); return 0; }, &BuildClumpT);
    }, 100);
    Run("CAnimBlendClumpData::ForAllFrames 0x4CF190", [&](Rng& r, std::string& d) {
        return ACase(r, d, [&]() -> uint64_t { g_f.cd->ForAllFrames(&FrameCb, P(kOut)); return 0; }, [&]() -> uint64_t { oracle::Fn<void __fastcall(void*, int, void*, void*)>(0x4CF190)(g_f.cd, 0, (void*)&FrameCb, P(kOut)); return 0; }, &BuildClumpT);
    }, 100);
    Run("CAnimBlendAssocGroup::InitEmptyAssociations 0x4CDFB0", [&](Rng& r, std::string& d) {
        g_maskImg = true;
        return ACase(r, d, [&]() -> uint64_t { reinterpret_cast<CAnimBlendAssocGroup*>(P(kGroups))->InitEmptyAssociations(g_f.clump); return 0; },
            [&]() -> uint64_t { oracle::Fn<void __fastcall(void*, int, void*)>(0x4CDFB0)(P(kGroups), 0, g_f.clump); return 0; }, &BuildGroupInit);
    }, 300);
}

// ORACLE-W3-IK rows A: BoneNode.cpp and BoneNodeManager.cpp (see wave3_oracle_ik.inc).
namespace w3k {
constexpr uintptr_t kMgr = 0xC10820, kMgrList = kMgr + 0x4C00;   // g_boneNodeMan: 128 bone nodes, then the free list (head, tail, count)
// free list of the exe's bone node manager holding `k` random nodes (consistent links); every node's links start as garbage
static std::vector<int> g_mgrOrder;
static int s_k = 0, s_ax = 0;
static void MgrBuild(Rng& r, int k) {
    for (int i = 0; i < 128; ++i) { auto* n = reinterpret_cast<uint32_t*>(kMgr + i * kNodeSz); n[0] = r.u32(); n[1] = r.u32(); }
    int idx[128]; for (int i = 0; i < 128; ++i) idx[i] = i;
    for (int i = 127; i > 0; --i) std::swap(idx[i], idx[r.below(i + 1)]);
    g_mgrOrder.assign(idx, idx + 128);
    auto Np = [&](int j) { return reinterpret_cast<uint32_t*>(kMgr + idx[j] * kNodeSz); };
    for (int j = 0; j < k; ++j) { Np(j)[0] = j ? (uint32_t)(uintptr_t)Np(j - 1) : 0; Np(j)[1] = j + 1 < k ? (uint32_t)(uintptr_t)Np(j + 1) : 0; }   // prev, next
    auto* l = reinterpret_cast<uint32_t*>(kMgrList);
    l[0] = k ? (uint32_t)(uintptr_t)Np(0) : 0; l[1] = k ? (uint32_t)(uintptr_t)Np(k - 1) : 0; l[2] = (uint32_t)k;
}
static void BuildNone(Rng&) {}
static void BuildMgrK(Rng& r) { MgrBuild(r, s_k); }
static void BuildMgr0(Rng& r) { MgrBuild(r, 0); }
static void BuildMgrCnt(Rng& r) { MgrBuild(r, 0); *reinterpret_cast<uint32_t*>(kMgrList + 8) = r.below(3) ? r.below(200) : r.u32(); }
// node s_k becomes a head bone in a third of the cases (Limit scales the Z range with the X angle for BONE_HEAD)
static void BuildLimitNodes(Rng& r) { BuildNodes(r); if (r.below(3) == 0) Nf<int32_t>(s_k, oTag) = 5; }
// parent node 0 with s_k children (nodes 2..) already chained, node 1 is the new child
static void BuildAddChild(Rng& r) {
    BuildNodes(r);
    const int c = s_k;
    for (int j = 0; j < c; ++j) {
        Nf<void*>(2 + j, oPrev) = j ? Nd(2 + j - 1) : nullptr; Nf<void*>(2 + j, oNext) = j + 1 < c ? Nd(2 + j + 1) : nullptr; Nf<void*>(2 + j, oParent) = Nd(0);
    }
    Nf<void*>(0, oChHead) = c ? Nd(2) : nullptr; Nf<void*>(0, oChTail) = c ? Nd(2 + c - 1) : nullptr; Nf<size_t>(0, oChCnt) = (size_t)c;
}
} // namespace w3k

static void TestW3IkA() {
    using namespace w2; using namespace w3k;
    const auto K = [&](Rng& r) { return (int)r.below((uint32_t)kMaxN); };
    // ---- BoneNode_c
    KROW("BoneNode_c::Constructor", 0x616B30, 0)
        const int k = K(r);
        return KCase(r, d, [&]() -> uint64_t { return U(Nd(k)->Constructor()); }, [&]() -> uint64_t { return U(ExeThis<void*>(0x616B30, Nd(k))); }, &BuildNodes, true);
    KEND(200)
    KROW("BoneNode_c::Destructor", 0x616B80, 0)
        const int k = K(r);
        return KCase(r, d, [&]() -> uint64_t { Nd(k)->Destructor(); return 0; }, [&]() -> uint64_t { ExeThis<void>(0x616B80, Nd(k)); return 0; }, &BuildNodes);
    KEND(100)
    KROW("BoneNode_c::Init", 0x6177B0, 0)
        const int k = K(r), ti = (int)r.below(32);
        const int32_t tag = TagAt(ti);
        RpHAnimBlendInterpFrame* fr = Frm(K(r));
        return KCase(r, d, [&]() -> uint64_t { return Nd(k)->Init(BT(tag), fr) ? 1 : 0; }, [&]() -> uint64_t { return (uint8_t)ExeThis<uint32_t, int32_t, void*>(0x6177B0, Nd(k), tag, fr) ? 1 : 0; }, &BuildNodes);
    KEND(600)
    KROW("BoneNode_c::InitLimits", 0x617490, 0)
        const int k = K(r);
        return KCase(r, d, [&]() -> uint64_t { Nd(k)->InitLimits(); return 0; }, [&]() -> uint64_t { ExeThis<void>(0x617490, Nd(k)); return 0; }, &BuildNodes);
    KEND(600)
    KROW("BoneNode_c::GetIdFromBoneTag", 0x617050, 0)
        int32_t tag;
        switch (r.below(6)) { case 0: tag = TagAt((int)r.below(32)); break; case 1: tag = (int32_t)r.below(60) - 4; break; case 2: tag = (int32_t)r.u32(); break; case 3: tag = (int32_t)(0x10000u + r.below(60)); break; default: tag = TagAt((int)r.below(32)); }
        return KCase(r, d, [&]() -> uint64_t { return (uint32_t)W2Get(W3K_GetId{})(BT(tag)); }, [&]() -> uint64_t { return (uint32_t)oracle::Fn<int32_t __cdecl(int32_t)>(0x617050)(tag); }, &BuildNone);
    KEND(400)
    KROW("BoneNode_c::ClampLimitsCurrent", 0x6175D0, 0)
        const int k = K(r); const bool lx = r.below(2), ly = r.below(2), lz = r.below(2);
        StaticRef<uint8_t>(0x8D2BD1) = r.below(3) == 0 ? 1 : 0;   // s_TestSkipClampCurrent: 1 in the exe's image (the clamp is dead code in the game); 0 here to run it
        return KCase(r, d, [&]() -> uint64_t { Nd(k)->ClampLimitsCurrent(lx, ly, lz); return 0; }, [&]() -> uint64_t { ExeThis<void, uint32_t, uint32_t, uint32_t>(0x6175D0, Nd(k), lx, ly, lz); return 0; }, &BuildNodes);
    KEND(500)
    KROW("BoneNode_c::ClampLimitsDefault", 0x617530, 0)
        const int k = K(r); const bool lx = r.below(2), ly = r.below(2), lz = r.below(2);
        StaticRef<uint8_t>(0x8D2BD0) = r.below(3) == 0 ? 1 : 0;   // s_TestSkipClampDefault
        return KCase(r, d, [&]() -> uint64_t { Nd(k)->ClampLimitsDefault(lx, ly, lz); return 0; }, [&]() -> uint64_t { ExeThis<void, uint32_t, uint32_t, uint32_t>(0x617530, Nd(k), lx, ly, lz); return 0; }, &BuildNodes);
    KEND(500)
    KROW("BoneNode_c::Limit", 0x617650, 0)
        s_k = K(r); const int k = s_k; const float blend = SpF(r, 1.f);
        return KCase(r, d, [&]() -> uint64_t { Nd(k)->Limit(blend); return 0; }, [&]() -> uint64_t { ExeThis<void, float>(0x617650, Nd(k), blend); return 0; }, &BuildLimitNodes);
    KEND(1500)
    KROW("BoneNode_c::GetSpeed", 0x616CB0, 0)
        const int k = K(r);
        return KCase(r, d, [&]() -> uint64_t { return FB(Nd(k)->GetSpeed()); }, [&]() -> uint64_t { return FB(ExeThis<float>(0x616CB0, Nd(k))); }, &BuildNodes);
    KEND(100)
    KROW("BoneNode_c::SetSpeed", 0x616CC0, 0)
        const int k = K(r); const float v = SpF(r, 4.f);
        return KCase(r, d, [&]() -> uint64_t { Nd(k)->SetSpeed(v); return 0; }, [&]() -> uint64_t { ExeThis<void, float>(0x616CC0, Nd(k), v); return 0; }, &BuildNodes);
    KEND(100)
    KROW("BoneNode_c::SetLimits", 0x616C50, 0)
        const int k = K(r), ax = (int)r.below(3); const float lo = SpF(r, 180.f), hi = SpF(r, 180.f);
        return KCase(r, d, [&]() -> uint64_t { Nd(k)->SetLimits((eRotationAxis)ax, lo, hi); return 0; }, [&]() -> uint64_t { ExeThis<void, int, float, float>(0x616C50, Nd(k), ax, lo, hi); return 0; }, &BuildNodes);
    KEND(200)
    KROW("BoneNode_c::GetLimits", 0x616BF0, 0)
        const int k = K(r), ax = (int)r.below(3);
        return KCase(r, d, [&]() -> uint64_t { Nd(k)->GetLimits((eRotationAxis)ax, At<float>(kOut), At<float>(kOut + 4)); return 0; }, [&]() -> uint64_t { ExeThis<void, int, void*, void*>(0x616BF0, Nd(k), ax, P(kOut), P(kOut + 4)); return 0; }, &BuildNodes);
    KEND(200)
    KROW("BoneNode_c::AddChild", 0x616BD0, 0)
        s_k = (int)r.below(4);
        return KCase(r, d, [&]() -> uint64_t { Nd(0)->AddChild(Nd(1)); return 0; }, [&]() -> uint64_t { ExeThis<void, void*>(0x616BD0, Nd(0), Nd(1)); return 0; }, &BuildAddChild);
    KEND(300)
    // ---- BoneNodeManager_c (the exe's global g_boneNodeMan: 128 nodes + free list)
    KROW("BoneNodeManager_c::Constructor", 0x617330, 0)
        const uint32_t seed = r.u32() | 1;
        FillGarbage(r, reinterpret_cast<void*>(kMgr), 0x4C0C);   // the object memory starts as garbage; lenient compare (the ctor of 128 nodes leaves their non-link fields alone in the port)
        Reg mgr{ kMgr, 0x4C0C }; mgr.lenient = true;
        Reg img{ kImgLo, kImgHi - kImgLo }; img.mask.assign(kImgHi - kImgLo, 0); for (size_t k = 0; k < 0x4C0C; ++k) img.mask[kMgr - kImgLo + k] = 1;
        return BothA(d, seed, { mgr, img }, [&]() -> uint64_t { return U((g_boneNodeMan.*W2Get(W3K_MgrCtor{}))()); }, [&]() -> uint64_t { return U(ExeThis<void*>(0x617330, &g_boneNodeMan)); });
    KEND(40)
    KROW("BoneNodeManager_c::Destructor", 0x617390, 0)
        s_k = (int)r.below(9);
        return KCase(r, d, [&]() -> uint64_t { (void)(g_boneNodeMan.*W2Get(W3K_MgrDtor{}))(); return 0; }, [&]() -> uint64_t { ExeThis<void>(0x617390, &g_boneNodeMan); return 0; }, &BuildMgrK);
    KEND(40)
    KROW("BoneNodeManager_c::Init", 0x6173F0, 0)
        return KCase(r, d, [&]() -> uint64_t { return g_boneNodeMan.Init() ? 1 : 0; }, [&]() -> uint64_t { return (uint8_t)ExeThis<uint32_t>(0x6173F0, &g_boneNodeMan) ? 1 : 0; }, &BuildMgr0);
    KEND(60)
    KROW("BoneNodeManager_c::Exit", 0x617420, 0)
        s_k = (int)r.below(9);
        return KCase(r, d, [&]() -> uint64_t { g_boneNodeMan.Exit(); return 0; }, [&]() -> uint64_t { ExeThis<void>(0x617420, &g_boneNodeMan); return 0; }, &BuildMgrK);
    KEND(100)
    KROW("BoneNodeManager_c::Reset", 0x617430, 0)
        s_k = (int)r.below(9);
        return KCase(r, d, [&]() -> uint64_t { g_boneNodeMan.Reset(); return 0; }, [&]() -> uint64_t { ExeThis<void>(0x617430, &g_boneNodeMan); return 0; }, &BuildMgrK);
    KEND(100)
    KROW("BoneNodeManager_c::GetBoneNode", 0x617460, 0)
        s_k = (int)r.below(8);
        return KCase(r, d, [&]() -> uint64_t { return U(g_boneNodeMan.GetBoneNode()); }, [&]() -> uint64_t { return U(ExeThis<void*>(0x617460, &g_boneNodeMan)); }, &BuildMgrK);
    KEND(400)
    KROW("BoneNodeManager_c::ReturnBoneNode", 0x617470, 0)
        s_k = (int)r.below(8);
        return KCase(r, d, [&]() -> uint64_t { g_boneNodeMan.ReturnBoneNode(reinterpret_cast<BoneNode_c*>(kMgr + g_mgrOrder[100] * kNodeSz)); return 0; },
            [&]() -> uint64_t { ExeThis<void, void*>(0x617470, &g_boneNodeMan, reinterpret_cast<void*>(kMgr + g_mgrOrder[100] * kNodeSz)); return 0; }, &BuildMgrK);
    KEND(400)
    KROW("BoneNodeManager_c::GetNumBoneNodesLeft", 0x617480, 0)
        return KCase(r, d, [&]() -> uint64_t { return g_boneNodeMan.GetNumBoneNodesLeft(); }, [&]() -> uint64_t { return ExeThis<uint32_t>(0x617480, &g_boneNodeMan); }, &BuildMgrCnt);
    KEND(100)
}

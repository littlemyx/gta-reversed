// ORACLE-W3-IK rows D: the IK chain set-up / update path on a ped clump fixture (IKChain_c::Init / SetupBones / Update / Exit / UpdateEntity, IKChainManager_c::AddIKChain / RemoveIKChain / Exit / Reset,
// BoneNodeManager_c constructor), see wave3_oracle_ik.inc.
// Fixture (inside g_fx above the compared bone-node block): a fake ped clump whose RpAnimBlend plugin slot (ClumpOffset 0xB5F878) points at a CAnimBlendClumpData with 32 frame datas / interp frames,
// and an RpHAnimHierarchy in RenderWare's layout with the 32 node ids of the BoneInfo table and a matrix array. The ped is a slot of the fake ped pool of the reference fixture (rf::PedE(0)): its clump
// pointer is the fake clump and bHasBeenRendered is set. RwHelper.cpp is not part of this target: GetAnimHierarchyFromSkinClump (0x734A40) / GetAnimHierarchyFromClump (0x734B10) are test-local functions
// (port) and Patch()ed host functions (exe) that return the fixture hierarchy.
namespace w3k {
constexpr size_t kClumpB = 0x1000, kCDB = 0x1100, kFramesB = 0x1200, kKeyB = 0x1500, kHierB = 0x1900, kNodeInfoB = 0x1A00, kMatB = 0x1C00, kPedCmp = 0x2400;
constexpr uint32_t kClumpOffV = 0x40;
static RpHAnimHierarchy* HierPtr() { return reinterpret_cast<RpHAnimHierarchy*>(g_fx + kHierB); }
static int g_perm[32];       // hierarchy node i has the node id of BoneInfo entry g_perm[i]
static CPed* PedPtr() { return reinterpret_cast<CPed*>(rf::PedE(0)); }
// bit-exact list of a few ancestors: BoneInfo parent links (tag at +0, parent tag at +2)
static int16_t ParentOf(int16_t tag) { for (int k = 0; k < 32; ++k) if (TagAt(k) == tag) return *reinterpret_cast<int16_t*>(0x8D26D0 + k * 0x28 + 2); return -1; }
static int g_effTag = 0, g_pivTag = 0;

static void BuildPedWorld(Rng& r, bool good) {
    std::memset(g_fx + 0x1000, 0, 0x1400);
    StaticRef<uint32_t>(0xB5F878) = kClumpOffV;
    g_fx[kClumpB] = 2;                                                                   // RwObject type rpCLUMP
    At<void*>(kClumpB + kClumpOffV) = P(kCDB);
    auto* cd = reinterpret_cast<CAnimBlendClumpData*>(P(kCDB));
    cd->m_NumFrameData = 32; cd->m_FrameDatas = reinterpret_cast<AnimBlendFrameData*>(P(kFramesB));
    int idx[32]; for (int i = 0; i < 32; ++i) idx[i] = i;
    for (int i = 31; i > 0; --i) std::swap(idx[i], idx[r.below(i + 1)]);
    std::memcpy(g_perm, idx, sizeof idx);
    auto* h = reinterpret_cast<uint32_t*>(P(kHierB));
    h[0] = 0; h[1] = 32; h[2] = (uint32_t)(uintptr_t)P(kMatB); h[3] = h[2]; h[4] = (uint32_t)(uintptr_t)P(kNodeInfoB);   // flags, numNodes, pMatrixArray, pMatrixArrayUnaligned, pNodeInfo
    for (int i = 0; i < 32; ++i) {
        auto* ni = reinterpret_cast<int32_t*>(P(kNodeInfoB + i * 16));
        ni[0] = TagAt(idx[i]); ni[1] = i; ni[2] = 0; ni[3] = 0;
        auto* fd = reinterpret_cast<AnimBlendFrameData*>(P(kFramesB + i * 0x18));
        fd->Flags = (uint8_t)r.u32(); fd->BoneTag = (uint32_t)TagAt(idx[i]);
        fd->KeyFrame = reinterpret_cast<RpHAnimBlendInterpFrame*>(P(kKeyB + i * 0x1C));
        float* kf = reinterpret_cast<float*>(P(kKeyB + i * 0x1C));
        RandQuatAt(r, kf, true);
        for (int k = 0; k < 3; ++k) kf[4 + k] = (good || r.below(3)) ? (r.below(12) == 0 ? 0.f : FinF(r, 3.f) + 0.01f) : 0.f;
        RandRwMat(r, kMatB + i * 0x40);
    }
    auto* f0 = reinterpret_cast<AnimBlendFrameData*>(P(kFramesB));
    f0->NeedsKeyFrameUpdate = (!good && r.below(3) == 0); f0->IsUpdatingFrame = (good || r.below(3) != 0);
    CPed* ped = PedPtr();
    *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(ped) + 0x18) = P(kClumpB);   // CEntity::m_pRwObject (private)
    if (ped->GetRwObject() != reinterpret_cast<RwObject*>(P(kClumpB))) { std::printf("  m_pRwObject offset wrong\n"); std::abort(); }
    ped->bHasBeenRendered = good || r.below(6) != 0;
}
// effector / pivot: pivot is a proper ancestor (BoneInfo parent links) of the effector, or equal to it (no bones)
static void PickChainTags(Rng& r) {
    for (;;) {
        g_effTag = TagAt((int)r.below(32));
        int anc[32], n = 0; for (int16_t t = (int16_t)g_effTag; t != -1 && n < 32; t = ParentOf(t)) anc[n++] = t;   // effector, its parent, ... root
        const int pick = n > 1 ? (r.below(12) == 0 ? 0 : 1 + (int)r.below(n - 1)) : 0;
        g_pivTag = anc[pick];
        if (g_pivTag != g_effTag || n == 1) return;
    }
}
static CEntity* g_entArg = nullptr;
static void BuildInit(Rng& r) {
    BuildNodes(r);
    BuildPedWorld(r, r.below(4) != 0);
    std::memset(P(kChain), 0, 0x58);                      // old m_Ped / m_TargetEntity null: neither side has a reference to drop
    PickChainTags(r);
    At<float>(kChain + cBlend) = r.below(3) == 0 ? 0.f : r.f01() * 1.2f;               // a re-used chain: Init resets the blend (0x61844F)
    g_entArg = r.below(3) == 0 ? nullptr : (r.below(2) ? rf::VehE(0) : rf::ObjE(0));
    MgrBuild(r, 40 + (int)r.below(88));
}
static void BuildSetup(Rng& r) { BuildInit(r); At<void*>(kChain + cPed) = PedPtr(); }   // SetupBones reads m_Ped (raw: no reference registered)
static void BuildChainClean(Rng& r) { BuildChain(r); At<void*>(kChain + cEntity) = nullptr; At<void*>(kChain + cPed) = nullptr; }
static void BuildUpdate(Rng& r) {
    BuildMove(r);
    BuildPedWorld(r, true);
    At<void*>(kChain + cPed) = PedPtr();
    PickChainTags(r);
    At<int16_t>(kChain + cPivBone) = (int16_t)g_pivTag;
    At<int16_t>(kChain + cEffBone) = (int16_t)g_effTag;
}
} // namespace w3k

RpHAnimHierarchy* GetAnimHierarchyFromSkinClump(RpClump*) { return w3k::HierPtr(); }
RpHAnimHierarchy* GetAnimHierarchyFromClump(RpClump*) { return w3k::HierPtr(); }
static RpHAnimHierarchy* __cdecl ExeHierStub(void*) { return w3k::HierPtr(); }

struct W3K_SetupBones { using type = void (IKChain_c::*)(eBoneTag32, CVector, eBoneTag32, AnimBlendFrameData*); friend type W2Get(W3K_SetupBones); };
template struct W2Rob<W3K_SetupBones, &IKChain_c::SetupBones>;

namespace w3k {
// one case with the reference world (rf) and the free bone-node list of the exe's manager in a defined state
template<class PF, class EF>
static bool PCase(Rng& r, std::string& d, PF&& pf, EF&& ef, void (*build)(Rng&)) {
    const uint32_t seed = r.u32() | 1;
    rf::Enter(); rf::Reset(r, 0);
    build(r);
    Reg fx{ (uintptr_t)g_fx, kPedCmp };
    MaskPads(fx);
    Reg img{ kImgLo, kImgHi - kImgLo };
    const bool ok = BothA(d, seed, { fx, img }, std::forward<PF>(pf), std::forward<EF>(ef), kRf);
    rf::Leave();
    return ok;
}
} // namespace w3k

static void TestW3IkD() {
    using namespace w2; using namespace w3k;
    oracle::Patch(0x734A40, reinterpret_cast<void*>(&ExeHierStub));
    oracle::Patch(0x734B10, reinterpret_cast<void*>(&ExeHierStub));
    KROW("IKChain_c::Init", 0x618370, 0)
        const int slot = (int)r.below(4); const float speed = r.f01() * 2.f; const int prio = (int)r.below(6) - 1;
        const CVector effPos{ FinF(r, 1.f), FinF(r, 1.f), FinF(r, 1.f) }, offPos{ FinF(r, 30.f), FinF(r, 30.f), FinF(r, 30.f) };
        const int offBone = r.below(2) ? -1 : (int)r.below(40);
        return PCase(r, d,
            [&]() -> uint64_t { return Ch()->Init("ik", (eIKChainSlot)slot, PedPtr(), BT(g_effTag), effPos, BT(g_pivTag), g_entArg, BT(offBone), offPos, speed, prio) ? 1 : 0; },
            [&]() -> uint64_t { return (uint8_t)ExeThis<uint32_t, const char*, int, void*, int32_t, CVector, int32_t, void*, int32_t, CVector, float, int32_t>(0x618370, Ch(), "ik", slot, PedPtr(), g_effTag, effPos, g_pivTag, g_entArg, offBone, offPos, speed, prio) ? 1 : 0; },
            &BuildInit);
    KEND(600)
    KROW("IKChain_c::SetupBones", 0x617CA0, 0)
        const CVector effPos{ FinF(r, 1.f), FinF(r, 1.f), FinF(r, 1.f) };
        return PCase(r, d,
            [&]() -> uint64_t { (Ch()->*W2Get(W3K_SetupBones{}))(BT(g_effTag), effPos, BT(g_pivTag), reinterpret_cast<AnimBlendFrameData*>(P(kFramesB))); return 0; },
            [&]() -> uint64_t { ExeThis<void, int32_t, CVector, int32_t, void*>(0x617CA0, Ch(), g_effTag, effPos, g_pivTag, P(kFramesB)); return 0; },
            &BuildSetup);
    KEND(500)
    KROW("IKChain_c::Exit (after SetupBones)", 0x617870, 0)
        const CVector effPos{ 0.f, 0.f, 0.f };
        return PCase(r, d,
            [&]() -> uint64_t { (Ch()->*W2Get(W3K_SetupBones{}))(BT(g_effTag), effPos, BT(g_pivTag), reinterpret_cast<AnimBlendFrameData*>(P(kFramesB))); Ch()->Exit(); return 0; },
            [&]() -> uint64_t { ExeThis<void, int32_t, CVector, int32_t, void*>(0x617CA0, Ch(), g_effTag, effPos, g_pivTag, P(kFramesB)); ExeThis<void>(0x617870, Ch()); return 0; },
            &BuildSetup);
    KEND(300)
    KROW("IKChain_c::Update", 0x6184B0, 0)
        const float dt = r.f01() * 50.f;
        return PCase(r, d, [&]() -> uint64_t { Ch()->Update(dt); return 0; }, [&]() -> uint64_t { ExeThis<void, float>(0x6184B0, Ch(), dt); return 0; }, &BuildUpdate);
    KEND(1000)
    KROW("IKChain_c::UpdateEntity", 0x617E00, 0)
        CEntity* const e = r.below(4) == 0 ? nullptr : GEntN(r);
        return PCase(r, d, [&]() -> uint64_t { Ch()->UpdateEntity(e); return 0; }, [&]() -> uint64_t { ExeThis<void, void*>(0x617E00, Ch(), e); return 0; }, &BuildChainClean);
    KEND(300)
}

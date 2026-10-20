// ORACLE-W3-IK rows C: the bone tree maths (BoneNode_c::CalcWldMat) and the IK solver (IKChain_c::MoveBonesToTarget) on a bone chain with computed world matrices, see wave3_oracle_ik.inc.
struct W3K_Move { using type = void (IKChain_c::*)(); friend type W2Get(W3K_Move); };
template struct W2Rob<W3K_Move, &IKChain_c::MoveBonesToTarget>;

namespace w3k {
constexpr size_t kPivotMat = 0x740;   // RwMatrix of the pivot bone / the parent matrix passed to CalcWldMat
static void Attach(int child, int parent) {   // TList_c::AddItem at the head of parent's child list + m_Parent
    Nf<void*>(child, oParent) = Nd(parent);
    uint8_t* const oldHead = Nf<uint8_t*>(parent, oChHead);
    Nf<void*>(child, oPrev) = nullptr; Nf<void*>(child, oNext) = oldHead;
    if (oldHead) *reinterpret_cast<void**>(oldHead + oPrev) = Nd(child); else Nf<void*>(parent, oChTail) = Nd(child);
    Nf<void*>(parent, oChHead) = Nd(child); Nf<size_t>(parent, oChCnt) += 1;
}
static void BuildTree(Rng& r) {   // node 0 is the root of a random tree over nodes 1..7 (some nodes stay detached)
    BuildNodes(r);
    for (int i = 1; i < (int)kMaxN; ++i) {
        if (r.below(6) == 0) continue;
        Attach(i, (int)r.below((uint32_t)i));
    }
    RandRwMat(r, kPivotMat);
}
static void BuildMove(Rng& r) {   // a chain of g_n bones: bone i's parent is bone i+1, the last one hangs off the pivot bone matrix; world matrices as after CalcWldMat of the root bone
    BuildChain(r);
    for (int i = 0; i < g_n; ++i) { Nf<void*>(i, oParent) = nullptr; Nf<void*>(i, oChHead) = nullptr; Nf<void*>(i, oChTail) = nullptr; Nf<size_t>(i, oChCnt) = 0; Nf<void*>(i, oPrev) = nullptr; Nf<void*>(i, oNext) = nullptr; }
    for (int i = 0; i + 1 < g_n; ++i) Attach(i, i + 1);
    RandRwMat(r, kPivotMat);
    At<void*>(kChain + cPivotMat) = P(kPivotMat);
    Nd(g_n - 1)->CalcWldMat(reinterpret_cast<const RwMatrix*>(P(kPivotMat)));
    At<uint8_t>(kChain + cUpdTarget) = r.below(4) != 0;
    At<void*>(kChain + cEntity) = nullptr;
    At<int32_t>(kChain + cOffBone) = -1;
    for (int k = 0; k < 3; ++k) {
        const float basep = Nf<float>(0, oWld + 0x30 + 4 * k);
        At<float>(kChain + cOffPos + 4 * k) = basep + FinF(r, r.below(3) ? 40.f : 400.f); At<float>(kChain + cOffWS + 4 * k) = basep + FinF(r, r.below(3) ? 40.f : 400.f);
        At<float>(kChain + cEffPos + 4 * k) = FinF(r, r.below(2) ? 0.3f : 8.f);
    }
    if (r.below(10) == 0) for (int k = 0; k < 3; ++k) At<float>(kChain + cOffWS + 4 * k) = Nf<float>(0, oWld + 0x30 + 4 * k);   // target exactly on the effector bone (zero-length direction)
}
} // namespace w3k

static void TestW3IkC() {
    using namespace w2; using namespace w3k;
    KROW("BoneNode_c::CalcWldMat", 0x616CD0, 0)
        return KCase(r, d, [&]() -> uint64_t { Nd(0)->CalcWldMat(reinterpret_cast<const RwMatrix*>(P(kPivotMat))); return 0; }, [&]() -> uint64_t { ExeThis<void, void*>(0x616CD0, Nd(0), P(kPivotMat)); return 0; }, &BuildTree, false, true);
    KEND(800)
    KROW("IKChain_c::MoveBonesToTarget", 0x6178B0, 0)
        return KCase(r, d, [&]() -> uint64_t { (Ch()->*W2Get(W3K_Move{}))(); return 0; }, [&]() -> uint64_t { ExeThis<void>(0x6178B0, Ch()); return 0; }, &BuildMove, false, true);
    KEND(1500)
}

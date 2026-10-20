// ORACLE-W3-IK rows C: the bone tree maths (BoneNode_c::CalcWldMat) and the IK solver (IKChain_c::MoveBonesToTarget) on a bone chain with computed world matrices, see wave3_oracle_ik.inc.
struct W3K_Move { using type = void (IKChain_c::*)(); friend type W2Get(W3K_Move); };
template struct W2Rob<W3K_Move, &IKChain_c::MoveBonesToTarget>;

// _CIacos takes its argument in st0 (fst qword [esp] ... call): a cdecl thunk loads it
static __declspec(naked) double __cdecl ExeAcos(double) { __asm { fld qword ptr [esp + 4]
 mov eax, 0x822380
 call eax
 ret } }

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
    for (int i = 0; i < g_n; ++i) Nf<float>(i, oSpeed) = r.below(4) == 0 ? 1.f : r.f01() * 2.f;   // finite bone speeds (the game's are 1.0 from Init; NaN speeds only exercise the NaN rules of the RtQuat code)
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
    // ---- the RenderWare maths the IK solver calls (shim vs the exe's own code), on the same kind of inputs: vectors / rigid matrices / unit quaternions of the fixture
    const auto BuildRw = [](Rng& r) {
        BuildTree(r);
        for (int k = 0; k < 3; ++k) { At<float>(kOut + 0x40 + 4 * k) = FinF(r, 10.f); At<float>(kOut + 0x50 + 4 * k) = r.below(2) ? FinF(r, 1.f) : FinF(r, 100.f); }
        RandQuatAt(r, reinterpret_cast<float*>(P(kOut + 0x60)), r.below(3) != 0);
    };
    static void (*const pBuildRw)(Rng&) = +BuildRw;
    KROW("RwV3dTransformVector", 0x7EDDC0, 0)
        return KCase(r, d, [&]() -> uint64_t { return U(RwV3dTransformVector(&At<RwV3d>(kOut), &At<RwV3d>(kOut + 0x40), reinterpret_cast<const RwMatrix*>(P(kNode + oWld)))); },
            [&]() -> uint64_t { return U(oracle::Fn<void* __cdecl(void*, void*, void*)>(0x7EDDC0)(P(kOut), P(kOut + 0x40), P(kNode + oWld))); }, pBuildRw, false, true);
    KEND(800)
    KROW("RwV3dNormalize", 0x7ED9B0, 0)
        return KCase(r, d, [&]() -> uint64_t { return FB(RwV3dNormalize(&At<RwV3d>(kOut), &At<RwV3d>(kOut + 0x50))); },
            [&]() -> uint64_t { return FB(oracle::Fn<float __cdecl(void*, void*)>(0x7ED9B0)(P(kOut), P(kOut + 0x50))); }, pBuildRw, false, true);
    KEND(800)
    KROW("RwV3dLength", 0x7EDAC0, 0)
        return KCase(r, d, [&]() -> uint64_t { return FB(RwV3dLength(&At<RwV3d>(kOut + 0x50))); },
            [&]() -> uint64_t { return FB(oracle::Fn<float __cdecl(void*)>(0x7EDAC0)(P(kOut + 0x50))); }, pBuildRw, false, true);
    KEND(800)
    KROW("RtQuatConvertFromMatrix", 0x7EB5C0, 0)
        return KCase(r, d, [&]() -> uint64_t { return RtQuatConvertFromMatrix(&At<RtQuat>(kOut), reinterpret_cast<const RwMatrix*>(P(kNode + oWld))) ? 1 : 0; },
            [&]() -> uint64_t { return oracle::Fn<int __cdecl(void*, void*)>(0x7EB5C0)(P(kOut), P(kNode + oWld)) ? 1 : 0; }, pBuildRw, false, true);
    KEND(1000)
    KROW("RtQuatTransformVectors", 0x7EBBB0, 0)
        return KCase(r, d, [&]() -> uint64_t { return U(RtQuatTransformVectors(&At<RwV3d>(kOut), &At<RwV3d>(kOut + 0x50), 1, &At<RtQuat>(kOut + 0x60))); },
            [&]() -> uint64_t { return U(oracle::Fn<void* __cdecl(void*, void*, int, void*)>(0x7EBBB0)(P(kOut), P(kOut + 0x50), 1, P(kOut + 0x60))); }, pBuildRw, false, true);
    KEND(800)
    KROW("RtQuatRotate", 0x7EB7C0, 0)
        const float ang = FinF(r, r.below(2) ? 5.f : 360.f);
        return KCase(r, d, [&]() -> uint64_t { return U(RtQuatRotate(&At<RtQuat>(kOut + 0x60), &At<RwV3d>(kOut + 0x50), ang, rwCOMBINEPOSTCONCAT)); },
            [&]() -> uint64_t { return U(oracle::Fn<void* __cdecl(void*, void*, float, int)>(0x7EB7C0)(P(kOut + 0x60), P(kOut + 0x50), ang, 2)); }, pBuildRw, false, true);
    KEND(1000)
    KROW("x87::acos (CRT _CIacos)", 0x822380, 0)
        const float x = r.below(2) ? 0.9f + r.f01() * 0.1f : FinF(r, 1.f);
        return KCase(r, d, [&]() -> uint64_t { const double v = x87::acos((double)x); uint64_t u; std::memcpy(&u, &v, 8); return u; },
            [&]() -> uint64_t { const double v = ExeAcos((double)x); uint64_t u; std::memcpy(&u, &v, 8); return u; }, &BuildNone);
    KEND(2000)
    KROW("IKChain_c::MoveBonesToTarget", 0x6178B0, 0)
        return KCase(r, d, [&]() -> uint64_t { (Ch()->*W2Get(W3K_Move{}))(); return 0; }, [&]() -> uint64_t { ExeThis<void>(0x6178B0, Ch()); return 0; }, &BuildMove, false, true);
    KEND(1500)
}

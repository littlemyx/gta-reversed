// ORACLE-W3-IK: exe-oracle rows for the uncovered functions of game_sa/Ragdoll (BoneNode_c, BoneNodeManager_c, IKChain_c, IKChainManager_c) on REAL fixtures:
// a pool of bone nodes with bone tags from the exe's BoneInfo table (0x8D26D0), interp frames, doubly linked child lists, world matrices, limits; IK chains whose bone array points at those nodes;
// the exe's own bone-node manager / IK chain manager globals (0xC10820 / 0xC15448) with consistent free / active lists; a ped clump with an RpHAnim hierarchy for the setup path (wave3_oracle_ik2.inc).
// Included by wave2_oracle_all.inc when W3_IK_ONLY is defined (executable wave3_ik_oracle_test = the W2 harness: deterministic arena, whole exe data image compared after every case).
// The fixture block g_fx is compared as a whole after every call.
#include "Ragdoll/IKChain.h"
#include "Ragdoll/BoneNode.h"
#include "Ragdoll/BoneNodeManager.h"
#include "Ragdoll/IKChainManager.h"

// private members through explicit instantiation (W2Rob, wave2_oracle_common.inc)
struct W3K_GetId { using type = int32 (*)(eBoneTag32); friend type W2Get(W3K_GetId); };
template struct W2Rob<W3K_GetId, &BoneNode_c::GetIdFromBoneTag>;
struct W3K_GetBoneNode { using type = BoneNode_c* (IKChain_c::*)(eBoneTag32); friend type W2Get(W3K_GetBoneNode); };
template struct W2Rob<W3K_GetBoneNode, &IKChain_c::GetBoneNodeFromTag>;

namespace w3k {
using namespace w2;
constexpr size_t kFxSize = 0x1000;                   // compared region (the fixture)
alignas(16) static uint8_t g_fx[0x6000];             // fixture block; only the first kFxSize bytes are compared / garbage filled (ped / hierarchy fixtures of ik2 live above)
constexpr size_t kNode = 0x000, kNodeSz = 0x98, kMaxN = 8, kFrm = 0x500, kFrmSz = 0x1C, kChain = 0x600, kBoneArr = 0x680, kOut = 0x700;
// BoneNode_c offsets
constexpr size_t oPrev = 0x0, oNext = 0x4, oTag = 0x8, oFrame = 0xC, oQ = 0x10, oPos = 0x20, oParent = 0x2C, oChHead = 0x30, oChTail = 0x34, oChCnt = 0x38, oWld = 0x3C, oMin = 0x7C, oMax = 0x88, oSpeed = 0x94;
// IKChain_c offsets
constexpr size_t cPrev = 0x0, cNext = 0x4, cPed = 0x8, cCount = 0xC, cBones = 0x10, cPivotMat = 0x14, cBlend = 0x18, cEffBone = 0x1C, cEffPos = 0x20, cPivBone = 0x2C, cEntity = 0x30, cOffBone = 0x34, cOffPos = 0x38,
                 cSpeed = 0x44, cOffWS = 0x48, cUpdTarget = 0x54, cSlot = 0x55, cPrio = 0x56;
static uint8_t* P(size_t off) { return g_fx + off; }
template<class T> static T& At(size_t off) { return *reinterpret_cast<T*>(g_fx + off); }
static BoneNode_c* Nd(int i) { return reinterpret_cast<BoneNode_c*>(g_fx + kNode + i * kNodeSz); }
static uint8_t* NdB(int i) { return g_fx + kNode + i * kNodeSz; }
template<class T> static T& Nf(int i, size_t off) { return *reinterpret_cast<T*>(NdB(i) + off); }
static RpHAnimBlendInterpFrame* Frm(int i) { return reinterpret_cast<RpHAnimBlendInterpFrame*>(g_fx + kFrm + i * kFrmSz); }
static IKChain_c* Ch() { return reinterpret_cast<IKChain_c*>(g_fx + kChain); }
static eBoneTag32 BT(int32_t v) { return std::bit_cast<eBoneTag32>(v); }
static uint64_t U(const void* p) { return (uint64_t)(uintptr_t)p; }
static uint64_t FB(float f) { uint32_t u; std::memcpy(&u, &f, 4); return u; }
template<class R, class... A> static R ExeThis(uint32_t addr, void* self, A... a) { return oracle::Fn<R __fastcall(void*, int, A...)>(addr)(self, 0, a...); }
static int16_t TagAt(int k) { return *reinterpret_cast<int16_t*>(0x8D26D0 + k * 0x28); }   // BoneInfo_t table of the exe (32 entries, 0x28 each)
static float PoseF(int k, size_t off) { return *reinterpret_cast<float*>(0x8D26D0 + k * 0x28 + off); }

// finite game-range value, or (7 %) one of the edge values
static float SpF(Rng& r, float scale) {
    if (r.below(14) == 0) {
        static const uint32_t sp[] = { 0x7FC00000u, 0x7F800000u, 0xFF800000u, 0x7F7FFFFFu, 0x00000001u, 0x80000000u, 0x00800000u, 0x7F000000u, 0x7F800001u };   // qNaN, +-inf, FLT_MAX, denormal, -0, FLT_MIN, 1.7e38, sNaN
        const uint32_t u = sp[r.below((uint32_t)(sizeof sp / sizeof sp[0]))]; float f; std::memcpy(&f, &u, 4); return f;
    }
    return (r.f01() * 2.f - 1.f) * scale;
}
static float FinF(Rng& r, float scale) { return (r.f01() * 2.f - 1.f) * scale; }
static float AngF(Rng& r) { return r.below(8) == 0 ? 0.f : FinF(r, r.below(4) ? 120.f : 180.f); }
static void RandQuatAt(Rng& r, float* q4, bool unit) {
    float q[4] = { r.f01() - 0.5f, r.f01() - 0.5f, r.f01() - 0.5f, r.f01() - 0.5f };
    const float m = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
    const float k = unit && m > 1e-6f ? 1.f / m : (r.below(2) ? 2.f : 1.f);
    for (int i = 0; i < 4; ++i) q4[i] = q[i] * k;
}
static void RandRwMat(Rng& r, size_t off) {   // a rigid RwMatrix (random rotation from a unit quaternion, position in game range)
    float q[4]; RandQuatAt(r, q, true);
    const float x = q[0], y = q[1], z = q[2], w = q[3];
    float* m = reinterpret_cast<float*>(g_fx + off);   // right(3) flags | up(3) pad | at(3) pad | pos(3) pad
    m[0] = 1.f - 2.f * (y * y + z * z); m[1] = 2.f * (x * y + z * w); m[2] = 2.f * (x * z - y * w);
    m[4] = 2.f * (x * y - z * w); m[5] = 1.f - 2.f * (x * x + z * z); m[6] = 2.f * (y * z + x * w);
    m[8] = 2.f * (x * z + y * w); m[9] = 2.f * (y * z - x * w); m[10] = 1.f - 2.f * (x * x + y * y);
    m[12] = FinF(r, 60.f); m[13] = FinF(r, 60.f); m[14] = FinF(r, 60.f);
    m[3] = 0.f; m[7] = 0.f; m[11] = 0.f; m[15] = 0.f;
    reinterpret_cast<uint32_t*>(m)[3] = 3;   // rwMATRIXTYPEORTHONORMAL, flags word (offset 0x0C of the RwMatrix: `flags`)
}
// distinct valid bone tags for the nodes
static void PickTags(Rng& r, int* out, int n) {
    int idx[32]; for (int i = 0; i < 32; ++i) idx[i] = i;
    for (int i = 31; i > 0; --i) std::swap(idx[i], idx[r.below(i + 1)]);
    for (int i = 0; i < n; ++i) out[i] = idx[i];
}
// the node pool: tags from the BoneInfo table, interp frames, orientation / position / world matrix / limits / speed
static int g_tagIdx[kMaxN];
static void BuildNodes(Rng& r) {
    FillGarbage(r, g_fx, kFxSize);
    PickTags(r, g_tagIdx, (int)kMaxN);
    for (int i = 0; i < (int)kMaxN; ++i) {
        Nf<int32_t>(i, oTag) = TagAt(g_tagIdx[i]);
        Nf<RpHAnimBlendInterpFrame*>(i, oFrame) = Frm(i);
        RandQuatAt(r, reinterpret_cast<float*>(g_fx + kFrm + i * kFrmSz), true); std::memcpy(g_fx + kFrm + i * kFrmSz + 16, std::array<float, 3>{ FinF(r, 5.f), FinF(r, 5.f), FinF(r, 5.f) }.data(), 12);
        RandQuatAt(r, &Nf<float>(i, oQ), r.below(5) != 0);       // m_Orientation: imag.xyz, real  (mostly unit)
        for (int k = 0; k < 3; ++k) Nf<float>(i, oPos + 4 * k) = FinF(r, 4.f);
        Nf<void*>(i, oParent) = nullptr; Nf<void*>(i, oChHead) = nullptr; Nf<void*>(i, oChTail) = nullptr; Nf<size_t>(i, oChCnt) = 0;
        RandRwMat(r, kNode + i * kNodeSz + oWld);
        const bool fromTable = r.below(2);
        for (int ax = 0; ax < 3; ++ax) {
            if (fromTable) {   // as InitLimits builds them (pose -+ range)
                const float pose = PoseF(g_tagIdx[i], 0x8 + 4 * ax);
                Nf<float>(i, oMin + 4 * ax) = pose - PoseF(g_tagIdx[i], 0x10 + 8 * ax); Nf<float>(i, oMax + 4 * ax) = pose + PoseF(g_tagIdx[i], 0x14 + 8 * ax);
            } else {           // arbitrary but ordered (std::clamp requires lo <= hi)
                const float lo = AngF(r); Nf<float>(i, oMin + 4 * ax) = lo; Nf<float>(i, oMax + 4 * ax) = lo + (r.below(6) == 0 ? 0.f : r.f01() * 200.f);
            }
        }
        Nf<float>(i, oSpeed) = r.below(5) == 0 ? SpF(r, 3.f) : r.f01() * 2.f;
    }
}
// chain object over the first n nodes (bone array pointing at them), plausible scalar fields
static int g_n = 0;
static void BuildChain(Rng& r) {
    BuildNodes(r);
    g_n = 1 + (int)r.below((uint32_t)kMaxN);
    for (int i = 0; i < g_n; ++i) At<BoneNode_c*>(kBoneArr + 4 * i) = Nd(i);
    At<int32_t>(kChain + cCount) = g_n; At<void*>(kChain + cBones) = P(kBoneArr);
    At<float>(kChain + cBlend) = r.below(4) == 0 ? (r.below(2) ? 1.f : 0.98f) : r.f01() * 1.2f;
    At<float>(kChain + cSpeed) = r.f01() * 2.f;
    for (int k = 0; k < 3; ++k) { At<float>(kChain + cOffWS + 4 * k) = FinF(r, 80.f); At<float>(kChain + cEffPos + 4 * k) = FinF(r, 1.f); At<float>(kChain + cOffPos + 4 * k) = FinF(r, 20.f); }
}
static void BuildMgrNone(Rng&) {}

// one case: `build` fills the fixture, port and exe run on identical bytes (fixture, arena, whole data image restored between the two runs)
template<class PF, class EF>
static bool KCase(Rng& r, std::string& d, PF&& pf, EF&& ef, void (*build)(Rng&) = &BuildChain, bool lenient = false) {
    const uint32_t seed = r.u32() | 1;
    build(r);
    Reg fx{ (uintptr_t)g_fx, kFxSize };
    if (lenient) fx.lenient = true;
    Reg img{ kImgLo, kImgHi - kImgLo };
    return BothA(d, seed, { fx, img }, std::forward<PF>(pf), std::forward<EF>(ef));
}
#define KROW(NAME, ADDR, CASES) Run(NAME " " #ADDR, [&](Rng& r, std::string& d) -> bool {
#define KEND(CASES) }, CASES);
} // namespace w3k

#include "wave3_oracle_ik_a.inc"   // BoneNode_c, BoneNodeManager_c
#include "wave3_oracle_ik_b.inc"   // IKChain_c, IKChainManager_c

namespace w3k {
static uint8_t g_engine[0x1000];   // stand-in for the exe's RwEngineInstance (0xC97B24): the allocator hooks and the plugin data the exe's RwV3d / RwMatrix code reads
static void* __cdecl FakeAlloc(size_t n, unsigned) { return std::calloc(1, n); }
static void  __cdecl FakeFree(void* p) { std::free(p); }
// the exe's RenderWare maths (RwV3dLength / RwV3dNormalize / RwMatrixMultiply ...) reads plugin data that RwEngineStart would have built: the approximate-sqrt tables (plugin constructor 0x7EDE90 stores
// their offset into 0xC97934) and the matrix plugin data (RwMatrixOpen 0x7F16C0: identity mask, multiply kernel, tolerances) - the same set-up as tests/standalone/rw_math_stream_test.cpp
static void InitExeRw() {
    StaticRef<void*>(0xC97B24) = g_engine;
    *reinterpret_cast<void**>(g_engine + 0x134) = reinterpret_cast<void*>(&FakeAlloc);
    *reinterpret_cast<void**>(g_engine + 0x138) = reinterpret_cast<void*>(&FakeFree);
    oracle::Fn<void* __cdecl(void*, int, int)>(0x7EDE90)(nullptr, 0x400, 0);
    StaticRef<int32_t>(0xC979BC) = 0x600;
    *reinterpret_cast<uint32_t*>(g_engine + 0x604) = 0x20000;
    *reinterpret_cast<void**>(g_engine + 0x608) = reinterpret_cast<void*>(0x7F12F0);
    for (int k = 0; k < 3; k++) *reinterpret_cast<uint32_t*>(g_engine + 0x60C + 4 * k) = 0x3C23D70Au;   // RwMatrixOpen's default tolerances (0.01f)
}
}
static void TestW3IkAll() {
    (void)spdlog::default_logger_raw();   // the default logger is created lazily by the first NOTSA_LOG_* of the port: do it outside the redirected allocator
    spdlog::set_level(spdlog::level::off);   // "bone tag not found" warnings (IKChain_c::GetBoneNodeFromTag) would allocate in the port only
    w3k::InitExeRw();
    TestW3IkA();
    TestW3IkB();
}

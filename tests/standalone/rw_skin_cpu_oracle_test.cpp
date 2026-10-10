// P2B-24c: differential test of the skin CPU path (source/standalone/rw/pipeline_skin_cpu_core.h) against the exe's own machine code (exe-oracle, see game_oracle.h:
// the test maps gta_sa_compact.exe, env RW_EXE_ORACLE=<path>, at its original addresses and calls the original functions next to the port).
//
// Sections (each compared BIT FOR BIT; N random cases per section, default 20000):
//   bonesse   0x7CAA30 (-> 0x7CB390 + the pad zeroing): the whole 64-byte slot of every bone, all hierarchy flavours (flags 0, 2, 0x4000, 0x4002) with fake
//             RpHAnimHierarchy / RpSkin / RwFrame data in the exe's RW 3.6 layout (random general / orthonormal / scaled / identity matrices, identity shortcuts included)
//   bonesx87  0x7CA850 (-> 0x7CA6C0): the 12 written floats of every slot
//   sse1b / sse1w / sse2w / sse4w   0x7CAAD0 / 0x7CAB80 / 0x7CAF50 / 0x7CAC60 over random vertex counts (1..40, tails not multiples of anything), random matrices (non
//             orthonormal, random pad lanes), weights (sum 1 and not 1, zeros, w0 == 1.0, w0 just below / above 1.0, negative, w2 / w3 zero or negative), indices, strides
//             (24..80, normals interleaved at +12 or in their own array), with and without normals; the destination buffers (incl. the gaps between vertices) are compared
//   x871b / x871w / x872w / x874w    the x87 twins 0x7CA330 / 0x7CA240 / 0x7CA410 / 0x7C9DA0 (matrix layout of 0x7CA850: transposed rows)
//   e2e       whole chain per skin: the exe's prep (0x7CAA30 / 0x7CA850) + the kernel the dispatch of 0x7C7B90 selects (numWeights, numUsedBones; coded from the asm
//             0x7C7E44..0x7C7FC3 / 0x7C7F10..0x7C7FC0) against BuildBones* + SkinVertices, for SSE and x87
// NOT oracle-able: the node body 0x7C7B90 as a whole (needs the engine: resentry, device locks), the draw 0x7C85B0 (D3D state), the librw marshalling in pipeline_skin_cpu.cpp.
// usage: rw_skin_cpu_oracle_test.exe [-n cases] [section ...]   (exit code 0 = no mismatch)
#include "game_oracle.h"

#include "pipeline_skin_cpu_core.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace rwskincpu;

//--------------------------------------------------------------------------------------------------
// helpers
//--------------------------------------------------------------------------------------------------
static uint32_t g_rng = 0x2468ACEu;
static uint32_t Rn() { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5; return g_rng; }
static int   RI(int lo, int hi) { return lo + static_cast<int>(Rn() % static_cast<uint32_t>(hi - lo + 1)); }
static float RF(float lo, float hi) { return lo + (hi - lo) * ((Rn() >> 8) * (1.0f / 16777216.0f)); }
static bool  Chance(int percent) { return static_cast<int>(Rn() % 100) < percent; }
static uint32_t FB(float f) { uint32_t b; std::memcpy(&b, &f, 4); return b; }
static float BF(uint32_t b) { float f; std::memcpy(&f, &b, 4); return f; }

alignas(16) static uint8_t g_arena[1 << 22];
static size_t g_arenaUsed = 0;
static uint8_t* Alloc(size_t n) { // zeroed, 16 aligned; the arena is reset per case
    g_arenaUsed = (g_arenaUsed + 15) & ~size_t(15);
    uint8_t* p = g_arena + g_arenaUsed;
    g_arenaUsed += n;
    if (g_arenaUsed > sizeof(g_arena)) { std::printf("arena overflow\n"); std::exit(2); }
    std::memset(p, 0, n);
    return p;
}
static uint32_t U32(const void* p) { return reinterpret_cast<uint32_t>(p); }
template <class T> static T& At(uint8_t* base, size_t off) { return *reinterpret_cast<T*>(base + off); }
template <class T> static T& G(uint32_t va) { return *reinterpret_cast<T*>(static_cast<uintptr_t>(va)); }

static int g_mismatch = 0, g_cases = 0, g_secStart = 0;
static int g_stat[16];
static std::string g_section;
static void Fail(const char* what, int caseNo) {
    if (g_mismatch - g_secStart < 5) std::printf("  MISMATCH [%s] case %d: %s\n", g_section.c_str(), caseNo, what);
    ++g_mismatch;
}

// random matrices
enum MatKind { kGeneral, kOrtho, kIdentity, kScaled };
static RwMatrix RndMat(MatKind k) {
    RwMatrix m;
    m.setIdentity();
    if (k == kIdentity) return m; // flags = identity | orthonormal, exactly identity
    m.right = {RF(-2, 2), RF(-2, 2), RF(-2, 2)};
    m.up    = {RF(-2, 2), RF(-2, 2), RF(-2, 2)};
    m.at    = {RF(-2, 2), RF(-2, 2), RF(-2, 2)};
    m.pos   = {RF(-300, 300), RF(-300, 300), RF(-300, 300)};
    m.flags = 0;
    if (k == kOrtho) {
        rwx::OrthoNormalize(&m, &m);
        m.flags = 3;
        return m;
    }
    if (k == kScaled) { // uniform scale
        rwx::OrthoNormalize(&m, &m);
        const float s = RF(0.3f, 4.0f);
        m.right = {m.right.x * s, m.right.y * s, m.right.z * s};
        m.up    = {m.up.x * s, m.up.y * s, m.up.z * s};
        m.at    = {m.at.x * s, m.at.y * s, m.at.z * s};
        m.flags = Chance(30) ? 1u : 0u;
    }
    return m;
}
static MatKind RndKind() { const int r = RI(0, 9); return r < 3 ? kGeneral : r < 6 ? kOrtho : r < 8 ? kScaled : kIdentity; }

//--------------------------------------------------------------------------------------------------
// the exe's world (same setup as rw_skin_hw_oracle_test: engine instance, matrix plugin, sqrt tables)
//--------------------------------------------------------------------------------------------------
alignas(16) static uint8_t g_engine[0x1000];
static void* __cdecl HostAlloc(size_t n, unsigned) { return std::calloc(1, n); }
static void  __cdecl HostFree(void* p) { std::free(p); }

static bool SetupExe(const char* path) {
    if (!oracle::Map(path)) return false;
    G<void*>(0xC97B24) = g_engine;
    At<void*>(g_engine, 0x134) = reinterpret_cast<void*>(&HostAlloc);
    At<void*>(g_engine, 0x138) = reinterpret_cast<void*>(&HostFree);
    oracle::Fn<void*(__cdecl)(void*, int, int)>(0x7EDE90)(nullptr, 0x400, 0);   // the exe's own plugin constructor builds the sqrt tables
    G<int>(0xC979BC) = 0x600;                                                    // matrix plugin (RwMatrixOpen): identity mask, multiply kernel, tolerances
    At<uint32_t>(g_engine, 0x604) = 0x20000;
    At<void*>(g_engine, 0x608) = reinterpret_cast<void*>(0x7F12F0);
    for (int k = 0; k < 3; k++) At<uint32_t>(g_engine, 0x60C + 4 * k) = 0x3C23D70Au;
    return true;
}

// a fake RwFrame (LTM at +0x50, root with the "not dirty" bit clear at +0xA0)
static uint8_t* MakeFrame(const RwMatrix& ltm) {
    uint8_t* f = Alloc(0xB0);
    uint8_t* root = Alloc(16);
    At<uint32_t>(f, 0xA0) = U32(root);
    std::memcpy(f + 0x50, &ltm, 64);
    return f;
}

//--------------------------------------------------------------------------------------------------
// fake hierarchy / skin / atomic in the exe's layout
//--------------------------------------------------------------------------------------------------
struct FakeHier {
    uint8_t* hier = nullptr;
    uint8_t* atomic = nullptr;
    uint8_t* skin = nullptr;
    std::vector<RwMatrix> skinToBone, hierMats, ltms;
    std::vector<const RwMatrix*> ltmPtr;
    RwMatrix atomicLtm;
    BoneSource src;
};

static FakeHier MakeHier(int n, uint32_t flags) {
    FakeHier h;
    h.skinToBone.resize(n); h.hierMats.resize(n); h.ltms.resize(n);
    for (int i = 0; i < n; i++) {
        h.skinToBone[i] = RndMat(RndKind());
        h.skinToBone[i].flags = Chance(85) ? 0u : h.skinToBone[i].flags;
        h.hierMats[i] = RndMat(RndKind());
        h.ltms[i] = RndMat(RndKind());
    }
    uint8_t* hier = Alloc(0x40);
    uint8_t* mats = Alloc(64 * n);
    uint8_t* info = Alloc(16 * n);
    std::memcpy(mats, h.hierMats.data(), 64 * n);
    At<uint32_t>(hier, 0) = flags;
    At<int32_t>(hier, 4) = n;
    At<uint32_t>(hier, 8) = U32(mats);
    At<uint32_t>(hier, 0x10) = U32(info);
    h.ltmPtr.resize(n);
    for (int i = 0; i < n; i++) {
        uint8_t* fr = MakeFrame(h.ltms[i]);
        At<uint32_t>(info, 16 * i + 0xC) = U32(fr);
        h.ltmPtr[i] = reinterpret_cast<const RwMatrix*>(fr + 0x50);
    }
    h.hier = hier;
    h.atomicLtm = RndMat(RndKind());
    h.atomic = Alloc(0x40);
    uint8_t* afr = MakeFrame(h.atomicLtm);
    At<uint32_t>(h.atomic, 4) = U32(afr);
    h.skin = Alloc(0x60);
    At<uint32_t>(h.skin, 0xC) = U32(h.skinToBone.data());
    h.src.hierFlags = flags;
    h.src.numNodes = n;
    h.src.skinToBone = h.skinToBone.data();
    h.src.hierMatrices = reinterpret_cast<const RwMatrix*>(mats);
    h.src.nodeLTM = h.ltmPtr.data();
    h.src.atomicLTM = reinterpret_cast<const RwMatrix*>(afr + 0x50);
    return h;
}

static const uint32_t kHierFlags[] = {0, 2, 0x4000, 0x4002, 0x1000, 0x2000};

//--------------------------------------------------------------------------------------------------
// section: bone matrices
//--------------------------------------------------------------------------------------------------
static void TestBones(int n, bool sse) {
    g_section = sse ? "bonesse" : "bonesx87";
    for (int c = 0; c < n; c++, g_cases++) {
        g_arenaUsed = 0;
        const int nodes = RI(1, 48);
        FakeHier h = MakeHier(nodes, kHierFlags[RI(0, 5)]);
        alignas(16) static float a[64 * 16 + 16], b[64 * 16 + 16];
        std::memset(a, 0, sizeof(a)); std::memset(b, 0, sizeof(b));
        G<float*>(0xC978AC) = a;
        G<uint32_t>(0xC978D4) = 0;                                       // matrix cache reset
        oracle::Fn<float*(__cdecl)(void*, void*, void*)>(sse ? 0x7CAA30 : 0x7CA850)(h.atomic, h.skin, h.hier);
        if (sse) BuildBonesSse(b, h.src); else BuildBonesX87(b, h.src);
        if (sse) {
            if (std::memcmp(a, b, nodes * 64) != 0) Fail("bone matrices differ (64-byte slots)", c);
        } else {
            for (int i = 0; i < nodes; i++) {
                if (std::memcmp(a + 16 * i, b + 16 * i, 12 * 4) != 0) { Fail("bone matrices differ (12 floats)", c); break; }
            }
        }
    }
}

//--------------------------------------------------------------------------------------------------
// kernel inputs
//--------------------------------------------------------------------------------------------------
struct KernelCase {
    uint32_t count;
    int numBones;
    float* mats;          // numBones * 16 floats, aligned
    float* weights;       // 4 per vertex
    uint8_t* indices;     // 4 per vertex
    float* srcPos;        // 3 per vertex
    float* srcNrm;        // 3 per vertex, or null
    uint32_t stride;
    bool interleaved;     // normals at +12 of the position
    size_t bufSize;
    uint8_t* posA; uint8_t* posB; uint8_t* nrmA; uint8_t* nrmB;
};

static void RndWeights(float* w) {
    const int mode = RI(0, 11);
    for (int k = 0; k < 4; k++) w[k] = RF(0.0f, 1.0f);
    float s = w[0] + w[1] + w[2] + w[3];
    switch (mode) {
    case 0: w[0] = 1.0f; w[1] = w[2] = w[3] = 0.0f; break;
    case 1: for (int k = 0; k < 4; k++) w[k] /= s; break;
    case 2: for (int k = 0; k < 4; k++) w[k] = RF(0.0f, 1.5f); break;
    case 3: w[2] = 0.0f; break;
    case 4: w[3] = 0.0f; break;
    case 5: w[0] = RF(-1.0f, 0.0f); break;
    case 6: w[0] = BF(0x3F7FFFFFu); break;                                   // just below 1.0
    case 7: w[0] = BF(0x3F800001u); break;                                   // just above 1.0
    case 8: w[1] = 0.0f; break;
    case 9: { for (int k = 0; k < 4; k++) w[k] /= s; std::sort(w, w + 4, [](float x, float y) { return x > y; }); break; }
    case 10: w[2] = RF(-0.5f, 0.0f); w[3] = RF(0.0f, 0.5f); break;           // negative third weight with a positive fourth
    default: w[0] = RF(0.5f, 3.0f); break;
    }
}

static KernelCase MakeKernelCase(bool x87) {
    KernelCase k{};
    k.count = Chance(10) ? 1 : static_cast<uint32_t>(RI(1, 40));
    k.numBones = RI(1, 30);
    k.mats = reinterpret_cast<float*>(Alloc(64 * k.numBones));
    for (int i = 0; i < k.numBones * 16; i++) k.mats[i] = RF(-3.0f, 3.0f);
    if (Chance(30)) for (int i = 0; i < k.numBones; i++) { // near-identity matrices: more exact intermediate values
        std::memset(k.mats + 16 * i, 0, 64);
        k.mats[16 * i] = k.mats[16 * i + 5] = k.mats[16 * i + 10] = 1.0f;
        k.mats[16 * i + 12] = RF(-5, 5);
    }
    (void)x87;
    k.weights = reinterpret_cast<float*>(Alloc(16 * k.count));
    k.indices = Alloc(4 * k.count);
    for (uint32_t v = 0; v < k.count; v++) {
        RndWeights(k.weights + 4 * v);
        for (int j = 0; j < 4; j++) k.indices[4 * v + j] = static_cast<uint8_t>(RI(0, k.numBones - 1));
    }
    k.srcPos = reinterpret_cast<float*>(Alloc(12 * k.count));
    for (uint32_t i = 0; i < 3 * k.count; i++) k.srcPos[i] = Chance(10) ? 0.0f : RF(-100.0f, 100.0f);
    if (Chance(75)) {
        k.srcNrm = reinterpret_cast<float*>(Alloc(12 * k.count));
        for (uint32_t i = 0; i < 3 * k.count; i++) k.srcNrm[i] = RF(-1.0f, 1.0f);
    }
    k.interleaved = Chance(60);
    k.stride = k.interleaved ? static_cast<uint32_t>(RI(6, 20) * 4) : static_cast<uint32_t>(RI(3, 20) * 4);
    k.bufSize = static_cast<size_t>(k.stride) * k.count + 64;
    k.posA = Alloc(k.bufSize); k.posB = Alloc(k.bufSize);
    std::memset(k.posA, 0xCD, k.bufSize); std::memset(k.posB, 0xCD, k.bufSize);
    if (k.interleaved) {
        k.nrmA = k.posA + 12; k.nrmB = k.posB + 12;
    } else {
        k.nrmA = Alloc(k.bufSize); k.nrmB = Alloc(k.bufSize);
        std::memset(k.nrmA, 0xAB, k.bufSize); std::memset(k.nrmB, 0xAB, k.bufSize);
    }
    return k;
}

static bool SameOutput(const KernelCase& k) {
    if (std::memcmp(k.posA, k.posB, k.bufSize) != 0) return false;
    if (!k.interleaved && std::memcmp(k.nrmA, k.nrmB, k.bufSize) != 0) return false;
    return true;
}

using KernelKind = int; // 0 = 1 bone, 1 = 1 weight several bones, 2 = 2 weights, 3 = 3-4 weights
static void TestKernel(int n, KernelKind kind, bool sse) {
    static const char* names[] = {"1b", "1w", "2w", "4w"};
    g_section = std::string(sse ? "sse" : "x87") + names[kind];
    static const uint32_t addrSse[] = {0x7CAAD0, 0x7CAB80, 0x7CAF50, 0x7CAC60};
    static const uint32_t addr87[]  = {0x7CA330, 0x7CA240, 0x7CA410, 0x7C9DA0};
    const uint32_t addr = (sse ? addrSse : addr87)[kind];
    for (int c = 0; c < n; c++, g_cases++) {
        g_arenaUsed = 0;
        KernelCase k = MakeKernelCase(!sse);
        const float* oneMat = k.mats + 16 * RI(0, k.numBones - 1);
        const uint32_t cnt = k.count;
        // like the node (0x7C7E18..): the destination normal pointer is null exactly when the source normals are (the x87 1-bone kernel tests the destination, the SSE ones the source)
        uint8_t* const nA = k.srcNrm ? k.nrmA : nullptr;
        uint8_t* const nB = k.srcNrm ? k.nrmB : nullptr;
        switch (kind) {
        case 0:
            oracle::Fn<void(__cdecl)(uint32_t, const float*, uint8_t*, const float*, uint8_t*, const float*, uint32_t)>(addr)(cnt, oneMat, k.posA, k.srcPos, nA, k.srcNrm, k.stride);
            if (sse) Skin1BoneSse(cnt, oneMat, k.posB, k.srcPos, nB, k.srcNrm, k.stride);
            else Skin1Bone87(cnt, oneMat, k.posB, k.srcPos, nB, k.srcNrm, k.stride);
            break;
        case 1:
            oracle::Fn<void(__cdecl)(uint32_t, const uint8_t*, const float*, uint8_t*, const float*, uint8_t*, const float*, uint32_t)>(addr)(cnt, k.indices, k.mats, k.posA, k.srcPos, nA, k.srcNrm, k.stride);
            if (sse) Skin1WeightSse(cnt, k.indices, k.mats, k.posB, k.srcPos, nB, k.srcNrm, k.stride);
            else Skin1Weight87(cnt, k.indices, k.mats, k.posB, k.srcPos, nB, k.srcNrm, k.stride);
            break;
        default:
            oracle::Fn<void(__cdecl)(uint32_t, const float*, const uint8_t*, const float*, uint8_t*, const float*, uint8_t*, const float*, uint32_t)>(addr)(cnt, k.weights, k.indices, k.mats, k.posA, k.srcPos, nA, k.srcNrm, k.stride);
            if (kind == 2) {
                if (sse) Skin2Sse(cnt, k.weights, k.indices, k.mats, k.posB, k.srcPos, nB, k.srcNrm, k.stride);
                else Skin2X87(cnt, k.weights, k.indices, k.mats, k.posB, k.srcPos, nB, k.srcNrm, k.stride);
            } else {
                if (sse) Skin4Sse(cnt, k.weights, k.indices, k.mats, k.posB, k.srcPos, nB, k.srcNrm, k.stride);
                else Skin4X87(cnt, k.weights, k.indices, k.mats, k.posB, k.srcPos, nB, k.srcNrm, k.stride);
            }
        }
        if (!SameOutput(k)) {
            Fail("output differs", c);
            if (g_mismatch - g_secStart <= 2) {
                for (size_t i = 0; i + 3 < k.bufSize; i += 4) {
                    uint32_t x, y; std::memcpy(&x, k.posA + i, 4); std::memcpy(&y, k.posB + i, 4);
                    if (x != y) { std::printf("    first diff at +%zu: exe %08X port %08X (count %u stride %u interleaved %d normals %d)\n", i, x, y, k.count, k.stride, k.interleaved, k.srcNrm != nullptr); break; }
                }
            }
        }
        g_stat[0] += k.srcNrm != nullptr;
    }
}

//--------------------------------------------------------------------------------------------------
// section: the whole chain per skin (prep + the kernel the dispatch selects)
//--------------------------------------------------------------------------------------------------
static void TestE2E(int n, bool sse) {
    g_section = sse ? "e2esse" : "e2ex87";
    for (int c = 0; c < n; c++, g_cases++) {
        g_arenaUsed = 0;
        const int nodes = RI(1, 40);
        FakeHier h = MakeHier(nodes, kHierFlags[RI(0, 5)]);
        alignas(16) static float a[64 * 16 + 16], b[64 * 16 + 16];
        std::memset(a, 0, sizeof(a)); std::memset(b, 0, sizeof(b));
        G<float*>(0xC978AC) = a;
        G<uint32_t>(0xC978D4) = 0;
        oracle::Fn<float*(__cdecl)(void*, void*, void*)>(sse ? 0x7CAA30 : 0x7CA850)(h.atomic, h.skin, h.hier);
        if (sse) BuildBonesSse(b, h.src); else BuildBonesX87(b, h.src);

        const uint32_t cnt = Chance(10) ? 1u : static_cast<uint32_t>(RI(1, 60));
        SkinInput in;
        in.numVertices = cnt;
        in.numWeights = RI(1, 4);
        const int used = Chance(30) ? 1 : RI(1, nodes);
        std::vector<uint8_t> usedBones(used);
        for (int i = 0; i < used; i++) usedBones[i] = static_cast<uint8_t>(RI(0, nodes - 1));
        in.numUsedBones = used;
        in.usedBones = usedBones.data();
        float* weights = reinterpret_cast<float*>(Alloc(16 * cnt));
        uint8_t* indices = Alloc(4 * cnt);
        for (uint32_t v = 0; v < cnt; v++) {
            RndWeights(weights + 4 * v);
            for (int j = 0; j < 4; j++) indices[4 * v + j] = usedBones[RI(0, used - 1)];
        }
        in.weights = weights; in.indices = indices;
        float* sp = reinterpret_cast<float*>(Alloc(12 * cnt));
        for (uint32_t i = 0; i < 3 * cnt; i++) sp[i] = RF(-100.0f, 100.0f);
        float* sn = reinterpret_cast<float*>(Alloc(12 * cnt));
        for (uint32_t i = 0; i < 3 * cnt; i++) sn[i] = RF(-1.0f, 1.0f);
        in.srcPos = sp;
        in.srcNrm = Chance(80) ? sn : nullptr;
        const uint32_t stride = static_cast<uint32_t>(RI(6, 16) * 4);
        const size_t sz = static_cast<size_t>(stride) * cnt + 64;
        uint8_t* da = Alloc(sz); uint8_t* db = Alloc(sz);
        std::memset(da, 0xCD, sz); std::memset(db, 0xCD, sz);
        uint8_t* na = in.srcNrm ? da + 12 : nullptr;
        uint8_t* nb = in.srcNrm ? db + 12 : nullptr;

        // the exe's dispatch (0x7C7E44.. SSE, 0x7C7F10.. x87)
        const uint32_t w = static_cast<uint32_t>(in.numWeights);
        if (sse) {
            if (w > 2)      oracle::Fn<void(__cdecl)(uint32_t, const float*, const uint8_t*, const float*, uint8_t*, const float*, uint8_t*, const float*, uint32_t)>(0x7CAC60)(cnt, weights, indices, a, da, sp, na, in.srcNrm, stride);
            else if (w > 1) oracle::Fn<void(__cdecl)(uint32_t, const float*, const uint8_t*, const float*, uint8_t*, const float*, uint8_t*, const float*, uint32_t)>(0x7CAF50)(cnt, weights, indices, a, da, sp, na, in.srcNrm, stride);
            else if (used == 1) oracle::Fn<void(__cdecl)(uint32_t, const float*, uint8_t*, const float*, uint8_t*, const float*, uint32_t)>(0x7CAAD0)(cnt, a + 16 * usedBones[0], da, sp, na, in.srcNrm, stride);
            else            oracle::Fn<void(__cdecl)(uint32_t, const uint8_t*, const float*, uint8_t*, const float*, uint8_t*, const float*, uint32_t)>(0x7CAB80)(cnt, indices, a, da, sp, na, in.srcNrm, stride);
        } else {
            if (w > 2)      oracle::Fn<void(__cdecl)(uint32_t, const float*, const uint8_t*, const float*, uint8_t*, const float*, uint8_t*, const float*, uint32_t)>(0x7C9DA0)(cnt, weights, indices, a, da, sp, na, in.srcNrm, stride);
            else if (w > 1) oracle::Fn<void(__cdecl)(uint32_t, const float*, const uint8_t*, const float*, uint8_t*, const float*, uint8_t*, const float*, uint32_t)>(0x7CA410)(cnt, weights, indices, a, da, sp, na, in.srcNrm, stride);
            else if (used == 1) oracle::Fn<void(__cdecl)(uint32_t, const float*, uint8_t*, const float*, uint8_t*, const float*, uint32_t)>(0x7CA330)(cnt, a + 16 * usedBones[0], da, sp, na, in.srcNrm, stride);
            else            oracle::Fn<void(__cdecl)(uint32_t, const uint8_t*, const float*, uint8_t*, const float*, uint8_t*, const float*, uint32_t)>(0x7CA240)(cnt, indices, a, da, sp, na, in.srcNrm, stride);
        }
        SkinVertices(sse, in, b, db, nb, stride);
        if (std::memcmp(da, db, sz) != 0) Fail("skinned vertices differ", c);
        g_stat[1 + (in.numWeights - 1)]++;
    }
}

//--------------------------------------------------------------------------------------------------
int main(int argc, char** argv) {
    int n = 20000;
    std::vector<std::string> only;
    for (int i = 1; i < argc; i++) {
        if (!std::strcmp(argv[i], "-n") && i + 1 < argc) n = std::atoi(argv[++i]);
        else only.push_back(argv[i]);
    }
    const char* exe = std::getenv("RW_EXE_ORACLE");
    if (!exe || !SetupExe(exe)) {
        std::printf("RW_EXE_ORACLE not set or the exe cannot be mapped: nothing to compare\n");
        return 2;
    }
    auto want = [&](const char* s) { return only.empty() || std::find(only.begin(), only.end(), s) != only.end(); };
    struct Sec { const char* name; void (*fn)(int); };
    static const Sec secs[] = {
        {"bonesse", [](int n) { TestBones(n, true); }},
        {"bonesx87", [](int n) { TestBones(n, false); }},
        {"sse1b", [](int n) { TestKernel(n, 0, true); }},
        {"sse1w", [](int n) { TestKernel(n, 1, true); }},
        {"sse2w", [](int n) { TestKernel(n, 2, true); }},
        {"sse4w", [](int n) { TestKernel(n, 3, true); }},
        {"x871b", [](int n) { TestKernel(n, 0, false); }},
        {"x871w", [](int n) { TestKernel(n, 1, false); }},
        {"x872w", [](int n) { TestKernel(n, 2, false); }},
        {"x874w", [](int n) { TestKernel(n, 3, false); }},
        {"e2esse", [](int n) { TestE2E(n, true); }},
        {"e2ex87", [](int n) { TestE2E(n, false); }},
    };
    for (auto& s : secs) {
        if (!want(s.name)) continue;
        const int before = g_mismatch, cb = g_cases;
        oracle::g_where = s.name;
        g_secStart = g_mismatch;
        s.fn(n);
        std::printf("[%s] %d cases, %d mismatches\n", s.name, g_cases - cb, g_mismatch - before);
    }
    std::printf("coverage: kernel cases with normals %d; e2e skins by numWeights 1:%d 2:%d 3:%d 4:%d\n", g_stat[0], g_stat[1], g_stat[2], g_stat[3], g_stat[4]);
    std::printf("TOTAL: %d cases, %d mismatches\n", g_cases, g_mismatch);
    return g_mismatch ? 1 : 0;
}

// P2B-24b: differential test of the skin HW render path (source/standalone/rw/pipeline_skin_core.h) against the exe's own machine code (exe-oracle, see
// game_oracle.h: the test maps gta_sa_compact.exe, env RW_EXE_ORACLE=<path>, at its original addresses and calls the original functions next to the port).
//
// Sections (each compared BIT FOR BIT; N random cases per section):
//   bones     0x7C78A0 bone matrix builder (all three hierarchy flavours) with fake RpHAnimHierarchy / RpSkin / RwFrame data in the exe's RW 3.6 layout
//   worldxf   0x764650 / 0x7646E0 / 0x7647B0 / 0x764D30 / 0x764E70 / 0x764F60 (c0..c3, inverse, direction / position / radius into object space);
//             the exe's D3DX dispatch slots are pointed at the SSE kernels it installs on SSE CPUs (0x7B92C6 .. 0x7B9D6E), run natively
//   begin     0x7CB190 shader key from geometry flags / fog state
//   lights    0x761170 light constants (fake world with global and sector light lists, ambient / directional / point / spot, caps of 15, 0x749330 patched)
//   needconst 0x75EDD0 number of constants (the port in this test is only the helper the render test uses for the exe-side budget; A's file is not linked here)
//   render    0x7C8060 as a whole: the exe runs with a table [begin 0x7CB190, lighting 0x761170, get-shader (host), mesh render 0x761030, end 0x761000], the RW
//             state helpers (0x7FC2D0 / 0x7FC340 / 0x7FDE70 / 0x7FE0A0 / 0x7FA090 / 0x7FC200) are patched with recorders and the device is a fake COM object whose
//             vtable records SetVertexShaderConstantF / SetIndices / SetVertexDeclaration / SetVertexShader / SetPixelShader / Draw*; the port runs on the same
//             logical data with a Dev that logs the same events. The two event logs (with the constant data) must be identical.
// NOT oracle-able: the vertex-shader composer (agent A's), the D3D device itself, the D3DX generic (x87) variants; the librw marshalling in pipeline_skin.cpp.
// usage: rw_skin_hw_oracle_test.exe [-n cases] [section ...]   (exit code 0 = no mismatch)
#include "game_oracle.h"

#include "pipeline_skin_core.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace rwskin;

//--------------------------------------------------------------------------------------------------
// helpers
//--------------------------------------------------------------------------------------------------
static uint32_t g_rng = 0x1234567u;
static uint32_t Rn() { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5; return g_rng; }
static int   RI(int lo, int hi) { return lo + static_cast<int>(Rn() % static_cast<uint32_t>(hi - lo + 1)); }
static float RF(float lo, float hi) { return lo + (hi - lo) * ((Rn() >> 8) * (1.0f / 16777216.0f)); }
static bool  Chance(int percent) { return static_cast<int>(Rn() % 100) < percent; }
static uint32_t FB(float f) { uint32_t b; std::memcpy(&b, &f, 4); return b; }

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
static bool Detail() { return g_mismatch - g_secStart <= 3; }
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
// the exe's world: engine instance, frames, lights
//--------------------------------------------------------------------------------------------------
alignas(16) static uint8_t g_engine[0x1000];
static void* __cdecl HostAlloc(size_t n, unsigned) { return std::calloc(1, n); }
static void  __cdecl HostFree(void* p) { std::free(p); }
static int   g_fogEnable = 0, g_fogType = 1;
static int __cdecl HostRenderStateGet(int state, void* out) {            // the engine's RwRenderStateGet (engine + 0x24)
    *reinterpret_cast<int*>(out) = state == 0xE ? g_fogEnable : state == 0x10 ? g_fogType : 0;
    return 1;
}
static float g_fogDensity = 0.0f;
static uint8_t* g_sphereOut;                                              // 0x749330 replacement result (x y z r)
static const float* __cdecl HostWorldSphere(void*) { return reinterpret_cast<const float*>(g_sphereOut); }

static bool SetupExe(const char* path) {
    if (!oracle::Map(path)) return false;
    G<void*>(0xC97B24) = g_engine;
    At<void*>(g_engine, 0x134) = reinterpret_cast<void*>(&HostAlloc);
    At<void*>(g_engine, 0x138) = reinterpret_cast<void*>(&HostFree);
    At<void*>(g_engine, 0x24)  = reinterpret_cast<void*>(&HostRenderStateGet);
    oracle::Fn<void*(__cdecl)(void*, int, int)>(0x7EDE90)(nullptr, 0x400, 0);   // the exe's own plugin constructor builds the sqrt tables
    G<int>(0xC979BC) = 0x600;                                                    // matrix plugin (RwMatrixOpen): identity mask, multiply kernel, tolerances
    At<uint32_t>(g_engine, 0x604) = 0x20000;
    At<void*>(g_engine, 0x608) = reinterpret_cast<void*>(0x7F12F0);
    for (int k = 0; k < 3; k++) At<uint32_t>(g_engine, 0x60C + 4 * k) = 0x3C23D70Au;
    // D3DX dispatch table (0x8D7490, 0x47 entries): the defaults (0x781A87 copies 0x8D75B0 over it), then the SSE variants 0x7AC5E2 installs for the entries used here;
    // D3DXMatrixTranspose (+0x94) keeps its default (pure element moves)
    std::memcpy(reinterpret_cast<void*>(0x8D7490), reinterpret_cast<void*>(0x8D75B0), 0x47 * 4);
    G<uint32_t>(0x8D749C) = 0x7B92C6; // D3DXMatrixMultiply
    G<uint32_t>(0x8D74A0) = 0x7BA139; // D3DXMatrixMultiplyTranspose
    G<uint32_t>(0x8D74A8) = 0x7B9BC5; // D3DXVec3TransformNormal
    G<uint32_t>(0x8D74B8) = 0x7B9D6E; // D3DXVec3TransformCoord
    oracle::Patch(0x749330, reinterpret_cast<void*>(&HostWorldSphere));
    // the composer's constant table (0x760CF0 fills it; the skin pipeline only reads it for lights modes b3 >> 2 != 0)
    const uint32_t tab[12] = {0, 2, 2, 4, 0, 0, 2, 2, 0, 2, 4, 6};
    for (int i = 0; i < 12; i++) G<uint32_t>(0xC94AF8 + 4 * i) = tab[i];
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
// section: bones
//--------------------------------------------------------------------------------------------------
struct FakeHier {
    uint8_t* hier = nullptr;
    std::vector<RwMatrix> skinToBone, hierMats;
    std::vector<const RwMatrix*> ltmPtr;
    std::vector<RwMatrix> ltms;
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
    h.src.hierFlags = flags;
    h.src.numNodes = n;
    h.src.skinToBone = h.skinToBone.data();
    h.src.hierMatrices = reinterpret_cast<const RwMatrix*>(mats);
    h.src.nodeLTM = h.ltmPtr.data();
    return h;
}

static void SetWorldExe(const RwMatrix* w) { oracle::Fn<void(__cdecl)(const RwMatrix*)>(0x764650)(w); }

static void TestBones(int n) {
    g_section = "bones";
    static const uint32_t kFlags[] = {0, 2, 0x4000, 0x4002, 0x1000, 0x2000};
    for (int c = 0; c < n; c++, g_cases++) {
        g_arenaUsed = 0;
        const int nodes = RI(1, 48);
        FakeHier h = MakeHier(nodes, kFlags[RI(0, 5)]);
        const bool hasWorld = Chance(90);
        RwMatrix world = RndMat(RndKind());
        const RwMatrix* wp = hasWorld ? &world : nullptr;
        uint8_t* atomic = Alloc(0x40);
        At<uint32_t>(atomic, 4) = U32(MakeFrame(world));
        G<uint32_t>(0xC978A4) = 0x10;                                    // atomic plugin offset of the hierarchy pointer
        At<uint32_t>(atomic, 0x10) = U32(h.hier);
        uint8_t* skin = Alloc(0x40);
        At<uint32_t>(skin, 0xC) = U32(h.skinToBone.data());
        std::vector<float> a(nodes * 12 + 16, 0.0f), b(nodes * 12 + 16, 0.0f);
        G<uint32_t>(0xC978AC) = 0; G<uint32_t>(0xC978D4) = 0;            // bone cache reset
        SetWorldExe(wp);
        oracle::Fn<void(__cdecl)(float*, void*, void*)>(0x7C78A0)(a.data(), atomic, skin);
        WorldXform wx;
        wx.Set(wp);
        BuildBoneMatrices(b.data(), h.src, wx);
        if (std::memcmp(a.data(), b.data(), nodes * 12 * sizeof(float)) != 0) Fail("bone matrices differ", c);
    }
}

//--------------------------------------------------------------------------------------------------
// section: world transform module
//--------------------------------------------------------------------------------------------------
static void TestWorld(int n) {
    g_section = "worldxf";
    for (int c = 0; c < n; c++, g_cases++) {
        const bool hasWorld = Chance(90);
        RwMatrix world = RndMat(RndKind());
        const RwMatrix* wp = hasWorld ? &world : nullptr;
        alignas(16) float view[16], proj[16];
        for (int i = 0; i < 16; i++) { view[i] = RF(-3, 3); proj[i] = RF(-3, 3); }
        std::memcpy(reinterpret_cast<void*>(0xC9BC80), view, 64);
        std::memcpy(reinterpret_cast<void*>(0x8E2458), proj, 64);
        G<uint32_t>(0xC97C64) = 0;
        SetWorldExe(wp);
        WorldXform wx;
        wx.Set(wp);
        alignas(16) float ea[16], eb[16];
        oracle::Fn<void(__cdecl)(float*)>(0x7646E0)(ea);
        wx.WorldViewProjT(eb, view, proj);
        if (std::memcmp(ea, eb, 64) != 0) Fail("c0..c3 (WorldViewProjT)", c);
        alignas(16) float ia[16], ib[16];
        oracle::Fn<void(__cdecl)(float*)>(0x7647B0)(ia);
        wx.InverseD3D(ib);
        if (std::memcmp(ia, ib, 64) != 0) {
            Fail("inverse", c);
            if (Detail()) {
                std::printf("    world flags %X has=%d\n", hasWorld ? world.flags : 0, hasWorld);
                for (int i = 0; i < 16; i++) std::printf("    [%2d] exe %08X port %08X\n", i, FB(ia[i]), FB(ib[i]));
            }
        }
        for (int k = 0; k < 4; k++) {
            alignas(16) float v[3] = {RF(-5, 5), RF(-5, 5), RF(-5, 5)};
            if (k == 0) { v[0] = 0; v[1] = 0; v[2] = 1; }
            alignas(16) float oa[4] = {1, 2, 3, 4}, ob[4] = {1, 2, 3, 4};
            oracle::Fn<void(__cdecl)(const float*, float*)>(0x764D30)(v, oa);
            wx.Direction(ob, v);
            if (std::memcmp(oa, ob, 16) != 0) Fail("direction", c);
            float pa[4] = {1, 2, 3, 4}, pb[4] = {1, 2, 3, 4};
            oracle::Fn<void(__cdecl)(const float*, float*)>(0x764E70)(v, pa);
            wx.Position(pb, v);
            if (std::memcmp(pa, pb, 16) != 0) Fail("position", c);
            const float r = RF(0.1f, 100.0f);
            float ra = 0, rb = wx.ObjectRadius(r);
            oracle::Fn<void(__cdecl)(float, float*)>(0x764F60)(r, &ra);
            if (std::memcmp(&ra, &rb, 4) != 0) Fail("radius", c);
        }
    }
}

//--------------------------------------------------------------------------------------------------
// section: begin key
//--------------------------------------------------------------------------------------------------
static void TestBegin() {
    g_section = "begin";
    int c = 0;
    for (uint32_t flags = 0; flags < 256; flags++) {
        for (uint32_t tex = 0; tex < 5; tex++) {
            for (int fogOn = 0; fogOn < 2; fogOn++) {
                for (int fogType = 0; fogType < 5; fogType++, c++, g_cases++) {
                    g_arenaUsed = 0;
                    uint8_t* geo = Alloc(0x80);
                    At<uint32_t>(geo, 8) = flags;
                    At<uint32_t>(geo, 0x1C) = tex;
                    uint8_t* atomic = Alloc(0x80);
                    At<uint32_t>(atomic, 0x18) = U32(geo);
                    g_fogEnable = fogOn; g_fogType = fogType;
                    uint8_t key[4] = {0xA5, 0x5A, 0xC3, 0x3C};
                    oracle::Fn<void(__cdecl)(void*, int, void*)>(0x7CB190)(atomic, 1, key);
                    const LightKey k = BeginKey(flags, tex, fogOn != 0, static_cast<u32>(fogType));
                    if (std::memcmp(key, k.b, 4) != 0) Fail("key", c);
                }
            }
        }
    }
}

//--------------------------------------------------------------------------------------------------
// fake lights / world in the exe's layout
//--------------------------------------------------------------------------------------------------
struct LightSetup {
    std::vector<LightRef> global, local;
    float sphere[4];
    uint8_t* world = nullptr;
    uint8_t* atomic = nullptr;
    uint8_t* geometry = nullptr;
};

static float g_spread = 70.0f;
static LightRef RndLight(bool local, const float* sphere) {
    LightRef l;
    static const int kGlobalTypes[] = {1, 1, 1, 2, 2, 0x80, 0x81, 3};
    static const int kLocalTypes[] = {0x80, 0x80, 0x81, 0x82, 0x82, 0x7F, 0x83, 1};
    l.type  = local ? kLocalTypes[RI(0, 7)] : kGlobalTypes[RI(0, 7)];
    l.flags = Chance(88) ? 1u : 0u;
    l.color[0] = RF(0, 1.2f); l.color[1] = RF(0, 1.2f); l.color[2] = RF(0, 1.2f);
    l.radius = RF(1, 60);
    const int a = RI(0, 9);
    l.minusCosAngle = a == 0 ? RF(-1.5f, 1.5f) : a == 1 ? RF(-1e-3f, 1e-3f) : a == 2 ? -0.9995f : a == 3 ? RF(-0.5f, 0.5f) : a == 4 ? -1.0f : RF(-1, 1);
    l.ltm = RndMat(Chance(60) ? kOrtho : kGeneral);
    if (local) {
        l.ltm.pos = {sphere[0] + RF(-g_spread, g_spread), sphere[1] + RF(-g_spread, g_spread), sphere[2] + RF(-g_spread, g_spread)};
    }
    return l;
}

// writes an exe RpLight and links it into `head` (list head at head: next, prev)
static void LinkLight(uint8_t* head, uint8_t* linkNode, uint8_t* data) {
    // circular list with sentinel `head`: append
    uint32_t prev = At<uint32_t>(head, 4);
    if (!At<uint32_t>(head, 0)) { At<uint32_t>(head, 0) = U32(head); At<uint32_t>(head, 4) = U32(head); prev = U32(head); }
    At<uint32_t>(linkNode, 0) = U32(head);
    At<uint32_t>(linkNode, 4) = prev;
    At<uint32_t>(reinterpret_cast<uint8_t*>(prev), 0) = U32(linkNode);
    At<uint32_t>(head, 4) = U32(linkNode);
    (void)data;
}

static uint8_t* MakeExeLight(const LightRef& l) {
    uint8_t* p = Alloc(0x60);
    At<uint8_t>(p, 0) = 3;
    At<uint8_t>(p, 1) = static_cast<uint8_t>(l.type);
    At<uint8_t>(p, 2) = static_cast<uint8_t>(l.flags);
    At<uint32_t>(p, 4) = U32(MakeFrame(l.ltm));
    At<float>(p, 0x14) = l.radius;
    At<float>(p, 0x18) = l.color[0]; At<float>(p, 0x1C) = l.color[1]; At<float>(p, 0x20) = l.color[2]; At<float>(p, 0x24) = 1.0f;
    At<float>(p, 0x28) = l.minusCosAngle;
    return p;
}

// geometry flags: bit 3 prelit, 4 normals, 5 light, 6 modulate; keep the others random
static LightSetup MakeLightSetup(uint32_t geoFlags, int maxGlobal, int maxLocal) {
    LightSetup s;
    s.sphere[0] = RF(-50, 50); s.sphere[1] = RF(-50, 50); s.sphere[2] = RF(-50, 50); s.sphere[3] = RF(0.1f, 10);
    s.world = Alloc(0x100);
    const int ng = RI(0, maxGlobal), nl = RI(0, maxLocal);
    uint8_t* gHead = s.world + 0x3C;
    for (int i = 0; i < ng; i++) {
        LightRef l = RndLight(false, s.sphere);
        s.global.push_back(l);
        uint8_t* p = MakeExeLight(l);
        LinkLight(gHead, p + 0x34, p);
    }
    if (!At<uint32_t>(gHead, 0)) { At<uint32_t>(gHead, 0) = U32(gHead); At<uint32_t>(gHead, 4) = U32(gHead); }
    s.atomic = Alloc(0x100);
    uint8_t* sectorLink = Alloc(16);
    uint8_t* sector = Alloc(0x80);
    uint8_t* aHead = s.atomic + 0x64;
    At<uint32_t>(aHead, 0) = U32(sectorLink); At<uint32_t>(aHead, 4) = U32(sectorLink);
    At<uint32_t>(sectorLink, 0) = U32(aHead); At<uint32_t>(sectorLink, 4) = U32(aHead); At<uint32_t>(sectorLink, 8) = U32(sector);
    uint8_t* lHead = sector + 0x40;
    for (int i = 0; i < nl; i++) {
        LightRef l = RndLight(true, s.sphere);
        s.local.push_back(l);
        uint8_t* p = MakeExeLight(l);
        uint8_t* node = Alloc(16);
        At<uint32_t>(node, 8) = U32(p);
        LinkLight(lHead, node, p);
    }
    if (!At<uint32_t>(lHead, 0)) { At<uint32_t>(lHead, 0) = U32(lHead); At<uint32_t>(lHead, 4) = U32(lHead); }
    s.geometry = Alloc(0x80);
    At<uint32_t>(s.geometry, 8) = geoFlags;
    At<uint32_t>(s.atomic, 0x18) = U32(s.geometry);
    g_sphereOut = Alloc(16);
    std::memcpy(g_sphereOut, s.sphere, 16);
    return s;
}

static WorldView ViewOf(const LightSetup& s) {
    WorldView v;
    v.numGlobal = static_cast<int>(s.global.size()); v.global = s.global.data();
    v.numLocal = static_cast<int>(s.local.size());   v.local = s.local.data();
    std::memcpy(v.sphere, s.sphere, 16);
    return v;
}

static void TestLights(int n) {
    g_section = "lights";
    for (int c = 0; c < n; c++, g_cases++) {
        g_arenaUsed = 0;
        const uint32_t geoFlags = (Chance(75) ? 0x10u : 0u) | (Chance(90) ? 0x20u : 0u) | (Chance(40) ? 0x8u : 0u) | (Chance(50) ? 0x40u : 0u) | (Chance(30) ? 4u : 0u);
        g_spread = Chance(50) ? 70.0f : 25.0f;
        LightSetup s = MakeLightSetup(geoFlags, 18, Chance(50) ? 22 : 45);
        const bool haveWorld = Chance(95);
        G<uint32_t>(0xC97B24) = U32(g_engine);
        At<uint32_t>(g_engine, 4) = haveWorld ? U32(s.world) : 0;
        At<uint16_t>(g_engine, 0xA) = static_cast<uint16_t>(RI(0, 60000));
        RwMatrix world = RndMat(RndKind());
        const bool hasW = Chance(92);
        SetWorldExe(hasW ? &world : nullptr);
        WorldXform wx;
        wx.Set(hasW ? &world : nullptr);
        alignas(16) float bufA[1024], bufB[1024];
        std::memset(bufA, 0xAB, sizeof(bufA)); std::memset(bufB, 0xAB, sizeof(bufB));
        uint8_t keyA[4] = {0x77, static_cast<uint8_t>(RI(0, 255)), 0x9C, 0x31};
        LightKey keyB; std::memcpy(keyB.b, keyA, 4);
        float* endA = oracle::Fn<float*(__cdecl)(void*, int, float*, uint8_t*)>(0x761170)(s.atomic, 1, bufA, keyA);
        const Collected col = CollectLights(ViewOf(s), geoFlags);
        float* endB = BuildLightConstants(bufB, geoFlags, haveWorld, col, wx, keyB);
        if (col.nDir) g_stat[0]++;
        if (col.nPoint) g_stat[1]++;
        if (col.nSpot) g_stat[2]++;
        if (col.nDir && col.nPoint && col.nSpot) g_stat[3]++;
        if (col.nPoint > 4 || col.nSpot > 4) g_stat[4]++;
        for (int i = 0; i < col.nSpot; i++) if (col.spot[i].type == 0x82 && std::cos(LightAcos(col.spot[i].minusCosAngle)) < 0.999) g_stat[5]++;
        if ((endA - bufA) != (endB - bufB)) { Fail("end pointer", c); continue; }
        if (std::memcmp(bufA, bufB, sizeof(bufA)) != 0) Fail("constants", c);
        if (std::memcmp(keyA, keyB.b, 4) != 0) Fail("key", c);
    }
}

//--------------------------------------------------------------------------------------------------
// section: number of constants (0x75EDD0): the helper the render test hands to the port
//--------------------------------------------------------------------------------------------------
static int NumConsts(const LightKey& k) { // port of 0x75EDD0 (also what notsa::skinvs::NumLightConstants computes)
    static const int tab[12] = {0, 2, 2, 4, 0, 0, 2, 2, 0, 2, 4, 6};
    int c = 5;
    const unsigned a = k.b[0];
    if (a & 0xF) c = (a & 0xF) * 2 + 5;
    if (a & 0xF0) { const int m = static_cast<int>(a >> 4); c = ((m + 3) >> 2) + c + m * 2; }
    if (k.b[1] & 0xF) { const int m = k.b[1] & 0xF; c = ((m + 3) >> 2) + c + m * 4; }
    if (k.b[2] & 0x80) c++;
    if (k.b[3] & 3) c++;
    if ((k.b[3] >> 2) && (k.b[3] >> 2) < 12) c += tab[k.b[3] >> 2];
    if (k.b[2] & 1) c++;
    return c;
}
static void TestNumConsts(int n) {
    g_section = "needconst";
    for (int c = 0; c < n; c++, g_cases++) {
        LightKey k; for (auto& b : k.b) b = static_cast<uint8_t>(Rn());
        k.b[3] &= 0x2F;
        const int a = oracle::Fn<int(__cdecl)(const uint8_t*)>(0x75EDD0)(k.b);
        if (a != NumConsts(k)) Fail("NumLightConstants", c);
    }
}

//--------------------------------------------------------------------------------------------------
// section: the whole render function
//--------------------------------------------------------------------------------------------------
struct Ev {
    int kind;           // 0 const, 1 rs, 2 valpha, 3 indices, 4 streams, 5 decl, 6 ps null, 7 texture, 8 tss, 9 vs, 10 flush, 11 drawIdx, 12 draw
    uint32_t a = 0, b = 0, c = 0, d = 0, e = 0, f = 0;
    std::vector<uint32_t> data;
    bool operator==(const Ev& o) const { return kind == o.kind && a == o.a && b == o.b && c == o.c && d == o.d && e == o.e && f == o.f && data == o.data; }
};
static std::vector<Ev> g_exeLog, g_portLog;
static std::vector<Ev>* g_cur;
static void Log(Ev e) { g_cur->push_back(std::move(e)); }

// the exe-side RW state caches the port's Dev has to mimic (0x8E2440..0x8E2450)
struct ExeCaches { uint32_t ib = 0, decl = 0, vs = 0, ps = 0; };

// ---- exe side recorders
static long __stdcall FdSetConst(void*, unsigned start, const float* data, unsigned count) {
    Ev e{0, start, count}; for (unsigned i = 0; i < count * 4; i++) e.data.push_back(FB(data[i])); Log(e); return 0;
}
static long __stdcall FdSetIndices(void*, void* ib) { Log(Ev{3, U32(ib)}); return 0; }
static long __stdcall FdSetDecl(void*, void* d) { Log(Ev{5, U32(d)}); return 0; }
static long __stdcall FdSetVS(void*, void* v) { Log(Ev{9, U32(v)}); return 0; }
static long __stdcall FdSetPS(void*, void* p) { Log(Ev{6, U32(p)}); return 0; }
static long __stdcall FdDrawIdx(void*, unsigned pt, int base, unsigned minI, unsigned nV, unsigned start, unsigned nP) { Log(Ev{11, pt, static_cast<uint32_t>(base), minI, nV, start, nP}); return 0; }
static long __stdcall FdDraw(void*, unsigned pt, unsigned start, unsigned nP) { Log(Ev{12, pt, start, nP}); return 0; }
static int __cdecl HxSetRS(unsigned s, unsigned v) { Log(Ev{1, s, v}); return 1; }
static int __cdecl HxTSS(unsigned st, unsigned t, unsigned v) { Log(Ev{8, st, t, v}); return 1; }
static int __cdecl HxTex(void* t, unsigned st) { Log(Ev{7, U32(t), st}); return 1; }
static int __cdecl HxVAlpha(int on) { Log(Ev{2, static_cast<uint32_t>(on != 0)}); return 1; }
static void __cdecl HxStreams(void*, int) { Log(Ev{4}); }
static void __cdecl HxFlush() { Log(Ev{10}); }

// GetShader stand-in used on both sides: a deterministic pointer per key and the register layout of 0x75F0B0
static void RegLayout(const uint8_t k[4], uint8_t info[8]) {
    static const int tab[12] = {0, 2, 2, 4, 0, 0, 2, 2, 0, 2, 4, 6};
    std::memset(info, 0xFF, 8);
    int ebx = 5;
    if (k[0] & 0xF) ebx = (k[0] & 0xF) * 2 + 5;
    if (k[0] & 0xF0) { const int c = k[0] >> 4; ebx = ((c + 3) >> 2) + ebx + c * 2; }
    if (k[1] & 0xF) { const int c = k[1] & 0xF; ebx = ((c + 3) >> 2) + ebx + c * 4; }
    if (k[2] >> 7) info[0] = static_cast<uint8_t>(ebx++);
    if ((k[3] & 3) > 0) info[1] = static_cast<uint8_t>(ebx++);
    if (k[2] & 1) info[3] = static_cast<uint8_t>(ebx++);
    const int w = (k[2] >> 1) & 7;
    if (w > 0) info[4] = static_cast<uint8_t>(ebx);
    const int m = k[3] >> 2;
    if (m && m < 12 && tab[m]) { if (w > 0) info[2] = static_cast<uint8_t>(0x100 - tab[m]); else { info[2] = static_cast<uint8_t>(ebx); ebx += tab[m]; } }
}
static void* ShaderFor(const uint8_t k[4]) { uint32_t v; std::memcpy(&v, k, 4); return reinterpret_cast<void*>(static_cast<uintptr_t>(0x00100000u + v)); }
static void* __cdecl HxGetShader(void*, const uint8_t* key, uint8_t* info) { uint32_t kd; std::memcpy(&kd, key, 4); Log(Ev{13, kd}); RegLayout(key, info); return ShaderFor(key); }

// ---- port side device
struct PortDev {
    ExeCaches c;
    uint32_t  ibFromHeader = 0, declFromHeader = 0;
    void SetConst(unsigned start, const float* d, unsigned count) { Ev e{0, start, count}; for (unsigned i = 0; i < count * 4; i++) e.data.push_back(FB(d[i])); Log(e); }
    void SetRenderState(unsigned s, unsigned v) { Log(Ev{1, s, v}); }
    void VertexAlpha(bool on) { Log(Ev{2, on ? 1u : 0u}); }
    void SetIndices(void* ib) { if (U32(ib) != c.ib) { c.ib = U32(ib); Log(Ev{3, c.ib}); } }
    void SetStreams() { Log(Ev{4}); }
    void SetDecl() { if (declFromHeader != c.decl) { c.decl = declFromHeader; Log(Ev{5, c.decl}); } }
    void SetPSNull() { if (c.ps) { c.ps = 0; Log(Ev{6, 0}); } }
    void SetTexture(const void* t) { Log(Ev{7, U32(t), 0}); }
    void SetTSS(unsigned st, unsigned t, unsigned v) { Log(Ev{8, st, t, v}); }
    void SetVS(void* v) { if (U32(v) != c.vs) { c.vs = U32(v); Log(Ev{9, c.vs}); } }
    void Flush() { Log(Ev{10}); }
    void DrawIndexed(unsigned pt, unsigned base, unsigned minI, unsigned nV, unsigned start, unsigned nP) { Log(Ev{11, pt, base, minI, nV, start, nP}); }
    void Draw(unsigned pt, unsigned start, unsigned nP) { Log(Ev{12, pt, start, nP}); }
    void* GetShader(const MeshData&, const LightKey& k, uint8_t info[8]) { uint32_t kd; std::memcpy(&kd, k.b, 4); Log(Ev{13, kd}); RegLayout(k.b, info); return ShaderFor(k.b); }
};

struct FakeDevice { void* vtbl[0x100]; };

static void TestRender(int n) {
    g_section = "render";
    static FakeDevice dev;
    dev.vtbl[0x178 / 4] = reinterpret_cast<void*>(&FdSetConst);
    dev.vtbl[0x1A0 / 4] = reinterpret_cast<void*>(&FdSetIndices);
    dev.vtbl[0x15C / 4] = reinterpret_cast<void*>(&FdSetDecl);
    dev.vtbl[0x170 / 4] = reinterpret_cast<void*>(&FdSetVS);
    dev.vtbl[0x1AC / 4] = reinterpret_cast<void*>(&FdSetPS);
    dev.vtbl[0x148 / 4] = reinterpret_cast<void*>(&FdDrawIdx);
    dev.vtbl[0x144 / 4] = reinterpret_cast<void*>(&FdDraw);
    static void* devPtr;
    devPtr = &dev;                                                    // [0xC97C28] -> pointer to the object whose first dword is the vtable
    static void* devObj[1];
    devObj[0] = dev.vtbl;
    G<void*>(0xC97C28) = devObj;
    oracle::Patch(0x7FC2D0, reinterpret_cast<void*>(&HxSetRS));
    oracle::Patch(0x7FC340, reinterpret_cast<void*>(&HxTSS));
    oracle::Patch(0x7FDE70, reinterpret_cast<void*>(&HxTex));
    oracle::Patch(0x7FE0A0, reinterpret_cast<void*>(&HxVAlpha));
    oracle::Patch(0x7FA090, reinterpret_cast<void*>(&HxStreams));
    oracle::Patch(0x7FC200, reinterpret_cast<void*>(&HxFlush));
    (void)devPtr;
    static void* table[5];
    table[0] = reinterpret_cast<void*>(0x7CB190); table[1] = reinterpret_cast<void*>(0x761170); table[2] = reinterpret_cast<void*>(&HxGetShader);
    table[3] = reinterpret_cast<void*>(0x761030); table[4] = reinterpret_cast<void*>(0x761000);
    int totalEvents = 0, splitCases = 0, droppedCases = 0, nonSplit = 0;
    for (int c = 0; c < n; c++, g_cases++) {
        g_arenaUsed = 0;
        const uint32_t geoFlags = (Chance(60) ? 0x10u : 0u) | (Chance(90) ? 0x20u : 0u) | (Chance(40) ? 0x8u : 0u) | (Chance(50) ? 0x40u : 0u) | (Chance(70) ? 4u : 0u);
        g_spread = 70.0f;
        LightSetup s = MakeLightSetup(geoFlags, 6, 8);
        const uint32_t numTex = static_cast<uint32_t>(RI(0, 2));
        At<uint32_t>(s.geometry, 0x1C) = numTex;
        At<uint32_t>(s.geometry, 0x18) = 1;                          // numMorphTargets == 1: the render entry is geometry + 0x58
        // hierarchy
        static const uint32_t kFlags[] = {0, 2, 0x4000, 0x4002};
        const int nodes = RI(2, 40);
        FakeHier h = MakeHier(nodes, kFlags[RI(0, 3)]);
        RwMatrix atomicLtm = RndMat(Chance(70) ? kOrtho : kGeneral);
        const bool hasWorld = Chance(95);
        At<uint32_t>(g_engine, 4) = hasWorld ? U32(s.world) : 0;
        At<uint32_t>(s.atomic, 4) = U32(MakeFrame(atomicLtm));
        G<uint32_t>(0xC978A4) = 0x10;
        At<uint32_t>(s.atomic, 0x10) = U32(h.hier);
        // camera for the fog constants
        uint8_t* cam = Alloc(0x200);
        At<float>(cam, 0x84) = RF(100, 1000); At<float>(cam, 0x88) = RF(10, 99);
        At<uint32_t>(g_engine, 0) = U32(cam);
        g_fogEnable = Chance(50); g_fogType = RI(0, 4);
        // fog density through the D3D render state 0x26 (0x7FC320 reads the state table at 0xC991D0 + 8 * state)
        const float density = RF(0.0001f, 0.05f);
        G<uint32_t>(0xC991D0 + 8 * 0x26) = FB(density);
        // skin
        const bool split = Chance(75);
        const int boneLimit = RI(4, 40);
        const uint32_t numWeights = static_cast<uint32_t>(RI(1, 4));
        std::vector<uint8_t> used, remap(256), rleCnt, rle;
        for (int i = 0; i < nodes; i++) if (Chance(75)) used.push_back(static_cast<uint8_t>(i));
        if (used.empty()) used.push_back(0);
        uint8_t* skin = Alloc(0x80);
        At<int32_t>(skin, 4) = static_cast<int32_t>(used.size());
        At<uint32_t>(skin, 8) = U32(used.data());
        At<uint32_t>(skin, 0xC) = U32(h.skinToBone.data());
        At<uint32_t>(skin, 0x10) = numWeights;
        At<int32_t>(skin, 0x1C) = boneLimit;
        const int numMeshes = RI(1, 6);
        for (int i = 0; i < 256; i++) remap[i] = static_cast<uint8_t>(RI(0, boneLimit > 0 ? boneLimit - 1 : 0));
        if (split) {
            int pos = 0;
            for (int m = 0; m < numMeshes; m++) {
                const int runs = RI(0, 3);
                rleCnt.push_back(static_cast<uint8_t>(pos));
                rleCnt.push_back(static_cast<uint8_t>(runs));
                for (int r = 0; r < runs; r++, pos++) {
                    const int first = RI(0, nodes - 1);
                    rle.push_back(static_cast<uint8_t>(first));
                    rle.push_back(static_cast<uint8_t>(RI(1, (std::min)(5, nodes - first))));
                }
            }
            At<uint32_t>(skin, 0x20) = static_cast<uint32_t>(numMeshes);
            At<uint32_t>(skin, 0x30) = U32(remap.data());
            At<uint32_t>(skin, 0x34) = U32(rleCnt.data());
            At<uint32_t>(skin, 0x38) = U32(rle.data());
            splitCases++;
        } else {
            At<uint32_t>(skin, 0x20) = 0;
            nonSplit++;
        }
        // resource entry / header / meshes
        uint8_t* res = Alloc(0x40 + 0x24 * numMeshes + 0x40);
        uint8_t* hdr = res + 0x18;
        At<uint32_t>(s.geometry, 0x58) = U32(res);
        At<int32_t>(hdr, 4) = numMeshes;
        const uint32_t ib = Chance(85) ? 0x00A00000u + Rn() % 1000 : 0u;
        const uint32_t decl = 0x00B00000u + Rn() % 1000;
        At<uint32_t>(hdr, 8) = ib;
        At<uint32_t>(hdr, 0xC) = 4;
        At<uint32_t>(hdr, 0x30) = 0;
        At<uint32_t>(hdr, 0x34) = decl;
        std::vector<MeshData> meshes(numMeshes);
        const bool sameMat = Chance(30);
        float baseAmb = RF(0.2f, 1.5f), baseDif = RF(0.2f, 1.5f);
        uint32_t baseCol = Rn();
        for (int m = 0; m < numMeshes; m++) {
            uint8_t* mat = Alloc(0x20);
            const bool tex = Chance(60);
            if (tex) At<uint32_t>(mat, 0) = 0x00C00000u + Rn() % 1000;
            uint32_t col = sameMat ? baseCol : Rn();
            if (Chance(60)) col |= 0xFF000000u;
            At<uint32_t>(mat, 4) = col;
            const float amb = Chance(35) ? 1.0f : (sameMat ? baseAmb : RF(0.0f, 1.5f)), dif = Chance(35) ? 1.0f : (sameMat ? baseDif : RF(0.0f, 1.5f));
            At<float>(mat, 0xC) = amb; At<float>(mat, 0x14) = dif;
            uint8_t* mesh = hdr + 0x40 + 0x24 * m;
            At<uint32_t>(mesh, 8) = U32(mat);
            At<uint32_t>(mesh, 0xC) = Chance(30) ? 1u : 0u;
            At<uint32_t>(mesh, 0x14) = Rn() % 5000; At<uint32_t>(mesh, 0x18) = Rn() % 5000; At<uint32_t>(mesh, 0x1C) = Rn() % 5000; At<uint32_t>(mesh, 0x20) = Rn() % 5000;
            MeshData& md = meshes[m];
            md.material = mat; md.texture = tex ? reinterpret_cast<void*>(static_cast<uintptr_t>(At<uint32_t>(mat, 0))) : nullptr;
            md.color = col; md.ambient = amb; md.diffuse = dif; md.vertexAlpha = At<uint32_t>(mesh, 0xC);
            md.baseIndex = At<uint32_t>(mesh, 0x14); md.numVertices = At<uint32_t>(mesh, 0x18); md.startIndex = At<uint32_t>(mesh, 0x1C); md.numPrimitives = At<uint32_t>(mesh, 0x20);
        }
        // caps
        const unsigned maxConst = Chance(50) ? 256u : static_cast<unsigned>(RI(24, 140));
        G<uint32_t>(0xC978EC) = maxConst;
        const unsigned maxBones = (maxConst - 12) / 3;
        G<uint32_t>(0xC978E8) = maxBones;
        std::vector<float> exeBones(nodes * 12 + 16, 0.0f), portBones(nodes * 12 + 16, 0.0f);
        G<uint32_t>(0xC978AC) = U32(exeBones.data());
        G<uint32_t>(0xC978D4) = 0;
        float view[16], proj[16];
        for (int i = 0; i < 16; i++) { view[i] = RF(-2, 2); proj[i] = RF(-2, 2); }
        std::memcpy(reinterpret_cast<void*>(0xC9BC80), view, 64);
        std::memcpy(reinterpret_cast<void*>(0x8E2458), proj, 64);
        G<uint32_t>(0xC97C64) = 0;
        std::memset(reinterpret_cast<void*>(0xC970A0), 0xAB, 0x800);
        G<uint32_t>(0x8E2450) = 0; G<uint32_t>(0x8E2444) = 0; G<uint32_t>(0x8E2448) = 0; G<uint32_t>(0x8E244C) = 0; G<uint32_t>(0x8E2440) = 0;
        // ---- exe
        g_exeLog.clear(); g_cur = &g_exeLog;
        oracle::Fn<void(__cdecl)(void*, void*, void*, void*, void*)>(0x7C8060)(table, s.atomic, s.geometry, skin, h.hier);
        // ---- port
        g_portLog.clear(); g_cur = &g_portLog;
        WorldXform wx;
        alignas(16) static float constBuf[256 * 4 + 16];
        std::memset(constBuf, 0xAB, sizeof(constBuf));
        Env e;
        e.world = &wx; e.atomicLTM = &atomicLtm; e.view = view; e.proj = proj;
        e.geometryFlags = geoFlags; e.numTexCoordSets = numTex; e.fogEnabled = g_fogEnable != 0; e.fogType = static_cast<u32>(g_fogType);
        e.fogDensity = density; e.camFar = At<float>(cam, 0x84); e.camFogPlane = At<float>(cam, 0x88);
        e.haveWorld = hasWorld; e.lights = ViewOf(s);
        e.skin.boneLimit = boneLimit; e.skin.numUsedBones = static_cast<int>(used.size()); e.skin.usedBones = used.data(); e.skin.numWeights = static_cast<int>(numWeights);
        if (split) { e.skin.remap = remap.data(); e.skin.rleCount = rleCnt.data(); e.skin.rle = rle.data(); }
        e.bones = h.src;
        e.indexBuffer = reinterpret_cast<void*>(static_cast<uintptr_t>(ib)); e.primType = 4;
        e.numMeshes = numMeshes; e.meshes = meshes.data();
        e.maxBones = maxBones; e.maxConstants = maxConst; e.numLightConstants = &NumConsts;
        e.constBuf = constBuf; e.boneBuf = portBones.data();
        PortDev pd;
        pd.declFromHeader = decl;
        RenderHW(pd, e);
        totalEvents += static_cast<int>(g_exeLog.size());
        {
            int shaders = 0, bone = 0;
            for (const Ev& ev : g_exeLog) { if (ev.kind == 13) shaders++; }
            (void)bone;
            if (shaders > numMeshes) g_stat[6]++;                                  // the light budget loop ran
            if (!split && static_cast<unsigned>(nodes) > maxBones) g_stat[7]++;     // packed bone registers
            else if (!split && used.size() < static_cast<size_t>(nodes)) g_stat[8]++; // sparse
            else if (!split) g_stat[9]++;                                          // everything at once
            if (g_fogEnable) g_stat[10]++;
            if (hasWorld && (geoFlags & 0x20)) g_stat[11]++;
        }
        bool dropped = false;
        for (const Ev& ev : g_exeLog) if (ev.kind == 0 && ev.a == 4 && c >= 0) dropped = dropped || false;
        (void)dropped; (void)droppedCases;
        if (g_exeLog.size() != g_portLog.size()) {
            Fail("event count", c);
            if (Detail()) std::printf("    exe %zu events, port %zu\n", g_exeLog.size(), g_portLog.size());
            continue;
        }
        for (size_t i = 0; i < g_exeLog.size(); i++) {
            if (!(g_exeLog[i] == g_portLog[i])) {
                Fail("event differs", c);
                if (Detail()) std::printf("    #%zu kind %d/%d a=%X/%X b=%X/%X c=%X/%X d=%X/%X data %zu/%zu\n", i, g_exeLog[i].kind, g_portLog[i].kind, g_exeLog[i].a, g_portLog[i].a,
                                                 g_exeLog[i].b, g_portLog[i].b, g_exeLog[i].c, g_portLog[i].c, g_exeLog[i].d, g_portLog[i].d, g_exeLog[i].data.size(), g_portLog[i].data.size());
                if (Detail()) {
                    for (size_t k = (i > 6 ? i - 6 : 0); k <= i + 1 && k < g_exeLog.size(); k++)
                        std::printf("      [%zu] exe kind %d a=%X b=%X c=%X d=%X | port kind %d a=%X b=%X c=%X d=%X\n", k, g_exeLog[k].kind, g_exeLog[k].a, g_exeLog[k].b, g_exeLog[k].c, g_exeLog[k].d,
                                    g_portLog[k].kind, g_portLog[k].a, g_portLog[k].b, g_portLog[k].c, g_portLog[k].d);
                    std::printf("      split=%d nodes=%d meshes=%d used=%zu maxConst=%u boneLimit=%d\n", split, nodes, numMeshes, used.size(), maxConst, boneLimit);
                    std::printf("      used:"); for (auto u : used) std::printf(" %d", u); std::printf("\n");
                    for (size_t k = 0; k < g_exeLog.size() && k < 24; k++)
                        std::printf("      full[%zu] exe kind %d a=%X b=%X c=%X | port kind %d a=%X b=%X c=%X\n", k, g_exeLog[k].kind, g_exeLog[k].a, g_exeLog[k].b, g_exeLog[k].c,
                                    k < g_portLog.size() ? g_portLog[k].kind : -1, k < g_portLog.size() ? g_portLog[k].a : 0, k < g_portLog.size() ? g_portLog[k].b : 0, k < g_portLog.size() ? g_portLog[k].c : 0);
                }
                if (Detail() && g_exeLog[i].data.size() == g_portLog[i].data.size()) {
                    for (size_t k = 0; k < g_exeLog[i].data.size(); k++)
                        if (g_exeLog[i].data[k] != g_portLog[i].data[k]) { std::printf("      data[%zu] exe %08X port %08X (reg %zu)\n", k, g_exeLog[i].data[k], g_portLog[i].data[k], k / 4); break; }
                }
                break;
            }
        }
    }
    std::printf("  render: %d cases, %d device events compared (%d split skins, %d unsplit)\n", n, totalEvents, splitCases, nonSplit);
}

//--------------------------------------------------------------------------------------------------
int main(int argc, char** argv) {
    int n = 3000;
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
    struct Sec { const char* name; void (*fn)(int); } secs[] = {
        {"bones", TestBones}, {"worldxf", TestWorld}, {"lights", TestLights}, {"needconst", TestNumConsts}, {"render", TestRender},
    };
    if (want("begin")) {
        int before = g_mismatch;
        TestBegin();
        std::printf("[begin] %s\n", g_mismatch == before ? "ok" : "MISMATCH");
    }
    for (auto& s : secs) {
        if (!want(s.name)) continue;
        const int before = g_mismatch, cb = g_cases;
        oracle::g_where = s.name;
        g_secStart = g_mismatch;
        s.fn(n);
        std::printf("[%s] %d cases, %d mismatches\n", s.name, g_cases - cb, g_mismatch - before);
    }
    std::printf("coverage: lights cases with dir %d / point %d / spot %d / all three %d / >4 point or spot %d / soft spot below 0.999: %d spots;\n", g_stat[0], g_stat[1], g_stat[2], g_stat[3], g_stat[4], g_stat[5]);
    std::printf("          render cases: budget loop ran %d, packed bones %d, sparse %d, full %d, fog on %d, lit %d\n", g_stat[6], g_stat[7], g_stat[8], g_stat[9], g_stat[10], g_stat[11]);
    std::printf("TOTAL: %d cases, %d mismatches\n", g_cases, g_mismatch);
    return g_mismatch ? 1 : 0;
}

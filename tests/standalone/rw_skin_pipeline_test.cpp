// P2B-24 unit test: the exe's skin HW pipeline (pipeline_skin.cpp + skin_vs.cpp) on a real D3D9 device (Wine/wined3d is fine), pixel read-back.
//  1. caps / install: the skin pipeline slot is used by the atomic, the composer picks a shader model, SA's male01.dff has split skin data (HW path)
//  2. -t prints the vertex shader text composed for the key of the test scene
//  3. male01.dff (argv path) in the BIND POSE, with one bone rotated, and in the bind pose again: ambient light only; the CPU reference skins every vertex
//     (sum of weight x (v x skinToBone x boneMatrix), x atomic LTM, into camera space), projects it and rasterises a coverage mask; pixels well inside / outside
//     the mask must be drawn / background on the device. The rotated pose must differ from the bind pose and bind-pose renders must be identical.
// Usage: rw_skin_pipeline_test.exe [-v] [-t] path\to\male01.dff. Needs a D3D9 HAL adapter. Exit code 0 = all checks passed.
#include "fakerw.h"
#include <src/d3d/rwd3dimpl.h>

#include "standalone/rw/skin_vs.h"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace notsa::standalone::Fixups { void Log(const char*, ...) {} }
void NotsaRwRenderState_OnEngineStarted();
void RwShimPipelineEnsure();

static int g_fail = 0, g_pass = 0;
static bool g_verbose = false;
#define CHECK(c) do { const bool ok_ = !!(c); if (ok_) ++g_pass; else ++g_fail; if (!ok_ || g_verbose) std::printf("%-4s %s\n", ok_ ? "ok" : "FAIL", #c); } while (0)
#define CHECKV(c, fmt, ...) do { const bool ok_ = !!(c); if (ok_) ++g_pass; else ++g_fail; if (!ok_ || g_verbose) std::printf("%-4s %s   " fmt "\n", ok_ ? "ok" : "FAIL", #c, __VA_ARGS__); } while (0)

static IDirect3DDevice9* g_dev = nullptr;
static RwCamera* g_cam;
static int g_w, g_h;

static std::vector<unsigned> ReadBack() {
    std::vector<unsigned> px;
    IDirect3DSurface9 *rt = nullptr, *sys = nullptr;
    if (SUCCEEDED(g_dev->GetRenderTarget(0, &rt))) {
        D3DSURFACE_DESC d{};
        rt->GetDesc(&d);
        if (SUCCEEDED(g_dev->CreateOffscreenPlainSurface(d.Width, d.Height, d.Format, D3DPOOL_SYSTEMMEM, &sys, nullptr)) && SUCCEEDED(g_dev->GetRenderTargetData(rt, sys))) {
            D3DLOCKED_RECT lr{};
            if (SUCCEEDED(sys->LockRect(&lr, nullptr, D3DLOCK_READONLY))) {
                px.resize(size_t(d.Width) * d.Height);
                for (UINT y = 0; y < d.Height; y++) std::memcpy(&px[size_t(y) * d.Width], (char*)lr.pBits + y * lr.Pitch, d.Width * 4);
                sys->UnlockRect();
            }
        }
        if (sys) sys->Release();
        rt->Release();
    }
    return px;
}

static void SetupCamera() {
    const auto& pp = rw::d3d::d3d9Globals.present;
    g_w = pp.BackBufferWidth; g_h = pp.BackBufferHeight;
    g_cam = RwCameraCreate();
    RwCameraSetFrame(g_cam, RwFrameCreate());
    RwCameraSetRaster(g_cam, RwRasterCreate(g_w, g_h, 0, rwRASTERTYPECAMERA));
    RwCameraSetZRaster(g_cam, RwRasterCreate(g_w, g_h, 0, rwRASTERTYPEZBUFFER));
    RwV2d vw{ 0.5f, 0.5f };
    RwCameraSetViewWindow(g_cam, &vw);
    RwCameraSetNearClipPlane(g_cam, 0.1f);
    RwCameraSetFarClipPlane(g_cam, 100.0f);
}

static std::vector<uint8_t> ReadFile(const char* path) {
    std::vector<uint8_t> b;
    if (FILE* f = std::fopen(path, "rb")) {
        std::fseek(f, 0, SEEK_END); b.resize(std::ftell(f)); std::fseek(f, 0, SEEK_SET);
        if (std::fread(b.data(), 1, b.size(), f) != b.size()) b.clear();
        std::fclose(f);
    }
    return b;
}

static RpAtomic* FirstAtomicCB_(RpAtomic* a, void* d) { *(RpAtomic**)d = a; return nullptr; }

struct V3 { float x, y, z; };
static V3 XformRow(const RwMatrix& m, V3 p) { // RW row-vector convention
    return { p.x * m.right.x + p.y * m.up.x + p.z * m.at.x + m.pos.x, p.x * m.right.y + p.y * m.up.y + p.z * m.at.y + m.pos.y, p.x * m.right.z + p.y * m.up.z + p.z * m.at.z + m.pos.z };
}

struct Mask { std::vector<uint8_t> m; int w, h; };
static Mask Rasterise(const std::vector<V3>& cs, RpGeometry* geo) {
    Mask r{ std::vector<uint8_t>(size_t(g_w) * g_h, 0), g_w, g_h };
    std::vector<float> sx(cs.size()), sy(cs.size());
    for (size_t i = 0; i < cs.size(); i++) {
        const float iz = 1.0f / cs[i].z;
        sx[i] = (cs[i].x * iz + 1.0f) * 0.5f * g_w;   // view window 0.5: ndc = (x/z) / 0.5
        sy[i] = (-cs[i].y * iz + 1.0f) * 0.5f * g_h;
    }
    for (int t = 0; t < geo->numTriangles; t++) {
        const RpTriangle& tr = RpGeometryGetTriangles(geo)[t];
        const int a = tr.v[0], b = tr.v[1], c = tr.v[2];
        if (cs[a].z < 0.2f || cs[b].z < 0.2f || cs[c].z < 0.2f) continue;
        const int x0 = std::max(0, (int)std::floor(std::min({ sx[a], sx[b], sx[c] }))), x1 = std::min(g_w - 1, (int)std::ceil(std::max({ sx[a], sx[b], sx[c] })));
        const int y0 = std::max(0, (int)std::floor(std::min({ sy[a], sy[b], sy[c] }))), y1 = std::min(g_h - 1, (int)std::ceil(std::max({ sy[a], sy[b], sy[c] })));
        for (int y = y0; y <= y1; y++)
            for (int x = x0; x <= x1; x++) {
                const float px = x + 0.5f, py = y + 0.5f;
                auto E = [](float ax, float ay, float bx, float by, float cx, float cy) { return (cx - ax) * (by - ay) - (cy - ay) * (bx - ax); };
                const float e0 = E(sx[a], sy[a], sx[b], sy[b], px, py), e1 = E(sx[b], sy[b], sx[c], sy[c], px, py), e2 = E(sx[c], sy[c], sx[a], sy[a], px, py);
                if ((e0 >= 0 && e1 >= 0 && e2 >= 0) || (e0 <= 0 && e1 <= 0 && e2 <= 0)) r.m[size_t(y) * g_w + x] = 1;
            }
    }
    return r;
}
static bool Uniform(const Mask& m, int x, int y, int r, uint8_t v) {
    for (int j = -r; j <= r; j++)
        for (int i = -r; i <= r; i++) {
            const int xx = x + i, yy = y + j;
            if (xx < 0 || yy < 0 || xx >= m.w || yy >= m.h || m.m[size_t(yy) * m.w + xx] != v) return false;
        }
    return true;
}

struct Result { int covered = 0, background = 0, badCovered = 0, badBackground = 0; unsigned sampleColor = 0; };
static Result Compare(const std::vector<unsigned>& px, const Mask& m, unsigned clear) {
    Result r;
    for (int y = 0; y < g_h; y++)
        for (int x = 0; x < g_w; x++) {
            const unsigned p = px[size_t(y) * g_w + x] & 0xFFFFFF;
            if (Uniform(m, x, y, 2, 1)) {
                r.covered++;
                if (p == clear) r.badCovered++; else r.sampleColor = p;
            } else if (Uniform(m, x, y, 2, 0)) {
                r.background++;
                if (p != clear) r.badBackground++;
            }
        }
    return r;
}

static RpAtomic* g_atomic;
static RpGeometry* g_geo;
static RpHAnimHierarchy* g_hier;

// LOCALSPACEMATRICES: bone = skinToBone x hier[i]; hier[i] = inverse(skinToBone[i]) is the bind pose (bone = identity)
static void SetPose(int bone, float angleDeg) {
    RwMatrix* mats = RpHAnimHierarchyGetMatrixArray(g_hier);
    RpSkin* skin = RpSkinGeometryGetSkin(g_geo);
    const RwMatrix* inv = RpSkinGetSkinToBoneMatrices(skin);
    for (int i = 0; i < g_hier->numNodes; i++) {
        RwMatrix b;
        rw::Matrix::invert(&b, const_cast<RwMatrix*>(&inv[i]));
        mats[i] = b;
    }
    if (bone >= 0) {
        RwMatrix rot; rot.setIdentity();
        const float a = angleDeg * 3.14159265f / 180.0f, c = std::cos(a), s = std::sin(a);
        rot.right = { c, s, 0 }; rot.up = { -s, c, 0 }; rot.at = { 0, 0, 1 };
        RwMatrix out;
        rw::Matrix::mult(&out, &rot, &mats[bone]);
        mats[bone] = out;
    }
}

static std::vector<V3> CpuSkin(const RwMatrix& atomicLtm, const RwMatrix& cam) {
    RpSkin* skin = RpSkinGeometryGetSkin(g_geo);
    const RwMatrix* inv = RpSkinGetSkinToBoneMatrices(skin);
    const RwUInt32* bi = RpSkinGetVertexBoneIndices(skin);
    RwMatrixWeights* bw = RpSkinGetVertexBoneWeights(skin);
    RwMatrix* mats = RpHAnimHierarchyGetMatrixArray(g_hier);
    std::vector<RwMatrix> bone(g_hier->numNodes);
    for (int i = 0; i < g_hier->numNodes; i++) rw::Matrix::mult(&bone[i], const_cast<RwMatrix*>(&inv[i]), &mats[i]);
    const V3* src = (const V3*)RpMorphTargetGetVertices(RpGeometryGetMorphTarget(g_geo, 0));
    std::vector<V3> out(g_geo->numVertices);
    RwMatrix wc; rw::Matrix::invert(&wc, const_cast<RwMatrix*>(&cam));
    for (int v = 0; v < g_geo->numVertices; v++) {
        const float* w = &bw[v].w0;
        const uint8_t* ix = reinterpret_cast<const uint8_t*>(&bi[v]);
        V3 acc{ 0, 0, 0 };
        for (int k = 0; k < 4; k++) {
            if (w[k] == 0.0f) continue;
            const V3 p = XformRow(bone[ix[k]], src[v]);
            acc.x += w[k] * p.x; acc.y += w[k] * p.y; acc.z += w[k] * p.z;
        }
        out[v] = XformRow(wc, XformRow(atomicLtm, acc));
    }
    return out;
}

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    _set_error_mode(_OUT_TO_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    const char* dffPath = nullptr;
    bool dumpText = false;
    for (int i = 1; i < argc; i++) {
        if (!std::strcmp(argv[i], "-v")) g_verbose = true; else if (!std::strcmp(argv[i], "-t")) dumpText = true; else dffPath = argv[i];
    }
    HWND wnd = CreateWindowA("STATIC", "rw_skin_pipeline_test", WS_OVERLAPPEDWINDOW, 0, 0, 320, 240, nullptr, nullptr, GetModuleHandleA(nullptr), nullptr);
    rw::MemoryFunctions mf{};
    mf.rwmalloc  = [](size_t sz, unsigned) -> void* { return sz ? malloc(sz) : nullptr; };
    mf.rwrealloc = [](void* p, size_t sz, unsigned) -> void* { return realloc(p, sz); };
    mf.rwfree    = [](void* p) { free(p); };
    CHECK(rw::Engine::init(&mf));
    CHECK(RpWorldPluginAttach() == TRUE);
    CHECK(RpSkinPluginAttach() == TRUE);
    CHECK(RpHAnimPluginAttach() == TRUE);
    rw::registerAnisotropyPlugin();
    rw::EngineOpenParams params{};
    params.window = wnd;
    CHECK(rw::Engine::open(&params));
    rw::Engine::start();
    g_dev = rw::d3d::d3ddevice;
    if (g_dev) NotsaRwRenderState_OnEngineStarted();
    std::printf("D3D9 device: %s\n", g_dev ? "yes" : "NO (nothing testable without a device)");
    if (!g_dev) return 1;
    RwRenderStateSet(rwRENDERSTATECULLMODE, reinterpret_cast<void*>(uintptr_t(rwCULLMODECULLNONE)));
    RwEngineInstance->dOpenDevice.zBufferNear = 0; RwEngineInstance->dOpenDevice.zBufferFar = 1;
    SetupCamera();
    RwShimPipelineEnsure();

    const D3DCAPS9& caps = rw::d3d::d3d9Globals.caps;
    std::printf("caps: VS %08X PS %08X maxVSC %u model 0x%X\n", caps.VertexShaderVersion, caps.PixelShaderVersion, caps.MaxVertexShaderConst, notsa::skinvs::Model());
    CHECK(notsa::skinvs::Model() != 0);
    rw::ObjPipeline* slot = rw::skinGlobals.pipelines[rw::PLATFORM_D3D9];
    CHECK(slot != nullptr);
    {
        // 1 sets of uv, 4 weights, normals, modulate material colour, no lights
        notsa::skinvs::Key k{ { 0x00, 0x10, uint8_t((4 << 1) | 0x20 | 0x80), 0x00 } };
        notsa::skinvs::Layout lay{};
        CHECK(notsa::skinvs::GetVertexShader(k, lay) != nullptr);
        if (dumpText) std::printf("---- vs text:\n%s\n----\n", notsa::skinvs::ComposeText(k, lay).c_str());
    }

    if (dffPath) {
        auto bytes = ReadFile(dffPath);
        CHECK(!bytes.empty());
        if (bytes.empty()) return 1;
        rw::StreamMemory ms;
        ms.open(bytes.data(), (uint32_t)bytes.size());
        uint32_t len = 0, ver = 0;
        CHECK(rw::findChunk(&ms, rw::ID_CLUMP, &len, &ver));
        RpClump* clump = RpClumpStreamRead(&ms);
        CHECK(clump != nullptr);
        if (!clump) return 1;
        RpClumpForAllAtomics(clump, FirstAtomicCB_, &g_atomic);
        g_geo = RpAtomicGetGeometry(g_atomic);
        RpSkin* skin = RpSkinGeometryGetSkin(g_geo);
        CHECK(skin != nullptr);
        std::printf("skin: %d bones, %d used, numWeights %d, numMeshes %d, boneLimit %d, geometry flags %X, atomic pipeline %p (skin slot %p)\n", skin->numBones, skin->numUsedBones,
                    skin->numWeights, skin->numMeshes, skin->boneLimit, g_geo->flags, (void*)g_atomic->pipeline, (void*)slot);
        CHECK(g_atomic->pipeline == slot);
        CHECK(skin->numMeshes != 0);
        RwFrame* cf = RpClumpGetFrame(clump);
        g_hier = RpHAnimFrameGetHierarchy(cf);
        if (!g_hier) {
            struct S { RpHAnimHierarchy* h; } s{ nullptr };
            RwFrameForAllChildren(cf, [](RwFrame* f, void* d) -> RwFrame* { auto* s = (S*)d; if (!s->h) s->h = RpHAnimFrameGetHierarchy(f); return f; }, &s);
            g_hier = s.h;
        }
        CHECK(g_hier != nullptr);
        if (!g_hier) return 1;
        g_hier->flags = (RpHAnimHierarchyFlag)(rw::HAnimHierarchy::LOCALSPACEMATRICES);
        RpSkinAtomicSetHAnimHierarchy(g_atomic, g_hier);
        RwV3d t{ 0.0f, 0.0f, 7.0f };
        RwFrameTranslate(cf, &t, rwCOMBINEREPLACE);

        RpWorld* world = RpWorldCreate(nullptr);
        RpLight* amb = RpLightCreate(rpLIGHTAMBIENT);
        RwRGBAReal col{ 0.5f, 0.25f, 0.75f, 1.0f };
        RpLightSetColor(amb, &col);
        RpWorldAddLight(world, amb);
        RpWorldAddCamera(world, g_cam);

        RwRGBA clear{ 0, 0, 0, 255 };
        std::vector<unsigned> imgs[3];
        const int poses[3][2] = { { -1, 0 }, { 5, 70 }, { -1, 0 } };
        for (int pi = 0; pi < 3; pi++) {
            SetPose(poses[pi][0], float(poses[pi][1]));
            RwCameraBeginUpdate(g_cam);
            RwCameraClear(g_cam, &clear, rwCAMERACLEARIMAGE | rwCAMERACLEARZ);
            RpAtomicRender(g_atomic);
            RwCameraEndUpdate(g_cam);
            imgs[pi] = ReadBack();
            const RwMatrix* altm = RwFrameGetLTM(RpAtomicGetFrame(g_atomic));
            const RwMatrix* camltm = RwFrameGetLTM(RwCameraGetFrame(g_cam));
            Mask m = Rasterise(CpuSkin(*altm, *camltm), g_geo);
            Result r = Compare(imgs[pi], m, 0);
            std::printf("pose %d (bone %d, %d deg): covered %d (bad %d), background %d (bad %d), sample colour %06X\n", pi, poses[pi][0], poses[pi][1], r.covered, r.badCovered,
                        r.background, r.badBackground, r.sampleColor);
            CHECKV(r.covered > 300, "covered samples %d", r.covered);
            CHECKV(r.badCovered * 100 <= r.covered, "pose %d: covered pixels left black %d / %d", pi, r.badCovered, r.covered);
            CHECKV(r.badBackground * 100 <= std::max(1, r.background), "pose %d: background pixels drawn %d / %d", pi, r.badBackground, r.background);
        }
        size_t diff01 = 0, diff02 = 0;
        for (size_t i = 0; i < imgs[0].size(); i++) { diff01 += imgs[0][i] != imgs[1][i]; diff02 += imgs[0][i] != imgs[2][i]; }
        std::printf("pixels differing bind vs rotated: %zu, bind vs bind: %zu\n", diff01, diff02);
        CHECK(diff01 > 50);
        CHECK(diff02 == 0);
        RpWorldRemoveCamera(world, g_cam);
        RpWorldRemoveLight(world, amb);
        RpLightDestroy(amb);
        RpWorldDestroy(world);
    }
    std::printf("%s: %d passed, %d failed\n", g_fail ? "FAILED" : "PASSED", g_pass, g_fail);
    return g_fail ? 1 : 0;
}

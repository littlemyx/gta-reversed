// P2B-08 unit test: the game's four custom pipelines + their plugins on the RxPipeline façade (P2B-07) and the FF device layer (P2B-02c).
// The REAL game TUs (CustomBuildingPipeline / CustomBuildingDNPipeline / CustomBuildingRenderer / CustomCarEnvMapPipeline(+EnvMat/EnvAtm/SpecMat) /
// PipelinePlugin) are compiled into this exe with the game's flags and PCH; only the symbols they call outside RenderWare are stubbed here
// (CMemoryMgr, g_fx quality, the hook registry, CVector helpers, CVisibilityPlugins::GetAtomicId, 05b's matfx accessors) and the data image globals they
// read through StaticRef<T>(addr) live in the exe's own 9 MB placeholder image (the same orig_image_pad.obj as the game exe: original VAs 8D0000.., C00000.., C80000..).
//  1. plugin registration (ExtraVertColour 0x253F2F9, EnvMat 0x253F2FC, EnvAtm 0x253F2F4, SpecMat 0x253F2F6, pipeline id 0x253F2F3), pipe creation, node callbacks
//  2. CCustomBuildingPipeline: prelit pixels; env stage 1 (MULTIPLYADD current + tex * TFACTOR) with exact expected pixels and TFACTOR readback
//  3. CCustomBuildingDNPipeline: day (m_fDNBalanceParam 0) vs night (1) colour blend written by PreRenderUpdate -> reinstance -> pixels; exact exe truncation
//  4. CCustomCarEnvMapPipeline: env stage (TFACTOR = specIntensity scaled), specular light (g_GameLight on slot 1) on vs off -> pixel differs; material block
//  5. optional argv[1..] = SA DFFs: vgsnbuild07.dff (ExtraVertColour chunk -> night vs day frames), infernus.dff (EnvMat/SpecMat chunks, specular on vs off)
// Needs a D3D9 HAL adapter (works under Wine/wined3d). Exit code 0 = all checks passed.
#include "CustomBuildingRenderer.h"
#include "CustomBuildingDNPipeline.h"
#include "CustomBuildingPipeline.h"
#include "CustomCarEnvMapPipeline.h"
#include "Plugins/PipelinePlugin/PipelinePlugin.h"
#include "app_light.h"
#include "Fx/Fx.h"
#include "standalone/Fixups.h"
#include "MemoryMgr.h"
#include "VisibilityPlugins.h"

#include <src/d3d/rwd3dimpl.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------------------------------------------------------------
void NVCPipelineProcess(RpAtomic* a, float DNBalance); // CustomBuildingDNPipeline.cpp (global, not in the header)
// stubs of the game-side symbols the pipeline TUs call
namespace notsa::standalone::detail { bool g_DataImageLoaded = true; }
namespace notsa::standalone::Fixups {
void RegisterFunction(uint32_t, void*, const char*) {}
void RegisterUnverified(uint32_t, const char*, int, bool, uint32_t) {}
}
void* CMemoryMgr::Malloc(uint32 size, uint32) { return std::malloc(size); }
void* CMemoryMgr::Malloc(uint32 size) { return std::malloc(size); }
void ReversibleHooks::RHManager::AddHookToCategory(std::string_view, HookInstallOptions, std::shared_ptr<ReversibleHook::TwoWayHook>) {} // InjectHooks() is never called here
void  CMemoryMgr::Free(void* p) { std::free(p); }
static Fx_c s_fxStub;
Fx_c&         g_fx = s_fxStub;
static FxQuality_e g_fxQuality = FX_QUALITY_HIGH;
FxQuality_e Fx_c::GetFxQuality() const { return g_fxQuality; }
static int32 g_atomicId = 0;
int32 CVisibilityPlugins::GetAtomicId(RpAtomic*) { return g_atomicId; }
CVector2D::CVector2D(const CVector& v) { x = v.x; y = v.y; }
float CVector::NormaliseAndMag() {
    const float m = std::sqrt(x * x + y * y + z * z);
    if (m > 0.f) { x /= m; y /= m; z /= m; } else { x = 1.f; }
    return m;
}

// 05b (matfx.cpp, running in parallel) is not part of this tree: the three accessors the pipelines call are stubbed from librw's own MatFX
// plugin (rw::MatFX is registered by the engine); they are only needed to resolve the symbols, the tests drive the game's env data directly
RpMatFXMaterialFlags RpMatFXMaterialGetEffects(const RpMaterial*) { return rpMATFXEFFECTNULL; }
RwTexture*           RpMatFXMaterialGetEnvMapTexture(const RpMaterial*) { return nullptr; }
MatFXEnvMapData*     MatFXD3D9EnvMapGetData(RpMaterial*, RwInt32) { static MatFXEnvMapData d{}; return &d; }

// ---------------------------------------------------------------------------------------------------------------------------------
static int g_fail = 0, g_pass = 0;
static bool g_verbose = false;
#define CHECK(c) do { const bool ok_ = !!(c); if (ok_) ++g_pass; else ++g_fail; if (!ok_ || g_verbose) std::printf("%-4s %s\n", ok_ ? "ok" : "FAIL", #c); } while (0)
#define CHECKV(c, fmt, ...) do { const bool ok_ = !!(c); if (ok_) ++g_pass; else ++g_fail; if (!ok_ || g_verbose) std::printf("%-4s %s   " fmt "\n", ok_ ? "ok" : "FAIL", #c, __VA_ARGS__); } while (0)
#define INFO(fmt, ...) std::printf("  " fmt "\n", __VA_ARGS__)

static IDirect3DDevice9* g_dev = nullptr;
static int g_w, g_h;
static RwCamera* g_cam;
static RwFrame*  g_camFrame;
static RwRaster *g_fb, *g_zb;
static RpWorld*  g_world;
static RpLight  *g_amb, *g_dir;
static RwFrame*  g_dirFrame;

static void MapDataImage() {
    // the original data image lives at fixed addresses, the pipelines read/write it through StaticRef<T>(addr)
    struct R { uintptr_t a; size_t n; } rs[] = { { 0x8D0000, 0x10000 }, { 0xC00000, 0x10000 }, { 0xC80000, 0x10000 } };
    for (auto& r : rs) {
        DWORD old = 0;
        if (!VirtualProtect((void*)r.a, r.n, PAGE_READWRITE, &old)) {
            MEMORY_BASIC_INFORMATION mbi{};
            VirtualQuery((void*)r.a, &mbi, sizeof(mbi));
            std::printf("FATAL: cannot make the data image at %p writable (error %lu; region base %p size %zx; exe image %p)\n", (void*)r.a, GetLastError(), mbi.AllocationBase, (size_t)mbi.RegionSize, (void*)GetModuleHandleA(nullptr));
            std::exit(2);
        }
        std::memset((void*)r.a, 0, r.n);
    }
    // initial values of the exe's .data (DN pipeline: 1.0 / -1 ; car: offsets -1, gSpecIntensity 1.0)
    StaticRef<float>(0x8D12C0)  = 1.0f;     // m_fDNBalanceParam
    StaticRef<int32>(0x8D12BC)  = -1;       // ms_extraVertColourPluginOffset
    StaticRef<int32>(0x8D12C4)  = -1;       // env map material
    StaticRef<int32>(0x8D12C8)  = -1;       // env map atomic
    StaticRef<int32>(0x8D12CC)  = -1;       // specular map material
    StaticRef<float>(0x8D12D0)  = 1.0f;     // gSpecIntensity
    StaticRef<int32>(0x8D6080)  = -1;       // pipeline plugin offset
}

static unsigned ReadPixel(int x, int y) {
    IDirect3DSurface9 *rt = nullptr, *sys = nullptr;
    unsigned px = 0xDEADBEEF;
    if (SUCCEEDED(g_dev->GetRenderTarget(0, &rt))) {
        D3DSURFACE_DESC d{};
        rt->GetDesc(&d);
        if (SUCCEEDED(g_dev->CreateOffscreenPlainSurface(d.Width, d.Height, d.Format, D3DPOOL_SYSTEMMEM, &sys, nullptr)) && SUCCEEDED(g_dev->GetRenderTargetData(rt, sys))) {
            D3DLOCKED_RECT lr{};
            if (SUCCEEDED(sys->LockRect(&lr, nullptr, D3DLOCK_READONLY))) {
                px = *(unsigned*)((char*)lr.pBits + y * lr.Pitch + x * 4) | 0xFF000000u;
                sys->UnlockRect();
            }
        }
        if (sys) sys->Release();
        rt->Release();
    }
    return px;
}
static std::vector<unsigned> ReadFrame() {
    std::vector<unsigned> out;
    IDirect3DSurface9 *rt = nullptr, *sys = nullptr;
    if (SUCCEEDED(g_dev->GetRenderTarget(0, &rt))) {
        D3DSURFACE_DESC d{};
        rt->GetDesc(&d);
        if (SUCCEEDED(g_dev->CreateOffscreenPlainSurface(d.Width, d.Height, d.Format, D3DPOOL_SYSTEMMEM, &sys, nullptr)) && SUCCEEDED(g_dev->GetRenderTargetData(rt, sys))) {
            D3DLOCKED_RECT lr{};
            if (SUCCEEDED(sys->LockRect(&lr, nullptr, D3DLOCK_READONLY))) {
                out.resize((size_t)d.Width * d.Height);
                for (UINT y = 0; y < d.Height; y++) {
                    std::memcpy(&out[(size_t)y * d.Width], (char*)lr.pBits + (size_t)y * lr.Pitch, d.Width * 4);
                }
                sys->UnlockRect();
            }
        }
        if (sys) sys->Release();
        rt->Release();
    }
    return out;
}
static bool NearC(unsigned px, int r, int g, int b, int tol = 3) {
    return std::abs(int((px >> 16) & 0xFF) - r) <= tol && std::abs(int((px >> 8) & 0xFF) - g) <= tol && std::abs(int(px & 0xFF) - b) <= tol;
}
static int Ch(unsigned px, int shift) { return (px >> shift) & 0xFF; }

static void SetupScene() {
    const auto& pp = rw::d3d::d3d9Globals.present;
    g_w = pp.BackBufferWidth; g_h = pp.BackBufferHeight;
    g_cam = RwCameraCreate();
    g_camFrame = RwFrameCreate();
    RwCameraSetFrame(g_cam, g_camFrame);
    g_fb = RwRasterCreate(g_w, g_h, 0, rwRASTERTYPECAMERA);
    g_zb = RwRasterCreate(g_w, g_h, 0, rwRASTERTYPEZBUFFER);
    RwCameraSetRaster(g_cam, g_fb);
    RwCameraSetZRaster(g_cam, g_zb);
    RwV2d vw{ 0.5f, 0.5f };
    RwCameraSetViewWindow(g_cam, &vw);
    RwCameraSetNearClipPlane(g_cam, 0.1f);
    RwCameraSetFarClipPlane(g_cam, 1000.0f);
    // world: white ambient + a white directional light (the game's pDirect) whose frame looks along +z (away from the camera)
    g_world = RpWorldCreate(nullptr);
    g_amb = RpLightCreate(rpLIGHTAMBIENT);
    g_dir = RpLightCreate(rpLIGHTDIRECTIONAL);
    g_dirFrame = RwFrameCreate();
    RpLightSetFrame(g_dir, g_dirFrame);
    RwRGBAReal c{ 0.5f, 0.5f, 0.5f, 1.f };
    RpLightSetColor(g_amb, &c);
    RpLightSetColor(g_dir, &c);
    RpWorldAddLight(g_world, g_amb);
    RpWorldAddLight(g_world, g_dir);
    RpWorldAddCamera(g_world, g_cam);
    StaticRef<RpLight*>(0xC886EC) = g_dir; // pDirect
}
static void TeardownScene() {
    RpWorldRemoveCamera(g_world, g_cam);
    RpWorldRemoveLight(g_world, g_dir);
    RpWorldRemoveLight(g_world, g_amb);
    RpLightSetFrame(g_dir, nullptr);
    RpLightDestroy(g_dir); RpLightDestroy(g_amb);
    RwFrameDestroy(g_dirFrame);
    RpWorldDestroy(g_world);
    RwCameraSetRaster(g_cam, nullptr); RwCameraSetZRaster(g_cam, nullptr);
    RwRasterDestroy(g_fb); RwRasterDestroy(g_zb);
    RwCameraSetFrame(g_cam, nullptr);
    RwCameraDestroy(g_cam); RwFrameDestroy(g_camFrame);
}
static void BeginScene(RwRGBA clear = { 0, 0, 255, 255 }) {
    RwCameraBeginUpdate(g_cam);
    RwCameraClear(g_cam, &clear, rwCAMERACLEARIMAGE | rwCAMERACLEARZ);
}
static void EndScene() { RwCameraEndUpdate(g_cam); }

// quad (-2,-2,5)..(2,2,5) facing the camera, prelit colour, normals towards the camera, one material
static RpGeometry* MakeQuad(RwUInt32 flags, RpMaterial* mat, RwRGBA prelit) {
    RpGeometry* g = RpGeometryCreate(4, 2, flags | rpGEOMETRYPOSITIONS);
    RwV3d* v = RpMorphTargetGetVertices(RpGeometryGetMorphTarget(g, 0));
    v[0] = { -2, -2, 5 }; v[1] = { 2, -2, 5 }; v[2] = { 2, 2, 5 }; v[3] = { -2, 2, 5 };
    if (flags & rpGEOMETRYNORMALS) {
        RwV3d* n = g->morphTargets[0].normals;
        for (int i = 0; i < 4; i++) n[i] = { 0, 0, -1 };
    }
    if (flags & rpGEOMETRYPRELIT) {
        for (int i = 0; i < 4; i++) g->colors[i] = prelit;
    }
    // winding is front-facing under RW's start default (cull back, applied at RwEngineStart)
    RpTriangle* t = RpGeometryGetTriangles(g);
    RpGeometryTriangleSetVertexIndices(g, &t[0], 0, 2, 1);
    RpGeometryTriangleSetVertexIndices(g, &t[1], 0, 3, 2);
    RpGeometryTriangleSetMaterial(g, &t[0], mat);
    RpGeometryTriangleSetMaterial(g, &t[1], mat);
    RpGeometryUnlock(g);
    return g;
}
static RpAtomic* MakeAtomic(RpGeometry* g) {
    RpAtomic* a = RpAtomicCreate();
    RpAtomicSetFrame(a, RwFrameCreate());
    RpAtomicSetGeometry(a, g, 0);
    return a;
}
static void FreeAtomic(RpAtomic* a) {
    RwFrame* f = RpAtomicGetFrame(a);
    RpAtomicSetFrame(a, nullptr);
    RpAtomicDestroy(a);
    if (f) RwFrameDestroy(f);
}
static RwTexture* MakeSolidTexture(RwRGBA c) {
    RwRaster* r = RwRasterCreate(16, 16, 32, rwRASTERTYPETEXTURE | rwRASTERFORMAT8888);
    unsigned char* p = RwRasterLock(r, 0, rwRASTERLOCKWRITE);
    for (int i = 0; i < 16 * 16; i++) { p[i * 4 + 0] = c.blue; p[i * 4 + 1] = c.green; p[i * 4 + 2] = c.red; p[i * 4 + 3] = c.alpha; }
    RwRasterUnlock(r);
    return RwTextureCreate(r);
}
static DWORD GetRS(D3DRENDERSTATETYPE t) { DWORD v = 0; RwD3D9GetRenderState(t, &v); return v; }

// ---------------------------------------------------------------------------------------------------------------------------------
// 1. registration / creation
static void SetupTests() {
    std::printf("--- plugin registration + pipe creation\n");
    CHECK(CCustomBuildingRenderer::PluginAttach());
    CHECK(CCustomCarEnvMapPipeline::RegisterPlugin());
    CHECK(PipelinePluginAttach() == TRUE);
}
static void PostStartTests() {
    CHECKV(CCustomBuildingDNPipeline::ms_extraVertColourPluginOffset > 0, "ExtraVertColour offset %d", (int)CCustomBuildingDNPipeline::ms_extraVertColourPluginOffset);
    CHECKV(CCustomCarEnvMapPipeline::ms_envMapPluginOffset > 0 && CCustomCarEnvMapPipeline::ms_envMapAtmPluginOffset > 0 && CCustomCarEnvMapPipeline::ms_specularMapPluginOffset > 0,
           "EnvMat %d EnvAtm %d SpecMat %d", (int)CCustomCarEnvMapPipeline::ms_envMapPluginOffset, (int)CCustomCarEnvMapPipeline::ms_envMapAtmPluginOffset, (int)CCustomCarEnvMapPipeline::ms_specularMapPluginOffset);
    CHECK(CCustomBuildingRenderer::Initialise());
    CHECK(CCustomCarEnvMapPipeline::CreatePipe());
    CHECK(CCustomBuildingPipeline::ObjPipeline && CCustomBuildingDNPipeline::ObjPipeline && CCustomCarEnvMapPipeline::ObjPipeline);
    CHECK(CCustomBuildingPipeline::ObjPipeline != CCustomBuildingDNPipeline::ObjPipeline && CCustomBuildingDNPipeline::ObjPipeline != CCustomCarEnvMapPipeline::ObjPipeline);
    // the node of every pipe carries the pipe's own render callback
    for (auto* p : { CCustomBuildingPipeline::ObjPipeline, CCustomBuildingDNPipeline::ObjPipeline, CCustomCarEnvMapPipeline::ObjPipeline }) {
        auto* node = RxPipelineFindNodeByName(p, RxNodeDefinitionGetD3D9AtomicAllInOne()->name, nullptr, nullptr);
        CHECK(node != nullptr);
        CHECK(node && RxD3D9AllInOneGetRenderCallBack(node) != _rpD3D9AtomicDefaultRenderCallback);
        CHECK(node && RxD3D9AllInOneGetInstanceCallBack(node) == _rpD3D9AtomicDefaultInstanceCallback);
    }
    CHECK(RwCompatPipelinePluginId(CCustomBuildingPipeline::ObjPipeline) == CUSTOM_BUILDING_DN_PIPELINE_ID - 0x98 + 0x9C);
    CHECK(RwCompatPipelinePluginId(CCustomBuildingDNPipeline::ObjPipeline) == CUSTOM_BUILDING_DN_PIPELINE_ID);
    CHECK(RwCompatPipelinePluginId(CCustomCarEnvMapPipeline::ObjPipeline) == CUSTOM_CAR_ENV_MAP_PIPELINE_PLUGIN_ID);
    CHECK(GetD3D9Device() != nullptr);
}

// ---------------------------------------------------------------------------------------------------------------------------------
// 2. CCustomBuildingPipeline
static void BuildingTests() {
    std::printf("--- CCustomBuildingPipeline (0x5D77D0)\n");
    const int cx = g_w / 2, cy = g_h / 2;
    RpMaterial* m = RpMaterialCreate();
    m->color = { 255, 255, 255, 255 };
    RpGeometry* g = MakeQuad(rpGEOMETRYPRELIT | rpGEOMETRYNORMALS, m, { 100, 40, 10, 255 });
    RpAtomic* a = MakeAtomic(g);
    CCustomBuildingPipeline::CustomPipeAtomicSetup(a);
    CHECK(a->pipeline == CCustomBuildingPipeline::ObjPipeline);
    CHECK(GetPipelineID(a) == rpPDS_MAKEPIPEID(rwVENDORID_DEVELOPER, 0x9C));
    CHECK(CCustomBuildingRenderer::IsCBPCPipelineAttached(a));
    CHECK(CCustomCarEnvMapPipeline::GetMaterialFlags(m) == 0);

    BeginScene(); RpAtomicRender(a); EndScene();
    unsigned px = ReadPixel(cx, cy);
    CHECKV(NearC(px, 100, 40, 10), "prelit, lighting off, no env: %08X expect (100,40,10)", px);
    CHECKV(NearC(ReadPixel(4, 4), 0, 0, 255), "outside stays clear: %08X", ReadPixel(4, 4));

    // env stage: material data with a solid green env texture, shininess raw 100 -> c = _ftol(100 * (1/255f) * 254) = 99, TFACTOR = FF636363
    RwTexture* env = MakeSolidTexture({ 0, 255, 0, 255 });
    CCustomCarEnvMapPipeline::SetFxEnvTexture(m, env);
    CCustomCarEnvMapPipeline::SetFxEnvShininess(m, 100.f / 255.f);
    auto* d = CCustomCarEnvMapPipeline::EnvMapPlGetData(m);
    CHECK(d && d != &CCustomCarEnvMapPipeline::fakeEnvMapPipeMatData);
    CHECKV(d && std::bit_cast<uint8>(d->Shininess) == 100, "SetFxEnvShininess(100/255) stores _ftol(v*255)=raw 100 (got %d)", d ? (int)std::bit_cast<uint8>(d->Shininess) : -1);
    CCustomCarEnvMapPipeline::SetMaterialFlags(m, CCustomCarEnvMapPipeline::MF_HAS_SHINE_CAM);
    BeginScene(); RpAtomicRender(a); EndScene();
    px = ReadPixel(cx, cy);
    CHECKV(NearC(px, 100, 40 + 99, 10, 3), "env stage current + tex*TFACTOR: %08X expect (100,139,10)", px);
    CHECKV(GetRS(D3DRS_TEXTUREFACTOR) == 0xFF636363u, "TFACTOR %08X expect FF636363", (unsigned)GetRS(D3DRS_TEXTUREFACTOR));
    DWORD v = 0;
    RwD3D9GetTextureStageState(1, D3DTSS_COLOROP, &v);
    CHECKV(v == D3DTOP_DISABLE, "stage 1 colour op disabled after the draw (%u)", (unsigned)v);
    RwD3D9GetTextureStageState(1, D3DTSS_TEXCOORDINDEX, &v);
    CHECKV(v == 1, "stage 1 texcoord index restored (%u)", (unsigned)v);
    // shininess 255 saturates: c = min(255, _ftol(255*(1/255f)*254)) -> 254 -> g clamps at 255
    CCustomCarEnvMapPipeline::SetFxEnvShininess(m, 1.f);
    BeginScene(); RpAtomicRender(a); EndScene();
    px = ReadPixel(cx, cy);
    CHECKV(NearC(px, 100, 255, 10, 3) && GetRS(D3DRS_TEXTUREFACTOR) == 0xFFFEFEFEu, "shininess 1.0: %08X TFACTOR %08X expect (100,255,10) / FFFEFEFE", px, (unsigned)GetRS(D3DRS_TEXTUREFACTOR));
    // material flags off again -> back to the plain prelit colour
    CCustomCarEnvMapPipeline::SetMaterialFlags(m, 0);
    BeginScene(); RpAtomicRender(a); EndScene();
    px = ReadPixel(cx, cy);
    CHECKV(NearC(px, 100, 40, 10), "env flag cleared: %08X", px);

    // vertex alpha: material alpha 128 -> alpha blending is switched on through _rwD3D9RenderStateVertexAlphaEnable
    m->color.alpha = 128;
    BeginScene(); RpAtomicRender(a); EndScene();
    CHECK(_rwD3D9RenderStateVertexAlphaIsEnabled());
    m->color.alpha = 255;
    BeginScene(); RpAtomicRender(a); EndScene();
    CHECK(!_rwD3D9RenderStateVertexAlphaIsEnabled());

    // reinstance callback: null instance callback counts as success, otherwise the result is passed on, the header handed over is the one INSIDE the resEntry
    auto* geoHdr = static_cast<RxD3D9ResEntryHeader*>(g->instData);
    auto* entry = reinterpret_cast<RwResEntry*>(geoHdr) - 1;
    static RxD3D9ResEntryHeader* s_seen; s_seen = nullptr;
    static bool s_ret; s_ret = true;
    auto cb = [](void*, RxD3D9ResEntryHeader* h, RwBool re) -> RwBool { s_seen = h; return s_ret && re; };
    // CustomPipeReinstanceCB is private: reach it through the node of the pipe
    auto* node = RxPipelineFindNodeByName(CCustomBuildingPipeline::ObjPipeline, RxNodeDefinitionGetD3D9AtomicAllInOne()->name, nullptr, nullptr);
    auto* reinst = node ? RxD3D9AllInOneGetReinstanceCallBack(node) : nullptr;
    CHECK(reinst != nullptr);
    if (reinst) {
        CHECK(reinst(a, entry, nullptr) == TRUE);
        CHECK(reinst(a, entry, cb) == TRUE && s_seen == geoHdr);
        s_ret = false;
        CHECK(reinst(a, entry, cb) == FALSE);
    }

    FreeAtomic(a); RpGeometryDestroy(g); RwTextureDestroy(env); RpMaterialDestroy(m);
}

// ---------------------------------------------------------------------------------------------------------------------------------
// 3. CCustomBuildingDNPipeline
static RwRGBA* NewColours(int n, RwRGBA c) {
    auto* p = static_cast<RwRGBA*>(CMemoryMgr::Malloc(sizeof(RwRGBA) * n));
    for (int i = 0; i < n; i++) p[i] = c;
    return p;
}
static void DNTests() {
    std::printf("--- CCustomBuildingDNPipeline (0x5D6480, 0x5D6850, 0x5D7200)\n");
    const int cx = g_w / 2, cy = g_h / 2;
    RpMaterial* m = RpMaterialCreate();
    m->color = { 255, 255, 255, 255 };
    const RwRGBA day{ 200, 40, 10, 255 }, night{ 10, 40, 200, 255 };
    RpGeometry* g = MakeQuad(rpGEOMETRYPRELIT | rpGEOMETRYNORMALS, m, day);
    auto* ev = CCustomBuildingDNPipeline::GetExtraVertColourPtr(g);
    CHECK(ev->NightColors == nullptr && ev->DayColors == nullptr && ev->DNBalance == 0.f); // ctor
    ev->NightColors = NewColours(4, night);
    ev->DayColors   = NewColours(4, day);
    ev->DNBalance   = 1.f;
    RpAtomic* a = MakeAtomic(g);
    CCustomBuildingRenderer::AtomicSetup(a); // ExtraVertColour pointer + prelit -> DN pipeline
    CHECK(a->pipeline == CCustomBuildingDNPipeline::ObjPipeline);
    CHECK(GetPipelineID(a) == CUSTOM_BUILDING_DN_PIPELINE_ID);
    CHECK(RpD3D9GeometryGetUsageFlags(g) == rpD3D9GEOMETRYUSAGE_DYNAMICPRELIT);
    CHECK(CCustomBuildingDNPipeline::UsesThisPipeline(a) == 1);

    // day: the vertex buffer is built from the prelit colours = day
    CCustomBuildingDNPipeline::m_fDNBalanceParam = 0.f;
    CCustomBuildingDNPipeline::PreRenderUpdate(a, true);
    BeginScene(); RpAtomicRender(a); EndScene();
    unsigned px = ReadPixel(cx, cy);
    CHECKV(NearC(px, day.red, day.green, day.blue), "balance 0 (day): %08X expect (200,40,10)", px);
    CHECK(ev->DNBalance == 0.f);

    // night
    CCustomBuildingDNPipeline::m_fDNBalanceParam = 1.f;
    CCustomBuildingDNPipeline::PreRenderUpdate(a, true);
    BeginScene(); RpAtomicRender(a); EndScene();
    px = ReadPixel(cx, cy);
    CHECKV(NearC(px, night.red, night.green, night.blue), "balance 1 (night): %08X expect (10,40,200)", px);
    CHECK(ev->DNBalance == 1.f);
    CHECK(g->colors[0].red == 10 && g->colors[0].blue == 200);

    // 0.5: out = _ftol(night * t + day * (1 - t)) = (105, 40, 105)
    CCustomBuildingDNPipeline::m_fDNBalanceParam = 0.5f;
    CCustomBuildingDNPipeline::PreRenderUpdate(a, true);
    BeginScene(); RpAtomicRender(a); EndScene();
    px = ReadPixel(cx, cy);
    CHECKV(NearC(px, 105, 40, 105), "balance 0.5: %08X expect (105,40,105)", px);

    // exe truncation: day == night == 200, t = 0.001f: night*t + day*fl(1-t) is 199.99999... in extended precision when fl(1-t) < 1-t
    {
        const float t = 0.001f;
        const double exact = 200.0 * (double)t + 200.0 * (double)(1.f - t);
        const int expect = (int)exact;
        for (int i = 0; i < 4; i++) { ev->DayColors[i] = { 200, 200, 200, 200 }; ev->NightColors[i] = { 200, 200, 200, 200 }; }
        NVCPipelineProcess(a, t);
        CHECKV(g->colors[0].red == expect && g->colors[0].alpha == expect, "t=0.001 day==night==200 -> %d (exact %.9f), got %d", expect, exact, (int)g->colors[0].red);
        for (int i = 0; i < 4; i++) { ev->DayColors[i] = day; ev->NightColors[i] = night; }
    }
    // clamp: NVCPipelineProcess(-5 / 9) clamps the weight but stores the raw balance
    NVCPipelineProcess(a, -5.f);
    CHECK(g->colors[0].red == day.red && ev->DNBalance == -5.f);
    NVCPipelineProcess(a, 9.f);
    CHECK(g->colors[0].red == night.red && ev->DNBalance == 9.f);

    // PreRenderUpdate gating (exe 0x5D7200): skip when |DNBalance - param| <= 0.01 (unless ignore), else process on the atomic's slot / when the slot bit is 0
    ev->DNBalance = 0.f;
    CCustomBuildingDNPipeline::m_fDNBalanceParam = 0.005f;
    for (int i = 0; i < 4; i++) g->colors[i] = day;
    CCustomBuildingDNPipeline::PreRenderUpdate(a, false);
    CHECK(ev->DNBalance == 0.f); // |0 - 0.005| <= 0.01: untouched
    CCustomBuildingDNPipeline::m_fDNBalanceParam = 0.2f;
    StaticRef<uint32>(0xC02C14) = (uint32)(((uintptr_t)a / 0x70) % 16); // s_Magic1 == this atomic's slot
    StaticRef<uint32>(0xC02C18) = 0;
    CCustomBuildingDNPipeline::PreRenderUpdate(a, false);
    CHECK(ev->DNBalance == 0.2f);
    ev->DNBalance = 0.f;
    StaticRef<uint32>(0xC02C14) = (uint32)(((uintptr_t)a / 0x70) % 16 + 1) % 16; // another slot: 0.2 <= 0.3 -> skipped
    CCustomBuildingDNPipeline::PreRenderUpdate(a, false);
    CHECK(ev->DNBalance == 0.f);
    StaticRef<uint32>(0xC02C18) = 1; // s_Magic2 != 0: processed
    CCustomBuildingDNPipeline::PreRenderUpdate(a, false);
    CHECK(ev->DNBalance == 0.2f);
    StaticRef<uint32>(0xC02C18) = 0;
    ev->DNBalance = 0.f;
    CCustomBuildingDNPipeline::m_fDNBalanceParam = 0.5f; // 0.5 > 0.3: processed on any slot
    CCustomBuildingDNPipeline::PreRenderUpdate(a, false);
    CHECK(ev->DNBalance == 0.5f);
    // not our pipeline: untouched
    a->pipeline = nullptr;
    ev->DNBalance = 0.f;
    CCustomBuildingDNPipeline::PreRenderUpdate(a, true);
    CHECK(ev->DNBalance == 0.f);
    a->pipeline = CCustomBuildingDNPipeline::ObjPipeline;

    // DN env stage (exe: TFACTOR is NOT used, COLORARG2 = DIFFUSE): current + tex * diffuse = (200,40,10) + green * (200,40,10)/255 -> g 80
    CCustomBuildingDNPipeline::m_fDNBalanceParam = 0.f;
    CCustomBuildingDNPipeline::PreRenderUpdate(a, true);
    RwTexture* env = MakeSolidTexture({ 0, 255, 0, 255 });
    CCustomCarEnvMapPipeline::SetFxEnvTexture(m, env);
    CCustomCarEnvMapPipeline::SetFxEnvShininess(m, 100.f / 255.f);
    CCustomCarEnvMapPipeline::SetMaterialFlags(m, CCustomCarEnvMapPipeline::MF_HAS_SHINE_CAM);
    BeginScene(); RpAtomicRender(a); EndScene();
    px = ReadPixel(cx, cy);
    CHECKV(NearC(px, 200, 80, 10, 4), "DN env stage current + tex*diffuse: %08X expect (200,80,10)", px);
    CHECKV(GetRS(D3DRS_TEXTUREFACTOR) == 0xFF636363u, "TFACTOR %08X expect FF636363", (unsigned)GetRS(D3DRS_TEXTUREFACTOR));
    CCustomCarEnvMapPipeline::SetMaterialFlags(m, 0);

    // CustomPipeMaterialSetup clears the flag word and only sets bit 0 for env data with shininess && texture
    CCustomBuildingDNPipeline::CustomPipeMaterialSetup(m, nullptr);
    CHECKV(CCustomCarEnvMapPipeline::GetMaterialFlags(m) == 1, "env data with shininess and texture -> bit 0 (flags %u)", CCustomCarEnvMapPipeline::GetMaterialFlags(m));
    CCustomCarEnvMapPipeline::SetFxEnvShininess(m, 0.f);
    CCustomBuildingDNPipeline::CustomPipeMaterialSetup(m, nullptr);
    CHECKV(CCustomCarEnvMapPipeline::GetMaterialFlags(m) == 0, "shininess 0 -> flags %u", CCustomCarEnvMapPipeline::GetMaterialFlags(m));

    FreeAtomic(a); RpGeometryDestroy(g); RwTextureDestroy(env); RpMaterialDestroy(m);
}

// ---------------------------------------------------------------------------------------------------------------------------------
// 4. CCustomCarEnvMapPipeline
static void CarTests() {
    std::printf("--- CCustomCarEnvMapPipeline (0x5D9900, 0x5DA790, 0x5D8870)\n");
    const int cx = g_w / 2, cy = g_h / 2;
    CCustomCarEnvMapPipeline::PreRenderUpdate();
    const D3DLIGHT9& gl = CCustomCarEnvMapPipeline::g_GameLight;
    CHECK(gl.Type == D3DLIGHT_DIRECTIONAL && gl.Diffuse.r == 0.25f && gl.Ambient.r == 0.75f && gl.Specular.g == 0.65f && gl.Range == 1000.f && gl.Attenuation0 == 1.f);
    CHECKV(std::fabs(gl.Direction.z - 1.f) < 1e-5f && std::fabs(gl.Direction.x) < 1e-5f, "light direction from pDirect's frame (%f %f %f)", gl.Direction.x, gl.Direction.y, gl.Direction.z);

    RpMaterial* m = RpMaterialCreate();
    m->color = { 128, 128, 128, 255 };
    m->surfaceProps = { 1.0f, 0.0f, 1.0f }; // ambient, specular, diffuse
    RpGeometry* g = MakeQuad(rpGEOMETRYLIGHT | rpGEOMETRYNORMALS | rpGEOMETRYMODULATEMATERIALCOLOR, m, { 255, 255, 255, 255 }); // not prelit: COLORVERTEX off, the D3D material colours rule
    RpAtomic* a = MakeAtomic(g);
    CCustomCarEnvMapPipeline::CustomPipeAtomicSetup(a);
    CHECK(a->pipeline == CCustomCarEnvMapPipeline::ObjPipeline);
    CHECK(GetPipelineID(a) == CUSTOM_CAR_ENV_MAP_PIPELINE_PLUGIN_ID);
    CHECKV(CCustomCarEnvMapPipeline::GetMaterialFlags(m) == 0, "fresh material -> flags %u", CCustomCarEnvMapPipeline::GetMaterialFlags(m));

    // --- plain lit render: baseline
    BeginScene(); RpAtomicRender(a); EndScene();
    const unsigned base = ReadPixel(cx, cy);
    DWORD spec = GetRS(D3DRS_SPECULARENABLE);
    CHECKV(spec == FALSE, "baseline: SPECULARENABLE off (%u)", (unsigned)spec);
    INFO("baseline lit pixel %08X", base);

    // --- specular light on: SpecMat data (specularity 0.5 + a texture) -> flags HAS_SPECULARITY; spec = min(1, 1.85*0.5*2) = 1, power = 50
    RwTexture* specTex = MakeSolidTexture({ 255, 255, 255, 255 });
    RwTextureAddRef(specTex); // the material's SpecMat destructor releases its texture (SetFxSpecTexture AddRefs, too)
    CCustomCarEnvMapPipeline::SpecMapPlGetData(m) = new (CCustomCarEnvMapPipeline::m_gSpecMapPipeMatDataPool->New()) CustomSpecMapPipeMaterialData{ 0.5f, specTex };
    CCustomCarEnvMapPipeline::CustomPipeMaterialSetup(m, nullptr);
    CHECKV(CCustomCarEnvMapPipeline::GetMaterialFlags(m) == CCustomCarEnvMapPipeline::MF_HAS_SPECULARITY, "flags with spec data: %u", CCustomCarEnvMapPipeline::GetMaterialFlags(m));
    BeginScene(); RpAtomicRender(a); EndScene();
    const unsigned withSpec = ReadPixel(cx, cy);
    INFO("specular pixel %08X (baseline %08X)", withSpec, base);
    CHECKV(Ch(withSpec, 16) > Ch(base, 16) + 40 && Ch(withSpec, 8) > Ch(base, 8) + 40 && Ch(withSpec, 0) > Ch(base, 0) + 40, "specular highlight raises every channel by >40 (%08X vs %08X)", withSpec, base);
    CHECKV(GetRS(D3DRS_SPECULARENABLE) == FALSE, "SPECULARENABLE restored after the mesh (%u)", (unsigned)GetRS(D3DRS_SPECULARENABLE));
    D3DLIGHT9 l1{}; RwD3D9GetLight(1, &l1);
    BOOL en = TRUE; g_dev->GetLightEnable(1, &en);
    CHECKV(l1.Type == D3DLIGHT_DIRECTIONAL && l1.Specular.r == 0.65f && !en, "g_GameLight on slot 1 (specular 0.65), disabled again after the draw (enabled %d)", (int)en);
    CHECKV(GetRS(D3DRS_LOCALVIEWER) == FALSE, "LOCALVIEWER off (%u)", (unsigned)GetRS(D3DRS_LOCALVIEWER));
    // low fx quality + "no extra passes" atomic: the specular pass is skipped
    g_atomicId = ATOMIC_PIPE_NO_EXTRA_PASSES; g_fxQuality = FX_QUALITY_MEDIUM;
    BeginScene(); RpAtomicRender(a); EndScene();
    unsigned px = ReadPixel(cx, cy);
    CHECKV(NearC(px, Ch(base, 16), Ch(base, 8), Ch(base, 0), 2), "atomic id 0x4000 (no extra passes), quality medium -> no specular (%08X)", px);
    g_atomicId = ATOMIC_PIPE_NO_EXTRA_PASSES_LOD;
    BeginScene(); RpAtomicRender(a); EndScene();
    px = ReadPixel(cx, cy);
    CHECKV(NearC(px, Ch(base, 16), Ch(base, 8), Ch(base, 0), 2), "atomic id 0x2000 -> no specular (%08X)", px);
    g_fxQuality = FX_QUALITY_HIGH; // high quality renders the specular pass even for "no extra passes" atomics (exe: quality >= 2 || !noReflections)
    BeginScene(); RpAtomicRender(a); EndScene();
    px = ReadPixel(cx, cy);
    CHECKV(px == withSpec, "quality high keeps specular on 0x2000 atomics (%08X)", px);
    g_atomicId = 0;

    // --- cam env map: solid green env texture, shininess raw 60, gSpecIntensity 1.0: c = _ftol(60 * (1/255f) * 1.85 * 254) = 110
    CustomSpecMapPipeMaterialData* sd = CCustomCarEnvMapPipeline::SpecMapPlGetData(m);
    sd->Specularity = 0.f;
    RwTexture* env = MakeSolidTexture({ 0, 255, 0, 255 });
    CCustomCarEnvMapPipeline::SetFxEnvTexture(m, env);
    CCustomCarEnvMapPipeline::SetFxEnvShininess(m, 60.f / 255.f);
    CCustomCarEnvMapPipeline::CustomPipeMaterialSetup(m, nullptr);
    CHECKV(CCustomCarEnvMapPipeline::GetMaterialFlags(m) == CCustomCarEnvMapPipeline::MF_HAS_SHINE_CAM, "flags with env data: %u", CCustomCarEnvMapPipeline::GetMaterialFlags(m));
    BeginScene(); RpAtomicRender(a); EndScene();
    const unsigned withEnv = ReadPixel(cx, cy);
    INFO("env pixel %08X (baseline %08X)", withEnv, base);
    CHECKV(Ch(withEnv, 8) >= Ch(base, 8) + 100 && Ch(withEnv, 8) <= Ch(base, 8) + 120 && std::abs(Ch(withEnv, 16) - Ch(base, 16)) <= 3 && std::abs(Ch(withEnv, 0) - Ch(base, 0)) <= 3,
           "env map adds tex * c (c = 110) to the green channel only (%08X vs %08X)", withEnv, base);
    CHECKV(GetRS(D3DRS_TEXTUREFACTOR) == 0xFF6E6E6Eu, "TFACTOR %08X expect FF6E6E6E", (unsigned)GetRS(D3DRS_TEXTUREFACTOR));
    // gSpecIntensity scales the env map: 0 -> nothing added
    StaticRef<float>(0x8D12D0) = 0.f;
    BeginScene(); RpAtomicRender(a); EndScene();
    px = ReadPixel(cx, cy);
    CHECKV(NearC(px, Ch(base, 16), Ch(base, 8), Ch(base, 0), 2), "gSpecIntensity 0 -> no env contribution (%08X)", px);
    StaticRef<float>(0x8D12D0) = 1.f;
    RwD3D9GetTextureStageState(1, D3DTSS_TEXTURETRANSFORMFLAGS, &en);
    CHECKV(en == 0, "stage 1 texture transform flags restored (%u)", (unsigned)en);

    // --- "wave" env (name starts with 'x'): needs a second texcoord set -> skipped here, flags only
    CCustomCarEnvMapPipeline::SetMaterialFlags(m, 0);
    CHECK(CCustomCarEnvMapPipeline::GetFxEnvShininess(m) > 60.f / 255.f - 0.005f && CCustomCarEnvMapPipeline::GetFxEnvShininess(m) < 60.f / 255.f + 0.005f);
    CHECKV(CCustomCarEnvMapPipeline::GetFxEnvShininess(m) > 0.f, "GetFxEnvShininess is unsigned: %f", CCustomCarEnvMapPipeline::GetFxEnvShininess(m));
    CCustomCarEnvMapPipeline::SetFxEnvShininess(m, 1.f); // raw 255 (an int8 FixedFloat would read it back as -1/255)
    CHECKV(CCustomCarEnvMapPipeline::GetFxEnvShininess(m) > 0.99f, "raw 255 reads back as %f", CCustomCarEnvMapPipeline::GetFxEnvShininess(m));

    // --- the 8 "paint over" key colours are drawn black when lit (exe compares the 24 bits as 0xBBGGRR)
    CCustomCarEnvMapPipeline::SetMaterialFlags(m, 0);
    CustomSpecMapPipeMaterialData* sd2 = CCustomCarEnvMapPipeline::SpecMapPlGetData(m);
    sd2->Specularity = 0.f;
    struct Key { RwRGBA c; bool black; } keys[] = {
        { { 255, 60, 0, 255 }, true }, { { 60, 255, 0, 255 }, true }, { { 185, 255, 0, 255 }, true }, { { 255, 175, 0, 255 }, true },
        { { 255, 0, 175, 255 }, true }, { { 0, 255, 200, 255 }, true }, { { 255, 0, 255, 255 }, true }, { { 0, 255, 255, 255 }, true },
        { { 0, 60, 255, 255 }, false }, { { 175, 0, 255, 255 }, false } };
    for (const Key& k : keys) {
        m->color = k.c;
        m->surfaceProps = { 1.0f, 0.0f, 1.0f };
        BeginScene(); RpAtomicRender(a); EndScene();
        px = ReadPixel(cx, cy);
        const int sum = Ch(px, 16) + Ch(px, 8) + Ch(px, 0);
        CHECKV(k.black ? sum <= 6 : sum > 60, "key colour (%d,%d,%d): %08X %s", k.c.red, k.c.green, k.c.blue, px, k.black ? "black" : "visible");
    }

    FreeAtomic(a); RpGeometryDestroy(g); RwTextureDestroy(env); RwTextureDestroy(specTex); RpMaterialDestroy(m);
}

// ---------------------------------------------------------------------------------------------------------------------------------
// plugin stream callbacks (EnvMat 0x5D9660/0x5D8CD0, SpecMat 0x5D9880/0x5D8D60): write -> read round trips through a memory stream
static void StreamTests() {
    std::printf("--- material plugin streams\n");
    RpMaterial* a = RpMaterialCreate();
    RpMaterial* b = RpMaterialCreate();
    RwTexture* env = MakeSolidTexture({ 0, 255, 0, 255 });
    CCustomCarEnvMapPipeline::SetFxEnvTexture(a, env);
    CCustomCarEnvMapPipeline::SetFxEnvShininess(a, 0.5f);
    CCustomCarEnvMapPipeline::SetFxEnvScale(a, 1.5f, -0.5f);
    CCustomCarEnvMapPipeline::SetFxEnvTransScl(a, 0.25f, 2.f);
    const int off = CCustomCarEnvMapPipeline::ms_envMapPluginOffset;
    CHECK(CCustomCarEnvMapPipeline::pluginEnvMatStreamGetSizeCB(a, off, 4) == (int)sizeof(EnvMapPipeMaterialDataBuffer));
    RwMemory mem{};
    RwStream* ws = RwStreamOpen(rwSTREAMMEMORY, rwSTREAMWRITE, &mem);
    CHECK(ws != nullptr);
    CCustomCarEnvMapPipeline::pluginEnvMatStreamWriteCB(ws, sizeof(EnvMapPipeMaterialDataBuffer), a, off, 4);
    RwStreamClose(ws, &mem);
    CHECK(mem.length == sizeof(EnvMapPipeMaterialDataBuffer));
    RwStream* rs = RwStreamOpen(rwSTREAMMEMORY, rwSTREAMREAD, &mem);
    CCustomCarEnvMapPipeline::pluginEnvMatStreamReadCB(rs, sizeof(EnvMapPipeMaterialDataBuffer), b, off, 4);
    RwStreamClose(rs, &mem);
    auto* d = CCustomCarEnvMapPipeline::EnvMapPlGetData(b);
    CHECK(d && d != &CCustomCarEnvMapPipeline::fakeEnvMapPipeMatData);
    CHECKV(d && std::bit_cast<uint8>(d->Shininess) == 127 && d->Scale.x == 1.5f && d->Scale.y == -0.5f && d->TranslationScale.x == 0.25f && d->TranslationScale.y == 2.f && d->Texture == env,
           "env data round trip (shininess raw %d, scale %f/%f, trans %f/%f)", d ? (int)std::bit_cast<uint8>(d->Shininess) : -1, d ? (float)d->Scale.x : 0.f, d ? (float)d->Scale.y : 0.f, d ? (float)d->TranslationScale.x : 0.f, d ? (float)d->TranslationScale.y : 0.f);
    RwFree(mem.start);
    // a default (fake) data block is shared again after the read of an all-default buffer
    RpMaterial* c = RpMaterialCreate();
    EnvMapPipeMaterialDataBuffer def = CustomEnvMapPipeMaterialData{}.ToBuffer();
    RwMemory dm{ (RwUInt8*)&def, sizeof(def) };
    rs = RwStreamOpen(rwSTREAMMEMORY, rwSTREAMREAD, &dm);
    CCustomCarEnvMapPipeline::pluginEnvMatStreamReadCB(rs, sizeof(def), c, off, 4);
    RwStreamClose(rs, &dm);
    CHECK(CCustomCarEnvMapPipeline::EnvMapPlGetData(c) == &CCustomCarEnvMapPipeline::fakeEnvMapPipeMatData);

    // SpecMat: buffer {specularity, name[24]}. A texture that cannot be found -> no data at all (exe 0x5D98CC), found in the current dictionary -> data
    SpecMapPipeMaterialDataBuffer sb{ 0.5f, "nosuchtex" };
    RwMemory sm{ (RwUInt8*)&sb, sizeof(sb) };
    const int soff = CCustomCarEnvMapPipeline::ms_specularMapPluginOffset;
    CHECK(CCustomCarEnvMapPipeline::pluginSpecMatStreamGetSizeCB(a, soff, 4) == (int)sizeof(SpecMapPipeMaterialDataBuffer));
    rs = RwStreamOpen(rwSTREAMMEMORY, rwSTREAMREAD, &sm);
    CCustomCarEnvMapPipeline::pluginSpecMatStreamReadCB(rs, sizeof(sb), a, soff, 4);
    RwStreamClose(rs, &sm);
    CHECK(CCustomCarEnvMapPipeline::SpecMapPlGetData(a) == nullptr);
    RwTexDictionary* dict = RwTexDictionaryCreate();
    RwTexture* specTex = MakeSolidTexture({ 255, 255, 255, 255 });
    RwTextureSetName(specTex, "spectex");
    RwTexDictionaryAddTexture(dict, specTex);
    RwTexDictionary* prev = RwTexDictionarySetCurrent(dict);
    SpecMapPipeMaterialDataBuffer sb2{ 0.5f, "spectex" };
    RwMemory sm2{ (RwUInt8*)&sb2, sizeof(sb2) };
    rs = RwStreamOpen(rwSTREAMMEMORY, rwSTREAMREAD, &sm2);
    CCustomCarEnvMapPipeline::pluginSpecMatStreamReadCB(rs, sizeof(sb2), a, soff, 4);
    RwStreamClose(rs, &sm2);
    CHECK(CCustomCarEnvMapPipeline::SpecMapPlGetData(a) != nullptr && CCustomCarEnvMapPipeline::GetFxSpecSpecularity(a) == 0.5f && CCustomCarEnvMapPipeline::GetFxSpecTexture(a) == specTex);
    // zero specularity -> no data even when the texture exists
    SpecMapPipeMaterialDataBuffer sb3{ 0.f, "spectex" };
    RwMemory sm3{ (RwUInt8*)&sb3, sizeof(sb3) };
    rs = RwStreamOpen(rwSTREAMMEMORY, rwSTREAMREAD, &sm3);
    CCustomCarEnvMapPipeline::pluginSpecMatStreamReadCB(rs, sizeof(sb3), c, soff, 4);
    RwStreamClose(rs, &sm3);
    CHECK(CCustomCarEnvMapPipeline::SpecMapPlGetData(c) == nullptr);
    RpMaterialDestroy(a); // releases the texture reference RwTextureRead took
    RwTexDictionarySetCurrent(prev);
    RwTexDictionaryDestroy(dict);
    RpMaterialDestroy(b); RpMaterialDestroy(c);
    RwTextureDestroy(env);
}

// ---------------------------------------------------------------------------------------------------------------------------------
// 5. SA assets
static std::vector<unsigned char> ReadFileBytes(const char* path) {
    std::vector<unsigned char> b;
    if (FILE* f = std::fopen(path, "rb")) {
        std::fseek(f, 0, SEEK_END); b.resize(std::ftell(f)); std::fseek(f, 0, SEEK_SET);
        b.resize(std::fread(b.data(), 1, b.size(), f)); std::fclose(f);
    }
    return b;
}
static RpAtomic* CollectAtomic(RpAtomic* a, void* data) { ((std::vector<RpAtomic*>*)data)->push_back(a); return a; }
static RpClump* LoadClump(const char* path, std::vector<unsigned char>& keep) {
    keep = ReadFileBytes(path);
    CHECKV(!keep.empty(), "read %s", path);
    if (keep.empty()) return nullptr;
    rw::StreamMemory sm;
    sm.open(keep.data(), (std::uint32_t)keep.size());
    std::uint32_t len = 0, ver = 0;
    CHECK(rw::findChunk(&sm, rw::ID_CLUMP, &len, &ver));
    RpClump* clump = RpClumpStreamRead(&sm);
    CHECK(clump != nullptr);
    return clump;
}
// camera looking at the union of the atomics' bounding spheres, from -z; the clump frame is rotated by `rotX` degrees about x
static void FrameClump(RpClump* clump, float rotXdeg) {
    RwFrame* f = RpClumpGetFrame(clump);
    if (rotXdeg != 0.f) {
        RwV3d ax{ 1, 0, 0 };
        RwFrameRotate(f, &ax, rotXdeg, rwCOMBINEPRECONCAT);
    }
    RwMatrix* ltm = RwFrameGetLTM(f);
    std::vector<RpAtomic*> atoms;
    RpClumpForAllAtomics(clump, CollectAtomic, &atoms);
    RwV3d lo{ 1e9f, 1e9f, 1e9f }, hi{ -1e9f, -1e9f, -1e9f };
    for (RpAtomic* a : atoms) {
        RwMatrix* am = RwFrameGetLTM(RpAtomicGetFrame(a));
        const RwSphere& s = a->geometry->morphTargets[0].boundingSphere;
        RwV3d c;
        RwV3dTransformPoints(&c, &s.center, 1, am);
        lo = { std::min(lo.x, c.x - s.radius), std::min(lo.y, c.y - s.radius), std::min(lo.z, c.z - s.radius) };
        hi = { std::max(hi.x, c.x + s.radius), std::max(hi.y, c.y + s.radius), std::max(hi.z, c.z + s.radius) };
    }
    (void)ltm;
    const RwV3d mid{ (lo.x + hi.x) / 2, (lo.y + hi.y) / 2, (lo.z + hi.z) / 2 };
    const float r = std::max({ hi.x - lo.x, hi.y - lo.y, hi.z - lo.z }) * 0.5f;
    const float dist = r * 2.2f;
    RwMatrix* cm = RwFrameGetMatrix(g_camFrame);
    RwMatrixSetIdentity(cm);
    RwV3d pos{ mid.x, mid.y, mid.z - dist };
    RwMatrixTranslate(cm, &pos, rwCOMBINEREPLACE);
    RwFrameUpdateObjects(g_camFrame);
    RwCameraSetNearClipPlane(g_cam, dist * 0.05f);
    RwCameraSetFarClipPlane(g_cam, dist * 4.f);
}
static int CountDiff(const std::vector<unsigned>& a, const std::vector<unsigned>& b, int minDelta, long long* sumDelta = nullptr) {
    int n = 0; long long s = 0;
    for (size_t i = 0; i < a.size() && i < b.size(); i++) {
        const int d = std::abs(Ch(a[i], 16) - Ch(b[i], 16)) + std::abs(Ch(a[i], 8) - Ch(b[i], 8)) + std::abs(Ch(a[i], 0) - Ch(b[i], 0));
        if (d >= minDelta) { ++n; s += d; }
    }
    if (sumDelta) *sumDelta = s;
    return n;
}
static long long Brightness(const std::vector<unsigned>& a) {
    long long s = 0;
    for (unsigned p : a) s += Ch(p, 16) + Ch(p, 8) + Ch(p, 0);
    return s;
}

static void BuildingAssetTest(const char* path) {
    std::printf("--- asset %s (ExtraVertColour -> DN pipeline)\n", path);
    std::vector<unsigned char> bytes;
    RpClump* clump = LoadClump(path, bytes);
    if (!clump) return;
    std::vector<RpAtomic*> atoms;
    RpClumpForAllAtomics(clump, CollectAtomic, &atoms);
    int withNight = 0, dn = 0, plain = 0, verts = 0, nightDiffers = 0;
    for (RpAtomic* a : atoms) {
        RpGeometry* geo = RpAtomicGetGeometry(a);
        auto* ev = CCustomBuildingDNPipeline::GetExtraVertColourPtr(geo);
        if (ev->NightColors) {
            ++withNight;
            const int n = RpGeometryGetNumVertices(geo);
            verts += n;
            CHECK(ev->DayColors != nullptr && ev->DNBalance == 1.f);
            bool same = true;
            for (int i = 0; i < n && i < 64; i++) if (std::memcmp(&ev->NightColors[i], &geo->colors[i], 4)) same = false;
            nightDiffers += !same;
            // day colours were copied from the prelit colours when the chunk was read
            bool dayEq = geo->colors != nullptr;
            for (int i = 0; dayEq && i < n; i++) if (std::memcmp(&ev->DayColors[i], &geo->colors[i], 4)) dayEq = false;
            CHECK(dayEq);
        }
        CCustomBuildingRenderer::AtomicSetup(a);
        (a->pipeline == CCustomBuildingDNPipeline::ObjPipeline ? dn : plain)++;
        CHECK((a->pipeline == CCustomBuildingDNPipeline::ObjPipeline) == (ev->NightColors && geo->colors));
    }
    INFO("%zu atomics, %d with night colours (%d vertices, %d differ from day), %d on the DN pipe, %d on the plain building pipe", atoms.size(), withNight, verts, nightDiffers, dn, plain);
    if (withNight == 0) {
        // no day/night chunk (a ped such as male01): CCustomBuildingRenderer::AtomicSetup tests the night colours pointer (exe 0x5D6E90), so every atomic stays on the plain building pipe
        CHECKV(dn == 0 && plain == (int)atoms.size(), "geometry without a day/night chunk is not on the DN pipe (dn %d, plain %d)", dn, plain);
        RpClumpDestroy(clump);
        return;
    }
    FrameClump(clump, 0.f);
    auto render = [&](float balance) {
        CCustomBuildingDNPipeline::m_fDNBalanceParam = balance;
        CCustomBuildingDNPipeline::PreRenderUpdate(clump, true);
        BeginScene(); RpClumpRender(clump); EndScene();
        return ReadFrame();
    };
    const auto day = render(0.f), night = render(1.f), mid = render(0.5f), day2 = render(0.f);
    long long delta = 0;
    const int nDiff = CountDiff(day, night, 8, &delta);
    INFO("frame %dx%d: %d pixels differ day vs night (sum delta %lld); brightness day %lld mid %lld night %lld", g_w, g_h, nDiff, delta, Brightness(day), Brightness(mid), Brightness(night));
    CHECK(nDiff > 200);
    CHECKV(CountDiff(day, day2, 1) == 0, "re-rendering day after night reproduces the day frame (%d pixels differ)", CountDiff(day, day2, 1));
    const long long bd = Brightness(day), bm = Brightness(mid), bn = Brightness(night);
    CHECKV((bm >= std::min(bd, bn) && bm <= std::max(bd, bn)), "mid balance brightness is between day and night (%lld / %lld / %lld)", bd, bm, bn);
    RpClumpDestroy(clump);
}

static void CarAssetTest(const char* path) {
    std::printf("--- asset %s (EnvMat / SpecMat chunks, car pipeline)\n", path);
    std::vector<unsigned char> bytes;
    RpClump* clump = LoadClump(path, bytes);
    if (!clump) return;
    std::vector<RpAtomic*> atoms;
    RpClumpForAllAtomics(clump, CollectAtomic, &atoms);
    int mats = 0, envMats = 0, specMats = 0;
    struct Ctx { int* mats; int* env; int* spec; } ctx{ &mats, &envMats, &specMats };
    for (RpAtomic* a : atoms) {
        RpGeometryForAllMaterials(RpAtomicGetGeometry(a), [](RpMaterial* mat, void* d) -> RpMaterial* {
            auto* c = (Ctx*)d;
            ++*c->mats;
            *c->env += CCustomCarEnvMapPipeline::EnvMapPlGetData(mat) != &CCustomCarEnvMapPipeline::fakeEnvMapPipeMatData;
            *c->spec += CCustomCarEnvMapPipeline::SpecMapPlGetData(mat) != nullptr;
            return mat;
        }, &ctx);
    }
    INFO("%zu atomics, %d materials, %d with an Env Map chunk, %d with a Specular Map chunk", atoms.size(), mats, envMats, specMats);
    CHECK(mats > 0 && envMats > 0); // (infernus carries no Specular Map chunk: those are covered by StreamTests)
    // make every material shiny + specular (the textures normally come from vehicle.txd): flags are derived by the game's own material setup
    RwTexture* env = MakeSolidTexture({ 0, 255, 0, 255 });
    RwTexture* specTex = MakeSolidTexture({ 255, 255, 255, 255 });
    // configure the materials through a small struct (captureless lambdas are the RpGeometryForAllMaterials callbacks)
    struct Cfg { bool shiny, spec; RwTexture *env, *specTex; };
    auto configure = [&](bool shiny, bool specular) {
        Cfg cfg{ shiny, specular, env, specTex };
        for (RpAtomic* a : atoms) {
            RpGeometryForAllMaterials(RpAtomicGetGeometry(a), [](RpMaterial* mat, void* d) -> RpMaterial* {
                auto* c = (Cfg*)d;
                auto*& sd = CCustomCarEnvMapPipeline::SpecMapPlGetData(mat);
                if (!sd) sd = new (CCustomCarEnvMapPipeline::m_gSpecMapPipeMatDataPool->New()) CustomSpecMapPipeMaterialData{ 0.f, nullptr };
                sd->Specularity = c->spec ? 0.8f : 0.f;
                if (c->spec && sd->Texture != c->specTex) { RwTextureAddRef(c->specTex); sd->Texture = c->specTex; } // released with the material
                if (c->shiny) {
                    CCustomCarEnvMapPipeline::SetFxEnvTexture(mat, c->env);
                    CCustomCarEnvMapPipeline::SetFxEnvShininess(mat, 0.5f);
                } else if (auto* ed = CCustomCarEnvMapPipeline::EnvMapPlGetData(mat); ed != &CCustomCarEnvMapPipeline::fakeEnvMapPipeMatData) {
                    ed->Texture = nullptr;
                    CCustomCarEnvMapPipeline::SetFxEnvShininess(mat, 0.f);
                }
                return mat;
            }, &cfg);
            CCustomCarEnvMapPipeline::CustomPipeAtomicSetup(a);
        }
    };
    FrameClump(clump, 180.f); // top of the car towards the camera
    CCustomCarEnvMapPipeline::PreRenderUpdate();
    auto render = [&]() {
        BeginScene(); RpClumpRender(clump); EndScene();
        return ReadFrame();
    };
    configure(false, false);
    const auto plain = render();
    configure(false, true);
    const auto spec = render();
    configure(true, false);
    const auto envOnly = render();
    configure(true, true);
    const auto both = render();
    long long dSpec = 0, dEnv = 0;
    const int nSpec = CountDiff(plain, spec, 12, &dSpec), nEnv = CountDiff(plain, envOnly, 12, &dEnv);
    INFO("plain brightness %lld, spec %lld (%d px differ), env %lld (%d px differ), both %lld", Brightness(plain), Brightness(spec), nSpec, Brightness(envOnly), nEnv, Brightness(both));
    CHECK(nSpec > 100 && Brightness(spec) > Brightness(plain));
    CHECK(nEnv > 100 && Brightness(envOnly) > Brightness(plain));
    CHECK(Brightness(both) > Brightness(spec) && Brightness(both) > Brightness(envOnly));
    configure(false, false);
    CHECKV(CountDiff(plain, render(), 1) == 0, "back to plain reproduces the plain frame (%d px differ)", CountDiff(plain, render(), 1));
    RwTextureDestroy(env); RwTextureDestroy(specTex);
    RpClumpDestroy(clump);
}

// ---------------------------------------------------------------------------------------------------------------------------------
int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    _set_error_mode(_OUT_TO_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    std::vector<const char*> dffs;
    for (int i = 1; i < argc; i++) {
        if (!std::strcmp(argv[i], "-v")) g_verbose = true; else dffs.push_back(argv[i]);
    }
    MapDataImage();
    HWND wnd = CreateWindowA("STATIC", "rw_custom_pipelines_test", WS_OVERLAPPEDWINDOW, 0, 0, 320, 240, nullptr, nullptr, GetModuleHandleA(nullptr), nullptr);
    CHECK(wnd != nullptr);
    CHECK(RwEngineInit(nullptr, 0, 0) == TRUE);
    CHECK(RpWorldPluginAttach() == TRUE);
    CHECK(RpAnisotPluginAttach() == TRUE);
    SetupTests(); // game plugin attach (before RwEngineOpen, like PluginAttach() in app.cpp)
    RwEngineOpenParams params{ wnd };
    CHECK(RwEngineOpen(&params) == TRUE);
    CHECK(RwEngineStart() == TRUE); // wires RwShimPipelineEnsure()
    g_dev = rw::d3d::d3ddevice;
    std::printf("D3D9 device: %s\n", g_dev ? "yes" : "NO (nothing testable without a device)");
    RwEngineInstance->dOpenDevice.zBufferNear = 0; RwEngineInstance->dOpenDevice.zBufferFar = 1;
    if (g_dev) {
        SetupScene();
        PostStartTests();
        BuildingTests();
        DNTests();
        CarTests();
        StreamTests();
        for (const char* p : dffs) {
            const std::string s = p;
            if (s.find("infernus") != std::string::npos) CarAssetTest(p); else BuildingAssetTest(p);
        }
        CCustomCarEnvMapPipeline::DestroyPipe();
        CCustomBuildingRenderer::Shutdown();
        TeardownScene();
        CHECK(RwEngineStop() == TRUE); // RwShimPipelineShutdown()
    }
    RwEngineClose();
    RwEngineTerm();
    DestroyWindow(wnd);
    std::printf("\nrw_custom_pipelines_test: %d checks passed, %d failed\n%s\n", g_pass, g_fail, g_fail ? "FAILED" : "PASSED");
    return g_fail;
}

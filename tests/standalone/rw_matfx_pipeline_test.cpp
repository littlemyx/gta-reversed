// P2B-23 unit test: the stock D3D9 MatFX AllInOne pipeline on the RxPipeline façade (source/standalone/rw/pipeline_matfx.cpp), rendered through a real D3D9
// device (Wine/wined3d is fine) and read back pixel by pixel.
//  1. plumbing: RwShimPipelineEnsure installs the MatFX pipeline as librw's D3D9 MatFX pipeline (plugin id 0x120), RpMatFXAtomicEnableEffects hands it to atomics
//     (only while the MatFX flag is clear), Shutdown restores librw's pipeline
//  2. NULL effect, unlit silhouette, prelit colour, lit (ambient) mesh: the exe's DefaultRender
//  3. ENVMAP: sphere map from the camera-space normal (left half red / right half blue env texture), coefficient 0.5 (two-pass SRCALPHA/ONE or the single
//     pass MULTIPLYADD) and 1.0 (single pass ADD), coefficient 0, no base texture, the env frame matrix path (flips u), env texture with alpha (pixel-shader path);
//     the env pass restores lighting / z write / blend states / texture transform
//  4. UVTRANSFORM: translate 0.5 swaps the two texels, scale 0.5, identity-flagged matrix ignored, stage transform flags reset
//  5. DUAL: the blend-mode table of 0x812D40 (DESTCOLOR/ZERO, SRCALPHA/INVSRCALPHA, ONE/ONE, ZERO/SRCALPHA, DESTCOLOR/SRCCOLOR) with / without a base texture,
//     an unsupported mode (SRCALPHA/ONE: second alpha-blended pass), DUALUVTRANSFORM (the dual texture shifted by its own matrix)
//  6. BUMPENVMAP falls back to plain + env; optional argv[1..] = SA DFFs (infernus.dff ...): the MatFX atomics come out of the stream reader with our pipeline and render
// Usage: rw_matfx_pipeline_test.exe [-v] [file.dff ...]. Needs a D3D9 HAL adapter. Exit code 0 = all checks passed.
#include "fakerw.h"
#include <src/d3d/rwd3dimpl.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

void NotsaRwRenderState_OnEngineStarted();
static int g_fail = 0, g_pass = 0;
static bool g_verbose = false;
#define CHECK(c) do { const bool ok_ = !!(c); if (ok_) ++g_pass; else ++g_fail; if (!ok_ || g_verbose) std::printf("%-4s %s\n", ok_ ? "ok" : "FAIL", #c); } while (0)
#define CHECKV(c, fmt, ...) do { const bool ok_ = !!(c); if (ok_) ++g_pass; else ++g_fail; if (!ok_ || g_verbose) std::printf("%-4s %s   " fmt "\n", ok_ ? "ok" : "FAIL", #c, __VA_ARGS__); } while (0)

static IDirect3DDevice9* g_dev = nullptr;

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
static bool NearC(unsigned px, int r, int g, int b, int tol = 4) {
    return std::abs(int((px >> 16) & 0xFF) - r) <= tol && std::abs(int((px >> 8) & 0xFF) - g) <= tol && std::abs(int(px & 0xFF) - b) <= tol;
}

// ---------------------------------------------------------------------------------------------------------------------------------
// scene helpers (camera at the origin looking along +z, a quad of +-3 at z = 5 covers the whole view; u = (x + 3) / 6)
static RwCamera* g_cam;
static RwFrame*  g_camFrame;
static RwRaster *g_fb, *g_zb;
static int g_w, g_h;

static void SetupCamera() {
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
    RwCameraSetFarClipPlane(g_cam, 100.0f);
}

static RwTexture* MakeTex(int w, int h, const RwRGBA* px, bool alpha = false) {
    RwRaster* r = RwRasterCreate(w, h, 32, rwRASTERTYPETEXTURE | (alpha ? rwRASTERFORMAT8888 : rwRASTERFORMAT888));
    unsigned char* p = RwRasterLock(r, 0, rwRASTERLOCKWRITE);
    const int stride = RwRasterGetStride(r);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            const RwRGBA c = px[y * w + x];
            unsigned char* d = p + y * stride + x * 4;
            d[0] = c.blue; d[1] = c.green; d[2] = c.red; d[3] = alpha ? c.alpha : 255;
        }
    RwRasterUnlock(r);
    RwTexture* t = RwTextureCreate(r);
    RwTextureSetFilterMode(t, rwFILTERNEAREST);
    RwTextureSetAddressing(t, rwTEXTUREADDRESSWRAP);
    return t;
}
static RwTexture* Solid(RwRGBA c, bool alpha = false) { RwRGBA px[2] = { c, c }; return MakeTex(2, 1, px, alpha); }
static RwTexture* Halves(RwRGBA l, RwRGBA r, bool alpha = false) { RwRGBA px[2] = { l, r }; return MakeTex(2, 1, px, alpha); }
static void FreeTex(RwTexture* t) { if (t) RwTextureDestroy(t); }

static const RwRGBA RED{ 255, 0, 0, 255 }, BLUE{ 0, 0, 255, 255 }, GREEN{ 0, 255, 0, 255 };

struct QuadOpts { bool normals = false; bool texcoords = false; RwRGBA prelit = { 255, 255, 255, 255 }; bool prelitOn = true; bool light = false; };

static RpGeometry* MakeQuad(const QuadOpts& o, RpMaterial* mat) {
    RwUInt32 flags = rpGEOMETRYPOSITIONS;
    if (o.normals) flags |= rpGEOMETRYNORMALS;
    if (o.texcoords) flags |= rpGEOMETRYTEXTURED;
    if (o.prelitOn) flags |= rpGEOMETRYPRELIT;
    if (o.light) flags |= rpGEOMETRYLIGHT | rpGEOMETRYMODULATEMATERIALCOLOR;
    RpGeometry* g = RpGeometryCreate(4, 2, flags);
    RwV3d* v = RpMorphTargetGetVertices(RpGeometryGetMorphTarget(g, 0));
    const float xs[4] = { -3, 3, -3, 3 }, ys[4] = { -3, -3, 3, 3 };
    for (int i = 0; i < 4; i++) v[i] = { xs[i], ys[i], 5.0f };
    if (o.normals) {
        RwV3d* n = g->morphTargets[0].normals;
        for (int i = 0; i < 4; i++) n[i] = o.light ? RwV3d{ 0, 0, -1 } : RwV3d{ xs[i] < 0 ? -1.0f : 1.0f, 0, 0 };
    }
    if (o.prelitOn) for (int i = 0; i < 4; i++) g->colors[i] = o.prelit;
    if (o.texcoords) for (int i = 0; i < 4; i++) g->texCoords[0][i] = { (xs[i] + 3) / 6, (3 - ys[i]) / 6 };
    RpTriangle* t = RpGeometryGetTriangles(g);
    RpGeometryTriangleSetVertexIndices(g, &t[0], 0, 1, 2);
    RpGeometryTriangleSetVertexIndices(g, &t[1], 1, 3, 2);
    RpGeometryTriangleSetMaterial(g, &t[0], mat);
    RpGeometryTriangleSetMaterial(g, &t[1], mat);
    RpGeometryUnlock(g);
    return g;
}

static RpAtomic* MakeAtomic(RpGeometry* g) {
    RpAtomic* a = RpAtomicCreate();
    RpAtomicSetFrame(a, RwFrameCreate());
    RpAtomicSetGeometry(a, g, 0);
    RpMatFXAtomicEnableEffects(a);
    return a;
}
static void FreeAtomic(RpAtomic* a) {
    RwFrame* f = RpAtomicGetFrame(a);
    RpAtomicSetFrame(a, nullptr);
    RpAtomicDestroy(a);
    if (f) RwFrameDestroy(f);
}

static void BeginScene(RwRGBA clear = { 0, 0, 0, 255 }) {
    RwCameraBeginUpdate(g_cam);
    RwCameraClear(g_cam, &clear, rwCAMERACLEARIMAGE | rwCAMERACLEARZ);
}
static void EndScene() { RwCameraEndUpdate(g_cam); }

static RpMaterial* MakeMat(RwTexture* tex, RwUInt32 effect = rpMATFXEFFECTNULL) {
    RpMaterial* m = RpMaterialCreate();
    m->color = { 255, 255, 255, 255 };
    if (tex) RpMaterialSetTexture(m, tex);
    if (effect) RpMatFXMaterialSetEffects(m, (RpMatFXMaterialFlags)effect);
    return m;
}

static unsigned PxL() { return ReadPixel(g_w / 4, g_h / 2); }
static unsigned PxR() { return ReadPixel(3 * g_w / 4, g_h / 2); }
static DWORD RS(D3DRENDERSTATETYPE t) { DWORD v = 0; RwD3D9GetRenderState(t, &v); return v; }
static DWORD TS(UINT stage, D3DTEXTURESTAGESTATETYPE t) { DWORD v = 0; RwD3D9GetTextureStageState(stage, t, &v); return v; }

// renders one atomic over a cleared target; returns left / right sample
struct Sample { unsigned l, r; };
static Sample Render(RpAtomic* a) {
    BeginScene();
    RpAtomicRender(a);
    EndScene();
    return { PxL(), PxR() };
}
static void Expect(const char* what, Sample s, int lr, int lg, int lb, int rr, int rg, int rb, int tol = 4) {
    CHECKV(NearC(s.l, lr, lg, lb, tol) && NearC(s.r, rr, rg, rb, tol), "%s: L %06X (want %02X%02X%02X) R %06X (want %02X%02X%02X)", what, s.l & 0xFFFFFF,
           lr, lg, lb, s.r & 0xFFFFFF, rr, rg, rb);
}

// state that every render must leave behind
static void CheckClean(const char* what, bool vertexAlphaOff = true) {
    IDirect3DPixelShader9* ps = nullptr;
    g_dev->GetPixelShader(&ps);
    CHECKV(ps == nullptr, "%s: pixel shader unbound", what);
    if (ps) ps->Release();
    CHECKV(TS(0, D3DTSS_TEXTURETRANSFORMFLAGS) == 0 && TS(1, D3DTSS_TEXTURETRANSFORMFLAGS) == 0, "%s: texture transforms off (%u %u)", what, TS(0, D3DTSS_TEXTURETRANSFORMFLAGS), TS(1, D3DTSS_TEXTURETRANSFORMFLAGS));
    CHECKV(RS(D3DRS_ZWRITEENABLE) == TRUE, "%s: z write restored", what);
    void* v = nullptr;
    RwRenderStateGet(rwRENDERSTATEVERTEXALPHAENABLE, &v);
    // (the render callback leaves the vertex-alpha state of the LAST mesh behind, like the exe's; the effect renderers themselves switch it off again)
    CHECKV(!vertexAlphaOff || v == nullptr, "%s: vertex alpha off", what);
    RwRenderStateGet(rwRENDERSTATESRCBLEND, &v);
    CHECKV((uintptr_t)v == rwBLENDSRCALPHA, "%s: src blend restored (%u)", what, (unsigned)(uintptr_t)v);
    RwRenderStateGet(rwRENDERSTATEDESTBLEND, &v);
    CHECKV((uintptr_t)v == rwBLENDINVSRCALPHA, "%s: dest blend restored (%u)", what, (unsigned)(uintptr_t)v);
}

// ---------------------------------------------------------------------------------------------------------------------------------
static void PlumbingTests() {
    std::printf("--- plumbing\n");
    rw::ObjPipeline* slot = rw::matFXGlobals.pipelines[rw::PLATFORM_D3D9];
    CHECK(slot != nullptr && slot->pluginID == 0x120);
    CHECK(slot->impl.render != nullptr);
    RpAtomic* a = RpAtomicCreate();
    CHECK(RpMatFXAtomicQueryEffects(a) == FALSE);
    CHECK(RpMatFXAtomicEnableEffects(a) == a);
    CHECK(RpMatFXAtomicQueryEffects(a) == TRUE && a->pipeline == slot);
    a->pipeline = nullptr;
    CHECK(RpMatFXAtomicEnableEffects(a) == a && a->pipeline == nullptr);   // 0x811C00: only touches the pipeline while the flag is clear
    CHECK(RpMatFXAtomicEnableEffects(nullptr) == nullptr);
    RpAtomicDestroy(a);
    RwShimPipelineEnsure();                                                // idempotent
    CHECK(rw::matFXGlobals.pipelines[rw::PLATFORM_D3D9] == slot);
}

static void DefaultTests() {
    std::printf("--- NULL effect / DefaultRender\n");
    RwTexture* gray = Solid({ 100, 100, 100, 255 });
    {
        RpMaterial* m = MakeMat(gray);
        QuadOpts o; o.texcoords = true;
        RpGeometry* g = MakeQuad(o, m); RpAtomic* a = MakeAtomic(g);
        Expect("textured prelit white", Render(a), 100, 100, 100, 100, 100, 100);
        CheckClean("default");
        FreeAtomic(a); RpGeometryDestroy(g); RpMaterialDestroy(m);
    }
    {
        RpMaterial* m = MakeMat(gray);
        QuadOpts o; o.texcoords = true; o.prelit = { 200, 100, 50, 255 };
        RpGeometry* g = MakeQuad(o, m); RpAtomic* a = MakeAtomic(g);
        Expect("textured * prelit (200,100,50)", Render(a), 78, 39, 20, 78, 39, 20);
        FreeAtomic(a); RpGeometryDestroy(g); RpMaterialDestroy(m);
    }
    {
        RpMaterial* m = MakeMat(nullptr);
        QuadOpts o; o.prelit = { 200, 100, 50, 255 };
        RpGeometry* g = MakeQuad(o, m); RpAtomic* a = MakeAtomic(g);
        Expect("untextured prelit", Render(a), 200, 100, 50, 200, 100, 50);
        FreeAtomic(a); RpGeometryDestroy(g); RpMaterialDestroy(m);
    }
    {
        RpMaterial* m = MakeMat(gray);
        QuadOpts o; o.texcoords = true; o.prelitOn = false;
        RpGeometry* g = MakeQuad(o, m); RpAtomic* a = MakeAtomic(g);
        Expect("no light, no prelit -> black silhouette", Render(a), 0, 0, 0, 0, 0, 0);
        FreeAtomic(a); RpGeometryDestroy(g); RpMaterialDestroy(m);
    }
    {   // lit: ambient light in the camera's world, material ambient 1
        RpWorld* world = RpWorldCreate(nullptr);
        RpLight* amb = RpLightCreate(rpLIGHTAMBIENT);
        RwRGBAReal white{ 1, 1, 1, 1 };
        RpLightSetColor(amb, &white);
        RpWorldAddLight(world, amb);
        RpWorldAddCamera(world, g_cam);
        RpMaterial* m = MakeMat(gray);
        m->surfaceProps = { 1.0f, 0.0f, 0.0f };
        QuadOpts o; o.texcoords = true; o.normals = true; o.light = true; o.prelitOn = false;
        RpGeometry* g = MakeQuad(o, m); RpAtomic* a = MakeAtomic(g);
        Expect("ambient lit textured", Render(a), 100, 100, 100, 100, 100, 100, 6);
        FreeAtomic(a); RpGeometryDestroy(g); RpMaterialDestroy(m);
        RpWorldRemoveCamera(world, g_cam);
        RpWorldRemoveLight(world, amb);
        RpLightDestroy(amb);
        RpWorldDestroy(world);
    }
    FreeTex(gray);
}

static void EnvTests() {
    std::printf("--- ENVMAP\n");
    RwTexture* base = Solid({ 100, 100, 100, 255 });
    RwTexture* env  = Halves(RED, BLUE);
    RwTexture* envDim = Halves({ 60, 0, 0, 255 }, { 0, 0, 60, 255 });
    if (g_verbose) {
        DWORD mag = 0;
        g_dev->GetSamplerState(1, D3DSAMP_MAGFILTER, &mag); std::printf("  stage1 MAG before: %u\n", (unsigned)mag);
        RwD3D9SetTexture(env, 1);
        g_dev->GetSamplerState(1, D3DSAMP_MAGFILTER, &mag); std::printf("  stage1 MAG after SetTexture(env,1): %u (tex filter %d aniso %d)\n", (unsigned)mag, (int)env->getFilter(), (int)env->getMaxAnisotropy());
        RwD3D9SetTexture(nullptr, 1);
    }
    const QuadOpts withBase = [] { QuadOpts o; o.texcoords = true; o.normals = true; return o; }();
    auto run = [&](const char* what, RwTexture* b, RwTexture* e, float coef, RwFrame* frame, bool fbAlpha, Sample* out = nullptr) {
        RpMaterial* m = MakeMat(b, rpMATFXEFFECTENVMAP);
        rw::MatFX* fx = rw::MatFX::get(m);
        fx->setEnvTexture(e); fx->setEnvCoefficient(coef); fx->setEnvFrame(frame); fx->setEnvFBAlpha(fbAlpha);
        QuadOpts o = withBase; o.texcoords = b != nullptr;
        RpGeometry* g = MakeQuad(o, m); RpAtomic* a = MakeAtomic(g);
        const Sample s = Render(a);
        CheckClean(what);
        if (out) *out = s;
        FreeAtomic(a); RpGeometryDestroy(g); RpMaterialDestroy(m);
        return s;
    };
    Sample s;
    s = run("env 0.5", base, env, 0.5f, nullptr, false);
    if (g_verbose) { DWORD mag = 0, mn = 0; g_dev->GetSamplerState(0, D3DSAMP_MAGFILTER, &mag); g_dev->GetSamplerState(1, D3DSAMP_MAGFILTER, &mn); std::printf("  sampler MAG filter stage0 %u stage1 %u; env filter %d\n", (unsigned)mag, (unsigned)mn, (int)env->getFilter()); }
    Expect("env coef 0.5: base + env * 128/255", s, 228, 100, 100, 100, 100, 228);
    s = run("env 1.0", base, envDim, 1.0f, nullptr, false);
    Expect("env coef 1.0 (single pass ADD or two pass)", s, 160, 100, 100, 100, 100, 160);
    s = run("env 0", base, env, 0.0f, nullptr, false);
    Expect("env coef 0 -> plain mesh", s, 100, 100, 100, 100, 100, 100);
    s = run("env 0.25 fbAlpha", base, env, 0.25f, nullptr, true);
    Expect("env coef 0.25 (TFACTOR 64)", s, 164, 100, 100, 100, 100, 164);
    {   // no base texture: the diffuse (prelit gray 100) + env
        RpMaterial* m = MakeMat(nullptr, rpMATFXEFFECTENVMAP);
        rw::MatFX* fx = rw::MatFX::get(m);
        fx->setEnvTexture(envDim); fx->setEnvCoefficient(1.0f);
        QuadOpts o; o.normals = true; o.prelit = { 100, 100, 100, 255 };
        RpGeometry* g = MakeQuad(o, m); RpAtomic* a = MakeAtomic(g);
        Expect("env coef 1.0 without base texture", Render(a), 160, 100, 100, 100, 100, 160);
        CheckClean("env no base");
        fx->setEnvCoefficient(0.5f);
        Expect("env coef 0.5 without base texture", Render(a), 100 + 60 * 128 / 255, 100, 100, 100, 100, 100 + 60 * 128 / 255);
        FreeAtomic(a); RpGeometryDestroy(g); RpMaterialDestroy(m);
    }
    {   // the env frame: camera LTM identity and an identity frame -> the matrix flips u (0x814270: right axis negated)
        RwFrame* ef = RwFrameCreate();
        s = run("env frame", base, env, 0.5f, ef, false);
        Expect("env frame matrix: u flipped", s, 100, 100, 228, 228, 100, 100);
        RwFrameDestroy(ef);
    }
    {   // an env texture with alpha: pixel-shader path (base + env.rgb * env.a * coef) when the device has ps_1_1, else the alpha-blended second pass
        RwTexture* envA = Halves({ 255, 0, 0, 128 }, { 0, 0, 255, 128 }, true);
        s = run("env alpha", base, envA, 0.5f, nullptr, false);
        Expect("env with alpha 128, coef 0.5", s, 100 + 64, 100, 100, 100, 100, 100 + 64, 6);
        // no base texture: the second shader (0x8853A0): diffuse + env.rgb * env.a * coef
        {
            RpMaterial* m = MakeMat(nullptr, rpMATFXEFFECTENVMAP);
            rw::MatFX* fx = rw::MatFX::get(m);
            fx->setEnvTexture(envA); fx->setEnvCoefficient(0.5f);
            QuadOpts o; o.normals = true; o.prelit = { 100, 100, 100, 255 };
            RpGeometry* g = MakeQuad(o, m); RpAtomic* a = MakeAtomic(g);
            Expect("env with alpha, no base texture", Render(a), 100 + 64, 100, 100, 100, 100, 100 + 64, 6);
            CheckClean("env alpha nobase");
            FreeAtomic(a); RpGeometryDestroy(g); RpMaterialDestroy(m);
        }
        FreeTex(envA);
    }
    {   // vertex alpha: not eligible for any single pass -> generic second pass, alpha blended onto the first
        RpMaterial* m = MakeMat(base, rpMATFXEFFECTENVMAP);
        rw::MatFX* fx = rw::MatFX::get(m);
        fx->setEnvTexture(env); fx->setEnvCoefficient(0.5f);
        QuadOpts o = withBase; o.prelit = { 255, 255, 255, 128 };   // vertex alpha: the first pass blends the base (100) at 128/255 over black = 50
        RpGeometry* g = MakeQuad(o, m); RpAtomic* a = MakeAtomic(g);
        // ... and the env pass adds env * 128/255 * (alpha_env = 1)
        // the env pass runs with black fog and restores the fog colour
        RwD3D9SetRenderState(D3DRS_FOGENABLE, TRUE);
        RwD3D9SetRenderState(D3DRS_FOGCOLOR, 0x00123456);
        RwD3D9SetRenderState(D3DRS_FOGTABLEMODE, D3DFOG_NONE);
        RwD3D9SetRenderState(D3DRS_FOGVERTEXMODE, D3DFOG_LINEAR);   // far away: the scene is not fogged
        const float fogStart = 1000.0f, fogEnd = 2000.0f;
        RwD3D9SetRenderState(D3DRS_FOGSTART, *reinterpret_cast<const DWORD*>(&fogStart));
        RwD3D9SetRenderState(D3DRS_FOGEND, *reinterpret_cast<const DWORD*>(&fogEnd));
        Expect("env on a translucent (vertex alpha) mesh", Render(a), 50 + 128, 50, 50, 50, 50, 50 + 128, 6);
        CheckClean("env translucent");
        CHECKV(RS(D3DRS_FOGCOLOR) == 0x00123456 && RS(D3DRS_FOGENABLE) == TRUE, "fog colour restored (%08X)", (unsigned)RS(D3DRS_FOGCOLOR));
        RwD3D9SetRenderState(D3DRS_FOGENABLE, FALSE);
        RwD3D9SetRenderState(D3DRS_FOGVERTEXMODE, D3DFOG_NONE);
        CHECKV(RS(D3DRS_LIGHTING) == FALSE, "lighting state restored by the env pass (%u)", (unsigned)RS(D3DRS_LIGHTING));
        FreeAtomic(a); RpGeometryDestroy(g); RpMaterialDestroy(m);
    }
    {   // BUMPENVMAP: plain mesh + the env pass (no bump perturbation)
        RpMaterial* m = MakeMat(base, rpMATFXEFFECTBUMPENVMAP);
        rw::MatFX* fx = rw::MatFX::get(m);
        fx->setEnvTexture(env); fx->setEnvCoefficient(0.5f);
        RpGeometry* g = MakeQuad(withBase, m); RpAtomic* a = MakeAtomic(g);
        Expect("bump env map -> plain + env", Render(a), 228, 100, 100, 100, 100, 228);
        FreeAtomic(a); RpGeometryDestroy(g); RpMaterialDestroy(m);
    }
    FreeTex(base); FreeTex(env); FreeTex(envDim);
}

static void UVTests() {
    std::printf("--- UVTRANSFORM\n");
    RwTexture* tex = Halves(RED, GREEN);
    auto run = [&](const char* what, float sx, float tx, bool identityFlag) {
        RpMaterial* m = MakeMat(tex, rpMATFXEFFECTUVTRANSFORM);
        rw::Matrix* mat = rw::Matrix::create();
        mat->right.x = sx; mat->pos.x = tx;
        if (!identityFlag) RwMatrixUpdate(mat);
        rw::MatFX::get(m)->setUVTransformMatrices(mat, nullptr);
        QuadOpts o; o.texcoords = true;
        RpGeometry* g = MakeQuad(o, m); RpAtomic* a = MakeAtomic(g);
        const Sample s = Render(a);
        CheckClean(what);
        FreeAtomic(a); RpGeometryDestroy(g); RpMaterialDestroy(m);
        mat->destroy();
        return s;
    };
    // RW's view matrix mirrors x: the quad's +x side (large u) is on the screen's LEFT
    Expect("uv identity flagged matrix is ignored", run("uv ident", 1.0f, 0.5f, true), 0, 255, 0, 255, 0, 0);
    Expect("uv translate 0.5 swaps the texels", run("uv translate", 1.0f, 0.5f, false), 255, 0, 0, 0, 255, 0);
    Expect("uv scale 0.5: u in [0.08,0.46] / [0.29,0.65]... both red", run("uv scale", 0.5f, 0.0f, false), 255, 0, 0, 255, 0, 0);
    // a following plain material is not shifted
    RpMaterial* m = MakeMat(tex);
    QuadOpts o; o.texcoords = true;
    RpGeometry* g = MakeQuad(o, m); RpAtomic* a = MakeAtomic(g);
    Expect("plain material after the transform", Render(a), 0, 255, 0, 255, 0, 0);
    FreeAtomic(a); RpGeometryDestroy(g); RpMaterialDestroy(m);
    FreeTex(tex);
}

static void DualTests() {
    std::printf("--- DUAL / DUALUVTRANSFORM\n");
    const auto& caps = rw::d3d::d3d9Globals.caps;
    std::printf("  caps: MaxTextureBlendStages %u MaxSimultaneousTextures %u TextureOpCaps %08X PixelShaderVersion %08X\n", (unsigned)caps.MaxTextureBlendStages,
                (unsigned)caps.MaxSimultaneousTextures, (unsigned)caps.TextureOpCaps, (unsigned)caps.PixelShaderVersion);
    auto run = [&](const char* what, RwTexture* base, RwTexture* dual, RwBlendFunction src, RwBlendFunction dst, bool uv = false, float uvTx = 0.5f, RwRGBA prelit = { 255, 255, 255, 255 }) {
        RpMaterial* m = MakeMat(base, uv ? rpMATFXEFFECTDUALUVTRANSFORM : rpMATFXEFFECTDUAL);
        rw::MatFX* fx = rw::MatFX::get(m);
        fx->setDualTexture(dual); fx->setDualSrcBlend(src); fx->setDualDestBlend(dst);
        rw::Matrix* mat = nullptr;
        if (uv) {
            mat = rw::Matrix::create();
            mat->pos.x = uvTx;
            RwMatrixUpdate(mat);
            fx->setUVTransformMatrices(nullptr, mat);
        }
        QuadOpts o; o.texcoords = true; o.prelit = prelit;
        RpGeometry* g = MakeQuad(o, m); RpAtomic* a = MakeAtomic(g);
        const Sample s = Render(a);
        CheckClean(what);
        DWORD stage1 = TS(1, D3DTSS_COLOROP);
        (void)stage1;
        FreeAtomic(a); RpGeometryDestroy(g); RpMaterialDestroy(m);
        if (mat) mat->destroy();
        return s;
    };
    RwTexture* a200 = Solid({ 200, 200, 200, 255 });
    RwTexture* a100 = Solid({ 100, 100, 100, 255 });
    RwTexture* b128 = Solid({ 128, 128, 128, 255 });
    RwTexture* b60  = Solid({ 60, 60, 60, 255 });
    RwTexture* bRedA = Solid({ 255, 0, 0, 128 }, true);
    RwTexture* bRed100A = Solid({ 100, 0, 0, 128 }, true);
    Expect("DESTCOLOR/ZERO: base * dual", run("dual mul", a200, b128, rwBLENDDESTCOLOR, rwBLENDZERO), 100, 100, 100, 100, 100, 100);
    Expect("ZERO/SRCCOLOR: base * dual", run("dual mul2", a200, b128, rwBLENDZERO, rwBLENDSRCCOLOR), 100, 100, 100, 100, 100, 100);
    Expect("DESTCOLOR/ZERO without base texture", run("dual mul nobase", nullptr, b128, rwBLENDDESTCOLOR, rwBLENDZERO, false, 0, { 200, 200, 200, 255 }), 100, 100, 100, 100, 100, 100);
    Expect("SRCALPHA/INVSRCALPHA: dual over base by its alpha", run("dual alpha", a200, bRedA, rwBLENDSRCALPHA, rwBLENDINVSRCALPHA), 228, 100, 100, 228, 100, 100, 5);
    // (textured geometry, NULL material texture: stage 0 keeps its MODULATE alpha, so the dual texture's alpha 128 also blends the result over the black clear)
    Expect("SRCALPHA/INVSRCALPHA without base texture", run("dual alpha nobase", nullptr, bRedA, rwBLENDSRCALPHA, rwBLENDINVSRCALPHA, false, 0, { 255, 255, 255, 255 }), 128, 64, 64, 128, 64, 64, 5);
    Expect("ONE/ONE: base + dual", run("dual add", a100, b60, rwBLENDONE, rwBLENDONE), 160, 160, 160, 160, 160, 160);
    Expect("ZERO/SRCALPHA: base * alpha(dual)", run("dual za", a200, bRedA, rwBLENDZERO, rwBLENDSRCALPHA), 100, 100, 100, 100, 100, 100, 5);
    Expect("DESTCOLOR/SRCCOLOR: 2 * base * dual", run("dual 2x", a100, a100, rwBLENDDESTCOLOR, rwBLENDSRCCOLOR), 78, 78, 78, 78, 78, 78);
    Expect("SRCALPHA/ONE (unsupported -> second pass)", run("dual 2pass", a100, bRed100A, rwBLENDSRCALPHA, rwBLENDONE), 150, 100, 100, 150, 100, 100, 5);
    Expect("ONE/ONE with a coloured dual texture", run("dual 2pass b", a100, bRed100A, rwBLENDONE, rwBLENDONE), 200, 100, 100, 200, 100, 100, 5);
    {   // DUALUVTRANSFORM: black base + two-coloured dual shifted by 0.5 (additive)
        RwTexture* dual = Halves(RED, GREEN);
        RwTexture* black = Solid({ 0, 0, 0, 255 });
        Expect("DUALUVTRANSFORM ONE/ONE: dual shifted by its own matrix", run("dual uv", black, dual, rwBLENDONE, rwBLENDONE, true, 0.5f), 255, 0, 0, 0, 255, 0);
        Expect("DUALUVTRANSFORM unsupported mode (second pass)", run("dual uv 2pass", black, dual, rwBLENDONE, rwBLENDSRCCOLOR, true, 0.5f), 255, 0, 0, 0, 255, 0);
        Expect("DUALUVTRANSFORM, no shift visible (matrix pos 1.0 wraps)", run("dual uv wrap", black, dual, rwBLENDONE, rwBLENDONE, true, 1.0f), 0, 255, 0, 255, 0, 0);
        FreeTex(dual); FreeTex(black);
    }
    FreeTex(a200); FreeTex(a100); FreeTex(b128); FreeTex(b60); FreeTex(bRedA); FreeTex(bRed100A);
}

static void CollectAtomic(RpAtomic* a, void* d) { static_cast<std::vector<RpAtomic*>*>(d)->push_back(a); }
static RpAtomic* CollectCB(RpAtomic* a, void* d) { CollectAtomic(a, d); return a; }

static std::vector<uint8_t> ReadFile(const char* path) {
    std::vector<uint8_t> b;
    if (FILE* f = std::fopen(path, "rb")) {
        std::fseek(f, 0, SEEK_END); b.resize(std::ftell(f)); std::fseek(f, 0, SEEK_SET);
        b.resize(std::fread(b.data(), 1, b.size(), f)); std::fclose(f);
    }
    return b;
}

static void DffTests(const char* path) {
    std::printf("--- real asset: %s\n", path);
    auto bytes = ReadFile(path);
    CHECKV(!bytes.empty(), "read %s", path);
    if (bytes.empty()) return;
    rw::StreamMemory sm;
    sm.open(bytes.data(), (std::uint32_t)bytes.size());
    std::uint32_t len = 0, ver = 0;
    CHECK(rw::findChunk(&sm, rw::ID_CLUMP, &len, &ver));
    RpClump* clump = RpClumpStreamRead(&sm);
    CHECK(clump != nullptr);
    if (!clump) return;
    std::vector<RpAtomic*> atoms;
    RpClumpForAllAtomics(clump, CollectCB, &atoms);
    rw::ObjPipeline* slot = rw::matFXGlobals.pipelines[rw::PLATFORM_D3D9];
    int flagged = 0, rendered = 0, envMats = 0;
    for (RpAtomic* a : atoms) {
        if (RpMatFXAtomicQueryEffects(a)) {
            ++flagged;
            CHECK(a->pipeline == slot);
        }
        if (!a->geometry || !a->geometry->meshHeader || a->geometry->numVertices <= 0) continue;
        for (int i = 0; i < a->geometry->matList.numMaterials; i++)
            envMats += RpMatFXMaterialGetEffects(a->geometry->matList.materials[i]) == rpMATFXEFFECTENVMAP;
        BeginScene();
        RpAtomicRender(a);
        EndScene();
        ++rendered;
        CHECK(a->geometry->instData != nullptr);
    }
    std::printf("  %zu atomics, %d with the MatFX flag, %d rendered, %d env-map materials\n", atoms.size(), flagged, rendered, envMats);
    CHECK(rendered > 0);
    CheckClean("dff", false);
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
    HWND wnd = CreateWindowA("STATIC", "rw_matfx_pipeline_test", WS_OVERLAPPEDWINDOW, 0, 0, 320, 240, nullptr, nullptr, GetModuleHandleA(nullptr), nullptr);
    CHECK(wnd != nullptr);
    rw::MemoryFunctions mf{};
    mf.rwmalloc  = [](size_t sz, unsigned) -> void* { return sz ? std::malloc(sz) : nullptr; };
    mf.rwrealloc = [](void* p, size_t sz, unsigned) -> void* { return std::realloc(p, sz); };
    mf.rwfree    = [](void* p) { std::free(p); };
    CHECK(rw::Engine::init(&mf));
    CHECK(RpWorldPluginAttach() == TRUE);
    CHECK(RpMatFXPluginAttach() == TRUE);
    rw::registerAnisotropyPlugin();
    rw::EngineOpenParams params{};
    params.window = wnd;
    CHECK(rw::Engine::open(&params));
    rw::Engine::start();
    g_dev = rw::d3d::d3ddevice;
    if (g_dev) NotsaRwRenderState_OnEngineStarted();
    if (g_dev) {
        // NotsaRwRenderState_OnEngineStarted sets the sampler filters of stages 1..7 to LINEAR behind librw's per-stage filter cache (which keeps believing NEAREST),
        // so a NEAREST texture on those stages would keep sampling linearly (finding, see the report). The test's textures are nearest: make the device agree.
        for (UINT st = 1; st < 8; st++) {
            rw::d3d::setSamplerState(st, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
            rw::d3d::setSamplerState(st, D3DSAMP_MINFILTER, D3DTEXF_POINT);
        }
    }
    if (g_dev) RwRenderStateSet(rwRENDERSTATECULLMODE, reinterpret_cast<void*>(uintptr_t(rwCULLMODECULLNONE)));
    std::printf("D3D9 device: %s\n", g_dev ? "yes" : "NO (nothing testable without a device)");
    RwEngineInstance->dOpenDevice.zBufferNear = 0; RwEngineInstance->dOpenDevice.zBufferFar = 1;
    if (g_dev) {
        SetupCamera();
        rw::ObjPipeline* librwPipe = rw::matFXGlobals.pipelines[rw::PLATFORM_D3D9];
        RwShimPipelineEnsure();
        PlumbingTests();
        DefaultTests();
        EnvTests();
        UVTests();
        DualTests();
        for (const char* p : dffs) DffTests(p);
        RwCameraSetRaster(g_cam, nullptr); RwCameraSetZRaster(g_cam, nullptr);
        RwRasterDestroy(g_fb); RwRasterDestroy(g_zb);
        RwCameraSetFrame(g_cam, nullptr);
        RwCameraDestroy(g_cam); RwFrameDestroy(g_camFrame);
        RwShimPipelineShutdown();
        CHECK(rw::matFXGlobals.pipelines[rw::PLATFORM_D3D9] == librwPipe);   // librw's own pipeline is back for matfxClose
        rw::Engine::stop();
    }
    rw::Engine::close();
    rw::Engine::term();
    DestroyWindow(wnd);
    std::printf("\nrw_matfx_pipeline_test: %d checks passed, %d failed\n%s\n", g_pass, g_fail, g_fail ? "FAILED" : "PASSED");
    return g_fail;
}

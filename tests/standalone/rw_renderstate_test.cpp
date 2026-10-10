// P2B-02c unit test: RwRenderStateSet/Get (source/standalone/rw/renderstate.cpp) and the RwD3D9 fixed-function state API (rwd3d_ff.cpp).
// Part 1 (always runs, no device): shadow / librw-cache paths. Part 2 (needs a D3D9 HAL adapter - works under Wine/wined3d): the same API against a
// real device created by librw (hidden window), states read back from IDirect3DDevice9. If no device can be created part 2 is reported SKIPPED.
// Exit code 0 = all checks passed.
#include "fakerw.h"
#include <src/d3d/rwd3dimpl.h>

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>

RwRGBAReal AmbientSaturated{}; // normally defined in rwglobals.cpp (kept out of this test's link line)

void NotsaRwRenderState_OnEngineStarted(); // renderstate.cpp: RW's default state, applied by RwEngineStart via platform.cpp

namespace notsa_rw02c {
void ResetRenderStateShadow();
void ResetFixedFunctionShadow();
const D3DMATERIAL9& SurfacePropsScratchMaterial();
bool LightEnabledShadow(int i);
} // namespace notsa_rw02c

static int g_fail = 0, g_pass = 0;
#define CHECK(c) do { if (c) { ++g_pass; } else { ++g_fail; std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)
static void Section(const char* n) { std::printf("[%s]\n", n); }

static RwUInt32 Get(RwRenderState s) { RwUInt32 v = 0xDEADBEEF; RwRenderStateGet(s, &v); return v; }
static RwBool   Set(RwRenderState s, RwUInt32 v) { return RwRenderStateSet(s, reinterpret_cast<void*>(static_cast<std::uintptr_t>(v))); }
static bool     Near(float a, float b) { return std::fabs(a - b) < 1e-5f; }

// every state the game uses (grep rwRENDERSTATE source/game_sa) with two distinct valid values each
struct StateCase { RwRenderState s; RwUInt32 a, b; const char* name; };
static const StateCase kCases[] = {
    {rwRENDERSTATETEXTUREADDRESS,      rwTEXTUREADDRESSCLAMP, rwTEXTUREADDRESSWRAP, "TEXTUREADDRESS"},
    {rwRENDERSTATETEXTUREADDRESSU,     rwTEXTUREADDRESSMIRROR, rwTEXTUREADDRESSBORDER, "TEXTUREADDRESSU"},
    {rwRENDERSTATETEXTUREADDRESSV,     rwTEXTUREADDRESSBORDER, rwTEXTUREADDRESSMIRROR, "TEXTUREADDRESSV"},
    {rwRENDERSTATEZTESTENABLE,         0, 1, "ZTESTENABLE"},
    {rwRENDERSTATESHADEMODE,           rwSHADEMODEFLAT, rwSHADEMODEGOURAUD, "SHADEMODE"},
    {rwRENDERSTATEZWRITEENABLE,        0, 1, "ZWRITEENABLE"},
    {rwRENDERSTATETEXTUREFILTER,       rwFILTERLINEAR, rwFILTERMIPNEAREST, "TEXTUREFILTER"},
    {rwRENDERSTATESRCBLEND,            rwBLENDONE, rwBLENDSRCALPHASAT, "SRCBLEND"},
    {rwRENDERSTATEDESTBLEND,           rwBLENDINVSRCCOLOR, rwBLENDDESTALPHA, "DESTBLEND"},
    {rwRENDERSTATEVERTEXALPHAENABLE,   1, 0, "VERTEXALPHAENABLE"},
    {rwRENDERSTATEBORDERCOLOR,         0xFF102030u, 0x00000000u, "BORDERCOLOR"},
    {rwRENDERSTATEFOGENABLE,           1, 0, "FOGENABLE"},
    {rwRENDERSTATEFOGCOLOR,            0x80112233u, 0xFFAABBCCu, "FOGCOLOR"},
    {rwRENDERSTATEFOGTYPE,             rwFOGTYPEEXPONENTIAL2, rwFOGTYPELINEAR, "FOGTYPE"},
    {rwRENDERSTATEFOGDENSITY,          0x3F000000u, 0x3F800000u, "FOGDENSITY"},
    {rwRENDERSTATECULLMODE,            rwCULLMODECULLBACK, rwCULLMODECULLFRONT, "CULLMODE"},
    {rwRENDERSTATESTENCILENABLE,       1, 0, "STENCILENABLE"},
    {rwRENDERSTATESTENCILFAIL,         rwSTENCILOPERATIONZERO, rwSTENCILOPERATIONINVERT, "STENCILFAIL"},
    {rwRENDERSTATESTENCILZFAIL,        rwSTENCILOPERATIONINCR, rwSTENCILOPERATIONDECRSAT, "STENCILZFAIL"},
    {rwRENDERSTATESTENCILPASS,         rwSTENCILOPERATIONREPLACE, rwSTENCILOPERATIONDECR, "STENCILPASS"},
    {rwRENDERSTATESTENCILFUNCTION,     rwSTENCILFUNCTIONEQUAL, rwSTENCILFUNCTIONNOTEQUAL, "STENCILFUNCTION"},
    {rwRENDERSTATESTENCILFUNCTIONREF,  0x55, 0x01, "STENCILFUNCTIONREF"},
    {rwRENDERSTATESTENCILFUNCTIONMASK, 0x0F, 0xFF, "STENCILFUNCTIONMASK"},
    {rwRENDERSTATESTENCILFUNCTIONWRITEMASK, 0xF0, 0xFF, "STENCILFUNCTIONWRITEMASK"},
    {rwRENDERSTATEALPHATESTFUNCTION,   rwALPHATESTFUNCTIONGREATER, rwALPHATESTFUNCTIONALWAYS, "ALPHATESTFUNCTION"},
    {rwRENDERSTATEALPHATESTFUNCTIONREF, 100, 2, "ALPHATESTFUNCTIONREF"},
};

static void TestRoundTrips(const char* what) {
    Section(what);
    for (const auto& c : kCases) {
        CHECK(Set(c.s, c.a) == TRUE);
        const RwUInt32 ga = Get(c.s);
        if (ga != c.a) std::printf("  %s: set 0x%x got 0x%x\n", c.name, c.a, ga);
        CHECK(ga == c.a);
        CHECK(Set(c.s, c.b) == TRUE);
        const RwUInt32 gb = Get(c.s);
        if (gb != c.b) std::printf("  %s: set 0x%x got 0x%x\n", c.name, c.b, gb);
        CHECK(gb == c.b);
    }
    // TEXTUREADDRESS sets both axes; U/V are independent afterwards
    Set(rwRENDERSTATETEXTUREADDRESS, rwTEXTUREADDRESSCLAMP);
    CHECK(Get(rwRENDERSTATETEXTUREADDRESSU) == rwTEXTUREADDRESSCLAMP && Get(rwRENDERSTATETEXTUREADDRESSV) == rwTEXTUREADDRESSCLAMP);
    Set(rwRENDERSTATETEXTUREADDRESSU, rwTEXTUREADDRESSWRAP);
    CHECK(Get(rwRENDERSTATETEXTUREADDRESSU) == rwTEXTUREADDRESSWRAP && Get(rwRENDERSTATETEXTUREADDRESSV) == rwTEXTUREADDRESSCLAMP);
    CHECK(Get(rwRENDERSTATETEXTUREADDRESS) == 0); // U != V -> no single mode (rwTEXTUREADDRESSNATEXTUREADDRESS)
    { // exe 0x7FD868: ... and the call itself reports FALSE (value 0 written)
        RwUInt32 a = 0xDEAD; CHECK(RwRenderStateGet(rwRENDERSTATETEXTUREADDRESS, &a) == FALSE && a == 0);
        Set(rwRENDERSTATETEXTUREADDRESSV, rwTEXTUREADDRESSWRAP);
        a = 0xDEAD; CHECK(RwRenderStateGet(rwRENDERSTATETEXTUREADDRESS, &a) == TRUE && a == rwTEXTUREADDRESSWRAP);
    }
    // TEXTUREPERSPECTIVE (exe 0x7FE8CF / 0x7FD810): Set changes nothing and returns (value != 0); Get is unsupported (FALSE, nothing written)
    CHECK(Set(rwRENDERSTATETEXTUREPERSPECTIVE, 0) == FALSE && Set(rwRENDERSTATETEXTUREPERSPECTIVE, 1) == TRUE);
    { RwUInt32 a = 0xDEAD; CHECK(RwRenderStateGet(rwRENDERSTATETEXTUREPERSPECTIVE, &a) == FALSE && a == 0xDEAD); }
    // a state set to 0 (RWRSTATE(rwRENDERSTATENARENDERSTATE), seen in the game) reads back false
    Set(rwRENDERSTATEZWRITEENABLE, rwRENDERSTATENARENDERSTATE);
    CHECK(Get(rwRENDERSTATEZWRITEENABLE) == 0);
    // invalid arguments are rejected and do not disturb the state
    Set(rwRENDERSTATESRCBLEND, rwBLENDONE);
    CHECK(Set(rwRENDERSTATESRCBLEND, 99) == FALSE && Get(rwRENDERSTATESRCBLEND) == rwBLENDONE);
    Set(rwRENDERSTATECULLMODE, rwCULLMODECULLBACK);
    CHECK(Set(rwRENDERSTATECULLMODE, 7) == FALSE && Get(rwRENDERSTATECULLMODE) == rwCULLMODECULLBACK);
    CHECK(Set(rwRENDERSTATENARENDERSTATE, 1) == FALSE && Set(static_cast<RwRenderState>(18), 1) == FALSE && Set(static_cast<RwRenderState>(19), 1) == FALSE && Set(static_cast<RwRenderState>(40), 1) == FALSE);
    RwUInt32 v;
    CHECK(RwRenderStateGet(rwRENDERSTATENARENDERSTATE, &v) == FALSE && RwRenderStateGet(rwRENDERSTATEZTESTENABLE, nullptr) == FALSE);
}

static void TestD3D9Caches() {
    Section("RwD3D9 render/texture-stage state caches");
    RwD3D9SetRenderState(D3DRS_TEXTUREFACTOR, 0xFF000000);
    RwUInt32 v = 0;
    RwD3D9GetRenderState(D3DRS_TEXTUREFACTOR, &v);
    CHECK(v == 0xFF000000);
    RwD3D9SetRenderState(D3DRS_FOGSTART, 0x3F800000);
    RwD3D9SetRenderState(D3DRS_FOGEND, 0x42C80000);
    RwD3D9GetRenderState(D3DRS_FOGSTART, &v); CHECK(v == 0x3F800000);
    RwD3D9GetRenderState(D3DRS_FOGEND, &v);   CHECK(v == 0x42C80000);
    RwD3D9SetRenderState(D3DRS_AMBIENT, 0x12345678); RwD3D9GetRenderState(D3DRS_AMBIENT, &v); CHECK(v == 0x12345678);
    RwD3D9SetRenderState(D3DRS_SPECULARENABLE, TRUE); RwD3D9GetRenderState(D3DRS_SPECULARENABLE, &v); CHECK(v == 1);
    RwD3D9SetRenderState(D3DRS_SPECULARENABLE, FALSE); RwD3D9GetRenderState(D3DRS_SPECULARENABLE, &v); CHECK(v == 0);
    v = 0x77; RwD3D9SetRenderState(9999, 1); RwD3D9GetRenderState(9999, &v); CHECK(v == 0x77); // out of range: ignored, *value untouched

    struct Tss { UINT stage, type, val; } tss[] = {
        {0, D3DTSS_COLOROP, D3DTOP_SELECTARG2}, {0, D3DTSS_COLORARG2, D3DTA_TFACTOR}, {1, D3DTSS_COLOROP, D3DTOP_MULTIPLYADD},
        {1, D3DTSS_COLORARG0, D3DTA_CURRENT}, {1, D3DTSS_TEXCOORDINDEX, 1 | D3DTSS_TCI_CAMERASPACENORMAL},
        {1, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_PROJECTED | D3DTTFF_COUNT3}, {2, D3DTSS_COLOROP, D3DTOP_DISABLE}, {1, D3DTSS_ALPHAOP, D3DTOP_BLENDFACTORALPHA},
    };
    for (const auto& t : tss) {
        RwD3D9SetTextureStageState(t.stage, t.type, t.val);
        v = 0xFFFFFFFF;
        RwD3D9GetTextureStageState(t.stage, t.type, &v);
        CHECK(v == t.val);
    }
    // stages are independent
    RwD3D9SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
    RwD3D9GetTextureStageState(0, D3DTSS_COLOROP, &v); CHECK(v == D3DTOP_SELECTARG2);
    RwD3D9GetTextureStageState(1, D3DTSS_COLOROP, &v); CHECK(v == D3DTOP_DISABLE);
    v = 0x77; RwD3D9SetTextureStageState(9, D3DTSS_COLOROP, 1); RwD3D9GetTextureStageState(9, D3DTSS_COLOROP, &v); CHECK(v == 0x77);
}

static void TestShadowFF() {
    Section("transforms / lights (shadow, no device)");
    D3DMATRIX m{}; m._11 = 2; m._22 = 3; m._33 = 4; m._44 = 1; m._41 = 5; m._42 = 6; m._43 = 7;
    RwD3D9SetTransform(D3DTS_VIEW, &m);
    D3DMATRIX g{};
    RwD3D9GetTransform(D3DTS_VIEW, &g);
    CHECK(std::memcmp(&g, &m, sizeof(m)) == 0);
    RwD3D9GetTransform(D3DTS_PROJECTION, &g); // never set: identity
    CHECK(g._11 == 1 && g._22 == 1 && g._33 == 1 && g._44 == 1 && g._12 == 0 && g._41 == 0);
    RwD3D9SetTransform(D3DTS_TEXTURE1, &m);
    RwD3D9GetTransform(D3DTS_TEXTURE1, &g); CHECK(std::memcmp(&g, &m, sizeof(m)) == 0);
    RwD3D9SetTransform(D3DTS_TEXTURE1, nullptr); // NULL = identity
    RwD3D9GetTransform(D3DTS_TEXTURE1, &g); CHECK(g._11 == 1 && g._41 == 0);
    CHECK(RwD3D9SetTransform(100000, &m) == FALSE);

    D3DLIGHT9 l{}; l.Type = D3DLIGHT_DIRECTIONAL; l.Specular = {1, 1, 1, 1}; l.Direction = {0, 0, -1};
    CHECK(RwD3D9SetLight(1, &l) == FALSE); // no device: stored, but the call reports failure
    D3DLIGHT9 gl{}; RwD3D9GetLight(1, &gl);
    CHECK(gl.Type == D3DLIGHT_DIRECTIONAL && gl.Direction.z == -1.f && gl.Specular.r == 1.f);
    RwD3D9EnableLight(1, TRUE);  CHECK(notsa_rw02c::LightEnabledShadow(1));
    RwD3D9EnableLight(1, FALSE); CHECK(!notsa_rw02c::LightEnabledShadow(1));
    CHECK(RwD3D9SetLight(-1, &l) == FALSE && RwD3D9SetLight(1000, &l) == FALSE && RwD3D9SetLight(1, nullptr) == FALSE);

    // device-less API calls must be safe no-ops
    RwD3D9SetVertexShader(nullptr); RwD3D9SetPixelShader(nullptr); RwD3D9SetIndices(nullptr); RwD3D9SetVertexDeclaration(nullptr);
    RwD3D9SetStreamSource(0, nullptr, 0, 0); RwD3D9SetFVF(D3DFVF_XYZ); RwD3D9DrawPrimitive(D3DPT_TRIANGLELIST, 0, 0); RwD3D9DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, 0, 0, 0);
    _rwD3D9SetStreams(nullptr, FALSE);
    CHECK(RwD3D9SetTexture(nullptr, 0) == FALSE);
}

//--------------------------------------------------------------------------------------------------
// Part 2: a real device
//--------------------------------------------------------------------------------------------------
static void* TMalloc(size_t s, unsigned) { return std::malloc(s); }
static void* TRealloc(void* p, size_t s, unsigned) { return std::realloc(p, s); }
static void  TFree(void* p) { std::free(p); }

static DWORD DevRS(D3DRENDERSTATETYPE t) { DWORD v = 0xDEAD; rw::d3d::d3ddevice->GetRenderState(t, &v); return v; }
static DWORD DevTSS(DWORD st, D3DTEXTURESTAGESTATETYPE t) { DWORD v = 0xDEAD; rw::d3d::d3ddevice->GetTextureStageState(st, t, &v); return v; }

static void TestDevice() {
    Section("device");
    HWND wnd = CreateWindowA("STATIC", "rw_renderstate_test", WS_OVERLAPPEDWINDOW, 0, 0, 640, 480, nullptr, nullptr, GetModuleHandleA(nullptr), nullptr);
    std::printf("  window %p\n", (void*)wnd);
    rw::MemoryFunctions mf{}; mf.rwmalloc = TMalloc; mf.rwrealloc = TRealloc; mf.rwfree = TFree;
    if (!wnd || !rw::Engine::init(&mf)) { std::printf("SKIPPED part 2: engine init failed\n"); return; }
    rw::EngineOpenParams p{}; p.window = wnd;
    std::printf("  engine init ok\n"); rw::Engine::open(&p);
    std::printf("  engine opened\n"); rw::Engine::start();
    std::printf("  engine started, device %p\n", (void*)rw::d3d::d3ddevice);
    if (!rw::d3d::d3ddevice) {
        std::printf("SKIPPED part 2: no D3D9 device could be created here\n");
        rw::Engine::state = rw::Engine::Opened;
        return;
    }
    notsa_rw02c::ResetRenderStateShadow();
    notsa_rw02c::ResetFixedFunctionShadow();
    IDirect3DDevice9* dev = rw::d3d::d3ddevice;
    auto flush = [] { rw::d3d::flushCache(); };

    // default read-back before any Set comes from librw's loaded device state
    RwUInt32 v = 0;
    RwD3D9GetRenderState(D3DRS_LIGHTING, &v);
    CHECK(v == 1); // D3D default (what CustomBuilding*/CarEnvMap query) - until RW's defaults are applied below

    // RwEngineStart -> NotsaRwPlatform_OnEngineStarted -> this: RW's render-state defaults (exe 0x7FCAC0..0x7FD0F6) on librw's caches + the device
    NotsaRwRenderState_OnEngineStarted();
    flush();
    CHECK(DevRS(D3DRS_ZENABLE) != D3DZB_FALSE && DevRS(D3DRS_ZWRITEENABLE) == TRUE && DevRS(D3DRS_ZFUNC) == D3DCMP_LESSEQUAL);
    CHECK(DevRS(D3DRS_CULLMODE) == D3DCULL_CW);                      // RW default cull BACK
    CHECK(DevRS(D3DRS_ALPHAFUNC) == D3DCMP_GREATER && DevRS(D3DRS_ALPHAREF) == 0 && DevRS(D3DRS_ALPHABLENDENABLE) == FALSE && DevRS(D3DRS_ALPHATESTENABLE) == FALSE);
    CHECK(DevRS(D3DRS_SRCBLEND) == D3DBLEND_SRCALPHA && DevRS(D3DRS_DESTBLEND) == D3DBLEND_INVSRCALPHA && DevRS(D3DRS_SHADEMODE) == D3DSHADE_GOURAUD);
    CHECK(DevRS(D3DRS_LIGHTING) == FALSE && DevRS(D3DRS_AMBIENT) == 0xFFFFFFFFu && DevRS(D3DRS_LOCALVIEWER) == FALSE && DevRS(D3DRS_SPECULARENABLE) == FALSE);
    CHECK(DevRS(D3DRS_DIFFUSEMATERIALSOURCE) == D3DMCS_MATERIAL && DevRS(D3DRS_AMBIENTMATERIALSOURCE) == D3DMCS_MATERIAL && DevRS(D3DRS_SPECULARMATERIALSOURCE) == D3DMCS_MATERIAL);
    CHECK(DevRS(D3DRS_FOGENABLE) == FALSE && DevRS(D3DRS_FOGDENSITY) == 0x3F800000u);
    {   // fog type (exe 0x7FE515): table fog only when the device has table + W fog, else vertex fog
        const DWORD caps = rw::d3d::d3d9Globals.caps.RasterCaps;
        const bool table = (caps & D3DPRASTERCAPS_FOGTABLE) && (caps & 0x100000);
        CHECK(DevRS(D3DRS_FOGTABLEMODE) == (table ? D3DFOG_LINEAR : D3DFOG_NONE) && DevRS(D3DRS_FOGVERTEXMODE) == (table ? D3DFOG_NONE : D3DFOG_LINEAR));
    }
    for (DWORD st = 0; st < 8; st++) {
        DWORD mag = 0, min = 0, mip = 0, au = 0, av = 0, bc = 0, an = 0;
        dev->GetSamplerState(st, D3DSAMP_MAGFILTER, &mag); dev->GetSamplerState(st, D3DSAMP_MINFILTER, &min); dev->GetSamplerState(st, D3DSAMP_MIPFILTER, &mip);
        dev->GetSamplerState(st, D3DSAMP_ADDRESSU, &au); dev->GetSamplerState(st, D3DSAMP_ADDRESSV, &av);
        dev->GetSamplerState(st, D3DSAMP_BORDERCOLOR, &bc); dev->GetSamplerState(st, D3DSAMP_MAXANISOTROPY, &an);
        CHECK(mag == D3DTEXF_LINEAR && min == D3DTEXF_LINEAR && mip == D3DTEXF_NONE && au == D3DTADDRESS_WRAP && av == D3DTADDRESS_WRAP && bc == 0xFF000000u && an == 1);
    }
    CHECK(DevTSS(0, D3DTSS_COLOROP) == D3DTOP_SELECTARG2 && DevTSS(0, D3DTSS_COLORARG2) == D3DTA_DIFFUSE && DevTSS(0, D3DTSS_ALPHAOP) == D3DTOP_SELECTARG2);
    for (DWORD st = 1; st < 8; st++) CHECK(DevTSS(st, D3DTSS_COLOROP) == D3DTOP_DISABLE && DevTSS(st, D3DTSS_ALPHAOP) == D3DTOP_DISABLE);
    CHECK(Get(rwRENDERSTATEZTESTENABLE) == 1 && Get(rwRENDERSTATEZWRITEENABLE) == 1 && Get(rwRENDERSTATECULLMODE) == rwCULLMODECULLBACK &&
          Get(rwRENDERSTATETEXTUREFILTER) == rwFILTERLINEAR && Get(rwRENDERSTATEALPHATESTFUNCTION) == rwALPHATESTFUNCTIONGREATER && Get(rwRENDERSTATEALPHATESTFUNCTIONREF) == 0 &&
          Get(rwRENDERSTATEBORDERCOLOR) == 0xFF000000u && Get(rwRENDERSTATEFOGDENSITY) == 0x3F800000u);
    // the first ZTEST=0 after start used to be dropped (librw's cache started at 0 while the device had ZENABLE TRUE)
    Set(rwRENDERSTATEZWRITEENABLE, 0); Set(rwRENDERSTATEZTESTENABLE, 0); flush();
    CHECK(DevRS(D3DRS_ZENABLE) == D3DZB_FALSE);
    Set(rwRENDERSTATEZWRITEENABLE, 1); flush();                       // write without test: ZENABLE on, func ALWAYS
    CHECK(DevRS(D3DRS_ZENABLE) != D3DZB_FALSE && DevRS(D3DRS_ZFUNC) == D3DCMP_ALWAYS && DevRS(D3DRS_ZWRITEENABLE) == TRUE);
    Set(rwRENDERSTATEZTESTENABLE, 1); flush();
    CHECK(DevRS(D3DRS_ZFUNC) == D3DCMP_LESSEQUAL);
    // alpha test tie (exe 0x7FE0A0 / 0x7FE96D): vertex alpha switches blending and the alpha test on together; while blending, compare ALWAYS turns the test off
    Set(rwRENDERSTATEVERTEXALPHAENABLE, 1); flush();
    CHECK(DevRS(D3DRS_ALPHABLENDENABLE) == TRUE && DevRS(D3DRS_ALPHATESTENABLE) == TRUE);
    Set(rwRENDERSTATEALPHATESTFUNCTION, rwALPHATESTFUNCTIONALWAYS); flush();
    CHECK(DevRS(D3DRS_ALPHATESTENABLE) == FALSE && DevRS(D3DRS_ALPHAFUNC) == D3DCMP_ALWAYS && DevRS(D3DRS_ALPHABLENDENABLE) == TRUE);
    Set(rwRENDERSTATEALPHATESTFUNCTION, rwALPHATESTFUNCTIONGREATER); flush();
    CHECK(DevRS(D3DRS_ALPHATESTENABLE) == TRUE);
    Set(rwRENDERSTATEVERTEXALPHAENABLE, 0); flush();
    CHECK(DevRS(D3DRS_ALPHABLENDENABLE) == FALSE && DevRS(D3DRS_ALPHATESTENABLE) == FALSE);
    CHECK(Get(rwRENDERSTATEVERTEXALPHAENABLE) == 0);

    // RW states -> D3D device states
    Set(rwRENDERSTATESRCBLEND, rwBLENDONE);           Set(rwRENDERSTATEDESTBLEND, rwBLENDINVSRCCOLOR);
    Set(rwRENDERSTATEZTESTENABLE, 1);                 Set(rwRENDERSTATEZWRITEENABLE, 1); Set(rwRENDERSTATEZWRITEENABLE, 0); // librw's cache starts with zwrite=0 while the device default is TRUE: only a change reaches the device
    Set(rwRENDERSTATECULLMODE, rwCULLMODECULLFRONT);  Set(rwRENDERSTATEVERTEXALPHAENABLE, 1);
    Set(rwRENDERSTATESTENCILENABLE, 1);               Set(rwRENDERSTATESTENCILFAIL, rwSTENCILOPERATIONZERO);
    Set(rwRENDERSTATESTENCILZFAIL, rwSTENCILOPERATIONINCR); Set(rwRENDERSTATESTENCILPASS, rwSTENCILOPERATIONREPLACE);
    Set(rwRENDERSTATESTENCILFUNCTION, rwSTENCILFUNCTIONNOTEQUAL); Set(rwRENDERSTATESTENCILFUNCTIONREF, 0x7F);
    Set(rwRENDERSTATESTENCILFUNCTIONMASK, 0x3F);      Set(rwRENDERSTATESTENCILFUNCTIONWRITEMASK, 0xF0);
    Set(rwRENDERSTATEALPHATESTFUNCTION, rwALPHATESTFUNCTIONGREATER); Set(rwRENDERSTATEALPHATESTFUNCTIONREF, 100);
    Set(rwRENDERSTATEFOGENABLE, 1);                   Set(rwRENDERSTATEFOGCOLOR, 0x80112233u);
    Set(rwRENDERSTATEFOGTYPE, rwFOGTYPEEXPONENTIAL2); Set(rwRENDERSTATEFOGDENSITY, 0x3F000000u);
    Set(rwRENDERSTATESHADEMODE, rwSHADEMODEFLAT);     Set(rwRENDERSTATEBORDERCOLOR, 0xFF102030u);
    flush();
    CHECK(DevRS(D3DRS_SRCBLEND) == D3DBLEND_ONE && DevRS(D3DRS_DESTBLEND) == D3DBLEND_INVSRCCOLOR);
    CHECK(DevRS(D3DRS_ZWRITEENABLE) == FALSE && DevRS(D3DRS_ZENABLE) != D3DZB_FALSE);
    CHECK(DevRS(D3DRS_CULLMODE) == D3DCULL_CCW);   // RW cull FRONT == D3DCULL_CCW (librw cullmodeMap)
    CHECK(DevRS(D3DRS_STENCILENABLE) == TRUE && DevRS(D3DRS_STENCILFAIL) == D3DSTENCILOP_ZERO && DevRS(D3DRS_STENCILZFAIL) == D3DSTENCILOP_INCR);
    CHECK(DevRS(D3DRS_STENCILPASS) == D3DSTENCILOP_REPLACE && DevRS(D3DRS_STENCILFUNC) == D3DCMP_NOTEQUAL && DevRS(D3DRS_STENCILREF) == 0x7F);
    CHECK(DevRS(D3DRS_STENCILMASK) == 0x3F && DevRS(D3DRS_STENCILWRITEMASK) == 0xF0);
    CHECK(DevRS(D3DRS_ALPHAFUNC) == D3DCMP_GREATER && DevRS(D3DRS_ALPHAREF) == 100);
    CHECK(DevRS(D3DRS_FOGENABLE) == TRUE && DevRS(D3DRS_FOGCOLOR) == 0x80112233u); // RW 0xAARRGGBB == D3DCOLOR
    {
        const DWORD caps = rw::d3d::d3d9Globals.caps.RasterCaps;
        const bool table = (caps & D3DPRASTERCAPS_FOGTABLE) && (caps & 0x100000);
        CHECK(DevRS(table ? D3DRS_FOGTABLEMODE : D3DRS_FOGVERTEXMODE) == D3DFOG_EXP2 && DevRS(table ? D3DRS_FOGVERTEXMODE : D3DRS_FOGTABLEMODE) == D3DFOG_NONE);
    }
    CHECK(DevRS(D3DRS_FOGDENSITY) == 0x3F000000u);
    CHECK(DevRS(D3DRS_SHADEMODE) == D3DSHADE_FLAT);
    { DWORD b = 0; dev->GetSamplerState(0, D3DSAMP_BORDERCOLOR, &b); CHECK(b == 0xFF102030u); }
    // librw's own view of the same states (what its draws will use) agrees with ours
    CHECK(rw::GetRenderState(rw::SRCBLEND) == rw::BLENDONE && rw::GetRenderState(rw::CULLMODE) == rw::CULLFRONT && rw::GetRenderState(rw::STENCILFUNCTION) == rw::STENCILNOTEQUAL);
    CHECK(rw::GetRenderState(rw::FOGCOLOR) == 0x80332211u); // librw packs ABGR
    // Get comes from librw (authoritative): a change made behind our back through librw is visible
    rw::SetRenderState(rw::SRCBLEND, rw::BLENDZERO);
    CHECK(Get(rwRENDERSTATESRCBLEND) == rwBLENDZERO);
    // mutation: different values reach the device
    Set(rwRENDERSTATESRCBLEND, rwBLENDDESTCOLOR); Set(rwRENDERSTATECULLMODE, rwCULLMODECULLBACK); Set(rwRENDERSTATEZWRITEENABLE, 1);
    Set(rwRENDERSTATEALPHATESTFUNCTION, rwALPHATESTFUNCTIONLESSEQUAL); Set(rwRENDERSTATEFOGCOLOR, 0xFFAABBCCu); Set(rwRENDERSTATEFOGENABLE, 0);
    flush();
    CHECK(DevRS(D3DRS_SRCBLEND) == D3DBLEND_DESTCOLOR && DevRS(D3DRS_CULLMODE) == D3DCULL_CW && DevRS(D3DRS_ZWRITEENABLE) == TRUE);
    CHECK(DevRS(D3DRS_ALPHAFUNC) == D3DCMP_LESSEQUAL && DevRS(D3DRS_FOGCOLOR) == 0xFFAABBCCu && DevRS(D3DRS_FOGENABLE) == FALSE);
    TestRoundTrips("RwRenderState round trips (librw-backed Get)");

    // RwD3D9 render / texture-stage states reach the device at the next flush
    RwD3D9SetRenderState(D3DRS_TEXTUREFACTOR, 0xFF336699); RwD3D9SetRenderState(D3DRS_SPECULARENABLE, TRUE); RwD3D9SetRenderState(D3DRS_FOGSTART, 0x3F800000);
    RwD3D9SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG2); RwD3D9SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_MULTIPLYADD);
    RwD3D9SetTextureStageState(1, D3DTSS_TEXCOORDINDEX, 1 | D3DTSS_TCI_CAMERASPACENORMAL);
    RwD3D9SetTextureStageState(1, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_PROJECTED | D3DTTFF_COUNT3);
    flush();
    CHECK(DevRS(D3DRS_TEXTUREFACTOR) == 0xFF336699 && DevRS(D3DRS_SPECULARENABLE) == TRUE && DevRS(D3DRS_FOGSTART) == 0x3F800000);
    CHECK(DevTSS(0, D3DTSS_COLOROP) == D3DTOP_SELECTARG2 && DevTSS(1, D3DTSS_COLOROP) == D3DTOP_MULTIPLYADD);
    CHECK(DevTSS(1, D3DTSS_TEXCOORDINDEX) == (1u | D3DTSS_TCI_CAMERASPACENORMAL) && DevTSS(1, D3DTSS_TEXTURETRANSFORMFLAGS) == (D3DTTFF_PROJECTED | D3DTTFF_COUNT3));
    RwD3D9SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE); flush();
    CHECK(DevTSS(1, D3DTSS_COLOROP) == D3DTOP_DISABLE && DevTSS(0, D3DTSS_COLOROP) == D3DTOP_SELECTARG2);

    // textures: RwRenderState TEXTURERASTER and RwD3D9SetTexture
    rw::Raster* ras = rw::Raster::create(8, 8, 32, rw::Raster::TEXTURE | rw::Raster::C8888);
    CHECK(ras != nullptr);
    if (ras) {
        CHECK(RwRenderStateSet(rwRENDERSTATETEXTURERASTER, ras) == TRUE);
        RwRaster* got = nullptr; RwRenderStateGet(rwRENDERSTATETEXTURERASTER, &got);
        CHECK(got == ras);
        IDirect3DBaseTexture9* t0 = nullptr; dev->GetTexture(0, &t0);
        CHECK(t0 != nullptr); if (t0) t0->Release();
        RwRenderStateSet(rwRENDERSTATETEXTURERASTER, nullptr);
        got = ras; RwRenderStateGet(rwRENDERSTATETEXTURERASTER, &got);
        CHECK(got == nullptr);
        rw::Texture* tex = rw::Texture::create(ras);
        if (tex) {
            tex->setFilter(rw::Texture::LINEAR); tex->setAddressU(rw::Texture::CLAMP); tex->setAddressV(rw::Texture::MIRROR);
            CHECK(RwD3D9SetTexture(tex, 1) == TRUE);
            IDirect3DBaseTexture9* t1 = nullptr; dev->GetTexture(1, &t1);
            CHECK(t1 != nullptr); if (t1) t1->Release();
            DWORD au = 0, av = 0; dev->GetSamplerState(1, D3DSAMP_ADDRESSU, &au); dev->GetSamplerState(1, D3DSAMP_ADDRESSV, &av);
            CHECK(au == D3DTADDRESS_CLAMP && av == D3DTADDRESS_MIRROR);
            {   // fix4: the stage >= 1 sampler filter reaches the device through librw's per-stage cache, also after the engine defaults were applied again
                const auto filt = [&](DWORD stg, D3DSAMPLERSTATETYPE t) { DWORD v = 0xDEAD; dev->GetSamplerState(stg, t, &v); return v; };
                tex->setFilter(rw::Texture::NEAREST);
                CHECK(RwD3D9SetTexture(tex, 1) == TRUE);
                CHECK(filt(1, D3DSAMP_MINFILTER) == D3DTEXF_POINT && filt(1, D3DSAMP_MAGFILTER) == D3DTEXF_POINT && filt(1, D3DSAMP_MIPFILTER) == D3DTEXF_NONE);
                NotsaRwRenderState_OnEngineStarted();   // device back to the exe's linear defaults; the cache must follow (it used to keep NEAREST)
                CHECK(filt(1, D3DSAMP_MINFILTER) == D3DTEXF_LINEAR && filt(1, D3DSAMP_MAGFILTER) == D3DTEXF_LINEAR);
                int32_t cf = -1, ca = -1, cu = -1, cv = -1;
                rw::d3d::getTextureStageSamplers(1, &cf, &ca, &cu, &cv);
                CHECK(cf == rw::Texture::LINEAR && ca == 1 && cu == rw::Texture::WRAP && cv == rw::Texture::WRAP);
                CHECK(RwD3D9SetTexture(tex, 1) == TRUE);   // same NEAREST texture again: must be POINT, not swallowed by a stale NEAREST cache
                CHECK(filt(1, D3DSAMP_MINFILTER) == D3DTEXF_POINT && filt(1, D3DSAMP_MAGFILTER) == D3DTEXF_POINT);
                CHECK(filt(1, D3DSAMP_ADDRESSU) == D3DTADDRESS_CLAMP && filt(1, D3DSAMP_ADDRESSV) == D3DTADDRESS_MIRROR);   // the texture's own addressing re-applied
            }
            CHECK(RwD3D9SetTexture(nullptr, 1) == TRUE);
            tex->destroy();
        } else {
            ras->destroy();
        }
        // (the raster is owned by the texture and destroyed with it)
    }

    // transforms / lights / material on the device
    D3DMATRIX m{}; m._11 = 2; m._22 = 3; m._33 = 4; m._44 = 1; m._41 = 5; m._42 = 6; m._43 = 7;
    CHECK(RwD3D9SetTransform(D3DTS_VIEW, &m) == TRUE);
    D3DMATRIX dm{}; dev->GetTransform(D3DTS_VIEW, &dm); CHECK(std::memcmp(&dm, &m, sizeof(m)) == 0);
    CHECK(RwD3D9SetTransform(D3DTS_TEXTURE1, &m) == TRUE);
    dev->GetTransform(D3DTS_TEXTURE1, &dm); CHECK(std::memcmp(&dm, &m, sizeof(m)) == 0);
    RwD3D9GetTransform(D3DTS_TEXTURE1, &dm); CHECK(std::memcmp(&dm, &m, sizeof(m)) == 0);

    D3DLIGHT9 l{}; l.Type = D3DLIGHT_DIRECTIONAL; l.Diffuse = {0, 0, 0, 0}; l.Specular = {1, 0.5f, 0.25f, 1}; l.Direction = {0, 0, -1};
    CHECK(RwD3D9SetLight(1, &l) == TRUE && RwD3D9EnableLight(1, TRUE) == TRUE);
    D3DLIGHT9 dl{}; BOOL en = FALSE;
    CHECK(SUCCEEDED(dev->GetLight(1, &dl)) && dl.Type == D3DLIGHT_DIRECTIONAL && dl.Specular.g == 0.5f && dl.Direction.z == -1.f);
    { const HRESULT hr = dev->GetLightEnable(1, &en); CHECK(SUCCEEDED(hr)); if (en != TRUE) std::printf("  GetLightEnable hr=0x%lx en=%d\n", (unsigned long)hr, (int)en); CHECK(en != FALSE); }
    CHECK(RwD3D9EnableLight(1, FALSE) == TRUE);
    dev->GetLightEnable(1, &en); CHECK(en == FALSE);

    D3DMATERIAL9 mat{}; mat.Diffuse = {0.25f, 0.5f, 0.75f, 1.f}; mat.Specular = {1, 1, 1, 1}; mat.Power = 20.f;
    CHECK(RwD3D9SetMaterial(&mat) == TRUE);
    D3DMATERIAL9 dmat{}; dev->GetMaterial(&dmat);
    CHECK(Near(dmat.Diffuse.g, 0.5f) && Near(dmat.Specular.b, 1.f) && Near(dmat.Power, 20.f));

    // RwD3D9SetSurfaceProperties (exe 0x7FC4D0): the four flag/colour combinations
    AmbientSaturated = {0.5f, 0.25f, 1.0f, 1.0f};
    const RwSurfaceProperties sp{0.5f, 0.f, 0.8f};            // ambient 0.5, specular 0, diffuse 0.8
    RwRGBA col{255, 128, 0, 200};
    // (1) no modulate: diffuse = (d,d,d,1), ambient = AmbientSaturated*ambient, ambient render state white / material source MATERIAL
    notsa_rw02c::ResetFixedFunctionShadow();
    CHECK(RwD3D9SetSurfaceProperties(&sp, &col, rxGEOMETRY_LIGHT) == TRUE);
    flush(); dev->GetMaterial(&dmat);
    CHECK(Near(dmat.Diffuse.r, 0.8f) && Near(dmat.Diffuse.b, 0.8f) && Near(dmat.Diffuse.a, 1.f));
    CHECK(Near(dmat.Ambient.r, 0.25f) && Near(dmat.Ambient.g, 0.125f) && Near(dmat.Ambient.b, 0.5f) && Near(dmat.Emissive.r, 0.f));
    CHECK(DevRS(D3DRS_AMBIENT) == 0xFFFFFFFFu && DevRS(D3DRS_COLORVERTEX) == FALSE && DevRS(D3DRS_AMBIENTMATERIALSOURCE) == D3DMCS_MATERIAL && DevRS(D3DRS_EMISSIVEMATERIALSOURCE) == D3DMCS_MATERIAL);
    // (2) modulate, not prelit: diffuse = colour*d, ambient = colour*AmbientSaturated*ambient, emissive 0
    CHECK(RwD3D9SetSurfaceProperties(&sp, &col, rxGEOMETRY_LIGHT | rxGEOMETRY_MODULATE) == TRUE);
    flush(); dev->GetMaterial(&dmat);
    CHECK(Near(dmat.Diffuse.r, 0.8f) && Near(dmat.Diffuse.g, 128 / 255.f * 0.8f) && Near(dmat.Diffuse.b, 0.f) && Near(dmat.Diffuse.a, 200 / 255.f));
    CHECK(Near(dmat.Ambient.r, 1.f * 0.5f * 0.5f) && Near(dmat.Ambient.g, 128 / 255.f * 0.25f * 0.5f) && Near(dmat.Emissive.g, 0.f));
    CHECK(DevRS(D3DRS_AMBIENT) == 0xFFFFFFFFu && DevRS(D3DRS_COLORVERTEX) == FALSE);
    // (3) modulate + prelit: ambient state = colour ARGB, source COLOR1, lit term goes to the emissive slot
    CHECK(RwD3D9SetSurfaceProperties(&sp, &col, rxGEOMETRY_LIGHT | rxGEOMETRY_MODULATE | rxGEOMETRY_PRELIT) == TRUE);
    flush(); dev->GetMaterial(&dmat);
    CHECK(DevRS(D3DRS_AMBIENT) == 0xC8FF8000u && DevRS(D3DRS_COLORVERTEX) == TRUE && DevRS(D3DRS_AMBIENTMATERIALSOURCE) == D3DMCS_COLOR1 && DevRS(D3DRS_EMISSIVEMATERIALSOURCE) == D3DMCS_MATERIAL);
    CHECK(Near(dmat.Ambient.r, 0.f) && Near(dmat.Emissive.r, 0.25f) && Near(dmat.Emissive.g, 128 / 255.f * 0.125f));
    // (4) white colour with modulate behaves like (1); prelit -> COLORVERTEX on, emissive source COLOR1
    const RwRGBA white{255, 255, 255, 255};
    CHECK(RwD3D9SetSurfaceProperties(&sp, &white, rxGEOMETRY_LIGHT | rxGEOMETRY_MODULATE | rxGEOMETRY_PRELIT) == TRUE);
    flush(); dev->GetMaterial(&dmat);
    CHECK(Near(dmat.Diffuse.r, 0.8f) && Near(dmat.Diffuse.a, 1.f) && DevRS(D3DRS_COLORVERTEX) == TRUE && DevRS(D3DRS_EMISSIVEMATERIALSOURCE) == D3DMCS_COLOR1 && DevRS(D3DRS_AMBIENT) == 0xFFFFFFFFu);
    // early-out cache: same arguments do nothing; a direct SetMaterial invalidates it
    RwD3D9SetRenderState(D3DRS_AMBIENT, 0x11111111); flush();
    CHECK(RwD3D9SetSurfaceProperties(&sp, &white, rxGEOMETRY_LIGHT | rxGEOMETRY_MODULATE | rxGEOMETRY_PRELIT) == TRUE);
    flush(); CHECK(DevRS(D3DRS_AMBIENT) == 0x11111111);          // cached call did not redo the state
    RwD3D9SetMaterial(&mat);
    CHECK(RwD3D9SetSurfaceProperties(&sp, &white, rxGEOMETRY_LIGHT | rxGEOMETRY_MODULATE | rxGEOMETRY_PRELIT) == TRUE);
    flush(); CHECK(DevRS(D3DRS_AMBIENT) == 0xFFFFFFFFu);          // applied again
    dev->GetMaterial(&dmat); CHECK(Near(dmat.Diffuse.r, 0.8f));

    // exe 0x7FC430: SetMaterial of the cached material is a no-op (the device keeps whatever it has); a different one zeroes the early-out flags word,
    // which does NOT invalidate a cached flags==0 SetSurfaceProperties entry: repeating the previous (flags 0) call afterwards keeps the SetMaterial material
    notsa_rw02c::ResetFixedFunctionShadow();
    CHECK(RwD3D9SetSurfaceProperties(&sp, &col, rxGEOMETRY_LIGHT) == TRUE);                 // cache: flags 0
    D3DMATERIAL9 mat2{}; mat2.Diffuse = {0.1f, 0.2f, 0.3f, 1.f}; mat2.Power = 4.f;
    CHECK(RwD3D9SetMaterial(&mat2) == TRUE); flush();
    dev->GetMaterial(&dmat); CHECK(Near(dmat.Diffuse.g, 0.2f));
    CHECK(RwD3D9SetSurfaceProperties(&sp, &col, rxGEOMETRY_LIGHT) == TRUE); flush();      // cache hit: material from SetMaterial stays
    dev->GetMaterial(&dmat); CHECK(Near(dmat.Diffuse.g, 0.2f) && Near(dmat.Power, 4.f));
    dev->SetMaterial(&mat);                                                                   // behind RW's back: identical SetMaterial arg is a cache hit
    CHECK(RwD3D9SetMaterial(&mat2) == TRUE);
    dev->GetMaterial(&dmat); CHECK(Near(dmat.Diffuse.g, 0.5f));
    // lights: EnableLight on an index that was never SetLight'ed is ignored (TRUE), nothing reaches the device
    CHECK(RwD3D9EnableLight(9, TRUE) == TRUE);
    { BOOL e2 = TRUE; const HRESULT hr = dev->GetLightEnable(9, &e2); CHECK(FAILED(hr) || e2 == FALSE); }

    // shaders/streams/draw with nothing bound must not crash; stream 0 can be set and cleared
    RwD3D9SetVertexShader(nullptr); RwD3D9SetPixelShader(nullptr); RwD3D9SetIndices(nullptr); RwD3D9SetVertexDeclaration(nullptr);
    RwD3D9SetStreamSource(0, nullptr, 0, 0);
    RxD3D9VertexStream streams[2]{}; _rwD3D9SetStreams(streams, TRUE);
    RwD3D9SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE);
    DWORD fvf = 0; dev->GetFVF(&fvf); CHECK(fvf == (D3DFVF_XYZ | D3DFVF_DIFFUSE));

    rw::Engine::stop();
    rw::Engine::close();
    rw::Engine::term();
    DestroyWindow(wnd);
}

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    notsa_rw02c::ResetRenderStateShadow();
    TestRoundTrips("RwRenderState round trips (shadow, no device)");
    TestD3D9Caches();
    TestShadowFF();
    TestDevice();
    std::printf("%s: %d checks passed, %d failed\n", g_fail ? "FAILED" : "PASSED", g_pass, g_fail);
    return g_fail ? 1 : 0;
}

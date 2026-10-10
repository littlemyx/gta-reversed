// P2B-23: the stock D3D9 MatFX AllInOne pipeline of RenderWare 3.7 on the RxPipeline façade (pipeline.cpp).
//
// librw's own D3D9 MatFX pipeline is shader based: it never reads the UV transform matrices, has its own lighting and its own env-map maths. The exe renders
// every atomic that carries the MatFX plugin flag (set by the DFF atomic chunk 0x120 or RpMatFXAtomicEnableEffects) through the fixed-function AllInOne
// pipeline created by _rpMatFXPipelinesCreate (0x815E60): the stock AtomicAllInOne node (the façade's, so instancing, reinstancing and lighting are the
// default callbacks) with the render callback 0x815C20 (+ 0x8160A0 as instance callback, which only matters for bump maps). This file ports that render
// callback and the per-effect renderers it calls, and installs the pipeline as matFXGlobals.pipelines[PLATFORM_D3D9] (RwShimMatFXPipelineEnsure, called from
// RwShimPipelineEnsure), so MatFX::enableEffects() (stream read / RpMatFXAtomicEnableEffects) hands atomics OUR pipeline.
//
// Ported from the exe (addresses, all .text): render callback 0x815C20; DefaultRender (NULL effect and UVTRANSFORM, uploads the base UV matrix) 0x812AC0;
// "no light" draw 0x812C90; dual-texture renderer 0x812D40 (DUAL and DUALUVTRANSFORM: single-pass multitexture for the blend modes (DESTCOLOR,ZERO)/(ZERO,SRCCOLOR),
// (SRCALPHA,INVSRCALPHA), (DESTCOLOR,SRCCOLOR), (ONE,ONE), (ZERO,SRCALPHA) when the caps allow it, else a second alpha-blended pass); env-map renderer 0x813A10
// (+ its texture matrix helper 0x814270); pipeline setup 0x815E60 (caps probing, the six ps_1_1 shaders at 0x8852F0..0x885608); RpMatFXAtomicEnableEffects 0x811C00.
// Material effect numbers: BUMPMAP 1, ENVMAP 2, BUMPENVMAP 3, DUAL 4, UVTRANSFORM 5, DUALUVTRANSFORM 6 (jump table at 0x815E18: 5 -> 0x812AC0, 6 -> dual with the
// pass-1 texture).
//
// Deliberate differences / things left out:
//  * BUMPMAP / BUMPENVMAP (0x814460: renders the bump perturbation into scratch rasters with tangent data, one model in the whole game: tikitorch01_lvs.dff)
//    render the plain material (+ the env pass for BUMPENVMAP); the façade instance callback has no tangents.
//  * the palette sharing call 0x4CD250 between two PAL8 rasters is not made (the standalone texture loader expands PAL8, no raster is ever PAL8).
//  * cube-map env textures (0x4C9F70 true: CAMERASPACEREFLECTIONVECTOR path of 0x814270) cannot exist (librw has no cube rasters): sphere map path only.
//  * the exe's own caches of the vertex shader / pixel shader / index buffer / declaration (0x8E2440..0x8E2450) are librw's d3d caches here.
//  * the exe's globals 0x8E29C0 / 0x8E29C4 (one-shot ValidateDevice probes of the MODULATE2X dual mode and of the single-pass env ADD) and 0xC9ABB4 / 0xC9ABB8 (their
//    results) live in `g`; the MODULATE/2-3 stage choices test MaxTextureBlendStages == 3 exactly like the exe (>= 4 takes the 2-stage branch).
// Needs only fakerw + librw + the CRT; excluded from the unity build.
#ifdef NOTSA_RW_LIBRW
#include "fakerw.h"
#include <src/d3d/rwd3dimpl.h>

#include <cmath>
#include <cstdint>
#include <cstring>

void RwShimMatFXPipelineEnsure();
void RwShimMatFXPipelineShutdown();

namespace {

using u32    = std::uint32_t;
using Header = rw::d3d9::InstanceDataHeader;
using Inst   = rw::d3d9::InstanceData;

constexpr u32 kPluginId    = 0x120;      // rwID_MATFX, [pipeline + 0x2C] at 0x815E71
constexpr u32 kTexturedMask = 0x84;      // rpGEOMETRYTEXTURED | rpGEOMETRYTEXTURED2
constexpr u32 kTextured2    = 0x80;
constexpr u32 kPrelit       = 0x08;
constexpr u32 kMatrixIdentity = 0x20000; // rwMATRIXINTERNALIDENTITY

// exe globals 0xC9ABAC.. / 0x8E29C0.. (initialised by 0x815F72..)
struct State {
    bool multiTex      = false; // 0xC9ABAC: MaxTextureBlendStages >= 2 && MaxSimultaneousTextures >= 2
    u32  blendStages   = 0;     // 0xC9ABBC
    bool multiplyAdd   = false; // 0xC9ABB0: TextureOpCaps & D3DTEXOPCAPS_MULTIPLYADD
    bool modulate2x    = false; // 0xC9ABB4: TextureOpCaps & D3DTEXOPCAPS_MODULATE2X (cleared when the validation probe fails)
    bool envAddOk      = false; // 0xC9ABB8: result of the single-pass env ADD probe
    bool probeDual     = false; // 0x8E29C0 (initial 1 when multiTex)
    bool probeEnv      = false; // 0x8E29C4 (initial 1 when multiTex)
    void* psEnvBase    = nullptr; // 0xC9AB98: env map with alpha, base texture present
    void* psEnvNoBase  = nullptr; // 0xC9AB9C: env map with alpha, no base texture
};
State g;

// the ps_1_1 shaders of the exe (.rdata 0x885350 / 0x8853A0), as plain token streams
//   0x885350: tex t0; tex t1; mul r0, t0, v0; mul r1, t1, t1.a; mad r0.rgb, r1, c0, r0
//   0x8853A0: tex t0; mul r0, t0, t0.a; mad r0.rgb, r0, c0, v0; +mov r0.a, v0.a
const u32 kPsEnvBase[] = {
    0xffff0101, 0x00000042, 0xb00f0000, 0x00000042, 0xb00f0001, 0x00000005, 0x800f0000, 0xb0e40000, 0x90e40000, 0x00000005, 0x800f0001, 0xb0e40001,
    0xb0ff0001, 0x00000004, 0x80070000, 0x80e40001, 0xa0e40000, 0x80e40000, 0x0000ffff,
};
const u32 kPsEnvNoBase[] = {
    0xffff0101, 0x00000042, 0xb00f0000, 0x00000005, 0x800f0000, 0xb0e40000, 0xb0ff0000, 0x00000004, 0x80070000, 0x80e40000, 0xa0e40000, 0x90e40000,
    0x40000001, 0x80080000, 0x90ff0000, 0x0000ffff,
};

// 0x8E29C8: the static texture-transform matrix; only _11 _12 _21 _22 _31 _32 are overwritten (COUNT2 reads nothing else), _33 is 1.0 in the exe's data
D3DMATRIX g_uvMatrix = {{{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1.0f, 0, 0, 0, 0, 0}}};

// 0x885608 (an RwMatrix): the sphere-map scale / bias, u = 0.5 nx + 0.5, v = -0.5 ny + 0.5
const RwMatrix kEnvBias = [] {
    RwMatrix m{};
    m.right = {0.5f, 0.0f, 0.0f};
    m.up    = {0.0f, -0.5f, 0.0f};
    m.at    = {0.0f, 0.0f, 1.0f};
    m.pos   = {0.5f, 0.5f, 0.0f};
    return m;
}();
static_assert(sizeof(RwMatrix) == sizeof(D3DMATRIX), "RwMatrix is copied into a D3DMATRIX");

void TSS(u32 stage, u32 type, u32 value) { RwD3D9SetTextureStageState(stage, type, value); }

bool RasterHasAlpha(const RwTexture* tex) { // exe 0x4C9EA0
    if (!tex || !tex->raster) {
        return false;
    }
    return GETD3DRASTEREXT(tex->raster)->hasAlpha;
}

// 0x812ae0-style texture transform upload: returns true when a matrix was set
bool UploadUVMatrix(const RwMatrix* m, u32 stage) {
    if (!m || (m->flags & kMatrixIdentity)) {
        return false;
    }
    TSS(stage, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);
    g_uvMatrix._11 = m->right.x;
    g_uvMatrix._12 = m->right.y;
    g_uvMatrix._21 = m->up.x;
    g_uvMatrix._22 = m->up.y;
    g_uvMatrix._31 = m->pos.x;
    g_uvMatrix._32 = m->pos.y;
    RwD3D9SetTransform(D3DTS_TEXTURE0 + stage, &g_uvMatrix);
    return true;
}

// first draw of an instance: pixel shader / vertex shader / flush / Draw(Indexed)Primitive (0x812BD4..0x812C70)
void Draw(const Header* h, const Inst* inst) {
    RwD3D9SetVertexShader(inst->vertexShader);
    if (h->indexBuffer) {
        RwD3D9DrawIndexedPrimitive(h->primType, inst->baseIndex, 0, inst->numVertices, inst->startIndex, inst->numPrimitives);
    } else {
        RwD3D9DrawPrimitive(h->primType, inst->baseIndex, inst->numPrimitives);
    }
}

void DrawNoPS(const Header* h, const Inst* inst) {
    RwD3D9SetPixelShader(nullptr);
    Draw(h, inst);
}

// The exe ends most effects with RwD3D9SetTexture(NULL, n) and leaves the stage's operations enabled: on D3D9 a stage without a texture that reads D3DTA_TEXTURE
// is skipped (it and all later stages are disabled). librw's setTexture(NULL) binds a WHITE texture instead, so the stage must be disabled explicitly or the
// leftover MULTIPLYADD / ADD would keep adding white to everything drawn afterwards (the stock callback relies on stages >= 1 being disabled).
void ClearStage(u32 stage) {
    RwD3D9SetTexture(nullptr, stage);
    if (stage > 0) {
        TSS(stage, D3DTSS_COLOROP, D3DTOP_DISABLE);
        TSS(stage, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
    }
}

void StageModulateTexDiffuse() {
    TSS(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    TSS(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    TSS(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    TSS(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    TSS(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    TSS(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
}

void StageDiffuseOnly() {
    TSS(0, D3DTSS_COLOROP, D3DTOP_SELECTARG2);
    TSS(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    TSS(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2);
    TSS(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
}

//--------------------------------------------------------------------------------------------------
// 0x812AC0: plain mesh (NULL effect, UVTRANSFORM (base matrix), unsupported effects)
//--------------------------------------------------------------------------------------------------
void DefaultRender(const Header* h, const Inst* inst, u32 flags, RwTexture* tex) {
    bool transformed = false;
    if (flags & kTexturedMask) {
        RwD3D9SetTexture(tex, 0);
        const rw::MatFX* fx = rw::MatFX::get(inst->material);
        if (fx && fx->type == rw::MatFX::UVTRANSFORM) {
            transformed = UploadUVMatrix(fx->fx[0].uvtransform.baseTransform, 0);
        }
        StageModulateTexDiffuse();
    } else {
        RwD3D9SetTexture(nullptr, 0);
        StageDiffuseOnly();
    }
    DrawNoPS(h, inst);
    if (transformed) {
        TSS(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
    }
}

//--------------------------------------------------------------------------------------------------
// 0x812D40: DUAL / DUALUVTRANSFORM
//--------------------------------------------------------------------------------------------------
void SetStage(u32 stage, u32 colorOp, u32 colorArg1, u32 colorArg2, u32 alphaOp, u32 alphaArg1, u32 alphaArg2) {
    TSS(stage, D3DTSS_COLOROP, colorOp);
    TSS(stage, D3DTSS_COLORARG1, colorArg1);
    TSS(stage, D3DTSS_COLORARG2, colorArg2);
    TSS(stage, D3DTSS_ALPHAOP, alphaOp);
    TSS(stage, D3DTSS_ALPHAARG1, alphaArg1);
    TSS(stage, D3DTSS_ALPHAARG2, alphaArg2);
}

bool ValidateOnePass() { // exe 0x813318 / 0x813F53: Flush; ValidateDevice(&numPasses) == OK && numPasses == 1
    DWORD passes = 0;
    rw::d3d::flushCache();
    const HRESULT hr = rw::d3d::d3ddevice->ValidateDevice(&passes);
    return SUCCEEDED(hr) && passes == 1;
}

void DualRender(const Header* h, const Inst* inst, u32 flags, RwTexture* tex, RwTexture* dualTex) {
    if (!dualTex || !dualTex->raster) { // 0x812D50 / 0x8139EE
        DefaultRender(h, inst, flags, tex);
        return;
    }
    const rw::MatFX* fx = rw::MatFX::get(inst->material);
    const rw::MatFX::UVtransform* uv = nullptr;
    const rw::MatFX::Dual*        d  = &fx->fx[0].dual;
    if (fx->type == rw::MatFX::DUALUVTRANSFORM) {
        uv = &fx->fx[0].uvtransform;
        d  = &fx->fx[1].dual;
    }
    bool secondPass = true; // ebx

    if (flags & kTexturedMask) {
        RwD3D9SetTexture(tex, 0);
        StageModulateTexDiffuse();
    } else {
        RwD3D9SetTexture(nullptr, 0);
        StageDiffuseOnly();
    }
    if (uv) {
        UploadUVMatrix(uv->baseTransform, 0);
    }
    // (0x4CD250 palette sharing of two PAL8 rasters: not applicable)

    const int src = d->srcBlend, dst = d->dstBlend;
    bool tailA = false; // 0x8133C8: texcoord set / dual UV transform of stage 1 for the single-pass modes
    auto singlePass = [&]() { secondPass = false; tailA = true; };

    if (g.multiTex) {
        if ((src == rwBLENDDESTCOLOR && dst == rwBLENDZERO) || (src == rwBLENDZERO && dst == rwBLENDSRCCOLOR)) {
            // modulate: base * dual (* diffuse)
            if (!tex) {
                RwD3D9SetTexture(dualTex, 0);
                StageModulateTexDiffuse();
                singlePass();
            } else {
                RwD3D9SetTexture(dualTex, 1);
                if (g.blendStages == 3) {
                    TSS(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
                    TSS(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
                    TSS(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
                    TSS(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
                    TSS(1, D3DTSS_COLOROP, D3DTOP_MODULATE);
                    TSS(1, D3DTSS_COLORARG1, D3DTA_TEXTURE);
                    TSS(1, D3DTSS_COLORARG2, D3DTA_CURRENT);
                    TSS(1, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2);
                    TSS(1, D3DTSS_ALPHAARG2, D3DTA_CURRENT);
                    SetStage(2, D3DTOP_MODULATE, D3DTA_DIFFUSE, D3DTA_CURRENT, D3DTOP_MODULATE, D3DTA_DIFFUSE, D3DTA_CURRENT);
                } else {
                    TSS(1, D3DTSS_COLOROP, D3DTOP_MODULATE);
                    TSS(1, D3DTSS_COLORARG1, D3DTA_TEXTURE);
                    TSS(1, D3DTSS_COLORARG2, D3DTA_CURRENT);
                    TSS(1, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2);
                    TSS(1, D3DTSS_ALPHAARG2, D3DTA_CURRENT);
                }
                singlePass();
            }
        } else if (src == rwBLENDSRCALPHA && dst == rwBLENDINVSRCALPHA) {
            // alpha blend of the dual texture over the base
            if (!tex) {
                RwD3D9SetRenderState(D3DRS_TEXTUREFACTOR, 0xFFFFFFFFu);
                RwD3D9SetTexture(dualTex, 0);
                TSS(0, D3DTSS_COLOROP, D3DTOP_BLENDTEXTUREALPHA);
                TSS(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
                TSS(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);
                TSS(1, D3DTSS_COLOROP, D3DTOP_MODULATE);
                TSS(1, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
                TSS(1, D3DTSS_COLORARG2, D3DTA_CURRENT);
                TSS(1, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
                TSS(1, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
                TSS(1, D3DTSS_ALPHAARG2, D3DTA_CURRENT);
                singlePass();
            } else if (g.blendStages >= 3) {
                RwD3D9SetTexture(dualTex, 1);
                TSS(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
                TSS(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
                TSS(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
                TSS(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
                SetStage(1, D3DTOP_BLENDTEXTUREALPHA, D3DTA_TEXTURE, D3DTA_CURRENT, D3DTOP_BLENDTEXTUREALPHA, D3DTA_TEXTURE, D3DTA_CURRENT);
                SetStage(2, D3DTOP_MODULATE, D3DTA_DIFFUSE, D3DTA_CURRENT, D3DTOP_MODULATE, D3DTA_DIFFUSE, D3DTA_CURRENT);
                singlePass();
            }
        } else if (src == rwBLENDDESTCOLOR && dst == rwBLENDSRCCOLOR) {
            // 2x modulate, needs MODULATE2X and a one-time ValidateDevice
            if (g.modulate2x) {
                if (!tex) {
                    RwD3D9SetTexture(dualTex, 0);
                    TSS(0, D3DTSS_COLOROP, D3DTOP_MODULATE2X);
                    TSS(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2);
                    TSS(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
                } else {
                    RwD3D9SetTexture(dualTex, 1);
                    TSS(1, D3DTSS_COLOROP, D3DTOP_MODULATE2X);
                    TSS(1, D3DTSS_COLORARG1, D3DTA_TEXTURE);
                    TSS(1, D3DTSS_COLORARG2, D3DTA_CURRENT);
                    if (g.blendStages == 3) {
                        TSS(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
                        TSS(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
                        TSS(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
                        TSS(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
                        TSS(1, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2);
                        TSS(1, D3DTSS_ALPHAARG2, D3DTA_CURRENT);
                        SetStage(2, D3DTOP_MODULATE, D3DTA_DIFFUSE, D3DTA_CURRENT, D3DTOP_MODULATE, D3DTA_DIFFUSE, D3DTA_CURRENT);
                    } else {
                        TSS(1, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2);
                        TSS(1, D3DTSS_ALPHAARG2, D3DTA_CURRENT);
                    }
                }
                bool ok = true;
                if (g.probeDual) {
                    ok = ValidateOnePass();
                    if (!ok) { // 0x81333D: the mode is disabled for good, back to the plain stage setup (second pass follows)
                        g.modulate2x = false;
                        RwD3D9SetTexture(tex, 0);
                        SetStage(0, D3DTOP_MODULATE, D3DTA_TEXTURE, D3DTA_DIFFUSE, D3DTOP_MODULATE, D3DTA_TEXTURE, D3DTA_DIFFUSE);
                        TSS(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
                        TSS(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
                        TSS(2, D3DTSS_COLOROP, D3DTOP_DISABLE);
                        TSS(2, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
                        ClearStage(1);
                    }
                    g.probeDual = false;
                }
                if (ok) {
                    singlePass();
                }
            }
        } else if (src == rwBLENDONE && dst == rwBLENDONE) {
            // additive: base + dual * diffuse
            if (!tex) {
                RwD3D9SetTexture(dualTex, 0);
                TSS(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2);
                TSS(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
                TSS(1, D3DTSS_COLOROP, D3DTOP_ADD);
                TSS(1, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
                TSS(1, D3DTSS_COLORARG2, D3DTA_CURRENT);
                TSS(1, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2);
                TSS(1, D3DTSS_ALPHAARG2, D3DTA_CURRENT);
                singlePass();
            } else if (g.multiplyAdd) {
                RwD3D9SetTexture(dualTex, 1);
                TSS(1, D3DTSS_COLOROP, D3DTOP_MULTIPLYADD);
                TSS(1, D3DTSS_COLORARG0, D3DTA_CURRENT);
                TSS(1, D3DTSS_COLORARG1, D3DTA_TEXTURE);
                TSS(1, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
                TSS(1, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2);
                TSS(1, D3DTSS_ALPHAARG2, D3DTA_CURRENT);
                singlePass();
            }
        } else if (src == rwBLENDZERO && dst == rwBLENDSRCALPHA) {
            // base * alpha(dual)
            if (!tex) {
                RwD3D9SetTexture(dualTex, 0);
                TSS(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
                TSS(0, D3DTSS_COLORARG1, D3DTA_TEXTURE | D3DTA_ALPHAREPLICATE);
                TSS(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
                TSS(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2);
                TSS(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
                singlePass();
            } else {
                RwD3D9SetTexture(dualTex, 1);
                if (g.blendStages == 3) {
                    TSS(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
                    TSS(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
                    TSS(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
                    TSS(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
                    TSS(1, D3DTSS_COLOROP, D3DTOP_MODULATE);
                    TSS(1, D3DTSS_COLORARG1, D3DTA_TEXTURE | D3DTA_ALPHAREPLICATE);
                    TSS(1, D3DTSS_COLORARG2, D3DTA_CURRENT);
                    TSS(1, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2);
                    TSS(1, D3DTSS_ALPHAARG2, D3DTA_CURRENT);
                    SetStage(2, D3DTOP_MODULATE, D3DTA_DIFFUSE, D3DTA_CURRENT, D3DTOP_MODULATE, D3DTA_DIFFUSE, D3DTA_CURRENT);
                } else {
                    TSS(1, D3DTSS_COLOROP, D3DTOP_MODULATE);
                    TSS(1, D3DTSS_COLORARG1, D3DTA_TEXTURE | D3DTA_ALPHAREPLICATE);
                    TSS(1, D3DTSS_COLORARG2, D3DTA_CURRENT);
                    TSS(1, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2);
                    TSS(1, D3DTSS_ALPHAARG2, D3DTA_CURRENT);
                }
                singlePass();
            }
        }
    }

    if (tailA) { // 0x8133C8
        if (!(flags & kTextured2)) {
            TSS(1, D3DTSS_TEXCOORDINDEX, 0);
        }
        if (uv) {
            UploadUVMatrix(uv->dualTransform, 1);
        }
    }

    // first draw (0x813443)
    DrawNoPS(h, inst);

    if (!secondPass) { // 0x81396C: undo the single-pass chain
        TSS(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
        TSS(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        TSS(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
        ClearStage(1);
        TSS(2, D3DTSS_COLOROP, D3DTOP_DISABLE);
        TSS(2, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
        if (!(flags & kTextured2)) {
            TSS(1, D3DTSS_TEXCOORDINDEX, 1);
        }
    } else {
        // second pass (0x813745): the dual texture blended over the first pass with the material's blend modes
        RwD3D9SetTexture(dualTex, 0);
        if (uv) {
            UploadUVMatrix(uv->dualTransform, 0);
        }
        if (!_rwD3D9RenderStateVertexAlphaIsEnabled()) {
            _rwD3D9RenderStateVertexAlphaEnable(TRUE);
        }
        if ((src == rwBLENDDESTCOLOR && dst == rwBLENDZERO) || (src == rwBLENDZERO && dst == rwBLENDSRCCOLOR) || (src == rwBLENDDESTCOLOR && dst == rwBLENDSRCCOLOR)) {
            RwD3D9SetRenderState(D3DRS_ALPHATESTENABLE, FALSE); // not restored by the exe either
        }
        void *savedSrc = nullptr, *savedDst = nullptr;
        RwRenderStateGet(rwRENDERSTATESRCBLEND, &savedSrc);
        RwRenderStateGet(rwRENDERSTATEDESTBLEND, &savedDst);
        RwRenderStateSet(rwRENDERSTATESRCBLEND, reinterpret_cast<void*>(static_cast<std::uintptr_t>(src)));
        RwRenderStateSet(rwRENDERSTATEDESTBLEND, reinterpret_cast<void*>(static_cast<std::uintptr_t>(dst)));
        DWORD zwrite = 0, fog = 0, fogColor = 0;
        RwD3D9GetRenderState(D3DRS_ZWRITEENABLE, &zwrite);
        RwD3D9SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        RwD3D9GetRenderState(D3DRS_FOGENABLE, &fog);
        if (fog) {
            RwD3D9GetRenderState(D3DRS_FOGCOLOR, &fogColor);
            if (dst == rwBLENDONE) {
                RwD3D9SetRenderState(D3DRS_FOGCOLOR, 0);
            } else if (src == rwBLENDDESTCOLOR || dst == rwBLENDSRCCOLOR) {
                RwD3D9SetRenderState(D3DRS_FOGCOLOR, 0xFFFFFFFFu);
            }
        }
        const bool tex2 = (flags & kTextured2) != 0;
        if (tex2) {
            TSS(0, D3DTSS_TEXCOORDINDEX, 1);
        }
        Draw(h, inst);
        RwD3D9SetRenderState(D3DRS_ZWRITEENABLE, zwrite);
        if (fog) {
            RwD3D9SetRenderState(D3DRS_FOGCOLOR, fogColor);
        }
        if (tex2) {
            TSS(0, D3DTSS_TEXCOORDINDEX, 0);
        }
        _rwD3D9RenderStateVertexAlphaEnable(FALSE);
        RwRenderStateSet(rwRENDERSTATESRCBLEND, savedSrc);
        RwRenderStateSet(rwRENDERSTATEDESTBLEND, savedDst);
    }
    if (uv) {
        TSS(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
        TSS(1, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
    }
}

//--------------------------------------------------------------------------------------------------
// 0x814270: texture matrix of an env map on `stage` (camera-space normal -> sphere map), RwFrame of the effect optional
//--------------------------------------------------------------------------------------------------
void SetEnvMapMatrix(RwTexture* env, u32 stage, RwFrame* frame) {
    RwD3D9SetTexture(env, stage);
    TSS(stage, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);
    TSS(stage, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_CAMERASPACENORMAL);
    if (frame) {
        RwCamera* cam = RwEngineInstance->curCamera;
        RwMatrix* camLTM   = RwFrameGetLTM(RwCameraGetFrame(cam));
        RwMatrix* frameLTM = RwFrameGetLTM(frame);
        RwMatrix inv, prod, res;
        RwMatrixInvert(&inv, frameLTM);
        RwMatrixMultiply(&prod, &inv, camLTM);
        prod.right.x = -prod.right.x;
        prod.right.y = -prod.right.y;
        prod.right.z = -prod.right.z;
        prod.flags   = 0;
        prod.pos.x = prod.pos.y = prod.pos.z = 0.0f;
        RwMatrixMultiply(&res, &prod, &kEnvBias);
        RwD3D9SetTransform(D3DTS_TEXTURE0 + stage, &res);
    } else {
        RwD3D9SetTransform(D3DTS_TEXTURE0 + stage, &kEnvBias);
    }
}

//--------------------------------------------------------------------------------------------------
// 0x813A10: ENVMAP (pass = 0), also the second half of BUMPENVMAP (pass = 1: no first draw)
//--------------------------------------------------------------------------------------------------
void EnvRender(const Header* h, const Inst* inst, u32 flags, u32 pass, RwTexture* base, RwTexture* env) {
    const rw::MatFX* fx = rw::MatFX::get(inst->material);
    const rw::MatFX::Env& e = fx->fx[pass].env;
    const float scaled = e.coefficient * 255.0f;
    const u32 cb = static_cast<u32>(std::lrintf(scaled)) & 0xFF;
    if (cb == 0 || !env || !env->raster) {
        if (pass == 0) {
            DefaultRender(h, inst, flags, base);
        }
        return;
    }
    RwD3D9SetVertexShader(inst->vertexShader);

    if (pass == 0) {
        bool hasBase = false, single = false;
        if ((flags & kTexturedMask) && base) {
            RwD3D9SetTexture(base, 0);
            hasBase = true;
        } else {
            RwD3D9SetTexture(nullptr, 0);
        }

        bool eligible = e.fbAlpha != 0;
        if (!eligible && inst->vertexAlpha == 0 && inst->material->color.alpha == 0xFF) {
            eligible = !hasBase || !RasterHasAlpha(base);
        }
        if (eligible) {
            if (RasterHasAlpha(env)) {
                if (g.psEnvBase) { // 0x813C7A: one pass with a pixel shader
                    float c[4] = {e.coefficient, e.coefficient, e.coefficient, e.coefficient};
                    // librw's flushCache writes its fog colour into PS constant 0 (PSLOC_fogColor) when the fog data is dirty (the exe's RW never uses c0 for
                    // fog): every PS-constant write is the LAST state change before the draw, after a flush, so nothing can overwrite c0 until Draw
                    // (Draw's own flush finds the fog data clean). The exe sets its constant before the matrix / shader; the draw sees the same value.
                    if (hasBase) {
                        SetEnvMapMatrix(env, 1, e.frame);
                        RwD3D9SetPixelShader(g.psEnvBase);
                        rw::d3d::flushCache();
                        rw::d3d::d3ddevice->SetPixelShaderConstantF(0, c, 1);
                        Draw(h, inst);
                        RwD3D9SetPixelShader(nullptr); // (the exe leaves it bound until the next effect resets it)
                        ClearStage(1);
                        TSS(1, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
                        TSS(1, D3DTSS_TEXCOORDINDEX, 1);
                    } else {
                        SetEnvMapMatrix(env, 0, e.frame);
                        RwD3D9SetPixelShader(g.psEnvNoBase);
                        rw::d3d::flushCache();
                        rw::d3d::d3ddevice->SetPixelShaderConstantF(0, c, 1);
                        Draw(h, inst);
                        RwD3D9SetPixelShader(nullptr);
                        RwD3D9SetTexture(nullptr, 0);
                        TSS(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
                        TSS(0, D3DTSS_TEXCOORDINDEX, 0);
                    }
                    return;
                }
            } else if (g.multiplyAdd || (cb == 0xFF && (!hasBase || g.envAddOk || g.probeEnv))) {
                single = true;
            }
        }
        RwD3D9SetPixelShader(nullptr);
        if (hasBase) {
            StageModulateTexDiffuse();
        } else {
            TSS(0, D3DTSS_COLOROP, D3DTOP_SELECTARG2);
            TSS(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
            TSS(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2);
            TSS(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
        }
        if (single) { // 0x813E1C: the env map on stage 1 (stage 0 when there is no base texture) in the same draw
            if (cb == 0xFF && !hasBase) {
                SetEnvMapMatrix(env, 0, e.frame);
                TSS(0, D3DTSS_COLOROP, D3DTOP_ADD);
                Draw(h, inst);
                RwD3D9SetTexture(nullptr, 0);
                TSS(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
                TSS(0, D3DTSS_TEXCOORDINDEX, 0);
                return;
            }
            SetEnvMapMatrix(env, 1, e.frame);
            if (cb == 0xFF) {
                TSS(1, D3DTSS_COLOROP, D3DTOP_ADD);
                TSS(1, D3DTSS_COLORARG1, D3DTA_TEXTURE);
                TSS(1, D3DTSS_COLORARG2, D3DTA_CURRENT);
                if (g.probeEnv) {
                    g.envAddOk = ValidateOnePass();
                    g.probeEnv = false;
                }
            } else {
                RwD3D9SetRenderState(D3DRS_TEXTUREFACTOR, (cb << 24) | (cb << 16) | (cb << 8) | cb);
                TSS(1, D3DTSS_COLOROP, D3DTOP_MULTIPLYADD);
                TSS(1, D3DTSS_COLORARG0, D3DTA_CURRENT);
                TSS(1, D3DTSS_COLORARG1, D3DTA_TEXTURE);
                TSS(1, D3DTSS_COLORARG2, D3DTA_TFACTOR);
            }
            Draw(h, inst);
            ClearStage(1);
            TSS(1, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
            TSS(1, D3DTSS_TEXCOORDINDEX, 1);
            return;
        }
        Draw(h, inst); // first pass: the plain mesh
    }

    // second pass (0x814003): the env texture added on top (SRCALPHA, ONE), no lighting, no z write, black fog
    SetEnvMapMatrix(env, 0, e.frame);
    if (!_rwD3D9RenderStateVertexAlphaIsEnabled()) {
        _rwD3D9RenderStateVertexAlphaEnable(TRUE);
    }
    void *savedSrc = nullptr, *savedDst = nullptr;
    RwRenderStateGet(rwRENDERSTATESRCBLEND, &savedSrc);
    RwRenderStateGet(rwRENDERSTATEDESTBLEND, &savedDst);
    RwRenderStateSet(rwRENDERSTATESRCBLEND, reinterpret_cast<void*>(static_cast<std::uintptr_t>(rwBLENDSRCALPHA)));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND, reinterpret_cast<void*>(static_cast<std::uintptr_t>(rwBLENDONE)));
    const u32 color = (cb << 24) | (cb << 16) | (cb << 8) | cb;
    if (color != 0xFFFFFFFFu) {
        RwD3D9SetRenderState(D3DRS_TEXTUREFACTOR, color);
        TSS(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
        TSS(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        TSS(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);
        TSS(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
        TSS(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
        TSS(0, D3DTSS_ALPHAARG2, D3DTA_TFACTOR);
    } else {
        TSS(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
        TSS(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        TSS(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
        TSS(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    }
    DWORD lighting = 0, zwrite = 0, fog = 0, fogColor = 0;
    RwD3D9GetRenderState(D3DRS_LIGHTING, &lighting);
    RwD3D9GetRenderState(D3DRS_ZWRITEENABLE, &zwrite);
    RwD3D9GetRenderState(D3DRS_FOGENABLE, &fog);
    RwD3D9SetRenderState(D3DRS_LIGHTING, FALSE);
    RwD3D9SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    if (fog) {
        RwD3D9GetRenderState(D3DRS_FOGCOLOR, &fogColor);
        RwD3D9SetRenderState(D3DRS_FOGCOLOR, 0);
    }
    Draw(h, inst);
    if (fog) {
        RwD3D9SetRenderState(D3DRS_FOGCOLOR, fogColor);
    }
    RwD3D9SetRenderState(D3DRS_ZWRITEENABLE, zwrite);
    RwD3D9SetRenderState(D3DRS_LIGHTING, lighting);
    if (color != 0xFFFFFFFFu) {
        TSS(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
        TSS(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
    } else {
        TSS(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
        TSS(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    }
    _rwD3D9RenderStateVertexAlphaEnable(FALSE);
    RwRenderStateSet(rwRENDERSTATESRCBLEND, savedSrc);
    RwRenderStateSet(rwRENDERSTATEDESTBLEND, savedDst);
    TSS(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
    TSS(0, D3DTSS_TEXCOORDINDEX, 0);
}

//--------------------------------------------------------------------------------------------------
// 0x815C20: the MatFX render callback
//--------------------------------------------------------------------------------------------------
void MatFXRenderCallback(RwResEntry* resEntry, void* object, RwUInt8 type, RwUInt32 flags) {
    const Header* h = reinterpret_cast<const Header*>(resEntry + 1);

    _rwD3D9EnableClippingIfNeeded(object, type);
    DWORD lighting = 0;
    RwD3D9GetRenderState(D3DRS_LIGHTING, &lighting);
    bool noLight = false;
    if (!lighting && !(flags & kPrelit)) {
        noLight = true;
        RwD3D9SetTexture(nullptr, 0);
        RwD3D9SetRenderState(D3DRS_TEXTUREFACTOR, 0xFF000000u);
        TSS(0, D3DTSS_COLOROP, D3DTOP_SELECTARG2);
        TSS(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);
        TSS(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2);
        TSS(0, D3DTSS_ALPHAARG2, D3DTA_TFACTOR);
    }
    if (h->indexBuffer) {
        RwD3D9SetIndices(h->indexBuffer);
    }
    _rwD3D9SetStreams(h->vertexStream, h->useOffsets);
    RwD3D9SetVertexDeclaration(h->vertexDeclaration);

    const Inst* inst = h->inst;
    for (u32 i = 0; i < h->numMeshes; i++, inst++) {
        RpMaterial* mat = inst->material;
        _rwD3D9RenderStateVertexAlphaEnable((inst->vertexAlpha == 0 && mat->color.alpha == 0xFF) ? FALSE : TRUE);
        if (noLight) {
            DrawNoPS(h, inst);
            continue;
        }
        if (lighting) {
            RwD3D9SetSurfaceProperties(&mat->surfaceProps, &mat->color, flags);
        }
        const rw::MatFX* fx = rw::MatFX::get(mat);
        const u32 effect = fx ? fx->type : 0;
        switch (effect) {
        case rw::MatFX::BUMPMAP:
            DefaultRender(h, inst, flags, mat->texture);
            break;
        case rw::MatFX::ENVMAP:
            EnvRender(h, inst, flags, 0, mat->texture, fx->fx[0].env.tex);
            break;
        case rw::MatFX::BUMPENVMAP:
            DefaultRender(h, inst, flags, mat->texture);
            EnvRender(h, inst, flags, 1, mat->texture, fx->fx[1].env.tex);
            break;
        case rw::MatFX::DUAL:
            DualRender(h, inst, flags, mat->texture, fx->fx[0].dual.tex);
            break;
        case rw::MatFX::DUALUVTRANSFORM:
            DualRender(h, inst, flags, mat->texture, fx->fx[1].dual.tex);
            break;
        default: // NULL, UVTRANSFORM (the matrix is applied by DefaultRender)
            DefaultRender(h, inst, flags, mat->texture);
            break;
        }
    }
}

//--------------------------------------------------------------------------------------------------
// pipeline object / caps (0x815E60)
//--------------------------------------------------------------------------------------------------
RxPipeline*     g_pipe     = nullptr;
rw::ObjPipeline* g_origPipe = nullptr; // librw's own shader MatFX pipeline (owned by librw's matfxClose)
bool            g_installed = false;

void ProbeCaps() {
    g = State{};
    if (!rw::d3d::d3ddevice) {
        return;
    }
    const D3DCAPS9& caps = rw::d3d::d3d9Globals.caps;
    g.blendStages = caps.MaxTextureBlendStages;
    if (caps.MaxTextureBlendStages >= 2) {
        g.multiTex    = caps.MaxSimultaneousTextures >= 2;
        g.multiplyAdd = (caps.TextureOpCaps & D3DTEXOPCAPS_MULTIPLYADD) != 0;
        g.modulate2x  = (caps.TextureOpCaps & D3DTEXOPCAPS_MODULATE2X) != 0;
        g.probeDual   = true;
        g.probeEnv    = true;
    }
    if ((caps.PixelShaderVersion & 0xFFFF) >= 0x101) {
        rw::d3d::d3ddevice->CreatePixelShader(reinterpret_cast<const DWORD*>(kPsEnvBase), reinterpret_cast<IDirect3DPixelShader9**>(&g.psEnvBase));
        rw::d3d::d3ddevice->CreatePixelShader(reinterpret_cast<const DWORD*>(kPsEnvNoBase), reinterpret_cast<IDirect3DPixelShader9**>(&g.psEnvNoBase));
    }
}

void ReleaseShaders() {
    if (g.psEnvBase) {
        RwD3D9SetPixelShader(nullptr);
        static_cast<IDirect3DPixelShader9*>(g.psEnvBase)->Release();
        g.psEnvBase = nullptr;
    }
    if (g.psEnvNoBase) {
        static_cast<IDirect3DPixelShader9*>(g.psEnvNoBase)->Release();
        g.psEnvNoBase = nullptr;
    }
}

} // namespace

//--------------------------------------------------------------------------------------------------
// Public API
//--------------------------------------------------------------------------------------------------

// installs the MatFX AllInOne pipeline as the D3D9 MatFX pipeline of librw; call after RwEngineStart (the device exists) and after RwShimPipelineEnsure
void RwShimMatFXPipelineEnsure() {
    static bool busy = false; // RxPipelineCreate calls RwShimPipelineEnsure, which calls this
    if (busy) {
        return;
    }
    busy = true;
    struct Unbusy { ~Unbusy() { busy = false; } } unbusy;
    if (!g_pipe) {
        RxPipeline* pipe = RxPipelineCreate();
        RxNodeDefinition* def = RxNodeDefinitionGetD3D9AtomicAllInOne();
        RxLockedPipe* locked = pipe ? RxPipelineLock(pipe) : nullptr;
        if (!locked || !RxLockedPipeAddFragment(locked, nullptr, def, nullptr) || !RxLockedPipeUnlock(locked)) {
            if (pipe) {
                RxPipelineDestroy(pipe);
            }
            return;
        }
        RxPipelineNode* node = RxPipelineFindNodeByName(pipe, def->name, nullptr, nullptr);
        RxD3D9AllInOneSetRenderCallBack(node, MatFXRenderCallback);
        pipe->pluginID   = kPluginId;
        pipe->pluginData = 0;
        g_pipe = pipe;
    }
    if (g.psEnvBase || g.psEnvNoBase) {
        ReleaseShaders();
    }
    ProbeCaps();
    rw::ObjPipeline*& slot = rw::matFXGlobals.pipelines[rw::PLATFORM_D3D9];
    if (!g_installed || slot != g_pipe) {
        g_origPipe  = slot;
        slot        = g_pipe;
        g_installed = true;
    }
}

// restores librw's pipeline (its matfxClose destroys whatever sits in the slot) and frees ours; call before RwEngineStop / Close
void RwShimMatFXPipelineShutdown() {
    if (g_installed) {
        rw::ObjPipeline*& slot = rw::matFXGlobals.pipelines[rw::PLATFORM_D3D9];
        if (slot == g_pipe) {
            slot = g_origPipe;
        }
        g_installed = false;
        g_origPipe  = nullptr;
    }
    ReleaseShaders();
    if (g_pipe) {
        RxPipelineDestroy(g_pipe);
        g_pipe = nullptr;
    }
}

// 0x811C00: the MatFX atomic flag + the MatFX pipeline, only when the flag was clear
RpAtomic* RpMatFXAtomicEnableEffects(RpAtomic* atomic) {
    if (!atomic) {
        return nullptr;
    }
    if (!rw::MatFX::getEffects(atomic)) {
        rw::MatFX::enableEffects(atomic);
    }
    return atomic;
}
#endif

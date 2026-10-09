// P2B-02c: the RwD3D9 device-state API used by the game's fixed-function pipelines (CustomBuilding / CustomBuildingDN / CustomCarEnvMap,
// PipelinesCommon.hpp, WindowedMode's view/proj upload), on top of librw's D3D9 backend.
//
//   RwD3D9{Set,Get}RenderState / {Set,Get}TextureStageState   -> rw::d3d::{set,get}RenderState / TextureStageState. These are librw's OWN state
//        caches (dirty lists flushed by d3d::flushCache() before every draw), so librw's draws and ours agree on what is on the device.
//   RwD3D9SetTexture(tex, stage)                              -> rw::d3d::setTexture(stage, tex) (argument order swapped; also applies the
//        texture's filter / addressing, as RW did)
//   RwD3D9{Set,Get}Transform, {Set,Get}Light, EnableLight     -> the device directly (librw has no cache for them) + a shadow for Get.
//   RwD3D9SetMaterial                                         -> rw::d3d::setD3dMaterial (librw's material cache)
//   RwD3D9SetSurfaceProperties                                -> port of the exe's 0x7FC4D0 (the game inlines the same logic at 0x5DA790,
//        see CustomCarEnvMapPipeline.cpp), with RW's early-out cache.
//   RwD3D9Set{VertexShader,PixelShader,Indices,VertexDeclaration,StreamSource}, _rwD3D9SetStreams -> rw::d3d::set* (cached)
//   RwD3D9Draw{Primitive,IndexedPrimitive}                    -> flushCache + the device
//
// NOT here (other batches own them): RwD3D9GetCurrentD3DDevice / GetCaps / DeviceSupportsDXTTexture (engine.cpp, 02a), RwD3D9SetStencilClear and
// RwD3D9CameraAttachWindow (camera.cpp, 02b), the restore callbacks (09).
//
// Needs only fakerw + librw + the CRT; excluded from the unity build.
#ifdef NOTSA_RW_LIBRW
#include "fakerw.h"

#include <cstdint>
#include <cstring>
#include <vector>

namespace {

using u32 = std::uint32_t;

constexpr u32 kMaxStages     = 8;                       // librw's MAXNUMSTAGES
constexpr u32 kNumRenderStates = D3DRS_BLENDOPALPHA + 1; // librw's MAXNUMSTATES
constexpr u32 kNumTexStates  = D3DTSS_CONSTANT + 1;     // librw's MAXNUMTEXSTATES
constexpr u32 kMaxTransforms = 512;                     // D3DTS_WORLD = 256, TEXTURE0..7 = 16..23
constexpr int kMaxLights     = 64;

IDirect3DDevice9* Dev() { return rw::d3d::d3ddevice; }

D3DMATRIX Identity() {
    D3DMATRIX m{};
    m._11 = m._22 = m._33 = m._44 = 1.0f;
    return m;
}

// Shadow of what was handed to the device (for Get*, and so Get works without a device in the unit tests)
struct Transforms {
    D3DMATRIX m[kMaxTransforms];
    Transforms() { for (auto& x : m) x = Identity(); }
};
Transforms g_transforms;

std::vector<D3DLIGHT9> g_lights;
std::vector<bool>      g_lightEnabled;

// RwD3D9SetSurfaceProperties early-out cache (exe: 0xC9A5D8..0xC9A600) and the persistent material scratch (exe: 0x8E2538)
struct SurfPropCache {
    float ambient = 0, diffuse = 0;
    u32   color = 0;
    u32   flags = 0;
    float ambSat[3] = {0, 0, 0};
} g_sp;
D3DMATERIAL9 g_spMaterial{};
// the exe's material cache (0xC98AF8, 17 dwords): the last material RwD3D9SetMaterial / RwD3D9SetSurfaceProperties handed to the device
D3DMATERIAL9 g_matCache{};
static_assert(sizeof(D3DMATERIAL9) == 17 * 4, "exe compares 0x11 dwords");

// Set the D3D material through librw's cache. No-op without a device.
bool ApplyMaterial(const D3DMATERIAL9* m) {
    if (!Dev()) {
        return false;
    }
    rw::d3d::setD3dMaterial(const_cast<D3DMATERIAL9*>(m));
    return true;
}

} // namespace

//--------------------------------------------------------------------------------------------------
// Render / texture-stage states (librw's caches; usable without a device, the device sees them at the next flush)
//--------------------------------------------------------------------------------------------------
void RwD3D9SetRenderState(RwUInt32 state, RwUInt32 value) {
    if (state < kNumRenderStates) {
        rw::d3d::setRenderState(state, value);
    }
}

void RwD3D9GetRenderState(RwUInt32 state, void* value) {
    if (state < kNumRenderStates && value) {
        rw::d3d::getRenderState(state, static_cast<u32*>(value));
    }
}

void RwD3D9SetTextureStageState(RwUInt32 stage, RwUInt32 type, RwUInt32 value) {
    if (stage < kMaxStages && type < kNumTexStates) {
        rw::d3d::setTextureStageState(stage, type, value);
    }
}

void RwD3D9GetTextureStageState(RwUInt32 stage, RwUInt32 type, void* value) {
    if (stage < kMaxStages && type < kNumTexStates && value) {
        rw::d3d::getTextureStageState(stage, type, static_cast<u32*>(value));
    }
}

// RW: RwD3D9SetTexture(texture, stage). NULL clears the stage.
RwBool RwD3D9SetTexture(RwTexture* texture, RwUInt32 stage) {
    if (stage >= kMaxStages || !Dev()) {
        return FALSE;
    }
    rw::d3d::setTexture(stage, texture);
    return TRUE;
}

//--------------------------------------------------------------------------------------------------
// Transforms / lights / material (device state librw does not cache)
//--------------------------------------------------------------------------------------------------
RwBool RwD3D9SetTransform(RwUInt32 state, const void* matrix) {
    if (state >= kMaxTransforms) {
        return FALSE;
    }
    g_transforms.m[state] = matrix ? *static_cast<const D3DMATRIX*>(matrix) : Identity(); // NULL = identity
    if (!Dev()) {
        return FALSE;
    }
    return SUCCEEDED(Dev()->SetTransform(static_cast<D3DTRANSFORMSTATETYPE>(state), &g_transforms.m[state]));
}

void RwD3D9GetTransform(RwUInt32 state, void* matrix) {
    if (state < kMaxTransforms && matrix) {
        *static_cast<D3DMATRIX*>(matrix) = g_transforms.m[state];
    }
}

RwBool RwD3D9SetLight(RwInt32 index, const void* light) {
    if (index < 0 || index >= kMaxLights || !light) {
        return FALSE;
    }
    if (static_cast<size_t>(index) >= g_lights.size()) {
        g_lights.resize(index + 1, D3DLIGHT9{});
        g_lightEnabled.resize(index + 1, false);
    }
    g_lights[index] = *static_cast<const D3DLIGHT9*>(light);
    if (!Dev()) {
        return FALSE;
    }
    return SUCCEEDED(Dev()->SetLight(index, &g_lights[index]));
}

void RwD3D9GetLight(RwInt32 index, void* light) {
    if (!light) {
        return;
    }
    if (index >= 0 && static_cast<size_t>(index) < g_lights.size()) {
        *static_cast<D3DLIGHT9*>(light) = g_lights[index];
    } else {
        *static_cast<D3DLIGHT9*>(light) = D3DLIGHT9{};
    }
}

RwBool RwD3D9EnableLight(RwInt32 index, RwBool enable) {
    if (index < 0 || index >= kMaxLights) {
        return FALSE;
    }
    if (static_cast<size_t>(index) >= g_lights.size()) {
        return TRUE; // exe 0x7FA860: a light index that was never SetLight'ed is ignored (TRUE), nothing is allocated or enabled
    }
    g_lightEnabled[index] = enable != FALSE;
    if (!Dev()) {
        return FALSE;
    }
    return SUCCEEDED(Dev()->LightEnable(index, enable ? TRUE : FALSE));
}

// Port of the exe's RwD3D9SetMaterial (0x7FC430): identical to the cached material -> TRUE and nothing else; otherwise remember it, zero the
// early-out cache's flags word (0x7FC470; this does NOT invalidate a cached flags==0 entry, as in the exe: a SetSurfaceProperties call that
// repeats the previous arguments with flags 0 right after a SetMaterial keeps the material set here) and hand it to the device.
RwBool RwD3D9SetMaterial(const void* material) {
    if (!material) {
        return FALSE;
    }
    if (std::memcmp(&g_matCache, material, sizeof(g_matCache)) == 0) {
        return TRUE;
    }
    std::memcpy(&g_matCache, material, sizeof(g_matCache));
    g_sp.flags = 0;
    return ApplyMaterial(static_cast<const D3DMATERIAL9*>(material)) ? TRUE : FALSE;
}

// Port of the exe's RwD3D9SetSurfaceProperties (0x7FC4D0). `flags` only matters for rxGEOMETRY_PRELIT (0x08) and rxGEOMETRY_MODULATE (0x40).
RwBool RwD3D9SetSurfaceProperties(const RwSurfaceProperties* sp, const RwRGBA* color, RwUInt32 flags) {
    if (!sp || !color) {
        return FALSE;
    }
    const u32 f = flags & (rxGEOMETRY_PRELIT | rxGEOMETRY_MODULATE);
    u32 colorBits;
    std::memcpy(&colorBits, color, sizeof(colorBits));

    const float ambSat[3] = {AmbientSaturated.red, AmbientSaturated.green, AmbientSaturated.blue};
    // the exe compares the cached values as dwords (0x7FC4E4..0x7FC53B), not as floats
    if (std::memcmp(&g_sp.ambient, &sp->ambient, 4) == 0 && std::memcmp(&g_sp.diffuse, &sp->diffuse, 4) == 0 && g_sp.color == colorBits &&
        g_sp.flags == f && std::memcmp(g_sp.ambSat, ambSat, sizeof(ambSat)) == 0) {
        return TRUE;
    }
    g_sp.ambient = sp->ambient;
    g_sp.diffuse = sp->diffuse;
    g_sp.color   = colorBits;
    g_sp.flags   = f;
    std::memcpy(g_sp.ambSat, ambSat, sizeof(ambSat));

    constexpr float inv255   = 1.0f / 255.0f;
    const bool      prelit   = (f & rxGEOMETRY_PRELIT) != 0;
    const bool      modulate = (f & rxGEOMETRY_MODULATE) != 0 && colorBits != 0xFFFFFFFFu;
    D3DMATERIAL9&   m        = g_spMaterial;

    if (modulate) {
        // x87: the scale factors stay in extended precision on the FPU stack (0x7FC59D: diffuse * (1/255) is not rounded to float); doubles
        const double r = color->red, g = color->green, b = color->blue, a = color->alpha;
        const double d = double(sp->diffuse) * double(inv255);
        m.Diffuse = {float(r * d), float(g * d), float(b * d), float(a * double(inv255))};

        if (prelit) {
            const u32 argb = (u32(color->alpha) << 24) | (u32(color->red) << 16) | (u32(color->green) << 8) | u32(color->blue);
            RwD3D9SetRenderState(D3DRS_AMBIENT, argb);
            RwD3D9SetRenderState(D3DRS_COLORVERTEX, TRUE);
            RwD3D9SetRenderState(D3DRS_AMBIENTMATERIALSOURCE, D3DMCS_COLOR1);
            RwD3D9SetRenderState(D3DRS_EMISSIVEMATERIALSOURCE, D3DMCS_MATERIAL);
        } else {
            RwD3D9SetRenderState(D3DRS_AMBIENT, 0xFFFFFFFFu);
            RwD3D9SetRenderState(D3DRS_COLORVERTEX, FALSE);
            RwD3D9SetRenderState(D3DRS_AMBIENTMATERIALSOURCE, D3DMCS_MATERIAL);
            RwD3D9SetRenderState(D3DRS_EMISSIVEMATERIALSOURCE, D3DMCS_MATERIAL);
        }
        const double k = double(sp->ambient) * double(inv255); // fld ambient; fmul 1/255 (0x7FC609), kept as st(1)
        // (byte * AmbientSaturated) * k : fild; fmul [AmbientSaturated]; fmul st(1) (0x7FC722..) - one rounding to float at the store
        const D3DCOLORVALUE lit = {float(r * double(ambSat[0]) * k), float(g * double(ambSat[1]) * k), float(b * double(ambSat[2]) * k), m.Ambient.a};
        const D3DCOLORVALUE zero = {0.f, 0.f, 0.f, 0.f};
        // prelit: the ambient colour comes from the vertices (COLOR1) so the lit term goes into the emissive slot; otherwise into ambient
        m.Ambient  = prelit ? D3DCOLORVALUE{0.f, 0.f, 0.f, m.Ambient.a} : D3DCOLORVALUE{lit.r, lit.g, lit.b, m.Ambient.a};
        m.Emissive = prelit ? D3DCOLORVALUE{lit.r, lit.g, lit.b, m.Emissive.a} : D3DCOLORVALUE{zero.r, zero.g, zero.b, m.Emissive.a};
    } else {
        const float d = sp->diffuse;
        m.Diffuse = {d, d, d, 1.0f};

        RwD3D9SetRenderState(D3DRS_AMBIENT, 0xFFFFFFFFu);
        RwD3D9SetRenderState(D3DRS_AMBIENTMATERIALSOURCE, D3DMCS_MATERIAL);
        if (prelit) {
            RwD3D9SetRenderState(D3DRS_COLORVERTEX, TRUE);
            RwD3D9SetRenderState(D3DRS_EMISSIVEMATERIALSOURCE, D3DMCS_COLOR1);
            // the exe leaves the emissive material as it was
        } else {
            RwD3D9SetRenderState(D3DRS_COLORVERTEX, FALSE);
            RwD3D9SetRenderState(D3DRS_EMISSIVEMATERIALSOURCE, D3DMCS_MATERIAL);
            m.Emissive = {0.f, 0.f, 0.f, m.Emissive.a};
        }
        const float k = sp->ambient;
        m.Ambient = (k == 1.0f) ? D3DCOLORVALUE{ambSat[0], ambSat[1], ambSat[2], m.Ambient.a}
                                : D3DCOLORVALUE{ambSat[0] * k, ambSat[1] * k, ambSat[2] * k, m.Ambient.a};
    }
    std::memcpy(&g_matCache, &m, sizeof(g_matCache)); // exe 0x7FCA91: rep movsd 0x11 dwords to the material cache, then the device call
    return ApplyMaterial(&m) ? TRUE : FALSE;
}

//--------------------------------------------------------------------------------------------------
// Shaders / buffers / declarations / streams (librw's device cache) and draws
//--------------------------------------------------------------------------------------------------
void RwD3D9SetVertexShader(void* shader)             { if (Dev()) rw::d3d::setVertexShader(shader); }
void RwD3D9SetPixelShader(void* shader)              { if (Dev()) rw::d3d::setPixelShader(shader); }
void RwD3D9SetIndices(void* indexBuffer)             { if (Dev()) rw::d3d::setIndices(indexBuffer); }
void RwD3D9SetVertexDeclaration(void* declaration)   { if (Dev()) rw::d3d::setVertexDeclaration(declaration); }

void RwD3D9SetStreamSource(RwUInt32 streamNumber, void* streamData, RwUInt32 offset, RwUInt32 stride) {
    if (Dev() && streamNumber < 3) {
        rw::d3d::setStreamSource(static_cast<int>(streamNumber), streamData, offset, stride);
    }
}

// RW: the FVF replaces the vertex declaration. librw's cache is reset first so a later setVertexDeclaration(x) is not skipped.
void RwD3D9SetFVF(RwUInt32 fvf) {
    if (!Dev()) {
        return;
    }
    rw::d3d::setVertexDeclaration(nullptr);
    Dev()->SetFVF(fvf);
}

// RW: bind the (up to 2) vertex streams of an instance header; stream slots after the last used one are cleared.
void _rwD3D9SetStreams(const RxD3D9VertexStream* streams, RwBool useOffsets) {
    if (!Dev() || !streams) {
        return;
    }
    int i = 0;
    for (; i < 2 && streams[i].vertexBuffer; i++) {
        rw::d3d::setStreamSource(i, streams[i].vertexBuffer, useOffsets ? streams[i].offset : 0, streams[i].stride);
    }
    for (; i < 2; i++) {
        rw::d3d::setStreamSource(i, nullptr, 0, 0);
    }
}

void RwD3D9DrawIndexedPrimitive(RwUInt32 primitiveType, RwInt32 baseVertexIndex, RwUInt32 minIndex, RwUInt32 numVertices, RwUInt32 startIndex, RwUInt32 primitiveCount) {
    if (!Dev()) {
        return;
    }
    rw::d3d::flushCache();
    Dev()->DrawIndexedPrimitive(static_cast<D3DPRIMITIVETYPE>(primitiveType), baseVertexIndex, minIndex, numVertices, startIndex, primitiveCount);
}

void RwD3D9DrawPrimitive(RwUInt32 primitiveType, RwUInt32 startVertex, RwUInt32 primitiveCount) {
    if (!Dev()) {
        return;
    }
    rw::d3d::flushCache();
    Dev()->DrawPrimitive(static_cast<D3DPRIMITIVETYPE>(primitiveType), startVertex, primitiveCount);
}

namespace notsa_rw02c {
// Test hook: drop the SetSurfaceProperties early-out cache and the light/transform shadows.
void ResetFixedFunctionShadow() {
    g_sp = SurfPropCache{};
    g_spMaterial = D3DMATERIAL9{};
    g_matCache = D3DMATERIAL9{};
    g_lights.clear();
    g_lightEnabled.clear();
    g_transforms = Transforms{};
}
const D3DMATERIAL9& SurfacePropsScratchMaterial() { return g_spMaterial; }
bool LightEnabledShadow(int i) { return i >= 0 && static_cast<size_t>(i) < g_lightEnabled.size() && g_lightEnabled[i]; }
} // namespace notsa_rw02c
#endif // NOTSA_RW_LIBRW

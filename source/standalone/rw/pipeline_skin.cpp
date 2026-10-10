// P2B-24b: the exe's HARDWARE skin render path (RenderWare 3.6 D3D9 skin pipeline, vs_1_1 vertex shader assembled at run time) on librw's skin instancing.
//
// librw's own D3D9 skin pipeline renders through precompiled vs_2_0 shaders with its own lighting (3 variants), no fog / bone-split handling and its own
// material constants. The exe renders every skinned atomic through the pipeline created by _rpSkinPipelinesCreate (0x7C8980): the skin node (0x7C7B90)
// instances the geometry and, when `skin->numMeshes != 0` (split skin data) and a hierarchy exists, calls the HW render function 0x7C8060 with the table
// of "vertex shader callbacks" stored in the node's private data (init 0x7CB240): begin 0x7CB190, lighting 0x761170, get-shader 0x761010 (-> the composer
// 0x75EED0 = agent A's notsa::skinvs), mesh render 0x761030, end 0x761000 (a bare ret).
//
// All of the numerics and the control flow of 0x7C8060 live in pipeline_skin_core.h (header-only, shared with the exe-oracle test
// tests/standalone/rw_skin_hw_oracle_test.cpp, see its header comment for what is verified bit for bit). This file is the glue:
//   * ProbeCaps (0x7CB2B0): HW T&L + VS/PS >= 1.1 + the constant budget from rw::d3d::d3d9Globals.caps
//   * the render callback installed into librw's skin pipeline object (marshals the atomic / geometry / skin / world lights into the core's structs)
//   * an instance callback that wraps librw's skinInstanceCB: the exe's vertex stream holds, in the BLENDINDICES element, the CONSTANT REGISTER OFFSET of the
//     bone (3 float4 per bone: the shader does `mov a0.x, v2.x` / `m4x3 ..., c[a0.x + base]`), i.e. 3 * remap[boneId] (the table built at 0x7C9374..0x7C93C8
//     of the exe's instance function 0x7C90D0); librw stores the raw bone id.
//   * RwShimSkinPipelineEnsure / Shutdown, called from RwShimPipelineEnsure / Shutdown (pipeline.cpp) like the MatFX pipeline.
//
// P2B-24c: the route is chosen per geometry like the exe does (resentry creation 0x7C8D60 stores `skin+0x20 = hierarchy && 0x7C8A00(atomic)`, i.e. HW T&L && VS/PS >= 1.1 &&
// the skin's bone budget `skin+0x1C` (boneLimit with split data, else numUsedBones) fits MaxVertexShaderConst): the librw pipeline's `impl.render` is replaced by a dispatcher;
// HW geometry goes on as described here (split skin data is NOT required: without it the exe uploads every bone, see 0x7C9374..0x7C942E / 0x7C8418), every other geometry
// renders through the CPU skinning route of pipeline_skin_cpu.cpp (the exe's 0x7C7B90 branch for `skin+0x20 == 0`; that route also draws a hierarchy-less skin unskinned).
//
// Deliberate differences / things left out:
//  * a geometry never changes route after its first render (its instance data is either librw's native block or the façade's resentry); the exe re-evaluates the flag
//    whenever it re-creates the resentry.
//  * a HW geometry without a hierarchy draws with identity bone matrices (the exe's node would take the non-skinned path 0x7C85B0).
//  * the bone-matrix cache of 0x7C78A0 (hierarchy, ~frame stamp, atomic frame -> skip the rebuild) is not kept: its stamp is RwEngineInstance->renderFrame, which
//    the shim never advances (see camera.cpp), so the cache would freeze the pose; rebuilding gives the same bits whenever the cache would hit.
//  * the lights touching the atomic come from librw's world->localLights (no world sectors, no per-sector stamps); the sphere test is the exe's.
//  * the D3DX kernels are the SSE variants the exe's dispatch table holds on SSE CPUs (the generic x87 ones differ in the last bit).
//  * mesh `vertexShader` is set and cleared like the exe (0x7C8060 stores the shader into the instance and zeroes it after the draw).
// Needs fakerw + librw + the CRT; excluded from the unity build.
#ifdef NOTSA_RW_LIBRW
#include "fakerw.h"
#include <src/d3d/rwd3dimpl.h>

#include "pipeline_skin_core.h"
#include "skin_vs.h"

#include <cstdint>
#include <cstring>
#include <vector>

// the stock D3D9 state helpers of the shim (rwd3d_ff.cpp / pipeline.cpp)
void _rwD3D9RenderStateVertexAlphaEnable(RwBool enable);

void RwShimSkinPipelineEnsure();
void RwShimSkinPipelineShutdown();

// P2B-24c: the CPU skinning route (pipeline_skin_cpu.cpp) and the façade's instance-data registry (pipeline.cpp)
void RwShimSkinCpuEnsure();
void RwShimSkinCpuShutdown();
void RwShimSkinCpuRender(rw::Atomic* atomic);
bool RwShimIsFacadeInstance(const void* instData);

namespace {

using u8  = std::uint8_t;
using u32 = std::uint32_t;

// exe 0xC978E0 .. 0xC978EC (filled by 0x7CB2B0)
struct Caps {
    bool hwtl         = false; // DevCaps & HWTRANSFORMANDLIGHT
    bool vs11ps11     = false; // VertexShaderVersion >= 1.1 && PixelShaderVersion >= 1.1
    u32  maxConstants = 0;     // MaxVertexShaderConst
    u32  maxBones     = 0;     // (MaxVertexShaderConst - 12) / 3
} g_caps;

rw::d3d9::ObjPipeline* g_pipe = nullptr;
void (*g_origImplRender)(rw::ObjPipeline*, rw::Atomic*) = nullptr;
void (*g_origInstance)(rw::Geometry*, rw::d3d9::InstanceDataHeader*, rw::bool32) = nullptr;
void (*g_origRender)(rw::Atomic*, rw::d3d9::InstanceDataHeader*) = nullptr;
bool g_installed = false;
bool g_forceCpu  = false; // test hook (RwShimSkinForceCpu)
// The exe's predicate sends a ped like male01 (no split data, 28 bones) to the vertex-shader path. That path renders broken geometry here (see the P2B-24c notes in the
// report: wined3d draws a few vertices of the skinned mesh at infinity although the vertex data, indices and bone constants checked on the CPU are right), so until
// it is fixed HW geometry is rendered by the CPU route; RwShimSkinEnableHardware(true) restores the exe's choice (tests / debugging).
bool g_hardwareEnabled = false;

// 0x7CB2B0 (caps block)
void ProbeCaps() {
    g_caps = Caps{};
    if (!rw::d3d::d3ddevice) {
        return;
    }
    const D3DCAPS9& c = rw::d3d::d3d9Globals.caps;
    g_caps.hwtl = (c.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT) != 0;
    if ((c.VertexShaderVersion & 0xFFFF) >= 0x101 && (c.PixelShaderVersion & 0xFFFF) >= 0x101) {
        g_caps.vs11ps11     = true;
        g_caps.maxConstants = c.MaxVertexShaderConst;
        g_caps.maxBones     = (c.MaxVertexShaderConst - 12) / 3;
    }
}

// does the skin carry the split data (bone remap + per-mesh run-length lists)? The exe tests the remap pointer (skin+0x34) at 0x7C8FEF / 0x7C9374
bool HasSplitData(const rw::Skin* skin) {
    return skin->numMeshes != 0 && skin->remapIndices && skin->rleCount && skin->rle;
}

// the exe's skin+0x1C (0x7C8FEF..0x7C9001): boneLimit with split data, numUsedBones without
int EffectiveBoneLimit(const rw::Skin* skin) {
    return HasSplitData(skin) ? skin->boneLimit : skin->numUsedBones;
}

// 0x7C8A00 (+ the hierarchy test of 0x7C9004): does this skin render through the HW path?
bool IsHardwareSkin(const rw::Skin* skin, bool hasHierarchy) {
    return skin && hasHierarchy && g_caps.hwtl && g_caps.vs11ps11 && static_cast<u32>(EffectiveBoneLimit(skin)) <= g_caps.maxBones;
}

//--------------------------------------------------------------------------------------------------
// instancing: librw's skinInstanceCB, then the exe's bone index encoding
//--------------------------------------------------------------------------------------------------
void SkinInstanceCB(rw::Geometry* geo, rw::d3d9::InstanceDataHeader* header, rw::bool32 reinstance) {
    rw::Skin* skin = rw::Skin::get(geo);
    if (reinstance || !skin || !skin->indices) { // (only HW geometry gets here: the dispatcher sends everything else to the CPU route)
        g_origInstance(geo, header, reinstance);
        return;
    }
    // 0x7C90D0 (0x7C9374..0x7C9430): a 256-byte table bone id -> constant register offset, applied to the four indices of every vertex
    u8 table[256] = {};
    if (HasSplitData(skin)) {
        if (static_cast<u32>(static_cast<u8>(skin->remapIndices[0])) >= static_cast<u32>(skin->numBones)) {
            skin->remapIndices[0] = 0; // 0x7C9385..: the exe clamps remap[0] to 0 when it is not a valid index (writes into the skin data)
        }
        for (int i = 0; i < skin->numBones && i < 256; i++) {
            const u8 slot = static_cast<u8>(skin->remapIndices[i]);
            if (slot < 0xFF) {
                table[i] = static_cast<u8>(slot * 3);
            }
        }
    } else if (static_cast<u32>(skin->numBones) <= g_caps.maxBones) { // 0x7C93CA: no split data, the whole hierarchy fits: register = 3 * id (the exe counts hierarchy +4 nodes)
        for (int i = 0; i < skin->numBones && i < 256; i++) {
            table[i] = static_cast<u8>(i * 3);
        }
    } else { // 0x7C93F8: packed by position in the used-bone list
        for (int i = 0; i < skin->numUsedBones; i++) {
            table[skin->usedBones[i]] = static_cast<u8>(i * 3);
        }
    }
    const u32 n = static_cast<u32>(geo->numVertices) * 4;
    std::vector<u8> idx(n);
    for (u32 i = 0; i < n; i++) {
        idx[i] = table[skin->indices[i]];
    }
    u8* saved = skin->indices;
    skin->indices = idx.data();
    g_origInstance(geo, header, reinstance);
    skin->indices = saved;
}

//--------------------------------------------------------------------------------------------------
// the device side of the core's RenderHW (every call maps to what 0x7C8060 / 0x761030 do through the device vtable and the RW state cache)
//--------------------------------------------------------------------------------------------------
struct LibrwDev {
    const rw::d3d9::InstanceDataHeader* header = nullptr;

    void SetConst(unsigned start, const float* data, unsigned count) { rw::d3d::d3ddevice->SetVertexShaderConstantF(start, data, count); } // vtable +0x178
    void SetRenderState(unsigned state, unsigned value) { RwD3D9SetRenderState(state, value); }                                              // 0x7FC2D0
    void VertexAlpha(bool on) { _rwD3D9RenderStateVertexAlphaEnable(on ? TRUE : FALSE); }                                                  // 0x7FE0A0
    void SetIndices(void* ib) { RwD3D9SetIndices(ib); }
    void SetStreams() { _rwD3D9SetStreams(header->vertexStream, header->useOffsets); }                                                      // 0x7FA090
    void SetDecl() { RwD3D9SetVertexDeclaration(header->vertexDeclaration); }
    void SetPSNull() { RwD3D9SetPixelShader(nullptr); }
    void SetTexture(const void* tex) { RwD3D9SetTexture(static_cast<RwTexture*>(const_cast<void*>(tex)), 0); }                             // 0x7FDE70
    void SetTSS(unsigned stage, unsigned type, unsigned value) { RwD3D9SetTextureStageState(stage, type, value); }                         // 0x7FC340
    void SetVS(void* vs) { RwD3D9SetVertexShader(vs); }
    void Flush() { rw::d3d::flushCache(); }                                                                                                  // 0x7FC200
    void DrawIndexed(unsigned prim, unsigned base, unsigned minIdx, unsigned nVerts, unsigned start, unsigned nPrims) {
        RwD3D9DrawIndexedPrimitive(prim, static_cast<RwInt32>(base), minIdx, nVerts, start, nPrims);
    }
    void Draw(unsigned prim, unsigned start, unsigned nPrims) { RwD3D9DrawPrimitive(prim, start, nPrims); }
    void* GetShader(const rwskin::MeshData&, const rwskin::LightKey& k, u8 info[8]) {                                                       // table[2] = 0x761010 -> 0x75EED0
        notsa::skinvs::Key key;
        std::memcpy(key.b, k.b, 4);
        notsa::skinvs::Layout lay{};
        IDirect3DVertexShader9* vs = notsa::skinvs::GetVertexShader(key, lay);
        std::memcpy(info, lay.reg, 8);
        return vs;
    }
};

//--------------------------------------------------------------------------------------------------
// the render callback
//--------------------------------------------------------------------------------------------------
void FillLight(rwskin::LightRef& out, rw::Light* l) {
    out.type          = l->getType();
    out.flags         = l->getFlags();
    out.color[0]      = l->color.red;
    out.color[1]      = l->color.green;
    out.color[2]      = l->color.blue;
    out.radius        = l->radius;
    out.minusCosAngle = l->minusCosAngle;
    if (rw::Frame* f = l->getFrame()) {
        out.ltm = *f->getLTM(); // 0x7F0990
    } else {
        out.ltm.setIdentity();
    }
}

void SkinRenderCB(rw::Atomic* atomic, rw::d3d9::InstanceDataHeader* header) {
    rw::Geometry* geo  = atomic->geometry;
    rw::Skin*     skin = rw::Skin::get(geo);
    rw::HAnimHierarchy* hier = rw::Skin::getHierarchy(atomic);
    if (!IsHardwareSkin(skin, hier != nullptr)) {
        g_origRender(atomic, header);
        return;
    }

    // ---- bones
    static std::vector<float> boneBuf;
    boneBuf.assign(static_cast<size_t>(skin->numBones > 0 ? skin->numBones : 1) * 12, 0.0f);
    static std::vector<const RwMatrix*> nodeLTM;
    rwskin::BoneSource bones;
    bones.numNodes = skin->numBones;
    if (hier) {
        bones.hierFlags    = static_cast<u32>(hier->flags);
        bones.numNodes     = hier->numNodes < skin->numBones ? hier->numNodes : skin->numBones;
        bones.skinToBone   = reinterpret_cast<const RwMatrix*>(skin->inverseMatrices);
        bones.hierMatrices = hier->matrices;
        if (hier->flags & rw::HAnimHierarchy::NOMATRICES) {
            nodeLTM.resize(static_cast<size_t>(bones.numNodes));
            for (int i = 0; i < bones.numNodes; i++) {
                static RwMatrix identity = [] { RwMatrix m; m.setIdentity(); return m; }();
                nodeLTM[i] = hier->nodeInfo[i].frame ? hier->nodeInfo[i].frame->getLTM() : &identity; // 0x7F0990
            }
            bones.nodeLTM = nodeLTM.data();
        }
        if (boneBuf.size() < static_cast<size_t>(bones.numNodes) * 12) {
            boneBuf.resize(static_cast<size_t>(bones.numNodes) * 12, 0.0f);
        }
    } else {
        for (size_t i = 0; i + 11 < boneBuf.size(); i += 12) { // no hierarchy: identity bones (librw's fallback)
            boneBuf[i] = 1.0f; boneBuf[i + 5] = 1.0f; boneBuf[i + 10] = 1.0f;
        }
    }

    // ---- lights of the current world
    RwCamera* camera = RwEngineInstance ? RwEngineInstance->curCamera : nullptr;
    rw::World* world = camera ? camera->world : nullptr;
    static std::vector<rwskin::LightRef> globalLights, localLights;
    globalLights.clear();
    localLights.clear();
    if (world && (geo->flags & rpGEOMETRYLIGHT)) {
        for (rw::LLLink* l = world->globalLights.link.next; l != &world->globalLights.link; l = l->next) {
            rwskin::LightRef r;
            FillLight(r, rw::Light::fromWorld(l));
            globalLights.push_back(r);
        }
        if (geo->flags & rpGEOMETRYNORMALS) {
            for (rw::LLLink* l = world->localLights.link.next; l != &world->localLights.link; l = l->next) {
                rwskin::LightRef r;
                FillLight(r, rw::Light::fromWorld(l));
                localLights.push_back(r);
            }
        }
    }

    // ---- environment of the core
    static float constBuf[256 * 4 + 16];
    static rwskin::WorldXform worldXform;
    static std::vector<rwskin::MeshData> meshes;
    rwskin::Env e;
    e.world          = &worldXform;
    e.atomicLTM      = atomic->getFrame() ? atomic->getFrame()->getLTM() : nullptr;
    if (camera) {
        e.view = reinterpret_cast<const float*>(&camera->devView);
        e.proj = reinterpret_cast<const float*>(&camera->devProj);
        e.camFar = camera->farPlane;
        e.camFogPlane = camera->fogPlane;
    }
    e.geometryFlags   = geo->flags;
    e.numTexCoordSets = static_cast<u32>(geo->numTexCoordSets);
    {
        void* v = nullptr;
        RwRenderStateGet(rwRENDERSTATEFOGENABLE, &v);
        e.fogEnabled = v != nullptr;
        if (e.fogEnabled) {
            RwRenderStateGet(rwRENDERSTATEFOGTYPE, &v);
            e.fogType = static_cast<u32>(reinterpret_cast<std::uintptr_t>(v));
        }
        RwD3D9GetRenderState(D3DRS_FOGDENSITY, &e.fogDensity);
    }
    e.haveWorld       = world != nullptr;
    e.lights.numGlobal = static_cast<int>(globalLights.size());
    e.lights.global    = globalLights.data();
    e.lights.numLocal  = static_cast<int>(localLights.size());
    e.lights.local     = localLights.data();
    if (const RwSphere* s = RpAtomicGetWorldBoundingSphere(atomic)) {
        e.lights.sphere[0] = s->center.x; e.lights.sphere[1] = s->center.y; e.lights.sphere[2] = s->center.z; e.lights.sphere[3] = s->radius;
    }
    e.skin.boneLimit    = EffectiveBoneLimit(skin);
    e.skin.numUsedBones = skin->numUsedBones;
    e.skin.usedBones    = skin->usedBones;
    e.skin.numWeights   = skin->numWeights;
    const bool split    = HasSplitData(skin);
    e.skin.remap        = split ? reinterpret_cast<const u8*>(skin->remapIndices) : nullptr;
    e.skin.rleCount     = split ? reinterpret_cast<const u8*>(skin->rleCount) : nullptr;
    e.skin.rle          = split ? reinterpret_cast<const u8*>(skin->rle) : nullptr;
    e.bones             = bones;
    e.indexBuffer       = header->indexBuffer;
    e.primType          = header->primType;
    e.numMeshes         = static_cast<int>(header->numMeshes);
    e.maxBones          = g_caps.maxBones;
    e.maxConstants      = g_caps.maxConstants;
    e.numLightConstants = [](const rwskin::LightKey& k) {
        notsa::skinvs::Key key;
        std::memcpy(key.b, k.b, 4);
        return notsa::skinvs::NumLightConstants(key);
    };
    e.constBuf = constBuf;
    e.boneBuf  = boneBuf.data();
    e.buildBones = hier != nullptr;

    meshes.resize(header->numMeshes);
    for (u32 i = 0; i < header->numMeshes; i++) {
        const rw::d3d9::InstanceData& inst = header->inst[i];
        rwskin::MeshData&             m    = meshes[i];
        const rw::Material*           mat  = inst.material;
        m                = rwskin::MeshData{};
        m.material       = mat;
        m.texture        = mat->texture;
        std::memcpy(&m.color, &mat->color, 4);
        m.ambient        = mat->surfaceProps.ambient;
        m.diffuse        = mat->surfaceProps.diffuse;
        m.vertexAlpha    = static_cast<u32>(inst.vertexAlpha);
        m.baseIndex      = inst.baseIndex;
        m.numVertices    = inst.numVertices;
        m.startIndex     = inst.startIndex;
        m.numPrimitives  = inst.numPrimitives;
    }
    e.meshes = meshes.data();

    notsa::skinvs::SetFrameStamp(static_cast<std::uint16_t>(RwEngineInstance ? RwEngineInstance->renderFrame : 0));
    LibrwDev dev;
    dev.header = header;
    rwskin::RenderHW(dev, e);
    for (u32 i = 0; i < header->numMeshes; i++) {
        header->inst[i].vertexShader = nullptr; // the exe clears it after the draw
    }
}

//--------------------------------------------------------------------------------------------------
// the dispatcher (replaces librw's `impl.render` of the skin pipeline): HW geometry -> librw's instance + our render callback, the rest -> the CPU route
//--------------------------------------------------------------------------------------------------
void SkinPipeRender(rw::ObjPipeline* p, rw::Atomic* atomic) {
    rw::Geometry* geo = atomic->geometry;
    bool cpu;
    if (geo && geo->instData) {
        cpu = RwShimIsFacadeInstance(geo->instData); // a geometry keeps the route of its first render
    } else {
        cpu = !geo || g_forceCpu || !g_hardwareEnabled || !IsHardwareSkin(rw::Skin::get(geo), rw::Skin::getHierarchy(atomic) != nullptr);
    }
    if (cpu) {
        RwShimSkinCpuRender(atomic);
    } else {
        g_origImplRender(p, atomic);
    }
}

} // namespace

//--------------------------------------------------------------------------------------------------
// Public API
//--------------------------------------------------------------------------------------------------

// patches librw's skin pipeline object: our render callback, an instance callback that encodes the bone indices; call after RwEngineStart (the device exists)
void RwShimSkinPipelineEnsure() {
    rw::ObjPipeline* pipe = rw::skinGlobals.pipelines[rw::PLATFORM_D3D9];
    if (!pipe || !rw::d3d::d3ddevice) {
        return;
    }
    ProbeCaps();
    notsa::skinvs::Init();
    if (g_installed && g_pipe == static_cast<rw::d3d9::ObjPipeline*>(pipe)) {
        return;
    }
    g_pipe           = static_cast<rw::d3d9::ObjPipeline*>(pipe); // (a re-opened engine has a new pipeline object: librw's skinOpen)
    g_origInstance   = g_pipe->instanceCB;
    g_origRender     = g_pipe->renderCB;
    g_origImplRender = g_pipe->impl.render;
    g_pipe->instanceCB   = SkinInstanceCB;
    g_pipe->renderCB     = SkinRenderCB;
    g_pipe->impl.render  = SkinPipeRender;
    g_installed          = true;
    RwShimSkinCpuEnsure();
}

// test hook: new geometries rendered while this is set take the CPU skinning route even where the exe's predicate would choose the vertex-shader path
void RwShimSkinForceCpu(bool force) {
    g_forceCpu = force;
}

// test hook: let geometry that satisfies the exe's HW predicate use the vertex-shader path (off by default, see g_hardwareEnabled)
void RwShimSkinEnableHardware(bool enable) {
    g_hardwareEnabled = enable;
}

// restores librw's callbacks (its skinClose destroys the pipeline object) and releases the cached shaders; call before RwEngineStop / Close
void RwShimSkinPipelineShutdown() {
    if (g_installed && g_pipe) {
        g_pipe->instanceCB  = g_origInstance;
        g_pipe->renderCB    = g_origRender;
        g_pipe->impl.render = g_origImplRender;
    }
    RwShimSkinCpuShutdown();
    g_installed = false;
    g_pipe      = nullptr;
    notsa::skinvs::Shutdown();
}
#endif

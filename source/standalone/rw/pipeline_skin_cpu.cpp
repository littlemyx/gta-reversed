// P2B-24c: the exe's CPU skinning route of the skin pipeline (node body 0x7C7B90, the branch for `skin+0x20 == 0` with a hierarchy) on the RxPipeline façade.
//
// The exe's skin node skins on the CPU when the skin is not drawn by the vertex-shader path (no HW T&L, no VS/PS 1.1, or the skin's bone budget does not fit the
// constant registers; see pipeline_skin.cpp for the predicate 0x7C8A00): every frame it computes the bone matrices (0x7CAA30 / 0x7CA850 -> 0x7CB390), runs one of the
// kernels of pipeline_skin_cpu_core.h over the geometry's morph target 0 (positions and, when the geometry has them, normals) into the LOCKED vertex buffer of the
// resentry, clears the vertices / normals dirty bits (`and byte [geometry+0xC], 0xF9`) and draws with the node's stock callbacks (0x7C85B0: lighting callback
// 0x757400, world matrix 0x7FA520, D3DRS_NORMALIZENORMALS, then the render callback 0x756DF0). A skin without a hierarchy is drawn from the unskinned buffer.
// Here that is a second façade pipeline (stock AtomicAllInOne node) whose render callback skins into the resentry's vertex buffer and then calls the stock render
// callback; the numerics (bone matrices, the eight kernels, the dispatch) are pipeline_skin_cpu_core.h, verified bit for bit by tests/standalone/rw_skin_cpu_oracle_test.cpp.
//
// Deliberate differences / things left out:
//  * the instance callback is the façade's stock one, wrapped so that the normals are always FLOAT3 (the kernels write 3 floats; the exe's declaration for this path
//    has float normals, the façade would pick DEC3N on hardware that supports it). Positions and normals are therefore written straight into the managed vertex
//    buffer of the instance (D3DLOCK_NOSYSLOCK like the façade's own locks), the way the exe writes into its resentry buffer; the other attributes stay as instanced.
//  * the dirty-flag handling is the façade's (RunNode re-instances BEFORE the render callback, the exe after the kernels with bits 1 and 2 cleared): the instance
//    callback writes the bind pose, the kernels then overwrite it in the same call, the result is identical.
//  * the matrix cache of 0x7CAA30 / 0x7CA850 (hierarchy, ~frame stamp, atomic frame -> skip the rebuild) is not kept (see pipeline_skin.cpp: the stamp never advances
//    in the shim, the cache would freeze the pose); rebuilding gives the same bits whenever the cache would hit.
//  * the bone count is min(hierarchy->numNodes, skin->numBones); positions / normals come from librw's Skin (indices uint8[4], weights float[4] per vertex).
//  * the SSE kernels are used (the exe takes them whenever [0xC980A4] != 0, which is every current CPU); RwShimSkinCpuSetSse(false) selects the x87 twins.
//  * vertices of a geometry whose position and normal elements are not in the same vertex stream (a geometry with dynamic usage flags) are skinned for positions only.
// Needs only fakerw + librw + the CRT; excluded from the unity build.
#ifdef NOTSA_RW_LIBRW
#include "fakerw.h"
#include <src/d3d/rwd3dimpl.h>

#include "pipeline_skin_cpu_core.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

void RwShimSkinCpuEnsure();
void RwShimSkinCpuShutdown();
void RwShimSkinCpuRender(rw::Atomic* atomic);
void RwShimSkinCpuSetSse(bool sse);

namespace {

using u8  = std::uint8_t;
using u32 = std::uint32_t;
using Header = rw::d3d9::InstanceDataHeader;

RxPipeline* g_pipe = nullptr;
bool        g_sse  = true; // [0xC980A4]

// the stock instance callback with float normals (see the header comment)
RwBool CpuInstanceCallback(void* object, RxD3D9ResEntryHeader* header, RwBool reinstance) {
    auto&      caps  = rw::d3d::d3d9Globals.caps;
    const auto saved = caps.DeclTypes;
    caps.DeclTypes &= ~static_cast<decltype(caps.DeclTypes)>(D3DDTCAPS_DEC3N);
    const RwBool r = _rpD3D9AtomicDefaultInstanceCallback(object, header, reinstance);
    caps.DeclTypes = saved;
    return r;
}

// the CPU skinning of 0x7C7C49..0x7C7FC3 for one atomic
void SkinIntoBuffer(RwResEntry* entry, rw::Atomic* atomic) {
    rw::Geometry*       geo  = atomic->geometry;
    rw::Skin*           skin = rw::Skin::get(geo);
    rw::HAnimHierarchy* hier = rw::Skin::getHierarchy(atomic);
    if (!skin || !hier || !skin->indices || !skin->weights || geo->numMorphTargets < 1 || geo->numVertices <= 0) {
        return;
    }
    const rw::MorphTarget& mt = geo->morphTargets[0];
    if (!mt.vertices) {
        return;
    }
    Header* header = reinterpret_cast<Header*>(entry + 1);

    // where the position and normal elements live
    rw::d3d9::VertexElement elems[20];
    std::memset(elems, 0, sizeof(elems));
    rw::d3d9::getDeclaration(header->vertexDeclaration, elems);
    const rw::d3d9::VertexElement *pos = nullptr, *nrm = nullptr;
    for (const auto* el = elems; el->type != D3DDECLTYPE_UNUSED; el++) {
        if (el->usage == D3DDECLUSAGE_POSITION && el->usageIndex == 0) {
            pos = el;
        } else if (el->usage == D3DDECLUSAGE_NORMAL && el->usageIndex == 0) {
            nrm = el;
        }
    }
    if (!pos || pos->type != D3DDECLTYPE_FLOAT3) {
        return;
    }
    const float* srcNrm = (geo->flags & rpGEOMETRYNORMALS) ? reinterpret_cast<const float*>(mt.normals) : nullptr;
    if (srcNrm && (!nrm || nrm->stream != pos->stream || nrm->type != D3DDECLTYPE_FLOAT3)) {
        srcNrm = nullptr;
    }
    auto& stream = header->vertexStream[pos->stream];
    if (!stream.vertexBuffer || stream.stride == 0) {
        return;
    }

    // bone matrices (0x7CAA30 / 0x7CA850 -> 0x7CB390)
    rwskincpu::BoneSource bones;
    static std::vector<const RwMatrix*> nodeLTM;
    bones.hierFlags    = static_cast<u32>(hier->flags);
    bones.numNodes     = std::min(hier->numNodes, skin->numBones);
    bones.skinToBone   = reinterpret_cast<const RwMatrix*>(skin->inverseMatrices);
    bones.hierMatrices = hier->matrices;
    static RwMatrix identity = [] { RwMatrix m; m.setIdentity(); return m; }();
    bones.atomicLTM    = atomic->getFrame() ? atomic->getFrame()->getLTM() : &identity; // 0x7F0990
    if (hier->flags & rw::HAnimHierarchy::NOMATRICES) {
        nodeLTM.resize(static_cast<size_t>(bones.numNodes));
        for (int i = 0; i < bones.numNodes; i++) {
            nodeLTM[i] = hier->nodeInfo[i].frame ? hier->nodeInfo[i].frame->getLTM() : &identity;
        }
        bones.nodeLTM = nodeLTM.data();
    }
    static std::vector<__m128> mats; // 16-byte aligned, 4 x __m128 per bone
    mats.assign(static_cast<size_t>(std::max(skin->numBones, 1)) * 4, _mm_setzero_ps());
    float* matBuf = reinterpret_cast<float*>(mats.data());
    if (g_sse) {
        rwskincpu::BuildBonesSse(matBuf, bones);
    } else {
        rwskincpu::BuildBonesX87(matBuf, bones);
    }

    rwskincpu::SkinInput in;
    in.numVertices  = static_cast<u32>(geo->numVertices);
    in.numWeights   = skin->numWeights;
    in.numUsedBones = skin->numUsedBones;
    in.usedBones    = skin->usedBones;
    in.weights      = skin->weights;
    in.indices      = skin->indices;
    in.srcPos       = reinterpret_cast<const float*>(mt.vertices);
    in.srcNrm       = srcNrm;

    u8* base = rw::d3d::lockVertices(stream.vertexBuffer, stream.offset, header->totalNumVertex * stream.stride, D3DLOCK_NOSYSLOCK);
    if (!base) {
        return;
    }
    rwskincpu::SkinVertices(g_sse, in, matBuf, base + pos->offset, srcNrm ? base + nrm->offset : nullptr, stream.stride);
    rw::d3d::unlockVertices(stream.vertexBuffer);
}

// the node's render callback: skin, then the stock draw (0x7C85B0: D3DRS_NORMALIZENORMALS is forced on with hardware T&L while lighting is enabled)
void CpuRenderCallback(RwResEntry* entry, void* object, RwUInt8 type, RwUInt32 flags) {
    SkinIntoBuffer(entry, static_cast<rw::Atomic*>(object));
    DWORD lighting = 0;
    RwD3D9GetRenderState(D3DRS_LIGHTING, &lighting);
    if (lighting && (rw::d3d::d3d9Globals.caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT)) {  // 0x7C85FA: [0xC978E0]
        RwD3D9SetRenderState(D3DRS_NORMALIZENORMALS, TRUE);
    }
    _rpD3D9AtomicDefaultRenderCallback(entry, object, type, flags);
}

} // namespace

void RwShimSkinCpuEnsure() {
    static bool busy = false; // RxPipelineCreate calls RwShimPipelineEnsure, which calls RwShimSkinPipelineEnsure, which calls this
    if (busy || g_pipe) {
        return;
    }
    busy = true;
    struct Unbusy { ~Unbusy() { busy = false; } } unbusy;
    RxPipeline*       pipe   = RxPipelineCreate();
    RxNodeDefinition* def    = RxNodeDefinitionGetD3D9AtomicAllInOne();
    RxLockedPipe*     locked = pipe ? RxPipelineLock(pipe) : nullptr;
    if (!locked || !RxLockedPipeAddFragment(locked, nullptr, def, nullptr) || !RxLockedPipeUnlock(locked)) {
        if (pipe) {
            RxPipelineDestroy(pipe);
        }
        return;
    }
    RxPipelineNode* node = RxPipelineFindNodeByName(pipe, def->name, nullptr, nullptr);
    if (!node) {
        RxPipelineDestroy(pipe);
        return;
    }
    RxD3D9AllInOneSetInstanceCallBack(node, CpuInstanceCallback);
    RxD3D9AllInOneSetRenderCallBack(node, CpuRenderCallback);
    g_pipe = pipe;
}

void RwShimSkinCpuShutdown() {
    if (g_pipe) {
        RxPipelineDestroy(g_pipe);
        g_pipe = nullptr;
    }
}

// renders `atomic` (a skinned atomic) through the CPU route; the geometry's instance data becomes the façade's resentry
void RwShimSkinCpuRender(rw::Atomic* atomic) {
    if (g_pipe) {
        g_pipe->render(atomic);
    }
}

// selects the kernel set ([0xC980A4]: SSE available); the default is SSE, false selects the x87 twins
void RwShimSkinCpuSetSse(bool sse) {
    g_sse = sse;
}
#endif

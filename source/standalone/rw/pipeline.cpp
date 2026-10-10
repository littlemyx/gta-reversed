// P2B-07: RxPipeline façade over librw's d3d9::ObjPipeline + the stock D3D9 "AtomicAllInOne" node and its default callbacks.
//
// What the game needs (source/game_sa/Pipelines/*): RxPipelineCreate / Lock / RxLockedPipeAddFragment / Unlock / FindNodeByName / Destroy,
// RxNodeDefinitionGetD3D9AtomicAllInOne()->name, RxD3D9AllInOne{Get,Set}{Instance,Reinstance,Render}CallBack on the found node, `atomic->pipeline = pipe`,
// `pipe->pluginID/pluginData`, and inside the render callbacks the casts `(RxD3D9ResEntryHeader*)(resEntry + 1)` / `(RxD3D9InstanceData*)(header + 1)`.
// Design:
//  * An RxPipeline* IS a rw::ObjPipeline* (fakerw.h typedef): RxPipelineImpl derives from d3d9::ObjPipeline and replaces impl.{instance,uninstance,render}
//    so that librw's Atomic::defaultRenderCB -> getPipeline()->render(atomic) runs the node body below. The node's four callbacks live in the
//    RxPipelineNode embedded in the pipeline.
//  * The instanced data of a geometry is ONE block [RwResEntry][d3d9::InstanceDataHeader][d3d9::InstanceData x numMeshes] (the layout the game's casts
//    expect; header->inst points at the mesh array). geometry->instData points at the header and the pointer is registered in g_entries, so the
//    NativeData plugin destructor (patched here) releases it together with the geometry. RW kept the entry in geometry->repEntry (morph targets == 1) or
//    atomic->repEntry; the shim always uses the geometry (the interpolator does not exist in librw, morph target 0 is the only one instanced).
//  * The default pipeline of the D3D9 platform is replaced by a façade with the stock callbacks (RwShimPipelineEnsure), so atomics without ->pipeline
//    render through the same fixed-function path as RW's did. librw's shader pipelines (skin, matfx) are still reached through atomic->pipeline.
//
// Ported from the exe (addresses): node body _rwD3D9AtomicAllInOneNode 0x7575F0 (init 0x757890: instance 0x7578C0, reinstance 0x758270, lighting 0x757400,
// render 0x756DF0), resentry creation 0x756960 (index buffer fill, strip->list on too many degenerates, triangle sort, numPrimitives), resentry destroy
// 0x4C9990, strip->list 0x756830, triangle comparator 0x7567A0, element comparator 0x758240, vertex data helpers 0x752AD0 (DEC3N 0x753060..), 0x754AE0
// (prelit ARGB + alpha detection), 0x7544E0 (texcoords), lights 0x756260 / 0x756600, world matrix 0x7FA520.
// Deliberate differences (behaviour-neutral on a HAL device):
//  * vertex buffers are standalone MANAGED buffers (stream.offset = 0, managed = 0, useOffsets = 0): the exe sub-allocates single-stream geometry from
//    shared "managed" pools (so baseIndex = minVert + offset/stride = minVert here); reinstance locks omit D3DLOCK_DISCARD (the buffers are not dynamic);
//  * no resource-arena LRU (geometry lifetime owns the entry), no morph interpolation, no tangents (rpD3D9GEOMETRYUSAGE_CREATETANGENTS: SA never sets it),
//    local (point / spot) lights are not enumerated (librw's world has no sectors; SA lights its atomics with ambient + directional lights only);
//  * RxLockedPipeAddFragment accepts exactly one node (the AllInOne node; the car pipeline passes no terminating NULL, so the rest of the varargs is ignored).
// Needs only fakerw + librw + the CRT; excluded from the unity build.
#ifdef NOTSA_RW_LIBRW
#include "fakerw.h"
#include <src/d3d/rwd3dimpl.h>

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <unordered_set>
#include <vector>

namespace rw { namespace d3d9 { void freeInstanceData(Geometry* geometry); } } // librw d3d9.cpp (external linkage, no header)

namespace {

using u32    = std::uint32_t;
using Header = rw::d3d9::InstanceDataHeader;
using Inst   = rw::d3d9::InstanceData;

constexpr const char* kNodeName = "nodeD3D9AtomicAllInOne.csl";   // exe 0x8D6318
constexpr u32 kMultiStreamExcludedPipeline = 0x12E;               // exe 0x7579A3: pipelines with this plugin id keep a single stream
constexpr u32 kLockAll = 0xFFF;                                   // "everything locked"

RxNodeDefinition g_nodeDef = { kNodeName };

// Registry of the headers created here (geometry->instData may also hold librw's own native data, which must not be touched)
std::unordered_set<const void*>& Entries() {
    static std::unordered_set<const void*> s;
    return s;
}

RwResEntry*           ResEntryOf(const Header* h) { return reinterpret_cast<RwResEntry*>(const_cast<Header*>(h)) - 1; }
Header*               HeaderOf(const RwResEntry* r) { return reinterpret_cast<Header*>(const_cast<RwResEntry*>(r) + 1); }
bool                  IsOurs(const rw::InstanceDataHeader* h) { return h && Entries().count(h) != 0; }

//--------------------------------------------------------------------------------------------------
// Resentry lifetime (exe 0x4C9990 destroy notify)
//--------------------------------------------------------------------------------------------------
void ReleaseBuffers(Header* h) {
    if (h->vertexDeclaration) {
        rw::d3d9::destroyVertexDeclaration(h->vertexDeclaration);
        h->vertexDeclaration = nullptr;
    }
    if (h->indexBuffer) {
        rw::d3d::destroyIndexBuffer(h->indexBuffer);
        h->indexBuffer = nullptr;
    }
    for (auto& s : h->vertexStream) {
        if (s.vertexBuffer) {
            rw::d3d::destroyVertexBuffer(s.vertexBuffer);
        }
        s = {};
    }
}

void ResEntryDestroy(RwResEntry* entry) {
    Header* h = HeaderOf(entry);
    if (auto* geo = static_cast<rw::Geometry*>(entry->owner)) {
        if (geo->instData == h) {
            geo->instData = nullptr;
        }
    }
    ReleaseBuffers(h);
    Entries().erase(h);
    rwFree(entry);
}

// librw's NativeData plugin destructor frees geometry->instData as ITS header layout (rwFree(header->inst); rwFree(header)): ours is one block
rw::Destructor g_origNativeDataDtor = nullptr;

void* NativeDataDtor(void* object, rw::int32 offset, rw::int32 size) {
    auto* geo = static_cast<rw::Geometry*>(object);
    if (IsOurs(geo->instData)) {
        ResEntryDestroy(ResEntryOf(static_cast<Header*>(geo->instData)));
        return object;
    }
    return g_origNativeDataDtor ? g_origNativeDataDtor(object, offset, size) : object;
}

void HookNativeDataPlugin() {
    for (rw::LLLink* l = rw::Geometry::s_plglist.plugins.link.next; l != &rw::Geometry::s_plglist.plugins.link; l = l->next) {
        rw::Plugin* p = LLLinkGetData(l, rw::Plugin, inParentList);
        if (p->id == rw::ID_NATIVEDATA) {
            if (p->destructor != NativeDataDtor) {
                g_origNativeDataDtor = p->destructor;
                p->destructor        = NativeDataDtor;
            }
            return;
        }
    }
}

//--------------------------------------------------------------------------------------------------
// Small D3D helpers
//--------------------------------------------------------------------------------------------------
u32 D3DPrimTypeOfMesh(u32 meshFlags) { // exe table at the mesh plugin (0x758974..): header flags -> RW prim type -> 0x874FEC -> D3DPRIMITIVETYPE
    switch (meshFlags & 0xFF) {
    case 0x00: return D3DPT_TRIANGLELIST;
    case 0x01: return D3DPT_TRIANGLESTRIP;
    case 0x02: return D3DPT_TRIANGLEFAN;
    case 0x04: return D3DPT_LINELIST;
    case 0x08: return D3DPT_LINESTRIP;
    case 0x10: return D3DPT_POINTLIST;
    case 0x20: return D3DPT_TRIANGLESTRIP;
    default:   return 0;
    }
}

u32 NumPrimitives(u32 primType, u32 numIndices) { // exe 0x756C9B jump table
    switch (primType) {
    case D3DPT_LINELIST:      return numIndices / 2;
    case D3DPT_LINESTRIP:     return numIndices - 1;
    case D3DPT_TRIANGLELIST:  return numIndices / 3;
    case D3DPT_TRIANGLESTRIP:
    case D3DPT_TRIANGLEFAN:   return numIndices - 2;
    default:                  return 0;
    }
}

// exe 0x7567A0: triangles are compared by their sorted vertex triple (min, mid, max)
int CompareTriangles(const void* pa, const void* pb) {
    const auto* a = static_cast<const std::uint16_t*>(pa);
    const auto* b = static_cast<const std::uint16_t*>(pb);
    std::uint16_t ta[3] = {a[0], a[1], a[2]}, tb[3] = {b[0], b[1], b[2]};
    std::sort(ta, ta + 3);
    std::sort(tb, tb + 3);
    for (int i = 0; i < 3; i++) {
        if (ta[i] != tb[i]) {
            return int(ta[i]) - int(tb[i]);
        }
    }
    return 0;
}

// exe 0x756830: strip -> list without the degenerate triangles (odd triangles are written reversed); returns the number of indices written
u32 StripToList(std::uint16_t* dst, const std::uint16_t* src, u32 numIndices, u32 minVert) {
    u32 written = 0;
    if (numIndices < 3) {
        return 0;
    }
    for (u32 i = 0; i + 2 < numIndices; i++) {
        const std::uint16_t a = src[i], b = src[i + 1], c = src[i + 2];
        if (a == b || a == c || b == c) {
            continue;
        }
        const std::uint16_t ra = std::uint16_t(a - minVert), rb = std::uint16_t(b - minVert), rc = std::uint16_t(c - minVert);
        if (i & 1) {
            dst[2] = ra; dst[1] = rb; dst[0] = rc;
        } else {
            dst[0] = ra; dst[1] = rb; dst[2] = rc;
        }
        dst += 3;
        written += 3;
    }
    return written;
}

//--------------------------------------------------------------------------------------------------
// Resentry creation (exe 0x756960): header + instance data for every mesh, index buffer fill, then the instance callback (reinstance = FALSE)
//--------------------------------------------------------------------------------------------------
RwResEntry* CreateResEntry(RpAtomic* atomic, RpGeometry* geo, const rw::MeshHeader* mh, RxD3D9AllInOneInstanceCallBack instanceCB) {
    const u32 numMeshes = mh->numMeshes;
    const size_t payload = sizeof(Header) + numMeshes * sizeof(Inst);
    auto* entry = static_cast<RwResEntry*>(rwMalloc(sizeof(RwResEntry) + payload, rw::MEMDUR_EVENT | rw::ID_GEOMETRY));
    if (!entry) {
        return nullptr;
    }
    std::memset(entry, 0, sizeof(RwResEntry) + payload);
    entry->size          = static_cast<RwInt32>(payload);
    entry->owner         = geo;
    entry->ownerRef      = nullptr;
    entry->destroyNotify = ResEntryDestroy;

    Header* header = HeaderOf(entry);
    Inst*   insts  = reinterpret_cast<Inst*>(header + 1);
    header->platform     = rw::PLATFORM_D3D9;
    header->serialNumber = mh->serialNum;
    header->numMeshes    = numMeshes;
    header->inst         = insts;
    Entries().insert(header);
    geo->instData = header;

    const rw::Mesh* meshes = const_cast<rw::MeshHeader*>(mh)->getMeshes();
    const bool unindexed = (mh->flags & 0x100) != 0;

    bool         clampToList = false;
    std::uint16_t* indices   = nullptr;
    if (!unindexed) {
        u32 total = 0;
        for (u32 i = 0; i < numMeshes; i++) {
            total += meshes[i].numIndices;
        }
        header->totalNumIndex = total;
        if (total) {
            // a strip geometry with more than 3 indices per triangle (lots of degenerates) is stored as a trilist (exe 0x756A61)
            if ((mh->flags & 0xFF) == 0x01) {
                const u32 limit = static_cast<u32>(geo->numTriangles) * 3;
                if (total > limit) {
                    clampToList           = true;
                    header->totalNumIndex = limit;
                }
            }
            header->indexBuffer = rw::d3d::createIndexBuffer(header->totalNumIndex * 2, false);
            if (header->indexBuffer) {
                indices = rw::d3d::lockIndices(header->indexBuffer, 0, 0, 0);
            }
        }
    }
    header->primType = clampToList ? u32(D3DPT_TRIANGLELIST) : D3DPrimTypeOfMesh(mh->flags);

    u32 runningVerts = 0, runningIndex = 0;
    std::uint16_t* dst = indices;
    for (u32 i = 0; i < numMeshes; i++) {
        const rw::Mesh& mesh = meshes[i];
        Inst&           inst = insts[i];
        u32 n = mesh.numIndices;

        if (unindexed) {
            inst.numVertices = n;
            inst.minVert     = runningVerts;
            runningVerts += n;
        } else if (n == 0) {
            inst.numVertices = 0;
            inst.minVert     = 0;
        } else {
            u32 mn = 0xFFFFFFFFu, mx = 0;
            for (u32 j = 0; j < n; j++) {
                const u32 v = mesh.indices[j];
                mn = std::min(mn, v);
                mx = std::max(mx, v);
            }
            inst.numVertices = mx - mn + 1;
            inst.minVert     = mn;
        }
        inst.material    = mesh.material;
        inst.vertexAlpha = 0;
        inst.vertexShader = nullptr;

        if (dst) {
            inst.startIndex = runningIndex;
            std::uint16_t* meshDst = dst;
            if (clampToList) {
                n = StripToList(meshDst, mesh.indices, n, inst.minVert);
            } else if (inst.minVert != 0) {
                for (u32 j = 0; j < n; j++) {
                    meshDst[j] = std::uint16_t(mesh.indices[j] - inst.minVert);
                }
            } else {
                std::memcpy(meshDst, mesh.indices, n * 2);
            }
            inst.numIndex = n;
            if (header->primType == D3DPT_TRIANGLELIST) {
                std::qsort(meshDst, n / 3, 6, CompareTriangles);
            }
            dst += n;
            runningIndex += n;
        } else {
            inst.startIndex = 0;
            inst.numIndex   = 0;
        }
        inst.numPrimitives = NumPrimitives(header->primType, n);
        inst.baseIndex     = 0;
    }
    if (indices) {
        rw::d3d::unlockIndices(header->indexBuffer);
    }

    if (instanceCB && !instanceCB(atomic, header, FALSE)) {
        ResEntryDestroy(entry);
        return nullptr;
    }
    return entry;
}

//--------------------------------------------------------------------------------------------------
// Stock instance callback (exe 0x7578C0)
//--------------------------------------------------------------------------------------------------
int CompareElements(const void* pa, const void* pb) { // exe 0x758240: stream, then offset
    const auto* a = static_cast<const rw::d3d9::VertexElement*>(pa);
    const auto* b = static_cast<const rw::d3d9::VertexElement*>(pb);
    if (a->stream != b->stream) {
        return int(a->stream) - int(b->stream);
    }
    return int(a->offset) - int(b->offset);
}

const rw::d3d9::VertexElement* FindElement(const rw::d3d9::VertexElement* elems, u32 usage, u32 usageIndex) {
    for (const auto* e = elems; e->type != D3DDECLTYPE_UNUSED; e++) {
        if (e->usage == usage && e->usageIndex == usageIndex) {
            return e;
        }
    }
    return nullptr;
}

bool DeclTypesSupportDec3N() { // exe tests the DeclTypes caps dword (0xC9BFEC) & D3DDTCAPS_DEC3N
    return (rw::d3d::d3d9Globals.caps.DeclTypes & D3DDTCAPS_DEC3N) != 0;
}

// exe 0x752AD0 (types FLOAT3 and DEC3N, the only ones the stock vertex layouts use)
void InstV3d(u32 type, std::uint8_t* dst, const rw::V3d* src, u32 numVertices, u32 stride) {
    if (type == D3DDECLTYPE_FLOAT3) {
        for (u32 i = 0; i < numVertices; i++, dst += stride) {
            std::memcpy(dst, &src[i], 12);
        }
    } else if (type == D3DDECLTYPE_DEC3N) {
        for (u32 i = 0; i < numVertices; i++, dst += stride) {
            const long x = std::lrintf(src[i].x * 511.0f);   // fistp, round to nearest even
            const long y = std::lrintf(src[i].y * 511.0f);
            const long z = std::lrintf(src[i].z * 511.0f);
            *reinterpret_cast<u32*>(dst) = ((u32(z) & 0x3FF) << 20) | ((u32(y) & 0x3FF) << 10) | (u32(x) & 0x3FF);
        }
    }
}

// exe 0x754AE0: RGBA bytes -> D3DCOLOR (ARGB); returns TRUE when some alpha != 0xFF
bool InstColor(std::uint8_t* dst, const rw::RGBA* src, u32 numVertices, u32 stride) {
    std::uint8_t andAlpha = 0xFF;
    for (u32 i = 0; i < numVertices; i++, dst += stride) {
        const rw::RGBA& c = src[i];
        *reinterpret_cast<u32*>(dst) = (u32(c.alpha) << 24) | (u32(c.red) << 16) | (u32(c.green) << 8) | u32(c.blue);
        andAlpha &= c.alpha;
    }
    return andAlpha != 0xFF;
}

RwBool InstanceCallback(void* object, RxD3D9ResEntryHeader* header, RwBool reinstance) {
    auto* atomic = static_cast<RpAtomic*>(object);
    RpGeometry* geo = atomic->geometry;
    const u32 geoFlags = geo->flags;
    u32 usage = RpD3D9GeometryGetUsageFlags(geo);
    const u32 numTex = static_cast<u32>(geo->numTexCoordSets);
    const auto& caps = rw::d3d::d3d9Globals.caps;

    rw::d3d9::VertexElement elems[20];
    std::memset(elems, 0, sizeof(elems));
    u32 numStreams = 1;

    if (!reinstance) {
        header->totalNumVertex = static_cast<u32>(geo->numVertices);
        for (auto& s : header->vertexStream) { // exe 0x7578F8: drop leftovers of a previous instance
            if (s.vertexBuffer) {
                rw::d3d::destroyVertexBuffer(s.vertexBuffer);
            }
            s = {};
        }

        // one stream, or (dynamic usage / morphing on hardware T&L) two: stream 0 = dynamic attributes, stream numStreams-1 = static ones
        bool multi = false;
        if (geo->numMorphTargets > 1 || (usage & rpD3D9GEOMETRYUSAGE_DYNAMICMASK)) {
            if (caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT) {
                const rw::Pipeline* pipe = atomic->pipeline;
                if (!(pipe && pipe->pluginID == kMultiStreamExcludedPipeline)) {
                    if (geo->numMorphTargets > 1) {
                        usage |= rpD3D9GEOMETRYUSAGE_DYNAMICPOSITIONS | rpD3D9GEOMETRYUSAGE_DYNAMICNORMALS;
                    }
                    multi = true;
                }
            }
        }
        numStreams = multi ? 2 : 1;

        u32 n = 0;
        const u32 staticStream = numStreams - 1;
        auto addElement = [&](bool dynamic, u32 size, std::uint8_t type, std::uint8_t usg, std::uint8_t usgIdx, u32 geometryFlagBit) {
            const u32 s = dynamic ? 0 : staticStream;
            auto& st = header->vertexStream[s];
            elems[n].stream     = static_cast<std::uint16_t>(s);
            elems[n].offset     = static_cast<std::uint16_t>(st.stride);
            elems[n].type       = type;
            elems[n].method     = 0;
            elems[n].usage      = usg;
            elems[n].usageIndex = usgIdx;
            n++;
            st.stride += size;
            st.geometryFlags = static_cast<std::uint16_t>(st.geometryFlags | geometryFlagBit);
        };
        addElement((usage & rpD3D9GEOMETRYUSAGE_DYNAMICPOSITIONS) != 0, 12, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_POSITION, 0, rpGEOMETRYLOCKVERTICES);
        if (geoFlags & rpGEOMETRYNORMALS) {
            const bool dec3n = DeclTypesSupportDec3N();
            addElement((usage & rpD3D9GEOMETRYUSAGE_DYNAMICNORMALS) != 0, dec3n ? 4 : 12, dec3n ? D3DDECLTYPE_DEC3N : D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_NORMAL, 0,
                       rpGEOMETRYLOCKNORMALS);
        }
        if (geoFlags & rpGEOMETRYPRELIT) {
            addElement((usage & rpD3D9GEOMETRYUSAGE_DYNAMICPRELIT) != 0, 4, D3DDECLTYPE_D3DCOLOR, D3DDECLUSAGE_COLOR, 0, rpGEOMETRYLOCKPRELIGHT);
        }
        for (u32 i = 0; i < numTex && i < 8; i++) {
            addElement((usage & (rpD3D9GEOMETRYUSAGE_DYNAMICTEXCOORDS1 << i)) != 0, 8, D3DDECLTYPE_FLOAT2, D3DDECLUSAGE_TEXCOORD, static_cast<std::uint8_t>(i),
                       rpGEOMETRYLOCKTEXCOORDS1 << i);
        }
        if ((usage & rpD3D9GEOMETRYUSAGE_CREATETANGENTS) && numTex > 0) {
            // exe 0x757B6B appends a TANGENT element (stream 0 when any texcoord set is dynamic); the data is built by 0x754E20 -- not ported (SA never
            // sets rpD3D9GEOMETRYUSAGE_CREATETANGENTS), the element is NOT created so the declaration stays consistent with the data.
        }
        while (numStreams > 1 && header->vertexStream[numStreams - 1].stride == 0) {
            numStreams--;
        }

        std::qsort(elems, n, sizeof(elems[0]), CompareElements);
        elems[n] = {0xFF, 0, D3DDECLTYPE_UNUSED, 0, 0, 0};
        header->vertexDeclaration = rw::d3d9::createVertexDeclaration(elems);
        if (!header->vertexDeclaration) {
            return FALSE;
        }
        for (u32 s = 0; s < numStreams; s++) {
            auto& st = header->vertexStream[s];
            st.vertexBuffer = rw::d3d::createVertexBuffer(header->totalNumVertex * st.stride, 0, false);
            if (!st.vertexBuffer) {
                return FALSE;
            }
            st.offset      = 0;
            st.managed     = 0;
            st.dynamicLock = 0;
        }
        header->useOffsets = 0;
        for (u32 m = 0; m < header->numMeshes; m++) { // exe 0x757C8A: baseIndex = minVert (+ stream offset / stride, which is 0 here)
            header->inst[m].baseIndex = header->inst[m].minVert;
        }
    } else {
        rw::d3d9::getDeclaration(header->vertexDeclaration, elems);
        numStreams = 0;
        for (const auto* e = elems; e->type != D3DDECLTYPE_UNUSED; e++) {
            numStreams = std::max<u32>(numStreams, u32(e->stream) + 1);
        }
    }

    // lock the streams whose attributes changed (all of them for a fresh instance)
    std::uint8_t* locked[2] = {nullptr, nullptr};
    u32 lockMask = reinstance ? geo->lockedSinceInst : kLockAll;
    for (u32 s = 0; s < numStreams; s++) {
        auto& st = header->vertexStream[s];
        if (lockMask == kLockAll || (lockMask & st.geometryFlags)) {
            locked[s] = rw::d3d::lockVertices(st.vertexBuffer, st.offset, header->totalNumVertex * st.stride, D3DLOCK_NOSYSLOCK);
            lockMask |= st.geometryFlags;
        }
    }

    const rw::MorphTarget& mt0 = geo->morphTargets[0];
    const u32 numVerts = header->totalNumVertex;

    if ((lockMask & rpGEOMETRYLOCKVERTICES) && mt0.vertices) {
        if (const auto* e = FindElement(elems, D3DDECLUSAGE_POSITION, 0)) {
            if (locked[e->stream]) {
                InstV3d(e->type, locked[e->stream] + e->offset, mt0.vertices, numVerts, header->vertexStream[e->stream].stride);
            }
        }
    }
    if ((geoFlags & rpGEOMETRYNORMALS) && (lockMask & rpGEOMETRYLOCKNORMALS) && mt0.normals) {
        if (const auto* e = FindElement(elems, D3DDECLUSAGE_NORMAL, 0)) {
            if (locked[e->stream]) {
                InstV3d(e->type, locked[e->stream] + e->offset, mt0.normals, numVerts, header->vertexStream[e->stream].stride);
            }
        }
    }
    if (geoFlags & rpGEOMETRYPRELIT) {
        if ((lockMask & rpGEOMETRYLOCKPRELIGHT) && geo->colors) {
            if (const auto* e = FindElement(elems, D3DDECLUSAGE_COLOR, 0)) {
                if (locked[e->stream]) {
                    const u32 stride = header->vertexStream[e->stream].stride;
                    for (u32 m = 0; m < header->numMeshes; m++) {
                        Inst& inst = header->inst[m];
                        inst.vertexAlpha = InstColor(locked[e->stream] + inst.minVert * stride + e->offset, geo->colors + inst.minVert, inst.numVertices, stride);
                    }
                }
            }
        }
    } else if (!reinstance) {
        for (u32 m = 0; m < header->numMeshes; m++) {
            header->inst[m].vertexAlpha = 0;
        }
    }
    if (numTex > 0 && (lockMask & 0xFF0)) {
        for (u32 i = 0; i < numTex && i < 8; i++) {
            if (!(lockMask & (rpGEOMETRYLOCKTEXCOORDS1 << i)) || !geo->texCoords[i]) {
                continue;
            }
            if (const auto* e = FindElement(elems, D3DDECLUSAGE_TEXCOORD, i)) {
                if (locked[e->stream]) {
                    const u32 stride = header->vertexStream[e->stream].stride;
                    std::uint8_t* dst = locked[e->stream] + e->offset;
                    for (u32 v = 0; v < numVerts; v++, dst += stride) {
                        std::memcpy(dst, &geo->texCoords[i][v], 8);
                    }
                }
            }
        }
    }

    for (u32 s = 0; s < numStreams; s++) {
        if (locked[s]) {
            rw::d3d::unlockVertices(header->vertexStream[s].vertexBuffer);
        }
    }
    return TRUE;
}

// exe 0x758270
RwBool ReinstanceCallback(void* object, RwResEntry* resEntry, RxD3D9AllInOneInstanceCallBack instanceCallback) {
    auto* atomic = static_cast<RpAtomic*>(object);
    RpGeometry* geo = atomic->geometry;
    if (geo->numMorphTargets == 1) {
        if (!instanceCallback) {
            return TRUE;
        }
        return instanceCallback(object, HeaderOf(resEntry), TRUE) ? TRUE : FALSE;
    }
    // (the interpolator-dirty bit does not exist in librw)
    if (geo->lockedSinceInst != 0) {
        const auto saved = geo->lockedSinceInst;
        geo->lockedSinceInst = static_cast<std::uint16_t>(saved | rpGEOMETRYLOCKVERTICES | rpGEOMETRYLOCKNORMALS);
        InstanceCallback(object, HeaderOf(resEntry), TRUE); // exe 0x7582A9: calls the stock instance callback (0x7578C0) directly, not the node's
        geo->lockedSinceInst = saved;
    }
    return TRUE;
}

//--------------------------------------------------------------------------------------------------
// Stock lighting callback (exe 0x757400, light manager 0x756260 / 0x756600)
//--------------------------------------------------------------------------------------------------
std::vector<int> g_curLights, g_prevLights;

void LightsEnable(bool enable) { // exe 0x756600 (type argument is always 1 = directional)
    if (!enable) {
        RwD3D9SetRenderState(D3DRS_LIGHTING, FALSE);
        return;
    }
    const u32 maxLights = rw::d3d::d3d9Globals.caps.MaxActiveLights;
    if (maxLights && g_curLights.size() > maxLights) {
        g_curLights.resize(maxLights);
    }
    for (const int prev : g_prevLights) {
        if (std::find(g_curLights.begin(), g_curLights.end(), prev) == g_curLights.end()) {
            RwD3D9EnableLight(prev, FALSE);
        }
    }
    if (!g_curLights.empty()) {
        for (const int cur : g_curLights) {
            RwD3D9EnableLight(cur, TRUE);
        }
        g_prevLights = g_curLights;
    } else {
        g_prevLights.clear();
    }
    g_curLights.clear();
    RwD3D9SetRenderState(D3DRS_LIGHTING, TRUE);
}

void DirectionalLightEnable(rw::Light* light) { // exe 0x756260: D3DLIGHT9 {DIRECTIONAL, diffuse = colour (alpha 1), direction = frame at}
    D3DLIGHT9 l{};
    l.Type      = D3DLIGHT_DIRECTIONAL;
    l.Diffuse.r = light->color.red;
    l.Diffuse.g = light->color.green;
    l.Diffuse.b = light->color.blue;
    l.Diffuse.a = 1.0f;
    l.Specular.a = 1.0f; // exe 0x755E50..0x755EF0: the static D3DLIGHT9 (0xC92648) is initialised with alpha 1.0 for diffuse, specular and ambient
    l.Ambient.a  = 1.0f;
    if (rw::Frame* f = light->getFrame()) {
        const rw::Matrix* m = f->getLTM();
        l.Direction = {m->at.x, m->at.y, m->at.z};
    }
    const int idx = static_cast<int>(g_curLights.size());
    RwD3D9SetLight(idx, &l);
    g_curLights.push_back(idx);
}

void LightingCallback(void* object) {
    auto* atomic = static_cast<RpAtomic*>(object);
    const u32 flags = atomic->geometry->flags;
    rw::World* world = (RwEngineInstance && RwEngineInstance->curCamera) ? RwEngineInstance->curCamera->world : nullptr;
    if (!(flags & rpGEOMETRYLIGHT) || !world) {
        LightsEnable(false);
        return;
    }
    AmbientSaturated = {0.0f, 0.0f, 0.0f, 1.0f};
    bool any = false;
    for (rw::LLLink* l = world->globalLights.link.next; l != &world->globalLights.link; l = l->next) {
        rw::Light* light = rw::Light::fromWorld(l);
        if (!light || !(light->getFlags() & rw::Light::LIGHTATOMICS)) {
            continue;
        }
        if (light->getType() == rw::Light::DIRECTIONAL) {
            if (!(flags & rpGEOMETRYNORMALS)) {
                continue;
            }
            DirectionalLightEnable(light);
        } else {
            AmbientSaturated.red += light->color.red;
            AmbientSaturated.green += light->color.green;
            AmbientSaturated.blue += light->color.blue;
        }
        any = true;
    }
    RwD3D9SetRenderState(D3DRS_AMBIENT, any ? 0xFFFFFFFFu : 0u);
    // exe 0x7574E0: with normals the lights linked into the world sectors the atomic touches follow (point / spot); none exist in librw's world
    LightsEnable(any);
}

//--------------------------------------------------------------------------------------------------
// Stock render callback (exe 0x756DF0)
//--------------------------------------------------------------------------------------------------
bool RasterHasAlpha(const RwTexture* tex) { // exe 0x4C9EA0: the D3D9 raster extension's alpha flag
    if (!tex || !tex->raster) {
        return false;
    }
    return GETD3DRASTEREXT(tex->raster)->hasAlpha;
}

void DrawMesh(const Header* header, const Inst* inst) { // exe 0x7572C3 / 0x757043: vertex shader (cached), flush, Draw(Indexed)Primitive
    RwD3D9SetVertexShader(inst->vertexShader);
    if (header->indexBuffer) {
        RwD3D9DrawIndexedPrimitive(header->primType, inst->baseIndex, 0, inst->numVertices, inst->startIndex, inst->numPrimitives);
    } else {
        RwD3D9DrawPrimitive(header->primType, inst->baseIndex, inst->numPrimitives);
    }
}

void RenderCallback(RwResEntry* resEntry, void* object, RwUInt8 type, RwUInt32 flags) {
    const Header* header = HeaderOf(resEntry);
    const Inst*   insts  = header->inst;

    RwD3D9SetPixelShader(nullptr);
    _rwD3D9EnableClippingIfNeeded(object, type);
    if (header->indexBuffer) {
        RwD3D9SetIndices(header->indexBuffer);
    }
    _rwD3D9SetStreams(header->vertexStream, header->useOffsets);
    RwD3D9SetVertexDeclaration(header->vertexDeclaration);

    DWORD lighting = 0;
    RwD3D9GetRenderState(D3DRS_LIGHTING, &lighting);

    if (!lighting && !(flags & rxGEOMETRY_PRELIT)) {
        // no lighting and no vertex colours: flat TFACTOR (black) silhouette, modulated by the texture alpha of alpha textures (exe 0x756EF6)
        DWORD dither = 0, shade = 0;
        RwD3D9GetRenderState(D3DRS_DITHERENABLE, &dither);
        RwD3D9GetRenderState(D3DRS_SHADEMODE, &shade);
        _rwD3D9RenderStateVertexAlphaEnable(FALSE);
        RwD3D9SetRenderState(D3DRS_TEXTUREFACTOR, 0xFF000000u);
        RwD3D9SetRenderState(D3DRS_DITHERENABLE, FALSE);
        RwD3D9SetRenderState(D3DRS_SHADEMODE, D3DSHADE_FLAT);
        RwD3D9SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG2);
        RwD3D9SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        RwD3D9SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);
        RwD3D9SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2);
        RwD3D9SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
        RwD3D9SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_TFACTOR);
        RwD3D9SetTexture(nullptr, 0);

        bool textured = false;
        for (u32 i = 0; i < header->numMeshes; i++) {
            const Inst* inst = &insts[i];
            const RwTexture* tex = inst->material ? inst->material->texture : nullptr;
            if ((flags & (rxGEOMETRY_TEXTURED | rxGEOMETRY_TEXTURED2)) && tex && RasterHasAlpha(tex)) {
                RwD3D9SetTexture(const_cast<RwTexture*>(tex), 0);
                if (!textured) {
                    textured = true;
                    RwD3D9SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
                }
            } else if (textured) {
                textured = false;
                RwD3D9SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2);
                RwD3D9SetTexture(nullptr, 0);
            }
            DrawMesh(header, inst);
        }
        RwD3D9SetRenderState(D3DRS_DITHERENABLE, dither);
        RwD3D9SetRenderState(D3DRS_SHADEMODE, shade);
        return;
    }

    // lit (or vertex coloured) geometry. cfg: bit 0 = texture on stage 0, bit 1 = TFACTOR modulation (stage 0, or stage 1 when textured)
    bool vertexAlphaOn = _rwD3D9RenderStateVertexAlphaIsEnabled() != FALSE;
    u32  lastCfg = 0x80000000u;
    for (u32 i = 0; i < header->numMeshes; i++) {
        const Inst*  inst = &insts[i];
        RpMaterial*  mat  = inst->material;
        const RwRGBA* color = &mat->color;

        if (color->alpha == 0xFF && inst->vertexAlpha == 0) {
            if (vertexAlphaOn) {
                vertexAlphaOn = false;
                _rwD3D9RenderStateVertexAlphaEnable(FALSE);
            }
        } else if (!vertexAlphaOn) {
            vertexAlphaOn = true;
            _rwD3D9RenderStateVertexAlphaEnable(TRUE);
        }

        u32 cfg = 0;
        if (lighting) {
            RwD3D9SetSurfaceProperties(&mat->surfaceProps, color, flags);
        } else if ((flags & rxGEOMETRY_MODULATE) && *reinterpret_cast<const u32*>(color) != 0xFFFFFFFFu) {
            RwD3D9SetRenderState(D3DRS_TEXTUREFACTOR, (u32(color->alpha) << 24) | (u32(color->red) << 16) | (u32(color->green) << 8) | u32(color->blue));
            cfg = 2;
        }
        if (mat->texture && (flags & (rxGEOMETRY_TEXTURED | rxGEOMETRY_TEXTURED2))) {
            RwD3D9SetTexture(mat->texture, 0);
            cfg |= 1;
        } else {
            RwD3D9SetTexture(nullptr, 0);
        }

        if (cfg != lastCfg) {
            if (cfg & 1) {
                if (!(lastCfg & 1)) {
                    RwD3D9SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
                    RwD3D9SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
                    RwD3D9SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
                    RwD3D9SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
                    RwD3D9SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
                    RwD3D9SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
                }
                if (cfg & 2) {
                    RwD3D9SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_MODULATE);
                    RwD3D9SetTextureStageState(1, D3DTSS_COLORARG1, D3DTA_CURRENT);
                    RwD3D9SetTextureStageState(1, D3DTSS_COLORARG2, D3DTA_TFACTOR);
                    RwD3D9SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
                    RwD3D9SetTextureStageState(1, D3DTSS_ALPHAARG1, D3DTA_CURRENT);
                    RwD3D9SetTextureStageState(1, D3DTSS_ALPHAARG2, D3DTA_TFACTOR);
                } else if (lastCfg == 3) {
                    RwD3D9SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
                    RwD3D9SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
                }
            } else {
                if (cfg & 2) {
                    RwD3D9SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
                    RwD3D9SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
                    RwD3D9SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);
                    RwD3D9SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
                    RwD3D9SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
                    RwD3D9SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_TFACTOR);
                } else {
                    RwD3D9SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG2);
                    RwD3D9SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
                    RwD3D9SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2);
                    RwD3D9SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
                }
                if (lastCfg == 3) {
                    RwD3D9SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
                    RwD3D9SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
                }
            }
            lastCfg = cfg;
        }
        DrawMesh(header, inst);
    }
    if (lastCfg == 3) {
        RwD3D9SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
        RwD3D9SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
    }
}

} // namespace

//--------------------------------------------------------------------------------------------------
// RxPipeline / node objects
//--------------------------------------------------------------------------------------------------
struct RxPipelineNode {
    RxNodeDefinition*                  def;
    RxD3D9AllInOneInstanceCallBack     instanceCB;
    RxD3D9AllInOneReinstanceCallBack   reinstanceCB;
    RxD3D9AllInOneLightingCallBack     lightingCB;
    RxD3D9AllInOneRenderCallBack       renderCB;
};

namespace {

struct RxPipelineImpl : rw::d3d9::ObjPipeline {
    RxPipelineNode node;
    bool           locked;
    unsigned       numNodes;
};

RxPipelineImpl* g_defaultPipe = nullptr;

void PipeRender(rw::ObjPipeline* p, rw::Atomic* atomic);
void PipeInstance(rw::ObjPipeline* p, rw::Atomic* atomic);
void PipeUninstance(rw::ObjPipeline* p, rw::Atomic* atomic);

RxPipelineImpl* AsImpl(const rw::ObjPipeline* p) {
    return (p && p->impl.render == PipeRender) ? static_cast<RxPipelineImpl*>(const_cast<rw::ObjPipeline*>(p)) : nullptr;
}

void InitNode(RxPipelineNode& n) { // exe 0x757890
    n.def          = &g_nodeDef;
    n.instanceCB   = _rpD3D9AtomicDefaultInstanceCallback;
    n.reinstanceCB = _rpD3D9AtomicDefaultReinstanceCallback;
    n.lightingCB   = _rpD3D9AtomicDefaultLightingCallback;
    n.renderCB     = _rpD3D9AtomicDefaultRenderCallback;
}

RxPipelineImpl* NewPipeline() {
    auto* p = new RxPipelineImpl();
    p->init();
    p->impl.instance   = PipeInstance;
    p->impl.uninstance = PipeUninstance;
    p->impl.render     = PipeRender;
    p->locked          = false;
    p->numNodes        = 0;
    InitNode(p->node);
    return p;
}

// World matrix as D3DTS_WORLD (exe 0x7FA520: NULL or identity-flagged matrix = identity)
void SetWorldTransform(const rw::Matrix* m) {
    D3DMATRIX d{};
    if (!m || (m->flags & rw::Matrix::IDENTITY)) {
        d._11 = d._22 = d._33 = d._44 = 1.0f;
    } else {
        d._11 = m->right.x; d._12 = m->right.y; d._13 = m->right.z;
        d._21 = m->up.x;    d._22 = m->up.y;    d._23 = m->up.z;
        d._31 = m->at.x;    d._32 = m->at.y;    d._33 = m->at.z;
        d._41 = m->pos.x;   d._42 = m->pos.y;   d._43 = m->pos.z;
        d._44 = 1.0f;
    }
    RwD3D9SetTransform(D3DTS_WORLD, &d);
}

// exe 0x757757: lighting, world matrix, D3DRS_NORMALIZENORMALS for non-unit matrices, then the node's render callback
void RenderGeometry(RxPipelineNode* node, RpAtomic* atomic, RwResEntry* entry) {
    if (node->lightingCB) {
        node->lightingCB(atomic);
    }
    rw::Frame* frame = atomic->getFrame();
    const rw::Matrix* ltm = frame ? frame->getLTM() : nullptr;
    SetWorldTransform(ltm);

    DWORD lighting = 0;
    RwD3D9GetRenderState(D3DRS_LIGHTING, &lighting);
    bool normalize = false;
    if (lighting && ltm && !(ltm->flags & 0x20001)) {
        auto len2Bad = [](const rw::V3d& v) {
            const float l = v.y * v.y + v.x * v.x + v.z * v.z;
            return l > 1.1f || l < 0.9f;
        };
        normalize = len2Bad(ltm->right) || len2Bad(ltm->up) || len2Bad(ltm->at);
    }
    RwD3D9SetRenderState(D3DRS_NORMALIZENORMALS, normalize ? TRUE : FALSE);
    if (node->renderCB) {
        node->renderCB(entry, atomic, 1, atomic->geometry->flags);
    }
}

// exe 0x7575F0
RwBool RunNode(RxPipelineNode* node, RpAtomic* atomic) {
    RpGeometry* geo = atomic->geometry;
    if (!geo || geo->numVertices <= 0 || !geo->meshHeader || geo->meshHeader->numMeshes == 0) {
        return TRUE;
    }
    const RwUInt32 flags = geo->flags;
    if (flags & rpGEOMETRYNATIVE) {
        return TRUE; // pre-instanced librw native data has no RwResEntry wrapper; SA uses generic geometry only
    }
    const rw::MeshHeader* mh = geo->meshHeader;

    Header* header = IsOurs(geo->instData) ? static_cast<Header*>(geo->instData) : nullptr;
    if (!header && geo->instData) {
        rw::d3d9::freeInstanceData(geo); // librw's own instance (another D3D9 pipeline rendered this geometry before)
    }
    RwResEntry* entry = header ? ResEntryOf(header) : nullptr;
    if (header) {
        if (header->serialNumber != mh->serialNum) {
            ResEntryDestroy(entry);
            header = nullptr;
            entry  = nullptr;
        } else if (geo->lockedSinceInst != 0 || geo->numMorphTargets != 1) {
            if (node->reinstanceCB && !node->reinstanceCB(atomic, entry, node->instanceCB)) {
                ResEntryDestroy(entry);
                return FALSE;
            }
            geo->lockedSinceInst = 0;
        }
    }
    if (!header) {
        HookNativeDataPlugin();
        entry = CreateResEntry(atomic, geo, mh, node->instanceCB);
        if (!entry) {
            return FALSE;
        }
        geo->lockedSinceInst = 0;
    }
    if (!(flags & rpGEOMETRYNATIVEINSTANCE)) {
        RenderGeometry(node, atomic, entry);
    }
    return TRUE;
}

void PipeRender(rw::ObjPipeline* p, rw::Atomic* atomic) {
    if (RxPipelineImpl* impl = AsImpl(p)) {
        RunNode(&impl->node, atomic);
    }
}

// rw::Atomic::instance() marks the geometry NATIVE afterwards (= pre-instanced stream data), which the lazy instancing of this node must never see:
// nothing calls it in the game, so it is a no-op here (the node instances on the first render).
void PipeInstance(rw::ObjPipeline*, rw::Atomic*) {}

void PipeUninstance(rw::ObjPipeline*, rw::Atomic* atomic) {
    if (atomic->geometry && IsOurs(atomic->geometry->instData)) {
        ResEntryDestroy(ResEntryOf(static_cast<Header*>(atomic->geometry->instData)));
    }
}

} // namespace

//--------------------------------------------------------------------------------------------------
// Public API
//--------------------------------------------------------------------------------------------------
// P2B-23: the MatFX AllInOne pipeline (pipeline_matfx.cpp); units that link pipeline.cpp without it get no-ops through /alternatename.
void RwShimMatFXPipelineEnsure();
void RwShimMatFXPipelineShutdown();
void NotsaPipelineNoMatFX() {}
#pragma comment(linker, "/alternatename:?RwShimMatFXPipelineEnsure@@YAXXZ=?NotsaPipelineNoMatFX@@YAXXZ")
#pragma comment(linker, "/alternatename:?RwShimMatFXPipelineShutdown@@YAXXZ=?NotsaPipelineNoMatFX@@YAXXZ")
// P2B-24b: the skin HW render path (pipeline_skin.cpp), same mechanism
void RwShimSkinPipelineEnsure();
void RwShimSkinPipelineShutdown();
void NotsaPipelineNoSkin() {}
#pragma comment(linker, "/alternatename:?RwShimSkinPipelineEnsure@@YAXXZ=?NotsaPipelineNoSkin@@YAXXZ")
#pragma comment(linker, "/alternatename:?RwShimSkinPipelineShutdown@@YAXXZ=?NotsaPipelineNoSkin@@YAXXZ")

void RwShimPipelineEnsure() {
    HookNativeDataPlugin();
    if (!g_defaultPipe) {
        g_defaultPipe = NewPipeline();
        g_defaultPipe->numNodes = 1;
    }
    if (rw::engine && rw::engine->driver[rw::PLATFORM_D3D9]) {
        rw::engine->driver[rw::PLATFORM_D3D9]->defaultPipeline = g_defaultPipe;
    }
    RwShimMatFXPipelineEnsure(); // atomics with the MatFX flag render through the exe's fixed-function MatFX AllInOne
    RwShimSkinPipelineEnsure();  // skinned atomics render through the exe's HW skin path (vs_1_1)
}

void RwShimPipelineShutdown() {
    RwShimSkinPipelineShutdown();
    RwShimMatFXPipelineShutdown();
    if (rw::engine && rw::engine->driver[rw::PLATFORM_D3D9] && rw::engine->driver[rw::PLATFORM_D3D9]->defaultPipeline == g_defaultPipe) {
        rw::engine->driver[rw::PLATFORM_D3D9]->defaultPipeline = rw::engine->dummyDefaultPipeline;
    }
    delete g_defaultPipe;
    g_defaultPipe = nullptr;
    g_curLights.clear();
    g_prevLights.clear();
}

// P2B-24c: does `instData` (geometry->instData) hold a resentry created by this façade (as opposed to librw's own native instance block)? Used by the skin dispatcher.
bool RwShimIsFacadeInstance(const void* instData) {
    return instData && Entries().count(instData) != 0;
}

RxNodeDefinition* RxNodeDefinitionGetD3D9AtomicAllInOne() {
    return &g_nodeDef;
}

RxPipeline* RxPipelineCreate(void) {
    RwShimPipelineEnsure();
    return NewPipeline();
}

RwBool RxPipelineDestroy(RxPipeline* pipeline) {
    RxPipelineImpl* impl = AsImpl(pipeline);
    if (!impl || impl == g_defaultPipe) {
        return FALSE;
    }
    delete impl;
    return TRUE;
}

RxLockedPipe* RxPipelineLock(RxPipeline* pipeline) {
    RxPipelineImpl* impl = AsImpl(pipeline);
    if (!impl || impl->locked) {
        return nullptr;
    }
    impl->locked = true;
    return pipeline;
}

RxLockedPipe* RxLockedPipeAddFragment(RxLockedPipe* pipeline, RwUInt32* firstIndex, RxNodeDefinition* nodeDef0, ...) {
    RxPipelineImpl* impl = AsImpl(pipeline);
    if (!impl || !impl->locked || nodeDef0 != &g_nodeDef || impl->numNodes != 0) {
        return nullptr;
    }
    if (firstIndex) {
        *firstIndex = 0;
    }
    impl->numNodes = 1;
    return pipeline;
}

RxPipeline* RxLockedPipeUnlock(RxLockedPipe* pipeline) {
    RxPipelineImpl* impl = AsImpl(pipeline);
    if (!impl || !impl->locked) {
        return nullptr;
    }
    impl->locked = false;
    return impl->numNodes ? pipeline : nullptr;
}

RxPipelineNode* RxPipelineFindNodeByName(RxPipeline* pipeline, const RwChar* name, RxPipelineNode* start, RwInt32* nodeIndex) {
    RxPipelineImpl* impl = AsImpl(pipeline);
    if (!impl || !impl->numNodes || !name || start == &impl->node || std::strcmp(name, kNodeName) != 0) {
        return nullptr;
    }
    if (nodeIndex) {
        *nodeIndex = 0;
    }
    return &impl->node;
}

RxD3D9AllInOneInstanceCallBack   RxD3D9AllInOneGetInstanceCallBack(RxPipelineNode* node) { return node->instanceCB; }
RxD3D9AllInOneReinstanceCallBack RxD3D9AllInOneGetReinstanceCallBack(RxPipelineNode* node) { return node->reinstanceCB; }
RxD3D9AllInOneLightingCallBack   RxD3D9AllInOneGetLightingCallBack(RxPipelineNode* node) { return node->lightingCB; }
RxD3D9AllInOneRenderCallBack     RxD3D9AllInOneGetRenderCallBack(RxPipelineNode* node) { return node->renderCB; }
void RxD3D9AllInOneSetInstanceCallBack(RxPipelineNode* node, RxD3D9AllInOneInstanceCallBack callback) { node->instanceCB = callback; }
void RxD3D9AllInOneSetReinstanceCallBack(RxPipelineNode* node, RxD3D9AllInOneReinstanceCallBack callback) { node->reinstanceCB = callback; }
void RxD3D9AllInOneSetLightingCallBack(RxPipelineNode* node, RxD3D9AllInOneLightingCallBack callback) { node->lightingCB = callback; }
void RxD3D9AllInOneSetRenderCallBack(RxPipelineNode* node, RxD3D9AllInOneRenderCallBack callback) { node->renderCB = callback; }

RwBool _rpD3D9AtomicDefaultInstanceCallback(void* object, RxD3D9ResEntryHeader* resEntryHeader, RwBool reinstance) {
    return InstanceCallback(object, resEntryHeader, reinstance);
}
RwBool _rpD3D9AtomicDefaultReinstanceCallback(void* object, RwResEntry* resEntry, RxD3D9AllInOneInstanceCallBack instanceCallback) {
    return ReinstanceCallback(object, resEntry, instanceCallback);
}
void _rpD3D9AtomicDefaultLightingCallback(void* object) { LightingCallback(object); }
void _rpD3D9AtomicDefaultRenderCallback(RwResEntry* resEntry, void* object, RwUInt8 type, RwUInt32 flags) { RenderCallback(resEntry, object, type, flags); }

// Port of the clip decision at the top of the stock render callback (exe 0x756E1D..0x756E5B): D3DRS_CLIPPING (136) is on unless the object's world
// bounding sphere lies completely inside the current camera's frustum (0x7FAD30: every plane (n.c - d) <= -r, == librw's SPHEREINSIDE). `type` 1 is an
// atomic (the only kind the shim renders; the exe's other branch tests a world sector box with 0x7FAD90).
void _rwD3D9EnableClippingIfNeeded(void* object, RwUInt32 type) {
    bool inside = false;
    RwCamera* camera = RwEngineInstance ? RwEngineInstance->curCamera : nullptr;
    if (camera && object && type == 1) {
        const RwSphere* sphere = RpAtomicGetWorldBoundingSphere(static_cast<RpAtomic*>(object));
        if (sphere) {
            // exe 0x7FAD30 verbatim: for each of the 6 planes, dist = n.c - d must satisfy !(dist > -r) (NaN counts as inside); librw's
            // frustumTestSphere differs only for a negative radius (it returns OUTSIDE when r < dist)
            inside = true;
            for (const auto& fp : camera->frustumPlanes) {
                const float dist = fp.plane.normal.y * sphere->center.y + fp.plane.normal.x * sphere->center.x + fp.plane.normal.z * sphere->center.z - fp.plane.distance;
                if (dist > -sphere->radius) {
                    inside = false;
                    break;
                }
            }
        }
    }
    RwD3D9SetRenderState(D3DRS_CLIPPING, inside ? FALSE : TRUE);
}

// exe 0x7FE0A0 / 0x7FE190 are RW's rwRENDERSTATEVERTEXALPHAENABLE setter / getter
void _rwD3D9RenderStateVertexAlphaEnable(RwBool enable) {
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, reinterpret_cast<void*>(static_cast<std::uintptr_t>(enable ? 1 : 0)));
}

RwBool _rwD3D9RenderStateVertexAlphaIsEnabled() {
    void* v = nullptr;
    RwRenderStateGet(rwRENDERSTATEVERTEXALPHAENABLE, &v);
    return v != nullptr ? TRUE : FALSE;
}
#endif

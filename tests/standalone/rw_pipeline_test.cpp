// P2B-07 unit test: RxPipeline façade + the stock D3D9 AtomicAllInOne callbacks (source/standalone/rw/pipeline.cpp) on librw/D3D9.
//  1. RxPipelineCreate / Lock / AddFragment / Unlock / FindNodeByName / Destroy, node definition name, callback get/set round trips
//  2. default pipeline (atomic->pipeline == NULL renders through the façade): instanced data layout (RwResEntry + header + meshes contiguous), header /
//     mesh fields against an independent recomputation from the mesh header, vertex declaration, vertex buffer contents (position, DEC3N/float
//     normals, ARGB prelit, texcoords), vertexAlpha detection, triangle sorting of trilists, strip geometry, dynamic usage -> 2 streams
//  3. rendering to the camera raster with known material colours / prelit colours (unlit black silhouette, prelit, TFACTOR modulation, ambient light),
//     read back with GetRenderTargetData
//  4. custom callbacks (the game's usage): instance (wrapping the default through RxD3D9AllInOneGetInstanceCallBack), reinstance, render: call counts,
//     arguments, (RxD3D9ResEntryHeader*)(resEntry + 1) / (header + 1) casts, re-instance after RpGeometryLock(PRELIGHT), full re-instance after LOCKPOLYGONS,
//     resource accounting (vertex/index buffers, declarations released with the geometry)
//  5. optional argv[1..] = SA DFFs (vgsnbuild07.dff, infernus.dff ...): every atomic is instanced and rendered, the same invariants are checked
// Needs a D3D9 HAL adapter (works under Wine/wined3d). Exit code 0 = all checks passed.
#include "fakerw.h"
#include <src/d3d/rwd3dimpl.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>
#include <vector>

void NotsaRwRenderState_OnEngineStarted();
static int g_fail = 0, g_pass = 0;
static bool g_verbose = false;
#define CHECK(c) do { const bool ok_ = !!(c); if (ok_) ++g_pass; else ++g_fail; if (!ok_ || g_verbose) std::printf("%-4s %s\n", ok_ ? "ok" : "FAIL", #c); } while (0)
#define CHECKV(c, fmt, ...) do { const bool ok_ = !!(c); if (ok_) ++g_pass; else ++g_fail; if (!ok_ || g_verbose) std::printf("%-4s %s   " fmt "\n", ok_ ? "ok" : "FAIL", #c, __VA_ARGS__); } while (0)

namespace d9 = rw::d3d9;
using Header = rw::d3d9::InstanceDataHeader;
using Inst   = rw::d3d9::InstanceData;

static IDirect3DDevice9* g_dev = nullptr;
static bool g_dec3n = false;

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
static bool NearC(unsigned px, int r, int g, int b, int tol = 3) {
    return std::abs(int((px >> 16) & 0xFF) - r) <= tol && std::abs(int((px >> 8) & 0xFF) - g) <= tol && std::abs(int(px & 0xFF) - b) <= tol;
}

// ---------------------------------------------------------------------------------------------------------------------------------
// scene helpers
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

static RpMaterial* MakeMaterial(RwRGBA c) {
    RpMaterial* m = RpMaterialCreate();
    m->color = c;
    return m;
}

// triangle (-2,-2,5) (2,-2,5) (0,2,5) covering the screen centre; prelit colour per vertex, normals towards the camera
static RpGeometry* MakeTri(RwUInt32 flags, RpMaterial* mat, RwRGBA prelit = { 255, 255, 255, 255 }) {
    RpGeometry* g = RpGeometryCreate(3, 1, flags | rpGEOMETRYPOSITIONS);
    RwV3d* v = RpMorphTargetGetVertices(RpGeometryGetMorphTarget(g, 0));
    v[0] = { -2, -2, 5 }; v[1] = { 2, -2, 5 }; v[2] = { 0, 2, 5 };
    if (flags & rpGEOMETRYNORMALS) {
        RwV3d* n = g->morphTargets[0].normals;
        for (int i = 0; i < 3; i++) n[i] = { 0, 0, -1 };
    }
    if (flags & rpGEOMETRYPRELIT) {
        for (int i = 0; i < 3; i++) g->colors[i] = prelit;
    }
    if (g->numTexCoordSets > 0) {
        g->texCoords[0][0] = { 0, 0 }; g->texCoords[0][1] = { 1, 0 }; g->texCoords[0][2] = { 0.5f, 1 };
    }
    RpTriangle* t = RpGeometryGetTriangles(g);
    RpGeometryTriangleSetVertexIndices(g, &t[0], 0, 1, 2);
    RpGeometryTriangleSetMaterial(g, &t[0], mat);
    RpGeometryUnlock(g);
    return g;
}

static RpAtomic* MakeAtomic(RpGeometry* g, RwFrame** outFrame = nullptr) {
    RpAtomic* a = RpAtomicCreate();
    RwFrame* f = RwFrameCreate();
    RpAtomicSetFrame(a, f);
    RpAtomicSetGeometry(a, g, 0);
    if (outFrame) *outFrame = f;
    return a;
}
static void FreeAtomic(RpAtomic* a) {
    RwFrame* f = RpAtomicGetFrame(a);
    RpAtomicSetFrame(a, nullptr);
    RpAtomicDestroy(a);
    if (f) RwFrameDestroy(f);
}

static void BeginScene(RwRGBA clear = { 0, 0, 255, 255 }) {
    RwCameraBeginUpdate(g_cam);
    RwCameraClear(g_cam, &clear, rwCAMERACLEARIMAGE | rwCAMERACLEARZ);
}
static void EndScene() { RwCameraEndUpdate(g_cam); }

static Header* HeaderOf(RpGeometry* g) { return static_cast<Header*>(g->instData); }

// ---------------------------------------------------------------------------------------------------------------------------------
// independent recomputation of the instance data
struct Tri { std::uint16_t a, b, c; bool operator<(const Tri& o) const { return std::tie(a, b, c) < std::tie(o.a, o.b, o.c); } };
static Tri Sorted(std::uint16_t a, std::uint16_t b, std::uint16_t c) {
    std::uint16_t v[3] = { a, b, c };
    std::sort(v, v + 3);
    return { v[0], v[1], v[2] };
}

static void CheckInstanceInvariants(RpAtomic* atomic, const char* what, bool checkVB = true) {
    RpGeometry* geo = atomic->geometry;
    Header* h = HeaderOf(geo);
    CHECKV(h != nullptr, "%s: instData present", what);
    if (!h) return;
    RwResEntry* entry = reinterpret_cast<RwResEntry*>(h) - 1;
    const rw::MeshHeader* mh = geo->meshHeader;
    const rw::Mesh* meshes = const_cast<rw::MeshHeader*>(mh)->getMeshes();
    CHECKV(h->platform == rw::PLATFORM_D3D9 && h->inst == reinterpret_cast<Inst*>(h + 1), "%s: platform / inst == (header + 1)", what);
    CHECKV(entry->owner == geo && entry->size == (RwInt32)(sizeof(Header) + h->numMeshes * sizeof(Inst)) && entry->destroyNotify, "%s: resentry owner/size/notify", what);
    CHECKV(h->numMeshes == mh->numMeshes && h->serialNumber == mh->serialNum, "%s: numMeshes %u serial %u", what, h->numMeshes, h->serialNumber);
    const bool strip = (mh->flags & 0xFF) == 1;
    const bool clamped = strip && h->primType == D3DPT_TRIANGLELIST;
    CHECKV(h->primType == (strip ? (clamped ? D3DPT_TRIANGLELIST : D3DPT_TRIANGLESTRIP) : D3DPT_TRIANGLELIST), "%s: primType %u", what, h->primType);
    CHECKV(h->totalNumVertex == (unsigned)geo->numVertices && h->indexBuffer != nullptr && h->vertexDeclaration != nullptr && h->useOffsets == 0, "%s: totals", what);

    unsigned sumIdx = 0, runningStart = 0;
    bool allMeshesOk = true, trisOk = true, alphaOk = true;
    std::uint16_t* ib = checkVB ? rw::d3d::lockIndices(h->indexBuffer, 0, 0, D3DLOCK_READONLY) : nullptr;
    for (unsigned i = 0; i < h->numMeshes; i++) {
        const Inst& in = h->inst[i];
        const rw::Mesh& m = meshes[i];
        unsigned mn = 0xFFFFFFFFu, mx = 0;
        for (unsigned j = 0; j < m.numIndices; j++) { mn = std::min<unsigned>(mn, m.indices[j]); mx = std::max<unsigned>(mx, m.indices[j]); }
        if (!m.numIndices) { mn = 0; mx = 0; }
        const unsigned expectVerts = m.numIndices ? mx - mn + 1 : 0;
        bool ok = in.minVert == mn && in.numVertices == expectVerts && in.material == m.material && in.baseIndex == in.minVert && in.vertexShader == nullptr;
        if (!clamped) {
            ok = ok && in.numIndex == m.numIndices && in.startIndex == runningStart &&
                 in.numPrimitives == (strip ? (m.numIndices >= 2 ? m.numIndices - 2 : 0) : m.numIndices / 3);
        } else {
            ok = ok && in.startIndex == runningStart && in.numPrimitives == in.numIndex / 3;
        }
        allMeshesOk = allMeshesOk && ok;
        if (ib && !strip) {   // the triangle SET of a list mesh is unchanged by the instancing sort
            std::multiset<std::uint32_t> want, got;
            for (unsigned j = 0; j + 2 < m.numIndices; j += 3) {
                const Tri t = Sorted(m.indices[j], m.indices[j + 1], m.indices[j + 2]);
                want.insert((t.a << 20) ^ (t.b << 10) ^ t.c ^ (t.a * 2654435761u));
            }
            for (unsigned j = 0; j + 2 < in.numIndex; j += 3) {
                const Tri t = Sorted(ib[in.startIndex + j] + in.minVert, ib[in.startIndex + j + 1] + in.minVert, ib[in.startIndex + j + 2] + in.minVert);
                got.insert((t.a << 20) ^ (t.b << 10) ^ t.c ^ (t.a * 2654435761u));
            }
            trisOk = trisOk && want == got;
            // sorted by their sorted vertex triple (exe 0x7567A0)
            Tri prev{ 0, 0, 0 };
            for (unsigned j = 0; j + 2 < in.numIndex; j += 3) {
                const Tri t = Sorted(ib[in.startIndex + j], ib[in.startIndex + j + 1], ib[in.startIndex + j + 2]);
                if (j && t < prev) trisOk = false;
                prev = t;
            }
        }
        sumIdx += in.numIndex;
        runningStart += in.numIndex;
        // vertexAlpha == some prelit alpha != 255 in the mesh's vertex range
        if (geo->flags & rpGEOMETRYPRELIT) {
            bool any = false;
            for (unsigned v = in.minVert; v < in.minVert + in.numVertices; v++) any = any || geo->colors[v].alpha != 255;
            alphaOk = alphaOk && ((in.vertexAlpha != 0) == any);
        } else {
            alphaOk = alphaOk && in.vertexAlpha == 0;
        }
    }
    if (ib) rw::d3d::unlockIndices(h->indexBuffer);
    CHECKV(allMeshesOk, "%s: per-mesh fields (%u meshes)", what, h->numMeshes);
    CHECKV(trisOk, "%s: list triangle set preserved and sorted", what);
    CHECKV(alphaOk, "%s: vertexAlpha", what);
    CHECKV(sumIdx == h->totalNumIndex, "%s: sum(numIndex) %u == totalNumIndex %u", what, sumIdx, h->totalNumIndex);

    // declaration
    d9::VertexElement el[20]{};
    const unsigned n = d9::getDeclaration(h->vertexDeclaration, el);   // includes the END element
    const d9::VertexElement* pos = nullptr; const d9::VertexElement* nrm = nullptr; const d9::VertexElement* col = nullptr; const d9::VertexElement* uv = nullptr;
    unsigned maxStream = 0;
    for (unsigned i = 0; i + 1 < n; i++) {
        maxStream = std::max<unsigned>(maxStream, el[i].stream);
        if (el[i].usage == D3DDECLUSAGE_POSITION) pos = &el[i];
        if (el[i].usage == D3DDECLUSAGE_NORMAL) nrm = &el[i];
        if (el[i].usage == D3DDECLUSAGE_COLOR) col = &el[i];
        if (el[i].usage == D3DDECLUSAGE_TEXCOORD && el[i].usageIndex == 0) uv = &el[i];
    }
    CHECKV(pos && pos->type == D3DDECLTYPE_FLOAT3, "%s: position element", what);
    CHECKV((nrm != nullptr) == ((geo->flags & rpGEOMETRYNORMALS) != 0) && (!nrm || nrm->type == (g_dec3n ? D3DDECLTYPE_DEC3N : D3DDECLTYPE_FLOAT3)), "%s: normal element (dec3n %d)", what, (int)g_dec3n);
    CHECKV((col != nullptr) == ((geo->flags & rpGEOMETRYPRELIT) != 0) && (!col || col->type == D3DDECLTYPE_D3DCOLOR), "%s: colour element", what);
    CHECKV((uv != nullptr) == (geo->numTexCoordSets > 0) && (!uv || uv->type == D3DDECLTYPE_FLOAT2), "%s: texcoord element", what);
    bool strideOk = true;
    for (unsigned s = 0; s <= maxStream; s++) {
        unsigned sz = 0;
        for (unsigned i = 0; i + 1 < n; i++) if (el[i].stream == s) sz = std::max<unsigned>(sz, el[i].offset + (el[i].type == D3DDECLTYPE_FLOAT3 ? 12 : el[i].type == D3DDECLTYPE_FLOAT2 ? 8 : 4));
        strideOk = strideOk && h->vertexStream[s].stride == sz && h->vertexStream[s].vertexBuffer && h->vertexStream[s].offset == 0;
    }
    CHECKV(strideOk, "%s: stream strides match the declaration (%u stream(s))", what, maxStream + 1);

    // vertex buffer contents
    if (checkVB && pos && geo->numVertices > 0) {
        auto& st = h->vertexStream[pos->stream];
        std::uint8_t* vb = rw::d3d::lockVertices(st.vertexBuffer, 0, 0, D3DLOCK_READONLY);
        if (vb) {
            bool posOk = true, nrmOk = true, colOk = true, uvOk = true;
            const int step = std::max(1, geo->numVertices / 64);
            for (int v = 0; v < geo->numVertices; v += step) {
                float p[3]; std::memcpy(p, vb + v * st.stride + pos->offset, 12);
                const RwV3d& s = geo->morphTargets[0].vertices[v];
                posOk = posOk && p[0] == s.x && p[1] == s.y && p[2] == s.z;
            }
            if (nrm) {
                auto& ns = h->vertexStream[nrm->stream];
                std::uint8_t* nvb = ns.vertexBuffer == st.vertexBuffer ? vb : rw::d3d::lockVertices(ns.vertexBuffer, 0, 0, D3DLOCK_READONLY);
                for (int v = 0; nvb && v < geo->numVertices; v += step) {
                    const RwV3d& s = geo->morphTargets[0].normals[v];
                    if (nrm->type == D3DDECLTYPE_DEC3N) {
                        const std::uint32_t w = *(std::uint32_t*)(nvb + v * ns.stride + nrm->offset);
                        auto sx = [](std::uint32_t b) { int x = b & 0x3FF; if (x & 0x200) x |= ~0x3FF; return x; };
                        nrmOk = nrmOk && sx(w) == std::lrintf(s.x * 511.f) && sx(w >> 10) == std::lrintf(s.y * 511.f) && sx(w >> 20) == std::lrintf(s.z * 511.f);
                    } else {
                        float p[3]; std::memcpy(p, nvb + v * ns.stride + nrm->offset, 12);
                        nrmOk = nrmOk && p[0] == s.x && p[1] == s.y && p[2] == s.z;
                    }
                }
                if (nvb && nvb != vb) rw::d3d::unlockVertices(ns.vertexBuffer);
            }
            if (col) {
                auto& cs = h->vertexStream[col->stream];
                std::uint8_t* cvb = cs.vertexBuffer == st.vertexBuffer ? vb : rw::d3d::lockVertices(cs.vertexBuffer, 0, 0, D3DLOCK_READONLY);
                for (unsigned i = 0; cvb && i < h->numMeshes; i++) {
                    const Inst& in = h->inst[i];
                    for (unsigned v = in.minVert; v < in.minVert + in.numVertices; v += std::max<unsigned>(1, in.numVertices / 16)) {
                        const RwRGBA& c = geo->colors[v];
                        colOk = colOk && *(std::uint32_t*)(cvb + v * cs.stride + col->offset) == ((unsigned(c.alpha) << 24) | (unsigned(c.red) << 16) | (unsigned(c.green) << 8) | c.blue);
                    }
                }
                if (cvb && cvb != vb) rw::d3d::unlockVertices(cs.vertexBuffer);
            }
            if (uv && geo->texCoords[0]) {
                auto& us = h->vertexStream[uv->stream];
                std::uint8_t* uvb = us.vertexBuffer == st.vertexBuffer ? vb : rw::d3d::lockVertices(us.vertexBuffer, 0, 0, D3DLOCK_READONLY);
                for (int v = 0; uvb && v < geo->numVertices; v += step) {
                    float t[2]; std::memcpy(t, uvb + v * us.stride + uv->offset, 8);
                    uvOk = uvOk && t[0] == geo->texCoords[0][v].u && t[1] == geo->texCoords[0][v].v;
                }
                if (uvb && uvb != vb) rw::d3d::unlockVertices(us.vertexBuffer);
            }
            rw::d3d::unlockVertices(st.vertexBuffer);
            CHECKV(posOk, "%s: VB positions", what);
            CHECKV(nrmOk, "%s: VB normals", what);
            CHECKV(colOk, "%s: VB prelit ARGB", what);
            CHECKV(uvOk, "%s: VB texcoords", what);
        } else {
            CHECKV(false, "%s: VB lock", what);
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------
static void PipelineApiTests() {
    std::printf("--- RxPipeline API\n");
    CHECK(RxNodeDefinitionGetD3D9AtomicAllInOne() != nullptr && std::strcmp(RxNodeDefinitionGetD3D9AtomicAllInOne()->name, "nodeD3D9AtomicAllInOne.csl") == 0);
    RxPipeline* pipe = RxPipelineCreate();
    CHECK(pipe != nullptr);
    CHECK(RxPipelineFindNodeByName(pipe, RxNodeDefinitionGetD3D9AtomicAllInOne()->name, nullptr, nullptr) == nullptr);        // nothing added yet
    RxLockedPipe* lp = RxPipelineLock(pipe);
    CHECK(lp == pipe && RxPipelineLock(pipe) == nullptr);                                                                       // double lock fails
    RwUInt32 first = 99;
    CHECK(RxLockedPipeAddFragment(lp, &first, RxNodeDefinitionGetD3D9AtomicAllInOne(), nullptr) == lp && first == 0);
    CHECK(RxLockedPipeAddFragment(lp, nullptr, RxNodeDefinitionGetD3D9AtomicAllInOne(), nullptr) == nullptr);                  // only one node exists
    CHECK(RxLockedPipeUnlock(lp) == pipe && RxLockedPipeUnlock(pipe) == nullptr);
    RwInt32 idx = 99;
    RxPipelineNode* node = RxPipelineFindNodeByName(pipe, RxNodeDefinitionGetD3D9AtomicAllInOne()->name, nullptr, &idx);
    CHECK(node != nullptr && idx == 0);
    CHECK(RxPipelineFindNodeByName(pipe, "nodeNoSuchThing.csl", nullptr, nullptr) == nullptr);
    CHECK(RxPipelineFindNodeByName(pipe, RxNodeDefinitionGetD3D9AtomicAllInOne()->name, node, nullptr) == nullptr);              // search starts after `start`
    // defaults of the node (exe 0x757890)
    CHECK(RxD3D9AllInOneGetInstanceCallBack(node) == _rpD3D9AtomicDefaultInstanceCallback);
    CHECK(RxD3D9AllInOneGetReinstanceCallBack(node) == _rpD3D9AtomicDefaultReinstanceCallback);
    CHECK(RxD3D9AllInOneGetLightingCallBack(node) == _rpD3D9AtomicDefaultLightingCallback);
    CHECK(RxD3D9AllInOneGetRenderCallBack(node) == _rpD3D9AtomicDefaultRenderCallback);
    RxD3D9AllInOneSetInstanceCallBack(node, nullptr); RxD3D9AllInOneSetReinstanceCallBack(node, nullptr);
    RxD3D9AllInOneSetLightingCallBack(node, nullptr); RxD3D9AllInOneSetRenderCallBack(node, nullptr);
    CHECK(!RxD3D9AllInOneGetInstanceCallBack(node) && !RxD3D9AllInOneGetReinstanceCallBack(node) && !RxD3D9AllInOneGetLightingCallBack(node) && !RxD3D9AllInOneGetRenderCallBack(node));
    // the game's pipeline plugin ids
    pipe->pluginData = 0x1234; pipe->pluginID = 0x5678;
    CHECK(pipe->pluginID == 0x5678 && pipe->pluginData == 0x1234);
    CHECK(RxPipelineDestroy(pipe) == TRUE);
    // a pipeline whose AddFragment is given a foreign node fails; destroy of a non-façade pipeline is refused
    RxPipeline* p2 = RxPipelineCreate();
    RxLockedPipe* l2 = RxPipelineLock(p2);
    RxNodeDefinition other{ "other" };
    CHECK(RxLockedPipeAddFragment(l2, nullptr, &other, nullptr) == nullptr && RxLockedPipeUnlock(l2) == nullptr);
    CHECK(RxPipelineDestroy(p2) == TRUE);
    rw::ObjPipeline* foreign = rw::ObjPipeline::create();
    CHECK(RxPipelineDestroy(foreign) == FALSE);
    foreign->destroy();
}

// ---------------------------------------------------------------------------------------------------------------------------------
static void DefaultPipelineTests() {
    std::printf("--- default pipeline: instancing\n");
    RwShimPipelineEnsure();
    CHECK(rw::engine->driver[rw::PLATFORM_D3D9]->defaultPipeline != nullptr);
    RpMaterial* mat = MakeMaterial({ 255, 255, 255, 255 });
    RpGeometry* g = MakeTri(rpGEOMETRYPRELIT | rpGEOMETRYNORMALS | rpGEOMETRYTEXTURED, mat, { 200, 40, 10, 255 });
    RpAtomic* a = MakeAtomic(g);
    CHECK(a->pipeline == nullptr && RxPipelineFindNodeByName(a->getPipeline(), "nodeD3D9AtomicAllInOne.csl", nullptr, nullptr) != nullptr);   // the façade is the default
    CHECK(g->instData == nullptr);
    const int vb0 = rw::d3d::d3d9Globals.numVertexBuffers, ib0 = rw::d3d::d3d9Globals.numIndexBuffers, vd0 = rw::d3d::d3d9Globals.numVertexDeclarations;
    BeginScene();
    RpAtomicRender(a);
    EndScene();
    CHECK(g->instData != nullptr && g->lockedSinceInst == 0 && !(g->flags & rpGEOMETRYNATIVE));
    CHECKV(rw::d3d::d3d9Globals.numVertexBuffers == vb0 + 1 && rw::d3d::d3d9Globals.numIndexBuffers == ib0 + 1 && rw::d3d::d3d9Globals.numVertexDeclarations == vd0 + 1,
           "buffers +1 each: vb %d ib %d vd %d", rw::d3d::d3d9Globals.numVertexBuffers - vb0, rw::d3d::d3d9Globals.numIndexBuffers - ib0, rw::d3d::d3d9Globals.numVertexDeclarations - vd0);
    CheckInstanceInvariants(a, "tri");
    Header* h = HeaderOf(g);
    CHECK(h->numMeshes == 1 && h->totalNumIndex == 3 && h->inst[0].numPrimitives == 1 && h->inst[0].numVertices == 3 && h->inst[0].minVert == 0 && h->vertexStream[1].vertexBuffer == nullptr);
    Header* before = h;
    BeginScene(); RpAtomicRender(a); EndScene();
    CHECK(HeaderOf(g) == before);                                                                    // unchanged geometry: no re-instance
    FreeAtomic(a);
    RpGeometryDestroy(g);   // (the atomic released its reference)
    CHECKV(rw::d3d::d3d9Globals.numVertexBuffers == vb0 && rw::d3d::d3d9Globals.numIndexBuffers == ib0 && rw::d3d::d3d9Globals.numVertexDeclarations == vd0,
           "buffers released with the geometry: vb %d ib %d vd %d", rw::d3d::d3d9Globals.numVertexBuffers - vb0, rw::d3d::d3d9Globals.numIndexBuffers - ib0, rw::d3d::d3d9Globals.numVertexDeclarations - vd0);
    RpMaterialDestroy(mat);

    // vertex alpha detection + two meshes + list sorting
    RpMaterial* m1 = MakeMaterial({ 255, 0, 0, 255 });
    RpMaterial* m2 = MakeMaterial({ 0, 255, 0, 255 });
    RpGeometry* g2 = RpGeometryCreate(8, 4, rpGEOMETRYPRELIT | rpGEOMETRYPOSITIONS);
    RwV3d* v = g2->morphTargets[0].vertices;
    for (int i = 0; i < 8; i++) { v[i] = { float(i), float(i * 2), 5 }; g2->colors[i] = { 10, 20, 30, i == 6 ? (RwUInt8)128 : (RwUInt8)255 }; }
    RpTriangle* t = RpGeometryGetTriangles(g2);
    const std::uint16_t idx[4][3] = { { 5, 4, 3 }, { 0, 1, 2 }, { 7, 6, 4 }, { 4, 6, 5 } };   // meshes: m1 = tris 0,1 (verts 0..5), m2 = tris 2,3 (verts 4..7)
    for (int i = 0; i < 4; i++) { RpGeometryTriangleSetVertexIndices(g2, &t[i], idx[i][0], idx[i][1], idx[i][2]); RpGeometryTriangleSetMaterial(g2, &t[i], i < 2 ? m1 : m2); }
    CHECK(RpGeometryUnlock(g2) == g2);
    RpAtomic* a2 = MakeAtomic(g2);
    BeginScene(); RpAtomicRender(a2); EndScene();
    CheckInstanceInvariants(a2, "two meshes + vertex alpha");
    Header* h2 = HeaderOf(g2);
    CHECK(h2 && h2->numMeshes == 2 && h2->inst[0].vertexAlpha == 0 && h2->inst[1].vertexAlpha == 1);
    CHECK(h2 && h2->inst[0].startIndex == 0 && h2->inst[1].startIndex == h2->inst[0].numIndex && h2->inst[1].minVert == 4);
    FreeAtomic(a2); RpGeometryDestroy(g2); RpMaterialDestroy(m1); RpMaterialDestroy(m2);

    // tristrip geometry: quad as a strip
    RpMaterial* ms = MakeMaterial({ 255, 255, 255, 255 });
    RpGeometry* gs = RpGeometryCreate(4, 2, rpGEOMETRYTRISTRIP | rpGEOMETRYPOSITIONS | rpGEOMETRYPRELIT);
    const RwV3d qv[4] = { { -2, -2, 5 }, { 2, -2, 5 }, { -2, 2, 5 }, { 2, 2, 5 } };
    for (int i = 0; i < 4; i++) { gs->morphTargets[0].vertices[i] = qv[i]; gs->colors[i] = { 20, 200, 20, 255 }; }
    RpTriangle* ts = RpGeometryGetTriangles(gs);
    RpGeometryTriangleSetVertexIndices(gs, &ts[0], 0, 1, 2); RpGeometryTriangleSetVertexIndices(gs, &ts[1], 2, 1, 3);
    RpGeometryTriangleSetMaterial(gs, &ts[0], ms); RpGeometryTriangleSetMaterial(gs, &ts[1], ms);
    CHECK(RpGeometryUnlock(gs) == gs && gs->meshHeader->flags == 1);
    RpAtomic* as = MakeAtomic(gs);
    BeginScene(); RpAtomicRender(as);
    EndScene();
    CheckInstanceInvariants(as, "tristrip");
    CHECK(HeaderOf(gs) && HeaderOf(gs)->primType == D3DPT_TRIANGLESTRIP && HeaderOf(gs)->inst[0].numPrimitives == HeaderOf(gs)->inst[0].numIndex - 2);
    CHECK(NearC(ReadPixel(g_w / 2, g_h / 2), 20, 200, 20));
    FreeAtomic(as); RpGeometryDestroy(gs); RpMaterialDestroy(ms);

    // dynamic prelit usage -> two streams (stream 0 = prelit only, stream 1 = rest)
    RpMaterial* md = MakeMaterial({ 255, 255, 255, 255 });
    RpGeometry* gd = MakeTri(rpGEOMETRYPRELIT | rpGEOMETRYTEXTURED, md, { 90, 90, 200, 255 });
    RpD3D9GeometrySetUsageFlags(gd, rpD3D9GEOMETRYUSAGE_DYNAMICPRELIT);
    RpAtomic* ad = MakeAtomic(gd);
    BeginScene(); RpAtomicRender(ad); EndScene();
    Header* hd = HeaderOf(gd);
    CHECK(hd && hd->vertexStream[0].vertexBuffer && hd->vertexStream[1].vertexBuffer && hd->vertexStream[0].stride == 4 && hd->vertexStream[0].geometryFlags == rpGEOMETRYLOCKPRELIGHT);
    CHECK(hd && hd->vertexStream[1].stride == 20 && (hd->vertexStream[1].geometryFlags == (rpGEOMETRYLOCKVERTICES | rpGEOMETRYLOCKTEXCOORDS1)));
    CheckInstanceInvariants(ad, "dynamic prelit (2 streams)");
    CHECK(NearC(ReadPixel(g_w / 2, g_h / 2), 90, 90, 200));
    FreeAtomic(ad); RpGeometryDestroy(gd); RpMaterialDestroy(md);
}

// ---------------------------------------------------------------------------------------------------------------------------------
static void PixelTests() {
    std::printf("--- default pipeline: rendering\n");
    const int cx = g_w / 2, cy = g_h / 2;
    struct Case { const char* name; RwUInt32 flags; RwRGBA mat; RwRGBA prelit; int r, g, b; } cases[] = {
        { "unlit, no vertex colours -> black TFACTOR silhouette", 0, { 255, 255, 255, 255 }, { 255, 255, 255, 255 }, 0, 0, 0 },
        { "prelit, lighting off", rpGEOMETRYPRELIT, { 255, 255, 255, 255 }, { 200, 40, 10, 255 }, 200, 40, 10 },
        { "prelit + modulate material (TFACTOR stage)", rpGEOMETRYPRELIT | rpGEOMETRYMODULATEMATERIALCOLOR, { 128, 255, 128, 255 }, { 200, 40, 10, 255 }, 100, 40, 5 },
        { "prelit + modulate white material (no TFACTOR)", rpGEOMETRYPRELIT | rpGEOMETRYMODULATEMATERIALCOLOR, { 255, 255, 255, 255 }, { 60, 120, 180, 255 }, 60, 120, 180 },
    };
    for (const Case& c : cases) {
        RpMaterial* m = MakeMaterial(c.mat);
        RpGeometry* g = MakeTri(c.flags, m, c.prelit);
        RpAtomic* a = MakeAtomic(g);
        BeginScene({ 0, 0, 255, 255 });
        RpAtomicRender(a);
        EndScene();
        const unsigned px = ReadPixel(cx, cy);
        CHECKV(NearC(px, c.r, c.g, c.b, 3), "%s: %08X expect (%d,%d,%d)", c.name, px, c.r, c.g, c.b);
        CHECKV(NearC(ReadPixel(4, 4), 0, 0, 255), "%s: outside stays clear colour", c.name);
        FreeAtomic(a); RpGeometryDestroy(g); RpMaterialDestroy(m);
    }

    // ambient light in the camera's world -> lit path (surface properties): red material, ambient 1 / diffuse 0
    RpWorld* world = RpWorldCreate(nullptr);
    RpLight* amb = RpLightCreate(rpLIGHTAMBIENT);
    RwRGBAReal white{ 1, 1, 1, 1 };
    RpLightSetColor(amb, &white);
    RpWorldAddLight(world, amb);
    RpWorldAddCamera(world, g_cam);
    RpMaterial* m = MakeMaterial({ 255, 0, 0, 255 });
    m->surfaceProps = { 1.0f, 0.0f, 0.0f };   // ambient, specular, diffuse
    RpGeometry* g = MakeTri(rpGEOMETRYLIGHT | rpGEOMETRYNORMALS | rpGEOMETRYMODULATEMATERIALCOLOR, m);
    RpAtomic* a = MakeAtomic(g);
    BeginScene({ 0, 0, 255, 255 });
    RpAtomicRender(a);
    EndScene();
    const unsigned px = ReadPixel(cx, cy);
    CHECKV(NearC(px, 255, 0, 0, 6), "ambient-lit red material: %08X", px);
    CHECK(AmbientSaturated.red == 1.0f && AmbientSaturated.green == 1.0f && AmbientSaturated.blue == 1.0f);
    DWORD lighting = 0; RwD3D9GetRenderState(D3DRS_LIGHTING, &lighting);
    CHECK(lighting == TRUE);
    // a directional light is picked up for geometry with normals (light slot 0), not for geometry without
    RpLight* dir = RpLightCreate(rpLIGHTDIRECTIONAL);
    RwFrame* lf = RwFrameCreate();
    RpLightSetFrame(dir, lf);
    RpLightSetColor(dir, &white);
    RpWorldAddLight(world, dir);
    BeginScene(); RpAtomicRender(a); EndScene();
    D3DLIGHT9 l0{}; RwD3D9GetLight(0, &l0);
    BOOL en = FALSE; g_dev->GetLightEnable(0, &en);
    CHECKV(l0.Type == D3DLIGHT_DIRECTIONAL && l0.Diffuse.r == 1.0f && en, "directional light 0 set + enabled (type %d enabled %d)", (int)l0.Type, (int)en);
    RpWorldRemoveLight(world, dir);
    BeginScene(); RpAtomicRender(a); EndScene();
    g_dev->GetLightEnable(0, &en);
    CHECKV(!en, "light 0 disabled again once the light is gone (enabled %d)", (int)en);
    RpLightDestroy(dir); RwFrameDestroy(lf);
    FreeAtomic(a); RpGeometryDestroy(g); RpMaterialDestroy(m);
    RpWorldRemoveCamera(world, g_cam);
    RpWorldRemoveLight(world, amb);
    RpLightDestroy(amb);
    RpWorldDestroy(world);
}

// D3DRS_CLIPPING after a stock render (exe 0x756E1D: on unless the world bounding sphere is completely inside the frustum)
static void ClippingTests() {
    std::printf("--- stock render callback: D3DRS_CLIPPING\n");
    RpMaterial* m = MakeMaterial({ 255, 255, 255, 255 });
    RpGeometry* g = MakeTri(rpGEOMETRYPRELIT, m);
    RwFrame* f = nullptr;
    RpAtomic* a = MakeAtomic(g, &f);
    auto clippingAfterRender = [&](float x, float y, float z) {
        RwV3d t{ x, y, z };
        RwFrameTranslate(f, &t, rwCOMBINEREPLACE);
        BeginScene(); RpAtomicRender(a);
        DWORD v = 0xDEAD; RwD3D9GetRenderState(D3DRS_CLIPPING, &v);
        EndScene();
        return v;
    };
    CHECK(clippingAfterRender(0, 0, 40) == FALSE);     // sphere well inside the frustum: clipping off
    CHECK(clippingAfterRender(0, 0, 0) == TRUE);       // sphere (radius ~3) straddles the side planes at z = 5 (half width 2.5): boundary -> on
    CHECK(clippingAfterRender(500, 0, 0) == TRUE);     // fully outside -> on
    CHECK(clippingAfterRender(0, 0, 40) == FALSE);     // and off again
    FreeAtomic(a); RpGeometryDestroy(g); RpMaterialDestroy(m);
}

// RwD3D9SetTexture sampler filters (exe 0x7FDE70): anisotropy plugin value > 1 forces MAG = MIN = ANISOTROPIC, MIP = LINEAR and sets MAXANISOTROPY;
// otherwise the filter table row of the texture's filter mode
static void AnisotropyFilterTests() {
    std::printf("--- RwD3D9SetTexture: anisotropy filters\n");
    auto samp = [&](DWORD type) { DWORD v = 0xDEAD; g_dev->GetSamplerState(0, (D3DSAMPLERSTATETYPE)type, &v); return v; };
    rw::Raster* ras = rw::Raster::create(8, 8, 32, rw::Raster::TEXTURE | rw::Raster::C8888);
    rw::Texture* tex = ras ? rw::Texture::create(ras) : nullptr;
    CHECK(tex != nullptr);
    if (!tex) return;
    tex->setFilter(rw::Texture::MIPNEAREST);                       // exe row 3: POINT / POINT mip... (mag POINT, min POINT, mip POINT)
    tex->setMaxAnisotropy(1);
    CHECK(RwD3D9SetTexture(tex, 0) == TRUE);
    CHECK(samp(D3DSAMP_MAGFILTER) == D3DTEXF_POINT && samp(D3DSAMP_MINFILTER) == D3DTEXF_POINT && samp(D3DSAMP_MIPFILTER) == D3DTEXF_POINT && samp(D3DSAMP_MAXANISOTROPY) == 1);
    tex->setMaxAnisotropy(4);
    CHECK(RwD3D9SetTexture(tex, 0) == TRUE);
    CHECK(samp(D3DSAMP_MAGFILTER) == D3DTEXF_ANISOTROPIC && samp(D3DSAMP_MINFILTER) == D3DTEXF_ANISOTROPIC && samp(D3DSAMP_MIPFILTER) == D3DTEXF_LINEAR && samp(D3DSAMP_MAXANISOTROPY) == 4);
    tex->setFilter(rw::Texture::NEAREST);                          // still forced while the value stays > 1
    CHECK(RwD3D9SetTexture(tex, 0) == TRUE);
    CHECK(samp(D3DSAMP_MAGFILTER) == D3DTEXF_ANISOTROPIC && samp(D3DSAMP_MINFILTER) == D3DTEXF_ANISOTROPIC && samp(D3DSAMP_MIPFILTER) == D3DTEXF_LINEAR);
    tex->setMaxAnisotropy(1);                                      // back to the filter row (NEAREST: POINT / POINT / NONE)
    CHECK(RwD3D9SetTexture(tex, 0) == TRUE);
    CHECK(samp(D3DSAMP_MAGFILTER) == D3DTEXF_POINT && samp(D3DSAMP_MINFILTER) == D3DTEXF_POINT && samp(D3DSAMP_MIPFILTER) == D3DTEXF_NONE && samp(D3DSAMP_MAXANISOTROPY) == 1);
    RwD3D9SetTexture(nullptr, 0);
    tex->destroy();
}

// ---------------------------------------------------------------------------------------------------------------------------------
// the game's usage: custom callbacks wrapping the defaults
static int g_instCalls, g_instReinst, g_reinstCalls, g_renderCalls, g_lightCalls;
static RxD3D9AllInOneInstanceCallBack g_defaultInst;
static RpAtomic* g_lastAtomic;
static RwResEntry* g_lastEntry;
static RwUInt8 g_lastType;
static RwUInt32 g_lastFlags;
static RxD3D9AllInOneInstanceCallBack g_reinstArgCb;

static RwBool MyInstance(void* obj, RxD3D9ResEntryHeader* h, RwBool reinstance) {
    ++g_instCalls; if (reinstance) ++g_instReinst;
    g_lastAtomic = (RpAtomic*)obj;
    return g_defaultInst(obj, h, reinstance);
}
static RwBool MyReinstance(void* obj, RwResEntry* e, RxD3D9AllInOneInstanceCallBack cb) {
    ++g_reinstCalls; g_reinstArgCb = cb;
    return cb && cb(obj, (RxD3D9ResEntryHeader*)(e + 1), TRUE);       // the game's CustomPipeReinstanceCB
}
static RxD3D9AllInOneLightingCallBack g_defaultLighting;
static void MyLighting(void* o) { ++g_lightCalls; g_defaultLighting(o); }
static void MyRender(RwResEntry* e, void* obj, RwUInt8 type, RwUInt32 flags) {
    ++g_renderCalls; g_lastEntry = e; g_lastAtomic = (RpAtomic*)obj; g_lastType = type; g_lastFlags = flags;
    // the casts the custom pipelines use
    auto* header = (RxD3D9ResEntryHeader*)(e + 1);
    auto* meshes = (RxD3D9InstanceData*)(header + 1);
    CHECK(header == ((RpAtomic*)obj)->geometry->instData && meshes == header->inst && header->numMeshes >= 1);
    // draw through the game's helper path (PipelinesCommon.hpp semantics)
    RwD3D9SetIndices(header->indexBuffer);
    _rwD3D9SetStreams(header->vertexStream, header->useOffsets);
    RwD3D9SetVertexDeclaration(header->vertexDeclaration);
    RwD3D9SetTexture(nullptr, 0);
    RwD3D9SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG2);
    RwD3D9SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    RwD3D9SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2);
    RwD3D9SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
    for (RwUInt32 i = 0; i < header->numMeshes; i++) {
        RwD3D9SetPixelShader(nullptr);
        RwD3D9SetVertexShader(meshes[i].vertexShader);
        RwD3D9DrawIndexedPrimitive(header->primType, meshes[i].baseIndex, 0, meshes[i].numVertices, meshes[i].startIndex, meshes[i].numPrimitives);
    }
}

static void CustomPipelineTests() {
    std::printf("--- custom pipeline callbacks (the game's CreateCustomObjPipe)\n");
    RxPipeline* pipe = RxPipelineCreate();
    RxLockedPipe* lp = RxPipelineLock(pipe);
    CHECK(lp && RxLockedPipeAddFragment(lp, nullptr, RxNodeDefinitionGetD3D9AtomicAllInOne(), nullptr) && RxLockedPipeUnlock(lp));
    RxPipelineNode* node = RxPipelineFindNodeByName(pipe, RxNodeDefinitionGetD3D9AtomicAllInOne()->name, nullptr, nullptr);
    g_defaultInst = RxD3D9AllInOneGetInstanceCallBack(node);
    g_defaultLighting = RxD3D9AllInOneGetLightingCallBack(node);
    RxD3D9AllInOneSetInstanceCallBack(node, MyInstance);
    RxD3D9AllInOneSetReinstanceCallBack(node, MyReinstance);
    RxD3D9AllInOneSetLightingCallBack(node, MyLighting);
    RxD3D9AllInOneSetRenderCallBack(node, MyRender);
    pipe->pluginID = 0x9C; pipe->pluginData = 0x9C;

    RpMaterial* mat = MakeMaterial({ 255, 255, 255, 255 });
    RpGeometry* g = MakeTri(rpGEOMETRYPRELIT, mat, { 50, 150, 250, 255 });
    RpAtomic* a = MakeAtomic(g);
    a->pipeline = pipe;
    const int vb0 = rw::d3d::d3d9Globals.numVertexBuffers;

    BeginScene(); RpAtomicRender(a); EndScene();
    CHECK(g_instCalls == 1 && g_instReinst == 0 && g_renderCalls == 1 && g_lightCalls == 1 && g_reinstCalls == 0);
    CHECK(g_lastAtomic == a && g_lastType == 1 && g_lastFlags == g->flags && g_lastEntry == (RwResEntry*)((char*)g->instData - sizeof(RwResEntry)));
    CHECK(NearC(ReadPixel(g_w / 2, g_h / 2), 50, 150, 250));
    CheckInstanceInvariants(a, "custom pipeline");
    Header* first = HeaderOf(g);

    BeginScene(); RpAtomicRender(a); EndScene();
    CHECK(g_instCalls == 1 && g_renderCalls == 2 && g_reinstCalls == 0 && HeaderOf(g) == first);

    // re-light the prelit colours: RpGeometryLock/Unlock -> reinstance callback with the instance callback as 3rd argument
    RpGeometryLock(g, rpGEOMETRYLOCKPRELIGHT);
    CHECK(g->lockedSinceInst == rpGEOMETRYLOCKPRELIGHT);
    for (int i = 0; i < 3; i++) g->colors[i] = { 250, 150, 50, 255 };
    RpGeometryUnlock(g);
    BeginScene(); RpAtomicRender(a); EndScene();
    CHECK(g_reinstCalls == 1 && g_reinstArgCb == MyInstance && g_instCalls == 2 && g_instReinst == 1 && g->lockedSinceInst == 0 && HeaderOf(g) == first);
    CHECK(NearC(ReadPixel(g_w / 2, g_h / 2), 250, 150, 50));
    CheckInstanceInvariants(a, "after prelit reinstance");

    // positions only: not re-written prelit; geometry moves
    RpGeometryLock(g, rpGEOMETRYLOCKVERTICES);
    g->morphTargets[0].vertices[2] = { 0, 20, 5 };
    RpGeometryUnlock(g);
    BeginScene(); RpAtomicRender(a); EndScene();
    CHECK(g_reinstCalls == 2 && g_instReinst == 2);
    CheckInstanceInvariants(a, "after vertex reinstance");

    // polygons locked -> new mesh header serial -> full re-instance (new block, old buffers released)
    RpGeometryLock(g, rpGEOMETRYLOCKPOLYGONS);
    RpGeometryUnlock(g);
    BeginScene(); RpAtomicRender(a); EndScene();
    CHECK(g_instCalls == 4 && g_instReinst == 2 && g_reinstCalls == 2 && g_renderCalls == 5);
    CHECKV(rw::d3d::d3d9Globals.numVertexBuffers == vb0 + 1, "full re-instance released the old vertex buffer (%d)", rw::d3d::d3d9Globals.numVertexBuffers - vb0);
    CheckInstanceInvariants(a, "after full re-instance");

    // more than one morph target (exe 0x758270): the STOCK instance callback runs directly (positions + normals added to the lock mask), the node's
    // instance callback is not called, and lockedSinceInst is restored afterwards
    {
        const int calls = g_instCalls;
        g->numMorphTargets = 2; g->lockedSinceInst = rpGEOMETRYLOCKPRELIGHT;
        CHECK(_rpD3D9AtomicDefaultReinstanceCallback(a, (RwResEntry*)((char*)g->instData - sizeof(RwResEntry)), MyInstance) == TRUE);
        CHECK(g_instCalls == calls && g->lockedSinceInst == rpGEOMETRYLOCKPRELIGHT);
        g->numMorphTargets = 1; g->lockedSinceInst = 0;
    }

    FreeAtomic(a); RpGeometryDestroy(g); RpMaterialDestroy(mat);
    CHECKV(rw::d3d::d3d9Globals.numVertexBuffers == vb0, "buffers freed with the geometry (%d)", rw::d3d::d3d9Globals.numVertexBuffers - vb0);
    CHECK(RxPipelineDestroy(pipe) == TRUE);
}

// ---------------------------------------------------------------------------------------------------------------------------------
static std::vector<unsigned char> ReadFile(const char* path) {
    std::vector<unsigned char> b;
    if (FILE* f = std::fopen(path, "rb")) {
        std::fseek(f, 0, SEEK_END); b.resize(std::ftell(f)); std::fseek(f, 0, SEEK_SET);
        b.resize(std::fread(b.data(), 1, b.size(), f)); std::fclose(f);
    }
    return b;
}

static RpAtomic* CollectAtomic(RpAtomic* a, void* data) { ((std::vector<RpAtomic*>*)data)->push_back(a); return a; }

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
    RpClumpForAllAtomics(clump, CollectAtomic, &atoms);
    const int vb0 = rw::d3d::d3d9Globals.numVertexBuffers, ib0 = rw::d3d::d3d9Globals.numIndexBuffers, vd0 = rw::d3d::d3d9Globals.numVertexDeclarations;
    int instanced = 0, meshesTotal = 0, alphaMeshes = 0;
    for (RpAtomic* a : atoms) {
        if (!a->geometry || !a->geometry->meshHeader) continue;
        a->pipeline = nullptr;   // default façade (the file may carry MatFX pipelines)
        BeginScene();
        RpAtomicRender(a);
        EndScene();
        if (a->geometry->numVertices <= 0 || a->geometry->meshHeader->numMeshes == 0) continue;
        char what[96]; std::snprintf(what, sizeof(what), "%s atomic %d", std::strrchr(path, '\\') ? std::strrchr(path, '\\') + 1 : path, instanced);
        CheckInstanceInvariants(a, what, instanced < 6);
        Header* h = HeaderOf(a->geometry);
        if (h) { ++instanced; meshesTotal += h->numMeshes; for (unsigned i = 0; i < h->numMeshes; i++) alphaMeshes += h->inst[i].vertexAlpha != 0; }
    }
    std::printf("  %zu atomics, %d instanced, %d meshes, %d with vertex alpha; buffers +%d vb +%d ib +%d decl\n", atoms.size(), instanced, meshesTotal, alphaMeshes,
                rw::d3d::d3d9Globals.numVertexBuffers - vb0, rw::d3d::d3d9Globals.numIndexBuffers - ib0, rw::d3d::d3d9Globals.numVertexDeclarations - vd0);
    CHECK(instanced > 0);
    RpClumpDestroy(clump);
    CHECKV(rw::d3d::d3d9Globals.numVertexBuffers <= vb0 && rw::d3d::d3d9Globals.numIndexBuffers <= ib0 && rw::d3d::d3d9Globals.numVertexDeclarations <= vd0,
           "clump destroyed -> instanced buffers released (vb %d ib %d decl %d vs %d/%d/%d)", rw::d3d::d3d9Globals.numVertexBuffers, rw::d3d::d3d9Globals.numIndexBuffers,
           rw::d3d::d3d9Globals.numVertexDeclarations, vb0, ib0, vd0);
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
    HWND wnd = CreateWindowA("STATIC", "rw_pipeline_test", WS_OVERLAPPEDWINDOW, 0, 0, 320, 240, nullptr, nullptr, GetModuleHandleA(nullptr), nullptr);
    CHECK(wnd != nullptr);
    rw::MemoryFunctions mf{};
    mf.rwmalloc  = [](size_t sz, unsigned) -> void* { return sz ? std::malloc(sz) : nullptr; };
    mf.rwrealloc = [](void* p, size_t sz, unsigned) -> void* { return std::realloc(p, sz); };
    mf.rwfree    = [](void* p) { std::free(p); };
    CHECK(rw::Engine::init(&mf));
    CHECK(RpWorldPluginAttach() == TRUE);                    // mesh + native data + rights plugins (the game's first PluginAttach)
    rw::registerAnisotropyPlugin();
    rw::EngineOpenParams params{};
    params.window = wnd;
    CHECK(rw::Engine::open(&params));
    rw::Engine::start();
    g_dev = rw::d3d::d3ddevice;
    if (g_dev) NotsaRwRenderState_OnEngineStarted();         // what RwEngineStart does via platform.cpp: RW's render-state defaults
    if (g_dev) RwRenderStateSet(rwRENDERSTATECULLMODE, reinterpret_cast<void*>(uintptr_t(rwCULLMODECULLNONE))); // RW's default is BACK (the game sets its own); the test triangle's winding is not meant to be culled
    std::printf("D3D9 device: %s\n", g_dev ? "yes" : "NO (nothing testable without a device)");
    RwEngineInstance->dOpenDevice.zBufferNear = 0; RwEngineInstance->dOpenDevice.zBufferFar = 1;
    if (g_dev) {
        g_dec3n = (rw::d3d::d3d9Globals.caps.DeclTypes & D3DDTCAPS_DEC3N) != 0;
        std::printf("caps: DevCaps HWTL %d, DEC3N %d, MaxActiveLights %u, MaxStreams %u, DevCaps2 STREAMOFFSET %d\n", (rw::d3d::d3d9Globals.caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT) != 0,
                    (int)g_dec3n, rw::d3d::d3d9Globals.caps.MaxActiveLights, rw::d3d::d3d9Globals.caps.MaxStreams, (rw::d3d::d3d9Globals.caps.DevCaps2 & D3DDEVCAPS2_STREAMOFFSET) != 0);
        SetupCamera();
        RwShimPipelineEnsure();
        PipelineApiTests();
        DefaultPipelineTests();
        PixelTests();
        ClippingTests();
        AnisotropyFilterTests();
        CustomPipelineTests();
        for (const char* p : dffs) DffTests(p);
        RwCameraSetRaster(g_cam, nullptr); RwCameraSetZRaster(g_cam, nullptr);
        RwRasterDestroy(g_fb); RwRasterDestroy(g_zb);
        RwCameraSetFrame(g_cam, nullptr);
        RwCameraDestroy(g_cam); RwFrameDestroy(g_camFrame);
        RwShimPipelineShutdown();
        rw::Engine::stop();
    }
    rw::Engine::close();
    rw::Engine::term();
    DestroyWindow(wnd);
    std::printf("\nrw_pipeline_test: %d checks passed, %d failed\n%s\n", g_pass, g_fail, g_fail ? "FAILED" : "PASSED");
    return g_fail;
}

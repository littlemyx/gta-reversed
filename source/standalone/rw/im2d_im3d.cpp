// P2B-06: immediate mode: RwIm2DRender{Primitive,IndexedPrimitive,Line,Triangle} and RwIm3D{Transform,RenderPrimitive,RenderIndexedPrimitive,
// RenderLine,RenderTriangle,End} on the D3D9 device owned by librw.
//
// Not a wrapper of rw::im2d / rw::im3d: those draw through librw's own shaders (Im2DVertex carries camera Z, not 1/w, and the Im3D path wants
// normals and librw's lighting callback), whereas the exe's RW 3.6 D3D9 driver is fixed function. This is a port of what the exe does
// (device table at 0x4E00A0: Im2D line/tri/prim/indexed = 0x7FB650/0x7FB8E0/0x7FBB00/0x7FBD30 (state setup 0x7FB810), Im3D transform 0x7EF450,
// end 0x7EF520, render pipeline node 0x80E270), minus the dynamic-vertex-buffer ring (DrawPrimitiveUP/DrawIndexedPrimitiveUP produce the same pixels):
//
//   Im2D  the game's RwIm2DVertex is the D3D vertex itself (D3DFVF_XYZRHW|DIFFUSE|TEX1, 28 bytes), so it goes to the device as is.
//         Vertex x/y are shifted by the offset of the current camera's frame buffer raster (sub-rasters), like the exe does with the
//         raster at [0xC9BCC0]+0x60. State: FVF 0x144, no vertex/pixel shader, CLIPPING off, LIGHTING off, texture stage 0 modulates when a texture
//         is bound (rwRENDERSTATETEXTURERASTER) and passes the diffuse colour/alpha otherwise. All other states (blend, z, fog, ...) are the caller's.
//         Return value = D3D call succeeded. Primitive count: line list n/2, polyline n-1, tri list n/3, strip/fan n-2; RwPrimitiveType 0 -> FALSE.
//         An empty draw (count 0: too few vertices for one list primitive, point lists) still sets the state and returns TRUE like the exe; a
//         negative count (strip / polyline with too few vertices) is rejected by D3D -> FALSE.
//   Im3D  RwIm3DTransform only records {vertices, count, LTM pointer, flags|VERTEXXYZ|VERTEXRGBA} (RW ran a transform pipeline, D3D9 does the
//         transform in hardware) and returns the vertex pointer (NULL when numVerts > 0x10000). Each Render* call then sets FVF XYZ|DIFFUSE(|TEX1 with
//         rwIM3D_VERTEXUV), the LTM as D3DTS_WORLD (NULL or identity-flagged = identity), vertex alpha on unless rwIM3D_ALLOPAQUE, CLIPPING off with
//         rwIM3D_NOCLIP, LIGHTING and NORMALIZENORMALS off, the stage-0 ops as in Im2D, and draws the vertices re-packed to 24 (or 16) bytes.
//         Primitive/index counts are rounded down to whole lines/triangles; an indexed call left with no index draws the whole vertex array
//         like the exe. The vertex count is a 16-bit field (0x10000 -> 0 vertices). The render node reports TRUE whatever the D3D draw returned.
//         Render* without a transform in progress (or a NULL vertex pointer), and RwPrimitiveType 0 / 6 (RW has no Im3D point lists) return
//         FALSE; RwIm3DEnd returns FALSE when no transform is in progress.
//
// Only needs fakerw + librw + the CRT; shares librw's state caches with rwd3d_ff.cpp (RwD3D9Set*), so librw's own draws stay consistent.
#ifdef NOTSA_RW_LIBRW
#include "fakerw.h"

// librw's D3D9 device internals (d3ddevice, flushCache, setStreamSource, ...)
#include <src/d3d/rwd3dimpl.h>

#include <cstdint>
#include <cstring>
#include <vector>

static_assert(sizeof(RwIm2DVertex) == 28 && sizeof(RwIm3DVertex) == 36);

namespace {

constexpr DWORD kIm2DFvf       = D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1;   // 0x144
constexpr DWORD kIm3DFvf       = D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1;      // 0x142
constexpr DWORD kIm3DFvfNoUV   = D3DFVF_XYZ | D3DFVF_DIFFUSE;                    // 0x042
constexpr RwUInt32 kMaxIm3DVerts = 0x10000;
constexpr RwInt32  kMaxIm2DIndexedIndices = 10000;                               // exe: more indices than vertices and > 0x2710 -> FALSE

IDirect3DDevice9* Dev() { return rw::d3d::d3ddevice; }

// exe table at 0x884888, indexed by RwPrimitiveType
D3DPRIMITIVETYPE D3DPrim(RwPrimitiveType t) {
    switch (t) {
    case rwPRIMTYPELINELIST: return D3DPT_LINELIST;
    case rwPRIMTYPEPOLYLINE: return D3DPT_LINESTRIP;
    case rwPRIMTYPETRILIST:  return D3DPT_TRIANGLELIST;
    case rwPRIMTYPETRISTRIP: return D3DPT_TRIANGLESTRIP;
    case rwPRIMTYPETRIFAN:   return D3DPT_TRIANGLEFAN;
    case rwPRIMTYPEPOINTLIST: return D3DPT_POINTLIST;
    default:                 return static_cast<D3DPRIMITIVETYPE>(0);
    }
}

// primitives that n vertices / indices make (exe jump tables 0x7FBD18 / 0x7FC1E4, Im3D node 0x80E944): 0 for point lists (the exe
// leaves the count at 0 for them) and invalid types; may be negative for strips / polylines with too few vertices (D3D rejects those)
int PrimCount(RwPrimitiveType t, int n) {
    switch (t) {
    case rwPRIMTYPELINELIST: return n / 2;
    case rwPRIMTYPEPOLYLINE: return n - 1;
    case rwPRIMTYPETRILIST:  return n / 3;
    case rwPRIMTYPETRISTRIP:
    case rwPRIMTYPETRIFAN:   return n - 2;
    default:                 return 0;
    }
}

// Stage 0 colour/alpha ops (0x7FB810 / 0x80E270): modulate texture and diffuse when a texture is bound, otherwise pass the diffuse colour
void SetStage0Ops() {
    if (rw::GetRenderStatePtr(rw::TEXTURERASTER) == nullptr) {
        RwD3D9SetTextureStageState(0, D3DTSS_COLOROP,   D3DTOP_SELECTARG2);
        RwD3D9SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
        RwD3D9SetTextureStageState(0, D3DTSS_ALPHAOP,   D3DTOP_SELECTARG2);
        RwD3D9SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
    } else {
        RwD3D9SetTextureStageState(0, D3DTSS_COLOROP,   D3DTOP_MODULATE);
        RwD3D9SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        RwD3D9SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
        RwD3D9SetTextureStageState(0, D3DTSS_ALPHAOP,   D3DTOP_MODULATE);
        RwD3D9SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
        RwD3D9SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
    }
}

// UP draws leave stream 0 / the index buffer unbound on the device: bring librw's cache in line (it skips redundant sets)
void SyncCachesAfterUP() {
    rw::d3d::setStreamSource(0, nullptr, 0, 0);
    rw::d3d::setIndices(nullptr);
}

//--------------------------------------------------------------------------------------------------
// Im2D
//--------------------------------------------------------------------------------------------------
// offset of the camera's frame buffer raster (sub-rasters); 0,0 outside a camera update
void FrameBufferOffset(float& ox, float& oy) {
    ox = oy = 0.0f;
    if (RwEngineInstance && RwEngineInstance->curCamera && RwEngineInstance->curCamera->frameBuffer) {
        ox = static_cast<float>(RwEngineInstance->curCamera->frameBuffer->offsetX);
        oy = static_cast<float>(RwEngineInstance->curCamera->frameBuffer->offsetY);
    }
}

void SetupIm2DState() {
    RwD3D9SetFVF(kIm2DFvf);
    RwD3D9SetVertexShader(nullptr);
    RwD3D9SetPixelShader(nullptr);
    RwD3D9SetRenderState(D3DRS_CLIPPING, FALSE);
    RwD3D9SetRenderState(D3DRS_LIGHTING, FALSE);
    SetStage0Ops();
    rw::d3d::flushCache();
}

// vertices `src[0..n)` with the raster offset added to x/y; returns `src` itself when there is nothing to add
const RwIm2DVertex* OffsetVertices(const RwIm2DVertex* src, RwInt32 n, std::vector<RwIm2DVertex>& scratch) {
    float ox, oy;
    FrameBufferOffset(ox, oy);
    if (ox == 0.0f && oy == 0.0f) {
        return src;
    }
    scratch.assign(src, src + n);
    for (auto& v : scratch) {
        v.x += ox;
        v.y += oy;
    }
    return scratch.data();
}

std::vector<RwIm2DVertex> s_Im2DScratch;

RwBool Im2DDraw(RwPrimitiveType type, const RwIm2DVertex* verts, RwInt32 numVerts) {
    if (!Dev() || !verts || numVerts < 0) {
        return FALSE;
    }
    const D3DPRIMITIVETYPE prim = D3DPrim(type);
    if (prim == 0) {
        if (type == rwPRIMTYPENAPRIMTYPE) {
            SetupIm2DState(); // exe (0x7FBB00): the state is set before the draw call, D3D then rejects primitive type 0
        }
        return FALSE;
    }
    const int primCount = PrimCount(type, numVerts);
    if (primCount < 0) {
        return FALSE; // strip / polyline with too few vertices: the exe passes the negative count on and D3D rejects the call
    }
    const RwIm2DVertex* data = OffsetVertices(verts, numVerts, s_Im2DScratch);
    SetupIm2DState();
    if (primCount == 0) {
        return TRUE; // the exe still issues the call for an empty draw (and point lists, whose count it leaves at 0): D3D9 returns S_OK
    }
    const HRESULT hr = Dev()->DrawPrimitiveUP(prim, primCount, data, sizeof(RwIm2DVertex));
    SyncCachesAfterUP();
    return SUCCEEDED(hr) ? TRUE : FALSE;
}

//--------------------------------------------------------------------------------------------------
// Im3D
//--------------------------------------------------------------------------------------------------
struct Im3DTransform {
    const RwIm3DVertex* verts = nullptr;
    RwUInt32            num   = 0;
    const RwMatrix*     ltm   = nullptr;
    RwUInt32            flags = 0;
    bool                active = false;
};
Im3DTransform s_Im3D;

struct PackedUV   { float x, y, z; DWORD color; float u, v; };
struct PackedNoUV { float x, y, z; DWORD color; };
std::vector<std::uint8_t> s_Im3DPacked;

D3DMATRIX WorldMatrix(const RwMatrix* m) {
    D3DMATRIX w{};
    if (m && !(m->flags & rw::Matrix::IDENTITY)) {
        w._11 = m->right.x; w._12 = m->right.y; w._13 = m->right.z;
        w._21 = m->up.x;    w._22 = m->up.y;    w._23 = m->up.z;
        w._31 = m->at.x;    w._32 = m->at.y;    w._33 = m->at.z;
        w._41 = m->pos.x;   w._42 = m->pos.y;   w._43 = m->pos.z;
    } else {
        w._11 = w._22 = w._33 = 1.0f;
    }
    w._44 = 1.0f;
    return w;
}

// round a vertex/index count down to whole lines / triangles (exe: 0x7EF59E, 0x7EF5EE)
RwInt32 RoundCount(RwPrimitiveType type, RwInt32 n) {
    if (type == rwPRIMTYPELINELIST) {
        return n - (n % 2);
    }
    if (type == rwPRIMTYPETRILIST) {
        return n - (n % 3);
    }
    return n;
}

// The render node (0x80E270) always reports success once the state is set, whatever the D3D draw call returns (also for an empty draw).
// `indices` + numIndices > 0: indexed draw; otherwise (RwIm3DRenderPrimitive, and RwIm3DRenderIndexedPrimitive with no index left after
// rounding: node 0x80E40C falls back to the non-indexed path) the whole vertex array is drawn.
bool Im3DDraw(RwPrimitiveType type, const RwImVertexIndex* indices, RwInt32 numIndices) {
    if (!Dev()) {
        return false;
    }
    const D3DPRIMITIVETYPE prim = D3DPrim(type);
    const RwUInt32 flags = s_Im3D.flags;
    const bool uv = (flags & rwIM3D_VERTEXUV) != 0;
    const UINT stride = uv ? sizeof(PackedUV) : sizeof(PackedNoUV);

    // state (0x80E270)
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, reinterpret_cast<void*>((flags & rwIM3D_ALLOPAQUE) ? 0 : 1));
    const D3DMATRIX world = WorldMatrix(s_Im3D.ltm);
    RwD3D9SetTransform(D3DTS_WORLD, &world);
    RwD3D9SetRenderState(D3DRS_LIGHTING, FALSE);
    RwD3D9SetRenderState(D3DRS_NORMALIZENORMALS, FALSE);
    RwD3D9SetRenderState(D3DRS_CLIPPING, (flags & rwIM3D_NOCLIP) ? FALSE : TRUE);
    SetStage0Ops();
    RwD3D9SetFVF(uv ? kIm3DFvf : kIm3DFvfNoUV);
    RwD3D9SetVertexShader(nullptr);
    RwD3D9SetPixelShader(nullptr);

    if (s_Im3D.num == 0) {
        return true; // 0x80E3F8: no vertices, no draw
    }

    // 36 -> 24 / 16 bytes
    s_Im3DPacked.resize(static_cast<size_t>(s_Im3D.num) * stride);
    for (RwUInt32 i = 0; i < s_Im3D.num; i++) {
        const RwIm3DVertex& s = s_Im3D.verts[i];
        std::uint8_t* d = s_Im3DPacked.data() + static_cast<size_t>(i) * stride;
        if (uv) {
            *reinterpret_cast<PackedUV*>(d) = PackedUV{s.position.x, s.position.y, s.position.z, s.color, s.u, s.v};
        } else {
            *reinterpret_cast<PackedNoUV*>(d) = PackedNoUV{s.position.x, s.position.y, s.position.z, s.color};
        }
    }
    rw::d3d::flushCache();

    if (indices && numIndices != 0) {
        const int primCount = PrimCount(type, numIndices);
        if (primCount > 0) {
            Dev()->DrawIndexedPrimitiveUP(prim, 0, s_Im3D.num, primCount, indices, D3DFMT_INDEX16, s_Im3DPacked.data(), stride);
        }
    } else {
        const int primCount = PrimCount(type, static_cast<RwInt32>(s_Im3D.num));
        if (primCount > 0) {
            Dev()->DrawPrimitiveUP(prim, primCount, s_Im3DPacked.data(), stride);
        }
    }
    SyncCachesAfterUP();
    return true;
}

bool Im3DPrimTypeSupported(RwPrimitiveType t) { return t >= rwPRIMTYPELINELIST && t <= rwPRIMTYPETRIFAN; }

} // namespace

//--------------------------------------------------------------------------------------------------
// Im2D
//--------------------------------------------------------------------------------------------------
RwBool RwIm2DRenderPrimitive(RwPrimitiveType primType, RwIm2DVertex* vertices, RwInt32 numVertices) {
    return Im2DDraw(primType, vertices, numVertices);
}

RwBool RwIm2DRenderLine(RwIm2DVertex* vertices, RwInt32 numVertices, RwInt32 vert1, RwInt32 vert2) {
    if (!vertices || vert1 < 0 || vert2 < 0 || vert1 >= numVertices || vert2 >= numVertices) {
        return FALSE;
    }
    const RwIm2DVertex pair[2] = {vertices[vert1], vertices[vert2]};
    return Im2DDraw(rwPRIMTYPELINELIST, pair, 2);
}

RwBool RwIm2DRenderTriangle(RwIm2DVertex* vertices, RwInt32 numVertices, RwInt32 vert1, RwInt32 vert2, RwInt32 vert3) {
    if (!vertices || vert1 < 0 || vert2 < 0 || vert3 < 0 || vert1 >= numVertices || vert2 >= numVertices || vert3 >= numVertices) {
        return FALSE;
    }
    const RwIm2DVertex tri[3] = {vertices[vert1], vertices[vert2], vertices[vert3]};
    return Im2DDraw(rwPRIMTYPETRILIST, tri, 3);
}

RwBool RwIm2DRenderIndexedPrimitive(RwPrimitiveType primType, RwIm2DVertex* vertices, RwInt32 numVertices, RwImVertexIndex* indices, RwInt32 numIndices) {
    if (!Dev() || !vertices || !indices || numVertices < 0 || numIndices < 0) {
        return FALSE;
    }
    const D3DPRIMITIVETYPE prim = D3DPrim(primType);
    if (prim == 0) {
        if (primType == rwPRIMTYPENAPRIMTYPE) {
            SetupIm2DState(); // exe (0x7FBD30): state first, then D3D rejects type 0
        }
        return FALSE;
    }
    if (numIndices > numVertices && numIndices > kMaxIm2DIndexedIndices) {
        return FALSE;
    }
    const int primCount = PrimCount(primType, numIndices);
    if (primCount < 0) {
        return FALSE;
    }
    const RwIm2DVertex* data = OffsetVertices(vertices, numVertices, s_Im2DScratch);
    SetupIm2DState();
    if (primCount == 0) {
        return TRUE; // empty draw (see Im2DDraw)
    }
    const HRESULT hr = Dev()->DrawIndexedPrimitiveUP(prim, 0, numVertices, primCount, indices, D3DFMT_INDEX16, data, sizeof(RwIm2DVertex));
    SyncCachesAfterUP();
    return SUCCEEDED(hr) ? TRUE : FALSE;
}

//--------------------------------------------------------------------------------------------------
// Im3D
//--------------------------------------------------------------------------------------------------
void* RwIm3DTransform(RwIm3DVertex* pVerts, RwUInt32 numVerts, RwMatrix* ltm, RwUInt32 flags) {
    if (numVerts > kMaxIm3DVerts) {
        return nullptr;
    }
    s_Im3D.verts  = pVerts;
    s_Im3D.num    = numVerts & 0xFFFF;   // the exe keeps the count in a 16-bit field (0x7EF498): exactly 0x10000 vertices become 0
    s_Im3D.ltm    = ltm;
    s_Im3D.flags  = flags | rwIM3D_VERTEXXYZ | rwIM3D_VERTEXRGBA;
    s_Im3D.active = pVerts != nullptr;   // the Render / End functions test the vertex pointer (0x7EF52D, 0x7EF55E)
    return pVerts;
}

RwBool RwIm3DEnd(void) {
    if (!s_Im3D.active) {
        return FALSE;
    }
    s_Im3D = Im3DTransform{};
    return TRUE;
}

RwBool RwIm3DRenderPrimitive(RwPrimitiveType primType) {
    if (!s_Im3D.active || !Im3DPrimTypeSupported(primType)) {
        return FALSE;
    }
    return Im3DDraw(primType, nullptr, 0) ? TRUE : FALSE;
}

RwBool RwIm3DRenderIndexedPrimitive(RwPrimitiveType primType, RwImVertexIndex* indices, RwInt32 numIndices) {
    if (!s_Im3D.active || !Im3DPrimTypeSupported(primType) || !indices) {
        return FALSE;
    }
    return Im3DDraw(primType, indices, RoundCount(primType, numIndices)) ? TRUE : FALSE;
}

RwBool RwIm3DRenderLine(RwInt32 vert1, RwInt32 vert2) {
    if (!s_Im3D.active) {
        return FALSE;
    }
    const RwImVertexIndex idx[2] = {static_cast<RwImVertexIndex>(vert1), static_cast<RwImVertexIndex>(vert2)};
    return Im3DDraw(rwPRIMTYPELINELIST, idx, 2) ? TRUE : FALSE;
}

RwBool RwIm3DRenderTriangle(RwInt32 vert1, RwInt32 vert2, RwInt32 vert3) {
    if (!s_Im3D.active) {
        return FALSE;
    }
    const RwImVertexIndex idx[3] = {static_cast<RwImVertexIndex>(vert1), static_cast<RwImVertexIndex>(vert2), static_cast<RwImVertexIndex>(vert3)};
    return Im3DDraw(rwPRIMTYPETRILIST, idx, 3) ? TRUE : FALSE;
}
#endif // NOTSA_RW_LIBRW

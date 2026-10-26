// Stream R, slice R-4: GL3 twin of standalone/rw/im2d_im3d.cpp (excluded in the portable build). Game-facing immediate mode API:
//   RwIm2DRender{Primitive,IndexedPrimitive,Line,Triangle} and RwIm3D{Transform,RenderPrimitive,RenderIndexedPrimitive,RenderLine,RenderTriangle,End}
// mapped onto librw's gl3 backend (rw::im2d::* / rw::im3d::* -> gl3immed.cpp, shaders im2d.vert / im3d.vert + simple.frag).
//
// What this layer adds on top of librw (all of it is exe behaviour taken from the D3D9 port in standalone/rw/im2d_im3d.cpp, which documents the exe
// addresses; the return-value contract is the one tests/standalone/rw_im2d_im3d_test.cpp pins):
//   * vertex conversion: the game's RwIm2DVertex {x,y,z,rhw, packed ARGB, u,v} (28 B) -> rw::gl3::Im2DVertex {x,y,z, w = 1/rhw, r,g,b,a bytes, u,v}; the game's
//     RwIm3DVertex {pos, normal, packed ARGB, u, v} (36 B) -> rw::gl3::Im3DVertex (same size, r,g,b,a bytes). BGRA-in-a-DWORD (D3DCOLOR) is what every
//     game-side setter writes (RwIm2DVertexSetIntRGBA, RwIm3DVertexSetRGBA, RxObjSpace3DVertexSetPreLitColor), gl3 wants memory order R,G,B,A.
//   * Im2D z: the game writes RwIm2DGetNearScreenZ() / FarScreenZ() (= the gl3 device range -1..1, engine_gl3.cpp copies it), gl3's vertex shader passes it
//     into clip space untouched, so the depth the exe's D3D 0..1 range gave is reproduced in the device range. 1/rhw becomes clip w (rhw == 0 -> w = 1).
//   * Im2D frame buffer offset (sub-rasters): the offset of the current camera's frame buffer raster is added to x / y of every vertex (caller's array untouched).
//   * Im3D: RwIm3DTransform only records {vertices, count, LTM, flags} like the D3D driver (RW's transform pipeline did the work in software, the D3D one in
//     hardware); each Render* converts the vertices, hands them to librw (Transform -> Render -> End, so librw's attribute set-up is balanced per draw) and
//     sees the vertices as they are at Render time. LTM NULL or identity-flagged = identity. Vertex alpha = unless rwIM3D_ALLOPAQUE (RwRenderStateSet
//     VERTEXALPHAENABLE, a persistent state change exactly as in the exe). Without rwIM3D_VERTEXUV the draw is untextured and TEXTURERASTER is restored
//     afterwards (librw's gl3 im3DTransform clears it for good; D3D only switched the stage ops).
//   * Counting / return values: primitive and index counts round down to whole lines / triangles, point lists and RwPrimitiveType 0 draw nothing, negative
//     primitive counts (strip / polyline with too few vertices) and out-of-range calls return FALSE, an empty draw returns TRUE.
//
//   * texture orientation: librw's gl3 shaders sample (u, 1 - v) (GL row order); game rasters are D3D order (top row first, uploaded as is), so v is sent as
//     1 - v whenever a non-camera-texture raster is bound (NotsaGl3TextureNeedsVFlip in raster_gl3.h; hand-over to R-5 / R-6: the stock pipelines need the
//     same compensation, or the shader flip has to go and this one with it).
//
// Honest gaps (R-n items, .notes/reports/MAC-R4.md): rwIM3D_NOCLIP is ignored (GL always clips to the frustum); Im3D line width is 1 px on macOS core GL
// (glLineWidth > 1 is ignored there); point lists draw nothing like in the exe; draws outside a camera update (no frame buffer) log once and return FALSE.
#ifdef NOTSA_RW_LIBRW
#include "fakerw.h"
#include "standalone/Fixups.h"
#include "raster_gl3.h" // + librw gl3 internals (rwgl3impl.h is unguarded: included once, there)

#include <chrono>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <vector>

static_assert(sizeof(RwIm2DVertex) == 28 && sizeof(RwIm3DVertex) == 36);
static_assert(sizeof(rw::gl3::Im2DVertex) == 28 && sizeof(rw::gl3::Im3DVertex) == 36);

namespace {

constexpr RwUInt32 kMaxIm3DVerts          = 0x10000;
constexpr RwInt32  kMaxIm2DIndexedIndices = 10000; // exe: more indices than vertices and > 0x2710 -> FALSE

void LogOnce(bool& flag, const char* what) {
    if (!flag) {
        flag = true;
        notsa::standalone::Fixups::Log("R-4: %s", what);
    }
}

// started device, a camera update in progress, a frame buffer to draw into (librw's gl3 Im2D / Im3D dereference currentCamera->frameBuffer)
bool CanDraw() {
    static bool s_logged = false;
    if (!rw::engine || rw::Engine::state != rw::Engine::Started || !rw::engine->currentCamera) {
        LogOnce(s_logged, "immediate mode draw outside RwCameraBeginUpdate / EndUpdate (or no device): ignored, the call returns FALSE");
        return false;
    }
    return static_cast<rw::Camera*>(rw::engine->currentCamera)->frameBuffer != nullptr;
}

bool ValidPrimType(RwPrimitiveType t) { return t >= rwPRIMTYPELINELIST && t <= rwPRIMTYPEPOINTLIST; }

// primitives that n vertices / indices make: 0 for point lists (the exe leaves the count at 0) and invalid types; negative for strips / polylines with
// too few vertices (D3D rejects those)
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

inline void UnpackArgb(RwUInt32 c, std::uint8_t& r, std::uint8_t& g, std::uint8_t& b, std::uint8_t& a) {
    r = static_cast<std::uint8_t>(c >> 16);
    g = static_cast<std::uint8_t>(c >> 8);
    b = static_cast<std::uint8_t>(c);
    a = static_cast<std::uint8_t>(c >> 24);
}

//--------------------------------------------------------------------------------------------------
// Im2D
//--------------------------------------------------------------------------------------------------
std::vector<rw::gl3::Im2DVertex> s_Im2DScratch;

// game vertices -> gl3 vertices with the frame buffer raster offset added
rw::gl3::Im2DVertex* ConvertIm2D(const RwIm2DVertex* src, RwInt32 n) {
    float ox = 0.0f, oy = 0.0f;
    if (const auto* cam = static_cast<rw::Camera*>(rw::engine->currentCamera); cam && cam->frameBuffer) {
        ox = static_cast<float>(cam->frameBuffer->offsetX);
        oy = static_cast<float>(cam->frameBuffer->offsetY);
    }
    const bool flipV = NotsaGl3TextureNeedsVFlip(static_cast<const RwRaster*>(rw::GetRenderStatePtr(rw::TEXTURERASTER)));
    s_Im2DScratch.resize(static_cast<size_t>(n > 0 ? n : 1));
    for (RwInt32 i = 0; i < n; i++) {
        const RwIm2DVertex& s = src[i];
        rw::gl3::Im2DVertex& d = s_Im2DScratch[static_cast<size_t>(i)];
        d.x = s.x + ox;
        d.y = s.y + oy;
        d.z = s.z;
        d.w = s.rhw != 0.0f ? 1.0f / s.rhw : 1.0f;
        UnpackArgb(s.emissiveColor, d.r, d.g, d.b, d.a);
        d.u = s.u;
        d.v = flipV ? 1.0f - s.v : s.v;
    }
    return s_Im2DScratch.data();
}


// INT1 diagnostics (NOTSA_INT1_DEPTH=1): logs when an Im2D draw changes the depth value at the screen centre (interior frames: depth was 0 before the shell draw)
void Int1DepthProbe(const char* what, const RwIm2DVertex* v, int n, bool post) {
    static const bool on = std::getenv("NOTSA_INT1_DEPTH") != nullptr;
    static int        lines = 0;
    static float      pre = -2.f;
    static const auto t0 = std::chrono::steady_clock::now();
    if (!on || lines > 80 || std::chrono::steady_clock::now() - t0 < std::chrono::seconds(std::atoi(std::getenv("NOTSA_INT1_DEPTH")))) {
        return;
    }
    GLint fb = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fb);
    if (fb != 0) {
        return;
    }
    float d = -3.f;
    glReadPixels(640, 360, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &d);
    if (!post) {
        pre = d;
        return;
    }
    if (d != pre) {
        GLboolean dm = 0;
        GLint     df = 0;
        glGetBooleanv(GL_DEPTH_WRITEMASK, &dm);
        glGetIntegerv(GL_DEPTH_FUNC, &df);
        lines++;
        notsa::standalone::Fixups::Log("INT1 depth: %s (%d verts, z0 %.3f) changed centre depth %.4f -> %.4f; rw ZTEST %d ZWRITE %d; GL depthTest %d mask %d func 0x%x", what, n, v ? v->z : 0.f, pre, d, (int)rw::GetRenderState(rw::ZTESTENABLE), (int)rw::GetRenderState(rw::ZWRITEENABLE), (int)glIsEnabled(GL_DEPTH_TEST), (int)dm, df);
    }
}

RwBool Im2DDraw(RwPrimitiveType type, const RwIm2DVertex* verts, RwInt32 numVerts) {
    if (!verts || numVerts < 0 || !ValidPrimType(type) || !CanDraw()) {
        return FALSE;
    }
    const int primCount = PrimCount(type, numVerts);
    if (primCount < 0) {
        return FALSE; // strip / polyline with too few vertices
    }
    if (primCount == 0) {
        return TRUE; // empty draw (too few vertices for one list primitive, point lists): the exe still "draws" and gets S_OK
    }
    Int1DepthProbe("RenderPrimitive", verts, numVerts, false);
    rw::im2d::RenderPrimitive(static_cast<rw::PrimitiveType>(type), ConvertIm2D(verts, numVerts), numVerts);
    Int1DepthProbe("RenderPrimitive", verts, numVerts, true);
    return TRUE;
}

//--------------------------------------------------------------------------------------------------
// Im3D
//--------------------------------------------------------------------------------------------------
struct Im3DTransform {
    const RwIm3DVertex* verts  = nullptr;
    RwUInt32            num    = 0;
    RwMatrix*           ltm    = nullptr;
    RwUInt32            flags  = 0;
    bool                active = false;
};
Im3DTransform s_Im3D;

std::vector<rw::gl3::Im3DVertex> s_Im3DScratch;

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

// `indices` + numIndices > 0: indexed draw; otherwise (RwIm3DRenderPrimitive, and RwIm3DRenderIndexedPrimitive with no index left after rounding: the exe
// falls back to the non-indexed path) the whole vertex array is drawn. Reports TRUE once the state is set, like the exe's render node.
bool Im3DDraw(RwPrimitiveType type, const RwImVertexIndex* indices, RwInt32 numIndices) {
    if (!CanDraw()) {
        return false;
    }
    const RwUInt32 flags = s_Im3D.flags;
    const bool     uv    = (flags & rwIM3D_VERTEXUV) != 0;

    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, reinterpret_cast<void*>((flags & rwIM3D_ALLOPAQUE) ? 0 : 1));
    if (s_Im3D.num == 0) {
        return true; // 0x80E3F8: no vertices, no draw
    }

    int primCount;
    if (indices && numIndices != 0) {
        primCount = PrimCount(type, numIndices);
    } else {
        primCount = PrimCount(type, static_cast<RwInt32>(s_Im3D.num));
        indices   = nullptr;
    }
    if (primCount <= 0) {
        return true; // D3D rejected / skipped such a draw, the node still reports success
    }

    const bool flipV = uv && NotsaGl3TextureNeedsVFlip(static_cast<const RwRaster*>(rw::GetRenderStatePtr(rw::TEXTURERASTER)));
    s_Im3DScratch.resize(s_Im3D.num);
    for (RwUInt32 i = 0; i < s_Im3D.num; i++) {
        const RwIm3DVertex& s = s_Im3D.verts[i];
        rw::gl3::Im3DVertex& d = s_Im3DScratch[i];
        d.position.x = s.position.x; d.position.y = s.position.y; d.position.z = s.position.z;
        d.normal.x   = s.normal.x;   d.normal.y   = s.normal.y;   d.normal.z   = s.normal.z;
        UnpackArgb(s.color, d.r, d.g, d.b, d.a);
        d.u = s.u;
        d.v = flipV ? 1.0f - s.v : s.v;
    }

    // librw: Transform uploads the vertices, selects the im3d shader and the world matrix, and (no UV) clears TEXTURERASTER for good
    void* const savedTex = uv ? nullptr : rw::GetRenderStatePtr(rw::TEXTURERASTER);
    RwMatrix*   world    = (s_Im3D.ltm && !(s_Im3D.ltm->flags & rw::Matrix::IDENTITY)) ? s_Im3D.ltm : nullptr;
    rw::im3d::Transform(s_Im3DScratch.data(), static_cast<int32_t>(s_Im3D.num), world, uv ? rw::im3d::VERTEXUV : 0u);
    if (indices) {
        rw::im3d::RenderIndexedPrimitive(static_cast<rw::PrimitiveType>(type), const_cast<RwImVertexIndex*>(indices), numIndices);
    } else {
        rw::im3d::RenderPrimitive(static_cast<rw::PrimitiveType>(type));
    }
    rw::im3d::End();
    if (!uv) {
        rw::SetRenderStatePtr(rw::TEXTURERASTER, savedTex);
    }
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
    if (!vertices || !indices || numVertices < 0 || numIndices < 0 || !ValidPrimType(primType) || !CanDraw()) {
        return FALSE;
    }
    if (numIndices > numVertices && numIndices > kMaxIm2DIndexedIndices) {
        return FALSE;
    }
    const int primCount = PrimCount(primType, numIndices);
    if (primCount < 0) {
        return FALSE;
    }
    if (primCount == 0) {
        return TRUE; // empty draw (see Im2DDraw)
    }
    Int1DepthProbe("RenderIndexedPrimitive", vertices, numVertices, false);
    rw::im2d::RenderIndexedPrimitive(static_cast<rw::PrimitiveType>(primType), ConvertIm2D(vertices, numVertices), numVertices, indices, numIndices);
    Int1DepthProbe("RenderIndexedPrimitive", vertices, numVertices, true);
    return TRUE;
}

//--------------------------------------------------------------------------------------------------
// Im3D
//--------------------------------------------------------------------------------------------------
void* RwIm3DTransform(RwIm3DVertex* pVerts, RwUInt32 numVerts, RwMatrix* ltm, RwUInt32 flags) {
    if (numVerts > kMaxIm3DVerts) {
        return nullptr;
    }
    s_Im3D.verts  = pVerts;
    s_Im3D.num    = numVerts & 0xFFFF; // the exe keeps the count in a 16-bit field (0x7EF498): exactly 0x10000 vertices become 0
    s_Im3D.ltm    = ltm;
    s_Im3D.flags  = flags | rwIM3D_VERTEXXYZ | rwIM3D_VERTEXRGBA;
    s_Im3D.active = pVerts != nullptr; // the Render / End functions test the vertex pointer (0x7EF52D, 0x7EF55E)
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

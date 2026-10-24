// Stream R, slice R-6: the stock librw gl3 pipelines (default / skin / matfx) render SA geometry. Run by tools/portable/run_pipe_test.sh (needs a display session; the
// real-data part needs the Steam copy, $GTA).
//
// Part A (synthetic, through the game's RW API: RpGeometryCreate / RpAtomicCreate / RpAtomicRender, pixel read-back of the default framebuffer):
//   * texture orientation (the R-4 v-flip rule): a 2x2 texture, texel words written with RwRGBAToPixel, uv (0,0) = top left as in a DFF -> screen quadrants top-first;
//     texel word order (B,G,R,A) -> a texel written as (200,100,50) draws as (200,100,50); nearest filtering so the quadrants are exact;
//   * prelit colours, material colour (MODULATEMATERIALCOLOR), ambient + directional lighting (LIGHT + NORMALS), alpha test (texel alpha 0 discarded), blend, z test,
//     cull mode, fog;
//   * the night-vertex-colour mechanism: RpGeometryLock(PRELIGHT) / change colours / Unlock after the first draw -> the next draw shows the new colours (this is what
//     NVCPipelineProcess does to the geometry; the default instance callback re-uploads the colour stream);
//   * matfx: env-map material draws (no GL error) and the env texture orientation matches D3D (reflection of an upward normal samples the TOP of the env map).
// Part B (real data): buildings (sw_diner1, farmhouse, bakery_sfse, cityhall_sfs), peds (male01, wmycr, player from gta3.img; clothes of player.img: skinned) and a car
// (infernus) are streamed from gta3.img / player.img (clump + dictionary, read in place) and drawn with RpAtomicRender into the framebuffer; per model: atomics / triangles,
// coverage of non-background pixels, number of distinct colours, glGetError() == 0. PPM/PNG dumps go to $NOTSA_PIPE_DUMP (default build/pipe_test).
// Result line: "PIPE RESULT OK" / "PIPE RESULT FAIL ..." / "PIPE RESULT SKIP ...".
#include "fakerw.h"
#include "standalone/Fixups.h"
#include "portable/render/engine_gl3.h"
#include "portable/render/camera_gl3.h"
#include "portable/render/raster_gl3.h"
#include "portable/render/pipelines_gl3.h"
#include "portable/render/pipelines_carenv_gl3.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <sys/stat.h>

namespace notsa::standalone::Fixups {
void Log(const char* fmt, ...) {
    va_list a;
    va_start(a, fmt);
    std::printf("[log] ");
    std::vprintf(fmt, a);
    std::printf("\n");
    va_end(a);
}
void Fatal(const char* fmt, ...) {
    va_list a;
    va_start(a, fmt);
    std::vfprintf(stderr, fmt, a);
    va_end(a);
    std::abort();
}
void LogFpuState(const char*) {}
} // namespace notsa::standalone::Fixups
RwUInt32 RwStreamRead(RwStream* stream, void* buffer, RwUInt32 length) { return stream->read8(buffer, length); } // (standalone/rw/stream.cpp needs the game's StdInc.h)
RwStream* RwStreamWrite(RwStream* stream, const void* buffer, RwUInt32 length) { return stream->write8(buffer, length) == length ? stream : nullptr; }

namespace {
int g_fail = 0;
int g_ok   = 0;

void Check(bool ok, const char* what, const char* detail = "") {
    if (!ok) {
        g_fail++;
    } else {
        g_ok++;
    }
    std::printf("  %-78s %s %s\n", what, ok ? "ok" : "FAIL", detail);
}

bool NoGlError(const char* where) {
    bool clean = true;
    for (GLenum e; (e = glGetError()) != GL_NO_ERROR;) {
        std::printf("  glGetError after %s: 0x%x\n", where, e);
        clean = false;
    }
    return clean;
}

struct Px { int r, g, b, a; };

int g_pw = 0, g_ph = 0;
std::vector<uint8_t> g_frame; // RGBA, row 0 = TOP

void ReadFrame() {
    g_frame.assign((size_t)g_pw * g_ph * 4, 0);
    std::vector<uint8_t> tmp(g_frame.size());
    glReadBuffer(GL_BACK);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, g_pw, g_ph, GL_RGBA, GL_UNSIGNED_BYTE, tmp.data());
    for (int y = 0; y < g_ph; ++y) {
        std::memcpy(&g_frame[(size_t)y * g_pw * 4], &tmp[(size_t)(g_ph - 1 - y) * g_pw * 4], (size_t)g_pw * 4);
    }
}
Px At(int x, int yTop) {
    const uint8_t* p = &g_frame[((size_t)yTop * g_pw + x) * 4];
    return { p[0], p[1], p[2], p[3] };
}
bool Near(Px p, int r, int g, int b, int tol = 3) { return std::abs(p.r - r) <= tol && std::abs(p.g - g) <= tol && std::abs(p.b - b) <= tol; }
std::string Str(Px p) { char b[64]; std::snprintf(b, sizeof(b), "(%d,%d,%d)", p.r, p.g, p.b); return b; }

void RS(RwRenderState st, uintptr_t v) { RwRenderStateSet(st, reinterpret_cast<void*>(v)); }

RwCamera* g_cam   = nullptr;
RpWorld*  g_world = nullptr;
RpLight*  g_ambient = nullptr;
RpLight*  g_dir     = nullptr;

void SetLights(float amb, float dir) { // grey levels; 0 = off
    RwRGBAReal a{ amb, amb, amb, 1.0f }, d{ dir, dir, dir, 1.0f };
    RpLightSetColor(g_ambient, &a);
    RpLightSetColor(g_dir, &d);
}

void Begin(RwRGBA bg = { 0, 0, 0, 255 }) {
    RwCameraBeginUpdate(g_cam);
    RwCameraClear(g_cam, &bg, rwCAMERACLEARIMAGE | rwCAMERACLEARZ);
    RS(rwRENDERSTATEZTESTENABLE, 1);
    RS(rwRENDERSTATEZWRITEENABLE, 1);
    RS(rwRENDERSTATECULLMODE, rwCULLMODECULLBACK);
}
void End() {
    ReadFrame();
    RwCameraEndUpdate(g_cam);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// synthetic quad: x,y in [-1,1] at depth z, uv (0,0) = top left (D3D / DFF convention), counter-clockwise as seen from the camera
// ---------------------------------------------------------------------------------------------------------------------------------------------
struct Quad {
    RpAtomic*   atomic = nullptr;
    RpGeometry* geo    = nullptr;
    RpMaterial* mat    = nullptr;
    RwFrame*    frame  = nullptr;
};

Quad MakeQuad(RwUInt32 flags, float z, RwTexture* tex, RwRGBA matColor, RwRGBA prelit, bool reverseWinding = false, float size = 1.0f) {
    Quad q;
    q.geo = RpGeometryCreate(4, 2, flags | rpGEOMETRYPOSITIONS);
    q.mat = RpMaterialCreate();
    RpMaterialSetColor(q.mat, &matColor);
    if (tex) {
        RpMaterialSetTexture(q.mat, tex);
    }
    RpGeometryLock(q.geo, rpGEOMETRYLOCKALL);
    RwV3d* v = q.geo->morphTargets[0].vertices;
    const float xs[4] = { size, -size, size, -size }, ys[4] = { size, size, -size, -size }; // TL TR BL BR as seen on screen (RW camera: world +X appears on the LEFT)
    for (int i = 0; i < 4; ++i) {
        v[i] = { xs[i], ys[i], z };
        if (q.geo->morphTargets[0].normals) {
            q.geo->morphTargets[0].normals[i] = { 0.0f, 0.0f, -1.0f }; // facing the camera
        }
        if (q.geo->texCoords[0]) {
            q.geo->texCoords[0][i] = { (i & 1) ? 1.0f : 0.0f, (i & 2) ? 1.0f : 0.0f };
        }
        if (q.geo->colors) {
            q.geo->colors[i] = prelit;
        }
    }
    RpTriangle* t = q.geo->triangles;
    // front faces of the RW camera (world +X on the screen's left): (TR,TL,BR) and (BR,TL,BL); the reversed quad swaps two indices
    if (reverseWinding) {
        RpGeometryTriangleSetVertexIndices(q.geo, &t[0], 1, 3, 0);
        RpGeometryTriangleSetVertexIndices(q.geo, &t[1], 3, 2, 0);
    } else {
        RpGeometryTriangleSetVertexIndices(q.geo, &t[0], 1, 0, 3);
        RpGeometryTriangleSetVertexIndices(q.geo, &t[1], 3, 0, 2);
    }
    RpGeometryTriangleSetMaterial(q.geo, &t[0], q.mat);
    RpGeometryTriangleSetMaterial(q.geo, &t[1], q.mat);
    RwSphere bs{};
    RpMorphTargetCalcBoundingSphere(&q.geo->morphTargets[0], &bs);
    q.geo->morphTargets[0].boundingSphere = bs;
    RpGeometryUnlock(q.geo);
    q.atomic = RpAtomicCreate();
    q.frame  = RwFrameCreate();
    RpAtomicSetFrame(q.atomic, q.frame);
    RpAtomicSetGeometry(q.atomic, q.geo, 0);
    return q;
}
void FreeQuad(Quad& q) {
    RpAtomicDestroy(q.atomic);
    RwFrameDestroy(q.frame);
    RpGeometryDestroy(q.geo);
    RpMaterialDestroy(q.mat);
}

RwTexture* MakeTex2x2(const RwRGBA c[4] /* TL TR BL BR */, bool nearest = true) {
    RwRaster* r = RwRasterCreate(2, 2, 32, rwRASTERTYPETEXTURE | rwRASTERFORMAT8888);
    RwUInt8*  p = RwRasterLock(r, 0, rwRASTERLOCKWRITE | rwRASTERLOCKNOFETCH);
    const int stride = RwRasterGetStride(r);
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 2; ++x) {
            RwRGBA c4 = c[y * 2 + x];
            *reinterpret_cast<RwUInt32*>(p + y * stride + x * 4) = RwRGBAToPixel(&c4, rwRASTERFORMAT8888);
        }
    }
    RwRasterUnlock(r);
    RwTexture* t = RwTextureCreate(r);
    RwTextureSetFilterMode(t, nearest ? rwFILTERNEAREST : rwFILTERLINEAR);
    RwTextureSetAddressing(t, rwTEXTUREADDRESSCLAMP);
    return t;
}

// quadrant centres of a quad of half-size `size` at depth z (fov 90 degrees: x_screen = x / z)
void QuadPixels(float size, float z, int out[4][2]) {
    const float cxy = size * 0.5f / z; // quadrant centre, in half-width units
    for (int i = 0; i < 4; ++i) {
        const float sx = (i & 1) ? cxy : -cxy, sy = (i & 2) ? -cxy : cxy; // screen y up positive
        out[i][0] = (int)(g_pw * 0.5f + sx * g_pw * 0.5f);
        out[i][1] = (int)(g_ph * 0.5f - sy * g_pw * 0.5f); // view window 1.0 x 0.75: the vertical scale equals the horizontal one (square pixels)
    }
}

void RunSynthetic() {
    std::printf("---- R-6 part A: synthetic geometry through the stock pipelines ----\n");
    char what[160];
    const float Z = 4.0f;
    int px[4][2];
    QuadPixels(1.0f, Z, px);

    // ---- texture orientation + texel order ----
    {
        const RwRGBA c[4] = { { 255, 0, 0, 255 }, { 0, 255, 0, 255 }, { 0, 0, 255, 255 }, { 200, 100, 50, 255 } };
        RwTexture* tex = MakeTex2x2(c);
        SetLights(0.0f, 0.0f);
        Quad q = MakeQuad(rpGEOMETRYTEXTURED | rpGEOMETRYPRELIT, Z, tex, { 255, 255, 255, 255 }, { 255, 255, 255, 255 });
        Begin();
        RpAtomicRender(q.atomic);
        End();
        const char* nm[4] = { "top-left", "top-right", "bottom-left", "bottom-right" };
        for (int i = 0; i < 4; ++i) {
            const Px p = At(px[i][0], px[i][1]);
            std::snprintf(what, sizeof(what), "v-flip: quadrant %s = texel (%d,%d,%d) (texel row 0 on screen top)", nm[i], c[i].red, c[i].green, c[i].blue);
            Check(Near(p, c[i].red, c[i].green, c[i].blue), what, Str(p).c_str());
        }
        Check(NoGlError("textured quad"), "glGetError clean (textured quad)");
        // outside the quad: background
        Check(Near(At(4, 4), 0, 0, 0, 0), "outside the quad = clear colour");

        // ---- prelit colours + vertex colour attribute order ----
        RpGeometryLock(q.geo, rpGEOMETRYLOCKPRELIGHT);
        for (int i = 0; i < 4; ++i) {
            q.geo->colors[i] = { 100, 150, 200, 255 };
        }
        RpGeometryUnlock(q.geo);
        Begin();
        RpAtomicRender(q.atomic);
        End();
        // texel * prelit: red(255,0,0) * (100,150,200)/255 = (100,0,0); (200,100,50) -> (78,59,39)
        Px p0 = At(px[0][0], px[0][1]), p3 = At(px[3][0], px[3][1]);
        Check(Near(p0, 100, 0, 0, 3), "NVC mechanism: Lock(PRELIGHT)+Unlock re-uploads the colours (TL)", Str(p0).c_str());
        Check(Near(p3, 78, 59, 39, 3), "NVC mechanism: texel x prelit (BR)", Str(p3).c_str());
        // blend towards another colour set once more (second reinstance)
        RpGeometryLock(q.geo, rpGEOMETRYLOCKPRELIGHT);
        for (int i = 0; i < 4; ++i) {
            q.geo->colors[i] = { 255, 255, 255, 255 };
        }
        RpGeometryUnlock(q.geo);
        Begin();
        RpAtomicRender(q.atomic);
        End();
        p0 = At(px[0][0], px[0][1]);
        Check(Near(p0, 255, 0, 0), "NVC mechanism: second update back to white", Str(p0).c_str());

        // ---- material colour (geometry flag MODULATEMATERIALCOLOR) ----
        RwRGBA mc{ 128, 255, 64, 255 };
        RpMaterialSetColor(q.mat, &mc);
        q.geo->flags |= rpGEOMETRYMODULATEMATERIALCOLOR;
        Begin();
        RpAtomicRender(q.atomic);
        End();
        p3 = At(px[3][0], px[3][1]);
        Check(Near(p3, 200 * 128 / 255, 100, 50 * 64 / 255, 3), "material colour modulates (MODULATEMATERIALCOLOR)", Str(p3).c_str());
        FreeQuad(q);
        RwTextureDestroy(tex);
    }

    // ---- lighting: LIGHT + NORMALS, ambient and directional ----
    {
        Quad q = MakeQuad(rpGEOMETRYNORMALS | rpGEOMETRYLIGHT | rpGEOMETRYPRELIT, Z, nullptr, { 255, 255, 255, 255 }, { 0, 0, 0, 255 });
        SetLights(0.4f, 0.0f);
        Begin();
        RpAtomicRender(q.atomic);
        End();
        Px p = At(g_pw / 2, g_ph / 2);
        Check(Near(p, 102, 102, 102, 4), "ambient 0.4 on a black prelit lit quad = 102", Str(p).c_str());
        // directional light pointing along +Z (towards the +Z looking camera): the quad normal is -Z, so N.(-dir) = 1
        SetLights(0.0f, 0.5f);
        Begin();
        RpAtomicRender(q.atomic);
        End();
        p = At(g_pw / 2, g_ph / 2);
        Check(Near(p, 128, 128, 128, 4), "directional 0.5 facing the quad = 128 (identity-frame light points along +Z)", Str(p).c_str());
        // geometry without the LIGHT flag ignores the lights
        q.geo->flags &= ~rpGEOMETRYLIGHT;
        RpGeometryLock(q.geo, rpGEOMETRYLOCKPRELIGHT);
        for (int i = 0; i < 4; ++i) {
            q.geo->colors[i] = { 10, 20, 30, 255 };
        }
        RpGeometryUnlock(q.geo);
        SetLights(0.9f, 0.9f);
        Begin();
        RpAtomicRender(q.atomic);
        End();
        p = At(g_pw / 2, g_ph / 2);
        Check(Near(p, 10, 20, 30, 2), "no LIGHT flag: prelit colour only, lights ignored", Str(p).c_str());
        FreeQuad(q);
        SetLights(0.0f, 0.0f);
        Check(NoGlError("lighting"), "glGetError clean (lighting)");
    }

    // ---- alpha test, blend, z, cull, fog ----
    {
        const RwRGBA c[4] = { { 255, 0, 0, 255 }, { 0, 255, 0, 0 }, { 0, 0, 255, 255 }, { 255, 255, 0, 128 } };
        RwTexture* tex = MakeTex2x2(c);
        Quad q = MakeQuad(rpGEOMETRYTEXTURED | rpGEOMETRYPRELIT, Z, tex, { 255, 255, 255, 255 }, { 255, 255, 255, 255 });
        RS(rwRENDERSTATEVERTEXALPHAENABLE, 0);
        RS(rwRENDERSTATEALPHATESTFUNCTION, rwALPHATESTFUNCTIONGREATER);
        RS(rwRENDERSTATEALPHATESTFUNCTIONREF, 10);
        Begin({ 20, 20, 20, 255 });
        RpAtomicRender(q.atomic);
        End();
        Px p = At(px[1][0], px[1][1]);
        Check(Near(p, 20, 20, 20, 1), "alpha test GREATER ref 10: texel alpha 0 discarded (shows the background)", Str(p).c_str());
        p = At(px[0][0], px[0][1]);
        Check(Near(p, 255, 0, 0, 2), "alpha test: opaque texel drawn", Str(p).c_str());
        RS(rwRENDERSTATEALPHATESTFUNCTIONREF, 0);
        RS(rwRENDERSTATEALPHATESTFUNCTION, rwALPHATESTFUNCTIONALWAYS);
        // blend: texel alpha 128 (yellow) over the background (20,20,20) with vertex alpha enabled
        RS(rwRENDERSTATEVERTEXALPHAENABLE, 1);
        RS(rwRENDERSTATESRCBLEND, rwBLENDSRCALPHA);
        RS(rwRENDERSTATEDESTBLEND, rwBLENDINVSRCALPHA);
        Begin({ 20, 20, 20, 255 });
        RpAtomicRender(q.atomic);
        End();
        p = At(px[3][0], px[3][1]);
        Check(Near(p, 137, 137, 10, 4), "blend SRCALPHA / INVSRCALPHA: yellow a=128 over (20,20,20)", Str(p).c_str());
        RS(rwRENDERSTATEVERTEXALPHAENABLE, 0);
        FreeQuad(q);
        RwTextureDestroy(tex);
    }
    {
        // z test: near red quad drawn first, far green quad second must not overwrite; reverse order the near one wins
        Quad nearQ = MakeQuad(rpGEOMETRYPRELIT, 3.0f, nullptr, { 255, 255, 255, 255 }, { 255, 0, 0, 255 });
        Quad farQ  = MakeQuad(rpGEOMETRYPRELIT, 6.0f, nullptr, { 255, 255, 255, 255 }, { 0, 255, 0, 255 }, false, 3.0f);
        Begin();
        RpAtomicRender(nearQ.atomic);
        RpAtomicRender(farQ.atomic);
        End();
        Px p = At(g_pw / 2, g_ph / 2);
        Check(Near(p, 255, 0, 0), "z test: near quad first, far quad behind stays hidden", Str(p).c_str());
        Begin();
        RpAtomicRender(farQ.atomic);
        RpAtomicRender(nearQ.atomic);
        End();
        p = At(g_pw / 2, g_ph / 2);
        Check(Near(p, 255, 0, 0), "z test: far quad first, near quad drawn over it", Str(p).c_str());
        // far quad visible around the near one
        p = At((int)(g_pw * 0.5f + g_pw * 0.2f), g_ph / 2);
        Check(Near(p, 0, 255, 0), "far quad visible outside the near one", Str(p).c_str());
        RS(rwRENDERSTATEZTESTENABLE, 0);
        RS(rwRENDERSTATEZWRITEENABLE, 0);
        FreeQuad(nearQ);
        FreeQuad(farQ);
    }
    {
        // cull: the quad with the reversed winding is culled with CULLBACK, drawn with CULLNONE
        Quad q = MakeQuad(rpGEOMETRYPRELIT, Z, nullptr, { 255, 255, 255, 255 }, { 255, 255, 255, 255 }, true);
        Begin();
        RpAtomicRender(q.atomic);
        End();
        Check(Near(At(g_pw / 2, g_ph / 2), 0, 0, 0, 0), "cull back: reversed winding not drawn");
        Begin();
        RS(rwRENDERSTATECULLMODE, rwCULLMODECULLNONE);
        RpAtomicRender(q.atomic);
        End();
        Check(Near(At(g_pw / 2, g_ph / 2), 255, 255, 255, 0), "cull none: reversed winding drawn");
        FreeQuad(q);
        Quad f = MakeQuad(rpGEOMETRYPRELIT, Z, nullptr, { 255, 255, 255, 255 }, { 255, 255, 255, 255 });
        Begin();
        RpAtomicRender(f.atomic);
        End();
        Check(Near(At(g_pw / 2, g_ph / 2), 255, 255, 255, 0), "cull back: front winding (TL,TR,BL clockwise on screen) drawn");
        // fog: linear, colour blue (RW value 0xAARRGGBB), fog starts at 2, far plane 8: the white quad at z = 4 is 2/3 fogged
        RS(rwRENDERSTATEFOGCOLOR, 0xFF0000FFu);
        RS(rwRENDERSTATEFOGTYPE, rwFOGTYPELINEAR);
        RS(rwRENDERSTATEFOGENABLE, 1);
        g_cam->fogPlane = 2.0f;
        RwCameraSetFarClipPlane(g_cam, 8.0f);
        Begin();
        RpAtomicRender(f.atomic);
        End();
        const Px p = At(g_pw / 2, g_ph / 2);
        Check(Near(p, 170, 170, 255, 6), "fog linear start 2 end 8 at z = 4: 2/3 white + 1/3 fog colour (0,0,255)", Str(p).c_str());
        RwCameraSetFarClipPlane(g_cam, 100.0f);
        g_cam->fogPlane = 1.0f;
        RS(rwRENDERSTATEFOGENABLE, 0);
        FreeQuad(f);
        Check(NoGlError("render states"), "glGetError clean (alpha / blend / z / cull / fog)");
    }

    // ---- matfx: env map (D3D orientation of the env texture) ----
    {
        // env texture: top half white, bottom half black (rows). The quad faces the camera (normal -Z). Reflection (camera-space normal) maps the centre of the quad to the
        // centre of the env map; use a quad with an UP-facing normal in camera space instead: normal (0,1,0) -> D3D v = 0.5 - 0.5*ny = 0 (top row) = white.
        const RwRGBA ec[4] = { { 255, 255, 255, 255 }, { 255, 255, 255, 255 }, { 0, 0, 0, 255 }, { 0, 0, 0, 255 } };
        RwTexture*   env = MakeTex2x2(ec, false);
        RwTextureSetAddressing(env, rwTEXTUREADDRESSCLAMP);
        Quad q = MakeQuad(rpGEOMETRYPRELIT | rpGEOMETRYNORMALS | rpGEOMETRYTEXTURED, Z, nullptr, { 255, 255, 255, 255 }, { 0, 0, 0, 255 });
        const RwRGBA grey[4] = { { 0, 0, 0, 255 }, { 0, 0, 0, 255 }, { 0, 0, 0, 255 }, { 0, 0, 0, 255 } };
        RwTexture* base = MakeTex2x2(grey);
        RpMaterialSetTexture(q.mat, base);
        RpMatFXMaterialSetEffects(q.mat, rpMATFXEFFECTENVMAP);
        rw::MatFX::get(q.mat)->fx[0].env.tex = env;
        env->refCount++; // the material owns one reference now
        RpMatFXMaterialSetEnvMapCoefficient(q.mat, 1.0f);
        rw::MatFX::enableEffects(q.atomic);
        Px envPx[2];
        for (int up = 0; up < 2; ++up) {
            RpGeometryLock(q.geo, rpGEOMETRYLOCKNORMALS);
            for (int i = 0; i < 4; ++i) {
                q.geo->morphTargets[0].normals[i] = up ? RwV3d{ 0.0f, 0.8f, -0.6f } : RwV3d{ 0.0f, -0.8f, -0.6f };
            }
            RpGeometryUnlock(q.geo);
            Begin();
            RpAtomicRender(q.atomic);
            End();
            envPx[up] = At(g_pw / 2, g_ph / 2);
        }
        // the env texture wraps in this test (REPEAT blends the rows at the edge): "up" must sample mostly the white top row, "down" mostly the black bottom row
        std::snprintf(what, sizeof(what), "matfx env map: camera-space normal up samples the TOP of the env texture (D3D v = 0.5 - 0.5 ny): up %s > down %s", Str(envPx[1]).c_str(), Str(envPx[0]).c_str());
        Check(envPx[1].r > envPx[0].r + 60 && envPx[1].r > 150, what);
        Check(NoGlError("matfx"), "glGetError clean (matfx env)");
        FreeQuad(q);
        RwTextureDestroy(env);
        RwTextureDestroy(base);
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// part C (R-9 / R-8): the SA env-map + specular pipeline (pipelines_carenv_gl3.cpp) against CPU-computed expectations
// ---------------------------------------------------------------------------------------------------------------------------------------------
NotsaSaEnvMesh g_envMesh;
void TestPrepare(rw::Atomic*, rw::Material*, NotsaSaEnvMesh& o) { o = g_envMesh; }

RwTexture* MakeTexWH(int w, int h, const std::vector<RwRGBA>& c) { // row 0 first
    RwRaster* r = RwRasterCreate(w, h, 32, rwRASTERTYPETEXTURE | rwRASTERFORMAT8888);
    RwUInt8*  p = RwRasterLock(r, 0, rwRASTERLOCKWRITE | rwRASTERLOCKNOFETCH);
    const int stride = RwRasterGetStride(r);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            RwRGBA c4 = c[y * w + x];
            *reinterpret_cast<RwUInt32*>(p + y * stride + x * 4) = RwRGBAToPixel(&c4, rwRASTERFORMAT8888);
        }
    }
    RwRasterUnlock(r);
    RwTexture* t = RwTextureCreate(r);
    RwTextureSetFilterMode(t, rwFILTERNEAREST);
    RwTextureSetAddressing(t, rwTEXTUREADDRESSCLAMP); // the pipeline must force WRAP for the env stage
    return t;
}
void SetNormals(Quad& q, RwV3d n) {
    RpGeometryLock(q.geo, rpGEOMETRYLOCKNORMALS);
    for (int i = 0; i < 4; ++i) q.geo->morphTargets[0].normals[i] = n;
    RpGeometryUnlock(q.geo);
}
void SetPrelit(Quad& q, RwRGBA c) {
    RpGeometryLock(q.geo, rpGEOMETRYLOCKPRELIGHT);
    for (int i = 0; i < 4; ++i) q.geo->colors[i] = c;
    RpGeometryUnlock(q.geo);
}
Px DrawCenter(Quad& q) {
    Begin();
    RpAtomicRender(q.atomic);
    End();
    return At(g_pw / 2, g_ph / 2);
}

void RunSaEnv() {
    std::printf("---- R-9 / R-8 part C: SA env map + specular pipeline vs CPU expectations ----\n");
    char what[240];
    SetLights(0.0f, 0.0f);
    rw::ObjPipeline* pipe = NotsaGl3CreateSaEnvPipeline(TestPrepare, 0x9A);
    Check(pipe != nullptr, "NotsaGl3CreateSaEnvPipeline");
    const float Z = 4.0f;
    const uint32_t d0 = NotsaGl3SaEnvDrawCount();

    // ---- CAR_CAM: s = u + sx*nx/nz, t = v + sy*ny/nz  (camera space normal, x flipped view space; 8x1 and 1x8 gradient env textures, WRAP, nearest) ----
    {
        std::vector<RwRGBA> row, col;
        for (int i = 0; i < 8; ++i) {
            row.push_back({ (RwUInt8)(i * 30), (RwUInt8)(255 - i * 30), (RwUInt8)(i * 10), 255 });
            col.push_back({ (RwUInt8)(i * 30), (RwUInt8)(255 - i * 30), (RwUInt8)(i * 10), 255 });
        }
        RwTexture* envRow = MakeTexWH(8, 1, row);
        RwTexture* envCol = MakeTexWH(1, 8, col);
        Quad q = MakeQuad(rpGEOMETRYPRELIT | rpGEOMETRYNORMALS, Z, nullptr, { 255, 255, 255, 255 }, { 0, 0, 0, 255 });
        q.atomic->pipeline = pipe;
        g_envMesh = NotsaSaEnvMesh{};
        g_envMesh.mode = NOTSA_SAENV_CAR_CAM;
        g_envMesh.factor = 0.5f;

        // (a) facing normal: s = u, t = v. u = 0.4375 -> texel 3 of 8
        g_envMesh.envTex = envRow;
        g_envMesh.env[0] = 0.5f; g_envMesh.env[1] = 0.5f; g_envMesh.env[2] = 0.4375f; g_envMesh.env[3] = 0.0f;
        SetNormals(q, { 0.0f, 0.0f, -1.0f });
        Px p = DrawCenter(q);
        auto texel = [&](int i) { return row[i]; };
        std::snprintf(what, sizeof(what), "CAR_CAM facing normal: s = u = 0.4375 -> env texel 3 * 0.5 = (%d,%d,%d)", texel(3).red / 2, texel(3).green / 2, texel(3).blue / 2);
        Check(Near(p, texel(3).red / 2, texel(3).green / 2, texel(3).blue / 2, 3), what, Str(p).c_str());

        // (b) tilted in x: world normal (3,0,-8)/|.|: view-space nx = -3k (x flipped), nz = -8k -> nx/nz = 0.375; s = 0.25 + 0.5*0.375 = 0.4375 -> texel 3 again; and with u = 0.5 -> 0.6875 = texel 5
        const float len = std::sqrt(9.0f + 64.0f);
        SetNormals(q, { 3.0f / len, 0.0f, -8.0f / len });
        g_envMesh.env[2] = 0.25f;
        p = DrawCenter(q);
        std::snprintf(what, sizeof(what), "CAR_CAM tilted normal (x flip + projective divide): s = 0.25 + 0.5*0.375 -> texel 3 (%d,%d,%d)", texel(3).red / 2, texel(3).green / 2, texel(3).blue / 2);
        Check(Near(p, texel(3).red / 2, texel(3).green / 2, texel(3).blue / 2, 3), what, Str(p).c_str());
        g_envMesh.env[2] = 0.5f;
        p = DrawCenter(q);
        std::snprintf(what, sizeof(what), "CAR_CAM u = 0.5 translation (nz row of the texture matrix): s = 0.6875 -> texel 5 (%d,%d,%d)", texel(5).red / 2, texel(5).green / 2, texel(5).blue / 2);
        Check(Near(p, texel(5).red / 2, texel(5).green / 2, texel(5).blue / 2, 3), what, Str(p).c_str());
        // wrap: u = 1.4375 -> same texel 3 (the exe forces D3DTADDRESS_WRAP, the texture itself says CLAMP)
        g_envMesh.env[2] = 1.25f; // 1.25 + 0.1875 = 1.4375 -> frac 0.4375
        p = DrawCenter(q);
        Check(Near(p, texel(3).red / 2, texel(3).green / 2, texel(3).blue / 2, 3), "CAR_CAM s > 1 wraps (WRAP forced on the env sampler although the texture says CLAMP)", Str(p).c_str());

        // (c) vertical: normal up (view +y): ny/nz = -c/b; t = v + sy*(-c/b); normal (0,3,-8): t = 0.5 - 0.5*0.375 = 0.3125 -> texel 2 of 8
        g_envMesh.envTex = envCol;
        g_envMesh.env[2] = 0.0f; g_envMesh.env[3] = 0.5f;
        SetNormals(q, { 0.0f, 3.0f / len, -8.0f / len });
        p = DrawCenter(q);
        std::snprintf(what, sizeof(what), "CAR_CAM normal up samples ABOVE v: t = 0.5 - 0.5*0.375 = 0.3125 -> texel 2 (%d,%d,%d)", col[2].red / 2, col[2].green / 2, col[2].blue / 2);
        Check(Near(p, col[2].red / 2, col[2].green / 2, col[2].blue / 2, 3), what, Str(p).c_str());

        // (d) additive on top of the lit colour: prelit (60,60,60): result = prelit + env*factor (clamped)
        SetPrelit(q, { 60, 60, 60, 255 });
        SetNormals(q, { 0.0f, 0.0f, -1.0f });
        g_envMesh.envTex = envRow; g_envMesh.env[0] = 0.5f; g_envMesh.env[1] = 0.5f; g_envMesh.env[2] = 0.4375f; g_envMesh.env[3] = 0.0f;
        p = DrawCenter(q);
        Check(Near(p, 60 + texel(3).red / 2, 60 + texel(3).green / 2, 60 + texel(3).blue / 2, 3), "CAR_CAM is additive (MULTIPLYADD): prelit 60 + env*0.5", Str(p).c_str());
        g_envMesh.factor = 1.0f;
        p = DrawCenter(q);
        Check(Near(p, std::min(255, 60 + texel(3).red), std::min(255, 60 + texel(3).green), std::min(255, 60 + texel(3).blue), 3), "CAR_CAM factor 1.0: saturates at 255", Str(p).c_str());
        g_envMesh.factor = 0.5f;

        // (e) painted-over placeholder colour renders black (isSpecialColor), env still added
        SetPrelit(q, { 255, 255, 255, 255 });
        RpGeometry* g = q.geo;
        g->flags |= rpGEOMETRYMODULATEMATERIALCOLOR;
        RwRGBA mc{ 60, 255, 0, 255 };
        RpMaterialSetColor(q.mat, &mc);
        g_envMesh.mode = NOTSA_SAENV_NONE;
        g_envMesh.envTex = nullptr;
        p = DrawCenter(q);
        Check(Near(p, 60, 255, 0, 2), "no env, no special colour: material colour modulates (60,255,0)", Str(p).c_str());
        g_envMesh.blackColor = true;
        // a mesh with neither env nor specular uses librw's default shader, which has no black override: the pipeline only honours blackColor on its own shader (specular / env)
        g_envMesh.mode = NOTSA_SAENV_CAR_CAM; g_envMesh.envTex = envRow;
        p = DrawCenter(q);
        Check(Near(p, texel(3).red / 2, texel(3).green / 2, texel(3).blue / 2, 3), "blackColor: base colour black, only the env term remains", Str(p).c_str());
        g_envMesh.blackColor = false;
        FreeQuad(q);
        RwTextureDestroy(envRow);
        RwTextureDestroy(envCol);
        Check(NoGlError("CAR_CAM"), "glGetError clean (CAR_CAM)");
    }

    // ---- CAR_WAVE: second UV set + (u,v) offset, blend by factor ----
    {
        const std::vector<RwRGBA> c = { { 255, 0, 0, 255 }, { 0, 255, 0, 255 }, { 0, 0, 255, 255 }, { 200, 100, 50, 255 } };
        RwTexture* env = MakeTexWH(2, 2, c);
        Quad q = MakeQuad(rpGEOMETRYPRELIT | rpGEOMETRYNORMALS | rpGEOMETRYTEXTURED2, Z, nullptr, { 255, 255, 255, 255 }, { 100, 100, 100, 255 });
        q.atomic->pipeline = pipe;
        RpGeometryLock(q.geo, rpGEOMETRYLOCKTEXCOORDS1);
        for (int i = 0; i < 4; ++i) q.geo->texCoords[1][i] = { (i & 1) ? 1.0f : 0.0f, (i & 2) ? 1.0f : 0.0f };
        RpGeometryUnlock(q.geo);
        g_envMesh = NotsaSaEnvMesh{};
        g_envMesh.mode = NOTSA_SAENV_CAR_WAVE;
        g_envMesh.envTex = env;
        g_envMesh.factor = 0.5f;
        int px[4][2];
        QuadPixels(1.0f, Z, px);
        const char* nm[4] = { "TL", "TR", "BL", "BR" };
        for (int i = 0; i < 4; ++i) {
            Begin();
            RpAtomicRender(q.atomic);
            End();
            const Px p = At(px[i][0], px[i][1]);
            std::snprintf(what, sizeof(what), "CAR_WAVE %s quadrant = mix(prelit 100, env texel (%d,%d,%d), 0.5)", nm[i], c[i].red, c[i].green, c[i].blue);
            Check(Near(p, (100 + c[i].red) / 2, (100 + c[i].green) / 2, (100 + c[i].blue) / 2, 3), what, Str(p).c_str());
        }
        // offset u = 0.5: columns swap (WRAP)
        g_envMesh.env[0] = 0.5f;
        Begin();
        RpAtomicRender(q.atomic);
        End();
        Px p = At(px[0][0], px[0][1]);
        Check(Near(p, (100 + c[1].red) / 2, (100 + c[1].green) / 2, (100 + c[1].blue) / 2, 3), "CAR_WAVE offset u = 0.5 shifts the env map by one texel column (TL shows TR)", Str(p).c_str());
        // offset v = 0.5: rows swap
        g_envMesh.env[0] = 0.0f; g_envMesh.env[1] = 0.5f;
        Begin();
        RpAtomicRender(q.atomic);
        End();
        p = At(px[0][0], px[0][1]);
        Check(Near(p, (100 + c[2].red) / 2, (100 + c[2].green) / 2, (100 + c[2].blue) / 2, 3), "CAR_WAVE offset v = 0.5 (D3D v down) shifts by one texel row (TL shows BL)", Str(p).c_str());
        FreeQuad(q);
        RwTextureDestroy(env);
        Check(NoGlError("CAR_WAVE"), "glGetError clean (CAR_WAVE)");
    }

    // ---- specular: per-vertex D3D FF model, non-local viewer, N.L > 0 only ----
    {
        Quad q = MakeQuad(rpGEOMETRYPRELIT | rpGEOMETRYNORMALS, Z, nullptr, { 255, 255, 255, 255 }, { 0, 0, 0, 255 });
        q.atomic->pipeline = pipe;
        SetNormals(q, { 0.0f, 0.0f, -1.0f });
        auto expect = [&](float lx, float ly, float lz, float spec, float power) { // light vector in world space (towards the light); identity camera: view = (-x, y, z)
            const float ll = std::sqrt(lx * lx + ly * ly + lz * lz);
            const float L[3] = { -lx / ll, ly / ll, lz / ll };
            const float N[3] = { 0.f, 0.f, -1.f };
            const float ndl = N[0] * L[0] + N[1] * L[1] + N[2] * L[2];
            if (ndl <= 0.f) return 0.f;
            float H[3] = { L[0], L[1], L[2] - 1.f };
            const float hl = std::sqrt(H[0] * H[0] + H[1] * H[1] + H[2] * H[2]);
            const float ndh = std::max(0.f, -(H[2] / hl));
            // + g_GameLight is an enabled D3D light too: diffuse 0.25 * N.L (ambient 0.75 * world ambient = 0 here), added before the texture stage
            return std::min(1.f, std::min(1.f, 0.25f * ndl) + std::pow(ndh, power) * spec * 0.65f);
        };
        struct Case { float lx, ly, lz, spec, power; };
        const Case cases[] = { { 0, 0, -1, 0.5f, 10 }, { 0.5f, 0, -1, 0.8f, 20 }, { 0, -0.3f, -1, 1.0f, 50 }, { 0, 0, 1, 1.0f, 10 }, { 1, 0, 0, 1.0f, 10 } };
        for (const Case& k : cases) {
            g_envMesh = NotsaSaEnvMesh{};
            g_envMesh.spec = k.spec; g_envMesh.power = k.power;
            g_envMesh.lightDir[0] = k.lx; g_envMesh.lightDir[1] = k.ly; g_envMesh.lightDir[2] = k.lz;
            const Px p = DrawCenter(q);
            const int e = (int)std::lround(expect(k.lx, k.ly, k.lz, k.spec, k.power) * 255.f);
            std::snprintf(what, sizeof(what), "specular L=(%.1f,%.1f,%.1f) spec %.1f power %.0f: expected %d (D3D FF: Gouraud spec + 0.25 N.L of g_GameLight)", k.lx, k.ly, k.lz, k.spec, k.power, e);
            Check(Near(p, e, e, e, 3), what, Str(p).c_str());
        }
        g_envMesh = NotsaSaEnvMesh{};
        FreeQuad(q);
        Check(NoGlError("specular"), "glGetError clean (specular)");
    }

    // ---- buildings: raw camera-space normal as uv (texture transform disabled), TFACTOR / DIFFUSE modulated ----
    {
        const std::vector<RwRGBA> c = { { 255, 0, 0, 255 }, { 0, 255, 0, 255 }, { 0, 0, 255, 255 }, { 200, 100, 50, 255 } };
        RwTexture* env = MakeTexWH(2, 2, c);
        Quad q = MakeQuad(rpGEOMETRYPRELIT | rpGEOMETRYNORMALS, Z, nullptr, { 255, 255, 255, 255 }, { 0, 0, 0, 255 });
        q.atomic->pipeline = pipe;
        // view-space normal (nx, ny) = (0.25, 0.25) -> TL texel; (0.75, 0.25) TR; (0.25, 0.75) BL; (0.75, 0.75) BR.  World x = -view x
        const float nxy[4][2] = { { 0.25f, 0.25f }, { 0.75f, 0.25f }, { 0.25f, 0.75f }, { 0.75f, 0.75f } };
        const char* nm[4] = { "TL", "TR", "BL", "BR" };
        g_envMesh = NotsaSaEnvMesh{};
        g_envMesh.mode = NOTSA_SAENV_BLD_CAM;
        g_envMesh.envTex = env;
        g_envMesh.factor = 0.5f;
        for (int i = 0; i < 4; ++i) {
            SetNormals(q, { -nxy[i][0], nxy[i][1], -0.5f });
            const Px p = DrawCenter(q);
            std::snprintf(what, sizeof(what), "BLD_CAM raw normal uv (%.2f,%.2f) -> texel %s * 0.5", nxy[i][0], nxy[i][1], nm[i]);
            Check(Near(p, c[i].red / 2, c[i].green / 2, c[i].blue / 2, 3), what, Str(p).c_str());
        }
        // DN variant: env * lit vertex colour: prelit 128 -> + env*128/255 on top of 128
        g_envMesh.mode = NOTSA_SAENV_BLD_CAM_DN;
        SetPrelit(q, { 128, 128, 128, 255 });
        SetNormals(q, { -0.25f, 0.25f, -0.5f });
        Px p = DrawCenter(q);
        Check(Near(p, 128 + 255 * 128 / 255, 128, 128, 3), "BLD_CAM_DN: env texel TL (255,0,0) * prelit 128 added to 128 -> (255,128,128)", Str(p).c_str());
        // NVC mechanism through this pipeline: Lock(PRELIGHT)/Unlock changes the colours (CPU day/night blend), twice
        g_envMesh.mode = NOTSA_SAENV_NONE;
        SetPrelit(q, { 200, 50, 20, 255 });
        p = DrawCenter(q);
        Check(Near(p, 200, 50, 20, 2), "NVC: night colours written under RpGeometryLock(PRELIGHT) reach a plain mesh of the SA pipeline", Str(p).c_str());
        g_envMesh.mode = NOTSA_SAENV_BLD_CAM_DN; // env mesh: the colour stream is re-uploaded too
        SetPrelit(q, { 10, 20, 30, 255 });
        p = DrawCenter(q);
        Check(Near(p, 10 + 255 * 10 / 255, 20, 30, 3), "NVC: second update on an env mesh: prelit (10,20,30) + env TL (255,0,0)*prelit", Str(p).c_str());
        FreeQuad(q);
        RwTextureDestroy(env);
        Check(NoGlError("buildings"), "glGetError clean (buildings)");
    }
    Check(NotsaGl3SaEnvDrawCount() > d0 + 20, "env / specular shader draws were counted");
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// part B: real data
// ---------------------------------------------------------------------------------------------------------------------------------------------
struct ImgEntry { long offset, size; };
using ImgDir = std::map<std::string, ImgEntry>;

uint32_t U32(const uint8_t* p) { uint32_t v; std::memcpy(&v, p, 4); return v; }
uint16_t U16(const uint8_t* p) { uint16_t v; std::memcpy(&v, p, 2); return v; }

bool LoadDir(const std::string& img, ImgDir& out) {
    FILE* f = std::fopen(img.c_str(), "rb");
    if (!f) return false;
    uint8_t hdr[8];
    if (std::fread(hdr, 1, 8, f) != 8 || std::memcmp(hdr, "VER2", 4) != 0) { std::fclose(f); return false; }
    const uint32_t n = U32(hdr + 4);
    std::vector<uint8_t> dir((size_t)n * 32);
    const bool okRead = std::fread(dir.data(), 1, dir.size(), f) == dir.size();
    std::fclose(f);
    if (!okRead) return false;
    for (uint32_t i = 0; i < n; ++i) {
        const uint8_t* e = &dir[(size_t)i * 32];
        char nm[25] = {};
        std::memcpy(nm, e + 8, 24);
        std::string s = nm;
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
        out[s] = { (long)U32(e) * 2048, (long)std::max<unsigned>(U16(e + 4), U16(e + 6)) * 2048 };
    }
    return true;
}

bool ReadEntry(const std::string& img, const ImgEntry& e, std::vector<uint8_t>& buf) {
    FILE* f = std::fopen(img.c_str(), "rb");
    if (!f) return false;
    std::fseek(f, e.offset, SEEK_SET);
    buf.resize((size_t)e.size);
    const size_t got = std::fread(buf.data(), 1, buf.size(), f);
    std::fclose(f);
    buf.resize(got);
    if (buf.size() >= 12) buf.resize(std::min<size_t>(buf.size(), 12 + (size_t)U32(buf.data() + 4)));
    return got > 12;
}

struct ModelStat { int atomics = 0, tris = 0, verts = 0, skinned = 0, prelit = 0, textured = 0, matfx = 0, ok = 0; };

RwSphere g_bs; // of the last clump

ModelStat Describe(RpClump* clump) {
    ModelStat s;
    RwV3d lo{ 1e30f, 1e30f, 1e30f }, hi{ -1e30f, -1e30f, -1e30f };
    RpClumpForAllAtomics(clump, [](RpAtomic* a, void* d) -> RpAtomic* {
        auto*  st  = static_cast<ModelStat*>(d);
        rw::Geometry* g = a->geometry;
        st->atomics++;
        st->tris += g->numTriangles;
        st->verts += g->numVertices;
        if (rw::Skin::get(g)) st->skinned++;
        if (g->flags & rpGEOMETRYPRELIT) st->prelit++;
        for (int m = 0; m < g->matList.numMaterials; ++m) {
            if (g->matList.materials[m]->texture) { st->textured++; }
            if (rw::MatFX::get(g->matList.materials[m])) { st->matfx++; }
        }
        return a;
    }, &s);
    return s;
}

void DumpPpm(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return;
    std::fprintf(f, "P6\n%d %d\n255\n", g_pw, g_ph);
    for (size_t i = 0; i < (size_t)g_pw * g_ph; ++i) std::fwrite(&g_frame[i * 4], 1, 3, f);
    std::fclose(f);
}

// Draws every atomic of a clump with RpAtomicRender; frames the model's bounding sphere. Returns coverage and colour count.
struct DrawResult { double coverage = 0; int colours = 0; bool glClean = true; Px mean{}; };

DrawResult DrawClump(RpClump* clump, float yawDeg, const std::string& dumpPath, float lightAmb, float lightDir, const std::function<void()>& pre = nullptr, const std::function<void()>& drawOverride = nullptr) {
    DrawResult r;
    // bounds from the vertices (atomic frames are identity relative to the clump frame in SA models)
    RwV3d lo{ 1e30f, 1e30f, 1e30f }, hi{ -1e30f, -1e30f, -1e30f };
    RwV3d bb[2] = { lo, hi };
    RpClumpForAllAtomics(clump, [](RpAtomic* a, void* d) -> RpAtomic* {
        rw::Geometry* g = a->geometry;
        auto* b = static_cast<RwV3d*>(d);
        const RwV3d* v = g->morphTargets[0].vertices;
        for (int i = 0; v && i < g->numVertices; ++i) {
            b[0] = { std::min(b[0].x, v[i].x), std::min(b[0].y, v[i].y), std::min(b[0].z, v[i].z) };
            b[1] = { std::max(b[1].x, v[i].x), std::max(b[1].y, v[i].y), std::max(b[1].z, v[i].z) };
        }
        return a;
    }, bb);
    const RwV3d c{ (bb[0].x + bb[1].x) * 0.5f, (bb[0].y + bb[1].y) * 0.5f, (bb[0].z + bb[1].z) * 0.5f };
    const float rad = 0.5f * std::sqrt((bb[1].x - bb[0].x) * (bb[1].x - bb[0].x) + (bb[1].y - bb[0].y) * (bb[1].y - bb[0].y) + (bb[1].z - bb[0].z) * (bb[1].z - bb[0].z));
    const float D   = std::max(rad, 0.5f) * 1.8f;

    // model: Z up -> camera Y up (rotate -90 deg about X), yaw about model Z first, then move the centre to (0, 0, D)
    RwFrame* cf = RpClumpGetFrame(clump);
    const RwV3d zaxis{ 0, 0, 1 }, xaxis{ 1, 0, 0 };
    const RwV3d toOrigin{ -c.x, -c.y, -c.z };
    RwFrameTranslate(cf, &toOrigin, rwCOMBINEREPLACE);
    RwFrameRotate(cf, &zaxis, yawDeg, rwCOMBINEPOSTCONCAT);
    RwFrameRotate(cf, &xaxis, -90.0f, rwCOMBINEPOSTCONCAT);
    const RwV3d toCam{ 0, 0, D };
    RwFrameTranslate(cf, &toCam, rwCOMBINEPOSTCONCAT);
    RwFrameUpdateObjects(cf);
    if (pre) pre();

    RwCameraSetNearClipPlane(g_cam, D * 0.02f);
    RwCameraSetFarClipPlane(g_cam, D * 20.0f);
    SetLights(lightAmb, lightDir);
    NoGlError("before draw");
    Begin({ 40, 60, 90, 255 });
    if (drawOverride) drawOverride();
    else RpClumpForAllAtomics(clump, [](RpAtomic* a, void*) -> RpAtomic* { RpAtomicRender(a); return a; }, nullptr);
    End();
    r.glClean = NoGlError(dumpPath.c_str());
    std::set<uint32_t> cols;
    size_t covered = 0;
    double sr = 0, sg = 0, sb = 0;
    for (size_t i = 0; i < (size_t)g_pw * g_ph; ++i) {
        const uint8_t* p = &g_frame[i * 4];
        if (!(std::abs(p[0] - 40) <= 1 && std::abs(p[1] - 60) <= 1 && std::abs(p[2] - 90) <= 1)) {
            covered++;
            sr += p[0]; sg += p[1]; sb += p[2];
            cols.insert(((uint32_t)(p[0] >> 2) << 12) | ((uint32_t)(p[1] >> 2) << 6) | (p[2] >> 2));
        }
    }
    r.coverage = (double)covered / ((double)g_pw * g_ph);
    r.colours  = (int)cols.size();
    if (covered) r.mean = { (int)(sr / covered), (int)(sg / covered), (int)(sb / covered), 255 };
    if (!dumpPath.empty()) DumpPpm(dumpPath);
    return r;
}

RpClump* LoadClump(const std::vector<uint8_t>& dff) {
    rw::StreamMemory sm;
    sm.open(const_cast<uint8_t*>(dff.data()) + 12, (uint32_t)dff.size() - 12);
    RpClump* c = RpClumpStreamRead(&sm);
    sm.close();
    return c;
}
RwTexDictionary* LoadDict(const std::vector<uint8_t>& txd) {
    rw::StreamMemory sm;
    sm.open(const_cast<uint8_t*>(txd.data()) + 12, (uint32_t)txd.size() - 12);
    RwTexDictionary* d = RwTexDictionaryStreamRead(&sm);
    sm.close();
    return d;
}

// R-9 / R-8 on real geometry: attach the SA env pipeline to every atomic and compare the frame with the stock-pipeline frame
RwTexture* g_realEnv = nullptr;
bool       g_realCar = true;
void RealPrepare(rw::Atomic*, rw::Material* m, NotsaSaEnvMesh& o) {
    o = NotsaSaEnvMesh{};
    if (g_realCar) {
        o.mode = NOTSA_SAENV_CAR_CAM; o.envTex = g_realEnv; o.env[0] = 0.5f; o.env[1] = 0.5f; o.factor = 0.4f;
        o.spec = 0.6f; o.power = 30.0f; o.lightDir[0] = 0.3f; o.lightDir[1] = 0.5f; o.lightDir[2] = -0.8f;
    } else if (m->texture) {
        o.mode = NOTSA_SAENV_BLD_CAM_DN; o.envTex = g_realEnv; o.factor = 0.5f;
    }
}
std::vector<uint8_t> g_baseFrame;

struct Model { const char* img; const char* dff; const char* txd; const char* kind; };

void RunReal(const std::string& gta, const std::string& dumpDir) {
    std::printf("---- R-6 part B: real SA models through RpAtomicRender ----\n");
    const std::string models = gta + "/models";
    ImgDir gta3, player;
    if (!LoadDir(models + "/gta3.img", gta3)) { std::printf("  PIPE note: no gta3.img\n"); return; }
    LoadDir(models + "/player.img", player);
    const Model list[] = {
        { "gta3.img", "sw_diner1.dff", "sw_diner1.txd", "building" },
        { "gta3.img", "farmhouse.dff", "farmhouse.txd", "building" },
        { "gta3.img", "bakery_sfse.dff", "bakery_sfse.txd", "building" },
        { "gta3.img", "cityhall_sfs.dff", "cityhall_sfs.txd", "building" },
        { "gta3.img", "male01.dff", "male01.txd", "ped (skinned)" },
        { "gta3.img", "wmycr.dff", "wmycr.txd", "ped (skinned)" },
        { "player.img", "chinos.dff", nullptr, "player.img clothes (skinned)" },
        { "player.img", "afro.dff", nullptr, "player.img hair" },
        { "player.img", "country.dff", nullptr, "player.img clothes" },
        { "gta3.img", "infernus.dff", "infernus.txd", "car (matfx env)" },
        { "gta3.img", "sentinel.dff", "sentinel.txd", "car (matfx env)" },
    };
    // models/generic/vehicle.txd: the game's shared car env textures (vehicleenvmap128 & co)
    RwTexDictionary* vehDict = nullptr;
    RwTexture*       vehEnv  = nullptr;
    {
        std::vector<uint8_t> buf;
        if (FILE* f = std::fopen((models + "/generic/vehicle.txd").c_str(), "rb")) {
            std::fseek(f, 0, SEEK_END); buf.resize((size_t)std::ftell(f)); std::fseek(f, 0, SEEK_SET);
            buf.resize(std::fread(buf.data(), 1, buf.size(), f)); std::fclose(f);
            if (buf.size() > 12) vehDict = LoadDict(buf);
        }
        if (vehDict) {
            RwTexDictionaryForAllTextures(vehDict, [](RwTexture* t, void* d) -> RwTexture* {
                if (std::strstr(RwTextureGetName(t), "env") && !*static_cast<RwTexture**>(d)) *static_cast<RwTexture**>(d) = t;
                return t;
            }, &vehEnv);
        }
        std::printf("  models/generic/vehicle.txd: %s, env texture %s\n", vehDict ? "loaded" : "not found", vehEnv ? RwTextureGetName(vehEnv) : "(none)");
    }
    int idx = 0;
    for (const Model& m : list) {
        ImgDir& dir = std::strcmp(m.img, "gta3.img") == 0 ? gta3 : player;
        auto it = dir.find(m.dff);
        if (it == dir.end()) { std::printf("  %s: not in %s (skipped)\n", m.dff, m.img); continue; }
        std::vector<uint8_t> dffBuf, txdBuf;
        if (!ReadEntry(models + "/" + m.img, it->second, dffBuf)) { Check(false, m.dff, "read failed"); continue; }
        RwTexDictionary* dict = nullptr;
        if (m.txd) {
            auto jt = dir.find(m.txd);
            if (jt != dir.end() && ReadEntry(models + "/" + m.img, jt->second, txdBuf)) {
                dict = LoadDict(txdBuf);
            }
        }
        RwTexDictionary* prev = RwTexDictionaryGetCurrent();
        if (dict) RwTexDictionarySetCurrent(dict);
        RpClump* clump = LoadClump(dffBuf);
        RwTexDictionarySetCurrent(prev);
        char what[200];
        std::snprintf(what, sizeof(what), "%s [%s]: clump streamed", m.dff, m.kind);
        Check(clump != nullptr, what);
        if (!clump) { if (dict) RwTexDictionaryDestroy(dict); continue; }
        const ModelStat st = Describe(clump);
        std::string dump = dumpDir + "/" + std::string(m.dff) + ".ppm";
        // two views: yaw 30 and 210 degrees (front / back), the best coverage counts (back faces are culled)
        DrawResult best;
        for (float yaw : { 30.0f, 210.0f }) {
            DrawResult r = DrawClump(clump, yaw, yaw < 100.0f ? dump : "", 0.6f, 0.4f);
            if (!r.glClean) best.glClean = false;
            if (r.coverage > best.coverage) { auto gl = best.glClean; best = r; best.glClean = gl && r.glClean; }
        }
        std::snprintf(what, sizeof(what), "%s [%s]: %d atomics %d tris %d verts, skinned %d prelit %d textured mats %d matfx mats %d; coverage %.1f%% colours %d mean (%d,%d,%d)", m.dff, m.kind,
                      st.atomics, st.tris, st.verts, st.skinned, st.prelit, st.textured, st.matfx, best.coverage * 100.0, best.colours, best.mean.r, best.mean.g, best.mean.b);
        Check(best.glClean && best.coverage > 0.002 && best.colours >= 4, what);
        if (!std::strcmp(m.kind, "car (matfx env)") || !std::strcmp(m.kind, "building")) {
            // SA env pipeline on the same model (R-9 cars, R-8 buildings): same view, count the pixels that changed; the env texture is the model's own matfx env texture, else a synthetic checker
            DrawClump(clump, 30.0f, "", 0.6f, 0.4f);
            g_baseFrame = g_frame;
            RwTexture* env = nullptr;
            RpClumpForAllAtomics(clump, [](RpAtomic* a, void* d) -> RpAtomic* {
                for (int k = 0; k < a->geometry->matList.numMaterials; ++k) {
                    rw::MatFX* fx = rw::MatFX::get(a->geometry->matList.materials[k]);
                    if (fx && fx->fx[0].env.tex && !*static_cast<RwTexture**>(d)) *static_cast<RwTexture**>(d) = fx->fx[0].env.tex;
                }
                return a;
            }, &env);
            RwTexture* synth = nullptr;
            if (!env && vehEnv && !std::strcmp(m.kind, "car (matfx env)")) env = vehEnv;
            if (!env) {
                std::vector<RwRGBA> ck;
                for (int i = 0; i < 64; ++i) ck.push_back(((i / 8 + i % 8) & 1) ? RwRGBA{ 255, 230, 200, 255 } : RwRGBA{ 40, 60, 120, 255 });
                synth = env = MakeTexWH(8, 8, ck);
            }
            g_realEnv = env;
            g_realCar = !std::strcmp(m.kind, "car (matfx env)");
            static rw::ObjPipeline* realPipe = NotsaGl3CreateSaEnvPipeline(RealPrepare, 0x9B);
            RpClumpForAllAtomics(clump, [](RpAtomic* a, void*) -> RpAtomic* { a->pipeline = realPipe; return a; }, nullptr);
            const uint32_t d1 = NotsaGl3SaEnvDrawCount();
            DrawResult r2 = DrawClump(clump, 30.0f, dumpDir + "/" + std::string(m.dff) + "_saenv.ppm", 0.6f, 0.4f);
            size_t diff = 0, cov = 0, covBase = 0;
            for (size_t i = 0; i < g_frame.size(); i += 4) {
                if (!(std::abs(g_baseFrame[i] - 40) <= 1 && std::abs(g_baseFrame[i + 1] - 60) <= 1 && std::abs(g_baseFrame[i + 2] - 90) <= 1)) covBase++;
                const int d = std::abs(g_frame[i] - g_baseFrame[i]) + std::abs(g_frame[i + 1] - g_baseFrame[i + 1]) + std::abs(g_frame[i + 2] - g_baseFrame[i + 2]);
                if (d > 12) diff++;
                if (!(std::abs(g_frame[i] - 40) <= 1 && std::abs(g_frame[i + 1] - 60) <= 1 && std::abs(g_frame[i + 2] - 90) <= 1)) cov++;
            }
            std::snprintf(what, sizeof(what), "%s: SA env pipeline (%s env texture %s): %u env draws, %zu of %zu covered pixels changed vs the stock pipeline (%.1f%%), coverage %.1f%% (stock %.1f%%), no GL error", m.dff,
                          g_realCar ? "CAR_CAM + specular" : "BLD_CAM_DN", synth ? "synthetic" : (env == vehEnv ? "vehicle.txd" : "matfx"), NotsaGl3SaEnvDrawCount() - d1, diff, cov, cov ? 100.0 * diff / cov : 0.0, r2.coverage * 100.0, 100.0 * covBase / ((double)g_pw * g_ph));
            Check(r2.glClean && NotsaGl3SaEnvDrawCount() > d1 && cov > 0 && diff * 100 >= cov * 1 && std::fabs((double)cov - (double)covBase) < 0.02 * (double)g_pw * g_ph, what);
            if (synth) RwTextureDestroy(synth);
            g_realEnv = nullptr;
        }
        idx++;
        RpClumpDestroy(clump);
        if (dict) RwTexDictionaryDestroy(dict);
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// R-10 part A: skin pipeline on synthetic skinned quads (hierarchy palette, normals, fog, identity fallback, weights, bone cap)
// ---------------------------------------------------------------------------------------------------------------------------------------------
std::vector<uint8_t> Snap() { return g_frame; }

struct FrameDiff { int covA = 0, covB = 0, bad = 0, maxd = 0; };
bool IsBg(const uint8_t* p, const RwRGBA& bg) { return std::abs(p[0] - bg.red) <= 1 && std::abs(p[1] - bg.green) <= 1 && std::abs(p[2] - bg.blue) <= 1; }
FrameDiff Diff(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b, const RwRGBA& bg, int tol) {
    FrameDiff d;
    for (size_t i = 0; i + 3 < a.size(); i += 4) {
        d.covA += !IsBg(&a[i], bg);
        d.covB += !IsBg(&b[i], bg);
        const int m = std::max({ std::abs(a[i] - b[i]), std::abs(a[i + 1] - b[i + 1]), std::abs(a[i + 2] - b[i + 2]) });
        d.maxd = std::max(d.maxd, m);
        if (m > tol) d.bad++;
    }
    return d;
}

rw::HAnimHierarchy* MakeHier(int n) {
    rw::HAnimHierarchy* h = rw::HAnimHierarchy::create(n, nullptr, nullptr, 0, 36);
    for (int i = 0; i < n; ++i) { h->matrices[i].setIdentity(); h->matrices[i].flags = rw::Matrix::TYPEORTHONORMAL; }
    return h;
}
void SetBone(rw::HAnimHierarchy* h, int i, const rw::Matrix& m) { h->matrices[i] = m; h->matrices[i].flags = rw::Matrix::TYPEORTHONORMAL; }

// skinned quad: weights/indices per vertex (order TL TR BL BR = MakeQuad's), all inverse matrices identity
Quad MakeSkinQuad(float z, RwUInt32 flags, int numBones, const float w[4][4], const uint8_t ix[4][4], uint8_t prelit = 255, float size = 1.0f) {
    Quad q = MakeQuad(flags | rpGEOMETRYNORMALS | rpGEOMETRYLIGHT, z, nullptr, { 255, 255, 255, 255 }, { prelit, prelit, prelit, 255 }, false, size);
    std::vector<rw::Matrix> inv(numBones);
    for (auto& m : inv) { std::memset(&m, 0, sizeof(m)); m.setIdentity(); m.flags = 0; m.pad3 = 0x3F800000u; }
    RwUInt32 idx[4];
    for (int v = 0; v < 4; ++v) std::memcpy(&idx[v], ix[v], 4);
    RpSkin* sk = RpSkinCreate(4, numBones, (RwMatrixWeights*)w, idx, (RwMatrix*)inv.data());
    RpSkinGeometrySetSkin(q.geo, sk);
    RpSkinAtomicSetType(q.atomic, rpSKINTYPEGENERIC);
    return q;
}
const float kW1[4][4] = { { 1, 0, 0, 0 }, { 1, 0, 0, 0 }, { 1, 0, 0, 0 }, { 1, 0, 0, 0 } };
const uint8_t kI0[4][4] = {};

std::vector<uint8_t> RenderAtomic(RpAtomic* a, const RwRGBA& bg) {
    Begin(bg);
    RpAtomicRender(a);
    End();
    return Snap();
}

void SetFog(bool on, float start = 2.0f, float farClip = 8.0f) {
    if (on) {
        RS(rwRENDERSTATEFOGCOLOR, 0xFF0000FFu);
        RS(rwRENDERSTATEFOGTYPE, rwFOGTYPELINEAR);
        RS(rwRENDERSTATEFOGENABLE, 1);
        g_cam->fogPlane = start;
        RwCameraSetFarClipPlane(g_cam, farClip);
    } else {
        RS(rwRENDERSTATEFOGENABLE, 0);
        g_cam->fogPlane = 1.0f;
        RwCameraSetFarClipPlane(g_cam, 100.0f);
    }
}

const RwUInt32 kLitPrelit = rpGEOMETRYPRELIT | rpGEOMETRYNORMALS | rpGEOMETRYLIGHT;

void RunSkinSynthetic() {
    std::printf("---- R-10 part A: gl3 skin pipeline, synthetic skinned quads ----\n");
    char what[240];
    const RwRGBA bg{ 40, 60, 90, 255 };
    const RwV3d xaxis{ 1, 0, 0 };
    rw::gl3::SkinStats& ST = rw::gl3::skinStats;

    // ---- 1. skinned == static (palette, position, normals, lights, fog), fog on / off, near / far ----
    for (int fog = 0; fog <= 1; ++fog) {
        for (float z : { 3.0f, 6.0f }) {
            SetLights(0.1f, 0.3f);
            SetFog(fog != 0);
            Quad st = MakeQuad(kLitPrelit, z, nullptr, { 255, 255, 255, 255 }, { 100, 100, 100, 255 });
            const RwV3d t{ 0.3f, -0.2f, 0.0f };
            RwFrameRotate(st.frame, &xaxis, 35.0f, rwCOMBINEREPLACE);
            RwFrameTranslate(st.frame, &t, rwCOMBINEPOSTCONCAT);
            RwFrameUpdateObjects(st.frame);
            const auto a = RenderAtomic(st.atomic, bg);
            Quad sk = MakeSkinQuad(z, rpGEOMETRYPRELIT, 1, kW1, kI0, 100);
            rw::HAnimHierarchy* h = MakeHier(1);
            SetBone(h, 0, *RwFrameGetLTM(st.frame));
            RpSkinAtomicSetHAnimHierarchy(sk.atomic, h);
            const auto b = RenderAtomic(sk.atomic, bg);
            const FrameDiff d = Diff(a, b, bg, 3);
            // centroid of the covered pixels of the skinned frame (the tilt moves the quad off the screen centre)
            long sx = 0, sy = 0, sn = 0;
            for (int y = 0; y < g_ph; ++y) for (int x = 0; x < g_pw; ++x) if (!IsBg(&b[((size_t)y * g_pw + x) * 4], bg)) { sx += x; sy += y; sn++; }
            const int cx = sn ? (int)(sx / sn) : 0, cy = sn ? (int)(sy / sn) : 0;
            const uint8_t* cp = &b[((size_t)cy * g_pw + cx) * 4];
            const Px pc{ cp[0], cp[1], cp[2], 255 };
            std::snprintf(what, sizeof(what), "skinned == static quad (35 deg tilt + offset, lights 0.1/0.3, fog %s, z %.0f): covered %d vs %d px, %d px differ by > 3, max diff %d, centroid colour %s", fog ? "on" : "off", z, d.covA, d.covB, d.bad, d.maxd, Str(pc).c_str());
            Check(d.covA > 5000 && d.covA == d.covB && d.bad == 0, what);
            // fog in numbers: the lit grey quad has r == g == b without fog; with the blue fog colour b stays and r / g drop
            if (fog) Check(pc.b - pc.r > 30 && pc.b - pc.g > 30, "skinned quad is fogged towards the fog colour (0,0,255)", Str(pc).c_str());
            else Check(std::abs(pc.b - pc.r) <= 4 && pc.r > 100 && pc.r < 255, "skinned quad without fog is lit grey (r == g == b, not saturated)", Str(pc).c_str());
            RpSkinAtomicSetHAnimHierarchy(sk.atomic, nullptr);
            h->destroy();
            FreeQuad(sk);
            FreeQuad(st);
        }
    }
    SetFog(false);
    SetLights(0.0f, 0.0f);
    Check(NoGlError("skin equivalence"), "glGetError clean (skin == static)");

    // ---- 2. per-bone regions: bone 1 (right screen half) moves up by 1, bone 0 (left half) stays ----
    {
        const float w[4][4] = { { 1, 0, 0, 0 }, { 1, 0, 0, 0 }, { 1, 0, 0, 0 }, { 1, 0, 0, 0 } };
        const uint8_t ix[4][4] = { { 0, 0, 0, 0 }, { 1, 0, 0, 0 }, { 0, 0, 0, 0 }, { 1, 0, 0, 0 } }; // TL TR BL BR: world +x = screen left: TL, BL = bone 0
        Quad sk = MakeSkinQuad(4.0f, rpGEOMETRYPRELIT, 2, w, ix);
        rw::HAnimHierarchy* h = MakeHier(2);
        rw::Matrix up; up.setIdentity(); up.pos = { 0.0f, 1.0f, 0.0f }; up.flags = rw::Matrix::TYPEORTHONORMAL;
        SetBone(h, 1, up);
        RpSkinAtomicSetHAnimHierarchy(sk.atomic, h);
        SetLights(0.0f, 0.0f);
        const auto f = RenderAtomic(sk.atomic, bg);
        auto px = [&](float wx, float wy) { // world point on the z = 4 plane
            const int x = (int)(g_pw * 0.5f + (-wx / 4.0f) * g_pw * 0.5f), y = (int)(g_ph * 0.5f - (wy / 4.0f) * g_pw * 0.5f);
            return !IsBg(&f[((size_t)y * g_pw + x) * 4], bg);
        };
        Check(px(0.8f, -0.5f) && !px(0.8f, 1.5f), "bone 0 region (screen left) stays: lower point covered, upper point outside");
        Check(px(-0.8f, 1.5f) && !px(-0.8f, -0.5f), "bone 1 region (screen right) moved up by 1: upper point covered, lower point outside");
        RpSkinAtomicSetHAnimHierarchy(sk.atomic, nullptr);
        h->destroy();
        FreeQuad(sk);
        Check(NoGlError("skin regions"), "glGetError clean (bone regions)");
    }

    // ---- 3. weights that do not sum to 1 are renormalised (0.5 total weight must not shrink the mesh towards the origin) ----
    {
        const float w[4][4] = { { 0.5f, 0, 0, 0 }, { 0.25f, 0.25f, 0, 0 }, { 0.5f, 0, 0, 0 }, { 0.3f, 0.3f, 0, 0 } };
        const uint8_t ix[4][4] = { { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 } };
        const int n0 = ST.geosWeightsRenormalised;
        SetLights(0.3f, 0.5f);
        Quad ref = MakeQuad(kLitPrelit, 4.0f, nullptr, { 255, 255, 255, 255 }, { 255, 255, 255, 255 });
        const auto a = RenderAtomic(ref.atomic, bg);
        Quad sk = MakeSkinQuad(4.0f, rpGEOMETRYPRELIT, 1, w, ix);
        const auto b = RenderAtomic(sk.atomic, bg); // no hierarchy: identity palette
        const FrameDiff d = Diff(a, b, bg, 3);
        std::snprintf(what, sizeof(what), "weights summing to 0.5 / 0.5 / 0.5 / 0.6 are renormalised: covered %d vs %d px, %d differ, renormalised-geometry counter +%d", d.covA, d.covB, d.bad, ST.geosWeightsRenormalised - n0);
        Check(d.covA > 5000 && d.covA == d.covB && d.bad == 0 && ST.geosWeightsRenormalised == n0 + 1, what);
        FreeQuad(sk);
        FreeQuad(ref);
        SetLights(0.0f, 0.0f);
        Check(NoGlError("skin weights"), "glGetError clean (weights)");
    }

    // ---- 4. no hierarchy -> identity palette (bind pose), no crash ----
    {
        const int n0 = ST.drawsNoHierarchy;
        Quad ref = MakeQuad(kLitPrelit, 4.0f, nullptr, { 255, 255, 255, 255 }, { 255, 255, 255, 255 });
        SetLights(0.3f, 0.5f);
        const auto a = RenderAtomic(ref.atomic, bg);
        Quad sk = MakeSkinQuad(4.0f, rpGEOMETRYPRELIT, 3, kW1, kI0);
        const auto b = RenderAtomic(sk.atomic, bg);
        const FrameDiff d = Diff(a, b, bg, 3);
        std::snprintf(what, sizeof(what), "skinned atomic without hierarchy renders in bind pose (identity palette): covered %d vs %d, %d differ, draws +%d", d.covA, d.covB, d.bad, ST.drawsNoHierarchy - n0);
        Check(d.covA > 5000 && d.covA == d.covB && d.bad == 0 && ST.drawsNoHierarchy == n0 + 1, what);
        FreeQuad(sk);
        FreeQuad(ref);
        Check(NoGlError("skin no hierarchy"), "glGetError clean (no hierarchy)");
    }

    // ---- 5. hierarchy smaller than the skin: the missing bones are identity, never the previous draw's palette ----
    {
        const uint8_t ix[4][4] = { { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 2, 0, 0, 0 }, { 2, 0, 0, 0 } }; // bottom vertices on bone 2
        // first draw: a 3-node hierarchy that moves bone 2 far away -> its matrix is now in the (static) upload buffer
        Quad a = MakeSkinQuad(4.0f, rpGEOMETRYPRELIT, 3, kW1, ix);
        rw::HAnimHierarchy* h3 = MakeHier(3);
        rw::Matrix far; far.setIdentity(); far.pos = { 0.0f, 0.0f, -30.0f }; far.flags = rw::Matrix::TYPEORTHONORMAL;
        SetBone(h3, 2, far);
        RpSkinAtomicSetHAnimHierarchy(a.atomic, h3);
        SetLights(0.0f, 0.0f);
        const auto fa = RenderAtomic(a.atomic, bg);
        Quad full = MakeQuad(kLitPrelit, 4.0f, nullptr, { 255, 255, 255, 255 }, { 255, 255, 255, 255 });
        const int covFull = Diff(RenderAtomic(full.atomic, bg), fa, bg, 0).covA;
        const int covMoved = Diff(fa, fa, bg, 0).covA;
        std::snprintf(what, sizeof(what), "bone 2 moved behind the camera: part of the quad is gone (%d of %d px)", covMoved, covFull);
        Check(covMoved < covFull - 2000, what);
        FreeQuad(full);
        // second draw: same skin, 1-node hierarchy
        const int m0 = ST.drawsHierMismatch;
        Quad b = MakeSkinQuad(4.0f, rpGEOMETRYPRELIT, 3, kW1, ix);
        rw::HAnimHierarchy* h1 = MakeHier(1);
        RpSkinAtomicSetHAnimHierarchy(b.atomic, h1);
        const auto fb = RenderAtomic(b.atomic, bg);
        Quad ref = MakeQuad(kLitPrelit, 4.0f, nullptr, { 255, 255, 255, 255 }, { 255, 255, 255, 255 });
        const auto fr = RenderAtomic(ref.atomic, bg);
        const FrameDiff d = Diff(fb, fr, bg, 3);
        std::snprintf(what, sizeof(what), "1-node hierarchy on a 3-bone skin: bones 1-2 identity (no stale palette): covered %d vs %d, %d differ, mismatch counter +%d", d.covA, d.covB, d.bad, ST.drawsHierMismatch - m0);
        Check(d.covB > 5000 && d.covA == d.covB && d.bad == 0 && ST.drawsHierMismatch == m0 + 1, what);
        for (Quad* q : { &a, &b }) RpSkinAtomicSetHAnimHierarchy(q->atomic, nullptr);
        h3->destroy(); h1->destroy();
        FreeQuad(a); FreeQuad(b); FreeQuad(ref);
        Check(NoGlError("skin short hierarchy"), "glGetError clean (short hierarchy)");
    }

    // ---- 6. more than 64 bones: no overflow of the palette, indices >= 64 / >= numBones are bone 0 ----
    {
        const uint8_t ix[4][4] = { { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 69, 0, 0, 0 }, { 200, 0, 0, 0 } };
        const int c0 = ST.geosOverBoneCap, i0 = ST.geosIndexClamped;
        Quad sk = MakeSkinQuad(4.0f, rpGEOMETRYPRELIT, 70, kW1, ix);
        rw::HAnimHierarchy* h = MakeHier(70);
        RpSkinAtomicSetHAnimHierarchy(sk.atomic, h);
        SetLights(0.3f, 0.5f);
        const auto b = RenderAtomic(sk.atomic, bg);
        Quad ref = MakeQuad(kLitPrelit, 4.0f, nullptr, { 255, 255, 255, 255 }, { 255, 255, 255, 255 });
        const auto a = RenderAtomic(ref.atomic, bg);
        const FrameDiff d = Diff(a, b, bg, 3);
        std::snprintf(what, sizeof(what), "70-bone skin with indices 69 / 200: > 64 bones counted (+%d), indices clamped (+%d), identity palette, covered %d vs %d, %d differ", ST.geosOverBoneCap - c0, ST.geosIndexClamped - i0, d.covA, d.covB, d.bad);
        Check(d.covA > 5000 && d.covA == d.covB && d.bad == 0 && ST.geosOverBoneCap == c0 + 1 && ST.geosIndexClamped == i0 + 1, what);
        RpSkinAtomicSetHAnimHierarchy(sk.atomic, nullptr);
        h->destroy();
        FreeQuad(sk); FreeQuad(ref);
        SetLights(0.0f, 0.0f);
        Check(NoGlError("skin 70 bones"), "glGetError clean (70 bones)");
    }

    // ---- 7. bone weights / indices rewritten under RpGeometryLock after the first draw (CClothesBuilder::BlendGeometry does this) reach the GPU ----
    {
        const float w[4][4] = { { 1, 0, 0, 0 }, { 1, 0, 0, 0 }, { 1, 0, 0, 0 }, { 1, 0, 0, 0 } };
        const uint8_t ix0[4][4] = {};
        Quad sk = MakeSkinQuad(4.0f, rpGEOMETRYPRELIT, 2, w, ix0);
        rw::HAnimHierarchy* h = MakeHier(2);
        rw::Matrix up; up.setIdentity(); up.pos = { 0.0f, 1.0f, 0.0f }; up.flags = rw::Matrix::TYPEORTHONORMAL;
        SetBone(h, 1, up);
        RpSkinAtomicSetHAnimHierarchy(sk.atomic, h);
        SetLights(0.0f, 0.0f);
        const auto f0 = RenderAtomic(sk.atomic, bg);
        auto covered = [&](const std::vector<uint8_t>& f, float wx, float wy) {
            const int x = (int)(g_pw * 0.5f + (-wx / 4.0f) * g_pw * 0.5f), y = (int)(g_ph * 0.5f - (wy / 4.0f) * g_pw * 0.5f);
            return !IsBg(&f[((size_t)y * g_pw + x) * 4], bg);
        };
        const bool before = covered(f0, -0.8f, -0.5f) && !covered(f0, -0.8f, 1.5f);
        rw::Skin* skin = rw::Skin::get(sk.geo);
        // (a) LOCKALL like CClothesBuilder: the mesh header is rebuilt -> full re-instance
        int i0 = ST.geosInstanced, r0 = ST.geosRefreshed;
        RpGeometryLock(sk.geo, rpGEOMETRYLOCKALL);
        skin->indices[4 * 1] = 1; // TR, BR -> bone 1
        skin->indices[4 * 3] = 1;
        RpGeometryUnlock(sk.geo);
        const auto f1 = RenderAtomic(sk.atomic, bg);
        const bool after = covered(f1, -0.8f, 1.5f) && !covered(f1, -0.8f, -0.5f);
        std::snprintf(what, sizeof(what), "indices rewritten under LOCKALL after the first draw (CClothesBuilder): right half moves up (re-instances +%d, refreshes +%d)", ST.geosInstanced - i0, ST.geosRefreshed - r0);
        Check(before && after, what);
        // (b) LOCKVERTICES only: the instance callback refreshes the weight / index streams
        i0 = ST.geosInstanced; r0 = ST.geosRefreshed;
        RpGeometryLock(sk.geo, rpGEOMETRYLOCKVERTICES);
        skin->indices[4 * 1] = 0;
        skin->indices[4 * 3] = 0;
        RpGeometryUnlock(sk.geo);
        const auto f2 = RenderAtomic(sk.atomic, bg);
        const bool back = covered(f2, -0.8f, -0.5f) && !covered(f2, -0.8f, 1.5f);
        std::snprintf(what, sizeof(what), "indices rewritten under LOCKVERTICES reach the GPU (right half back down), re-instances +%d, stream refreshes +%d", ST.geosInstanced - i0, ST.geosRefreshed - r0);
        Check(back && ST.geosRefreshed == r0 + 1 && ST.geosInstanced == i0, what);
        RpSkinAtomicSetHAnimHierarchy(sk.atomic, nullptr);
        h->destroy();
        FreeQuad(sk);
        Check(NoGlError("skin relock"), "glGetError clean (re-lock)");
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// R-10 part B: real peds. Rest pose through the palette == bind pose (identity fallback); a bent pose on the GPU == the same pose skinned on the CPU
// ---------------------------------------------------------------------------------------------------------------------------------------------
RwFrame* FindFrameById(RwFrame* root, int id) {
    struct Ctx { int id; RwFrame* found; } c{ id, nullptr };
    std::function<void(RwFrame*)> rec = [&](RwFrame* f) {
        if (c.found) return;
        rw::HAnimData* hd = rw::HAnimData::get(f);
        if (hd && hd->id == id) { c.found = f; return; }
        for (RwFrame* ch = f->child; ch; ch = ch->next) rec(ch);
    };
    rec(root);
    return c.found;
}

rw::Matrix Mul(const rw::Matrix& a, const rw::Matrix& b) { rw::Matrix r; rw::Matrix::mult(&r, &a, &b); return r; }
rw::V3d XfP(const rw::Matrix& m, const rw::V3d& p) { return { p.x * m.right.x + p.y * m.up.x + p.z * m.at.x + m.pos.x, p.x * m.right.y + p.y * m.up.y + p.z * m.at.y + m.pos.y, p.x * m.right.z + p.y * m.up.z + p.z * m.at.z + m.pos.z }; }
rw::V3d XfN(const rw::Matrix& m, const rw::V3d& p) { return { p.x * m.right.x + p.y * m.up.x + p.z * m.at.x, p.x * m.right.y + p.y * m.up.y + p.z * m.at.y, p.x * m.right.z + p.y * m.up.z + p.z * m.at.z }; }

// CPU skinning of one atomic with the palette formula of the exe / librw, returned as a plain static atomic that shares the frame
RpAtomic* MakeCpuSkinned(RpAtomic* src, rw::HAnimHierarchy* hier) {
    rw::Geometry* g = src->geometry;
    rw::Skin* sk = rw::Skin::get(g);
    std::vector<rw::Matrix> pal(sk->numBones);
    rw::Matrix invAtm;
    rw::Matrix::invert(&invAtm, src->getFrame()->getLTM());
    for (int i = 0; i < sk->numBones; ++i) {
        rw::Matrix inv = reinterpret_cast<rw::Matrix*>(sk->inverseMatrices)[i];
        inv.flags = 0;
        if (hier->flags & rw::HAnimHierarchy::LOCALSPACEMATRICES) pal[i] = Mul(inv, hier->matrices[i]);
        else pal[i] = Mul(inv, Mul(hier->matrices[i], invAtm));
    }
    const RwUInt32 fl = (g->flags & (rpGEOMETRYPRELIT | rpGEOMETRYNORMALS | rpGEOMETRYLIGHT | rpGEOMETRYMODULATEMATERIALCOLOR | rpGEOMETRYTEXTURED | rpGEOMETRYTEXTURED2)) | rpGEOMETRYPOSITIONS;
    RpGeometry* ng = RpGeometryCreate(g->numVertices, g->numTriangles, fl);
    RpGeometryLock(ng, rpGEOMETRYLOCKALL);
    const rw::V3d* sp = g->morphTargets[0].vertices;
    const rw::V3d* sn = g->morphTargets[0].normals;
    for (int v = 0; v < g->numVertices; ++v) {
        rw::V3d p{ 0, 0, 0 }, n{ 0, 0, 0 };
        for (int k = 0; k < 4; ++k) {
            const float w = sk->weights[v * 4 + k];
            if (w == 0.0f) continue;
            const rw::Matrix& m = pal[sk->indices[v * 4 + k]];
            const rw::V3d q = XfP(m, sp[v]);
            p = { p.x + q.x * w, p.y + q.y * w, p.z + q.z * w };
            if (sn) { const rw::V3d r = XfN(m, sn[v]); n = { n.x + r.x * w, n.y + r.y * w, n.z + r.z * w }; }
        }
        ng->morphTargets[0].vertices[v] = p;
        if (sn && ng->morphTargets[0].normals) ng->morphTargets[0].normals[v] = n;
        if (g->colors && ng->colors) ng->colors[v] = g->colors[v];
        for (int t = 0; t < g->numTexCoordSets && t < ng->numTexCoordSets; ++t) ng->texCoords[t][v] = g->texCoords[t][v];
    }
    for (int t = 0; t < g->numTriangles; ++t) {
        RpTriangle* tri = &ng->triangles[t];
        RpGeometryTriangleSetVertexIndices(ng, tri, g->triangles[t].v[0], g->triangles[t].v[1], g->triangles[t].v[2]);
        RpGeometryTriangleSetMaterial(ng, tri, g->matList.materials[g->triangles[t].matId]);
    }
    RwSphere bs{};
    RpMorphTargetCalcBoundingSphere(&ng->morphTargets[0], &bs);
    ng->morphTargets[0].boundingSphere = bs;
    RpGeometryUnlock(ng);
    RpAtomic* na = RpAtomicCreate();
    RpAtomicSetFrame(na, src->getFrame());
    RpAtomicSetGeometry(na, ng, 0);
    RpGeometryDestroy(ng); // the atomic keeps its reference
    return na;
}

void RunSkinReal(const std::string& gta, const std::string& dumpDir) {
    std::printf("---- R-10 part B: real peds, palette from the DFF's HAnim hierarchy ----\n");
    const std::string models = gta + "/models";
    ImgDir gta3;
    if (!LoadDir(models + "/gta3.img", gta3)) { std::printf("  PIPE note: no gta3.img\n"); return; }
    const char* names[][2] = { { "male01.dff", "male01.txd" }, { "wmycr.dff", "wmycr.txd" }, { "player.dff", "player.txd" }, { "bmycg.dff", "bmycg.txd" } };
    rw::gl3::SkinStats& ST = rw::gl3::skinStats;
    for (auto& nm : names) {
        auto it = gta3.find(nm[0]);
        if (it == gta3.end()) { std::printf("  %s: not in gta3.img (skipped)\n", nm[0]); continue; }
        std::vector<uint8_t> dffBuf, txdBuf;
        if (!ReadEntry(models + "/gta3.img", it->second, dffBuf)) continue;
        RwTexDictionary* dict = nullptr;
        auto jt = gta3.find(nm[1]);
        if (jt != gta3.end() && ReadEntry(models + "/gta3.img", jt->second, txdBuf)) dict = LoadDict(txdBuf);
        RwTexDictionary* prev = RwTexDictionaryGetCurrent();
        if (dict) RwTexDictionarySetCurrent(dict);
        RpClump* clump = LoadClump(dffBuf);
        RwTexDictionarySetCurrent(prev);
        if (!clump) { Check(false, nm[0], "stream failed"); continue; }
        char what[300];
        rw::HAnimHierarchy* hier = rw::HAnimHierarchy::get(clump);
        std::vector<RpAtomic*> skinned;
        RpClumpForAllAtomics(clump, [](RpAtomic* a, void* d) -> RpAtomic* { if (rw::Skin::get(a->geometry)) static_cast<std::vector<RpAtomic*>*>(d)->push_back(a); return a; }, &skinned);
        std::snprintf(what, sizeof(what), "%s: %zu skinned atomics, hierarchy %s%d nodes", nm[0], skinned.size(), hier ? "" : "MISSING ", hier ? (int)hier->numNodes : 0);
        Check(hier && !skinned.empty() && hier->numNodes <= 64, what);
        if (!hier || skinned.empty()) { RpClumpDestroy(clump); if (dict) RwTexDictionaryDestroy(dict); continue; }
        // frame of each node
        std::vector<RwFrame*> nf(hier->numNodes, nullptr);
        int missing = 0;
        for (int i = 0; i < hier->numNodes; ++i) {
            nf[i] = hier->nodeInfo[i].frame ? hier->nodeInfo[i].frame : FindFrameById(RpClumpGetFrame(clump), hier->nodeInfo[i].id);
            if (!nf[i]) missing++;
        }
        Check(missing == 0, "  every hierarchy node maps to a frame of the clump", std::to_string(missing).c_str());
        if (missing) { RpClumpDestroy(clump); if (dict) RwTexDictionaryDestroy(dict); continue; }
        const int nb = hier->numNodes;
        std::vector<rw::Matrix> rest(nb);
        auto loadRest = [&]() { for (int i = 0; i < nb; ++i) { rest[i] = *RwFrameGetLTM(nf[i]); rest[i].flags = 0; hier->matrices[i] = rest[i]; } };
        auto loadPosed = [&]() {
            loadRest();
            for (int i = 0; i < nb; ++i) {
                const rw::V3d ax = (i % 3 == 0) ? rw::V3d{ 1, 0, 0 } : (i % 3 == 1) ? rw::V3d{ 0, 1, 0 } : rw::V3d{ 0, 0, 1 };
                rw::Matrix r; r.setIdentity(); r.flags = 0; r.rotate(&ax, 35.0f * std::sin(1.7f * (float)i + 0.3f), rw::COMBINEREPLACE); r.flags = 0;
                hier->matrices[i] = Mul(r, rest[i]); // bend about the bone's own origin, then the rest transform
                hier->matrices[i].flags = 0;
            }
        };
        const std::string dump = dumpDir + "/" + nm[0];
        // A: identity palette (hierarchy not set)
        for (RpAtomic* a : skinned) rw::Skin::setHierarchy(a, nullptr);
        const int noH0 = ST.drawsNoHierarchy;
        DrawResult ra = DrawClump(clump, 30.0f, dump + "_skin_bind_identity.ppm", 0.6f, 0.4f);
        const auto fA = Snap();
        const int noHDraws = ST.drawsNoHierarchy - noH0;
        // B: hierarchy with the rest pose from the frame LTMs
        for (RpAtomic* a : skinned) rw::Skin::setHierarchy(a, hier);
        DrawResult rb = DrawClump(clump, 30.0f, dump + "_skin_bind_hier.ppm", 0.6f, 0.4f, loadRest);
        const auto fB = Snap();
        const RwRGBA bg{ 40, 60, 90, 255 };
        const FrameDiff dAB = Diff(fA, fB, bg, 10);
        std::snprintf(what, sizeof(what), "  rest-pose palette == identity palette: covered %d vs %d px, %d px differ by > 10 (%.2f%%), %d nodes, %d identity-fallback draws", dAB.covA, dAB.covB, dAB.bad, dAB.covA ? 100.0 * dAB.bad / dAB.covA : 0.0, nb, noHDraws);
        Check(ra.glClean && rb.glClean && dAB.covA > 100 && dAB.bad * 200 <= dAB.covA && std::abs(dAB.covA - dAB.covB) * 100 <= dAB.covA, what);
        // C: posed on the GPU vs the same pose skinned on the CPU and drawn with the default pipeline
        DrawResult rc = DrawClump(clump, 30.0f, dump + "_skin_posed.ppm", 0.6f, 0.4f, loadPosed);
        const auto fC = Snap();
        std::vector<RpAtomic*> ref;
        DrawResult rd = DrawClump(clump, 30.0f, dump + "_skin_posed_cpu.ppm", 0.6f, 0.4f, [&]() {
            loadPosed();
            for (RpAtomic* a : skinned) ref.push_back(MakeCpuSkinned(a, hier));
        }, [&]() { for (RpAtomic* a : ref) RpAtomicRender(a); });
        const auto fD = Snap();
        const FrameDiff dCD = Diff(fC, fD, bg, 12);
        const FrameDiff dAC = Diff(fA, fC, bg, 12);
        std::snprintf(what, sizeof(what), "  posed (all %d bones bent) GPU skin == CPU skin: covered %d vs %d px, %d px differ by > 12 (%.2f%%); the pose itself moved %d px vs bind", nb, dCD.covA, dCD.covB, dCD.bad, dCD.covA ? 100.0 * dCD.bad / dCD.covA : 0.0, dAC.bad);
        Check(rc.glClean && rd.glClean && dCD.covA > 100 && dCD.bad * 200 <= dCD.covA && dAC.bad * 100 >= dCD.covA, what);
        for (RpAtomic* a : ref) { RpGeometry* gg = a->geometry; RpAtomicDestroy(a); (void)gg; }
        // a few more yaws on the GPU path only: no GL error
        for (float yaw : { 120.0f, 210.0f, 300.0f }) {
            DrawResult r = DrawClump(clump, yaw, "", 0.6f, 0.4f, loadPosed);
            Check(r.glClean && r.coverage > 0.0, "  posed, other yaw: drawn, no GL error");
        }
        RpClumpDestroy(clump);
        if (dict) RwTexDictionaryDestroy(dict);
    }
    Check(NoGlError("skin real"), "glGetError clean (real peds)");
    std::printf("  skin stats: geometries instanced %d, weights renormalised %d, indices clamped %d, over 64 bones %d, draws without hierarchy %d, hierarchy size mismatches %d\n",
                ST.geosInstanced, ST.geosWeightsRenormalised, ST.geosIndexClamped, ST.geosOverBoneCap, ST.drawsNoHierarchy, ST.drawsHierMismatch);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// R-10: skinned-ped audit on the real data (every skinned DFF of gta3.img and player.img): bone count vs the 64-bone cap of the gl3 skin shader,
// weight sums, bone indices vs numBones, hierarchy presence / size
// ---------------------------------------------------------------------------------------------------------------------------------------------
struct SkinAuditAgg {
    int files = 0, clumps = 0, skinnedAtomics = 0, noHier = 0, hierMismatch = 0, over64 = 0, badIdxW = 0, badIdxZeroW = 0, zeroSum = 0, nanW = 0, negW = 0, offSum = 0, offSum1e2 = 0, ungroupedHier = 0;
    long long verts = 0;
    int maxBones = 0, maxUsed = 0, maxNodes = 0;
    double minSum = 1e9, maxSum = -1e9, worstDev = 0;
    std::string worstFile;
    std::vector<std::string> notes;
};
struct SkinAuditCtx { SkinAuditAgg* a; const char* file; rw::HAnimHierarchy* hier; };

void SkinAuditAtomic(RpAtomic* at, SkinAuditCtx* c) {
    SkinAuditAgg& A = *c->a;
    rw::Geometry* g = at->geometry;
    rw::Skin* sk = rw::Skin::get(g);
    if (!sk) return;
    A.skinnedAtomics++;
    A.maxBones = std::max(A.maxBones, (int)sk->numBones);
    A.maxUsed = std::max(A.maxUsed, (int)sk->numUsedBones);
    if (sk->numBones > 64) { A.over64++; A.notes.push_back(std::string(c->file) + ": numBones " + std::to_string(sk->numBones) + " > 64"); }
    if (!c->hier) A.noHier++;
    else {
        A.maxNodes = std::max(A.maxNodes, (int)c->hier->numNodes);
        if (c->hier->numNodes != sk->numBones) { A.hierMismatch++; A.notes.push_back(std::string(c->file) + ": hierarchy nodes " + std::to_string(c->hier->numNodes) + " != skin bones " + std::to_string(sk->numBones)); }
    }
    int offHere = 0;
    for (int v = 0; v < g->numVertices; ++v) {
        const float* w = &sk->weights[v * 4];
        const uint8_t* ix = &sk->indices[v * 4];
        double sum = 0; bool bad = false;
        for (int k = 0; k < 4; ++k) {
            if (std::isnan(w[k]) || std::isinf(w[k])) { A.nanW++; bad = true; continue; }
            if (w[k] < 0.0f) A.negW++;
            sum += w[k];
            if (ix[k] >= sk->numBones) { if (w[k] != 0.0f) A.badIdxW++; else A.badIdxZeroW++; }
        }
        A.verts++;
        if (bad) continue;
        A.minSum = std::min(A.minSum, sum); A.maxSum = std::max(A.maxSum, sum);
        if (sum == 0.0) A.zeroSum++;
        const double dev = std::fabs(sum - 1.0);
        if (dev > A.worstDev) { A.worstDev = dev; A.worstFile = c->file; }
        if (dev > 1e-3) { A.offSum++; offHere++; }
        if (dev > 1e-2) A.offSum1e2++;
    }
    (void)offHere;
}

void RunSkinAudit(const std::string& gta) {
    std::printf("---- R-10 audit: skinned DFFs of gta3.img / player.img ----\n");
    const std::string models = gta + "/models";
    SkinAuditAgg A;
    for (const char* imgName : { "gta3.img", "player.img" }) {
        ImgDir dir;
        if (!LoadDir(models + "/" + imgName, dir)) continue;
        SkinAuditAgg I; // per image
        for (auto& [name, ent] : dir) {
            if (name.size() < 5 || name.compare(name.size() - 4, 4, ".dff") != 0) continue;
            std::vector<uint8_t> buf;
            if (!ReadEntry(models + "/" + imgName, ent, buf)) continue;
            // cheap prefilter: the skin plugin chunk id (0x116) in an extension
            static const uint8_t kSkinTag[4] = { 0x16, 0x01, 0x00, 0x00 };
            const bool has = memmem(buf.data() + 12, buf.size() - 12, kSkinTag, 4) != nullptr;
            if (!has) continue;
            RpClump* clump = LoadClump(buf);
            if (!clump) { A.notes.push_back(name + ": clump failed to stream"); continue; }
            SkinAuditCtx c{ &A, name.c_str(), rw::HAnimHierarchy::get(clump) };
            const int before = A.skinnedAtomics;
            RpClumpForAllAtomics(clump, [](RpAtomic* a, void* d) -> RpAtomic* { SkinAuditAtomic(a, static_cast<SkinAuditCtx*>(d)); return a; }, &c);
            if (A.skinnedAtomics > before) { A.files++; A.clumps++; }
            RpClumpDestroy(clump);
        }
    }
    std::printf("  skinned clumps %d, skinned atomics %d, vertices %lld\n", A.clumps, A.skinnedAtomics, A.verts);
    std::printf("  bones: max numBones %d (cap 64), max usedBones %d, hierarchy max nodes %d; atomics over the cap: %d\n", A.maxBones, A.maxUsed, A.maxNodes, A.over64);
    std::printf("  hierarchy: atomics whose clump has no hierarchy %d, hierarchy.numNodes != skin.numBones %d\n", A.noHier, A.hierMismatch);
    std::printf("  weights: sum min %.6f max %.6f worst |sum-1| %.2e (%s); |sum-1|>1e-3: %d, >1e-2: %d, sum==0: %d, negative: %d, NaN/Inf: %d\n", A.minSum, A.maxSum, A.worstDev, A.worstFile.c_str(), A.offSum, A.offSum1e2, A.zeroSum, A.negW, A.nanW);
    std::printf("  indices >= numBones: with weight>0 %d, with weight==0 %d\n", A.badIdxW, A.badIdxZeroW);
    for (size_t i = 0; i < A.notes.size() && i < 12; ++i) std::printf("  note: %s\n", A.notes[i].c_str());
    Check(A.clumps > 50, "R-10 audit: found skinned DFFs", std::to_string(A.clumps).c_str());
    Check(A.over64 == 0, "R-10 audit: no skin has more than 64 bones (gl3 skin shader cap)");
    Check(A.badIdxW == 0, "R-10 audit: no vertex has a weighted bone index >= numBones");
    Check(A.nanW == 0 && A.negW == 0, "R-10 audit: no NaN / negative weights");
    // weights are renormalised at instance time by the gl3 skin pipeline (R-10), so a sum that is off is a note, not a failure
}
} // namespace

int main() {
    setvbuf(stdout, nullptr, _IONBF, 0);
    const std::string home = std::getenv("HOME") ? std::getenv("HOME") : "";
    const char* gtaEnv = std::getenv("GTA");
    const std::string gta = gtaEnv ? gtaEnv : home + "/Library/Application Support/Steam/steamapps/common/grand theft auto - san andreas/Grand Theft Auto - San Andreas.app/Contents/Resources/transgaming/c_drive/Program Files/Rockstar Games/GTA San Andreas";
    const char* dumpEnv = std::getenv("NOTSA_PIPE_DUMP");
    const std::string dumpDir = dumpEnv ? dumpEnv : "build/pipe_test";
    struct stat st {};
    const bool haveData = stat((gta + "/models/gta3.img").c_str(), &st) == 0;

    if (!std::getenv("NOTSA_STANDALONE_WINDOWED")) setenv("NOTSA_STANDALONE_WINDOWED", "640x480", 1);
    if (!SDL_Init(SDL_INIT_VIDEO)) { std::printf("PIPE RESULT SKIP: no display session (SDL_Init: %s)\n", SDL_GetError()); return 0; }
    SDL_Window* win = SDL_CreateWindow("gta-reversed pipe test", 800, 600, SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL);
    if (!win) { std::printf("PIPE RESULT SKIP: no window (SDL_CreateWindow: %s)\n", SDL_GetError()); return 0; }
    Check(RwEngineInit(nullptr, 0, 0) == TRUE, "RwEngineInit");
    // the plugins the game attaches (RsRwInitialize order) before RwEngineOpen
    Check(RpHAnimPluginAttach() && RpSkinPluginAttach() && RpMatFXPluginAttach(), "RpHAnim / RpSkin / RpMatFX plugins attached");
    RwEngineOpenParams op{};
    op.displayID = win;
    Check(RwEngineOpen(&op) == TRUE, "RwEngineOpen");
    const int wvm = NotsaRwWindowedVideoMode();
    Check(RwEngineSetVideoMode(wvm >= 0 ? wvm : 0) == TRUE, "RwEngineSetVideoMode");
    Check(RwEngineStart() == TRUE, "RwEngineStart");
    if (g_fail) { std::printf("PIPE RESULT FAIL: engine did not start\n"); return 1; }
    Check(NotsaGl3StockPipelinesReady(), "stock gl3 default / skin / matfx pipelines exist after RwEngineStart (RwShimPipelineEnsure)");
    Check(rw::gl3::d3dTexelOrder == 1, "d3d texel order flag set");
    SDL_GetWindowSizeInPixels(win, &g_pw, &g_ph);

    g_cam = RwCameraCreate();
    g_cam->setFrame(RwFrameCreate());
    RwCameraSetNearClipPlane(g_cam, 0.1f);
    RwCameraSetFarClipPlane(g_cam, 100.0f);
    const RwV2d vw{ 1.0f, 0.75f };
    RwCameraSetViewWindow(g_cam, &vw);
    g_cam->frameBuffer = RwRasterCreate(g_pw, g_ph, 0, rwRASTERTYPECAMERA);
    g_cam->zBuffer     = RwRasterCreate(g_pw, g_ph, 0, rwRASTERTYPEZBUFFER);
    RwBBox bb{ { 1000, 1000, 1000 }, { -1000, -1000, -1000 } };
    g_world = RpWorldCreate(&bb);
    RpWorldAddCamera(g_world, g_cam);
    g_ambient = RpLightCreate(rpLIGHTAMBIENT);
    g_dir     = RpLightCreate(rpLIGHTDIRECTIONAL);
    RpLightSetFrame(g_dir, RwFrameCreate());
    RpLightSetFrame(g_ambient, RwFrameCreate());
    RpWorldAddLight(g_world, g_ambient);
    RpWorldAddLight(g_world, g_dir);
    SetLights(0.0f, 0.0f);

    std::string mk = "mkdir -p '" + dumpDir + "'";
    (void)std::system(mk.c_str());
    RunSynthetic();
    RunSaEnv();
    RunSkinSynthetic();
    if (haveData) {
        RunReal(gta, dumpDir);
        RunSkinReal(gta, dumpDir);
        RunSkinAudit(gta);
    } else {
        std::printf("  no game data at %s: part B skipped (set GTA)\n", gta.c_str());
    }
    Check(NoGlError("end"), "glGetError clean at the end");
    std::printf("PIPE checks: %d ok, %d failed\n", g_ok, g_fail);
    if (g_fail) { std::printf("PIPE RESULT FAIL: %d checks failed\n", g_fail); return 1; }
    std::printf("PIPE RESULT OK%s\n", haveData ? "" : " (synthetic only, no game data)");
    return 0;
}

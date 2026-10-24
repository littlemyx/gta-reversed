// Stream R, slice R-3a: GL smoke app for the portable build (macOS arm64, librw GL3 + SDL3). Run by tools/portable/run_gl_smoke.sh.
//
// What it proves (exit 0 only when ALL hold):
//   * a window created exactly like WinMain does (SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL) is ADOPTED by librw's gl3 backend through the REAL shim code path:
//     RwEngineInit -> RwEngineOpen({displayID = SDL_Window*}) -> RwEngineSetVideoMode -> RwEngineStart (standalone/rw/engine.cpp portable view +
//     portable/render/engine_gl3.cpp + platform_gl3.cpp);
//   * the context is a CORE profile >= 3.3, the default framebuffer has depth >= 24 and stencil >= 8 (as given by the driver, read back with
//     glGetFramebufferAttachmentParameteriv, not just what was requested);
//   * every gl3 shader of the engine compiled (no "Error in ... shader" on stderr: the script greps it) and glGetError() is clean after start;
//   * N frames: RwCamera-less librw camera clear (colour/depth/stencil) -> glReadPixels matches the clear colour; an Im2D triangle covers the expected pixels
//     and leaves the others; the stencil / depth clear values read back; showRaster presents (vsync off);
//   * R-3b (camera / raster / renderstate shim, through the game's RW API): RwCameraCreate / SetNear / SetFar / SetViewWindow / BeginUpdate / Clear / EndUpdate /
//     ShowRaster; Im3D triangles land at the pixels the gl3 projection predicts, depth read back == the camera's zScale / zShift (z convention), depth test and
//     near / far clipping work, a Z-only clear of the main camera also clears the stencil (to RwD3D9SetStencilClear's value, even with a closed write mask);
//     the vsync flag maps to the swap interval; RwRenderStateSet / Get round trips (incl. a 64 bit raster pointer); raster create / lock / unlock / upload round
//     trips (8888 texture, mip level, RwRasterSetFromImage, textured quad, camera texture clear + read-back, camera without z raster);
//   * R-4 (Im2D / Im3D, portable/render/im2d_im3d_gl3.cpp, through RwIm2D* / RwIm3D*): triangle / strip / fan / indexed list / indexed strip / lines /
//     polyline pixel read-back, ARGB->RGBA vertex colours, textured + vertex-coloured, vertex alpha, frame buffer offset, return-value contract, Im3D transform /
//     LTM / indexed / UV / ALLOPAQUE / lines / > 10000 vertices and indices, depth z convention;
//   * clean shutdown: RwEngineStop / Close / Term, window kept by the owner (adopted, not destroyed by librw), exit code 0.
// The result line is "SMOKE RESULT OK" / "SMOKE RESULT FAIL: <what>".
#include "fakerw.h"
#include "standalone/Fixups.h"
#include <src/gl/rwgl3impl.h>
#include "portable/render/engine_gl3.h"
#include "portable/render/camera_gl3.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

// ---- link seams: what the game provides / later slices replace -----------------------------------------------------------------------------
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
void RwShimPipelineEnsure() {}                  // R-6 (stock gl3 pipelines): the librw default pipeline is used by this test
void RwShimPipelineShutdown() {}
// RwImage* (image.cpp, not part of this build): raster.cpp's RwRasterRead* are not exercised here
RwImage* RwImageRead(const RwChar*) { return nullptr; }
RwImage* RwImageReadMaskedImage(const RwChar*, const RwChar*) { return nullptr; }
RwBool RwImageDestroy(RwImage*) { return TRUE; }
RwImage* RwImageFindRasterFormat(RwImage*, RwInt32, RwInt32*, RwInt32*, RwInt32*, RwInt32*) { return nullptr; }

namespace {

int g_fail = 0;

void Check(bool ok, const char* what, const char* detail = "") {
    std::printf("  %-62s %s %s\n", what, ok ? "ok" : "FAIL", detail);
    if (!ok) {
        g_fail++;
    }
}

struct Px { unsigned char r, g, b, a; };

Px ReadPixel(int x, int yFromBottom) {
    Px p{};
    glReadBuffer(GL_BACK);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(x, yFromBottom, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &p);
    return p;
}

bool Near(Px p, int r, int g, int b, int tol = 1) {
    return std::abs((int)p.r - r) <= tol && std::abs((int)p.g - g) <= tol && std::abs((int)p.b - b) <= tol;
}

bool NoGlError(const char* where) {
    bool clean = true;
    for (GLenum e; (e = glGetError()) != GL_NO_ERROR;) {
        std::printf("  glGetError after %s: 0x%x\n", where, e);
        clean = false;
    }
    return clean;
}


// ---- R-3b: camera / raster / renderstate shim through the game's RW API --------------------------------------------------------------------------
struct Rgba8 { unsigned char r, g, b, a; };

// pixel `x, yTopDown` of the framebuffer currently bound for reading (default framebuffer after a camera update)
Px ReadTop(int x, int yTop, int ph) { return ReadPixel(x, ph - 1 - yTop); }

void Im3DTri(const float p[3][3], unsigned char r, unsigned char g, unsigned char b) {
    rw::RWDEVICE::Im3DVertex v[3] = {};
    for (int i = 0; i < 3; i++) {
        v[i].setX(p[i][0]);
        v[i].setY(p[i][1]);
        v[i].setZ(p[i][2]);
        v[i].setColor(r, g, b, 255);
        v[i].setU(0.0f);
        v[i].setV(0.0f);
    }
    rw::im3d::Transform(v, 3, nullptr, rw::im3d::VERTEXXYZ | rw::im3d::VERTEXRGBA);
    rw::im3d::RenderTriangle(0, 1, 2);
    rw::im3d::End();
}

void RS(RwRenderState st, uintptr_t v) { RwRenderStateSet(st, reinterpret_cast<void*>(v)); }
uint32_t RSGet(RwRenderState st) { uint32_t v = 0xCDCDCDCDu; RwRenderStateGet(st, &v); return v; }

// right after RwEngineStart: the RW defaults NotsaRwRenderState_OnEngineStarted applied (nothing has touched the state yet)
void R3bCheckRenderStateDefaults() {
    Check(RSGet(rwRENDERSTATEZTESTENABLE) == 1 && RSGet(rwRENDERSTATEZWRITEENABLE) == 1 && RSGet(rwRENDERSTATECULLMODE) == rwCULLMODECULLBACK &&
          RSGet(rwRENDERSTATESRCBLEND) == rwBLENDSRCALPHA && RSGet(rwRENDERSTATEDESTBLEND) == rwBLENDINVSRCALPHA && RSGet(rwRENDERSTATEFOGENABLE) == 0 &&
          RSGet(rwRENDERSTATETEXTUREFILTER) == rwFILTERLINEAR && RSGet(rwRENDERSTATESTENCILFUNCTION) == rwSTENCILFUNCTIONALWAYS,
          "RW render state defaults after RwEngineStart");
}

void RunR3b(SDL_Window* win, int pw, int ph) {
    std::printf("---- R-3b: camera / raster / renderstate shim ----\n");
    char what[160];

    // -- render state set / get round trips (renderstate.cpp portable view)
    RS(rwRENDERSTATECULLMODE, rwCULLMODECULLNONE);
    RS(rwRENDERSTATEFOGTYPE, rwFOGTYPEEXPONENTIAL);        // no gl3 equivalent: shadow only
    RS(rwRENDERSTATEFOGCOLOR, 0x80112233u);
    RS(rwRENDERSTATETEXTUREADDRESSU, rwTEXTUREADDRESSCLAMP);
    Check(RSGet(rwRENDERSTATECULLMODE) == rwCULLMODECULLNONE && RSGet(rwRENDERSTATEFOGTYPE) == rwFOGTYPEEXPONENTIAL && RSGet(rwRENDERSTATEFOGCOLOR) == 0x80112233u &&
          RSGet(rwRENDERSTATETEXTUREADDRESSU) == rwTEXTUREADDRESSCLAMP, "render state set / get round trip (cull, fog type shadow, fog colour R/B swap, address)");
    Check(RwRenderStateSet(rwRENDERSTATECULLMODE, reinterpret_cast<void*>(77)) == FALSE && RSGet(rwRENDERSTATECULLMODE) == rwCULLMODECULLNONE, "out-of-range cull mode rejected");
    RS(rwRENDERSTATECULLMODE, rwCULLMODECULLBACK);
    RS(rwRENDERSTATETEXTUREADDRESSU, rwTEXTUREADDRESSWRAP);
    RS(rwRENDERSTATEFOGTYPE, rwFOGTYPELINEAR);
    RS(rwRENDERSTATEFOGCOLOR, 0);

    // -- camera through the RW API
    RwCamera* cam = RwCameraCreate();
    Check(cam != nullptr, "RwCameraCreate");
    cam->setFrame(rw::Frame::create());
    RwCameraSetNearClipPlane(cam, 0.5f);
    RwCameraSetFarClipPlane(cam, 10.0f);
    const RwV2d vw{ 1.0f, 0.75f };
    RwCameraSetViewWindow(cam, &vw);
    cam->frameBuffer = RwRasterCreate(pw, ph, 0, rwRASTERTYPECAMERA);
    cam->zBuffer     = RwRasterCreate(pw, ph, 0, rwRASTERTYPEZBUFFER);
    Check(cam->frameBuffer && cam->zBuffer, "RwRasterCreate CAMERA + ZBUFFER");
    std::snprintf(what, sizeof(what), "device z range %.1f..%.1f (gl3: NDC), RwEngineInstance dOpenDevice %.1f..%.1f", (double)rw::engine->device.zNear, (double)rw::engine->device.zFar,
                  (double)RwEngineInstance->dOpenDevice.zBufferNear, (double)RwEngineInstance->dOpenDevice.zBufferFar);
    Check(RwEngineInstance->dOpenDevice.zBufferNear == rw::engine->device.zNear && RwEngineInstance->dOpenDevice.zBufferFar == rw::engine->device.zFar, what);
    const double zsN = cam->zScale / 0.5 + cam->zShift, zsF = cam->zScale / 10.0 + cam->zShift;
    std::snprintf(what, sizeof(what), "zScale %.5f zShift %.5f: z(near)=%.5f z(far)=%.5f within 1e-3 of the device range", (double)cam->zScale, (double)cam->zShift, zsN, zsF);
    Check(std::fabs(zsN - rw::engine->device.zNear) < 1e-3 && std::fabs(zsF - rw::engine->device.zFar) < 1e-3, what);

    Check(RwCameraBeginUpdate(nullptr) == nullptr, "RwCameraBeginUpdate(null) fails");
    Check(RwCameraBeginUpdate(cam) == cam && RwEngineInstance->curCamera == cam, "RwCameraBeginUpdate");
    RwD3D9SetStencilClear(0x2A);
    const RwRGBA bg{ 30, 60, 90, 255 };
    RwRGBA       col = bg;
    Check(RwCameraClear(cam, &col, rwCAMERACLEARIMAGE | rwCAMERACLEARZ | rwCAMERACLEARSTENCIL) == cam, "RwCameraClear(image | z | stencil)");
    {
        const Px c = ReadTop(pw / 2, ph / 2, ph);
        unsigned char st = 0;
        float         dp = 0;
        glReadPixels(pw / 2, ph / 2, 1, 1, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, &st);
        glReadPixels(pw / 2, ph / 2, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &dp);
        std::snprintf(what, sizeof(what), "clear: colour %d,%d,%d  stencil 0x%02x (RwD3D9SetStencilClear 0x2A)  depth %.3f (1.0)", c.r, c.g, c.b, st, (double)dp);
        Check(Near(c, 30, 60, 90) && st == 0x2A && std::fabs(dp - 1.0f) < 1e-4f, what);
    }

    // Identity camera frame, view window 1 x 0.75: NDC x = -X / (Z * 1) (the RW camera view mirrors X, same code in the d3d9 backend), NDC y = Y / (Z * 0.75).
    RS(rwRENDERSTATETEXTURERASTER, 0);
    RS(rwRENDERSTATEZTESTENABLE, 1);
    RS(rwRENDERSTATEZWRITEENABLE, 1);
    RS(rwRENDERSTATEVERTEXALPHAENABLE, 0);
    RS(rwRENDERSTATECULLMODE, rwCULLMODECULLNONE);
    const float near3[3][3] = { { 0.0f, 0.0f, 2.0f }, { -1.0f, 0.0f, 2.0f }, { 0.0f, 0.75f, 2.0f } };       // NDC (0,0) (0.5,0) (0,0.5), z = 2
    const float far3[3][3]  = { { -4.0f, -3.0f, 4.0f }, { 4.0f, -3.0f, 4.0f }, { 0.0f, 3.0f, 4.0f } };       // NDC (1,-1) (-1,-1) (0,1), z = 4
    const float beyond[3][3] = { { -6.0f, -4.0f, 12.0f }, { 6.0f, -4.0f, 12.0f }, { 0.0f, 4.0f, 12.0f } };   // z = 12 > far 10
    const float inside[3][3] = { { -0.2f, -0.2f, 0.2f }, { 0.2f, -0.2f, 0.2f }, { 0.0f, 0.2f, 0.2f } };       // z = 0.2 < near 0.5
    Im3DTri(near3, 0, 255, 0);
    Im3DTri(far3, 255, 0, 0);          // drawn AFTER the nearer green one: the depth test must keep green in the overlap
    Im3DTri(beyond, 0, 0, 255);        // clipped by the far plane
    Im3DTri(inside, 255, 255, 0);      // clipped by the near plane
    auto ndcX = [&](double n) { return (int)(pw * 0.5 + n * pw * 0.5); };
    auto ndcY = [&](double n) { return (int)(ph * 0.5 - n * ph * 0.5); };   // top-down row of an NDC y
    const Px pIn = ReadTop(ndcX(0.1), ndcY(0.1), ph), pOut = ReadTop(ndcX(0.4), ndcY(0.4), ph), pMirror = ReadTop(ndcX(-0.1), ndcY(0.1), ph), pRed = ReadTop(ndcX(-0.1), ndcY(-0.5), ph);
    std::snprintf(what, sizeof(what), "Im3D at the projected pixels: in %d,%d,%d out %d,%d,%d mirrored side %d,%d,%d far-only %d,%d,%d", pIn.r, pIn.g, pIn.b, pOut.r, pOut.g, pOut.b, pMirror.r,
                  pMirror.g, pMirror.b, pRed.r, pRed.g, pRed.b);
    Check(Near(pIn, 0, 255, 0) && !Near(pOut, 0, 255, 0) && Near(pMirror, 255, 0, 0) && Near(pRed, 255, 0, 0), what);
    Check(Near(pOut, 255, 0, 0) || Near(pOut, 30, 60, 90), "far / near clip: z = 12 and z = 0.2 triangles leave no blue / yellow pixel");
    {
        float dp = 0;
        glReadPixels(ndcX(0.1), ph - 1 - ndcY(0.1), 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &dp);
        const double expect = (cam->zScale / 2.0 + cam->zShift + 1.0) * 0.5; // NDC z -> [0,1] window depth
        std::snprintf(what, sizeof(what), "depth at z = 2: read %.4f, from zScale / zShift %.4f (z convention)", (double)dp, expect);
        Check(std::fabs(dp - expect) < 2e-3, what);
    }
    Check(NoGlError("camera update"), "glGetError clean after the camera update");

    // read-back helper: top-down RGB of the back buffer
    {
        std::vector<unsigned char> rgb;
        int                        w = 0, h = 0;
        const bool                 ok = NotsaRwReadBackBuffer(rgb, w, h);
        const bool                 px = ok && w == pw && h == ph && rgb[((size_t)ndcY(0.1) * w + ndcX(0.1)) * 3 + 1] == 255 && rgb[((size_t)ndcY(0.1) * w + ndcX(0.1)) * 3 + 0] == 0;
        Check(px, "NotsaRwReadBackBuffer: size == window pixels, top-down rows (green triangle at its NDC row)");
    }

    // Z-only clear of the main camera also clears the stencil, to the RwD3D9SetStencilClear value, even when the stencil write mask is closed (D3D Clear ignores it)
    RS(rwRENDERSTATESTENCILFUNCTIONWRITEMASK, 0x0F);
    Im3DTri(near3, 0, 255, 0);                 // flushes the lazy GL state: glStencilMask(0x0F) is in effect now
    RwD3D9SetStencilClear(0x7C);
    Check(RwCameraClear(cam, nullptr, rwCAMERACLEARZ) == cam, "RwCameraClear(z only)");
    {
        unsigned char st = 0;
        float         dp = 0;
        glReadPixels(ndcX(0.1), ph - 1 - ndcY(0.1), 1, 1, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, &st);
        glReadPixels(ndcX(0.1), ph - 1 - ndcY(0.1), 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &dp);
        std::snprintf(what, sizeof(what), "z-only clear: stencil 0x%02x == 0x7C (also cleared, write mask 0x0F bypassed), depth %.3f == 1.0", st, (double)dp);
        Check(st == 0x7C && std::fabs(dp - 1.0f) < 1e-4f, what);
    }
    RS(rwRENDERSTATESTENCILFUNCTIONWRITEMASK, 0xFFFFFFFFu);
    RwD3D9SetStencilClear(0);

    Check(RwCameraEndUpdate(cam) == cam && RwEngineInstance->curCamera == nullptr, "RwCameraEndUpdate");
    int iv = -1;
    // A vsynced swap of a window that is not on screen (no display session / occluded) can block forever: the vsync leg is opt-in (NOTSA_SMOKE_VSYNC=1)
    if (std::getenv("NOTSA_SMOKE_VSYNC")) {
        Check(RwCameraShowRaster(cam, nullptr, rwRASTERFLIPWAITVSYNC) == cam && SDL_GL_GetSwapInterval(&iv) && iv == 1, "RwCameraShowRaster(FLIPWAITVSYNC) -> swap interval 1");
    }
    Check(RwCameraShowRaster(cam, nullptr, 0) == cam && SDL_GL_GetSwapInterval(&iv) && iv == 0, "RwCameraShowRaster(0) -> swap interval 0");

    // -- rasters: 8888 texture lock / write / unlock (upload) / lock / read
    {
        RwRaster* r = RwRasterCreate(16, 8, 32, rwRASTERTYPETEXTURE | rwRASTERFORMAT8888);
        Check(r && RwRasterGetWidth(r) == 16 && RwRasterGetHeight(r) == 8, "RwRasterCreate 16x8 8888 texture");
        RwUInt8* p = RwRasterLock(r, 0, rwRASTERLOCKWRITE | rwRASTERLOCKNOFETCH);
        Check(p && RwRasterGetStride(r) == 64, "RwRasterLock(write, nofetch): stride 64");
        Check(RwRasterLock(r, 0, rwRASTERLOCKREAD) == nullptr, "second lock of a locked raster fails");
        if (p) {
            for (int y = 0; y < 8; y++) {
                for (int x = 0; x < 16; x++) {
                    RwUInt8* d = p + y * 64 + x * 4;
                    d[0] = (RwUInt8)(x * 16); d[1] = (RwUInt8)(y * 32); d[2] = (RwUInt8)(x ^ y); d[3] = 255;
                }
            }
        }
        RwRasterUnlock(r);
        const RwUInt8* q = RwRasterLock(r, 0, rwRASTERLOCKREAD);
        bool same = q != nullptr;
        for (int y = 0; same && y < 8; y++) {
            for (int x = 0; x < 16 && same; x++) {
                const RwUInt8* d = q + y * 64 + x * 4;
                same = d[0] == (RwUInt8)(x * 16) && d[1] == (RwUInt8)(y * 32) && d[2] == (RwUInt8)(x ^ y) && d[3] == 255;
            }
        }
        Check(same, "unlock uploaded, lock(read) returns the same 16x8 texels");
        RwRasterUnlock(r);
        Check(RwRasterGetWidth(r) == 16 && RwRasterGetHeight(r) == 8, "raster size restored after unlock");
        Check(NoGlError("raster round trip"), "glGetError clean after the raster round trip");

        // pointer valued state: 8 bytes
        RwRaster* back = reinterpret_cast<RwRaster*>(uintptr_t(0xDEADBEEFDEADBEEFull));
        RwRenderStateSet(rwRENDERSTATETEXTURERASTER, r);
        RwRenderStateGet(rwRENDERSTATETEXTURERASTER, &back);
        Check(back == r, "RwRenderStateGet(TEXTURERASTER) returns the full 64 bit pointer");
        RwRenderStateSet(rwRENDERSTATETEXTURERASTER, nullptr);
        RwRasterDestroy(r);
    }
    // -- mip level: the lock level is remembered for the unlock (upload target)
    {
        RwRaster* r = RwRasterCreate(16, 16, 32, rwRASTERTYPETEXTURE | rwRASTERFORMAT8888 | rwRASTERFORMATMIPMAP);
        Check(r && RwRasterGetNumLevels(r) == 5, "16x16 mipmapped raster has 5 levels");
        RwUInt8* p = r ? RwRasterLock(r, 2, rwRASTERLOCKWRITE | rwRASTERLOCKNOFETCH) : nullptr;
        Check(p && RwRasterGetWidth(r) == 4 && RwRasterGetStride(r) == 16, "lock level 2: 4 wide, stride 16");
        if (p) {
            for (int i = 0; i < 16; i++) { p[i * 4 + 0] = (RwUInt8)(i * 10); p[i * 4 + 1] = 7; p[i * 4 + 2] = (RwUInt8)(255 - i); p[i * 4 + 3] = 255; }
        }
        RwRasterUnlock(r);
        const RwUInt8* q = r ? RwRasterLock(r, 2, rwRASTERLOCKREAD) : nullptr;
        bool           same = q != nullptr;
        for (int i = 0; same && i < 16; i++) {
            same = q[i * 4 + 0] == (RwUInt8)(i * 10) && q[i * 4 + 1] == 7 && q[i * 4 + 2] == (RwUInt8)(255 - i) && q[i * 4 + 3] == 255;
        }
        Check(same, "level 2 upload + read-back round trip");
        RwRasterUnlock(r);
        Check(r && RwRasterGetWidth(r) == 16, "mip raster size restored");
        Check(NoGlError("mip level"), "glGetError clean after the mip level round trip");
        RwRasterDestroy(r);
    }
    // -- RwRasterSetFromImage (8888 and 888)
    {
        rw::Image* img = rw::Image::create(16, 8, 32);
        img->allocate();
        for (int y = 0; y < 8; y++) {
            for (int x = 0; x < 16; x++) {
                unsigned char* d = img->pixels + y * img->stride + x * 4;
                d[0] = (unsigned char)(x * 15); d[1] = (unsigned char)(y * 30); d[2] = 99; d[3] = (unsigned char)(255 - x);
            }
        }
        RwRaster* r8 = RwRasterCreate(16, 8, 32, rwRASTERTYPETEXTURE | rwRASTERFORMAT8888);
        Check(r8 && RwRasterSetFromImage(r8, img) == r8, "RwRasterSetFromImage(8888)");
        const RwUInt8* q = RwRasterLock(r8, 0, rwRASTERLOCKREAD);
        bool           same = q != nullptr;
        for (int y = 0; same && y < 8; y++) {
            for (int x = 0; x < 16 && same; x++) {
                const RwUInt8* d = q + y * 64 + x * 4;
                same = d[0] == (RwUInt8)(x * 15) && d[1] == (RwUInt8)(y * 30) && d[2] == 99 && d[3] == (RwUInt8)(255 - x);
            }
        }
        Check(same, "8888 raster holds the image pixels (gl3 byte order R,G,B,A)");
        RwRasterUnlock(r8);
        rw::Image* small = rw::Image::create(4, 4, 32);
        small->allocate();
        Check(RwRasterSetFromImage(r8, small) == nullptr, "RwRasterSetFromImage rejects a size mismatch");
        small->destroy();
        RwRasterDestroy(r8);
        RwRaster* r3 = RwRasterCreate(16, 8, 24, rwRASTERTYPETEXTURE | rwRASTERFORMAT888);
        Check(r3 && RwRasterSetFromImage(r3, img) == r3, "RwRasterSetFromImage(888)");
        const RwUInt8* q3 = r3 ? RwRasterLock(r3, 0, rwRASTERLOCKREAD) : nullptr;
        Check(q3 && q3[0] == 0 && q3[1] == 0 && q3[2] == 99 && q3[3 * 15 + 0] == 15 * 15, "888 raster: 3 bytes per texel, R,G,B");
        RwRasterUnlock(r3);
        RwRasterDestroy(r3);
        img->destroy();
    }

    // -- textured quad (RwRasterRender): upload -> sample -> pixel, inside a camera update
    {
        RwRaster* tex = RwRasterCreate(16, 8, 32, rwRASTERTYPETEXTURE | rwRASTERFORMAT8888);
        RwUInt8*  p   = tex ? RwRasterLock(tex, 0, rwRASTERLOCKWRITE | rwRASTERLOCKNOFETCH) : nullptr;
        for (int i = 0; p && i < 16 * 8; i++) { p[i * 4 + 0] = 200; p[i * 4 + 1] = 100; p[i * 4 + 2] = 50; p[i * 4 + 3] = 255; }
        RwRasterUnlock(tex);
        RwCameraBeginUpdate(cam);
        RwCameraClear(cam, &col, rwCAMERACLEARIMAGE | rwCAMERACLEARZ);
        Check(RwRasterRender(tex, 10, 20) == tex, "RwRasterRender inside a camera update");
        const Px a = ReadTop(10 + 8, 20 + 4, ph), b = ReadTop(10 + 17, 20 + 4, ph), c = ReadTop(10 + 8, 20 + 9, ph);
        std::snprintf(what, sizeof(what), "textured quad: inside %d,%d,%d (200,100,50), right of it %d,%d,%d, below it %d,%d,%d (background)", a.r, a.g, a.b, b.r, b.g, b.b, c.r, c.g, c.b);
        Check(Near(a, 200, 100, 50) && Near(b, 30, 60, 90) && Near(c, 30, 60, 90), what);
        // RwRasterClearRect on the camera raster: pushes a context, clears a window rect to a D3DCOLOR (0xAARRGGBB)
        RwRasterPushContext(cam->frameBuffer);
        RwRect rc{ 100, 50, 20, 10 };
        Check(RwRasterClearRect(&rc, (RwInt32)0xFF8040C0u) == TRUE, "RwRasterClearRect(camera raster)");
        RwRasterPopContext();
        const Px in = ReadTop(105, 55, ph), edge = ReadTop(99, 55, ph), below = ReadTop(105, 61, ph);
        std::snprintf(what, sizeof(what), "clear rect (100,50,20x10): inside %d,%d,%d (128,64,192), left %d,%d,%d, below %d,%d,%d", in.r, in.g, in.b, edge.r, edge.g, edge.b, below.r, below.g, below.b);
        Check(Near(in, 128, 64, 192) && Near(edge, 30, 60, 90) && Near(below, 30, 60, 90), what);
        RwCameraEndUpdate(cam);
        RwRasterDestroy(tex);
        Check(NoGlError("textured quad"), "glGetError clean after the quad / clear rect");
    }

    // -- camera texture: clear through the raster API, read back (top row first), render into it with a camera WITHOUT a z raster
    {
        RwRaster* ct = RwRasterCreate(64, 32, 0, rwRASTERTYPECAMERATEXTURE | rwRASTERFORMAT8888);
        Check(ct != nullptr, "RwRasterCreate CAMERATEXTURE 64x32");
        RwRasterPushContext(ct);
        Check(RwRasterClear((RwInt32)0xFF336699u) == TRUE, "RwRasterClear(camera texture)");
        RwRect rc{ 0, 0, 16, 8 };                                // top-left in RW's top-down coordinates
        Check(RwRasterClearRect(&rc, (RwInt32)0xFFCC2211u) == TRUE, "RwRasterClearRect(camera texture, top-left)");
        RwRasterPopContext();
        const RwUInt8* q = ct ? RwRasterLock(ct, 0, rwRASTERLOCKREAD) : nullptr;
        Check(q != nullptr, "RwRasterLock(camera texture, read)");
        if (q) {
            const int      stride = RwRasterGetStride(ct);
            const RwUInt8* tl = q + 2 * stride + 2 * 4, *br = q + 30 * stride + 60 * 4, *tr = q + 2 * stride + 40 * 4;
            std::snprintf(what, sizeof(what), "top-left (%d,%d,%d) = clear-rect colour CC,22,11; top-right (%d,%d,%d) and bottom-right (%d,%d,%d) = 33,66,99; row 0 first", tl[0], tl[1], tl[2], tr[0], tr[1], tr[2],
                          br[0], br[1], br[2]);
            Check(tl[0] == 0xCC && tl[1] == 0x22 && tl[2] == 0x11 && tr[0] == 0x33 && tr[1] == 0x66 && tr[2] == 0x99 && br[0] == 0x33 && br[1] == 0x66 && br[2] == 0x99, what);
        }
        RwRasterUnlock(ct);

        RwCamera* cam2 = RwCameraCreate();
        cam2->setFrame(rw::Frame::create());
        cam2->frameBuffer = ct;
        cam2->zBuffer     = nullptr;                              // shadow / mirror cameras have none
        RwCameraSetViewWindow(cam2, &vw);
        Check(RwCameraBeginUpdate(cam2) == cam2, "RwCameraBeginUpdate on a camera texture without z raster");
        RwRGBA c2{ 10, 200, 30, 255 };
        Check(RwCameraClear(cam2, &c2, rwCAMERACLEARIMAGE | rwCAMERACLEARZ | rwCAMERACLEARSTENCIL) == cam2, "RwCameraClear(image | z | stencil) without z raster (z / stencil dropped)");
        RwCameraEndUpdate(cam2);
        const RwUInt8* q2 = RwRasterLock(ct, 0, rwRASTERLOCKREAD);
        Check(q2 && q2[0] == 10 && q2[1] == 200 && q2[2] == 30, "camera texture holds the camera's clear colour");
        RwRasterUnlock(ct);
        Check(NoGlError("camera texture"), "glGetError clean after the camera texture round trip");
        cam2->frameBuffer = nullptr;
        cam2->getFrame()->destroy();
        cam2->destroy();
        RwRasterDestroy(ct);
        RwCameraBeginUpdate(cam);                                 // the main framebuffer must still be usable afterwards
        RwCameraClear(cam, &col, rwCAMERACLEARIMAGE | rwCAMERACLEARZ);
        const Px m = ReadTop(pw / 2, ph / 2, ph);
        Check(Near(m, 30, 60, 90), "main camera usable after the camera texture pass (framebuffer binding restored)");
        RwCameraEndUpdate(cam);
    }
    Check(RwD3D9CameraAttachWindow(cam, nullptr) == TRUE && RwD3D9CameraAttachWindow(cam, win) == TRUE, "RwD3D9CameraAttachWindow(null / device window)");

    RwRasterDestroy(cam->frameBuffer);
    RwRasterDestroy(cam->zBuffer);
    cam->frameBuffer = cam->zBuffer = nullptr;
    cam->getFrame()->destroy();
    RwCameraDestroy(cam);
    Check(NoGlError("R-3b"), "glGetError clean after R-3b");
}

// ---- R-4: Im2D / Im3D through the game's RW API (im2d_im3d_gl3.cpp) ------------------------------------------------------------------------------
struct Snap {
    int w = 0, h = 0;
    std::vector<unsigned char> d; // RGBA, top row first
    void grab(int pw, int ph) {
        w = pw; h = ph;
        std::vector<unsigned char> raw((size_t)pw * ph * 4);
        glReadBuffer(GL_BACK);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadPixels(0, 0, pw, ph, GL_RGBA, GL_UNSIGNED_BYTE, raw.data());
        d.resize(raw.size());
        for (int y = 0; y < ph; y++) {
            std::memcpy(&d[(size_t)y * pw * 4], &raw[(size_t)(ph - 1 - y) * pw * 4], (size_t)pw * 4);
        }
    }
    Px at(int x, int y) const { const unsigned char* p = &d[((size_t)y * w + x) * 4]; return Px{ p[0], p[1], p[2], p[3] }; }
    bool is(int x, int y, int r, int g, int b, int tol = 1) const { return Near(at(x, y), r, g, b, tol); }
    int countNot(int r, int g, int b, int tol = 1) const {
        int n = 0;
        for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) n += !is(x, y, r, g, b, tol);
        return n;
    }
    int count(int r, int g, int b, int tol = 1) const { return w * h - countNot(r, g, b, tol); }
    // bounding box of the pixels that differ from the background (black)
    bool bbox(int& x0, int& y0, int& x1, int& y1) const {
        x0 = y0 = 1 << 30; x1 = y1 = -1;
        for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
            if (!is(x, y, 0, 0, 0, 0)) { x0 = std::min(x0, x); y0 = std::min(y0, y); x1 = std::max(x1, x); y1 = std::max(y1, y); }
        }
        return x1 >= 0;
    }
};

RwIm2DVertex V2(float x, float y, unsigned a, unsigned r, unsigned g, unsigned b, float u = 0.0f, float v = 0.0f) {
    RwIm2DVertex vt{};
    RwIm2DVertexSetScreenX(&vt, x);
    RwIm2DVertexSetScreenY(&vt, y);
    RwIm2DVertexSetScreenZ(&vt, RwIm2DGetNearScreenZ());
    RwIm2DVertexSetRecipCameraZ(&vt, 2.0f);
    RwIm2DVertexSetU(&vt, u, 2.0f);
    RwIm2DVertexSetV(&vt, v, 2.0f);
    RwIm2DVertexSetIntRGBA(&vt, r, g, b, a);
    return vt;
}

RwIm3DVertex V3(float x, float y, float z, unsigned r, unsigned g, unsigned b, unsigned a = 255, float u = 0.0f, float v = 0.0f) {
    RwIm3DVertex vt{};
    RwIm3DVertexSetPos(&vt, x, y, z);
    RwIm3DVertexSetRGBA(&vt, r, g, b, a);
    RwIm3DVertexSetU(&vt, u);
    RwIm3DVertexSetV(&vt, v);
    return vt;
}

void RunR4(int pw, int ph) {
    std::printf("---- R-4: Im2D / Im3D through RwIm2D* / RwIm3D* ----\n");
    char what[200];

    // -- contract without a camera update: nothing can be drawn, the Im3D bookkeeping still works
    RwIm2DVertex t0[3] = { V2(10, 10, 255, 255, 0, 0), V2(110, 10, 255, 255, 0, 0), V2(10, 110, 255, 255, 0, 0) };
    RwIm3DVertex v3[3] = { V3(0, 0, 1, 1, 2, 3), V3(1, 0, 1, 4, 5, 6), V3(0, 1, 1, 7, 8, 9) };
    Check(RwIm2DRenderPrimitive(rwPRIMTYPETRILIST, t0, 3) == FALSE, "Im2D outside a camera update fails (logged once)");
    Check(RwIm3DEnd() == FALSE && RwIm3DRenderPrimitive(rwPRIMTYPETRILIST) == FALSE, "Im3D End / Render without a transform fail");
    RwImVertexIndex idx3[3] = { 0, 1, 2 };
    Check(RwIm3DRenderIndexedPrimitive(rwPRIMTYPETRILIST, idx3, 3) == FALSE && RwIm3DRenderLine(0, 1) == FALSE && RwIm3DRenderTriangle(0, 1, 2) == FALSE, "Im3D indexed / line / triangle without a transform fail");
    Check(RwIm3DTransform(v3, 0x10001, nullptr, 0) == nullptr && RwIm3DEnd() == FALSE, "RwIm3DTransform > 0x10000 vertices fails and starts nothing");
    Check(RwIm3DTransform(v3, 3, nullptr, rwIM3D_VERTEXUV) == v3, "RwIm3DTransform returns the vertex pointer");
    Check(RwIm3DRenderPrimitive(rwPRIMTYPENAPRIMTYPE) == FALSE && RwIm3DRenderPrimitive(rwPRIMTYPEPOINTLIST) == FALSE, "Im3D rejects primitive type 0 and point lists");
    Check(RwIm3DEnd() == TRUE && RwIm3DEnd() == FALSE, "RwIm3DEnd pairs with one transform");
    Check(v3[1].color == 0xFF040506u || v3[1].color == 0x00040506u, "RwIm3DVertexSetRGBA packs ARGB (D3DCOLOR)");
    Check(RwIm2DGetNearScreenZ() == rw::engine->device.zNear && RwIm2DGetFarScreenZ() == rw::engine->device.zFar, "RwIm2DGetNear/FarScreenZ come from the device z range");

    // -- camera: 2D tests only need the frame buffer; the same camera serves the Im3D tests (identity frame, view window 1 x 0.75, near 0.5, far 10)
    RwCamera* cam = RwCameraCreate();
    cam->setFrame(rw::Frame::create());
    RwCameraSetNearClipPlane(cam, 0.5f);
    RwCameraSetFarClipPlane(cam, 10.0f);
    const RwV2d vw{ 1.0f, 0.75f };
    RwCameraSetViewWindow(cam, &vw);
    cam->frameBuffer = RwRasterCreate(pw, ph, 0, rwRASTERTYPECAMERA);
    cam->zBuffer     = RwRasterCreate(pw, ph, 0, rwRASTERTYPEZBUFFER);
    const RwRGBA black{ 0, 0, 0, 255 };
    auto begin = [&]() {
        RwCameraBeginUpdate(cam);
        RwRGBA c = black;
        RwCameraClear(cam, &c, rwCAMERACLEARIMAGE | rwCAMERACLEARZ | rwCAMERACLEARSTENCIL);
        RS(rwRENDERSTATETEXTURERASTER, 0);
        RS(rwRENDERSTATEZTESTENABLE, 0);
        RS(rwRENDERSTATEZWRITEENABLE, 0);
        RS(rwRENDERSTATEVERTEXALPHAENABLE, 0);
        RS(rwRENDERSTATECULLMODE, rwCULLMODECULLNONE);
        RS(rwRENDERSTATETEXTUREFILTER, rwFILTERNEAREST);
    };
    auto clear = [&]() { RwRGBA c = black; RwCameraClear(cam, &c, rwCAMERACLEARIMAGE | rwCAMERACLEARZ); };
    Snap s;
    int x0, y0, x1, y1;

    begin();
    // ---- Im2D ----
    // triangle (10,10) (110,10) (10,110): vertex colour order (ARGB packed -> R,G,B bytes), bbox, area
    {
        RwIm2DVertex t[3] = { V2(10, 10, 255, 255, 0, 0), V2(110, 10, 255, 255, 0, 0), V2(10, 110, 255, 255, 0, 0) };
        Check(RwIm2DRenderPrimitive(rwPRIMTYPETRILIST, t, 3) == TRUE, "Im2D triangle list draws");
        s.grab(pw, ph);
        Check(s.is(20, 20, 255, 0, 0) && s.is(50, 50, 255, 0, 0) && s.is(200, 200, 0, 0, 0) && s.is(100, 100, 0, 0, 0), "triangle: red where inside, black outside (ARGB -> RGBA)");
        Check(s.bbox(x0, y0, x1, y1) && std::abs(x0 - 10) <= 1 && std::abs(y0 - 10) <= 1 && std::abs(x1 - 109) <= 2 && std::abs(y1 - 109) <= 2, "triangle bbox == (10,10)-(110,110), screen y down");
        std::snprintf(what, sizeof(what), "triangle area %d (expected ~5000)", s.countNot(0, 0, 0, 0));
        Check(std::abs(s.countNot(0, 0, 0, 0) - 5000) < 250, what);
        Check(NoGlError("Im2D triangle"), "glGetError clean after Im2D triangle");
        t[0] = V2(10, 10, 255, 10, 100, 200);
        clear();
        t[1] = V2(110, 10, 255, 10, 100, 200); t[2] = V2(10, 110, 255, 10, 100, 200);
        RwIm2DRenderPrimitive(rwPRIMTYPETRILIST, t, 3);
        s.grab(pw, ph);
        Check(s.is(30, 30, 10, 100, 200), "triangle colour (10,100,200) exact: R/B not swapped");
    }
    // quad (150,20)-(230,100): strip, fan, indexed list, indexed strip all cover the same 80 x 80
    {
        RwIm2DVertex q[4]  = { V2(150, 20, 255, 0, 255, 0), V2(230, 20, 255, 0, 255, 0), V2(150, 100, 255, 0, 255, 0), V2(230, 100, 255, 0, 255, 0) };
        RwIm2DVertex f[4]  = { q[0], q[1], q[3], q[2] };
        RwImVertexIndex il[6] = { 0, 1, 2, 2, 1, 3 }, is[4] = { 0, 1, 2, 3 };
        auto quadOk = [&](const char* name, RwBool rc) {
            s.grab(pw, ph);
            std::snprintf(what, sizeof(what), "%s: rect (150,20)-(230,100) filled, area %d", name, s.count(0, 255, 0));
            Check(rc == TRUE && s.is(190, 60, 0, 255, 0) && s.is(149, 60, 0, 0, 0) && s.is(231, 60, 0, 0, 0) && s.is(190, 101, 0, 0, 0) && std::abs(s.count(0, 255, 0) - 6400) < 200, what);
        };
        clear(); quadOk("Im2D tri strip", RwIm2DRenderPrimitive(rwPRIMTYPETRISTRIP, q, 4));
        clear(); quadOk("Im2D tri fan", RwIm2DRenderPrimitive(rwPRIMTYPETRIFAN, f, 4));
        clear(); quadOk("Im2D indexed tri list", RwIm2DRenderIndexedPrimitive(rwPRIMTYPETRILIST, q, 4, il, 6));
        clear(); quadOk("Im2D indexed tri strip", RwIm2DRenderIndexedPrimitive(rwPRIMTYPETRISTRIP, q, 4, is, 4));
        clear();
        Check(RwIm2DRenderTriangle(q, 4, 0, 1, 2) == TRUE, "RwIm2DRenderTriangle draws");
        s.grab(pw, ph);
        Check(s.is(160, 30, 0, 255, 0) && s.is(220, 90, 0, 0, 0), "RwIm2DRenderTriangle: only the upper-left half");
        Check(RwIm2DRenderTriangle(q, 4, 0, 1, 4) == FALSE && RwIm2DRenderLine(q, 4, -1, 1) == FALSE, "out-of-array vertex index fails");
        Check(NoGlError("Im2D quads"), "glGetError clean after the Im2D quads");
    }
    // vertex colour interpolation black -> white across x
    {
        clear();
        RwIm2DVertex g[3] = { V2(300, 20, 255, 0, 0, 0), V2(400, 20, 255, 255, 255, 255), V2(300, 120, 255, 0, 0, 0) };
        g[2] = V2(400, 120, 255, 255, 255, 255);
        RwIm2DVertex gq[4] = { V2(300, 20, 255, 0, 0, 0), V2(400, 20, 255, 255, 255, 255), V2(300, 120, 255, 0, 0, 0), g[2] };
        RwIm2DRenderPrimitive(rwPRIMTYPETRISTRIP, gq, 4);
        s.grab(pw, ph);
        const Px mid = s.at(350, 70), lo = s.at(305, 70), hi = s.at(395, 70);
        std::snprintf(what, sizeof(what), "gradient: x=305 -> %d, x=350 -> %d, x=395 -> %d", lo.r, mid.r, hi.r);
        Check(mid.r > 110 && mid.r < 145 && mid.r == mid.g && lo.r < 30 && hi.r > 225, what);
    }
    // lines: RwIm2DRenderLine (1 px horizontal at y = 60), polyline, line list
    {
        clear();
        RwIm2DVertex l[3] = { V2(20, 60.5f, 255, 255, 255, 255), V2(120, 60.5f, 255, 255, 255, 255), V2(120, 90.5f, 255, 255, 255, 255) };
        Check(RwIm2DRenderLine(l, 3, 0, 1) == TRUE, "RwIm2DRenderLine draws");
        s.grab(pw, ph);
        const int onLine = s.countNot(0, 0, 0, 0);
        std::snprintf(what, sizeof(what), "line: %d pixels (expected ~100), bbox y within 1 row", onLine);
        Check(onLine >= 90 && onLine <= 110 && s.bbox(x0, y0, x1, y1) && y1 - y0 <= 1 && std::abs(x0 - 20) <= 1, what);
        clear();
        Check(RwIm2DRenderPrimitive(rwPRIMTYPEPOLYLINE, l, 3) == TRUE, "Im2D polyline draws");
        s.grab(pw, ph);
        Check(s.is(70, 60, 255, 255, 255, 2) && (s.is(119, 75, 255, 255, 255, 2) || s.is(120, 75, 255, 255, 255, 2)) && s.countNot(0, 0, 0, 0) >= 125, "polyline: both segments");
        clear();
        RwIm2DVertex ll[4] = { l[0], l[1], V2(20, 100.5f, 255, 255, 255, 255), V2(60, 100.5f, 255, 255, 255, 255) };
        Check(RwIm2DRenderPrimitive(rwPRIMTYPELINELIST, ll, 4) == TRUE, "Im2D line list draws");
        s.grab(pw, ph);
        Check(s.is(70, 60, 255, 255, 255, 2) && s.is(40, 100, 255, 255, 255, 2) && s.is(90, 100, 0, 0, 0), "line list: two separate segments");
    }
    // textured: 2 x 2 texture, nearest filter, white vertex colour = the texel, grey vertex colour modulates; top row of the lock = top of the quad
    {
        RwRaster* tr = RwRasterCreate(2, 2, 32, rwRASTERTYPETEXTURE | rwRASTERFORMAT8888);
        RwUInt8*  p  = RwRasterLock(tr, 0, rwRASTERLOCKWRITE | rwRASTERLOCKNOFETCH);
        const unsigned char texel[4][4] = { { 255, 0, 0, 255 }, { 0, 255, 0, 255 }, { 0, 0, 255, 255 }, { 255, 255, 0, 255 } }; // row 0: red green; row 1: blue yellow (R,G,B,A)
        if (p) {
            for (int y = 0; y < 2; y++) for (int x = 0; x < 2; x++) std::memcpy(p + y * RwRasterGetStride(tr) + x * 4, texel[y * 2 + x], 4);
        }
        RwRasterUnlock(tr);
        RS(rwRENDERSTATETEXTURERASTER, (uintptr_t)tr);
        clear();
        RwIm2DVertex tq[4] = { V2(400, 200, 255, 255, 255, 255, 0, 0), V2(480, 200, 255, 255, 255, 255, 1, 0), V2(400, 280, 255, 255, 255, 255, 0, 1), V2(480, 280, 255, 255, 255, 255, 1, 1) };
        Check(RwIm2DRenderPrimitive(rwPRIMTYPETRISTRIP, tq, 4) == TRUE, "textured Im2D quad draws");
        s.grab(pw, ph);
        {
            const Px a = s.at(420, 220), b = s.at(460, 220), c = s.at(420, 260), d = s.at(460, 260);
            std::snprintf(what, sizeof(what), "texture quadrants red green / blue yellow (top row first, RGBA texel order): %d,%d,%d %d,%d,%d / %d,%d,%d %d,%d,%d", a.r, a.g, a.b, b.r, b.g, b.b, c.r, c.g, c.b, d.r, d.g, d.b);
            Check(Near(a, 255, 0, 0) && Near(b, 0, 255, 0) && Near(c, 0, 0, 255) && Near(d, 255, 255, 0), what);
        }
        clear();
        for (auto& v : tq) { v.emissiveColor = 0xFF808080u; }
        RwIm2DRenderPrimitive(rwPRIMTYPETRISTRIP, tq, 4);
        s.grab(pw, ph);
        Check(s.is(420, 220, 128, 0, 0, 2) && s.is(460, 260, 128, 128, 0, 2), "texture * vertex colour (0x80 grey) modulates");
        // texture off again: the same vertices give the plain vertex colour
        RS(rwRENDERSTATETEXTURERASTER, 0);
        clear();
        RwIm2DRenderPrimitive(rwPRIMTYPETRISTRIP, tq, 4);
        s.grab(pw, ph);
        Check(s.is(420, 220, 128, 128, 128, 1), "no texture: vertex colour only");
        Check(NoGlError("Im2D textured"), "glGetError clean after textured Im2D");
        RwRasterDestroy(tr);
    }
    // camera texture (rendered, GL row order): drawn upright with D3D UVs (v = 0 at the top), no v flip compensation for it
    {
        RwRaster* ct = RwRasterCreate(64, 32, 0, rwRASTERTYPECAMERATEXTURE | rwRASTERFORMAT8888);
        RwRasterPushContext(ct);
        RwRasterClear((RwInt32)0xFF0000FFu);                       // blue
        RwRect rc{ 0, 0, 16, 8 };                                  // top-left in RW's top-down coordinates
        RwRasterClearRect(&rc, (RwInt32)0xFFFF0000u);              // red
        RwRasterPopContext();
        RS(rwRENDERSTATETEXTURERASTER, (uintptr_t)ct);
        clear();
        RwIm2DVertex tq[4] = { V2(400, 200, 255, 255, 255, 255, 0, 0), V2(528, 200, 255, 255, 255, 255, 1, 0), V2(400, 264, 255, 255, 255, 255, 0, 1), V2(528, 264, 255, 255, 255, 255, 1, 1) };
        RwIm2DRenderPrimitive(rwPRIMTYPETRISTRIP, tq, 4);
        s.grab(pw, ph);
        std::snprintf(what, sizeof(what), "camera texture upright via Im2D: top-left %d,%d,%d (red), bottom-right %d,%d,%d (blue)", s.at(405, 205).r, s.at(405, 205).g, s.at(405, 205).b, s.at(520, 260).r, s.at(520, 260).g, s.at(520, 260).b);
        Check(s.is(405, 205, 255, 0, 0) && s.is(520, 260, 0, 0, 255) && s.is(520, 205, 0, 0, 255) && s.is(405, 260, 0, 0, 255), what);
        RS(rwRENDERSTATETEXTURERASTER, 0);
        RwRasterDestroy(ct);
    }
    // alpha: vertex alpha only counts when the caller enabled vertex alpha (Im2D leaves the blend state to the caller)
    {
        RwIm2DVertex h[3] = { V2(10, 10, 128, 255, 0, 0), V2(110, 10, 128, 255, 0, 0), V2(10, 110, 128, 255, 0, 0) };
        clear();
        RwIm2DRenderPrimitive(rwPRIMTYPETRILIST, h, 3);
        s.grab(pw, ph);
        Check(s.is(30, 30, 255, 0, 0), "blending off: alpha ignored, opaque");
        RS(rwRENDERSTATEVERTEXALPHAENABLE, 1);
        RS(rwRENDERSTATESRCBLEND, rwBLENDSRCALPHA);
        RS(rwRENDERSTATEDESTBLEND, rwBLENDINVSRCALPHA);
        clear();
        RwIm2DRenderPrimitive(rwPRIMTYPETRILIST, h, 3);
        s.grab(pw, ph);
        Check(s.is(30, 30, 128, 0, 0, 2), "vertex alpha enabled: 50% red over black");
        RS(rwRENDERSTATEVERTEXALPHAENABLE, 0);
    }
    // contract: empty draws, invalid calls
    {
        RwIm2DVertex t[3] = { V2(10, 10, 255, 255, 0, 0), V2(110, 10, 255, 255, 0, 0), V2(10, 110, 255, 255, 0, 0) };
        RwImVertexIndex il[6] = { 0, 1, 2, 2, 1, 0 };
        clear();
        Check(RwIm2DRenderPrimitive(rwPRIMTYPENAPRIMTYPE, t, 3) == FALSE && RwIm2DRenderPrimitive(rwPRIMTYPETRILIST, nullptr, 3) == FALSE, "Im2D type 0 / null vertices fail");
        Check(RwIm2DRenderPrimitive(rwPRIMTYPETRILIST, t, 2) == TRUE && RwIm2DRenderPrimitive(rwPRIMTYPELINELIST, t, 1) == TRUE && RwIm2DRenderPrimitive(rwPRIMTYPEPOLYLINE, t, 1) == TRUE &&
              RwIm2DRenderPrimitive(rwPRIMTYPETRISTRIP, t, 2) == TRUE && RwIm2DRenderPrimitive(rwPRIMTYPEPOINTLIST, t, 3) == TRUE, "empty Im2D draws (too few vertices, point list) return TRUE");
        Check(RwIm2DRenderPrimitive(rwPRIMTYPETRISTRIP, t, 1) == FALSE && RwIm2DRenderPrimitive(rwPRIMTYPETRIFAN, t, 0) == FALSE, "strip / fan with too few vertices return FALSE");
        Check(RwIm2DRenderIndexedPrimitive(rwPRIMTYPETRILIST, t, 3, il, 0) == TRUE && RwIm2DRenderIndexedPrimitive(rwPRIMTYPETRILIST, t, 3, il, 2) == TRUE &&
              RwIm2DRenderIndexedPrimitive(rwPRIMTYPETRILIST, t, 3, il, -1) == FALSE && RwIm2DRenderIndexedPrimitive(rwPRIMTYPETRISTRIP, t, 3, il, 1) == FALSE &&
              RwIm2DRenderIndexedPrimitive(rwPRIMTYPENAPRIMTYPE, t, 3, il, 3) == FALSE, "Im2D indexed count contract");
        std::vector<RwImVertexIndex> many(10002, 0);
        Check(RwIm2DRenderIndexedPrimitive(rwPRIMTYPETRILIST, t, 3, many.data(), 10002) == FALSE, "> 10000 indices for fewer vertices fail (exe index buffer limit)");
        s.grab(pw, ph);
        Check(s.countNot(0, 0, 0, 0) == 0, "none of the empty / invalid calls drew anything");
    }
    // sub-raster offset: the offset of the camera's frame buffer raster is added to x / y of every vertex; the caller's vertices stay untouched
    {
        RwIm2DVertex ot[3] = { V2(10, 10, 255, 255, 0, 0), V2(110, 10, 255, 255, 0, 0), V2(10, 110, 255, 255, 0, 0) };
        cam->frameBuffer->offsetX = 7; cam->frameBuffer->offsetY = 3;
        clear();
        Check(RwIm2DRenderPrimitive(rwPRIMTYPETRILIST, ot, 3) == TRUE, "Im2D with a frame buffer offset draws");
        s.grab(pw, ph);
        Check(s.bbox(x0, y0, x1, y1) && std::abs(x0 - 17) <= 1 && std::abs(y0 - 13) <= 1 && std::abs(x1 - 116) <= 2 && std::abs(y1 - 112) <= 2, "offset (7,3) added: bbox starts at (17,13)");
        Check(ot[0].x == 10.0f && ot[0].y == 10.0f, "the caller's vertices are not modified");
        clear();
        RwIm2DRenderLine(ot, 3, 0, 1);
        s.grab(pw, ph);
        Check(s.bbox(x0, y0, x1, y1) && std::abs(x0 - 17) <= 1 && y0 >= 12 && y1 <= 14, "RwIm2DRenderLine honours the offset");
        clear();
        RwImVertexIndex oi[3] = { 0, 1, 2 };
        RwIm2DRenderIndexedPrimitive(rwPRIMTYPETRILIST, ot, 3, oi, 3);
        s.grab(pw, ph);
        Check(s.bbox(x0, y0, x1, y1) && std::abs(x0 - 17) <= 1 && std::abs(y0 - 13) <= 1, "indexed Im2D honours the offset");
        cam->frameBuffer->offsetX = cam->frameBuffer->offsetY = 0;
    }
    // more than 10000 vertices / indices (librw's start-size buffers grow): a visible triangle at the very end of a long list
    {
        std::vector<RwIm2DVertex> big(12000, V2(0, 0, 255, 0, 0, 0));
        big[11997] = V2(500, 400, 255, 255, 255, 255); big[11998] = V2(560, 400, 255, 255, 255, 255); big[11999] = V2(500, 460, 255, 255, 255, 255);
        clear();
        Check(RwIm2DRenderPrimitive(rwPRIMTYPETRILIST, big.data(), 12000) == TRUE, "Im2D 12000 vertices");
        s.grab(pw, ph);
        Check(s.is(510, 410, 255, 255, 255), "12000-vertex Im2D list: the triangle at the end is drawn (buffer grown)");
        std::vector<RwImVertexIndex> bi(12000, 0);
        bi[11997] = 11997; bi[11998] = 11998; bi[11999] = 11999;
        clear();
        Check(RwIm2DRenderIndexedPrimitive(rwPRIMTYPETRILIST, big.data(), 12000, bi.data(), 12000) == TRUE, "Im2D 12000 indices");
        s.grab(pw, ph);
        Check(s.is(510, 410, 255, 255, 255), "12000-index Im2D list: the triangle at the end is drawn");
        Check(NoGlError("Im2D big"), "glGetError clean after the big Im2D draws");
    }

    // ---- Im3D (identity camera, view window 1 x 0.75: NDC x = -X/Z, NDC y = Y/(0.75 Z)) ----
    auto ndcX = [&](double n) { return (int)(pw * 0.5 + n * pw * 0.5); };
    auto ndcY = [&](double n) { return (int)(ph * 0.5 - n * ph * 0.5); };
    {
        clear();
        RS(rwRENDERSTATEZTESTENABLE, 1);
        RS(rwRENDERSTATEZWRITEENABLE, 1);
        // triangle at z = 2: NDC (0,0) (0.5,0) (0,0.5); colour (200,50,10): R/B order
        RwIm3DVertex t[3] = { V3(0, 0, 2, 200, 50, 10), V3(-1, 0, 2, 200, 50, 10), V3(0, 0.75f, 2, 200, 50, 10) };
        Check(RwIm3DTransform(t, 3, nullptr, 0) == t, "RwIm3DTransform");
        Check(RwIm3DRenderPrimitive(rwPRIMTYPETRILIST) == TRUE, "RwIm3DRenderPrimitive(tri list)");
        Check(RwIm3DEnd() == TRUE, "RwIm3DEnd");
        s.grab(pw, ph);
        std::snprintf(what, sizeof(what), "Im3D triangle at its projected pixels: in %d,%d,%d, mirrored side %d,%d,%d", s.at(ndcX(0.1), ndcY(0.1)).r, s.at(ndcX(0.1), ndcY(0.1)).g, s.at(ndcX(0.1), ndcY(0.1)).b,
                      s.at(ndcX(-0.1), ndcY(0.1)).r, s.at(ndcX(-0.1), ndcY(0.1)).g, s.at(ndcX(-0.1), ndcY(0.1)).b);
        Check(s.is(ndcX(0.1), ndcY(0.1), 200, 50, 10) && s.is(ndcX(-0.1), ndcY(0.1), 0, 0, 0) && s.is(ndcX(0.4), ndcY(0.4), 0, 0, 0), what);
        // depth buffer holds the z-convention value (R-3b proved it for librw's own Im3D; the game path must agree)
        float dp = 0;
        glReadPixels(ndcX(0.1), ph - 1 - ndcY(0.1), 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &dp);
        const double expect = (cam->zScale / 2.0 + cam->zShift + 1.0) * 0.5;
        std::snprintf(what, sizeof(what), "Im3D depth %.4f vs zScale/zShift %.4f", (double)dp, expect);
        Check(std::fabs(dp - expect) < 2e-3, what);
        RS(rwRENDERSTATEZTESTENABLE, 0);
        RS(rwRENDERSTATEZWRITEENABLE, 0);

        // LTM translate (+1, 0, 0): X mirrored -> NDC x shifts by -0.5
        clear();
        rw::Matrix m{};
        m.right = { 1, 0, 0 }; m.up = { 0, 1, 0 }; m.at = { 0, 0, 1 }; m.pos = { 1, 0, 0 }; m.flags = rw::Matrix::TYPEORTHONORMAL;
        RwIm3DTransform(t, 3, &m, 0);
        RwIm3DRenderPrimitive(rwPRIMTYPETRILIST);
        RwIm3DEnd();
        s.grab(pw, ph);
        Check(s.is(ndcX(-0.4), ndcY(0.1), 200, 50, 10) && s.is(ndcX(0.1), ndcY(0.1), 0, 0, 0), "LTM translation moves the triangle to NDC x -0.5..0");
        // LTM scale 2 about the origin: area x4 (count against the plain triangle)
        clear();
        RwIm3DTransform(t, 3, nullptr, 0); RwIm3DRenderPrimitive(rwPRIMTYPETRILIST); RwIm3DEnd();
        s.grab(pw, ph);
        const int base = s.countNot(0, 0, 0, 0);
        clear();
        m.right = { 2, 0, 0 }; m.up = { 0, 2, 0 }; m.at = { 0, 0, 2 }; m.pos = { 0, 0, 0 };
        RwIm3DTransform(t, 3, &m, 0); RwIm3DRenderPrimitive(rwPRIMTYPETRILIST); RwIm3DEnd();
        s.grab(pw, ph);
        // x2 in x, y and z: z = 4 halves the NDC extent again -> same size as the base triangle (a wrong/ignored LTM would give 1/4 or 4x)
        const int scaled = s.countNot(0, 0, 0, 0);
        std::snprintf(what, sizeof(what), "LTM uniform scale 2: z doubles too, projected size unchanged (%d vs %d px)", scaled, base);
        Check(std::abs(scaled - base) < base / 10 + 8, what);
        // identity-flagged LTM is treated as identity whatever its numbers say
        clear();
        rw::Matrix idm{};
        idm.right = { 5, 0, 0 }; idm.up = { 0, 5, 0 }; idm.at = { 0, 0, 5 }; idm.pos = { 9, 9, 9 }; idm.flags = rw::Matrix::IDENTITY;
        RwIm3DTransform(t, 3, &idm, 0); RwIm3DRenderPrimitive(rwPRIMTYPETRILIST); RwIm3DEnd();
        s.grab(pw, ph);
        Check(s.is(ndcX(0.1), ndcY(0.1), 200, 50, 10) && std::abs(s.countNot(0, 0, 0, 0) - base) < 8, "identity-flagged LTM == no LTM");
        // after End the transform is gone
        Check(RwIm3DRenderPrimitive(rwPRIMTYPETRILIST) == FALSE, "Im3D render after End fails");

        // indexed quad with UV and a texture; index count 5 rounds down to one triangle
        RwRaster* tr = RwRasterCreate(2, 2, 32, rwRASTERTYPETEXTURE | rwRASTERFORMAT8888);
        RwUInt8*  p  = RwRasterLock(tr, 0, rwRASTERLOCKWRITE | rwRASTERLOCKNOFETCH);
        const unsigned char texel[4][4] = { { 255, 0, 0, 255 }, { 0, 255, 0, 255 }, { 0, 0, 255, 255 }, { 255, 255, 0, 255 } };
        if (p) {
            for (int y = 0; y < 2; y++) for (int x = 0; x < 2; x++) std::memcpy(p + y * RwRasterGetStride(tr) + x * 4, texel[y * 2 + x], 4);
        }
        RwRasterUnlock(tr);
        RS(rwRENDERSTATETEXTURERASTER, (uintptr_t)tr);
        // quad in NDC [-0.5,0.5] x [-0.5,0.5] at z = 2: X = -2 * ndcx, Y = 1.5 * ndcy; u grows with screen x, v grows downwards
        RwIm3DVertex q[4] = { V3(1, 0.75f, 2, 255, 255, 255, 255, 0, 0), V3(-1, 0.75f, 2, 255, 255, 255, 255, 1, 0), V3(1, -0.75f, 2, 255, 255, 255, 255, 0, 1), V3(-1, -0.75f, 2, 255, 255, 255, 255, 1, 1) };
        RwImVertexIndex qi[6] = { 0, 1, 2, 2, 1, 3 };
        clear();
        Check(RwIm3DTransform(q, 4, nullptr, rwIM3D_VERTEXUV) == q && RwIm3DRenderIndexedPrimitive(rwPRIMTYPETRILIST, qi, 6) == TRUE && RwIm3DEnd() == TRUE, "textured Im3D indexed quad");
        s.grab(pw, ph);
        {
            const Px a = s.at(ndcX(-0.25), ndcY(0.25)), b = s.at(ndcX(0.25), ndcY(0.25)), c = s.at(ndcX(-0.25), ndcY(-0.25)), d = s.at(ndcX(0.25), ndcY(-0.25));
            std::snprintf(what, sizeof(what), "Im3D texture quadrants (UV flag keeps the texture): %d,%d,%d %d,%d,%d / %d,%d,%d %d,%d,%d", a.r, a.g, a.b, b.r, b.g, b.b, c.r, c.g, c.b, d.r, d.g, d.b);
            Check(Near(a, 255, 0, 0) && Near(b, 0, 255, 0) && Near(c, 0, 0, 255) && Near(d, 255, 255, 0), what);
        }
        const int quadPx = s.countNot(0, 0, 0, 0);
        clear();
        RwIm3DTransform(q, 4, nullptr, rwIM3D_VERTEXUV);
        Check(RwIm3DRenderIndexedPrimitive(rwPRIMTYPETRILIST, qi, 5) == TRUE, "5 indices accepted (rounded to 3)");
        RwIm3DEnd();
        s.grab(pw, ph);
        std::snprintf(what, sizeof(what), "5 indices draw one triangle: %d of %d quad px", s.countNot(0, 0, 0, 0), quadPx);
        Check(s.countNot(0, 0, 0, 0) < quadPx * 6 / 10 && s.countNot(0, 0, 0, 0) > quadPx * 4 / 10, what);
        // no UV flag: untextured (vertex colour only) and TEXTURERASTER is left as the caller set it
        clear();
        RwIm3DVertex nq[4] = { V3(1, 0.75f, 2, 90, 90, 200), V3(-1, 0.75f, 2, 90, 90, 200), V3(1, -0.75f, 2, 90, 90, 200), V3(-1, -0.75f, 2, 90, 90, 200) };
        RwIm3DTransform(nq, 4, nullptr, 0);
        RwIm3DRenderIndexedPrimitive(rwPRIMTYPETRILIST, qi, 6);
        RwIm3DEnd();
        s.grab(pw, ph);
        RwRaster* back = nullptr;
        RwRenderStateGet(rwRENDERSTATETEXTURERASTER, &back);
        Check(s.is(ndcX(0.25), ndcY(0.25), 90, 90, 200) && s.is(ndcX(-0.25), ndcY(-0.25), 90, 90, 200), "no UV flag: vertex colour only");
        Check(back == tr, "no UV flag: the caller's TEXTURERASTER is restored after the draw");
        RS(rwRENDERSTATETEXTURERASTER, 0);
        RwRasterDestroy(tr);

        // vertex alpha: flags 0 -> blended (VERTEXALPHAENABLE set by the call), rwIM3D_ALLOPAQUE -> opaque
        RS(rwRENDERSTATESRCBLEND, rwBLENDSRCALPHA);
        RS(rwRENDERSTATEDESTBLEND, rwBLENDINVSRCALPHA);
        RwIm3DVertex at[3] = { V3(0, 0, 2, 255, 0, 0, 128), V3(-1, 0, 2, 255, 0, 0, 128), V3(0, 0.75f, 2, 255, 0, 0, 128) };
        clear();
        RwIm3DTransform(at, 3, nullptr, 0); RwIm3DRenderPrimitive(rwPRIMTYPETRILIST); RwIm3DEnd();
        s.grab(pw, ph);
        Check(s.is(ndcX(0.1), ndcY(0.1), 128, 0, 0, 2) && RSGet(rwRENDERSTATEVERTEXALPHAENABLE) == 1, "Im3D vertex alpha 128: 50% red over black, VERTEXALPHAENABLE on");
        clear();
        RwIm3DTransform(at, 3, nullptr, rwIM3D_ALLOPAQUE); RwIm3DRenderPrimitive(rwPRIMTYPETRILIST); RwIm3DEnd();
        s.grab(pw, ph);
        Check(s.is(ndcX(0.1), ndcY(0.1), 255, 0, 0) && RSGet(rwRENDERSTATEVERTEXALPHAENABLE) == 0, "rwIM3D_ALLOPAQUE: alpha ignored, VERTEXALPHAENABLE off");

        // lines: RwIm3DRenderLine and an indexed line list
        clear();
        RwIm3DVertex lv[3] = { V3(1, 0, 2, 255, 255, 255), V3(-1, 0, 2, 255, 255, 255), V3(0, 0.75f, 2, 255, 255, 255) };
        RwIm3DTransform(lv, 3, nullptr, 0);
        Check(RwIm3DRenderLine(0, 1) == TRUE, "RwIm3DRenderLine");
        RwIm3DEnd();
        s.grab(pw, ph);
        const int lp = s.countNot(0, 0, 0, 0);
        std::snprintf(what, sizeof(what), "Im3D line: %d px, spans NDC x -0.5..0.5 (~%d px)", lp, pw / 2);
        Check(lp >= pw / 2 - 12 && lp <= pw / 2 + 12 && (s.is(ndcX(0.0), ndcY(0.0), 255, 255, 255) || s.is(ndcX(0.0), ndcY(0.0) - 1, 255, 255, 255)), what);
        clear();
        RwIm3DTransform(lv, 3, nullptr, 0);
        RwImVertexIndex ll[4] = { 0, 1, 1, 2 };
        Check(RwIm3DRenderIndexedPrimitive(rwPRIMTYPELINELIST, ll, 4) == TRUE, "Im3D indexed line list");
        RwIm3DEnd();
        s.grab(pw, ph);
        Check(s.countNot(0, 0, 0, 0) > lp + 20, "two lines drawn");
        clear();
        RwIm3DTransform(lv, 3, nullptr, 0);
        Check(RwIm3DRenderTriangle(0, 1, 2) == TRUE, "RwIm3DRenderTriangle");
        RwIm3DEnd();
        s.grab(pw, ph);
        Check(s.is(ndcX(0.0), ndcY(0.2), 255, 255, 255) || s.is(ndcX(0.05), ndcY(0.1), 255, 255, 255), "Im3D triangle by indices");

        // > 10000 vertices (buffers grow) and > 10000 indices; 0x10000 vertices become 0
        std::vector<RwIm3DVertex> big(30003, V3(0, 0, 2, 255, 255, 255));
        big[30000] = V3(0, 0, 2, 0, 255, 255); big[30001] = V3(-1, 0, 2, 0, 255, 255); big[30002] = V3(0, 0.75f, 2, 0, 255, 255);
        clear();
        RwIm3DTransform(big.data(), 30003, nullptr, 0);
        Check(RwIm3DRenderPrimitive(rwPRIMTYPETRILIST) == TRUE, "Im3D 30003 vertices");
        RwIm3DEnd();
        s.grab(pw, ph);
        Check(s.is(ndcX(0.1), ndcY(0.1), 0, 255, 255), "30003-vertex Im3D list: the triangle at the end is drawn (buffer grown)");
        std::vector<RwIm3DVertex> bv(10, V3(0, 0, 2, 255, 0, 255));
        bv[7] = V3(0, 0, 2, 255, 0, 255); bv[8] = V3(-1, 0, 2, 255, 0, 255); bv[9] = V3(0, 0.75f, 2, 255, 0, 255);
        std::vector<RwImVertexIndex> bi2(20004, 0);
        bi2[20001] = 7; bi2[20002] = 8; bi2[20003] = 9;
        clear();
        RwIm3DTransform(bv.data(), 10, nullptr, 0);
        Check(RwIm3DRenderIndexedPrimitive(rwPRIMTYPETRILIST, bi2.data(), 20004) == TRUE, "Im3D 20004 indices");
        RwIm3DEnd();
        s.grab(pw, ph);
        Check(s.is(ndcX(0.1), ndcY(0.1), 255, 0, 255), "20004-index Im3D list: the triangle at the end is drawn");
        Check(NoGlError("Im3D"), "glGetError clean after the Im3D tests");
    }
    // Im2D after Im3D: shader / attribute state is not left in a broken state by the Im3D End
    {
        clear();
        RwIm2DVertex t[3] = { V2(10, 10, 255, 255, 0, 0), V2(110, 10, 255, 255, 0, 0), V2(10, 110, 255, 255, 0, 0) };
        RwIm2DRenderPrimitive(rwPRIMTYPETRILIST, t, 3);
        s.grab(pw, ph);
        Check(s.is(30, 30, 255, 0, 0), "Im2D still draws after the Im3D sequence");
    }
    RwCameraEndUpdate(cam);
    RwRasterDestroy(cam->frameBuffer);
    RwRasterDestroy(cam->zBuffer);
    cam->frameBuffer = cam->zBuffer = nullptr;
    cam->getFrame()->destroy();
    RwCameraDestroy(cam);
    Check(NoGlError("R-4"), "glGetError clean after R-4");
}

} // namespace

int main(int argc, char** argv) {
    int frames = 5;
    for (int i = 1; i + 1 < argc; i++) {
        if (!std::strcmp(argv[i], "--frames")) {
            frames = std::atoi(argv[i + 1]);
        }
    }
    // The engine twin's windowed-run mode (defaults to 1280x720 in the portable build); a small window keeps the test cheap
    if (!std::getenv("NOTSA_STANDALONE_WINDOWED")) {
        setenv("NOTSA_STANDALONE_WINDOWED", "640x480", 1);
    }

    std::printf("gl_smoke: SDL %d.%d.%d, video driver init\n", SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_MICRO_VERSION);
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::printf("SMOKE RESULT FAIL: SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    // exactly the flags WinMain.cpp uses in the portable build (no HIGH_PIXEL_DENSITY: 1x backing store)
    SDL_Window* win = SDL_CreateWindow("gta-reversed gl_smoke", 800, 600, SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL);
    if (!win) {
        std::printf("SMOKE RESULT FAIL: SDL_CreateWindow: %s\n", SDL_GetError());
        return 1;
    }
    std::printf("window created: %s\n", SDL_GetCurrentVideoDriver());

    // ---- engine through the game's RW API -------------------------------------------------------------------------------------------------
    Check(RwEngineInit(nullptr, 0, 0) == TRUE, "RwEngineInit");
    RwEngineOpenParams op{};
    op.displayID = win;
    Check(RwEngineOpen(&op) == TRUE, "RwEngineOpen(SDL_Window*)");
    const int numVM = RwEngineGetNumVideoModes();
    std::printf("  sub systems %d, video modes %d, current %d\n", RwEngineGetNumSubSystems(), numVM, RwEngineGetCurrentVideoMode());
    RwSubSystemInfo ssi{};
    if (RwEngineGetSubSystemInfo(&ssi, 0)) {
        std::printf("  sub system 0: %s\n", ssi.name);
    }
    for (int i = 0; i < numVM && i < 6; i++) {
        RwVideoMode vm{};
        RwEngineGetVideoModeInfo(&vm, i);
        std::printf("  mode %d: %dx%d depth %d flags %d rr %d fmt 0x%x\n", i, (int)vm.width, (int)vm.height, (int)vm.depth, (int)vm.flags, (int)vm.refRate, (unsigned)vm.format);
    }
    const int wvm = NotsaRwWindowedVideoMode();
    Check(wvm >= 0, "windowed-run video mode exists");
    Check(RwEngineSetVideoMode(wvm >= 0 ? wvm : 0) == TRUE, "RwEngineSetVideoMode");
    Check(RwEngineStart() == TRUE, "RwEngineStart (context + gl3 device + shaders)");
    if (g_fail) {
        std::printf("SMOKE RESULT FAIL: engine did not start\n");
        return 1;
    }
    Check(NoGlError("RwEngineStart"), "glGetError clean after start");
    Check(SDL_GL_GetCurrentContext() != nullptr, "GL context is current");
    Check(rw::gl3::glGlobals.window == win, "librw adopted the host window (no second window)");

    NotsaGlInfo gi{};
    Check(NotsaRwGetGlInfo(&gi), "NotsaRwGetGlInfo");
    std::printf("  GL_VERSION  : %s\n  GL_RENDERER : %s\n  GL_VENDOR   : %s\n  GLSL        : %s\n", gi.version, gi.renderer, gi.vendor, gi.glsl);
    std::printf("  context     : %d.%d %s profile\n  framebuffer : colour %d bit, alpha %d, depth %d, stencil %d  (SDL attributes: depth %d stencil %d)\n", gi.major, gi.minor, gi.core ? "core" : "compat",
                gi.redBits, gi.alphaBits, gi.depthBits, gi.stencilBits, gi.sdlDepthBits, gi.sdlStencilBits);
    std::printf("  S3TC %d, max anisotropy %.1f, max samples %d\n", (int)gi.s3tc, (double)gi.maxAnisotropy, gi.maxSamples);
    Check(gi.core, "core profile");
    Check(gi.major > 3 || (gi.major == 3 && gi.minor >= 3), "GL >= 3.3");
    Check(gi.depthBits >= 24, "default framebuffer depth >= 24");
    Check(gi.stencilBits >= 8, "default framebuffer stencil >= 8");
    Check(rw::gl3::defaultShader && rw::gl3::defaultShader_noAT && rw::gl3::defaultShader_fullLight && rw::gl3::defaultShader_fullLight_noAT, "gl3 default shaders compiled");
    Check(RwD3D9DeviceSupportsDXTTexture() == (gi.s3tc ? TRUE : FALSE), "DXT support flag matches the context");
    R3bCheckRenderStateDefaults();

    int pw = 0, ph = 0, ww = 0, wh = 0;
    SDL_GetWindowSizeInPixels(win, &pw, &ph);
    SDL_GetWindowSize(win, &ww, &wh);
    std::printf("  window %dx%d points, %dx%d pixels\n", ww, wh, pw, ph);
    Check(pw == ww && ph == wh, "1x backing store (no HIGH_PIXEL_DENSITY)");

    // ---- librw camera on the adopted window ----------------------------------------------------------------------------------------------
    rw::Camera* cam = rw::Camera::create();
    cam->setFrame(rw::Frame::create());
    cam->frameBuffer = rw::Raster::create(pw, ph, 0, rw::Raster::CAMERA);
    cam->zBuffer     = rw::Raster::create(pw, ph, 0, rw::Raster::ZBUFFER);
    Check(cam->frameBuffer && cam->zBuffer, "camera + z rasters");
    rw::SetRenderState(rw::ZTESTENABLE, 0);
    rw::SetRenderState(rw::ZWRITEENABLE, 0);

    struct Clear { rw::RGBA c; int r, g, b; } clears[] = {
        { { 255, 0, 0, 255 }, 255, 0, 0 }, { { 0, 255, 0, 255 }, 0, 255, 0 }, { { 0, 0, 255, 255 }, 0, 0, 255 },
        { { 64, 128, 192, 255 }, 64, 128, 192 }, { { 10, 20, 30, 255 }, 10, 20, 30 },
    };
    for (int f = 0; f < frames; f++) {
        SDL_PumpEvents();
        const auto& c = clears[f % 5];
        rw::RGBA col  = c.c;
        cam->beginUpdate();
        cam->clear(&col, rw::Camera::CLEARIMAGE | rw::Camera::CLEARZ | rw::Camera::CLEARSTENCIL);
        char what[96];

        // clear colour at two corners and the centre
        const Px a = ReadPixel(2, 2), b = ReadPixel(pw / 2, ph / 2), d = ReadPixel(pw - 3, ph - 3);
        std::snprintf(what, sizeof(what), "frame %d: clear colour %d,%d,%d", f, c.r, c.g, c.b);
        Check(Near(a, c.r, c.g, c.b) && Near(b, c.r, c.g, c.b) && Near(d, c.r, c.g, c.b), what);

        if (f == frames - 1 || f == 1) { // Im2D triangle (white) covering the centre and not the bottom-left corner; screen y is down
            rw::gl3::Im2DVertex v[3] = {};
            const float cx = pw * 0.5f, cy = ph * 0.5f;
            const float xs[3] = { cx - 100.0f, cx + 100.0f, cx };
            const float ys[3] = { cy + 80.0f, cy + 80.0f, cy - 100.0f };
            for (int i = 0; i < 3; i++) {
                v[i].setScreenX(xs[i]);
                v[i].setScreenY(ys[i]);
                v[i].setScreenZ(0.0f);
                v[i].setCameraZ(1.0f);
                v[i].setColor(255, 255, 255, 255);
                v[i].setU(0.0f, 1.0f);
                v[i].setV(0.0f, 1.0f);
            }
            rw::SetRenderStatePtr(rw::TEXTURERASTER, nullptr);
            rw::im2d::RenderTriangle(v, 3, 0, 1, 2);
            // centre of the triangle (y down) -> GL row from the bottom
            const Px in  = ReadPixel(pw / 2, ph - 1 - (int)(cy + 20.0f));
            const Px out = ReadPixel(4, 4);
            std::snprintf(what, sizeof(what), "frame %d: Im2D triangle covers centre, leaves the corner", f);
            Check(Near(in, 255, 255, 255) && Near(out, c.r, c.g, c.b), what);
            Check(NoGlError("Im2D triangle"), "glGetError clean after Im2D");
        }
        if (f == 2) { // stencil / depth clear values really exist in the default framebuffer
            glClearStencil(0x5A);
            glStencilMask(0xFF);
            glClear(GL_STENCIL_BUFFER_BIT);
            glClearDepth(0.5);
            glDepthMask(GL_TRUE);
            glClear(GL_DEPTH_BUFFER_BIT);
            unsigned char st = 0;
            float         dp = 0;
            glReadPixels(pw / 2, ph / 2, 1, 1, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, &st);
            glReadPixels(pw / 2, ph / 2, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &dp);
            std::snprintf(what, sizeof(what), "stencil 0x5A read back = 0x%02x, depth 0.5 read back = %.4f", st, (double)dp);
            Check(st == 0x5A && std::fabs(dp - 0.5f) < 0.001f, what);
        }
        cam->endUpdate();
        cam->showRaster(0); // vsync off
    }
    Check(NoGlError("frames"), "glGetError clean after frames");

    // ---- R-3b: the camera / raster / renderstate shim (needs the librw camera above destroyed first: it was the only user of the window's z-buffer slot)
    cam->frameBuffer->destroy();
    cam->zBuffer->destroy();
    cam->getFrame()->destroy();
    cam->destroy();
    cam = nullptr;
    RunR3b(win, pw, ph);
    RunR4(pw, ph);

    // ---- shutdown ------------------------------------------------------------------------------------------------------------------------
    Check(RwEngineStop() == TRUE, "RwEngineStop");
    Check(RwEngineClose() == TRUE, "RwEngineClose");
    Check(RwEngineTerm() == TRUE, "RwEngineTerm");
    const SDL_WindowFlags wf = SDL_GetWindowFlags(win); // the adopted window must still be valid
    Check((wf & SDL_WINDOW_OPENGL) != 0, "adopted window survived the engine (owner destroys it)");
    SDL_DestroyWindow(win);
    SDL_Quit();

    if (g_fail) {
        std::printf("SMOKE RESULT FAIL: %d check(s) failed\n", g_fail);
        return 1;
    }
    std::printf("SMOKE RESULT OK\n");
    return 0;
}

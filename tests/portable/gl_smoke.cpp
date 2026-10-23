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
//   * clean shutdown: RwEngineStop / Close / Term, window kept by the owner (adopted, not destroyed by librw), exit code 0.
// The result line is "SMOKE RESULT OK" / "SMOKE RESULT FAIL: <what>".
#include "fakerw.h"
#include "standalone/Fixups.h"
#include <src/gl/rwgl3impl.h>
#include "portable/render/engine_gl3.h"

#include <SDL3/SDL.h>

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

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
void NotsaRwRenderState_OnEngineStarted() {}    // R-3b (renderstate.cpp is not part of this build)

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

    // ---- shutdown ------------------------------------------------------------------------------------------------------------------------
    cam->frameBuffer->destroy();
    cam->zBuffer->destroy();
    cam->getFrame()->destroy();
    cam->destroy();
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

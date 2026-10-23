// Stream R, slice R-3b: portable twin of the D3D9 parts of standalone/rw/camera.cpp (those functions are `#ifndef NOTSA_PORTABLE` there).
//   RwCameraBeginUpdate / EndUpdate / Clear / ShowRaster, RwD3D9CameraAttachWindow, RequestMarkScreenshot, NotsaRwReadBackBuffer.
//
// librw's gl3 backend does the work (device beginUpdate: view / projection into cam->devView / devProj and the shader uniforms, framebuffer + viewport;
// clearCamera; showRaster = SDL_GL_SwapWindow). What the shim adds on top, all taken from the exe's D3D9 behaviour:
//   * Clear flag rules of _rwD3D9CameraClear (0x7F7C48..0x7F7F3B): a camera without a z raster drops the Z / STENCIL bits; on the main camera (frame buffer
//     not a camera texture) a Z or STENCIL request also clears the stencil buffer when there is one; the stencil is cleared to RwD3D9SetStencilClear's value
//     (glClearStencil). librw's glClear honours the stencil write mask like GL always does, D3D's Clear does not: vendor/librw gl3 clearCamera masks it open.
//   * Present: rwRASTERFLIPWAITVSYNC -> swap interval 1 (librw), NOTSA_STANDALONE_NOVSYNC=1 forces interval 0 (frame limiter option, loading screens),
//     NOTSA_STANDALONE_NOPRESENT=1 skips the swap, NOTSA_STANDALONE_SCREENSHOT=<k> / _MAX / _ARM / _BURST dump the back buffer to frame_<n>.bmp (like the D3D build).
//
// Z convention (verified by tests/portable/gl_smoke.cpp): the gl3 device range is zNear = -1, zFar = +1 (clip-space NDC), the D3D one 0..1. camera.cpp's
// CalcZShiftScale uses rw::engine->device.zNear/zFar, so zScale / zShift map camera depth to the SAME range the gl3 projection (devProj) produces;
// RwEngineInstance->dOpenDevice.zBufferNear/Far (engine_gl3.cpp) copy the range, Sprite2d's NearScreenZ / RwIm2DGetNearScreenZ read it, and gl3's Im2D
// vertex shader passes z straight into clip space. Nothing in the shim feeds a D3D 0..1 projection into GL.
//
// Honest gaps (R-n items, .notes/reports/MAC-R3b.md): the NOTSA_STANDALONE_MEMLOG block of the D3D RwCameraShowRaster (Windows heap statistics) is not ported;
// RwD3D9CameraAttachWindow only accepts the device window (there is a single SDL window).
#ifdef NOTSA_RW_LIBRW
#include "fakerw.h"
#include "standalone/Fixups.h"
#include <src/gl/rwgl3impl.h>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <strings.h>
#include <vector>

#include "camera_gl3.h"

namespace {

using rw::gl3::glGlobals;

void LogF(const char* fmt, ...) {
    char    buf[512];
    va_list a;
    va_start(a, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, a);
    va_end(a);
    notsa::standalone::Fixups::Log("%s", buf);
}

// started librw device with a current GL context (all GL calls below need one)
bool DeviceLive() {
    return rw::engine && rw::Engine::state == rw::Engine::Started && SDL_GL_GetCurrentContext() != nullptr;
}

// stencil bits of the default framebuffer, queried once (binding restored: librw caches the bound fbo)
int DefaultFramebufferStencilBits() {
    static int s_Bits = -1;
    if (s_Bits < 0) {
        GLint prev = 0, v = 0;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_STENCIL, GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE, &v);
        glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(prev));
        while (glGetError() != GL_NO_ERROR) {}
        s_Bits = v;
    }
    return s_Bits;
}

bool EnvOn(const char* name) {
    const char* v = std::getenv(name);
    return v && *v && std::strcmp(v, "0") && strcasecmp(v, "off") && strcasecmp(v, "no") && strcasecmp(v, "false");
}

// ---- screenshots (NOTSA_STANDALONE_SCREENSHOT*) ------------------------------------------------------------------------------------------------
char s_MarkShot[48];              // pending "mark_<name>.bmp" request from the input script's `mark:` lines (outside the cadence and the cap)
int  s_BurstLeft = 0, s_BurstTotal = 0;
char s_BurstName[48];
bool s_ShotArmed = false;         // NOTSA_STANDALONE_SCREENSHOT_ARM=<mark>: the cadence only starts once the script reached `mark:<mark>`

void DumpBackBuffer() {
    static int s_Every = -2;
    static int s_Frame = 0, s_Written = 0;
    if (s_Every == -2) {
        const char* e = std::getenv("NOTSA_STANDALONE_SCREENSHOT");
        s_Every       = e ? (std::atoi(e) > 0 ? std::atoi(e) : 1) : -1;
    }
    char markName[48]{};
    if (s_BurstLeft > 0) {
        std::snprintf(markName, sizeof(markName), "%s_%02d.bmp", s_BurstName, s_BurstTotal - s_BurstLeft);
        s_BurstLeft--;
    } else if (s_MarkShot[0]) {
        std::memcpy(markName, s_MarkShot, sizeof(markName));
        s_MarkShot[0] = 0;
    } else if ((std::getenv("NOTSA_STANDALONE_SCREENSHOT_ARM") && !s_ShotArmed) || s_Every < 0 ||
               s_Written >= (std::getenv("NOTSA_STANDALONE_SCREENSHOT_MAX") ? std::atoi(std::getenv("NOTSA_STANDALONE_SCREENSHOT_MAX")) : 30) || (s_Frame++ % s_Every) != 0) {
        return;
    }
    std::vector<std::uint8_t> rgb;
    int                       w = 0, h = 0;
    if (!NotsaRwReadBackBuffer(rgb, w, h)) {
        return;
    }
    char name[64];
    if (markName[0]) {
        std::snprintf(name, sizeof(name), "%s", markName);
    } else {
        std::snprintf(name, sizeof(name), "frame_%d.bmp", s_Written++);
    }
    if (FILE* f = std::fopen(name, "wb")) {
        const std::uint32_t rowBytes = (static_cast<std::uint32_t>(w) * 3 + 3) & ~3u, imgSize = rowBytes * static_cast<std::uint32_t>(h);
        std::uint8_t        hdr[54] = { 'B', 'M' };
        auto                put32   = [&](int o, std::uint32_t v) { std::memcpy(hdr + o, &v, 4); };
        put32(2, 54 + imgSize); put32(10, 54); put32(14, 40); put32(18, static_cast<std::uint32_t>(w)); put32(22, static_cast<std::uint32_t>(h));
        hdr[26] = 1; hdr[28] = 24; put32(34, imgSize);
        std::fwrite(hdr, 1, 54, f);
        std::vector<std::uint8_t> row(rowBytes, 0);
        for (int y = h - 1; y >= 0; y--) { // BMP rows are bottom-up
            const std::uint8_t* src = rgb.data() + static_cast<size_t>(y) * static_cast<size_t>(w) * 3;
            for (int x = 0; x < w; x++) {
                row[x * 3 + 0] = src[x * 3 + 2]; row[x * 3 + 1] = src[x * 3 + 1]; row[x * 3 + 2] = src[x * 3 + 0];
            }
            std::fwrite(row.data(), 1, rowBytes, f);
        }
        std::fclose(f);
    }
}

} // namespace

// W: tools/standalone/soak.sh input script `mark:<name>` lines (SDLWrapper.cpp) -> one mark_<name>.bmp at the next present (needs NOTSA_STANDALONE_SCREENSHOT)
void RequestMarkScreenshot(const char* name) {
    if (const char* b = std::getenv("NOTSA_STANDALONE_SCREENSHOT_BURST"); b && std::strncmp(name, "burst", 5) == 0 && std::atoi(b) > 0) {
        s_BurstLeft = s_BurstTotal = std::atoi(b);
        std::snprintf(s_BurstName, sizeof(s_BurstName), "%s", name);
        return;
    }
    if (const char* arm = std::getenv("NOTSA_STANDALONE_SCREENSHOT_ARM"); arm && std::strcmp(arm, name) == 0) {
        s_ShotArmed = true;
        return;
    }
    if (std::getenv("NOTSA_STANDALONE_SCREENSHOT")) {
        std::snprintf(s_MarkShot, sizeof(s_MarkShot), "mark_%s.bmp", name);
    }
}

bool NotsaRwReadBackBuffer(std::vector<std::uint8_t>& rgb, int& width, int& height) {
    if (!DeviceLive() || !glGlobals.window) {
        return false;
    }
    int w = 0, h = 0;
    SDL_GetWindowSizeInPixels(glGlobals.window, &w, &h);
    if (w <= 0 || h <= 0) {
        return false;
    }
    GLint prev = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev);
    glBindFramebuffer(GL_FRAMEBUFFER, 0); // direct call on purpose: restored below, librw's cached binding stays valid
    glReadBuffer(GL_BACK);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    std::vector<std::uint8_t> bottomUp(static_cast<size_t>(w) * static_cast<size_t>(h) * 3);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, bottomUp.data());
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(prev));
    const bool ok = glGetError() == GL_NO_ERROR;
    rgb.resize(bottomUp.size());
    const size_t rowBytes = static_cast<size_t>(w) * 3;
    for (int y = 0; y < h; y++) {
        std::memcpy(rgb.data() + static_cast<size_t>(y) * rowBytes, bottomUp.data() + static_cast<size_t>(h - 1 - y) * rowBytes, rowBytes);
    }
    width  = w;
    height = h;
    return ok;
}

// A: Camera::beginUpdate = syncDirty (LTMs, camera frustum / view matrix through camera_sync), gl3 device beginUpdate (view / projection into
// devView / devProj and the shader uniforms, fog range, render surfaces, viewport). The exe (0x7EF370) sets `curCamera` first and returns NULL if the
// device's begin-update failed; renderFrame is not touched (see camera.cpp).
RwCamera* RwCameraBeginUpdate(RwCamera* camera) {
    if (!camera || !camera->getFrame() || !camera->frameBuffer || !DeviceLive()) {
        return nullptr;
    }
    RwEngineInstance->curCamera = camera;
    camera->beginUpdate();
    return camera;
}

// D: RW clears curCamera outside an update (gl3 endUpdate is empty: there is no BeginScene / EndScene).
RwCamera* RwCameraEndUpdate(RwCamera* camera) {
    if (!camera || !DeviceLive()) {
        return nullptr;
    }
    camera->endUpdate();
    RwEngineInstance->curCamera = nullptr;
    return camera;
}

// A: Camera::clear with the exe's flag rules (see the header comment) and the stencil clear VALUE. z is cleared to 1.0 (GL default clear depth).
RwCamera* RwCameraClear(RwCamera* camera, RwRGBA* colour, RwInt32 clearMode) {
    if (!camera || !camera->frameBuffer || !DeviceLive()) {
        return nullptr;
    }
    RwUInt32 mode = static_cast<RwUInt32>(clearMode) & (rwCAMERACLEARIMAGE | rwCAMERACLEARZ | rwCAMERACLEARSTENCIL);
    // camera texture: its z raster is a depth+stencil texture (gl3 rasterCreateZbuffer); main camera: whatever the default framebuffer has
    const bool isCamTex     = camera->frameBuffer->type == rw::Raster::CAMERATEXTURE;
    const bool hasStencil   = isCamTex || DefaultFramebufferStencilBits() > 0;
    if ((mode & rwCAMERACLEARSTENCIL) && !hasStencil) {
        mode &= ~static_cast<RwUInt32>(rwCAMERACLEARSTENCIL);
    }
    if (!camera->zBuffer) { // exe 0x7F7DC8: no z raster -> no depth surface bound, RW drops Z and STENCIL
        mode &= ~static_cast<RwUInt32>(rwCAMERACLEARZ | rwCAMERACLEARSTENCIL);
    }
    if (!isCamTex && (mode & (rwCAMERACLEARZ | rwCAMERACLEARSTENCIL)) && hasStencil) { // exe 0x7F7F03..0x7F7F3B
        mode |= rwCAMERACLEARSTENCIL;
    }
    RwRGBA black{0, 0, 0, 0};
    if (mode & rwCAMERACLEARSTENCIL) {
        glClearStencil(static_cast<GLint>(NotsaRwGetStencilClear()));
    }
    if (mode & rwCAMERACLEARZ) {
        glClearDepth(1.0); // D3D Clear(.., Z = 1.0f): a stray glClearDepth elsewhere must not leak into the camera clear
    }
    camera->clear(colour ? colour : &black, mode); // binds the camera's framebuffer, sets the viewport, clears
    return camera;
}

// A: Raster::show on the camera's frame buffer = swap. flags: rwRASTERFLIPWAITVSYNC == Raster::FLIPWAITVSYNCH (librw maps it to the swap interval).
RwCamera* RwCameraShowRaster(RwCamera* camera, void* /*pDev*/, RwUInt32 flags) {
    if (!camera || !camera->frameBuffer || !DeviceLive()) {
        return nullptr;
    }
    DumpBackBuffer();
    if (EnvOn("NOTSA_STANDALONE_NOPRESENT")) { // diagnostics: skip the swap
        return camera;
    }
    // NOTSA_STANDALONE_NOVSYNC=1 (off by default): present with swap interval 0 even when the game asks for rwRASTERFLIPWAITVSYNC (menu "frame limiter"
    // option, loading screens). The game's software frame limiter and all game-logic timing are untouched.
    static const bool s_NoVsync = [] {
        const bool on = EnvOn("NOTSA_STANDALONE_NOVSYNC");
        if (on) {
            LogF("NOTSA_STANDALONE_NOVSYNC: present with swap interval 0 (the WAITVSYNC flag of RwCameraShowRaster is ignored)");
        }
        return on;
    }();
    if (s_NoVsync) {
        flags &= ~static_cast<RwUInt32>(rwRASTERFLIPWAITVSYNC);
    }
    camera->showRaster(flags);
    return camera;
}

// W: the D3D build moved the swap chain between its main and top-level windows (WindowedMode.cpp, not built). Only the one SDL window exists here:
// null / the device window succeed, anything else fails (R3b-1).
RwBool RwD3D9CameraAttachWindow(void* camera, void* hwnd) {
    if (!camera || !DeviceLive()) {
        return FALSE;
    }
    if (!hwnd || hwnd == glGlobals.window) {
        return TRUE;
    }
    LogF("R3b-1 stub: RwD3D9CameraAttachWindow to a window other than the device window is not supported (portable build)");
    return FALSE;
}

#endif // NOTSA_RW_LIBRW

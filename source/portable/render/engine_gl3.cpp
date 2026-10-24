// Stream R, slice R-3a (.notes/STREAM_R_PLAN.md): the RwEngine device layer on librw's GL3 backend (SDL3), portable build only
// (source/portable/ is globbed for the portable target; the Windows builds never see this file).
//
// standalone/rw/engine.cpp keeps everything that is renderer neutral (memory / string tables, RwEngineInit/Stop/Close/Term, sub-system and video
// mode COUNTS); its D3D9 parts are `#ifndef NOTSA_PORTABLE` on their own lines and are replaced here:
//   RwEngineOpen / Start, RwEngine{GetSubSystemInfo,SetSubSystem,GetVideoModeInfo,SetVideoMode},
//   RwD3D9{ChangeVideoMode,EngineGetMaxMultiSamplingLevels,EngineSetMultiSamplingLevels,ChangeMultiSamplingLevels,EngineSetRefreshRate,
//          GetCaps,GetCurrentD3DDevice,DeviceSupportsDXTTexture}, NotsaRw{WindowedVideoMode,WindowedRun,MsaaState}.
//
// Window: WinMain creates the SDL_Window (SDL_WINDOW_OPENGL, portable only) and hands it over as RwEngineOpenParams::displayID. librw's gl3 backend
// ADOPTS it (vendor/librw gl3device.cpp startSDL3, fork branch notsa): it creates the core 3.3 context (macOS returns 4.1 core) with depth 24 /
// stencil 8 / double buffer on that window and does not destroy the window at stop. RwEngineStart reports a failed context creation as FALSE.
// Video modes are librw's SDL display modes (mode 0 = desktop as a windowed mode); the optional windowed-run mode (below) is appended at the END.
//
// Honest gaps (logged on use, listed in .notes/reports/MAC-R3a.md):
//   * multisampling needs a new GL context: RwD3D9EngineSetMultiSamplingLevels / ChangeMultiSamplingLevels are stored and applied at the next
//     RwEngineStart ("restart required" for a running device);
//   * RwD3D9EngineSetRefreshRate is stored only (SDL fullscreen modes carry their own refresh rate);
//   * RwD3D9GetCaps / RwD3D9GetCurrentD3DDevice are nullptr (no D3D device), the callers are D3D-only code.
//
// Only needs fakerw + librw + SDL3 + the CRT: tests/portable/gl_smoke.cpp builds it (with engine.cpp / platform_gl3.cpp) without the game's PCH.
#ifdef NOTSA_RW_LIBRW
#include "../path_resolve.h"
#include "standalone/Fixups.h"
#include "fakerw.h"
#include <src/gl/rwgl3impl.h>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "portable/posix_compat_win.h"
#include <vector>

#include "engine_gl3.h"

namespace {

using rw::gl3::glGlobals;

void LogF(const char* fmt, ...) {
    char    buf[512];
    va_list a;
    va_start(a, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, a);
    va_end(a);
#ifdef NOTSA_STANDALONE_RUN
    notsa::standalone::Fixups::Log("%s", buf);
#else
    std::fprintf(stderr, "%s\n", buf);
#endif
}

bool Is(rw::Engine::State s) { return rw::Engine::state == s; }
bool IsOpenedOrStarted() { return (Is(rw::Engine::Opened) || Is(rw::Engine::Started)) && rw::engine; }
bool ModeIndexValid(int i) { return i >= 0 && i < glGlobals.numModes && glGlobals.modes; }

SDL_Window* s_Window      = nullptr; // librw keeps a POINTER to this slot (EngineOpenParams::window)
RwUInt32    s_RefreshRate = 0;

// ---- multisampling ----------------------------------------------------------------------------------------------------------------------
// GL_MAX_SAMPLES can only be read with a context: cached at RwEngineStart (1 before). The selection is applied to the context created at the next start.
uint32_t s_MsMax = 1, s_MsSel = 1;

// ---- windowed run -----------------------------------------------------------------------------------------------------------------------
// NOTSA_STANDALONE_WINDOWED=<W>x<H> (same syntax as the Windows run build): a synthetic WxH video mode, no display mode change, appended at the END of the
// mode list (indices of the real modes stay stable); psSelectDevice selects it. It is REPORTED to the game as an exclusive mode (the game's own logic then
// sees an ordinary WxH x 32 mode: RsGlobal.maximumWidth/Height, HUD scale) while librw keeps it windowed. Differs from the Windows run build: the portable
// build defaults to 1280x720 (the reference-frame size, STREAM_R_PLAN s.6.2) when the variable is unset; "0" / "off" / "no" / "false" selects the real modes.
struct WindowedEmu {
    bool inited   = false;
    int  w = 0, h = 0;
    int  synthIdx = -1;
    bool Active() const { return w > 0 && h > 0; }
} s_Emu;

void WindowedEmuInit() {
    if (s_Emu.inited) {
        return;
    }
    s_Emu.inited = true;
    const char* v = std::getenv("NOTSA_STANDALONE_WINDOWED");
    int w = 1280, h = 720;
    if (v && *v) {
        if (!std::strcmp(v, "0") || !strcasecmp(v, "off") || !strcasecmp(v, "no") || !strcasecmp(v, "false")) {
            return;
        }
        if (!(!std::strcmp(v, "1") || !strcasecmp(v, "on") || !strcasecmp(v, "yes") || !strcasecmp(v, "true"))) {
            if (std::sscanf(v, "%d%*1[xX*]%d", &w, &h) != 2 || w < 320 || h < 240 || w > 16384 || h > 16384) {
                LogF("NOTSA_STANDALONE_WINDOWED=\"%s\" is not <W>x<H> (320x240 .. 16384x16384): ignored", v);
                return;
            }
        }
    } else if (v) { // set but empty = off, as on Windows
        return;
    }
    s_Emu.w = w;
    s_Emu.h = h;
}

// Appends the synthetic mode (copy of the desktop mode 0 with the requested size, librw flags windowed). The list is librw's own allocation: replaced by a larger one.
void WindowedEmuAfterOpen() {
    WindowedEmuInit();
    s_Emu.synthIdx = -1;
    if (!s_Emu.Active() || glGlobals.numModes <= 0 || !glGlobals.modes) {
        return;
    }
    auto* modes = rwNewT(rw::gl3::DisplayMode, glGlobals.numModes + 1, rw::ID_DRIVER | rw::MEMDUR_EVENT);
    if (!modes) {
        return;
    }
    std::memcpy(modes, glGlobals.modes, sizeof(rw::gl3::DisplayMode) * (size_t)glGlobals.numModes);
    rw::Engine::memfuncs.rwfree(glGlobals.modes);
    glGlobals.modes = modes;
    auto& m         = modes[glGlobals.numModes];
    m               = modes[0];
    m.mode.w        = s_Emu.w;
    m.mode.h        = s_Emu.h;
    m.flags         = 0;
    s_Emu.synthIdx  = glGlobals.numModes++;
    LogF("windowed run: synthetic video mode %d = %dx%d (window, no display mode change)", s_Emu.synthIdx, s_Emu.w, s_Emu.h);
}

// RwVideoMode::format for a display pixel format (the exe's D3D9 table: 8888, 888, 565, 555, 1555; A2R10G10B10 counts as 8888, everything else 0)
RwInt32 RasterFormatOf(SDL_PixelFormat f) {
    switch (f) {
    case SDL_PIXELFORMAT_ARGB8888:
    case SDL_PIXELFORMAT_ARGB2101010: return rwRASTERFORMAT8888;
    case SDL_PIXELFORMAT_XRGB8888:    return rwRASTERFORMAT888;
    case SDL_PIXELFORMAT_RGB565:      return rwRASTERFORMAT565;
    case SDL_PIXELFORMAT_XRGB1555:    return rwRASTERFORMAT555;
    case SDL_PIXELFORMAT_ARGB1555:    return rwRASTERFORMAT1555;
    default:                          return rwRASTERFORMATDEFAULT;
    }
}

void PublishDeviceRange() {
    if (rw::engine) {
        RwEngineInstance->dOpenDevice.zBufferNear = rw::engine->device.zNear;
        RwEngineInstance->dOpenDevice.zBufferFar  = rw::engine->device.zFar;
    }
}

} // namespace

// RwShimPipelineEnsure / Shutdown: stubs in rw_stubs_gl3.cpp until R-6 (stock gl3 pipelines)
void RwShimPipelineEnsure();

//--------------------------------------------------------------------------------------------------
// GL info (RwEngineStart logs it; the smoke test asserts on it)
//--------------------------------------------------------------------------------------------------
// Name of the first thing NotsaRwGetGlInfo needs and does not have (nullptr = all there). The entry points come from librw's glad loader; when it failed (or the driver lacks them) they are null.
static const char* GlInfoBlocker() {
    if (!SDL_GL_GetCurrentContext())                return "no current GL context";
    if (!glGetString)                               return "glGetString";
    if (!glGetIntegerv)                             return "glGetIntegerv";
    if (!glBindFramebuffer)                         return "glBindFramebuffer";
    if (!glGetFramebufferAttachmentParameteriv)     return "glGetFramebufferAttachmentParameteriv";
    return nullptr;
}

bool NotsaRwGetGlInfo(NotsaGlInfo* out) {
    if (!out || GlInfoBlocker()) { // never call through a null GL entry point
        return false;
    }
    *out = {};
    auto str = [](GLenum e) { const auto* s = glGetString(e); return s ? reinterpret_cast<const char*>(s) : "(null)"; };
    std::snprintf(out->version, sizeof(out->version), "%s", str(GL_VERSION));
    std::snprintf(out->renderer, sizeof(out->renderer), "%s", str(GL_RENDERER));
    std::snprintf(out->vendor, sizeof(out->vendor), "%s", str(GL_VENDOR));
    std::snprintf(out->glsl, sizeof(out->glsl), "%s", str(GL_SHADING_LANGUAGE_VERSION));
    glGetIntegerv(GL_MAJOR_VERSION, &out->major);
    glGetIntegerv(GL_MINOR_VERSION, &out->minor);
    GLint profile = 0;
    glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &profile);
    out->core = (profile & GL_CONTEXT_CORE_PROFILE_BIT) != 0;
    // the pixel format of the DEFAULT framebuffer as the driver gave it (not what was requested)
    GLint v = 0;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_DEPTH, GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE, &v);
    out->depthBits = v;
    v = 0;
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_STENCIL, GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE, &v);
    out->stencilBits = v;
    v = 0;
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_BACK_LEFT, GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE, &v);
    out->redBits = v;
    v = 0;
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_BACK_LEFT, GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE, &v);
    out->alphaBits = v;
    int sdlDepth = 0, sdlStencil = 0;
    SDL_GL_GetAttribute(SDL_GL_DEPTH_SIZE, &sdlDepth);
    SDL_GL_GetAttribute(SDL_GL_STENCIL_SIZE, &sdlStencil);
    out->sdlDepthBits   = sdlDepth;
    out->sdlStencilBits = sdlStencil;
    out->s3tc           = rw::gl3::gl3Caps.dxtSupported;
    out->maxAnisotropy  = rw::gl3::gl3Caps.maxAnisotropy;
    GLint maxSamples    = 0;
    glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
    out->maxSamples = maxSamples;
    return true;
}

static void LogGlInfo() {
    NotsaGlInfo i;
    if (!NotsaRwGetGlInfo(&i)) {
        return;
    }
    LogF("GL: %s | %s | %s", i.vendor, i.renderer, i.version);
    LogF("GL: GLSL %s | context %d.%d %s profile | default framebuffer: color %d bit alpha %d, depth %d, stencil %d (SDL reports depth %d stencil %d)", i.glsl, i.major, i.minor,
         i.core ? "core" : "compatibility", i.redBits, i.alphaBits, i.depthBits, i.stencilBits, i.sdlDepthBits, i.sdlStencilBits);
    LogF("GL: S3TC %d | max anisotropy %.1f | max samples %d", (int)i.s3tc, (double)i.maxAnisotropy, i.maxSamples);
}

//--------------------------------------------------------------------------------------------------
// Engine lifecycle
//--------------------------------------------------------------------------------------------------

// A: RwEngineOpenParams{displayID = SDL_Window*} -> EngineOpenParams{window slot}. The window must have been created with SDL_WINDOW_OPENGL.
RwBool RwEngineOpen(RwEngineOpenParams* initParams) {
    if (!Is(rw::Engine::Initialized) || rw::engine || !initParams || !initParams->displayID) {
        return FALSE;
    }
    s_Window = static_cast<SDL_Window*>(initParams->displayID);
    int w = 0, h = 0;
    SDL_GetWindowSize(s_Window, &w, &h);
    static rw::EngineOpenParams p{}; // librw keeps pointers into it (window slot, title) until the engine is closed
    p.window        = &s_Window;
    p.fullscreen    = false;
    p.width         = w;
    p.height        = h;
    p.windowtitle   = "Grand Theft Auto: San Andreas";
    s_RefreshRate   = 0;
    s_MsMax = s_MsSel = 1;
    glGlobals.numDisplays = 0; // openSDL3 sets it on success; librw ignores a failing DEVICEOPEN (SDL video init / no display)
    rw::Engine::open(&p);
    if (!rw::engine || glGlobals.numDisplays <= 0 || glGlobals.numModes <= 0) {
        LogF("RwEngineOpen: no SDL display / video modes (%s)", SDL_GetError());
        if (rw::engine) {
            rw::Engine::close();
        }
        return FALSE;
    }
    rw::gl3::d3dTexelOrder = 1; // R-5: texture rasters hold d3d9 texel words (B,G,R,A bytes, 565, 4444, ...) like the exe's rasters; the TXD reader and RwRGBAToPixel rely on it
    rw::engine->filefuncs.rwfopen = &notsa::portable::RwFopenResolved; // W-7: RwStreamOpen/texture file names are Windows-style ('\\', any case)
    WindowedEmuAfterOpen();
    PublishDeviceRange();
    return TRUE;
}

// Reports a fatal engine-start problem: log line + (unless NOTSA_NO_MESSAGEBOX is set, for headless runs / smoke tests) an SDL message box; the caller then fails RwEngineStart
// and the game exits through the normal RsRwInitialize failure path.
static void ReportGlStartFailure(const char* what) {
    const char* sdlErr = SDL_GetError();
    LogF("FATAL: OpenGL 3.3 core is not available: %s (SDL: %s)", what, sdlErr && *sdlErr ? sdlErr : "-");
    if (!SDL_getenv("NOTSA_NO_MESSAGEBOX")) {
        char msg[512];
        std::snprintf(msg, sizeof(msg),
                      "This game needs an OpenGL 3.3 (core profile) capable graphics driver.\n\n%s\n\nUpdate the graphics driver, or run on a GPU / session with OpenGL 3.3 support.", what);
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Grand Theft Auto: San Andreas", msg, s_Window);
    }
}

// A: Engine::start creates the GL context (adopting the window) and initialises the gl3 device; its result is dropped by librw, so the context is checked here.
// librw's startSDL3 fails (no context kept, glGlobals.glcontext stays null) when the context cannot be created, gladLoadGLLoader fails or a required GL entry point is null.
RwBool RwEngineStart(void) {
    if (!Is(rw::Engine::Opened) || !rw::engine) {
        return FALSE;
    }
    rw::Engine::start();
    if (!glGlobals.glcontext || !SDL_GL_GetCurrentContext()) {
        ReportGlStartFailure("The OpenGL context could not be created, or the GL function loader / a required GL function is missing (see the log).");
        rw::Engine::state = rw::Engine::Opened; // what RW reports for a failed start; RwEngineClose is then valid
        return FALSE;
    }
    if (rw::gl3::gl3Caps.gles || rw::gl3::gl3Caps.glversion < 33) { // librw falls back to core 2.1 / ES contexts when 3.3 core is refused; those lack the framebuffer / UBO entry points
        char what[256];
        std::snprintf(what, sizeof(what), "The driver only provided an OpenGL %d.%d%s context.", rw::gl3::gl3Caps.glversion / 10, rw::gl3::gl3Caps.glversion % 10, rw::gl3::gl3Caps.gles ? " ES" : "");
        ReportGlStartFailure(what);
        rw::Engine::state = rw::Engine::Opened;
        return FALSE;
    }
    NotsaGlInfo gl{};
    if (!NotsaRwGetGlInfo(&gl)) {
        char what[256];
        std::snprintf(what, sizeof(what), "Required OpenGL functions could not be loaded from the driver (%s).", GlInfoBlocker());
        ReportGlStartFailure(what);
        rw::Engine::state = rw::Engine::Opened;
        return FALSE;
    }
    if (!gl.core || gl.major < 3 || (gl.major == 3 && gl.minor < 3)) {
        char what[256];
        std::snprintf(what, sizeof(what), "The driver gave an OpenGL %d.%d %s context (%s).", gl.major, gl.minor, gl.core ? "core" : "compatibility", gl.renderer);
        ReportGlStartFailure(what);
        rw::Engine::state = rw::Engine::Opened;
        return FALSE;
    }
    PublishDeviceRange();
    s_MsMax = rw::Engine::getMaxMultiSamplingLevels(); // glGetIntegerv(GL_MAX_SAMPLES): needs the context
    if (s_MsMax < 1) {
        s_MsMax = 1;
    }
    LogGlInfo();
    RwShimPipelineEnsure();            // R-6 stub for now
    NotsaRwPlatform_OnEngineStarted(); // platform_gl3.cpp
    return TRUE;
}

//--------------------------------------------------------------------------------------------------
// Sub-systems (= SDL displays) and video modes
//--------------------------------------------------------------------------------------------------

// A: librw's DEVICEGETSUBSSYSTEMINFO passes the INDEX to SDL_GetDisplayName(SDL_DisplayID), so ask SDL for the id of display `subSystemIndex`.
RwSubSystemInfo* RwEngineGetSubSystemInfo(RwSubSystemInfo* subSystemInfo, RwInt32 subSystemIndex) {
    if (!IsOpenedOrStarted() || !subSystemInfo || subSystemIndex < 0 || subSystemIndex >= glGlobals.numDisplays) {
        return nullptr;
    }
    int           count    = 0;
    SDL_DisplayID* displays = SDL_GetDisplays(&count);
    const char*   name     = (displays && subSystemIndex < count) ? SDL_GetDisplayName(displays[subSystemIndex]) : nullptr;
    if (!name) {
        SDL_free(displays);
        return nullptr;
    }
    std::strncpy(subSystemInfo->name, name, sizeof(subSystemInfo->name) - 1);
    subSystemInfo->name[sizeof(subSystemInfo->name) - 1] = '\0';
    SDL_free(displays);
    return subSystemInfo;
}

// A: only between Open and Start. Switching the display of an already adopted window is not supported by librw (the mode list is built at open): the index is
// stored for librw's bookkeeping, the mode selection restarts at 0.
RwBool RwEngineSetSubSystem(RwInt32 subSystemIndex) {
    if (!Is(rw::Engine::Opened) || !rw::engine || subSystemIndex < 0) {
        return FALSE;
    }
    if (!rw::Engine::setSubSystem(subSystemIndex)) {
        return FALSE;
    }
    glGlobals.currentMode = 0;
    return TRUE;
}

RwVideoMode* RwEngineGetVideoModeInfo(RwVideoMode* modeinfo, RwInt32 modeIndex) {
    if (!IsOpenedOrStarted() || !modeinfo || !ModeIndexValid(modeIndex)) {
        return nullptr;
    }
    rw::VideoMode vm{};
    if (!rw::Engine::getVideoModeInfo(&vm, modeIndex)) {
        return nullptr;
    }
    const auto& dm   = glGlobals.modes[modeIndex].mode;
    modeinfo->width  = vm.width;
    modeinfo->height = vm.height;
    modeinfo->depth  = vm.depth;
    modeinfo->flags  = (RwVideoModeFlag)vm.flags;
    if (modeIndex == s_Emu.synthIdx) { // windowed run: the game still sees an exclusive mode
        modeinfo->flags = (RwVideoModeFlag)(modeinfo->flags | rwVIDEOMODEEXCLUSIVE);
    }
    modeinfo->refRate = (RwInt32)(dm.refresh_rate + 0.5f);
    modeinfo->format  = RasterFormatOf(dm.format);
    return modeinfo;
}

RwBool RwEngineSetVideoMode(RwInt32 modeIndex) {
    if (!Is(rw::Engine::Opened) || !rw::engine || !ModeIndexValid(modeIndex)) {
        return FALSE;
    }
    if (!rw::Engine::setVideoMode(modeIndex)) {
        return FALSE;
    }
    if (modeIndex == s_Emu.synthIdx && s_Window) { // the window becomes the mode's size now (a live window is resized by RwD3D9ChangeVideoMode)
        SDL_SetWindowSize(s_Window, s_Emu.w, s_Emu.h);
    }
    return TRUE;
}

// Windowed run (see WindowedEmu): index of the synthetic WxH video mode to select, or -1 when it is off / not available
int NotsaRwWindowedVideoMode(void) {
    WindowedEmuInit();
    return s_Emu.Active() ? s_Emu.synthIdx : -1;
}
bool NotsaRwWindowedRun(void) {
    WindowedEmuInit();
    return s_Emu.Active();
}
// Test aid (input injector `aa:N`): the multisampling state of the engine
void NotsaRwMsaaState(char* buf, size_t n) {
    std::snprintf(buf, n, "max=%u sel=%u samples=%u dev=%s", (unsigned)s_MsMax, (unsigned)s_MsSel, (unsigned)glGlobals.numSamples, SDL_GL_GetCurrentContext() ? "gl" : "none");
}

//--------------------------------------------------------------------------------------------------
// RwD3D9 device-level API (names kept for the game code; there is no D3D9 here)
//--------------------------------------------------------------------------------------------------

// W: switches the running window to video mode `modeIndex` (exclusive <-> windowed, size). The GL context and the default framebuffer are kept.
RwBool RwD3D9ChangeVideoMode(RwInt32 modeIndex) {
    if (!Is(rw::Engine::Started) || !ModeIndexValid(modeIndex) || !glGlobals.window) {
        return FALSE;
    }
    const auto& m    = glGlobals.modes[modeIndex];
    const bool  excl = (m.flags & rw::VIDEOMODEEXCLUSIVE) != 0;
    bool        ok   = true;
    if (excl) {
        ok = SDL_SetWindowFullscreenMode(glGlobals.window, &m.mode) && SDL_SetWindowFullscreen(glGlobals.window, true);
    } else {
        ok = SDL_SetWindowFullscreen(glGlobals.window, false) && SDL_SetWindowSize(glGlobals.window, m.mode.w, m.mode.h);
    }
    SDL_SyncWindow(glGlobals.window);
    if (!ok) {
        LogF("RwD3D9ChangeVideoMode(%d): %s", modeIndex, SDL_GetError());
        return FALSE;
    }
    glGlobals.currentMode = modeIndex;
    return TRUE;
}

// The value found when the context was created (1 before)
RwUInt32 RwD3D9EngineGetMaxMultiSamplingLevels(void) { return s_MsMax; }

// selected = min(request, max), below 2 = off; librw stores it and uses it when the context is created at the next RwEngineStart
void RwD3D9EngineSetMultiSamplingLevels(RwUInt32 numLevels) {
    if (!IsOpenedOrStarted()) {
        return;
    }
    uint32_t lvl = numLevels > s_MsMax ? s_MsMax : numLevels;
    if (lvl < 2) {
        lvl = 1;
    }
    s_MsSel = lvl;
    rw::Engine::setMultiSamplingLevels(lvl);
}

// A running GL context cannot change its multisampling: stored, applies after a restart. Returns TRUE like the exe when nothing changes.
RwBool RwD3D9ChangeMultiSamplingLevels(RwUInt32 numLevels) {
    if (!Is(rw::Engine::Started)) {
        return TRUE;
    }
    uint32_t lvl = numLevels > s_MsMax ? s_MsMax : numLevels;
    if (lvl < 2) {
        lvl = 1;
    }
    if (s_MsSel == lvl) {
        return TRUE;
    }
    RwD3D9EngineSetMultiSamplingLevels(numLevels);
    LogF("R-3a: multisampling %u requested at runtime; a GL context cannot change it, it applies after a restart", (unsigned)lvl);
    return TRUE;
}

// W: remembered only (SDL fullscreen modes carry their own refresh rate)
void RwD3D9EngineSetRefreshRate(RwUInt32 refreshRate) {
    if (s_RefreshRate != refreshRate) {
        LogF("R-3a: RwD3D9EngineSetRefreshRate(%u) is stored but not applied (SDL display mode refresh rate is used)", (unsigned)refreshRate);
    }
    s_RefreshRate = refreshRate;
}

// No D3D device / caps
const void* RwD3D9GetCaps(void) { return nullptr; }
void*       RwD3D9GetCurrentD3DDevice(void) { return nullptr; }

// S3TC/DXT: known from the context (GL_EXT_texture_compression_s3tc). Before RwEngineStart there is no context: DXT is assumed (macOS GL exposes S3TC; the
// real value is logged by RwEngineStart and read by NotsaRwGetGlInfo).
RwBool RwD3D9DeviceSupportsDXTTexture(void) {
    if (!Is(rw::Engine::Started) || !SDL_GL_GetCurrentContext()) {
        return TRUE;
    }
    return rw::gl3::gl3Caps.dxtSupported ? TRUE : FALSE;
}

#endif // NOTSA_RW_LIBRW

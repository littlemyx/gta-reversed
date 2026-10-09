// P2B-02a: RenderWare engine functions on top of librw (D3D9 backend).
//   RwEngine{Init,Open,Start,Stop,Close,Term}, sub-system / video-mode enumeration, RwEngineGetVersion, RwFree,
//   RwD3D9{ChangeVideoMode,ChangeMultiSamplingLevels,EngineGetMaxMultiSamplingLevels,EngineSetMultiSamplingLevels,EngineSetRefreshRate,
//          GetCaps,GetCurrentD3DDevice,DeviceSupportsDXTTexture}.
// Function classes (see .notes/P2B_SHIM_PLAN.md): D = direct, A = adapter, W = written here because librw has nothing equivalent.
// Deliberately NOT here (not called by the game, nothing to adapt): RwEngineRegisterPlugin family (the game registers plugins through the
// per-type Rp*/Rw*RegisterPlugin thunks), RwEngineGetMetrics, RwEngineGetTextureMemorySize, RwEngineGetMaxTextureSize, RwEngineSetFocus.
// Not here either: _rwD3D9Device{Set,Get}RestoreCallback (P2B-09, platform.cpp; RwEngineStart/Stop call its NotsaRwPlatform_* hooks), RwD3D9CameraAttachWindow (P2B-02b; only WindowedMode.cpp called it).
//
// This file only needs fakerw + librw + the CRT, so the standalone unit test (tests/standalone/rw_engine_test.cpp) builds it without
// the game's PCH. Excluded from the unity build.
#ifdef NOTSA_RW_LIBRW
#include "fakerw.h"

// librw's D3D9 device internals (d3d9Globals: adapter / mode list / present parameters). Same header the library itself compiles with.
#include <src/d3d/rwd3dimpl.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <cstdarg>
#include <initializer_list>

#pragma warning(disable : 4996)

namespace {

using rw::d3d::d3d9Globals;

//--------------------------------------------------------------------------------------------------
// Memory (D3): every RW allocation goes through the CRT this image is linked with, i.e. the same heap the game's
// CMemoryMgr uses in the standalone build. The RwMemoryFunctions table the game passes (psGetMemoryFunctions) is the exe's table at 0x8D6228
// in the ORIGINAL RW layout {malloc, free, realloc, calloc}; its type is librw's layout here, so it must never be read.
//--------------------------------------------------------------------------------------------------
void* RwMalloc_(size_t sz, unsigned /*hint*/) { return sz ? std::malloc(sz) : nullptr; }
void* RwRealloc_(void* p, size_t sz, unsigned /*hint*/) { return std::realloc(p, sz); }
void  RwFree_(void* p) { std::free(p); }

//--------------------------------------------------------------------------------------------------
// RwEngineInstance->stringFuncs (the game calls these through the table, preset_view.cpp)
//--------------------------------------------------------------------------------------------------
int   SfSprintf(RwChar* b, const RwChar* f, ...) { va_list a; va_start(a, f); const int r = std::vsprintf(b, f, a); va_end(a); return r; }
int   SfVsprintf(RwChar* b, const RwChar* f, va_list a) { return std::vsprintf(b, f, a); }
RwChar* SfStrcpy(RwChar* d, const RwChar* s) { return std::strcpy(d, s); }
RwChar* SfStrncpy(RwChar* d, const RwChar* s, size_t n) { return std::strncpy(d, s, n); }
RwChar* SfStrcat(RwChar* d, const RwChar* s) { return std::strcat(d, s); }
RwChar* SfStrncat(RwChar* d, const RwChar* s, size_t n) { return std::strncat(d, s, n); }
RwChar* SfStrrchr(const RwChar* s, int c) { return const_cast<RwChar*>(std::strrchr(s, c)); }
RwChar* SfStrchr(const RwChar* s, int c) { return const_cast<RwChar*>(std::strchr(s, c)); }
RwChar* SfStrstr(const RwChar* s, const RwChar* f) { return const_cast<RwChar*>(std::strstr(s, f)); }
int   SfStrcmp(const RwChar* a, const RwChar* b) { return std::strcmp(a, b); }
int   SfStrncmp(const RwChar* a, const RwChar* b, size_t n) { return std::strncmp(a, b, n); }
int   SfStricmp(const RwChar* a, const RwChar* b) { return _stricmp(a, b); }
size_t SfStrlen(const RwChar* s) { return std::strlen(s); }
RwChar* SfStrupr(RwChar* s) { return _strupr(s); }
RwChar* SfStrlwr(RwChar* s) { return _strlwr(s); }
RwChar* SfStrtok(RwChar* s, const RwChar* d) { return std::strtok(s, d); }
int   SfSscanf(const RwChar* b, const RwChar* f, ...) { va_list a; va_start(a, f); const int r = vsscanf(b, f, a); va_end(a); return r; }

void FillStringFuncs(RwStringFunctions& sf) {
    sf.vecSprintf = SfSprintf;   sf.vecVsprintf = SfVsprintf; sf.vecStrcpy = SfStrcpy;   sf.vecStrncpy = SfStrncpy;
    sf.vecStrcat  = SfStrcat;    sf.vecStrncat  = SfStrncat;  sf.vecStrrchr = SfStrrchr; sf.vecStrchr = SfStrchr;
    sf.vecStrstr  = SfStrstr;    sf.vecStrcmp   = SfStrcmp;   sf.vecStrncmp = SfStrncmp; sf.vecStricmp = SfStricmp;
    sf.vecStrlen  = SfStrlen;    sf.vecStrupr   = SfStrupr;   sf.vecStrlwr = SfStrlwr;   sf.vecStrtok = SfStrtok;
    sf.vecSscanf  = SfSscanf;
}

//--------------------------------------------------------------------------------------------------
// State helpers. RW's rwstate sequence is Init -> Open -> Start (and back); librw tracks it in Engine::state.
//--------------------------------------------------------------------------------------------------
bool Is(rw::Engine::State s) { return rw::Engine::state == s; }
bool IsOpenedOrStarted() { return (Is(rw::Engine::Opened) || Is(rw::Engine::Started)) && rw::engine; }

bool ModeIndexValid(RwInt32 i) { return i >= 0 && i < d3d9Globals.numModes && d3d9Globals.modes; }

// Raster format RW reports for a display format (RwVideoMode::format). Table of the exe's D3D9 driver (0x7F6105 -> jump table 0x7F6AB0):
// A8R8G8B8 8888, X8R8G8B8 888, R5G6B5 565, X1R5G5B5 555, A1R5G5B5 1555, A2R10G10B10 8888; every other format (A4R4G4B4 too) -> 0
RwInt32 RasterFormatOf(D3DFORMAT f) {
    switch (f) {
    case D3DFMT_A8R8G8B8:
    case D3DFMT_A2R10G10B10: return rwRASTERFORMAT8888;
    case D3DFMT_X8R8G8B8: return rwRASTERFORMAT888;
    case D3DFMT_R5G6B5:   return rwRASTERFORMAT565;
    case D3DFMT_X1R5G5B5: return rwRASTERFORMAT555;
    case D3DFMT_A1R5G5B5: return rwRASTERFORMAT1555;
    default:              return rwRASTERFORMATDEFAULT;
    }
}

// W: the exe's video mode list (0x7F7540), whose ORDER the game's settings file depends on (VideoMode.cpp picks by index).
// librw lists X8R8G8B8 then R5G6B5 and skips A2R10G10B10; the exe lists R5G6B5, X8R8G8B8, A2R10G10B10 (formats at 0x884788). Both keep mode 0 =
// the desktop mode as a windowed mode and merge entries with equal width/height/format keeping the first position and the highest refresh rate.
// The exe also reports NO modes at all when the desktop format is not one of A8R8G8B8 / X8R8G8B8 / R5G6B5 / X1R5G5B5 / A1R5G5B5.
void BuildExeModeList() {
    auto& g = d3d9Globals;
    if (!g.d3d9) {
        return;
    }
    static const D3DFORMAT kFormats[] = { D3DFMT_R5G6B5, D3DFMT_X8R8G8B8, D3DFMT_A2R10G10B10 };
    int total = 1;
    for (const D3DFORMAT f : kFormats) {
        total += (int)g.d3d9->GetAdapterModeCount((UINT)g.adapter, f);
    }
    rw::Engine::memfuncs.rwfree(g.modes);
    g.modes = rwNewT(rw::d3d::DisplayMode, total, rw::ID_DRIVER | rw::MEMDUR_EVENT);
    std::memset(g.modes, 0, sizeof(rw::d3d::DisplayMode) * total);
    g.d3d9->GetAdapterDisplayMode((UINT)g.adapter, &g.modes[0].mode);
    g.modes[0].flags = 0;
    switch (g.modes[0].mode.Format) {
    case D3DFMT_A8R8G8B8: case D3DFMT_X8R8G8B8: case D3DFMT_R5G6B5: case D3DFMT_X1R5G5B5: case D3DFMT_A1R5G5B5:
        g.numModes = 1;
        break;
    default:
        g.numModes = 0;
        return;
    }
    for (const D3DFORMAT f : kFormats) {
        const UINT n = g.d3d9->GetAdapterModeCount((UINT)g.adapter, f);
        for (UINT j = 0; j < n; j++) {
            D3DDISPLAYMODE m{};
            g.d3d9->EnumAdapterModes((UINT)g.adapter, f, j, &m);
            int i = 1;
            for (; i < g.numModes; i++) {
                if (g.modes[i].mode.Width == m.Width && g.modes[i].mode.Height == m.Height && g.modes[i].mode.Format == m.Format) {
                    break;
                }
            }
            if (i < g.numModes) {
                if (g.modes[i].mode.RefreshRate < m.RefreshRate) {
                    g.modes[i].mode.RefreshRate = m.RefreshRate;
                }
            } else {
                g.modes[g.numModes].mode  = m;
                g.modes[g.numModes].flags = rw::VIDEOMODEEXCLUSIVE;
                g.numModes++;
            }
        }
    }
}

void PublishDeviceRange() {
    if (rw::engine) {
        RwEngineInstance->dOpenDevice.zBufferNear = rw::engine->device.zNear;
        RwEngineInstance->dOpenDevice.zBufferFar  = rw::engine->device.zFar;
    }
}

// W: refresh rate chosen via RwD3D9EngineSetRefreshRate (librw always asks for D3DPRESENT_RATE_DEFAULT)
RwUInt32 s_RefreshRate = 0;

// W: Direct3D9 device reset in place. librw only resets from inside Camera::beginUpdate (window size changed) and Device::showRaster
// (presentation interval changed); both helpers (releaseVideoMemory / restoreVideoMemory) are file-static. showRaster does the whole
// "release video memory, IDirect3DDevice9::Reset(present), restore" dance as soon as the requested interval differs from
// present.PresentationInterval, so the interval is flipped on purpose and showRaster is asked for the original one. It never dereferences
// the raster. The final Present() is a harmless extra flip.
bool ResetDeviceNow() {
    if (!rw::d3d::d3ddevice || !rw::engine) {
        return false;
    }
    auto&      present = d3d9Globals.present;
    const bool vsync   = present.PresentationInterval == D3DPRESENT_INTERVAL_ONE;
    present.PresentationInterval = vsync ? D3DPRESENT_INTERVAL_IMMEDIATE : D3DPRESENT_INTERVAL_ONE;
    rw::engine->device.showRaster(reinterpret_cast<rw::Raster*>(1), vsync ? rw::Raster::FLIPWAITVSYNCH : 0);
    return rw::d3d::d3ddevice->TestCooperativeLevel() == D3D_OK;
}

// Same back buffer / depth format choice as librw's startD3D (alpha channel when available, D24S8 for 32 bit)
void ChooseFormats(D3DFORMAT displayFmt, bool windowed, D3DFORMAT& backFmt, D3DFORMAT& depthFmt) {
    backFmt = displayFmt;
    if (displayFmt == D3DFMT_X8R8G8B8 &&
        d3d9Globals.d3d9->CheckDeviceType(d3d9Globals.adapter, D3DDEVTYPE_HAL, displayFmt, D3DFMT_A8R8G8B8, windowed) == D3D_OK) {
        backFmt = D3DFMT_A8R8G8B8;
    }
    depthFmt = rw::d3d::findFormatDepth(backFmt) == 32 ? D3DFMT_D24S8 : D3DFMT_D16;
}

DWORD s_WindowedStyle = 0; // window style before the first switch to exclusive mode (restored when going back)

// Applies the video mode `idx` to the live device (W). Returns false if the device could not be reset.
bool ApplyModeToDevice(int idx) {
    auto&       g    = d3d9Globals;
    const auto& mode = g.modes[idx];
    const bool  excl = (mode.flags & rw::VIDEOMODEEXCLUSIVE) != 0;
    auto&       p    = g.present;
    D3DFORMAT   back, depth;
    ChooseFormats(mode.mode.Format, !excl, back, depth);

    if (excl) {
        if (!s_WindowedStyle) {
            s_WindowedStyle = (DWORD)GetWindowLong(g.window, GWL_STYLE);
        }
        SetWindowLong(g.window, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        SetWindowPos(g.window, HWND_TOP, 0, 0, (int)mode.mode.Width, (int)mode.mode.Height, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        p.BackBufferWidth            = mode.mode.Width;
        p.BackBufferHeight           = mode.mode.Height;
        p.FullScreen_RefreshRateInHz = s_RefreshRate ? s_RefreshRate : D3DPRESENT_RATE_DEFAULT;
    } else {
        if (s_WindowedStyle) {
            SetWindowLong(g.window, GWL_STYLE, (LONG)s_WindowedStyle);
            SetWindowPos(g.window, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
            s_WindowedStyle = 0;
        }
        RECT r{};
        GetClientRect(g.window, &r);
        p.BackBufferWidth            = r.right > 0 ? r.right : p.BackBufferWidth;
        p.BackBufferHeight           = r.bottom > 0 ? r.bottom : p.BackBufferHeight;
        p.FullScreen_RefreshRateInHz = 0;
    }
    p.Windowed               = !excl;
    p.BackBufferFormat       = back;
    p.AutoDepthStencilFormat = depth;
    g.startMode              = mode;
    g.currentMode            = idx;
    return ResetDeviceNow();
}

} // namespace

//--------------------------------------------------------------------------------------------------
// Engine lifecycle
//--------------------------------------------------------------------------------------------------

// A: Engine::init(MemoryFunctions*). The game's memFuncs/initFlags/resArenaSize are not used (see the memory note above).
RwBool RwEngineInit(const RwMemoryFunctions* /*memFuncs*/, RwUInt32 /*initFlags*/, RwUInt32 /*resArenaSize*/) {
    rw::MemoryFunctions mf{};
    mf.rwmalloc  = RwMalloc_;
    mf.rwrealloc = RwRealloc_;
    mf.rwfree    = RwFree_;
    // rwmustmalloc / rwmustrealloc stay nil: librw substitutes its exit-on-OOM wrappers around rwmalloc/rwrealloc
    if (!rw::Engine::init(&mf)) {
        return FALSE;
    }
    FillStringFuncs(RwEngineInstance->stringFuncs);
    RwEngineInstance->curCamera   = nullptr;
    RwEngineInstance->renderFrame = 0;
    RwInitialized                 = true;
    return TRUE;
}

// A: RwEngineOpenParams{displayID = HWND} -> EngineOpenParams{window}. librw ignores a failing DEVICEOPEN (Direct3DCreate9 failed / no HAL
// adapter) and still reports "opened"; a later Engine::close would then dereference the missing IDirect3D9. That case is detected here and
// the half-opened engine is torn down by hand (Engine::close minus the device call).
RwBool RwEngineOpen(RwEngineOpenParams* initParams) {
    if (!Is(rw::Engine::Initialized) || rw::engine || !initParams || !initParams->displayID) {
        return FALSE;
    }
    rw::EngineOpenParams p{};
    p.window        = static_cast<HWND>(initParams->displayID);
    s_RefreshRate   = 0;
    s_WindowedStyle = 0;
    rw::Engine::open(&p);
    if (!d3d9Globals.d3d9) {
        for (auto* drv : rw::engine->driver) {
            rw::Engine::memfuncs.rwfree(drv);
        }
        rw::engine->dummyDefaultPipeline->destroy();
        rw::Engine::memfuncs.rwfree(rw::engine);
        rw::engine        = nullptr;
        rw::Engine::state = rw::Engine::Initialized;
        return FALSE;
    }
    BuildExeModeList();
    PublishDeviceRange();
    return TRUE;
}

// A: Engine::start. librw's DEVICEINIT result is dropped by Engine::start (a failing CreateDevice would leave state "Started" with no device).
RwBool RwEngineStart(void) {
    if (!Is(rw::Engine::Opened) || !rw::engine) {
        return FALSE;
    }
    rw::Engine::start();
    if (!rw::d3d::d3ddevice) {
        rw::Engine::state = rw::Engine::Opened; // roll back to what RW reports for a failed start; RwEngineClose is then valid
        return FALSE;
    }
    PublishDeviceRange();
    NotsaRwPlatform_OnEngineStarted(); // 09: Reset watch for the restore callback (platform.cpp)
    // librw requests D3DPRESENT_RATE_DEFAULT in startD3D; honour a rate chosen with RwD3D9EngineSetRefreshRate for exclusive modes
    if (s_RefreshRate && !d3d9Globals.present.Windowed && d3d9Globals.present.FullScreen_RefreshRateInHz != s_RefreshRate) {
        d3d9Globals.present.FullScreen_RefreshRateInHz = s_RefreshRate;
        ResetDeviceNow();
    }
    return TRUE;
}

// D (+ state check, librw logs an error and returns void)
RwBool RwEngineStop(void) {
    if (!Is(rw::Engine::Started) || !rw::engine) {
        return FALSE;
    }
    NotsaRwPlatform_OnEngineStopping(); // 09 (platform.cpp)
    rw::Engine::stop();
    return TRUE;
}

RwBool RwEngineClose(void) {
    if (!Is(rw::Engine::Opened) || !rw::engine) {
        return FALSE;
    }
    rw::Engine::close();
    RwEngineInstance->curCamera = nullptr;
    return TRUE;
}

RwBool RwEngineTerm(void) {
    if (!Is(rw::Engine::Initialized) || rw::engine) {
        return FALSE;
    }
    rw::Engine::term();
    RwInitialized = false;
    return TRUE;
}

// A: the game prints it as 3.6.0.3 (MenuManager_Draw.cpp); librw's stream version is the same number
RwUInt32 RwEngineGetVersion(void) { return rw::version; }

// A: the original RwFree went through the engine's memory functions
void RwFree(void* ptr) {
    if (ptr) {
        rw::Engine::memfuncs.rwfree ? rw::Engine::memfuncs.rwfree(ptr) : std::free(ptr);
    }
}

//--------------------------------------------------------------------------------------------------
// Sub-systems (= D3D adapters) and video modes. Original semantics: counts/indices are -1 / NULL / FALSE when the engine is not opened.
// Mode 0 is the desktop mode as a windowed mode (flags without rwVIDEOMODEEXCLUSIVE), like in RW's D3D9 driver.
//--------------------------------------------------------------------------------------------------
RwInt32 RwEngineGetNumSubSystems(void) { return IsOpenedOrStarted() ? rw::Engine::getNumSubSystems() : -1; }

RwInt32 RwEngineGetCurrentSubSystem(void) { return IsOpenedOrStarted() ? rw::Engine::getCurrentSubSystem() : -1; }

// A: librw's DEVICEGETSUBSSYSTEMINFO always describes the CURRENT adapter (it ignores `n`), so ask Direct3D directly.
RwSubSystemInfo* RwEngineGetSubSystemInfo(RwSubSystemInfo* subSystemInfo, RwInt32 subSystemIndex) {
    if (!IsOpenedOrStarted() || !subSystemInfo || subSystemIndex < 0 || subSystemIndex >= d3d9Globals.numAdapters) {
        return nullptr;
    }
    D3DADAPTER_IDENTIFIER9 id;
    if (d3d9Globals.d3d9->GetAdapterIdentifier((UINT)subSystemIndex, 0, &id) != D3D_OK) {
        return nullptr;
    }
    std::strncpy(subSystemInfo->name, id.Description, sizeof(subSystemInfo->name) - 1);
    subSystemInfo->name[sizeof(subSystemInfo->name) - 1] = '\0';
    return subSystemInfo;
}

// A: only between Open and Start (as in RW); librw rebuilds the mode list but keeps a stale current mode, RW selects the default one
RwBool RwEngineSetSubSystem(RwInt32 subSystemIndex) {
    if (!Is(rw::Engine::Opened) || !rw::engine || subSystemIndex < 0) {
        return FALSE;
    }
    if (!rw::Engine::setSubSystem(subSystemIndex)) {
        return FALSE;
    }
    BuildExeModeList(); // the exe rebuilds the list too (0x7F7540)
    d3d9Globals.currentMode = 0;
    return TRUE;
}

RwInt32 RwEngineGetNumVideoModes(void) { return IsOpenedOrStarted() ? rw::Engine::getNumVideoModes() : -1; }

RwInt32 RwEngineGetCurrentVideoMode(void) { return IsOpenedOrStarted() ? rw::Engine::getCurrentVideoMode() : -1; }

// A: librw's VideoMode has no refresh rate / raster format; both come from the D3DDISPLAYMODE the enumeration stored
RwVideoMode* RwEngineGetVideoModeInfo(RwVideoMode* modeinfo, RwInt32 modeIndex) {
    if (!IsOpenedOrStarted() || !modeinfo || !ModeIndexValid(modeIndex)) {
        return nullptr;
    }
    rw::VideoMode vm{};
    if (!rw::Engine::getVideoModeInfo(&vm, modeIndex)) {
        return nullptr;
    }
    const auto& dm  = d3d9Globals.modes[modeIndex].mode;
    modeinfo->width   = vm.width;
    modeinfo->height  = vm.height;
    modeinfo->depth   = vm.depth;
    modeinfo->flags   = (RwVideoModeFlag)vm.flags;
    modeinfo->refRate = (RwInt32)dm.RefreshRate;
    modeinfo->format  = RasterFormatOf(dm.Format);
    return modeinfo;
}

RwBool RwEngineSetVideoMode(RwInt32 modeIndex) {
    if (!Is(rw::Engine::Opened) || !rw::engine || !ModeIndexValid(modeIndex)) {
        return FALSE;
    }
    return rw::Engine::setVideoMode(modeIndex) ? TRUE : FALSE;
}

//--------------------------------------------------------------------------------------------------
// RwD3D9 device-level API
//--------------------------------------------------------------------------------------------------

// W: switch the running device to video mode `modeIndex` (exclusive <-> windowed, size, format) and reset it
RwBool RwD3D9ChangeVideoMode(RwInt32 modeIndex) {
    if (!Is(rw::Engine::Started) || !rw::d3d::d3ddevice || !ModeIndexValid(modeIndex)) {
        return FALSE;
    }
    return ApplyModeToDevice(modeIndex) ? TRUE : FALSE;
}

RwUInt32 RwD3D9EngineGetMaxMultiSamplingLevels(void) {
    if (!IsOpenedOrStarted() || !d3d9Globals.d3d9 || !ModeIndexValid(d3d9Globals.currentMode)) {
        return 1;
    }
    // librw queries the format of `startMode`, which is only filled by Start; use the selected mode so this also works before Start
    const auto& m    = d3d9Globals.modes[d3d9Globals.currentMode];
    const bool  wind = !(m.flags & rw::VIDEOMODEEXCLUSIVE);
    for (UINT lvl = D3DMULTISAMPLE_16_SAMPLES; lvl > D3DMULTISAMPLE_NONMASKABLE; lvl--) {
        DWORD quality = 0;
        if (SUCCEEDED(d3d9Globals.d3d9->CheckDeviceMultiSampleType((UINT)d3d9Globals.adapter, D3DDEVTYPE_HAL, m.mode.Format, wind, (D3DMULTISAMPLE_TYPE)lvl, &quality))) {
            return lvl;
        }
    }
    return 1;
}

// A: Engine::setMultiSamplingLevels with RW's clamp to what the adapter supports (applied when the device is created)
void RwD3D9EngineSetMultiSamplingLevels(RwUInt32 numLevels) {
    if (!IsOpenedOrStarted()) {
        return;
    }
    const auto maxLevels = RwD3D9EngineGetMaxMultiSamplingLevels();
    rw::Engine::setMultiSamplingLevels(numLevels > maxLevels ? maxLevels : numLevels);
}

// A: set + apply to the running device
RwBool RwD3D9ChangeMultiSamplingLevels(RwUInt32 numLevels) {
    if (!Is(rw::Engine::Started) || !rw::d3d::d3ddevice) {
        return FALSE;
    }
    RwD3D9EngineSetMultiSamplingLevels(numLevels);
    const auto lvl = d3d9Globals.msLevel;
    d3d9Globals.present.MultiSampleType    = lvl <= 1 ? D3DMULTISAMPLE_NONE : (D3DMULTISAMPLE_TYPE)lvl;
    d3d9Globals.present.MultiSampleQuality = 0;
    return ResetDeviceNow() ? TRUE : FALSE;
}

// W: remembered; librw has no refresh-rate parameter, it is applied by RwEngineStart / ApplyModeToDevice
void RwD3D9EngineSetRefreshRate(RwUInt32 refreshRate) { s_RefreshRate = refreshRate; }

// A: the D3DCAPS9 of the adapter the engine opened (valid from RwEngineOpen on)
const void* RwD3D9GetCaps(void) { return IsOpenedOrStarted() && d3d9Globals.d3d9 ? &d3d9Globals.caps : nullptr; }

// D
void* RwD3D9GetCurrentD3DDevice(void) { return rw::d3d::d3ddevice; }

// A: DXT1/3/5 texture support for the selected display format (RW caches the same three CheckDeviceFormat results at start)
RwBool RwD3D9DeviceSupportsDXTTexture(void) {
    if (!IsOpenedOrStarted() || !d3d9Globals.d3d9 || !ModeIndexValid(d3d9Globals.currentMode)) {
        return FALSE;
    }
    const auto fmt = d3d9Globals.modes[d3d9Globals.currentMode].mode.Format;
    for (const D3DFORMAT dxt : {D3DFMT_DXT1, D3DFMT_DXT3, D3DFMT_DXT5}) {
        if (FAILED(d3d9Globals.d3d9->CheckDeviceFormat((UINT)d3d9Globals.adapter, D3DDEVTYPE_HAL, fmt, 0, D3DRTYPE_TEXTURE, dxt))) {
            return FALSE;
        }
    }
    return TRUE;
}

#endif // NOTSA_RW_LIBRW

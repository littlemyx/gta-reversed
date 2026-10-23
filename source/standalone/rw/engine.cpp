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
#include "standalone/Fixups.h"
#include "fakerw.h"

// librw's D3D9 device internals (d3d9Globals: adapter / mode list / present parameters). Same header the library itself compiles with.
#ifndef NOTSA_PORTABLE
#include <src/d3d/rwd3dimpl.h>
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <cstdarg>
#include <algorithm>
#include <vector>
#include <initializer_list>

#pragma warning(disable : 4996)

namespace {

#ifndef NOTSA_PORTABLE
using rw::d3d::d3d9Globals;
#endif

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

#ifndef NOTSA_PORTABLE
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

#ifdef NOTSA_STANDALONE_RUN
//--------------------------------------------------------------------------------------------------
// NOTSA_STANDALONE_WINDOWED=<W>x<H> (run build only; off by default): windowed run for test/soak/measurement tooling. NO desktop mode change, no topmost window.
// Every exclusive video mode of the list is turned INTO a windowed one inside the device layer (the librw mode flags lose rwVIDEOMODEEXCLUSIVE, so startD3D creates
// a windowed device sized to the window client area and ApplyModeToDevice sizes the window to the mode), while RwEngineGetVideoModeInfo keeps REPORTING them as exclusive:
// the game's own logic (fullScreen, RsGlobal.maximumWidth/Height, HUD scale, the mode list) sees an ordinary exclusive WxH x 32 mode. One synthetic WxH mode is
// appended at the END of the list (indices of the real modes, which gta_sa.set stores, are untouched); psSelectDevice selects it. "0", "off" or an empty value opt out
// (soak.sh / census.py set 960x600 by default), "1"/"on" = 960x600.
//--------------------------------------------------------------------------------------------------
struct WindowedEmu {
    bool inited = false;
    int  w = 0, h = 0;
    int  synthIdx = -1;                     // index of the appended WxH mode in the current list (-1 = none: unsupported desktop format)
    std::vector<unsigned char> reported;    // per mode index: reported to the game as rwVIDEOMODEEXCLUSIVE
    bool Active() const { return w > 0 && h > 0; }
    bool Excl(int i) const { return i >= 0 && i < (int)reported.size() && reported[i]; }
};
WindowedEmu s_Emu;

void WindowedEmuInit() {
    if (s_Emu.inited) {
        return;
    }
    s_Emu.inited = true;
    const char* v = std::getenv("NOTSA_STANDALONE_WINDOWED");
    if (!v || !*v || !std::strcmp(v, "0") || !_stricmp(v, "off") || !_stricmp(v, "no") || !_stricmp(v, "false")) {
        return;
    }
    int w = 0, h = 0;
    if (!std::strcmp(v, "1") || !_stricmp(v, "on") || !_stricmp(v, "yes") || !_stricmp(v, "true")) {
        w = 960; h = 600;
    } else if (std::sscanf(v, "%d%*1[xX*]%d", &w, &h) != 2 || w < 320 || h < 240 || w > 16384 || h > 16384) {
        notsa::standalone::Fixups::Log("NOTSA_STANDALONE_WINDOWED=\"%s\" is not <W>x<H> (320x240 .. 16384x16384): ignored", v);
        return;
    }
    s_Emu.w = w;
    s_Emu.h = h;
    notsa::standalone::Fixups::Log("NOTSA_STANDALONE_WINDOWED: windowed device %dx%d, no desktop mode change", w, h);
}

// Sizes the game window so its CLIENT area is w x h (caption, no sizing border: the back buffer size is fixed), centred on the primary monitor's work area,
// not topmost, not activated. Called before the device exists (RwEngineSetVideoMode) and on every live mode change (ApplyModeToDevice).
void WindowedEmuPlaceWindow(int w, int h) {
    HWND wnd = d3d9Globals.window;
    if (!wnd) {
        return;
    }
    const DWORD style = WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | (IsWindowVisible(wnd) ? WS_VISIBLE : 0);
    RECT r{ 0, 0, w, h };
    AdjustWindowRect(&r, style & ~WS_VISIBLE, FALSE);
    int ow = r.right - r.left, oh = r.bottom - r.top;
    for (int pass = 0; pass < 2; pass++) {
        MONITORINFO mi{ sizeof(mi) };
        GetMonitorInfo(MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY), &mi);
        const int x = (int)mi.rcWork.left + (int)(std::max)(0L, (mi.rcWork.right - mi.rcWork.left - ow) / 2);
        const int y = (int)mi.rcWork.top + (int)(std::max)(0L, (mi.rcWork.bottom - mi.rcWork.top - oh) / 2);
        SetWindowLong(wnd, GWL_STYLE, (LONG)style);
        SetWindowPos(wnd, HWND_NOTOPMOST, x, y, ow, oh, SWP_FRAMECHANGED | SWP_NOACTIVATE);
        RECT c{};
        GetClientRect(wnd, &c);
        if (c.right == w && c.bottom == h) {
            break;
        }
        ow += w - c.right; // per-monitor DPI differences in the frame size: correct once
        oh += h - c.bottom;
    }
    RECT c{};
    GetClientRect(wnd, &c);
    notsa::standalone::Fixups::Log("windowed: window client %ldx%ld (wanted %dx%d)", c.right, c.bottom, w, h);
}

// Applies the emulation to the freshly built mode list (mode 0 = desktop windowed mode stays as it is).
void WindowedEmuAfterModeList() {
    auto& g = d3d9Globals;
    WindowedEmuInit();
    s_Emu.synthIdx = -1;
    s_Emu.reported.assign((size_t)g.numModes + 1, 0);
    if (!s_Emu.Active() || g.numModes <= 0) {
        return;
    }
    for (int i = 0; i < g.numModes; i++) {
        if (g.modes[i].flags & rw::VIDEOMODEEXCLUSIVE) {
            s_Emu.reported[i] = 1;
            g.modes[i].flags &= ~rw::VIDEOMODEEXCLUSIVE;
        }
    }
    // the list was allocated with one spare entry (BuildExeModeList)
    auto& m       = g.modes[g.numModes];
    m             = g.modes[0];
    m.mode.Width  = (UINT)s_Emu.w;
    m.mode.Height = (UINT)s_Emu.h;
    m.flags       = 0;
    s_Emu.reported[g.numModes] = 1;
    s_Emu.synthIdx = g.numModes++;
}
#endif

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
    int total = 1 + 1; // +1: spare entry for the windowed-run synthetic mode (WindowedEmuAfterModeList)
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
#ifdef NOTSA_STANDALONE_RUN
        s_Emu.synthIdx = -1;
        s_Emu.reported.clear();
#endif
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
#ifdef NOTSA_STANDALONE_RUN
    WindowedEmuAfterModeList();
    notsa::standalone::Fixups::Log("rw video modes: %d (desktop %ux%u fmt %d)", g.numModes, g.modes[0].mode.Width, g.modes[0].mode.Height, (int)g.modes[0].mode.Format);
    for (int i = 0; i < g.numModes && i < 64; i++) {
        notsa::standalone::Fixups::Log("  mode %d: %ux%u fmt %d rr %u flags %d", i, g.modes[i].mode.Width, g.modes[i].mode.Height, (int)g.modes[i].mode.Format, g.modes[i].mode.RefreshRate, (int)g.modes[i].flags);
    }
    // depth/stencil formats the device accepts per display format (the exe tries D15S1 -> D16 in 16 bit, D24S8 -> D24X4S4 -> D32 -> D24X8 in 32 bit, 0x7F6DF1..0x7F6FED)
    for (const D3DFORMAT disp : { D3DFMT_R5G6B5, D3DFMT_X8R8G8B8 }) {
        notsa::standalone::Fixups::Log("  depth formats for display fmt %d: D16=%d D15S1=%d D24S8=%d D24X4S4=%d D24X8=%d D32=%d", (int)disp,
            g.d3d9->CheckDeviceFormat((UINT)g.adapter, D3DDEVTYPE_HAL, disp, D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, D3DFMT_D16) == D3D_OK,
            g.d3d9->CheckDeviceFormat((UINT)g.adapter, D3DDEVTYPE_HAL, disp, D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, D3DFMT_D15S1) == D3D_OK,
            g.d3d9->CheckDeviceFormat((UINT)g.adapter, D3DDEVTYPE_HAL, disp, D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, D3DFMT_D24S8) == D3D_OK,
            g.d3d9->CheckDeviceFormat((UINT)g.adapter, D3DDEVTYPE_HAL, disp, D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, D3DFMT_D24X4S4) == D3D_OK,
            g.d3d9->CheckDeviceFormat((UINT)g.adapter, D3DDEVTYPE_HAL, disp, D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, D3DFMT_D24X8) == D3D_OK,
            g.d3d9->CheckDeviceFormat((UINT)g.adapter, D3DDEVTYPE_HAL, disp, D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, D3DFMT_D32) == D3D_OK);
    }
#endif
}

#endif
void PublishDeviceRange() {
    if (rw::engine) {
        RwEngineInstance->dOpenDevice.zBufferNear = rw::engine->device.zNear;
        RwEngineInstance->dOpenDevice.zBufferFar  = rw::engine->device.zFar;
    }
}

#ifndef NOTSA_PORTABLE
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

// ---- Multisampling, as in the exe's RW (PLAYTEST-1 U17) ------------------------------------------------------------------------------------
// The exe keeps four globals: 0x8E242C max level, 0x8E2430 selected level, 0x8E2434 number of NONMASKABLE quality levels (+1), 0x8E2438 the
// selected NONMASKABLE quality "level". librw only knows `msLevel` and puts it into D3DPRESENT_PARAMETERS verbatim, so a level the adapter does not
// support (the menu offers 1..min(4, max), i.e. 3 samples on an NVIDIA GPU) made every Reset fail, and the exe's choice of the quality level
// (highest the runtime reports) was lost. librw's msLevel is therefore never set (stays 0 = no multisampling at startD3D); the exe's logic
// below fills the present parameters instead.
struct MsState {
    uint32_t maxLevel = 1, sel = 1, nmCount = 1, nonMask = 1;
} s_Ms;

D3DFORMAT ModeDisplayFormat() {
    return ModeIndexValid(d3d9Globals.currentMode) ? d3d9Globals.modes[d3d9Globals.currentMode].mode.Format : D3DFMT_X8R8G8B8;
}
bool ModeIsWindowed() { // exe: !(flags & 1) of the selected mode (flags at 0xC9BEF8)
    return ModeIndexValid(d3d9Globals.currentMode) ? !(d3d9Globals.modes[d3d9Globals.currentMode].flags & rw::VIDEOMODEEXCLUSIVE) : true;
}

// exe 0x7F6BF0 (same body inlined at 0x7F5FE0 = video mode setter, 0x7F65DA): all four globals = 1, then every plain level 2..16 that
// CheckDeviceMultiSampleType accepts becomes the max, then the NONMASKABLE quality count (+1) raises the max if it is larger.
void MsFindLevels(D3DFORMAT fmt, bool windowed) {
    s_Ms = {};
    if (!d3d9Globals.d3d9) {
        return;
    }
    for (UINT lvl = 2; lvl <= 16; lvl++) {
        if (SUCCEEDED(d3d9Globals.d3d9->CheckDeviceMultiSampleType((UINT)d3d9Globals.adapter, D3DDEVTYPE_HAL, fmt, windowed, (D3DMULTISAMPLE_TYPE)lvl, nullptr))) {
            s_Ms.maxLevel = lvl;
        }
    }
    DWORD quality = 0;
    if (SUCCEEDED(d3d9Globals.d3d9->CheckDeviceMultiSampleType((UINT)d3d9Globals.adapter, D3DDEVTYPE_HAL, fmt, windowed, D3DMULTISAMPLE_NONMASKABLE, &quality))) {
        s_Ms.nmCount = quality + 1;
        if (s_Ms.nmCount > s_Ms.maxLevel) {
            s_Ms.maxLevel = s_Ms.nmCount;
        }
    }
}

// exe 0x7F84F0 RwD3D9EngineSetMultiSamplingLevels: selected = min(request, max); below 2 -> off; else step down until the runtime accepts the type
// (so 3 samples becomes 2 on hardware without 3x); the NONMASKABLE quality "level" scales with the request.
void MsSetLevels(uint32_t numLevels) {
    const uint32_t lvl = numLevels > s_Ms.maxLevel ? s_Ms.maxLevel : numLevels;
    s_Ms.sel = lvl;
    if (lvl < 2) {
        s_Ms.sel     = 1;
        s_Ms.nonMask = 1;
        return;
    }
    const auto fmt      = ModeDisplayFormat();
    const bool windowed = ModeIsWindowed();
    while (FAILED(d3d9Globals.d3d9->CheckDeviceMultiSampleType((UINT)d3d9Globals.adapter, D3DDEVTYPE_HAL, fmt, windowed, (D3DMULTISAMPLE_TYPE)s_Ms.sel, nullptr))) {
        if (--s_Ms.sel < 2) {
            break;
        }
    }
    s_Ms.nonMask = (uint32_t)(((uint64_t)s_Ms.nmCount * lvl) / s_Ms.maxLevel);
}

// exe 0x7F6CB0 (the tail, 0x7F6FFD..0x7F70E2): multisample type / quality of the presentation parameters for the back buffer format and windowed flag
// in `p`. The selected plain type with the HIGHEST quality level the runtime reports; else NONMASKABLE with the quality derived from the NONMASKABLE
// level; no multisampling when neither is available. Multisampling forces D3DSWAPEFFECT_DISCARD.
void MsApplyToPresent(D3DPRESENT_PARAMETERS& p) {
    p.MultiSampleType    = D3DMULTISAMPLE_NONE;
    p.MultiSampleQuality = 0;
    if (!(s_Ms.sel > 1 || s_Ms.nonMask > 1) || !d3d9Globals.d3d9) {
        return;
    }
    DWORD   q  = 0;
    HRESULT hr = d3d9Globals.d3d9->CheckDeviceMultiSampleType((UINT)d3d9Globals.adapter, D3DDEVTYPE_HAL, p.BackBufferFormat, p.Windowed, (D3DMULTISAMPLE_TYPE)s_Ms.sel, &q);
    if (SUCCEEDED(hr) && q > 0) {
        p.MultiSampleType    = (D3DMULTISAMPLE_TYPE)s_Ms.sel;
        p.MultiSampleQuality = q - 1;
    } else {
        q  = 0;
        hr = d3d9Globals.d3d9->CheckDeviceMultiSampleType((UINT)d3d9Globals.adapter, D3DDEVTYPE_HAL, p.BackBufferFormat, p.Windowed, D3DMULTISAMPLE_NONMASKABLE, &q);
        if (SUCCEEDED(hr) && q > 0) {
            uint32_t nq = s_Ms.nonMask - 2; // unsigned, as in the exe
            if (nq > q - 1) {
                nq = q - 1;
            }
            p.MultiSampleType    = D3DMULTISAMPLE_NONMASKABLE;
            p.MultiSampleQuality = nq;
        }
    }
    if (p.MultiSampleType != D3DMULTISAMPLE_NONE) {
        p.SwapEffect = D3DSWAPEFFECT_DISCARD;
    }
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
#ifdef NOTSA_STANDALONE_RUN
    if (s_Emu.Excl(idx)) { // windowed run: the "exclusive" mode is a window of the mode's size (client area == back buffer)
        WindowedEmuPlaceWindow((int)mode.mode.Width, (int)mode.mode.Height);
    }
#endif

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
    MsApplyToPresent(p); // exe 0x7F86B5: the present parameters are rebuilt (multisampling included) for the new mode
    return ResetDeviceNow();
}

#endif
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

#ifndef NOTSA_PORTABLE
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
    MsFindLevels(ModeDisplayFormat(), ModeIsWindowed()); // exe 0x7F6468
    return TRUE;
}

#endif
// P2B-08: the RxPipeline façade (pipeline.cpp) must replace the D3D9 default pipeline after every Engine::start/open and be dropped before the engine
// stops. Units that link engine.cpp without pipeline.cpp (the lower-level unit tests) get no-ops through /alternatename.
void RwShimPipelineEnsure();
void RwShimPipelineShutdown();
#ifndef NOTSA_PORTABLE
void NotsaRwEngineNoPipeline() {}
#pragma comment(linker, "/alternatename:?RwShimPipelineEnsure@@YAXXZ=?NotsaRwEngineNoPipeline@@YAXXZ")
#pragma comment(linker, "/alternatename:?RwShimPipelineShutdown@@YAXXZ=?NotsaRwEngineNoPipeline@@YAXXZ")
#endif

#ifndef NOTSA_PORTABLE
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
    RwShimPipelineEnsure(); // 08: default pipeline façade, custom pipelines created by the game afterwards (CCustomBuildingRenderer::Initialise ...)
    NotsaRwPlatform_OnEngineStarted(); // 09: Reset watch for the restore callback (platform.cpp)
    // librw requests D3DPRESENT_RATE_DEFAULT in startD3D; honour a rate chosen with RwD3D9EngineSetRefreshRate for exclusive modes
    bool reset = false;
    if (s_RefreshRate && !d3d9Globals.present.Windowed && d3d9Globals.present.FullScreen_RefreshRateInHz != s_RefreshRate) {
        d3d9Globals.present.FullScreen_RefreshRateInHz = s_RefreshRate;
        reset = true;
    }
    // multisampling selected before the start (psSelectDevice): startD3D creates the device without it, the exe's present parameters carry it
    const D3DMULTISAMPLE_TYPE oldType = d3d9Globals.present.MultiSampleType;
    const DWORD               oldQ    = d3d9Globals.present.MultiSampleQuality;
    MsApplyToPresent(d3d9Globals.present);
    if (oldType != d3d9Globals.present.MultiSampleType || oldQ != d3d9Globals.present.MultiSampleQuality) {
        reset = true;
    }
    if (reset) {
        ResetDeviceNow();
    }
    return TRUE;
}

#endif
// D (+ state check, librw logs an error and returns void)
RwBool RwEngineStop(void) {
    if (!Is(rw::Engine::Started) || !rw::engine) {
        return FALSE;
    }
    NotsaRwPlatform_OnEngineStopping(); // 09 (platform.cpp)
    RwShimPipelineShutdown(); // 08 (pipeline.cpp)
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

#ifndef NOTSA_PORTABLE
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

#endif
RwInt32 RwEngineGetNumVideoModes(void) { return IsOpenedOrStarted() ? rw::Engine::getNumVideoModes() : -1; }

RwInt32 RwEngineGetCurrentVideoMode(void) { return IsOpenedOrStarted() ? rw::Engine::getCurrentVideoMode() : -1; }

#ifndef NOTSA_PORTABLE
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
#ifdef NOTSA_STANDALONE_RUN
    if (s_Emu.Excl(modeIndex)) { // windowed run: the game still sees an exclusive mode
        modeinfo->flags = (RwVideoModeFlag)(modeinfo->flags | rwVIDEOMODEEXCLUSIVE);
    }
#endif
    modeinfo->refRate = (RwInt32)dm.RefreshRate;
    modeinfo->format  = RasterFormatOf(dm.Format);
    return modeinfo;
}

RwBool RwEngineSetVideoMode(RwInt32 modeIndex) {
    if (!Is(rw::Engine::Opened) || !rw::engine || !ModeIndexValid(modeIndex)) {
        return FALSE;
    }
    const bool ok = rw::Engine::setVideoMode(modeIndex);
    if (ok) {
        MsFindLevels(ModeDisplayFormat(), ModeIsWindowed()); // exe 0x7F5FE0: the levels the adapter offers for this mode's format
    }
#ifdef NOTSA_STANDALONE_RUN
    if (ok && s_Emu.Excl(modeIndex)) { // windowed run: size the window now, the device is created windowed with its client size at RwEngineStart
        WindowedEmuPlaceWindow((int)d3d9Globals.modes[modeIndex].mode.Width, (int)d3d9Globals.modes[modeIndex].mode.Height);
    }
#endif
    return ok ? TRUE : FALSE;
}

#ifdef NOTSA_STANDALONE_RUN
// NOTSA_STANDALONE_WINDOWED (see WindowedEmu above). Index of the synthetic WxH video mode to select, or -1 when the windowed run is off / impossible.
int NotsaRwWindowedVideoMode(void) {
    WindowedEmuInit();
    return s_Emu.Active() ? s_Emu.synthIdx : -1;
}
// Test aid (input injector `aa:N`): the multisampling state of the engine and of the LIVE swap chain, "max=8 msLevel=4 live=4 q=0 ..."
void NotsaRwMsaaState(char* buf, size_t n) {
    D3DPRESENT_PARAMETERS live{};
    bool                  haveLive = false;
    if (rw::d3d::d3ddevice) {
        IDirect3DSwapChain9* sc = nullptr;
        if (SUCCEEDED(rw::d3d::d3ddevice->GetSwapChain(0, &sc)) && sc) {
            haveLive = SUCCEEDED(sc->GetPresentParameters(&live));
            sc->Release();
        }
    }
    std::snprintf(buf, n, "max=%u sel=%u nm=%u nonmask=%u live_type=%d live_quality=%u present_type=%d windowed=%d back=%ux%u dev=%s", RwD3D9EngineGetMaxMultiSamplingLevels(), (unsigned)s_Ms.sel, (unsigned)s_Ms.nmCount, (unsigned)s_Ms.nonMask,
                  haveLive ? (int)live.MultiSampleType : -1, haveLive ? (unsigned)live.MultiSampleQuality : 0u, (int)d3d9Globals.present.MultiSampleType, (int)d3d9Globals.present.Windowed,
                  haveLive ? live.BackBufferWidth : 0u, haveLive ? live.BackBufferHeight : 0u,
                  rw::d3d::d3ddevice ? (rw::d3d::d3ddevice->TestCooperativeLevel() == D3D_OK ? "ok" : "LOST") : "none");
}
// true when NOTSA_STANDALONE_WINDOWED is active: the window keeps its caption (WinMain) and is shown without stealing the focus
bool NotsaRwWindowedRun(void) {
    WindowedEmuInit();
    return s_Emu.Active();
}
#endif

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

// exe 0x7F84E0: the value found when the video mode was selected
RwUInt32 RwD3D9EngineGetMaxMultiSamplingLevels(void) { return s_Ms.maxLevel; }

// exe 0x7F84F0 (see MsSetLevels); applied to the presentation parameters at the next device creation / Reset
void RwD3D9EngineSetMultiSamplingLevels(RwUInt32 numLevels) {
    if (!IsOpenedOrStarted()) {
        return;
    }
    MsSetLevels(numLevels);
}

// exe 0x7F8A90: nothing when the device does not exist or the level equals the selected one; else select, rebuild the multisampling part of the
// present parameters, Reset; when that fails, the old selection / parameters are restored and the device is reset again.
RwBool RwD3D9ChangeMultiSamplingLevels(RwUInt32 numLevels) {
    if (!Is(rw::Engine::Started) || !rw::d3d::d3ddevice) {
        return TRUE;
    }
    if (s_Ms.sel == numLevels) {
        return TRUE;
    }
    const MsState               oldMs      = s_Ms;
    const D3DPRESENT_PARAMETERS oldPresent = d3d9Globals.present;
    MsSetLevels(numLevels);
    MsApplyToPresent(d3d9Globals.present);
    if (ResetDeviceNow()) {
        return TRUE;
    }
    s_Ms                = oldMs;
    d3d9Globals.present = oldPresent;
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

#endif
#endif // NOTSA_RW_LIBRW

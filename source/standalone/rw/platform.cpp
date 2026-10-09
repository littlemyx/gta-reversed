// P2B-09: platform glue between the game's WinPs/Gamma/Game code and librw's D3D9 device.
//   _rwD3D9Device{Set,Get}RestoreCallback + the lost-device / Reset path that invokes the callback (see below),
//   NotsaRwPlatform_* (the only entry points engine.cpp calls: install / remove the Reset watch on RwEngineStart / RwEngineStop).
//
// Original RW (WindowedMode.cpp: D3D9DeviceRestoreVideoMemory, exe 0x7F7xxx): after EVERY IDirect3DDevice9::Reset (lost device, window resize,
// video-mode / multisampling / vsync change) RW restores its video-memory objects and, as the last step, calls the callback stored with
// _rwD3D9DeviceSetRestoreCallback (global 0xC980B0, NULL by default, void(*)(void)). SA itself only does Set(Get()) (Game.cpp), i.e. a no-op.
// librw does the same restore (restoreVideoMemory) inside static functions of d3ddevice.cpp and knows no callback, so the Reset is *observed*:
//   - IDirect3DDevice9::Reset is proxied (own copy of the device's vtable, slot 16) and only counts the resets,
//   - rw::engine->device.beginUpdate / .showRaster (the only two librw entry points that can Reset; engine.cpp's ResetDeviceNow goes through
//     showRaster as well) are wrapped: when the count changed while they ran, the restore callback is invoked right after librw finished
//     restoring (same point as RW: render target, depth surface, rasters, dynamic buffers and render-state cache are valid again).
//
// Also here: RpAnisotPluginAttach, and the no-op RwCoreInjectHooks / RtAnim::InjectHooks (S).
//
// Gamma (CGamma) and WinPs (psGrabScreen, WndProc) need nothing here: they fetch the device with RwD3D9GetCurrentD3DDevice() (engine.cpp)
// and the caps with RwD3D9GetCaps() (the mutable D3DCAPS9 cache). D3DResourceSystem (game_sa/RenderWare) is compiled unchanged: its pools live in
// the data image and are only fed by RW-internal texture / index buffer creation, which librw does not route through it -> the pools stay
// empty and Init/Tidy/Shutdown/CancelBuffering are inert (see .notes/P2B_SHIM_PLAN.md, D3DResourceSystem = S).
//
// Only needs fakerw + librw + the CRT (also built by tests/standalone/rw_platform_test.cpp without the game PCH).
#ifdef NOTSA_RW_LIBRW
#include "fakerw.h"
#include <src/d3d/rwd3dimpl.h>

#include <cstring>

namespace {

rwD3D9DeviceRestoreCallBack s_RestoreCallback = nullptr;

// ---- Reset watch -------------------------------------------------------------------------------------------------------------------
constexpr int kResetVtblSlot = 16;  // IDirect3DDevice9::Reset (IUnknown 0-2, TestCooperativeLevel 3 ... Reset 16)
constexpr int kVtblSlots     = 119; // IDirect3DDevice9 (non-Ex)

using ResetFn = HRESULT(__stdcall*)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);

unsigned s_ResetCount = 0;
void**   s_OrigVtbl   = nullptr;          // the device's own vtable (restored on stop)
void**   s_ProxyVtbl  = nullptr;          // heap copy with the Reset slot replaced
IDirect3DDevice9* s_WatchedDevice = nullptr;
ResetFn  s_OrigReset  = nullptr;

void (*s_OrigBeginUpdate)(rw::Camera*)              = nullptr;
void (*s_OrigShowRaster)(rw::Raster*, rw::uint32)   = nullptr;

HRESULT __stdcall ProxyReset(IDirect3DDevice9* self, D3DPRESENT_PARAMETERS* pp) {
    const HRESULT hr = s_OrigReset(self, pp);
    if (SUCCEEDED(hr)) {
        s_ResetCount++;
    }
    return hr;
}

void FireRestoreCallbackIfReset(unsigned countBefore) {
    if (s_ResetCount != countBefore && s_RestoreCallback) {
        s_RestoreCallback();
    }
}

void WrappedBeginUpdate(rw::Camera* cam) {
    const unsigned before = s_ResetCount;
    s_OrigBeginUpdate(cam);
    FireRestoreCallbackIfReset(before);
}

void WrappedShowRaster(rw::Raster* raster, rw::uint32 flags) {
    const unsigned before = s_ResetCount;
    s_OrigShowRaster(raster, flags);
    FireRestoreCallbackIfReset(before);
}

} // namespace

// W: stored; invoked by the Reset watch above
void _rwD3D9DeviceSetRestoreCallback(rwD3D9DeviceRestoreCallBack callback) { s_RestoreCallback = callback; }

rwD3D9DeviceRestoreCallBack _rwD3D9DeviceGetRestoreCallback(void) { return s_RestoreCallback; }

// Called by RwEngineStart (engine.cpp) once the device exists. Idempotent.
void NotsaRwPlatform_OnEngineStarted() {
    auto* dev = rw::d3d::d3ddevice;
    if (!dev || !rw::engine || s_WatchedDevice) {
        return;
    }
    s_WatchedDevice = dev;
    s_OrigVtbl      = *reinterpret_cast<void***>(dev);
    s_ProxyVtbl     = new void*[kVtblSlots];
    std::memcpy(s_ProxyVtbl, s_OrigVtbl, sizeof(void*) * kVtblSlots);
    s_OrigReset                 = reinterpret_cast<ResetFn>(s_OrigVtbl[kResetVtblSlot]);
    s_ProxyVtbl[kResetVtblSlot] = reinterpret_cast<void*>(&ProxyReset);
    *reinterpret_cast<void***>(dev) = s_ProxyVtbl;

    s_OrigBeginUpdate = rw::engine->device.beginUpdate;
    s_OrigShowRaster  = rw::engine->device.showRaster;
    rw::engine->device.beginUpdate = &WrappedBeginUpdate;
    rw::engine->device.showRaster  = &WrappedShowRaster;
}

// Called by RwEngineStop (engine.cpp) before librw releases the device. Idempotent.
void NotsaRwPlatform_OnEngineStopping() {
    if (!s_WatchedDevice) {
        return;
    }
    if (rw::engine) {
        rw::engine->device.beginUpdate = s_OrigBeginUpdate;
        rw::engine->device.showRaster  = s_OrigShowRaster;
    }
    *reinterpret_cast<void***>(s_WatchedDevice) = s_OrigVtbl;
    delete[] s_ProxyVtbl;
    s_ProxyVtbl = nullptr; s_OrigVtbl = nullptr; s_WatchedDevice = nullptr; s_OrigReset = nullptr;
    s_OrigBeginUpdate = nullptr; s_OrigShowRaster = nullptr;
}

// W: RpAnisot plugin (app.cpp PluginAttach, before RwEngineOpen): the anisotropy level lives in a Texture plugin that librw registers on request only.
// rwtexdict.cpp (RwTexDictionaryGtaStreamRead) and RpAnisotTextureSetMaxAnisotropy read it.
RwBool RpAnisotPluginAttach(void) {
    static bool s_Attached = false;
    if (!s_Attached) {
        rw::registerAnisotropyPlugin();
        s_Attached = true;
    }
    return TRUE;
}

// S: hook installers of the exe-RenderWare wrappers (game_sa/RenderWare/rw/*.cpp, not built with librw) called from InjectHooksMain.cpp:
// there is no exe RW to patch.
void RwCoreInjectHooks() {}
namespace RtAnim { void InjectHooks() {} }

// Number of successful device Resets since RwEngineStart (diagnostics / tests)
unsigned NotsaRwPlatform_DeviceResetCount() { return s_ResetCount; }

#endif // NOTSA_RW_LIBRW
